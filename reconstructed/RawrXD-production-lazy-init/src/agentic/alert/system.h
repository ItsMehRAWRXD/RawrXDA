/**
 * @file agentic_alert_system.h
 * @brief Production-grade alert generation and notification system for agentic operations
 * @author RawrXD Team
 * @date 2026
 * 
 * Provides:
 * - Threshold-triggered alerts for metrics anomalies
 * - Real-time alert routing (email, webhook, syslog, dashboard)
 * - Alert escalation policies
 * - Suppression and deduplication to prevent alert fatigue
 * - Integration with observability stack
 */

#pragma once

#include <QString>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QList>
#include <QMutex>
#include <memory>
#include <vector>
#include <functional>

/**
 * @class AlertRule
 * @brief Defines a condition that triggers an alert
 */
class AlertRule {
public:
    enum class AlertSeverity {
        INFO,
        WARNING,
        CRITICAL,
        EMERGENCY
    };
    
    enum class ThresholdOperator {
        GREATER_THAN,
        LESS_THAN,
        EQUAL_TO,
        NOT_EQUAL_TO,
        GREATER_OR_EQUAL,
        LESS_OR_EQUAL
    };
    
    QString ruleId;
    QString name;
    QString description;
    QString metricName;
    double thresholdValue;
    ThresholdOperator op;
    AlertSeverity severity;
    int evaluationIntervalSeconds;
    int suppressionWindowSeconds;
    bool enabled;
    QString component;
    QJsonObject metadata;
    
    AlertRule() = default;
    AlertRule(const QString& id, const QString& metric, double threshold, 
              ThresholdOperator operation, AlertSeverity sev, int interval = 60);
    
    bool evaluate(double metricValue) const;
};

/**
 * @class Alert
 * @brief Represents an active alert instance
 */
class Alert {
public:
    QString alertId;
    QString ruleId;
    AlertRule::AlertSeverity severity;
    QString title;
    QString message;
    QDateTime timestamp;
    QDateTime acknowledgedTime;
    bool isAcknowledged;
    int triggerCount;
    QString component;
    QString metricName;
    double metricValue;
    QJsonObject context;
    
    Alert() : isAcknowledged(false), triggerCount(0), metricValue(0.0) {}
    
    QString toJson() const;
};

/**
 * @class AlertChannel
 * @brief Abstract base for alert delivery channels
 */
class AlertChannel {
public:
    virtual ~AlertChannel() = default;
    virtual bool send(const Alert& alert) = 0;
    virtual QString channelName() const = 0;
};

/**
 * @class EmailAlertChannel
 * @brief Sends alerts via email
 */
class EmailAlertChannel : public AlertChannel {
public:
    EmailAlertChannel(const QString& smtpServer, int port, const QString& fromAddr);
    
    bool send(const Alert& alert) override;
    QString channelName() const override { return "Email"; }
    void addRecipient(const QString& email);
    
private:
    QString m_smtpServer;
    int m_smtpPort;
    QString m_fromAddress;
    QList<QString> m_recipients;
};

/**
 * @class WebhookAlertChannel
 * @brief Sends alerts via HTTP webhooks
 */
class WebhookAlertChannel : public AlertChannel {
public:
    explicit WebhookAlertChannel(const QString& webhookUrl);
    
    bool send(const Alert& alert) override;
    QString channelName() const override { return "Webhook"; }
    
private:
    QString m_webhookUrl;
};

/**
 * @class SyslogAlertChannel
 * @brief Sends alerts to system syslog
 */
class SyslogAlertChannel : public AlertChannel {
public:
    explicit SyslogAlertChannel(const QString& facility = "LOCAL0");
    
    bool send(const Alert& alert) override;
    QString channelName() const override { return "Syslog"; }
    
private:
    QString m_facility;
};

/**
 * @class DashboardAlertChannel
 * @brief Broadcasts alerts to connected dashboard instances
 */
class DashboardAlertChannel : public AlertChannel {
public:
    bool send(const Alert& alert) override;
    QString channelName() const override { return "Dashboard"; }
    
    // Register dashboard instance for receiving alerts
    void registerDashboard(QObject* dashboard);
    void unregisterDashboard(QObject* dashboard);
    
private:
    QList<QObject*> m_dashboards;
    QMutex m_dashboardMutex;
};

/**
 * @class AlertManager
 * @brief Centralized alert generation and routing system
 * 
 * Responsibilities:
 * - Rule evaluation against metrics
 * - Alert creation and deduplication
 * - Suppression window management
 * - Multi-channel alert routing
 * - Escalation policy enforcement
 * - Alert acknowledgment and lifecycle
 */
class AlertManager : public QObject {
    Q_OBJECT
    
public:
    explicit AlertManager(QObject* parent = nullptr);
    ~AlertManager();
    
    // Rule Management
    void addAlertRule(const AlertRule& rule);
    void removeAlertRule(const QString& ruleId);
    void updateAlertRule(const AlertRule& rule);
    AlertRule getAlertRule(const QString& ruleId) const;
    QList<AlertRule> getAllRules() const;
    void enableRule(const QString& ruleId, bool enabled);
    
    // Channel Management
    void addAlertChannel(std::shared_ptr<AlertChannel> channel);
    void removeAlertChannel(const QString& channelName);
    QList<std::shared_ptr<AlertChannel>> getChannels() const;
    
    // Alert Management
    void evaluateMetric(const QString& metricName, double value, const QString& component = "");
    void dismissAlert(const QString& alertId);
    void acknowledgeAlert(const QString& alertId);
    QList<Alert> getActiveAlerts(AlertRule::AlertSeverity minSeverity = AlertRule::AlertSeverity::INFO) const;
    QList<Alert> getAlertHistory(int lastNMinutes = 60) const;
    
    // Bulk Operations
    void evaluateMetrics(const QJsonObject& metrics, const QString& component = "");
    void clearExpiredAlerts();
    
    // Configuration
    void setSuppressionWindow(const QString& ruleId, int seconds);
    void setEscalationThreshold(int count);
    void setMaxActiveAlerts(int max);
    
signals:
    void alertTriggered(const Alert& alert);
    void alertAcknowledged(const QString& alertId);
    void alertDismissed(const QString& alertId);
    void ruleEvaluated(const QString& ruleId, bool triggered);
    void channelFailure(const QString& channelName, const QString& error);
    
private slots:
    void onCleanupTimer();
    
private:
    void sendAlert(const Alert& alert);
    bool isAlertSuppressed(const QString& ruleId) const;
    QString generateAlertId();
    
    QMap<QString, AlertRule> m_rules;
    QMap<QString, Alert> m_activeAlerts;
    QList<Alert> m_alertHistory;
    QMap<QString, QDateTime> m_suppressionWindows;
    
    std::vector<std::shared_ptr<AlertChannel>> m_channels;
    
    int m_escalationThreshold;
    int m_maxActiveAlerts;
    
    QMutex m_mutex;
    QTimer* m_cleanupTimer;
};

#endif // AGENTIC_ALERT_SYSTEM_H
