#pragma once

#include <QWidget>
#include <QVector>
#include <QPair>

class HourlyGraphWidget : public QWidget {
    Q_OBJECT

public:
    struct DataPoint {
        QString time; // e.g., "PM 12시"
        int temp;     // e.g., -7
        QString icon; // Material icon code
        int weatherType = 0; // 0: Normal, 1: Rain, 2: Snow
    };

    explicit HourlyGraphWidget(QWidget *parent = nullptr);
    void setData(const QVector<DataPoint> &data);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<DataPoint> data_;
};
