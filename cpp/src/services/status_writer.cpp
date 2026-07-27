#include "services/status_writer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utils/logger.h"

namespace {
// ContextHub의 폴링 주기와 동일. 더 자주 써봐야 낭비다.
constexpr int kWriteIntervalMs = 3000;

const QString kSep = QString::fromUtf8(u8" · ");

QJsonObject makeAction(const QString &label, const QString &command, const QString &target) {
    QJsonObject action;
    action["label"] = label;
    if (!command.isEmpty()) {
        action["command"] = command;
    }
    action["target"] = target;
    return action;
}

// 수치 항목도 target="script"인 이유: "none"은 트레이에선 라벨로 잘 나오지만
// 퀵런처(QuickLauncherViewModel)에서 필터링되어 안 보인다. command="show"면
// 양쪽 다 노출되고 눌러도 창이 뜰 뿐이라 해롭지 않다.
QJsonObject makeStatAction(const QString &label) {
    return makeAction(label, "show", "script");
}

QString formatCpu(const SystemStats &stats) {
    QString text = QString("CPU  %1%").arg(qRound(stats.cpuPercent));
    if (stats.capabilities.cpuTempAvailable && stats.cpuTempC > 0.0) {
        text += kSep + QString::fromUtf8(u8"%1°C").arg(qRound(stats.cpuTempC));
    }
    if (stats.capabilities.cpuPowerAvailable && stats.cpuPowerW > 0.0) {
        text += kSep + QString("%1W").arg(qRound(stats.cpuPowerW));
    }
    return text;
}

QString formatGpu(const SystemStats &stats) {
    QStringList parts;
    if (stats.capabilities.gpuLoadAvailable) {
        parts << QString("%1%").arg(qRound(stats.gpuLoad));
    }
    if (stats.capabilities.gpuTempAvailable && stats.gpuTempC > 0.0) {
        parts << QString::fromUtf8(u8"%1°C").arg(qRound(stats.gpuTempC));
    }
    if (stats.capabilities.vramAvailable && stats.vramTotalGB > 0.0) {
        parts << QString("%1/%2GB")
                     .arg(stats.vramUsedGB, 0, 'f', 1)
                     .arg(stats.vramTotalGB, 0, 'f', 1);
    }
    if (parts.isEmpty()) {
        return "GPU  N/A";
    }
    return "GPU  " + parts.join(kSep);
}

QString formatNet(const SystemStats &stats) {
    return QString::fromUtf8(u8"NET  ↓%1 ↑%2 MB/s")
        .arg(stats.netRecvMBs, 0, 'f', 2)
        .arg(stats.netSentMBs, 0, 'f', 2);
}
}  // namespace

StatusWriter::StatusWriter(QObject *parent)
    : QObject(parent) {
    timer_.setParent(this);
    timer_.setInterval(kWriteIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &StatusWriter::writeRunning);

    // 종료 시 stopped를 안 남기면 ContextHub 트레이에 ▶ 표시와 초록 상태점이 남는다.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &StatusWriter::writeStopped);
}

void StatusWriter::start() {
    writeRunning();  // 기동 직후 한 번은 남겨 둔다.
    timer_.start();
}

void StatusWriter::updateStats(const SystemStats &stats) {
    stats_ = stats;
    has_stats_ = true;
}

void StatusWriter::writeRunning() {
    QJsonArray actions;
    // ContextHub의 IsStatusChanged는 status/pid/액션 개수/각 label만 비교한다.
    // 즉 갱신 트리거는 라벨 문자열이다.
    if (has_stats_) {
        actions.append(makeStatAction(formatCpu(stats_)));
        actions.append(makeStatAction(formatGpu(stats_)));
        actions.append(makeStatAction(formatNet(stats_)));
        actions.append(makeAction("---", QString(), "none"));
    }
    actions.append(makeAction(QString::fromUtf8(u8"열기/숨기기"), "toggle", "script"));
    actions.append(makeAction(QString::fromUtf8(u8"종료"), "quit", "script"));

    QJsonObject root;
    root["status"] = "running";
    root["pid"] = static_cast<qint64>(QCoreApplication::applicationPid());
    root["dynamic_tray_actions"] = actions;

    writeAtomic(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void StatusWriter::writeStopped() {
    timer_.stop();

    QJsonObject root;
    root["status"] = "stopped";
    writeAtomic(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString StatusWriter::filePath() const {
    return QDir(QCoreApplication::applicationDirPath()).filePath("status.json");
}

void StatusWriter::writeAtomic(const QByteArray &payload) {
    // ContextHub의 CheckStatusesInternalAsync는 파싱 실패를 catch{}로 조용히 삼킨다.
    // 부분 쓰기와 폴링이 겹치면 갱신이 무음으로 멈추므로 temp에 다 쓴 뒤 교체한다.
    const QString path = filePath();
    const QString tmp_path = path + ".tmp";

    QFile file(tmp_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (!logged_write_error_) {
            Logger::warn("status", QString("Failed to open %1: %2").arg(tmp_path, file.errorString()));
            logged_write_error_ = true;
        }
        return;
    }
    const bool written = file.write(payload) == payload.size();
    file.flush();
    file.close();
    if (!written) {
        QFile::remove(tmp_path);
        return;
    }

#ifdef _WIN32
    if (!MoveFileExW(reinterpret_cast<const wchar_t *>(tmp_path.utf16()),
            reinterpret_cast<const wchar_t *>(path.utf16()),
            MOVEFILE_REPLACE_EXISTING)) {
        if (!logged_write_error_) {
            Logger::warn("status", QString("Failed to replace %1 (error=%2)").arg(path).arg(GetLastError()));
            logged_write_error_ = true;
        }
        QFile::remove(tmp_path);
    }
#else
    QFile::remove(path);
    QFile::rename(tmp_path, path);
#endif
}
