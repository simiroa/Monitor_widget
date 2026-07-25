#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QVector>

class QLabel;

class SidebarItem : public QAbstractButton {
    Q_OBJECT

public:
    explicit SidebarItem(const QString &label, const QString &icon = QString(), QWidget *parent = nullptr);

    void updateData(double percent, const QString &value_text = QString(), const QColor &color = QColor());
    void updateClockData(const QString &date_text, const QString &time_text); // For clock mode
    void setClock(bool is_clock); // Dashboard mode: centered text, no sparkline

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void appendHistory(double value);
    void drawSparkline(QPainter &painter) const;
    static QColor colorForPercent(double percent);

    QString label_text_;
    QString icon_char_; // Added
    QLabel *icon_label_ = nullptr; // Added
    QLabel *name_label_ = nullptr;
    QLabel *value_label_ = nullptr;

    QVector<double> history_;
    int max_history_ = 40;
    double percent_ = 0.0;
    QColor color_;
    bool hovered_ = false;
    bool is_clock_ = false; // Dashboard mode
};
