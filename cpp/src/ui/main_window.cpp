#include "ui/main_window.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMouseEvent>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShowEvent>
#include <QSlider>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>

#include "engine/stats_engine.h"
#include "models/process_info.h"
#include "models/system_stats.h"
#include "ui/pages/disk_page.h"
#include "ui/pages/gpu_page.h"
#include "ui/pages/net_page.h"
#include "ui/pages/process_list_page.h"
#include "ui/pages/server_page.h"
#include "ui/pages/dashboard_page.h"
#include "ui/sidebar_item.h"
#include "ui/style_tokens.h"
#include "ui/icons_material.h" // Added this include
#include "utils/logger.h"
namespace {
constexpr int kSidebarWidth = 120;
constexpr int kExpandedWidth = 400;
constexpr int kWindowHeight = 320; // Increased from 285 to prevent sidebar overlap
constexpr int kCompactHeight = 75; 
}

#include "services/status_writer.h"
#include "services/weather_service.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), stack_(new QStackedWidget(this)) {
    setWindowTitle("Monitor Widget");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground); // Enable transparency for rounded corners
    setFixedSize(kSidebarWidth, kWindowHeight);

    buildUi();
    restoreWindowState();
    // 닫기 버튼과 --hub quit이 모두 qApp->quit()으로 직행하므로 closeEvent는 안 온다.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::saveWindowState);

    // Init Weather Service
    weather_service_ = new WeatherService(this);
    if (dashboard_page_) {
        connect(weather_service_, &WeatherService::weatherUpdated, dashboard_page_, &DashboardPage::updateWeather);
        connect(dashboard_page_, &DashboardPage::locationChanged, [this](const LocationConfig &loc) {
            if (weather_service_) {
                if (loc.name == "AUTO") {
                    weather_service_->detectLocation();
                } else {
                    weather_service_->saveLocation(loc);
                    weather_service_->fetchWeather(loc);
                }
            }
        });
        connect(dashboard_page_, &DashboardPage::saveLocationRequested, [this]() {
            if (weather_service_) {
                // Save current location
                weather_service_->saveLocation(weather_service_->currentLocation());
            }
        });
    }
    
    // Initial Fetch Weather (저장된 위치 사용)
    QTimer::singleShot(2000, this, [this]() {
        if (weather_service_) {
            weather_service_->fetchWeather(weather_service_->currentLocation());
        }
    });

    // Setup recurring 30-min update timer
    auto *weatherTimer = new QTimer(this);
    connect(weatherTimer, &QTimer::timeout, [this]() {
        if (weather_service_) {
            weather_service_->fetchWeather(weather_service_->currentLocation());
        }
    });
    weatherTimer->start(1800000); // 30 minutes
    
    engine_thread_ = new QThread(this);
    engine_ = new StatsEngine();
    engine_->moveToThread(engine_thread_);

    connect(engine_thread_, &QThread::started, engine_, &StatsEngine::start);
    connect(engine_thread_, &QThread::finished, engine_, &QObject::deleteLater);

    connect(engine_, &StatsEngine::statsUpdated, this, &MainWindow::handleStatsUpdated);
    connect(engine_, &StatsEngine::processesUpdated, this, &MainWindow::handleProcessesUpdated);
    connect(engine_, &StatsEngine::netProcessesUpdated, this, &MainWindow::handleNetProcessesUpdated);
    
    // ContextHub용 status.json 발행. 엔진이 다른 스레드에 있으므로 queued로 전달된다.
    status_writer_ = new StatusWriter(this);
    connect(engine_, &StatsEngine::statsUpdated, status_writer_, &StatusWriter::updateStats);
    status_writer_->start();

    engine_thread_->start();

    // 1-second timer for Dashboard clock
    clock_timer_ = new QTimer(this);
    connect(clock_timer_, &QTimer::timeout, this, &MainWindow::updateDashboardTime);
    clock_timer_->start(1000);
    updateDashboardTime(); // Initial update
}

