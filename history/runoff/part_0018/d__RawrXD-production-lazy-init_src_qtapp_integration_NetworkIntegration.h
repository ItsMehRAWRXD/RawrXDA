#pragma once
#include "ProdIntegration.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QPointer>
#include <QTimer>
#include <functional>

namespace RawrXD {
namespace Integration {
namespace Network {

// Network request tracker with timeout and retry
class NetworkTracker {
public:
    explicit NetworkTracker(QObject* parent = nullptr)
        : m_manager(new QNetworkAccessManager(parent)) {
        if (Config::stubLoggingEnabled()) {
            logDebug(QStringLiteral("NetworkTracker"), QStringLiteral("created"), QStringLiteral("Network tracker initialized"));
        }
    }

    ~NetworkTracker() {
        if (Config::stubLoggingEnabled()) {
            logDebug(QStringLiteral("NetworkTracker"), QStringLiteral("destroyed"), QStringLiteral("Network tracker cleaned up"));
        }
    }

    struct Request {
        QString url;
        QString method;
        QJsonObject headers;
        QByteArray data;
        qint64 timeoutMs = 10000; // 10 second default
        int maxRetries = 1;
    };

    struct Response {
        bool success = false;
        int statusCode = -1;
        QByteArray data;
        QString errorString;
        qint64 durationMs = 0;
    };

    // Execute network request with timeout and retry
    Response execute(const Request& req) {
        ScopedTimer timer(QStringLiteral("NetworkTracker"), QStringLiteral("execute"), req.url);

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(static_cast<int>(req.timeoutMs));

        QEventLoop loop;
        QPointer<QNetworkReply> reply;

        auto executeOnce = [&]() -> Response {
            Response r;
            QElapsedTimer totalTimer;
            totalTimer.start();

            QNetworkRequest request;
            request.setUrl(QUrl(req.url));

            // Set headers
            for (auto it = req.headers.begin(); it != req.headers.end(); ++it) {
                request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
            }

            QNetworkReply* rawReply = nullptr;
            if (req.method == QStringLiteral("GET")) {
                rawReply = m_manager->get(request);
            } else if (req.method == QStringLiteral("POST")) {
                rawReply = m_manager->post(request, req.data);
            } else if (req.method == QStringLiteral("PUT")) {
                rawReply = m_manager->put(request, req.data);
            } else if (req.method == QStringLiteral("DELETE")) {
                rawReply = m_manager->deleteResource(request);
            } else {
                rawReply = m_manager->sendCustomRequest(request, req.method.toUtf8(), req.data);
            }

            reply = rawReply;

            // Connect signals
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QObject::connect(reply, QOverload<const QList<QSslError>&>::of(&QNetworkReply::sslErrors), 
                           &loop, &QEventLoop::quit);
            QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

            timeoutTimer.start();
            loop.exec();

            if (timeoutTimer.isActive()) {
                r.success = (reply->error() == QNetworkReply::NoError);
                r.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                r.data = reply->readAll();
                r.errorString = reply->errorString();
            } else {
                r.errorString = QStringLiteral("Request timeout");
                reply->abort();
            }

            r.durationMs = totalTimer.elapsed();
            reply->deleteLater();

            return r;
        };

        Response result;
        try {
            if (req.maxRetries > 1) {
                result = retryWithBackoff(executeOnce, req.maxRetries, 500);
            } else {
                result = executeOnce();
            }

            if (result.success) {
                if (Config::metricsEnabled()) {
                    recordMetric("network_request_success", 1);
                }
                if (Config::loggingEnabled()) {
                    logDebug(QStringLiteral("NetworkTracker"), QStringLiteral("success"),
                             QStringLiteral("%1 %2 completed in %3ms").arg(req.method).arg(req.url).arg(result.durationMs));
                }
            } else {
                if (Config::metricsEnabled()) {
                    recordMetric("network_request_failure", 1);
                }
                if (Config::loggingEnabled()) {
                    logWarn(QStringLiteral("NetworkTracker"), QStringLiteral("failure"),
                             QStringLiteral("%1 %2 failed: %3").arg(req.method).arg(req.url).arg(result.errorString));
                }
            }
        } catch (const std::exception& ex) {
            result.errorString = QString::fromUtf8(ex.what());
            logError(QStringLiteral("NetworkTracker"), QStringLiteral("exception"), result.errorString);
        }

        return result;
    }

private:
    QPointer<QNetworkAccessManager> m_manager;
};

// Convenience methods for common HTTP operations
class HttpClient {
public:
    explicit HttpClient(QObject* parent = nullptr) : m_tracker(parent) {}

    NetworkTracker::Response get(const QString& url, const QJsonObject& headers = QJsonObject()) {
        NetworkTracker::Request req;
        req.url = url;
        req.method = QStringLiteral("GET");
        req.headers = headers;
        return m_tracker.execute(req);
    }

    NetworkTracker::Response post(const QString& url, const QByteArray& data,
                                  const QJsonObject& headers = QJsonObject()) {
        NetworkTracker::Request req;
        req.url = url;
        req.method = QStringLiteral("POST");
        req.data = data;
        req.headers = headers;
        return m_tracker.execute(req);
    }

    NetworkTracker::Response put(const QString& url, const QByteArray& data,
                                 const QJsonObject& headers = QJsonObject()) {
        NetworkTracker::Request req;
        req.url = url;
        req.method = QStringLiteral("PUT");
        req.data = data;
        req.headers = headers;
        return m_tracker.execute(req);
    }

    NetworkTracker::Response del(const QString& url, const QJsonObject& headers = QJsonObject()) {
        NetworkTracker::Request req;
        req.url = url;
        req.method = QStringLiteral("DELETE");
        req.headers = headers;
        return m_tracker.execute(req);
    }

private:
    NetworkTracker m_tracker;
};

} // namespace Network
} // namespace Integration
} // namespace RawrXD
