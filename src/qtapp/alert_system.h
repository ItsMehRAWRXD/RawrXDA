#pragma once
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
class AlertSystem  {

public:
    explicit AlertSystem( = nullptr);
    virtual ~AlertSystem();

    // Alert management
    std::string createAlert(const std::string& type, const std::string& message, const void*& context = void*());
    void resolveAlert(const std::string& alertId);
    void dismissAlert(const std::string& alertId);
    void* getActiveAlerts();
    void* getAlertHistory(const std::string& category = std::string());
    
    // Alert configuration
    void setAlertThreshold(const std::string& metric, double threshold);
    void enableAlertCategory(const std::string& category, bool enabled);
    void setNotificationSettings(const void*& settings);
    
    // Proactive monitoring
    void startProactiveMonitoring();
    void stopProactiveMonitoring();
    void checkSystemHealth();
    void monitorPerformance();
    
    // Alert escalation
    void escalateAlert(const std::string& alertId, const std::string& reason);
    void assignAlert(const std::string& alertId, const std::string& assignee);
    void setAlertSLA(const std::string& alertId, const void*& sla);
\npublic:\n    void onPerformanceAnomaly(const std::string& metric, double currentValue, double threshold);
    void onResourceExhaustion(const std::string& resourceType, double usage);
    void onErrorPatternDetected(const std::string& errorType, int frequency);
    void onBuildFailure(const std::string& buildId, const std::string& error);
    void onSecurityIssue(const std::string& severity, const std::string& description);
    void onDependencyUpdate(const std::string& package, const std::string& version);
    void onConfigurationDrift(const std::string& setting, const std::string& expected, const std::string& actual);
\npublic:\n    void alertCreated(const std::string& alertId, const void*& alert);
    void alertResolved(const std::string& alertId);
    void alertEscalated(const std::string& alertId, const std::string& level);
    void alertAssigned(const std::string& alertId, const std::string& assignee);
    void proactiveAlert(const std::string& type, const std::string& message);
    void alertSummary(const void*& summary);
    void notificationSent(const std::string& alertId, const std::string& channel);

private:
    // Alert processing
    std::string generateAlertId();
    void* categorizeAlert(const std::string& type, const std::string& message);
    int calculatePriority(const void*& alert);
    void sendNotification(const void*& alert, const std::string& channel);
    
    // Monitoring helpers
    void checkMemoryUsage();
    void checkCPUUsage();
    void checkDiskSpace();
    void checkNetworkConnectivity();
    void checkServiceHealth();
    
    // Alert deduplication
    bool isDuplicateAlert(const void*& alert);
    void groupSimilarAlerts();
    
    // Data structures
    struct Alert {
        std::string id;
        std::string type;
        std::string category;
        std::string message;
        std::string status; // active, resolved, dismissed, escalated
        int priority;
        std::string createdAt;
        std::string resolvedAt;
        std::string assignedTo;
        void* context;
        std::stringList relatedAlerts;
    };
    
    std::map<std::string, Alert> m_activeAlerts;
    std::map<std::string, Alert> m_alertHistory;
    QSystemTrayIcon* m_trayIcon;
    
    // Alert configuration
    struct AlertConfig {
        std::map<std::string, double> thresholds;
        std::map<std::string, bool> enabledCategories;
        void* notificationSettings;
        void* escalationRules;
        int maxAlerts = 100;
    };
    
    AlertConfig m_config;
    
    // Monitoring state
    // Timer m_monitoringTimer;
    // Timer m_healthCheckTimer;
    bool m_proactiveMonitoringEnabled = false;
    
    // Statistics
    struct AlertStats {
        int totalAlerts = 0;
        int resolvedAlerts = 0;
        int escalatedAlerts = 0;
        double averageResolutionTime = 0.0;
        std::map<std::string, int> alertsByCategory;
    };
    
    AlertStats m_stats;
};

