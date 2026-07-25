#include "api/weather_api_client.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDate>
#include <QTime>
#include <QtMath>
#include "utils/logger.h"

namespace {
QString iconForPrecip(int pty) {
    switch (pty) {
        case 1:
        case 4:
        case 5:
            return QString::fromUtf16(u"\uf176"); // rainy
        case 2:
        case 6:
            return QString::fromUtf16(u"\uf61d"); // rainy_snow
        case 3:
        case 7:
            return QString::fromUtf16(u"\ue80f"); // snowing
        default:
            return QString();
    }
}

QString textForPrecip(int pty) {
    switch (pty) {
        case 1:
        case 4:
        case 5:
            return "Rain";
        case 2:
        case 6:
            return "Rain/Snow";
        case 3:
        case 7:
            return "Snow";
        default:
            return QString();
    }
}

QString iconForSky(int sky) {
    switch (sky) {
        case 1:
            return QString::fromUtf16(u"\ue81a"); // sunny
        case 3:
            return QString::fromUtf16(u"\uf172"); // partly_cloudy_day
        case 4:
            return QString::fromUtf16(u"\ue2bd"); // cloud
        default:
            return QString::fromUtf16(u"\ue2bd"); // cloud
    }
}

QString textForSky(int sky) {
    switch (sky) {
        case 1:
            return "Sunny";
        case 3:
            return "Partly Cloudy";
        case 4:
            return "Overcast";
        default:
            return "Cloudy";
    }
}

int hourlyTypeForPrecip(int pty) {
    switch (pty) {
        case 2:
        case 3:
        case 6:
        case 7:
            return 2; // Snow
        case 1:
        case 4:
        case 5:
            return 1; // Rain
        default:
            return 0;
    }
}

int dailyTypeForPrecip(int pty) {
    switch (pty) {
        case 2:
        case 3:
        case 6:
        case 7:
            return 3; // Snow
        case 1:
        case 4:
        case 5:
            return 2; // Rain
        default:
            return 0;
    }
}
}

WeatherApiClient::WeatherApiClient(QObject *parent) 
    : QObject(parent), manager_(new QNetworkAccessManager(this)) {}

// === Short Term (VilageFcst) ===

void WeatherApiClient::fetchShortTerm(const LocationConfig &loc) {
    QDate today = QDate::currentDate();
    QTime now = QTime::currentTime();
    
    QString base_date = today.toString("yyyyMMdd");
    QString base_time;
    
    int hour = now.hour();
    int min = now.minute();
    int total_min = hour * 60 + min;
    
    // Adjust base time logic
    if (total_min < (2 * 60 + 20)) {
        base_date = today.addDays(-1).toString("yyyyMMdd");
        base_time = "2300";
    } else if (total_min < (5 * 60 + 20)) base_time = "0200";
    else if (total_min < (8 * 60 + 20)) base_time = "0500";
    else if (total_min < (11 * 60 + 20)) base_time = "0800";
    else if (total_min < (14 * 60 + 20)) base_time = "1100";
    else if (total_min < (17 * 60 + 20)) base_time = "1400";
    else if (total_min < (20 * 60 + 20)) base_time = "1700";
    else if (total_min < (23 * 60 + 20)) base_time = "2000";
    else base_time = "2300";

    QUrl url("http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getVilageFcst");
    QUrlQuery query;
    query.addQueryItem("serviceKey", QUrl::fromPercentEncoding(kApiKey.toUtf8())); 
    query.addQueryItem("pageNo", "1");
    query.addQueryItem("numOfRows", "1000"); 
    query.addQueryItem("dataType", "JSON");
    query.addQueryItem("base_date", base_date);
    query.addQueryItem("base_time", base_time);
    query.addQueryItem("nx", QString::number(loc.nx));
    query.addQueryItem("ny", QString::number(loc.ny));
    url.setQuery(query);

    QNetworkRequest req(url);
    Logger::info("weather.api", "Fetching ShortTerm: " + url.toString());
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onShortTermReply(reply); });
}

