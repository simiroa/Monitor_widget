#include "services/instance_ipc.h"

#include <QCoreApplication>
#include <QFile>
#include <QLocalSocket>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utils/logger.h"

namespace {
constexpr int kConnectTimeoutMs = 500;
// 트레이 클릭 반응 지연의 상한. 파일 1개 stat이라 비용은 무시할 수준이다.
constexpr int kPollIntervalMs = 500;

// exe가 있는 폴더. QCoreApplication 생성 전에도 불려야 하므로
// applicationDirPath()를 쓸 수 없다(명령 프로세스는 QApplication을 만들지 않는다).
QString exeDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return QString();
    }
    const QString path = QString::fromWCharArray(buf, static_cast<int>(len));
    const int slash = path.lastIndexOf(QLatin1Char('\\'));
    return slash > 0 ? path.left(slash) : QString();
#else
    return QCoreApplication::applicationDirPath();
#endif
}

// status.json과 같은 원자적 쓰기 패턴(tmp → rename).
// ponytail: 큐가 아니라 슬롯 1개다. 500ms 안에 명령 2개가 오면 뒤엣것만 남는다.
// 트레이 클릭 간격이 그보다 짧을 일이 없어 그대로 둔다. 필요해지면 파일명에 순번을 붙인다.
bool writeCommandFile(const QString &path, const QString &command) {
    const QString tmp_path = path + ".tmp";
    QFile file(tmp_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(command.toUtf8());
    file.close();

#ifdef _WIN32
    if (MoveFileExW(reinterpret_cast<const wchar_t *>(tmp_path.utf16()),
            reinterpret_cast<const wchar_t *>(path.utf16()),
            MOVEFILE_REPLACE_EXISTING)) {
        return true;
    }
    QFile::remove(tmp_path);
    return false;
#else
    QFile::remove(path);
    return QFile::rename(tmp_path, path);
#endif
}
}  // namespace

InstanceIpc::InstanceIpc(QObject *parent)
    : QObject(parent) {
    server_.setParent(this);
    connect(&server_, &QLocalServer::newConnection, this, &InstanceIpc::handleNewConnection);
    poll_timer_.setParent(this);
    poll_timer_.setInterval(kPollIntervalMs);
    connect(&poll_timer_, &QTimer::timeout, this, &InstanceIpc::pollCommandFile);
}

HubArgs InstanceIpc::parseArgs(int argc, char *argv[]) {
    HubArgs result;
    for (int i = 1; i < argc; ++i) {
        const QString token = QString::fromLocal8Bit(argv[i]).trimmed();
        if (token == "--hub") {
            result.fromHub = true;
            continue;
        }
        if (token == "--action") {
            // 값이 빠져 있으면 그냥 무시한다.
            if (i + 1 < argc) {
                result.command = QString::fromLocal8Bit(argv[++i]).trimmed();
            }
            continue;
        }
        if (token.startsWith("--")) {
            // 모르는 플래그로 기동을 막지 않는다.
            continue;
        }
        if (result.command.isEmpty()) {
            result.command = token;
        }
    }
    return result;
}

QString InstanceIpc::serverName() {
#ifdef _WIN32
    DWORD session_id = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &session_id)) {
        return QString("monitor_widget_ipc_%1").arg(session_id);
    }
#endif
    // 세션 ID를 못 얻으면 사용자 이름으로 대체한다. 목적은 사용자 간 충돌 방지다.
    return QString("monitor_widget_ipc_%1").arg(qEnvironmentVariable("USERNAME", "default"));
}

QString InstanceIpc::commandFilePath() {
    const QString dir = exeDir();
    return dir.isEmpty() ? QString() : dir + QLatin1String("/command.txt");
}

bool InstanceIpc::deliverCommand(const QString &command) {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        // 서버 자체가 없으면 우리가 정상 기동해야 한다.
        // 그 외 실패(승격 인스턴스라 접근 거부 등)는 "살아있다"로 본다.
        if (socket.error() == QLocalSocket::ServerNotFoundError ||
            socket.error() == QLocalSocket::ConnectionRefusedError) {
            return false;
        }
    } else {
        socket.abort();
    }

    // 인수 없는 재실행: 두 번째 창만 막고 끝낸다.
    if (command.isEmpty()) {
        return true;
    }
    if (!writeCommandFile(commandFilePath(), command)) {
        // 명령 전달은 실패했지만 인스턴스는 살아있다. 두 번째 창을 띄우면 더 나쁘다.
        Logger::warn("ipc", QString("Failed to write command file for command=%1").arg(command));
    }
    return true;
}

bool InstanceIpc::listen() {
    const QString name = serverName();
    bool ok = server_.listen(name);
    if (!ok) {
        // 여기까지 왔다는 건 deliverCommand()가 이미 서버를 못 찾았다는 뜻이므로
        // 살아있는 서버가 아니라 죽은 프로세스가 남긴 stale 소켓이다.
        QLocalServer::removeServer(name);
        ok = server_.listen(name);
        if (ok) {
            Logger::info("ipc", QString("Listening on %1 (stale socket removed).").arg(name));
        }
    } else {
        Logger::info("ipc", QString("Listening on %1.").arg(name));
    }

    if (!ok) {
        Logger::warn("ipc", QString("Failed to listen on %1: %2").arg(name, server_.errorString()));
        return false;
    }

    // 기동 직전에 남아있던 명령은 우리 것이 아니다(예: 죽은 인스턴스 앞으로 온 quit).
    const QString command_path = commandFilePath();
    if (!command_path.isEmpty()) {
        QFile::remove(command_path);
    }
    poll_timer_.start();
    return true;
}

// 소켓은 생존 감지 전용이라 페이로드가 없다. 받아서 버리지 않으면
// 대기 큐(기본 30개)가 차서 이후 감지가 실패한다.
void InstanceIpc::handleNewConnection() {
    while (QLocalSocket *socket = server_.nextPendingConnection()) {
        socket->abort();
        socket->deleteLater();
    }
}

void InstanceIpc::pollCommandFile() {
    const QString path = commandFilePath();
    if (path.isEmpty() || !QFile::exists(path)) {
        return;
    }

    QFile file(path);
    QString command;
    if (file.open(QIODevice::ReadOnly)) {
        command = QString::fromUtf8(file.readAll()).trimmed();
        file.close();
    }
    // 읽기에 실패해도 지운다. 안 지우면 매 폴링마다 같은 실패를 반복한다.
    QFile::remove(path);

    if (command.isEmpty()) {
        return;
    }
    Logger::info("ipc", QString("Received command=%1").arg(command));
    // 알 수 없는 명령은 수신자가 무시한다(no-op).
    emit commandReceived(command);
}
