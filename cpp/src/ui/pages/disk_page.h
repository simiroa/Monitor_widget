#pragma once

#include <QWidget>
#include <QMap>

#include "models/system_stats.h"
#include "services/cleanup_service.h"

class QLabel;
class QVBoxLayout;
class QPushButton;
class DriveRowWidget;

class DiskPage : public QWidget {
    Q_OBJECT

public:
    explicit DiskPage(QWidget *parent = nullptr);

    void updateStats(const SystemStats &stats);

private slots:
    void openCleanup();

private:
    QLabel *speed_label_ = nullptr;
    QLabel *header_stats_label_ = nullptr;
    QLabel *header_name_label_ = nullptr;
    QVBoxLayout *drive_layout_ = nullptr;
    QPushButton *cleanup_button_ = nullptr;
    CleanupService cleanup_service_;

    QMap<QString, DriveRowWidget*> drive_widgets_;
};
