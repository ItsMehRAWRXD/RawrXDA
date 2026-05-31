// main_stabilization_demo.cpp - Complete stabilization and observability demo
// This is the entry point that proves the system stays stable, predictive, and fair.
//
// Usage:
//   ./stabilization_demo --test=all --duration=300
//   ./stabilization_demo --dashboard --refresh=100
//   ./stabilization_demo --tune --param=scheduler.confidence --value=0.8
//
// Part of the Copilot-like inference pipeline.

#include "final_production_pipeline.h"
#include "stabilization_test_harness.h"
#include "real_time_dashboard.h"
#include "live_parameter_tuning.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace RawrXD;

void PrintUsage() {
    std::cout << "RawrXD Stabilization & Observability Demo\n";
    std::cout << "==========================================\n\n";
    std::cout << "Usage:\n";
    std::cout << "  --test=<type>     Run stress test (all|loop|prediction|arbitration|frame|kv)\n";
    std::cout << "  --duration=<sec>  Test duration in seconds (default: 300)\n";
    std::cout << "  --dashboard      Show real-time dashboard\n";
    std::cout << "  --refresh=<ms>   Dashboard refresh rate in ms (default: 1000)\n";
    std::cout << "  --tune           Enter live parameter tuning mode\n";
    std::cout << "  --param=<name>   Parameter name to tune\n";
    std::cout << "  --value=<val>    Parameter value\n";
    std::cout << "  --export=<file> Export results to file\n";
    std::cout << "  --help           Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  ./stabilization_demo --test=all --duration=300\n";
    std::cout << "  ./stabilization_demo --dashboard --refresh=100\n";
    std::cout << "  ./stabilization_demo --tune --param=scheduler.confidence --value=0.8\n";
}

