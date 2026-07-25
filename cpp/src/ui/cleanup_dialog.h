#pragma once

#include <QDialog>
#include <QVector>
#include <QHash>

class QLabel;
class QPushButton;
class QProgressBar;
class QCheckBox;
class QThread;
class QVBoxLayout;

class CleanupService;
class QMouseEvent;

class CleanupDialog : public QDialog {
    Q_OBJECT

public:
    explicit CleanupDialog(CleanupService *service, QWidget *parent = nullptr);
    ~CleanupDialog() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void startCleanup();

private:
    static QString formatSize(quint64 bytes);

    CleanupService *service_ = nullptr;
    QVector<QCheckBox *> checkboxes_;
    QVBoxLayout *list_layout_ = nullptr;
    QLabel *summary_value_label_ = nullptr;
    QLabel *summary_desc_label_ = nullptr;
    QProgressBar *progress_bar_ = nullptr;
    QPushButton *run_button_ = nullptr;
    QPushButton *rescan_button_ = nullptr;
    QPushButton *cancel_button_ = nullptr;
    QHash<QString, QLabel*> status_labels_;
    QHash<QString, QLabel*> size_labels_;
    QHash<QString, QWidget*> row_widgets_;
    
private slots:
    void onScanProgress(const QString &id, quint64 size);
    void onScanFinished(bool foundAny);
    void onCleanProgress(const QString &id, bool success, const QString &error);
    void onCleanFinished(const QString &summary);
    void updateTotalSize();
    void startRescan();
    void cancelCleanup();

private:
    QPoint drag_start_position_;
};
