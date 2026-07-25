#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct HourlyItem {
    QString time; // "12 PM"
    int temp;
    QString icon;
    int weather_type = 0; // 0: Normal, 1: Rain, 2: Snow
};

struct DailyItem {
    QString day; // "Tue", "Wed"
    QString icon;
    int min_temp;
    int max_temp;
    int weather_type = 0; // 0: Sunny, 1: Cloudy, 2: Rain, 3: Snow
};

struct WeatherData {
    // Current
    QString temp_current;
    QString sky_code; // 1:sunny, 3:cloudy, 4:overcast
    QString pty_code; // 0:none, 1:rain, 2:rain/snow, 3:snow, 4:shower
    QString icon_code; // Mapped code
    
    // New Fields for Redesign
    QString location = "Seoul"; 
    QString wind_speed;         // WSD
    QString precipitation_prob; // POP
    QString precipitation_amount; // PCP
    QString humidity;           // REH
    QString fine_dust;          // PM10
    QString ultra_fine_dust;    // PM2.5
    
    QString sunrise_time;
    QString sunset_time;
    
    QStringList active_warnings;

    QVector<HourlyItem> hourly_list;
    QVector<DailyItem> daily_list;
};

struct LocationConfig {
    QString name;
    int nx;
    int ny;
    QString regIdLand;
    QString regIdTemp;
    int stnId; // Station ID for Warning API
    
    LocationConfig() {}
    LocationConfig(QString n, int x, int y, QString rl, QString rt, int s) 
        : name(n), nx(x), ny(y), regIdLand(rl), regIdTemp(rt), stnId(s) {}
};
