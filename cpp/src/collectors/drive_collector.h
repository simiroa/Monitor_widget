#pragma once

#include <map>
#include <pdh.h>

#include "models/system_stats.h"

class DriveCollector {
public:
    DriveCollector();
    ~DriveCollector();
    void update(SystemStats &stats);

private:
    bool initPdh();
    void clearCounters();
    void ensureCountersForDrive(const QString &mountpoint);

    PDH_HQUERY query_ = nullptr;
   
    struct DriveCounters {
        PDH_HCOUNTER readCounter = nullptr;
        PDH_HCOUNTER writeCounter = nullptr;
        PDH_HCOUNTER activeCounter = nullptr;
    };

    std::map<QString, DriveCounters> counters_;
};
