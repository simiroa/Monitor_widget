#pragma once

#include <unordered_map>

#include "models/system_stats.h"

class ServerDetector {
public:
    ServerDetector();
    void update();
    QVector<ServerPoint> buildServerPoints() const;
    QVector<quint16> portsForPid(unsigned long pid) const;
    bool isServer(unsigned long pid) const;

private:
    QString detectServerType(const QString &name, const QString &exe_path, const QVector<quint16> &ports) const;
    bool isBlacklisted(const QString &name) const;
    static QString basenameFromPath(const QString &path);

    std::unordered_map<unsigned long, QVector<quint16>> port_cache_;
    unsigned long long last_update_ms_ = 0;
    unsigned int update_interval_ms_ = 5000;
};
