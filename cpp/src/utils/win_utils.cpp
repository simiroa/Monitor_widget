#include "utils/win_utils.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <powrprof.h>
#include <pdh.h>
#include <shlobj.h>
#include <vector>

// Link libraries
#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "Pdh.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")
#endif

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace WinUtils {

bool isElevated() {
#ifdef _WIN32
    return IsUserAnAdmin() != FALSE;
#else
    return false;
#endif
}

QString knownFolderPath(const KNOWNFOLDERID &id) {
    QString result;
#ifdef _WIN32
    PWSTR path = nullptr;
    if (SHGetKnownFolderPath(id, 0, nullptr, &path) == S_OK) {
        result = QString::fromWCharArray(path);
        CoTaskMemFree(path);
    }
#else
    Q_UNUSED(id);
#endif
    return result;
}

QString envPath(const char *name) {
    return QString::fromLocal8Bit(qgetenv(name));
}

bool removeContents(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) return false;

    bool ok = true;
    const QFileInfoList entries = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System
    );

    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            QDir entry_dir(entry.absoluteFilePath());
            if (!entry_dir.removeRecursively()) ok = false;
        } else {
            if (!QFile::remove(entry.absoluteFilePath())) ok = false;
        }
    }
    return ok;
}

quint64 folderSize(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) return 0;

    quint64 total = 0;
    QDirIterator it(path,
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDirIterator::Subdirectories
    );

    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.isFile()) total += static_cast<quint64>(info.size());
    }
    return total;
}

#ifdef _WIN32
namespace {
QString cleanCpuNameInternal(QString name) {
    name.replace("(R)", "");
    name.replace("(TM)", "");
    name.replace("Processor", "");
    name.replace("CPU", "");
    name.replace("12th Gen", "");
    name.replace("13th Gen", "");
    name.replace("14th Gen", "");
    return name.simplified();
}

QString readRegistryString(const wchar_t *subkey, const wchar_t *value) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return {};

    wchar_t buffer[256] = {};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    QString result;
    if (RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS) {
        if (type == REG_SZ || type == REG_EXPAND_SZ) result = QString::fromWCharArray(buffer);
    }
    RegCloseKey(key);
    return result;
}

int readRegistryDword(const wchar_t *subkey, const wchar_t *value) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subkey, 0, KEY_READ, &key) != ERROR_SUCCESS) return -1;

    DWORD data = 0;
    DWORD size = sizeof(data);
    DWORD type = 0;
    int result = -1;
    if (RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size) == ERROR_SUCCESS) {
        if (type == REG_DWORD) result = static_cast<int>(data);
    }
    RegCloseKey(key);
    return result;
}

struct PROCESSOR_POWER_INFO_EX {
    ULONG Number;
    ULONG MaxMhz;
    ULONG CurrentMhz;
    ULONG MhzLimit;
    ULONG MaxIdleState;
    ULONG CurrentIdleState;
};
}

QString fetchCpuName() {
    static QString cached;
    if (!cached.isEmpty()) return cached;

    const QString reg_name = readRegistryString(
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString"
    );
    if (!reg_name.isEmpty()) return (cached = cleanCpuNameInternal(reg_name));

    const QString env_name = QString::fromLocal8Bit(qgetenv("PROCESSOR_IDENTIFIER"));
    if (!env_name.isEmpty()) return (cached = cleanCpuNameInternal(env_name));

    return (cached = "CPU");
}

int fetchPhysicalCores() {
    DWORD length = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || length == 0) return 0;

    std::vector<unsigned char> buffer(length);
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &length)) return 0;

    int cores = 0;
    unsigned char *ptr = buffer.data();
    unsigned char *end = buffer.data() + length;
    while (ptr < end) {
        auto *info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(ptr);
        if (info->Relationship == RelationProcessorCore) ++cores;
        ptr += info->Size;
    }
    return cores;
}

int fetchLogicalThreads() {
    const DWORD threads = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (threads > 0) return static_cast<int>(threads);
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<int>(info.dwNumberOfProcessors);
}

double fetchCpuClockMHz() {
    const int reg_mhz = readRegistryDword(
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"~MHz"
    );
    return reg_mhz > 0 ? static_cast<double>(reg_mhz) : 0.0;
}

double queryCurrentClockMHz() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const DWORD count = info.dwNumberOfProcessors;
    if (count == 0) return 0.0;

    std::vector<PROCESSOR_POWER_INFO_EX> data(count);
    const ULONG status = CallNtPowerInformation(ProcessorInformation, nullptr, 0, data.data(),
        static_cast<ULONG>(data.size() * sizeof(PROCESSOR_POWER_INFO_EX)));
    if (status != 0) return 0.0;

    double total = 0.0;
    for (const auto &entry : data) total += static_cast<double>(entry.CurrentMhz);
    return total / static_cast<double>(count);
}

double queryCpuPerformancePercent() {
    static PDH_HQUERY query = nullptr;
    static PDH_HCOUNTER counter = nullptr;
    static bool initialized = false;
    static bool failed = false;

    if (failed) return 0.0;
    if (!initialized) {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) { failed = true; return 0.0; }
        if (PdhAddEnglishCounterW(query, L"\\Processor Information(_Total)\\% Processor Performance", 0, &counter) != ERROR_SUCCESS) {
            PdhCloseQuery(query); failed = true; return 0.0;
        }
        PdhCollectQueryData(query);
        initialized = true;
    }

    if (PdhCollectQueryData(query) != ERROR_SUCCESS) return 0.0;
    PDH_FMT_COUNTERVALUE value{};
    if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) != ERROR_SUCCESS) return 0.0;
    double val = value.doubleValue;
    return val < 0.0 ? 0.0 : val;
}
#else
QString fetchCpuName() { return "CPU"; }
int fetchPhysicalCores() { return 0; }
int fetchLogicalThreads() { return 0; }
double fetchCpuClockMHz() { return 0.0; }
#endif

} // namespace WinUtils
