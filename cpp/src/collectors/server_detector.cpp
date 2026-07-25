#include "collectors/server_detector.h"

#include <algorithm>
#include <vector>

#include <QSet>

#include <winsock2.h>
#include <iphlpapi.h>
#include <windows.h>

#include "utils/logger.h"

namespace {
const char *kBlacklist[] = {
    "svchost.exe", "System", "wininit.exe", "services.exe", "lsass.exe",
    "spoolsv.exe", "explorer.exe", "ApplicationFrameHost.exe",
    "RuntimeBroker.exe", "SearchIndexer.exe", "SecurityHealthService.exe",
    "discord.exe", "steam.exe", "steamwebhelper.exe", "EpicGamesLauncher.exe",
    "Spotify.exe", "chrome.exe", "msedge.exe"
};

bool isInBlacklist(const QString &name) {
    for (const char *entry : kBlacklist) {
        if (name.compare(entry, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
}

ServerDetector::ServerDetector() = default;

QString ServerDetector::basenameFromPath(const QString &path) {
    const int idx = path.lastIndexOf('\\');
    if (idx >= 0 && idx + 1 < path.size()) {
        return path.mid(idx + 1);
    }
    return path;
}

bool ServerDetector::isBlacklisted(const QString &name) const {
    return isInBlacklist(name);
}

void ServerDetector::update() {
    const unsigned long long now_ms = GetTickCount64();
    if (now_ms - last_update_ms_ < update_interval_ms_) {
        return;
    }
    last_update_ms_ = now_ms;
    port_cache_.clear();

    ULONG size = 0;
    static bool logged_table_failure = false;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (size == 0) {
        if (!logged_table_failure) {
            Logger::warn("collector.server", "GetExtendedTcpTable size returned 0.");
            logged_table_failure = true;
        }
        return;
    }

    std::vector<unsigned char> buffer(size);
    const DWORD status = GetExtendedTcpTable(buffer.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (status != NO_ERROR) {
        if (!logged_table_failure) {
            Logger::warn("collector.server", QString("GetExtendedTcpTable failed status=%1.").arg(status));
            logged_table_failure = true;
        }
        return;
    }
    logged_table_failure = false;

    auto *table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID *>(buffer.data());
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        const MIB_TCPROW_OWNER_PID &row = table->table[i];
        if (row.dwState != MIB_TCP_STATE_LISTEN) {
            continue;
        }

        const unsigned long pid = row.dwOwningPid;
        const quint16 port = ntohs(static_cast<u_short>(row.dwLocalPort));
        auto &ports = port_cache_[pid];
        if (!ports.contains(port)) {
            ports.push_back(port);
        }
    }

    for (auto &entry : port_cache_) {
        std::sort(entry.second.begin(), entry.second.end());
    }
}

QVector<quint16> ServerDetector::portsForPid(unsigned long pid) const {
    const auto it = port_cache_.find(pid);
    if (it == port_cache_.end()) {
        return {};
    }
    return it->second;
}

bool ServerDetector::isServer(unsigned long pid) const {
    const auto it = port_cache_.find(pid);
    return it != port_cache_.end() && !it->second.isEmpty();
}

QString ServerDetector::detectServerType(const QString &name, const QString &exe_path, const QVector<quint16> &ports) const {
    const QString lower_name = name.toLower();
    const QString lower_path = exe_path.toLower();
    const QString combined = lower_name + " " + lower_path;

    if (combined.contains("comfy")) {
        return "comfyui";
    }
    if (combined.contains("docker") || combined.contains("containerd")) {
        return "docker";
    }
    if (combined.contains("jupyter")) {
        return "jupyter";
    }
    if (combined.contains("vite")) {
        return "vite";
    }
    if (combined.contains("webpack")) {
        return "webpack";
    }
    if (combined.contains("react")) {
        return "react";
    }
    if (combined.contains("vue")) {
        return "vue";
    }
    if (combined.contains("angular")) {
        return "angular";
    }
    if (combined.contains("flask") || combined.contains("werkzeug")) {
        return "flask";
    }
    if (combined.contains("django") || combined.contains("manage.py")) {
        return "django";
    }
    if (combined.contains("uvicorn") || combined.contains("fastapi")) {
        return "fastapi";
    }
    if (combined.contains("streamlit")) {
        return "streamlit";
    }
    if (combined.contains("gradio")) {
        return "gradio";
    }
    if (lower_name == "node.exe" || lower_name == "node") {
        return "nodejs";
    }
    if (combined.contains("unity")) {
        return "unity";
    }
    if (combined.contains("unreal") || combined.contains("ue4") || combined.contains("ue5")) {
        return "unreal";
    }
    if (combined.contains("blender")) {
        return "blender";
    }

    if (lower_name == "python.exe" || lower_name == "python" || lower_name == "python3" || lower_name == "pythonw.exe") {
        return "python";
    }

    if (ports.contains(3000)) {
        return "nodejs";
    }
    if (ports.contains(5000)) {
        return "flask";
    }
    if (ports.contains(8000)) {
        return "django";
    }
    if (ports.contains(8080)) {
        return "webserver";
    }
    if (ports.contains(8188)) {
        return "comfyui";
    }
    if (ports.contains(8888)) {
        return "jupyter";
    }

    return "";
}

QVector<ServerPoint> ServerDetector::buildServerPoints() const {
    QVector<ServerPoint> results;

    for (const auto &entry : port_cache_) {
        const unsigned long pid = entry.first;
        const QVector<quint16> ports = entry.second;

        QString exe_path;
        QString name;
        HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (handle) {
            wchar_t path_buf[MAX_PATH] = {};
            DWORD path_size = MAX_PATH;
            if (QueryFullProcessImageNameW(handle, 0, path_buf, &path_size)) {
                exe_path = QString::fromWCharArray(path_buf);
                name = basenameFromPath(exe_path);
            }
            CloseHandle(handle);
        }

        if (name.isEmpty()) {
            name = QString("pid_%1").arg(pid);
        }

        if (isBlacklisted(name)) {
            continue;
        }

        const QString server_type = detectServerType(name, exe_path, ports);

        if (server_type.isEmpty()) {
            continue;
        }

        for (quint16 port : ports) {
            ServerPoint point;
            point.port = port;
            point.name = name;
            point.pid = static_cast<qint64>(pid);
            point.serverType = server_type;
            point.exePath = exe_path;
            results.push_back(point);
        }
    }

    std::sort(results.begin(), results.end(), [](const ServerPoint &a, const ServerPoint &b) {
        if (a.serverType != b.serverType) {
            return a.serverType < b.serverType;
        }
        return a.port < b.port;
    });

    QVector<ServerPoint> filtered;
    QSet<QString> seen_keys;
    for (const auto &point : results) {
        if (point.serverType == "unity" || point.serverType == "unreal" ||
            point.serverType == "blender" || point.serverType == "comfyui") {
            const QString key = point.serverType + QString("_%1").arg(point.pid);
            if (seen_keys.contains(key)) {
                continue;
            }
            seen_keys.insert(key);
        }
        filtered.push_back(point);
    }

    return filtered;
}
