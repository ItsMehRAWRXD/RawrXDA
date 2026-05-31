// stabilization_test_harness.h - Comprehensive stress test for all 5 critical points
// Validates: loop stability, prediction accuracy, arbitration fairness, 
//            frame-time consistency, KV fault rate
//
// This is where we prove the system stays stable, predictive, and fair.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "final_production_pipeline.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD {

// Test configuration
struct StabilizationTestConfig {
    // Duration
    std::chrono::seconds test_duration{300};  // 5 minutes default
    
    // Load parameters
    int tokens_per_second = 20;           // Target generation rate
    int max_concurrent_requests = 4;      // Max parallel completions
    int context_size = 4096;              // Context window size
    
    // Stress parameters
    bool enable_jitter_injection = true;   // Inject random delays
    bool enable_context_churn = true;      // Rapidly change context
    bool enable_model_switching = true;   // Switch models mid-test
    bool enable_thermal_stress = false;    // Run until thermal throttle
    
    // Thresholds
    struct {
        float max_p50_latency_ms = 50.0f;
        float max_p99_latency_ms = 150.0f;
        float max_jitter_ms = 20.0f;
        float min_gpu_utilization = 0.75f;
        float min_prediction_accuracy = 0.70f;
        float max_kv_fault_rate = 0.10f;
        float min_arbitration_fairness = 0.80f;
        float max_latency_drift_percent = 10.0f;
    } thresholds;
    
    // Reporting
    bool enable_real_time_report = true;
    std::chrono::seconds report_interval{10};
    std::string output_file = "stabilization_report.json";
};

// Test result
struct StabilizationTestResult {
    bool passed;
    std::string failure_reason;
    
    // Timing
    std::chrono::seconds duration;
    int total_tokens_generated;
    int total_requests_completed;
    
    // Latency stats
    float avg_latency_ms;
    float p50_latency_ms;
    float p90_latency_ms;
    float p99_latency_ms;
    float min_latency_ms;
    float max_latency_ms;
    float jitter_ms;  // std deviation
    float latency_drift_percent;  // (end - start) / start
    
    // GPU stats
    float avg_gpu_utilization;
    float min_gpu_utilization;
    float max_gpu_utilization;
    int gpu_idle_events;
    
    // Prediction stats
    float prediction_accuracy;
    int total_predictions;
    int correct_predictions;
    int early_predictions;
    int late_predictions;
    
    // KV stats
    float kv_fault_rate;
    int total_kv_accesses;
    int kv_faults;
    float kv_hit_rate;
    
    // Arbitration stats
    float arbitration_fairness;
    int total_arbitrations;
    int starved_requests;
    int priority_inversions;
    
    // Loop stability
    float loop_stability_score;  // 0-1, higher is better
    int loop_resets;
    int loop_degradation_events;
    
    // Thermal
    float avg_temperature;
    float max_temperature;
    bool thermal_throttled;
};

// Real-time test reporter
class StabilizationTestReporter {
public:
    StabilizationTestReporter(const std::string& output_file);
    ~StabilizationTestReporter();
    
    void StartTest(const StabilizationTestConfig& config);
    void RecordSample(const StabilizationTestResult& sample);
    void EndTest(const StabilizationTestResult& result);
    
private:
    std::ofstream output_;
    std::chrono::steady_clock::time_point start_time_;
};

// Main test harness
class StabilizationTestHarness {
public:
    StabilizationTestHarness(FinalProductionPipeline* pipeline);
    ~StabilizationTestHarness();
    
    // Run full stabilization test
    StabilizationTestResult RunTest(const StabilizationTestConfig& config);
    
    // Run individual stress tests
    StabilizationTestResult TestLoopStability(int duration_seconds);
    StabilizationTestResult TestPredictionAccuracy(int duration_seconds);
    StabilizationTestResult TestArbitrationFairness(int duration_seconds);
    StabilizationTestResult TestFrameTimeConsistency(int duration_seconds);
    StabilizationTestResult TestKVFaultRate(int duration_seconds);
    
    // Get latest results
    StabilizationTestResult GetLatestResults() const;
    
    // Check if system is healthy
    bool IsHealthy() const;
    
    // Get health score (0-100)
    int GetHealthScore() const;
    
private:
    // Test threads
    void LoadGeneratorThread(const StabilizationTestConfig& config);
    void MetricsCollectorThread(const StabilizationTestConfig& config);
    void JitterInjectorThread(const StabilizationTestConfig& config);
    
    // Metrics
    void CollectLatencySample();
    void CollectGPUSample();
    void CollectPredictionSample();
    void CollectKVSample();
    void CollectArbitrationSample();
    
    // Analysis
    StabilizationTestResult AnalyzeResults();
    bool CheckThresholds(const StabilizationTestResult& result, 
                        const StabilizationTestConfig& config);
    
    // Members
    FinalProductionPipeline* pipeline_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::vector<std::thread> threads_;
    
    // Metrics storage
    mutable std::mutex metrics_mutex_;
    std::vector<float> latency_samples_;
    std::vector<float> gpu_utilization_samples_;
    std::vector<bool> prediction_samples_;
    std::vector<bool> kv_fault_samples_;
    std::vector<float> arbitration_samples_;
    
    // Latest result
    mutable std::mutex result_mutex_;
    StabilizationTestResult latest_result_;
};

} // namespace RawrXD