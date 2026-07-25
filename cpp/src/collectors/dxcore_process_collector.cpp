#include "collectors/dxcore_process_collector.h"

#include <algorithm>
#include <vector>

#include <QString>

#ifdef _WIN32
#include <psapi.h>
#endif

#include "utils/logger.h"

namespace {
constexpr double kBytesToMB = 1.0 / (1024.0 * 1024.0);
constexpr double kMicrosToPercent = 100.0;
}

DxcoreProcessCollector::DxcoreProcessCollector() = default;

DxcoreProcessCollector::~DxcoreProcessCollector() {
    resetAdapter();
    if (factory_) {
        factory_->Release();
        factory_ = nullptr;
    }
    if (module_) {
        FreeLibrary(module_);
        module_ = nullptr;
    }
}

bool DxcoreProcessCollector::loadFactory() {
    if (factory_) {
        return true;
    }

    module_ = LoadLibraryW(L"dxcore.dll");
    if (!module_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.dxcore_process", "dxcore.dll not available.");
            logged_load_failure_ = true;
        }
        return false;
    }

    create_factory_ = reinterpret_cast<PFN_DXCORE_CREATE_ADAPTER_FACTORY>(
        GetProcAddress(module_, "DXCoreCreateAdapterFactory"));
    if (!create_factory_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.dxcore_process", "DXCoreCreateAdapterFactory not found.");
            logged_load_failure_ = true;
        }
        return false;
    }

    HRESULT hr = create_factory_(IID_IDXCoreAdapterFactory, reinterpret_cast<void **>(&factory_));
    if (FAILED(hr) || !factory_) {
        if (!logged_factory_failure_) {
            Logger::warn("gpu.dxcore_process", QString("DXCoreCreateAdapterFactory failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_factory_failure_ = true;
        }
        return false;
    }

    return true;
}

void DxcoreProcessCollector::resetAdapter() {
    if (adapter_) {
        adapter_->Release();
        adapter_ = nullptr;
    }
    has_adapter_ = false;
    last_times_.clear();
    timer_started_ = false;
    sample_ready_ = false;
}

bool DxcoreProcessCollector::ensureAdapter(const LUID &luid) {
    if (!loadFactory()) {
        return false;
    }

    if (has_adapter_ && adapter_ &&
        adapter_luid_.HighPart == luid.HighPart &&
        adapter_luid_.LowPart == luid.LowPart) {
        return true;
    }

    resetAdapter();

    HRESULT hr = factory_->GetAdapterByLuid(luid, IID_IDXCoreAdapter, reinterpret_cast<void **>(&adapter_));
    if (FAILED(hr) || !adapter_) {
        if (!logged_adapter_failure_) {
            Logger::warn("gpu.dxcore_process", QString("GetAdapterByLuid failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_adapter_failure_ = true;
        }
        return false;
    }

    adapter_luid_ = luid;
    has_adapter_ = true;
    return true;
}

bool DxcoreProcessCollector::queryEngineCount(uint32_t &count) {
    count = 0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsPropertySupported(DXCoreAdapterProperty::AdapterEngineCount)) {
        if (!logged_engine_failure_) {
            Logger::warn("gpu.dxcore_process", "AdapterEngineCount not supported.");
            logged_engine_failure_ = true;
        }
        return false;
    }

    uint32_t value = 0;
    HRESULT hr = adapter_->GetProperty(DXCoreAdapterProperty::AdapterEngineCount, sizeof(value), &value);
    if (FAILED(hr) || value == 0) {
        if (!logged_engine_failure_) {
            Logger::warn("gpu.dxcore_process", QString("GetProperty(AdapterEngineCount) failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_engine_failure_ = true;
        }
        return false;
    }

    count = value;
    return true;
}

bool DxcoreProcessCollector::queryProcessList(QVector<uint32_t> &pids) {
    pids.clear();
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterInUseProcessSet)) {
        if (!logged_process_unsupported_) {
            Logger::warn("gpu.dxcore_process", "AdapterInUseProcessSet not supported; falling back to system process list.");
            logged_process_unsupported_ = true;
        }
        return enumSystemProcesses(pids);
    }

    uint32_t capacity = 64;
    std::vector<uint32_t> buffer(capacity);

    for (int attempt = 0; attempt < 2; ++attempt) {
        DXCoreAdapterProcessSetQueryInput input{};
        input.arraySize = capacity;
        input.processIds = buffer.data();

        DXCoreAdapterProcessSetQueryOutput output{};
        HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterInUseProcessSet,
            sizeof(input), &input, sizeof(output), &output);
        if (FAILED(hr)) {
            if (!logged_process_failure_) {
                Logger::warn("gpu.dxcore_process", QString("QueryState(AdapterInUseProcessSet) failed hr=0x%1.")
                    .arg(static_cast<unsigned long>(hr), 0, 16));
                logged_process_failure_ = true;
            }
            if (!logged_process_fallback_) {
                Logger::warn("gpu.dxcore_process", "AdapterInUseProcessSet failed; falling back to system process list.");
                logged_process_fallback_ = true;
            }
            return enumSystemProcesses(pids);
        }

        if (output.processesTotal > capacity) {
            capacity = output.processesTotal;
            buffer.resize(capacity);
            continue;
        }

        pids.reserve(static_cast<int>(output.processesWritten));
        for (uint32_t i = 0; i < output.processesWritten; ++i) {
            pids.push_back(buffer[i]);
        }
        return true;
    }

    return true;
}

bool DxcoreProcessCollector::enumSystemProcesses(QVector<uint32_t> &pids) {
    pids.clear();
#ifdef _WIN32
    DWORD bytes = 0;
    std::vector<DWORD> buffer(1024);
    for (int attempt = 0; attempt < 4; ++attempt) {
        if (!EnumProcesses(buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(DWORD)), &bytes)) {
            return false;
        }
        const size_t count = bytes / sizeof(DWORD);
        if (count < buffer.size()) {
            pids.reserve(static_cast<int>(count));
            for (size_t i = 0; i < count; ++i) {
                if (buffer[i] != 0) {
                    pids.push_back(buffer[i]);
                }
            }
            return true;
        }
        buffer.resize(buffer.size() * 2);
    }
    return false;
#else
    return false;
#endif
}