void RunStressTest(const std::string& test_type, int duration_seconds) {
    std::cout << "\n=== Running Stress Test: " << test_type << " ===\n\n";
    
    // Create pipeline
    FinalProductionConfig config;
    config.phase1_config.residency_policy.max_hot_models = 2;
    config.phase1_config.residency_policy.max_warm_models = 4;
    config.phase1_config.kernel_config.fast_token_count = 5;
    config.phase1_config.kernel_config.confidence_threshold = 0.7f;
    config.phase1_config.debounce_config.fast_debounce = std::chrono::milliseconds(200);
    config.phase1_config.debounce_config.slow_debounce = std::chrono::milliseconds(80);
    config.phase1_config.prefetch_config.max_prefetch_count = 3;
    config.phase2.enable_persistent_gpu_loop = true;
    config.phase2.enable_kv_paging = true;
    config.phase2.enable_async_overlap = true;
    config.phase3.enable_predictive_scheduler = true;
    config.phase3.enable_multi_model_arbitration = true;
    config.phase3.enable_context_heat_map = true;
    
    auto pipeline = std::make_unique<FinalProductionPipeline>();
    if (!pipeline->Initialize(config)) {
        std::cerr << "Failed to initialize pipeline\n";
        return;
    }
    
    // Create test harness
    StabilizationTestHarness harness(pipeline.get());
    
    // Run test
    StabilizationTestResult result;
    
    if (test_type == "all") {
        result = harness.RunTest([&]() {
            StabilizationTestConfig test_config;
            test_config.test_duration = std::chrono::seconds(duration_seconds);
            test_config.tokens_per_second = 20;
            test_config.max_concurrent_requests = 4;
            test_config.enable_jitter_injection = true;
            test_config.enable_context_churn = true;
            test_config.enable_model_switching = true;
            return test_config;
        }());
    } else if (test_type == "loop") {
        result = harness.TestLoopStability(duration_seconds);
    } else if (test_type == "prediction") {
        result = harness.TestPredictionAccuracy(duration_seconds);
    } else if (test_type == "arbitration") {
        result = harness.TestArbitrationFairness(duration_seconds);
    } else if (test_type == "frame") {
        result = harness.TestFrameTimeConsistency(duration_seconds);
    } else if (test_type == "kv") {
        result = harness.TestKVFaultRate(duration_seconds);
    } else {
        std::cerr << "Unknown test type: " << test_type << "\n";
        return;
    }
    
    // Print results
    std::cout << "\n=== Test Results ===\n\n";
    std::cout << "Status: " << (result.passed ? "✅ PASSED" : "❌ FAILED") << "\n";
    if (!result.passed) {
        std::cout << "Reason: " << result.failure_reason << "\n";
    }
    std::cout << "Duration: " << result.duration.count() << " seconds\n";
    std::cout << "Tokens Generated: " << result.total_tokens_generated << "\n";
    std::cout << "Requests Completed: " << result.total_requests_completed << "\n\n";
    
    std::cout << "Latency:\n";
    std::cout << "  Average: " << result.avg_latency_ms << " ms\n";
    std::cout << "  P50: " << result.p50_latency_ms << " ms\n";
    std::cout << "  P90: " << result.p90_latency_ms << " ms\n";
    std::cout << "  P99: " << result.p99_latency_ms << " ms\n";
    std::cout << "  Jitter: " << result.jitter_ms << " ms\n";
    std::cout << "  Drift: " << result.latency_drift_percent << "%\n\n";
    
    std::cout << "GPU:\n";
    std::cout << "  Utilization: " << (result.avg_gpu_utilization * 100.0f) << "%\n";
    std::cout << "  Min: " << (result.min_gpu_utilization * 100.0f) << "%\n";
    std::cout << "  Max: " << (result.max_gpu_utilization * 100.0f) << "%\n\n";
    
    std::cout << "Prediction:\n";
    std::cout << "  Accuracy: " << (result.prediction_accuracy * 100.0f) << "%\n";
    std::cout << "  Total: " << result.total_predictions << "\n";
    std::cout << "  Correct: " << result.correct_predictions << "\n\n";
    
    std::cout << "KV:\n";
    std::cout << "  Fault Rate: " << (result.kv_fault_rate * 100.0f) << "%\n";
    std::cout << "  Hit Rate: " << (result.kv_hit_rate * 100.0f) << "%\n";
    std::cout << "  Faults: " << result.kv_faults << " / " << result.total_kv_accesses << "\n\n";
    
    std::cout << "Arbitration:\n";
    std::cout << "  Fairness: " << (result.arbitration_fairness * 100.0f) << "%\n";
    std::cout << "  Starved: " << result.starved_requests << "\n\n";
    
    std::cout << "Loop:\n";
    std::cout << "  Stability: " << (result.loop_stability_score * 100.0f) << "%\n";
    std::cout << "  Resets: " << result.loop_resets << "\n";
    std::cout << "  Degradation: " << result.loop_degradation_events << "\n\n";
    
    std::cout << "Health Score: " << harness.GetHealthScore() << "/100\n";
}

void RunDashboard(int refresh_ms) {
    std::cout << "\n=== Starting Real-Time Dashboard ===\n\n";
    
    // Create pipeline
    FinalProductionConfig config;
    auto pipeline = std::make_unique<FinalProductionPipeline>();
    if (!pipeline->Initialize(config)) {
        std::cerr << "Failed to initialize pipeline\n";
        return;
    }
    
    // Create dashboard
    RealTimeDashboard dashboard(pipeline.get());
    auto renderer = std::make_unique<ConsoleDashboardRenderer>();
    dashboard.Initialize(std::move(renderer));
    dashboard.SetRefreshRate(std::chrono::milliseconds(refresh_ms));
    
    // Start dashboard
    dashboard.Start();
    
    std::cout << "Dashboard running. Press Enter to stop...\n";
    std::cin.get();
    
    // Stop dashboard
    dashboard.Stop();
    
    // Export final snapshot
    std::cout << "\nFinal Snapshot:\n";
    std::cout << dashboard.ExportSnapshotJSON() << "\n";
}

