#pragma once

#include "models/capability_flags.h"
#include "models/system_stats.h"

class GpuBackend {
public:
    virtual ~GpuBackend() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual void update(SystemStats &stats, CapabilityFlags &caps) = 0;
    virtual bool isAvailable() const = 0;
};
