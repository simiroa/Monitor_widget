#include "services/cleanup_service.h"

#include <windows.h>
#include <shellapi.h>

#include <atomic>

#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QThread>

#include "utils/logger.h"
#include "utils/win_utils.h"

// --- Private Worker Class ---

class CleanupService::Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(CleanupService *service) : service_(service), stop_(false) {}
    
    void requestStop() {
        stop_ = true;
    }

public slots:
    void scan() {
        stop_ = false;
        bool found_any = false;
        const auto items = service_->items();

        for (const auto &item : items) {
            if (stop_) break;
            
            const quint64 bytes = service_->estimateSize(item.id);
            if (bytes > 0 || item.isAction) {
                emit scanProgress(item.id, bytes);
                found_any = true;
            }
        }
        emit scanFinished(found_any);
    }

    void clean(const QStringList &ids) {
        stop_ = false;
        int success_count = 0;
        int fail_count = 0;
        quint64 total_cleared = 0;
        int total_failed_files = 0;

        for (const QString &id : ids) {
            if (stop_) break;

            const auto result = service_->run(id);
            if (result.success) {
                success_count++;
                total_cleared += result.bytesCleared;
                total_failed_files += result.failedFiles;
                emit cleanProgress(id, true, QString());
            } else {
                fail_count++;
                emit cleanProgress(id, false, result.error);
            }
        }

        QString summary;
        if (stop_) {
            summary = "Cleanup cancelled.";
        } else {
            // Format total cleared size
            QString sizeStr;
            const double mb = 1024.0 * 1024.0;
            const double gb = mb * 1024.0;
            if (total_cleared >= gb) sizeStr = QString("%1 GB").arg(total_cleared / gb, 0, 'f', 1);
            else sizeStr = QString("%1 MB").arg(total_cleared / mb, 0, 'f', 1);
            
            summary = QString("Cleared: %1").arg(sizeStr);
            if (total_failed_files > 0) {
                 summary += QString(" (%1 files locked)").arg(total_failed_files);
            }
        }
        emit cleanFinished(summary);
    }

signals:
    void scanProgress(const QString &id, quint64 size);
    void scanFinished(bool foundAny);
    void cleanProgress(const QString &id, bool success, QString error);
    void cleanFinished(const QString &summary);

private:
    CleanupService *service_;
    std::atomic<bool> stop_;
};

// --- CleanupService Implementation ---

CleanupService::CleanupService(QObject *parent) : QObject(parent) {}

CleanupService::~CleanupService() {
    cancel();
}

void CleanupService::startScan() {
    if (worker_) return;

    worker_ = new Worker(this);
    thread_ = new QThread();
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &Worker::scan);

    connect(worker_, &Worker::scanProgress, this, &CleanupService::scanProgress);
    connect(worker_, &Worker::scanFinished, this, &CleanupService::scanFinished);
    
    connect(worker_, &Worker::scanFinished, thread_, &QThread::quit);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, this, [this]() {
        worker_ = nullptr;
        thread_ = nullptr;
    });

    thread_->start();
}

void CleanupService::startClean(const QStringList &ids) {
    if (worker_) return;

    worker_ = new Worker(this);
    thread_ = new QThread();
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, [this, ids]() {
        worker_->clean(ids);
    });
    
    connect(worker_, &Worker::cleanProgress, this, &CleanupService::cleanProgress);
    connect(worker_, &Worker::cleanFinished, this, &CleanupService::cleanFinished);

    connect(worker_, &Worker::cleanFinished, thread_, &QThread::quit);
    connect(thread_, &QThread::finished, thread_, &QThread::deleteLater);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, this, [this]() {
        worker_ = nullptr;
        thread_ = nullptr;
    });

    thread_->start();
}

void CleanupService::cancel() {
    if (worker_) {
        worker_->requestStop();
    }
    if (thread_) {
        thread_->quit();
        thread_->wait();
    }
}

