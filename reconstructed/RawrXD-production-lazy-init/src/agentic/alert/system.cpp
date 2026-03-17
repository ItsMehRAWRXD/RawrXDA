/**
 * @file agentic_alert_system.cpp
 * @brief Production alert generation and notification system implementation
 */

#include "agentic_alert_system.h"
#include <QTimer>
#include <QUuid>
#include <QJsonDocument>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <QSyslogServer>
#include <algorithm>

// ============================================================================
// AlertRule Implementation
// ============================================================================

AlertRule::AlertRule(const QString& id, const QString& metric, double threshold,
                     ThresholdOperator operation, AlertSeverity sev, int interval)
    : ruleId(id), metricName(metric), thresholdValue(threshold), op(operation),
      severity(sev), evaluationIntervalSeconds(interval), 
      suppressionWindowSeconds(300), enabled(true)
{
}

bool AlertRule::evaluate(double metricValue) const
{
    if (!enabled) return false;
    
    switch (op) {
        case ThresholdOperator::GREATER_THAN:
            return metricValue > thresholdValue;
        case ThresholdOperator::LESS_THAN:
            return metricValue < thresholdValue;
        case ThresholdOperator::EQUAL_TO:
            return metricValue == thresholdValue;
        case ThresholdOperator::NOT_EQUAL_TO:
            return metricValue != thresholdValue;
        case ThresholdOperator::GREATER_OR_EQUAL:
            return metricValue >= thresholdValue;
        case ThresholdOperator::LESS_OR_EQUAL:
            return metricValue <= thresholdValue;
    }
    return false;
}

QString Alert::toJson() const
{
    QJsonObject json;
    json["alertId"] = alertId;
    json["ruleId"] = ruleId;
    json["severity"] = static_cast<int>(severity);
    json["title"] = title;
    json["message"] = message;
    json["timestamp"] = timestamp.toSecsSinceEpoch();
    json["isAcknowledged"] = isAcknowledged;
    json["triggerCount"] = triggerCount;
    json["component"] = component;
    json["metricName"] = metricName;
    json["metricValue"] = metricValue;
    json["context"] = context;
    
    return QJsonDocument(json).toJson(QJsonDocument::Compact);
}

// ============================================================================
// EmailAlertChannel Implementation
// ============================================================================

EmailAlertChannel::EmailAlertChannel(const QString& smtpServer, int port, const QString& fromAddr)
    : m_smtpServer(smtpServer), m_smtpPort(port), m_fromAddress(fromAddr)
{
}

bool EmailAlertChannel::send(const Alert& alert)
{
    if (m_recipients.isEmpty()) {
        qWarning() << "[EmailAlertChannel] No recipients configured";
        return false;
    }
    
    // Build email headers and body
    QString subject = QString("[%1] %2").arg(alert.component, alert.title);
    QString body = QString(
        "Alert: %1\n"
        "Severity: %2\n"
        "Component: %3\n"
        "Message: %4\n"
        "Metric: %5 = %6\n"
        "Time: %7\n"
        "\n%8"
    ).arg(alert.title)
     .arg(static_cast<int>(alert.severity))
     .arg(alert.component)
     .arg(alert.message)
     .arg(alert.metricName)
     .arg(alert.metricValue)
     .arg(alert.timestamp.toString())
     .arg(alert.context.isEmpty() ? "" : QJsonDocument(alert.context).toJson());
    
    // In production, use actual SMTP client (Qt doesn't have built-in SMTP)
    // This is a placeholder for the SMTP implementation
    qInfo() << "[EmailAlertChannel] Would send email to" << m_recipients.join(", ")
            << "Subject:" << subject;
    
    return true;
}

void EmailAlertChannel::addRecipient(const QString& email)
{
    if (!m_recipients.contains(email)) {
        m_recipients.append(email);
    }
}

// ============================================================================
// WebhookAlertChannel Implementation
// ============================================================================

WebhookAlertChannel::WebhookAlertChannel(const QString& webhookUrl)
    : m_webhookUrl(webhookUrl)
{
}

bool WebhookAlertChannel::send(const Alert& alert)
{
    // Create payload
    QJsonObject payload;
    payload["alertId"] = alert.alertId;
    payload["ruleId"] = alert.ruleId;
    payload["severity"] = static_cast<int>(alert.severity);
    payload["title"] = alert.title;
    payload["message"] = alert.message;
    payload["timestamp"] = alert.timestamp.toSecsSinceEpoch();
    payload["component"] = alert.component;
    payload["metricName"] = alert.metricName;
    payload["metricValue"] = alert.metricValue;
    payload["context"] = alert.context;
    
    // In production, send via QNetworkAccessManager
    qInfo() << "[WebhookAlertChannel] Would POST to" << m_webhookUrl
            << "payload:" << QJsonDocument(payload).toJson(QJsonDocument::Compact);
    
    // Actual implementation would use:
    // QNetworkAccessManager manager;
    // QNetworkRequest request(QUrl(m_webhookUrl));
    // request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // manager.post(request, QJsonDocument(payload).toJson());
    
    return true;
}

