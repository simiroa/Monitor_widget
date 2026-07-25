#include "ui/pages/net_page.h"
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QHBoxLayout>
#include <QAbstractSocket>
#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QResizeEvent>
#include <QStyle>
#include <QVBoxLayout>
#include <QClipboard>
#include <QApplication>
#include <QMouseEvent>

#include "ui/dialogs/speed_test_dialog.h"
#include "ui/pages/process_list_page.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"
#include "utils/logger.h"

namespace {
QString formatNetSpeed(double mbs) {
    if (mbs <= 0.0) return "0 KB/s";
    if (mbs >= 1024.0) return QString("%1 GB/s").arg(mbs / 1024.0, 0, 'f', 1);
    if (mbs >= 1.0) return QString("%1 MB/s").arg(mbs, 0, 'f', 1);
    return QString("%1 KB/s").arg(mbs * 1024.0, 0, 'f', 0);
}

void setElidedText(QLabel *label, const QString &text, int maxWidth) {
    if (!label) return;
    const QFontMetrics fm(label->font());
    label->setText(fm.elidedText(text, Qt::ElideRight, qMax(30, maxWidth)));
    label->setToolTip(text);
}
} // namespace

NetPage::NetPage(QWidget *parent)
    : QWidget(parent) {
    auto *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(UiStyle::kPageMarginLeft, UiStyle::kPageMarginTop, UiStyle::kPageMarginRight, UiStyle::kPageMarginBottom);
    main_layout->setSpacing(UiStyle::kPageSpacing);

    auto *title = new QLabel("Network Usage", this);
    title->setStyleSheet(UiStyle::kTitle);
    main_layout->addWidget(title);

    // --- Standard Header (ProcessListPage Style) ---
    auto *header_container = new QWidget(this);
    auto *header_layout = new QVBoxLayout(header_container);
    header_layout->setContentsMargins(0, 0, 0, 8);
    header_layout->setSpacing(4);

    // Row 1: Public IP
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(6);
    auto *pub_icon = new QLabel(QString::fromUtf16(Icons::kLanguage), this);
    pub_icon->setStyleSheet(QString("font-family: '%1'; font-size: 14px; color: %2;").arg(Icons::fontName()).arg(UiStyle::kColorBlue));
    
    public_ip_label_ = new QLabel("Public IP: Loading...", this);
    public_ip_label_->setStyleSheet(UiStyle::kSubtitle);
    public_ip_label_->setCursor(Qt::PointingHandCursor);
    public_ip_label_->installEventFilter(this);
    
    row1->addWidget(pub_icon);
    row1->addWidget(public_ip_label_);
    row1->addStretch();
    
    // Local IP (Same Row or Next?) -> Let's keep it next for now to save common header
    // Actually user said remove ISP. Let's put Local IP on next line or same line?
    // Let's try putting them on separate rows to be safe for now, or compact.
    // Given the user wants "compact", maybe same row if possible?
    // Let's stick to the previous 3-line structure minus ISP, but maybe merge rows if efficient?
    // User accepted 3 lines. I'll stick to distinct rows for clarity.
    // Row 1: Public
    // Row 2: Local
    header_layout->addLayout(row1);

    // Row 2: Local IP
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(6);
    auto *loc_icon = new QLabel(QString::fromUtf16(Icons::kComputer), this);
    loc_icon->setStyleSheet(QString("font-family: '%1'; font-size: 14px; color: %2;").arg(Icons::fontName()).arg(UiStyle::kColorTextDim));
    
    local_ip_label_ = new QLabel("Local IP: Loading...", this);
    local_ip_label_->setStyleSheet(UiStyle::kDetailSmall);
    local_ip_label_->setCursor(Qt::PointingHandCursor);
    local_ip_label_->installEventFilter(this);
    
    row2->addWidget(loc_icon);
    row2->addWidget(local_ip_label_);
    row2->addStretch();
    header_layout->addLayout(row2);

    // Row 3: Traffic Stats + Buttons
    auto *row3 = new QHBoxLayout();
    row3->setSpacing(8);

    speed_detail_label_ = new QLabel("D: 0.0 MB/s | U: 0.0 MB/s", this);
    speed_detail_label_->setStyleSheet(QString("font-weight: 600; font-family: 'Outfit'; font-size: 14px; color: %1;").arg(UiStyle::kColorTextMain));
    speed_detail_label_->setTextFormat(Qt::RichText); // Enable HTML
    row3->addWidget(speed_detail_label_);
    
    row3->addStretch();

    // Buttons: Vertical Layout
    auto *btn_layout = new QVBoxLayout();
    btn_layout->setSpacing(4);
    btn_layout->setContentsMargins(0, 0, 0, 0);

    test_button_ = new QPushButton(QString::fromUtf16(Icons::kSpeed), this);
    test_button_->setFixedSize(32, 24); // Slightly shorter for stacking
    test_button_->setCursor(Qt::PointingHandCursor);
    test_button_->setStyleSheet(QString("QPushButton { font-family: '%1'; font-size: 16px; padding: 0px; }").arg(Icons::fontName()) + UiStyle::kButtonIcon);
    test_button_->setToolTip("Speed Test");
    connect(test_button_, &QPushButton::clicked, this, &NetPage::openSpeedTest);
    btn_layout->addWidget(test_button_);

    settings_button_ = new QPushButton(QString::fromUtf16(Icons::kSettings), this);
    settings_button_->setFixedSize(32, 24);
    settings_button_->setCursor(Qt::PointingHandCursor);
    settings_button_->setStyleSheet(QString("QPushButton { font-family: '%1'; font-size: 16px; padding: 0px; }").arg(Icons::fontName()) + UiStyle::kButtonIcon);
    settings_button_->setToolTip("Network Settings");
    connect(settings_button_, &QPushButton::clicked, this, &NetPage::openNetworkSettings);
    btn_layout->addWidget(settings_button_);

    row3->addLayout(btn_layout);
    header_layout->addLayout(row3);
    main_layout->addWidget(header_container);

    process_list_ = new ProcessListPage("", ProcessListPage::Mode::Net, nullptr, this);
    process_list_->setStyleSheet("margin-top: 0px;"); // Ensure no extra margin
    main_layout->addWidget(process_list_, 1);

    net_manager_ = new QNetworkAccessManager(this);
    QTimer::singleShot(500, this, &NetPage::fetchNetworkInfo);
}

