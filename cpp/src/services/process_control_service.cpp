#include "services/process_control_service.h"

#include <windows.h>

#include <QCoreApplication>
#include <QFileInfo>

#include "utils/logger.h"

namespace {
const char *kProtectedNames[] = {
    "System", "Registry", "smss.exe", "csrss.exe", "wininit.exe",
    "services.exe", "lsass.exe", "svchost.exe", "dwm.exe",
    "explorer.exe", "SearchIndexer.exe", "SecurityHealthService.exe",
    "python.exe", "pythonw.exe"
};
}

ProcessControlService::ProcessControlService() {
    ntdll_ = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll_) {
        ntdll_ = LoadLibraryW(L"ntdll.dll");
        loaded_ntdll_ = ntdll_ != nullptr;
    }

    if (ntdll_) {
        nt_suspend_ = reinterpret_cast<NtSuspendProcess_t>(GetProcAddress(ntdll_, "NtSuspendProcess"));
        nt_resume_ = reinterpret_cast<NtResumeProcess_t>(GetProcAddress(ntdll_, "NtResumeProcess"));
    }

    if (!nt_suspend_ || !nt_resume_) {
        Logger::warn("service.process", "Suspend/resume not available.");
    }
}

ProcessControlService::~ProcessControlService() {
    if (loaded_ntdll_ && ntdll_) {
        FreeLibrary(ntdll_);
        ntdll_ = nullptr;
    }
}

bool ProcessControlService::isProtectedName(const QString &name) const {
    for (const char *entry : kProtectedNames) {
        if (name.compare(entry, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool ProcessControlService::killProcess(qulonglong pid, QString *error_message) const {
    auto setError = [&](const QString &message) {
        if (error_message) {
            *error_message = message;
        }
    };

    Logger::info("service.process", QString("Kill requested pid=%1.").arg(pid));

    if (pid == static_cast<qulonglong>(QCoreApplication::applicationPid())) {
        setError("Refusing to kill current process.");
        Logger::warn("service.process", "Refusing to kill current process.");
        return false;
    }

    HANDLE handle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!handle) {
        setError("Access denied.");
        Logger::warn("service.process", "Access denied opening process.");
        return false;
    }

    wchar_t path_buf[MAX_PATH] = {};
    DWORD path_size = MAX_PATH;
    if (QueryFullProcessImageNameW(handle, 0, path_buf, &path_size)) {
        const QString exe_name = QFileInfo(QString::fromWCharArray(path_buf)).fileName();
        if (isProtectedName(exe_name)) {
            CloseHandle(handle);
            setError("Protected process.");
            Logger::warn("service.process", "Protected process blocked.");
            return false;
        }
    }

    const BOOL ok = TerminateProcess(handle, 1);
    CloseHandle(handle);

    if (!ok) {
        setError("TerminateProcess failed.");
        Logger::warn("service.process", "TerminateProcess failed.");
        return false;
    }

    Logger::info("service.process", "TerminateProcess success.");
    return true;
}

bool ProcessControlService::supportsSuspend() const {
    return nt_suspend_ != nullptr && nt_resume_ != nullptr;
}

bool ProcessControlService::isSuspended(qulonglong pid) const {
    return suspended_.contains(pid);
}

bool ProcessControlService::suspendProcess(qulonglong pid, QString *error_message) {
    auto setError = [&](const QString &message) {
        if (error_message) {
            *error_message = message;
        }
    };

    if (!supportsSuspend()) {
        setError("Suspend unavailable.");
        return false;
    }

    if (pid == static_cast<qulonglong>(QCoreApplication::applicationPid())) {
        setError("Refusing to suspend current process.");
        return false;
    }

    HANDLE handle = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!handle) {
        setError("Access denied.");
        Logger::warn("service.process", "Suspend access denied.");
        return false;
    }

    wchar_t path_buf[MAX_PATH] = {};
    DWORD path_size = MAX_PATH;
    if (QueryFullProcessImageNameW(handle, 0, path_buf, &path_size)) {
        const QString exe_name = QFileInfo(QString::fromWCharArray(path_buf)).fileName();
        if (isProtectedName(exe_name)) {
            CloseHandle(handle);
            setError("Protected process.");
            Logger::warn("service.process", "Suspend blocked for protected process.");
            return false;
        }
    }

    const LONG status = nt_suspend_(handle);
    CloseHandle(handle);
    if (status != 0) {
        setError("Suspend failed.");
        Logger::warn("service.process", QString("Suspend failed status=%1.").arg(status));
        return false;
    }

    suspended_.insert(pid);
    Logger::info("service.process", QString("Process suspended pid=%1.").arg(pid));
    return true;
}

bool ProcessControlService::resumeProcess(qulonglong pid, QString *error_message) {
    auto setError = [&](const QString &message) {
        if (error_message) {
            *error_message = message;
        }
    };

    if (!supportsSuspend()) {
        setError("Resume unavailable.");
        return false;
    }

    HANDLE handle = OpenProcess(PROCESS_SUSPEND_RESUME | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!handle) {
        setError("Access denied.");
        Logger::warn("service.process", "Resume access denied.");
        return false;
    }

    const LONG status = nt_resume_(handle);
    CloseHandle(handle);
    if (status != 0) {
        setError("Resume failed.");
        Logger::warn("service.process", QString("Resume failed status=%1.").arg(status));
        return false;
    }

    suspended_.remove(pid);
    Logger::info("service.process", QString("Process resumed pid=%1.").arg(pid));
    return true;
}
