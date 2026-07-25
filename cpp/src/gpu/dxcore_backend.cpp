#include "gpu/dxcore_backend.h"

#include <algorithm>

#include <QString>

#include "utils/logger.h"

namespace {
constexpr double kBytesToGB = 1.0 / (1024.0 * 1024.0 * 1024.0);
}

DxcoreBackend::DxcoreBackend() = default;

DxcoreBackend::~DxcoreBackend() {
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

bool DxcoreBackend::sameLuid(const LUID &a, const LUID &b) {
    return a.HighPart == b.HighPart && a.LowPart == b.LowPart;
}

void DxcoreBackend::resetAdapter() {
    if (adapter_) {
        adapter_->Release();
        adapter_ = nullptr;
    }
    has_adapter_ = false;
    has_usage_ = false;
    has_total_ = false;
}

bool DxcoreBackend::loadFactory() {
    if (factory_) {
        return true;
    }

    module_ = LoadLibraryW(L"dxcore.dll");
    if (!module_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.dxcore", "dxcore.dll not available.");
            logged_load_failure_ = true;
        }
        return false;
    }

    create_factory_ = reinterpret_cast<PFN_DXCORE_CREATE_ADAPTER_FACTORY>(
        GetProcAddress(module_, "DXCoreCreateAdapterFactory"));
    if (!create_factory_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.dxcore", "DXCoreCreateAdapterFactory not found.");
            logged_load_failure_ = true;
        }
        return false;
    }

    HRESULT hr = create_factory_(IID_IDXCoreAdapterFactory, reinterpret_cast<void **>(&factory_));
    if (FAILED(hr) || !factory_) {
        if (!logged_factory_failure_) {
            Logger::warn("gpu.dxcore", QString("DXCoreCreateAdapterFactory failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_factory_failure_ = true;
        }
        return false;
    }

    return true;
}

bool DxcoreBackend::ensureAdapter(const LUID &luid) {
    if (!loadFactory()) {
        return false;
    }

    if (has_adapter_ && adapter_ && sameLuid(adapter_luid_, luid)) {
        return true;
    }

    resetAdapter();

    HRESULT hr = factory_->GetAdapterByLuid(luid, IID_IDXCoreAdapter, reinterpret_cast<void **>(&adapter_));
    if (FAILED(hr) || !adapter_) {
        if (!logged_adapter_failure_) {
            Logger::warn("gpu.dxcore", QString("GetAdapterByLuid failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_adapter_failure_ = true;
        }
        return false;
    }

    adapter_luid_ = luid;
    has_adapter_ = true;
    return true;
}

bool DxcoreBackend::queryUsageBudget(double &used_gb) {
    used_gb = 0.0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterMemoryBudget)) {
        return false;
    }

    DXCoreAdapterMemoryBudgetNodeSegmentGroup input{};
    input.nodeIndex = 0;
    input.segmentGroup = DXCoreSegmentGroup::Local;

    DXCoreAdapterMemoryBudget output{};
    HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterMemoryBudget,
        sizeof(input), &input, sizeof(output), &output);
    if (FAILED(hr)) {
        if (!logged_budget_failure_) {
            Logger::warn("gpu.dxcore", QString("QueryState(AdapterMemoryBudget) failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_budget_failure_ = true;
        }
        return false;
    }

    used_gb = std::max(0.0, static_cast<double>(output.currentUsage) * kBytesToGB);
    return true;
}

bool DxcoreBackend::queryUsageBytes(double &used_gb) {
    used_gb = 0.0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterMemoryUsageBytes)) {
        return false;
    }

    DXCoreMemoryQueryInput input{};
    input.physicalAdapterIndex = 0;
    input.memoryType = DXCoreMemoryType::Dedicated;

    DXCoreMemoryUsage output{};
    HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterMemoryUsageBytes,
        sizeof(input), &input, sizeof(output), &output);
    if (FAILED(hr)) {
        if (!logged_usage_bytes_failure_) {
            Logger::warn("gpu.dxcore", QString("QueryState(AdapterMemoryUsageBytes) failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_usage_bytes_failure_ = true;
        }
        return false;
    }

    const uint64_t bytes = output.resident > 0 ? output.resident : output.committed;
    used_gb = std::max(0.0, static_cast<double>(bytes) * kBytesToGB);
    return true;
}

bool DxcoreBackend::queryTotal(double &total_gb) {
    total_gb = 0.0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsPropertySupported(DXCoreAdapterProperty::DedicatedAdapterMemory)) {
        return false;
    }

    uint64_t bytes = 0;
    HRESULT hr = adapter_->GetProperty(DXCoreAdapterProperty::DedicatedAdapterMemory,
        sizeof(bytes), &bytes);
    if (FAILED(hr)) {
        if (!logged_property_failure_) {
            Logger::warn("gpu.dxcore", QString("GetProperty(DedicatedAdapterMemory) failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_property_failure_ = true;
        }
        return false;
    }

    total_gb = std::max(0.0, static_cast<double>(bytes) * kBytesToGB);
    has_total_ = bytes > 0;
    return true;
}

bool DxcoreBackend::queryTemperature(double &temp_c) {
    temp_c = 0.0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterTemperatureCelsius)) {
        if (!logged_temp_unsupported_) {
            Logger::warn("gpu.dxcore", "AdapterTemperatureCelsius not supported.");
            logged_temp_unsupported_ = true;
        }
        return false;
    }

    float temp_f = 0.0f;
    HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterTemperatureCelsius,
        0, nullptr, sizeof(temp_f), &temp_f);
    if (SUCCEEDED(hr) && temp_f > 0.1f && temp_f < 200.0f) {
        temp_c = static_cast<double>(temp_f);
        return true;
    }

    uint32_t temp_u = 0;
    hr = adapter_->QueryState(DXCoreAdapterState::AdapterTemperatureCelsius,
        0, nullptr, sizeof(temp_u), &temp_u);
    if (SUCCEEDED(hr) && temp_u > 0 && temp_u < 200) {
        temp_c = static_cast<double>(temp_u);
        return true;
    }

    if (FAILED(hr) && !logged_temp_failure_) {
        Logger::warn("gpu.dxcore", QString("QueryState(AdapterTemperatureCelsius) failed hr=0x%1.")
            .arg(static_cast<unsigned long>(hr), 0, 16));
        logged_temp_failure_ = true;
    }
    return false;
}

