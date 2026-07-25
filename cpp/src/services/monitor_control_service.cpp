#include "services/monitor_control_service.h"

#include <algorithm>
#include <mutex>

#include <QElapsedTimer>
#include <QVector>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <oleauto.h>
#include <wbemidl.h>

#include "utils/logger.h"

// No SetupAPI pragmas needed here anymore

namespace {
class WmiConnection {
public:
    WmiConnection() {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == S_OK || hr == S_FALSE) {
            uninit_ = true;
        } else if (hr != RPC_E_CHANGED_MODE) {
            return;
        }

        if (!ensureSecurity()) {
            return;
        }

        IWbemLocator *locator = nullptr;
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<void **>(&locator));
        if (FAILED(hr) || !locator) {
            return;
        }

        BSTR ns = SysAllocString(L"ROOT\\WMI");
        hr = locator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services_);
        SysFreeString(ns);
        locator->Release();
        if (FAILED(hr) || !services_) {
            return;
        }

        hr = CoSetProxyBlanket(services_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
        if (FAILED(hr)) {
            services_->Release();
            services_ = nullptr;
        }
    }

    ~WmiConnection() {
        if (services_) {
            services_->Release();
        }
        if (uninit_) {
            CoUninitialize();
        }
    }

    bool ok() const { return services_ != nullptr; }
    IWbemServices *services() const { return services_; }

private:
    static bool ensureSecurity() {
        static std::once_flag flag;
        static HRESULT cached_hr = E_FAIL;
        std::call_once(flag, []() {
            cached_hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr, EOAC_NONE, nullptr);
        });
        return SUCCEEDED(cached_hr) || cached_hr == RPC_E_TOO_LATE;
    }

    IWbemServices *services_ = nullptr;
    bool uninit_ = false;
};

bool queryWmiBrightness(int &value) {
    WmiConnection conn;
    if (!conn.ok()) {
        return false;
    }

    IEnumWbemClassObject *enumerator = nullptr;
    BSTR query = SysAllocString(L"SELECT CurrentBrightness FROM WmiMonitorBrightness");
    BSTR lang = SysAllocString(L"WQL");
    HRESULT hr = conn.services()->ExecQuery(lang, query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    SysFreeString(query);
    SysFreeString(lang);
    if (FAILED(hr) || !enumerator) {
        return false;
    }

    IWbemClassObject *obj = nullptr;
    ULONG returned = 0;
    hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
    enumerator->Release();
    if (FAILED(hr) || returned == 0 || !obj) {
        return false;
    }

    VARIANT vt{};
    VariantInit(&vt);
    hr = obj->Get(L"CurrentBrightness", 0, &vt, nullptr, nullptr);
    obj->Release();
    if (FAILED(hr)) {
        VariantClear(&vt);
        return false;
    }

    bool ok = false;
    if (vt.vt == VT_UI1) {
        value = static_cast<int>(vt.bVal);
        ok = true;
    } else if (vt.vt == VT_I4 || vt.vt == VT_UI4) {
        value = static_cast<int>(vt.lVal);
        ok = true;
    }
    VariantClear(&vt);
    return ok;
}

bool setWmiBrightness(int level) {
    WmiConnection conn;
    if (!conn.ok()) {
        return false;
    }

    IEnumWbemClassObject *enumerator = nullptr;
    BSTR query = SysAllocString(L"SELECT * FROM WmiMonitorBrightnessMethods");
    BSTR lang = SysAllocString(L"WQL");
    HRESULT hr = conn.services()->ExecQuery(lang, query,
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
    SysFreeString(query);
    SysFreeString(lang);
    if (FAILED(hr) || !enumerator) {
        return false;
    }

    IWbemClassObject *method_class = nullptr;
    BSTR class_name = SysAllocString(L"WmiMonitorBrightnessMethods");
    hr = conn.services()->GetObject(class_name, 0, nullptr, &method_class, nullptr);
    SysFreeString(class_name);
    if (FAILED(hr) || !method_class) {
        enumerator->Release();
        return false;
    }

    IWbemClassObject *in_class = nullptr;
    hr = method_class->GetMethod(L"WmiSetBrightness", 0, &in_class, nullptr);
    method_class->Release();
    if (FAILED(hr) || !in_class) {
        enumerator->Release();
        return false;
    }

    bool ok = false;
    while (true) {
        IWbemClassObject *obj = nullptr;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
        if (FAILED(hr) || returned == 0 || !obj) {
            break;
        }

        VARIANT path{};
        VariantInit(&path);
        hr = obj->Get(L"__PATH", 0, &path, nullptr, nullptr);
        obj->Release();
        if (FAILED(hr) || path.vt != VT_BSTR) {
            VariantClear(&path);
            continue;
        }

        IWbemClassObject *in_inst = nullptr;
        hr = in_class->SpawnInstance(0, &in_inst);
        if (SUCCEEDED(hr) && in_inst) {
            VARIANT var_timeout{};
            VariantInit(&var_timeout);
            var_timeout.vt = VT_UI4;
            var_timeout.ulVal = 0;
            in_inst->Put(L"Timeout", 0, &var_timeout, 0);
            VariantClear(&var_timeout);

            VARIANT var_level{};
            VariantInit(&var_level);
            var_level.vt = VT_UI1;
            var_level.bVal = static_cast<unsigned char>(level);
            in_inst->Put(L"Brightness", 0, &var_level, 0);
            VariantClear(&var_level);

            BSTR method_name = SysAllocString(L"WmiSetBrightness");
            hr = conn.services()->ExecMethod(path.bstrVal, method_name,
                0, nullptr, in_inst, nullptr, nullptr);
            SysFreeString(method_name);
            in_inst->Release();
            if (SUCCEEDED(hr)) {
                ok = true;
            }
        }
        VariantClear(&path);
    }

    in_class->Release();
    enumerator->Release();
    return ok;
}

BOOL CALLBACK enumDisplayDevicesProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto *names = reinterpret_cast<QVector<QString> *>(data);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const QString name = QString::fromWCharArray(info.szDevice);
        if (!name.isEmpty() && !names->contains(name)) {
            names->push_back(name);
        }
    }
    return TRUE;
}

