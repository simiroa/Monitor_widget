#pragma once

#include <pdh.h>

#include "models/system_stats.h"

class DiskCollector {
public:
    DiskCollector();
    ~DiskCollector();

    void update(SystemStats &stats);
    bool isAvailable() const { return available_; }

private:
    bool initPdh();

    bool available_ = false;
    PDH_HQUERY query_ = nullptr;
    PDH_HCOUNTER read_counter_ = nullptr;
    PDH_HCOUNTER write_counter_ = nullptr;
};
