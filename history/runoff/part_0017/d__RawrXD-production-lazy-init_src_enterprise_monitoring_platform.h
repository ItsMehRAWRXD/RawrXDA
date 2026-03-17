// SKIP_AUTOGEN
// Enterprise-Grade Monitoring, Analytics & Observability Platform
// Distributed tracing, metrics, alerting, and SLA tracking
#pragma once

#include <QString>
#include <QVector>
#include <QMap>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Agentic {

// ========== OBSERVABILITY STRUCTURES ==========

struct Span {
    QString traceId;
    QString spanId;
    QString parentSpanId;
    QString operationName;
    QDateTime startTime;
    QDateTime endTime;
    qint64 durationMs;
    QString serviceName;
    QMap<QString, QString> tags;
    QVector<QString> logs;
    QString status; // "ok", "error", "timeout"
};

struct Trace {
    QString traceId;
    QVector<Span> spans;
    QDateTime startTime;
    QDateTime endTime;
    qint64 totalDurationMs;
    QString rootOperation;
    int spanCount;
    bool hasErrors;
};

struct Metric {
    QString metricName;
    QString metricType; // "gauge", "counter", "histogram", "summary"
    double value;
    QMap<QString, QString> labels;
    QDateTime timestamp;
    QString unit; // "ms", "bytes", "count", etc.
};

struct Alert {
    QString alertId;
    QString alertName;
    QString severity; // "info", "warning", "critical"
    QString condition; // PromQL/LogQL query
    double threshold;
    QString message;
    QVector<QString> recipients;
    QDateTime createdAt;
    bool enabled;
};

struct AlertEvent {
    QString alertId;
    QString status; // "firing", "resolved"
    QDateTime timestamp;
    QString value;
    QVector<QString> affectedServices;
    QString description;
};

struct SLAObjective {
    QString objectiveId;
    QString serviceName;
    QString metric; // "availability", "latency", "error_rate"
    double targetValue;
    QString period; // "daily", "weekly", "monthly"
    double allowedErrorBudget;
    QDateTime startDate;
    QDateTime endDate;
};

struct SLAReport {
    QString serviceName;
    QMap<QString, double> sloStatus; // SLO name -> achievement %
    double overallCompliance;
    double remainingErrorBudget;
    QVector<QString> breaches;
    QDateTime reportDate;
};

struct BusinessMetric {
    QString metricName;
    QString metricType; // "revenue", "user_engagement", "conversion_rate", "customer_satisfaction"
    double value;
    double targetValue;
    double trend; // percentage change
    QDateTime timestamp;
    QString dimension; // "by_region", "by_product", etc.
};

struct PerformanceMetric {
    QString operationName;
    double p50Latency;
    double p95Latency;
    double p99Latency;
    double p999Latency;
    double avgLatency;
    double maxLatency;
    int throughput; // operations per second
    int errorCount;
    double errorRate;
};

struct LogEntry {
    QString serviceName;
    QString level; // "DEBUG", "INFO", "WARNING", "ERROR", "FATAL"
    QString message;
    QMap<QString, QString> structuredData;
    QString traceId;
    QString spanId;
    QDateTime timestamp;
    QString source; // file:line
};

// ========== DISTRIBUTED TRACING ==========

class DistributedTracingCollector : public QObject {
    Q_OBJECT

public:
    explicit DistributedTracingCollector(QObject* parent = nullptr);
    ~DistributedTracingCollector();

    // Span Management
    void startSpan(const QString& operationName, const QString& serviceName);
    void updateSpanTag(const QString& spanId, const QString& tagName, const QString& tagValue);
    void addSpanLog(const QString& spanId, const QString& logMessage);
    void endSpan(const QString& spanId, const QString& status);

    // Span Querying
    Span getSpan(const QString& spanId);
    QVector<Span> getSpansInTrace(const QString& traceId);
    Trace getTrace(const QString& traceId);

