#include "collectors/disk_collector.h"

#include <pdh.h>

#include "utils/logger.h"

namespace {
bool logged_collect_failure = false;
bool logged_read_failure = false;
bool logged_write_failure = false;
}

namespace {
constexpr double kBytesToMB = 1.0 / (1024.0 * 1024.0);
}

DiskCollector::DiskCollector() {
    available_ = initPdh();
}

DiskCollector::~DiskCollector() {
    if (query_) {
        PdhCloseQuery(query_);
        query_ = nullptr;
    }
}

bool DiskCollector::initPdh() {
    PDH_STATUS status = PdhOpenQuery(nullptr, 0, &query_);
    if (status != ERROR_SUCCESS) {
        Logger::warn("collector.disk", QString("PdhOpenQuery failed status=%1.").arg(status));
        return false;
    }

    status = PdhAddCounterW(query_, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &read_counter_);
    if (status != ERROR_SUCCESS) {
        Logger::warn("collector.disk", QString("PdhAddCounter read failed status=%1.").arg(status));
        PdhCloseQuery(query_);
        query_ = nullptr;
        return false;
    }

    status = PdhAddCounterW(query_, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &write_counter_);
    if (status != ERROR_SUCCESS) {
        Logger::warn("collector.disk", QString("PdhAddCounter write failed status=%1.").arg(status));
        PdhCloseQuery(query_);
        query_ = nullptr;
        return false;
    }

    PdhCollectQueryData(query_);
    return true;
}

void DiskCollector::update(SystemStats &stats) {
    if (!available_ || !query_) {
        stats.diskReadMBs = 0.0;
        stats.diskWriteMBs = 0.0;
        return;
    }

    if (PdhCollectQueryData(query_) != ERROR_SUCCESS) {
        if (!logged_collect_failure) {
            Logger::warn("collector.disk", "PdhCollectQueryData failed.");
            logged_collect_failure = true;
        }
        stats.diskReadMBs = 0.0;
        stats.diskWriteMBs = 0.0;
        return;
    }

    PDH_FMT_COUNTERVALUE read_value{};
    PDH_FMT_COUNTERVALUE write_value{};

    if (PdhGetFormattedCounterValue(read_counter_, PDH_FMT_DOUBLE, nullptr, &read_value) == ERROR_SUCCESS) {
        stats.diskReadMBs = read_value.doubleValue * kBytesToMB;
    } else {
        stats.diskReadMBs = 0.0;
        if (!logged_read_failure) {
            Logger::warn("collector.disk", "PdhGetFormattedCounterValue read failed.");
            logged_read_failure = true;
        }
    }

    if (PdhGetFormattedCounterValue(write_counter_, PDH_FMT_DOUBLE, nullptr, &write_value) == ERROR_SUCCESS) {
        stats.diskWriteMBs = write_value.doubleValue * kBytesToMB;
    } else {
        stats.diskWriteMBs = 0.0;
        if (!logged_write_failure) {
            Logger::warn("collector.disk", "PdhGetFormattedCounterValue write failed.");
            logged_write_failure = true;
        }
    }
}
