#include "telemetry_collector.hpp"
#include <QSettings>
#include <QUuid>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QRegularExpression>

TelemetryCollector* TelemetryCollector::s_instance = nullptr;

TelemetryCollector* TelemetryCollector::instance() {
    if (!s_instance) {
        s_instance = new TelemetryCollector();
    }
    return s_instance;
}

TelemetryCollector::TelemetryCollector(QObject* parent)
    : QObject(parent)
    , m_enabled(false)
    , m_sessionStartTime(QDateTime::currentMSecsSinceEpoch())
{
    // PRODUCTION-READY: Generate anonymous session ID (not stored, regenerated each session)
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

TelemetryCollector::~TelemetryCollector() {
    // Flush remaining data on shutdown (if enabled)
    if (m_enabled && !m_events.isEmpty()) {
        flushData();
    }
}

bool TelemetryCollector::initialize() {
    // PRODUCTION-READY: Check user consent first
    m_enabled = loadUserConsent();
    
    // PRODUCTION-READY: Also check environment variable override
    QString envEnabled = qEnvironmentVariable("TELEMETRY_ENABLED");
    if (!envEnabled.isEmpty()) {
        m_enabled = (envEnabled == "1" || envEnabled.toLower() == "true");
    }
    
    if (m_enabled) {
        qInfo().noquote() << QString("[Telemetry] INITIALIZED | SessionID: %1*** | Opt-in: YES")
            .arg(m_sessionId.left(8));
    } else {
        qInfo().noquote() << "[Telemetry] DISABLED | User has not opted in (privacy-first)";
    }
    
    return m_enabled;
}

void TelemetryCollector::enableTelemetry() {
    m_enabled = true;
    saveUserConsent(true);
    
    qInfo().noquote() << "[Telemetry] ENABLED | User opted in to anonymous usage tracking";
    
    // Track telemetry activation event
    trackFeatureUsage("telemetry.enabled");
    
    emit telemetryEnabled();
}

void TelemetryCollector::disableTelemetry() {
    m_enabled = false;
    saveUserConsent(false);
    
    // Clear all collected data when user opts out
    clearAllData();
    
    qInfo().noquote() << "[Telemetry] DISABLED | User opted out, all data cleared";
    
    emit telemetryDisabled();
}

void TelemetryCollector::trackFeatureUsage(const QString& featureName, const QJsonObject& metadata) {
    if (!m_enabled) {
        return;
    }
    
    // PRODUCTION-READY: Sanitize feature name to prevent PII leakage
    QString sanitizedFeature = sanitize(featureName);
    
    // Update aggregated feature usage count
    m_featureUsage[sanitizedFeature]++;
    
    // Create event
    QJsonObject event;
    event["type"] = "feature_usage";
    event["feature"] = sanitizedFeature;
    event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    event["session_id"] = m_sessionId;
    
    // PRODUCTION-READY: Only include metadata that doesn't contain PII
    if (!metadata.isEmpty()) {
        QJsonObject sanitizedMetadata;
        for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            // Skip any keys that might contain PII
            QString key = it.key().toLower();
            if (!key.contains("user") && !key.contains("email") && !key.contains("name") && 
                !key.contains("ip") && !key.contains("address")) {
                sanitizedMetadata[it.key()] = it.value();
            }
        }
        event["metadata"] = sanitizedMetadata;
    }
    
    m_events.append(event);
    
    qDebug().noquote() << QString("[Telemetry] FEATURE_TRACKED | Feature: %1 | Count: %2")
        .arg(sanitizedFeature)
        .arg(m_featureUsage[sanitizedFeature]);
    
    // PRODUCTION-READY: Auto-flush after 50 events to prevent memory bloat
    if (m_events.size() >= 50) {
        flushData();
    }
}

void TelemetryCollector::trackCrash(const QString& crashReason) {
    if (!m_enabled) {
        return;
    }
    
    // PRODUCTION-READY: Sanitize crash reason (may contain file paths with usernames)
    QString sanitizedReason = sanitize(crashReason);
    
    QJsonObject event;
    event["type"] = "crash";
    event["reason"] = sanitizedReason;
    event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    event["session_id"] = m_sessionId;
    event["app_version"] = QCoreApplication::applicationVersion();
    
    m_events.append(event);
    
    qWarning().noquote() << QString("[Telemetry] CRASH_TRACKED | Reason: %1").arg(sanitizedReason);
    
    // PRODUCTION-READY: Immediate flush for crash events
    flushData();
}

void TelemetryCollector::trackPerformance(const QString& metricName, double value, const QString& unit) {
    if (!m_enabled) {
        return;
    }
    
    QString sanitizedMetric = sanitize(metricName);
    
    QJsonObject event;
    event["type"] = "performance";
    event["metric"] = sanitizedMetric;
    event["value"] = value;
    event["unit"] = unit.isEmpty() ? "ms" : unit;
    event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    event["session_id"] = m_sessionId;
    
    m_events.append(event);
    
    qDebug().noquote() << QString("[Telemetry] PERFORMANCE_TRACKED | Metric: %1 | Value: %2%3")
        .arg(sanitizedMetric)
        .arg(value)
        .arg(unit.isEmpty() ? "ms" : unit);
}

