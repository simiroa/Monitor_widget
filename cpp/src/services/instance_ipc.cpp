#include "services/instance_ipc.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QStandardPaths>

#ifdef _WIN32
#include <windows.h>
#endif

#include "utils/logger.h"

namespace {
constexpr int kConnectTimeoutMs = 500;
// 트레이 클릭 반응 지연의 상한. 파일 1개 stat이라 비용은 무시할 수준이다.
constexpr int kPollIntervalMs = 500;

// 명령 파일이 사는 폴더. exe 옆이 아니라 사용자별 고정 경로다.
// 이유 두 가지:
//   1) 스코프 일치. serverName()은 세션 단위(폴더 무관)라 위젯 사본이 두 폴더에 있으면
//      B 폴더 exe가 A 폴더 인스턴스를 "살아있다"고 감지한 뒤 명령은 B 폴더에 써서
//      영영 도달하지 않았다. 서버 이름과 같은 스코프로 옮겨야 만난다.
//   2) 쓰기 권한. 명령을 쓰는 쪽은 비승격 프로세스라 exe 폴더가 Program Files면 실패한다.
//      (HUB_CONTRACT.md §3-7: 앱은 CTX_APP_DATA_DIR 밖에 쓰지 않는다.)
// CTX_APP_DATA_DIR은 아직 허브가 주입하지 않는("planned") 값이라 폴백이 필수다.
// 폴백에 %LOCALAPPDATA%를 직접 쓰는 이유: AppDataLocation은
// QCoreApplication::setApplicationName() 이후에만 올바른 값을 주는데, deliverCommand()는
// 그 호출보다 먼저 돌아 두 프로세스가 서로 다른 경로를 보게 된다.
QString commandDir() {
    const QString injected = qEnvironmentVariable("CTX_APP_DATA_DIR");
    if (!injected.isEmpty()) {
        return injected;
    }
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    }
    return base.isEmpty() ? QString() : base + QLatin1String("/Contexthub/monitor_widget");
}

// serverName()과 commandFilePath()가 공유하는 스코프 키. 둘이 어긋나면 버그가 되돌아온다.
QString sessionKey() {
#ifdef _WIN32
    DWORD session_id = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &session_id)) {
        return QString::number(session_id);
    }
#endif
    // 세션 ID를 못 얻으면 사용자 이름으로 대체한다. 목적은 사용자 간 충돌 방지다.
    return qEnvironmentVariable("USERNAME", "default");
}

// status.json과 같은 원자적 쓰기 패턴(tmp → rename).
// ponytail: 큐가 아니라 슬롯 1개다. 500ms 안에 명령 2개가 오면 뒤엣것만 남는다.
// 트레이 클릭 간격이 그보다 짧을 일이 없어 그대로 둔다. 필요해지면 파일명에 순번을 붙인다.
bool writeCommandFile(const QString &path, const QString &command) {
    QDir().mkpath(QFileInfo(path).absolutePath());
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
    return QString("monitor_widget_ipc_%1").arg(sessionKey());
}

QString InstanceIpc::commandFilePath() {
    const QString dir = commandDir();
    // 파일명의 세션 키가 serverName()의 스코프와 짝을 이룬다.
    return dir.isEmpty() ? QString()
                         : dir + QLatin1String("/command_") + sessionKey() + QLatin1String(".txt");
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
