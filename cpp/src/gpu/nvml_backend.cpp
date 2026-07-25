#include "gpu/nvml_backend.h"

#include <vector>

#include <QString>

#include "utils/logger.h"

namespace {
constexpr int kNvmlSuccess = 0;
constexpr int kNvmlErrorInsufficientSize = 7;
constexpr unsigned int kNvmlTempGpu = 0;
constexpr unsigned int kNvmlClockGraphics = 0;
constexpr double kBytesToGB = 1.0 / (1024.0 * 1024.0 * 1024.0);
constexpr double kBytesToMB = 1.0 / (1024.0 * 1024.0);
constexpr unsigned long long kNvmlValueNotAvailable = 0xFFFFFFFFFFFFFFFFull;
}

NvmlBackend::NvmlBackend() = default;

NvmlBackend::~NvmlBackend() {
    shutdown();
}

bool NvmlBackend::initialize() {
    if (available_) {
        return true;
    }

    if (!loadLibrary() || !resolveSymbols()) {
        if (!nvml_module_ && !logged_load_failure_) {
            Logger::warn("gpu.nvml", "nvml.dll not found.");
            logged_load_failure_ = true;
        } else if (nvml_module_ && !logged_symbol_failure_) {
            Logger::warn("gpu.nvml", "nvml symbols missing.");
            logged_symbol_failure_ = true;
        }
        unloadLibrary();
        return false;
    }

    const int init_status = nvmlInit_v2_();
    if (init_status != kNvmlSuccess) {
        if (!logged_init_failure_) {
            Logger::warn("gpu.nvml", QString("nvmlInit failed status=%1.").arg(init_status));
            logged_init_failure_ = true;
        }
        unloadLibrary();
        return false;
    }

    unsigned int count = 0;
    const int count_status = nvmlDeviceGetCount_v2_(&count);
    if (count_status != kNvmlSuccess || count == 0) {
        if (!logged_count_failure_) {
            Logger::warn("gpu.nvml", QString("nvmlDeviceGetCount failed status=%1 count=%2.").arg(count_status).arg(count));
            logged_count_failure_ = true;
        }
        shutdown();
        return false;
    }

    const int handle_status = nvmlDeviceGetHandleByIndex_v2_(0, &device_);
    if (handle_status != kNvmlSuccess) {
        if (!logged_device_failure_) {
            Logger::warn("gpu.nvml", QString("nvmlDeviceGetHandleByIndex failed status=%1.").arg(handle_status));
            logged_device_failure_ = true;
        }
        shutdown();
        return false;
    }

    available_ = true;
    if (!logged_available_) {
        Logger::info("gpu.nvml", "NVML initialized.");
        logged_available_ = true;
    }
    return true;
}

void NvmlBackend::shutdown() {
    if (available_ && nvmlShutdown_) {
        nvmlShutdown_();
    }
    available_ = false;
    device_ = nullptr;
    unloadLibrary();
}

void NvmlBackend::update(SystemStats &stats, CapabilityFlags &caps) {
    if (!available_ && !initialize()) {
        return;
    }

    stats.gpuName.clear();
    stats.gpuLoad = 0.0;
    stats.gpuTempC = 0.0;
    stats.vramPercent = 0.0;
    stats.vramUsedGB = 0.0;
    stats.vramTotalGB = 0.0;
    stats.gpuClockMHz = 0.0;
    stats.gpuPowerW = 0.0;

    bool name_ok = false;
    bool load_ok = false;
    bool temp_ok = false;
    bool vram_ok = false;
    bool clock_ok = false;
    bool power_ok = false;

    char name_buf[96] = {};
    if (nvmlDeviceGetName_(device_, name_buf, sizeof(name_buf)) == kNvmlSuccess) {
        stats.gpuName = QString::fromUtf8(name_buf).trimmed();
        name_ok = !stats.gpuName.isEmpty();
    }

    nvmlUtilization_t util{};
    if (nvmlDeviceGetUtilizationRates_(device_, &util) == kNvmlSuccess) {
        stats.gpuLoad = static_cast<double>(util.gpu);
        load_ok = true;
    }

    unsigned int temp = 0;
    if (nvmlDeviceGetTemperature_(device_, kNvmlTempGpu, &temp) == kNvmlSuccess) {
        stats.gpuTempC = static_cast<double>(temp);
        temp_ok = true;
    }

    nvmlMemory_t mem{};
    if (nvmlDeviceGetMemoryInfo_(device_, &mem) == kNvmlSuccess) {
        stats.vramTotalGB = static_cast<double>(mem.total) * kBytesToGB;
        stats.vramUsedGB = static_cast<double>(mem.used) * kBytesToGB;
        if (stats.vramTotalGB > 0.0) {
            stats.vramPercent = (stats.vramUsedGB / stats.vramTotalGB) * 100.0;
        } else {
            stats.vramPercent = 0.0;
        }
        vram_ok = stats.vramTotalGB > 0.0;
    }

    unsigned int clock_mhz = 0;
    if (nvmlDeviceGetClockInfo_(device_, kNvmlClockGraphics, &clock_mhz) == kNvmlSuccess) {
        stats.gpuClockMHz = static_cast<double>(clock_mhz);
        clock_ok = true;
    }

    unsigned int power_mw = 0;
    if (nvmlDeviceGetPowerUsage_(device_, &power_mw) == kNvmlSuccess) {
        stats.gpuPowerW = static_cast<double>(power_mw) / 1000.0;
        power_ok = true;
    }

    caps.gpuNameAvailable = name_ok;
    caps.gpuLoadAvailable = load_ok;
    caps.gpuTempAvailable = temp_ok;
    caps.vramAvailable = vram_ok;
    caps.gpuClockAvailable = clock_ok;
    caps.gpuPowerAvailable = power_ok;
    caps.gpuBackend = "NVML";
    caps.gpuVendor = "NVIDIA";
}

