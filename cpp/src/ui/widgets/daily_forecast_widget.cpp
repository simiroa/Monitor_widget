#include "ui/widgets/daily_forecast_widget.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>

DailyForecastWidget::DailyForecastWidget(QWidget *parent) : QWidget(parent) {
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(4, 0, 4, 0);
    layout_->setSpacing(8);
    layout_->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
}

void DailyForecastWidget::updateData(const QVector<DailyItem> &data) {
    // Clear old items
    QLayoutItem *child;
    while ((child = layout_->takeAt(0)) != nullptr) {
        if (child->widget()) delete child->widget();
        delete child;
    }

    for (const auto &d : data) {
        // Determine card theme color
        QString cardColor = "#FFD700"; // Sunny
        if (d.weather_type == 1) cardColor = "#9CA3AF"; // Cloudy
        else if (d.weather_type == 2) cardColor = "#0047AB"; // Rain
        else if (d.weather_type == 3) cardColor = "#6699CC"; // Snow
        
        auto *card = new QWidget(this);
        card->setFixedSize(58, 80); 
        card->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(UiStyle::kColorSectionBg)); 
        
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(2, 4, 2, 4); 
        cl->setSpacing(0); 
        cl->setAlignment(Qt::AlignCenter);
        
        auto *lday = new QLabel(d.day, card);
        lday->setStyleSheet(QString("font-family: 'Inter'; color: %1; font-size: 10px; font-weight: 600;").arg(UiStyle::kColorTextDim));
        lday->setAlignment(Qt::AlignCenter);
        
        auto *licon = new QLabel(card);
        licon->setStyleSheet(QString("font-family: '%1'; font-size: 22px; color: %2;").arg(Icons::fontName()).arg(cardColor)); 
        licon->setText(d.icon);
        licon->setAlignment(Qt::AlignCenter);
        
        auto *ltemp_max = new QLabel(card);
        auto *ltemp_min = new QLabel(card);
        if (d.min_temp == -99) {
             ltemp_max->setText("--°");
             ltemp_min->setText("--°");
        } else {
             ltemp_max->setText(QString("%1°").arg(d.max_temp));
             ltemp_min->setText(QString("%1°").arg(d.min_temp));
        }
        ltemp_max->setStyleSheet(QString("font-family: 'Inter'; color: %1; font-size: 10px; font-weight: 700;").arg(UiStyle::kColorTextMain));
        ltemp_min->setStyleSheet(QString("font-family: 'Inter'; color: %1; font-size: 9px; font-weight: 500;").arg(UiStyle::kColorTextDim));
        ltemp_max->setAlignment(Qt::AlignCenter);
        ltemp_min->setAlignment(Qt::AlignCenter);
        
        cl->addWidget(lday);
        cl->addWidget(licon);
        cl->addWidget(ltemp_max);
        cl->addWidget(ltemp_min);
        
        layout_->addWidget(card);
    }
}
