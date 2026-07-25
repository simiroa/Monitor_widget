#include "collectors/gpu_process_collector.h"

#include <algorithm>
#include <vector>
#include <cwchar>

#include <pdhmsg.h>

#include <QStringList>

#include "utils/logger.h"

namespace {
constexpr double kBytesToMB = 1.0 / (1024.0 * 1024.0);
constexpr PDH_STATUS kPdhObjectMissing = 0x80000715;
}

GpuProcessCollector::GpuProcessCollector() = default;

GpuProcessCollector::~GpuProcessCollector() {
    clearQuery();
}

void GpuProcessCollector::clearQuery() {
    engine_counters_.clear();
    memory_counters_.clear();

    if (query_) {
        PdhCloseQuery(query_);
        query_ = nullptr;
    }
}

bool GpuProcessCollector::extractPid(const QString &instance, qulonglong &pid) {
    const int idx = instance.indexOf("pid_");
    if (idx < 0) {
        return false;
    }

    int pos = idx + 4;
    int end = pos;
    while (end < instance.size() && instance[end].isDigit()) {
        ++end;
    }

    if (end == pos) {
        return false;
    }

    bool ok = false;
    pid = instance.mid(pos, end - pos).toULongLong(&ok);
    return ok && pid > 0;
}

bool GpuProcessCollector::enumInstances(const wchar_t *object_name, QStringList &instances) const {
    DWORD counter_size = 0;
    DWORD instance_size = 0;

    PDH_STATUS status = PdhEnumObjectItemsW(nullptr, nullptr, object_name,
        nullptr, &counter_size, nullptr, &instance_size, PERF_DETAIL_WIZARD, 0);

    if (status != PDH_MORE_DATA) {
        return false;
    }

    std::vector<wchar_t> counter_buffer(counter_size);
    std::vector<wchar_t> instance_buffer(instance_size);

    status = PdhEnumObjectItemsW(nullptr, nullptr, object_name,
        counter_buffer.data(), &counter_size,
        instance_buffer.data(), &instance_size,
        PERF_DETAIL_WIZARD, 0);

    if (status != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t *ptr = instance_buffer.data();
    while (*ptr) {
        instances.push_back(QString::fromWCharArray(ptr));
        ptr += wcslen(ptr) + 1;
    }

    return !instances.isEmpty();
}

bool GpuProcessCollector::refreshQuery() {
    clearQuery();

    if (PdhOpenQuery(nullptr, 0, &query_) != ERROR_SUCCESS) {
        query_ = nullptr;
        if (!logged_query_failure_) {
            Logger::warn("collector.gpu_process", "PdhOpenQuery failed.");
            logged_query_failure_ = true;
        }
        return false;
    }

    QStringList engine_instances;
    if (enumInstances(L"GPU Engine", engine_instances)) {
        logged_engine_missing_ = false;
        for (const QString &instance : engine_instances) {
            qulonglong pid = 0;
            if (!extractPid(instance, pid)) {
                continue;
            }

            const QString path = QString("\\\\GPU Engine(%1)\\\\Utilization Percentage").arg(instance);
            const std::wstring path_w = path.toStdWString();

            PDH_HCOUNTER counter = nullptr;
            if (PdhAddCounterW(query_, path_w.c_str(), 0, &counter) == ERROR_SUCCESS) {
                engine_counters_.push_back({counter, pid});
            }
        }
    } else if (!logged_engine_missing_) {
        Logger::warn("collector.gpu_process", "PDH object GPU Engine not available.");
        logged_engine_missing_ = true;
    }

    QStringList memory_instances;
    if (enumInstances(L"GPU Process Memory", memory_instances)) {
        logged_memory_missing_ = false;
        for (const QString &instance : memory_instances) {
            qulonglong pid = 0;
            if (!extractPid(instance, pid)) {
                continue;
            }

            const QString path = QString("\\\\GPU Process Memory(%1)\\\\Dedicated Usage").arg(instance);
            const std::wstring path_w = path.toStdWString();

            PDH_HCOUNTER counter = nullptr;
            if (PdhAddCounterW(query_, path_w.c_str(), 0, &counter) == ERROR_SUCCESS) {
                memory_counters_.push_back({counter, pid});
            }
        }
    } else if (!logged_memory_missing_) {
        Logger::warn("collector.gpu_process", "PDH object GPU Process Memory not available.");
        logged_memory_missing_ = true;
    }

    Logger::debug("collector.gpu_process", QString("Counters engine=%1 memory=%2")
        .arg(engine_counters_.size())
        .arg(memory_counters_.size()));

    if (engine_counters_.isEmpty() && memory_counters_.isEmpty()) {
        clearQuery();
        return false;
    }

    PdhCollectQueryData(query_);
    return true;
}

bool GpuProcessCollector::update(QHash<qulonglong, double> &gpu_percent, QHash<qulonglong, double> &vram_mb) {
    gpu_percent.clear();
    vram_mb.clear();
    overall_util_ = 0.0;
    has_overall_util_ = false;

    if (disabled_) {
        return false;
    }

    if (!refresh_started_) {
        refresh_timer_.start();
        refresh_started_ = true;
        refreshQuery();
    } else if (!query_ || refresh_timer_.elapsed() >= refresh_interval_ms_) {
        refreshQuery();
        refresh_timer_.restart();
    }

    if (!query_) {
        has_gpu_util_ = false;
        has_vram_ = false;
        return false;
    }

    PDH_STATUS collect_status = PdhCollectQueryData(query_);
    if (collect_status != ERROR_SUCCESS) {
        refreshQuery();
        collect_status = query_ ? PdhCollectQueryData(query_) : collect_status;
        if (collect_status != ERROR_SUCCESS) {
            has_gpu_util_ = false;
            has_vram_ = false;
            if (!logged_collect_failure_) {
                Logger::warn("collector.gpu_process",
                    QString("PdhCollectQueryData failed status=%1.").arg(static_cast<unsigned long>(collect_status)));
                logged_collect_failure_ = true;
            }
            if (collect_status == PDH_CSTATUS_NO_OBJECT ||
                collect_status == PDH_CSTATUS_NO_COUNTER ||
                collect_status == PDH_ACCESS_DENIED ||
                collect_status == kPdhObjectMissing) {
                disabled_ = true;
                clearQuery();
            }
            return false;
        }
    }

    bool got_gpu = false;
    bool got_vram = false;

    for (const auto &entry : engine_counters_) {
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(entry.counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
            const double util = std::max(0.0, value.doubleValue);
            double &slot = gpu_percent[entry.pid];
            if (util > slot) {
                slot = util;
            }
            if (util > overall_util_) {
                overall_util_ = util;
            }
            got_gpu = true;
        }
    }

    for (const auto &entry : memory_counters_) {
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(entry.counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
            const double mb = std::max(0.0, value.doubleValue * kBytesToMB);
            vram_mb[entry.pid] += mb;
            got_vram = true;
        }
    }

    has_gpu_util_ = got_gpu;
    has_vram_ = got_vram;
    has_overall_util_ = got_gpu;
    return got_gpu || got_vram;
}
