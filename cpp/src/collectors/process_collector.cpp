#include "collectors/process_collector.h"

#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include <chrono>
#include <map>
#include <set>
#include <vector>
#include <cstring>

#pragma comment(lib, "psapi.lib")

#include "utils/logger.h"

namespace {
const char *kWhitelist[] = {
    "System", "Registry", "smss.exe", "csrss.exe", "wininit.exe",
    "services.exe", "lsass.exe", "svchost.exe", "dwm.exe",
    "explorer.exe", "SearchIndexer.exe", "SecurityHealthService.exe"
};

bool isWhitelistedName(const QString &name) {
    for (const char *entry : kWhitelist) {
        if (name.compare(entry, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// Static time tracking for I/O rate calculation
static std::chrono::steady_clock::time_point last_io_time;
static bool has_last_io_time = false;
}

ProcessCollector::ProcessCollector() = default;

unsigned long long ProcessCollector::fileTimeToULL(const FILETIME &ft) {
    return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) |
           static_cast<unsigned long long>(ft.dwLowDateTime);
}

QString ProcessCollector::basenameFromPath(const QString &path) {
    const int idx = path.lastIndexOf('\\');
    if (idx >= 0 && idx + 1 < path.size()) {
        return path.mid(idx + 1);
    }
    return path;
}

void ProcessCollector::update(QVector<ProcessInfo> &processes) {
    processes.clear();

    // Calculate elapsed time for I/O rate
    auto now = std::chrono::steady_clock::now();
    double elapsedSec = 0.0;
    if (has_last_io_time) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_io_time);
        elapsedSec = duration.count() / 1000.0;
    }
    last_io_time = now;

    FILETIME idle_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    unsigned long long total_time = 0;

    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        total_time = fileTimeToULL(kernel_time) + fileTimeToULL(user_time);
    }

    const unsigned long long total_delta = has_last_ ? (total_time - last_total_time_) : 0;

    std::unordered_map<unsigned long, NetProcessInfo> net_stats;
    net_collector_.collect(net_stats);

    static bool logged_snapshot_failure = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        if (!logged_snapshot_failure) {
            Logger::warn("collector.process", "CreateToolhelp32Snapshot failed.");
            logged_snapshot_failure = true;
        }
        return;
    }
    logged_snapshot_failure = false;

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);

    std::unordered_set<unsigned long> current_pids;

    if (Process32First(snapshot, &entry)) {
        do {
            const unsigned long pid = entry.th32ProcessID;
            if (pid <= 4) {
                continue;
            }

            current_pids.insert(pid);

            ProcessInfo info;
            info.pid = static_cast<qint64>(pid);
            info.name = QString::fromWCharArray(entry.szExeFile);
            info.isWhitelisted = isWhitelistedName(info.name);

            HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (handle) {
                FILETIME create_time{};
                FILETIME exit_time{};
                FILETIME kernel_proc{};
                FILETIME user_proc{};

                if (GetProcessTimes(handle, &create_time, &exit_time, &kernel_proc, &user_proc)) {
                    const unsigned long long proc_time = fileTimeToULL(kernel_proc) + fileTimeToULL(user_proc);
                    auto it = last_proc_times_.find(pid);
                    if (has_last_ && it != last_proc_times_.end() && total_delta > 0) {
                        const unsigned long long delta = proc_time - it->second;
                        info.cpuPercent = (static_cast<double>(delta) / static_cast<double>(total_delta)) * 100.0;
                    }
                    last_proc_times_[pid] = proc_time;
                }

                PROCESS_MEMORY_COUNTERS_EX mem{};
                if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&mem), sizeof(mem))) {
                    info.ramMB = static_cast<double>(mem.WorkingSetSize) / (1024.0 * 1024.0);
                }

                // Disk I/O counters
                IO_COUNTERS ioCounters{};
                if (GetProcessIoCounters(handle, &ioCounters)) {
                    auto ioIt = last_io_counters_.find(pid);
                    if (has_last_io_time && ioIt != last_io_counters_.end() && elapsedSec > 0.0) {
                        unsigned long long deltaRead = ioCounters.ReadTransferCount - ioIt->second.first;
                        unsigned long long deltaWrite = ioCounters.WriteTransferCount - ioIt->second.second;
                        
                        // Convert to MB/s
                        double readMBs = static_cast<double>(deltaRead) / (1024.0 * 1024.0) / elapsedSec;
                        double writeMBs = static_cast<double>(deltaWrite) / (1024.0 * 1024.0) / elapsedSec;
                        info.diskIoMBs = readMBs + writeMBs;
                    }
                    last_io_counters_[pid] = std::make_pair(ioCounters.ReadTransferCount, ioCounters.WriteTransferCount);
                }

                wchar_t path_buf[MAX_PATH] = {};
                DWORD path_size = MAX_PATH;
                if (QueryFullProcessImageNameW(handle, 0, path_buf, &path_size)) {
                    info.exePath = QString::fromWCharArray(path_buf);
                    if (info.name.isEmpty()) {
                        info.name = basenameFromPath(info.exePath);
                    }
                }

                auto netIt = net_stats.find(pid);
                if (netIt != net_stats.end()) {
                    info.checkConnectionCount = netIt->second.connectionCount;
                    info.remoteEndpoints = netIt->second.endpoints;
                    info.netReadMBs = netIt->second.readMBs;
                    info.netWriteMBs = netIt->second.writeMBs;
                }


                CloseHandle(handle);
            }

            processes.push_back(info);
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);

    // Clean up stale PIDs from tracking maps
    for (auto it = last_proc_times_.begin(); it != last_proc_times_.end();) {
        if (current_pids.find(it->first) == current_pids.end()) {
            it = last_proc_times_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = last_io_counters_.begin(); it != last_io_counters_.end();) {
        if (current_pids.find(it->first) == current_pids.end()) {
            it = last_io_counters_.erase(it);
        } else {
            ++it;
        }
    }

    last_total_time_ = total_time;
    has_last_ = true;
    has_last_io_time = true;
}

void ProcessCollector::updateNetworkOnly(QVector<ProcessInfo> &processes) {
    processes.clear();

    std::unordered_map<unsigned long, NetProcessInfo> net_stats;
    net_collector_.collect(net_stats);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }

    PROCESSENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Process32First(snapshot, &entry)) {
        do {
            const unsigned long pid = entry.th32ProcessID;
            if (pid <= 4) {
                continue;
            }

            ProcessInfo info;
            info.pid = static_cast<qint64>(pid);
            info.name = QString::fromWCharArray(entry.szExeFile);
            info.isWhitelisted = isWhitelistedName(info.name);

            auto netIt = net_stats.find(pid);
            if (netIt != net_stats.end()) {
                info.checkConnectionCount = netIt->second.connectionCount;
                info.remoteEndpoints = netIt->second.endpoints;
                info.netReadMBs = netIt->second.readMBs;
                info.netWriteMBs = netIt->second.writeMBs;
            }

            processes.push_back(info);
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
}
