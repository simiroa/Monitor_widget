#pragma once

#include <unordered_map>
#include <unordered_set>

#include <windows.h>

#include "models/process_info.h"
#include "collectors/net_process_collector.h"

class ProcessCollector {
public:
    ProcessCollector();
    void update(QVector<ProcessInfo> &processes);
    void updateNetworkOnly(QVector<ProcessInfo> &processes);

private:
    static unsigned long long fileTimeToULL(const FILETIME &ft);
    static QString basenameFromPath(const QString &path);

    bool has_last_ = false;
    unsigned long long last_total_time_ = 0;
    std::unordered_map<unsigned long, unsigned long long> last_proc_times_;
    
    // I/O counters: pid -> (lastRead, lastWrite)
    std::unordered_map<unsigned long, std::pair<unsigned long long, unsigned long long>> last_io_counters_;
    NetProcessCollector net_collector_;
};