bool DxcoreProcessCollector::queryProcessMemory(uint32_t pid, double &vram_mb) {
    vram_mb = 0.0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterMemoryUsageByProcessBytes)) {
        if (!logged_memory_unsupported_) {
            Logger::warn("gpu.dxcore_process", "AdapterMemoryUsageByProcessBytes not supported.");
            logged_memory_unsupported_ = true;
        }
        return false;
    }

    DXCoreProcessMemoryQueryInput input{};
    input.physicalAdapterIndex = 0;
    input.memoryType = DXCoreMemoryType::Dedicated;
    input.processId = pid;

    DXCoreProcessMemoryQueryOutput output{};
    HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterMemoryUsageByProcessBytes,
        sizeof(input), &input, sizeof(output), &output);
    if (FAILED(hr)) {
        if (!logged_memory_failure_) {
            Logger::warn("gpu.dxcore_process", QString("QueryState(AdapterMemoryUsageByProcessBytes) failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_memory_failure_ = true;
        }
        return false;
    }

    if (!output.processQuerySucceeded) {
        return false;
    }

    const uint64_t bytes = output.memoryUsage.resident > 0
        ? output.memoryUsage.resident
        : output.memoryUsage.committed;
    vram_mb = static_cast<double>(bytes) * kBytesToMB;
    return true;
}

bool DxcoreProcessCollector::queryProcessEngineTimes(uint32_t pid, uint32_t engine_count,
    QVector<uint64_t> &times, bool &has_any) {
    times.fill(0, static_cast<int>(engine_count));
    has_any = false;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterEngineRunningTimeByProcessMicroseconds)) {
        if (!logged_engine_unsupported_) {
            Logger::warn("gpu.dxcore_process", "AdapterEngineRunningTimeByProcessMicroseconds not supported.");
            logged_engine_unsupported_ = true;
        }
        return false;
    }

    for (uint32_t i = 0; i < engine_count; ++i) {
        DXCoreEngineQueryInput input{};
        input.adapterEngineIndex.physicalAdapterIndex = 0;
        input.adapterEngineIndex.engineIndex = i;
        input.processId = pid;

        DXCoreEngineQueryOutput output{};
        HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterEngineRunningTimeByProcessMicroseconds,
            sizeof(input), &input, sizeof(output), &output);
        if (FAILED(hr)) {
            if (!logged_query_failure_) {
                Logger::warn("gpu.dxcore_process", QString("QueryState(AdapterEngineRunningTimeByProcessMicroseconds) failed hr=0x%1.")
                    .arg(static_cast<unsigned long>(hr), 0, 16));
                logged_query_failure_ = true;
            }
            return false;
        }

        if (output.processQuerySucceeded) {
            times[static_cast<int>(i)] = output.runningTime;
            has_any = true;
        }
    }

    return true;
}

