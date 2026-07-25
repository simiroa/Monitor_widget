#include "collectors/gpu_adapter_memory_collector.h"

#include <algorithm>
#include <vector>
#include <cwchar>

#include <pdhmsg.h>

#include "utils/logger.h"

namespace {
constexpr double kBytesToGB = 1.0 / (1024.0 * 1024.0 * 1024.0);
constexpr PDH_STATUS kPdhObjectMissing = 0x80000715;
}

GpuAdapterMemoryCollector::GpuAdapterMemoryCollector() = default;

GpuAdapterMemoryCollector::~GpuAdapterMemoryCollector() {
    clearQuery();
}

void GpuAdapterMemoryCollector::clearQuery() {
    if (query_) {
        PdhCloseQuery(query_);
        query_ = nullptr;
    }
    usage_counter_ = nullptr;
    limit_counter_ = nullptr;
}

bool GpuAdapterMemoryCollector::enumInstances(const wchar_t *object_name, QStringList &instances) {
    DWORD counter_size = 0;
    DWORD instance_size = 0;

    last_enum_status_ = PdhEnumObjectItemsW(nullptr, nullptr, object_name,
        nullptr, &counter_size, nullptr, &instance_size, PERF_DETAIL_WIZARD, 0);

    if (last_enum_status_ != PDH_MORE_DATA) {
        return false;
    }

    std::vector<wchar_t> counter_buffer(counter_size);
    std::vector<wchar_t> instance_buffer(instance_size);

    last_enum_status_ = PdhEnumObjectItemsW(nullptr, nullptr, object_name,
        counter_buffer.data(), &counter_size,
        instance_buffer.data(), &instance_size,
        PERF_DETAIL_WIZARD, 0);

    if (last_enum_status_ != ERROR_SUCCESS) {
        return false;
    }

    const wchar_t *ptr = instance_buffer.data();
    while (*ptr) {
        instances.push_back(QString::fromWCharArray(ptr));
        ptr += wcslen(ptr) + 1;
    }

    return !instances.isEmpty();
}

bool GpuAdapterMemoryCollector::parseInstanceLuid(const QString &instance, uint32_t &part_a, uint32_t &part_b) {
    const int luid_idx = instance.indexOf("luid_0x", 0, Qt::CaseInsensitive);
    if (luid_idx < 0) {
        return false;
    }

    const int first_start = luid_idx + 7;
    const int second_marker = instance.indexOf("_0x", first_start, Qt::CaseInsensitive);
    if (second_marker < 0) {
        return false;
    }

    const auto is_hex = [](QChar ch) {
        return ch.isDigit() || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    };

    int first_end = first_start;
    while (first_end < instance.size() && is_hex(instance[first_end])) {
        ++first_end;
    }

    const int second_start = second_marker + 3;
    int second_end = second_start;
    while (second_end < instance.size() && is_hex(instance[second_end])) {
        ++second_end;
    }

    const QString first_part = instance.mid(first_start, first_end - first_start);
    const QString second_part = instance.mid(second_start, second_end - second_start);

    bool ok_first = false;
    bool ok_second = false;
    const uint32_t first_value = first_part.toUInt(&ok_first, 16);
    const uint32_t second_value = second_part.toUInt(&ok_second, 16);
    if (!ok_first || !ok_second) {
        return false;
    }

    part_a = first_value;
    part_b = second_value;
    return true;
}

bool GpuAdapterMemoryCollector::selectInstance(const LUID &luid, const QStringList &instances, QString &out_instance) {
    if (instances.isEmpty()) {
        return false;
    }

    const uint32_t low = static_cast<uint32_t>(luid.LowPart);
    const uint32_t high = static_cast<uint32_t>(luid.HighPart);

    for (const QString &instance : instances) {
        uint32_t part_a = 0;
        uint32_t part_b = 0;
        if (!parseInstanceLuid(instance, part_a, part_b)) {
            continue;
        }
        if ((part_a == low && part_b == high) || (part_a == high && part_b == low)) {
            out_instance = instance;
            logged_luid_mismatch_ = false;
            return true;
        }
    }

    if (instances.size() == 1) {
        out_instance = instances.front();
        return true;
    }

    if (!logged_luid_mismatch_) {
        Logger::warn("collector.gpu_adapter_memory", "No GPU Adapter Memory instance matched the adapter LUID.");
        logged_luid_mismatch_ = true;
    }
    return false;
}

