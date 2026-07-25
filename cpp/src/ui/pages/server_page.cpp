#include "ui/pages/server_page.h"

#include <QColor>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include "services/process_control_service.h"
#include "ui/layout_utils.h"
#include "ui/sidebar_item.h"
#include "ui/style_tokens.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h"
#include "utils/logger.h"

namespace {
QString elideName(const QString &name) {
    if (name.size() <= 18) {
        return name;
    }
    return name.left(16) + "..";
}

QColor typeColor(const QString &type) {
    if (type == "comfyui") {
        return QColor("#ff6b00");
    }
    if (type == "docker") {
        return QColor("#2496ed");
    }
    if (type == "jupyter") {
        return QColor("#f37626");
    }
    if (type == "vite") {
        return QColor("#646cff");
    }
    if (type == "webpack") {
        return QColor("#8dd6f9");
    }
    if (type == "react") {
        return QColor("#61dafb");
    }
    if (type == "vue") {
        return QColor("#42b883");
    }
    if (type == "angular") {
        return QColor("#dd1b16");
    }
    if (type == "flask") {
        return QColor("#ffffff");
    }
    if (type == "django") {
        return QColor("#092e20");
    }
    if (type == "fastapi") {
        return QColor("#009688");
    }
    if (type == "nodejs") {
        return QColor("#339933");
    }
    if (type == "streamlit") {
        return QColor("#ff4b4b");
    }
    if (type == "gradio") {
        return QColor("#ff7c00");
    }
    if (type == "unity") {
        return QColor("#ffffff");
    }
    if (type == "unreal") {
        return QColor("#313131");
    }
    if (type == "blender") {
        return QColor("#f5792a");
    }
    if (type == "python") {
        return QColor("#3776ab");
    }
    if (type == "webserver") {
        return QColor("#888888");
    }
    return QColor(UiStyle::kColorTextDim);
}
}  // namespace

ServerPage::ServerPage(ProcessControlService *process_service, QWidget *parent)
    : QWidget(parent), process_service_(process_service) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(UiStyle::kPageMarginLeft, UiStyle::kPageMarginTop, UiStyle::kPageMarginRight, UiStyle::kPageMarginBottom);
    layout->setSpacing(UiStyle::kPageSpacing);

    auto *title = new QLabel("Local Servers", this);
    title->setStyleSheet(UiStyle::kTitle);
    layout->addWidget(title);

    count_label_ = new QLabel("Active ports: 0", this);
    count_label_->setStyleSheet(UiStyle::kDetail);
    layout->addWidget(count_label_);

    auto *search_row = new QWidget(this);
    auto *search_layout = new QHBoxLayout(search_row);
    search_layout->setContentsMargins(0, 0, 0, 0);
    search_layout->setSpacing(4);

    auto *search_icon = new QLabel(QString::fromUtf16(Icons::kSearch), search_row);
    search_icon->setStyleSheet(QString("font-family: '%1'; font-size: 16px; color: #8b8b94;").arg(Icons::fontName()));
    search_layout->addWidget(search_icon);

    search_box_ = new QLineEdit(search_row);
    search_box_->setPlaceholderText("Search port or name...");
    search_box_->setStyleSheet(UiStyle::kLineEdit);
    connect(search_box_, &QLineEdit::textChanged, this, [this](const QString &text) {
        rebuildList(text.trimmed().toLower());
    });
    search_layout->addWidget(search_box_, 1);
    layout->addWidget(search_row);

    scroll_area_ = new QScrollArea(this);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_area_->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: rgba(0,0,0,0.05); width: 6px; border-radius: 3px; }"
        "QScrollBar::handle:vertical { background: #2d2d34; min-height: 30px; border-radius: 3px; }"
        "QScrollBar::handle:vertical:hover { background: #ffffff; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
    );

    auto *scroll_container = new QWidget(this);
    scroll_container->setStyleSheet("background: transparent;");
    list_layout_ = new QVBoxLayout(scroll_container);
    list_layout_->setContentsMargins(0, 0, 5, 0);
    list_layout_->setSpacing(2); // Reduced from 4
    list_layout_->setAlignment(Qt::AlignTop);
    scroll_area_->setWidget(scroll_container);
    layout->addWidget(scroll_area_, 1);
}

bool ServerPage::samePoints(const QVector<ServerPoint> &next) const {
    if (next.size() != all_points_.size()) {
        return false;
    }
    for (int i = 0; i < next.size(); ++i) {
        const auto &a = next[i];
        const auto &b = all_points_[i];
        if (a.port != b.port || a.pid != b.pid || a.name != b.name || a.serverType != b.serverType || a.exePath != b.exePath) {
            return false;
        }
    }
    return true;
}

