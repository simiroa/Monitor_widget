#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include "models/weather_data.h"

class LocationManager : public QObject {
    Q_OBJECT

public:
    explicit LocationManager(QObject *parent = nullptr);
    
    // Core Actions
    void detectLocation(); // Auto detect via IP
    void saveLocation(const LocationConfig &loc); // Save to QSettings
    void loadSavedLocation(); // Load from QSettings
    void registerCustomLocation(const QString &name, double lat, double lon);
    
    // Accessor
    LocationConfig currentLocation() const { return current_location_; }

signals:
    void locationUpdated(const LocationConfig &loc);
    void errorOccurred(const QString &message);

private:
    void onIpLocationReply(QNetworkReply *reply);
    
    QNetworkAccessManager *manager_;
    LocationConfig current_location_;
};
