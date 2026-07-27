#pragma once

#include <QMainWindow>
#include <QPoint>
#include <QVector>

#include "models/system_stats.h"
#include "services/monitor_control_service.h"
#include "services/process_control_service.h"

class QThread;
class QTimer;
class QPushButton;
class QShowEvent;
class QResizeEvent;
class QMouseEvent;
class QStackedWidget;
class QFrame;
class QWidget;
class QPushButton;
class SidebarItem;
class QCheckBox;
class QSlider;

struct ProcessInfo;
class StatsEngine;
class ProcessListPage;
class DiskPage;
class NetPage;
class ServerPage;
class GpuPage;
class DashboardPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    // ContextHub IPC 명령 처리(toggle/show/quit, 그 외는 무시).
    // quit은 닫기 버튼과 동일하게 qApp->quit()을 타야
    // MonitorControlService::restore()가 실행되어 밝기·주사율이 원복된다.
    void handleHubCommand(const QString &command);

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void toggleExpand(int index);
    void handleStatsUpdated(const SystemStats &stats);
    void handleProcessesUpdated(const QVector<ProcessInfo> &processes);
    void handleNetProcessesUpdated(const QVector<ProcessInfo> &processes);

private:
    void buildUi();
    void showAndRaise();
    void updateSidebar(const SystemStats &stats);
    void positionOpacitySlider();
    void ensureWindowBounds();
    void setAlwaysOnTop(bool enable);
    void updateOpacity(int value);
    void updateDashboardTime();
    void setCompactMode(bool enable);
    void toggleCompactMode();
    void cycleCompactStats();

    bool expanded_ = false;
    bool compact_mode_ = false;
    int compact_cycle_index_ = 1;
    int current_page_ = 0;

    QWidget *central_ = nullptr;
    QFrame *sidebar_ = nullptr;
    QWidget *content_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    QPushButton *close_button_ = nullptr;
    QPushButton *collapse_button_ = nullptr;
    QPushButton *top_button_ = nullptr; // Changed from QCheckBox
    QSlider *opacity_slider_ = nullptr;
    QVector<SidebarItem *> sidebar_items_;

    DashboardPage *dashboard_page_ = nullptr;
    ProcessListPage *cpu_page_ = nullptr;
    ProcessListPage *ram_page_ = nullptr;
    ProcessListPage *vram_page_ = nullptr;
    DiskPage *disk_page_ = nullptr;
    NetPage *net_page_ = nullptr;
    ServerPage *server_page_ = nullptr;
    GpuPage *gpu_page_ = nullptr;

    MonitorControlService monitor_controls_;
    ProcessControlService process_controls_;

    StatsEngine *engine_ = nullptr;
    QThread *engine_thread_ = nullptr;

    bool dragging_ = false;
    bool moved_during_drag_ = false;
    QPoint drag_offset_;
    bool first_show_ = true;
    QTimer *clock_timer_ = nullptr;
    QTimer *compact_timer_ = nullptr;
    class StatusWriter *status_writer_ = nullptr;
    class WeatherService *weather_service_ = nullptr;
    SystemStats last_stats_;
};