    // Trace Analysis
    Trace findTraceByOperation(const QString& operationName, const QDateTime& startTime, const QDateTime& endTime);
    QVector<Trace> findSlowTraces(qint64 minDurationMs);
    QVector<Trace> findErrorTraces(const QString& serviceName);

    // Trace Sampling
    bool shouldSampleTrace(const QString& traceId);
    void setSamplingRate(double rate);
    void setAdaptiveSampling(bool enabled);

    // Export & Visualization
    QString exportTraceAsJson(const QString& traceId);
    QString generateTraceVisualization(const QString& traceId);
    QVector<Trace> getTracesForService(const QString& serviceName, int limit = 100);

signals:
    void spanStarted(Span span);
    void spanEnded(Span span);
    void traceCollected(Trace trace);

private:
    QMap<QString, Trace> m_traces;
    QMap<QString, Span> m_activeSpans;
    double m_samplingRate = 0.1;
    bool m_adaptiveSampling = false;

    Trace buildTraceFromSpans(const QVector<Span>& spans);
};

// ========== METRICS COLLECTION & AGGREGATION ==========

class MetricsCollector : public QObject {
    Q_OBJECT

public:
    explicit MetricsCollector(QObject* parent = nullptr);
    ~MetricsCollector();

    // Metric Recording
    void recordGauge(const QString& metricName, double value, const QMap<QString, QString>& labels = {});
    void incrementCounter(const QString& metricName, const QMap<QString, QString>& labels = {});
    void recordHistogram(const QString& metricName, double value, const QMap<QString, QString>& labels = {});
    void recordSummary(const QString& metricName, double value, const QMap<QString, QString>& labels = {});

    // Metric Querying
    double getMetricValue(const QString& metricName);
    QVector<Metric> getMetricsByLabel(const QString& labelName, const QString& labelValue);
    QVector<Metric> getAllMetrics();

    // Time Series
    QVector<Metric> getMetricTimeSeries(const QString& metricName, const QDateTime& start, const QDateTime& end);
    QVector<double> getMetricValues(const QString& metricName, const QDateTime& start, const QDateTime& end);

    // Aggregation
    double calculateAverage(const QString& metricName, const QDateTime& start, const QDateTime& end);
    double calculateMax(const QString& metricName, const QDateTime& start, const QDateTime& end);
    double calculateMin(const QString& metricName, const QDateTime& start, const QDateTime& end);
    double calculatePercentile(const QString& metricName, double percentile, const QDateTime& start, const QDateTime& end);

    // Custom Metrics
    void registerCustomMetric(const QString& metricName, const QString& metricType, const QString& unit);
    void recordBusinessMetric(const BusinessMetric& metric);

signals:
    void metricRecorded(Metric metric);
    void anomalyDetected(QString metricName, double anomalyScore);

private:
    QMap<QString, QVector<Metric>> m_metricsHistory;
    QMap<QString, QString> m_customMetricTypes;
    int m_maxHistorySize = 100000;

    void pruneOldMetrics();
};

// ========== LOGGING SYSTEM ==========

class StructuredLogger : public QObject {
    Q_OBJECT

public:
    explicit StructuredLogger(const QString& serviceName, QObject* parent = nullptr);
    ~StructuredLogger();

    // Logging
    void debug(const QString& message, const QMap<QString, QString>& data = {});
    void info(const QString& message, const QMap<QString, QString>& data = {});
    void warning(const QString& message, const QMap<QString, QString>& data = {});
    void error(const QString& message, const QMap<QString, QString>& data = {});
    void fatal(const QString& message, const QMap<QString, QString>& data = {});

    // Contextual Logging
    void setTraceContext(const QString& traceId, const QString& spanId);
    void clearTraceContext();
    void attachContext(const QString& key, const QString& value);

    // Log Querying
    QVector<LogEntry> queryLogs(const QString& query, const QDateTime& start, const QDateTime& end);
    QVector<LogEntry> getLogsForTrace(const QString& traceId);
    QVector<LogEntry> getErrorLogs(const QString& serviceName, int limit = 100);

