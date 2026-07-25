#include "ui/pages/gpu_page.h"

#include <QColor>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QSlider>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>

#include "ui/icons_material.h"

#include "services/monitor_control_service.h"
#include "ui/style_tokens.h"
#include "utils/logger.h"

GpuPage::GpuPage(MonitorControlService *control_service, QWidget *parent)
    : QWidget(parent), control_service_(control_service) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(UiStyle::kPageMarginLeft, UiStyle::kPageMarginTop, UiStyle::kPageMarginRight, UiStyle::kPageMarginBottom);
    layout->setSpacing(UiStyle::kPageSpacing);

    auto *title = new QLabel("GPU Monitor", this);
    title->setStyleSheet(UiStyle::kTitle);
    layout->addWidget(title);

    auto *header_container = new QWidget(this);
    auto *header_layout = new QVBoxLayout(header_container);
    header_layout->setContentsMargins(0, 0, 0, 6);
    header_layout->setSpacing(2);

    // Subtitle Row: GPU Name + Display Settings
    auto *subtitle_row = new QHBoxLayout();
    subtitle_row->setContentsMargins(0, 0, 0, 0);
    subtitle_row->setSpacing(4);

    name_label_ = new QLabel("GPU", this);
    name_label_->setStyleSheet(UiStyle::kSubtitle);
    name_label_->setFixedHeight(16);
    subtitle_row->addWidget(name_label_);

    subtitle_row->addStretch();

    auto *display_btn = new QPushButton("Display Set.", this);
    display_btn->setFixedSize(75, 18);
    display_btn->setStyleSheet(UiStyle::kButtonToggle + QString(" font-size: 9px; padding: 0 4px;"));
    display_btn->setToolTip("Open Windows Display Settings");
    connect(display_btn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("ms-settings:display"));
    });
    subtitle_row->addWidget(display_btn);

    header_layout->addLayout(subtitle_row);

    vram_label_ = new QLabel("VRAM: N/A", this);
    vram_label_->setStyleSheet(UiStyle::kValueLarge);
    vram_label_->setFixedHeight(22);
    header_layout->addWidget(vram_label_);

    stats_label_ = new QLabel("Clock: N/A | Power: N/A | Temp: N/A", this);
    stats_label_->setStyleSheet(UiStyle::kDetailSmall);
    stats_label_->setFixedHeight(16);
    header_layout->addWidget(stats_label_);

    layout->addWidget(header_container);

    notice_label_ = new QLabel("", this);
    notice_label_->setStyleSheet(UiStyle::kNote);
    notice_label_->setWordWrap(true);
    layout->addWidget(notice_label_);

    auto *ctrl_title = new QLabel("Monitor Controls", this);
    ctrl_title->setStyleSheet(QString("%1 margin-top: 6px;").arg(UiStyle::kSection));
    layout->addWidget(ctrl_title);

    auto *row1 = new QHBoxLayout();
    row1->setSpacing(4);

    night_button_ = new QPushButton("Night mode", this);
    night_button_->setCheckable(true);
    night_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    night_button_->setFixedHeight(22);
    night_button_->setStyleSheet(UiStyle::kButtonToggle);
    connect(night_button_, &QPushButton::toggled, this, &GpuPage::toggleNightMode);
    row1->addWidget(night_button_);

    hdr_button_ = new QPushButton("HDR", this);
    hdr_button_->setCheckable(true);
    hdr_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hdr_button_->setFixedHeight(22);
    hdr_button_->setStyleSheet(UiStyle::kButtonToggle);
    connect(hdr_button_, &QPushButton::toggled, this, &GpuPage::toggleHdr);
    row1->addWidget(hdr_button_);

    sleep_button_ = new QPushButton("Monitor sleep", this);
    sleep_button_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sleep_button_->setFixedHeight(22);
    sleep_button_->setStyleSheet(UiStyle::kButtonDanger);
    connect(sleep_button_, &QPushButton::clicked, this, [this]() {
        if (control_service_) {
            control_service_->turnOffMonitors();
        }
    });

    row1->addWidget(sleep_button_);
    layout->addLayout(row1);

    auto *bright_row = new QHBoxLayout();
    auto *bright_label = new QLabel("Brightness", this);
    bright_label->setFixedWidth(65); // Align with Refresh label
    bright_label->setStyleSheet(UiStyle::kDetailSmall);

    brightness_slider_ = new QSlider(Qt::Horizontal, this);
    brightness_slider_->setRange(0, 100);
    brightness_slider_->setFixedHeight(18);
    connect(brightness_slider_, &QSlider::valueChanged, this, &GpuPage::brightnessChanged);

    brightness_value_ = new QLabel("-", this);
    brightness_value_->setStyleSheet(UiStyle::kSubtitle);
    brightness_value_->setFixedWidth(52); // Match Apply button width
    brightness_value_->setAlignment(Qt::AlignCenter);

    bright_row->addWidget(bright_label);
    bright_row->addWidget(brightness_slider_, 1);
    bright_row->addWidget(brightness_value_);
    layout->addLayout(bright_row);

    auto *refresh_row = new QHBoxLayout();
    auto *refresh_label = new QLabel("Refresh", this);
    refresh_label->setFixedWidth(65); // Align with Brightness label
    refresh_label->setStyleSheet(UiStyle::kDetailSmall);

    refresh_combo_ = new QComboBox(this);
    refresh_combo_->setFixedHeight(22);
    refresh_combo_->setStyleSheet(UiStyle::kComboBox);

    refresh_apply_ = new QPushButton("Apply", this);
    refresh_apply_->setFixedSize(52, 22);
    refresh_apply_->setStyleSheet(UiStyle::kButtonPrimary);
    connect(refresh_apply_, &QPushButton::clicked, this, &GpuPage::applyRefreshRate);

    refresh_row->addWidget(refresh_label);
    refresh_row->addWidget(refresh_combo_, 1);
    refresh_row->addWidget(refresh_apply_);
    layout->addLayout(refresh_row);

    control_notice_label_ = new QLabel("", this);
    control_notice_label_->setStyleSheet(UiStyle::kNote);
    control_notice_label_->setWordWrap(true);
    control_notice_label_->setVisible(false);
    layout->addWidget(control_notice_label_);

    layout->addStretch();

    syncControls();
}

void GpuPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    syncControls();
}

void GpuPage::syncControls() {
    if (!control_service_) {
        return;
    }

    Logger::info("ui.gpu", "Syncing controls...");

    night_button_->blockSignals(true);
    night_button_->setChecked(control_service_->nightModeEnabled());
    night_button_->blockSignals(false);

    const bool hdr_supported = control_service_->hdrSupported();
    hdr_button_->setEnabled(hdr_supported);
    hdr_button_->setToolTip(hdr_supported ? "Toggle HDR" : "HDR not supported on primary display");
    
    if (hdr_supported) {
        hdr_button_->blockSignals(true);
        hdr_button_->setChecked(control_service_->hdrEnabled());
        hdr_button_->blockSignals(false);
    }

    Logger::info("ui.gpu", "Getting brightness...");
    const int brightness = control_service_->brightness();
    Logger::info("ui.gpu", QString("Brightness=%1").arg(brightness));
    
    if (brightness >= 0) {
        brightness_slider_->setEnabled(true);
        brightness_slider_->setToolTip("");
        brightness_slider_->blockSignals(true);
        brightness_slider_->setValue(brightness);
        brightness_slider_->blockSignals(false);
        brightness_value_->setText(QString("%1%").arg(brightness));
    } else {
        brightness_slider_->setEnabled(false);
        brightness_slider_->setToolTip("Brightness control unavailable.");
        brightness_value_->setText("N/A");
    }

    Logger::info("ui.gpu", "Getting refresh rates...");
    refresh_combo_->clear();
    const QVector<int> rates = control_service_->availableRefreshRates();
    for (int rate : rates) {
        refresh_combo_->addItem(QString("%1 Hz").arg(rate), rate);
    }

    const int current = control_service_->currentRefreshRate();
    int index = refresh_combo_->findData(current);
    if (index >= 0) {
        refresh_combo_->setCurrentIndex(index);
    }

    const bool has_rates = !rates.isEmpty();
    refresh_combo_->setEnabled(has_rates);
    refresh_apply_->setEnabled(has_rates);
    if (!has_rates) {
        refresh_combo_->setToolTip("Refresh rate control unavailable.");
    } else {
        refresh_combo_->setToolTip("");
    }

    if (control_notice_label_) {
        QStringList notices;
        if (!hdr_supported) {
            notices << "HDR 미지원";
        }
        if (brightness < 0) {
            notices << "밝기 제어 미지원";
        }
        if (!has_rates) {
            notices << "주사율 변경 미지원";
        }

        if (notices.isEmpty()) {
            control_notice_label_->setVisible(false);
            control_notice_label_->setText("");
        } else {
            control_notice_label_->setVisible(true);
            control_notice_label_->setText("안내: " + notices.join(" · "));
        }
    }
}