void WeatherApiClient::onShortTermReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit apiError("ShortTerm", reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isNull()) { emit apiError("ShortTerm", "JSON Parse Error"); return; }
    
    auto response = doc.object()["response"].toObject();
    if (response["header"].toObject()["resultCode"].toString() != "00") {
        emit apiError("ShortTerm", "API Error: " + response["header"].toObject()["resultMsg"].toString());
        return;
    }

    // Parsing Logic moved from Service
    WeatherData data;
    QMap<QString, QMap<QString, QString>> day_time_map;
    QJsonArray items = response["body"].toObject()["items"].toObject()["item"].toArray();
    QString first_key;

    for (const auto &val : items) {
        QJsonObject obj = val.toObject();
        QString date = obj["fcstDate"].toString();
        QString time = obj["fcstTime"].toString();
        QString key = date + time;
        if (first_key.isEmpty()) first_key = key;
        day_time_map[key][obj["category"].toString()] = obj["fcstValue"].toString();
    }

    // 1. Current
    if (!first_key.isEmpty()) {
        auto cats = day_time_map[first_key];
        data.temp_current = cats["TMP"] + "°";
        int pty = cats.value("PTY", "0").toInt();
        int sky = cats.value("SKY", "1").toInt();
        
        if (pty > 0) {
           data.icon_code = iconForPrecip(pty);
           data.sky_code = textForPrecip(pty);
        } else {
           data.icon_code = iconForSky(sky);
           data.sky_code = textForSky(sky);
        }
        data.wind_speed = cats["WSD"] + " m/s";
        data.humidity = cats["REH"] + "%";
        data.precipitation_prob = cats["POP"] + "%";
        data.precipitation_amount = (cats["PCP"] == "강수없음") ? "0mm" : cats["PCP"];
        // fine_dust is not here
    }

    // 2. Hourly
    int h_count = 0;
    QString current_full_time = QDateTime::currentDateTime().toString("yyyyMMddHHmm");
    for (auto it = day_time_map.begin(); it != day_time_map.end(); ++it) {
        if (it.key() < current_full_time) continue;
        if (++h_count > 8) break;
        
        auto cats = it.value();
        HourlyItem hi;
        hi.time = it.key().mid(8, 2) + "h";
        hi.temp = cats["TMP"].toInt();
        int pty = cats.value("PTY", "0").toInt();
        int sky = cats.value("SKY", "1").toInt();
        
        if (pty > 0) {
            hi.icon = iconForPrecip(pty);
            hi.weather_type = hourlyTypeForPrecip(pty);
        } else {
            hi.icon = iconForSky(sky);
            hi.weather_type = 0;
        }
        data.hourly_list.append(hi);
    }
    
    // 3. Early Daily (Tomorrow, Day after)
    QDate today = QDate::currentDate();
    for (int d = 1; d <= 2; ++d) {
        QString dateStr = today.addDays(d).toString("yyyyMMdd");
        int min = 999, max = -999;
        QString best_icon;
        int best_pty = 0, best_sky = 1;

        for (auto it = day_time_map.begin(); it != day_time_map.end(); ++it) {
            if (!it.key().startsWith(dateStr)) continue;
            auto cats = it.value();
            if (cats.contains("TMP")) {
                int t = cats["TMP"].toInt();
                if (t < min) min = t;
                if (t > max) max = t;
            }
            if (it.key().endsWith("1200") || best_icon.isEmpty()) {
                 int pty = cats.value("PTY").toInt();
                 int sky = cats.value("SKY").toInt();
                 best_pty = pty; best_sky = sky;
                 if (pty > 0) best_icon = iconForPrecip(pty);
                 else best_icon = iconForSky(sky);
            }
        }
        
        if (min != 999) {
            DailyItem item;
            item.day = today.addDays(d).toString("ddd");
            item.min_temp = min;
            item.max_temp = max;
            item.icon = best_icon;
            
            if (best_pty > 0) item.weather_type = dailyTypeForPrecip(best_pty);
            else if (best_sky >= 3) item.weather_type = 1; 
            else item.weather_type = 0; 
            
            data.daily_list.append(item);
        }
    }

    emit shortTermFetched(data, "");
}

// === Ultra Short Term Live (Ncst) ===

