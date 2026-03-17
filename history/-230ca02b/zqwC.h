#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <atomic>
#include <memory>

/**
 * @brief Inference performance metrics
 */
struct PerformanceMetrics {
    double tokensPerSecond;     // Primary metric
    double avgLatencyMs;
    double p50LatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    int requestCount;
    int errorCount;
    double successRate;
    
    PerformanceMetrics() : tokensPerSecond(0), avgLatencyMs(0), 
                          p50LatencyMs(0), p95LatencyMs(0), p99LatencyMs(0),
                          requestCount(0), errorCount(0), successRate(100.0) {}
};

/**
 * @brief Resource utilization metrics
 */
struct ResourceMetrics {
    double memoryUsageMb;
    double peakMemoryMb;
    double gpuUtilization;
    double gpuMemoryMb;
    int activeConnections;
    int queuedRequests;
    
    ResourceMetrics() : memoryUsageMb(0), peakMemoryMb(0), gpuUtilization(0),
                       gpuMemoryMb(0), activeConnections(0), queuedRequests(0) {}
};

/**
 * @brief Cost tracking metrics
 */
struct CostMetrics {
    double totalCost;
    double costPerToken;
    double costPerInference;
    int tokensProcessed;
    int inferenceCount;
    QString currency;
    
    CostMetrics() : totalCost(0), costPerToken(0), costPerInference(0),
                   tokensProcessed(0), inferenceCount(0), currency("USD") {}
};

/**
 * @brief Quality metrics for responses
 */
struct QualityMetrics {
    double coherenceScore;       // 0-1
    double relevanceScore;       // 0-1
    double factualityScore;      // 0-1
    double harmfulnessScore;     // 0-1 (lower is better)
    double overallQualityScore;  // 0-1
    int ratingsCount;
    
    QualityMetrics() : coherenceScore(0.8), relevanceScore(0.85),
                      factualityScore(0.8), harmfulnessScore(0.05),
                      overallQualityScore(0.8), ratingsCount(0) {}
};

/**
 * @brief Time-series data point
 */
struct TimeSeriesPoint {
    QDateTime timestamp;
    double value;
    QString label;
};

/**
 * @brief Phase 5: Analytics Dashboard
 * 
 * Real-time performance monitoring and analytics for model inference.
 * Tracks:
 * - Tokens per second and latency
 * - Resource utilization (CPU, GPU, memory)
 * - Cost tracking
 * - Quality metrics
 * - Historical trends and forecasting
 */
class Phase5AnalyticsDashboard {
public:
    explicit Phase5AnalyticsDashboard();
    ~Phase5AnalyticsDashboard();
    
    // ===== Data Collection =====
    
    /**
     * @brief Record inference request completion
     */
    void recordInference(const QString& modelId, double tokensPerSecond, 
                        double latencyMs, int tokensGenerated, bool success);
    
    /**
     * @brief Record resource usage
     */
    void recordResourceUsage(double memoryMb, double gpuUtil, int activeConn);
    
    /**
     * @brief Record cost
     */
    void recordCost(const QString& modelId, int tokensUsed, double costUsd);
    
    /**
     * @brief Record quality rating
     */
    void recordQualityRating(const QString& modelId, double coherence,
                            double relevance, double factuality);
    
    // ===== Performance Analytics =====
    
    /**
     * @brief Get overall performance metrics
     */
    PerformanceMetrics getPerformanceMetrics() const;
    
    /**
     * @brief Get metrics for specific model
     */
    PerformanceMetrics getModelPerformanceMetrics(const QString& modelId) const;
    
    /**
     * @brief Get tokens per second (primary metric)
     */
    double getCurrentTPS() const;
    
    /**
     * @brief Get average latency
     */
    double getAverageLatency() const;
    
    /**
     * @brief Get percentile latency
     */
    double getPercentileLatency(double percentile) const;  // 0-1
    
    /**
     * @brief Get success rate
     */
    double getSuccessRate() const;
    
    // ===== Resource Analytics =====
    
    /**
     * @brief Get current resource usage
     */
    ResourceMetrics getResourceMetrics() const;
    
    /**
     * @brief Get peak memory usage
     */
    double getPeakMemoryUsage() const;
    
    /**
     * @brief Get average GPU utilization
     */
    double getAverageGpuUtilization() const;
    
    // ===== Cost Analytics =====
    