MainWindow::~MainWindow() {
    if (engine_) {
        QMetaObject::invokeMethod(engine_, "stop", Qt::BlockingQueuedConnection);
    }
    if (engine_thread_) {
        engine_thread_->quit();
        engine_thread_->wait(2000);
    }
}

void MainWindow::handleHubCommand(const QString &command) {
    if (command == "toggle") {
        if (isVisible()) {
            hide();
        } else {
            showAndRaise();
        }
        return;
    }
    if (command == "show") {
        showAndRaise();
        return;
    }
    if (command == "quit") {
        // 닫기 버튼과 동일 경로. MonitorControlService::restore()가 여기서 돌아야
        // 밝기·주사율이 원복된다.
        qApp->quit();
        return;
    }
    // 빈 문자열/알 수 없는 명령은 무시한다.
    Logger::info("ipc", QString("Ignored command=%1").arg(command));
}

void MainWindow::showAndRaise() {
    show();
    raise();
    activateWindow();
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    ensureWindowBounds();
    positionOpacitySlider();
    QTimer::singleShot(0, this, [this]() {
        raise();
        activateWindow();
    });
    Logger::info("ui.main", QString("MainWindow shown at %1,%2 size=%3x%4.")
        .arg(x()).arg(y()).arg(width()).arg(height()));
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    positionOpacitySlider();
}

