#pragma once

#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include "backend_selector.h"
#include "inference_engine.h"

namespace RawrXD {

/**
 * @struct BenchmarkResult
 * @brief Result of a single benchmark run
 */
struct BenchmarkResult {
    BackendType backend;
    std::string backendName;
    std::string modelPath;
    std::string testPrompt;
    int maxTokens;
    double totalTimeMs;
    double tokensPerSec;
    double latencyP50Ms;
    double latencyP95Ms;
    double latencyP99Ms;
    size_t memoryUsageBytes;
    bool success;
    std::string errorMessage;
};

/**
 * @struct BenchmarkConfig
 * @brief Configuration for benchmark runs
 */
struct BenchmarkConfig {
    std::vector<std::string> modelPaths;
    std::vector<std::string> testPrompts;
    std::vector<int> maxTokensList = {10, 50, 100};
    int warmupRuns = 3;
    int benchmarkRuns = 5;
    bool enableMemoryTracking = true;
    bool enableLatencyProfiling = true;
};

/**
 * @class InferenceBenchmark
 * @brief Comprehensive benchmarking system for inference backends
 *
 * Provides detailed performance metrics, memory usage tracking, and
 * comparative analysis across different hardware backends.
 */
class InferenceBenchmark {
public:
    InferenceBenchmark();
    ~InferenceBenchmark() = default;

    /**
     * @brief Run comprehensive benchmark suite
     * @param config Benchmark configuration
     * @return Vector of benchmark results
     */
    std::vector<BenchmarkResult> runBenchmarkSuite(const BenchmarkConfig& config);

    /**
     * @brief Benchmark a specific backend with a model
     * @param backend Backend to test
     * @param modelPath Path to model file
     * @param testPrompt Test prompt
     * @param maxTokens Maximum tokens to generate
     * @param numRuns Number of runs for averaging
     * @return Benchmark result
     */
    BenchmarkResult benchmarkBackend(BackendType backend,
                                   const std::string& modelPath,
                                   const std::string& testPrompt,
                                   int maxTokens,
                                   int numRuns = 5);

    /**
     * @brief Compare backends and generate performance report
     * @param results Benchmark results to compare
     * @return Formatted comparison report
     */
    std::string generateComparisonReport(const std::vector<BenchmarkResult>& results);

    /**
     * @brief Get recommended backend for a model
     * @param modelPath Path to model file
     * @param results Previous benchmark results
     * @return Recommended backend type
     */
    BackendType getRecommendedBackend(const std::string& modelPath,
                                    const std::vector<BenchmarkResult>& results);

private:
    std::unique_ptr<BackendSelector> m_backendSelector;

    // Benchmarking helpers
    BenchmarkResult runSingleBenchmark(BackendType backend,
                                     const std::string& modelPath,
                                     const std::string& testPrompt,
                                     int maxTokens);

    std::vector<double> measureLatencies(std::unique_ptr<InferenceEngine>& engine,
                                       const std::vector<int32_t>& inputTokens,
                                       int maxTokens,
                                       int numRuns);

    size_t measureMemoryUsage(std::unique_ptr<InferenceEngine>& engine);

    void warmupEngine(std::unique_ptr<InferenceEngine>& engine,
                     const std::string& modelPath,
                     const std::string& testPrompt,
                     int warmupRuns);
};

} // namespace RawrXD