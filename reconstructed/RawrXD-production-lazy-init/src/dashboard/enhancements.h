/**
 * @file dashboard_enhancements.h
 * @brief Enhanced dashboard with real-time alerting, metrics, and monitoring
 */

#ifndef DASHBOARD_ENHANCEMENTS_H
#define DASHBOARD_ENHANCEMENTS_H

#include <QString>
#include <QObject>
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <vector>

// Forward declarations
class AlertManagerWidget;
class MetricsVisualizationWidget;
class ResourceMonitorWidget;
class FirewallRuleWidget;
class ConversationSessionWidget;
class SecurityAuditWidget;
class PerformanceGraphWidget;

/**
 * @class DashboardMetricsProvider
 * @brief Aggregates metrics from all monitoring systems
 */
class DashboardMetricsProvider : public QObject
{
    Q_OBJECT
    
public:
    struct SystemMetrics {
        // CPU and memory
        double cpuUsage = 0.0;          // 0-100%
        double memoryUsage = 0.0;       // 0-100%
        uint64_t memoryBytesUsed = 0;
        uint64_t memoryBytesAvailable = 0;
        
        // Network
        uint64_t networkBytesIn = 0;
        uint64_t networkBytesOut = 0;
        uint64_t activeConnections = 0;
        
        // Alerts
        int activeAlerts = 0;
        int criticalAlerts = 0;
        int suppressedAlerts = 0;
        
        // Agent execution
        int activeAgents = 0;
        int completedTasks = 0;
        int failedTasks = 0;
        double averageTaskDuration = 0.0;  // milliseconds
        
        // Metrics drop rate
        double metricsDropRate = 0.0;   // 0-100%
        double samplingRate = 0.0;      // 0-100%
        
        // Security
        int securityVulnerabilities = 0;
        double securityRiskScore = 0.0;
    };
    
    explicit DashboardMetricsProvider(QObject* parent = nullptr);
    
    /**
     * Get current system metrics
     */
    SystemMetrics getMetrics() const;
    
    /**
     * Get metrics history (last N samples)
     */
    std::vector<SystemMetrics> getMetricsHistory(int samples = 60) const;
    
    /**
     * Get metric statistics
     */
    struct MetricStats {
        double minimum = 0.0;
        double maximum = 0.0;
        double average = 0.0;
        double stdDev = 0.0;
    };
    MetricStats getMetricStats(const QString& metric) const;
    
    /**
     * Reset metrics
     */
    void resetMetrics();
    
signals:
    /**
     * Emitted when metrics are updated
     */
    void metricsUpdated(const SystemMetrics& metrics);
    
    /**
     * Emitted when alert count changes
     */
    void alertCountChanged(int activeAlerts, int critical);
    
    /**
     * Emitted on metric threshold crossing
     */
    void thresholdCrossed(const QString& metric, double value);
    
private:
    mutable QMutex m_mutex;
    SystemMetrics m_currentMetrics;
    std::vector<SystemMetrics> m_history;
    
    const int MAX_HISTORY_SIZE = 3600;  // 1 hour at 1 sample/sec
};

/**
 * @class AlertManagerWidget
 * @brief Real-time alert display and management
 */
class AlertManagerWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit AlertManagerWidget(QWidget* parent = nullptr);
    
    /**
     * Add alert to display
     */
    void addAlert(const QString& alertName, const QString& message, int severity);
    
    /**
     * Remove alert
     */
    void removeAlert(const QString& alertId);
    
    /**
     * Clear all alerts
     */
    void clearAlerts();
    
    /**
     * Get active alert count
     */
    int getActiveAlertCount() const;
    
    /**
     * Acknowledge alert
     */
    void acknowledgeAlert(const QString& alertId);
    
    /**
     * Dismiss alert
     */
    void dismissAlert(const QString& alertId);
    
    /**
     * Get suppressed alert rules
     */
    QJsonArray getSuppressionRules() const;
    
    /**
     * Add suppression rule
     */
    void addSuppressionRule(const QString& alertPattern, int durationSeconds);
    
    /**
     * Remove suppression rule
     */
    void removeSuppressionRule(const QString& ruleId);
    
signals:
    void alertAcknowledged(const QString& alertId);
    void alertDismissed(const QString& alertId);
    void alertCountChanged(int count);
    
private:
    struct AlertItem {
        QString id;
        QString name;
        QString message;
        int severity;  // 0=info, 1=warning, 2=critical
        QString timestamp;
        bool acknowledged = false;
    };
    
    std::vector<AlertItem> m_alerts;
    mutable QMutex m_mutex;
    
    void updateUI();
};

/**
 * @class MetricsVisualizationWidget
 * @brief Real-time metrics visualization (graphs, gauges)
 */
class MetricsVisualizationWidget : public QWidget
{
    Q_OBJECT
    
public:
    enum ChartType {
        LINE_CHART,
        BAR_CHART,
        GAUGE_CHART,
        AREA_CHART
    };
    
    explicit MetricsVisualizationWidget(QWidget* parent = nullptr);
    