void ServerPage::rebuildList(const QString &filter) {
    clearLayout(list_layout_);

    if (all_points_.isEmpty()) {
        auto *label = new QLabel("Searching for active ports...", this);
        label->setStyleSheet(UiStyle::kListItemMuted);
        list_layout_->addWidget(label);
        return;
    }

    int shown = 0;
    for (const auto &point : all_points_) {
        const QString search_text = QString("%1 %2 %3").arg(point.port).arg(point.name).arg(point.serverType).toLower();
        if (!filter.isEmpty() && !search_text.contains(filter)) {
            continue;
        }

        ++shown;
        auto *row = new QFrame(this);
        row->setStyleSheet("background: transparent;");
        row->setFixedHeight(24);

        auto *row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(4, 0, 4, 0);
        row_layout->setSpacing(2); // Reduced from 4

        if (!point.serverType.isEmpty()) {
            auto *indicator = new QFrame(row);
            indicator->setFixedSize(3, 14);
            indicator->setStyleSheet(QString("background-color: %1; border-radius: 1px;")
                .arg(typeColor(point.serverType).name()));
            row_layout->addWidget(indicator);
        } else {
            auto *spacer = new QFrame(row);
            spacer->setFixedSize(3, 14);
            spacer->setStyleSheet("background: transparent;");
            row_layout->addWidget(spacer);
        }

        auto *port_label = new QLabel(QString(":%1").arg(point.port), row);
        port_label->setFixedWidth(50);
        port_label->setStyleSheet("color: #ffffff; font-weight: bold; font-family: Consolas, monospace; font-size: 11px;");
        row_layout->addWidget(port_label);

        const QString display_name = elideName(point.name);
        auto *name_label = new QLabel(display_name, row);
        name_label->setStyleSheet(UiStyle::kListItem);
        if (!point.exePath.isEmpty()) {
            name_label->setToolTip(QString("PID: %1\nPath: %2").arg(point.pid).arg(point.exePath));
        } else {
            name_label->setToolTip(QString("PID: %1").arg(point.pid));
        }
        row_layout->addWidget(name_label, 1);

        auto *open_btn = new QPushButton(QString::fromUtf16(Icons::kOpenInNew), row);
        open_btn->setFixedSize(20, 20);
        open_btn->setCursor(Qt::PointingHandCursor);
        open_btn->setStyleSheet(
            QString("QPushButton { color: #8b8b94; background: transparent; border: none; font-family: '%1'; font-size: 14px; }")
            .arg(Icons::fontName()) +
            "QPushButton:hover { color: #3ba55c; }"
        );
        connect(open_btn, &QPushButton::clicked, this, [point]() {
            const QUrl url(QString("http://localhost:%1").arg(point.port));
            QDesktopServices::openUrl(url);
        });
        row_layout->addWidget(open_btn);

        auto *kill_btn = new QPushButton(QString::fromUtf16(Icons::kKill), row);
        kill_btn->setFixedSize(20, 20);
        kill_btn->setCursor(Qt::PointingHandCursor);
        kill_btn->setStyleSheet(
            QString("QPushButton { color: #ed4245; background: transparent; border: none; font-family: '%1'; font-size: 14px; }")
            .arg(Icons::fontName()) +
            "QPushButton:disabled { color: #555555; }"
        );
        const bool can_kill = process_service_ && !process_service_->isProtectedName(point.name);
        kill_btn->setEnabled(can_kill);
        connect(kill_btn, &QPushButton::clicked, this, [this, point]() {
            if (!process_service_) {
                return;
            }
            Logger::info("ui.server", QString("Kill requested pid=%1.").arg(point.pid));
            process_service_->killProcess(static_cast<qulonglong>(point.pid), nullptr);
        });
        row_layout->addWidget(kill_btn);

        list_layout_->addWidget(row);
    }

    if (shown == 0) {
        auto *label = new QLabel("No servers match filter.", this);
        label->setStyleSheet(UiStyle::kListItemMuted);
        list_layout_->addWidget(label);
    }
}

void ServerPage::updateStats(const SystemStats &stats) {
    if (stats.serverPoints.isEmpty() && all_points_.isEmpty()) {
        count_label_->setText("Active ports: 0");
        if (list_layout_ && list_layout_->count() == 0) {
            rebuildList(QString());
        }
        return;
    }

    // If stats.serverPoints is empty, we should clear the list (unless we want to persist old data, but usually we don't).
    // The previous logic prevented clearing: if (stats.serverPoints.isEmpty() && !all_points_.isEmpty()) return;
    // We removed it to allow clearing.

    if (samePoints(stats.serverPoints)) {
        return;
    }

    all_points_ = stats.serverPoints;
    count_label_->setText(QString("Active ports: %1").arg(all_points_.size()));
    rebuildList(search_box_->text().trimmed().toLower());
}
