#include "ui/sidebar_item.h"

#include <algorithm>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include "ui/style_tokens.h"
#include "ui/icons_material.h"

namespace {
constexpr int kItemHeight = 34; // Adjusted to fit 8 tabs (approx 310px total usage)
constexpr int kSparklineHeight = 14;
}

SidebarItem::SidebarItem(const QString &label, const QString &icon, QWidget *parent)
    : QAbstractButton(parent), label_text_(label) {
    setFixedHeight(kItemHeight);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 0, 8, 0); // Reduced left margin for indicator space
    layout->setSpacing(4);

    // Create icon label unconditionally, but make it a member
    icon_label_ = new QLabel(this);
    icon_label_->setFixedSize(20, 20);
    icon_label_->setAlignment(Qt::AlignCenter);
    icon_label_->setStyleSheet(QString("font-family: '%1'; font-size: 16px; color: #8b8b94; background: transparent;")
                             .arg(Icons::fontName()));
    layout->addWidget(icon_label_);

    if (!icon_char_.isEmpty()) {
        icon_label_->setText(icon_char_);
    } else {
        icon_label_->setVisible(false);
        layout->setContentsMargins(12, 0, 8, 0); // Shift text left
    }

    name_label_ = new QLabel(label, this);
    // Avant-garde rounded font: Outfit
    name_label_->setStyleSheet(QString("font-family: 'Outfit', 'Poppins', sans-serif; "
                                       "font-size: 11px; font-weight: bold; "
                                       "color: %1; background: transparent;")
                                       .arg(UiStyle::kColorTextMain));
    name_label_->setAttribute(Qt::WA_TranslucentBackground);

    value_label_ = new QLabel("0%", this);
    value_label_->setAttribute(Qt::WA_TranslucentBackground);

    // Clock mode: empty label means two-line layout (date + time)
    if (label.isEmpty()) {
        is_clock_ = true;
        
        // Use VBoxLayout for two-line display
        auto *vbox = new QVBoxLayout();
        vbox->setContentsMargins(0, 2, 0, 2);
        vbox->setSpacing(0);
        
        // Date label (top, small) - reuse name_label_
        name_label_->setAlignment(Qt::AlignCenter);
        name_label_->setStyleSheet(QString("font-family: 'Outfit', 'Poppins', sans-serif; "
                                           "font-size: 10px; font-weight: normal; "
                                           "color: %1; background: transparent;")
                                           .arg(UiStyle::kColorTextDim));
        name_label_->setText(""); // Will be set by updateClockData
        
        // Time label (bottom, large)
        value_label_->setAlignment(Qt::AlignCenter);
        value_label_->setStyleSheet(QString("font-family: 'Outfit', 'Poppins', sans-serif; "
                                            "font-size: 14px; font-weight: bold; "
                                            "color: %1; background: transparent;")
                                            .arg(UiStyle::kColorTextMain));
        
        vbox->addWidget(name_label_);
        vbox->addWidget(value_label_);
        
        layout->addStretch();
        layout->addLayout(vbox);
        layout->addStretch();
    } else {
        value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value_label_->setStyleSheet(QString("font-family: 'Outfit', 'Poppins', sans-serif; "
                                            "font-size: 10px; font-weight: bold; "
                                            "color: %1; background: transparent;")
                                            .arg(UiStyle::kColorTextDim));
        layout->addWidget(name_label_);
        layout->addStretch();
        layout->addWidget(value_label_);
    }

    color_ = colorForPercent(0.0);
}

