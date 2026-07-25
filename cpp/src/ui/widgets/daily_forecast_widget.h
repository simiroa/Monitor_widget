#pragma once

#include <QWidget>
#include <QVector>
#include "models/weather_data.h"

class QHBoxLayout;

class DailyForecastWidget : public QWidget {
    Q_OBJECT

public:
    explicit DailyForecastWidget(QWidget *parent = nullptr);
    void updateData(const QVector<DailyItem> &data);

private:
    QHBoxLayout *layout_ = nullptr;
};
