#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QTimer>

#include "collectors/cpu_mem_collector.h"
#include "collectors/d3dkmt_process_collector.h"
#include "collectors/disk_collector.h"
#include "collectors/drive_collector.h"
#include "collectors/dxcore_process_collector.h"
#include "collectors/gpu_collector.h"
#include "collectors/gpu_process_collector.h"
#include "collectors/net_collector.h"
#include "collectors/process_collector.h"
#include "collectors/server_detector.h"
#include "models/process_info.h"
#include "models/system_stats.h"
#include "services/sensor_host_client.h"

class StatsEngine : public QObject {
    Q_OBJECT

public:
    explicit StatsEngine(QObject *parent = nullptr);

    bool isRunning() const;

signals:
    void statsUpdated(const SystemStats &stats);
    void processesUpdated(const QVector<ProcessInfo> &processes);
    void netProcessesUpdated(const QVector<ProcessInfo> &processes);

public slots:
    void start();
    void stop();
    void setNetProcessUpdatesEnabled(bool enabled);

private slots:
    void updateOnce();

private:
    QTimer timer_;
    bool running_ = false;
    SystemStats stats_;
    QVector<ProcessInfo> cached_processes_;
    QVector<ProcessInfo> net_processes_;
    QElapsedTimer process_timer_;
    bool process_timer_started_ = false;
    bool process_initial_update_ = false;
    int process_interval_ms_ = 5000;
    QElapsedTimer net_process_timer_;
    bool net_process_timer_started_ = false;
    bool net_process_initial_update_ = false;
    int net_process_interval_ms_ = 2000;
    bool net_updates_enabled_ = false;
    QElapsedTimer drive_timer_;
    bool drive_timer_started_ = false;
    int drive_interval_ms_ = 10000;
    QElapsedTimer server_timer_;
    bool server_timer_started_ = false;
    int server_interval_ms_ = 5000;
    QElapsedTimer stats_log_timer_;
    bool stats_log_started_ = false;
    int stats_log_interval_ms_ = 10000;

    CpuMemCollector cpu_mem_;
    DiskCollector disk_;
    NetCollector net_;
    DriveCollector drive_;
    GpuCollector gpu_;
    GpuProcessCollector gpu_process_;
    DxcoreProcessCollector dxcore_process_;
    D3dkmtProcessCollector d3dkmt_process_;
    ProcessCollector process_collector_;
    ServerDetector server_detector_;
    SensorHostClient sensor_host_;
};