    // Log Aggregation
    QJsonObject analyzeLogPattern(const QString& pattern, const QDateTime& start, const QDateTime& end);
    int countLogsByLevel(const QString& level, const QDateTime& start, const QDateTime& end);

signals:
    void logEmitted(LogEntry entry);
    void errorLogDetected(LogEntry entry);

private:
    QString m_serviceName;
    QString m_currentTraceId;
    QString m_currentSpanId;
    QMap<QString, QString> m_contextData;
    QVector<LogEntry> m_logBuffer;

    LogEntry createLogEntry(const QString& level, const QString& message, const QMap<QString, QString>& data);
    void flushLogs();
};

// ========== ALERTING SYSTEM ==========

class AlertingEngine : public QObject {
    Q_OBJECT

public:
    explicit AlertingEngine(QObject* parent = nullptr);
    ~AlertingEngine();

    // Alert Configuration
    bool createAlert(const Alert& alert);
    bool updateAlert(const Alert& alert);
    bool deleteAlert(const QString& alertId);
    Alert getAlert(const QString& alertId);
    QVector<Alert> getAllAlerts();

    // Alert Conditions
    bool evaluateAlertCondition(const Alert& alert);
    bool triggerAlert(const QString& alertId);
    bool resolveAlert(const QString& alertId);

    // Alert Routing
    bool configureAlertRoute(const QString& alertName, const QString& receiver);
    bool addAlertRecipient(const QString& alertId, const QString& recipient);
    bool removeAlertRecipient(const QString& alertId, const QString& recipient);

    // Alert History
    QVector<AlertEvent> getAlertHistory(const QString& alertId);
    QVector<AlertEvent> getActiveAlerts();
    int getAlertCount(const QString& severity);

    // Alert Suppression
    bool suppressAlert(const QString& alertId, qint64 durationSeconds);
    bool acknowledgeAlert(const QString& alertEventId);

    // Notification Channels
    bool configureEmailNotification(const QString& address);
    bool configureSlackNotification(const QString& webhookUrl);
    bool configurePagerDutyNotification(const QString& integrationKey);
    bool configureSMSNotification(const QString& phoneNumber);

signals:
    void alertFired(Alert alert);
    void alertResolved(QString alertId);
    void alertAcknowledged(QString alertId);
    void notificationSent(QString alertId, QString channel);

private:
    QMap<QString, Alert> m_alerts;
    QMap<QString, QVector<AlertEvent>> m_alertHistory;
    QMap<QString, QDateTime> m_suppressedUntil;

    void checkAllAlerts();
    void sendNotification(const Alert& alert, const QString& channel);
};

// ========== SLA TRACKING ==========

class SLATracker : public QObject {
    Q_OBJECT

public:
    explicit SLATracker(QObject* parent = nullptr);
    ~SLATracker();

    // SLO Management
    bool createSLO(const SLAObjective& slo);
    bool updateSLO(const SLAObjective& slo);
    bool deleteSLO(const QString& objectiveId);
    SLAObjective getSLO(const QString& objectiveId);
    QVector<SLAObjective> getSLOsByService(const QString& serviceName);

    // SLA Calculation
    double calculateServiceAvailability(const QString& serviceName, const QDateTime& start, const QDateTime& end);
    double calculateErrorBudgetUsed(const QString& objectiveId);
    double calculateRemainingErrorBudget(const QString& objectiveId);

    // SLO Compliance
    bool isSLOCompliant(const QString& objectiveId);
    int getBreachCount(const QString& objectiveId, const QDateTime& start, const QDateTime& end);

    // SLA Reports
    SLAReport generateSLAReport(const QString& serviceName);
    QString generateSLAReportHTML(const QString& serviceName);
    QJsonObject generateSLAJSON(const QString& serviceName);

