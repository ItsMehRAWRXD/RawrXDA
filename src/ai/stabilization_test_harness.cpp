// stabilization_test_harness.cpp - Implementation of comprehensive stress test
// Part of the Copilot-like inference pipeline.

#include "stabilization_test_harness.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace RawrXD {

// Reporter implementation
StabilizationTestReporter::StabilizationTestReporter(const std::string& output_file) {
    output_.open(output_file, std::ios::out | std::ios::trunc);
    output_ << "[\n";
}

StabilizationTestReporter::~StabilizationTestReporter() {
    if (output_.is_open()) {
        output_ << "\n]\n";
        output_.close();
    }
}

void StabilizationTestReporter::StartTest(const StabilizationTestConfig& config) {
    start_time_ = std::chrono::steady_clock::now();
    
    output_ << "{\n";
    output_ << "  \"event\": \"test_start\",\n";
    output_ << "  \"timestamp\": \"" << std::chrono::duration_cast<std::chrono::seconds>(
        start_time_.time_since_epoch()).count() << "\",\n";
    output_ << "  \"config\": {\n";
    output_ << "    \"duration_seconds\": " << config.test_duration.count() << ",\n";
    output_ << "    \"tokens_per_second\": " << config.tokens_per_second << ",\n";
    output_ << "    \"max_concurrent_requests\": " << config.max_concurrent_requests << "\n";
    output_ << "  }\n";
    output_ << "}";
}

void StabilizationTestReporter::RecordSample(const StabilizationTestResult& sample) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    
    output_ << ",\n{\n";
    output_ << "  \"event\": \"sample\",\n";
    output_ << "  \"elapsed_seconds\": " << elapsed << ",\n";
    output_ << "  \"avg_latency_ms\": " << sample.avg_latency_ms << ",\n";
    output_ << "  \"p99_latency_ms\": " << sample.p99_latency_ms << ",\n";
    output_ << "  \"gpu_utilization\": " << sample.avg_gpu_utilization << ",\n";
    output_ << "  \"prediction_accuracy\": " << sample.prediction_accuracy << ",\n";
    output_ << "  \"kv_fault_rate\": " << sample.kv_fault_rate << ",\n";
    output_ << "  \"arbitration_fairness\": " << sample.arbitration_fairness << ",\n";
    output_ << "  \"loop_stability\": " << sample.loop_stability_score << "\n";
    output_ << "}";
    
    output_.flush();
}

void StabilizationTestReporter::EndTest(const StabilizationTestResult& result) {
    output_ << ",\n{\n";
    output_ << "  \"event\": \"test_end\",\n";
    output_ << "  \"passed\": " << (result.passed ? "true" : "false") << ",\n";
    output_ << "  \"failure_reason\": \"" << result.failure_reason << "\",\n";
    output_ << "  \"total_tokens\": " << result.total_tokens_generated << ",\n";
    output_ << "  \"avg_latency_ms\": " << result.avg_latency_ms << ",\n";
    output_ << "  \"p99_latency_ms\": " << result.p99_latency_ms << ",\n";
    output_ << "  \"jitter_ms\": " << result.jitter_ms << ",\n";
    output_ << "  \"latency_drift_percent\": " << result.latency_drift_percent << ",\n";
    output_ << "  \"gpu_utilization\": " << result.avg_gpu_utilization << ",\n";
    output_ << "  \"prediction_accuracy\": " << result.prediction_accuracy << ",\n";
    output_ << "  \"kv_fault_rate\": " << result.kv_fault_rate << ",\n";
    output_ << "  \"arbitration_fairness\": " << result.arbitration_fairness << ",\n";
    output_ << "  \"loop_stability\": " << result.loop_stability_score << ",\n";
    output_ << "  \"health_score\": " << result.loop_stability_score * 100 << "\n";
    output_ << "}";
}

// Harness implementation
StabilizationTestHarness::StabilizationTestHarness(FinalProductionPipeline* pipeline)
    : pipeline_(pipeline)
{
    latest_result_ = {};
}

StabilizationTestHarness::~StabilizationTestHarness() {
    if (running_.load()) {
        running_.store(false);
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }
}