    /**
     * Set metric to visualize
     */
    void setMetric(const QString& metricName, ChartType type = LINE_CHART);
    
    /**
     * Update metric value
     */
    void updateMetricValue(const QString& metricName, double value);
    
    /**
     * Set threshold values for alerts
     */
    void setThresholds(double warning = 70.0, double critical = 90.0);
    
    /**
     * Clear visualization
     */
    void clearData();
    
signals:
    void thresholdExceeded(const QString& metric, double value);
    
private:
    struct MetricData {
        QString name;
        ChartType type;
        std::vector<double> values;
        double warningThreshold = 70.0;
        double criticalThreshold = 90.0;
    };
    
    std::vector<MetricData> m_metrics;
    
    void drawChart();
};

/**
 * @class ResourceMonitorWidget
 * @brief System resource monitoring (CPU, memory, network, process limits)
 */
class ResourceMonitorWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit ResourceMonitorWidget(QWidget* parent = nullptr);
    
    /**
     * Update resource display
     */
    void updateResources(double cpuUsage, double memoryUsage, uint64_t networkBytes);
    
    /**
     * Get resource limits
     */
    struct ResourceLimits {
        double cpuLimit = 100.0;
        double memoryLimitMB = 0;
        uint64_t networkLimitBytesPerSec = 0;
        int processLimit = 0;
    };
    ResourceLimits getLimits() const;
    
    /**
     * Set resource limits
     */
    void setLimits(const ResourceLimits& limits);
    
    /**
     * Get enforcement status
     */
    bool isEnforcementEnabled() const;
    
    /**
     * Enable/disable enforcement
     */
    void setEnforcementEnabled(bool enabled);
    
signals:
    void resourceLimitReached(const QString& resource, double value);
    void enforcementStatusChanged(bool enabled);
    
private:
    ResourceLimits m_limits;
    bool m_enforcementEnabled = false;
    
    void updateLimitIndicators();
};

/**
 * @class FirewallRuleWidget
 * @brief Firewall rule management UI
 */
class FirewallRuleWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit FirewallRuleWidget(QWidget* parent = nullptr);
    
    /**
     * Add rule to list
     */
    void addRule(const QString& ruleId, const QString& ruleName, 
                const QString& srcIP, int srcPort, const QString& dstIP, int dstPort,
                const QString& action);
    
    /**
     * Remove rule
     */
    void removeRule(const QString& ruleId);
    
    /**
     * Update rule
     */
    void updateRule(const QString& ruleId, const QString& ruleName, bool enabled);
    
    /**
     * Get all rules
     */
    QJsonArray getAllRules() const;
    
    /**
     * Create new rule (shows dialog)
     */
    void createNewRule();
    
    /**
     * Delete selected rule
     */
    void deleteSelectedRule();
    
    /**
     * Get block statistics
     */
    struct BlockStats {
        uint64_t blockedConnections = 0;
        uint64_t blockedBytes = 0;
        int activeRules = 0;
    };
    BlockStats getBlockStats() const;
    
signals:
    void ruleAdded(const QJsonObject& rule);
    void ruleRemoved(const QString& ruleId);
    void ruleUpdated(const QString& ruleId, const QJsonObject& rule);
    void blockStatsChanged(const BlockStats& stats);
    
private:
    struct Rule {
        QString id;
        QString name;
        QString srcIP;
        int srcPort = 0;
        QString dstIP;
        int dstPort = 0;
        QString action;  // "allow" or "block"
        bool enabled = true;
    };
    
    std::vector<Rule> m_rules;
    BlockStats m_blockStats;
};

/**
 * @class ConversationSessionWidget
 * @brief Conversation session management and history
 */
class ConversationSessionWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit ConversationSessionWidget(QWidget* parent = nullptr);
    
    /**
     * Add session to list
     */
    void addSession(const QString& sessionId, const QString& title, int messageCount, int tokenCount);
    
    /**
     * Remove session
     */
    void removeSession(const QString& sessionId);
    
    /**
     * Update session info
     */
    void updateSession(const QString& sessionId, int messageCount, int tokenCount);
    
    /**
     * Load session
     */
    void loadSession(const QString& sessionId);
    
    /**
     * Display message in current session
     */
    void displayMessage(const QString& role, const QString& content);
    
    /**
     * Get session statistics
     */
    struct SessionStats {
        int totalSessions = 0;
        int activeSessions = 0;
        uint64_t totalTokensUsed = 0;
        uint64_t totalMessages = 0;
    };
    SessionStats getStats() const;
    
signals:
    void sessionLoaded(const QString& sessionId);
    void sessionDeleted(const QString& sessionId);
    void sessionCreated(const QString& title);
    
private:
    struct SessionInfo {
        QString id;
        QString title;
        int messageCount = 0;
        int tokenCount = 0;
    };
    
    std::vector<SessionInfo> m_sessions;
    QString m_currentSessionId;
};

/**
 * @class SecurityAuditWidget
 * @brief Security audit results and vulnerability display
 */
class SecurityAuditWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit SecurityAuditWidget(QWidget* parent = nullptr);
    
    /**
     * Display audit results
     */
    void displayAuditResults(const QJsonObject& results);
    
    /**
     * Get risk level display (0-1)
     */
    double getRiskLevel() const { return m_riskScore; }
    
    /**
     * Get vulnerability count by severity
     */
    struct VulnerabilityCount {
        int critical = 0;
        int high = 0;
        int medium = 0;
        int low = 0;
    };
    VulnerabilityCount getVulnerabilityCount() const;
    
    /**
     * Export report
     */
    QString generateReport() const;
    
    /**
     * Run security audit
     */
    void runSecurityAudit();
    
signals:
    void auditStarted();
    void auditCompleted();
    void vulnerabilityFound(const QString& vulnerability);
    void riskScoreChanged(double score);
    
private:
    double m_riskScore = 0.0;
    QJsonObject m_lastAuditResults;
    VulnerabilityCount m_vulnCount;
};

/**
 * @class PerformanceGraphWidget
 * @brief Performance graphs for latency, throughput, error rates
 */
class PerformanceGraphWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit PerformanceGraphWidget(QWidget* parent = nullptr);
    
    /**
     * Add performance sample
     */
    void addSample(const QString& metric, double latencyMs, uint64_t throughput, double errorRate);
    
    /**
     * Get performance summary
     */
    struct PerformanceSummary {
        double avgLatencyMs = 0.0;
        double p95LatencyMs = 0.0;
        double p99LatencyMs = 0.0;
        double minLatencyMs = 0.0;
        double maxLatencyMs = 0.0;
        uint64_t totalThroughput = 0;
        double avgErrorRate = 0.0;
    };
    PerformanceSummary getSummary() const;
    
    /**
     * Clear data
     */
    void clearData();
    
signals:
    void performanceThresholdExceeded(const QString& metric, double value);
    
private:
    struct PerformanceSample {
        QString metric;
        double latencyMs = 0.0;
        uint64_t throughput = 0;
        double errorRate = 0.0;
        QString timestamp;
    };
    
    std::vector<PerformanceSample> m_samples;
    const int MAX_SAMPLES = 1000;
};

/**
 * @class EnhancedDashboard
 * @brief Main dashboard combining all monitoring widgets
 */
class EnhancedDashboard : public QWidget
{
    Q_OBJECT
    
public:
    explicit EnhancedDashboard(QWidget* parent = nullptr);
    ~EnhancedDashboard();
    
    /**
     * Initialize dashboard with data sources
     */
    void initialize();
    
    /**
     * Get alert manager widget
     */
    AlertManagerWidget* getAlertWidget() { return m_alertWidget.get(); }
    
    /**
     * Get metrics visualization widget
     */
    MetricsVisualizationWidget* getMetricsWidget() { return m_metricsWidget.get(); }
    
    /**
     * Get resource monitor widget
     */
    ResourceMonitorWidget* getResourceWidget() { return m_resourceWidget.get(); }
    
    /**
     * Get firewall rule widget
     */
    FirewallRuleWidget* getFirewallWidget() { return m_firewallWidget.get(); }
    
    /**
     * Get conversation session widget
     */
    ConversationSessionWidget* getConversationWidget() { return m_conversationWidget.get(); }
    
    /**
     * Get security audit widget
     */
    SecurityAuditWidget* getSecurityWidget() { return m_securityWidget.get(); }
    
    /**
     * Get performance graph widget
     */
    PerformanceGraphWidget* getPerformanceWidget() { return m_performanceWidget.get(); }
    
    /**
     * Export dashboard state
     */
    QJsonObject exportState() const;
    
    /**
     * Import dashboard state
     */
    bool importState(const QJsonObject& state);
    
    /**
     * Save dashboard layout
     */
    void saveLayout(const QString& filename);
    
    /**
     * Load dashboard layout
     */
    bool loadLayout(const QString& filename);
    
signals:
    void dashboardInitialized();
    void systemHealthChanged(double healthScore);
    void criticalEventOccurred(const QString& event);
    
private:
    std::unique_ptr<DashboardMetricsProvider> m_metricsProvider;
    std::unique_ptr<AlertManagerWidget> m_alertWidget;
    std::unique_ptr<MetricsVisualizationWidget> m_metricsWidget;
    std::unique_ptr<ResourceMonitorWidget> m_resourceWidget;
    std::unique_ptr<FirewallRuleWidget> m_firewallWidget;
    std::unique_ptr<ConversationSessionWidget> m_conversationWidget;
    std::unique_ptr<SecurityAuditWidget> m_securityWidget;
    std::unique_ptr<PerformanceGraphWidget> m_performanceWidget;
    
    std::unique_ptr<QTimer> m_updateTimer;
    
    double m_systemHealth = 1.0;
    
    void setupUI();
    void setupConnections();
    void onUpdateTimer();
    void calculateSystemHealth();
};

#endif // DASHBOARD_ENHANCEMENTS_H
