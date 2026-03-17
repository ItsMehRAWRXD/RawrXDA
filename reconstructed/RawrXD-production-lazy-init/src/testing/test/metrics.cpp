/**
 * @file test_metrics.cpp
 * @brief Implementation of test metrics collection
 */

#include "test_metrics.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>
#include <cmath>
#include <numeric>

// ─────────────────────────────────────────────────────────────────────
// TestMetricsCollector Implementation
// ─────────────────────────────────────────────────────────────────────

TestMetricsCollector& TestMetricsCollector::instance()
{
    static TestMetricsCollector collector;
    return collector;
}

void TestMetricsCollector::recordTestExecution(const TestMetric& metric)
{
    QMutexLocker locker(&m_mutex);
    m_metrics.append(metric);
    
    qInfo() << "[TestMetrics]" << metric.testName << "Duration:" << metric.durationMs << "ms"
            << "Status:" << (metric.passed ? "PASS" : "FAIL");
}

QVector<TestMetric> TestMetricsCollector::getAllMetrics() const
{
    QMutexLocker locker(&m_mutex);
    return m_metrics;
}

QVector<TestMetric> TestMetricsCollector::getMetricsForSuite(const QString& suite) const
{
    QMutexLocker locker(&m_mutex);
    QVector<TestMetric> result;
    
    for (const auto& metric : m_metrics) {
        if (metric.testSuite == suite) {
            result.append(metric);
        }
    }
    
    return result;
}

QVector<TestMetric> TestMetricsCollector::getMetricsForTest(const QString& testName) const
{
    QMutexLocker locker(&m_mutex);
    QVector<TestMetric> result;
    
    for (const auto& metric : m_metrics) {
        if (metric.testName == testName) {
            result.append(metric);
        }
    }
    
    // Sort by timestamp
    std::sort(result.begin(), result.end(),
             [](const TestMetric& a, const TestMetric& b) {
                 return a.timestamp < b.timestamp;
             });
    
    return result;
}

long long TestMetricsCollector::getAverageDuration(const QString& testName) const
{
    QVector<TestMetric> metrics = getMetricsForTest(testName);
    
    if (metrics.isEmpty()) {
        return 0;
    }
    
    long long totalDuration = 0;
    for (const auto& metric : metrics) {
        totalDuration += metric.durationMs;
    }
    
    return totalDuration / metrics.size();
}

double TestMetricsCollector::getSuccessRate(const QString& testName) const
{
    QVector<TestMetric> metrics = getMetricsForTest(testName);
    
    if (metrics.isEmpty()) {
        return 0.0;
    }
    
    int passedCount = 0;
    for (const auto& metric : metrics) {
        if (metric.passed) {
            passedCount++;
        }
    }
    
    return (passedCount * 100.0) / metrics.size();
}

long long TestMetricsCollector::getLatencyPercentile(const QString& testName, int percentile) const
{
    QVector<TestMetric> metrics = getMetricsForTest(testName);
    
    if (metrics.isEmpty()) {
        return 0;
    }
    
    QVector<long long> durations;
    for (const auto& metric : metrics) {
        durations.append(metric.durationMs);
    }
    
    std::sort(durations.begin(), durations.end());
    
    int index = (percentile * durations.size()) / 100;
    if (index >= durations.size()) {
        index = durations.size() - 1;
    }
    
    return durations[index];
}

size_t TestMetricsCollector::getPeakMemory(const QString& testName) const
{
    QVector<TestMetric> metrics = getMetricsForTest(testName);
    
    size_t peak = 0;
    for (const auto& metric : metrics) {
        peak = std::max(peak, metric.peakMemoryBytes);
    }
    
    return peak;
}

