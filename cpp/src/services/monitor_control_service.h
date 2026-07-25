#pragma once

#include <QVector>

#ifdef _WIN32
#include <windows.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#endif

class MonitorControlService {
public:
    MonitorControlService();
    ~MonitorControlService();

    int brightness() const;
    bool setBrightness(int level) const;

    QVector<int> availableRefreshRates() const;
    int currentRefreshRate() const;
    bool setRefreshRate(int rate) const;

    void setNightMode(bool enable);
    bool nightModeEnabled() const { return night_mode_active_; }

    bool hdrSupported() const;
    bool hdrEnabled() const;
    bool setHdrMode(bool enable);

    void turnOffMonitors() const;
    void restore();

private:
    struct MonitorEnumData {
        QVector<PHYSICAL_MONITOR> monitors;
    };

    static BOOL CALLBACK enumMonitorsProc(HMONITOR monitor, HDC dc, LPRECT rect, LPARAM data);
    bool enumerateMonitors(MonitorEnumData &data) const;
    static void destroyMonitors(MonitorEnumData &data);

    void applyGammaRamp(bool night);

    int initial_brightness_ = -1;
    int initial_refresh_rate_ = -1;
    bool night_mode_active_ = false;
};
