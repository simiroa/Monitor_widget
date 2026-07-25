#pragma once

#include <QObject>
#include <QByteArray>
#include <QLocalSocket>
#include <QProcess>
#include <QTimer>

#include "models/capability_flags.h"
#include "models/system_stats.h"

struct SensorHostSnapshot {
    qint64 timestampMs = 0;
    double cpuTempC = 0.0;
    bool cpuTempValid = false;
    double cpuPowerW = 0.0;
    bool cpuPowerValid = false;
    double gpuTempC = 0.0;
    bool gpuTempValid = false;
    double gpuPowerW = 0.0;
    bool gpuPowerValid = false;
    double gpuClockMHz = 0.0;
    bool gpuClockValid = false;
    QString cpuName;
    QString gpuName;
};

class SensorHostClient : public QObject {
    Q_OBJECT

public:
    explicit SensorHostClient(QObject *parent = nullptr);

    void start();
    void stop();
    void apply(SystemStats &stats, CapabilityFlags &caps);

private slots:
    void handleReadyRead();
    void handleConnected();
    void handleDisconnected();
    void handleSocketError(QLocalSocket::LocalSocketError error);
    void handleProcessError(QProcess::ProcessError error);
    void handleProcessFinished(int exit_code, QProcess::ExitStatus status);
    void ensureConnected();

private:
    void parseLine(const QByteArray &line);
    QString resolveHostPath() const;
    void startProcess(const QString &path);

    QLocalSocket socket_;
    QProcess process_;
    QTimer reconnect_timer_;
    QByteArray buffer_;
    SensorHostSnapshot snapshot_;
    QString pipe_name_;
    QString host_path_;
    bool started_ = false;
    bool logged_missing_host_ = false;
    bool logged_socket_error_ = false;
    bool logged_process_error_ = false;
    bool logged_connected_ = false;
    bool logged_started_ = false;
};
