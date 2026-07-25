#pragma once

#include <windows.h>

#include <QElapsedTimer>
#include <QHash>
#include <QVector>

#include "gpu/dxcore_min.h"

class DxcoreProcessCollector {
public:
    DxcoreProcessCollector();
    ~DxcoreProcessCollector();

    bool update(const LUID &luid, QHash<qulonglong, double> &gpu_percent, QHash<qulonglong, double> &vram_mb);

    bool hasGpuUtil() const { return has_gpu_util_; }
    bool hasVram() const { return has_vram_; }
    bool hasOverallUtil() const { return has_overall_util_; }
    double overallUtil() const { return overall_util_; }

private:
    bool loadFactory();
    bool ensureAdapter(const LUID &luid);
    void resetAdapter();

    bool queryEngineCount(uint32_t &count);
    bool queryProcessList(QVector<uint32_t> &pids);
    bool enumSystemProcesses(QVector<uint32_t> &pids);
    bool queryProcessMemory(uint32_t pid, double &vram_mb);
    bool queryProcessEngineTimes(uint32_t pid, uint32_t engine_count, QVector<uint64_t> &times, bool &has_any);

    HMODULE module_ = nullptr;
    PFN_DXCORE_CREATE_ADAPTER_FACTORY create_factory_ = nullptr;
    IDXCoreAdapterFactory *factory_ = nullptr;
    IDXCoreAdapter *adapter_ = nullptr;
    LUID adapter_luid_{};
    bool has_adapter_ = false;

    QElapsedTimer timer_;
    bool timer_started_ = false;
    bool sample_ready_ = false;

    QHash<qulonglong, QVector<uint64_t>> last_times_;

    bool has_gpu_util_ = false;
    bool has_vram_ = false;
    bool has_overall_util_ = false;
    double overall_util_ = 0.0;
    bool disabled_ = false;

    bool logged_load_failure_ = false;
    bool logged_factory_failure_ = false;
    bool logged_adapter_failure_ = false;
    bool logged_engine_failure_ = false;
    bool logged_process_failure_ = false;
    bool logged_process_unsupported_ = false;
    bool logged_process_fallback_ = false;
    bool logged_query_failure_ = false;
    bool logged_memory_failure_ = false;
    bool logged_memory_unsupported_ = false;
    bool logged_engine_unsupported_ = false;
};
