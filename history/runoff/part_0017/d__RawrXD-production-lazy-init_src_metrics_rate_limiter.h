/**
 * @file metrics_rate_limiter.h
 * @brief Rate limiting system for high-frequency metrics to prevent monitoring overload
 * @author RawrXD Team
 * @date 2026
 * 
 * Provides:
 * - Token bucket rate limiting
 * - Per-metric and global rate limits
 * - Adaptive throttling based on system load
 * - Metrics aggregation and sampling
 * - Burst allowance for critical metrics
 */

#pragma once

#include <QString>
#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <memory>
#include <vector>

/**
 * @class TokenBucket
 * @brief Token bucket implementation for rate limiting
 */
class TokenBucket {
public:
    TokenBucket(double capacity, double refillRate);
    
    bool tryConsume(double tokens = 1.0);
    void refill();
    double getAvailableTokens() const;
    void reset();
    
private:
    double m_capacity;
    double m_refillRate;
    double m_tokens;
    QDateTime m_lastRefillTime;
    QMutex m_mutex;
};

/**
 * @class MetricsRateLimiter
 * @brief Centralized rate limiting for metrics to prevent system overload
 * 
 * Features:
 * - Per-metric rate limits with token bucket algorithm
 * - Global limit across all metrics
 * - Adaptive throttling based on system load
 * - Metrics aggregation and batching
 * - Sampling strategies (uniform, adaptive, priority-based)
 * - Burst allowance for critical metrics
 */
class MetricsRateLimiter : public QObject {
    Q_OBJECT
    
public:
    enum class SamplingStrategy {
        UNIFORM,        // Fixed sampling rate
        ADAPTIVE,       // Adjust based on system load
        PRIORITY_BASED  // Different rates for different metrics
    };
    
    explicit MetricsRateLimiter(QObject* parent = nullptr);
    ~MetricsRateLimiter();
    
    // Configuration
    void setGlobalLimit(int metricsPerSecond);
    void setMetricLimit(const QString& metricName, int perSecond);
    void setSamplingStrategy(SamplingStrategy strategy);
    void setSamplingRate(const QString& metricName, double rate);
    void setBurstAllowance(const QString& metricName, double burst);
    
    // Rate limiting decisions
    bool shouldRecord(const QString& metricName, double importance = 1.0);
    bool shouldEmit(const QString& metricName);
    
    // Metrics aggregation
    void addMetric(const QString& name, double value);
    QJsonObject getAggregatedMetrics(int windowSeconds = 60);
    
    // Monitoring state
    int getDroppedMetricsCount() const;
    int getRecordedMetricsCount() const;
    double getSystemLoad() const;
    double getThrottleRatio() const;
    
    // Adaptation
    void onSystemLoad(double cpuPercent, double memoryPercent);
    void resetCounters();
    
signals:
    void metricDropped(const QString& metricName);
    void rateLimitExceeded(const QString& metricName);
    void systemLoadHigh(double cpuPercent, double memoryPercent);
    
private slots:
    void onAggregationTimer();
    
private:
    double calculateAdaptiveRate(double systemLoad);
    void aggregateMetrics();
    
    QMap<QString, std::shared_ptr<TokenBucket>> m_metricBuckets;
    std::shared_ptr<TokenBucket> m_globalBucket;
    
    QMap<QString, double> m_samplingRates;
    QMap<QString, double> m_burstAllowances;
    QMap<QString, int> m_droppedCounts;
    QMap<QString, int> m_recordedCounts;
    
    QMap<QString, std::vector<double>> m_metricsBuffer;
    QMap<QString, QDateTime> m_lastEmitTime;
    
    SamplingStrategy m_samplingStrategy;
    int m_globalLimitPerSecond;
    double m_currentSystemLoad;
    double m_cpuPercent;
    double m_memoryPercent;
    
    int m_totalDropped;
    int m_totalRecorded;
    
    QTimer* m_aggregationTimer;
    QMutex m_mutex;
};

#endif // METRICS_RATE_LIMITER_H
