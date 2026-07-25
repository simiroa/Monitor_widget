#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVector>
#include <QString>
#include <QStringList>

#include "api/weather_api_client.h"
#include "services/location_manager.h"

// Forward
class WeatherApiClient;

class WeatherService : public QObject {
    Q_OBJECT

public:
    explicit WeatherService(QObject *parent = nullptr);
    void fetchWeather(const LocationConfig &loc);
    
    // Delegated methods
    void detectLocation(); 
    void saveLocation(const LocationConfig &loc);
    void registerCustomLocation(const QString &name, double lat, double lon);
    
    LocationConfig currentLocation() const; 

signals:
    void weatherUpdated(const WeatherData &data);
    void errorOccurred(const QString &message);

private:
    void checkAllDone();

    LocationManager *location_manager_; 
    WeatherApiClient *api_client_;
    
    WeatherData current_data_;
    bool short_term_done_ = false;
    bool mid_land_done_ = false;
    bool mid_temp_done_ = false;
    bool warning_done_ = false;
    bool ultra_srt_done_ = false;
    bool air_quality_done_ = false;
};
