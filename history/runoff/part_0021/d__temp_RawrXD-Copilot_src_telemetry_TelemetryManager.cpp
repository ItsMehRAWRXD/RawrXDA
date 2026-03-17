#include "TelemetryManager.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QUuid>
#include <QSysInfo>
#include <QApplication>
#include <QDebug>

TelemetryManager::TelemetryManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_flushTimer(new QTimer(this))
    , m_sessionStart(QDateTime::currentMSecsSinceEpoch())
{
    // Generate unique session ID
    initializeSession();
    
    // Setup flush timer
    connect(m_flushTimer, &QTimer::timeout, this, &TelemetryManager::onFlushTimer);
    
    // Default configuration
    m_config = TelemetryConfig();
    m_config.enabled = false; // Opt-in by default
    m_config.flushInterval = 30000; // 30 seconds
    m_config.batchSize = 10;
    
    m_flushTimer->setInterval(m_config.flushInterval);
    m_flushTimer->start();
}

TelemetryManager::~TelemetryManager()
{
    // Flush any remaining events
    if (!m_eventQueue.isEmpty()) {
        flush();
    }
}

void TelemetryManager::setConfig(const TelemetryConfig& config)
{
    m_config = config;
    m_flushTimer->setInterval(m_config.flushInterval);
    
    if (m_config.enabled) {
        m_flushTimer->start();
    } else {
        m_flushTimer->stop();
    }
}

TelemetryManager::TelemetryConfig TelemetryManager::getConfig() const
{
    return m_config;
}

void TelemetryManager::setEnabled(bool enabled)
{
    if (m_config.enabled != enabled) {
        m_config.enabled = enabled;
        
        if (enabled) {
            m_flushTimer->start();
            trackEvent("telemetry_enabled");
        } else {
            flush(); // Send any pending events before disabling
            m_flushTimer->stop();
            trackEvent("telemetry_disabled");
        }
    }
}

bool TelemetryManager::isEnabled() const
{
    return m_config.enabled;
}

void TelemetryManager::trackEvent(const QString& eventName, const QJsonObject& properties)
{
    if (!m_config.enabled) {
        return;
    }
    
    // Check if event is excluded
    if (m_config.excludedEvents.contains(eventName)) {
        return;
    }
    
    QJsonObject event = createEvent(eventName, properties);
    m_eventQueue.append(event);
    
    emit eventTracked(eventName, properties);
    
    // Auto-flush if batch is full
    if (m_eventQueue.size() >= m_config.batchSize) {
        flush();
    }
}

void TelemetryManager::trackSuggestionShown(const QString& suggestionType, int rank, double score)
{
    QJsonObject props;
    props["suggestionType"] = suggestionType;
    props["rank"] = rank;
    props["score"] = score;
    
    trackEvent("suggestion_shown", props);
}

void TelemetryManager::trackSuggestionAccepted(const QString& suggestionType, int rank, double score)
{
    QJsonObject props;
    props["suggestionType"] = suggestionType;
    props["rank"] = rank;
    props["score"] = score;
    
    trackEvent("suggestion_accepted", props);
}

void TelemetryManager::trackSuggestionRejected(const QString& suggestionType, int rank, double score)
{
    QJsonObject props;
    props["suggestionType"] = suggestionType;
    props["rank"] = rank;
    props["score"] = score;
    
    trackEvent("suggestion_rejected", props);
}

void TelemetryManager::trackPluginAction(const QString& pluginName, const QString& action, bool success)
{
    QJsonObject props;
    props["pluginName"] = pluginName;
    props["action"] = action;
    props["success"] = success;
    
    trackEvent("plugin_action", props);
}

void TelemetryManager::trackError(const QString& errorType, const QString& errorMessage)
{
    QJsonObject props;
    props["errorType"] = errorType;
    props["errorMessage"] = errorMessage.left(200); // Limit message length
    
    trackEvent("error", props);
}

void TelemetryManager::trackPerformance(const QString& operation, qint64 durationMs)
{
    QJsonObject props;
    props["operation"] = operation;
    props["durationMs"] = durationMs;
    
    trackEvent("performance", props);
}

void TelemetryManager::flush()
{
    if (!m_config.enabled || m_eventQueue.isEmpty()) {
        return;
    }
    
    sendBatch();
}

void TelemetryManager::onFlushTimer()
{
    if (m_config.enabled && !m_eventQueue.isEmpty()) {
        sendBatch();
    }
}

void TelemetryManager::onNetworkReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() != QNetworkReply::NoError) {
        handleError(reply->errorString());
    } else {
        emit batchSent(m_eventQueue.size());
    }
    
    reply->deleteLater();
    
    // Clear sent events
    m_eventQueue.clear();
}

void TelemetryManager::initializeSession()
{
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QJsonObject TelemetryManager::createEvent(const QString& eventName, const QJsonObject& properties)
{
    QJsonObject event;
    
    // Event metadata
    event["eventId"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
    event["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    event["eventName"] = eventName;
    event["sessionId"] = m_sessionId;
    event["sessionDuration"] = QDateTime::currentMSecsSinceEpoch() - m_sessionStart;
    
    // Application metadata
    event["appVersion"] = "1.0.0"; // TODO: Get from build system
    event["platform"] = QSysInfo::productType();
    event["platformVersion"] = QSysInfo::productVersion();
    event["architecture"] = QSysInfo::currentCpuArchitecture();
    
    // Add custom properties
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        event[it.key()] = it.value();
    }
    
    return event;
}

void TelemetryManager::sendBatch()
{
    if (m_eventQueue.isEmpty()) {
        return;
    }
    
    QNetworkRequest request(QUrl(m_config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject payload;
    payload["events"] = QJsonArray::fromVariantList(
        QVariantList() << QVariant(m_eventQueue)
    );
    
    QJsonDocument doc(payload);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    QNetworkReply* reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, this, &TelemetryManager::onNetworkReply);
}

void TelemetryManager::handleError(const QString& error)
{
    qWarning() << "[TELEMETRY] Error:" << error;
    emit errorOccurred(error);
}
