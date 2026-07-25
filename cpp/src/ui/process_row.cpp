#include "ui/process_row.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

#include "ui/style_tokens.h"
#include "ui/icons_material.h"

ProcessRowWidget::ProcessRowWidget(QWidget *parent)
    : QWidget(parent) {
    setStyleSheet("background: transparent;");

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(12, 4, 12, 4);
    layout_->setSpacing(8);
    layout_->setAlignment(Qt::AlignVCenter);
    name_label_ = new QLabel(this);
    name_label_->setStyleSheet(UiStyle::kSubtitle);

    value_label_ = new QLabel(this);
    value_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_label_->setFixedWidth(100);
    value_label_->setStyleSheet(UiStyle::kValueStrong);

    suspend_button_ = new QPushButton(this);
    suspend_button_->setFixedSize(20, 20);
    suspend_button_->setCursor(Qt::PointingHandCursor);
    suspend_button_->setStyleSheet(
        QString("QPushButton { color: #8b8b94; background: transparent; border: none; font-family: '%1'; font-size: 14px; }")
        .arg(Icons::fontName()) +
        "QPushButton:disabled { color: #555555; }"
        "QPushButton:hover { color: #ffffff; }"
    );
    suspend_button_->hide();

    kill_button_ = new QPushButton(QString::fromUtf16(Icons::kClose), this);
    kill_button_->setFixedSize(20, 20);
    kill_button_->setCursor(Qt::PointingHandCursor);
    kill_button_->setStyleSheet(
        QString("QPushButton { color: #ed4245; background: transparent; border: none; font-family: '%1'; font-size: 14px; }")
        .arg(Icons::fontName()) +
        "QPushButton:disabled { color: #555555; }"
    );

    connect(kill_button_, &QPushButton::clicked, this, [this]() {
        emit killRequested(pid_);
    });

    connect(suspend_button_, &QPushButton::clicked, this, [this]() {
        emit suspendRequested(pid_, !is_suspended_);
    });

    layout_->addWidget(name_label_); // No stretch factor on label itself
    layout_->addStretch(1);          // Explicit spacer
    layout_->addWidget(value_label_);
    layout_->addWidget(suspend_button_);
    layout_->addWidget(kill_button_);
}

void ProcessRowWidget::setData(const QString &name, const QString &value, qulonglong pid, bool can_kill,
    bool can_suspend, bool is_suspended) {
    pid_ = pid;
    can_suspend_ = can_suspend;
    is_suspended_ = is_suspended;
    name_label_->setText(name);
    value_label_->setText(value);
    kill_button_->setEnabled(can_kill);
    updateSuspendButton();
}

void ProcessRowWidget::setSuspended(bool is_suspended) {
    is_suspended_ = is_suspended;
    updateSuspendButton();
}

void ProcessRowWidget::setCompactLayout(bool compact) {
    if (compact_layout_ == compact) {
        return;
    }
    compact_layout_ = compact;
    if (compact_layout_) {
        layout_->setContentsMargins(8, 2, 8, 2);
        layout_->setSpacing(6);
        value_label_->setFixedWidth(80); // Reduced to avoid clipping in side bar
        // Default style with safe color
        value_label_->setStyleSheet(QString("font-size: 11px; font-weight: 600; color: %1;").arg(UiStyle::kColorTextMain));
        value_label_->setTextFormat(Qt::PlainText); // Revert to plain text
    } else {
        layout_->setContentsMargins(12, 4, 12, 4);
        layout_->setSpacing(8);
        name_label_->setStyleSheet(UiStyle::kSubtitle);
        value_label_->setFixedWidth(100);
        value_label_->setStyleSheet(UiStyle::kValueStrong);
    }
}

void ProcessRowWidget::updateSuspendButton() {
    suspend_button_->setEnabled(can_suspend_);
    if (!can_suspend_) {
        suspend_button_->setText(QString::fromUtf16(Icons::kPause));
        suspend_button_->setToolTip("Suspend not available.");
        return;
    }

    if (is_suspended_) {
        suspend_button_->setText(QString::fromUtf16(Icons::kPlay));
        suspend_button_->setToolTip("Resume process");
    } else {
        suspend_button_->setText(QString::fromUtf16(Icons::kPause));
        suspend_button_->setToolTip("Suspend process");
    }
}

void ProcessRowWidget::enterEvent(QEnterEvent *event) {
    QWidget::enterEvent(event);
    if (can_suspend_) {
        suspend_button_->show();
    }
}

void ProcessRowWidget::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    if (can_suspend_) {
        suspend_button_->hide();
    }
}

void ProcessRowWidget::setValueColor(const QString &color) {
    if (compact_layout_) {
         // Debug: background color to verify widget existence, remove later
         value_label_->setStyleSheet(QString("font-size: 11px; font-weight: 600; color: %1;").arg(color));
    } else {
         value_label_->setStyleSheet(QString("font-size: 12px; font-weight: bold; font-family: 'Segoe UI'; color: %1;").arg(color));
    }
}
