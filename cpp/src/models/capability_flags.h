#pragma once

#include <QString>

struct CapabilityFlags {
    bool cpuTempAvailable = false;
    bool cpuPowerAvailable = false;
    bool gpuNameAvailable = false;
    bool gpuLoadAvailable = false;
    bool gpuTempAvailable = false;
    bool vramAvailable = false;
    bool gpuClockAvailable = false;
    bool gpuPowerAvailable = false;
    bool nvmlEnabled = false;
    bool nvmlAvailable = false;
    bool perProcessGpuAvailable = false;
    bool perProcessVramAvailable = false;
    QString gpuBackend;
    QString gpuVendor;
};