// HDR Definitions (Undocumented/Private API structs for MinGW)
#ifndef DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO
#define DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO 9
#endif

#ifndef DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE
#define DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE 10
#endif

typedef struct _DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    UINT32 value; // Generic access to bitfields
    DWORD colorEncoding;
    DWORD bitsPerColorChannel;
} DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO;

typedef struct _DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE {
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    UINT32 value;
} DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE;

bool getHdrState(bool &supported, bool &enabled) {
    supported = false;
    enabled = false;
    
    UINT32 numPathArrayElements = 0;
    UINT32 numModeInfoArrayElements = 0;
    long ret = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements);
    if (ret != ERROR_SUCCESS) return false;

    QVector<DISPLAYCONFIG_PATH_INFO> pathArray(numPathArrayElements);
    QVector<DISPLAYCONFIG_MODE_INFO> modeInfoArray(numModeInfoArrayElements);

    ret = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray.data(), &numModeInfoArrayElements, modeInfoArray.data(), nullptr);
    if (ret != ERROR_SUCCESS) return false;

    for (UINT32 i = 0; i < numPathArrayElements; ++i) {
        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO colorInfo = {};
        colorInfo.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        colorInfo.header.size = sizeof(colorInfo);
        colorInfo.header.adapterId = pathArray[i].targetInfo.adapterId;
        colorInfo.header.id = pathArray[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&colorInfo.header) == ERROR_SUCCESS) {
            bool advSupported = (colorInfo.value & 0x1) != 0; // advancedColorSupported
            bool advEnabled = (colorInfo.value & 0x2) != 0;   // advancedColorEnabled

            if (advSupported) {
                supported = true;
                if (advEnabled) {
                    enabled = true; 
                    // If any supported monitor is enabled, we consider the "global toggle" to be effectively on or partially on.
                }
            }
        }
    }
    return true;
}

bool setHdrState(bool enable) {
    UINT32 numPathArrayElements = 0;
    UINT32 numModeInfoArrayElements = 0;
    GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, &numModeInfoArrayElements);
    
    QVector<DISPLAYCONFIG_PATH_INFO> pathArray(numPathArrayElements);
    QVector<DISPLAYCONFIG_MODE_INFO> modeInfoArray(numModeInfoArrayElements);
    
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &numPathArrayElements, pathArray.data(), &numModeInfoArrayElements, modeInfoArray.data(), nullptr) != ERROR_SUCCESS) {
        return false;
    }

    bool success = false;
    for (UINT32 i = 0; i < numPathArrayElements; ++i) {
        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO getInfo = {};
        getInfo.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        getInfo.header.size = sizeof(getInfo);
        getInfo.header.adapterId = pathArray[i].targetInfo.adapterId;
        getInfo.header.id = pathArray[i].targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&getInfo.header) == ERROR_SUCCESS) {
             bool supported = (getInfo.value & 0x1) != 0;
             if (supported) {
                DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE setInfo = {};
                setInfo.header.type = (DISPLAYCONFIG_DEVICE_INFO_TYPE)DISPLAYCONFIG_DEVICE_INFO_SET_ADVANCED_COLOR_STATE;
                setInfo.header.size = sizeof(setInfo);
                setInfo.header.adapterId = pathArray[i].targetInfo.adapterId;
                setInfo.header.id = pathArray[i].targetInfo.id;
                
                if (enable) setInfo.value = 1; // enableAdvancedColor = 1
                else setInfo.value = 0;

                if (DisplayConfigSetDeviceInfo(&setInfo.header) == ERROR_SUCCESS) {
                    success = true;
                    Logger::info("service.monitor", QString("HDR set to %1 for display %2").arg(enable).arg(pathArray[i].targetInfo.id));
                }
             }
        }
    }
    return success;
}

}  // namespace

