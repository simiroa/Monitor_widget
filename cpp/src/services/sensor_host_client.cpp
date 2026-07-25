#include "services/sensor_host_client.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include "utils/logger.h"

namespace {
constexpr int kReconnectIntervalMs = 3000;
constexpr int kStaleTimeoutMs = 5000;

bool readDouble(const QJsonObject &obj, const QString &key, double &out) {
    const QJsonValue value = obj.value(key);
    if (!value.isDouble()) {
        return false;
    }
    out = value.toDouble();
    return true;
}
}  // namespace

SensorHostClient::SensorHostClient(QObject *parent)
    : QObject(parent) {
    socket_.setParent(this);
    process_.setParent(this);
    reconnect_timer_.setParent(this);

    pipe_name_ = QString("monitor_widget_sensors_%1").arg(QCoreApplication::applicationPid());

    connect(&socket_, &QLocalSocket::readyRead, this, &SensorHostClient::handleReadyRead);
    connect(&socket_, &QLocalSocket::connected, this, &SensorHostClient::handleConnected);
    connect(&socket_, &QLocalSocket::disconnected, this, &SensorHostClient::handleDisconnected);
    connect(&socket_, &QLocalSocket::errorOccurred, this, &SensorHostClient::handleSocketError);

    connect(&process_, &QProcess::errorOccurred, this, &SensorHostClient::handleProcessError);
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &SensorHostClient::handleProcessFinished);

    connect(&process_, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray data = process_.readAllStandardError();
        if (!data.isEmpty()) {
            Logger::warn("sensor.host", QString::fromUtf8(data).trimmed());
        }
    });

    reconnect_timer_.setInterval(kReconnectIntervalMs);
    connect(&reconnect_timer_, &QTimer::timeout, this, &SensorHostClient::ensureConnected);
}

void SensorHostClient::start() {
    if (started_) {
        return;
    }
    started_ = true;
    ensureConnected();
    reconnect_timer_.start();
}

void SensorHostClient::stop() {
    started_ = false;
    reconnect_timer_.stop();
    socket_.abort();
    if (process_.state() != QProcess::NotRunning) {
        process_.terminate();
    }
}

void SensorHostClient::apply(SystemStats &stats, CapabilityFlags &caps) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (snapshot_.timestampMs == 0 || (now - snapshot_.timestampMs) > kStaleTimeoutMs) {
        return;
    }

    if (snapshot_.cpuTempValid) {
        stats.cpuTempC = snapshot_.cpuTempC;
        caps.cpuTempAvailable = true;
    }
    if (snapshot_.cpuPowerValid) {
        stats.cpuPowerW = snapshot_.cpuPowerW;
        caps.cpuPowerAvailable = true;
    }

    bool used_gpu = false;
    if (snapshot_.gpuTempValid && !caps.gpuTempAvailable) {
        stats.gpuTempC = snapshot_.gpuTempC;
        caps.gpuTempAvailable = true;
        used_gpu = true;
    }
    if (snapshot_.gpuPowerValid && !caps.gpuPowerAvailable) {
        stats.gpuPowerW = snapshot_.gpuPowerW;
        caps.gpuPowerAvailable = true;
        used_gpu = true;
    }
    if (snapshot_.gpuClockValid && !caps.gpuClockAvailable) {
        stats.gpuClockMHz = snapshot_.gpuClockMHz;
        caps.gpuClockAvailable = true;
        used_gpu = true;
    }

    if (used_gpu) {
        if (caps.gpuBackend.isEmpty()) {
            caps.gpuBackend = "LHM";
        } else if (!caps.gpuBackend.contains("LHM")) {
            caps.gpuBackend += "+LHM";
        }
    }

    if (stats.gpuName.isEmpty() && !snapshot_.gpuName.isEmpty()) {
        stats.gpuName = snapshot_.gpuName;
        caps.gpuNameAvailable = true;
    }
}

