// ============================================================================
// execution_scheduler_integration.hpp — Integration: All P0/P1/TRES Components
// ============================================================================
// Wires together:
//   - KV-Cache FP8 Quantization (P0)
//   - Double-Buffer Token Pipeline (P0)
//   - Fused Speculative Verify (P1)
//   - TRES Stabilization Layer (TRES)
//
// Provides unified interface to ExecutionScheduler
// ============================================================================

#pragma once

#include "execution_scheduler.h"
#include "tres_stabilization_layer.hpp"
#include "../kv_cache/kv_cache_fp8_quantizer.hpp"
#include "../inference/token_pipeline_double_buffer.hpp"
#include "../speculative/speculative_fused_verify.hpp"

namespace RawrXD {

// ============================================================================
// Integrated Scheduler Configuration
// ============================================================================
struct IntegratedSchedulerConfig {
    // Base scheduler config
    SchedulerConfig base;

    // KV FP8 quantization
    bool enableKVQuantization = true;
    KV::FP8Format kvFormat = KV::FP8Format::E4M3;

    // Double-buffer pipeline
    bool enableDoubleBuffer = true;
    uint32_t vocabSize = 32000;
    uint32_t embeddingDim = 4096;

    // Speculative decoding
    bool enableSpeculative = true;
    uint32_t maxDraftTokens = 8;

    // TRES stabilization
    bool enableTRES = true;
    uint32_t tresIntervalMs = 50;
};

// ============================================================================
// Execution Scheduler Integration
// ============================================================================
class ExecutionSchedulerIntegration {
public:
    ExecutionSchedulerIntegration();
    ~ExecutionSchedulerIntegration();

    // No copy/move
    ExecutionSchedulerIntegration(const ExecutionSchedulerIntegration&) = delete;
    ExecutionSchedulerIntegration& operator=(const ExecutionSchedulerIntegration&) = delete;

    // Initialize all subsystems
    bool initialize(const IntegratedSchedulerConfig& config,
                    CPUInferenceEngine* engine,
                    StreamingEngineRegistry* registry = nullptr);

    // Shutdown
    void shutdown();

    // Run forward pass with all optimizations
    bool runForwardPassIntegrated(float* state, float* scratch, int seqPos);

    // KV cache operations
    bool quantizeKVCache(const float* k_data, const float* v_data,
                         uint32_t layer_idx, uint32_t num_tokens);
    bool dequantizeKVCache(uint32_t layer_idx, uint32_t token_start,
                           uint32_t num_tokens, float* k_out, float* v_out);

    // Token pipeline operations
    int32_t beginTokenGeneration(uint64_t generation_id);
    void submitTokenToGPU(int32_t buffer_idx);
    void markTokenComplete(int32_t buffer_idx);

    // Speculative decoding
    int verifyDraftTokens(const float* draft_logits,
                          const float* target_logits,
                          const int32_t* draft_tokens,
                          uint32_t draft_len,
                          int32_t* accepted_tokens);

    // TRES control
    void startTRES();
    void stopTRES();
    bool isSystemStable() const;
    TRES::SystemTelemetry getTelemetry() const;

    // Statistics
    void printStatistics() const;

    // Access components
    ExecutionScheduler* getBaseScheduler() { return &base_scheduler_; }
    KV::KVCacheFP8Quantizer* getKVQuantizer() { return kv_quantizer_.get(); }
    Inference::TokenPipelineDoubleBuffer* getTokenPipeline() { return token_pipeline_.get(); }
    Speculative::FusedSpeculativeVerifier* getSpecVerifier() { return spec_verifier_.get(); }
    TRES::TRESSystem* getTRESSystem() { return tres_system_.get(); }

private:
    // Base scheduler
    ExecutionScheduler base_scheduler_;

    // P0: KV FP8 Quantization
    std::unique_ptr<KV::KVCacheFP8Quantizer> kv_quantizer_;

    // P0: Double-Buffer Token Pipeline
    std::unique_ptr<Inference::TokenPipelineDoubleBuffer> token_pipeline_;

    // P1: Fused Speculative Verify
    std::unique_ptr<Speculative::FusedSpeculativeVerifier> spec_verifier_;

    // TRES: Stabilization System
    std::unique_ptr<TRES::TRESSystem> tres_system_;

    // Configuration
    IntegratedSchedulerConfig config_;

    // State
    bool initialized_ = false;
    uint64_t forward_pass_count_ = 0;
};

// ============================================================================
// Global singleton accessor
// ============================================================================
ExecutionSchedulerIntegration* getExecutionSchedulerIntegration();

// ============================================================================
// C API for integration
// ============================================================================
extern "C" {
    typedef void* RawrXD_IntegratedScheduler;

    RawrXD_IntegratedScheduler rawrxd_scheduler_create(
        uint32_t num_layers, uint32_t num_heads, uint32_t head_dim,
        uint32_t max_seq_len, uint32_t vocab_size);

    void rawrxd_scheduler_destroy(RawrXD_IntegratedScheduler handle);

    int rawrxd_scheduler_run_forward(RawrXD_IntegratedScheduler handle,
                                      float* state, float* scratch, int seq_pos);

    int rawrxd_scheduler_is_stable(RawrXD_IntegratedScheduler handle);
}

} // namespace RawrXD
