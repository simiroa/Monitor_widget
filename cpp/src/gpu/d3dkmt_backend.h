#pragma once

#include <QElapsedTimer>
#include <QVector>

#ifdef _WIN32
#include <windows.h>
#endif

class D3dkmtBackend {
public:
    D3dkmtBackend();
    ~D3dkmtBackend();

    bool update(const LUID &adapter_luid, double &gpu_percent);
    bool isAvailable() const { return available_; }
    bool hasSample() const { return sample_ready_; }
    double lastUtil() const { return last_util_; }

private:
    bool load();
    bool ensureAdapter(const LUID &adapter_luid);
    bool queryNodeTimes(QVector<qulonglong> &out_times);

    static bool sameLuid(const LUID &a, const LUID &b);
    void reset();

    HMODULE gdi_module_ = nullptr;
    void *query_fn_ = nullptr;
    bool available_ = false;
    bool initialized_ = false;

    LUID adapter_luid_{};
    UINT node_count_ = 0;

    QVector<qulonglong> last_running_us_;
    QElapsedTimer timer_;
    bool sample_ready_ = false;
    double last_util_ = 0.0;

    int failure_count_ = 0;
    bool logged_load_failure_ = false;
    bool logged_query_failure_ = false;
    bool logged_no_nodes_ = false;
    bool logged_node_count_ = false;
    LONG last_status_ = 0;
};