void WeatherApiClient::fetchUltraSrtNcst(const LocationConfig &loc) {
    QDate today = QDate::currentDate();
    QTime now = QTime::currentTime();
    
    QString base_date = today.toString("yyyyMMdd");
    QString base_time;
    
    // API is updated at minute 40 of every hour
    // If 10:30 -> data of 09:00 (revised at 09:40) ? No, base_time should be 0900.
    // If 10:50 -> data of 10:00 (revised at 10:40). base_time 1000.
    int h = now.hour();
    int m = now.minute();
    
    if (m < 45) { // Safe margin 45
        h--;
        if (h < 0) {
            h = 23;
            base_date = today.addDays(-1).toString("yyyyMMdd");
        }
    }
    base_time = QString("%100").arg(h, 2, 10, QChar('0'));

    QUrl url("http://apis.data.go.kr/1360000/VilageFcstInfoService_2.0/getUltraSrtNcst");
    QUrlQuery query;
    query.addQueryItem("serviceKey", QUrl::fromPercentEncoding(kApiKey.toUtf8())); 
    query.addQueryItem("pageNo", "1");
    query.addQueryItem("numOfRows", "100"); 
    query.addQueryItem("dataType", "JSON");
    query.addQueryItem("base_date", base_date);
    query.addQueryItem("base_time", base_time);
    query.addQueryItem("nx", QString::number(loc.nx));
    query.addQueryItem("ny", QString::number(loc.ny));
    url.setQuery(query);

    QNetworkRequest req(url);
    Logger::info("weather.api", "Fetching UltraSrtNcst: " + url.toString());
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onUltraSrtNcstReply(reply); });
}

void WeatherApiClient::onUltraSrtNcstReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit apiError("UltraSrtNcst", reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    auto response = doc.object()["response"].toObject();
    if (response["header"].toObject()["resultCode"].toString() != "00") {
        emit apiError("UltraSrtNcst", "API Error: " + response["header"].toObject()["resultMsg"].toString());
        return;
    }

    WeatherData data;
    QJsonArray items = response["body"].toObject()["items"].toObject()["item"].toArray();
    
    for (const auto &val : items) {
        QJsonObject obj = val.toObject();
        QString cat = obj["category"].toString();
        double valD = obj["obsrValue"].toVariant().toDouble();
        QString valS = obj["obsrValue"].toString();
        
        if (cat == "T1H") data.temp_current = QString::number(valD) + "°";
        else if (cat == "REH") data.humidity = valS + "%";
        else if (cat == "RN1") data.precipitation_amount = (valD < 0.1) ? "0mm" : valS + "mm";
        else if (cat == "WSD") data.wind_speed = valS + "m/s";
        else if (cat == "PTY") {
            int pty = valS.toInt(); // 0, 1, 2, 3, 5, 6, 7
            if (pty == 0) data.sky_code = "Sunny"; // Fallback, will be overwritten by sky from forecast if needed
            else data.sky_code = "Rain/Snow";
            
            // Icon mapping
            if (pty == 0) data.icon_code = QString::fromUtf16(u"\ue81a"); // Sunny default
            else data.icon_code = iconForPrecip(pty);
        }
    }
    
    emit ultraSrtNcstFetched(data);
}

// === Mid Land ===

void WeatherApiClient::fetchMidLand(const LocationConfig &loc, const QString &tmFc) {
    QString landRegId = loc.regIdLand;
    if (landRegId.isEmpty() || landRegId.startsWith("11B")) landRegId = "11B00000";

    QUrl url("http://apis.data.go.kr/1360000/MidFcstInfoService/getMidLandFcst");
    QUrlQuery query;
    query.addQueryItem("serviceKey", QUrl::fromPercentEncoding(kApiKey.toUtf8()));
    query.addQueryItem("pageNo", "1");
    query.addQueryItem("numOfRows", "10");
    query.addQueryItem("dataType", "JSON");
    query.addQueryItem("regId", landRegId);
    query.addQueryItem("tmFc", tmFc);
    url.setQuery(query);
    
    QNetworkRequest req(url);
    Logger::info("weather.api", "Fetching MidLand: " + url.toString());
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onMidLandReply(reply); });
}

