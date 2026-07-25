#include "utils/logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QStringConverter>
#include <QStringList>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

namespace {
QFile g_log_file;
QTextStream g_log_stream;
QMutex g_log_mutex;
QString g_log_path;
bool g_initialized = false;
bool g_verbose = false;
QStringList g_attempts;

bool tryOpenLogFile(const QString &dir_path, const QString &stamp) {
    if (dir_path.isEmpty()) {
        return false;
    }

    QDir dir(dir_path);
    if (!dir.exists() && !dir.mkpath(".")) {
        return false;
    }

    const QString path = dir.filePath(QString("monitor_widget_%1.log").arg(stamp));
    g_log_file.setFileName(path);
    if (!g_log_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        g_attempts.push_back(QString("%1 -> %2").arg(path, g_log_file.errorString()));
        return false;
    }

    g_log_path = path;
    g_log_stream.setDevice(&g_log_file);
    g_log_stream.setEncoding(QStringConverter::Utf8);
    return true;
}

void writeLine(const QString &level, const QString &tag, const QString &message) {
    QMutexLocker locker(&g_log_mutex);
    if (!g_log_file.isOpen()) {
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString thread_id = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);

    g_log_stream << ts << " [" << level << "] [" << tag << "] [t:" << thread_id << "] "
                 << message << "\n";
    g_log_stream.flush();
}
}

void Logger::init() {
    if (g_initialized) {
        return;
    }
    g_initialized = true;
    g_verbose = qEnvironmentVariableIntValue("MONITOR_WIDGET_LOG_VERBOSE") > 0;

    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QString app_logs = app_dir.isEmpty() ? QString() : QDir(app_dir).filePath("logs");
    const QString appdata = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString current_dir = QDir::currentPath();
    const QString temp_dir = QDir::tempPath();
    const QString override_dir = QString::fromLocal8Bit(qgetenv("MONITOR_WIDGET_LOG_PATH"));

    const QString location_path = QDir(temp_dir).filePath("monitor_widget_log_location.txt");
    QFile location_file(location_path);
    if (location_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&location_file);
        stream << "init=" << stamp << "\n";
        stream << "app_dir=" << app_dir << "\n";
        stream << "app_logs=" << app_logs << "\n";
        stream << "appdata=" << appdata << "\n";
        stream << "current_dir=" << current_dir << "\n";
        stream << "temp_dir=" << temp_dir << "\n";
        stream.flush();
    }

    if (!tryOpenLogFile(override_dir, stamp) &&
        !tryOpenLogFile(app_logs, stamp) &&
        !tryOpenLogFile(appdata, stamp) &&
        !tryOpenLogFile(current_dir, stamp) &&
        !tryOpenLogFile(temp_dir, stamp)) {
        const QString fallback_path = QDir(temp_dir).filePath("monitor_widget_log_error.txt");
        QFile fallback(fallback_path);
        if (fallback.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            QTextStream stream(&fallback);
            stream << "Logger init failed.\n";
            for (const auto &entry : g_attempts) {
                stream << entry << "\n";
            }
            stream.flush();
        }
        return;
    }

    writeLine("INFO", "logger", QString("Log started path=%1 verbose=%2")
        .arg(g_log_path, g_verbose ? "1" : "0"));
    writeLine("INFO", "logger", QString("Qt=%1 pid=%2 os=%3")
        .arg(QString::fromLatin1(qVersion()))
        .arg(QCoreApplication::applicationPid())
        .arg(QSysInfo::prettyProductName()));
}

void Logger::shutdown() {
    if (!g_initialized) {
        return;
    }

    writeLine("INFO", "logger", "Log closed");
    g_log_stream.flush();
    g_log_file.close();
    g_initialized = false;
}

void Logger::info(const QString &tag, const QString &message) {
    writeLine("INFO", tag, message);
}

void Logger::warn(const QString &tag, const QString &message) {
    writeLine("WARN", tag, message);
}

void Logger::error(const QString &tag, const QString &message) {
    writeLine("ERROR", tag, message);
}

void Logger::debug(const QString &tag, const QString &message) {
    if (!g_verbose) {
        return;
    }
    writeLine("DEBUG", tag, message);
}

bool Logger::isVerbose() {
    return g_verbose;
}

QString Logger::logFilePath() {
    return g_log_path;
}
