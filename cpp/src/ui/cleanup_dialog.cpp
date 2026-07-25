#include "ui/cleanup_dialog.h"

#include <QCheckBox>
#include <QLabel>
#include <QLayoutItem>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStringList>
#include <QTimer>
#include <QMouseEvent>

#include "services/cleanup_service.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"
#include "utils/logger.h"

namespace {
const CleanupItem* findItemById(const CleanupService *service, const QString &id) {
    static const auto items = service->items();
    for (const auto &item : items) {
        if (item.id == id) return &item;
    }
    return nullptr;
}
}

CleanupDialog::CleanupDialog(CleanupService *service, QWidget *parent)
    : QDialog(parent), service_(service) {
    setWindowTitle("Disk Cleanup");
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog); // Frameless
    resize(380, 500);

    setStyleSheet(QString("QDialog { background-color: %1; border: 1px solid %2; border-radius: 8px; } "
                          "QToolTip { font-family: 'Outfit', sans-serif; font-size: 12px; padding: 6px; border: 1px solid #444; background-color: #1e1e22; color: #ffffff; }")
        .arg(UiStyle::kColorMainBg).arg(UiStyle::kBorder));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // --- Custom Title Bar (Premium Design) ---
    auto *title_bar = new QWidget(this);
    title_bar->setStyleSheet(QString("background-color: %1; border-top-left-radius: 8px; border-top-right-radius: 8px; border-bottom: 1px solid %2;")
        .arg(UiStyle::kSurface).arg(UiStyle::kBorder)); // Or kSectionBg for slightly lighter
    auto *title_layout = new QHBoxLayout(title_bar);
    title_layout->setContentsMargins(16, 12, 16, 12);
    title_layout->setSpacing(8);

    auto *app_icon = new QLabel(QString::fromUtf16(Icons::kCleanup), this);
    app_icon->setStyleSheet(QString("font-family: '%1'; font-size: 14px; color: %2;").arg(Icons::fontName()).arg(UiStyle::kColorGreen));
    title_layout->addWidget(app_icon);

    auto *title_label = new QLabel("Disk Cleanup", this);
    title_label->setStyleSheet("color: #ffffff; font-weight: 600; font-size: 13px; font-family: 'Segoe UI';");
    title_layout->addWidget(title_label);

    title_layout->addStretch();

    // Custom Close Button
    auto *close_btn = new QPushButton(QString::fromUtf16(Icons::kClose), this);
    close_btn->setFixedSize(24, 24);
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-family: '%2'; font-size: 16px; }"
        "QPushButton:hover { color: %3; background: %4; border-radius: 4px; }"
    ).arg(UiStyle::kColorTextDim).arg(Icons::fontName()).arg(UiStyle::kColorRed).arg("#303036"));
    
    connect(close_btn, &QPushButton::clicked, this, &QDialog::reject);
    title_layout->addWidget(close_btn);

    main_layout->addWidget(title_bar);


    // --- Summary Header (Compact) ---
    auto *summary_container = new QWidget(this);
    summary_container->setStyleSheet(QString("background-color: %1; border-bottom: 1px solid %2;").arg(UiStyle::kColorMainBg).arg(UiStyle::kBorder));
    auto *summary_layout = new QHBoxLayout(summary_container);
    summary_layout->setContentsMargins(20, 12, 20, 12); // Reduced from 16
    
    summary_value_label_ = new QLabel("Scanning...", this);
    summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kTextSecondary));
    summary_layout->addWidget(summary_value_label_);
    
    summary_layout->addStretch();
    
    summary_desc_label_ = new QLabel("", this); // Optional detail
    summary_desc_label_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    summary_desc_label_->setVisible(false);
    summary_layout->addWidget(summary_desc_label_);

    main_layout->addWidget(summary_container);


    // --- List Content ---
    auto *scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area->setStyleSheet("QScrollArea { background: transparent; border: none; } QWidget { background: transparent; } QScrollBar:vertical { width: 6px; background: transparent; } QScrollBar::handle:vertical { background: #333; border-radius: 3px; }");
    
    auto *scroll_content = new QWidget(scroll_area);
    list_layout_ = new QVBoxLayout(scroll_content);
    list_layout_->setContentsMargins(12, 12, 12, 12); // Reduced from 16
    list_layout_->setSpacing(6);
    list_layout_->setAlignment(Qt::AlignTop);

    scroll_area->setWidget(scroll_content);
    main_layout->addWidget(scroll_area);


    // --- Footer ---
    auto *footer_widget = new QWidget(this);
    footer_widget->setStyleSheet(QString("background-color: %1; border-top: 1px solid %2; border-bottom-left-radius: 8px; border-bottom-right-radius: 8px;")
        .arg(UiStyle::kColorMainBg).arg(UiStyle::kBorder));
    auto *footer_layout = new QVBoxLayout(footer_widget);
    footer_layout->setContentsMargins(16, 12, 16, 16);
    footer_layout->setSpacing(12);

    progress_bar_ = new QProgressBar(this);
    progress_bar_->setFixedHeight(4);
    progress_bar_->setTextVisible(false);
    progress_bar_->setRange(0, 0); 
    progress_bar_->setStyleSheet(UiStyle::kProgressBar);
    footer_layout->addWidget(progress_bar_);

    auto *btn_layout = new QHBoxLayout();
    
    // Rescan Button
    rescan_button_ = new QPushButton("Rescan", this);
    rescan_button_->setFixedWidth(80);
    rescan_button_->setCursor(Qt::PointingHandCursor);
    rescan_button_->setStyleSheet(UiStyle::kButton);
    connect(rescan_button_, &QPushButton::clicked, this, &CleanupDialog::startRescan);
    btn_layout->addWidget(rescan_button_);

    btn_layout->addStretch();

    cancel_button_ = new QPushButton("Cancel", this);
    cancel_button_->setFixedWidth(80);
    cancel_button_->setCursor(Qt::PointingHandCursor);
    cancel_button_->setStyleSheet(UiStyle::kButton);
    cancel_button_->setEnabled(false);
    connect(cancel_button_, &QPushButton::clicked, this, &CleanupDialog::cancelCleanup);
    btn_layout->addWidget(cancel_button_);

    run_button_ = new QPushButton("Clean Selected", this);
    run_button_->setFixedWidth(140); 
    run_button_->setCursor(Qt::PointingHandCursor);
    run_button_->setStyleSheet(UiStyle::kButton);
    run_button_->setEnabled(false);
    connect(run_button_, &QPushButton::clicked, this, &CleanupDialog::startCleanup);
    btn_layout->addWidget(run_button_);

    footer_layout->addLayout(btn_layout);
    main_layout->addWidget(footer_widget);


    connect(service_, &CleanupService::scanProgress, this, &CleanupDialog::onScanProgress);
    connect(service_, &CleanupService::scanFinished, this, &CleanupDialog::onScanFinished);
    connect(service_, &CleanupService::cleanProgress, this, &CleanupDialog::onCleanProgress);
    connect(service_, &CleanupService::cleanFinished, this, &CleanupDialog::onCleanFinished);

    QTimer::singleShot(200, this, [this](){
        qDeleteAll(checkboxes_);
        checkboxes_.clear();
        status_labels_.clear();
        size_labels_.clear();
        row_widgets_.clear();
        QLayoutItem *child;
        while ((child = list_layout_->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        service_->startScan();
    });
}

CleanupDialog::~CleanupDialog() = default;

void CleanupDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        drag_start_position_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void CleanupDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - drag_start_position_);
        event->accept();
    }
}