void MainWindow::buildUi() {
    central_ = new QWidget(this);
    central_->setStyleSheet("background: transparent;");
    setCentralWidget(central_);

    auto *main_layout = new QHBoxLayout(central_);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    sidebar_ = new QFrame(central_);
    sidebar_->setFixedWidth(kSidebarWidth);
    sidebar_->setStyleSheet(QString("background-color: %1; border-top-left-radius: 8px; border-bottom-left-radius: 8px;")
        .arg(UiStyle::kColorSidebarBg));

    auto *side_layout = new QVBoxLayout(sidebar_);
    side_layout->setContentsMargins(6, 6, 6, 8);
    side_layout->setSpacing(2);

    auto *top_row = new QWidget(sidebar_);
    auto *top_row_layout = new QHBoxLayout(top_row); // Renamed from top_layout to top_row_layout for clarity
    top_row_layout->setContentsMargins(2, 0, 2, 0);
    // Top Row Container Layout
    // Already defined as top_row_layout

    // Top Toggle Button (Text only, simpler design)
    top_button_ = new QPushButton("Top", top_row);
    top_button_->setFixedHeight(24);
    top_button_->setCursor(Qt::PointingHandCursor);
    top_button_->setCheckable(true);
    top_button_->setToolTip("Always on Top");

    auto updateTopState = [this](bool checked) {
        QString bg = checked ? "#27272b" : "transparent";
        QString color = checked ? "#ffffff" : "#8b8b94";
        
        top_button_->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border: none; border-radius: 4px; "
            "font-family: 'Segoe UI'; font-size: 11px; font-weight: bold; padding: 0px 4px; }"
            "QPushButton:hover { color: #ffffff; background: #303036; }"
        ).arg(bg).arg(color));
    };

    connect(top_button_, &QPushButton::toggled, this, &MainWindow::setAlwaysOnTop);
    connect(top_button_, &QPushButton::toggled, this, updateTopState);
    updateTopState(false); // Initial State

    // Add Top Button
    top_row_layout->addWidget(top_button_);
    
    // Add stretch to push Close button to the end
    top_row_layout->addStretch();
    
    // Close Button
    close_button_ = new QPushButton(QString::fromUtf16(Icons::kClose), top_row);
    close_button_->setFixedSize(24, 24); // Reduced size to 24x24
    close_button_->setCursor(Qt::PointingHandCursor);
    close_button_->setToolTip("Close Widget");
    close_button_->setStyleSheet(
        QString("QPushButton { color: %1; background: transparent; border: none; font-family: '%2'; font-size: 16px; }")
        .arg("#8b8b94").arg(Icons::fontName()) +
        "QPushButton:hover { color: #ed4245; }"
    );
    // Directly quits the application (rather than MainWindow::close()) so that the
    // process exits unconditionally, independent of Qt::Tool/quitOnLastWindowClosed
    // semantics. This is the sole exit path now that the tray icon (and its "Exit"
    // action) is gone, and the app runs elevated so an external host cannot kill it.
    connect(close_button_, &QPushButton::clicked, qApp, &QCoreApplication::quit);

    // Collapse Button (-)
    collapse_button_ = new QPushButton("-", top_row);
    collapse_button_->setFixedSize(24, 24);
    collapse_button_->setCursor(Qt::PointingHandCursor);
    collapse_button_->setToolTip("Compact Mode");
    collapse_button_->setStyleSheet(
        QString("QPushButton { color: %1; background: transparent; border: none; font-family: 'Outfit'; font-size: 20px; font-weight: bold; padding-bottom: 2px; }")
        .arg("#8b8b94") +
        "QPushButton:hover { color: #ffffff; background: #303036; border-radius: 4px; }"
    );
    connect(collapse_button_, &QPushButton::clicked, this, &MainWindow::toggleCompactMode);

    top_row_layout->addWidget(collapse_button_);
    top_row_layout->addWidget(close_button_);
    side_layout->addWidget(top_row);

    const QStringList labels = {"", "CPU", "RAM", "GPU", "VRAM", "DISK", "NET", "SRV"}; // Empty label for Dashboard (time only)
    const QList<const char16_t*> icons = {
        nullptr, Icons::kCpu, Icons::kRam, Icons::kGpu, Icons::kVram, Icons::kDisk, Icons::kNet, Icons::kDns
    };

    for (int i = 0; i < labels.size(); ++i) {
        auto *item = new SidebarItem(labels[i], QString(), sidebar_); // No icons
        connect(item, &QAbstractButton::clicked, this, [this, i]() { toggleExpand(i); });
        side_layout->addWidget(item);
        sidebar_items_.push_back(item);
    }

    side_layout->addStretch();
    main_layout->addWidget(sidebar_);

    content_ = new QWidget(central_);
    content_->setVisible(false);
    content_->setStyleSheet(QString("background-color: %1; border-top-right-radius: 8px; border-bottom-right-radius: 8px;")
        .arg(UiStyle::kColorMainBg));

    stack_ = new QStackedWidget(content_);

    dashboard_page_ = new DashboardPage(content_);
    cpu_page_ = new ProcessListPage("CPU Usage", ProcessListPage::Mode::Cpu, &process_controls_, content_);
    ram_page_ = new ProcessListPage("RAM Usage", ProcessListPage::Mode::Ram, &process_controls_, content_);
    gpu_page_ = new GpuPage(&monitor_controls_, content_);
    vram_page_ = new ProcessListPage("VRAM Usage", ProcessListPage::Mode::Vram, &process_controls_, content_);
    disk_page_ = new DiskPage(content_);
    net_page_ = new NetPage(content_);
    server_page_ = new ServerPage(&process_controls_, content_);

    stack_->addWidget(dashboard_page_);
    stack_->addWidget(cpu_page_);
    stack_->addWidget(ram_page_);
    stack_->addWidget(gpu_page_);
    stack_->addWidget(vram_page_);
    stack_->addWidget(disk_page_);
    stack_->addWidget(net_page_);
    stack_->addWidget(server_page_);

    auto *content_layout = new QVBoxLayout(content_);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->addWidget(stack_);


    main_layout->addWidget(content_);

    // close_button_ is now in sidebar top_row (always visible)

    opacity_slider_ = new QSlider(Qt::Horizontal, central_);
    opacity_slider_->setRange(20, 100);
    opacity_slider_->setValue(100);
    opacity_slider_->setFixedHeight(12);
    opacity_slider_->setStyleSheet(
        "QSlider { background: transparent; }"
        "QSlider::groove:horizontal { height: 2px; background: #2d2d34; border-radius: 1px; }"
        "QSlider::handle:horizontal { background: #8b8b94; width: 8px; height: 8px; border-radius: 4px; margin-top: -3px; }"
    );
    connect(opacity_slider_, &QSlider::valueChanged, this, &MainWindow::updateOpacity);
    opacity_slider_->hide();
    updateOpacity(opacity_slider_->value());

    // positionCloseButton(); removed - close button is now in sidebar

    Logger::info("ui.main", "UI built.");

    // Compact Mode Timer
    compact_timer_ = new QTimer(this);
    connect(compact_timer_, &QTimer::timeout, this, &MainWindow::cycleCompactStats);
}

