#ifndef ALERT_DISPATCHER_H
#define ALERT_DISPATCHER_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QList>
#include <QMap>
#include <memory>
#include <vector>

class QNetworkAccessManager;

/**
 * @brief Production-ready alert dispatcher for SLA violations and system alerts
 * 
 * Supports multiple notification channels:
 * - Email (SMTP)
 * - Slack webhooks
 * - Custom webhooks
 * - PagerDuty integration
 * 
 * Automatically triggers remediation actions for SLA breaches.
 */
class AlertDispatcher : public QObject {
    Q_OBJECT
    
public:
    // Alert severity levels
    enum class AlertSeverity {
        INFO,
        LOW,
        MEDIUM,
        HIGH,
        CRITICAL
    };
    
    // Alert structure
    struct Alert {
        QString alert_type;
        QString message;
        AlertSeverity severity = AlertSeverity::MEDIUM;
        QDateTime timestamp;
        QMap<QString, QString> tags;
    };
    
    // Alert statistics
    struct AlertStats {
        int total_alerts = 0;
        int critical_count = 0;
        int high_count = 0;
        int medium_count = 0;
        int low_count = 0;
        int info_count = 0;
    };
    
    // Configuration for alert dispatcher
    struct AlertConfig {
        // Email notifications
        bool email_enabled = false;
        std::vector<std::string> email_recipients;
        QString smtp_server;
        int smtp_port = 587;
        QString smtp_username;
        QString smtp_password;
        bool smtp_tls = true;
        
        // Slack notifications
        bool slack_enabled = false;
        QString slack_webhook_url;
        
        // Generic webhooks
        bool webhook_enabled = false;
        QString webhook_url;
        QString webhook_auth_token;
        
        // PagerDuty integration
        bool pagerduty_enabled = false;
        QString pagerduty_integration_key;
        
        // Auto-remediation configuration
        bool auto_remediate_sla = true;
        enum class RemediationStrategy {
            REBALANCE_MODELS,      // Redistribute load across instances
            SCALE_UP,              // Add more resources
            PRIORITY_ADJUST,       // Increase priority for latency-critical ops
            CACHE_FLUSH,           // Clear caches to reset state
            FAILOVER,              // Switch to backup infrastructure
            RESTART_SERVICE        // Restart troubled service
        };
        std::vector<RemediationStrategy> remediation_strategies = {
            RemediationStrategy::REBALANCE_MODELS,
            RemediationStrategy::SCALE_UP,
            RemediationStrategy::PRIORITY_ADJUST
        };
        int max_auto_remediation_per_day = 5;  // Rate limit automatic fixes
        
        // Alert filtering
        AlertSeverity min_severity = AlertSeverity::MEDIUM;
        int max_alerts_per_minute = 10;  // Rate limit
    };
    
    // Singleton access
    static AlertDispatcher& instance();
    
    // Configuration
    void initialize(const AlertConfig& config);
    
    // Send alert
    void dispatch(const Alert& alert);
    
    // Send SLA violation alert (convenience method)
    void dispatchSLAViolation(const QString& violation_details);
    
    // Manual remediation trigger
    void executeRemediation(AlertConfig::RemediationStrategy strategy, const QString& reason = "");
    
    // Query remediation status
    int getRemediationCount() const;
    QDateTime getLastRemediationTime() const;
    
    // History and statistics
    QList<Alert> getAlertHistory(int limit = -1) const;
    void clearAlertHistory();
    AlertStats getAlertStats() const;
    
signals:
    // Dispatched signals
    void alertDispatched(const Alert& alert);
    void emailSent(const QString& alert_type, const QString& recipient_count);
    void slackMessageSent(const QString& alert_type);
    void webhookDelivered(const QString& alert_type);
    void pagerdutyEventSent(const QString& alert_type);
    
    // Remediation
    void remediationTriggered(const QString& action, const QString& details);
    void remediationExecuted(const QString& strategy, bool success, const QString& result);
    void remediationRateLimited(const QString& reason);
    
private:
    AlertDispatcher(QObject* parent = nullptr);
    ~AlertDispatcher();
    
    // Dispatch to individual channels
    void dispatchEmail(const Alert& alert);
    void dispatchSlack(const Alert& alert);
    void dispatchWebhook(const Alert& alert);
    void dispatchPagerDuty(const Alert& alert);
    
    // Remediation actions
    void triggerSLARemediation(const QString& violation_details);
    void executeRemediationStrategy(AlertConfig::RemediationStrategy strategy);
    
    // Utility methods
    QString severityToString(AlertSeverity severity) const;
    QString severityToPagerDutySeverity(AlertSeverity severity) const;
    
    // Configuration
    AlertConfig alert_config;
    
    // HTTP client
    QNetworkAccessManager* network_manager;
    
    // History
    QList<Alert> alert_history;
    
    // Remediation tracking
    int remediation_count = 0;
    QDateTime last_remediation_time;
    QMap<QString, int> remediation_strategy_counts;  // Track usage of each strategy
};

#endif // ALERT_DISPATCHER_H
