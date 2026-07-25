#include "services/weather_service.h"
#include "utils/logger.h"
#include <QDate>
#include <QTime>

// Ensure we include the client header as it's forward declared in service header
#include "api/weather_api_client.h" 
#include "utils/sun_calculator.h" 

WeatherService::WeatherService(QObject *parent)
    : QObject(parent), location_manager_(new LocationManager(this)), api_client_(new WeatherApiClient(this)) {
    
    connect(location_manager_, &LocationManager::locationUpdated, this, &WeatherService::fetchWeather);
    connect(location_manager_, &LocationManager::errorOccurred, this, &WeatherService::errorOccurred);
    
    // Connect API Client Signals
    connect(api_client_, &WeatherApiClient::shortTermFetched, this, [this](const WeatherData &data, const QString &baseTime){
        Q_UNUSED(baseTime);
        // Merge partial data
        current_data_.temp_current = data.temp_current;
        current_data_.sky_code = data.sky_code;
        current_data_.icon_code = data.icon_code;
        current_data_.wind_speed = data.wind_speed;
        current_data_.humidity = data.humidity;
        current_data_.precipitation_prob = data.precipitation_prob;
        current_data_.precipitation_amount = data.precipitation_amount;
        current_data_.hourly_list = data.hourly_list;
        
        // Daily list 1-2
        if (data.daily_list.size() >= 2) {
            // Ensure size
             while(current_data_.daily_list.size() < 2) current_data_.daily_list.append(DailyItem());
             current_data_.daily_list[0] = data.daily_list[0];
             current_data_.daily_list[1] = data.daily_list[1];
        }

        short_term_done_ = true;
        checkAllDone();
    });

    connect(api_client_, &WeatherApiClient::midLandFetched, this, [this](const QVector<DailyItem> &items){
        // items should be Day 3, 4, 5, 6
        int startIndex = 2; 
        for(int i=0; i<items.size(); ++i) {
            if (startIndex + i < current_data_.daily_list.size()) {
                current_data_.daily_list[startIndex + i].icon = items[i].icon;
                current_data_.daily_list[startIndex + i].day = items[i].day;
                current_data_.daily_list[startIndex + i].weather_type = items[i].weather_type;
            } else {
                current_data_.daily_list.append(items[i]);
            }
        }
        mid_land_done_ = true;
        checkAllDone();
    });

    connect(api_client_, &WeatherApiClient::midTempFetched, this, [this](const QVector<DailyItem> &items){
        // items should be Day 3, 4, 5, 6
        int startIndex = 2;
        for(int i=0; i<items.size(); ++i) {
             if (startIndex + i < current_data_.daily_list.size()) {
                current_data_.daily_list[startIndex + i].min_temp = items[i].min_temp;
                current_data_.daily_list[startIndex + i].max_temp = items[i].max_temp;
             }
        }
        mid_temp_done_ = true;
        checkAllDone();
    });

    connect(api_client_, &WeatherApiClient::warningsFetched, this, [this](const QStringList &warns){
        current_data_.active_warnings = warns;
        warning_done_ = true;
        checkAllDone();
    });

    connect(api_client_, &WeatherApiClient::apiError, this, [this](const QString &step, const QString &msg){
        Logger::error("weather", QString("API Error in %1: %2").arg(step, msg));
        // Mark as done anyway to not block UI update
        if (step == "ShortTerm") short_term_done_ = true;
        else if (step == "MidLand") mid_land_done_ = true;
        else if (step == "MidTemp") mid_temp_done_ = true;
        else if (step == "Warning") warning_done_ = true;
        else if (step == "UltraSrtNcst") ultra_srt_done_ = true;
        else if (step == "AirQuality") air_quality_done_ = true;
        checkAllDone();
    });

    // New Connections
    connect(api_client_, &WeatherApiClient::ultraSrtNcstFetched, this, [this](const WeatherData &data){
        // Update Live Data (Prioritize over ShortTerm Forecast)
        current_data_.temp_current = data.temp_current;
        current_data_.humidity = data.humidity;
        current_data_.wind_speed = data.wind_speed;
        current_data_.precipitation_amount = data.precipitation_amount;
        
        // Only update Sky/Icon if it is raining/snowing in Real-time
        // If it is NOT raining in real-time, we keep the Forecast's Sky (Sunny/Cloudy)
        // because Ncst doesn't provide Sky condition (only PTY).
        if (data.sky_code == "Rain/Snow") {
             current_data_.sky_code = data.sky_code;
             current_data_.icon_code = data.icon_code;
        }
        
        ultra_srt_done_ = true;
        checkAllDone();
    });

    connect(api_client_, &WeatherApiClient::airQualityFetched, this, [this](const QString &pm10, const QString &pm25){
        current_data_.fine_dust = pm10;
        current_data_.ultra_fine_dust = pm25;
        air_quality_done_ = true;
        checkAllDone();
    });
    
    // Initial load
    location_manager_->loadSavedLocation();
}

void WeatherService::fetchWeather(const LocationConfig &loc) {
    current_data_.location = loc.name;
    
    short_term_done_ = false;
    ultra_srt_done_ = false;
    mid_land_done_ = false;
    mid_temp_done_ = false;
    warning_done_ = false;
    air_quality_done_ = false;
    
    // Reset Daily List to placeholders
    current_data_.daily_list.clear();
    QDate today = QDate::currentDate();
    for(int i=1; i<=6; ++i) { // Up to 6 days
        DailyItem d;
        d.day = today.addDays(i).toString("ddd");
        d.icon = QString::fromUtf16(u"\ue518");
        d.min_temp = -99;
        d.max_temp = -99;
        current_data_.daily_list.append(d);
    }

    api_client_->fetchShortTerm(loc);
    api_client_->fetchUltraSrtNcst(loc); // Live
    api_client_->fetchAirQuality(loc.name); // Fine Dust
    
    // Mid Term Params
    QString tmFc;
    QTime now = QTime::currentTime();
    if (now.hour() < 6) tmFc = today.addDays(-1).toString("yyyyMMdd") + "1800";
    else if (now.hour() < 18) tmFc = today.toString("yyyyMMdd") + "0600";
    else tmFc = today.toString("yyyyMMdd") + "1800";
    
    api_client_->fetchMidLand(loc, tmFc);
    api_client_->fetchMidTemp(loc, tmFc);
    api_client_->fetchWarnings(loc);
}

void WeatherService::checkAllDone() {
    if (short_term_done_ && mid_land_done_ && mid_temp_done_ && warning_done_ && ultra_srt_done_ && air_quality_done_) {
        emit weatherUpdated(current_data_);
    }
}

void WeatherService::detectLocation() {
    if (location_manager_) location_manager_->detectLocation();
}

LocationConfig WeatherService::currentLocation() const {
    return location_manager_ ? location_manager_->currentLocation() : LocationConfig();
}

void WeatherService::saveLocation(const LocationConfig &loc) {
    if (location_manager_) location_manager_->saveLocation(loc);
}

void WeatherService::registerCustomLocation(const QString &name, double lat, double lon) {
    if (location_manager_) location_manager_->registerCustomLocation(name, lat, lon);
}