void MainWindow::toggleCompactMode() {
    setCompactMode(!compact_mode_);
}

void MainWindow::setCompactMode(bool enable) {
    if (compact_mode_ == enable) return;
    
    compact_mode_ = enable;

    if (!compact_mode_) {
        // Exit Compact Mode
        compact_timer_->stop();
        
        // Restore size
        setFixedSize(expanded_ ? kExpandedWidth : kSidebarWidth, kWindowHeight);
        
        // Restore items visibility
        for (auto *item : sidebar_items_) {
            item->show();
        }
        
        // Show collapse button
        if(collapse_button_) collapse_button_->show();
        
        // Show content if expanded
        if (expanded_) {
            content_->setVisible(true);
            if(opacity_slider_) opacity_slider_->show();
        }
        
        // Restore collapse button icon
        if (collapse_button_) collapse_button_->setText("-");

        Logger::info("ui.main", "Exited Compact Mode.");
    } else {
        // Enter Compact Mode
        // Hide content (even if expanded state is true logic-wise, visual is collapsed)
        content_->setVisible(false);
        if(opacity_slider_) opacity_slider_->hide();
        
        // Resize to compact
        setFixedSize(kSidebarWidth, kCompactHeight);
        
        // Change collapse button icon to '='
        if (collapse_button_) collapse_button_->setText("=");
        
        // Start Timer
        compact_cycle_index_ = 0; // Start with Clock (index 0)
        cycleCompactStats(); // Initial set
        compact_timer_->start(3000);
        
        Logger::info("ui.main", "Entered Compact Mode.");
    }
}

void MainWindow::cycleCompactStats() {
    if (!compact_mode_) return;
    
    // Items: 0=Dash, 1=CPU, 2=RAM, 3=GPU, 4=VRAM, 5=Disk, 6=NET, 7=SRV
    // Only show current index item
    for (int i = 0; i < sidebar_items_.size(); ++i) {
        if (i == compact_cycle_index_) {
            sidebar_items_[i]->show();
        } else {
            sidebar_items_[i]->hide();
        }
    }
    
    // Prepare next index
    compact_cycle_index_++;
    if (compact_cycle_index_ >= sidebar_items_.size()) {
        compact_cycle_index_ = 0;
    }
}

