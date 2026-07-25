#include "gpu/dxgi_backend.h"

#include <dxgi1_4.h>

#include "utils/logger.h"

namespace {
constexpr double kBytesToGB = 1.0 / (1024.0 * 1024.0 * 1024.0);
}

DxgiBackend::DxgiBackend() = default;

bool DxgiBackend::initialize() {
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) != S_OK) {
        if (!logged_init_failure_) {
            Logger::warn("gpu.dxgi", "CreateDXGIFactory1 failed.");
            logged_init_failure_ = true;
        }
        return false;
    }

    for (UINT i = 0; ; ++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (adapter->GetDesc1(&desc) != S_OK) {
            continue;
        }

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        adapter_ = adapter;
        desc_ = desc;
        adapter.As(&adapter3_);
        available_ = true;
        if (!logged_available_) {
            Logger::info("gpu.dxgi", QString("Adapter=%1 vendor=%2 vramGB=%3")
                .arg(QString::fromWCharArray(desc_.Description).trimmed())
                .arg(vendorNameFromId(desc_.VendorId))
                .arg(static_cast<double>(desc_.DedicatedVideoMemory) * kBytesToGB, 0, 'f', 1));
            logged_available_ = true;
        }
        break;
    }

    if (!available_ && !logged_no_adapter_) {
        Logger::warn("gpu.dxgi", "No hardware adapter found.");
        logged_no_adapter_ = true;
    }
    return available_;
}

void DxgiBackend::shutdown() {
    adapter3_.Reset();
    adapter_.Reset();
    available_ = false;
}

bool DxgiBackend::adapterLuid(LUID &out) const {
    if (!available_) {
        return false;
    }
    out = desc_.AdapterLuid;
    return true;
}

void DxgiBackend::update(SystemStats &stats, CapabilityFlags &caps) {
    if (!available_ && !initialize()) {
        stats.gpuName.clear();
        stats.gpuLoad = 0.0;
        stats.gpuTempC = 0.0;
        stats.vramPercent = 0.0;
        stats.vramUsedGB = 0.0;
        stats.vramTotalGB = 0.0;
        stats.gpuClockMHz = 0.0;
        stats.gpuPowerW = 0.0;
        caps.gpuNameAvailable = false;
        caps.vramAvailable = false;
        caps.gpuLoadAvailable = false;
        caps.gpuTempAvailable = false;
        caps.gpuClockAvailable = false;
        caps.gpuPowerAvailable = false;
        caps.gpuBackend.clear();
        caps.gpuVendor.clear();
        return;
    }

    stats.gpuName = QString::fromWCharArray(desc_.Description).trimmed();
    stats.vramTotalGB = static_cast<double>(desc_.DedicatedVideoMemory) * kBytesToGB;

    if (adapter3_) {
        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
        const HRESULT hr = adapter3_->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info);
        if (hr == S_OK) {
            stats.vramUsedGB = static_cast<double>(info.CurrentUsage) * kBytesToGB;
            logged_vram_query_failure_ = false;
        } else {
            stats.vramUsedGB = 0.0;
            if (!logged_vram_query_failure_) {
                Logger::warn("gpu.dxgi", QString("QueryVideoMemoryInfo failed hr=0x%1.")
                    .arg(static_cast<unsigned long>(hr), 0, 16));
                logged_vram_query_failure_ = true;
            }
        }
    } else {
        stats.vramUsedGB = 0.0;
    }

    if (stats.vramTotalGB > 0.0) {
        stats.vramPercent = (stats.vramUsedGB / stats.vramTotalGB) * 100.0;
    } else {
        stats.vramPercent = 0.0;
    }

    stats.gpuLoad = 0.0;
    stats.gpuTempC = 0.0;
    stats.gpuClockMHz = 0.0;
    stats.gpuPowerW = 0.0;

    caps.gpuNameAvailable = !stats.gpuName.isEmpty();
    caps.vramAvailable = stats.vramTotalGB > 0.0;
    caps.gpuLoadAvailable = false;
    caps.gpuTempAvailable = false;
    caps.gpuClockAvailable = false;
    caps.gpuPowerAvailable = false;
    caps.gpuBackend = "DXGI";
    caps.gpuVendor = vendorNameFromId(desc_.VendorId);
}

QString DxgiBackend::vendorNameFromId(unsigned int vendor_id) {
    switch (vendor_id) {
        case 0x10DE:
            return "NVIDIA";
        case 0x1002:
        case 0x1022:
            return "AMD";
        case 0x8086:
            return "Intel";
        default:
            return "";
    }
}
