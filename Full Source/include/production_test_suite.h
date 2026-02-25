#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <atomic>

#include "ai_integration_hub.h"
#include "logging/logger.h"
#include "metrics/metrics.h"

struct TestResult {
    std::string testName;
    bool passed;
    std::string message;
    double durationMs;
    std::vector<std::string> warnings;
};

struct PerformanceBenchmark {
    std::string operation;
    double avgLatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    size_t requestCount;
    size_t errorCount;
    double throughput;
};

class ProductionTestSuite {
private:
    std::shared_ptr<AIIntegrationHub> m_aiHub;
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

    std::vector<TestResult> m_results;
    std::vector<PerformanceBenchmark> m_benchmarks;

public:
    ProductionTestSuite(std::shared_ptr<AIIntegrationHub> aiHub);

    // Comprehensive test suite
    bool runFullTestSuite();

    // Individual test categories
    bool testModelLoading();
    bool testCompletionEngine();
    bool testSmartRewriteEngine();
    bool testMultiModalRouting();
    bool testLanguageServerFeatures();
    bool testPerformanceOptimization();
    bool testAdvancedCodingAgent();

    // Performance benchmarks
    bool benchmarkCompletionLatency();
    bool benchmarkRewritePerformance();
    bool benchmarkAgentFeatures();

    // Error handling tests
    bool testErrorRecovery();
    bool testResourceCleanup();

    // Integration tests
    bool testEndToEndWorkflow();

    // Results
    std::vector<TestResult> getResults() const { return m_results; }
    std::vector<PerformanceBenchmark> getBenchmarks() const { return m_benchmarks; }

    std::string generateReport() const;
    bool isProductionReady() const;

private:
    void recordResult(const std::string& testName, bool passed,
                     const std::string& message = "", double durationMs = 0.0);

    void recordBenchmark(const std::string& operation, double avgLatency,
                        double p95, double p99, size_t requests, size_t errors);
};
