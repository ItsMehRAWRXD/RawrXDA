// ============================================================================
// vulkan_inference_engine_instrumented.h — Timing Breakdown Integration
// ============================================================================
// Wraps VulkanInferenceEngine to inject three-clock timing instrumentation
// ============================================================================

#pragma once

#include "vulkan_inference_engine.h"
#include "telemetry/inference_timing_breakdown.h"
#include <memory>
#include <string>
#include <iostream>

namespace RawrXD {

class VulkanInferenceEngineInstrumented {
public:
    VulkanInferenceEngineInstrumented(std::unique_ptr<VulkanInferenceEngine> engine)
        : engine_(std::move(engine)) {
    }
    
    // Proxy methods to underlying engine
    bool LoadModel(const std::string& path) {
        return engine_->LoadModel(path);
    }
    
    bool IsModelLoaded() const {
        return engine_->IsModelLoaded();
    }
    
    void UnloadModel() {
        engine_->UnloadModel();
    }
    
    std::vector<int32_t> Tokenize(const std::string& text) {
        return engine_->Tokenize(text);
    }
    
    // Instrumented Generate with three-clock breakdown
    std::vector<int32_t> GenerateWithTiming(
        const std::vector<int32_t>& tokens, 
        int maxTokens,
        Telemetry::BatchTimingOrchestrator::BatchResult& out_result
    ) {
        Telemetry::BatchTimingOrchestrator orchestrator;
        orchestrator.start_batch(maxTokens);
        auto& tracker = orchestrator.get_tracker();
        
        std::vector<int32_t> result;
        
        // Phase 1: Setup and initial memory loading
        tracker.start_compute();
        // [Simulated] Initial KV cache setup, model weights pre-fetch
        // In reality, this would be actual compute ops on first forward pass
        tracker.end_compute_start_memory();
        
        // [Simulated] Memory staging for batch (DDR5→VRAM transfer, KV placement)
        tracker.end_memory_start_emission();
        
        // Generate tokens with timing instrumentation
        for (int i = 0; i < maxTokens && i < static_cast<int>(tokens.size()); ++i) {
            // Clock 1: Compute phase (transformer forward pass for position i)
            tracker.start_compute();
            {
                // Simulated: attention, FFN, layer norm operations
                // Real implementation: dispatch to Vulkan compute shaders
                result.push_back(tokens[i]);
            }
            tracker.end_compute_start_memory();
            
            // Clock 2: Memory phase (cache writes, prefetch next iteration)
            {
                // Simulated: KV cache write-back, prefetch stage of next layer weights
                // Real: async memory ops during compute gaps
            }
            tracker.end_memory_start_emission();
            
            // Clock 3: Token emission (sampling, output formatting)
            {
                // Simulated: logits decode, top-k sampling, token emission
            }
            tracker.end_emission();
        }
        
        out_result = orchestrator.finalize_batch();
        return result;
    }
    
    // Streaming variant with timing
    void GenerateStreamingWithTiming(
        const std::vector<int32_t>& tokens,
        int maxTokens,
        std::function<void(const std::string&)> onToken,
        std::function<void(const Telemetry::BatchTimingOrchestrator::BatchResult&)> onTimingReport,
        std::function<void()> onComplete
    ) {
        Telemetry::BatchTimingOrchestrator orchestrator;
        orchestrator.start_batch(maxTokens);
        auto& tracker = orchestrator.get_tracker();
        
        for (int i = 0; i < maxTokens && i < static_cast<int>(tokens.size()); ++i) {
            tracker.start_compute();
            {
                // Compute: transformer ops for token i
            }
            tracker.end_compute_start_memory();
            {
                // Memory: cache management, prefetch
            }
            tracker.end_memory_start_emission();
            {
                // Emission: sample and emit token
                std::string token = "token_" + std::to_string(tokens[i]) + " ";
                if (onToken) {
                    onToken(token);
                }
            }
            tracker.end_emission();
        }
        
        auto result = orchestrator.finalize_batch();
        if (onTimingReport) {
            onTimingReport(result);
        }
        if (onComplete) {
            onComplete();
        }
    }
    
    void SetContextLimit(size_t limit) {
        engine_->SetContextLimit(limit);
    }
    
    size_t GetContextLimit() const {
        return engine_->GetContextLimit();
    }
    
    void SetThreadCount(int count) {
        engine_->SetThreadCount(count);
    }
    
    int GetThreadCount() const {
        return engine_->GetThreadCount();
    }
    
    const char* GetBackendName() const {
        return engine_->GetBackendName();
    }
    
private:
    std::unique_ptr<VulkanInferenceEngine> engine_;
};

}  // namespace RawrXD
