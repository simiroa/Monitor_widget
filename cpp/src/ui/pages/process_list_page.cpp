#include "ui/pages/process_list_page.h"

#include <algorithm>
#include <vector>
#include <cmath>

#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

#include "utils/win_utils.h"
#include "ui/style_tokens.h"
#include "models/system_stats.h"
#include "services/process_control_service.h"
#include "ui/layout_utils.h"
#include "ui/process_row.h"
#include "ui/net_tooltip_helper.h"
#include "utils/logger.h"

#ifdef _WIN32
#include <pdh.h>
#pragma comment(lib, "PowrProf.lib")
#endif

namespace {
constexpr int kMaxRowsDefault = 6;
constexpr int kMaxRowsRam = 6;

// (CPU helper functions moved to WinUtils)

QColor usageColor(double percent) {
    if (percent >= 90.0) return QColor(UiStyle::kColorRed);
    if (percent >= 70.0) return QColor(UiStyle::kColorOrange);
    if (percent >= 40.0) return QColor(UiStyle::kColorYellow);
    return QColor(UiStyle::kColorGreen);
}

}  // namespace

ProcessListPage::ProcessListPage(const QString &title, Mode mode, ProcessControlService *service, QWidget *parent)
    : QWidget(parent), mode_(mode), process_service_(service) {
    auto *layout = new QVBoxLayout(this);
    if (mode_ == Mode::Net) {
        layout->setContentsMargins(0, 0, 0, 0);
    } else {
        layout->setContentsMargins(UiStyle::kPageMarginLeft, UiStyle::kPageMarginTop, UiStyle::kPageMarginRight, UiStyle::kPageMarginBottom);
    }
    layout->setSpacing(UiStyle::kPageSpacing);

    if (!title.isEmpty()) {
        title_label_ = new QLabel(title, this);
        title_label_->setStyleSheet(UiStyle::kTitle);
        layout->addWidget(title_label_);
    }

    if (mode_ != Mode::Gpu && mode_ != Mode::Net) {
        auto *header_container = new QWidget(this);
        auto *header_layout = new QVBoxLayout(header_container);
        header_layout->setContentsMargins(0, 0, 0, 6);
        header_layout->setSpacing(2);

        header_name_label_ = new QLabel(this);
        header_name_label_->setStyleSheet(UiStyle::kSubtitle);
        header_name_label_->setFixedHeight(16);
        header_layout->addWidget(header_name_label_);

        header_main_label_ = new QLabel(this);
        header_main_label_->setStyleSheet(UiStyle::kValueLarge);
        header_main_label_->setFixedHeight(22);
        header_layout->addWidget(header_main_label_);

        header_stats_label_ = new QLabel(this);
        header_stats_label_->setStyleSheet(UiStyle::kDetailSmall);
        header_stats_label_->setFixedHeight(16);
        header_layout->addWidget(header_stats_label_);

        layout->addWidget(header_container);
    } else if (mode_ == Mode::Gpu) {
        subtitle_label_ = new QLabel(this);
        subtitle_label_->setStyleSheet(UiStyle::kSubtitle);
        subtitle_label_->setFixedHeight(16);
        layout->addWidget(subtitle_label_);

        detail_label_ = new QLabel(this);
        detail_label_->setStyleSheet(UiStyle::kDetail);
        detail_label_->setFixedHeight(22);
        detail_label_->setWordWrap(true);
        layout->addWidget(detail_label_);
    }

    if (mode_ == Mode::Gpu || mode_ == Mode::Vram) {
        notice_label_ = new QLabel(this);
        notice_label_->setStyleSheet(UiStyle::kNote);
        notice_label_->setWordWrap(true);
        layout->addWidget(notice_label_);
    }

    if (mode_ != Mode::Net) {
        auto *list_title = new QLabel("Top processes", this);
        list_title->setStyleSheet(QString("%1 margin-top: 4px;").arg(UiStyle::kSection));
        layout->addWidget(list_title);
    }

    auto *list_container = new QWidget(this);
    list_layout_ = new QVBoxLayout(list_container);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(2);
    layout->addWidget(list_container);

    layout->addStretch();
}

void ProcessListPage::updateStats(const SystemStats &stats) {
    last_caps_ = stats.capabilities;

    switch (mode_) {
        case Mode::Cpu:  updateCpuHeader(stats);  break;
        case Mode::Ram:  updateRamHeader(stats);  break;
        case Mode::Gpu:  updateGpuHeader(stats);  break;
        case Mode::Vram: updateVramHeader(stats); break;
        case Mode::Net:  break;
    }
}