    /**
     * @brief Get cost metrics
     */
    CostMetrics getCostMetrics() const;
    
    /**
     * @brief Get total cost
     */
    double getTotalCost() const;
    
    /**
     * @brief Estimate cost for tokens
     */
    double estimateCost(int tokenCount) const;
    
    /**
     * @brief Set cost per million tokens
     */
    void setCostPerMillionTokens(double costUsd);
    
    // ===== Quality Analytics =====
    
    /**
     * @brief Get quality metrics
     */
    QualityMetrics getQualityMetrics() const;
    
    /**
     * @brief Get quality metrics by model
     */
    QualityMetrics getQualityMetricsByModel(const QString& modelId) const;
    
    /**
     * @brief Get overall quality score
     */
    double getOverallQualityScore() const;
    
    // ===== Time Series Data =====
    
    /**
     * @brief Get TPS history (tokens per second over time)
     */
    QVector<TimeSeriesPoint> getTpsHistory(int lastMinutes = 60) const;
    
    /**
     * @brief Get latency history
     */
    QVector<TimeSeriesPoint> getLatencyHistory(int lastMinutes = 60) const;
    
    /**
     * @brief Get memory usage history
     */
    QVector<TimeSeriesPoint> getMemoryHistory(int lastMinutes = 60) const;
    
    /**
     * @brief Get cost history
     */
    QVector<TimeSeriesPoint> getCostHistory(int lastMinutes = 60) const;
    
    // ===== Forecasting =====
    
    /**
     * @brief Forecast TPS trend
     */
    double forecastTPS(int minutesAhead = 5) const;
    
    /**
     * @brief Forecast memory usage
     */
    double forecastMemoryUsage(int minutesAhead = 5) const;
    
    /**
     * @brief Forecast cost
     */
    double forecastCost(int minutesAhead = 5) const;
    
    // ===== Anomaly Detection =====
    
    /**
     * @brief Check for performance anomalies
     */
    QStringList detectAnomalies() const;
    
    /**
     * @brief Get anomaly score (0-1)
     */
    double getAnomalyScore() const;
    
    // ===== Reporting =====
    
    /**
     * @brief Generate comprehensive dashboard JSON
     */
    QJsonObject generateDashboardData() const;
    
    /**
     * @brief Generate daily report
     */
    QString generateDailyReport() const;
    
    /**
     * @brief Generate model comparison report
     */
    QJsonObject generateModelComparison() const;
    
    /**
     * @brief Export metrics to JSON
     */
    QString exportMetricsToJson() const;
    
    /**
     * @brief Export metrics to CSV
     */
    QString exportMetricsToCsv() const;
    
    // ===== Configuration =====
    
    /**
     * @brief Set retention period (days)
     */
    void setRetentionPeriod(int days);
    
    /**
     * @brief Set sampling interval (seconds)
     */
    void setSamplingInterval(int seconds);
    
    /**
     * @brief Enable/disable detailed tracking
     */
    void enableDetailedTracking(bool enable);
    
    /**
     * @brief Clear historical data
     */
    void clearHistoricalData();

private:
    // Data structures
    struct InferenceRecord {
        QString modelId;
        QDateTime timestamp;
        double tps;
        double latency;
        int tokensGenerated;
        bool success;
    };
    
    struct ResourceRecord {
        QDateTime timestamp;
        double memoryMb;
        double gpuUtil;
        int activeConnections;
    };
    
    // Internal methods
    void pruneOldData();
    double calculateTrend(const QVector<double>& values) const;
    bool isOutlier(double value, const QVector<double>& history) const;
    
    // Member variables
    QVector<InferenceRecord> m_inferenceHistory;
    QVector<ResourceRecord> m_resourceHistory;
    
    PerformanceMetrics m_currentPerformance;
    ResourceMetrics m_currentResources;
    CostMetrics m_currentCost;
    QualityMetrics m_currentQuality;
    
    QMap<QString, PerformanceMetrics> m_modelMetrics;
    QMap<QString, QualityMetrics> m_modelQuality;
    
    int m_retentionPeriodDays;
    int m_samplingIntervalSeconds;
    bool m_detailedTrackingEnabled;
    double m_costPerMillionTokens;
    
    mutable std::atomic<double> m_currentTps;
    mutable std::atomic<double> m_currentLatency;
    mutable std::atomic<double> m_peakMemory;
};

#endif // PHASE5_ANALYTICS_DASHBOARD_H
