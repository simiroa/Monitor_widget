#include "stats_engine.h"

#include <algorithm>

#include <QSet>
#include <QStringList>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utils/logger.h"

namespace {
QString formatDriveSummary(const QVector<DriveInfo> &drives) {
    if (drives.isEmpty()) {
        return "-";
    }

    QStringList parts;
    int count = 0;
    for (const auto &drive : drives) {
        if (count >= 3) {
            break;
        }
        parts.push_back(QString("%1 %2%").arg(drive.mountpoint).arg(drive.percent, 0, 'f', 0));
        ++count;
    }
    if (drives.size() > count) {
        parts.push_back(QString("+%1").arg(drives.size() - count));
    }

    return parts.join(" ");
}
}

StatsEngine::StatsEngine(QObject *parent)
    : QObject(parent),
      sensor_host_(this) {
    timer_.setParent(this);
    timer_.setInterval(2000);
    connect(&timer_, &QTimer::timeout, this, &StatsEngine::updateOnce);
}

void StatsEngine::start() {
    if (running_) {
        return;
    }
#ifdef _WIN32
    // Initialize COM for DxCore on this thread
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        Logger::warn("engine", QString("CoInitializeEx failed hr=0x%1").arg(static_cast<unsigned long>(hr), 0, 16));
    }
#endif
    running_ = true;
    timer_.start();
    sensor_host_.start();
}

void StatsEngine::stop() {
    running_ = false;
    timer_.stop();
    sensor_host_.stop();
#ifdef _WIN32
    CoUninitialize();
#endif
}

void StatsEngine::setNetProcessUpdatesEnabled(bool enabled) {
    net_updates_enabled_ = enabled;
    if (!enabled) {
        net_process_initial_update_ = false;
        net_process_timer_started_ = false;
    }
}

bool StatsEngine::isRunning() const {
    return running_;
}