void ProcessListPage::updateCpuHeader(const SystemStats &stats) {
    if (header_name_label_) {
        header_name_label_->setText(WinUtils::fetchCpuName());
    }
    if (header_main_label_) {
        header_main_label_->setText(QString("<span style='color:%1; font-weight:bold;'>%2%</span>")
            .arg(UiStyle::kColorGreen).arg(stats.cpuPercent, 0, 'f', 1));
    }

    if (header_stats_label_) {
        const double clock_mhz = WinUtils::fetchCpuClockMHz();
        double current_mhz = 0.0;
#ifdef _WIN32
        current_mhz = WinUtils::queryCurrentClockMHz();
        if (current_mhz <= 0.0 && clock_mhz > 0.0) {
            const double perf = WinUtils::queryCpuPerformancePercent();
            if (perf > 0.0) current_mhz = clock_mhz * (perf / 100.0);
        }
#endif

        QStringList parts;
        if (clock_mhz > 0.0) parts.append(QString("Base: %1GHz").arg(clock_mhz / 1000.0, 0, 'f', 1));
        if (current_mhz > 0.0) parts.append(QString("Cur: %1GHz").arg(current_mhz / 1000.0, 0, 'f', 1));

        const int cores = WinUtils::fetchPhysicalCores();
        const int threads = WinUtils::fetchLogicalThreads();
        if (cores > 0 && threads > 0) parts.append(QString("%1C/%2T").arg(cores).arg(threads));

        QString stats_line;
        for (int i = 0; i < parts.size(); ++i) {
            if (i > 0) stats_line += QString(" <span style='color:%1;'>|</span> ").arg(UiStyle::kColorTextDim);
            stats_line += QString("<span style='color:%1;'>%2</span>").arg(UiStyle::kColorTextDim).arg(parts[i]);
        }

        if (stats.cpuTempC > 0.0) {
            const QString temp_color = stats.cpuTempC > 75.0 ? UiStyle::kColorRed : UiStyle::kColorGreen;
            stats_line += QString(" <span style='color:%1;'>|</span> Temp: <span style='color:%2; font-weight:bold;'>%3C</span>")
                .arg(UiStyle::kColorTextDim).arg(temp_color).arg(stats.cpuTempC, 0, 'f', 0);
        }

        if (stats.capabilities.cpuPowerAvailable && stats.cpuPowerW > 0.0) {
            stats_line += QString(" <span style='color:%1;'>|</span> Power: <span style='color:%2; font-weight:bold;'>%3W</span>")
                .arg(UiStyle::kColorTextDim).arg(UiStyle::kColorTextMain).arg(stats.cpuPowerW, 0, 'f', 0);
        }
        header_stats_label_->setText(stats_line);
    }
}

void ProcessListPage::updateRamHeader(const SystemStats &stats) {
    if (header_name_label_) header_name_label_->setText("Memory");
    if (header_main_label_) {
        header_main_label_->setText(QString("<span style='color:%1; font-weight:bold;'>%2GB</span> <span style='color:%3;'>/ %4GB</span>")
            .arg(UiStyle::kColorGreen).arg(stats.ramUsedGB, 0, 'f', 1)
            .arg(UiStyle::kColorTextDim).arg(stats.ramTotalGB, 0, 'f', 1));
    }
    if (header_stats_label_) {
        QString speed = stats.ramClockMHz > 0.0 ? QString("%1 MHz").arg(stats.ramClockMHz, 0, 'f', 0) : "N/A";
        header_stats_label_->setText(QString("Speed: <span style='color:%1; font-weight:bold;'>%2</span>")
            .arg(UiStyle::kColorTextMain).arg(speed));
    }
}

