#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QTimer>

// ContextHub가 넘기는 인수. 두 경로에서 argv 모양이 다르다.
//   트레이 메뉴  : --hub --action toggle
//   퀵런처 카드  : --hub toggle
// 판단은 전부 argv로 한다. ShellExecute runas는 AppInfo 서비스를 경유하므로
// CTX_APP_ROOT 같은 환경변수 보존을 보장할 수 없다.
struct HubArgs {
    bool fromHub = false;  // --hub. 지금은 소비처가 없고 향후 분기용으로만 보관한다.
    QString command;       // --action <cmd> 또는 플래그가 아닌 첫 잔여 토큰.
};

// 단일 인스턴스 보장 + 기존 인스턴스로의 명령 전달.
class InstanceIpc : public QObject {
    Q_OBJECT

public:
    explicit InstanceIpc(QObject *parent = nullptr);

    static HubArgs parseArgs(int argc, char *argv[]);

    // 다중 사용자 환경 충돌 방지를 위해 세션 ID를 이름에 넣는다.
    static QString serverName();

    // 실행 중인 인스턴스를 찾았으면 true. true면 호출자는 즉시 종료해야 한다
    // (두 번째 창이 뜨지 않도록). command가 비어있지 않으면 명령 파일로 전달한다.
    //
    // 소켓은 "생존 감지"에만 쓰고 명령은 파일로 넘긴다. 승격(High IL) 인스턴스가 만든
    // named pipe는 High 무결성 레이블을 갖고, MIC 기본 정책 NO_WRITE_UP 때문에
    // 비승격(Medium) 클라이언트는 write 접근이 거부된다.
    // 실측(Qt 6.10.1, QLocalSocket): Low→Medium connect = SocketAccessError(err=3),
    // Medium→Medium = 성공. 즉 승격 인스턴스에 소켓으로 명령을 보낼 수 없다.
    // 다만 실패 코드가 ServerNotFoundError와 다르므로 "살아있다"는 사실은 알 수 있다.
    static bool deliverCommand(const QString &command);

    bool listen();

    // 명령 파일 경로. 승격 인스턴스가 폴링해서 소비하고 지운다.
    // exe 폴더가 아니라 CTX_APP_DATA_DIR(없으면 %LOCALAPPDATA%\Contexthub\monitor_widget)
    // 아래이며 파일명에 세션 키가 들어간다 — serverName()과 스코프를 맞추기 위해서다.
    // exe 옆에 두면 사본이 두 폴더에 있을 때 명령이 실행 중인 인스턴스에 도달하지 못한다.
    static QString commandFilePath();

signals:
    void commandReceived(const QString &command);

private slots:
    void handleNewConnection();
    void pollCommandFile();

private:
    QLocalServer server_;
    QTimer poll_timer_;
};
