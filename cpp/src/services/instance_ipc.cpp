#include "services/instance_ipc.h"

#include <QLocalSocket>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utils/logger.h"

namespace {
constexpr int kConnectTimeoutMs = 500;
constexpr int kIoTimeoutMs = 1000;
}  // namespace

InstanceIpc::InstanceIpc(QObject *parent)
    : QObject(parent) {
    server_.setParent(this);
    connect(&server_, &QLocalServer::newConnection, this, &InstanceIpc::handleNewConnection);
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

bool InstanceIpc::sendToRunningInstance(const QString &command) {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        return false;
    }

    // 서버는 개행 단위로 읽는다.
    socket.write(command.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(kIoTimeoutMs);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState) {
        socket.waitForDisconnected(kIoTimeoutMs);
    }
    return true;
}

bool InstanceIpc::listen() {
    const QString name = serverName();
    if (server_.listen(name)) {
        Logger::info("ipc", QString("Listening on %1.").arg(name));
        return true;
    }

    // 여기까지 왔다는 건 sendToRunningInstance()가 이미 실패했다는 뜻이므로
    // 살아있는 서버가 아니라 죽은 프로세스가 남긴 stale 소켓이다.
    QLocalServer::removeServer(name);
    if (server_.listen(name)) {
        Logger::info("ipc", QString("Listening on %1 (stale socket removed).").arg(name));
        return true;
    }

    Logger::warn("ipc", QString("Failed to listen on %1: %2").arg(name, server_.errorString()));
    return false;
}

void InstanceIpc::handleNewConnection() {
    while (QLocalSocket *socket = server_.nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            while (socket->canReadLine()) {
                const QString command = QString::fromUtf8(socket->readLine()).trimmed();
                Logger::info("ipc", QString("Received command=%1").arg(command));
                // 빈 문자열/알 수 없는 명령은 수신자가 무시한다(no-op).
                emit commandReceived(command);
            }
        });
    }
}