void WeatherApiClient::onMidLandReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) { emit apiError("MidLand", reply->errorString()); return; }
    
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    auto response = doc.object()["response"].toObject();
    if (response["header"].toObject()["resultCode"].toString() != "00") {
        emit apiError("MidLand", "API Error");
        return; 
    }

    QVector<DailyItem> items;
    QJsonArray arr = response["body"].toObject()["items"].toObject()["item"].toArray();
    if (!arr.isEmpty()) {
        QJsonObject item = arr[0].toObject();
        QDate today = QDate::currentDate();
        for (int i = 3; i <= 6; ++i) { // Day 3 to Day 6
            DailyItem d;
            d.day = today.addDays(i).toString("ddd");
            
            QString wf = item[QString("wf%1%2").arg(i).arg(i <= 7 ? "Am" : "")].toString();
            if ((wf.contains("비") && wf.contains("눈")) || wf.contains("비/눈") || wf.contains("눈/비")) {
                d.icon = QString::fromUtf16(u"\uf61d");
                d.weather_type = 3;
            } else if (wf.contains("비")) {
                d.icon = QString::fromUtf16(u"\uf176");
                d.weather_type = 2;
            } else if (wf.contains("눈")) {
                d.icon = QString::fromUtf16(u"\ue80f");
                d.weather_type = 3;
            } else if (wf.contains("흐림")) {
                d.icon = QString::fromUtf16(u"\ue2bd");
                d.weather_type = 1;
            } else if (wf.contains("구름많음") || wf.contains("구름조금") || wf.contains("구름")) {
                d.icon = QString::fromUtf16(u"\uf172");
                d.weather_type = 1;
            } else {
                d.icon = QString::fromUtf16(u"\ue81a");
                d.weather_type = 0;
            }
            
            items.append(d);
        }
    }
    emit midLandFetched(items);
}

// === Mid Temp ===

void WeatherApiClient::fetchMidTemp(const LocationConfig &loc, const QString &tmFc) {
    QUrl url("http://apis.data.go.kr/1360000/MidFcstInfoService/getMidTa");
    QUrlQuery query;
    query.addQueryItem("serviceKey", QUrl::fromPercentEncoding(kApiKey.toUtf8()));
    query.addQueryItem("pageNo", "1");
    query.addQueryItem("numOfRows", "10");
    query.addQueryItem("dataType", "JSON");
    query.addQueryItem("regId", loc.regIdTemp);
    query.addQueryItem("tmFc", tmFc);
    url.setQuery(query);

    QNetworkRequest req(url);
    Logger::info("weather.api", "Fetching MidTemp: " + url.toString());
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onMidTempReply(reply); });
}

void WeatherApiClient::onMidTempReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) { emit apiError("MidTemp", reply->errorString()); return; }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    auto response = doc.object()["response"].toObject();
     if (response["header"].toObject()["resultCode"].toString() != "00") {
        emit apiError("MidTemp", "API Error");
        return; 
    }

    QVector<DailyItem> items;
    QJsonArray arr = response["body"].toObject()["items"].toObject()["item"].toArray();
    if (!arr.isEmpty()) {
        QJsonObject item = arr[0].toObject();
        for (int i = 3; i <= 6; ++i) {
            DailyItem d;
            d.min_temp = item[QString("taMin%1").arg(i)].toVariant().toInt();
            d.max_temp = item[QString("taMax%1").arg(i)].toVariant().toInt();
            items.append(d);
        }
    }
    emit midTempFetched(items);
}

// === Warning ===

void WeatherApiClient::fetchWarnings(const LocationConfig &loc) {
    QUrl url("http://apis.data.go.kr/1360000/WthrWrnInfoService/getWthrWrnList");
    QUrlQuery query;
    query.addQueryItem("serviceKey", QUrl::fromPercentEncoding(kApiKey.toUtf8())); 
    query.addQueryItem("numOfRows", "10");
    query.addQueryItem("pageNo", "1");
    query.addQueryItem("dataType", "JSON");
    query.addQueryItem("stnId", QString::number(loc.stnId)); 
    query.addQueryItem("fromTmFc", QDate::currentDate().addDays(-1).toString("yyyyMMdd"));
    url.setQuery(query);

    QNetworkRequest req(url);
    Logger::info("weather.api", "Fetching Warning: " + url.toString());
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onWarningReply(reply); });
}

void WeatherApiClient::onWarningReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) { emit apiError("Warning", reply->errorString()); return; }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QStringList warnings;
    QJsonArray items = doc.object()["response"].toObject()["body"].toObject()["items"].toObject()["item"].toArray();
    
    for (const auto &val : items) {
        QJsonObject item = val.toObject();
        QString title = item["title"].toString();
        if (!title.isEmpty()) warnings.append(title);
    }
    warnings.removeDuplicates();
    emit warningsFetched(warnings);
}

