#include "collectors/gpu_collector.h"

#include <QByteArray>
#include <QString>

#include "utils/logger.h"

GpuCollector::GpuCollector() = default;

void GpuCollector::refreshNvmlConfig() const {
    if (nvml_checked_) {
        return;
    }

    nvml_checked_ = true;
    const QByteArray raw = qgetenv("MONITOR_WIDGET_ENABLE_NVML");
    if (raw.isEmpty()) {
        nvml_env_set_ = false;
        nvml_mode_ = NvmlMode::Auto;
        return;
    }

    nvml_env_set_ = true;
    const QString value = QString::fromLocal8Bit(raw).trimmed().toLower();
    if (value == "0" || value == "false" || value == "off" || value == "no") {
        nvml_mode_ = NvmlMode::Disabled;
    } else if (value == "1" || value == "true" || value == "on" || value == "yes") {
        nvml_mode_ = NvmlMode::Enabled;
    } else {
        nvml_mode_ = NvmlMode::Enabled;
    }
}

bool GpuCollector::nvmlEnabled() const {
    refreshNvmlConfig();
    return nvml_mode_ != NvmlMode::Disabled;
}

void GpuCollector::update(SystemStats &stats) {
    caps_ = CapabilityFlags{};

    refreshNvmlConfig();
    const bool nvml_allowed = nvmlEnabled();
    bool nvml_available = false;
    bool used_nvml = false;
    if (nvml_allowed) {
        nvml_available = nvml_.initialize();
        if (nvml_available) {
            nvml_.update(stats, caps_);
            used_nvml = true;
        }
    }

    if (!used_nvml) {
        dxgi_.update(stats, caps_);
    } else {
        dxgi_.initialize();
    }

    caps_.nvmlEnabled = nvml_allowed;
    caps_.nvmlAvailable = nvml_available;

    if (!nvml_allowed && !logged_nvml_disabled_) {
        Logger::warn("collector.gpu", "NVML disabled via MONITOR_WIDGET_ENABLE_NVML=0.");
        logged_nvml_disabled_ = true;
    } else if (nvml_allowed && !nvml_available && !logged_nvml_unavailable_) {
        Logger::warn("collector.gpu", "NVML unavailable; using DXGI/D3D backends.");
        logged_nvml_unavailable_ = true;
    }

    LUID luid{};
    const bool has_luid = dxgi_.adapterLuid(luid);

    if (!used_nvml && !caps_.gpuLoadAvailable && has_luid) {
        double load = 0.0;
        const bool d3d_ok = d3dkmt_.update(luid, load);
        if (d3d_ok || d3dkmt_.hasSample()) {
            stats.gpuLoad = d3d_ok ? load : d3dkmt_.lastUtil();
            caps_.gpuLoadAvailable = d3dkmt_.hasSample();
            if (caps_.gpuBackend == "DXGI") {
                caps_.gpuBackend = "DXGI+D3DKMT";
            }
        }
    }

    if (!used_nvml && has_luid) {
        double temp_c = 0.0;
        if (dxcore_.queryTemperature(luid, temp_c) && temp_c > 0.0) {
            stats.gpuTempC = temp_c;
            caps_.gpuTempAvailable = true;
        }

        double clock_mhz = 0.0;
        if (dxcore_.queryEngineClock(luid, clock_mhz) && clock_mhz > 0.0) {
            stats.gpuClockMHz = clock_mhz;
            caps_.gpuClockAvailable = true;
        }

        double dxcore_used = 0.0;
        double dxcore_total = 0.0;
        const bool dxcore_ok = dxcore_.update(luid, dxcore_used, dxcore_total);
        if (dxcore_ok) {
            if (dxcore_.hasUsage() && stats.vramUsedGB <= 0.0) {
                stats.vramUsedGB = dxcore_used;
            }
            if (dxcore_.hasTotal() && stats.vramTotalGB <= 0.0) {
                stats.vramTotalGB = dxcore_total;
            }
            if ((dxcore_.hasUsage() || dxcore_.hasTotal()) && !caps_.gpuBackend.contains("DXCore")) {
                caps_.gpuBackend = caps_.gpuBackend.isEmpty()
                    ? "DXCore"
                    : caps_.gpuBackend + "+DXCore";
            }
        }

        double used_gb = 0.0;
        double limit_gb = 0.0;
        const bool pdh_ok = adapter_memory_.update(luid, used_gb, limit_gb);
        if (pdh_ok && adapter_memory_.hasUsage() && stats.vramUsedGB <= 0.0) {
            stats.vramUsedGB = used_gb;
        }
        if (pdh_ok && adapter_memory_.hasLimit() && stats.vramTotalGB <= 0.0 && limit_gb > 0.0) {
            stats.vramTotalGB = limit_gb;
        }
        if (pdh_ok && (adapter_memory_.hasUsage() || adapter_memory_.hasLimit())) {
            if (!caps_.gpuBackend.contains("PDH")) {
                caps_.gpuBackend = caps_.gpuBackend.isEmpty()
                    ? "PDH"
                    : caps_.gpuBackend + "+PDH";
            }
        }

        if (stats.vramTotalGB > 0.0) {
            stats.vramPercent = (stats.vramUsedGB / stats.vramTotalGB) * 100.0;
        } else {
            stats.vramPercent = 0.0;
        }
        caps_.vramAvailable = stats.vramTotalGB > 0.0;
    }

    if (caps_.gpuBackend.isEmpty()) {
        if (!logged_no_backend_) {
            Logger::warn("collector.gpu", "No GPU backend available.");
            logged_no_backend_ = true;
        }
    } else if (caps_.gpuBackend != last_backend_) {
        Logger::info("collector.gpu", QString("Backend=%1 vendor=%2")
            .arg(caps_.gpuBackend, caps_.gpuVendor.isEmpty() ? "-" : caps_.gpuVendor));
        last_backend_ = caps_.gpuBackend;
    }
}

bool GpuCollector::queryProcessVram(QHash<qulonglong, double> &vram_mb, QSet<qulonglong> &pids) {
    if (!nvmlEnabled()) {
        return false;
    }
    if (!nvml_.isAvailable() && !nvml_.initialize()) {
        return false;
    }
    return nvml_.queryProcessMemory(vram_mb, pids);
}

bool GpuCollector::adapterLuid(LUID &out) const {
    return dxgi_.adapterLuid(out);
}