bool GpuAdapterMemoryCollector::refreshQuery(const LUID &luid) {
    clearQuery();

    if (PdhOpenQuery(nullptr, 0, &query_) != ERROR_SUCCESS) {
        query_ = nullptr;
        if (!logged_query_failure_) {
            Logger::warn("collector.gpu_adapter_memory", "PdhOpenQuery failed.");
            logged_query_failure_ = true;
        }
        return false;
    }

    QStringList instances;
    if (!enumInstances(L"GPU Adapter Memory", instances)) {
        if (!logged_object_missing_) {
            Logger::warn("collector.gpu_adapter_memory", "PDH object GPU Adapter Memory not available.");
            logged_object_missing_ = true;
        }
        if (last_enum_status_ == PDH_CSTATUS_NO_OBJECT ||
            last_enum_status_ == PDH_CSTATUS_NO_COUNTER ||
            last_enum_status_ == PDH_ACCESS_DENIED ||
            last_enum_status_ == kPdhObjectMissing) {
            disabled_ = true;
        }
        clearQuery();
        return false;
    }

    QString instance;
    if (!selectInstance(luid, instances, instance)) {
        clearQuery();
        return false;
    }

    const QString usage_path = QString("\\\\GPU Adapter Memory(%1)\\\\Dedicated Usage").arg(instance);
    const std::wstring usage_path_w = usage_path.toStdWString();
    if (PdhAddCounterW(query_, usage_path_w.c_str(), 0, &usage_counter_) != ERROR_SUCCESS) {
        usage_counter_ = nullptr;
    }

    const QString limit_path = QString("\\\\GPU Adapter Memory(%1)\\\\Dedicated Limit").arg(instance);
    const std::wstring limit_path_w = limit_path.toStdWString();
    if (PdhAddCounterW(query_, limit_path_w.c_str(), 0, &limit_counter_) != ERROR_SUCCESS) {
        limit_counter_ = nullptr;
    }

    if (!usage_counter_ && !limit_counter_) {
        if (!logged_counter_missing_) {
            Logger::warn("collector.gpu_adapter_memory", "No GPU Adapter Memory counters available.");
            logged_counter_missing_ = true;
        }
        clearQuery();
        return false;
    }

    last_luid_ = luid;
    has_last_luid_ = true;
    PdhCollectQueryData(query_);
    return true;
}

bool GpuAdapterMemoryCollector::update(const LUID &luid, double &used_gb, double &limit_gb) {
    used_gb = 0.0;
    limit_gb = 0.0;
    has_usage_ = false;
    has_limit_ = false;

    if (disabled_) {
        return false;
    }

    const bool luid_changed = !has_last_luid_ ||
        last_luid_.LowPart != luid.LowPart ||
        last_luid_.HighPart != luid.HighPart;

    if (!refresh_started_) {
        refresh_timer_.start();
        refresh_started_ = true;
        refreshQuery(luid);
    } else if (!query_ || luid_changed || refresh_timer_.elapsed() >= refresh_interval_ms_) {
        refreshQuery(luid);
        refresh_timer_.restart();
    }

    if (!query_) {
        return false;
    }

    PDH_STATUS collect_status = PdhCollectQueryData(query_);
    if (collect_status != ERROR_SUCCESS) {
        refreshQuery(luid);
        collect_status = query_ ? PdhCollectQueryData(query_) : collect_status;
        if (collect_status != ERROR_SUCCESS) {
            if (!logged_collect_failure_) {
                Logger::warn("collector.gpu_adapter_memory",
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

    if (usage_counter_) {
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(usage_counter_, PDH_FMT_LARGE, nullptr, &value) == ERROR_SUCCESS) {
            const double bytes = static_cast<double>(value.largeValue);
            used_gb = std::max(0.0, bytes * kBytesToGB);
            has_usage_ = true;
        }
    }

    if (limit_counter_) {
        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(limit_counter_, PDH_FMT_LARGE, nullptr, &value) == ERROR_SUCCESS) {
            const double bytes = static_cast<double>(value.largeValue);
            limit_gb = std::max(0.0, bytes * kBytesToGB);
            has_limit_ = true;
        }
    }

    return has_usage_ || has_limit_;
}
