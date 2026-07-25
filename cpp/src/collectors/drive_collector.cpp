#include "collectors/drive_collector.h"

#include <windows.h>

#include "utils/logger.h"

namespace {
bool logged_drive_strings_failure = false;
bool logged_disk_space_failure = false;
bool logged_pdh_failure = false;
}

DriveCollector::DriveCollector() {
    initPdh();
}

DriveCollector::~DriveCollector() {
    clearCounters();
    if (query_) {
        PdhCloseQuery(query_);
        query_ = nullptr;
    }
}

bool DriveCollector::initPdh() {
    PDH_STATUS status = PdhOpenQueryW(nullptr, 0, &query_);
    if (status != ERROR_SUCCESS) {
        Logger::warn("collector.drive", QString("PdhOpenQuery failed: 0x%1").arg(status, 0, 16));
        return false;
    }
    return true;
}

void DriveCollector::clearCounters() {
    counters_.clear();
}

void DriveCollector::ensureCountersForDrive(const QString &mountpoint) {
    if (counters_.find(mountpoint) != counters_.end()) {
        return;  // Already registered
    }

    if (!query_) {
        return;
    }

    // Extract drive letter (e.g., "C:\\" -> "C:")
    QString mount = mountpoint.left(2);
    
    DriveCounters dc;

    // PDH counter paths
    QString readPath = QString("\\LogicalDisk(%1)\\Disk Read Bytes/sec").arg(mount);
    QString writePath = QString("\\LogicalDisk(%1)\\Disk Write Bytes/sec").arg(mount);
    QString activePath = QString("\\LogicalDisk(%1)\\% Disk Time").arg(mount);

    PDH_STATUS status;

    status = PdhAddCounterW(query_, reinterpret_cast<LPCWSTR>(readPath.utf16()), 0, &dc.readCounter);
    if (status != ERROR_SUCCESS) {
        Logger::debug("collector.drive", QString("Failed to add read counter for %1: 0x%2").arg(mount).arg(status, 0, 16));
    }

    status = PdhAddCounterW(query_, reinterpret_cast<LPCWSTR>(writePath.utf16()), 0, &dc.writeCounter);
    if (status != ERROR_SUCCESS) {
        Logger::debug("collector.drive", QString("Failed to add write counter for %1: 0x%2").arg(mount).arg(status, 0, 16));
    }

    status = PdhAddCounterW(query_, reinterpret_cast<LPCWSTR>(activePath.utf16()), 0, &dc.activeCounter);
    if (status != ERROR_SUCCESS) {
        Logger::debug("collector.drive", QString("Failed to add active counter for %1: 0x%2").arg(mount).arg(status, 0, 16));
    }

    counters_[mountpoint] = dc;
    Logger::debug("collector.drive", QString("Registered PDH counters for %1").arg(mount));
}

void DriveCollector::update(SystemStats &stats) {
    stats.drives.clear();

    DWORD length = GetLogicalDriveStringsW(0, nullptr);
    if (length == 0) {
        if (!logged_drive_strings_failure) {
            Logger::warn("collector.drive", "GetLogicalDriveStringsW length failed.");
            logged_drive_strings_failure = true;
        }
        return;
    }

    std::wstring buffer;
    buffer.resize(length + 1);
    if (GetLogicalDriveStringsW(length, buffer.data()) == 0) {
        if (!logged_drive_strings_failure) {
            Logger::warn("collector.drive", "GetLogicalDriveStringsW read failed.");
            logged_drive_strings_failure = true;
        }
        return;
    }

    // Collect drives and register PDH counters
    QVector<DriveInfo> drives;
    const wchar_t *ptr = buffer.c_str();
    while (*ptr) {
        std::wstring drive(ptr);
        const UINT type = GetDriveTypeW(drive.c_str());
        
        // Fixed drives only (exclude CD-ROM, RAM disk, etc.)
        if (type == DRIVE_FIXED) {
            ULARGE_INTEGER free_bytes{};
            ULARGE_INTEGER total_bytes{};
            ULARGE_INTEGER free_total{};
            if (GetDiskFreeSpaceExW(drive.c_str(), &free_bytes, &total_bytes, &free_total)) {
                DriveInfo info;
                info.mountpoint = QString::fromWCharArray(drive.c_str());
                info.totalGB = static_cast<double>(total_bytes.QuadPart) / (1024.0 * 1024.0 * 1024.0);
                info.freeGB = static_cast<double>(free_total.QuadPart) / (1024.0 * 1024.0 * 1024.0);
                info.usedGB = info.totalGB - info.freeGB;
                if (info.totalGB > 0.0) {
                    info.percent = (info.usedGB / info.totalGB) * 100.0;
                }
                info.readMBs = 0.0;
                info.writeMBs = 0.0;
                info.activeTime = 0.0;
                info.ioValid = false;
                
                // Ensure PDH counter is registered
                ensureCountersForDrive(info.mountpoint);
                
                drives.push_back(info);
                logged_disk_space_failure = false;
            } else if (!logged_disk_space_failure) {
                Logger::warn("collector.drive", QString("GetDiskFreeSpaceExW failed for %1.").arg(QString::fromWCharArray(drive.c_str())));
                logged_disk_space_failure = true;
            }
        }

        ptr += drive.size() + 1;
    }

    // Query PDH data
    if (query_) {
        PDH_STATUS status = PdhCollectQueryData(query_);
        if (status == ERROR_SUCCESS) {
            for (DriveInfo &info : drives) {
                auto it = counters_.find(info.mountpoint);
                if (it == counters_.end()) {
                    continue;
                }

                const DriveCounters &dc = it->second;
                PDH_FMT_COUNTERVALUE value;

                // Read speed (Bytes/sec -> MB/s)
                if (dc.readCounter) {
                    status = PdhGetFormattedCounterValue(dc.readCounter, PDH_FMT_DOUBLE, nullptr, &value);
                    if (status == ERROR_SUCCESS) {
                        info.readMBs = value.doubleValue / (1024.0 * 1024.0);
                        info.ioValid = true;
                    }
                }

                // Write speed (Bytes/sec -> MB/s)
                if (dc.writeCounter) {
                    status = PdhGetFormattedCounterValue(dc.writeCounter, PDH_FMT_DOUBLE, nullptr, &value);
                    if (status == ERROR_SUCCESS) {
                        info.writeMBs = value.doubleValue / (1024.0 * 1024.0);
                        info.ioValid = true;
                    }
                }

                // Active time (%)
                if (dc.activeCounter) {
                    status = PdhGetFormattedCounterValue(dc.activeCounter, PDH_FMT_DOUBLE, nullptr, &value);
                    if (status == ERROR_SUCCESS) {
                        info.activeTime = value.doubleValue;
                        // Clamp to 0-100%
                        if (info.activeTime > 100.0) info.activeTime = 100.0;
                        if (info.activeTime < 0.0) info.activeTime = 0.0;
                        info.ioValid = true;
                    }
                }
            }
            logged_pdh_failure = false;
        } else if (!logged_pdh_failure) {
            Logger::warn("collector.drive", QString("PdhCollectQueryData failed: 0x%1").arg(status, 0, 16));
            logged_pdh_failure = true;
        }
    }

    stats.drives = drives;
}
