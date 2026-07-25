#pragma once

#include <QWidget>

#include "models/process_info.h"
#include "models/system_stats.h"

class QLabel;
class QVBoxLayout;
class ProcessControlService;

class ProcessListPage : public QWidget {
    Q_OBJECT

public:
    enum class Mode {
        Cpu,
        Ram,
        Gpu,
        Vram,
        Net
    };

    explicit ProcessListPage(const QString &title, Mode mode, ProcessControlService *service, QWidget *parent = nullptr);

    void updateStats(const SystemStats &stats);
    void updateProcesses(const QVector<ProcessInfo> &processes);

private:
    static QString elideName(const QString &name);
    QString formatValue(const ProcessInfo &proc, const CapabilityFlags &caps) const;

    void updateCpuHeader(const SystemStats &stats);
    void updateRamHeader(const SystemStats &stats);
    void updateGpuHeader(const SystemStats &stats);
    void updateVramHeader(const SystemStats &stats);

    Mode mode_;
    QLabel *title_label_ = nullptr;
    QLabel *subtitle_label_ = nullptr;
    QLabel *detail_label_ = nullptr;
    QLabel *header_name_label_ = nullptr;
    QLabel *header_main_label_ = nullptr;
    QLabel *header_stats_label_ = nullptr;
    QLabel *notice_label_ = nullptr;
    QVBoxLayout *list_layout_ = nullptr;

    CapabilityFlags last_caps_{};
    ProcessControlService *process_service_ = nullptr;
};
