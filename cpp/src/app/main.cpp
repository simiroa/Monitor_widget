#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>
#include <QFontDatabase>
#include <QObject>
#include <QVector>

#include "models/process_info.h"
#include "models/speed_test_result.h"
#include "models/system_stats.h"
#include "ui/main_window.h"
#include "utils/logger.h"

namespace {
void loadFonts() {
    // Load Material Symbols
    if (QFontDatabase::addApplicationFont(":/fonts/MaterialSymbolsOutlined.ttf") == -1) {
        Logger::warn("app", "Failed to load Material Symbols font");
    }
    // Load Outfit
    if (QFontDatabase::addApplicationFont(":/fonts/Outfit-Bold.ttf") == -1) {
        Logger::warn("app", "Failed to load Outfit-Bold font");
    }
    if (QFontDatabase::addApplicationFont(":/fonts/Outfit-Regular.ttf") == -1) {
        Logger::warn("app", "Failed to load Outfit-Regular font");
    }
}
}

int main(int argc, char *argv[]) {
    QCoreApplication::setOrganizationName("HG");
    QCoreApplication::setApplicationName("MonitorWidgetCpp");

    qRegisterMetaType<SystemStats>("SystemStats");
    qRegisterMetaType<ProcessInfo>("ProcessInfo");
    qRegisterMetaType<QVector<ProcessInfo>>("QVector<ProcessInfo>");
    qRegisterMetaType<SpeedTestResult>("SpeedTestResult");

    QApplication app(argc, argv);
    loadFonts();

    Logger::init();
    Logger::info("app", QString("Log path=%1").arg(Logger::logFilePath()));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        Logger::shutdown();
    });

    MainWindow window;
    window.show();

    return app.exec();
}
