#pragma once

#include <QString>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#endif

/**
 * @class VirtualMonitorDriver
 * @brief Handles low-level Windows driver operations for the IddSampleDriver (Virtual Display).
 * 
 * This class encapsulates SetupAPI and CfgMgr32 logic to install, enable, disable,
 * and check the status of the virtual monitor driver.
 */
class VirtualMonitorDriver {
public:
    static bool isDriverInstalled();
    static bool isDriverEnabled();
    static bool setDriverEnabled(bool enable);
    
    static bool hasBundledDriver();
    static bool installBundledDriver();

private:
#ifdef _WIN32
    static bool createRootDevice(const wchar_t *hwid);
    static bool findDevice(bool present_only, HDEVINFO &info, SP_DEVINFO_DATA &data);
    static bool toggleDevice(bool enable);
#endif

    static const wchar_t* kHardwareId;
    static const char* kBundledRelPath;
};
