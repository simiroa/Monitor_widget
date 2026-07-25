#pragma once

#include <QPair>
#include <QList>
#include <QString>
#include "models/weather_data.h"

namespace GeoUtils {

    // Helper to convert Lat/Lon to Grid
    QPair<int, int> latLonToGrid(double lat, double lon);

    // Get list of predefined Korean cities
    QList<LocationConfig> getAvailableCities();

    // Helper to find Region ID based on grid coordinates
    QString findRegIdForCoords(int nx, int ny);

} // namespace GeoUtils