void SidebarItem::updateData(double percent, const QString &value_text, const QColor &color) {
    percent_ = percent;
    color_ = color.isValid() ? color : colorForPercent(percent);

    if (value_text.isEmpty()) {
        value_label_->setText(QString("%1%").arg(static_cast<int>(percent)));
    } else {
        value_label_->setText(value_text);
    }

    const QColor label_color = (label_text_ == "SRV")
        ? QColor(UiStyle::kColorGreen)
        : color_;
    name_label_->setStyleSheet(QString("font-family: 'Outfit', 'Poppins', 'Segoe UI', sans-serif; "
                                       "font-size: 11px; font-weight: bold; "
                                       "color: %1; background: transparent;")
                                       .arg(label_color.name()));
    value_label_->setStyleSheet(QString("font-family: 'Outfit', 'Poppins', 'Segoe UI', sans-serif; "
                                        "font-size: 10px; font-weight: bold; "
                                        "color: %1; background: transparent;")
                                        .arg(color_.name()));

    appendHistory(percent);
    update();
}

void SidebarItem::updateClockData(const QString &date_text, const QString &time_text) {
    if (!is_clock_) return;
    name_label_->setText(date_text);
    value_label_->setText(time_text);
    update();
}

void SidebarItem::setClock(bool is_clock) {
    is_clock_ = is_clock;
    update();
}

void SidebarItem::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (isChecked()) {
        QColor color(UiStyle::kColorSectionBg);
        color.setAlpha(100);
        painter.fillRect(rect().adjusted(2, 2, -2, -2), color);

        QColor indicator(UiStyle::kColorActiveTab);
        indicator.setAlpha(255);
        painter.fillRect(width() - 3, 4, 3, height() - 8, indicator);
    } else if (hovered_) {
        QColor hover(UiStyle::kColorSectionBg);
        painter.fillRect(rect(), hover.lighter(110));
    }

    if (percent_ > 0.0) {
        const int fill_width = static_cast<int>(width() * (std::min(percent_, 100.0) / 100.0));
        QColor fill = color_;
        fill.setAlpha(30);
        painter.fillRect(0, 0, fill_width, height(), fill);
    }

    // Skip sparkline for clock mode
    if (!is_clock_ && history_.size() > 1) {
        drawSparkline(painter);
    }
}

void SidebarItem::enterEvent(QEnterEvent *event) {
    QAbstractButton::enterEvent(event);
    hovered_ = true;
    update();
}

void SidebarItem::leaveEvent(QEvent *event) {
    QAbstractButton::leaveEvent(event);
    hovered_ = false;
    update();
}

void SidebarItem::appendHistory(double value) {
    history_.push_back(value);
    if (history_.size() > max_history_) {
        history_.erase(history_.begin());
    }
}

void SidebarItem::drawSparkline(QPainter &painter) const {
    if (history_.size() < 2) {
        return;
    }

    const int w = width();
    const int h = height();
    const double step = static_cast<double>(w) / static_cast<double>(max_history_ - 1);

    QPainterPath area_path;
    area_path.moveTo(0, h);

    for (int i = 0; i < history_.size(); ++i) {
        const double percent = std::min(history_[i], 100.0);
        const double y = (h - 1) - (percent / 100.0) * kSparklineHeight;
        area_path.lineTo(i * step, y);
    }

    area_path.lineTo((history_.size() - 1) * step, h);
    area_path.closeSubpath();

    QColor fill(color_);
    fill.setAlpha(40);
    painter.fillPath(area_path, fill);

    painter.setPen(QPen(color_, 1.2));
    QPainterPath line_path;
    for (int i = 0; i < history_.size(); ++i) {
        const double percent = std::min(history_[i], 100.0);
        const double y = (h - 1) - (percent / 100.0) * kSparklineHeight;
        if (i == 0) {
            line_path.moveTo(0, y);
        } else {
            line_path.lineTo(i * step, y);
        }
    }
    painter.drawPath(line_path);
}

QColor SidebarItem::colorForPercent(double percent) {
    if (percent >= 90.0) {
        return QColor(UiStyle::kColorRed);
    }
    if (percent >= 70.0) {
        return QColor(UiStyle::kColorOrange);
    }
    if (percent >= 40.0) {
        return QColor(UiStyle::kColorYellow);
    }
    return QColor(UiStyle::kColorGreen);
}
