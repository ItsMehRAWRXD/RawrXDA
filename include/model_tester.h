#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>

#include "logging/logger.h"
#include "metrics/metrics.h"
#include "response_parser.h"

/**
 * ModelTester: Comprehensive framework for testing real model integration
 * 
 * Capabilities:
 * - Send real requests to Ollama/Local models
 * - Measure latency (time-to-first-token, time-to-completion)
 * - Parse and validate responses
 * - Benchmark different models
 * - Generate test reports
 */

struct ModelTestResult {
    std::string modelName;
    std::string prompt;
    std::string response;
    int completionCount;
    
    // Latency measurements (microseconds)
    int64_t timeToFirstTokenUs;
    int64_t totalLatencyUs;
    double avgTokenLatencyUs;
    
    // Quality metrics
    double responseQuality;
    int tokenCount;
    bool parseSuccessful;
    
    // Timestamp
    std::string timestamp;
};

struct LatencyBenchmark {
    std::string modelName;
    int totalRequests;
    double avgLatencyMs;
    double minLatencyMs;
    double maxLatencyMs;
    double p50LatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    double throughputTokensPerSecond;
};

class ModelTester {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<ResponseParser> m_parser;

    // State tracking
    std::vector<ModelTestResult> m_testResults;
    std::map<std::string, std::vector<int64_t>> m_latencyHistory; // model -> latencies

public:
    ModelTester(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics,
        std::shared_ptr<ResponseParser> parser
    );

    /**
     * Test with Ollama local model (http://localhost:11434)
     * @param modelName Model to test (e.g., "llama2", "mistral")
     * @param prompt Test prompt to send
     * @param maxTokens Maximum tokens to generate
     * @return Test result with latency measurements
     */
    ModelTestResult testWithOllama(
        const std::string& modelName,
        const std::string& prompt,
        int maxTokens = 50
    );

    /**
     * Benchmark multiple models with series of tests
     * @param modelNames Models to benchmark
     * @param testPrompts Prompts to use for benchmarking
     * @param runsPerModel How many times to run each model
     * @return Benchmark results for each model
     */
    std::vector<LatencyBenchmark> benchmarkModels(
        const std::vector<std::string>& modelNames,
        const std::vector<std::string>& testPrompts,
        int runsPerModel = 3
    );

    /**
     * Test response parsing on real model output
     * @param modelOutput Raw output from model
     * @return Parsed completions with quality scores
     */
    std::vector<ParsedCompletion> testResponseParsing(const std::string& modelOutput);

    /**
     * Measure latency distribution (percentiles)
     * @param modelName Model to measure
     * @param testCount How many requests to send
     * @return Detailed latency percentiles
     */
    LatencyBenchmark measureLatencyDistribution(
        const std::string& modelName,
        int testCount = 10
    );

    /**
     * Validate model is responding correctly
     * @param modelName Model to check
     * @param prompt Validation prompt
     * @return True if model responds with reasonable output
     */
    bool validateModelResponse(
        const std::string& modelName,
        const std::string& prompt
    );

    /**
     * Get test results
     * @return All recorded test results
     */
    const std::vector<ModelTestResult>& getTestResults() const {
        return m_testResults;
    }

    /**
     * Generate test report
     * @return Human-readable test report
     */
    std::string generateTestReport() const;

    /**
     * Export results to JSON
     * @return JSON string with all test data
     */
    std::string exportToJSON() const;

    /**
     * Reset test data
     */
    void resetResults();

private:
    /**
     * Make HTTP request to Ollama API
     * @param endpoint API endpoint (e.g., "/api/generate")
     * @param payload JSON request payload
     * @return Response text
     */
    std::string makeOllamaRequest(
        const std::string& endpoint,
        const std::string& payload
    );

    /**
     * Parse Ollama streaming response
     * @param streamResponse Raw streaming response
     * @return Extracted completion text
     */
    std::string parseOllamaStreamingResponse(const std::string& streamResponse);

    /**
     * Calculate percentiles from latency history
     * @param latencies Vector of latency values
     * @param percentile Target percentile (0-100)
     * @return Latency at percentile
     */
    double calculatePercentile(const std::vector<int64_t>& latencies, double percentile) const;

    /**
     * Score response quality
     * @param response Model response text
     * @return Quality score 0.0-1.0
     */
    double scoreResponseQuality(const std::string& response) const;
};
