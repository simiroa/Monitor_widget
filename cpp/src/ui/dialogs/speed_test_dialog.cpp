#include "ui/dialogs/speed_test_dialog.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include "ui/style_tokens.h"
#include "ui/icons_material.h"
#include "utils/logger.h"

SpeedTestDialog::SpeedTestDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Network Speed Test");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setWindowModality(Qt::ApplicationModal);
    resize(300, 380); // Reduced size
    
    setStyleSheet(QString("QDialog { background-color: %1; border: 1px solid %2; border-radius: 12px; }")
        .arg(UiStyle::kColorMainBg).arg(UiStyle::kBorder));

    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // --- Header ---
    auto *header = new QWidget(this);
    header->setFixedHeight(40); // Smaller header
    header->setStyleSheet(QString("background-color: %1; border-top-left-radius: 12px; border-top-right-radius: 12px; border-bottom: 1px solid %2;")
        .arg(UiStyle::kSurface).arg(UiStyle::kBorder));
    
    auto *header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(12, 0, 12, 0);
    
    auto *icon = new QLabel(QString::fromUtf16(Icons::kSpeed), this);
    icon->setStyleSheet(QString("font-family: '%1'; font-size: 16px; color: %2;").arg(Icons::fontName()).arg(UiStyle::kColorBlue));
    header_layout->addWidget(icon);
    
    title_label_ = new QLabel("Speed Test", this);
    title_label_->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 14px; margin-left: 4px;").arg(UiStyle::kColorTextMain));
    header_layout->addWidget(title_label_);
    
    header_layout->addStretch();
    
    close_button_ = new QPushButton(QString::fromUtf16(Icons::kClose), this);
    close_button_->setFixedSize(24, 24);
    close_button_->setCursor(Qt::PointingHandCursor);
    close_button_->setStyleSheet(QString(
        "QPushButton { background: transparent; color: %1; border: none; font-family: '%2'; font-size: 16px; border-radius: 4px; }"
        "QPushButton:hover { background: #303036; color: #ed4245; }"
    ).arg(UiStyle::kColorTextDim).arg(Icons::fontName()));
    connect(close_button_, &QPushButton::clicked, this, &QDialog::reject);
    header_layout->addWidget(close_button_);
    
    main_layout->addWidget(header);

    // --- Content ---
    auto *content = new QWidget(this);
    auto *content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(20, 20, 20, 20);
    content_layout->setSpacing(16);

    // Speed Display (Big)
    auto *display_container = new QWidget(this);
    auto *display_layout = new QVBoxLayout(display_container);
    display_layout->setSpacing(2);
    display_layout->setAlignment(Qt::AlignCenter);

    status_label_ = new QLabel("Ready", this);
    status_label_->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; text-transform: uppercase; letter-spacing: 1px;")
        .arg(UiStyle::kColorTextDim));
    display_layout->addWidget(status_label_);

    auto *val_line = new QHBoxLayout();
    val_line->setAlignment(Qt::AlignCenter);
    
    speed_value_label_ = new QLabel("0.0", this);
    speed_value_label_->setStyleSheet("color: #ffffff; font-size: 48px; font-weight: bold; font-family: 'Outfit';");
    val_line->addWidget(speed_value_label_);
    
    speed_unit_label_ = new QLabel("Mbps", this);
    speed_unit_label_->setStyleSheet(QString("color: %1; font-size: 14px; font-weight: bold; margin-top: 16px;").arg(UiStyle::kColorTextDim));
    val_line->addWidget(speed_unit_label_);
    
    display_layout->addLayout(val_line);
    content_layout->addWidget(display_container);

    // Gauge Bar (Visual)
    gauge_frame_ = new QFrame(this);
    gauge_frame_->setFixedHeight(4);
    gauge_frame_->setStyleSheet("background: #2d2d34; border-radius: 2px;");
    gauge_fill_ = new QWidget(gauge_frame_);
    gauge_fill_->setFixedHeight(4);
    gauge_fill_->setStyleSheet(QString("background: %1; border-radius: 2px;").arg(UiStyle::kColorBlue));
    gauge_fill_->setFixedWidth(0);
    content_layout->addWidget(gauge_frame_);

    // Grid Results
    auto *grid_container = new QFrame(this);
    grid_container->setStyleSheet(QString("background: %1; border-radius: 8px;").arg(UiStyle::kSurface));
    auto *grid = new QGridLayout(grid_container);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setSpacing(12);

    auto createCell = [this](const QString &label, const QString &icon, QLabel* &val) {
        auto *w = new QWidget(this);
        auto *l = new QVBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(2);
        
        auto *hl = new QHBoxLayout();
        auto *ic = new QLabel(icon, this);
        ic->setStyleSheet(QString("font-family: '%1'; font-size: 12px; color: %2;").arg(Icons::fontName()).arg(UiStyle::kColorTextDim));
        auto *lb = new QLabel(label, this);
        lb->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: bold;").arg(UiStyle::kColorTextDim));
        hl->addWidget(ic);
        hl->addWidget(lb);
        hl->addStretch();
        l->addLayout(hl);
        
        val = new QLabel("-", this);
        val->setStyleSheet("color: #ffffff; font-size: 13px; font-weight: bold; font-family: 'Outfit';");
        l->addWidget(val);
        return w;
    };

    grid->addWidget(createCell("PING", QString::fromUtf16(Icons::kTemp), ping_val_), 0, 0);
    grid->addWidget(createCell("JITTER", QString::fromUtf16(Icons::kRefresh), jitter_val_), 0, 1);
    grid->addWidget(createCell("DOWNLOAD", QString::fromUtf16(Icons::kArrowDown), down_val_), 1, 0);
    grid->addWidget(createCell("UPLOAD", QString::fromUtf16(Icons::kArrowUp), up_val_), 1, 1);

    content_layout->addWidget(grid_container);

    content_layout->addStretch();

    // Start Button
    start_button_ = new QPushButton("Start Test", this);
    start_button_->setFixedHeight(36);
    start_button_->setCursor(Qt::PointingHandCursor);
    start_button_->setStyleSheet(QString(
        "QPushButton { background: %1; color: #ffffff; border: none; border-radius: 6px; font-family: 'Outfit'; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background: #4a4ab5; }"
        "QPushButton:disabled { background: #2d2d34; color: #555; }"
    ).arg(UiStyle::kColorBlue));
    connect(start_button_, &QPushButton::clicked, this, &SpeedTestDialog::startTest);
    content_layout->addWidget(start_button_);

    main_layout->addWidget(content);

    service_ = new SpeedTestService(this);
    connect(service_, &SpeedTestService::progress, this, &SpeedTestDialog::handleProgress);
    connect(service_, &SpeedTestService::finished, this, &SpeedTestDialog::handleFinished);
}