void SensorHostClient::handleReadyRead() {
    buffer_.append(socket_.readAll());
    while (true) {
        const int newline = buffer_.indexOf('\n');
        if (newline < 0) {
            break;
        }
        const QByteArray line = buffer_.left(newline).trimmed();
        buffer_.remove(0, newline + 1);
        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void SensorHostClient::handleConnected() {
    if (!logged_connected_) {
        Logger::info("sensor.host", QString("Connected to %1.").arg(pipe_name_));
        logged_connected_ = true;
    }
}

void SensorHostClient::handleDisconnected() {
    socket_.abort();
}

void SensorHostClient::handleSocketError(QLocalSocket::LocalSocketError error) {
    if (error == QLocalSocket::ServerNotFoundError || error == QLocalSocket::ConnectionRefusedError) {
        return;
    }
    if (!logged_socket_error_) {
        Logger::warn("sensor.host", QString("Socket error=%1.").arg(static_cast<int>(error)));
        logged_socket_error_ = true;
    }
}

void SensorHostClient::handleProcessError(QProcess::ProcessError error) {
    if (!logged_process_error_) {
        Logger::warn("sensor.host", QString("Process error=%1.").arg(static_cast<int>(error)));
        logged_process_error_ = true;
    }
}

void SensorHostClient::handleProcessFinished(int exit_code, QProcess::ExitStatus status) {
    if (status == QProcess::CrashExit) {
        Logger::error("sensor.host", QString("Process crashed with code %1").arg(exit_code));
    } else if (exit_code != 0) {
        Logger::warn("sensor.host", QString("Process exited with code %1").arg(exit_code));
    } else {
        Logger::info("sensor.host", "Process finished normally.");
    }
    
    // Schedule restart if needed
    if (started_) {
        QTimer::singleShot(kReconnectIntervalMs, this, &SensorHostClient::ensureConnected);
    }
}

void SensorHostClient::ensureConnected() {
    if (!started_) {
        return;
    }

    if (socket_.state() == QLocalSocket::ConnectedState ||
        socket_.state() == QLocalSocket::ConnectingState) {
        return;
    }

    if (!logged_started_) {
       Logger::info("sensor.host", QString("Connecting to pipe: %1").arg(pipe_name_));
    }

    if (process_.state() == QProcess::NotRunning) {
        const QString path = resolveHostPath();
        if (path.isEmpty()) {
            if (!logged_missing_host_) {
                Logger::warn("sensor.host", "sensor_host.exe not found; external sensors disabled.");
                logged_missing_host_ = true;
            }
            return;
        }
        startProcess(path);
    }

    socket_.connectToServer(pipe_name_);
}

QString SensorHostClient::resolveHostPath() const {
    const QString app_dir = QCoreApplication::applicationDirPath();
    const QString direct = QDir(app_dir).filePath("sensor_host.exe");
    if (QFileInfo::exists(direct)) {
        return direct;
    }

    const QString rel_release = QDir(app_dir).filePath("../sensor_host/bin/Release/net8.0/sensor_host.exe");
    if (QFileInfo::exists(rel_release)) {
        return QDir::cleanPath(rel_release);
    }

    const QString rel_debug = QDir(app_dir).filePath("../sensor_host/bin/Debug/net8.0/sensor_host.exe");
    if (QFileInfo::exists(rel_debug)) {
        return QDir::cleanPath(rel_debug);
    }

    const QString sub_dir = QDir(app_dir).filePath("sensor_host/sensor_host.exe");
    if (QFileInfo::exists(sub_dir)) {
        return sub_dir;
    }

    return {};
}

void SensorHostClient::startProcess(const QString &path) {
    QStringList args;
    args << "--pipe" << pipe_name_;
    args << "--interval" << "1000";
    args << "--verbose";

    process_.setProgram(path);
    process_.setArguments(args);
    process_.setWorkingDirectory(QFileInfo(path).absolutePath());
    process_.start();

    if (!logged_started_) {
        Logger::info("sensor.host", QString("Started %1.").arg(path));
        logged_started_ = true;
    }
}

void SensorHostClient::parseLine(const QByteArray &line) {
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
    if (doc.isNull() || !doc.isObject()) {
        return;
    }

    const QJsonObject obj = doc.object();
    double value = 0.0;

    snapshot_.cpuTempValid = false;
    snapshot_.cpuPowerValid = false;
    snapshot_.gpuTempValid = false;
    snapshot_.gpuPowerValid = false;
    snapshot_.gpuClockValid = false;

    if (readDouble(obj, "timestampMs", value)) {
        snapshot_.timestampMs = static_cast<qint64>(value);
    } else {
        snapshot_.timestampMs = QDateTime::currentMSecsSinceEpoch();
    }

    if (readDouble(obj, "cpuTempC", value)) {
        snapshot_.cpuTempC = value;
        snapshot_.cpuTempValid = true;
    }

    if (readDouble(obj, "cpuPowerW", value)) {
        snapshot_.cpuPowerW = value;
        snapshot_.cpuPowerValid = true;
    }

    if (readDouble(obj, "gpuTempC", value)) {
        snapshot_.gpuTempC = value;
        snapshot_.gpuTempValid = true;
    }

    if (readDouble(obj, "gpuPowerW", value)) {
        snapshot_.gpuPowerW = value;
        snapshot_.gpuPowerValid = true;
    }

    if (readDouble(obj, "gpuClockMHz", value)) {
        snapshot_.gpuClockMHz = value;
        snapshot_.gpuClockValid = true;
    }

    const QJsonValue cpu_name = obj.value("cpuName");
    if (cpu_name.isString()) {
        snapshot_.cpuName = cpu_name.toString();
    }

    const QJsonValue gpu_name = obj.value("gpuName");
    if (gpu_name.isString()) {
        snapshot_.gpuName = gpu_name.toString();
    }
}
