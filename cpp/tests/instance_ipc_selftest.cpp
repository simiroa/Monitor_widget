// instance_ipc.cpp 의 "명령 파일과 소켓 이름의 스코프가 같아야 한다" 불변식 점검.
// 이게 깨지면 위젯 사본이 두 폴더에 있을 때 트레이 명령이 조용히 사라진다.
// CMake 타깃에 넣지 않았다(테스트 인프라가 없는 레포다). 아래로 직접 빌드해서 돌린다:
//
//   QT=C:/Qt/6.10.1/mingw_64
//   $QT/bin/moc.exe cpp/src/services/instance_ipc.h -o moc_instance_ipc.cpp
//   g++ -std=c++17 -O1 -I cpp/src -I $QT/include -I $QT/include/QtCore \
//       -I $QT/include/QtNetwork \
//       cpp/tests/instance_ipc_selftest.cpp cpp/src/services/instance_ipc.cpp \
//       cpp/src/utils/logger.cpp moc_instance_ipc.cpp \
//       -L $QT/lib -lQt6Core -lQt6Network -o instance_ipc_selftest.exe
//
// 종료 코드 0 + "OK" 출력이면 통과. assert 기반이라 NDEBUG 없이 빌드할 것.

#include "services/instance_ipc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalServer>
#include <cassert>
#include <cstdio>

namespace {
QString dirOf(const QString &path) {
    return QDir::cleanPath(QFileInfo(path).absolutePath());
}
}  // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    const QString scratch = QDir(QDir::tempPath()).filePath("instance_ipc_selftest");
    QDir(scratch).removeRecursively();

    // 1) 명령 파일은 exe 옆이면 안 된다. 그게 버그의 원인이었다: serverName()은 세션 단위인데
    //    command.txt만 폴더 단위라, B 폴더 exe가 A 폴더 인스턴스를 감지하고도 명령은
    //    B 폴더에 써서 영영 도달하지 않았다.
    qunsetenv("CTX_APP_DATA_DIR");
    const QString path = InstanceIpc::commandFilePath();
    printf("command file: %s\n", qPrintable(path));
    assert(!path.isEmpty());
    assert(dirOf(path) != QDir::cleanPath(QCoreApplication::applicationDirPath()));

    // 2) 파일명의 스코프 키가 서버 이름의 스코프 키와 같아야 한다.
    const QString server = InstanceIpc::serverName();
    const QString prefix = QStringLiteral("monitor_widget_ipc_");
    assert(server.startsWith(prefix));
    const QString key = server.mid(prefix.size());
    printf("server=%s key=%s\n", qPrintable(server), qPrintable(key));
    assert(!key.isEmpty());
    assert(QFileInfo(path).fileName().contains(key));

    // 3) HUB_CONTRACT §3-7: CTX_APP_DATA_DIR 이 주입되면 그 아래에 쓴다.
    const QString injected = QDir(scratch).filePath("injected");
    qputenv("CTX_APP_DATA_DIR", QDir::toNativeSeparators(injected).toLocal8Bit());
    const QString injected_path = InstanceIpc::commandFilePath();
    printf("injected: %s\n", qPrintable(injected_path));
    assert(dirOf(injected_path) == QDir::cleanPath(injected));
    assert(QFileInfo(injected_path).fileName() == QFileInfo(path).fileName());

    // 4) 인스턴스가 없으면 false — 호출자가 정상 기동해야 한다.
    QLocalServer::removeServer(server);
    assert(InstanceIpc::deliverCommand("toggle") == false);
    assert(!QFile::exists(injected_path));

    // 5) 인스턴스가 있으면 true + 명령이 파일로 떨어진다. 디렉터리는 없으면 만든다.
    QLocalServer live;
    assert(live.listen(server));
    assert(!QDir(injected).exists());
    assert(InstanceIpc::deliverCommand("toggle") == true);
    assert(QFile::exists(injected_path));
    {
        QFile f(injected_path);
        assert(f.open(QIODevice::ReadOnly));
        assert(QString::fromUtf8(f.readAll()).trimmed() == "toggle");
    }
    // 마지막 명령만 남는 슬롯 1개 구조. 덮어쓰기가 되는지 확인한다.
    assert(InstanceIpc::deliverCommand("quit") == true);
    {
        QFile f(injected_path);
        assert(f.open(QIODevice::ReadOnly));
        assert(QString::fromUtf8(f.readAll()).trimmed() == "quit");
    }
    // 인수 없는 재실행은 두 번째 창만 막고 명령은 남기지 않는다.
    QFile::remove(injected_path);
    assert(InstanceIpc::deliverCommand("") == true);
    assert(!QFile::exists(injected_path));
    live.close();

    // 6) 인수 파서: 트레이(--hub --action X)와 퀵런처(--hub X) 두 모양.
    {
        char a0[] = "x", a1[] = "--hub", a2[] = "--action", a3[] = "toggle";
        char *argv_tray[] = {a0, a1, a2, a3};
        const HubArgs tray = InstanceIpc::parseArgs(4, argv_tray);
        assert(tray.fromHub && tray.command == "toggle");

        char b3[] = "show";
        char *argv_ql[] = {a0, a1, b3};
        const HubArgs ql = InstanceIpc::parseArgs(3, argv_ql);
        assert(ql.fromHub && ql.command == "show");

        char *argv_bare[] = {a0, a1};
        const HubArgs bare = InstanceIpc::parseArgs(2, argv_bare);
        assert(bare.fromHub && bare.command.isEmpty());
    }

    QDir(scratch).removeRecursively();
    printf("OK\n");
    return 0;
}
