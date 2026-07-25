#pragma once

#include "models/system_stats.h"

class NetCollector {
public:
    NetCollector();
    void update(SystemStats &stats);

private:
    bool has_last_ = false;
    unsigned long long last_rx_ = 0;
    unsigned long long last_tx_ = 0;
    long long last_time_ms_ = 0;
};
