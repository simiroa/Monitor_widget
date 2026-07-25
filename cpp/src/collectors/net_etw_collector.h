#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <evntrace.h>
#endif

class NetEtwCollector {
public:
    static NetEtwCollector &instance();

    bool getProcessRates(std::unordered_map<unsigned long, std::pair<double, double>> &out);
    bool isActive() const { return active_.load(); }

private:
    NetEtwCollector();
    ~NetEtwCollector();

    NetEtwCollector(const NetEtwCollector &) = delete;
    NetEtwCollector &operator=(const NetEtwCollector &) = delete;

    void ensureStarted();
    bool startSession();
    void stopSession();
    void traceThreadMain();
    static void NTAPI eventRecordCallback(PEVENT_RECORD record);
    void handleEvent(PEVENT_RECORD record);

    struct Totals {
        unsigned long long rx = 0;
        unsigned long long tx = 0;
    };

    std::atomic<bool> started_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> active_{false};
    std::thread trace_thread_;
    std::mutex mutex_;
    std::unordered_map<unsigned long, Totals> totals_;
    std::unordered_map<unsigned long, Totals> last_totals_;
    std::chrono::steady_clock::time_point last_snapshot_;

#ifdef _WIN32
    TRACEHANDLE session_handle_ = 0;
    TRACEHANDLE trace_handle_ = 0;
#endif
};
