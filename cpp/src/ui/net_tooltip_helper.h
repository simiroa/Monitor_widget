#pragma once

#include <QVector>
#include <QString>

struct ProcessInfo;

class NetTooltipHelper {
public:
    static QString buildTooltip(const ProcessInfo &proc);

    static QString formatNetSpeedShort(double mbs);

private:
    static QString geoLabelForIp(const QString &ip);
};
