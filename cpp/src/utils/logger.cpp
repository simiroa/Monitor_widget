#include "utils/logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QStringConverter>
#include <QStringList>
#include <QSysInfo>
#include <QTextStream>
#include <QThread>

namespace {
// 로그 파일 상한. 한 파일이 이 크기에 도달하면 새 파일로 넘기고,
// logs 폴더에는 최신 kMaxLogFiles 개만 남긴다(총 15MB 상한).
// 이전에는 상한이 전혀 없어 단일 파일이 13MB, 폴더가 34MB까지 불어났다.
constexpr qint64 kMaxLogFileBytes = 5 * 1024 * 1024;
constexpr int kMaxLogFiles = 3;

// 고빈도 INFO 태그의 태그별 최소 기록 간격.
constexpr qint64 kHighFreqInfoIntervalMs = 60 * 1000;

QFile g_log_file;
QTextStream g_log_stream;
QMutex g_log_mutex;
QString g_log_path;
bool g_initialized = false;
bool g_verbose = false;
QStringList g_attempts;
QHash<QString, QString> g_last_info_message;
QHash<QString, qint64> g_last_info_ms;

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

void writeLineLocked(const QString &level, const QString &tag, const QString &message) {
    if (!g_log_file.isOpen()) {
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    const QString thread_id = QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16);

    g_log_stream << ts << " [" << level << "] [" << tag << "] [t:" << thread_id << "] "
                 << message << "\n";
    g_log_stream.flush();
}

// logs 폴더에 최신 kMaxLogFiles 개만 남긴다. 이름 패턴이 같은 파일만 건드리므로
// 다른 파일은 지우지 않는다.
// ponytail: 인스턴스가 1개라는 전제. 동시에 여러 인스턴스를 띄우면 서로의 로그를
// 지울 수 있으나, 이 앱은 단일 위젯이라 그 상황이 없다.
void pruneOldLogsLocked(const QString &dir_path) {
    const QFileInfoList files =
        QDir(dir_path).entryInfoList(QStringList{"monitor_widget_*.log"}, QDir::Files, QDir::Time);
    const QString current = QFileInfo(g_log_path).absoluteFilePath();
    for (int i = kMaxLogFiles; i < files.size(); ++i) {
        // 짧은 시간에 여러 번 rotation 하면 mtime 이 같아 정렬 순서가 흔들린다.
        // 지금 쓰고 있는 파일은 어떤 경우에도 지우지 않는다.
        if (files.at(i).absoluteFilePath() == current) {
            continue;
        }
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

void rotateLocked() {
    const QString previous = g_log_path;
    const QString dir_path = QFileInfo(previous).absolutePath();

    g_log_stream.flush();
    g_log_file.close();

    // 밀리초까지 넣어 같은 초에 두 번 도는 경우에도 이름이 겹치지 않게 한다.
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    if (!tryOpenLogFile(dir_path, stamp)) {
        // 새 파일을 못 열면 기존 파일로 되돌린다. 상한을 지키는 것보다 로그가
        // 아예 끊기지 않는 쪽이 중요하다.
        g_log_file.setFileName(previous);
        if (g_log_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            g_log_path = previous;
            g_log_stream.setDevice(&g_log_file);
        }
        return;
    }

    pruneOldLogsLocked(dir_path);
    writeLineLocked("INFO", "logger", QString("Rotated from %1").arg(previous));
}

// 반복 INFO 억제.
// (c) "직전과 같은 메시지면 생략"을 기본으로 골랐다. 실제 13MB 로그를 세어 보니
//     [sensor.host] "Connecting to pipe: ..." 재시도 라인이 61,230줄(전체의 66%,
//     6.3MB)로 최대 오염원이었는데, 내용이 전혀 변하지 않아 (c)로 한 줄이 된다.
//     호출부를 하나도 건드리지 않고 태그와 무관하게 동작하는 것도 장점.
// (b) [stats]/[process]는 매번 수치가 달라 (c)로 못 잡으므로 태그별 최소 간격만
//     추가로 둔다(호출부에 이미 10초 스로틀이 있어 60초로 6배 감소).
// WARN/ERROR/DEBUG는 절대 억제하지 않는다 — heap corruption·ETW/PDH/DXCore 실패
// 추적이 이 로그에 걸려 있다. verbose 모드에서도 억제하지 않는다.
bool isHighFrequencyTag(const QString &tag) {
    return tag == "stats" || tag == "process";
}

bool shouldSuppressInfoLocked(const QString &tag, const QString &message) {
    if (g_verbose) {
        return false;
    }

    if (g_last_info_message.contains(tag) && g_last_info_message.value(tag) == message) {
        return true;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (isHighFrequencyTag(tag) && g_last_info_ms.contains(tag) &&
        now - g_last_info_ms.value(tag) < kHighFreqInfoIntervalMs) {
        return true;
    }

    g_last_info_message[tag] = message;
    g_last_info_ms[tag] = now;
    return false;
}

void writeLine(const QString &level, const QString &tag, const QString &message,
               bool suppressible = false) {
    QMutexLocker locker(&g_log_mutex);
    if (suppressible && shouldSuppressInfoLocked(tag, message)) {
        return;
    }
    writeLineLocked(level, tag, message);

    if (g_log_file.isOpen() && g_log_file.size() >= kMaxLogFileBytes) {
        rotateLocked();
    }
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

    // 시작 시에도 세대 수를 맞춘다. rotation 이전에 쌓인 파일들이 여기서 정리된다.
    {
        QMutexLocker locker(&g_log_mutex);
        pruneOldLogsLocked(QFileInfo(g_log_path).absolutePath());
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
    writeLine("INFO", tag, message, /*suppressible=*/true);
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
