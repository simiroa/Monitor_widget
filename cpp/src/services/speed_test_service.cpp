#include "services/speed_test_service.h"

#include <algorithm>
#include <cmath>
#include <atomic>

#include <QElapsedTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QVector>

#include "utils/logger.h"

namespace {
constexpr int kPingCount = 10;
constexpr int kDownloadDurationMs = 8000;
constexpr int kUploadDurationMs = 8000;
constexpr int kTickIntervalMs = 200;
constexpr int kUploadPayloadBytes = 256 * 1024; 

const QList<QUrl> kDownloadUrls = {
    QUrl("https://speed.cloudflare.com/__down?bytes=50000000"), 
    QUrl("https://speed.hetzner.de/100MB.bin"),
    QUrl("http://speedtest-nyc1.digitalocean.com/100mb.test")
};

const QUrl kUploadUrl("https://speed.cloudflare.com/__up");

}

// --- Private Worker Class ---

class SpeedTestService::Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker() : stop_(false) {}
    
    void requestStop() {
        stop_ = true;
    }

    void run() {
        SpeedTestResult result;
        Logger::info("service.speed_test", "Speed test started.");

        QUrl pingUrl = kDownloadUrls.first();
        if (stop_) { emit finished(result); return; }
        
        if (!runPingPhase(pingUrl, result)) {
             Logger::warn("service.speed_test", "Ping phase failed or returned errors.");
        }
        
        if (stop_) {
            result.error = "Cancelled.";
            emit finished(result);
            return;
        }

        if (!runDownloadPhase(result)) {
            if (result.error.isEmpty()) result.error = "Download failed completely.";
            Logger::warn("service.speed_test", "Download phase failed.");
            emit finished(result);
            return;
        }

        if (stop_) {
            result.error = "Cancelled.";
            emit finished(result);
            return;
        }

        if (!runUploadPhase(result)) {
             Logger::warn("service.speed_test", "Upload phase failed.");
        }

        Logger::info("service.speed_test", "Speed test complete.");
        emit finished(result);
    }

signals:
    void progress(const QString &phase, int percent, double currentMbps, double maxMbps);
    void finished(const SpeedTestResult &result);