QString CleanupDialog::formatSize(quint64 bytes) {
    // ... same as before
    if (bytes == 0) return "0 B";
    const double kb = 1024.0;
    const double mb = kb * 1024.0;
    const double gb = mb * 1024.0;

    if (bytes >= static_cast<quint64>(gb)) {
        return QString("%1 GB").arg(bytes / gb, 0, 'f', 1);
    }
    if (bytes >= static_cast<quint64>(mb)) {
        return QString("%1 MB").arg(bytes / mb, 0, 'f', 1);
    }
    if (bytes >= static_cast<quint64>(kb)) {
        return QString("%1 KB").arg(bytes / kb, 0, 'f', 1);
    }
    return QString("%1 B").arg(bytes);
}

void CleanupDialog::startCleanup() {
    if (!service_) return;

    QStringList selections;
    for (auto *cb : checkboxes_) {
        if (cb->isChecked()) {
            selections.push_back(cb->property("id").toString());
        }
    }

    if (selections.isEmpty()) return;

    run_button_->setEnabled(false);
    run_button_->setText("Cleaning...");
    rescan_button_->setEnabled(false);
    cancel_button_->setEnabled(true);
    progress_bar_->setRange(0, selections.size()); 
    progress_bar_->setValue(0);
    progress_bar_->setVisible(true);
    
    for(auto *cb : checkboxes_) cb->setEnabled(false);

    summary_value_label_->setText("Cleaning...");
    summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kColorBlue));

    service_->startClean(selections);
}

