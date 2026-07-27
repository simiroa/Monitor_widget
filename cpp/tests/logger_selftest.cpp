// logger.cpp 의 rotation / 반복 INFO 억제 자체 점검.
// CMake 타깃에 넣지 않았다(테스트 인프라가 없는 레포다). 아래 한 줄로 직접 빌드해서 돌린다:
//
//   g++ -std=c++17 -O1 -I cpp/src -I C:/Qt/6.10.1/mingw_64/include \
//       -I C:/Qt/6.10.1/mingw_64/include/QtCore \
//       cpp/tests/logger_selftest.cpp cpp/src/utils/logger.cpp \
//       -L C:/Qt/6.10.1/mingw_64/lib -lQt6Core -o logger_selftest.exe
//
// 종료 코드 0 + "OK" 출력이면 통과. assert 기반이라 NDEBUG 없이 빌드할 것.

#include "utils/logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <cassert>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    const QString dir = QDir(QDir::tempPath()).filePath("logger_selftest");
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);
    qputenv("MONITOR_WIDGET_LOG_PATH", dir.toLocal8Bit());
    qunsetenv("MONITOR_WIDGET_LOG_VERBOSE");

    Logger::init();
    printf("log path: %s\n", qPrintable(Logger::logFilePath()));

    // 1) 같은 메시지 반복 -> 1줄만 (sensor.host 재접속 시도 패턴, 실제 로그의 66%였다).
    for (int i = 0; i < 500; ++i) {
        Logger::info("sensor.host", "Connecting to pipe: monitor_widget_sensors_1234");
    }
    // 2) WARN/ERROR 는 반복돼도 절대 억제되지 않는다.
    for (int i = 0; i < 5; ++i) {
        Logger::warn("sensor.host", "same warn line");
        Logger::error("sensor.host", "same error line");
    }
    // 3) 고빈도 태그는 내용이 매번 달라도 60초에 1줄.
    for (int i = 0; i < 100; ++i) {
        Logger::info("stats", QString("cpu=%1").arg(i));
    }
    // 4) 일반 태그는 내용이 다르면 전부 기록된다.
    for (int i = 0; i < 10; ++i) {
        Logger::info("ui.main", QString("step %1").arg(i));
    }

    QFile f(Logger::logFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        printf("cannot read log\n");
        return 1;
    }
    const QString body = QString::fromUtf8(f.readAll());
    f.close();

    printf("connecting=%d warn=%d error=%d stats=%d ui.main=%d\n",
           body.count("Connecting to pipe"), body.count("[WARN]"), body.count("[ERROR]"),
           body.count("[stats]"), body.count("[ui.main]"));
    assert(body.count("Connecting to pipe") == 1);
    assert(body.count("[WARN]") == 5);
    assert(body.count("[ERROR]") == 5);
    assert(body.count("[stats]") == 1);
    assert(body.count("[ui.main]") == 10);

    // 5) rotation: 상한(5MB)을 넘길 때까지 서로 다른 INFO 를 쏟아붓는다.
    const QString filler(4000, QLatin1Char('x'));
    for (int i = 0; i < 6000; ++i) {
        Logger::info("bulk", QString("%1 %2").arg(i).arg(filler));
    }

    const QStringList files =
        QDir(dir).entryList(QStringList{"monitor_widget_*.log"}, QDir::Files, QDir::Time);
    printf("files after flood: %d\n", int(files.size()));
    for (const QString &name : files) {
        const qint64 size = QFileInfo(QDir(dir).filePath(name)).size();
        printf("  %s  %lld bytes\n", qPrintable(name), (long long)size);
        // rotation 은 기록 후에 도므로 마지막 한 줄만큼 초과할 수 있다.
        assert(size <= 6LL * 1024 * 1024);
    }
    assert(files.size() > 1);   // rotation 이 실제로 돌았다
    assert(files.size() <= 3);  // 세대 상한이 지켜졌다

    Logger::shutdown();
    printf("OK\n");
    return 0;
}
