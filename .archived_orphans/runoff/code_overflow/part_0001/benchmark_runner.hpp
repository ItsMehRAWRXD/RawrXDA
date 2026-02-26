#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

// Forward declarations
namespace RawrXD {
    class CPUInferenceEngine;
}
class RealTimeCompletionEngine;

struct BenchmarkTestResult {
    std::string testName;
    bool passed = false;
    double avgLatencyMs = 0.0;
    double minLatencyMs = 0.0;
    double maxLatencyMs = 0.0;
    double medianLatencyMs = 0.0;
    double p95LatencyMs = 0.0;
    double p99LatencyMs = 0.0;
    size_t totalRequests = 0;
    size_t successfulRequests = 0;
    double successRate = 0.0;
    double throughputReqSec = 0.0;
    std::string notes;
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(void* parent = nullptr);
    ~BenchmarkRunner();

    // Configuration
    void runBenchmarks(const std::vector<std::string>& selectedTests,
                      const std::string& modelPath,
                      bool gpuEnabled,
                      bool verbose);
    
    void stop();
    const std::vector<BenchmarkTestResult>& getResults() const;

    // Callbacks/Signals replacement
    using LogCallback = std::function<void(const std::string& message, int level)>;
    using ProgressCallback = std::function<void(int current, int total)>;
    using TestStartedCallback = std::function<void(const std::string& testName)>;
    using TestCompletedCallback = std::function<void(const std::string& testName, bool success, double avgLatencyMs)>;
    using FinishedCallback = std::function<void(int passed, int total, double executionTime)>;

    void setLogCallback(LogCallback cb) { logCallback_ = cb; }
    void setProgressCallback(ProgressCallback cb) { progressCallback_ = cb; }

private:
    void executeRun(); // Internal run loop

    // Individual tests
    bool testColdStart(BenchmarkTestResult& result);
    bool testWarmCache(BenchmarkTestResult& result);
    bool testRapidFire(BenchmarkTestResult& result);
    bool testMultiLanguage(BenchmarkTestResult& result);
    bool testContextAware(BenchmarkTestResult& result);
    bool testMultiLine(BenchmarkTestResult& result);
    bool testGPUAcceleration(BenchmarkTestResult& result);
    bool testMemory(BenchmarkTestResult& result);

    // Helpers
    void log(const std::string& message, int level);
    BenchmarkTestResult calculateStats(const std::string& name,
                                     const std::vector<double>& latencies,
                                     size_t successful,
                                     size_t total);

    // Member data
    std::unique_ptr<RawrXD::CPUInferenceEngine> engine_;
    std::unique_ptr<RealTimeCompletionEngine> completionEngine_;
    
    std::vector<std::string> selectedTests_;
    std::string modelPath_;
    bool gpuEnabled_;
    bool verbose_;
    std::atomic<bool> shouldStop_;
    std::vector<BenchmarkTestResult> results_;

    // Config
    LogCallback logCallback_;
    ProgressCallback progressCallback_;
};
