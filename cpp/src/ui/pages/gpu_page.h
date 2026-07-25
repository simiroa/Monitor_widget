#pragma once

#include <QWidget>

#include "models/system_stats.h"

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class MonitorControlService;

class GpuPage : public QWidget {
    Q_OBJECT

public:
    explicit GpuPage(MonitorControlService *control_service, QWidget *parent = nullptr);

    void updateStats(const SystemStats &stats);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void syncControls();
    void applyRefreshRate();
    void toggleNightMode(bool enabled);
    void toggleHdr(bool enabled);
    void brightnessChanged(int value);

private:
    MonitorControlService *control_service_ = nullptr;

    QLabel *name_label_ = nullptr;
    QLabel *vram_label_ = nullptr;
    QLabel *stats_label_ = nullptr;
    QLabel *notice_label_ = nullptr;
    QLabel *control_notice_label_ = nullptr;

    QPushButton *night_button_ = nullptr;
    QPushButton *hdr_button_ = nullptr;
    QPushButton *sleep_button_ = nullptr;
    QSlider *brightness_slider_ = nullptr;
    QLabel *brightness_value_ = nullptr;
    QComboBox *refresh_combo_ = nullptr;
    QPushButton *refresh_apply_ = nullptr;
};
