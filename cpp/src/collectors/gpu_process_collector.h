#pragma once

#include <pdh.h>

#include <QElapsedTimer>
#include <QHash>
#include <QStringList>
#include <QVector>

class GpuProcessCollector {
public:
    GpuProcessCollector();
    ~GpuProcessCollector();

    bool update(QHash<qulonglong, double> &gpu_percent, QHash<qulonglong, double> &vram_mb);

    bool hasGpuUtil() const { return has_gpu_util_; }
    bool hasVram() const { return has_vram_; }
    bool hasOverallUtil() const { return has_overall_util_; }
    double overallUtil() const { return overall_util_; }

private:
    struct CounterEntry {
        PDH_HCOUNTER counter = nullptr;
        qulonglong pid = 0;
    };

    bool refreshQuery();
    void clearQuery();

    bool enumInstances(const wchar_t *object_name, QStringList &instances) const;
    static bool extractPid(const QString &instance, qulonglong &pid);

    PDH_HQUERY query_ = nullptr;
    QVector<CounterEntry> engine_counters_;
    QVector<CounterEntry> memory_counters_;

    QElapsedTimer refresh_timer_;
    bool refresh_started_ = false;
    int refresh_interval_ms_ = 10000;

    bool has_gpu_util_ = false;
    bool has_vram_ = false;
    bool has_overall_util_ = false;
    double overall_util_ = 0.0;
    bool disabled_ = false;
    bool logged_engine_missing_ = false;
    bool logged_memory_missing_ = false;
    bool logged_query_failure_ = false;
    bool logged_collect_failure_ = false;
};