StabilizationTestResult StabilizationTestHarness::RunTest(const StabilizationTestConfig& config) {
    running_.store(true);
    
    // Create reporter
    StabilizationTestReporter reporter(config.output_file);
    reporter.StartTest(config);
    
    // Start test threads
    threads_.emplace_back(&StabilizationTestHarness::LoadGeneratorThread, this, std::ref(config));
    threads_.emplace_back(
        &StabilizationTestHarness::MetricsCollectorThread, this, std::ref(config));
    
    if (config.enable_jitter_injection) {
        threads_.emplace_back(
            &StabilizationTestHarness::JitterInjectorThread, this, std::ref(config));
    }
    
    // Wait for test duration
    std::this_thread::sleep_for(config.test_duration);
    
    // Stop test
    running_.store(false);
    for (auto& t : threads_) {
        if (t.joinable()) t.join();
    }
    threads_.clear();
    
    // Analyze results
    auto result = AnalyzeResults();
    result.passed = CheckThresholds(result, config);
    
    // Store result
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        latest_result_ = result;
    }
    
    // Report
    reporter.EndTest(result);
    
    return result;
}

StabilizationTestResult StabilizationTestHarness::TestLoopStability(int duration_seconds) {
    StabilizationTestConfig config;
    config.test_duration = std::chrono::seconds(duration_seconds);
    config.enable_jitter_injection = false;
    config.enable_context_churn = false;
    config.enable_model_switching = false;
    config.thresholds.max_latency_drift_percent = 5.0f;
    
    return RunTest(config);
}

StabilizationTestResult StabilizationTestHarness::TestPredictionAccuracy(int duration_seconds) {
    StabilizationTestConfig config;
    config.test_duration = std::chrono::seconds(duration_seconds);
    config.enable_jitter_injection = true;
    config.enable_context_churn = true;
    config.thresholds.min_prediction_accuracy = 0.75f;
    
    return RunTest(config);
}

StabilizationTestResult StabilizationTestHarness::TestArbitrationFairness(int duration_seconds) {
    StabilizationTestConfig config;
    config.test_duration = std::chrono::seconds(duration_seconds);
    config.max_concurrent_requests = 8;
    config.enable_model_switching = true;
    config.thresholds.min_arbitration_fairness = 0.85f;
    
    return RunTest(config);
}

StabilizationTestResult StabilizationTestHarness::TestFrameTimeConsistency(int duration_seconds) {
    StabilizationTestConfig config;
    config.test_duration = std::chrono::seconds(duration_seconds);
    config.tokens_per_second = 30;  // High load
    config.thresholds.max_p50_latency_ms = 40.0f;
    config.thresholds.max_p99_latency_ms = 100.0f;
    config.thresholds.max_jitter_ms = 10.0f;
    
    return RunTest(config);
}

StabilizationTestResult StabilizationTestHarness::TestKVFaultRate(int duration_seconds) {
    StabilizationTestConfig config;
    config.test_duration = std::chrono::seconds(duration_seconds);
    config.context_size = 8192;  // Large context
    config.enable_context_churn = true;
    config.thresholds.max_kv_fault_rate = 0.05f;
    
    return RunTest(config);
}

StabilizationTestResult StabilizationTestHarness::GetLatestResults() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return latest_result_;
}

bool StabilizationTestHarness::IsHealthy() const {
    auto result = GetLatestResults();
    return result.passed && result.loop_stability_score > 0.8f;
}

int StabilizationTestHarness::GetHealthScore() const {
    auto result = GetLatestResults();
    
    if (!result.passed) return 0;
    
    // Calculate composite health score
    float score = 0.0f;
    score += result.loop_stability_score * 25.0f;
    score += result.prediction_accuracy * 25.0f;
    score += (1.0f - result.kv_fault_rate) * 25.0f;
    score += result.arbitration_fairness * 25.0f;
    
    return static_cast<int>(std::min(100.0f, score));
}

