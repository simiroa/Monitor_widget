#include "ui/virtual_monitor_controller.h"

#include <QTimer>

#include "services/monitor_control_service.h"
#include "utils/logger.h"

VirtualMonitorController::VirtualMonitorController(MonitorControlService *service, QObject *parent)
    : QObject(parent), service_(service) {
    debounce_timer_ = new QTimer(this);
    debounce_timer_->setSingleShot(true);
    debounce_timer_->setInterval(2000);
    connect(debounce_timer_, &QTimer::timeout, this, &VirtualMonitorController::handleDebounce);
}

void VirtualMonitorController::setAutoEnabled(bool enabled) {
    auto_enabled_ = enabled;
}

void VirtualMonitorController::handleDisplayChange() {
    if (!auto_enabled_ || !service_) {
        return;
    }
    if (!debounce_timer_->isActive()) {
        debounce_timer_->start();
    }
}

void VirtualMonitorController::handleDebounce() {
    if (!auto_enabled_ || !service_) {
        return;
    }

    const int physicalCount = service_->activePhysicalMonitorCount();
    Logger::info("ui.main", QString("VM Debounce check: physical=%1").arg(physicalCount));

    if (physicalCount == 0) {
        if (!service_->isVirtualMonitorEnabled()) {
            service_->setVirtualMonitorState(true);
            Logger::info("ui.main", "Auto-activated Virtual Monitor due to all monitors off.");
        }
    } else {
        // Physical monitors are present.
        // 1. Save this layout as a "Good" state?
        // Actually, if VM is currently ON, and physical monitors just appeared, the current layout is "Physical + VM".
        // We DON'T want to save this "Mixed" layout. We want to restore the layout from BEFORE VM was enabled.
        // But if VM is OFF, then this is a pure physical layout. We should save it as the latest known good configuration.
        
        if (!service_->isVirtualMonitorEnabled()) {
             // VM is OFF, Physical is ON. This is a clean state. Save it.
             service_->saveCurrentLayout();
        } else {
             // VM is ON, Physical is ON (Just came back).
             // Disable VM.
             service_->setVirtualMonitorState(false);
             Logger::info("ui.main", "Auto-disabled Virtual Monitor due to monitor recovery.");
             
             // Layout restoration is now handled inside setVirtualMonitorState(false).
        }
    }
}
