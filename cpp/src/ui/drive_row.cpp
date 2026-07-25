#include "ui/drive_row.h"

#include <QColor>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>

#include "ui/style_tokens.h"

namespace {
QColor usageColor(double percent) {
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

QColor activeTimeColor(double percent) {
    if (percent >= 90.0) {
        return QColor(UiStyle::kColorRed);
    }
    if (percent >= 70.0) {
        return QColor(UiStyle::kColorOrange);
    }
    return QColor(UiStyle::kColorGreen);
}

QString progressBackground() {
    QColor bg(UiStyle::kColorTextDim);
    bg.setAlpha(50);
    return bg.name(QColor::HexArgb);
}
}  // namespace

DriveRowWidget::DriveRowWidget(const DriveInfo &info, QWidget *parent)
    : QWidget(parent), mountpoint_(info.mountpoint), current_info_(info) {
    setFixedHeight(22);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 8, 2); 
    layout->setSpacing(6);

    QString label = info.mountpoint;
    if (!label.isEmpty()) {
        label = label.left(1).toUpper() + ":";
    } else {
        label = "?";
    }

    // 1. Drive Letter
    auto *btn_letter = new QPushButton(label, this);
    btn_letter->setCursor(Qt::PointingHandCursor);
    btn_letter->setFixedWidth(28); // Match header DRIVE width
    btn_letter->setFlat(true);
    btn_letter->setStyleSheet(
        "QPushButton { color: #ffffff; font-weight: bold; font-size: 13px; border: none; background: transparent; text-align: center; }"
        "QPushButton:hover { color: #8b8b94; }"
    );
    connect(btn_letter, &QPushButton::clicked, this, &DriveRowWidget::openMountpoint);

    // 2. Usage Text
    usage_label_ = new QLabel(this);
    usage_label_->setFixedWidth(55); // Match header FREE width
    usage_label_->setAlignment(Qt::AlignCenter);
    usage_label_->setStyleSheet("font-size: 12px; font-family: 'Segoe UI', sans-serif; font-weight: bold;");

    // 3. Usage Bar
    bar_ = new QProgressBar(this);
    bar_->setFixedHeight(6); 
    bar_->setTextVisible(false);
    bar_->setRange(0, 100);
    bar_->setStyleSheet(QString("QProgressBar { background: %1; border-radius: 3px; border: none; min-width: 50px; }").arg(progressBackground()));

    // 4. Speed Label
    speed_label_ = new QLabel(this);
    speed_label_->setFixedWidth(65); // Match header SPEED width
    speed_label_->setAlignment(Qt::AlignCenter);
    
    layout->addWidget(btn_letter);
    layout->addWidget(usage_label_);
    layout->addWidget(bar_, 1);
    layout->addWidget(speed_label_);

    // Initial Update
    updateInfo(info);
}

void DriveRowWidget::updateInfo(const DriveInfo &info) {
    current_info_ = info;

    // Updates
    updateUsageText();

    bar_->setValue(static_cast<int>(info.percent));
    const QColor usageColorVal = usageColor(info.percent);
    // Only update chunk color if needed (optimization? or just always set stylesheet)
    // Always setting is safer for now to ensure color changes with load
    bar_->setStyleSheet(QString(
        "QProgressBar { background: %1; border-radius: 3px; border: none; min-width: 40px; }"
        "QProgressBar::chunk { background: %2; border-radius: 3px; }")
        .arg(progressBackground())
        .arg(usageColorVal.name()));

    // Speed
    QString speedText;
    QColor speedColor = QColor(UiStyle::kColorGreen); 

    if (info.ioValid) {
        double totalSpeed = info.readMBs + info.writeMBs;
        if (totalSpeed >= 1000.0) {
             speedText = QString("%1 GB/s").arg(totalSpeed / 1024.0, 0, 'f', 1);
        } else if (totalSpeed >= 1.0) {
            speedText = QString("%1 MB/s").arg(totalSpeed, 0, 'f', 1);
        } else {
             if (totalSpeed < 0.001) {
                 speedText = "0 MB/s";
             } else {
                 speedText = QString("%1 KB/s").arg(totalSpeed * 1024.0, 0, 'f', 0);
             }
        }

        if (info.activeTime >= 90.0) {
            speedColor = QColor(UiStyle::kColorRed);
        } else if (info.activeTime >= 70.0) {
            speedColor = QColor(UiStyle::kColorOrange);
        } else {
            if (totalSpeed == 0.0) speedColor = QColor(UiStyle::kColorTextDim);
        }
    } else {
        speedText = "- MB/s";
        speedColor = QColor(UiStyle::kColorTextDim);
    }
    
    speed_label_->setText(speedText);
    speed_label_->setStyleSheet(QString("color: %1; font-size: 11px; font-family: Consolas, monospace; font-weight: bold;")
        .arg(speedColor.name()));

    // Tooltip
    QString tooltip = QString(
        "Drive: %1\n"
        "Usage: %2% (%3/%4 GB)\n"
        "------------------\n"
        "Read: %5 MB/s\n"
        "Write: %6 MB/s\n"
        "Active Time: %7%")
        .arg(info.mountpoint)
        .arg(info.percent, 0, 'f', 1)
        .arg(info.usedGB, 0, 'f', 1)
        .arg(info.totalGB, 0, 'f', 1)
        .arg(info.readMBs, 0, 'f', 1)
        .arg(info.writeMBs, 0, 'f', 1)
        .arg(info.activeTime, 0, 'f', 1);
    
    if (this->toolTip() != tooltip) {
        this->setToolTip(tooltip);
    }
}

void DriveRowWidget::updateUsageText() {
    const QColor usageColorVal = usageColor(current_info_.percent);
    QString text;
    
    if (show_percentage_) {
        // Show %
        text = QString("%1%").arg(current_info_.percent, 0, 'f', 1);
    } else {
        // Show Free GB
        double freeGB = current_info_.totalGB - current_info_.usedGB;
        text = QString("%1 <span style='font-size:9px; color:#8b8b94;'>GB</span>")
            .arg(static_cast<int>(freeGB));
    }

    usage_label_->setText(text);
    // Keep styling consistent with constructor
    usage_label_->setStyleSheet(QString("color: %1; font-size: 12px; font-family: 'Segoe UI', sans-serif; font-weight: bold;")
                                .arg(usageColorVal.name()));
}

void DriveRowWidget::toggleDisplayMode() {
    show_percentage_ = !show_percentage_;
    updateUsageText();
}

void DriveRowWidget::mousePressEvent(QMouseEvent *event) {
    // Check if clicked clearly on usage label or bar
    // Since the whole row is small, clicking anywhere except the drive letter button (which consumes its own event)
    // should probably toggle? Or just strict areas?
    // Let's make the whole row toggle for ease of use, as openUrl is on the button.
    if (event->button() == Qt::LeftButton) {
        toggleDisplayMode();
        event->accept();
    } else {
        QWidget::mousePressEvent(event);
    }
}

void DriveRowWidget::openMountpoint() {
    if (mountpoint_.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(mountpoint_));
}