QVector<CleanupItem> CleanupService::items() const {
    return {
        {"temp", "Temp Files", "User and system temp files.", false, false, 0, CleanupSafety::Safe},
        {"bin", "Recycle Bin", "Empty deleted files.", false, true, 0, CleanupSafety::Safe},
        {"downloads", "Downloads", "Remove files older than 7 days in Downloads.", false, false, 7, CleanupSafety::Caution},
        {"shader", "Shader Cache", "GPU shader caches.", false, false, 0, CleanupSafety::Safe},
        {"directx", "DirectX Shader Cache", "DirectX shader cache files.", false, false, 0, CleanupSafety::Safe},
        {"engine", "3D Engine Cache", "Unreal Engine DDC.", false, false, 0, CleanupSafety::Caution},
        {"thumbnails", "Thumbnail Cache", "Windows thumbnail/icon cache.", false, false, 0, CleanupSafety::Safe},
        {"crash", "Crash Dumps", "App crash dump files.", false, false, 0, CleanupSafety::Caution},
        {"wer", "Windows Error Reports", "WER report queue/archive.", false, false, 0, CleanupSafety::Safe},
        {"logs", "Windows Logs", "CBS/DISM log files.", false, false, 0, CleanupSafety::Caution},
        {"delivery", "Delivery Optimization", "Windows update cache.", true, false, 0, CleanupSafety::Advanced},
        {"winsxs", "WinSxS Cleanup", "Component store cleanup.", true, true, 0, CleanupSafety::Advanced},
        {"wsl", "WSL Shutdown", "Shut down WSL instances.", false, true, 0, CleanupSafety::Safe},
        {"optimize", "Drive Optimization", "Run TRIM/Defrag on C:.", true, true, 0, CleanupSafety::Advanced},
        {"hiber", "Disable Hibernation", "Turn off hibernation (frees hiberfil.sys).", true, true, 0, CleanupSafety::Advanced},
    };
}

bool CleanupService::isElevated() const {
    return WinUtils::isElevated();
}

QString CleanupService::downloadsPath() const {
    const QString known = WinUtils::knownFolderPath(FOLDERID_Downloads);
    if (!known.isEmpty()) {
        return known;
    }
    const QString user_profile = WinUtils::envPath("USERPROFILE");
    if (user_profile.isEmpty()) {
        return QString();
    }
    return QDir(user_profile).filePath("Downloads");
}

QString CleanupService::tempPath() const {
    return WinUtils::envPath("TEMP");
}

QString CleanupService::windowsTempPath() const {
    const QString windir = WinUtils::envPath("WINDIR");
    if (windir.isEmpty()) {
        return QString();
    }
    return QDir(windir).filePath("Temp");
}

QString CleanupService::deliveryCachePath() const {
    return QStringLiteral("C:/Windows/ServiceProfiles/NetworkService/AppData/Local/Microsoft/DeliveryOptimization/Cache");
}

QStringList CleanupService::shaderCachePaths() const {
    QStringList paths;
    const QString local = WinUtils::envPath("LOCALAPPDATA");
    if (!local.isEmpty()) {
        paths.push_back(QDir(local).filePath("NVIDIA/DXCache"));
        paths.push_back(QDir(local).filePath("AMD/DxCache"));
    }
    return paths;
}

QStringList CleanupService::directXCachePaths() const {
    QStringList paths;
    const QString local = WinUtils::envPath("LOCALAPPDATA");
    if (!local.isEmpty()) {
        paths.push_back(QDir(local).filePath("D3DSCache"));
        paths.push_back(QDir(local).filePath("Microsoft/DirectX Shader Cache"));
    }
    return paths;
}

QString CleanupService::engineCachePath() const {
    const QString local = WinUtils::envPath("LOCALAPPDATA");
    if (local.isEmpty()) {
        return QString();
    }
    return QDir(local).filePath("UnrealEngine/Common/DerivedDataCache");
}

QStringList CleanupService::windowsLogPaths() const {
    QStringList paths;
    const QString windir = WinUtils::envPath("WINDIR");
    if (!windir.isEmpty()) {
        paths.push_back(QDir(windir).filePath("Logs/CBS"));
        paths.push_back(QDir(windir).filePath("Logs/DISM"));
    }
    return paths;
}

QStringList CleanupService::werReportPaths() const {
    QStringList paths;
    const QString program_data = WinUtils::envPath("PROGRAMDATA");
    if (!program_data.isEmpty()) {
        paths.push_back(QDir(program_data).filePath("Microsoft/Windows/WER/ReportQueue"));
        paths.push_back(QDir(program_data).filePath("Microsoft/Windows/WER/ReportArchive"));
    }
    return paths;
}

