#pragma once

#include <QString>

#ifdef _WIN32
#include <ShlObj.h>
#endif

namespace WinUtils {
bool isElevated();
QString knownFolderPath(const KNOWNFOLDERID &id);
QString envPath(const char *name);
bool removeContents(const QString &path);
quint64 folderSize(const QString &path);

// CPU Info Helpers
QString fetchCpuName();
int fetchPhysicalCores();
int fetchLogicalThreads();
double fetchCpuClockMHz();
#ifdef _WIN32
double queryCurrentClockMHz();
double queryCpuPerformancePercent();
#endif
}
