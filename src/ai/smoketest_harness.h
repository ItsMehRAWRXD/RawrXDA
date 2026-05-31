// smoketest_harness.h - Comprehensive smoketest harness for production pipeline validation
// Validates 5 critical stress points:
//   1. Dispatch Elimination (GPU submissions ~0)
//   2. Frame-Time Consistency (p50 vs p99)
//   3. KV Fault Rate (missing pages, late fetches)
//   4. Overlap Efficiency (GPU active %, PCIe bandwidth)
//   5. Arbitration Fairness (starvation, cache poisoning)
//
// This proves the system stays stable, predictive, and fair under pressure.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "final_production_pipeline.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace RawrXD {

// Test result
struct TestResult {
    std::string name;
    bool passed;
    std::string message;
    std::chrono::microseconds duration;
    std::vector<std::pair<std::string, std::string>> metrics;
};

// Smoketest configuration
struct SmoketestConfig {
    int warmup_tokens = 10;           // Tokens to warmup before measuring
    int measurement_tokens = 100;     // Tokens to measure
    int long_session_tokens = 1000;   // Tokens for long session test
    int concurrent_requests = 4;      // Concurrent requests for arbitration test
    std::chrono::seconds test_timeout{30};  // Max test duration
    bool verbose = true;              // Print detailed output
    bool generate_report = true;      // Generate HTML report
    std::string report_path = "smoketest_report.html";
};

// Smoketest harness
class SmoketestHarness {
public:
    SmoketestHarness();
    ~SmoketestHarness();
    
    // Initialize harness
    bool Initialize(const SmoketestConfig& config);
    
    // Run all tests
    std::vector<TestResult> RunAllTests();
    
    // Individual tests
    TestResult TestDispatchElimination();
    TestResult TestFrameTimeConsistency();
    TestResult TestKVFaultRate();
    TestResult TestOverlapEfficiency();
    TestResult TestArbitrationFairness();
    
    // Long session stability test
    TestResult TestLongSessionStability();
    
    // Thermal stability test
    TestResult TestThermalStability();
    
    // Generate report
    std::string GenerateReport() const;
    
    // Export report to file
    void ExportReport(const std::string& filename) const;
    
    // Get summary
    std::string GetSummary() const;
    
private:
    // Run single test with timing
    TestResult RunTest(
        const std::string& name,
        std::function<TestResult()> test_fn
    );
    
    // Measure GPU submissions
    int MeasureGPUSubmissions();
    
    // Measure frame time distribution
    std::vector<std::chrono::microseconds> MeasureFrameTimes(int count);
    
    // Measure KV faults
    std::pair<int, int> MeasureKVFaults();
    
    // Measure GPU utilization
    float MeasureGPUUtilization();
    
    // Measure PCIe bandwidth
    float MeasurePCIeBandwidth();
    
    // Measure arbitration fairness
    std::vector<float> MeasureArbitrationFairness();
    
    // Members
    SmoketestConfig config_;
    std::unique_ptr<FinalProductionPipeline> pipeline_;
    std::vector<TestResult> results_;
    
    // Timing
    std::chrono::steady_clock::time_point test_start_;
};

// Inline implementations

inline std::string SmoketestHarness::GetSummary() const {
    std::ostringstream summary;
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : results_) {
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }
    
    summary << "=== Smoketest Summary ===\n";
    summary << "Total tests: " << results_.size() << "\n";
    summary << "Passed: " << passed << "\n";
    summary << "Failed: " << failed << "\n";
    summary << "Success rate: " << (static_cast<float>(passed) / results_.size() * 100.0f) << "%\n";
    
    return summary.str();
}

} // namespace RawrXD