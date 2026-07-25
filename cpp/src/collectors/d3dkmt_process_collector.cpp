#include "collectors/d3dkmt_process_collector.h"

#include <algorithm>

#include "utils/logger.h"

namespace {
constexpr double kBytesToMB = 1.0 / (1024.0 * 1024.0);
constexpr int kFailureThreshold = 5;
}

D3dkmtProcessCollector::D3dkmtProcessCollector() = default;

D3dkmtProcessCollector::~D3dkmtProcessCollector() {
    if (gdi_module_) {
        FreeLibrary(gdi_module_);
        gdi_module_ = nullptr;
    }
}

bool D3dkmtProcessCollector::load() {
    if (query_fn_) {
        return true;
    }

    gdi_module_ = LoadLibraryW(L"gdi32.dll");
    if (!gdi_module_) {
        gdi_module_ = LoadLibraryW(L"gdi32full.dll");
    }
    if (!gdi_module_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.d3dkmt_process", "Failed to load gdi32 for D3DKMT.");
            logged_load_failure_ = true;
        }
        return false;
    }

    query_fn_ = reinterpret_cast<void *>(GetProcAddress(gdi_module_, "D3DKMTQueryStatistics"));
    if (!query_fn_) {
        if (!logged_load_failure_) {
            Logger::warn("gpu.d3dkmt_process", "D3DKMTQueryStatistics not available.");
            logged_load_failure_ = true;
        }
        FreeLibrary(gdi_module_);
        gdi_module_ = nullptr;
        return false;
    }

    return true;
}

bool D3dkmtProcessCollector::queryProcessUsage(HANDLE process, const LUID &luid, uint64_t &bytes) {
    bytes = 0;
    if (!query_fn_) {
        return false;
    }

    auto fn = reinterpret_cast<PFND3DKMT_QUERYSTATISTICS>(query_fn_);
    D3DKMT_QUERYSTATISTICS query{};
    query.Type = D3DKMT_QUERYSTATISTICS_PROCESS_SEGMENT_GROUP;
    query.AdapterLuid = luid;
    query.hProcess = process;
    query.QueryProcessSegmentGroup = D3DKMT_MEMORY_SEGMENT_GROUP_LOCAL;

    const LONG status = fn(&query);
    if (status != 0) {
        if (status == static_cast<LONG>(0xC0000022)) {  // STATUS_ACCESS_DENIED
            if (!logged_access_denied_) {
                Logger::warn("gpu.d3dkmt_process", "Access denied querying process segment group.");
                logged_access_denied_ = true;
            }
        } else if (!logged_query_failure_) {
            Logger::warn("gpu.d3dkmt_process", QString("QueryStatistics PROCESS_SEGMENT_GROUP failed status=%1.")
                .arg(status));
            logged_query_failure_ = true;
        }
        return false;
    }

    const auto *info =
        reinterpret_cast<const D3DKMT_QUERYSTATISTICS_PROCESS_SEGMENT_GROUP_INFORMATION *>(query.QueryResult);
    bytes = static_cast<uint64_t>(info->Usage);
    return true;
}

bool D3dkmtProcessCollector::update(const LUID &luid, const QVector<qulonglong> &pids,
    QHash<qulonglong, double> &vram_mb) {
    vram_mb.clear();
    has_vram_ = false;

    if (disabled_) {
        return false;
    }

    if (!load()) {
        return false;
    }

    int failures = 0;
    for (qulonglong pid : pids) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (!process) {
            continue;
        }

        uint64_t bytes = 0;
        if (queryProcessUsage(process, luid, bytes)) {
            const double mb = std::max(0.0, static_cast<double>(bytes) * kBytesToMB);
            if (mb > 0.0) {
                vram_mb.insert(pid, mb);
                has_vram_ = true;
            }
        } else {
            ++failures;
        }

        CloseHandle(process);
    }

    if (failures >= kFailureThreshold && !has_vram_) {
        disabled_ = true;
    }

    return has_vram_;
}
