#pragma once

#include <QLayout>

inline void clearLayout(QLayout *layout) {
    if (!layout) {
        return;
    }

    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        if (QLayout *child_layout = item->layout()) {
            clearLayout(child_layout);
        }
        delete item;
    }
}
