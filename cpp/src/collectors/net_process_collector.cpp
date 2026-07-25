#include "collectors/net_process_collector.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include <vector>

#include "collectors/net_etw_collector.h"

bool NetProcessCollector::collect(std::unordered_map<unsigned long, NetProcessInfo> &out) {
    out.clear();

    std::unordered_map<unsigned long, std::pair<double, double>> etw_rates;
    const bool use_etw = NetEtwCollector::instance().getProcessRates(etw_rates);

    const auto now = std::chrono::steady_clock::now();
    double elapsedSec = 0.0;
    if (has_last_) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time_);
        elapsedSec = duration.count() / 1000.0;
    }
    last_time_ = now;
    has_last_ = true;

    std::map<QString, std::pair<ULONG64, ULONG64>> current_conn_stats;

    auto getConnKey = [](const MIB_TCPROW_OWNER_PID &row) {
        return QString("%1:%2|%3:%4").arg(row.dwLocalAddr).arg(row.dwLocalPort).arg(row.dwRemoteAddr).arg(row.dwRemotePort);
    };

    DWORD size = 0;
    if (GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        std::vector<unsigned char> buf(size);
        if (GetExtendedTcpTable(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            auto *table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                const auto &row = table->table[i];

                struct in_addr rAddr;
                rAddr.S_un.S_addr = row.dwRemoteAddr;
                char rIp[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &rAddr, rIp, INET_ADDRSTRLEN)) {
                    if (strncmp(rIp, "127.", 4) == 0) continue;
                    if (strcmp(rIp, "0.0.0.0") == 0) continue;
                }
                if (row.dwRemoteAddr == row.dwLocalAddr) continue;

                NetProcessInfo &info = out[row.dwOwningPid];
                info.connectionCount++;

                if (!use_etw && elapsedSec > 0.05) {
                    TCP_ESTATS_DATA_ROD_v0 data = {0};
                    MIB_TCPROW queryRow;
                    queryRow.dwState = row.dwState;
                    queryRow.dwLocalAddr = row.dwLocalAddr;
                    queryRow.dwLocalPort = row.dwLocalPort;
                    queryRow.dwRemoteAddr = row.dwRemoteAddr;
                    queryRow.dwRemotePort = row.dwRemotePort;

                    if (GetPerTcpConnectionEStats((PMIB_TCPROW)&queryRow, TcpConnectionEstatsData,
                        NULL, 0, 0, NULL, 0, 0, (PUCHAR)&data, 0, sizeof(data)) == NO_ERROR) {
                        const QString key = getConnKey(row);
                        current_conn_stats[key] = std::make_pair(data.DataBytesIn, data.DataBytesOut);

                        auto it = last_conn_stats_.find(key);
                        if (it != last_conn_stats_.end()) {
                            ULONG64 dIn = (data.DataBytesIn >= it->second.first) ? (data.DataBytesIn - it->second.first) : 0;
                            ULONG64 dOut = (data.DataBytesOut >= it->second.second) ? (data.DataBytesOut - it->second.second) : 0;
                            double rSpeed = static_cast<double>(dIn) / (1024.0 * 1024.0) / elapsedSec;
                            double wSpeed = static_cast<double>(dOut) / (1024.0 * 1024.0) / elapsedSec;

                            if (rSpeed > 1024.0 || wSpeed > 1024.0) {
                                rSpeed = 0.0;
                                wSpeed = 0.0;
                            }

                            info.readMBs += rSpeed;
                            info.writeMBs += wSpeed;
                        }
                    }
                }

                if (info.endpoints.size() < 4) {
                    struct in_addr addr;
                    addr.S_un.S_addr = row.dwRemoteAddr;
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &addr, ip, INET_ADDRSTRLEN);
                    if (row.dwRemoteAddr != 0) {
                        const QString ipStr = QString::fromLatin1(ip);
                        if (!info.endpoints.contains(ipStr)) {
                            info.endpoints.push_back(ipStr);
                        }
                    }
                }
            }
        }
    }

    if (!use_etw) {
        last_conn_stats_ = current_conn_stats;
    }

    for (const auto &entry : etw_rates) {
        const unsigned long pid = entry.first;
        NetProcessInfo &info = out[pid];
        info.readMBs = entry.second.first;
        info.writeMBs = entry.second.second;
    }

    const double alpha = 0.2;
    for (auto &entry : out) {
        const qint64 pid = static_cast<qint64>(entry.first);
        const double currentRead = entry.second.readMBs;
        const double currentWrite = entry.second.writeMBs;

        const double prevRead = last_read_speeds_[pid];
        const double prevWrite = last_write_speeds_[pid];

        entry.second.readMBs = (alpha * currentRead) + ((1.0 - alpha) * prevRead);
        entry.second.writeMBs = (alpha * currentWrite) + ((1.0 - alpha) * prevWrite);

        if (entry.second.readMBs < 0.001) entry.second.readMBs = 0.0;
        if (entry.second.writeMBs < 0.001) entry.second.writeMBs = 0.0;

        last_read_speeds_[pid] = entry.second.readMBs;
        last_write_speeds_[pid] = entry.second.writeMBs;
    }

    return use_etw;
}