QString TestMetricsCollector::exportPrometheus() const
{
    QMutexLocker locker(&m_mutex);
    QString output;
    
    // Test execution counter
    output += "# HELP test_executions_total Total number of test executions\n";
    output += "# TYPE test_executions_total counter\n";
    output += QString("test_executions_total{} %1\n").arg(m_metrics.size());
    
    // Test pass rate
    int passedCount = 0;
    for (const auto& metric : m_metrics) {
        if (metric.passed) {
            passedCount++;
        }
    }
    
    double passRate = m_metrics.isEmpty() ? 0.0 : (passedCount * 100.0) / m_metrics.size();
    output += "# HELP test_pass_rate_percent Test pass rate\n";
    output += "# TYPE test_pass_rate_percent gauge\n";
    output += QString("test_pass_rate_percent{} %1\n").arg(passRate, 0, 'f', 2);
    
    // Test duration histogram (aggregated)
    if (!m_metrics.isEmpty()) {
        long long minDuration = m_metrics[0].durationMs;
        long long maxDuration = m_metrics[0].durationMs;
        long long totalDuration = 0;
        
        for (const auto& metric : m_metrics) {
            minDuration = std::min(minDuration, metric.durationMs);
            maxDuration = std::max(maxDuration, metric.durationMs);
            totalDuration += metric.durationMs;
        }
        
        output += "# HELP test_duration_ms_bucket Test duration histogram\n";
        output += "# TYPE test_duration_ms_bucket histogram\n";
        output += QString("test_duration_ms_bucket{le=\"100\"} %1\n").arg(0);
        output += QString("test_duration_ms_bucket{le=\"500\"} %1\n").arg(0);
        output += QString("test_duration_ms_bucket{le=\"1000\"} %1\n").arg(0);
        output += QString("test_duration_ms_bucket{le=\"+Inf\"} %1\n").arg(m_metrics.size());
        output += QString("test_duration_ms_sum %1\n").arg(totalDuration);
        output += QString("test_duration_ms_count %1\n").arg(m_metrics.size());
        
        // Per-test metrics
        QMap<QString, QVector<long long>> testDurations;
        for (const auto& metric : m_metrics) {
            testDurations[metric.testName].append(metric.durationMs);
        }
        
        output += "# HELP test_duration_ms_avg Average test duration\n";
        output += "# TYPE test_duration_ms_avg gauge\n";
        for (auto it = testDurations.begin(); it != testDurations.end(); ++it) {
            long long avg = 0;
            for (long long dur : it.value()) {
                avg += dur;
            }
            avg /= it.value().size();
            output += QString("test_duration_ms_avg{test=\"%1\"} %2\n").arg(it.key()).arg(avg);
        }
    }
    
    return output;
}

QString TestMetricsCollector::exportJSON() const
{
    QMutexLocker locker(&m_mutex);
    QJsonArray metricsArray;
    
    for (const auto& metric : m_metrics) {
        QJsonObject obj;
        obj["test_name"] = metric.testName;
        obj["test_suite"] = metric.testSuite;
        obj["duration_ms"] = static_cast<qint64>(metric.durationMs);
        obj["passed"] = metric.passed;
        obj["error_message"] = metric.errorMessage;
        obj["timestamp"] = metric.timestamp.time_since_epoch().count();
        obj["peak_memory_bytes"] = static_cast<qint64>(metric.peakMemoryBytes);
        obj["thread_count"] = metric.threadCount;
        obj["cpu_usage_percent"] = metric.cpuUsagePercent;
        
        metricsArray.append(obj);
    }
    
    QJsonDocument doc(metricsArray);
    return QString::fromUtf8(doc.toJson());
}

QString TestMetricsCollector::exportCSV() const
{
    QMutexLocker locker(&m_mutex);
    QString csv = "test_name,test_suite,duration_ms,passed,error_message,peak_memory_mb,thread_count,cpu_usage_percent\n";
    
    for (const auto& metric : m_metrics) {
        csv += QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
            .arg(metric.testName)
            .arg(metric.testSuite)
            .arg(metric.durationMs)
            .arg(metric.passed ? "1" : "0")
            .arg(metric.errorMessage)
            .arg(metric.peakMemoryBytes / (1024.0 * 1024.0), 0, 'f', 2)
            .arg(metric.threadCount)
            .arg(metric.cpuUsagePercent, 0, 'f', 2);
    }
    
    return csv;
}

void TestMetricsCollector::clear()
{
    QMutexLocker locker(&m_mutex);
    m_metrics.clear();
    qInfo() << "[TestMetrics] Metrics cleared";
}

int TestMetricsCollector::count() const
{
    QMutexLocker locker(&m_mutex);
    return m_metrics.size();
}

QStringList TestMetricsCollector::getTestNames() const
{
    QMutexLocker locker(&m_mutex);
    QSet<QString> names;
    
    for (const auto& metric : m_metrics) {
        names.insert(metric.testName);
    }
    
    return names.values();
}

