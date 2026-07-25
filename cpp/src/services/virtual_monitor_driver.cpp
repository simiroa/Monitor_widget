#include "services/virtual_monitor_driver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>
#include <newdev.h>
#endif

#include "utils/logger.h"

const wchar_t* VirtualMonitorDriver::kHardwareId = L"MttVDD";
const char* VirtualMonitorDriver::kBundledRelPath = "VirtualDisplayDriver/MttVDD.inf";

#ifdef _WIN32
static bool deviceIdMatches(const wchar_t *device_id) {
    if (!device_id) return false;
    // Matching logic for MttVDD
    const QString id = QString::fromWCharArray(device_id);
    return id.contains(L"MttVDD", Qt::CaseInsensitive);
}
#endif

bool VirtualMonitorDriver::isDriverInstalled() {
#ifdef _WIN32
    HDEVINFO info = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA data = { sizeof(SP_DEVINFO_DATA) };
    bool found = findDevice(false, info, data);
    if (info != INVALID_HANDLE_VALUE) {
        SetupDiDestroyDeviceInfoList(info);
    }
    return found;
#else
    return false;
#endif
}

bool VirtualMonitorDriver::isDriverEnabled() {
#ifdef _WIN32
    HDEVINFO info = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA data = { sizeof(SP_DEVINFO_DATA) };
    if (!findDevice(true, info, data)) {
        return false;
    }

    DWORD status = 0, problem = 0;
    bool enabled = false;
    if (CM_Get_DevNode_Status(&status, &problem, data.DevInst, 0) == CR_SUCCESS) {
        // Driver is "enabled" if it's started and doesn't have a problem
        enabled = (status & DN_STARTED) && !(status & DN_HAS_PROBLEM);
    }

    SetupDiDestroyDeviceInfoList(info);
    return enabled;
#else
    return false;
#endif
}

bool VirtualMonitorDriver::setDriverEnabled(bool enable) {
#ifdef _WIN32
    if (enable && !isDriverInstalled()) {
        if (!installBundledDriver()) return false;
    }
    return toggleDevice(enable);
#else
    Q_UNUSED(enable);
    return false;
#endif
}

bool VirtualMonitorDriver::hasBundledDriver() {
    QString path = QCoreApplication::applicationDirPath() + "/" + kBundledRelPath;
    return QFileInfo::exists(path);
}

bool VirtualMonitorDriver::installBundledDriver() {
#ifdef _WIN32
    QString inf_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/" + kBundledRelPath);
    if (!QFileInfo::exists(inf_path)) return false;

    if (!createRootDevice(kHardwareId)) return false;

    BOOL reboot = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(nullptr, kHardwareId, 
        reinterpret_cast<LPCWSTR>(inf_path.utf16()), INSTALLFLAG_FORCE, &reboot)) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

#ifdef _WIN32
bool VirtualMonitorDriver::createRootDevice(const wchar_t *hwid) {
    HDEVINFO info = SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_DISPLAY, nullptr);
    if (info == INVALID_HANDLE_VALUE) return false;

    SP_DEVINFO_DATA data = { sizeof(SP_DEVINFO_DATA) };
    data.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfoW(info, L"MttVDD", &GUID_DEVCLASS_DISPLAY, nullptr, nullptr, DICD_GENERATE_ID, &data)) {
        SetupDiDestroyDeviceInfoList(info);
        return false;
    }

    const wchar_t hwids[] = L"Root\\MttVDD\0";
    if (!SetupDiSetDeviceRegistryPropertyW(info, &data, SPDRP_HARDWAREID, reinterpret_cast<const BYTE*>(hwids), sizeof(hwids))) {
        SetupDiDestroyDeviceInfoList(info);
        return false;
    }

    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, info, &data)) {
        SetupDiDestroyDeviceInfoList(info);
        return false;
    }

    SetupDiDestroyDeviceInfoList(info);
    return true;
}

bool VirtualMonitorDriver::findDevice(bool present_only, HDEVINFO &info, SP_DEVINFO_DATA &data) {
    info = SetupDiGetClassDevs(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | (present_only ? DIGCF_PRESENT : 0));
    if (info == INVALID_HANDLE_VALUE) return false;

    data.cbSize = sizeof(SP_DEVINFO_DATA);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(info, i, &data); ++i) {
        wchar_t id[MAX_DEVICE_ID_LEN];
        if (CM_Get_Device_IDW(data.DevInst, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            if (deviceIdMatches(id)) return true;
        }
        data.cbSize = sizeof(SP_DEVINFO_DATA);
    }
    SetupDiDestroyDeviceInfoList(info);
    info = INVALID_HANDLE_VALUE;
    return false;
}

bool VirtualMonitorDriver::toggleDevice(bool enable) {
    HDEVINFO info = INVALID_HANDLE_VALUE;
    SP_DEVINFO_DATA data = { sizeof(SP_DEVINFO_DATA) };
    if (!findDevice(false, info, data)) return false;

    SP_PROPCHANGE_PARAMS params = {sizeof(SP_PROPCHANGE_PARAMS)};
    params.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER);
    params.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
    params.StateChange = enable ? DICS_ENABLE : DICS_DISABLE;
    params.Scope = DICS_FLAG_GLOBAL;

    bool ok = false;
    if (SetupDiSetClassInstallParams(info, &data, &params.ClassInstallHeader, sizeof(params))) {
        if (SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, info, &data)) {
            ok = true;
        }
    }
    SetupDiDestroyDeviceInfoList(info);
    return ok;
}
#endif
