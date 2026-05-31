// src/collaboration/LogitArbiter.h
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <string>
#include <vector>

// Forward declarations for sovereign runtime
struct sov_ctx_t;
struct sov_model_t;

namespace RawrXD {

class LogitArbiter {
public:
    static constexpr int VOCAB_SIZE = 32000; // LLaMA/Mistral standard
    
    struct ArbitrationScore {
        float agreement;      // 1.0 = identical distributions, 0.0 = orthogonal
        float confidence;     // max probability of the agreed-upon token
        int   divergence_pos; // offset where paths diverge
    };

    struct ContextPair {
        sov_ctx_t* local;
        sov_ctx_t* remote;
        std::atomic<bool> ready{false};
    };

    void init(sov_model_t* model, uint32_t max_ctx);
    
    // Pre-compute divergence map for a conflict region
    ArbitrationScore analyzeDivergence(const std::string& base,
                                       const std::string& local,
                                       const std::string& remote);
    
    // Fast path: check if next token prediction agrees
    bool predictionsAgree(ContextPair& pair, int32_t& consensus_token);

private:
    sov_model_t* model_;
    uint32_t max_ctx_;
    
    // Aligned storage for logit buffers (cache-line aligned to prevent false sharing)
    alignas(64) float buf_local_[VOCAB_SIZE];
    alignas(64) float buf_remote_[VOCAB_SIZE];
    
    // Statistical divergence using Jensen-Shannon (symmetric, smooth)
    float js_divergence(const float* p_local, const float* p_remote, int vocab);
};

} // namespace RawrXD