QStringList TestMetricsCollector::getTestSuites() const
{
    QMutexLocker locker(&m_mutex);
    QSet<QString> suites;
    
    for (const auto& metric : m_metrics) {
        suites.insert(metric.testSuite);
    }
    
    return suites.values();
}

QVector<TestMetric> TestMetricsCollector::getExecutionTrend(const QString& testName, int limit) const
{
    QVector<TestMetric> trend = getMetricsForTest(testName);
    
    if (trend.size() > limit) {
        trend = trend.mid(trend.size() - limit);
    }
    
    return trend;
}

QVector<QString> TestMetricsCollector::detectSlowTests(double percentageThreshold) const
{
    QMutexLocker locker(&m_mutex);
    QVector<QString> slowTests;
    QMap<QString, long long> avgDurations;
    QMap<QString, int> testCounts;
    
    // Calculate averages
    for (const auto& metric : m_metrics) {
        if (avgDurations.contains(metric.testName)) {
            avgDurations[metric.testName] += metric.durationMs;
        } else {
            avgDurations[metric.testName] = metric.durationMs;
        }
        testCounts[metric.testName]++;
    }
    
    for (auto it = avgDurations.begin(); it != avgDurations.end(); ++it) {
        if (testCounts[it.key()] > 0) {
            it.value() /= testCounts[it.key()];
        }
    }
    
    // Find slow tests
    long long globalAvg = 0;
    int globalCount = 0;
    for (auto it = avgDurations.begin(); it != avgDurations.end(); ++it) {
        globalAvg += it.value();
        globalCount++;
    }
    globalAvg /= std::max(1, globalCount);
    
    double threshold = globalAvg * (percentageThreshold / 100.0);
    for (auto it = avgDurations.begin(); it != avgDurations.end(); ++it) {
        if (it.value() > threshold) {
            slowTests.append(it.key());
        }
    }
    
    return slowTests;
}

QVector<QString> TestMetricsCollector::getFlakyTests() const
{
    QMutexLocker locker(&m_mutex);
    QVector<QString> flakyTests;
    QMap<QString, int> passCount;
    QMap<QString, int> failCount;
    
    for (const auto& metric : m_metrics) {
        if (metric.passed) {
            passCount[metric.testName]++;
        } else {
            failCount[metric.testName]++;
        }
    }
    
    // Test is flaky if it has both pass and fail
    for (auto it = passCount.begin(); it != passCount.end(); ++it) {
        if (failCount.contains(it.key()) && failCount[it.key()] > 0) {
            flakyTests.append(it.key());
        }
    }
    
    return flakyTests;
}

double TestMetricsCollector::calculateStdDev(const QVector<long long>& durations) const
{
    if (durations.size() < 2) {
        return 0.0;
    }
    
    long long mean = std::accumulate(durations.begin(), durations.end(), 0LL) / durations.size();
    
    double variance = 0.0;
    for (long long dur : durations) {
        double diff = dur - mean;
        variance += diff * diff;
    }
    variance /= durations.size();
    
    return std::sqrt(variance);
}

// ─────────────────────────────────────────────────────────────────────
// TestExecutionTimer Implementation
// ─────────────────────────────────────────────────────────────────────

TestExecutionTimer::TestExecutionTimer(const QString& testName, const QString& testSuite)
    : m_testName(testName), m_testSuite(testSuite)
{
    m_metric.testName = testName;
    m_metric.testSuite = testSuite;
    m_metric.timestamp = std::chrono::high_resolution_clock::now();
    m_startTime = m_metric.timestamp;
}

TestExecutionTimer::~TestExecutionTimer()
{
    if (!m_reported) {
        auto endTime = std::chrono::high_resolution_clock::now();
        m_metric.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - m_startTime).count();
        
        TestMetricsCollector::instance().recordTestExecution(m_metric);
    }
}

void TestExecutionTimer::markPassed()
{
    m_metric.passed = true;
}

void TestExecutionTimer::markFailed(const QString& errorMessage)
{
    m_metric.passed = false;
    m_metric.errorMessage = errorMessage;
}

void TestExecutionTimer::setMemoryUsage(size_t peakBytes)
{
    m_metric.peakMemoryBytes = peakBytes;
}

void TestExecutionTimer::setThreadCount(int count)
{
    m_metric.threadCount = count;
}