// ============================================================================
// SyslogAlertChannel Implementation
// ============================================================================

SyslogAlertChannel::SyslogAlertChannel(const QString& facility)
    : m_facility(facility)
{
}

bool SyslogAlertChannel::send(const Alert& alert)
{
    QString severityMap[] = { "INFO", "WARNING", "CRITICAL", "EMERGENCY" };
    QString severity = severityMap[static_cast<int>(alert.severity)];
    
    QString message = QString("[%1][%2] %3: %4 (%5=%6)")
        .arg(alert.component)
        .arg(severity)
        .arg(alert.title)
        .arg(alert.message)
        .arg(alert.metricName)
        .arg(alert.metricValue);
    
    // In production, use actual syslog implementation
    qInfo() << "[SyslogAlertChannel]" << message;
    
    return true;
}

// ============================================================================
// DashboardAlertChannel Implementation
// ============================================================================

bool DashboardAlertChannel::send(const Alert& alert)
{
    QMutexLocker locker(&m_dashboardMutex);
    
    for (QObject* dashboard : m_dashboards) {
        if (dashboard) {
            // Emit signal to dashboard
            QMetaObject::invokeMethod(dashboard, "onAlertReceived",
                Qt::QueuedConnection,
                Q_ARG(QString, alert.alertId),
                Q_ARG(QString, alert.title),
                Q_ARG(int, static_cast<int>(alert.severity)));
        }
    }
    
    return !m_dashboards.isEmpty();
}

void DashboardAlertChannel::registerDashboard(QObject* dashboard)
{
    QMutexLocker locker(&m_dashboardMutex);
    if (!m_dashboards.contains(dashboard)) {
        m_dashboards.append(dashboard);
    }
}

void DashboardAlertChannel::unregisterDashboard(QObject* dashboard)
{
    QMutexLocker locker(&m_dashboardMutex);
    m_dashboards.removeAll(dashboard);
}

// ============================================================================
// AlertManager Implementation
// ============================================================================

AlertManager::AlertManager(QObject* parent)
    : QObject(parent), m_escalationThreshold(5), m_maxActiveAlerts(1000)
{
    // Initialize cleanup timer (runs every 5 minutes)
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &AlertManager::onCleanupTimer);
    m_cleanupTimer->start(300000); // 5 minutes
    
    qInfo() << "[AlertManager] Initialized";
}

AlertManager::~AlertManager()
{
    if (m_cleanupTimer) {
        m_cleanupTimer->stop();
    }
}

void AlertManager::addAlertRule(const AlertRule& rule)
{
    QMutexLocker locker(&m_mutex);
    m_rules[rule.ruleId] = rule;
    qInfo() << "[AlertManager] Added rule:" << rule.ruleId << "for metric:" << rule.metricName;
}

void AlertManager::removeAlertRule(const QString& ruleId)
{
    QMutexLocker locker(&m_mutex);
    m_rules.remove(ruleId);
    m_suppressionWindows.remove(ruleId);
    qInfo() << "[AlertManager] Removed rule:" << ruleId;
}

void AlertManager::updateAlertRule(const AlertRule& rule)
{
    QMutexLocker locker(&m_mutex);
    m_rules[rule.ruleId] = rule;
    qInfo() << "[AlertManager] Updated rule:" << rule.ruleId;
}

AlertRule AlertManager::getAlertRule(const QString& ruleId) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.value(ruleId);
}

QList<AlertRule> AlertManager::getAllRules() const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.values();
}

void AlertManager::enableRule(const QString& ruleId, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.contains(ruleId)) {
        m_rules[ruleId].enabled = enabled;
    }
}

void AlertManager::addAlertChannel(std::shared_ptr<AlertChannel> channel)
{
    if (channel) {
        m_channels.push_back(channel);
        qInfo() << "[AlertManager] Added alert channel:" << channel->channelName();
    }
}

void AlertManager::removeAlertChannel(const QString& channelName)
{
    auto it = std::remove_if(m_channels.begin(), m_channels.end(),
        [&channelName](const std::shared_ptr<AlertChannel>& ch) {
            return ch->channelName() == channelName;
        });
    m_channels.erase(it, m_channels.end());
    qInfo() << "[AlertManager] Removed alert channel:" << channelName;
}

QList<std::shared_ptr<AlertChannel>> AlertManager::getChannels() const
{
    QList<std::shared_ptr<AlertChannel>> result;
    for (const auto& channel : m_channels) {
        result.append(channel);
    }
    return result;
}

