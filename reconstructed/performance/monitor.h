// performance_monitor.h - Real-time Performance Monitoring and SLA Tracking
#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QDateTime>
#include <QElapsedTimer>

// Performance metric data point
struct MetricData {
    QString metricName;
    QString component;
    double value;
    QString unit;              // ms, bytes, percentage, etc.
    QDateTime timestamp;
    QJsonObject tags;
};

// Performance threshold
struct PerformanceThreshold {
    QString metricName;
    double warningThreshold;
    double criticalThreshold;
    QString comparison;        // "greater_than", "less_than", "equals"
    bool enabled;
};

// SLA definition
struct SLADefinition {
    QString slaId;
    QString name;
    QString component;
    QString metric;
    double targetValue;
    double acceptableValue;
    QString timeWindow;        // "1h", "24h", "7d", "30d"
    bool enabled;
};

// SLA compliance status
struct SLACompliance {
    QString slaId;
    bool isCompliant;
    double currentValue;
    double targetValue;
    double compliancePercentage;
    int violationCount;
    QDateTime lastViolation;
    QDateTime evaluatedAt;
};

// Performance snapshot
struct PerformanceSnapshot {
    QDateTime timestamp;
    double cpuUsage;           // Percentage
    qint64 memoryUsage;        // Bytes
    double diskUsage;          // Percentage
    double networkUsage;       // Bytes/sec
    int activeConnections;
    int queuedRequests;
    double averageLatency;     // ms
    int requestsPerSecond;
};

// Bottleneck detection
struct Bottleneck {
    QString bottleneckId;
    QString component;
    QString type;              // cpu, memory, io, network
    QString description;
    double severity;           // 0-10
    QDateTime detectedAt;
    QJsonObject recommendations;
};

// Performance trend
struct PerformanceTrend {
    QString metricName;
    QString trendDirection;    // "improving", "degrading", "stable"
    double changeRate;         // Percentage change
    QVector<double> recentValues;
    QString forecast;
};

// RAII Timer for automatic performance measurement
class ScopedTimer {
public:
    explicit ScopedTimer(const QString& operation,
                        const QString& component,
                        class PerformanceMonitor* monitor);
    ~ScopedTimer();

private:
    QString operation;
    QString component;
    PerformanceMonitor* monitor;
    QElapsedTimer timer;
};

class PerformanceMonitor : public QObject {
    Q_OBJECT

public:
    explicit PerformanceMonitor(QObject* parent = nullptr);
    ~PerformanceMonitor();

    // Metric recording
    void recordMetric(const QString& component,
                     const QString& operation,
                     double value,
                     const QString& unit = "ms");
    void recordLatency(const QString& component,
                      const QString& operation,
                      qint64 durationMs);
    void recordThroughput(const QString& component,
                         int requestCount,
                         qint64 timeWindowMs);
    void recordResourceUsage(double cpuPercent,
                           qint64 memoryBytes,
                           double diskPercent);
    
    // Metric retrieval
    QVector<MetricData> getMetrics(const QString& component,
                                   const QString& metricName,
                                   const QDateTime& startTime = QDateTime(),
                                   const QDateTime& endTime = QDateTime()) const;
    double getAverageLatency(const QString& component,
                            const QString& operation) const;
    double getP95Latency(const QString& component,
                        const QString& operation) const;
    double getP99Latency(const QString& component,
                        const QString& operation) const;
    
    // Performance thresholds
    void addThreshold(const PerformanceThreshold& threshold);
    void removeThreshold(const QString& metricName);
    QVector<PerformanceThreshold> getThresholds() const;
    bool checkThreshold(const QString& metricName, double value);
    QVector<QString> getThresholdViolations() const;
    
    // SLA management
    void defineSLA(const SLADefinition& sla);
    void removeSLA(const QString& slaId);
    QVector<SLADefinition> getSLAs() const;
    SLACompliance evaluateSLA(const QString& slaId);
    QVector<SLACompliance> evaluateAllSLAs();
    bool isMeetingSLA(const QString& slaId) const;
    
    // Performance snapshots
    PerformanceSnapshot captureSnapshot();
    QVector<PerformanceSnapshot> getSnapshotHistory(int lastNMinutes = 60) const;
    void enableSnapshotRecording(bool enable, int intervalSeconds = 60);
    
    // Bottleneck detection
    QVector<Bottleneck> detectBottlenecks();
    Bottleneck analyzeComponentPerformance(const QString& component);
    bool isBottleneck(const QString& component) const;
    
