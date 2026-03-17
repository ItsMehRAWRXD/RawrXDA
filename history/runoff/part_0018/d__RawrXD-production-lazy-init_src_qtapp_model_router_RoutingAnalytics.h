#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>

// Forward declarations
struct RoutingRequest;
struct RoutingDecision;
struct ModelEndpoint;

/**
 * @brief Request metrics
 */
struct RequestMetrics {
    QString requestId;
    QString endpointId;
    QDateTime timestamp;
    double responseTime;       // milliseconds
    int tokensGenerated;
    bool success;
    QString errorMessage;
    qint64 estimatedCost;      // microcents
    
    RequestMetrics() : responseTime(0), tokensGenerated(0), success(true), estimatedCost(0) {
        timestamp = QDateTime::currentDateTime();
    }
    
    QJsonObject toJson() const;
};

/**
 * @brief Endpoint performance metrics
 */
struct EndpointMetrics {
    QString endpointId;
    qint64 totalRequests;
    qint64 successfulRequests;
    qint64 failedRequests;
    double averageResponseTime;
    double p50ResponseTime;    // Median
    double p95ResponseTime;    // 95th percentile
    double p99ResponseTime;    // 99th percentile
    double successRate;
    qint64 totalCost;          // microcents
    QDateTime lastUpdated;
    
    EndpointMetrics() : totalRequests(0), successfulRequests(0), failedRequests(0),
                       averageResponseTime(0), p50ResponseTime(0), p95ResponseTime(0),
                       p99ResponseTime(0), successRate(1.0), totalCost(0) {
        lastUpdated = QDateTime::currentDateTime();
    }
    
    QJsonObject toJson() const;
};

/**
 * @brief A/B test configuration
 */
struct ABTestConfig {
    QString testId;
    QString variantA;          // Endpoint ID for variant A
    QString variantB;          // Endpoint ID for variant B
    double trafficSplitA;      // 0.0 - 1.0 (e.g., 0.5 = 50%)
    double trafficSplitB;      // 0.0 - 1.0
    QDateTime startTime;
    QDateTime endTime;
    bool active;
    QString hypothesis;
    
    ABTestConfig() : trafficSplitA(0.5), trafficSplitB(0.5), active(false) {}
    
    QJsonObject toJson() const;
};

/**
 * @brief A/B test results
 */
struct ABTestResults {
    QString testId;
    EndpointMetrics variantAMetrics;
    EndpointMetrics variantBMetrics;
    double confidenceLevel;    // Statistical confidence (0.0-1.0)
    QString winningVariant;    // "A", "B", or "inconclusive"
    QString conclusion;
    QDateTime analyzedAt;
    
    ABTestResults() : confidenceLevel(0), winningVariant("inconclusive") {
        analyzedAt = QDateTime::currentDateTime();
    }
    
    QJsonObject toJson() const;
};

/**
 * @brief Cost analysis report
 */
struct CostAnalysisReport {
    QString endpointId;
    QDateTime startTime;
    QDateTime endTime;
    qint64 totalRequests;
    qint64 totalTokens;
    qint64 totalCost;          // microcents
    double averageCostPerRequest;
    double averageCostPerToken;
    QMap<QString, qint64> costByHour;  // Hour -> cost
    
    CostAnalysisReport() : totalRequests(0), totalTokens(0), totalCost(0),
                          averageCostPerRequest(0), averageCostPerToken(0) {}
    
    QJsonObject toJson() const;
};

/**
 * @brief Routing Analytics
 * 
 * Comprehensive analytics for model routing including performance tracking,
 * A/B testing, cost analysis, and comparative metrics.
 */
class RoutingAnalytics : public QObject {
    Q_OBJECT
    
public:
    explicit RoutingAnalytics(QObject* parent = nullptr);
    ~RoutingAnalytics();
    
    // ============ Request Tracking ============
    
    /**
     * @brief Track a request
     */
    void trackRequest(const RoutingRequest& request, const RoutingDecision& decision);
    
    /**
     * @brief Record request completion
     */
    void recordRequestCompletion(const QString& requestId, double responseTime, 
                                 int tokensGenerated, bool success, const QString& error = QString());
    
    /**
     * @brief Get request metrics
     */
    RequestMetrics getRequestMetrics(const QString& requestId) const;
    
    /**
     * @brief Get all requests in time range
     */
    QList<RequestMetrics> getRequestsInTimeRange(const QDateTime& start, const QDateTime& end) const;
    
    // ============ Performance Metrics ============
    