private:
    std::atomic<bool> stop_;

    bool runPingPhase(const QUrl &url, SpeedTestResult &result) {
        QNetworkAccessManager manager;
        QVector<double> latencies;
        
        for (int i = 0; i < kPingCount; ++i) {
            if (stop_) return false;

            QNetworkRequest request(url);
            request.setTransferTimeout(2000);
            
            QElapsedTimer timer;
            timer.start();
            
            QNetworkReply *reply = manager.head(request);
            QEventLoop loop;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();
            
            double ms = static_cast<double>(timer.elapsed());
            
            if (reply->error() == QNetworkReply::NoError) {
                latencies.push_back(ms);
                emit progress("ping", (i + 1) * 100 / kPingCount, ms, 0); 
            }
            reply->deleteLater();
            QThread::msleep(100);
        }

        if (latencies.isEmpty()) {
            return false;
        }

        double sum = 0;
        for (double v : latencies) sum += v;
        double avg = sum / latencies.size();

        double jitterSum = 0;
        for (double v : latencies) jitterSum += std::abs(v - avg);
        
        result.pingMs = avg;
        result.jitterMs = jitterSum / latencies.size();
        return true;
    }

    bool runDownloadPhase(SpeedTestResult &result) {
        for (const QUrl &url : kDownloadUrls) {
            if (stop_) return false;
            
            result.error.clear();
            double maxMbps = 0;
            if (runSingleDownload(url, maxMbps, result.error)) {
                result.downloadMbps = maxMbps;
                return true; 
            }
            if (stop_) return false;
        }
        return false;
    }

    bool runSingleDownload(const QUrl &url, double &outMaxMbps, QString &outError) {
        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
        request.setTransferTimeout(kDownloadDurationMs + 2000);

        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        
        QElapsedTimer totalTimer;
        totalTimer.start();
        
        QElapsedTimer intervalTimer;
        intervalTimer.start();
        qint64 intervalBytes = 0;
        
        QTimer tick;
        tick.setInterval(kTickIntervalMs);
        
        connect(&tick, &QTimer::timeout, [&]() {
            if (stop_ || totalTimer.elapsed() > kDownloadDurationMs) {
                reply->abort();
                return;
            }
            
            double sec = intervalTimer.restart() / 1000.0;
            if (sec > 0) {
                    double currentMbps = (intervalBytes * 8.0) / (1024.0 * 1024.0) / sec;
                    if (currentMbps > outMaxMbps) outMaxMbps = currentMbps;
                    
                    int pct = std::min(99, (int)(totalTimer.elapsed() * 100 / kDownloadDurationMs));
                    emit progress("download", pct, currentMbps, outMaxMbps);
                    intervalBytes = 0;
            }
        });

        connect(reply, &QNetworkReply::readyRead, [&]() {
            QByteArray data = reply->readAll();
            intervalBytes += data.size();
        });
        
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        tick.start();
        loop.exec();
        tick.stop();

        reply->deleteLater();
        
        if (stop_) return false;

        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
                outError = reply->errorString();
                return false;
        }

        emit progress("download", 100, 0, outMaxMbps);
        return true;
    }

    bool runUploadPhase(SpeedTestResult &result) {
        QNetworkAccessManager manager;
        QNetworkRequest request(kUploadUrl);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
        request.setTransferTimeout(kUploadDurationMs + 2000);
        
        QByteArray payload(kUploadPayloadBytes, 'x'); // 256KB

        QElapsedTimer totalTimer;
        totalTimer.start();
        
        double maxMbps = 0;
        
        while (totalTimer.elapsed() < kUploadDurationMs && !stop_) {
            QElapsedTimer reqTimer;
            reqTimer.start();
            
            QNetworkReply *reply = manager.post(request, payload);
            QEventLoop loop;
            connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            
            QTimer::singleShot(2000, reply, &QNetworkReply::abort);
            
            loop.exec();
            
            double ms = reqTimer.elapsed();
            if (stop_) { reply->deleteLater(); return false; }
            
            if (reply->error() == QNetworkReply::NoError) {
                double sec = ms / 1000.0;
                double currentMbps = (payload.size() * 8.0) / (1024.0 * 1024.0) / sec;
                if (currentMbps > maxMbps) maxMbps = currentMbps;
                
                int pct = std::min(99, (int)(totalTimer.elapsed() * 100 / kUploadDurationMs));
                emit progress("upload", pct, currentMbps, maxMbps);
            }
            reply->deleteLater();
        }
        
        emit progress("upload", 100, 0, maxMbps);
        result.uploadMbps = maxMbps;
        return true;
    }
};

// --- SpeedTestService Implementation ---

SpeedTestService::SpeedTestService(QObject *parent) : QObject(parent) {}

SpeedTestService::~SpeedTestService() {
    cancel();
}

bool SpeedTestService::isRunning() const {
    return worker_ != nullptr;
}

void SpeedTestService::start() {
    if (worker_) return;

    worker_ = new Worker();
    thread_ = new QThread();
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::started, worker_, &Worker::run);
    connect(worker_, &Worker::progress, this, &SpeedTestService::progress);
    connect(worker_, &Worker::finished, this, &SpeedTestService::finished);
    
    connect(worker_, &Worker::finished, thread_, &QThread::quit);
    connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(thread_, &QThread::finished, this, [this]() {
        worker_ = nullptr;
        thread_ = nullptr;
    });

    thread_->start();
}

void SpeedTestService::cancel() {
    if (worker_) {
        worker_->requestStop();
    }
    if (thread_) {
        thread_->quit();
        thread_->wait();
    }
}

#include "speed_test_service.moc"