    // Alerts for SLO
    bool createSLOAlert(const QString& objectiveId, double thresholdPercent);
    QVector<Alert> getSLOAlerts(const QString& serviceName);

signals:
    void sloCreated(SLAObjective slo);
    void sloBreached(QString objectiveId, double actualValue);
    void reportGenerated(SLAReport report);

private:
    QMap<QString, SLAObjective> m_slos;
    QMap<QString, double> m_sloCompliance;

    double calculateMetricCompliance(const SLAObjective& slo);
};

// ========== BUSINESS INTELLIGENCE ==========

class BusinessIntelligence : public QObject {
    Q_OBJECT

public:
    explicit BusinessIntelligence(QObject* parent = nullptr);
    ~BusinessIntelligence();

    // Business Metric Tracking
    void recordBusinessMetric(const BusinessMetric& metric);
    BusinessMetric getBusinessMetric(const QString& metricName);
    QVector<BusinessMetric> getAllBusinessMetrics();

    // Analytics
    double calculateMetricTrend(const QString& metricName, const QDateTime& start, const QDateTime& end);
    QVector<BusinessMetric> getMetricsForDimension(const QString& dimension, const QString& dimensionValue);
    
    // Forecasting
    double forecastMetricValue(const QString& metricName, const QDateTime& futureDate);
    QVector<double> forecastTimeSeries(const QString& metricName, int periods);

    // Dashboards
    QString generateExecutiveDashboard();
    QString generateProductDashboard();
    QString generateFinancialDashboard();

    // Reporting
    QJsonObject generateBusinessReport(const QDateTime& start, const QDateTime& end);
    QString generateInsightsSummary();

signals:
    void metricRecorded(BusinessMetric metric);
    void insightDiscovered(QString insight);

private:
    QMap<QString, QVector<BusinessMetric>> m_metrics;
};

// ========== MONITORING COORDINATOR ==========

class MonitoringCoordinator : public QObject {
    Q_OBJECT

public:
    explicit MonitoringCoordinator(QObject* parent = nullptr);
    ~MonitoringCoordinator();

    void initialize(const QString& environment);

    // Unified Observability
    QString startOperation(const QString& operationName, const QString& serviceName);
    void recordOperationMetric(const QString& operationId, const QString& metricName, double value);
    void endOperation(const QString& operationId, const QString& status);

    // Health Checks
    bool performHealthCheck(const QString& serviceName);
    QJsonObject getSystemHealth();
    QString getHealthStatus(const QString& serviceName);

    // Dashboards
    QString generateUnifiedDashboard();
    QString generateServiceDashboard(const QString& serviceName);
    QString generateBusinessDashboard();

    // Reports
    QString generateDailyReport();
    QString generateWeeklyReport();
    QString generateMonthlyReport();

    // Anomaly Detection
    QVector<QString> detectAnomalies();
    void configureAnomalyDetection(bool enabled, double sensitivity);

signals:
    void operationStarted(QString operationId);
    void operationCompleted(QString operationId);
    void anomalyDetected(QString anomalyDescription);
    void healthStatusChanged(QString serviceName, QString status);
    void reportGenerated(QString reportType);

private:
    std::unique_ptr<DistributedTracingCollector> m_tracer;
    std::unique_ptr<MetricsCollector> m_metricsCollector;
    std::unique_ptr<StructuredLogger> m_logger;
    std::unique_ptr<AlertingEngine> m_alerting;
    std::unique_ptr<SLATracker> m_slaTracker;
    std::unique_ptr<BusinessIntelligence> m_businessIntelligence;

    QString m_environment;
    QMap<QString, QDateTime> m_operationStartTimes;
};

// ========== UTILITIES ==========

class MonitoringUtils {
public:
    static QString calculateHealthStatus(const QJsonObject& metrics);
    static QVector<QString> detectPerformanceBottlenecks(const QVector<PerformanceMetric>& metrics);
    static QString generateInsightFromMetrics(const QVector<Metric>& metrics);
    static double calculateServiceReliability(const QVector<AlertEvent>& events);
};

} // namespace Agentic
} // namespace RawrXD