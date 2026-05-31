// ============================================================================
// execution_scheduler_integration.cpp — Integration Implementation
// ============================================================================
// Wires all P0/P1/TRES components into unified ExecutionScheduler
// ============================================================================

#include "execution_scheduler_integration.hpp"
#include <iostream>
#include <chrono>

namespace RawrXD {

// Global singleton
static ExecutionSchedulerIntegration* g_integration = nullptr;

ExecutionSchedulerIntegration* getExecutionSchedulerIntegration() {
    if (!g_integration) {
        g_integration = new ExecutionSchedulerIntegration();
    }
    return g_integration;
}

// ============================================================================
// Construction / Destruction
// ============================================================================
ExecutionSchedulerIntegration::ExecutionSchedulerIntegration() = default;

ExecutionSchedulerIntegration::~ExecutionSchedulerIntegration() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================
bool ExecutionSchedulerIntegration::initialize(const IntegratedSchedulerConfig& config,
                                                  CPUInferenceEngine* engine,
                                                  StreamingEngineRegistry* registry) {
    if (initialized_) {
        shutdown();
    }

    config_ = config;

    // Initialize base scheduler
    base_scheduler_.configure(config.base);
    if (!base_scheduler_.bind(engine, registry)) {
        std::cerr << "[Integration] Failed to bind base scheduler" << std::endl;
        return false;
    }

    // P0: Initialize KV FP8 Quantization
    if (config.enableKVQuantization) {
        kv_quantizer_ = std::make_unique<KV::KVCacheFP8Quantizer>();
        if (!kv_quantizer_->initialize(config.base.prefetchAhead,  // num_layers
                                        config.num_heads,
                                        config.head_dim,
                                        config.max_seq_len,
                                        config.kvFormat)) {
            std::cerr << "[Integration] Failed to initialize KV quantizer" << std::endl;
            return false;
        }
        std::cout << "[Integration] KV FP8 Quantization enabled (format="
                  << (config.kvFormat == KV::FP8Format::E4M3 ? "E4M3" : "E5M2")
                  << ")" << std::endl;
    }

    // P0: Initialize Double-Buffer Token Pipeline
    if (config.enableDoubleBuffer) {
        token_pipeline_ = std::make_unique<Inference::TokenPipelineDoubleBuffer>();
        if (!token_pipeline_->initialize(config.vocabSize, config.embeddingDim)) {
            std::cerr << "[Integration] Failed to initialize token pipeline" << std::endl;
            return false;
        }
        std::cout << "[Integration] Double-buffer token pipeline enabled" << std::endl;
    }

    // P1: Initialize Fused Speculative Verify
    if (config.enableSpeculative) {
        spec_verifier_ = std::make_unique<Speculative::FusedSpeculativeVerifier>();
        if (!spec_verifier_->initialize(config.vocabSize,
                                         config.num_heads,
                                         config.head_dim,
                                         config.max_seq_len)) {
            std::cerr << "[Integration] Failed to initialize spec verifier" << std::endl;
            return false;
        }
        std::cout << "[Integration] Fused speculative verify enabled" << std::endl;
    }

    // TRES: Initialize Stabilization System
    if (config.enableTRES) {
        tres_system_ = std::make_unique<TRES::TRESSystem>();

        // Telemetry callback
        auto telemetry_cb = [this]() -> TRES::SystemTelemetry {
            TRES::SystemTelemetry telemetry = {};

            // Gather metrics from components
            if (kv_quantizer_) {
                telemetry.kv_bytes_used = kv_quantizer_->getTotalBytesUsed();
                telemetry.kv_bytes_total = kv_quantizer_->getTotalBytesSaved();
            }

            // Token generation rate
            if (token_pipeline_) {
                telemetry.tokens_generated = token_pipeline_->getTokensGenerated();
            }

            // Timestamp
            telemetry.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            return telemetry;
        };

        // Autopatch callback
        auto autopatch_cb = [](const TRES::AutopatchSignal& signal) {
            std::cout << "[TRES] Autopatch triggered: " << signal.reason
                      << " (severity=" << signal.severity << ")" << std::endl;
        };

        if (!tres_system_->initialize(telemetry_cb, autopatch_cb)) {
            std::cerr << "[Integration] Failed to initialize TRES" << std::endl;
            return false;
        }

        std::cout << "[Integration] TRES stabilization enabled" << std::endl;
    }

    initialized_ = true;
    std::cout << "[Integration] All subsystems initialized successfully" << std::endl;
    return true;
}

void ExecutionSchedulerIntegration::shutdown() {
    if (!initialized_) return;

    // Stop TRES first
    if (tres_system_) {
        tres_system_->stop();
    }

    // Shutdown components
    spec_verifier_.reset();
    token_pipeline_.reset();
    kv_quantizer_.reset();
    tres_system_.reset();

    // Shutdown base scheduler
    base_scheduler_.shutdown();

    initialized_ = false;
    std::cout << "[Integration] Shutdown complete" << std::endl;
}

// ============================================================================
// Forward Pass with All Optimizations
// ============================================================================
bool ExecutionSchedulerIntegration::runForwardPassIntegrated(float* state,
                                                              float* scratch,
                                                              int seqPos) {
    if (!initialized_) {
        std::cerr << "[Integration] Not initialized" << std::endl;
        return false;
    }

    // Start token generation in double-buffer pipeline
    int32_t buffer_idx = -1;
    if (token_pipeline_) {
        buffer_idx = token_pipeline_->beginTokenGeneration(forward_pass_count_);
    }

    // Run base scheduler forward pass
    bool result = base_scheduler_.runForwardPass(state, scratch, seqPos);

    // Mark token complete in pipeline
    if (token_pipeline_ && buffer_idx >= 0) {
        token_pipeline_->markTokenComplete(buffer_idx);
    }

    forward_pass_count_++;
    return result;
}

// ============================================================================
// KV Cache Operations
// ============================================================================
bool ExecutionSchedulerIntegration::quantizeKVCache(const float* k_data,
                                                    const float* v_data,
                                                    uint32_t layer_idx,
                                                    uint32_t num_tokens) {
    if (!kv_quantizer_) return false;

    bool k_ok = kv_quantizer_->quantizeKCache(k_data, layer_idx, num_tokens) > 0;
    bool v_ok = kv_quantizer_->quantizeVCache(v_data, layer_idx, num_tokens) > 0;

    return k_ok && v_ok;
}

bool ExecutionSchedulerIntegration::dequantizeKVCache(uint32_t layer_idx,
                                                        uint32_t token_start,
                                                        uint32_t num_tokens,
                                                        float* k_out,
                                                        float* v_out) {
    if (!kv_quantizer_) return false;

    kv_quantizer_->dequantizeKCache(layer_idx, token_start, num_tokens, k_out);
    kv_quantizer_->dequantizeVCache(layer_idx, token_start, num_tokens, v_out);

    return true;
}

// ============================================================================
// Token Pipeline Operations
// ============================================================================
int32_t ExecutionSchedulerIntegration::beginTokenGeneration(uint64_t generation_id) {
    if (!token_pipeline_) return -1;
    return static_cast<int32_t>(token_pipeline_->beginTokenGeneration(generation_id));
}

void ExecutionSchedulerIntegration::submitTokenToGPU(int32_t buffer_idx) {
    if (!token_pipeline_ || buffer_idx < 0) return;
    token_pipeline_->submitToGPU(static_cast<uint32_t>(buffer_idx));
}

void ExecutionSchedulerIntegration::markTokenComplete(int32_t buffer_idx) {
    if (!token_pipeline_ || buffer_idx < 0) return;
    token_pipeline_->markGPUComplete(static_cast<uint32_t>(buffer_idx));
}

// ============================================================================
// Speculative Decoding
// ============================================================================
int ExecutionSchedulerIntegration::verifyDraftTokens(const float* draft_logits,
                                                       const float* target_logits,
                                                       const int32_t* draft_tokens,
                                                       uint32_t draft_len,
                                                       int32_t* accepted_tokens) {
    if (!spec_verifier_) return 0;

    Speculative::FusedVerifyConfig config;
    auto result = spec_verifier_->verifyAndAccept(draft_logits, target_logits,
                                                   draft_tokens, draft_len, config);

    for (uint32_t i = 0; i < result.num_accepted; ++i) {
        accepted_tokens[i] = result.accepted_tokens[i];
    }

    return static_cast<int>(result.num_accepted);
}

// ============================================================================
// TRES Control
// ============================================================================
void ExecutionSchedulerIntegration::startTRES() {
    if (tres_system_) {
        tres_system_->start(config_.tresIntervalMs);
    }
}

void ExecutionSchedulerIntegration::stopTRES() {
    if (tres_system_) {
        tres_system_->stop();
    }
}

bool ExecutionSchedulerIntegration::isSystemStable() const {
    return tres_system_ && tres_system_->isStable();
}

TRES::SystemTelemetry ExecutionSchedulerIntegration::getTelemetry() const {
    if (tres_system_) {
        return tres_system_->getCurrentTelemetry();
    }
    return {};
}

// ============================================================================
// Statistics
// ============================================================================
void ExecutionSchedulerIntegration::printStatistics() const {
    std::cout << "\n=== Execution Scheduler Integration Statistics ===" << std::endl;
    std::cout << "Forward passes: " << forward_pass_count_ << std::endl;

    if (kv_quantizer_) {
        std::cout << "KV Cache compression ratio: " << kv_quantizer_->getCompressionRatio() << std::endl;
        std::cout << "KV Bytes saved: " << kv_quantizer_->getTotalBytesSaved() << std::endl;
    }

    if (token_pipeline_) {
        std::cout << "Tokens generated: " << token_pipeline_->getTokensGenerated() << std::endl;
        std::cout << "Tokens in flight: " << token_pipeline_->getTokensInFlight() << std::endl;
        std::cout << "Avg latency: " << token_pipeline_->getAverageLatencyMs() << " ms" << std::endl;
    }

    if (spec_verifier_) {
        std::cout << "Speculative acceptance rate: " << spec_verifier_->getAcceptanceRate() << std::endl;
        std::cout << "Avg verify time: " << spec_verifier_->getAverageVerifyTimeUs() << " us" << std::endl;
    }

    if (tres_system_) {
        std::cout << "System stable: " << (isSystemStable() ? "YES" : "NO") << std::endl;
    }

    std::cout << "===================================================\n" << std::endl;
}

// ============================================================================
// C API Implementation
// ============================================================================
extern "C" {

RawrXD_IntegratedScheduler rawrxd_scheduler_create(
    uint32_t num_layers, uint32_t num_heads, uint32_t head_dim,
    uint32_t max_seq_len, uint32_t vocab_size) {

    auto* integration = new ExecutionSchedulerIntegration();

    IntegratedSchedulerConfig config;
    config.base.prefetchAhead = static_cast<int>(num_layers);
    config.num_heads = num_heads;
    config.head_dim = head_dim;
    config.max_seq_len = max_seq_len;
    config.vocabSize = vocab_size;

    // Note: engine and registry would need to be passed in real usage
    if (!integration->initialize(config, nullptr, nullptr)) {
        delete integration;
        return nullptr;
    }

    return integration;
}

void rawrxd_scheduler_destroy(RawrXD_IntegratedScheduler handle) {
    if (handle) {
        auto* integration = static_cast<ExecutionSchedulerIntegration*>(handle);
        integration->shutdown();
        delete integration;
    }
}

int rawrxd_scheduler_run_forward(RawrXD_IntegratedScheduler handle,
                                float* state, float* scratch, int seq_pos) {
    if (!handle) return -1;
    auto* integration = static_cast<ExecutionSchedulerIntegration*>(handle);
    return integration->runForwardPassIntegrated(state, scratch, seq_pos) ? 0 : -1;
}

int rawrxd_scheduler_is_stable(RawrXD_IntegratedScheduler handle) {
    if (!handle) return 0;
    auto* integration = static_cast<ExecutionSchedulerIntegration*>(handle);
    return integration->isSystemStable() ? 1 : 0;
}

} // extern "C"

} // namespace RawrXD