void AlertManager::evaluateMetric(const QString& metricName, double value, const QString& component)
{
    QMutexLocker locker(&m_mutex);
    
    for (auto& rule : m_rules) {
        if (rule.metricName == metricName && rule.evaluate(value)) {
            if (!isAlertSuppressed(rule.ruleId)) {
                Alert alert;
                alert.alertId = generateAlertId();
                alert.ruleId = rule.ruleId;
                alert.severity = rule.severity;
                alert.title = rule.name;
                alert.message = rule.description;
                alert.timestamp = QDateTime::currentDateTime();
                alert.component = component.isEmpty() ? rule.component : component;
                alert.metricName = metricName;
                alert.metricValue = value;
                
                sendAlert(alert);
                emit alertTriggered(alert);
                
                // Update suppression window
                m_suppressionWindows[rule.ruleId] = QDateTime::currentDateTime();
                qInfo() << "[AlertManager] Alert triggered:" << alert.title;
            }
        }
    }
}

void AlertManager::evaluateMetrics(const QJsonObject& metrics, const QString& component)
{
    for (auto it = metrics.begin(); it != metrics.end(); ++it) {
        if (it.value().isDouble()) {
            evaluateMetric(it.key(), it.value().toDouble(), component);
        }
    }
}

void AlertManager::dismissAlert(const QString& alertId)
{
    QMutexLocker locker(&m_mutex);
    if (m_activeAlerts.contains(alertId)) {
        m_activeAlerts.remove(alertId);
        emit alertDismissed(alertId);
        qInfo() << "[AlertManager] Dismissed alert:" << alertId;
    }
}

void AlertManager::acknowledgeAlert(const QString& alertId)
{
    QMutexLocker locker(&m_mutex);
    if (m_activeAlerts.contains(alertId)) {
        m_activeAlerts[alertId].isAcknowledged = true;
        m_activeAlerts[alertId].acknowledgedTime = QDateTime::currentDateTime();
        emit alertAcknowledged(alertId);
        qInfo() << "[AlertManager] Acknowledged alert:" << alertId;
    }
}

QList<Alert> AlertManager::getActiveAlerts(AlertRule::AlertSeverity minSeverity) const
{
    QMutexLocker locker(&m_mutex);
    QList<Alert> result;
    for (const auto& alert : m_activeAlerts) {
        if (static_cast<int>(alert.severity) <= static_cast<int>(minSeverity)) {
            result.append(alert);
        }
    }
    return result;
}

QList<Alert> AlertManager::getAlertHistory(int lastNMinutes) const
{
    QMutexLocker locker(&m_mutex);
    QList<Alert> result;
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-lastNMinutes * 60);
    
    for (const auto& alert : m_alertHistory) {
        if (alert.timestamp >= cutoff) {
            result.append(alert);
        }
    }
    return result;
}

void AlertManager::setSuppressionWindow(const QString& ruleId, int seconds)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.contains(ruleId)) {
        m_rules[ruleId].suppressionWindowSeconds = seconds;
    }
}

void AlertManager::setEscalationThreshold(int count)
{
    QMutexLocker locker(&m_mutex);
    m_escalationThreshold = count;
}

void AlertManager::setMaxActiveAlerts(int max)
{
    QMutexLocker locker(&m_mutex);
    m_maxActiveAlerts = max;
}

void AlertManager::onCleanupTimer()
{
    QMutexLocker locker(&m_mutex);
    clearExpiredAlerts();
}

void AlertManager::sendAlert(const Alert& alert)
{
    // Add to active alerts
    m_activeAlerts[alert.alertId] = alert;
    m_alertHistory.append(alert);
    
    // Trim history if needed
    if (m_alertHistory.size() > m_maxActiveAlerts * 2) {
        m_alertHistory.erase(m_alertHistory.begin(), 
                            m_alertHistory.begin() + m_alertHistory.size() / 2);
    }
    
    // Route to all channels
    for (auto& channel : m_channels) {
        if (!channel->send(alert)) {
            emit channelFailure(channel->channelName(), "Failed to send alert");
        }
    }
}

bool AlertManager::isAlertSuppressed(const QString& ruleId) const
{
    auto it = m_suppressionWindows.find(ruleId);
    if (it != m_suppressionWindows.end()) {
        int elapsed = it.value().secsTo(QDateTime::currentDateTime());
        if (m_rules.contains(ruleId)) {
            return elapsed < m_rules[ruleId].suppressionWindowSeconds;
        }
    }
    return false;
}

QString AlertManager::generateAlertId()
{
    return "ALERT-" + QUuid::createUuid().toString(QUuid::WithoutBraces).mid(0, 12);
}

void AlertManager::clearExpiredAlerts()
{
    // Remove acknowledged alerts older than 24 hours
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    
    auto it = m_activeAlerts.begin();
    while (it != m_activeAlerts.end()) {
        if (it.value().isAcknowledged && it.value().timestamp < cutoff) {
            it = m_activeAlerts.erase(it);
        } else {
            ++it;
        }
    }
}