void CleanupDialog::onScanProgress(const QString &id, quint64 size) {
    const CleanupItem* item = findItemById(service_, id);
    if (!item) {
        return;
    }

    QString labelText = item ? item->label : id;
    QString descText = item ? item->description : "No description.";
    
    auto *row_widget = new QWidget(this);
    row_widget->setStyleSheet(QString("background-color: %1; border-radius: 6px;").arg(UiStyle::kSurface));

    auto *row_layout = new QHBoxLayout(row_widget);
    row_layout->setContentsMargins(12, 6, 12, 6); // Reduced from 10
    row_layout->setSpacing(10);
    row_layout->setAlignment(Qt::AlignVCenter);

    // Name
    auto *title = new QLabel(labelText, this);
    title->setStyleSheet(QString("font-family: 'Outfit'; font-size: 12px; font-weight: 600; color: %1;").arg(UiStyle::kColorTextMain));
    title->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); // Strict centering
    row_layout->addWidget(title);

    // Size or Status
    QString sizeStr;
    if (item->isAction) {
        sizeStr = "Action";
    } else {
        sizeStr = formatSize(size);
    }
    
    auto *size_box = new QWidget(this);
    auto *size_layout = new QVBoxLayout(size_box);
    size_layout->setContentsMargins(0, 0, 0, 0);
    size_layout->setSpacing(2);
    size_layout->setAlignment(Qt::AlignVCenter); // Force vertical centering inside box

    auto *size_label = new QLabel(sizeStr, this);
    size_label->setStyleSheet(size > 0 || item->isAction ? QString("color: %1; font-weight: 600; font-size: 12px;").arg(UiStyle::kColorGreen) : UiStyle::kDetail);
    size_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    size_layout->addWidget(size_label);
    size_labels_[id] = size_label;

    auto *status_label = new QLabel("", this);
    status_label->setStyleSheet(QString("color: %1; font-size: 10px;").arg(UiStyle::kColorTextDim));
    status_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    status_label->setVisible(false); // Hide by default until content added
    size_layout->addWidget(status_label);
    status_labels_[id] = status_label;

    size_box->setFixedWidth(80);
    row_layout->addWidget(size_box);

    auto *risk_icon = new QLabel(this);
    QString risk_tooltip;
    if (item->safety == CleanupSafety::Caution) {
        risk_icon->setText(QString::fromUtf16(Icons::kWeatherWarning));
        risk_icon->setStyleSheet(QString("font-family: '%1'; font-size: 14px; color: #f5c56b;").arg(Icons::fontName()));
        risk_tooltip = "Caution: may remove useful cache.";
    } else if (item->safety == CleanupSafety::Advanced) {
        risk_icon->setText(QString::fromUtf16(Icons::kWeatherWarning));
        risk_icon->setStyleSheet(QString("font-family: '%1'; font-size: 14px; color: #f08a8a;").arg(Icons::fontName()));
        risk_tooltip = "Advanced: system-level change.";
    }
    if (item->requiresAdmin) {
        if (!risk_tooltip.isEmpty()) {
            risk_tooltip += " ";
        }
        risk_tooltip += "Admin required.";
    }
    if (risk_icon->text().isEmpty()) {
        risk_icon->setFixedWidth(14);
    } else {
        risk_icon->setToolTip(risk_tooltip);
    }
    row_layout->addWidget(risk_icon);

    // Tooltip style is now handled at Dialog level for consistency


    // Checkbox
    auto *checkbox = new QCheckBox(this);
    checkbox->setStyleSheet(QString(
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid %1; border-radius: 4px; background: transparent; }"
        "QCheckBox::indicator:checked { background-color: #ffffff; border-color: #ffffff; }" 
        "QCheckBox::indicator:hover { border-color: %2; }"
    ).arg(UiStyle::kTextSecondary).arg(UiStyle::kColorTextMain));
    
    bool defaultChecked = (item->safety == CleanupSafety::Safe) && !item->isAction;
    if (item->safety != CleanupSafety::Safe) defaultChecked = false;

    checkbox->setChecked(defaultChecked);
    if (item->requiresAdmin && !service_->isElevated()) {
        checkbox->setChecked(false);
        checkbox->setEnabled(false);
        status_label->setText("Admin required");
        status_label->setVisible(true); // Show if admin required
    }
    
    checkbox->setFixedWidth(22);
    row_layout->addWidget(checkbox);

    checkbox->setProperty("id", id);
    checkbox->setProperty("bytes", item->isAction ? 0 : size); 
    connect(checkbox, &QCheckBox::toggled, this, &CleanupDialog::updateTotalSize); 

    if (!risk_tooltip.isEmpty()) {
        row_widget->setToolTip(descText + "\n" + risk_tooltip);
    } else {
        row_widget->setToolTip(descText);
    }

    checkboxes_.push_back(checkbox);
    list_layout_->addWidget(row_widget);
    row_widgets_[id] = row_widget;
}

