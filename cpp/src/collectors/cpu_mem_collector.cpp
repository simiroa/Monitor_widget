#include "collectors/cpu_mem_collector.h"

#include "utils/logger.h"

namespace {
bool logged_system_times_failure = false;
bool logged_memory_failure = false;
}

#include <windows.h>
#include <sysinfoapi.h>

// Helper to get SMBIOS data
struct RawSMBIOSData {
    BYTE Used20CallingMethod;
    BYTE SMBIOSMajorVersion;
    BYTE SMBIOSMinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[];
};

struct SMBIOSHeader {
    BYTE Type;
    BYTE Length;
    WORD Handle;
};

CpuMemCollector::CpuMemCollector() {
    // Fetch RAM speed using SMBIOS (Type 17 - Memory Device)
    const DWORD signature = 'RSMB';
    const UINT size = GetSystemFirmwareTable(signature, 0, nullptr, 0);
    if (size > 0) {
        std::vector<BYTE> buffer(size);
        GetSystemFirmwareTable(signature, 0, buffer.data(), size);
        
        const RawSMBIOSData* smbios = reinterpret_cast<RawSMBIOSData*>(buffer.data());
        const BYTE* ptr = smbios->SMBIOSTableData;
        const BYTE* end = buffer.data() + size;

        double max_speed = 0.0;

        while (ptr < end) {
            const SMBIOSHeader* header = reinterpret_cast<const SMBIOSHeader*>(ptr);
            if (header->Type == 17 && header->Length >= 0x17) { // Type 17: Memory Device
                // Offset 0x15: Configured Memory Speed (WORD)
                // Offset 0x17: Manufacturer (String Index) - Optional check
                const WORD speed = *reinterpret_cast<const WORD*>(ptr + 0x15);
                if (speed > 0 && speed < 20000) { // Valid speed
                     if (speed > max_speed) max_speed = static_cast<double>(speed);
                }
            }

            // Move to next structure (skip strings)
            ptr += header->Length;
            while (ptr < end - 1 && (*ptr != 0 || *(ptr + 1) != 0)) {
                ptr++;
            }
            ptr += 2; // Skip double null
        }
        cached_ram_mhz_ = max_speed;
    }
}

unsigned long long CpuMemCollector::fileTimeToULL(const FILETIME &ft) {
    return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) |
           static_cast<unsigned long long>(ft.dwLowDateTime);
}

void CpuMemCollector::update(SystemStats &stats, CapabilityFlags &caps) {
    FILETIME idle_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};

    if (GetSystemTimes(&idle_time, &kernel_time, &user_time)) {
        const unsigned long long idle = fileTimeToULL(idle_time);
        const unsigned long long kernel = fileTimeToULL(kernel_time);
        const unsigned long long user = fileTimeToULL(user_time);
        const unsigned long long total = kernel + user;

        if (has_last_) {
            const unsigned long long idle_delta = idle - last_idle_;
            const unsigned long long kernel_delta = kernel - last_kernel_;
            const unsigned long long user_delta = user - last_user_;
            const unsigned long long total_delta = kernel_delta + user_delta;

            if (total_delta > 0) {
                const double busy = static_cast<double>(total_delta - idle_delta);
                stats.cpuPercent = (busy / static_cast<double>(total_delta)) * 100.0;
            } else {
                stats.cpuPercent = 0.0;
            }
        }

        last_idle_ = idle;
        last_kernel_ = kernel;
        last_user_ = user;
        has_last_ = true;
        logged_system_times_failure = false;
    } else {
        stats.cpuPercent = 0.0;
        if (!logged_system_times_failure) {
            Logger::warn("collector.cpu", "GetSystemTimes failed.");
            logged_system_times_failure = true;
        }
    }

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        stats.ramPercent = static_cast<double>(mem.dwMemoryLoad);
        stats.ramTotalGB = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
        const unsigned long long used = mem.ullTotalPhys - mem.ullAvailPhys;
        stats.ramUsedGB = static_cast<double>(used) / (1024.0 * 1024.0 * 1024.0);
        stats.ramClockMHz = cached_ram_mhz_;
        logged_memory_failure = false;
    } else {
        stats.ramPercent = 0.0;
        stats.ramTotalGB = 0.0;
        stats.ramUsedGB = 0.0;
        if (!logged_memory_failure) {
            Logger::warn("collector.ram", "GlobalMemoryStatusEx failed.");
            logged_memory_failure = true;
        }
    }

    stats.cpuTempC = 0.0;
    stats.cpuPowerW = 0.0;
    caps.cpuTempAvailable = false;
    caps.cpuPowerAvailable = false;

    // Fetch Cache/Core info if not cached
    if (stats.threadCount == 0) { // One-time fetch check
        DWORD len = 0;
        GetLogicalProcessorInformation(nullptr, &len);
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buffer(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            if (GetLogicalProcessorInformation(buffer.data(), &len)) {
                double l1 = 0, l2 = 0, l3 = 0;
                int cores = 0;
                
                for (const auto& info : buffer) {
                     if (info.Relationship == RelationCache) {
                         double sizeMB = static_cast<double>(info.Cache.Size) / (1024.0 * 1024.0);
                         if (info.Cache.Level == 1) l1 += sizeMB;
                         else if (info.Cache.Level == 2) l2 += sizeMB;
                         else if (info.Cache.Level == 3) l3 += sizeMB;
                     } else if (info.Relationship == RelationProcessorCore) {
                         cores++;
                     }
                }
                cached_l1_mb_ = l1;
                cached_l2_mb_ = l2;
                cached_l3_mb_ = l3;
                cached_cores_ = cores;
            }
        }
        cached_threads_ = static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
    }
    
    stats.l1CacheMB = cached_l1_mb_;
    stats.l2CacheMB = cached_l2_mb_;
    stats.l3CacheMB = cached_l3_mb_;
    stats.coreCount = cached_cores_;
    stats.threadCount = cached_threads_;
}