bool DxcoreProcessCollector::update(const LUID &luid, QHash<qulonglong, double> &gpu_percent,
    QHash<qulonglong, double> &vram_mb) {
    gpu_percent.clear();
    vram_mb.clear();
    has_gpu_util_ = false;
    has_vram_ = false;
    has_overall_util_ = false;
    overall_util_ = 0.0;

    if (disabled_) {
        return false;
    }

    if (!ensureAdapter(luid)) {
        return false;
    }

    uint32_t engine_count = 0;
    const bool has_engine_count = queryEngineCount(engine_count);
    if (!has_engine_count) {
        engine_count = 0;
    }

    QVector<uint32_t> pids;
    if (!queryProcessList(pids)) {
        return false;
    }

    if (!timer_started_) {
        timer_.start();
        timer_started_ = true;
    }
    const qint64 elapsed_us = timer_.nsecsElapsed() / 1000;
    const bool can_compute = sample_ready_ && elapsed_us > 0;

    QHash<qulonglong, QVector<uint64_t>> current_times;
    current_times.reserve(pids.size());

    for (uint32_t pid : pids) {
        double mem_mb = 0.0;
        if (queryProcessMemory(pid, mem_mb)) {
            vram_mb.insert(pid, mem_mb);
            has_vram_ = true;
        }

        if (engine_count > 0) {
            QVector<uint64_t> times;
            bool has_any = false;
            if (queryProcessEngineTimes(pid, engine_count, times, has_any) && has_any) {
                current_times.insert(pid, times);
                if (can_compute) {
                    const auto last_it = last_times_.find(pid);
                    double max_util = 0.0;
                    if (last_it != last_times_.end() && last_it->size() == times.size()) {
                        for (int i = 0; i < times.size(); ++i) {
                            const qint64 delta = static_cast<qint64>(times[i]) - static_cast<qint64>(last_it->at(i));
                            const double delta_us = delta > 0 ? static_cast<double>(delta) : 0.0;
                            double util = (delta_us / static_cast<double>(elapsed_us)) * kMicrosToPercent;
                            if (util > 100.0) {
                                util = 100.0;
                            } else if (util < 0.0) {
                                util = 0.0;
                            }
                            if (util > max_util) {
                                max_util = util;
                            }
                        }
                    }
                    gpu_percent.insert(pid, max_util);
                    has_gpu_util_ = true;
                    if (max_util > overall_util_) {
                        overall_util_ = max_util;
                    }
                }
            }
        }
    }

    last_times_ = current_times;
    sample_ready_ = true;
    if (elapsed_us > 0) {
        timer_.restart();
    }

    has_overall_util_ = has_gpu_util_;
    return has_gpu_util_ || has_vram_;
}
