#include "ui/widgets/hourly_graph_widget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QLinearGradient>

#include "ui/style_tokens.h"
#include "ui/icons_material.h"

HourlyGraphWidget::HourlyGraphWidget(QWidget *parent)
    : QWidget(parent) {
    setFixedHeight(100); // Fixed height for graph area (reduced from 160)
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HourlyGraphWidget::setData(const QVector<DataPoint> &data) {
    data_ = data;
    update();
}

void HourlyGraphWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    if (data_.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();
    const int margin_x = 10; // Reduced from 30 to expand width
    const int margin_y = 40; 
    const int count = data_.size();
    if (count < 2) return;

    // Determine Min/Max Temp
    int min_temp = 100;
    int max_temp = -100;
    for (const auto &p : data_) {
        if (p.temp < min_temp) min_temp = p.temp;
        if (p.temp > max_temp) max_temp = p.temp;
    }
    
    // Add some padding to range
    int range = max_temp - min_temp;
    if (range == 0) range = 10;
    
    const double step_x = (double)(w - 2 * margin_x) / (count - 1);
    const double scale_y = (double)(h - 2 * margin_y) / range;

    QVector<QPointF> points;
    for (int i = 0; i < count; ++i) {
        double x = margin_x + i * step_x;
        double y = margin_y + (max_temp - data_[i].temp) * scale_y;
        points.append(QPointF(x, y));
    }

    // Draw Smooth Curve
    QPen pen;
    pen.setColor(QColor("#FFD700")); // Gold/Yellow color
    pen.setWidth(2);
    painter.setPen(pen);
    
    QPainterPath path;
    path.moveTo(points[0]);
    for (int i = 0; i < count - 1; ++i) {
        QPointF p1 = points[i];
        QPointF p2 = points[i + 1];
        double cpx1 = p1.x() + (p2.x() - p1.x()) / 2.0;
        path.cubicTo(QPointF(cpx1, p1.y()), QPointF(cpx1, p2.y()), p2);
    }
    painter.drawPath(path);

    // Draw Filled Area (Gradient)
    QLinearGradient gradient(0, margin_y, 0, h - margin_y);
    gradient.setColorAt(0, QColor(255, 215, 0, 40));
    gradient.setColorAt(1, QColor(255, 215, 0, 0));
    
    QPainterPath fillPath = path;
    fillPath.lineTo(points.last().x(), h - 10);
    fillPath.lineTo(points.first().x(), h - 10);
    fillPath.closeSubpath();
    painter.fillPath(fillPath, gradient);

    // Draw Points and Text
    painter.setFont(QFont("Inter", 8, QFont::Medium));
    for (int i = 0; i < count; ++i) {
        QPointF pt = points[i];
        
        // Dynamic Color logic for Dot
        QColor dotColor("#FFD700"); // Default Gold
        if (data_[i].weatherType == 1) dotColor = QColor("#0047AB"); // Cobalt Blue (Rain)
        else if (data_[i].weatherType == 2) dotColor = QColor("#6699CC"); // Blue-Gray (Snow)

        // Dot
        painter.setPen(Qt::NoPen);
        painter.setBrush(dotColor);
        painter.drawEllipse(pt, 2.5, 2.5);

        // Temp Text (Above every point)
        painter.setPen(QColor(UiStyle::kColorTextMain));
        painter.drawText(QRectF(pt.x() - 20, pt.y() - 22, 40, 20), Qt::AlignCenter, QString::number(data_[i].temp) + "°");

        // Time Text (Below only every 2nd point: 0, 2, 4, 6)
        if (i % 2 == 0) {
            painter.setPen(QColor(UiStyle::kColorTextDim));
            painter.drawText(QRectF(pt.x() - 30, h - 22, 60, 20), Qt::AlignCenter, data_[i].time);
        }
    }
}
