#pragma once

#include <QWidget>

#include <QEnterEvent>

#include <QtGlobal>

class QHBoxLayout;
class QLabel;
class QPushButton;

class ProcessRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit ProcessRowWidget(QWidget *parent = nullptr);
    void setData(const QString &name, const QString &value, qulonglong pid, bool can_kill,
        bool can_suspend, bool is_suspended);
    void setSuspended(bool is_suspended);
    void setCompactLayout(bool compact);
    void setValueColor(const QString &color);

signals:
    void killRequested(qulonglong pid);
    void suspendRequested(qulonglong pid, bool suspend);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateSuspendButton();

    QLabel *name_label_ = nullptr;
    QLabel *value_label_ = nullptr;
    QHBoxLayout *layout_ = nullptr;
    QPushButton *suspend_button_ = nullptr;
    QPushButton *kill_button_ = nullptr;
    qulonglong pid_ = 0;
    bool can_suspend_ = false;
    bool is_suspended_ = false;
    bool compact_layout_ = false;
};
