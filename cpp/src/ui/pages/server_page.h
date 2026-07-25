#pragma once

#include <QWidget>

#include "models/system_stats.h"

class QLabel;
class QLineEdit;
class QVBoxLayout;
class QScrollArea;
class ProcessControlService;

class ServerPage : public QWidget {
    Q_OBJECT

public:
    explicit ServerPage(ProcessControlService *process_service, QWidget *parent = nullptr);

    void updateStats(const SystemStats &stats);

private:
    void rebuildList(const QString &filter);
    bool samePoints(const QVector<ServerPoint> &next) const;

    ProcessControlService *process_service_ = nullptr;
    QLabel *count_label_ = nullptr;
    QLineEdit *search_box_ = nullptr;
    QScrollArea *scroll_area_ = nullptr;
    QVBoxLayout *list_layout_ = nullptr;
    QVector<ServerPoint> all_points_;
};