void StatsEngine::updateOnce() {
    CapabilityFlags caps = stats_.capabilities;

    cpu_mem_.update(stats_, caps);
    disk_.update(stats_);
    net_.update(stats_);

    if (!drive_timer_started_) {
        drive_timer_.start();
        drive_timer_started_ = true;
        drive_.update(stats_);
    } else if (drive_timer_.elapsed() >= drive_interval_ms_) {
        drive_timer_.restart();
        drive_.update(stats_);
    }

    gpu_.update(stats_);
    const CapabilityFlags gpu_caps = gpu_.capabilities();
    caps.gpuNameAvailable = gpu_caps.gpuNameAvailable;
    caps.gpuLoadAvailable = gpu_caps.gpuLoadAvailable;
    caps.gpuTempAvailable = gpu_caps.gpuTempAvailable;
    caps.vramAvailable = gpu_caps.vramAvailable;
    caps.gpuClockAvailable = gpu_caps.gpuClockAvailable;
    caps.gpuPowerAvailable = gpu_caps.gpuPowerAvailable;
    caps.nvmlEnabled = gpu_caps.nvmlEnabled;
    caps.nvmlAvailable = gpu_caps.nvmlAvailable;
    caps.gpuBackend = gpu_caps.gpuBackend;
    caps.gpuVendor = gpu_caps.gpuVendor;

    sensor_host_.apply(stats_, caps);

    if (!server_timer_started_) {
        server_timer_.start();
        server_timer_started_ = true;
        server_detector_.update();
        stats_.serverPoints = server_detector_.buildServerPoints();
    } else if (server_timer_.elapsed() >= server_interval_ms_) {
        server_timer_.restart();
        server_detector_.update();
        stats_.serverPoints = server_detector_.buildServerPoints();
    }

    if (!process_timer_started_) {
        process_timer_.start();
        process_timer_started_ = true;
    }

    if (!process_initial_update_ || process_timer_.elapsed() >= process_interval_ms_) {
        process_timer_.restart();
        process_initial_update_ = true;
        process_collector_.update(cached_processes_);

        QHash<qulonglong, double> gpu_usage;
        QHash<qulonglong, double> vram_usage;
        gpu_process_.update(gpu_usage, vram_usage);

        QHash<qulonglong, double> dxcore_gpu;
        QHash<qulonglong, double> dxcore_vram;
        bool dxcore_ok = false;
        QHash<qulonglong, double> d3dkmt_vram;
        bool d3dkmt_ok = false;
#ifdef _WIN32
        LUID luid{};
        const bool has_luid = gpu_.adapterLuid(luid);
        if (has_luid && (!gpu_process_.hasGpuUtil() || !gpu_process_.hasVram())) {
            // Re-enable DxCore (safe now with COM init)
            dxcore_ok = dxcore_process_.update(luid, dxcore_gpu, dxcore_vram);
            // dxcore_ok = false;
        }
        if (has_luid && (!gpu_process_.hasVram() && !dxcore_process_.hasVram())) {
            QVector<qulonglong> pids;
            pids.reserve(cached_processes_.size());
            for (const auto &proc : cached_processes_) {
                pids.push_back(static_cast<qulonglong>(proc.pid));
            }
            // Disable D3DKMT
            // d3dkmt_ok = d3dkmt_process_.update(luid, pids, d3dkmt_vram);
            d3dkmt_ok = false;
        }
#endif
        if (dxcore_ok) {
            for (auto it = dxcore_gpu.constBegin(); it != dxcore_gpu.constEnd(); ++it) {
                const qulonglong pid = it.key();
                const double value = it.value();
                if (!gpu_usage.contains(pid) || gpu_usage.value(pid) < value) {
                    gpu_usage.insert(pid, value);
                }
            }
            for (auto it = dxcore_vram.constBegin(); it != dxcore_vram.constEnd(); ++it) {
                const qulonglong pid = it.key();
                const double value = it.value();
                if (!vram_usage.contains(pid) || vram_usage.value(pid) < value) {
                    vram_usage.insert(pid, value);
                }
            }
        }
        if (d3dkmt_ok) {
            for (auto it = d3dkmt_vram.constBegin(); it != d3dkmt_vram.constEnd(); ++it) {
                const qulonglong pid = it.key();
                const double value = it.value();
                if (!vram_usage.contains(pid) || vram_usage.value(pid) < value) {
                    vram_usage.insert(pid, value);
                }
            }
        }

        QHash<qulonglong, double> nvml_vram;
        QSet<qulonglong> nvml_pids;
        // Turn off NVML query as user reported 0MB. Use DxCore instead.
        // const bool nvml_ok = gpu_.queryProcessVram(nvml_vram, nvml_pids);
        const bool nvml_ok = false;
        if (nvml_ok) {
            for (auto it = nvml_vram.constBegin(); it != nvml_vram.constEnd(); ++it) {
                const qulonglong pid = it.key();
                const double value = it.value();
                if (!vram_usage.contains(pid) || vram_usage.value(pid) < value) {
                    vram_usage.insert(pid, value);
                }
            }
            for (const auto pid : nvml_pids) {
                if (!vram_usage.contains(pid)) {
                    vram_usage.insert(pid, 0.0);
                }
            }
        }

        const bool dxcore_gpu_available = dxcore_process_.hasGpuUtil();
        const bool dxcore_vram_available = dxcore_process_.hasVram();
        const bool d3dkmt_vram_available = d3dkmt_process_.hasVram();
        caps.perProcessGpuAvailable = gpu_process_.hasGpuUtil() || dxcore_gpu_available;
        caps.perProcessVramAvailable = gpu_process_.hasVram() || dxcore_vram_available || d3dkmt_vram_available || nvml_ok;
        if (!caps.gpuLoadAvailable && (gpu_process_.hasOverallUtil() || dxcore_process_.hasOverallUtil())) {
            const double pdh_util = gpu_process_.hasOverallUtil() ? gpu_process_.overallUtil() : 0.0;
            const double dxcore_util = dxcore_process_.hasOverallUtil() ? dxcore_process_.overallUtil() : 0.0;
            stats_.gpuLoad = std::max(pdh_util, dxcore_util);
            caps.gpuLoadAvailable = true;
        }

        for (auto &proc : cached_processes_) {
            const unsigned long pid = static_cast<unsigned long>(proc.pid);
            proc.serverPorts = server_detector_.portsForPid(pid);
            proc.isServer = !proc.serverPorts.isEmpty();

            proc.gpuPercent = 0.0;
            proc.vramMB = 0.0;
            proc.isGpu = false;

            const qulonglong pid_key = static_cast<qulonglong>(proc.pid);
            if (gpu_usage.contains(pid_key)) {
                proc.gpuPercent = gpu_usage.value(pid_key);
                proc.isGpu = true;
            }
            if (vram_usage.contains(pid_key)) {
                proc.vramMB = vram_usage.value(pid_key);
                proc.isGpu = true;
            }
        }

        const bool log_process = Logger::isVerbose();
        if (log_process || !stats_log_started_ || stats_log_timer_.elapsed() >= stats_log_interval_ms_) {
            int server_count = 0;
            int gpu_count = 0;
            const ProcessInfo *top_cpu = nullptr;
            const ProcessInfo *top_ram = nullptr;

            for (const auto &proc : cached_processes_) {
                if (proc.isServer) {
                    ++server_count;
                }
                if (proc.isGpu) {
                    ++gpu_count;
                }
                if (!top_cpu || proc.cpuPercent > top_cpu->cpuPercent) {
                    top_cpu = &proc;
                }
                if (!top_ram || proc.ramMB > top_ram->ramMB) {
                    top_ram = &proc;
                }
            }

            Logger::info("process", QString("count=%1 topCpu=%2 %3% topRam=%4 %5MB gpuTagged=%6 servers=%7")
                .arg(cached_processes_.size())
                .arg(top_cpu ? top_cpu->name : "-")
                .arg(top_cpu ? top_cpu->cpuPercent : 0.0, 0, 'f', 1)
                .arg(top_ram ? top_ram->name : "-")
                .arg(top_ram ? top_ram->ramMB : 0.0, 0, 'f', 0)
                .arg(gpu_count)
                .arg(server_count));
        }

        emit processesUpdated(cached_processes_);
    }

    if (net_updates_enabled_) {
        if (!net_process_timer_started_) {
            net_process_timer_.start();
            net_process_timer_started_ = true;
            net_process_initial_update_ = false;
        }
        if (!net_process_initial_update_ || net_process_timer_.elapsed() >= net_process_interval_ms_) {
            net_process_timer_.restart();
            net_process_initial_update_ = true;
            process_collector_.updateNetworkOnly(net_processes_);
            emit netProcessesUpdated(net_processes_);
        }
    }

    stats_.capabilities = caps;
    bool log_stats = Logger::isVerbose();
    if (!stats_log_started_) {
        stats_log_timer_.start();
        stats_log_started_ = true;
        log_stats = true;
    } else if (stats_log_timer_.elapsed() >= stats_log_interval_ms_) {
        stats_log_timer_.restart();
        log_stats = true;
    }

    if (log_stats) {
        Logger::info("stats", QString("cpu=%1 cpuTemp=%2 cpuPower=%3 ram=%4/%5 gpu=%6 vram=%7/%8 diskR=%9 diskW=%10 netRx=%11 netTx=%12 servers=%13 drives=%14 caps[gpuLoad=%15 gpuTemp=%16 vram=%17 perGpu=%18 perVram=%19 nvmlOn=%20 nvmlAvail=%21 backend=%22 vendor=%23]")
            .arg(stats_.cpuPercent, 0, 'f', 1)
            .arg(stats_.cpuTempC, 0, 'f', 0)
            .arg(stats_.cpuPowerW, 0, 'f', 0)
            .arg(stats_.ramUsedGB, 0, 'f', 1)
            .arg(stats_.ramTotalGB, 0, 'f', 1)
            .arg(stats_.gpuLoad, 0, 'f', 1)
            .arg(stats_.vramUsedGB, 0, 'f', 1)
            .arg(stats_.vramTotalGB, 0, 'f', 1)
            .arg(stats_.diskReadMBs, 0, 'f', 1)
            .arg(stats_.diskWriteMBs, 0, 'f', 1)
            .arg(stats_.netRecvMBs, 0, 'f', 2)
            .arg(stats_.netSentMBs, 0, 'f', 2)
            .arg(stats_.serverPoints.size())
            .arg(formatDriveSummary(stats_.drives))
            .arg(stats_.capabilities.gpuLoadAvailable ? "1" : "0")
            .arg(stats_.capabilities.gpuTempAvailable ? "1" : "0")
            .arg(stats_.capabilities.vramAvailable ? "1" : "0")
            .arg(stats_.capabilities.perProcessGpuAvailable ? "1" : "0")
            .arg(stats_.capabilities.perProcessVramAvailable ? "1" : "0")
            .arg(stats_.capabilities.nvmlEnabled ? "1" : "0")
            .arg(stats_.capabilities.nvmlAvailable ? "1" : "0")
            .arg(stats_.capabilities.gpuBackend.isEmpty() ? "-" : stats_.capabilities.gpuBackend)
            .arg(stats_.capabilities.gpuVendor.isEmpty() ? "-" : stats_.capabilities.gpuVendor));
    }
    emit statsUpdated(stats_);
}
