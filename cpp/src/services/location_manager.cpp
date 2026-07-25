#include "services/location_manager.h"
#include "utils/geo_utils.h"
#include "utils/logger.h"

#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

LocationManager::LocationManager(QObject *parent)
    : QObject(parent), manager_(new QNetworkAccessManager(this)) {
}

void LocationManager::loadSavedLocation() {
    QSettings settings("MonitorWidget", "Weather");
    
    QString name = settings.value("location/name", "서울").toString();
    int nx = settings.value("location/nx", 60).toInt();
    int ny = settings.value("location/ny", 127).toInt();
    QString regIdLand = settings.value("location/regIdLand", "11B00000").toString();
    QString regIdTemp = settings.value("location/regIdTemp", "11B10101").toString();
    int stnId = settings.value("location/stnId", 108).toInt();
    
    current_location_ = LocationConfig(name, nx, ny, regIdLand, regIdTemp, stnId);
    Logger::info("location", "Loaded saved location: " + name);
    
    emit locationUpdated(current_location_);
}

void LocationManager::saveLocation(const LocationConfig &loc) {
    QSettings settings("MonitorWidget", "Weather");
    
    settings.setValue("location/name", loc.name);
    settings.setValue("location/nx", loc.nx);
    settings.setValue("location/ny", loc.ny);
    settings.setValue("location/regIdLand", loc.regIdLand);
    settings.setValue("location/regIdTemp", loc.regIdTemp);
    settings.setValue("location/stnId", loc.stnId);
    
    current_location_ = loc;
    Logger::info("location", "Saved location: " + loc.name);
    
    emit locationUpdated(current_location_);
}

void LocationManager::detectLocation() {
    Logger::info("location", "Detecting location via IP...");
    QUrl url("http://ip-api.com/json/?fields=status,message,city,lat,lon");
    QNetworkRequest req(url);
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        onIpLocationReply(reply);
    });
}

void LocationManager::onIpLocationReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred("Location Detection Failed: " + reply->errorString());
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();
    if (obj["status"].toString() == "success") {
        QString city = obj["city"].toString();
        Logger::info("location", "IP City detected: " + city);
        
        // Try to match city name to our list
        auto cities = GeoUtils::getAvailableCities();
        LocationConfig best = cities[0]; // Seoul default
        for (const auto &c : cities) {
            if (city.contains(c.name, Qt::CaseInsensitive)) {
                best = c;
                break;
            }
        }
        
        saveLocation(best); // Save determined location
    } else {
         emit errorOccurred("Location detection service error");
    }
}

void LocationManager::registerCustomLocation(const QString &name, double lat, double lon) {
    auto [nx, ny] = GeoUtils::latLonToGrid(lat, lon);
    QString regIdLand = GeoUtils::findRegIdForCoords(nx, ny);
    
    // regIdTemp는 정확한 매핑이 필요하지만, 일단 Land와 동일하게
    QString regIdTemp = regIdLand;
    if (regIdLand == "11B00000") regIdTemp = "11B10101"; // 서울
    
    // stnId는 가장 가까운 도시에서 찾기
    int stnId = 108; // Default: Seoul
    auto cities = GeoUtils::getAvailableCities();
    int minDist = INT_MAX;
    for (const auto &c : cities) {
        int dist = (c.nx - nx) * (c.nx - nx) + (c.ny - ny) * (c.ny - ny);
        if (dist < minDist) {
            minDist = dist;
            stnId = c.stnId;
        }
    }
    
    LocationConfig loc(name, nx, ny, regIdLand, regIdTemp, stnId);
    saveLocation(loc);
    
    Logger::info("location", QString("Registered custom location: %1 (lat=%2, lon=%3 -> nx=%4, ny=%5)")
        .arg(name).arg(lat).arg(lon).arg(nx).arg(ny));
}
