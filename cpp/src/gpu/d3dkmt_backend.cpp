#include "gpu/d3dkmt_backend.h"

#include "gpu/d3dkmt_min.h"

#include <algorithm>

#include "utils/logger.h"

namespace {
constexpr int kMaxNodes = 64;
constexpr int kFailureThreshold = 3;
}

D3dkmtBackend::D3dkmtBackend() = default;

D3dkmtBackend::~D3dkmtBackend() {
    if (gdi_module_) {
        FreeLibrary(gdi_module_);
        gdi_module_ = nullptr;
    }
}

bool D3dkmtBackend::load() {
    if (query_fn_) {
        return true;
    }

    gdi_module_ = LoadLibraryW(L"gdi32.dll");
    if (!gdi_module_) {
        gdi_module_ = LoadLibraryW(L"gdi32full.dll");
    }
    if (!gdi_module_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.d3dkmt", "Failed to load gdi32 for D3DKMT.");
            logged_load_failure_ = true;
        }
        return false;
    }

    query_fn_ = reinterpret_cast<void *>(GetProcAddress(gdi_module_, "D3DKMTQueryStatistics"));
    if (!query_fn_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.d3dkmt", "D3DKMTQueryStatistics not available.");
            logged_load_failure_ = true;
        }
        FreeLibrary(gdi_module_);
        gdi_module_ = nullptr;
        return false;
    }

    return true;
}

bool D3dkmtBackend::sameLuid(const LUID &a, const LUID &b) {
    return a.HighPart == b.HighPart && a.LowPart == b.LowPart;
}

void D3dkmtBackend::reset() {
    available_ = false;
    initialized_ = false;
    node_count_ = 0;
    last_running_us_.clear();
    sample_ready_ = false;
    timer_.invalidate();
    failure_count_ = 0;
    logged_no_nodes_ = false;
    logged_node_count_ = false;
}

bool D3dkmtBackend::ensureAdapter(const LUID &adapter_luid) {
    if (!load()) {
        return false;
    }

    if (!initialized_ || !sameLuid(adapter_luid, adapter_luid_)) {
        reset();
        adapter_luid_ = adapter_luid;
        available_ = true;
        initialized_ = true;
    }

    return available_;
}

bool D3dkmtBackend::queryNodeTimes(QVector<qulonglong> &out_times) {
    if (!available_) {
        return false;
    }

    auto fn = reinterpret_cast<PFND3DKMT_QUERYSTATISTICS>(query_fn_);
    auto query_node = [&](UINT node_id, qulonglong &running_out) {
        D3DKMT_QUERYSTATISTICS query{};
        query.Type = D3DKMT_QUERYSTATISTICS_NODE;
        query.AdapterLuid = adapter_luid_;
        query.QueryNodeId = node_id;

        const LONG status = fn(&query);
        if (status != 0) {
            last_status_ = status;
            return false;
        }

        const auto *node_info = reinterpret_cast<const D3DKMT_QUERYSTATISTICS_NODE_INFORMATION *>(query.QueryResult);
        const LONGLONG running = node_info->GlobalInformation.RunningTime.QuadPart;
        running_out = running > 0 ? static_cast<qulonglong>(running) : 0;
        return true;
    };

    if (node_count_ == 0) {
        out_times.clear();
        for (UINT i = 0; i < static_cast<UINT>(kMaxNodes); ++i) {
            qulonglong running = 0;
            if (!query_node(i, running)) {
                if (out_times.isEmpty()) {
                    if (!logged_no_nodes_) {
                        Logger::warn("gpu.d3dkmt", QString("No GPU nodes found via D3DKMT status=%1.").arg(last_status_));
                        logged_no_nodes_ = true;
                    }
                    return false;
                }
                break;
            }
            out_times.push_back(running);
        }

        node_count_ = static_cast<UINT>(out_times.size());
        last_running_us_.fill(0, static_cast<int>(node_count_));
        if (!logged_node_count_) {
            Logger::info("gpu.d3dkmt", QString("Detected GPU nodes=%1.").arg(node_count_));
            logged_node_count_ = true;
        }
        return node_count_ > 0;
    }

    out_times.fill(0, static_cast<int>(node_count_));
    for (UINT i = 0; i < node_count_; ++i) {
        if (!query_node(i, out_times[static_cast<int>(i)])) {
            if (!logged_query_failure_) {
                Logger::warn("gpu.d3dkmt", QString("Query node failed status=%1 node=%2.")
                    .arg(last_status_)
                    .arg(i));
                logged_query_failure_ = true;
            }
            return false;
        }
    }

    return true;
}

bool D3dkmtBackend::update(const LUID &adapter_luid, double &gpu_percent) {
    gpu_percent = 0.0;

    if (!ensureAdapter(adapter_luid)) {
        return false;
    }

    QVector<qulonglong> current;
    current.resize(static_cast<int>(node_count_));
    if (!queryNodeTimes(current)) {
        failure_count_++;
        if (failure_count_ >= kFailureThreshold) {
            Logger::warn("gpu.d3dkmt", "Node query failed repeatedly; disabling D3DKMT.");
            reset();
        }
        return false;
    }

    failure_count_ = 0;

    if (!sample_ready_) {
        last_running_us_ = current;
        timer_.restart();
        sample_ready_ = true;
        last_util_ = 0.0;
        return false;
    }

    const qint64 elapsed_us = timer_.nsecsElapsed() / 1000;
    if (elapsed_us <= 0) {
        last_running_us_ = current;
        timer_.restart();
        return false;
    }

    double max_util = 0.0;
    for (int i = 0; i < current.size(); ++i) {
        const qint64 delta = static_cast<qint64>(current[i]) - static_cast<qint64>(last_running_us_[i]);
        const double delta_us = delta > 0 ? static_cast<double>(delta) : 0.0;
        double util = (delta_us / static_cast<double>(elapsed_us)) * 100.0;
        if (util > 100.0) {
            util = 100.0;
        } else if (util < 0.0) {
            util = 0.0;
        }
        if (util > max_util) {
            max_util = util;
        }
    }

    last_running_us_ = current;
    timer_.restart();

    gpu_percent = std::min(max_util, 100.0);
    last_util_ = gpu_percent;
    return true;
}