NetPage::~NetPage() = default;

void NetPage::updateStats(const SystemStats &stats) {
    last_stats_ = stats;
    double total = stats.netRecvMBs + stats.netSentMBs;
    
    if (speed_detail_label_) {
        // Color coding for D/U
        auto getColor = [](double val) {
            if (val >= 10.0) return UiStyle::kColorRed;
            if (val >= 1.0) return UiStyle::kColorOrange;
            if (val > 0.0) return UiStyle::kColorGreen;
            return UiStyle::kColorTextMain;
        };
        
        QString d_color = getColor(stats.netRecvMBs);
        QString u_color = getColor(stats.netSentMBs);

        speed_detail_label_->setText(QString("D: <font color='%1'>%2</font> <span style='color:#555'>|</span> U: <font color='%3'>%4</font>")
            .arg(d_color)
            .arg(formatNetSpeed(stats.netRecvMBs))
            .arg(u_color)
            .arg(formatNetSpeed(stats.netSentMBs)));
    }
    // if (total_speed_label_) {
    //     total_speed_label_->setText(QString("(Total: %1)").arg(formatNetSpeed(total)));
    // }
    
    if (process_list_) {
        process_list_->updateStats(stats);
    }
    refreshTooltip();
}

void NetPage::updateProcesses(const QVector<ProcessInfo> &processes) {
    QVector<ProcessInfo> adjusted = processes;
    const double total_system = last_stats_.netRecvMBs + last_stats_.netSentMBs;
    double total_proc = 0.0;
    for (const auto &proc : adjusted) {
        total_proc += (proc.netReadMBs + proc.netWriteMBs);
    }

    if (total_system > 0.0 && total_proc > (total_system * 2.0)) {
        const double scale = total_system / total_proc;
        for (auto &proc : adjusted) {
            proc.netReadMBs *= scale;
            proc.netWriteMBs *= scale;
            if (proc.netReadMBs < 0.0) proc.netReadMBs = 0.0;
            if (proc.netWriteMBs < 0.0) proc.netWriteMBs = 0.0;
        }
    }

    last_processes_ = adjusted;
    if (process_list_) {
        process_list_->updateProcesses(adjusted);
    }
    refreshTooltip();
}

void NetPage::refreshTooltip() {
    // Tooltip logic remains similar
}

void NetPage::fetchNetworkInfo() {
    QString local_ip = "Unknown";
    const auto addresses = QNetworkInterface::allAddresses();
    for (const auto &addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) {
            local_ip = addr.toString();
            break;
        }
    }
    raw_local_ip_ = QString("Local IP: %1").arg(local_ip);
    
    const QUrl url("http://ip-api.com/json/?fields=query"); // No ISP
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "MonitorWidget");
    QNetworkReply *reply = net_manager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            raw_public_ip_ = "Public IP: Offline";
            updateNetInfoLabels();
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;
        const QJsonObject obj = doc.object();
        const QString ip = obj.value("query").toString();
        raw_public_ip_ = QString("Public IP: %1").arg(ip.isEmpty() ? "Unknown" : ip);
        updateNetInfoLabels();
    });
    updateNetInfoLabels();
}

void NetPage::updateNetInfoLabels() {
    const int w = width() > 0 ? width() : 400;
    setElidedText(public_ip_label_, raw_public_ip_, w - 40);
    setElidedText(local_ip_label_, raw_local_ip_, w - 40);
}

void NetPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateNetInfoLabels();
}

bool NetPage::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (watched == public_ip_label_) {
                QString ip = raw_public_ip_.contains(':') ? raw_public_ip_.mid(raw_public_ip_.indexOf(':') + 2) : raw_public_ip_;
                copyToClipboard(ip, "Public IP");
                return true;
            } else if (watched == local_ip_label_) {
                QString ip = raw_local_ip_.contains(':') ? raw_local_ip_.mid(raw_local_ip_.indexOf(':') + 2) : raw_local_ip_;
                copyToClipboard(ip, "Local IP");
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NetPage::copyToClipboard(const QString &text, const QString &labelName) {
    QGuiApplication::clipboard()->setText(text.trimmed());
    Logger::info("ui.net", QString("%1 copied to clipboard: %2").arg(labelName).arg(text));
}

void NetPage::openNetworkSettings() {
    QProcess::startDetached("ms-settings:network");
}

void NetPage::openSpeedTest() {
    SpeedTestDialog dialog(this);
    dialog.exec();
}
