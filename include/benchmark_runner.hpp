/**
 * benchmark_runner.hpp — C++20, no Qt. Background benchmark execution; callbacks replace signals.
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

class InferenceEngine;
class RealTimeCompletionEngine;

struct BenchmarkTestResult {
    std::string testName;
    bool passed = false;
    double avgLatencyMs = 0;
    double minLatencyMs = 0;
    double maxLatencyMs = 0;
    double p95LatencyMs = 0;
    double p99LatencyMs = 0;
    double medianLatencyMs = 0;
    size_t totalRequests = 0;
    size_t successfulRequests = 0;
    double successRate = 0;
    double throughputReqSec = 0;
    std::string notes;
};

class BenchmarkRunner
{
public:
    using StartedFn = std::function<void()>;
    using TestStartedFn = std::function<void(const std::string& testName)>;
    using ProgressFn = std::function<void(int current, int total)>;
    using TestCompletedFn = std::function<void(const std::string& testName, bool passed, double latencyMs)>;
    using FinishedFn = std::function<void(int passed, int total, double executionTimeSec)>;
    using ErrorFn = std::function<void(const std::string&)>;
    using LogMessageFn = std::function<void(const std::string& message, int level)>;

    BenchmarkRunner() = default;
    ~BenchmarkRunner();

    void setOnStarted(StartedFn f) { m_onStarted = std::move(f); }
    void setOnTestStarted(TestStartedFn f) { m_onTestStarted = std::move(f); }
    void setOnProgress(ProgressFn f) { m_onProgress = std::move(f); }
    void setOnTestCompleted(TestCompletedFn f) { m_onTestCompleted = std::move(f); }
    void setOnFinished(FinishedFn f) { m_onFinished = std::move(f); }
    void setOnError(ErrorFn f) { m_onError = std::move(f); }
    void setOnLogMessage(LogMessageFn f) { m_onLogMessage = std::move(f); }

    void runBenchmarks(const std::vector<std::string>& selectedTests,
                      const std::string& modelPath,
                      bool gpuEnabled,
                      bool verbose);
    void stop();
    const std::vector<BenchmarkTestResult>& getResults() const;

private:
    void executeRun();
    bool testColdStart(BenchmarkTestResult& result);
    bool testWarmCache(BenchmarkTestResult& result);
    bool testRapidFire(BenchmarkTestResult& result);
    bool testMultiLanguage(BenchmarkTestResult& result);
    bool testContextAware(BenchmarkTestResult& result);
    bool testMultiLine(BenchmarkTestResult& result);
    bool testGPUAcceleration(BenchmarkTestResult& result);
    bool testMemory(BenchmarkTestResult& result);
    BenchmarkTestResult calculateStats(const std::string& name,
                                       const std::vector<double>& latencies,
                                       size_t successful,
                                       size_t total);
    void log(const std::string& message, int level);

    std::vector<std::string> selectedTests_;
    std::string modelPath_;
    bool gpuEnabled_ = false;
    bool verbose_ = false;
    bool shouldStop_ = false;
    std::vector<BenchmarkTestResult> results_;
    std::unique_ptr<InferenceEngine> engine_;
    std::unique_ptr<RealTimeCompletionEngine> completionEngine_;

    StartedFn       m_onStarted;
    TestStartedFn   m_onTestStarted;
    ProgressFn      m_onProgress;
    TestCompletedFn m_onTestCompleted;
    FinishedFn      m_onFinished;
    ErrorFn         m_onError;
    LogMessageFn    m_onLogMessage;
};
