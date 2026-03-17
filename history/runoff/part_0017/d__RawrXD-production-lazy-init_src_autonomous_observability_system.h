#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMap>
#include <QVector>
#include <QTimer>

/**
 * @class AutonomousObservabilitySystem
 * @brief Production-grade observability for autonomous agents
 * 
 * Provides comprehensive monitoring and analysis:
 * - Structured logging at all levels
 * - Distributed tracing across operations
 * - Real-time metrics collection and aggregation
 * - Performance profiling and bottleneck detection
 * - Health status monitoring
 * - Alerting and anomaly detection
 */
class AutonomousObservabilitySystem : public QObject {
    Q_OBJECT

public:
    explicit AutonomousObservabilitySystem(QObject* parent = nullptr);
    ~AutonomousObservabilitySystem();

    // Structured logging
    void logDebug(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logInfo(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logWarning(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logError(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    void logCritical(const QString& component, const QString& message, const QJsonObject& context = QJsonObject());
    
    // Distributed tracing
    QString startTrace(const QString& operationName, const QJsonObject& metadata = QJsonObject());
    void recordTraceEvent(const QString& traceId, const QString& eventName, const QJsonObject& eventData);
    QJsonObject endTrace(const QString& traceId);
    QJsonArray getTraceHistory(const QString& operationName) const;
    
    // Metrics collection
    void recordMetric(const QString& metricName, double value, const QJsonObject& labels = QJsonObject());
    void recordHistogram(const QString& metricName, double value);
    void incrementCounter(const QString& metricName, int increment = 1);
    void recordGauge(const QString& metricName, double value);
    
    // Performance profiling
    void startProfiling(const QString& functionName);
    void endProfiling(const QString& functionName);
    QJsonObject getProfilingReport() const;
    QString identifyPerformanceBottlenecks() const;
    
    // Health monitoring
    QJsonObject getSystemHealth() const;
    QJsonObject getComponentHealth(const QString& componentName) const;
    double getHealthScore() const;
    
    // Anomaly detection
    bool detectAnomaly(const QString& metricName, double value);
    QJsonArray getDetectedAnomalies() const;
    void clearAnomalies();
    
    // Alerting
    void setAlertThreshold(const QString& metricName, double threshold, const QString& severity);
    QJsonArray getActiveAlerts() const;
    void acknowledgeAlert(const QString& alertId);
    
    // Configuration
    void setLogLevel(const QString& level);  // DEBUG, INFO, WARNING, ERROR, CRITICAL
    void enableDetailedMetrics(bool enable);
    void enableDistributedTracing(bool enable);
    void setMetricsAggregationInterval(int intervalMs);
    
    // Export and reporting
    QString exportMetricsAsPrometheus() const;
    QString exportTracesAsJSON() const;
    QString generateHealthReport() const;
    QString generatePerformanceReport() const;
    QString generateAuditLog() const;

signals:
    void logEmitted(const QString& level, const QString& component, const QString& message);
    void metricRecorded(const QString& metricName, double value);
    void traceCreated(const QString& traceId, const QString& operationName);
    void anomalyDetected(const QString& anomalyType, const QJsonObject& details);
    void alertTriggered(const QString& alertId, const QString& severity, const QString& message);
    void healthStatusChanged(const QString& newStatus, double healthScore);

private:
    // Log entry structure
    struct LogEntry {
        QDateTime timestamp;
        QString level;
        QString component;
        QString message;
        QJsonObject context;
        QString traceId;
    };
    
    // Trace structure
    struct TraceRecord {
        QString traceId;
        QString operationName;
        QDateTime startTime;
        QDateTime endTime;
        QJsonArray events;
        QJsonObject metadata;
        double durationMs = 0.0;
    };
    
    // Metric storage
    struct MetricValue {
        QString metricName;
        double value = 0.0;
        QDateTime timestamp;
        QJsonObject labels;
        QString metricType;  // gauge, counter, histogram, summary
    };
    
    // Performance profile
    struct PerformanceProfile {
        QString functionName;
        qint64 callCount = 0;
        qint64 totalDurationMs = 0;
        qint64 minDurationMs = LLONG_MAX;
        qint64 maxDurationMs = 0;
        QDateTime lastCalled;
    };
    
    // Alert structure
    struct Alert {
        QString alertId;
        QString metricName;
        double threshold = 0.0;
        QString severity;  // low, medium, high, critical
        QString message;
        QDateTime triggeredAt;
        bool acknowledged = false;
    };
    
    // Anomaly structure
    struct Anomaly {
        QString metricName;
        double expectedValue = 0.0;
        double actualValue = 0.0;
        double deviationPercent = 0.0;
        QDateTime detectedAt;
    };
    
    // Internal methods
    void processLogEntry(const LogEntry& entry);
    void aggregateMetrics();
    void checkHealthStatus();
    void detectAnomalies();
    void evaluateAlerts();
    
    void onMetricsAggregationTimer();
    void onHealthCheckTimer();
    
    // Members
    QVector<LogEntry> m_logHistory;
    QMap<QString, TraceRecord> m_activeTraces;
    QVector<TraceRecord> m_completedTraces;
    QVector<MetricValue> m_metricsBuffer;
    QMap<QString, PerformanceProfile> m_performanceProfiles;
    QMap<QString, double> m_currentMetrics;
    QVector<Alert> m_alertThresholds;
    QVector<Alert> m_activeAlerts;
    QVector<Anomaly> m_detectedAnomalies;
    
    QString m_currentLogLevel = "INFO";
    bool m_detailedMetricsEnabled = true;
    bool m_distributedTracingEnabled = true;
    int m_metricsAggregationIntervalMs = 5000;
    
    QTimer m_metricsTimer;
    QTimer m_healthCheckTimer;
    
    // Health tracking
    QString m_currentHealthStatus = "healthy";
    double m_healthScore = 1.0;
    
    // Profiling state
    QMap<QString, qint64> m_profilingStartTimes;
};
