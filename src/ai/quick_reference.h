// quick_reference.h - Quick reference for using the final production pipeline
//
// This is the complete API for the latency-aware, perception-optimized inference engine.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "final_production_pipeline.h"
#include <iostream>
#include <chrono>

namespace RawrXD {

// Quick start example
inline void QuickStart() {
    // Create pipeline
    FinalProductionPipeline pipeline;
    
    // Configure
    FinalProductionConfig config;
    
    // Phase 1 config
    config.phase1_config.residency_policy.max_hot_models = 2;
    config.phase1_config.residency_policy.max_warm_models = 4;
    config.phase1_config.kernel_config.fast_token_count = 5;
    config.phase1_config.kernel_config.confidence_threshold = 0.7f;
    config.phase1_config.debounce_config.fast_debounce = std::chrono::milliseconds(200);
    config.phase1_config.debounce_config.slow_debounce = std::chrono::milliseconds(80);
    config.phase1_config.prefetch_config.max_prefetch_count = 3;
    
    // Phase 2 config
    config.phase2.enable_persistent_gpu_loop = true;
    config.phase2.enable_kv_paging = true;
    config.phase2.enable_async_overlap = true;
    
    // Phase 3 config
    config.phase3.enable_predictive_scheduler = true;
    config.phase3.enable_multi_model_arbitration = true;
    config.phase3.enable_context_heat_map = true;
    
    // Initialize
    pipeline.Initialize(config);
    
    // Request completion
    CompletionRequest request;
    request.file_path = "main.cpp";
    request.file_content = "// Your code here";
    request.cursor_line = 42;
    request.cursor_column = 15;
    request.max_tokens = 100;
    
    pipeline.RequestCompletion(request, [](const CompletionResult& result) {
        std::cout << "Completion: " << result.text << "\n";
        std::cout << "Latency: " << result.latency.count() << " us\n";
        std::cout << "Kernel: " << result.kernel_used << "\n";
    });
    
    // Get statistics
    auto stats = pipeline.GetStats();
    std::cout << "First token latency: " << stats.avg_first_token_latency.count() << " us\n";
    std::cout << "Cache hit rate: " << (stats.cache_hit_rate * 100.0f) << "%\n";
    std::cout << "Predictive accuracy: " << (stats.predictive_accuracy * 100.0f) << "%\n";
    
    // Get optimization suggestions
    auto suggestions = pipeline.GetOptimizationSuggestions();
    for (const auto& suggestion : suggestions) {
        std::cout << "Suggestion: " << suggestion << "\n";
    }
    
    // Export latency profile
    pipeline.ExportLatencyProfile("latency_profile.csv");
    
    // Get breakdown report
    std::cout << pipeline.GetBreakdownReport() << "\n";
}

// Advanced example: Custom kernel switching
inline void AdvancedKernelSwitching() {
    FinalProductionPipeline pipeline;
    FinalProductionConfig config;
    
    // Custom kernel switching
    config.phase1_config.kernel_config.fast_token_count = 3;
    config.phase1_config.kernel_config.confidence_threshold = 0.8f;
    config.phase1_config.kernel_config.entropy_threshold = 2.0f;
    config.phase1_config.kernel_config.max_switches_per_gen = 3;
    
    pipeline.Initialize(config);
    
    // Request with custom kernel mode
    CompletionRequest request;
    request.file_content = "// Code requiring high quality";
    request.max_tokens = 50;
    
    pipeline.RequestCompletion(request, [](const CompletionResult& result) {
        std::cout << "High quality completion: " << result.text << "\n";
    });
}

// Advanced example: Predictive scheduling
inline void AdvancedPredictiveScheduling() {
    FinalProductionPipeline pipeline;
    FinalProductionConfig config;
    
    // Enable predictive scheduling
    config.phase3.enable_predictive_scheduler = true;
    
    pipeline.Initialize(config);
    
    // The pipeline will now:
    // 1. Learn your typing pattern
    // 2. Predict when you'll stop
    // 3. Start completions before you stop
    // 4. Have results ready when you pause
    
    CompletionRequest request;
    request.file_content = "// Your code";
    request.max_tokens = 100;
    
    pipeline.RequestCompletion(request, [](const CompletionResult& result) {
        std::cout << "Predictive completion: " << result.text << "\n";
    });
}

// Advanced example: Multi-model arbitration
inline void AdvancedMultiModelArbitration() {
    FinalProductionPipeline pipeline;
    FinalProductionConfig config;
    
    // Enable multi-model arbitration
    config.phase3.enable_multi_model_arbitration = true;
    
    pipeline.Initialize(config);
    
    // Register multiple models
    // pipeline.RegisterModel({"small", ModelTier::SMALL, ...});
    // pipeline.RegisterModel({"medium", ModelTier::MEDIUM, ...});
    // pipeline.RegisterModel({"large", ModelTier::LARGE, ...});
    
    // The pipeline will now:
    // 1. Select best model for latency budget
    // 2. Use small model for instant draft
    // 3. Refine with large model in background
    
    CompletionRequest request;
    request.file_content = "// Your code";
    request.max_tokens = 100;
    
    pipeline.RequestCompletion(request, [](const CompletionResult& result) {
        std::cout << "Arbitrated completion: " << result.text << "\n";
    });
}

// Performance tuning example
inline void PerformanceTuning() {
    FinalProductionPipeline pipeline;
    FinalProductionConfig config;
    
    // Tune for maximum speed
    config.phase1_config.residency_policy.max_hot_models = 3;
    config.phase1_config.kernel_config.fast_token_count = 10;
    config.phase1_config.early_exit_config.confidence_threshold = 0.9f;
    config.phase2.enable_persistent_gpu_loop = true;
    
    pipeline.Initialize(config);
    
    // Run benchmark
    auto start = std::chrono::steady_clock::now();
    
    CompletionRequest request;
    request.file_content = "// Benchmark code";
    request.max_tokens = 50;
    
    pipeline.RequestCompletion(request, [](const CompletionResult& result) {
        // Handle result
    });
    
    auto end = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Benchmark latency: " << latency.count() << " ms\n";
    
    // Get stats
    auto stats = pipeline.GetStats();
    std::cout << "GPU utilization: " << (stats.persistent_loop_stats.gpu_utilization * 100.0f) << "%\n";
    std::cout << "Cache hit rate: " << (stats.cache_hit_rate * 100.0f) << "%\n";
    std::cout << "Early exit rate: " << (stats.early_exit_rate * 100.0f) << "%\n";
}

} // namespace RawrXD