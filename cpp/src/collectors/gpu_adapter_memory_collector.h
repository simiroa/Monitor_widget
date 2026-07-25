#pragma once

#include <windows.h>
#include <pdh.h>

#include <cstdint>

#include <QElapsedTimer>
#include <QStringList>

class GpuAdapterMemoryCollector {
public:
    GpuAdapterMemoryCollector();
    ~GpuAdapterMemoryCollector();

    bool update(const LUID &luid, double &used_gb, double &limit_gb);
    bool hasUsage() const { return has_usage_; }
    bool hasLimit() const { return has_limit_; }

private:
    bool refreshQuery(const LUID &luid);
    void clearQuery();

    bool enumInstances(const wchar_t *object_name, QStringList &instances);
    static bool parseInstanceLuid(const QString &instance, uint32_t &part_a, uint32_t &part_b);
    bool selectInstance(const LUID &luid, const QStringList &instances, QString &out_instance);

    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER usage_counter_ = nullptr;
    PDH_HCOUNTER limit_counter_ = nullptr;

    LUID last_luid_{};
    bool has_last_luid_ = false;

    QElapsedTimer refresh_timer_;
    bool refresh_started_ = false;
    int refresh_interval_ms_ = 15000;

    bool has_usage_ = false;
    bool has_limit_ = false;
    bool disabled_ = false;

    bool logged_object_missing_ = false;
    bool logged_query_failure_ = false;
    bool logged_counter_missing_ = false;
    bool logged_collect_failure_ = false;
    bool logged_luid_mismatch_ = false;
    PDH_STATUS last_enum_status_ = ERROR_SUCCESS;
};