bool NvmlBackend::queryProcessMemory(QHash<qulonglong, double> &vram_mb, QSet<qulonglong> &pids) const {
    if (!available_ || !device_) {
        return false;
    }
    if (!nvmlDeviceGetGraphicsRunningProcesses_v2_ && !nvmlDeviceGetComputeRunningProcesses_v2_) {
        return false;
    }

    auto collect = [&](nvmlDeviceGetGraphicsRunningProcesses_v2_t fn) {
        if (!fn) {
            return false;
        }
        unsigned int count = 0;
        int status = fn(device_, &count, nullptr);
        if (status != kNvmlSuccess && status != kNvmlErrorInsufficientSize) {
            return false;
        }
        if (count == 0) {
            return true;
        }
        std::vector<nvmlProcessInfo_t> infos(count);
        status = fn(device_, &count, infos.data());
        if (status == kNvmlErrorInsufficientSize) {
            infos.resize(count);
            status = fn(device_, &count, infos.data());
        }
        if (status != kNvmlSuccess) {
            return false;
        }
        for (unsigned int i = 0; i < count; ++i) {
            const auto &info = infos[i];
            if (info.pid == 0) {
                continue;
            }
            pids.insert(static_cast<qulonglong>(info.pid));
            if (info.usedGpuMemory != kNvmlValueNotAvailable) {
                const double mb = static_cast<double>(info.usedGpuMemory) * kBytesToMB;
                vram_mb[static_cast<qulonglong>(info.pid)] += mb;
            }
        }
        return true;
    };

    bool ok = false;
    ok |= collect(nvmlDeviceGetGraphicsRunningProcesses_v2_);
    ok |= collect(nvmlDeviceGetComputeRunningProcesses_v2_);
    return ok;
}

bool NvmlBackend::loadLibrary() {
    if (nvml_module_) {
        return true;
    }

    nvml_module_ = LoadLibraryW(L"nvml.dll");
    return nvml_module_ != nullptr;
}

void NvmlBackend::unloadLibrary() {
    if (nvml_module_) {
        FreeLibrary(nvml_module_);
        nvml_module_ = nullptr;
    }
}

bool NvmlBackend::resolveSymbols() {
    nvmlInit_v2_ = reinterpret_cast<nvmlInit_v2_t>(GetProcAddress(nvml_module_, "nvmlInit_v2"));
    nvmlShutdown_ = reinterpret_cast<nvmlShutdown_t>(GetProcAddress(nvml_module_, "nvmlShutdown"));
    nvmlDeviceGetCount_v2_ = reinterpret_cast<nvmlDeviceGetCount_v2_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetCount_v2"));
    nvmlDeviceGetHandleByIndex_v2_ = reinterpret_cast<nvmlDeviceGetHandleByIndex_v2_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetHandleByIndex_v2"));
    nvmlDeviceGetName_ = reinterpret_cast<nvmlDeviceGetName_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetName"));
    nvmlDeviceGetUtilizationRates_ = reinterpret_cast<nvmlDeviceGetUtilizationRates_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetUtilizationRates"));
    nvmlDeviceGetTemperature_ = reinterpret_cast<nvmlDeviceGetTemperature_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetTemperature"));
    nvmlDeviceGetMemoryInfo_ = reinterpret_cast<nvmlDeviceGetMemoryInfo_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetMemoryInfo"));
    nvmlDeviceGetClockInfo_ = reinterpret_cast<nvmlDeviceGetClockInfo_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetClockInfo"));
    nvmlDeviceGetPowerUsage_ = reinterpret_cast<nvmlDeviceGetPowerUsage_t>(GetProcAddress(nvml_module_, "nvmlDeviceGetPowerUsage"));
    nvmlDeviceGetComputeRunningProcesses_v2_ = reinterpret_cast<nvmlDeviceGetComputeRunningProcesses_v2_t>(
        GetProcAddress(nvml_module_, "nvmlDeviceGetComputeRunningProcesses_v2"));
    nvmlDeviceGetGraphicsRunningProcesses_v2_ = reinterpret_cast<nvmlDeviceGetGraphicsRunningProcesses_v2_t>(
        GetProcAddress(nvml_module_, "nvmlDeviceGetGraphicsRunningProcesses_v2"));

    return nvmlInit_v2_ && nvmlShutdown_ && nvmlDeviceGetCount_v2_ &&
           nvmlDeviceGetHandleByIndex_v2_ && nvmlDeviceGetName_ &&
           nvmlDeviceGetUtilizationRates_ && nvmlDeviceGetTemperature_ &&
           nvmlDeviceGetMemoryInfo_ && nvmlDeviceGetClockInfo_ &&
           nvmlDeviceGetPowerUsage_;
}
