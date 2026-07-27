#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

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

    // 이미 실행 중인 인스턴스에 command를 전달했으면 true.
    // true면 호출자는 즉시 종료해야 한다(두 번째 창이 뜨지 않도록).
    static bool sendToRunningInstance(const QString &command);

    bool listen();

signals:
    void commandReceived(const QString &command);

private slots:
    void handleNewConnection();

private:
    QLocalServer server_;
};
