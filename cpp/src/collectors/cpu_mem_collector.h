#pragma once

#include <windows.h>

#include "models/capability_flags.h"
#include "models/system_stats.h"

class CpuMemCollector {
public:
    CpuMemCollector();
    void update(SystemStats &stats, CapabilityFlags &caps);

private:
    unsigned long long fileTimeToULL(const _FILETIME &ft);

    unsigned long long last_idle_ = 0;
    unsigned long long last_kernel_ = 0;
    unsigned long long last_user_ = 0;
    bool has_last_ = false;
    double cached_ram_mhz_ = 0.0;
    
    double cached_l1_mb_ = 0.0;
    double cached_l2_mb_ = 0.0;
    double cached_l3_mb_ = 0.0;
    int cached_cores_ = 0;
    int cached_threads_ = 0;
};
