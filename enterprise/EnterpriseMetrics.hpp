#ifndef ENTERPRISE_METRICS_COLLECTOR_HPP
#define ENTERPRISE_METRICS_COLLECTOR_HPP

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QAtomicInt>
#include <QMap>
#include <QTimer>

struct ToolMetrics {
    QString toolName;
    int totalExecutions;
    int successfulExecutions;
    int failedExecutions;
    double averageExecutionTime;
    double maxExecutionTime;
    double minExecutionTime;
    QDateTime lastExecution;
};

struct MissionMetrics {
    QString missionId;
    QString missionType;
    int toolCount;
    double totalExecutionTime;
    bool success;
    QDateTime startTime;
    QDateTime endTime;
    int retryCount;
};

class EnterpriseMetricsCollector : public QObject {
    Q_OBJECT
    
public:
    static EnterpriseMetricsCollector* instance();
    
    // Tool execution metrics
    void recordToolExecution(const QString& toolName, double executionTime, bool success);
    void recordToolError(const QString& toolName, const QString& errorType);
    void recordToolTimeout(const QString& toolName, int timeoutMs);
    
    // Mission metrics
    void recordMissionStart(const QString& missionId, const QString& missionType, int toolCount);
    void recordMissionCompletion(const QString& missionId, double executionTime, bool success);
    void recordMissionFailure(const QString& missionId, const QString& error);
    void recordMissionRetry(const QString& missionId, int retryCount);
    
    // Workflow metrics
    void recordWorkflowStart(const QString& workflowId, int stepCount);
    void recordWorkflowStepCompletion(const QString& workflowId, const QString& stepId, double executionTime);
    void recordWorkflowCompletion(const QString& workflowId, double totalExecutionTime);
    
    // Enterprise reporting
    QJsonObject generateMetricsReport(const QDateTime& startTime, const QDateTime& endTime);
    QJsonObject getRealTimeMetrics();
    QJsonObject getToolMetrics(const QString& toolName);
    QJsonObject getMissionMetrics(const QString& missionId);
    
    // Export capabilities
    void exportMetricsToPrometheus();
    void exportMetricsToOpenTelemetry();
    void exportMetricsToFile(const QString& filePath);
    
    // Alert management
    void setAlertThreshold(const QString& metricType, double threshold);
    void enableRealTimeAlerts(bool enabled);
    
    // Performance analysis
    double calculateToolSuccessRate(const QString& toolName);
    double calculateMissionSuccessRate();
    double calculateAverageExecutionTime(const QString& toolName);
    QJsonArray getPerformanceTrends(int hours);
    
    // Security metrics
    void recordSecurityEvent(const QString& eventType, const QString& details);
    QJsonObject getSecurityMetrics();
    
    // Resource metrics
    void recordResourceUsage(const QString& resourceType, double usage);
    QJsonObject getResourceMetrics();
    
    // Cleanup and maintenance
    void cleanupOldMetrics(int daysToKeep);
    void compressMetricsData();
    
signals:
    void metricsUpdated(const QJsonObject& metrics);
    void alertTriggered(const QString& alertName, const QJsonObject& details);
    void performanceDegradation(const QString& metric, double currentValue, double threshold);
    void securityAlert(const QString& alertType, const QJsonObject& details);
    
private:
    explicit EnterpriseMetricsCollector(QObject *parent = nullptr);
    ~EnterpriseMetricsCollector();
    
    class Private;
    QScopedPointer<Private> d_ptr;
    
    Q_DISABLE_COPY(EnterpriseMetricsCollector)
};

#endif // ENTERPRISE_METRICS_COLLECTOR_HPP