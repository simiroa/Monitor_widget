#pragma once

#include <windows.h>

#include <QHash>
#include <QSet>

#include "gpu/gpu_backend.h"

class NvmlBackend : public GpuBackend {
public:
    NvmlBackend();
    ~NvmlBackend() override;

    bool initialize() override;
    void shutdown() override;
    void update(SystemStats &stats, CapabilityFlags &caps) override;
    bool isAvailable() const override { return available_; }
    bool queryProcessMemory(QHash<qulonglong, double> &vram_mb, QSet<qulonglong> &pids) const;

private:
    bool loadLibrary();
    void unloadLibrary();
    bool resolveSymbols();

    HMODULE nvml_module_ = nullptr;
    bool available_ = false;

    using nvmlReturn_t = int;
    using nvmlDevice_t = void *;

    struct nvmlUtilization_t {
        unsigned int gpu;
        unsigned int memory;
    };

    struct nvmlMemory_t {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };

    struct nvmlProcessInfo_t {
        unsigned int pid;
        unsigned long long usedGpuMemory;
        unsigned int gpuInstanceId;
        unsigned int computeInstanceId;
    };

    using nvmlInit_v2_t = nvmlReturn_t (*)(void);
    using nvmlShutdown_t = nvmlReturn_t (*)(void);
    using nvmlDeviceGetCount_v2_t = nvmlReturn_t (*)(unsigned int *);
    using nvmlDeviceGetHandleByIndex_v2_t = nvmlReturn_t (*)(unsigned int, nvmlDevice_t *);
    using nvmlDeviceGetName_t = nvmlReturn_t (*)(nvmlDevice_t, char *, unsigned int);
    using nvmlDeviceGetUtilizationRates_t = nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t *);
    using nvmlDeviceGetTemperature_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int, unsigned int *);
    using nvmlDeviceGetMemoryInfo_t = nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t *);
    using nvmlDeviceGetClockInfo_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int, unsigned int *);
    using nvmlDeviceGetPowerUsage_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int *);
    using nvmlDeviceGetComputeRunningProcesses_v2_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_t *);
    using nvmlDeviceGetGraphicsRunningProcesses_v2_t = nvmlReturn_t (*)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_t *);

    nvmlInit_v2_t nvmlInit_v2_ = nullptr;
    nvmlShutdown_t nvmlShutdown_ = nullptr;
    nvmlDeviceGetCount_v2_t nvmlDeviceGetCount_v2_ = nullptr;
    nvmlDeviceGetHandleByIndex_v2_t nvmlDeviceGetHandleByIndex_v2_ = nullptr;
    nvmlDeviceGetName_t nvmlDeviceGetName_ = nullptr;
    nvmlDeviceGetUtilizationRates_t nvmlDeviceGetUtilizationRates_ = nullptr;
    nvmlDeviceGetTemperature_t nvmlDeviceGetTemperature_ = nullptr;
    nvmlDeviceGetMemoryInfo_t nvmlDeviceGetMemoryInfo_ = nullptr;
    nvmlDeviceGetClockInfo_t nvmlDeviceGetClockInfo_ = nullptr;
    nvmlDeviceGetPowerUsage_t nvmlDeviceGetPowerUsage_ = nullptr;
    nvmlDeviceGetComputeRunningProcesses_v2_t nvmlDeviceGetComputeRunningProcesses_v2_ = nullptr;
    nvmlDeviceGetGraphicsRunningProcesses_v2_t nvmlDeviceGetGraphicsRunningProcesses_v2_ = nullptr;

    nvmlDevice_t device_ = nullptr;
    bool logged_available_ = false;
    bool logged_load_failure_ = false;
    bool logged_symbol_failure_ = false;
    bool logged_init_failure_ = false;
    bool logged_device_failure_ = false;
    bool logged_count_failure_ = false;
};