void ProcessListPage::updateGpuHeader(const SystemStats &stats) {
    const QString name = stats.gpuName.isEmpty() ? "GPU" : stats.gpuName;
    if (subtitle_label_) subtitle_label_->setText(name);

    QString load = stats.capabilities.gpuLoadAvailable ? QString("%1%").arg(stats.gpuLoad, 0, 'f', 1) : "N/A";
    QString temp = stats.capabilities.gpuTempAvailable ? QString("%1C").arg(stats.gpuTempC, 0, 'f', 0) : "N/A";
    QString vram = stats.capabilities.vramAvailable ? QString("%1 / %2 GB").arg(stats.vramUsedGB, 0, 'f', 1).arg(stats.vramTotalGB, 0, 'f', 1) : "N/A";

    if (detail_label_) detail_label_->setText(QString("Load: %1 | Temp: %2 | VRAM: %3").arg(load, temp, vram));

    if (notice_label_) {
        if (stats.capabilities.gpuBackend == "NVML") {
            notice_label_->setText(""); notice_label_->setVisible(false);
        } else {
            if (stats.capabilities.gpuVendor == "NVIDIA") {
                if (!stats.capabilities.nvmlEnabled) notice_label_->setText("NVML disabled. Remove MONITOR_WIDGET_ENABLE_NVML=0 to enable NVIDIA-only metrics.");
                else if (!stats.capabilities.nvmlAvailable) notice_label_->setText("NVML unavailable. Update NVIDIA driver (nvml.dll) to enable power/temp/clock.");
                else notice_label_->setText("NVIDIA-only metrics unavailable.");
            } else notice_label_->setText("NVIDIA-only metrics show N/A on other GPUs.");
            notice_label_->setVisible(true);
        }
    }
}

void ProcessListPage::updateVramHeader(const SystemStats &stats) {
    const QString name = stats.gpuName.isEmpty() ? "GPU" : stats.gpuName;
    if (header_name_label_) header_name_label_->setText(name);

    if (header_main_label_) {
        if (stats.capabilities.vramAvailable && stats.vramTotalGB > 0.0) {
            header_main_label_->setText(QString("<span style='color:%1; font-weight:bold;'>%2GB</span> <span style='color:%3;'>/ %4GB</span>")
                .arg(UiStyle::kColorGreen).arg(stats.vramUsedGB, 0, 'f', 1)
                .arg(UiStyle::kColorTextDim).arg(stats.vramTotalGB, 0, 'f', 1));
        } else header_main_label_->setText("VRAM: N/A");
    }

    if (header_stats_label_) {
        QString clock = stats.capabilities.gpuClockAvailable ? QString("%1GHz").arg(stats.gpuClockMHz / 1000.0, 0, 'f', 1) : "N/A";
        QString power = stats.capabilities.gpuPowerAvailable ? QString("%1W").arg(stats.gpuPowerW, 0, 'f', 0) : "N/A";
        QString temp = "N/A", temp_color = UiStyle::kColorTextDim;
        if (stats.capabilities.gpuTempAvailable) {
            temp = QString("%1C").arg(stats.gpuTempC, 0, 'f', 0);
            temp_color = stats.gpuTempC > 80.0 ? UiStyle::kColorRed : UiStyle::kColorGreen;
        }
        header_stats_label_->setText(QString("%1 <span style='color:%2;'>|</span> %3 <span style='color:%4;'>|</span> <span style='color:%5; font-weight:bold;'>%6</span>")
            .arg(clock).arg(UiStyle::kColorTextDim).arg(power).arg(UiStyle::kColorTextDim).arg(temp_color).arg(temp));
    }

    if (notice_label_) {
        QStringList notices;
        if (stats.capabilities.gpuBackend != "NVML") {
            if (stats.capabilities.gpuVendor == "NVIDIA") {
                if (!stats.capabilities.nvmlEnabled) notices << "NVML disabled. Remove MONITOR_WIDGET_ENABLE_NVML=0 to enable NVIDIA-only metrics.";
                else if (!stats.capabilities.nvmlAvailable) notices << "NVML unavailable. Update NVIDIA driver (nvml.dll) to enable power/temp/clock.";
                else notices << "NVIDIA-only metrics unavailable.";
            } else notices << "NVIDIA-only metrics show N/A on other GPUs.";
        }
        if (!stats.capabilities.perProcessVramAvailable) notices << "Per-process VRAM usage unavailable.";
        notice_label_->setText(notices.join(" "));
        notice_label_->setVisible(!notices.isEmpty());
    }
}

QString ProcessListPage::elideName(const QString &name) {
    if (name.size() <= 20) return name;
    return name.left(18) + "..";
}