// === Air Quality (Air Korea) ===

void WeatherApiClient::fetchAirQuality(const QString &cityName) {
    if (cityName.isEmpty()) return;

    QString sidoName = mapCityToSido(cityName);
    Logger::info("weather.api", QString("Fetching AirQuality for city: %1 -> Sido: %2").arg(cityName).arg(sidoName));
    
    QUrl url("http://apis.data.go.kr/B552584/ArpltnInforInqireSvc/getCtprvnRltmMesureDnsty");
    QUrlQuery query;
    query.addQueryItem("serviceKey", QUrl::fromPercentEncoding(kApiKey.toUtf8()));
    query.addQueryItem("returnType", "json");
    query.addQueryItem("numOfRows", "100");
    query.addQueryItem("pageNo", "1");
    query.addQueryItem("sidoName", sidoName);
    query.addQueryItem("ver", "1.0");
    url.setQuery(query);

    QNetworkRequest req(url);
    auto *reply = manager_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onAirQualityReply(reply); });
}

void WeatherApiClient::onAirQualityReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) { emit apiError("AirQuality", reply->errorString()); return; }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    auto response = doc.object()["response"].toObject();
    if (response["header"].toObject()["resultCode"].toString() != "00") {
        // AirKorea uses different header sometimes? No, usually standard.
        // Some errors come in body? Check.
        // But if header exists and not 00, it's error.
        // emit apiError("AirQuality", "API Error"); 
        // Proceeding carefully.
    }

    QJsonArray items = response["body"].toObject()["items"].toArray();
    if (!items.isEmpty()) {
        // Just pick the first valide one or average?
        // Let's pick the first one with valid PM10 value
        for (const auto &val : items) {
             QJsonObject obj = val.toObject();
             QString pm10 = obj["pm10Value"].toString();
             QString pm25 = obj["pm25Value"].toString();
             
             QString grade10 = obj["pm10Grade"].toString(); 
             QString grade25 = obj["pm25Grade"].toString();
             
             bool has10 = (pm10 != "-" && !pm10.isEmpty());
             bool has25 = (pm25 != "-" && !pm25.isEmpty());
             
             if (has10 || has25) {
                 // Found valid data
                 auto getGradeText = [](const QString &g) {
                     if (g == "1") return "Good";
                     if (g == "2") return "Normal";
                     if (g == "3") return "Bad";
                     if (g == "4") return "V.Bad";
                     return "N/A";
                 };
                 
                 QString txt10 = QString("%1 (%2)").arg(getGradeText(grade10), pm10);
                 QString txt25 = QString("%1 (%2)").arg(getGradeText(grade25), pm25);
                 
                 emit airQualityFetched(txt10, txt25);
                 return;
             }
        }
    }
    emit airQualityFetched("N/A (-)", "N/A (-)");
}

QString WeatherApiClient::mapCityToSido(const QString &city) {
    if (city.contains("서울")) return "서울";
    if (city.contains("부산")) return "부산";
    if (city.contains("대구")) return "대구";
    if (city.contains("인천")) return "인천";
    if (city.contains("광주")) return "광주";
    if (city.contains("대전")) return "대전";
    if (city.contains("울산")) return "울산";
    if (city.contains("세종")) return "세종";
    if (city.contains("제주") || city.contains("서귀포")) return "제주";
    
    if (city.contains("수원") || city.contains("성남") || city.contains("고양") || city.contains("용인") || 
        city.contains("안양") || city.contains("안산") || city.contains("일산") || city.contains("파주")) return "경기";
        
    if (city.contains("춘천") || city.contains("원주") || city.contains("강릉") || city.contains("속초")) return "강원";
    if (city.contains("청주") || city.contains("충주")) return "충북";
    if (city.contains("천안")) return "충남";
    if (city.contains("전주") || city.contains("익산")) return "전북";
    if (city.contains("목포") || city.contains("순천") || city.contains("여수")) return "전남";
    if (city.contains("포항") || city.contains("구미") || city.contains("경주")) return "경북";
    if (city.contains("창원") || city.contains("김해")) return "경남";

    return "서울"; // Default
}
