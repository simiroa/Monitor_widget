#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

#include "services/speed_test_service.h"

class SpeedTestDialog : public QDialog {
    Q_OBJECT

public:
    explicit SpeedTestDialog(QWidget *parent = nullptr);
    ~SpeedTestDialog();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void reject() override;

private slots:
    void startTest();
    void handleProgress(const QString &phase, int percent, double currentMbps, double maxMbps);
    void handleFinished(const SpeedTestResult &result);

private:
    void updateGauge(double mbps, const QString &phase);

    SpeedTestService *service_ = nullptr;
    
    // UI Elements
    QLabel *title_label_ = nullptr;
    QLabel *status_label_ = nullptr;
    QLabel *speed_value_label_ = nullptr;
    QLabel *speed_unit_label_ = nullptr;
    
    // Results Grid
    QLabel *ping_val_ = nullptr;
    QLabel *jitter_val_ = nullptr;
    QLabel *down_val_ = nullptr;
    QLabel *up_val_ = nullptr;

    QPushButton *start_button_ = nullptr;
    QPushButton *close_button_ = nullptr;
    
    // Visualization
    QFrame *gauge_frame_ = nullptr;
    QWidget *gauge_fill_ = nullptr;

    QPoint drag_start_position_;
    bool is_running_ = false;
};
