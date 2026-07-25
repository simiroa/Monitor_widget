#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

struct ProcessInfo {
    qint64 pid = 0;
    QString name;
    QString exePath;
    double cpuPercent = 0.0;
    double ramMB = 0.0;
    double vramMB = 0.0;
    double gpuPercent = 0.0;
    double diskIoMBs = 0.0;
    bool isServer = false;
    QVector<quint16> serverPorts;
    bool isLeakSuspect = false;
    bool isGpu = false;
    bool isWhitelisted = false;
    
    // Network additions
    int checkConnectionCount = 0; // Active connections
    double netReadMBs = 0.0;
    double netWriteMBs = 0.0;
    QVector<QString> remoteEndpoints; // Top remote IPs for tooltip
};

Q_DECLARE_METATYPE(ProcessInfo)