void CleanupDialog::onScanFinished(bool foundAny) {
    progress_bar_->setVisible(false);
    run_button_->setEnabled(true);
    run_button_->setText("Clean Selected");
    rescan_button_->setEnabled(true);
    cancel_button_->setEnabled(false);
    
    if (foundAny) {
        summary_value_label_->setText("Scan Complete");
        summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kColorGreen));
        
        summary_desc_label_->setVisible(true);
        updateTotalSize(); // Initial calc
    } else {
        summary_value_label_->setText("System Clean");
        summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kColorBlue));
        summary_desc_label_->setVisible(false);
        run_button_->setEnabled(false);
    }
}

void CleanupDialog::updateTotalSize() {
    quint64 total = 0;
    for (auto *cb : checkboxes_) {
        if (cb->isChecked()) {
            total += cb->property("bytes").toULongLong();
        }
    }
    summary_desc_label_->setText(formatSize(total));
    summary_desc_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: #ffffff;"));
}

void CleanupDialog::onCleanProgress(const QString &id, bool success, const QString &error) {
    progress_bar_->setValue(progress_bar_->value() + 1);
    auto *status_label = status_labels_.value(id, nullptr);
    if (status_label) {
        if (success) {
            status_label->setText("Done");
            status_label->setStyleSheet("color: #7ce38b; font-size: 11px; font-weight: 600;");
            status_label->setVisible(true);
        } else {
            status_label->setText("Failed");
            status_label->setStyleSheet("color: #f08a8a; font-size: 11px; font-weight: 600;");
            status_label->setVisible(true);
        }
    }
    if (!success) {
        auto *row = row_widgets_.value(id, nullptr);
        if (row) {
            row->setToolTip(error);
        }
    }
}

void CleanupDialog::onCleanFinished(const QString &summary) {
    progress_bar_->setVisible(false);
    cancel_button_->setEnabled(false);
    
    // Show cleared amount from summary (e.g., "Cleared: 1.3 GB")
    summary_value_label_->setText("Cleanup Finished");
    summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kColorGreen));
    
    // Show the actual amount cleared
    summary_desc_label_->setText(summary);
    summary_desc_label_->setStyleSheet(QString("font-size: 14px; color: #ffffff;"));
    summary_desc_label_->setVisible(true);
    
    // Automatically rescan after a short delay
    QTimer::singleShot(1500, this, [this]() {
        // Clear old items
        qDeleteAll(checkboxes_);
        checkboxes_.clear();
        status_labels_.clear();
        size_labels_.clear();
        row_widgets_.clear();
        QLayoutItem *child;
        while ((child = list_layout_->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
        
        summary_value_label_->setText("Rescanning...");
        summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kTextSecondary));
        summary_desc_label_->setVisible(false);
        progress_bar_->setRange(0, 0);
        progress_bar_->setVisible(true);
        run_button_->setEnabled(false);
        run_button_->setText("Scanning...");
        cancel_button_->setEnabled(false);
        
        service_->startScan();
    });

    for(auto *cb : checkboxes_) cb->setEnabled(false);
}

void CleanupDialog::startRescan() {
    // Clear old items
    qDeleteAll(checkboxes_);
    checkboxes_.clear();
    status_labels_.clear();
    size_labels_.clear();
    row_widgets_.clear();
    QLayoutItem *child;
    while ((child = list_layout_->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    
    summary_value_label_->setText("Scanning...");
    summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kTextSecondary));
    summary_desc_label_->setVisible(false);
    progress_bar_->setRange(0, 0);
    progress_bar_->setVisible(true);
    run_button_->setEnabled(false);
    run_button_->setText("Scanning...");
    rescan_button_->setEnabled(false);
    cancel_button_->setEnabled(false);
    
    service_->startScan();
}

void CleanupDialog::cancelCleanup() {
    if (!service_) return;
    cancel_button_->setEnabled(false);
    service_->cancel();
    summary_value_label_->setText("Canceling...");
    summary_value_label_->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(UiStyle::kColorTextDim));
}
