#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QPushButton;

class LocationSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit LocationSelectionDialog(QWidget *parent = nullptr, const QStringList &items = QStringList(), int currentIdx = 0);
    
    QString getSelectedItem() const;

private:
    void setupUi();

    QComboBox *combo_ = nullptr;
    QPushButton *btn_ok_ = nullptr;
    QPushButton *btn_cancel_ = nullptr;
    
    // Data
    QStringList items_;
    int initial_idx_ = 0;
};
