// ============================================================================
// swarm_saturation_test.hpp — Tri-Lane Saturation Test Harness
// ============================================================================
//
// Purpose: Empirically measure the latency impact of running three concurrent
// orchestration lanes (Primary, Verifier, Librarian) on the SwarmOrchestrator.
//
// Methodology:
//   1. Spawn 100 concurrent inference requests
//   2. Enable/disable Librarian lane via policy
//   3. Measure P50/P99/P99.9 TTFT (time-to-first-token)
//   4. Compare dual-lane vs. tri-lane overhead
//   5. Report backpressure (queue depth, pending lookups, dispatch count)
//
// Metrics Collected:
//   - Request latencies (ms)
//   - Queue depths at steady state
//   - Dispatch counts (Librarian lane activity)
//   - Memory consumption during test
//   - Thread context switches (from perfmon, if available)
//
// Output:
//   - CSV results: request_id, TTFT (ms), queue_depth_P, queue_depth_V, queue_depth_L, status
//   - JSON summary: p50, p99, p99.9, mean, stddev, baseline comparison
//   - Console report: Verdict ("RAG integration is SAFE" or "caution: scale back")
//
// ============================================================================
#pragma once

#include "swarm_orchestrator.h"
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace RawrXD::test {

using json = nlohmann::json;
using TimePoint = std::chrono::high_resolution_clock::time_point;
using Duration = std::chrono::high_resolution_clock::duration;

struct SaturationTestConfig {
    std::uint32_t numConcurrentRequests = 100;
    std::uint32_t tokensPerRequest = 64;
    std::uint32_t verifierChecksPerRequest = 32;  // Every N tokens
    bool enableLibrarian = true;
    std::uint32_t librarianDispatchEveryTokens = 32;
    std::uint32_t librarianMinPulseBytes = 1024;
    std::uint32_t librarianMaxPendingLookups = 8;
    std::string outputDir = "d:\\saturation_test_results";
    bool verboseLogging = true;
};

struct RequestMetrics {
    std::uint64_t requestId = 0;
    TimePoint submitTime;
    TimePoint firstTokenTime;
    double ttft_ms = 0.0;
    int queueDepthPrimary = 0;
    int queueDepthVerifier = 0;
    int queueDepthLibrarian = 0;
    std::string status = "OK";
};

struct SaturationTestResult {
    std::string testId;
    SaturationTestConfig config;
    std::vector<RequestMetrics> metrics;
    
    // Percentiles
    double p50_ms = 0.0;
    double p99_ms = 0.0;
    double p99_9_ms = 0.0;
    double meanTtft_ms = 0.0;
    double stddevTtft_ms = 0.0;
    
    // Baseline comparison (set when comparing with dual-lane run)
    double baselineTtft_ms = 0.0;
    double overheadPercent = 0.0;
    
    // Engine telemetry (from SwarmOrchestrator::getStatus())
    uint64_t librarianDispatchCount = 0;
    uint32_t maxQueueDepth = 0;
    double testDurationSec = 0.0;
    
    std::string verdict;  // "SAFE", "CAUTION", or "SATURATED"
};

class SwarmSaturationTester {
public:
    explicit SwarmSaturationTester(SwarmOrchestrator* orchestra)
        : m_orchestra(orchestra), m_currentRequestId(1000000) {}

    /// Run saturation test with given configuration
    SaturationTestResult runSaturationTest(const SaturationTestConfig& config);

    /// Compare two test results (baseline vs. current)
    static void compareResults(
        const SaturationTestResult& baseline,
        const SaturationTestResult& current,
        SaturationTestResult& out_comparison);

    /// Export results to CSV and JSON
    static bool exportResults(
        const SaturationTestResult& result,
        const std::string& outputDir);

    /// Generate console report with verdict
    static std::string generateReport(const SaturationTestResult& result);

private:
    SwarmOrchestrator* m_orchestra;
    std::uint64_t m_currentRequestId;

    /// Simulate a single inference request by submitting pulses to all three lanes
    RequestMetrics simulateInferenceRequest(
        std::uint64_t requestId,
        const SaturationTestConfig& config);

    /// Calculate percentile from sorted array
    static double calculatePercentile(const std::vector<double>& sortedValues, double percentile);

    /// Compute latency statistics
    static void computeStatistics(SaturationTestResult& result);
};

} // namespace RawrXD::test
