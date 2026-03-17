#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QTimer>
#include <QSystemTrayIcon>

/**
 * @class AlertSystem
 * @brief Provides proactive issue detection and intelligent notifications
 * 
 * Features:
 * - Proactive issue detection based on patterns
 * - Intelligent alert prioritization and grouping
 * - Contextual notification delivery
 * - Alert escalation and resolution tracking
 * - Performance anomaly detection
 * - Resource usage monitoring and alerts
 */
class AlertSystem : public QObject {
    Q_OBJECT
public:
    explicit AlertSystem(QObject* parent = nullptr);
    virtual ~AlertSystem();

    // Alert management
    QString createAlert(const QString& type, const QString& message, const QJsonObject& context = QJsonObject());
    void resolveAlert(const QString& alertId);
    void dismissAlert(const QString& alertId);
    QJsonArray getActiveAlerts();
    QJsonArray getAlertHistory(const QString& category = QString());
    
    // Alert configuration
    void setAlertThreshold(const QString& metric, double threshold);
    void enableAlertCategory(const QString& category, bool enabled);
    void setNotificationSettings(const QJsonObject& settings);
    
    // Proactive monitoring
    void startProactiveMonitoring();
    void stopProactiveMonitoring();
    void checkSystemHealth();
    void monitorPerformance();
    
    // Alert escalation
    void escalateAlert(const QString& alertId, const QString& reason);
    void assignAlert(const QString& alertId, const QString& assignee);
    void setAlertSLA(const QString& alertId, const QJsonObject& sla);

public slots:
    void onPerformanceAnomaly(const QString& metric, double currentValue, double threshold);
    void onResourceExhaustion(const QString& resourceType, double usage);
    void onErrorPatternDetected(const QString& errorType, int frequency);
    void onBuildFailure(const QString& buildId, const QString& error);
    void onSecurityIssue(const QString& severity, const QString& description);
    void onDependencyUpdate(const QString& package, const QString& version);
    void onConfigurationDrift(const QString& setting, const QString& expected, const QString& actual);

signals:
    void alertCreated(const QString& alertId, const QJsonObject& alert);
    void alertResolved(const QString& alertId);
    void alertEscalated(const QString& alertId, const QString& level);
    void alertAssigned(const QString& alertId, const QString& assignee);
    void proactiveAlert(const QString& type, const QString& message);
    void alertSummary(const QJsonObject& summary);
    void notificationSent(const QString& alertId, const QString& channel);

private:
    // Alert processing
    QString generateAlertId();
    QJsonObject categorizeAlert(const QString& type, const QString& message);
    int calculatePriority(const QJsonObject& alert);
    void sendNotification(const QJsonObject& alert, const QString& channel);
    
    // Monitoring helpers
    void checkMemoryUsage();
    void checkCPUUsage();
    void checkDiskSpace();
    void checkNetworkConnectivity();
    void checkServiceHealth();
    
    // Alert deduplication
    bool isDuplicateAlert(const QJsonObject& alert);
    void groupSimilarAlerts();
    
    // Data structures
    struct Alert {
        QString id;
        QString type;
        QString category;
        QString message;
        QString status; // active, resolved, dismissed, escalated
        int priority;
        QString createdAt;
        QString resolvedAt;
        QString assignedTo;
        QJsonObject context;
        QStringList relatedAlerts;
    };
    
    QMap<QString, Alert> m_activeAlerts;
    QMap<QString, Alert> m_alertHistory;
    QSystemTrayIcon* m_trayIcon;
    
    // Alert configuration
    struct AlertConfig {
        QMap<QString, double> thresholds;
        QMap<QString, bool> enabledCategories;
        QJsonObject notificationSettings;
        QJsonObject escalationRules;
        int maxAlerts = 100;
    };
    
    AlertConfig m_config;
    
    // Monitoring state
    QTimer* m_monitoringTimer;
    QTimer* m_healthCheckTimer;
    bool m_proactiveMonitoringEnabled = false;
    
    // Statistics
    struct AlertStats {
        int totalAlerts = 0;
        int resolvedAlerts = 0;
        int escalatedAlerts = 0;
        double averageResolutionTime = 0.0;
        QMap<QString, int> alertsByCategory;
    };
    
    AlertStats m_stats;
};
