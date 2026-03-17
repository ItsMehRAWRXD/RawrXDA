/**
 * \file benchmark_runner.hpp
 * \brief Background thread runner for benchmark execution with real tests
 * \author RawrXD Team
 * \date 2025-12-13
 */

#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <vector>
#include <string>
#include <memory>

class InferenceEngine;
class RealTimeCompletionEngine;

/**
 * @brief Benchmark result for a single test
 */
struct BenchmarkTestResult {
    std::string testName;
    bool passed;
    double avgLatencyMs;
    double minLatencyMs;
    double maxLatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    double medianLatencyMs;
    size_t totalRequests;
    size_t successfulRequests;
    double successRate;
    double throughputReqSec;
    std::string notes;
};

/**
 * @brief Runs benchmarks in a background thread
 */
class BenchmarkRunner : public QObject {
    Q_OBJECT

public:
    explicit BenchmarkRunner(QObject* parent = nullptr);
    ~BenchmarkRunner();

    // Start a benchmark run
    void runBenchmarks(const std::vector<std::string>& selectedTests,
                       const QString& modelPath,
                       bool gpuEnabled,
                       bool verbose);

    // Stop current run
    void stop();

    // Get results
    const std::vector<BenchmarkTestResult>& getResults() const;

signals:
    // Progress signals
    void started();
    void testStarted(const QString& testName);
    void progress(int current, int total);
    void testCompleted(const QString& testName, bool passed, double latencyMs);
    void finished(int passed, int total, double executionTimeSec);
    void error(const QString& errorMessage);

    // Logging signals
    void logMessage(const QString& message, int level);  // 0=DEBUG, 1=INFO, 2=SUCCESS, 3=WARNING, 4=ERROR

private slots:
    void executeRun();

private:
    // Test implementations
    bool testColdStart(BenchmarkTestResult& result);
    bool testWarmCache(BenchmarkTestResult& result);
    bool testRapidFire(BenchmarkTestResult& result);
    bool testMultiLanguage(BenchmarkTestResult& result);
    bool testContextAware(BenchmarkTestResult& result);
    bool testMultiLine(BenchmarkTestResult& result);
    bool testGPUAcceleration(BenchmarkTestResult& result);
    bool testMemory(BenchmarkTestResult& result);

    // Helper methods
    BenchmarkTestResult calculateStats(const std::string& name,
                                       const std::vector<double>& latencies,
                                       size_t successful,
                                       size_t total);

    void log(const QString& message, int level);

    // State
    std::vector<std::string> selectedTests_;
    QString modelPath_;
    bool gpuEnabled_;
    bool verbose_;
    bool shouldStop_;

    std::vector<BenchmarkTestResult> results_;
    std::unique_ptr<InferenceEngine> engine_;
    std::unique_ptr<RealTimeCompletionEngine> completionEngine_;
};

// End of benchmark_runner.hpp