QString CleanupService::crashDumpsPath() const {
    const QString local = WinUtils::envPath("LOCALAPPDATA");
    if (local.isEmpty()) {
        return QString();
    }
    return QDir(local).filePath("CrashDumps");
}

QString CleanupService::explorerCachePath() const {
    const QString local = WinUtils::envPath("LOCALAPPDATA");
    if (local.isEmpty()) {
        return QString();
    }
    return QDir(local).filePath("Microsoft/Windows/Explorer");
}

quint64 CleanupService::estimateSize(const QString &id) const {
    if (id == "temp") {
        return WinUtils::folderSize(tempPath()) + WinUtils::folderSize(windowsTempPath());
    }
    if (id == "downloads") {
        return WinUtils::folderSize(downloadsPath());
    }
    if (id == "shader") {
        quint64 total = 0;
        for (const QString &path : shaderCachePaths()) {
            total += WinUtils::folderSize(path);
        }
        return total;
    }
    if (id == "directx") {
        quint64 total = 0;
        for (const QString &path : directXCachePaths()) {
            total += WinUtils::folderSize(path);
        }
        return total;
    }
    if (id == "engine") {
        return WinUtils::folderSize(engineCachePath());
    }
    if (id == "thumbnails") {
        return WinUtils::folderSize(explorerCachePath());
    }
    if (id == "crash") {
        return WinUtils::folderSize(crashDumpsPath());
    }
    if (id == "wer") {
        quint64 total = 0;
        for (const QString &path : werReportPaths()) {
            total += WinUtils::folderSize(path);
        }
        return total;
    }
    if (id == "logs") {
        quint64 total = 0;
        for (const QString &path : windowsLogPaths()) {
            total += WinUtils::folderSize(path);
        }
        return total;
    }
    if (id == "delivery") {
        return WinUtils::folderSize(deliveryCachePath());
    }
    return 0;
}

quint64 CleanupService::cleanDirectory(const QString &path, int &failCount) {
    QDir dir(path);
    if (!dir.exists()) return 0;

    quint64 cleared = 0;
    
    // 1. Files
    const auto files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto &info : files) {
        quint64 sz = info.size();
        if (QFile::remove(info.absoluteFilePath())) {
            cleared += sz;
        } else {
            failCount++;
        }
    }

    // 2. Directories (Recursive)
    const auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto &info : subdirs) {
        // Recurse first
        cleared += cleanDirectory(info.absoluteFilePath(), failCount);
        // Try removing the directory itself (failed if not empty)
        QDir subdir(info.absoluteFilePath());
        if (!subdir.removeRecursively()) { 
             // removeRecursively removes everything. If we want partial stats, we should not use it?
             // Actually, removeRecursively is all-or-nothing for the call usually, but Qt might delete partially.
             // But to be safe and accurate with stats, we just recursed.
             // If we recursed, we already deleted files inside. Now we just try rmdir.
             if (!dir.rmdir(info.fileName())) { 
                 // If dir not empty (because some file failed), rmdir fails.
                 // We don't count folder failure as file failure, just ignore.
             }
        }
    }
    
    return cleared;
}

quint64 CleanupService::cleanDirectoryWithRetention(const QString &path, int keepDays, int &failCount) {
    QDir dir(path);
    if (!dir.exists()) return 0;

    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-keepDays);
    quint64 cleared = 0;

    const auto files = dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto &info : files) {
        if (info.lastModified() > cutoff) {
            continue;
        }
        quint64 sz = info.size();
        if (QFile::remove(info.absoluteFilePath())) {
            cleared += sz;
        } else {
            failCount++;
        }
    }

    const auto subdirs = dir.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto &info : subdirs) {
        cleared += cleanDirectoryWithRetention(info.absoluteFilePath(), keepDays, failCount);
        QDir subdir(info.absoluteFilePath());
        if (subdir.isEmpty()) {
            if (!dir.rmdir(info.fileName())) {
                // ignore
            }
        }
    }

    return cleared;
}

