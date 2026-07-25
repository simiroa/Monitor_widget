#include "collectors/net_etw_collector.h"

#ifdef _WIN32
#include <evntrace.h>
#include <tdh.h>
#endif

#include <string>
#include <vector>

#include "utils/logger.h"

namespace {
const GUID kSystemTraceControlGuid = {0x9e814aad, 0x3204, 0x11d2, {0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39}};

struct EventKey {
    GUID provider{};
    USHORT id = 0;
    UCHAR opcode = 0;
    UCHAR version = 0;
};

struct EventKeyHash {
    size_t operator()(const EventKey &key) const {
        const auto *p = reinterpret_cast<const unsigned long long *>(&key.provider);
        size_t h = std::hash<unsigned long long>()(p[0]) ^ std::hash<unsigned long long>()(p[1]);
        h ^= (static_cast<size_t>(key.id) << 1);
        h ^= (static_cast<size_t>(key.opcode) << 16);
        h ^= (static_cast<size_t>(key.version) << 24);
        return h;
    }
};

struct EventKeyEq {
    bool operator()(const EventKey &a, const EventKey &b) const {
        return a.id == b.id && a.opcode == b.opcode && a.version == b.version &&
            memcmp(&a.provider, &b.provider, sizeof(GUID)) == 0;
    }
};

struct PropertyCache {
    bool valid = false;
    std::wstring size_prop;
};

std::mutex cache_mutex;
std::unordered_map<EventKey, PropertyCache, EventKeyHash, EventKeyEq> cache;

std::wstring toLower(const std::wstring &value) {
    std::wstring out = value;
    for (auto &ch : out) {
        ch = static_cast<wchar_t>(towlower(ch));
    }
    return out;
}

bool readUint64Property(PEVENT_RECORD record, const std::wstring &name, unsigned long long *out) {
#ifdef _WIN32
    PROPERTY_DATA_DESCRIPTOR desc{};
    desc.PropertyName = reinterpret_cast<ULONGLONG>(name.c_str());
    desc.ArrayIndex = ULONG_MAX;

    ULONG size = 0;
    if (TdhGetPropertySize(record, 0, nullptr, 1, &desc, &size) != ERROR_SUCCESS || size == 0) {
        return false;
    }

    std::vector<unsigned char> buffer(size);
    if (TdhGetProperty(record, 0, nullptr, 1, &desc, size, buffer.data()) != ERROR_SUCCESS) {
        return false;
    }

    unsigned long long value = 0;
    switch (size) {
    case 1:
        value = buffer[0];
        break;
    case 2:
        value = *reinterpret_cast<const unsigned short *>(buffer.data());
        break;
    case 4:
        value = *reinterpret_cast<const unsigned int *>(buffer.data());
        break;
    default:
        value = *reinterpret_cast<const unsigned long long *>(buffer.data());
        break;
    }

    *out = value;
    return true;
#else
    (void)record;
    (void)name;
    (void)out;
    return false;
#endif
}
}  // namespace

NetEtwCollector &NetEtwCollector::instance() {
    static NetEtwCollector instance;
    return instance;
}

NetEtwCollector::NetEtwCollector() = default;

NetEtwCollector::~NetEtwCollector() {
    stopSession();
}

void NetEtwCollector::ensureStarted() {
#ifdef _WIN32
    if (started_.load()) {
        return;
    }
    started_.store(true);
    if (!startSession()) {
        Logger::warn("collector.net_etw", "ETW session start failed. Falling back.");
        active_.store(false);
    }
#endif
}

bool NetEtwCollector::startSession() {
#ifdef _WIN32
    if (running_.load()) {
        return true;
    }

    const std::wstring session_name = L"MonitorWidgetNetTrace";
    const size_t buffer_size = sizeof(EVENT_TRACE_PROPERTIES) + (session_name.size() + 1) * sizeof(wchar_t);
    auto *props = reinterpret_cast<EVENT_TRACE_PROPERTIES *>(calloc(1, buffer_size));
    if (!props) {
        return false;
    }

    props->Wnode.BufferSize = static_cast<ULONG>(buffer_size);
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->Wnode.ClientContext = 1;
    props->Wnode.Guid = kSystemTraceControlGuid;
    props->EnableFlags = EVENT_TRACE_FLAG_NETWORK_TCPIP;
    props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG status = StartTraceW(&session_handle_, session_name.c_str(), props);
    if (status == ERROR_ALREADY_EXISTS) {
        ControlTraceW(0, session_name.c_str(), props, EVENT_TRACE_CONTROL_STOP);
        status = StartTraceW(&session_handle_, session_name.c_str(), props);
    }
    if (status != ERROR_SUCCESS) {
        free(props);
        if (status == ERROR_ACCESS_DENIED) {
            Logger::warn("collector.net_etw", "ETW access denied (admin required).");
        } else {
            Logger::warn("collector.net_etw", QString("StartTrace failed status=%1.").arg(status));
        }
        return false;
    }

    EVENT_TRACE_LOGFILEW logfile{};
    logfile.LoggerName = const_cast<LPWSTR>(session_name.c_str());
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = &NetEtwCollector::eventRecordCallback;
    trace_handle_ = OpenTraceW(&logfile);
    if (trace_handle_ == INVALID_PROCESSTRACE_HANDLE) {
        ControlTraceW(session_handle_, session_name.c_str(), props, EVENT_TRACE_CONTROL_STOP);
        free(props);
        return false;
    }

    running_.store(true);
    active_.store(true);
    trace_thread_ = std::thread(&NetEtwCollector::traceThreadMain, this);
    free(props);
    return true;
#else
    return false;
#endif
}

