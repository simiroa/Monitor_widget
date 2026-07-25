#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

enum class CleanupSafety {
    Safe,
    Caution,
    Advanced
};

struct CleanupItem {
    QString id;
    QString label;
    QString description;
    bool requiresAdmin = false;
    bool isAction = false;
    int keepDays = 0;
    CleanupSafety safety = CleanupSafety::Safe;
};

class QThread;

class CleanupService : public QObject {
    Q_OBJECT

public:
    explicit CleanupService(QObject *parent = nullptr);
    ~CleanupService() override;

    QVector<CleanupItem> items() const;
    quint64 estimateSize(const QString &id) const;
    
    struct CleanResult {
        bool success;
        quint64 bytesCleared;
        int failedFiles;
        QString error;
    };
    
    CleanResult run(const QString &id) const;
    bool isElevated() const;

public slots:
    void startScan();
    void startClean(const QStringList &ids);
    void cancel();

signals:
    void scanProgress(const QString &id, quint64 size);
    void scanFinished(bool foundAny);
    
    void cleanProgress(const QString &id, bool success, QString error);
    void cleanFinished(const QString &summary);

private:
    // Helper to clean directory and count stats
    static quint64 cleanDirectory(const QString &path, int &failCount);
    static quint64 cleanDirectoryWithRetention(const QString &path, int keepDays, int &failCount);
    static quint64 cleanMatchingFiles(const QString &path, const QStringList &patterns, int &failCount);

    QString downloadsPath() const;
    QString tempPath() const;
    QString windowsTempPath() const;
    QString deliveryCachePath() const;
    QStringList shaderCachePaths() const;
    QStringList directXCachePaths() const;
    QString engineCachePath() const;
    QStringList windowsLogPaths() const;
    QStringList werReportPaths() const;
    QString crashDumpsPath() const;
    QString explorerCachePath() const;

    static bool runCommand(const QString &program, const QStringList &args);

    class Worker;
    Worker *worker_ = nullptr;
    QThread *thread_ = nullptr;
};