MonitorControlService::MonitorControlService() {
    initial_brightness_ = brightness();
    initial_refresh_rate_ = currentRefreshRate();
}

MonitorControlService::~MonitorControlService() {
    restore();
}

BOOL CALLBACK MonitorControlService::enumMonitorsProc(HMONITOR monitor, HDC, LPRECT, LPARAM data) {
    auto *enum_data = reinterpret_cast<MonitorEnumData *>(data);
    DWORD count = 0;
    if (!GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count) || count == 0) {
        return TRUE;
    }

    QVector<PHYSICAL_MONITOR> temp(static_cast<int>(count));
    if (GetPhysicalMonitorsFromHMONITOR(monitor, count, temp.data())) {
        for (const auto &entry : temp) {
            enum_data->monitors.push_back(entry);
        }
    }

    return TRUE;
}

bool MonitorControlService::enumerateMonitors(MonitorEnumData &data) const {
    data.monitors.clear();
    EnumDisplayMonitors(nullptr, nullptr, &MonitorControlService::enumMonitorsProc,
        reinterpret_cast<LPARAM>(&data));
    return !data.monitors.isEmpty();
}

void MonitorControlService::destroyMonitors(MonitorEnumData &data) {
    if (!data.monitors.isEmpty()) {
        DestroyPhysicalMonitors(static_cast<DWORD>(data.monitors.size()), data.monitors.data());
        data.monitors.clear();
    }
}

int MonitorControlService::brightness() const {
    MonitorEnumData data;
    if (!enumerateMonitors(data)) {
        int wmi_value = -1;
        if (queryWmiBrightness(wmi_value)) {
            Logger::info("service.monitor", "Brightness read via WMI fallback.");
            return wmi_value;
        }
        static bool logged_no_monitors = false;
        if (!logged_no_monitors) {
            Logger::warn("service.monitor", "No monitors available for brightness.");
            logged_no_monitors = true;
        }
        return -1;
    }

    DWORD min_b = 0;
    DWORD cur_b = 0;
    DWORD max_b = 0;
    int value = -1;

    if (!data.monitors.isEmpty()) {
        for (const auto &monitor : data.monitors) {
            if (GetMonitorBrightness(monitor.hPhysicalMonitor, &min_b, &cur_b, &max_b)) {
                value = static_cast<int>(cur_b);
                break;
            }
        }
    }

    destroyMonitors(data);
    if (value >= 0) {
        return value;
    }

    int wmi_value = -1;
    if (queryWmiBrightness(wmi_value)) {
        Logger::info("service.monitor", "Brightness read via WMI fallback.");
        return wmi_value;
    }
    return -1;
}

bool MonitorControlService::setBrightness(int level) const {
    MonitorEnumData data;
    if (!enumerateMonitors(data)) {
        if (setWmiBrightness(level)) {
            Logger::info("service.monitor", QString("Brightness set via WMI level=%1.").arg(level));
            return true;
        }
        Logger::warn("service.monitor", "No monitors available for brightness set.");
        return false;
    }

    bool ok = false;
    static QElapsedTimer log_timer;
    static bool log_started = false;
    if (!log_started) {
        log_timer.start();
        log_started = true;
    }
    for (const auto &monitor : data.monitors) {
        if (SetMonitorBrightness(monitor.hPhysicalMonitor, static_cast<DWORD>(level))) {
            ok = true;
        }
    }

    destroyMonitors(data);
    if (!ok) {
        if (setWmiBrightness(level)) {
            Logger::info("service.monitor", QString("Brightness set via WMI level=%1.").arg(level));
            return true;
        }
        Logger::warn("service.monitor", QString("Set brightness failed level=%1.").arg(level));
        return false;
    }

    if (Logger::isVerbose() || log_timer.elapsed() > 1000) {
        Logger::info("service.monitor", QString("Brightness set level=%1.").arg(level));
        log_timer.restart();
    }
    return true;
}

QVector<int> MonitorControlService::availableRefreshRates() const {
    QVector<int> rates;
    DEVMODE mode{};
    mode.dmSize = sizeof(mode);

    for (int i = 0; EnumDisplaySettings(nullptr, i, &mode); ++i) {
        const int hz = static_cast<int>(mode.dmDisplayFrequency);
        if (!rates.contains(hz)) {
            rates.push_back(hz);
        }
    }

    std::sort(rates.begin(), rates.end());
    if (rates.isEmpty()) {
        Logger::warn("service.monitor", "No refresh rates available.");
    }
    return rates;
}

