#include "ui/dialogs/location_selection_dialog.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QMouseEvent>
#include <QApplication>

LocationSelectionDialog::LocationSelectionDialog(QWidget *parent, const QStringList &items, int currentIdx)
    : QDialog(parent), items_(items), initial_idx_(currentIdx) {
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setupUi();
}

void LocationSelectionDialog::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    // Main Container (for rounded corners & border)
    auto *container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet(QString(
        "QWidget#container { "
        "   background-color: %1; "
        "   border: 1px solid %2; "
        "   border-radius: 8px; "
        "}"
    ).arg(UiStyle::kColorMainBg).arg(UiStyle::kBorder));
    
    auto *container_layout = new QVBoxLayout(container);
    container_layout->setContentsMargins(20, 20, 20, 20);
    container_layout->setSpacing(16);
    
    // 1. Header (Title)
    auto *title_lbl = new QLabel("위치 선택", container);
    title_lbl->setStyleSheet(UiStyle::kTitle);
    title_lbl->setAlignment(Qt::AlignCenter);
    container_layout->addWidget(title_lbl);
    
    // 2. Body (Label + ComboBox)
    auto *body_layout = new QVBoxLayout();
    body_layout->setSpacing(8);
    
    auto *desc_lbl = new QLabel("지역:", container);
    desc_lbl->setStyleSheet(UiStyle::kSubtitle);
    
    combo_ = new QComboBox(container);
    combo_->addItems(items_);
    if (initial_idx_ >= 0 && initial_idx_ < items_.size()) {
        combo_->setCurrentIndex(initial_idx_);
    }
    
    // Custom styling for ComboBox to match design
    combo_->setStyleSheet(
        "QComboBox { "
        "   background-color: #202026; "
        "   color: #f0f0f3; "
        "   border: 1px solid #2d2d34; "
        "   border-radius: 6px; "
        "   padding: 8px 12px; "
        "   font-family: 'Segoe UI', sans-serif; "
        "   font-size: 13px; "
        "}"
        "QComboBox:hover { border-color: #ffffff; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox::down-arrow { "
        "   image: url(none); " 
        "   border: none; "
        "}" 
        // We can add a custom arrow icon if we had one as image resource or just stick to simple
        "QComboBox QAbstractItemView { "
        "   background-color: #202026; "
        "   color: #f0f0f3; "
        "   selection-background-color: #3ba55c; "
        "   selection-color: #ffffff; "
        "   border: 1px solid #2d2d34; "
        "}"
    );
    
    body_layout->addWidget(desc_lbl);
    body_layout->addWidget(combo_);
    container_layout->addLayout(body_layout);
    
    // 3. Buttons (OK / Cancel)
    auto *btn_layout = new QHBoxLayout();
    btn_layout->setSpacing(10);
    
    btn_cancel_ = new QPushButton("Cancel", container);
    btn_cancel_->setCursor(Qt::PointingHandCursor);
    btn_cancel_->setStyleSheet(UiStyle::kButton);
    connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);
    
    btn_ok_ = new QPushButton("OK", container);
    btn_ok_->setCursor(Qt::PointingHandCursor);
    btn_ok_->setStyleSheet(UiStyle::kButtonPrimary);
    connect(btn_ok_, &QPushButton::clicked, this, &QDialog::accept);
    
    btn_layout->addStretch();
    btn_layout->addWidget(btn_cancel_);
    btn_layout->addWidget(btn_ok_);
    
    container_layout->addLayout(btn_layout);
    
    layout->addWidget(container);
}

QString LocationSelectionDialog::getSelectedItem() const {
    return combo_->currentText();
}
