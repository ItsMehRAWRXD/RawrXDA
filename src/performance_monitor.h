// performance_monitor.h - Real-time Performance Monitoring and SLA Tracking
#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QHash>
#include <QJsonObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <QPair>

struct MetricData {
    QString metricId;
    QString component;
    QString operation;
    double value;
    QString unit;
    QJsonObject tags;
    QDateTime timestamp;
};

struct PerformanceThreshold {
    QString component;
    QString metric;
    double warningThreshold;
    double criticalThreshold;
    bool isEnabled;
};

struct SLADefinition {
    QString slaId;
    QString name;
    QString component;
    QString metric;
    double targetValue;
    double acceptableValue;
    QString timeWindow; // "1h", "24h", "7d", "30d"
    bool isActive;
};

struct SLACompliance {
    QString slaId;
    bool isCompliant;
    double currentValue;
    double targetValue;
    double compliancePercentage;
    int violationCount;
    QDateTime lastViolation;
};

struct PerformanceSnapshot {
    QDateTime timestamp;
    double cpuUsage;
    double memoryUsage;
    double diskUsage;
    double networkUsage;
    int activeConnections;
    double averageLatency;
    double requestsPerSecond;
};

struct Bottleneck {
    QString bottleneckId;
    QString component;
    QString type;
    double severity;
    QString description;
    QDateTime detectedAt;
    QStringList recommendations;
};

struct TrendAnalysis {
    QString component;
    QString operation;
    QString trend;
    double changePercentage;
    double forecast;
    double confidence;
};

class PerformanceMonitor;

class ScopedTimer {
public:
    ScopedTimer(PerformanceMonitor* monitor, const QString& component, const QString& operation);
    ~ScopedTimer();

private:
    PerformanceMonitor* performanceMonitor;
    QString component;
    QString operation;
    QString timerId;
};

class PerformanceMonitor : public QObject {
    Q_OBJECT

public:
    explicit PerformanceMonitor(QObject* parent = nullptr);
    ~PerformanceMonitor();

    void recordMetric(const QString& component, const QString& operation, double value, const QString& unit = "ms");
    void recordMetricWithTags(const QString& component, const QString& operation, double value, const QString& unit, const QJsonObject& tags);
    void startTimer(const QString& timerId, const QString& component, const QString& operation);
    double stopTimer(const QString& timerId);
    ScopedTimer createScopedTimer(const QString& component, const QString& operation);

    QVector<MetricData> getMetrics(const QString& component, const QString& operation,
                                   const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime::currentDateTime()) const;
    double getAverageMetric(const QString& component, const QString& operation,
                            const QDateTime& startTime = QDateTime(), const QDateTime& endTime = QDateTime::currentDateTime()) const;
    double getP95Latency(const QString& component, const QString& operation) const;
    double getP99Latency(const QString& component, const QString& operation) const;
    double getPercentile(const QString& component, const QString& operation, double percentile) const;

    SLACompliance evaluateSLA(const QString& slaId) const;
    QVector<SLACompliance> evaluateAllSLAs() const;

    PerformanceSnapshot capturePerformanceSnapshot();
    QVector<PerformanceSnapshot> getPerformanceHistory(int minutes = 0) const;

    QVector<Bottleneck> detectBottlenecks() const;
    TrendAnalysis analyzeTrend(const QString& component, const QString& operation, int windowMinutes = 60) const;

    bool exportToPrometheus(const QString& outputPath) const;

    void enableMonitoring(bool enable);
    void setSnapshotInterval(int milliseconds);
    void setMetricsRetention(int hours);
    void clearMetrics();

signals:
    void metricRecorded(const MetricData& metric);
    void thresholdViolation(const MetricData& metric, const QString& severity);
    void snapshotCaptured(const PerformanceSnapshot& snapshot);

private:
    void setupDefaultSLAs();
    void setupDefaultThresholds();
    void checkThreshold(const MetricData& metric);
    void pruneOldMetrics(const QString& metricKey);
    QDateTime calculateTimeWindow(const QDateTime& endTime, const QString& window) const;

    double calculateUptimePercentage(const QDateTime& startTime, const QDateTime& endTime) const;
    double calculateErrorRate(const QDateTime& startTime, const QDateTime& endTime) const;

    double getCPUUsageWindows() const;
    double getMemoryUsageWindows() const;
    double getCPUUsageLinux() const;
    double getMemoryUsageLinux() const;
    double getCPUUsageMac() const;
    double getMemoryUsageMac() const;

    // Data members
    QHash<QString, QVector<MetricData>> metricHistory;
    QVector<PerformanceSnapshot> performanceHistory;
    QHash<QString, PerformanceThreshold> thresholds;
    QHash<QString, SLADefinition> slaDefinitions;
    QHash<QString, QElapsedTimer> activeTimers;
    QHash<QString, QPair<QString, QString>> timerContext;

    bool monitoringEnabled;
    int snapshotIntervalMs;
    int metricsRetentionHours;
    QTimer* snapshotTimer;
};

#endif // PERFORMANCE_MONITOR_H
