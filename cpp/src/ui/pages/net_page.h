#pragma once

#include <QWidget>

#include "models/process_info.h"
#include "models/system_stats.h"

class ProcessListPage;
class QLabel;
class QPushButton;
class QNetworkAccessManager;
class NetPage : public QWidget {
    Q_OBJECT

public:
    explicit NetPage(QWidget *parent = nullptr);
    ~NetPage() override;

    void updateStats(const SystemStats &stats);
    void updateProcesses(const QVector<ProcessInfo> &processes);

private:
    void fetchNetworkInfo();
    void openNetworkSettings();
    void openSpeedTest();

    void refreshTooltip();
    void updateNetInfoLabels();
    void copyToClipboard(const QString &text, const QString &labelName);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    QLabel *total_speed_label_ = nullptr;
    QLabel *speed_detail_label_ = nullptr;
    QVector<ProcessInfo> last_processes_;
    SystemStats last_stats_;
    
    // Header labels
    QLabel *local_ip_label_ = nullptr;
    QLabel *public_ip_label_ = nullptr;
    
    QString raw_public_ip_;
    QString raw_local_ip_;
    
    QPushButton *test_button_ = nullptr;
    QPushButton *settings_button_ = nullptr;

    ProcessListPage *process_list_ = nullptr;
    QNetworkAccessManager *net_manager_ = nullptr;
};
