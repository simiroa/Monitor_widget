#pragma once

#include <unordered_map>
#include <map>
#include <utility>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

#include <QString>
#include <QVector>

struct NetProcessInfo {
    int connectionCount = 0;
    double readMBs = 0.0;
    double writeMBs = 0.0;
    QVector<QString> endpoints;
};

class NetProcessCollector {
public:
    bool collect(std::unordered_map<unsigned long, NetProcessInfo> &out);

private:
    std::map<QString, std::pair<ULONG64, ULONG64>> last_conn_stats_;
    std::chrono::steady_clock::time_point last_time_;
    bool has_last_ = false;
    std::map<qint64, double> last_read_speeds_;
    std::map<qint64, double> last_write_speeds_;
};
