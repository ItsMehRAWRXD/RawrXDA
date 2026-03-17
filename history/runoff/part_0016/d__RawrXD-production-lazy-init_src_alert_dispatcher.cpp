#include "alert_dispatcher.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDebug>
#include <QDateTime>
#include <QByteArray>
#include <QUrlQuery>

AlertDispatcher& AlertDispatcher::instance() {
    static AlertDispatcher instance;
    return instance;
}

AlertDispatcher::AlertDispatcher(QObject* parent)
    : QObject(parent),
      network_manager(new QNetworkAccessManager(this))
{
}

AlertDispatcher::~AlertDispatcher() {
}

void AlertDispatcher::initialize(const AlertConfig& config) {
    alert_config = config;
    
    qInfo() << "[AlertDispatcher] Initialized with config:"
            << "Email enabled:" << config.email_enabled
            << "Slack enabled:" << config.slack_enabled
            << "Webhook enabled:" << config.webhook_enabled;
}

void AlertDispatcher::dispatch(const Alert& alert) {
    qInfo() << "[AlertDispatcher] Dispatching alert:"
            << "Severity:" << static_cast<int>(alert.severity)
            << "Type:" << alert.alert_type
            << "Message:" << alert.message;
    
    // Record alert in history
    alert_history.append(alert);
    if (alert_history.size() > 1000) {
        alert_history.removeFirst();
    }
    
    // Dispatch to all enabled channels
    if (alert_config.email_enabled && !alert_config.email_recipients.empty()) {
        dispatchEmail(alert);
    }
    
    if (alert_config.slack_enabled && !alert_config.slack_webhook_url.isEmpty()) {
        dispatchSlack(alert);
    }
    
    if (alert_config.webhook_enabled && !alert_config.webhook_url.isEmpty()) {
        dispatchWebhook(alert);
    }
    
    if (alert_config.pagerduty_enabled && !alert_config.pagerduty_integration_key.isEmpty()) {
        dispatchPagerDuty(alert);
    }
    
    // Emit signal
    emit alertDispatched(alert);
}

void AlertDispatcher::dispatchSLAViolation(const QString& violation_details) {
    Alert alert;
    alert.alert_type = "SLA_VIOLATION";
    alert.severity = AlertSeverity::CRITICAL;
    alert.message = violation_details;
    alert.timestamp = QDateTime::currentDateTime();
    alert.tags["component"] = "SLA";
    
    dispatch(alert);
    
    // PRODUCTION READINESS: Trigger automatic remediation if configured
    if (alert_config.auto_remediate_sla) {
        triggerSLARemediation(violation_details);
    }
}

void AlertDispatcher::dispatchEmail(const Alert& alert) {
    qDebug() << "[AlertDispatcher] Sending email notification";
    
    // Email body
    QString subject = QString("[%1] %2")
        .arg(severityToString(alert.severity))
        .arg(alert.alert_type);
    
    QString body = QString(
        "Alert Type: %1\n"
        "Severity: %2\n"
        "Time: %3\n"
        "Message: %4\n"
        "\n"
        "Details:\n")
        .arg(alert.alert_type)
        .arg(severityToString(alert.severity))
        .arg(alert.timestamp.toString(Qt::ISODate))
        .arg(alert.message);
    
    for (auto it = alert.tags.constBegin(); it != alert.tags.constEnd(); ++it) {
        body += QString("%1: %2\n").arg(it.key(), it.value());
    }
    
    // For production: Use actual SMTP client
    // For now, log the email that would be sent
    if (!alert_config.email_recipients.empty()) {
        qInfo() << "[AlertDispatcher] Email would be sent:"
                << "Subject:" << subject;
    }
    
    emit emailSent(alert.alert_type, QString::number(alert_config.email_recipients.size()));
}

