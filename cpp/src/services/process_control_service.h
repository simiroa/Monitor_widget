#pragma once

#include <QString>
#include <QSet>

#ifdef _WIN32
#include <windows.h>
#endif

class ProcessControlService {
public:
    ProcessControlService();
    ~ProcessControlService();

    bool killProcess(qulonglong pid, QString *error_message = nullptr) const;
    bool isProtectedName(const QString &name) const;
    bool supportsSuspend() const;
    bool suspendProcess(qulonglong pid, QString *error_message = nullptr);
    bool resumeProcess(qulonglong pid, QString *error_message = nullptr);
    bool isSuspended(qulonglong pid) const;

private:
    using NtSuspendProcess_t = LONG (WINAPI *)(HANDLE);
    using NtResumeProcess_t = LONG (WINAPI *)(HANDLE);

    NtSuspendProcess_t nt_suspend_ = nullptr;
    NtResumeProcess_t nt_resume_ = nullptr;
    HMODULE ntdll_ = nullptr;
    bool loaded_ntdll_ = false;
    mutable QSet<qulonglong> suspended_;
};
