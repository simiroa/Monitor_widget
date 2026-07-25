#pragma once

#include <QString>

namespace Logger {
void init();
void shutdown();
void info(const QString &tag, const QString &message);
void warn(const QString &tag, const QString &message);
void error(const QString &tag, const QString &message);
void debug(const QString &tag, const QString &message);
bool isVerbose();
QString logFilePath();
}