SpeedTestDialog::~SpeedTestDialog() = default;

void SpeedTestDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    updateGauge(0, "");
    
    // Auto-start test
    QTimer::singleShot(200, this, [this]() {
        startTest();
    });
}

void SpeedTestDialog::reject() {
    if (service_ && service_->isRunning()) {
        service_->cancel();
    }
    QDialog::reject();
}

void SpeedTestDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        drag_start_position_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void SpeedTestDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - drag_start_position_);
        event->accept();
    }
}

void SpeedTestDialog::startTest() {
    if (service_->isRunning()) return;

    start_button_->setEnabled(false);
    start_button_->setText("Testing...");
    close_button_->setEnabled(false);
    
    status_label_->setText("Initializing...");
    status_label_->setStyleSheet(QString("color: %1;").arg(UiStyle::kColorTextDim));
    speed_value_label_->setText("0.0");
    speed_value_label_->setStyleSheet("color: #ffffff; font-size: 64px; font-weight: bold; font-family: 'Outfit';");
    
    ping_val_->setText("-");
    jitter_val_->setText("-");
    down_val_->setText("-");
    up_val_->setText("-");
    
    gauge_fill_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(UiStyle::kColorBlue));
    updateGauge(0, "");

    service_->start();
}

void SpeedTestDialog::updateGauge(double mbps, const QString &phase) {
    // Logarithmic scale for better viz: 0-1000 Mbps
    // 0 = 0
    // 10 = ~20%
    // 100 = ~50%
    // 500 = ~80%
    // 1000 = 100%
    
    double percent = 0;
    if (mbps > 0) {
        percent = std::log10(mbps + 1) / std::log10(1001) * 100.0;
    }
    if (percent > 100) percent = 100;

    int w = gauge_frame_->width() * (percent / 100.0);
    gauge_fill_->setFixedWidth(w);
    
    if (phase == "upload") {
         gauge_fill_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(UiStyle::kColorBlue)); // Purple-ish
         status_label_->setStyleSheet(QString("color: %1;").arg(UiStyle::kColorBlue));
    } else if (phase == "download") {
         gauge_fill_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(UiStyle::kColorGreen));
         status_label_->setStyleSheet(QString("color: %1;").arg(UiStyle::kColorGreen));
    } else {
         gauge_fill_->setStyleSheet(QString("background: %1; border-radius: 3px;").arg(UiStyle::kColorTextDim));
    }
}

void SpeedTestDialog::handleProgress(const QString &phase, int percent, double currentMbps, double maxMbps) {
    if (phase == "ping") {
        status_label_->setText("Ping Test...");
        ping_val_->setText(QString("%1 ms").arg(currentMbps, 0, 'f', 0));
        speed_value_label_->setText(QString("%1").arg(currentMbps, 0, 'f', 0));
        speed_unit_label_->setText("ms");
    } else if (phase == "download") {
        status_label_->setText("Downloading...");
        down_val_->setText(QString("%1").arg(maxMbps, 0, 'f', 1));
        speed_value_label_->setText(QString("%1").arg(currentMbps, 0, 'f', 1));
        speed_unit_label_->setText("Mbps");
        updateGauge(currentMbps, "download");
    } else if (phase == "upload") {
        status_label_->setText("Uploading...");
        up_val_->setText(QString("%1").arg(maxMbps, 0, 'f', 1));
        speed_value_label_->setText(QString("%1").arg(currentMbps, 0, 'f', 1));
        speed_unit_label_->setText("Mbps");
        updateGauge(currentMbps, "upload");
    }
}

void SpeedTestDialog::handleFinished(const SpeedTestResult &result) {
    start_button_->setEnabled(true);
    start_button_->setText("Restart Test");
    close_button_->setEnabled(true);

    if (!result.error.isEmpty()) {
        status_label_->setText("Failed");
        status_label_->setStyleSheet(QString("color: %1;").arg(UiStyle::kColorRed));
        speed_value_label_->setText("ERR");
        return;
    }

    status_label_->setText("Test Complete");
    status_label_->setStyleSheet(QString("color: %1;").arg(UiStyle::kColorGreen));
    
    ping_val_->setText(QString("%1 ms").arg(result.pingMs, 0, 'f', 0));
    jitter_val_->setText(QString("%1 ms").arg(result.jitterMs, 0, 'f', 0));
    down_val_->setText(QString("%1 Mbps").arg(result.downloadMbps, 0, 'f', 1));
    up_val_->setText(QString("%1 Mbps").arg(result.uploadMbps, 0, 'f', 1));
    
    speed_value_label_->setText(QString("%1").arg(result.downloadMbps, 0, 'f', 1));
    speed_unit_label_->setText("Mbps (Down)");
    updateGauge(result.downloadMbps, "download");
}
