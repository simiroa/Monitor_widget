#include "collectors/net_collector.h"

#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <windows.h>

#include <vector>

#include "utils/logger.h"

#ifndef IF_OPER_STATUS_OPERATIONAL
#define IF_OPER_STATUS_OPERATIONAL 5
#endif

NetCollector::NetCollector() = default;

void NetCollector::update(SystemStats &stats) {
    stats.netRecvMBs = 0.0;
    stats.netSentMBs = 0.0;

    unsigned long long total_rx = 0;
    unsigned long long total_tx = 0;

    static bool logged_table2 = false;
    static bool logged_table1 = false;
    bool using_table2 = false;
    MIB_IF_TABLE2 *table2 = nullptr;
    const DWORD table2_status = GetIfTable2(&table2);
    if (table2_status == NO_ERROR && table2) {
        using_table2 = true;
        if (!logged_table2) {
            Logger::info("collector.net", "Using GetIfTable2.");
            logged_table2 = true;
        }
        for (ULONG i = 0; i < table2->NumEntries; ++i) {
            const MIB_IF_ROW2 &row = table2->Table[i];
            if (row.OperStatus != IfOperStatusUp) {
                continue;
            }
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK || row.Type == IF_TYPE_TUNNEL) {
                continue;
            }

            total_rx += row.InOctets;
            total_tx += row.OutOctets;
        }

        FreeMibTable(table2);
    } else {
        if (!logged_table1) {
            Logger::warn("collector.net", QString("GetIfTable2 failed status=%1, falling back to GetIfTable.").arg(table2_status));
            logged_table1 = true;
        }
        ULONG size = 0;
        const DWORD size_status = GetIfTable(nullptr, &size, FALSE);
        if (size_status != ERROR_INSUFFICIENT_BUFFER) {
            Logger::warn("collector.net", QString("GetIfTable size failed status=%1.").arg(size_status));
            return;
        }

        std::vector<unsigned char> buffer(size);
        MIB_IFTABLE *table = reinterpret_cast<MIB_IFTABLE *>(buffer.data());
        const DWORD table_status = GetIfTable(table, &size, FALSE);
        if (table_status != NO_ERROR) {
            Logger::warn("collector.net", QString("GetIfTable failed status=%1.").arg(table_status));
            return;
        }

        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            const MIB_IFROW &row = table->table[i];
            if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL) {
                continue;
            }
            if (row.dwType == IF_TYPE_SOFTWARE_LOOPBACK || row.dwType == IF_TYPE_TUNNEL) {
                continue;
            }

            total_rx += row.dwInOctets;
            total_tx += row.dwOutOctets;
        }
    }

    const unsigned long long now_ms = GetTickCount64();
    if (has_last_) {
        const double dt = static_cast<double>(now_ms - last_time_ms_) / 1000.0;
        if (dt > 0.1) {
            unsigned long long rx_delta = 0;
            unsigned long long tx_delta = 0;
            if (total_rx >= last_rx_) {
                rx_delta = total_rx - last_rx_;
            }
            if (total_tx >= last_tx_) {
                tx_delta = total_tx - last_tx_;
            }
            stats.netRecvMBs = (static_cast<double>(rx_delta) / (1024.0 * 1024.0)) / dt;
            stats.netSentMBs = (static_cast<double>(tx_delta) / (1024.0 * 1024.0)) / dt;
        }
    }

    last_rx_ = total_rx;
    last_tx_ = total_tx;
    last_time_ms_ = static_cast<long long>(now_ms);
    has_last_ = true;
}