    // Trend analysis
    PerformanceTrend analyzeTrend(const QString& metricName,
                                 int historicalDays = 7);
    QVector<PerformanceTrend> getTrendAnalysis();
    QString predictFuturePerformance(const QString& metricName,
                                    int daysAhead = 7);
    
    // Optimization suggestions
    QJsonObject getOptimizationSuggestions();
    QVector<QString> suggestImprovements(const QString& component);
    QString recommendScaling(const QString& component);
    
    // Alerting
    void enableAlerting(bool enable);
    void setAlertThreshold(double threshold);
    void subscribeToAlerts(QObject* receiver, const char* slot);
    
    // Real-time monitoring
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    bool isMonitoring() const;
    
    // Performance reports
    QJsonObject generatePerformanceReport(const QString& component = "",
                                         int lastNDays = 7);
    QString exportMetrics(const QString& format = "json"); // json, csv, prometheus
    void exportToPrometheus(const QString& outputPath);
    
    // System resource monitoring
    double getCurrentCPUUsage() const;
    qint64 getCurrentMemoryUsage() const;
    double getCurrentDiskUsage() const;
    qint64 getNetworkThroughput() const;
    
    // Configuration
    void setMetricsRetentionDays(int days);
    void setAggregationInterval(int seconds);
    void enableAutoAggregation(bool enable);
    int getMetricsRetentionDays() const;

signals:
    void metricRecorded(const MetricData& metric);
    void thresholdViolation(const QString& metricName, double value);
    void slaViolation(const QString& slaId);
    void bottleneckDetected(const Bottleneck& bottleneck);
    void performanceDegraded(const QString& component, double degradation);
    void alertTriggered(const QString& alertMessage);

private slots:
    void onSnapshotTimerTimeout();
    void onAggregationTimerTimeout();

private:
    // Metric aggregation
    void aggregateMetrics();
    double calculateAverage(const QVector<double>& values) const;
    double calculatePercentile(const QVector<double>& values, double percentile) const;
    double calculateStandardDeviation(const QVector<double>& values) const;
    
    // Threshold checking
    void checkAllThresholds();
    bool evaluateThreshold(const PerformanceThreshold& threshold, double value) const;
    
    // SLA evaluation
    double calculateSLACompliance(const SLADefinition& sla,
                                  const QVector<MetricData>& metrics) const;
    bool isWithinSLA(const SLADefinition& sla, double currentValue) const;
    
    // Bottleneck analysis
    bool isCPUBottleneck() const;
    bool isMemoryBottleneck() const;
    bool isIOBottleneck() const;
    bool isNetworkBottleneck() const;
    
    // Trend calculations
    QString determineTrendDirection(const QVector<double>& values) const;
    double calculateChangeRate(const QVector<double>& values) const;
    QVector<double> forecastValues(const QVector<double>& historicalData, int periods) const;
    
    // Resource monitoring
    double measureCPUUsage() const;
    qint64 measureMemoryUsage() const;
    double measureDiskUsage() const;
    qint64 measureNetworkThroughput() const;
    
    // Data management
    void pruneOldMetrics();
    void cleanupExpiredData();
    
    // Data members
    QHash<QString, QVector<MetricData>> metricsByComponent;
    QHash<QString, PerformanceThreshold> thresholds;
    QHash<QString, SLADefinition> slaDefinitions;
    QHash<QString, SLACompliance> slaCompliance;
    QVector<PerformanceSnapshot> snapshotHistory;
    QVector<Bottleneck> detectedBottlenecks;
    QVector<QString> activeViolations;
    
    QTimer* snapshotTimer;
    QTimer* aggregationTimer;
    
    bool realTimeMonitoringEnabled;
    bool alertingEnabled;
    bool snapshotRecordingEnabled;
    bool autoAggregationEnabled;
    
    int metricsRetentionDays;
    int aggregationIntervalSeconds;
    int snapshotIntervalSeconds;
    double alertThreshold;
    
    // Statistics
    qint64 totalMetricsRecorded;
    qint64 totalThresholdViolations;
    qint64 totalSLAViolations;
    
    // Configuration
    static constexpr int DEFAULT_RETENTION_DAYS = 30;
    static constexpr int DEFAULT_AGGREGATION_INTERVAL = 300; // 5 minutes
    static constexpr int DEFAULT_SNAPSHOT_INTERVAL = 60; // 1 minute
};

#endif // PERFORMANCE_MONITOR_H