void GpuPage::applyRefreshRate() {
    if (!control_service_) {
        return;
    }

    const int index = refresh_combo_->currentIndex();
    if (index < 0) {
        return;
    }

    const int rate = refresh_combo_->currentData().toInt();
    Logger::info("ui.gpu", QString("Apply refresh rate %1Hz.").arg(rate));
    control_service_->setRefreshRate(rate);
}

void GpuPage::toggleNightMode(bool enabled) {
    if (!control_service_) {
        return;
    }
    Logger::info("ui.gpu", QString("Night mode toggled %1.").arg(enabled ? "on" : "off"));
    control_service_->setNightMode(enabled);
}

void GpuPage::toggleHdr(bool enabled) {
    if (!control_service_) {
        return;
    }
    Logger::info("ui.gpu", QString("HDR toggled %1.").arg(enabled ? "on" : "off"));
    if (!control_service_->setHdrMode(enabled)) {
        // Revert check state if failed
        hdr_button_->blockSignals(true);
        hdr_button_->setChecked(!enabled); 
        hdr_button_->blockSignals(false);
    }
}

void GpuPage::brightnessChanged(int value) {
    brightness_value_->setText(QString("%1%").arg(value));
    if (control_service_) {
        control_service_->setBrightness(value);
    }
}

void GpuPage::updateStats(const SystemStats &stats) {
    name_label_->setText(stats.gpuName.isEmpty() ? "GPU" : stats.gpuName);

    if (stats.capabilities.vramAvailable && stats.vramTotalGB > 0.0) {
        const double vram_percent = (stats.vramUsedGB / stats.vramTotalGB) * 100.0;
        vram_label_->setText(QString("<span style='color:%1; font-weight:bold;'>%2GB</span> "
                                     "<span style='color:%3;'>/ %4GB</span>")
            .arg(UiStyle::kColorGreen)
            .arg(stats.vramUsedGB, 0, 'f', 1)
            .arg(UiStyle::kColorTextDim)
            .arg(stats.vramTotalGB, 0, 'f', 1));
    } else {
        vram_label_->setText("VRAM: N/A");
    }

    const QString clock = stats.capabilities.gpuClockAvailable
        ? QString("%1GHz").arg(stats.gpuClockMHz / 1000.0, 0, 'f', 1)
        : "N/A";
    const QString power = stats.capabilities.gpuPowerAvailable
        ? QString("%1W").arg(stats.gpuPowerW, 0, 'f', 0)
        : "N/A";
    const QString temp = stats.capabilities.gpuTempAvailable
        ? QString("%1C").arg(stats.gpuTempC, 0, 'f', 0)
        : "N/A";

    const QColor temp_color_val = stats.gpuTempC >= 80.0 ? QColor(UiStyle::kColorRed)
        : (stats.gpuTempC >= 65.0 ? QColor(UiStyle::kColorOrange) : QColor(UiStyle::kColorGreen));

    stats_label_->setText(QString("%1 <span style='color:%2;'>|</span> %3 <span style='color:%2;'>|</span> <span style='color:%4; font-weight:bold;'>%5</span>")
        .arg(clock)
        .arg(UiStyle::kColorTextDim)
        .arg(power)
        .arg(temp_color_val.name())
        .arg(temp));

    if (stats.capabilities.gpuBackend == "NVML") {
        notice_label_->setVisible(false);
        notice_label_->setText("");
    } else {
        notice_label_->setVisible(true);
        if (stats.capabilities.gpuVendor == "NVIDIA") {
            if (!stats.capabilities.nvmlEnabled) {
                notice_label_->setText("NVML disabled. Remove MONITOR_WIDGET_ENABLE_NVML=0 to enable NVIDIA-only metrics.");
            } else if (!stats.capabilities.nvmlAvailable) {
                notice_label_->setText("NVML unavailable. Update NVIDIA driver (nvml.dll) to enable power/temp/clock.");
            } else {
                notice_label_->setText("NVIDIA-only metrics unavailable.");
            }
        } else {
            notice_label_->setText("NVIDIA-only metrics show N/A on other GPUs.");
        }
    }
}