bool DxcoreBackend::queryEngineClock(double &clock_mhz) {
    clock_mhz = 0.0;
    if (!adapter_) {
        return false;
    }

    if (!adapter_->IsQueryStateSupported(DXCoreAdapterState::AdapterEngineFrequencyHertz)) {
        if (!logged_clock_unsupported_) {
            Logger::warn("gpu.dxcore", "AdapterEngineFrequencyHertz not supported.");
            logged_clock_unsupported_ = true;
        }
        return false;
    }

    DXCoreFrequencyQueryOutput output{};
    HRESULT hr = adapter_->QueryState(DXCoreAdapterState::AdapterEngineFrequencyHertz,
        0, nullptr, sizeof(output), &output);
    if (FAILED(hr)) {
        if (!logged_clock_failure_) {
            Logger::warn("gpu.dxcore", QString("QueryState(AdapterEngineFrequencyHertz) failed hr=0x%1.")
                .arg(static_cast<unsigned long>(hr), 0, 16));
            logged_clock_failure_ = true;
        }
        return false;
    }

    if (output.frequency == 0) {
        return false;
    }

    clock_mhz = static_cast<double>(output.frequency) / 1000000.0;
    return clock_mhz > 0.0;
}

bool DxcoreBackend::update(const LUID &luid, double &used_gb, double &total_gb) {
    used_gb = 0.0;
    total_gb = 0.0;
    has_usage_ = false;
    has_total_ = false;

    if (!ensureAdapter(luid)) {
        return false;
    }

    bool ok = false;
    double budget_used = 0.0;
    const bool budget_ok = queryUsageBudget(budget_used);
    if (budget_ok) {
        ok = true;
    }

    double bytes_used = 0.0;
    const bool bytes_ok = queryUsageBytes(bytes_used);
    if (bytes_ok) {
        ok = true;
    }

    if (budget_ok && budget_used > 0.0) {
        used_gb = budget_used;
    } else if (bytes_ok) {
        used_gb = bytes_used;
    } else if (budget_ok) {
        used_gb = budget_used;
    }

    has_usage_ = budget_ok || bytes_ok;

    if (queryTotal(total_gb)) {
        ok = true;
    }

    if (has_usage_ && !logged_zero_usage_ && used_gb <= 0.0) {
        Logger::warn("gpu.dxcore", "DXCore reported zero VRAM usage.");
        logged_zero_usage_ = true;
    }

    if (!logged_sample_ && Logger::isVerbose()) {
        Logger::debug("gpu.dxcore", QString("Usage budget=%1GB bytes=%2GB total=%3GB.")
            .arg(budget_used, 0, 'f', 2)
            .arg(bytes_used, 0, 'f', 2)
            .arg(total_gb, 0, 'f', 2));
        logged_sample_ = true;
    }

    return ok;
}

bool DxcoreBackend::queryTemperature(const LUID &luid, double &temp_c) {
    if (!ensureAdapter(luid)) {
        return false;
    }
    return queryTemperature(temp_c);
}

bool DxcoreBackend::queryEngineClock(const LUID &luid, double &clock_mhz) {
    if (!ensureAdapter(luid)) {
        return false;
    }
    return queryEngineClock(clock_mhz);
}
