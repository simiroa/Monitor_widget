#pragma once

#include <QDateTime>
#include <QtMath>

class SunCalculator {
public:
    struct SunTimes {
        QTime sunrise;
        QTime sunset;
        bool isValid = false;
    };

    // Simple implementation of Solar Time calculation
    // Based on NOAA formulas or simplified versions suitable for UI visuals
    static SunTimes calculate(const QDate &date, double lat, double lon) {
        SunTimes times;
        
        int doy = date.dayOfYear();
        
        // Convert to radians
        double latRad = lat * M_PI / 180.0;
        
        // Declination of the sun
        // approx formula: 23.45 * sin(360/365 * (doy - 81))
        double declination = 23.45 * qSin((360.0 / 365.0) * (doy - 81) * M_PI / 180.0);
        double declRad = declination * M_PI / 180.0;
        
        // Equation of time (minutes)
        // EQT = 9.87 * sin(2B) - 7.53 * cos(B) - 1.5 * sin(B)
        // B = 360/365 * (doy - 81)
        double B = (360.0 / 365.0) * (doy - 81) * M_PI / 180.0;
        double eot = 9.87 * qSin(2 * B) - 7.53 * qCos(B) - 1.5 * qSin(B);
        
        // Hour angle
        // cos(H) = -tan(lat) * tan(decl)
        double cosH = -qTan(latRad) * qTan(declRad);
        
        if (cosH < -1.0 || cosH > 1.0) {
            // Polar day or night
            return times; // Invalid
        }
        
        double H = qAcos(cosH) * 180.0 / M_PI; // degrees
        
        // H is half-day duration in degrees. 15 degrees = 1 hour.
        double daylightMinutes = H * 4.0; // minutes
        
        // Solar Noon
        // noon = 12:00 - (Lon - 135) * 4 - Eot (For Korea Standard Time GMT+9 -> 135 deg reference?)
        // KST is GMT+9, which is 135 degrees East.
        // If we use standard formula: NoonUTC = 12:00 - Lon/15 - Eot/60
        // NoonLocal = NoonUTC + Offset
        // Simplified for KST (Timezone 9):
        // SolarNoon = 12:00 + (135 - Lon) * 4 minutes - Eot
        
        double solarNoonMins = 720.0 + (135.0 - lon) * 4.0 - eot;
        
        double sunriseMins = solarNoonMins - daylightMinutes;
        double sunsetMins = solarNoonMins + daylightMinutes;
        
        times.sunrise = minsToTime(sunriseMins);
        times.sunset = minsToTime(sunsetMins);
        times.isValid = true;
        
        return times;
    }

private:
    static QTime minsToTime(double mins) {
        int h = static_cast<int>(mins / 60.0);
        int m = static_cast<int>(mins) % 60;
        if (h < 0) h += 24;
        if (h >= 24) h -= 24;
        return QTime(h, m);
    }
};
