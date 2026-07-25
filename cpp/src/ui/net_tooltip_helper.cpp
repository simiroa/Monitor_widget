#include "ui/net_tooltip_helper.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QStringList>

#include "models/process_info.h"
#include "ui/style_tokens.h"

namespace {
struct GeoCacheEntry {
    QString label;
    QDateTime updated;
    QDateTime last_failed;
    bool pending = false;
};

QHash<QString, GeoCacheEntry> geo_cache;
QSet<QString> geo_pending;
QNetworkAccessManager *geo_manager = nullptr;
const int kGeoMaxPending = 24;

bool isPrivateIp(const QString &ip) {
    if (ip.startsWith("10.") || ip.startsWith("127.") || ip.startsWith("0.") || ip.startsWith("169.254.")) {
        return true;
    }
    if (ip.startsWith("192.168.")) {
        return true;
    }
    if (ip.startsWith("172.")) {
        const QStringList parts = ip.split('.');
        if (parts.size() > 1) {
            bool ok = false;
            const int second = parts[1].toInt(&ok);
            if (ok && second >= 16 && second <= 31) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

QString NetTooltipHelper::formatNetSpeedShort(double mbs) {
    if (mbs <= 0.0) {
        return "0K/s";
    }
    if (mbs >= 1024.0) {
        return QString("%1G/s").arg(mbs / 1024.0, 0, 'f', 1);
    }
    if (mbs >= 1.0) {
        return QString("%1M/s").arg(mbs, 0, 'f', 1);
    }
    return QString("%1K/s").arg(mbs * 1024.0, 0, 'f', 0);
}

QString NetTooltipHelper::geoLabelForIp(const QString &ip) {
    if (isPrivateIp(ip)) {
        return "Local";
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    auto it = geo_cache.find(ip);
    if (it != geo_cache.end()) {
        if (it->updated.isValid() && it->updated.secsTo(now) < (7 * 24 * 60 * 60) && !it->label.isEmpty()) {
            return it->label;
        }
        if (it->last_failed.isValid() && it->last_failed.secsTo(now) < (10 * 60)) {
            return it->label;
        }
        if (it->pending) {
            return it->label;
        }
    }

    if (!geo_manager) {
        if (!QCoreApplication::instance()) {
            return QString();
        }
        geo_manager = new QNetworkAccessManager(QCoreApplication::instance());
    }

    if (!geo_pending.contains(ip)) {
        if (geo_pending.size() >= kGeoMaxPending) {
            return QString();
        }
        geo_pending.insert(ip);
        GeoCacheEntry entry = it != geo_cache.end() ? *it : GeoCacheEntry{};
        entry.pending = true;
        geo_cache[ip] = entry;

        const QUrl url(QString("http://ip-api.com/json/%1?fields=status,country,regionName,city").arg(ip));
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", "MonitorWidget");
        QNetworkReply *reply = geo_manager->get(request);
        QObject::connect(reply, &QNetworkReply::finished, [reply, ip]() {
            reply->deleteLater();
            GeoCacheEntry entry = geo_cache.value(ip);
            entry.pending = false;
            entry.updated = QDateTime::currentDateTimeUtc();

            if (reply->error() == QNetworkReply::NoError) {
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (doc.isObject()) {
                    const QJsonObject obj = doc.object();
                    if (obj.value("status").toString() == "success") {
                        const QString city = obj.value("city").toString();
                        const QString region = obj.value("regionName").toString();
                        const QString country = obj.value("country").toString();
                        QStringList parts;
                        if (!city.isEmpty()) parts << city;
                        if (!region.isEmpty()) parts << region;
                        if (!country.isEmpty()) parts << country;
                        entry.label = parts.join(", ");
                        entry.last_failed = QDateTime();
                    } else {
                        entry.last_failed = QDateTime::currentDateTimeUtc();
                    }
                }
            } else {
                entry.last_failed = QDateTime::currentDateTimeUtc();
            }

            geo_cache[ip] = entry;
            geo_pending.remove(ip);
        });
    }

    return QString();
}

QString NetTooltipHelper::buildTooltip(const ProcessInfo &proc) {
    QStringList tips;
    tips << QString("<b>Download:</b> %1").arg(formatNetSpeedShort(proc.netReadMBs));
    tips << QString("<b>Upload:</b> %1").arg(formatNetSpeedShort(proc.netWriteMBs));
    if (proc.checkConnectionCount > 0) {
        tips << QString("<b>Connections:</b> %1").arg(proc.checkConnectionCount);
    }
    if (!proc.remoteEndpoints.isEmpty()) {
        tips << "<hr><b>Remote Targets:</b>";
        QSet<QString> seen;
        for (const auto &ep : proc.remoteEndpoints) {
            if (seen.contains(ep)) {
                continue;
            }
            seen.insert(ep);
            QString label = geoLabelForIp(ep);
            if (!label.isEmpty()) {
                tips << QString("%1 <span style='color:%2'>(%3)</span>").arg(ep).arg(UiStyle::kColorTextDim).arg(label);
            } else {
                tips << ep;
            }
            if (seen.size() >= 4) {
                break;
            }
        }
        if (proc.checkConnectionCount > seen.size()) {
            tips << QString("... and %1 more").arg(proc.checkConnectionCount - seen.size());
        }
    }
    return tips.join("<br>");
}
