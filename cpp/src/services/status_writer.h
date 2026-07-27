#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>

#include "models/system_stats.h"

// ContextHub(ProcessAppMonitor)가 3초 주기로 폴링하는 status.json 발행기.
// 스키마는 ContextHub.Core/Models/AppStatus.cs + TrayAction.cs를 따른다.
class StatusWriter : public QObject {
    Q_OBJECT

public:
    explicit StatusWriter(QObject *parent = nullptr);

    void start();

public slots:
    void updateStats(const SystemStats &stats);

private slots:
    void writeRunning();
    void writeStopped();

private:
    void writeAtomic(const QByteArray &payload);
    QString filePath() const;

    QTimer timer_;
    SystemStats stats_;
    bool has_stats_ = false;
    bool logged_write_error_ = false;
};