QJsonObject TelemetryCollector::getAllTelemetryData() const {
    // PRODUCTION-READY: Transparency - user can view all collected data
    QJsonObject data;
    data["session_id"] = m_sessionId;
    data["session_duration_ms"] = QDateTime::currentMSecsSinceEpoch() - m_sessionStartTime;
    data["app_version"] = QCoreApplication::applicationVersion();
    data["enabled"] = m_enabled;
    
    // Feature usage aggregates
    QJsonObject usageObj;
    for (auto it = m_featureUsage.constBegin(); it != m_featureUsage.constEnd(); ++it) {
        usageObj[it.key()] = it.value();
    }
    data["feature_usage"] = usageObj;
    
    // Buffered events
    QJsonArray eventsArray;
    for (const auto& event : m_events) {
        eventsArray.append(event);
    }
    data["buffered_events"] = eventsArray;
    data["event_count"] = m_events.size();
    
    return data;
}

void TelemetryCollector::clearAllData() {
    m_events.clear();
    m_featureUsage.clear();
    
    qInfo().noquote() << "[Telemetry] DATA_CLEARED | All telemetry data deleted";
}

void TelemetryCollector::flushData() {
    if (!m_enabled || m_events.isEmpty()) {
        return;
    }
    
    QElapsedTimer timer;
    timer.start();
    
    // PRODUCTION-READY: Batch send for efficiency
    QJsonObject payload;
    payload["session_id"] = m_sessionId;
    payload["app_version"] = QCoreApplication::applicationVersion();
    payload["session_duration_ms"] = QDateTime::currentMSecsSinceEpoch() - m_sessionStartTime;
    
    QJsonArray eventsArray;
    for (const auto& event : m_events) {
        eventsArray.append(event);
    }
    payload["events"] = eventsArray;
    
    int eventCount = m_events.size();
    
    // Clear events after creating payload
    m_events.clear();
    
    qInfo().noquote() << QString("[Telemetry] FLUSH_START | Events: %1").arg(eventCount);
    
    sendTelemetry(payload);
    
    qint64 latency = timer.elapsed();
    qInfo().noquote() << QString("[Telemetry] FLUSH_COMPLETE | Events: %1 | Latency: %2ms")
        .arg(eventCount)
        .arg(latency);
    
    emit dataFlushed(eventCount);
}

QString TelemetryCollector::sanitize(const QString& input) const {
    // PRODUCTION-READY: Remove any PII (usernames, paths with usernames, emails, IPs)
    QString sanitized = input;
    
    // Remove Windows user paths (C:\Users\username\...)
    sanitized.replace(QRegularExpression("C:\\\\Users\\\\[^\\\\]+"), "C:\\Users\\[USER]");
    
    // Remove Unix user paths (/home/username/...)
    sanitized.replace(QRegularExpression("/home/[^/]+"), "/home/[USER]");
    
    // Remove email addresses
    sanitized.replace(QRegularExpression("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}"), "[EMAIL]");
    
    // Remove IP addresses
    sanitized.replace(QRegularExpression("\\b(?:[0-9]{1,3}\\.){3}[0-9]{1,3}\\b"), "[IP]");
    
    // Truncate long strings (prevent data bloat)
    if (sanitized.length() > 200) {
        sanitized = sanitized.left(197) + "...";
    }
    
    return sanitized;
}

void TelemetryCollector::sendTelemetry(const QJsonObject& payload) {
    // PRODUCTION-READY: External configuration via environment variable
    QString telemetryUrl = qEnvironmentVariable("TELEMETRY_ENDPOINT");
    
    if (telemetryUrl.isEmpty()) {
        // Default endpoint (can be changed via config)
        telemetryUrl = "https://telemetry.rawrxd.io/api/v1/events";
    }
    
    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    
    QNetworkRequest request;
    request.setUrl(QUrl(telemetryUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    QNetworkReply* reply = nam->post(request, data);
    
    // PRODUCTION-READY: Resource guard - cleanup reply when finished
    connect(reply, &QNetworkReply::finished, [reply, nam]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning().noquote() << QString("[Telemetry] SEND_FAILED | Error: %1").arg(reply->errorString());
        } else {
            qDebug().noquote() << "[Telemetry] SEND_SUCCESS | Status: OK";
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

bool TelemetryCollector::loadUserConsent() const {
    QSettings settings;
    return settings.value("Telemetry/Enabled", false).toBool();
}

void TelemetryCollector::saveUserConsent(bool enabled) {
    QSettings settings;
    settings.setValue("Telemetry/Enabled", enabled);
    settings.sync();
}
