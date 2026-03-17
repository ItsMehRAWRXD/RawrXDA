/**
 * @file test_metrics.hpp
 * @brief Test metrics collection and Prometheus export
 */

#ifndef TEST_METRICS_HPP
#define TEST_METRICS_HPP

#include <QString>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <chrono>
#include <memory>

/**
 * @struct TestMetric
 * @brief Single test execution metric
 */
struct TestMetric
{
    QString testName;
    QString testSuite;
    long long durationMs;
    bool passed;
    QString errorMessage;
    std::chrono::high_resolution_clock::time_point timestamp;
    
    // Resource usage
    size_t peakMemoryBytes = 0;
    int threadCount = 0;
    double cpuUsagePercent = 0.0;
};

/**
 * @class TestMetricsCollector
 * @brief Collects metrics from test execution
 * 
 * Features:
 * - Per-test duration tracking
 * - Success/failure rates
 * - Memory and CPU usage
 * - Latency percentiles (p50, p95, p99)
 * - Prometheus format export
 * - Test execution trends
 */
class TestMetricsCollector
{
public:
    /**
     * Get singleton instance
     */
    static TestMetricsCollector& instance();

    /**
     * Record a test execution
     */
    void recordTestExecution(const TestMetric& metric);

    /**
     * Get all metrics
     */
    QVector<TestMetric> getAllMetrics() const;

    /**
     * Get metrics for specific test suite
     */
    QVector<TestMetric> getMetricsForSuite(const QString& suite) const;

    /**
     * Get metrics for specific test
     */
    QVector<TestMetric> getMetricsForTest(const QString& testName) const;

    /**
     * Calculate average duration for test
     */
    long long getAverageDuration(const QString& testName) const;

    /**
     * Calculate success rate for test
     */
    double getSuccessRate(const QString& testName) const;

    /**
     * Calculate latency percentile
     * @param testName Name of test
     * @param percentile Value 0-100
     * @return Latency in milliseconds
     */
    long long getLatencyPercentile(const QString& testName, int percentile) const;

    /**
     * Get peak memory usage
     */
    size_t getPeakMemory(const QString& testName) const;

    /**
     * Export metrics in Prometheus format
     */
    QString exportPrometheus() const;

    /**
     * Export metrics as JSON
     */
    QString exportJSON() const;

    /**
     * Export metrics as CSV
     */
    QString exportCSV() const;

    /**
     * Clear all metrics
     */
    void clear();

    /**
     * Get metric count
     */
    int count() const;

    /**
     * Get list of all test names
     */
    QStringList getTestNames() const;

    /**
     * Get list of all test suites
     */
    QStringList getTestSuites() const;

    /**
     * Get test execution trend (for graphing)
     * @param testName Name of test
     * @param limit Maximum number of recent executions
     */
    QVector<TestMetric> getExecutionTrend(const QString& testName, int limit = 100) const;

    /**
     * Detect slow tests
     * @param percentageThreshold Tests slower than this % of average (e.g., 150%)
     */
    QVector<QString> detectSlowTests(double percentageThreshold = 150.0) const;

    /**
     * Get flaky tests (high variance in pass/fail)
     */
    QVector<QString> getFlakyTests() const;

private:
    TestMetricsCollector() = default;
    TestMetricsCollector(const TestMetricsCollector&) = delete;
    TestMetricsCollector& operator=(const TestMetricsCollector&) = delete;

    mutable QMutex m_mutex;
    QVector<TestMetric> m_metrics;

    /**
     * Calculate standard deviation for test durations
     */
    double calculateStdDev(const QVector<long long>& durations) const;
};

/**
 * @class TestExecutionTimer
 * @brief RAII timer for automatic test metric recording
 * 
 * Usage:
 *   TestExecutionTimer timer("TestName", "TestSuite");
 *   // ... test code ...
 *   // Automatically records on destruction
 */
class TestExecutionTimer
{
public:
    TestExecutionTimer(const QString& testName, const QString& testSuite);
    ~TestExecutionTimer();

    /**
     * Mark test as passed/failed
     */
    void markPassed();
    void markFailed(const QString& errorMessage);

    /**
     * Record resource usage
     */
    void setMemoryUsage(size_t peakBytes);
    void setThreadCount(int count);
    void setCPUUsage(double percentValue);

private:
    QString m_testName;
    QString m_testSuite;
    std::chrono::high_resolution_clock::time_point m_startTime;
    TestMetric m_metric;
    bool m_reported = false;
};

/**
 * @class TestDashboard
 * @brief Dashboard-friendly metrics aggregation
 */
class TestDashboard
{
public:
    /**
     * Get summary statistics
     */
    struct Summary {
        int totalTests;
        int passedTests;
        int failedTests;
        double successRate;
        long long avgDurationMs;
        long long medianDurationMs;
        long long maxDurationMs;
        long long minDurationMs;
        size_t peakMemoryMB;
    };

    /**
     * Get overall summary
     */
    static Summary getSummary();

    /**
     * Get summary for suite
     */
    static Summary getSummaryForSuite(const QString& suite);

    /**
     * Generate HTML dashboard report
     */
    static QString generateHTMLReport();

    /**
     * Generate text report for console
     */
    static QString generateTextReport();
};

#endif // TEST_METRICS_HPP