int MonitorControlService::currentRefreshRate() const {
    DEVMODE mode{};
    mode.dmSize = sizeof(mode);

    if (EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &mode)) {
        return static_cast<int>(mode.dmDisplayFrequency);
    }

    return -1;
}

bool MonitorControlService::setRefreshRate(int rate) const {
    DEVMODE mode{};
    mode.dmSize = sizeof(mode);

    if (!EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &mode)) {
        Logger::warn("service.monitor", "EnumDisplaySettings failed.");
        return false;
    }

    mode.dmDisplayFrequency = static_cast<DWORD>(rate);
    const LONG test = ChangeDisplaySettings(&mode, CDS_TEST);
    if (test != DISP_CHANGE_SUCCESSFUL) {
        Logger::warn("service.monitor", QString("Refresh rate test failed rate=%1.").arg(rate));
        return false;
    }

    const LONG apply = ChangeDisplaySettings(&mode, CDS_UPDATEREGISTRY);
    if (apply != DISP_CHANGE_SUCCESSFUL) {
        Logger::warn("service.monitor", QString("Refresh rate apply failed rate=%1.").arg(rate));
    } else {
        Logger::info("service.monitor", QString("Refresh rate set rate=%1.").arg(rate));
    }
    return apply == DISP_CHANGE_SUCCESSFUL;
}

void MonitorControlService::setNightMode(bool enable) {
    night_mode_active_ = enable;
    applyGammaRamp(enable);
    Logger::info("service.monitor", QString("Night mode %1.").arg(enable ? "on" : "off"));
}

void MonitorControlService::applyGammaRamp(bool night) {
    struct GammaRamp {
        WORD red[256];
        WORD green[256];
        WORD blue[256];
    } ramp{};

    for (int i = 0; i < 256; ++i) {
        const WORD value = static_cast<WORD>(i * 256);
        ramp.red[i] = value;
        if (night) {
            ramp.green[i] = static_cast<WORD>(value * 0.9);
            ramp.blue[i] = static_cast<WORD>(value * 0.8);
        } else {
            ramp.green[i] = value;
            ramp.blue[i] = value;
        }
    }

    QVector<QString> devices;
    EnumDisplayMonitors(nullptr, nullptr, &enumDisplayDevicesProc, reinterpret_cast<LPARAM>(&devices));

    bool applied = false;
    for (const auto &device : devices) {
        HDC dc = CreateDCW(L"DISPLAY", reinterpret_cast<LPCWSTR>(device.utf16()), nullptr, nullptr);
        if (!dc) {
            Logger::warn("service.monitor", QString("CreateDC failed for %1.").arg(device));
            continue;
        }
        if (!SetDeviceGammaRamp(dc, &ramp)) {
            Logger::warn("service.monitor", QString("SetDeviceGammaRamp failed for %1.").arg(device));
        } else {
            applied = true;
        }
        DeleteDC(dc);
    }

    if (!applied) {
        HDC dc = GetDC(nullptr);
        if (dc) {
            if (!SetDeviceGammaRamp(dc, &ramp)) {
                Logger::warn("service.monitor", "SetDeviceGammaRamp failed.");
            }
            ReleaseDC(nullptr, dc);
        }
    }
}

bool MonitorControlService::hdrSupported() const {
    bool supported = false;
    bool enabled = false;
    getHdrState(supported, enabled);
    return supported;
}

bool MonitorControlService::hdrEnabled() const {
    bool supported = false;
    bool enabled = false;
    getHdrState(supported, enabled);
    return enabled;
}

bool MonitorControlService::setHdrMode(bool enable) {
    if (setHdrState(enable)) {
        return true;
    }
    Logger::warn("service.monitor", "Failed to set HDR state.");
    return false;
}

void MonitorControlService::turnOffMonitors() const {
    PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
    Logger::info("service.monitor", "Monitor power off requested.");
}

void MonitorControlService::restore() {
    // 1. Always restore Night Mode as it's a software gamma change (no flicker)
    if (night_mode_active_) {
        setNightMode(false);
    }

    // 2. Restore individual hardware settings ONLY if they differ from initial state.
    // This minimizes screen flickers on app exit for users who didn't touch these.

    // Brightness
    if (initial_brightness_ >= 0) {
        int current = brightness();
        if (current != initial_brightness_) {
            setBrightness(initial_brightness_);
            Logger::info("service.monitor", QString("Restored brightness to %1").arg(initial_brightness_));
        }
    }

    // Refresh Rate
    if (initial_refresh_rate_ > 0) {
        int current = currentRefreshRate();
        if (current != initial_refresh_rate_) {
            setRefreshRate(initial_refresh_rate_);
            Logger::info("service.monitor", QString("Restored refresh rate to %1").arg(initial_refresh_rate_));
        }
    }
}
