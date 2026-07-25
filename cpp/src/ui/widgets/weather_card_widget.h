#pragma once

#include <QWidget>
#include <QTime>
#include "models/weather_data.h"

class QLabel;
class QPushButton;

class WeatherCardWidget : public QWidget {
    Q_OBJECT

public:
    explicit WeatherCardWidget(QWidget *parent = nullptr);
    void updateData(const WeatherData &data);
    QString getLocationName() const;

signals:
    void locationClicked(); // Triggers the location dialog logic in parent

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void updateBackground();

private:
    void setupUi();

    // Color Helpers
    QString getWindColor(float val) const;
    QString getHumidColor(float val) const;
    QString getPrecipColor(float val, bool isProb) const;
    QString getDustColor(const QString &val) const;

    // UI Elements
    QPushButton *loc_btn_ = nullptr;
    QLabel *icon_label_ = nullptr;
    QLabel *temp_label_ = nullptr;
    QLabel *cond_label_ = nullptr;
    QLabel *bg_watermark_ = nullptr;

    // Extra Data
    QLabel *wind_value_ = nullptr;
    QLabel *humidity_value_ = nullptr;
    QLabel *precip_value_ = nullptr;
    QLabel *dust_value_ = nullptr;

    // Warning / Status Section (Right Side)
    QWidget *right_status_box_ = nullptr;
    QLabel *right_status_icon_ = nullptr;
    QLabel *right_status_text_ = nullptr;
    
    // Internal State
    QTimer *refresh_timer_ = nullptr;
    QTime sunrise_time_ = QTime(6, 0);
    QTime sunset_time_ = QTime(19, 0);
    QString current_theme_hex_ = "#255, 213, 79"; // Default base color for gradient
};