    /**
     * @brief Get endpoint metrics
     */
    EndpointMetrics getEndpointMetrics(const QString& endpointId) const;
    
    /**
     * @brief Get all endpoint metrics
     */
    QMap<QString, EndpointMetrics> getAllEndpointMetrics() const;
    
    /**
     * @brief Calculate percentile response time
     */
    double calculatePercentile(const QString& endpointId, double percentile) const;
    
    /**
     * @brief Get top performing endpoints
     */
    QList<QString> getTopEndpoints(int count = 5) const;
    
    /**
     * @brief Get bottom performing endpoints
     */
    QList<QString> getBottomEndpoints(int count = 5) const;
    
    // ============ Comparative Analysis ============
    
    /**
     * @brief Compare two endpoints
     */
    QJsonObject compareEndpoints(const QString& endpointA, const QString& endpointB) const;
    
    /**
     * @brief Get performance trends over time
     */
    QVector<double> getPerformanceTrend(const QString& endpointId, int hours = 24) const;
    
    /**
     * @brief Get throughput trend
     */
    QVector<qint64> getThroughputTrend(const QString& endpointId, int hours = 24) const;
    
    // ============ A/B Testing ============
    
    /**
     * @brief Start A/B test
     */
    QString startABTest(const ABTestConfig& config);
    
    /**
     * @brief Stop A/B test
     */
    void stopABTest(const QString& testId);
    
    /**
     * @brief Get A/B test configuration
     */
    ABTestConfig getABTestConfig(const QString& testId) const;
    
    /**
     * @brief Analyze A/B test results
     */
    ABTestResults analyzeABTest(const QString& testId) const;
    
    /**
     * @brief Get all active A/B tests
     */
    QList<ABTestConfig> getActiveABTests() const;
    
    /**
     * @brief Route request for A/B test
     */
    QString routeForABTest(const QString& testId, const RoutingRequest& request);
    
    // ============ Cost Analysis ============
    
    /**
     * @brief Track request cost
     */
    void trackRequestCost(const QString& requestId, qint64 cost);
    
    /**
     * @brief Get cost analysis for endpoint
     */
    CostAnalysisReport getCostAnalysis(const QString& endpointId, 
                                       const QDateTime& start, const QDateTime& end) const;
    
    /**
     * @brief Get total cost for all endpoints
     */
    qint64 getTotalCost(const QDateTime& start, const QDateTime& end) const;
    
    /**
     * @brief Get cost by endpoint
     */
    QMap<QString, qint64> getCostByEndpoint(const QDateTime& start, const QDateTime& end) const;
    
    /**
     * @brief Forecast cost
     */
    qint64 forecastCost(const QString& endpointId, int hours) const;
    
    // ============ Reporting ============
    
    /**
     * @brief Generate performance report
     */
    QJsonObject generatePerformanceReport() const;
    
    /**
     * @brief Generate cost report
     */
    QJsonObject generateCostReport(const QDateTime& start, const QDateTime& end) const;
    
    /**
     * @brief Generate comparative report
     */
    QJsonObject generateComparativeReport(const QStringList& endpointIds) const;
    
    /**
     * @brief Export metrics to JSON
     */
    QJsonObject exportMetrics() const;
    
    // ============ Data Management ============
    
    /**
     * @brief Clear old data
     */
    void clearOldData(int daysToKeep = 30);
    
    /**
     * @brief Reset all metrics
     */
    void resetMetrics();
    
    /**
     * @brief Get data size in bytes
     */
    qint64 getDataSize() const;
    
signals:
    /**
     * @brief Emitted when significant performance change detected
     */
    void performanceAlert(const QString& endpointId, const QString& message);
    
    /**
     * @brief Emitted when A/B test completes
     */
    void abTestCompleted(const QString& testId, const ABTestResults& results);
    
    /**
     * @brief Emitted when cost threshold exceeded
     */
    void costThresholdExceeded(const QString& endpointId, qint64 cost);
    
private:
    // Helper functions
    void updateEndpointMetrics(const QString& endpointId);
    double calculateStatisticalSignificance(const EndpointMetrics& a, const EndpointMetrics& b) const;
    QString determineWinner(const EndpointMetrics& a, const EndpointMetrics& b, double confidence) const;
    
    // Data members
    QMap<QString, RequestMetrics> m_requestMetrics;
    QMap<QString, EndpointMetrics> m_endpointMetrics;
    QMap<QString, QList<double>> m_responseTimes;  // For percentile calculation
    QMap<QString, ABTestConfig> m_abTests;
    QMap<QString, qint64> m_requestCosts;
    
    mutable QMutex m_mutex;
};
