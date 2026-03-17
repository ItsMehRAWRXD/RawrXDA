#ifndef METRICS_COLLECTOR_HPP
#define METRICS_COLLECTOR_HPP

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QMap>
#include <QDateTime>
#include <memory>

/**
 * @class MetricsCollector
 * @brief Collects and aggregates application metrics for monitoring and analysis
 * 
 * This class provides a high-level interface for collecting various application metrics
 * including performance metrics, resource usage, and business metrics.
 */
class MetricsCollector : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     */
    explicit MetricsCollector(QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~MetricsCollector() override = default;

    /**
     * @brief Record a gauge metric (instantaneous value)
     * @param name Metric name
     * @param value The metric value
     * @param tags Optional key-value tags for categorization
     */
    void recordGaugeMetric(const QString& name, double value, const QMap<QString, QString>& tags = {});

    /**
     * @brief Record a counter metric (incrementing value)
     * @param name Metric name
     * @param value The increment value
     * @param tags Optional key-value tags for categorization
     */
    void recordCounterMetric(const QString& name, double value, const QMap<QString, QString>& tags = {});

    /**
     * @brief Record a histogram metric (distribution of values)
     * @param name Metric name
     * @param value The value to add to the histogram
     * @param tags Optional key-value tags for categorization
     */
    void recordHistogramMetric(const QString& name, double value, const QMap<QString, QString>& tags = {});

    /**
     * @brief Record a latency metric
     * @param operationName Name of the operation
     * @param durationMs Duration in milliseconds
     * @param success Whether the operation succeeded
     */
    void recordLatency(const QString& operationName, qint64 durationMs, bool success = true);

    /**
     * @brief Record memory usage
     * @param allocatedBytes Bytes currently allocated
     * @param peakBytes Peak bytes allocated
     */
    void recordMemoryUsage(quint64 allocatedBytes, quint64 peakBytes);

    /**
     * @brief Record CPU usage percentage
     * @param cpuPercent CPU usage as percentage (0-100)
     */
    void recordCpuUsage(double cpuPercent);

    /**
     * @brief Get all collected metrics as JSON
     * @return JSON object containing all metrics
     */
    QJsonObject getMetricsAsJson() const;

    /**
     * @brief Reset all collected metrics
     */
    void resetMetrics();

    /**
     * @brief Get a summary of current metrics
     * @return JSON object with summary statistics
     */
    QJsonObject getSummary() const;

signals:
    /**
     * @brief Emitted when metrics exceed configured thresholds
     * @param alertName Name of the alert
     * @param value Current metric value
     */
    void metricsThresholdExceeded(const QString& alertName, double value);

    /**
     * @brief Emitted when metrics are updated
     * @param metrics The updated metrics
     */
    void metricsUpdated(const QJsonObject& metrics);

private:
    /// Internal map of collected metrics
    QMap<QString, QJsonObject> m_metrics;
    
    /// Map of metric timestamps for latency calculations
    QMap<QString, QDateTime> m_metricTimestamps;
    
    /// Mutex for thread-safe access
    mutable std::mutex m_mutex;
};

#endif // METRICS_COLLECTOR_HPP
