#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "models/weather_data.h"

// Forward Declaration
class QNetworkAccessManager;

class WeatherApiClient : public QObject {
    Q_OBJECT

public:
    explicit WeatherApiClient(QObject *parent = nullptr);

    // Main Fetch Functions
    void fetchShortTerm(const LocationConfig &loc);
    void fetchUltraSrtNcst(const LocationConfig &loc); // Ultra Short-term Live
    void fetchMidLand(const LocationConfig &loc, const QString &tmFc);
    void fetchMidTemp(const LocationConfig &loc, const QString &tmFc);
    void fetchWarnings(const LocationConfig &loc);
    void fetchAirQuality(const QString &cityName); // Fine Dust

signals:
    // Success Signals
    void shortTermFetched(const WeatherData &partialData, const QString &baseTime); // Emits partial update or just data
    void ultraSrtNcstFetched(const WeatherData &liveData);
    void midLandFetched(const QVector<DailyItem> &items);
    void midTempFetched(const QVector<DailyItem> &items); // Contains min/max mostly
    void warningsFetched(const QStringList &warnings);
    void airQualityFetched(const QString &pm10, const QString &pm25);

    // Error
    void apiError(const QString &step, const QString &msg);

private slots:
    void onShortTermReply(QNetworkReply *reply);
    void onUltraSrtNcstReply(QNetworkReply *reply);
    void onMidLandReply(QNetworkReply *reply);
    void onMidTempReply(QNetworkReply *reply);
    void onWarningReply(QNetworkReply *reply);
    void onAirQualityReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *manager_;
    const QString kApiKey = "bfdd875687daab9c6fe41eec3ef96d1fbaecc21dc6325639e41d100e7b7b2042"; 
    
    // Helpers
    QString getBaseTime(const QTime &t);
    QString mapCityToSido(const QString &city);
};