quint64 CleanupService::cleanMatchingFiles(const QString &path, const QStringList &patterns, int &failCount) {
    QDir dir(path);
    if (!dir.exists()) return 0;

    quint64 cleared = 0;
    const auto files = dir.entryInfoList(patterns, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
    for (const auto &info : files) {
        quint64 sz = info.size();
        if (QFile::remove(info.absoluteFilePath())) {
            cleared += sz;
        } else {
            failCount++;
        }
    }
    return cleared;
}

bool CleanupService::runCommand(const QString &program, const QStringList &args) {
    const int exit_code = QProcess::execute(program, args);
    return exit_code == 0;
}

CleanupService::CleanResult CleanupService::run(const QString &id) const {
    CleanResult res { true, 0, 0, QString() };
    
    auto setError = [&](const QString &msg) {
        res.success = false;
        res.error = msg;
    };

    Logger::info("service.cleanup", QString("Run id=%1.").arg(id));

    if (id == "temp") {
        int fails = 0;
        res.bytesCleared += cleanDirectory(tempPath(), fails);
        res.bytesCleared += cleanDirectory(windowsTempPath(), fails);
        res.failedFiles = fails;
        Logger::info("service.cleanup", QString("Temp done. Cleared: %1, Failed Files: %2").arg(res.bytesCleared).arg(fails));
        return res;
    }
    if (id == "bin") {
        SHEmptyRecycleBinW(nullptr, nullptr, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        return res;
    }
    if (id == "downloads") {
        int fails = 0;
        res.bytesCleared += cleanDirectoryWithRetention(downloadsPath(), 7, fails);
        res.failedFiles = fails;
        return res;
    }
    if (id == "shader") {
        int fails = 0;
        for (const QString &path : shaderCachePaths()) {
            res.bytesCleared += cleanDirectory(path, fails);
        }
        res.failedFiles = fails;
        return res;
    }
    if (id == "engine") {
        int fails = 0;
        res.bytesCleared += cleanDirectory(engineCachePath(), fails);
        res.failedFiles = fails;
        return res;
    }
    if (id == "directx") {
        int fails = 0;
        for (const QString &path : directXCachePaths()) {
            res.bytesCleared += cleanDirectory(path, fails);
        }
        res.failedFiles = fails;
        return res;
    }
    if (id == "thumbnails") {
        int fails = 0;
        const QString path = explorerCachePath();
        if (!path.isEmpty()) {
            res.bytesCleared += cleanMatchingFiles(path, {"thumbcache*", "iconcache*"}, fails);
        }
        res.failedFiles = fails;
        return res;
    }
    if (id == "crash") {
        int fails = 0;
        res.bytesCleared += cleanDirectory(crashDumpsPath(), fails);
        res.failedFiles = fails;
        return res;
    }
    if (id == "wer") {
        int fails = 0;
        for (const QString &path : werReportPaths()) {
            res.bytesCleared += cleanDirectory(path, fails);
        }
        res.failedFiles = fails;
        return res;
    }
    if (id == "logs") {
        int fails = 0;
        for (const QString &path : windowsLogPaths()) {
            res.bytesCleared += cleanDirectory(path, fails);
        }
        res.failedFiles = fails;
        return res;
    }
    if (id == "delivery") {
        if (!isElevated()) {
            setError("Admin required.");
            return res;
        }
        if (!runCommand("cmd", {"/c", "net stop DoSvc"})) {
            setError("Failed to stop Delivery Optimization.");
            return res;
        }
        int fails = 0;
        res.bytesCleared += cleanDirectory(deliveryCachePath(), fails);
        res.failedFiles = fails;
        if (!runCommand("cmd", {"/c", "net start DoSvc"})) {
            setError("Failed to start Delivery Optimization.");
            return res;
        }
        return res;
    }
    if (id == "hiber") {
        if (!isElevated()) {
            setError("Admin required.");
            return res;
        }
        if (!runCommand("powercfg", {"-h", "off"})) {
            setError("powercfg failed.");
            return res;
        }
        return res;
    }
    if (id == "winsxs") {
         if (!isElevated()) {
            setError("Admin required.");
            return res;
        }
        if (!runCommand("dism", {"/online", "/cleanup-image", "/startcomponentcleanup", "/quiet"})) {
            setError("dism failed.");
            return res;
        }
        return res;
    }
    if (id == "wsl") {
        runCommand("wsl", {"--shutdown"});
        return res;
    }
    if (id == "optimize") {
        if (!isElevated()) {
            setError("Admin required.");
            return res;
        }
        if (!runCommand("defrag", {"C:", "/O"})) {
            setError("defrag failed.");
            return res;
        }
        return res;
    }

    setError("Unknown cleanup item.");
    return res;
}

#include "cleanup_service.moc"