void MainWindow::toggleExpand(int index) {
    setCompactMode(false); // Reset mini mode before expanding

    if (expanded_ && current_page_ == index) {
        expanded_ = false;
        content_->setVisible(false);
        setFixedSize(kSidebarWidth, kWindowHeight);
        // close_button_ is always visible in sidebar now
        if (opacity_slider_) {
            opacity_slider_->hide();
        }
        for (auto *btn : sidebar_items_) {
            btn->setChecked(false);
        }
        if (engine_) {
            QMetaObject::invokeMethod(engine_, "setNetProcessUpdatesEnabled",
                                      Qt::QueuedConnection, Q_ARG(bool, false));
        }
        Logger::info("ui.main", "Collapsed sidebar.");
        return;
    }

    if (!expanded_) {
        expanded_ = true;
        content_->setVisible(true);
        setFixedSize(kExpandedWidth, kWindowHeight);
        // close_button_ is always visible in sidebar now
        if (opacity_slider_) {
            opacity_slider_->show();
        }
    }

    current_page_ = index;
    stack_->setCurrentIndex(index);
    {
        QWidget *current = stack_->currentWidget();
        if (current == cpu_page_) {
            cpu_page_->updateStats(last_stats_);
        } else if (current == ram_page_) {
            ram_page_->updateStats(last_stats_);
        } else if (current == gpu_page_) {
            gpu_page_->updateStats(last_stats_);
        } else if (current == vram_page_) {
            vram_page_->updateStats(last_stats_);
        } else if (current == disk_page_) {
            disk_page_->updateStats(last_stats_);
        } else if (current == net_page_) {
            net_page_->updateStats(last_stats_);
        } else if (current == server_page_) {
            server_page_->updateStats(last_stats_);
        }
    }
    for (int i = 0; i < sidebar_items_.size(); ++i) {
        sidebar_items_[i]->setChecked(i == index);
    }
    positionOpacitySlider();
    if (engine_) {
        QMetaObject::invokeMethod(engine_, "setNetProcessUpdatesEnabled",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, expanded_ && current_page_ == 6));
    }
    Logger::info("ui.main", QString("Expanded page index=%1.").arg(index));
}

void MainWindow::positionOpacitySlider() {
    if (!opacity_slider_) {
        return;
    }
    const int margin = 6;
    const int slider_width = 60;
    opacity_slider_->setFixedWidth(slider_width);
    opacity_slider_->move(width() - slider_width - margin, margin + 3);
    opacity_slider_->raise();
}

void MainWindow::ensureWindowBounds() {
    int target_width = expanded_ ? kExpandedWidth : kSidebarWidth;
    int target_height = compact_mode_ ? kCompactHeight : kWindowHeight;
    
    bool resized = false;
    if (width() != target_width || height() != target_height) {
        setFixedSize(target_width, target_height);
        resized = true;
    }

    QRect frame = frameGeometry();
    const auto screens = QGuiApplication::screens();
    bool on_any_screen = false;
    for (QScreen *screen : screens) {
        if (frame.intersects(screen->availableGeometry())) {
            on_any_screen = true;
            break;
        }
    }

    // 첫 표시라고 primary만 따지면 안 된다. 보조 모니터에 복원된 창이 매번 primary로
    // 끌려온다. 판정 기준은 항상 "어느 화면에도 안 걸침"이다.
    QScreen *primary = QGuiApplication::primaryScreen();
    if (primary && !on_any_screen) {
        const QRect primary_area = primary->availableGeometry();
        move(primary_area.left() + 20, primary_area.top() + 20);
        Logger::warn("ui.main", "Window was off-screen; moved to primary display.");
    }

    if (resized) {
        Logger::info("ui.main", QString("Adjusted window size to %1x%2.").arg(width()).arg(height()));
    }
}

