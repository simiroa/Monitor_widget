#pragma once

#include <windows.h>

#include "gpu/dxcore_min.h"

class DxcoreBackend {
public:
    DxcoreBackend();
    ~DxcoreBackend();

    bool update(const LUID &luid, double &used_gb, double &total_gb);
    bool queryTemperature(const LUID &luid, double &temp_c);
    bool queryEngineClock(const LUID &luid, double &clock_mhz);
    bool hasUsage() const { return has_usage_; }
    bool hasTotal() const { return has_total_; }

private:
    bool loadFactory();
    bool ensureAdapter(const LUID &luid);
    void resetAdapter();

    bool queryUsageBudget(double &used_gb);
    bool queryUsageBytes(double &used_gb);
    bool queryTotal(double &total_gb);
    bool queryTemperature(double &temp_c);
    bool queryEngineClock(double &clock_mhz);
    static bool sameLuid(const LUID &a, const LUID &b);

    HMODULE module_ = nullptr;
    PFN_DXCORE_CREATE_ADAPTER_FACTORY create_factory_ = nullptr;
    IDXCoreAdapterFactory *factory_ = nullptr;
    IDXCoreAdapter *adapter_ = nullptr;
    LUID adapter_luid_{};
    bool has_adapter_ = false;
    bool has_usage_ = false;
    bool has_total_ = false;

    bool logged_load_failure_ = false;
    bool logged_factory_failure_ = false;
    bool logged_adapter_failure_ = false;
    bool logged_budget_failure_ = false;
    bool logged_usage_bytes_failure_ = false;
    bool logged_property_failure_ = false;
    bool logged_zero_usage_ = false;
    bool logged_sample_ = false;
    bool logged_temp_failure_ = false;
    bool logged_temp_unsupported_ = false;
    bool logged_clock_failure_ = false;
    bool logged_clock_unsupported_ = false;
};
