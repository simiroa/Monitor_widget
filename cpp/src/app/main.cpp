#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QMetaObject>
#include <QFontDatabase>
#include <QObject>
#include <QVector>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <string>

#include "models/process_info.h"
#include "models/speed_test_result.h"
#include "models/system_stats.h"
#include "services/instance_ipc.h"
#include "ui/main_window.h"
#include "utils/logger.h"
#include "utils/win_utils.h"

#ifdef _WIN32
namespace {
std::wstring quoteArg(const std::wstring &arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    if (arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }

    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
            backslashes = 0;
            continue;
        }
        if (backslashes) {
            out.append(backslashes, L'\\');
            backslashes = 0;
        }
        out.push_back(ch);
    }
    if (backslashes) {
        out.append(backslashes * 2, L'\\');
    }
    out.push_back(L'"');
    return out;
}

bool relaunchElevated() {
    wchar_t exe_path[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return false;
    }

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::wstring params;
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            if (!params.empty()) {
                params.push_back(L' ');
            }
            params += quoteArg(argv[i]);
        }
        LocalFree(argv);
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"runas", exe_path,
        params.empty() ? nullptr : params.c_str(), nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

bool ensureElevated() {
    if (WinUtils::isElevated()) {
        return true;
    }
    const bool launched = relaunchElevated();
    if (!launched) {
        MessageBoxW(nullptr,
            L"Administrator privileges are required to run Monitor Widget.",
            L"Monitor Widget",
            MB_OK | MB_ICONERROR);
    }
    return false;
}
}  // namespace
#endif

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
    // ContextHub 트레이/퀵런처가 넘기는 인수. 판단은 전부 argv로 한다.
    const HubArgs hub_args = InstanceIpc::parseArgs(argc, argv);

    // 이미 실행 중이면 명령만 넘기고 즉시 종료 — 두 번째 창이 뜨지 않도록.
    // 이 검사는 반드시 ensureElevated()보다 먼저 와야 한다. 그러지 않으면
    // 트레이/퀵런처 명령마다 UAC 프롬프트가 뜬다(명령만 전달할 프로세스도 승격되므로).
    if (InstanceIpc::deliverCommand(hub_args.command)) {
        return 0;
    }

#ifdef _WIN32
    // 순서 제약은 listen()에 대한 것이지 감지(connect)에 대한 것이 아니다.
    // 비권한 트램폴린이 IPC 서버를 먼저 잡으면 승격본이 2번째 인스턴스로 오인되어 죽으므로
    // listen()은 계속 승격 이후(아래)에 둔다.
    if (!ensureElevated()) {
        return 0;
    }
#endif

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

    auto *ipc = new InstanceIpc(&app);
    QObject::connect(ipc, &InstanceIpc::commandReceived, &window, &MainWindow::handleHubCommand);
    ipc->listen();

    window.show();

    return app.exec();
}