void MainWindow::setAlwaysOnTop(bool enable) {
    setWindowFlag(Qt::WindowStaysOnTopHint, enable);
    setWindowFlag(Qt::Tool, true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    show();
}

void MainWindow::updateOpacity(int value) {
    const double opacity = static_cast<double>(value) / 100.0;
    setWindowOpacity(opacity);
}

// QSettings 스코프는 기존 관례(LocationManager)와 같은 organization "MonitorWidget"을 쓴다.
//
// 크기는 저장하지 않는다. 창 크기는 expanded_/compact_ 상태에서 setFixedSize로 결정되고
// ensureWindowBounds()가 매 표시마다 다시 강제하므로 저장해봐야 죽은 값이다.
// saveGeometry()/restoreGeometry()도 쓰지 않는다. 프레임리스 창에서 실측하니 복원할 때마다
// y가 26px(타이틀바 두께)씩 밀려 재시작을 반복하면 창이 화면 아래로 기어 내려갔다.
void MainWindow::saveWindowState() {
    QSettings settings("MonitorWidget", "Window");
    settings.setValue("window/pos", pos());
    if (opacity_slider_) {
        settings.setValue("window/opacity", opacity_slider_->value());
    }
}

void MainWindow::restoreWindowState() {
    QSettings settings("MonitorWidget", "Window");
    // 저장값이 없으면 아무것도 건드리지 않는다 — 최초 실행은 기존 기본 동작 그대로.
    // 화면 밖 보정은 showEvent의 ensureWindowBounds()가 맡는다(모니터 분리 대비).
    if (settings.contains("window/pos")) {
        move(settings.value("window/pos").toPoint());
    }
    if (opacity_slider_ && settings.contains("window/opacity")) {
        // setValue → valueChanged → updateOpacity. 범위 밖 값은 슬라이더가 클램프한다.
        opacity_slider_->setValue(settings.value("window/opacity").toInt());
    }
    Logger::info("ui.main", QString("Restored pos=%1,%2 opacity=%3.")
        .arg(x()).arg(y()).arg(opacity_slider_ ? opacity_slider_->value() : -1));
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        QMainWindow::mousePressEvent(event);
        return;
    }

    QWidget *child = childAt(event->position().toPoint());
    if (child) {
        if (qobject_cast<QAbstractButton *>(child) ||
            qobject_cast<QAbstractSlider *>(child) ||
            qobject_cast<QLineEdit *>(child) ||
            qobject_cast<QComboBox *>(child) ||
            qobject_cast<QScrollArea *>(child)) {

            // Special case: In compact mode, if we click, we want to expand.
            // But if we click "Top" or "Close", those should still work.
            // If we click "SidebarItem", it usually toggles expand.
            // In compact mode, clicking ANYWHERE should Expand.
            // But we don't want to break "Close".
            // Implementation in mouseReleaseEvent is better for "Clicking body".
            // Here we just let standard buttons work.
            
            QMainWindow::mousePressEvent(event);
            return;
        }
    }

    dragging_ = true;
    moved_during_drag_ = false;
    drag_offset_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
    event->accept();
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        bool was_dragging = dragging_;
        dragging_ = false;
        
        // If in compact mode and it was a click (not a drag), expand!
        if (compact_mode_ && was_dragging && !moved_during_drag_) {
             // Find current index (it was incremented in cycle, so take previous)
             int target_index = compact_cycle_index_ - 1;
             if (target_index < 0) target_index = sidebar_items_.size() - 1;

             // First exit compact mode
             setCompactMode(false);
             
             // Prepare state to ensure toggleExpand(target_index) actually EXPANDS
             expanded_ = false; 
             current_page_ = -1; 
             
             // Expand to specific page
             toggleExpand(target_index);
        }
    }
    QMainWindow::mouseReleaseEvent(event);
}
void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (!dragging_ || !(event->buttons() & Qt::LeftButton)) {
        QMainWindow::mouseMoveEvent(event);
        return;
    }

    QPoint new_pos = event->globalPosition().toPoint() - drag_offset_;
    
    if (!moved_during_drag_) {
        moved_during_drag_ = true;
    }
    
    const int snap_margin = 25;
    QScreen *screen = QGuiApplication::screenAt(event->globalPosition().toPoint());
    if (screen) {
        QRect geo = screen->availableGeometry();
        if (std::abs(new_pos.x() - geo.left()) < snap_margin) {
            new_pos.setX(geo.left());
        }
        if (std::abs(new_pos.y() - geo.top()) < snap_margin) {
            new_pos.setY(geo.top());
        }
        if (std::abs((new_pos.x() + width()) - geo.right()) < snap_margin) {
            new_pos.setX(geo.right() - width());
        }
        if (std::abs((new_pos.y() + height()) - geo.bottom()) < snap_margin) {
            new_pos.setY(geo.bottom() - height());
        }
    }

    move(new_pos);
    event->accept();
}

