#pragma once

#include <QMetaType>
#include <QString>

struct SpeedTestResult {
    double pingMs = 0.0;
    double jitterMs = 0.0;
    double downloadMbps = 0.0;
    double uploadMbps = 0.0;
    QString error;
};

Q_DECLARE_METATYPE(SpeedTestResult)