void TestExecutionTimer::setCPUUsage(double percentValue)
{
    m_metric.cpuUsagePercent = percentValue;
}

// ─────────────────────────────────────────────────────────────────────
// TestDashboard Implementation
// ─────────────────────────────────────────────────────────────────────

TestDashboard::Summary TestDashboard::getSummary()
{
    auto metrics = TestMetricsCollector::instance().getAllMetrics();
    
    Summary summary;
    summary.totalTests = metrics.size();
    summary.passedTests = 0;
    summary.failedTests = 0;
    summary.avgDurationMs = 0;
    summary.maxDurationMs = 0;
    summary.minDurationMs = std::numeric_limits<long long>::max();
    summary.peakMemoryMB = 0;
    
    long long totalDuration = 0;
    
    for (const auto& metric : metrics) {
        if (metric.passed) {
            summary.passedTests++;
        } else {
            summary.failedTests++;
        }
        
        totalDuration += metric.durationMs;
        summary.maxDurationMs = std::max(summary.maxDurationMs, metric.durationMs);
        summary.minDurationMs = std::min(summary.minDurationMs, metric.durationMs);
        summary.peakMemoryMB = std::max(summary.peakMemoryMB, metric.peakMemoryBytes / (1024 * 1024));
    }
    
    if (summary.totalTests > 0) {
        summary.avgDurationMs = totalDuration / summary.totalTests;
        summary.successRate = (summary.passedTests * 100.0) / summary.totalTests;
    }
    
    return summary;
}

TestDashboard::Summary TestDashboard::getSummaryForSuite(const QString& suite)
{
    auto metrics = TestMetricsCollector::instance().getMetricsForSuite(suite);
    
    Summary summary;
    summary.totalTests = metrics.size();
    summary.passedTests = 0;
    summary.failedTests = 0;
    
    for (const auto& metric : metrics) {
        if (metric.passed) {
            summary.passedTests++;
        } else {
            summary.failedTests++;
        }
    }
    
    if (summary.totalTests > 0) {
        summary.successRate = (summary.passedTests * 100.0) / summary.totalTests;
    }
    
    return summary;
}

QString TestDashboard::generateHTMLReport()
{
    auto summary = getSummary();
    
    QString html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Test Execution Dashboard</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; }
            .summary { background: #f0f0f0; padding: 15px; border-radius: 5px; }
            .pass { color: green; font-weight: bold; }
            .fail { color: red; font-weight: bold; }
            table { border-collapse: collapse; margin-top: 20px; }
            th, td { border: 1px solid #ddd; padding: 10px; text-align: left; }
            th { background: #4CAF50; color: white; }
        </style>
    </head>
    <body>
        <h1>Test Execution Dashboard</h1>
        <div class="summary">
            <h2>Summary Statistics</h2>
            <p>Total Tests: <strong>)";
    
    html += QString::number(summary.totalTests) + R"(</strong></p>
            <p class="pass">Passed: <strong>)" + QString::number(summary.passedTests) + R"(</strong></p>
            <p class="fail">Failed: <strong>)" + QString::number(summary.failedTests) + R"(</strong></p>
            <p>Success Rate: <strong>)" + QString::number(summary.successRate, 'f', 2) + R"(%</strong></p>
            <p>Average Duration: <strong>)" + QString::number(summary.avgDurationMs) + R"(ms</strong></p>
            <p>Peak Memory: <strong>)" + QString::number(summary.peakMemoryMB) + R"(MB</strong></p>
        </div>
    </body>
    </html>
    )";
    
    return html;
}

QString TestDashboard::generateTextReport()
{
    auto summary = getSummary();
    
    QString report = "\n=== Test Execution Report ===\n";
    report += QString("Total Tests: %1\n").arg(summary.totalTests);
    report += QString("Passed: %1 (green)\n").arg(summary.passedTests);
    report += QString("Failed: %1 (red)\n").arg(summary.failedTests);
    report += QString("Success Rate: %1%\n").arg(summary.successRate, 0, 'f', 2);
    report += QString("Average Duration: %1ms\n").arg(summary.avgDurationMs);
    report += QString("Max Duration: %1ms\n").arg(summary.maxDurationMs);
    report += QString("Min Duration: %1ms\n").arg(summary.minDurationMs);
    report += QString("Peak Memory: %1MB\n").arg(summary.peakMemoryMB);
    
    return report;
}