void MainWindow::updateDashboardTime() {
    if (!sidebar_items_.isEmpty()) {
        QDateTime now = QDateTime::currentDateTime();
        QString date_str = now.toString("yyyy.MM.dd (ddd)");
        QString time_str = now.toString("HH:mm:ss");
        sidebar_items_[0]->updateClockData(date_str, time_str);
    }
}

void MainWindow::updateSidebar(const SystemStats &stats) {
    // Dashboard time is updated by clock_timer_ (1 second)
    if (sidebar_items_.size() >= 2) {
        sidebar_items_[1]->updateData(stats.cpuPercent, QString("%1%").arg(stats.cpuPercent, 0, 'f', 0));
    }
    if (sidebar_items_.size() >= 3) {
        sidebar_items_[2]->updateData(stats.ramPercent, QString("%1%").arg(stats.ramPercent, 0, 'f', 0));
    }
    if (sidebar_items_.size() >= 4) {
        if (stats.capabilities.gpuLoadAvailable) {
            sidebar_items_[3]->updateData(stats.gpuLoad, QString("%1%").arg(stats.gpuLoad, 0, 'f', 0));
        } else {
            sidebar_items_[3]->updateData(0.0, "N/A", QColor(UiStyle::kColorTextDim));
        }
    }
    if (sidebar_items_.size() >= 5) {
        if (stats.capabilities.vramAvailable) {
            sidebar_items_[4]->updateData(stats.vramPercent, QString("%1%").arg(stats.vramPercent, 0, 'f', 0));
        } else {
            sidebar_items_[4]->updateData(0.0, "N/A", QColor(UiStyle::kColorTextDim));
        }
    }
    if (sidebar_items_.size() >= 6) {
        const double disk_total = stats.diskReadMBs + stats.diskWriteMBs;
        const double disk_pct = std::min(disk_total, 100.0);
        sidebar_items_[5]->updateData(disk_pct, QString("%1 MB/s").arg(disk_total, 0, 'f', 0));
    }
    if (sidebar_items_.size() >= 7) {
        const double net_total = stats.netRecvMBs + stats.netSentMBs;
        const double net_pct = std::min(net_total * 5.0, 100.0);
        sidebar_items_[6]->updateData(net_pct, QString("%1 MB/s").arg(net_total, 0, 'f', 1));
    }
    if (sidebar_items_.size() >= 8) {
        const int count = stats.serverPoints.size();
        const double pct = std::min(static_cast<double>(count) * 10.0, 100.0);
        sidebar_items_[7]->updateData(pct, QString("%1 Port").arg(count), QColor(UiStyle::kColorGreen));
    }
}

void MainWindow::handleStatsUpdated(const SystemStats &stats) {
    last_stats_ = stats;
    updateSidebar(stats);

    if (!content_ || !content_->isVisible()) {
        return;
    }

    QWidget *current = stack_ ? stack_->currentWidget() : nullptr;
    if (current == cpu_page_) {
        cpu_page_->updateStats(stats);
    } else if (current == ram_page_) {
        ram_page_->updateStats(stats);
    } else if (current == gpu_page_) {
        gpu_page_->updateStats(stats);
    } else if (current == vram_page_) {
        vram_page_->updateStats(stats);
    } else if (current == disk_page_) {
        disk_page_->updateStats(stats);
    } else if (current == net_page_) {
        net_page_->updateStats(stats);
    } else if (current == server_page_) {
        server_page_->updateStats(stats);
    }
}

void MainWindow::handleProcessesUpdated(const QVector<ProcessInfo> &processes) {
    if (cpu_page_) {
        cpu_page_->updateProcesses(processes);
    }
    if (ram_page_) {
        ram_page_->updateProcesses(processes);
    }
    if (vram_page_) {
        vram_page_->updateProcesses(processes);
    }
}

void MainWindow::handleNetProcessesUpdated(const QVector<ProcessInfo> &processes) {
    if (net_page_) {
        net_page_->updateProcesses(processes);
    }
}
