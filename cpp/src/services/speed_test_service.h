#pragma once

#include <QObject>
#include <QUrl>

#include "models/speed_test_result.h"

class QThread;

class SpeedTestService : public QObject {
    Q_OBJECT

public:
    explicit SpeedTestService(QObject *parent = nullptr);
    ~SpeedTestService() override;

    bool isRunning() const;

public slots:
    void start();
    void cancel();

signals:
    void progress(const QString &phase, int percent, double currentMbps, double maxMbps);
    void finished(const SpeedTestResult &result);

private:
    class Worker;
    Worker *worker_ = nullptr;
    QThread *thread_ = nullptr;
};
