#pragma once

#include <QString>

namespace Icons {
    // Material Symbols Outlined Unicode Constants
    constexpr const char16_t* kClose = u"\ue5cd";
    constexpr const char16_t* kSearch = u"\ue8b6";
    constexpr const char16_t* kDesktop = u"\ue30b";
    constexpr const char16_t* kLan = u"\ueb2f";
    constexpr const char16_t* kKill = u"\ue872";
    constexpr const char16_t* kPower = u"\uea0b";
    constexpr const char16_t* kTemp = u"\ue8d1";
    constexpr const char16_t* kSpeed = u"\ue9e4";
    constexpr const char16_t* kMemory = u"\ue322";
    constexpr const char16_t* kStorage = u"\ue1db";
    constexpr const char16_t* kSettings = u"\ue8b8";
    constexpr const char16_t* kRefresh = u"\ue5d5";
    constexpr const char16_t* kOpenInNew = u"\ue89e";
    constexpr const char16_t* kPause = u"\ue034";
    constexpr const char16_t* kPlay = u"\ue037";
    constexpr const char16_t* kCpu = u"\ue322"; // memory
    constexpr const char16_t* kRam = u"\ue245"; // data_usage
    constexpr const char16_t* kGpu = u"\ue30f"; // developer_board
    constexpr const char16_t* kVram = u"\ue071"; // video_label
    constexpr const char16_t* kDisk = u"\ue1db"; // storage
    constexpr const char16_t* kNet = u"\ueb2f"; // lan
    constexpr const char16_t* kDns = u"\ue875"; // dns
    constexpr const char16_t* kCleanup = u"\uf0ff"; // cleaning_services
    constexpr const char16_t* kArrowDown = u"\ue5db"; // arrow_downward
    constexpr const char16_t* kArrowUp = u"\ue5d8"; // arrow_upward
    constexpr const char16_t* kLanguage = u"\ue894"; // language
    constexpr const char16_t* kComputer = u"\ue30a"; // computer
    constexpr const char16_t* kRouter = u"\ue328"; // router
    constexpr const char16_t* kCheckBox = u"\ue834";
    constexpr const char16_t* kCheckBoxOutlineBlank = u"\ue835";
    constexpr const char16_t* kHome = u"\ue88a"; // home
    constexpr const char16_t* kWeatherSunny = u"\ue81a"; // sunny
    constexpr const char16_t* kWeatherPartlyCloudyDay = u"\uf172"; // partly_cloudy_day
    constexpr const char16_t* kWeatherCloud = u"\ue2bd"; // cloud
    constexpr const char16_t* kWeatherRain = u"\uf176"; // rainy
    constexpr const char16_t* kWeatherRainSnow = u"\uf61d"; // rainy_snow
    constexpr const char16_t* kWeatherSnow = u"\ue80f"; // snowing
    constexpr const char16_t* kWeatherSnowflake = u"\ueb3b"; // ac_unit (snowflake)
    constexpr const char16_t* kWeatherThunder = u"\uebdb"; // thunderstorm
    constexpr const char16_t* kWeatherFog = u"\ue818"; // foggy
    constexpr const char16_t* kWeatherWind = u"\uec0c"; // wind_power
    constexpr const char16_t* kWeatherHumidity = u"\uf87e"; // humidity_percentage
    constexpr const char16_t* kWeatherPrecip = u"\ue798"; // water_drop
    constexpr const char16_t* kWeatherDust = u"\uefd8"; // air
    constexpr const char16_t* kWeatherWarning = u"\ue8b2"; // warning

    inline QString fontName() { return "Material Symbols Outlined"; }
}
