#include "ui/pages/disk_page.h"
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>

#include "services/cleanup_service.h"
#include "ui/drive_row.h"
#include "ui/cleanup_dialog.h"
#include "ui/layout_utils.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"
#include "utils/logger.h"

DiskPage::DiskPage(QWidget *parent)
    : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(UiStyle::kPageMarginLeft, UiStyle::kPageMarginTop, UiStyle::kPageMarginRight, UiStyle::kPageMarginBottom);
    layout->setSpacing(UiStyle::kPageSpacing);

    // Top Row: Title (Added directly to match ProcessList)
    auto *title_label = new QLabel("Fixed Drives", this);
    title_label->setStyleSheet(UiStyle::kTitle);
    layout->addWidget(title_label);

    // Header Container (Usage Stats)
    auto *header_container = new QWidget(this);
    auto *header_layout = new QVBoxLayout(header_container);
    header_layout->setContentsMargins(0, 0, 0, 6); // Matched to ProcessList (was 2)
    header_layout->setSpacing(2);

    // Subtitle Row: Disk Count + Buttons
    auto *subtitle_row = new QHBoxLayout();
    subtitle_row->setContentsMargins(0, 0, 0, 0);
    subtitle_row->setSpacing(4);

    header_name_label_ = new QLabel("0 Disks", this);
    header_name_label_->setStyleSheet(UiStyle::kSubtitle);
    header_name_label_->setFixedHeight(16);
    subtitle_row->addWidget(header_name_label_);

    subtitle_row->addStretch();

    header_layout->addLayout(subtitle_row);

    // Total Speed Row with Buttons
    auto *speed_row = new QHBoxLayout();
    speed_row->setContentsMargins(0, 0, 0, 0);
    speed_row->setSpacing(8);

    speed_label_ = new QLabel(this);
    speed_label_->setStyleSheet(UiStyle::kValueLarge);
    speed_label_->setFixedHeight(22);
    speed_label_->setText("0.0 MB/s");
    speed_row->addWidget(speed_label_);

    speed_row->addStretch();

    // Storage Settings Button (Opens Windows Storage Settings)
    auto *storage_button = new QPushButton(QString::fromUtf16(Icons::kSettings), this);
    storage_button->setFixedSize(28, 28);
    storage_button->setCursor(Qt::PointingHandCursor);
    storage_button->setToolTip("Open Storage Settings");
    storage_button->setStyleSheet(QString("QPushButton { font-family: '%1'; font-size: 16px; }").arg(Icons::fontName()) + UiStyle::kButtonIcon);
    connect(storage_button, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("ms-settings:storagesense"));
    });
    speed_row->addWidget(storage_button);

    // Cleanup Button
    cleanup_button_ = new QPushButton(QString::fromUtf16(Icons::kCleanup), this);
    cleanup_button_->setFixedSize(28, 28);
    cleanup_button_->setCursor(Qt::PointingHandCursor);
    cleanup_button_->setToolTip("Disk Cleanup");
    cleanup_button_->setStyleSheet(QString("QPushButton { font-family: '%1'; font-size: 16px; }").arg(Icons::fontName()) + UiStyle::kButtonIcon);
    connect(cleanup_button_, &QPushButton::clicked, this, &DiskPage::openCleanup);
    speed_row->addWidget(cleanup_button_);

    header_layout->addLayout(speed_row);

    // Detail row for R/W split
    header_stats_label_ = new QLabel(" ", this);
    header_stats_label_->setStyleSheet(UiStyle::kDetailSmall);
    header_stats_label_->setFixedHeight(16);
    header_layout->addWidget(header_stats_label_);

    layout->addWidget(header_container);

    // Drive List Header - Widths must match DriveRowWidget
    auto *header_row = new QHBoxLayout();
    header_row->setContentsMargins(4, 0, 8, 0); // Match row margins
    header_row->setSpacing(6); // Match row spacing

    auto *h_drives = new QLabel("DRIVE", this);
    h_drives->setStyleSheet(UiStyle::kSection);
    h_drives->setAlignment(Qt::AlignCenter);
    h_drives->setFixedWidth(28); // Match drive letter button

    auto *h_free = new QLabel("FREE", this);
    h_free->setStyleSheet(UiStyle::kSection);
    h_free->setAlignment(Qt::AlignCenter);
    h_free->setFixedWidth(55); // Reduced to give more space to bar
    
    auto *h_bar = new QLabel("", this); // Spacer for bar

    auto *h_speed = new QLabel("SPEED", this);
    h_speed->setStyleSheet(UiStyle::kSection);
    h_speed->setAlignment(Qt::AlignCenter);
    h_speed->setFixedWidth(65); // Slightly reduced

    header_row->addWidget(h_drives);
    header_row->addWidget(h_free);
    header_row->addWidget(h_bar, 1);
    header_row->addWidget(h_speed);

    layout->addLayout(header_row);

    // Scrollable Drive List
    auto *scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Disable horizontal scroll
    scroll_area->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: rgba(0,0,0,0.05); width: 6px; border-radius: 3px; }"
        "QScrollBar::handle:vertical { background: #2d2d34; min-height: 30px; border-radius: 3px; }"
        "QScrollBar::handle:vertical:hover { background: #ffffff; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    auto *scroll_content = new QWidget(scroll_area);
    drive_layout_ = new QVBoxLayout(scroll_content);
    drive_layout_->setContentsMargins(0, 0, 5, 0); // Right margin for scrollbar
    drive_layout_->setSpacing(2);
    drive_layout_->setAlignment(Qt::AlignTop);

    scroll_area->setWidget(scroll_content);
    layout->addWidget(scroll_area, 1); // Expand to fill space
}