QString ProcessListPage::formatValue(const ProcessInfo &proc, const CapabilityFlags &caps) const {
    switch (mode_) {
        case Mode::Cpu: return QString("%1%").arg(proc.cpuPercent, 0, 'f', 1);
        case Mode::Ram: return QString("%1 MB").arg(proc.ramMB, 0, 'f', 0);
        case Mode::Gpu:
            if (caps.perProcessGpuAvailable && proc.gpuPercent > 0.0) return QString("%1%").arg(proc.gpuPercent, 0, 'f', 0);
            if (caps.perProcessVramAvailable && proc.vramMB > 0.0) return QString("%1 MB").arg(proc.vramMB, 0, 'f', 0);
            if (proc.isGpu) return caps.perProcessVramAvailable ? "0 MB" : "N/A";
            return "";
        case Mode::Vram:
            if (caps.perProcessVramAvailable) {
                if (proc.vramMB > 0.0) return QString("%1 MB").arg(proc.vramMB, 0, 'f', 0);
                if (proc.isGpu) return "0 MB";
            } else if (proc.isGpu) return "N/A";
            return "";
        case Mode::Net: {
             double total = proc.netReadMBs + proc.netWriteMBs;
             if (total <= 0.0) return "0 K/s";
             return NetTooltipHelper::formatNetSpeedShort(total);
        }
    }
    return "";
}

void ProcessListPage::updateProcesses(const QVector<ProcessInfo> &processes) {
    QVector<ProcessInfo> sorted = processes;
    std::sort(sorted.begin(), sorted.end(), [this](const ProcessInfo &a, const ProcessInfo &b) {
        switch (mode_) {
            case Mode::Cpu: return a.cpuPercent > b.cpuPercent;
            case Mode::Ram: return a.ramMB > b.ramMB;
            case Mode::Gpu:
                if (last_caps_.perProcessGpuAvailable) return a.gpuPercent > b.gpuPercent;
                return a.vramMB > b.vramMB;
            case Mode::Vram: return a.vramMB > b.vramMB;
            case Mode::Net:
                double totalA = a.netReadMBs + a.netWriteMBs;
                double totalB = b.netReadMBs + b.netWriteMBs;
                if (std::abs(totalA - totalB) > 0.01) return totalA > totalB;
                return a.checkConnectionCount > b.checkConnectionCount;
        }
        return false;
    });

    clearLayout(list_layout_);
    const int max_rows = (mode_ == Mode::Ram) ? kMaxRowsRam : kMaxRowsDefault;
    int added = 0;
    for (const auto &proc : sorted) {
        if (added >= max_rows) break;
        QString value = formatValue(proc, last_caps_);
        const bool allow_empty = (mode_ == Mode::Gpu || mode_ == Mode::Vram) && proc.isGpu;
        if ((mode_ == Mode::Gpu || mode_ == Mode::Vram) && value.isEmpty() && !allow_empty) continue;
        if (value.isEmpty() && allow_empty) value = last_caps_.perProcessVramAvailable ? "0 MB" : "N/A";

        const bool can_kill = !proc.isWhitelisted && process_service_ != nullptr;
        const bool can_suspend = !proc.isWhitelisted && process_service_ != nullptr && process_service_->supportsSuspend();
        const bool is_suspended = can_suspend && process_service_->isSuspended(static_cast<qulonglong>(proc.pid));

        auto *row = new ProcessRowWidget();
        row->setData(elideName(proc.name), value, static_cast<qulonglong>(proc.pid), can_kill, can_suspend, is_suspended);
        if (mode_ == Mode::Net) {
            row->setCompactLayout(true);
            double total = proc.netReadMBs + proc.netWriteMBs;
            QString color = UiStyle::kColorTextDim;
            if (total >= 10.0) color = UiStyle::kColorRed;
            else if (total >= 1.0) color = UiStyle::kColorOrange;
            else if (total > 0.01) color = UiStyle::kColorGreen;
            row->setValueColor(color);
            row->setToolTip(NetTooltipHelper::buildTooltip(proc));
        }

        if (can_kill) {
            connect(row, &ProcessRowWidget::killRequested, this, [this](qulonglong pid) {
                if (process_service_) process_service_->killProcess(pid, nullptr);
            });
        }
        if (can_suspend) {
            connect(row, &ProcessRowWidget::suspendRequested, this, [this, row](qulonglong pid, bool suspend) {
                if (!process_service_) return;
                if (suspend) process_service_->suspendProcess(pid, nullptr);
                else process_service_->resumeProcess(pid, nullptr);
                row->setSuspended(process_service_->isSuspended(pid));
            });
        }
        list_layout_->addWidget(row);
        added++;
    }
    if (added == 0) {
        auto *empty_label = new QLabel("No active processes", this);
        empty_label->setStyleSheet(UiStyle::kListItemMuted);
        list_layout_->addWidget(empty_label);
    }
}
