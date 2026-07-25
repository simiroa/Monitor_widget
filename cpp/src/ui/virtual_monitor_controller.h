#pragma once

#include <QObject>

class MonitorControlService;
class QTimer;

class VirtualMonitorController : public QObject {
    Q_OBJECT

public:
    VirtualMonitorController(MonitorControlService *service, QObject *parent = nullptr);

    void handleDisplayChange();
    void setAutoEnabled(bool enabled);

private slots:
    void handleDebounce();

private:
    MonitorControlService *service_ = nullptr;
    QTimer *debounce_timer_ = nullptr;
    bool auto_enabled_ = false;
};