void DiskPage::updateStats(const SystemStats &stats) {
    // total speed
    const double total_speed = stats.diskReadMBs + stats.diskWriteMBs;
    speed_label_->setText(QString("<span style='color:%1; font-weight:bold;'>%2 MB/s</span>")
        .arg(UiStyle::kColorTextMain)
        .arg(total_speed, 0, 'f', 1));

    // detail breakdown
    if (header_stats_label_) {
        header_stats_label_->setText(QString("R: <span style='color:%1;'>%2 MB/s</span> <span style='color:%3;'>|</span> "
                                             "W: <span style='color:%4;'>%5 MB/s</span>")
            .arg(UiStyle::kColorGreen)
            .arg(stats.diskReadMBs, 0, 'f', 1)
            .arg(UiStyle::kColorTextDim)
            .arg(UiStyle::kColorBlue)
            .arg(stats.diskWriteMBs, 0, 'f', 1));
    }

// ... (in updateStats)
    
    // Track current drives to identify removals
    QSet<QString> current_mountpoints;

    if (stats.drives.isEmpty()) {
        if (drive_widgets_.isEmpty()) {
             // Only add label if no widgets exist (first time empty)
             // But managing the "No drives" label with the map approach is tricky.
             // Simplest: if empty and layout empty, show label.
             if (drive_layout_->count() == 0) {
                 auto *label = new QLabel("No drives detected.", this);
                 label->setStyleSheet(UiStyle::kListItemMuted);
                 drive_layout_->addWidget(label);
             }
        }
        // If we have widgets, we might need to remove them?
        // Let's rely on the removal loop below.
    } else {
        // Remove "No drives" label if it exists (simplistic check: if first item is label)
        // Better: clear layout if it contains the label (non-widget item or mismatch)
        // Actually, just proceeding with update map logic is safer.
        // If layout has "No drives" label, drive_widgets_ will be empty.
        if (drive_widgets_.isEmpty() && drive_layout_->count() > 0) {
             clearLayout(drive_layout_);
        }
    }

    for (const auto &drive : stats.drives) {
        current_mountpoints.insert(drive.mountpoint);
        
        if (drive_widgets_.contains(drive.mountpoint)) {
            // Update existing
            drive_widgets_[drive.mountpoint]->updateInfo(drive);
        } else {
            // New drive
            auto *w = new DriveRowWidget(drive, this);
            drive_layout_->addWidget(w);
            drive_widgets_.insert(drive.mountpoint, w);
        }
    }

    // Remove stale widgets
    auto it = drive_widgets_.begin();
    while (it != drive_widgets_.end()) {
        if (!current_mountpoints.contains(it.key())) {
            DriveRowWidget *w = it.value();
            drive_layout_->removeWidget(w);
            delete w;
            it = drive_widgets_.erase(it);
        } else {
            ++it;
        }
    }
    
    if (header_name_label_) {
        header_name_label_->setText(QString("%1 Disks").arg(stats.drives.size()));
    }
}

void DiskPage::openCleanup() {
    Logger::info("ui.disk", "Open cleanup dialog.");
    CleanupDialog dialog(&cleanup_service_, this);
    dialog.exec();
}