void RunParameterTuning(const std::string& param_name, const std::string& param_value) {
    std::cout << "\n=== Live Parameter Tuning ===\n\n";
    
    // Create pipeline
    FinalProductionConfig config;
    auto pipeline = std::make_unique<FinalProductionPipeline>();
    if (!pipeline->Initialize(config)) {
        std::cerr << "Failed to initialize pipeline\n";
        return;
    }
    
    // Create tuner
    LiveParameterTuner tuner(pipeline.get());
    
    // Register parameters
    ParameterDef scheduler_confidence;
    scheduler_confidence.name = "scheduler.confidence";
    scheduler_confidence.description = "Scheduler confidence threshold";
    scheduler_confidence.category = "scheduler";
    scheduler_confidence.type = ParameterType::FLOAT;
    scheduler_confidence.range.min_float = 0.0f;
    scheduler_confidence.range.max_float = 1.0f;
    scheduler_confidence.is_hot_swappable = true;
    tuner.RegisterParameter(scheduler_confidence);
    
    ParameterDef kv_page_size;
    kv_page_size.name = "kv.page_size";
    kv_page_size.description = "KV cache page size";
    kv_page_size.category = "kv";
    kv_page_size.type = ParameterType::INT;
    kv_page_size.range.min_int = 64;
    kv_page_size.range.max_int = 1024;
    kv_page_size.is_hot_swappable = true;
    tuner.RegisterParameter(kv_page_size);
    
    ParameterDef arbitration_fairness;
    arbitration_fairness.name = "arbitration.fairness";
    arbitration_fairness.description = "Arbitration fairness threshold";
    arbitration_fairness.category = "arbitration";
    arbitration_fairness.type = ParameterType::FLOAT;
    arbitration_fairness.range.min_float = 0.0f;
    arbitration_fairness.range.max_float = 1.0f;
    arbitration_fairness.is_hot_swappable = true;
    tuner.RegisterParameter(arbitration_fairness);
    
    // Set parameter if provided
    if (!param_name.empty() && !param_value.empty()) {
        std::cout << "Setting parameter: " << param_name << " = " << param_value << "\n";
        
        if (tuner.SetParameterString(param_name, param_value)) {
            std::cout << "✅ Parameter set successfully\n";
        } else {
            std::cout << "❌ Failed to set parameter\n";
        }
    }
    
    // Show current parameters
    std::cout << "\nCurrent Parameters:\n";
    auto params = tuner.GetAllParameters();
    for (const auto& param : params) {
        auto value = tuner.GetParameter(param.name);
        std::cout << "  " << param.name << " = ";
        
        switch (value.type) {
            case ParameterType::FLOAT:
                std::cout << value.float_value;
                break;
            case ParameterType::INT:
                std::cout << value.int_value;
                break;
            case ParameterType::BOOL:
                std::cout << (value.bool_value ? "true" : "false");
                break;
            case ParameterType::STRING:
            case ParameterType::ENUM:
                std::cout << value.string_value;
                break;
        }
        
        std::cout << " (" << param.description << ")\n";
    }
    
    // Export parameters
    std::cout << "\nExported Parameters:\n";
    std::cout << tuner.ExportParametersJSON() << "\n";
}

int main(int argc, char* argv[]) {
    // Parse arguments
    std::string test_type;
    int duration_seconds = 300;
    bool show_dashboard = false;
    int refresh_ms = 1000;
    bool tune_mode = false;
    std::string param_name;
    std::string param_value;
    std::string export_file;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            return 0;
        } else if (arg.substr(0, 7) == "--test=") {
            test_type = arg.substr(7);
        } else if (arg.substr(0, 11) == "--duration=") {
            duration_seconds = std::stoi(arg.substr(11));
        } else if (arg == "--dashboard") {
            show_dashboard = true;
        } else if (arg.substr(0, 10) == "--refresh=") {
            refresh_ms = std::stoi(arg.substr(10));
        } else if (arg == "--tune") {
            tune_mode = true;
        } else if (arg.substr(0, 8) == "--param=") {
            param_name = arg.substr(8);
        } else if (arg.substr(0, 8) == "--value=") {
            param_value = arg.substr(8);
        } else if (arg.substr(0, 9) == "--export=") {
            export_file = arg.substr(9);
        }
    }
    
    // Run requested mode
    if (!test_type.empty()) {
        RunStressTest(test_type, duration_seconds);
    } else if (show_dashboard) {
        RunDashboard(refresh_ms);
    } else if (tune_mode) {
        RunParameterTuning(param_name, param_value);
    } else {
        PrintUsage();
        return 1;
    }
    
    return 0;
}