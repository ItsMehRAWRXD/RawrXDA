#pragma once

#include <QObject>
#include <QString>
#include <QHash>
#include <QElapsedTimer>
#include <QDateTime>
#include <QMutex>

/**
 * @brief Performance metrics collector for telemetry and monitoring
 * 
 * Features:
 * - Real-time performance tracking
 * - Token generation metrics (tokens/sec, latency)
 * - Memory usage monitoring
 * - Request/response timing
 * - Statistical aggregation (min, max, avg, p50, p95, p99)
 * - Export to JSON/CSV
 */
class MetricsCollector : public QObject {
    Q_OBJECT

public:
    struct RequestMetrics {
        qint64 requestId;
        QDateTime startTime;
        QDateTime endTime;
        qint64 durationMs;
        int tokensGenerated;
        int promptTokens;
        float tokensPerSecond;
        size_t memoryUsed;
        QString modelName;
        bool success;
        QString errorMessage;
    };

    struct AggregateMetrics {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        
        qint64 minLatencyMs = INT64_MAX;
        qint64 maxLatencyMs = 0;
        qint64 avgLatencyMs = 0;
        qint64 p50LatencyMs = 0;
        qint64 p95LatencyMs = 0;
        qint64 p99LatencyMs = 0;
        
        float minTokensPerSec = FLT_MAX;
        float maxTokensPerSec = 0;
        float avgTokensPerSec = 0;
        
        size_t peakMemoryUsage = 0;
        size_t avgMemoryUsage = 0;
        
        QDateTime firstRequest;
        QDateTime lastRequest;
    };

    static MetricsCollector& instance();
    ~MetricsCollector();

    /**
     * @brief Start tracking a request
     */
    void startRequest(qint64 requestId, const QString& modelName, int promptTokens);

    /**
     * @brief End tracking a request
     */
    void endRequest(qint64 requestId, int tokensGenerated, bool success, const QString& error = QString());

    /**
     * @brief Record token generation event
     */
    void recordToken(qint64 requestId);

    /**
     * @brief Record memory usage
     */
    void recordMemoryUsage(size_t bytes);

    /**
     * @brief Get metrics for specific request
     */
    RequestMetrics getRequestMetrics(qint64 requestId) const;

    /**
     * @brief Get aggregate metrics
     */
    AggregateMetrics getAggregateMetrics() const;

    /**
     * @brief Export metrics to JSON
     */
    QString exportToJson() const;

    /**
     * @brief Export metrics to CSV
     */
    QString exportToCsv() const;

    /**
     * @brief Reset all metrics
     */
    void reset();

    /**
     * @brief Enable/disable metrics collection
     */
    void setEnabled(bool enabled);

    /**
     * @brief Check if metrics collection is enabled
     */
    bool isEnabled() const;

signals:
    void requestStarted(qint64 requestId);
    void requestCompleted(qint64 requestId, const RequestMetrics& metrics);
    void performanceWarning(const QString& message);
    void metricsUpdated();

private:
    MetricsCollector();  // Singleton
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    void calculatePercentiles();

    mutable QMutex m_mutex;
    QHash<qint64, RequestMetrics> m_activeRequests;
    QHash<qint64, QElapsedTimer> m_timers;
    QList<RequestMetrics> m_completedRequests;
    
    size_t m_currentMemoryUsage = 0;
    bool m_enabled = true;
};
