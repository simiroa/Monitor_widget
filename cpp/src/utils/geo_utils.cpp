#include "utils/geo_utils.h"

#include <QtMath>

namespace GeoUtils {

QPair<int, int> latLonToGrid(double lat, double lon) {
    const double RE = 6371.00877;     // 지구 반경 (km)
    const double GRID = 5.0;          // 격자 간격 (km)
    const double SLAT1 = 30.0;        // 표준 위도 1
    const double SLAT2 = 60.0;        // 표준 위도 2
    const double OLON = 126.0;        // 기준점 경도
    const double OLAT = 38.0;         // 기준점 위도
    const double XO = 43;             // 기준점 X
    const double YO = 136;            // 기준점 Y
    
    const double DEGRAD = M_PI / 180.0;
    
    double re = RE / GRID;
    double slat1 = SLAT1 * DEGRAD;
    double slat2 = SLAT2 * DEGRAD;
    double olon = OLON * DEGRAD;
    double olat = OLAT * DEGRAD;
    
    double sn = qTan(M_PI * 0.25 + slat2 * 0.5) / qTan(M_PI * 0.25 + slat1 * 0.5);
    sn = qLn(qCos(slat1) / qCos(slat2)) / qLn(sn);
    double sf = qTan(M_PI * 0.25 + slat1 * 0.5);
    sf = qPow(sf, sn) * qCos(slat1) / sn;
    double ro = qTan(M_PI * 0.25 + olat * 0.5);
    ro = re * sf / qPow(ro, sn);
    
    double ra = qTan(M_PI * 0.25 + lat * DEGRAD * 0.5);
    ra = re * sf / qPow(ra, sn);
    double theta = lon * DEGRAD - olon;
    if (theta > M_PI) theta -= 2.0 * M_PI;
    if (theta < -M_PI) theta += 2.0 * M_PI;
    theta *= sn;
    
    int nx = static_cast<int>(ra * qSin(theta) + XO + 0.5);
    int ny = static_cast<int>(ro - ra * qCos(theta) + YO + 0.5);
    
    return {nx, ny};
}

QList<LocationConfig> getAvailableCities() {
    return {
        // 특별시/광역시
        {"서울", 60, 127, "11B00000", "11B10101", 108},
        {"부산", 98, 76, "11H20000", "11H20201", 159},
        {"인천", 55, 124, "11B00000", "11B20201", 112},
        {"대구", 89, 90, "11H10000", "11H10701", 143},
        {"대전", 67, 100, "11C20000", "11C20401", 133},
        {"광주", 58, 74, "11F20000", "11F20501", 156},
        {"울산", 102, 84, "11H20000", "11H20101", 152},
        {"세종", 66, 103, "11C20000", "11C20404", 133},
        // 경기도
        {"수원", 60, 121, "11B00000", "11B20601", 119},
        {"성남", 62, 123, "11B00000", "11B20605", 119},
        {"고양", 57, 128, "11B00000", "11B20302", 108},
        {"용인", 64, 119, "11B00000", "11B20612", 119},
        {"안양", 59, 123, "11B00000", "11B20602", 119},
        {"안산", 52, 121, "11B00000", "11B20604", 119},
        {"일산", 56, 129, "11B00000", "11B20302", 108},
        {"파주", 56, 131, "11B00000", "11B20305", 108},
        // 경상도
        {"창원", 90, 77, "11H20000", "11H20301", 155},
        {"포항", 102, 94, "11H10000", "11H10201", 138},
        {"김해", 95, 77, "11H20000", "11H20304", 159},
        {"구미", 84, 96, "11H10000", "11H10501", 143},
        {"경주", 100, 91, "11H10000", "11H10202", 138},
        // 충청도
        {"청주", 69, 107, "11C10000", "11C10301", 131},
        {"천안", 63, 110, "11C20000", "11C20301", 232},
        {"충주", 76, 114, "11C10000", "11C10102", 127},
        // 전라도
        {"전주", 63, 89, "11F10000", "11F10201", 146},
        {"익산", 60, 91, "11F10000", "11F10202", 146},
        {"목포", 50, 67, "11F20000", "11F20401", 165},
        {"순천", 70, 70, "11F20000", "11F20602", 156},
        {"여수", 73, 66, "11F20000", "11F20601", 168},
        // 강원도
        {"춘천", 73, 134, "11D10000", "11D10301", 101},
        {"원주", 76, 122, "11D10000", "11D10401", 114},
        {"강릉", 92, 131, "11D20000", "11D20501", 105},
        {"속초", 87, 141, "11D20000", "11D20401", 90},
        // 제주도
        {"제주시", 53, 38, "11G00000", "11G00201", 184},
        {"서귀포", 52, 33, "11G00000", "11G00401", 189}
    };
}

QString findRegIdForCoords(int nx, int ny) {
    if (ny < 50) return "11G00000"; // 제주
    if (nx > 85 && ny < 95) return "11H20000"; // 경남/부산
    if (nx > 80 && ny >= 85) return "11H10000"; // 경북/대구
    if (ny > 120 && nx > 75) return "11D20000"; // 강원영동
    if (ny > 120) return "11D10000"; // 강원영서
    if (nx < 55 && ny < 85) return "11F20000"; // 전남
    if (ny < 95) return "11F10000"; // 전북
    if (ny < 115 && nx < 65) return "11C20000"; // 충남
    if (ny < 120 && nx >= 65) return "11C10000"; // 충북
    return "11B00000"; // 경기/서울
}

} // namespace GeoUtils
