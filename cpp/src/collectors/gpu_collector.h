#pragma once

#include <QHash>
#include <QSet>

#ifdef _WIN32
#include <windows.h>
#endif

#include "models/capability_flags.h"
#include "models/system_stats.h"

#include "collectors/gpu_adapter_memory_collector.h"
#include "gpu/dxgi_backend.h"
#include "gpu/dxcore_backend.h"
#include "gpu/d3dkmt_backend.h"
#include "gpu/nvml_backend.h"

class GpuCollector {
public:
    GpuCollector();
    void update(SystemStats &stats);
    const CapabilityFlags &capabilities() const { return caps_; }
    bool queryProcessVram(QHash<qulonglong, double> &vram_mb, QSet<qulonglong> &pids);
    bool adapterLuid(LUID &out) const;
    bool nvmlAvailable() const { return nvml_.isAvailable(); }

private:
    enum class NvmlMode {
        Auto,
        Enabled,
        Disabled
    };

    void refreshNvmlConfig() const;
    bool nvmlEnabled() const;
    NvmlBackend nvml_;
    DxgiBackend dxgi_;
    DxcoreBackend dxcore_;
    D3dkmtBackend d3dkmt_;
    GpuAdapterMemoryCollector adapter_memory_;
    CapabilityFlags caps_;
    QString last_backend_;
    bool logged_no_backend_ = false;
    mutable bool nvml_checked_ = false;
    mutable bool nvml_env_set_ = false;
    mutable NvmlMode nvml_mode_ = NvmlMode::Auto;
    bool logged_nvml_disabled_ = false;
    bool logged_nvml_unavailable_ = false;
};
