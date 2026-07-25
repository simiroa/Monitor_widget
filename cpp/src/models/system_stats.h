#pragma once

#include <QMetaType>
#include <QString>
#include <QVector>

#include "capability_flags.h"

struct DriveInfo {
    QString mountpoint;
    double totalGB = 0.0;
    double usedGB = 0.0;
    double freeGB = 0.0;
    double percent = 0.0;
    double readMBs = 0.0;
    double writeMBs = 0.0;
    double activeTime = 0.0;
    bool ioValid = false;
};

struct ServerPoint {
    quint16 port = 0;
    QString name;
    qint64 pid = 0;
    QString serverType;
    QString exePath;
};

struct SystemStats {
    double cpuPercent = 0.0;
    double cpuTempC = 0.0;
    double cpuPowerW = 0.0;
    
    // New CPU details
    double l1CacheMB = 0.0;
    double l2CacheMB = 0.0;
    double l3CacheMB = 0.0;
    int coreCount = 0;
    int threadCount = 0;

    double ramPercent = 0.0;
    double ramUsedGB = 0.0;
    double ramTotalGB = 0.0;
    double ramClockMHz = 0.0; // Added for detailed memory info

    double gpuLoad = 0.0;
    double gpuTempC = 0.0;
    double vramPercent = 0.0;
    double vramUsedGB = 0.0;
    double vramTotalGB = 0.0;
    double gpuClockMHz = 0.0;
    double gpuPowerW = 0.0;
    QString gpuName;

    double diskReadMBs = 0.0;
    double diskWriteMBs = 0.0;
    QVector<DriveInfo> drives;

    double netRecvMBs = 0.0;
    double netSentMBs = 0.0;

    QVector<ServerPoint> serverPoints;
    CapabilityFlags capabilities;
};

Q_DECLARE_METATYPE(SystemStats)
