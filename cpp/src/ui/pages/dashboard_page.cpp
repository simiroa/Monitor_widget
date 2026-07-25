#include "ui/pages/dashboard_page.h"
#include "ui/widgets/hourly_graph_widget.h"
#include "ui/widgets/weather_card_widget.h"
#include "ui/widgets/daily_forecast_widget.h"
#include "ui/dialogs/location_selection_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QInputDialog>

#include "utils/geo_utils.h"
#include "ui/style_tokens.h"
#include "utils/logger.h"

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent) {
    setupUi();
    
    connect(btn_hourly_, &QPushButton::clicked, this, &DashboardPage::onTabClicked);
    connect(btn_daily_, &QPushButton::clicked, this, &DashboardPage::onTabClicked);
    
    onTabClicked(); 
}

void DashboardPage::setupUi() {
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 32, 8, 8); 
    main_layout->setSpacing(6);
    main_layout->setAlignment(Qt::AlignTop);

    // --- 1. Top Section: Weather Card ---
    weather_card_ = new WeatherCardWidget(this);
    connect(weather_card_, &WeatherCardWidget::locationClicked, [this]() {
        auto cities = GeoUtils::getAvailableCities();
        QStringList names;
        names << "내 위치 (자동 감지)";
        names << "📍 현재 위치 저장";
        for (const auto &c : cities) names.append(c.name);
        
        if (weather_card_) {
            // Calculate current index
            int currentIdx = 0;
            QString currentLoc = weather_card_->getLocationName(); // We need to add this accessor or just default 0
            // Actually, let's just default to 0 (Auto) or find "currentLoc" in names
             for (int i=0; i<names.size(); ++i) {
                if (names[i] == currentLoc) {
                    currentIdx = i;
                    break;
                }
            }

            LocationSelectionDialog dlg(this, names, currentIdx);
            if (dlg.exec() == QDialog::Accepted) {
                QString item = dlg.getSelectedItem();
                if (!item.isEmpty()) {
                    if (item == names[0]) {
                        emit locationChanged({"AUTO", 0, 0, "", "", 0});
                        return;
                    }
                    if (item == names[1]) {
                        emit saveLocationRequested();
                        return;
                    }
                    for (const auto &c : cities) {
                        if (c.name == item) {
                            emit locationChanged(c);
                            break;
                        }
                    }
                }
            }
        }
    });
    main_layout->addWidget(weather_card_);
    
    // --- 2. Middle Section: Tabs ---
    tab_container_ = new QWidget(this);
    auto *tab_layout = new QHBoxLayout(tab_container_);
    tab_layout->setContentsMargins(0, 0, 0, 0);
    tab_layout->setSpacing(8);

    auto setupTabBtn = [](QPushButton *btn, const QString &text) {
        btn->setText(text);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(22);
        btn->setStyleSheet(QString(
            "QPushButton { "
            "   background: transparent; "
            "   border: none; "
            "   border-bottom: 2px solid transparent; "
            "   color: %1; "
            "   font-family: 'Outfit'; font-size: 11px; font-weight: 600; "
            "}"
            "QPushButton:checked { "
            "   color: %2; "
            "   border-bottom: none; "
            "}"
            "QPushButton:hover { color: %2; }"
        ).arg(UiStyle::kColorTextDim).arg(UiStyle::kColorAccent));
    };

    btn_hourly_ = new QPushButton(this);
    setupTabBtn(btn_hourly_, "Hourly");
    btn_hourly_->setChecked(true); 
    
    btn_daily_ = new QPushButton(this);
    setupTabBtn(btn_daily_, "Weekly");

    tab_layout->addStretch();
    tab_layout->addWidget(btn_hourly_);
    tab_layout->addWidget(btn_daily_);
    tab_layout->addStretch();
    
    main_layout->addWidget(tab_container_);

    // --- 3. Bottom Section: Stacked Content ---
    stack_ = new QStackedWidget(this);
    
    // Page 1: Hourly Graph
    hourly_graph_ = new HourlyGraphWidget(stack_);
    stack_->addWidget(hourly_graph_);
    
    // Page 2: Weekly List
    auto *daily_scroll = new QScrollArea(stack_);
    daily_scroll->setWidgetResizable(true);
    daily_scroll->setFrameShape(QFrame::NoFrame);
    daily_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    daily_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    daily_scroll->setStyleSheet("background: transparent;");
    
    daily_forecast_ = new DailyForecastWidget(daily_scroll);
    daily_scroll->setWidget(daily_forecast_);
    stack_->addWidget(daily_scroll);
    
    main_layout->addWidget(stack_);
}

void DashboardPage::updateWeather(const WeatherData &data) {
    Logger::info("dashboard", QString("Updating weather UI. Temp: %1, Sky: %2").arg(data.temp_current).arg(data.sky_code));
    
    if (weather_card_) weather_card_->updateData(data);
    
    // Update Hourly
    QVector<HourlyGraphWidget::DataPoint> graph_data;
    for (const auto &item : data.hourly_list) {
        graph_data.append({item.time, item.temp, item.icon, item.weather_type});
    }
    if (hourly_graph_) hourly_graph_->setData(graph_data);

    // Update Weekly
    if (daily_forecast_) daily_forecast_->updateData(data.daily_list);
}

void DashboardPage::onTabClicked() {
    auto *sender = qobject_cast<QPushButton*>(QObject::sender());
    
    if (sender == btn_daily_) {
        btn_hourly_->setChecked(false);
        btn_daily_->setChecked(true);
        stack_->setCurrentIndex(1);
    } else {
        btn_hourly_->setChecked(true);
        btn_daily_->setChecked(false);
        stack_->setCurrentIndex(0);
    }
}