void NetEtwCollector::stopSession() {
#ifdef _WIN32
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    active_.store(false);

    if (session_handle_ != 0) {
        EVENT_TRACE_PROPERTIES props{};
        props.Wnode.BufferSize = sizeof(props);
        ControlTraceW(session_handle_, L"MonitorWidgetNetTrace", &props, EVENT_TRACE_CONTROL_STOP);
        session_handle_ = 0;
    }

    if (trace_thread_.joinable()) {
        trace_thread_.join();
    }
#endif
}

void NetEtwCollector::traceThreadMain() {
#ifdef _WIN32
    if (trace_handle_ == 0 || trace_handle_ == INVALID_PROCESSTRACE_HANDLE) {
        active_.store(false);
        return;
    }

    ProcessTrace(&trace_handle_, 1, nullptr, nullptr);
    CloseTrace(trace_handle_);
    trace_handle_ = 0;
    active_.store(false);
#endif
}

void NTAPI NetEtwCollector::eventRecordCallback(PEVENT_RECORD record) {
    NetEtwCollector::instance().handleEvent(record);
}

void NetEtwCollector::handleEvent(PEVENT_RECORD record) {
#ifdef _WIN32
    if (!record || !active_.load()) {
        return;
    }

    const UCHAR opcode = record->EventHeader.EventDescriptor.Opcode;
    const bool is_send = (opcode == EVENT_TRACE_TYPE_SEND || opcode == 10);
    const bool is_recv = (opcode == EVENT_TRACE_TYPE_RECEIVE || opcode == 11);
    if (!is_send && !is_recv) {
        return;
    }

    const unsigned long pid = record->EventHeader.ProcessId;
    if (pid == 0) {
        return;
    }

    EventKey key{};
    key.provider = record->EventHeader.ProviderId;
    key.id = record->EventHeader.EventDescriptor.Id;
    key.opcode = record->EventHeader.EventDescriptor.Opcode;
    key.version = record->EventHeader.EventDescriptor.Version;

    PropertyCache prop_cache{};
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            prop_cache = it->second;
        } else {
            ULONG info_size = 0;
            TdhGetEventInformation(record, 0, nullptr, nullptr, &info_size);
            std::vector<unsigned char> info_buffer(info_size);
            auto *info = reinterpret_cast<PTRACE_EVENT_INFO>(info_buffer.data());
            if (TdhGetEventInformation(record, 0, nullptr, info, &info_size) == ERROR_SUCCESS) {
                for (ULONG i = 0; i < info->TopLevelPropertyCount; ++i) {
                    const auto &prop = info->EventPropertyInfoArray[i];
                    const auto *name_ptr = reinterpret_cast<const wchar_t *>(reinterpret_cast<const unsigned char *>(info) + prop.NameOffset);
                    const std::wstring name_lower = toLower(name_ptr ? name_ptr : L"");
                    if (name_lower == L"size" || name_lower == L"transfersize" || name_lower == L"datalength" ||
                        name_lower == L"length" || name_lower == L"bytes") {
                        prop_cache.size_prop = name_ptr ? name_ptr : L"";
                        prop_cache.valid = !prop_cache.size_prop.empty();
                        break;
                    }
                }
            }
            cache[key] = prop_cache;
        }
    }

    if (!prop_cache.valid) {
        return;
    }

    unsigned long long size = 0;
    if (!readUint64Property(record, prop_cache.size_prop, &size)) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto &totals = totals_[pid];
    if (is_send) {
        totals.tx += size;
    } else {
        totals.rx += size;
    }
#else
    (void)record;
#endif
}

bool NetEtwCollector::getProcessRates(std::unordered_map<unsigned long, std::pair<double, double>> &out) {
    ensureStarted();
#ifdef _WIN32
    if (!active_.load()) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    if (last_snapshot_.time_since_epoch().count() == 0) {
        last_snapshot_ = now;
        return false;
    }

    const double dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_snapshot_).count() / 1000.0;
    if (dt <= 0.1) {
        return false;
    }

    std::unordered_map<unsigned long, Totals> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = totals_;
    }

    out.clear();
    for (const auto &entry : snapshot) {
        const unsigned long pid = entry.first;
        const Totals current = entry.second;
        const Totals last = last_totals_[pid];

        unsigned long long rx_delta = 0;
        unsigned long long tx_delta = 0;
        if (current.rx >= last.rx) {
            rx_delta = current.rx - last.rx;
        }
        if (current.tx >= last.tx) {
            tx_delta = current.tx - last.tx;
        }

        const double rx_mbs = static_cast<double>(rx_delta) / (1024.0 * 1024.0) / dt;
        const double tx_mbs = static_cast<double>(tx_delta) / (1024.0 * 1024.0) / dt;

        out[pid] = {rx_mbs, tx_mbs};
    }

    last_totals_ = snapshot;
    last_snapshot_ = now;
    return true;
#else
    (void)out;
    return false;
#endif
}
