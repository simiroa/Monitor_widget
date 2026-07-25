#pragma once

#include <QWidget>

#include "models/system_stats.h"

class QLabel;
class QPushButton;
class QProgressBar;
class QMouseEvent;

class DriveRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit DriveRowWidget(const DriveInfo &info, QWidget *parent = nullptr);
    
    void updateInfo(const DriveInfo &info);

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void openMountpoint();
    void updateUsageText();
    void toggleDisplayMode();

private:
    QString mountpoint_;
    DriveInfo current_info_;
    bool show_percentage_ = true;

    QLabel *usage_label_ = nullptr;
    QProgressBar *bar_ = nullptr;
    QLabel *speed_label_ = nullptr;
};
