#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QPushButton>

#include "services/weather_service.h"

class HourlyGraphWidget;
class WeatherCardWidget;
class DailyForecastWidget;

class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);

public slots:
    void updateWeather(const WeatherData &data);

signals:
    void locationChanged(const LocationConfig &loc);
    void saveLocationRequested();

private slots:
    void onTabClicked();

private:
    void setupUi();
    void updateTabStyle();

    // Top Section: Current Weather Card
    WeatherCardWidget *weather_card_ = nullptr;

    // Middle Section: Tabs
    QWidget *tab_container_ = nullptr;
    QPushButton *btn_hourly_ = nullptr;
    QPushButton *btn_daily_ = nullptr;

    // Bottom Section: Content
    QStackedWidget *stack_ = nullptr;
    HourlyGraphWidget *hourly_graph_ = nullptr; // Existing
    DailyForecastWidget *daily_forecast_ = nullptr; // New
};