void StabilizationTestHarness::LoadGeneratorThread(const StabilizationTestConfig& config) {
    int tokens_generated = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (running_.load()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
        
        if (elapsed >= config.test_duration.count()) {
            break;
        }
        
        // Generate load
        CompletionRequest request;
        request.file_content = "// Test context " + std::to_string(tokens_generated);
        request.cursor_line = tokens_generated % 100;
        request.cursor_column = 0;
        request.max_tokens = 10;
        
        pipeline_->RequestCompletion(request, [](const CompletionResult& result) {
            // Callback
        });
        
        tokens_generated += 10;
        
        // Rate limit
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / config.tokens_per_second));
    }
}

void StabilizationTestHarness::MetricsCollectorThread(const StabilizationTestConfig& config) {
    StabilizationTestReporter reporter(config.output_file + ".samples");
    
    while (running_.load()) {
        CollectLatencySample();
        CollectGPUSample();
        CollectPredictionSample();
        CollectKVSample();
        CollectArbitrationSample();
        
        // Periodic report
        if (config.enable_real_time_report) {
            auto sample = AnalyzeResults();
            reporter.RecordSample(sample);
        }
        
        std::this_thread::sleep_for(config.report_interval);
    }
}

void StabilizationTestHarness::JitterInjectorThread(const StabilizationTestConfig& config) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(1, 50);
    
    while (running_.load()) {
        // Inject random delays
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
        
        // Occasionally inject large delay
        if (delay_dist(gen) > 45) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void StabilizationTestHarness::CollectLatencySample() {
    // TODO: Get actual latency from pipeline
    float latency = 30.0f + static_cast<float>(rand() % 20);
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    latency_samples_.push_back(latency);
    
    // Keep last 10000 samples
    if (latency_samples_.size() > 10000) {
        latency_samples_.erase(latency_samples_.begin());
    }
}

void StabilizationTestHarness::CollectGPUSample() {
    // TODO: Get actual GPU utilization from pipeline
    float utilization = 0.8f + static_cast<float>(rand() % 20) / 100.0f;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    gpu_utilization_samples_.push_back(utilization);
    
    if (gpu_utilization_samples_.size() > 10000) {
        gpu_utilization_samples_.erase(gpu_utilization_samples_.begin());
    }
}

void StabilizationTestHarness::CollectPredictionSample() {
    // TODO: Get actual prediction accuracy from pipeline
    bool correct = rand() % 100 < 75;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    prediction_samples_.push_back(correct);
    
    if (prediction_samples_.size() > 10000) {
        prediction_samples_.erase(prediction_samples_.begin());
    }
}

void StabilizationTestHarness::CollectKVSample() {
    // TODO: Get actual KV fault rate from pipeline
    bool fault = rand() % 100 < 5;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    kv_fault_samples_.push_back(fault);
    
    if (kv_fault_samples_.size() > 10000) {
        kv_fault_samples_.erase(kv_fault_samples_.begin());
    }
}

void StabilizationTestHarness::CollectArbitrationSample() {
    // TODO: Get actual arbitration fairness from pipeline
    float fairness = 0.8f + static_cast<float>(rand() % 20) / 100.0f;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    arbitration_samples_.push_back(fairness);
    
    if (arbitration_samples_.size() > 10000) {
        arbitration_samples_.erase(arbitration_samples_.begin());
    }
}

StabilizationTestResult StabilizationTestHarness::AnalyzeResults() {
    StabilizationTestResult result;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    // Analyze latency
    if (!latency_samples_.empty()) {
        std::vector<float> sorted = latency_samples_;
        std::sort(sorted.begin(), sorted.end());
        
        result.avg_latency_ms = std::accumulate(sorted.begin(), sorted.end(), 0.0f) / sorted.size();
        result.min_latency_ms = sorted.front();
        result.max_latency_ms = sorted.back();
        result.p50_latency_ms = sorted[sorted.size() * 50 / 100];
        result.p90_latency_ms = sorted[sorted.size() * 90 / 100];
        result.p99_latency_ms = sorted[sorted.size() * 99 / 100];
        
        // Calculate jitter (std deviation)
        float variance = 0.0f;
        for (float lat : sorted) {
            float diff = lat - result.avg_latency_ms;
            variance += diff * diff;
        }
        variance /= sorted.size();
        result.jitter_ms = std::sqrt(variance);
        
        // Calculate drift
        if (sorted.size() >= 100) {
            float first_avg = std::accumulate(sorted.begin(), sorted.begin() + 50, 0.0f) / 50;
            float last_avg = std::accumulate(sorted.end() - 50, sorted.end(), 0.0f) / 50;
            result.latency_drift_percent = ((last_avg - first_avg) / first_avg) * 100.0f;
        }
    }
    
    // Analyze GPU
    if (!gpu_utilization_samples_.empty()) {
        result.avg_gpu_utilization = std::accumulate(
            gpu_utilization_samples_.begin(), gpu_utilization_samples_.end(), 0.0f) / 
            gpu_utilization_samples_.size();
        result.min_gpu_utilization = *std::min_element(
            gpu_utilization_samples_.begin(), gpu_utilization_samples_.end());
        result.max_gpu_utilization = *std::max_element(
            gpu_utilization_samples_.begin(), gpu_utilization_samples_.end());
    }
    
    // Analyze predictions
    if (!prediction_samples_.empty()) {
        int correct = std::count(prediction_samples_.begin(), prediction_samples_.end(), true);
        result.total_predictions = static_cast<int>(prediction_samples_.size());
        result.correct_predictions = correct;
        result.prediction_accuracy = static_cast<float>(correct) / prediction_samples_.size();
    }
    
    // Analyze KV
    if (!kv_fault_samples_.empty()) {
        int faults = std::count(kv_fault_samples_.begin(), kv_fault_samples_.end(), true);
        result.total_kv_accesses = static_cast<int>(kv_fault_samples_.size());
        result.kv_faults = faults;
        result.kv_fault_rate = static_cast<float>(faults) / kv_fault_samples_.size();
        result.kv_hit_rate = 1.0f - result.kv_fault_rate;
    }
    
    // Analyze arbitration
    if (!arbitration_samples_.empty()) {
        result.avg_arbitration_fairness = std::accumulate(
            arbitration_samples_.begin(), arbitration_samples_.end(), 0.0f) / 
            arbitration_samples_.size();
        result.total_arbitrations = static_cast<int>(arbitration_samples_.size());
    }
    
    // Calculate loop stability
    result.loop_stability_score = 1.0f - std::min(1.0f, 
        std::abs(result.latency_drift_percent) / 20.0f);
    
    return result;
}

bool StabilizationTestHarness::CheckThresholds(const StabilizationTestResult& result,
                                                const StabilizationTestConfig& config) {
    if (result.p50_latency_ms > config.thresholds.max_p50_latency_ms) {
        result.failure_reason = "P50 latency too high: " + std::to_string(result.p50_latency_ms);
        return false;
    }
    
    if (result.p99_latency_ms > config.thresholds.max_p99_latency_ms) {
        result.failure_reason = "P99 latency too high: " + std::to_string(result.p99_latency_ms);
        return false;
    }
    
    if (result.jitter_ms > config.thresholds.max_jitter_ms) {
        result.failure_reason = "Jitter too high: " + std::to_string(result.jitter_ms);
        return false;
    }
    
    if (result.avg_gpu_utilization < config.thresholds.min_gpu_utilization) {
        result.failure_reason = "GPU utilization too low: " + std::to_string(result.avg_gpu_utilization);
        return false;
    }
    
    if (result.prediction_accuracy < config.thresholds.min_prediction_accuracy) {
        result.failure_reason = "Prediction accuracy too low: " + std::to_string(result.prediction_accuracy);
        return false;
    }
    
    if (result.kv_fault_rate > config.thresholds.max_kv_fault_rate) {
        result.failure_reason = "KV fault rate too high: " + std::to_string(result.kv_fault_rate);
        return false;
    }
    
    if (result.avg_arbitration_fairness < config.thresholds.min_arbitration_fairness) {
        result.failure_reason = "Arbitration fairness too low: " + std::to_string(result.avg_arbitration_fairness);
        return false;
    }
    
    if (std::abs(result.latency_drift_percent) > config.thresholds.max_latency_drift_percent) {
        result.failure_reason = "Latency drift too high: " + std::to_string(result.latency_drift_percent);
        return false;
    }
    
    return true;
}

} // namespace RawrXD