#pragma once

#include <windows.h>

#include <QHash>
#include <QVector>

#include "gpu/d3dkmt_min.h"

class D3dkmtProcessCollector {
public:
    D3dkmtProcessCollector();
    ~D3dkmtProcessCollector();

    bool update(const LUID &luid, const QVector<qulonglong> &pids, QHash<qulonglong, double> &vram_mb);
    bool hasVram() const { return has_vram_; }

private:
    bool load();
    bool queryProcessUsage(HANDLE process, const LUID &luid, uint64_t &bytes);

    HMODULE gdi_module_ = nullptr;
    void *query_fn_ = nullptr;
    bool has_vram_ = false;
    bool disabled_ = false;

    bool logged_load_failure_ = false;
    bool logged_query_failure_ = false;
    bool logged_access_denied_ = false;
};