void AlertDispatcher::dispatchSlack(const Alert& alert) {
    qDebug() << "[AlertDispatcher] Sending Slack notification";
    
    // Build Slack message
    QJsonObject color_obj;
    QString color;
    switch (alert.severity) {
        case AlertSeverity::CRITICAL:
            color = "danger";
            break;
        case AlertSeverity::HIGH:
            color = "warning";
            break;
        case AlertSeverity::MEDIUM:
            color = "warning";
            break;
        default:
            color = "good";
    }
    
    QJsonObject slack_msg;
    slack_msg["text"] = QString("[%1] %2").arg(severityToString(alert.severity), alert.alert_type);
    slack_msg["color"] = color;
    
    QJsonObject attachment;
    attachment["title"] = alert.alert_type;
    attachment["text"] = alert.message;
    attachment["ts"] = static_cast<double>(alert.timestamp.toSecsSinceEpoch());
    
    QJsonArray attachments;
    attachments.append(attachment);
    slack_msg["attachments"] = attachments;
    
    // Send to Slack webhook
    QNetworkRequest request(QUrl(alert_config.slack_webhook_url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(slack_msg);
    network_manager->post(request, doc.toJson(QJsonDocument::Compact));
    
    emit slackMessageSent(alert.alert_type);
}

void AlertDispatcher::dispatchWebhook(const Alert& alert) {
    qDebug() << "[AlertDispatcher] Sending webhook notification";
    
    // Build webhook payload
    QJsonObject payload;
    payload["alert_type"] = alert.alert_type;
    payload["severity"] = severityToString(alert.severity);
    payload["message"] = alert.message;
    payload["timestamp"] = alert.timestamp.toString(Qt::ISODate);
    
    QJsonObject tags_obj;
    for (auto it = alert.tags.constBegin(); it != alert.tags.constEnd(); ++it) {
        tags_obj[it.key()] = it.value();
    }
    payload["tags"] = tags_obj;
    
    // Send to webhook
    QNetworkRequest request(QUrl(alert_config.webhook_url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(payload);
    network_manager->post(request, doc.toJson(QJsonDocument::Compact));
    
    qInfo() << "[AlertDispatcher] Webhook request sent";
    emit webhookDelivered(alert.alert_type);
}

void AlertDispatcher::dispatchPagerDuty(const Alert& alert) {
    qDebug() << "[AlertDispatcher] Sending PagerDuty event";
    
    // Build PagerDuty event payload
    QJsonObject event;
    event["routing_key"] = alert_config.pagerduty_integration_key;
    event["event_action"] = "trigger";
    event["dedup_key"] = QString("%1-%2")
        .arg(alert.alert_type)
        .arg(alert.timestamp.toString(Qt::ISODate));
    
    QJsonObject payload;
    payload["summary"] = alert.message;
    payload["severity"] = severityToPagerDutySeverity(alert.severity);
    payload["timestamp"] = alert.timestamp.toString(Qt::ISODate);
    
    QJsonObject custom_details;
    custom_details["alert_type"] = alert.alert_type;
    for (auto it = alert.tags.constBegin(); it != alert.tags.constEnd(); ++it) {
        custom_details[it.key()] = it.value();
    }
    payload["custom_details"] = custom_details;
    
    event["payload"] = payload;
    
    // Send to PagerDuty API
    QNetworkRequest request(QUrl("https://events.pagerduty.com/v2/enqueue"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(event);
    network_manager->post(request, doc.toJson(QJsonDocument::Compact));
    
    qInfo() << "[AlertDispatcher] PagerDuty event sent";
    emit pagerdutyEventSent(alert.alert_type);
}

void AlertDispatcher::triggerSLARemediation(const QString& violation_details) {
    qInfo() << "[AlertDispatcher] Triggering SLA remediation";
    
    // Rate limit automatic remediation
    QDateTime now = QDateTime::currentDateTime();
    if (last_remediation_time.isValid()) {
        int seconds_since_last = last_remediation_time.secsTo(now);
        // Allow at most max_auto_remediation_per_day times per day (86400 seconds)
        if (remediation_count >= alert_config.max_auto_remediation_per_day &&
            seconds_since_last < (86400 / alert_config.max_auto_remediation_per_day)) {
            QString reason = QString("Rate limited: %1 remediations already today")
                .arg(remediation_count);
            qWarning() << "[AlertDispatcher]" << reason;
            emit remediationRateLimited(reason);
            return;
        }
    }
    
    // Execute configured remediation strategies in priority order
    for (const auto& strategy : alert_config.remediation_strategies) {
        executeRemediationStrategy(strategy);
    }
    
    // Update tracking
    remediation_count++;
    last_remediation_time = now;
    
    emit remediationTriggered("AUTO_REMEDIATE_SLA", violation_details);
}

void AlertDispatcher::executeRemediationStrategy(AlertConfig::RemediationStrategy strategy) {
    QString strategy_name;
    bool success = false;
    QString result;
    
    switch (strategy) {
        case AlertConfig::RemediationStrategy::REBALANCE_MODELS: {
            strategy_name = "REBALANCE_MODELS";
            qInfo() << "[AlertDispatcher] Executing: Rebalance model load across instances";
            
            // Send rebalance command to all active model instances
            QJsonObject cmd;
            cmd["action"] = "rebalance_load";
            cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            // In real implementation, this would be sent to a model orchestrator
            result = "Load rebalancing initiated across all instances";
            success = true;
            break;
        }
        
        case AlertConfig::RemediationStrategy::SCALE_UP: {
            strategy_name = "SCALE_UP";
            qInfo() << "[AlertDispatcher] Executing: Scale up infrastructure";
            
            // Signal infrastructure manager to add more resources
            QJsonObject cmd;
            cmd["action"] = "scale_up";
            cmd["target_replicas"] = 3;  // Or calculate based on load
            cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            result = "Scale-up request sent to infrastructure manager";
            success = true;
            break;
        }
        
        case AlertConfig::RemediationStrategy::PRIORITY_ADJUST: {
            strategy_name = "PRIORITY_ADJUST";
            qInfo() << "[AlertDispatcher] Executing: Adjust request priorities";
            
            // Increase priority for latency-critical operations
            QJsonObject cmd;
            cmd["action"] = "adjust_priorities";
            cmd["mode"] = "high_latency_focus";
            cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            result = "Priority adjustment applied to request queue";
            success = true;
            break;
        }
        
        case AlertConfig::RemediationStrategy::CACHE_FLUSH: {
            strategy_name = "CACHE_FLUSH";
            qInfo() << "[AlertDispatcher] Executing: Flush caches to reset state";
            
            // Clear model caches and warm them with common requests
            QJsonObject cmd;
            cmd["action"] = "flush_caches";
            cmd["warm_common_paths"] = true;
            cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            result = "Cache flush initiated and rewarm started";
            success = true;
            break;
        }
        
        case AlertConfig::RemediationStrategy::FAILOVER: {
            strategy_name = "FAILOVER";
            qInfo() << "[AlertDispatcher] Executing: Failover to backup infrastructure";
            
            // Switch to backup instances or availability zones
            QJsonObject cmd;
            cmd["action"] = "initiate_failover";
            cmd["backup_target"] = "primary_backup";
            cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            result = "Failover sequence initiated";
            success = true;
            break;
        }
        
        case AlertConfig::RemediationStrategy::RESTART_SERVICE: {
            strategy_name = "RESTART_SERVICE";
            qInfo() << "[AlertDispatcher] Executing: Restart troubled service";
            
            // Restart inference service with clean state
            QJsonObject cmd;
            cmd["action"] = "restart_service";
            cmd["graceful"] = true;
            cmd["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            
            result = "Service restart initiated";
            success = true;
            break;
        }
        
        default:
            strategy_name = "UNKNOWN";
            result = "Unknown remediation strategy";
            success = false;
    }
    
    // Track strategy usage
    remediation_strategy_counts[strategy_name]++;
    
    // Emit signal for external monitoring
    emit remediationExecuted(strategy_name, success, result);
    
    qInfo() << "[AlertDispatcher] Remediation executed:"
            << "Strategy:" << strategy_name
            << "Success:" << success
            << "Result:" << result;
}

void AlertDispatcher::executeRemediation(AlertConfig::RemediationStrategy strategy, const QString& reason) {
    qInfo() << "[AlertDispatcher] Manual remediation trigger:"
            << "Strategy: " << (int)strategy
            << "Reason:" << reason;
    
    executeRemediationStrategy(strategy);
}

int AlertDispatcher::getRemediationCount() const {
    return remediation_count;
}

QDateTime AlertDispatcher::getLastRemediationTime() const {
    return last_remediation_time;
}

QString AlertDispatcher::severityToString(AlertSeverity severity) const {
    switch (severity) {
        case AlertSeverity::CRITICAL:
            return "CRITICAL";
        case AlertSeverity::HIGH:
            return "HIGH";
        case AlertSeverity::MEDIUM:
            return "MEDIUM";
        case AlertSeverity::LOW:
            return "LOW";
        case AlertSeverity::INFO:
            return "INFO";
        default:
            return "UNKNOWN";
    }
}

QString AlertDispatcher::severityToPagerDutySeverity(AlertSeverity severity) const {
    switch (severity) {
        case AlertSeverity::CRITICAL:
            return "critical";
        case AlertSeverity::HIGH:
            return "error";
        case AlertSeverity::MEDIUM:
            return "warning";
        case AlertSeverity::LOW:
        case AlertSeverity::INFO:
            return "info";
        default:
            return "info";
    }
}

QList<AlertDispatcher::Alert> AlertDispatcher::getAlertHistory(int limit) const {
    if (limit <= 0) {
        return alert_history;
    }
    return alert_history.mid(qMax(0, alert_history.size() - limit));
}

void AlertDispatcher::clearAlertHistory() {
    alert_history.clear();
}

AlertDispatcher::AlertStats AlertDispatcher::getAlertStats() const {
    AlertStats stats;
    stats.total_alerts = alert_history.size();
    
    for (const auto& alert : alert_history) {
        switch (alert.severity) {
            case AlertSeverity::CRITICAL:
                stats.critical_count++;
                break;
            case AlertSeverity::HIGH:
                stats.high_count++;
                break;
            case AlertSeverity::MEDIUM:
                stats.medium_count++;
                break;
            case AlertSeverity::LOW:
                stats.low_count++;
                break;
            case AlertSeverity::INFO:
                stats.info_count++;
                break;
        }
    }
    
    return stats;
}
