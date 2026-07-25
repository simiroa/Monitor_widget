#pragma once

#include <dxgi1_4.h>
#include <wrl/client.h>

#include "gpu/gpu_backend.h"

class DxgiBackend : public GpuBackend {
public:
    DxgiBackend();
    bool initialize() override;
    void shutdown() override;
    void update(SystemStats &stats, CapabilityFlags &caps) override;
    bool isAvailable() const override { return available_; }
    bool adapterLuid(LUID &out) const;

private:
    static QString vendorNameFromId(unsigned int vendor_id);

    bool available_ = false;
    DXGI_ADAPTER_DESC1 desc_{};
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter_;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3_;
    bool logged_init_failure_ = false;
    bool logged_no_adapter_ = false;
    bool logged_available_ = false;
    bool logged_vram_query_failure_ = false;
};
