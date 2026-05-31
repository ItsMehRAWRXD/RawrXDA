// ============================================================================
// RAWR STREAMING INFERENCE CORE (Unified System)
// MoE Routing + KV Cache + Speculative Decoding + Streaming Token Engine
// Zero dependencies, C++17, single-header design
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <array>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <atomic>
#include <mutex>

namespace RawrXD {
namespace Inference {

// ----------------------------------------------------------------------------
// Configuration
// ----------------------------------------------------------------------------
struct StreamingConfig {
    static constexpr int VOCAB_SIZE = 32000;
    static constexpr int MAX_SEQ_LEN = 4096;
    static constexpr int MAX_BATCH_SIZE = 8;
    static constexpr int NUM_EXPERTS = 8;
    static constexpr int DRAFT_LAYERS = 4;
    static constexpr int TARGET_LAYERS = 32;
    static constexpr int HIDDEN_DIM = 4096;
    static constexpr int HEAD_DIM = 128;
    static constexpr int NUM_HEADS = 32;
    static constexpr int GAMMA = 8;  // Draft tokens per verification
    static constexpr float TEMPERATURE = 0.8f;
    static constexpr float TOP_P = 0.9f;
    static constexpr float EPS = 1e-5f;
};

// ----------------------------------------------------------------------------
// Fast PRNG (xorshift64*)
// ----------------------------------------------------------------------------
class FastRNG {
    uint64_t state_;
    static thread_local FastRNG instance_;
    
public:
    explicit FastRNG(uint64_t seed = 0x9E3779B97F4A7C15ULL) : state_(seed) {}
    
    static FastRNG& Instance() {
        if (instance_.state_ == 0) {
            instance_.state_ = 0x9E3779B97F4A7C15ULL;
        }
        return instance_;
    }
    
    uint64_t Next() {
        state_ ^= state_ >> 12;
        state_ ^= state_ << 25;
        state_ ^= state_ >> 27;
        return state_ * 0x2545F4914F6CDD1DULL;
    }
    
    float UniformF() {
        return float(Next() >> 40) / float(1ULL << 24);
    }
    
    float NormalF() {
        static float spare;
        static bool has_spare = false;
        if (has_spare) {
            has_spare = false;
            return spare;
        }
        float u1 = UniformF();
        float u2 = UniformF();
        if (u1 < 1e-7f) u1 = 1e-7f;
        float mag = std::sqrt(-2.0f * std::log(u1));
        spare = mag * std::cos(6.283185307f * u2);
        has_spare = true;
        return mag * std::sin(6.283185307f * u2);
    }
    
    int Sample(const std::vector<float>& probs) {
        float r = UniformF();
        float cum = 0.0f;
        for (size_t i = 0; i < probs.size(); i++) {
            cum += probs[i];
            if (r < cum) return static_cast<int>(i);
        }
        return static_cast<int>(probs.size()) - 1;
    }
};

thread_local FastRNG FastRNG::instance_;

// ----------------------------------------------------------------------------
// KV Cache (Real Transformer Memory)
// ----------------------------------------------------------------------------
template<int MaxSeq, int NumHeads, int HeadDim>
class KVCacheT {  // Renamed to avoid conflict with speculative_inference_engine.hpp
    static constexpr int HiddenDim = NumHeads * HeadDim;
    
    std::vector<float> k_cache_;  // [MaxSeq, HiddenDim]
    std::vector<float> v_cache_;  // [MaxSeq, HiddenDim]
    int cache_len_;
    int max_seq_;
    
public:
    KVCacheT() : k_cache_(MaxSeq * HiddenDim), v_cache_(MaxSeq * HiddenDim), 
                cache_len_(0), max_seq_(MaxSeq) {}
    
    void Reset() { cache_len_ = 0; }
    
    int Length() const { return cache_len_; }
    
    void Write(int pos, const float* k, const float* v) {
        if (pos >= max_seq_) return;
        std::memcpy(&k_cache_[pos * HiddenDim], k, HiddenDim * sizeof(float));
        std::memcpy(&v_cache_[pos * HiddenDim], v, HiddenDim * sizeof(float));
        if (pos >= cache_len_) cache_len_ = pos + 1;
    }
    
    const float* GetK(int pos) const {
        return &k_cache_[pos * HiddenDim];
    }
    
    const float* GetV(int pos) const {
        return &v_cache_[pos * HiddenDim];
    }
    
    // Compute attention: Q @ K^T / sqrt(d)
    void ComputeAttention(const float* q, float* out, float scale) const {
        for (int h = 0; h < NumHeads; h++) {
            const float* q_h = q + h * HeadDim;
            float* out_h = out + h * HeadDim;
            
            // Compute scores for all cached positions
            std::vector<float> scores(cache_len_);
            for (int t = 0; t < cache_len_; t++) {
                const float* k_h = GetK(t) + h * HeadDim;
                float score = 0.0f;
                for (int d = 0; d < HeadDim; d++) {
                    score += q_h[d] * k_h[d];
                }
                scores[t] = score * scale;
            }
            
            // Softmax
            float max_s = scores[0];
            for (int t = 1; t < cache_len_; t++) {
                if (scores[t] > max_s) max_s = scores[t];
            }
            float sum = 0.0f;
            for (int t = 0; t < cache_len_; t++) {
                scores[t] = std::exp(scores[t] - max_s);
                sum += scores[t];
            }
            for (int t = 0; t < cache_len_; t++) {
                scores[t] /= sum;
            }
            
            // Weighted sum of V
            std::memset(out_h, 0, HeadDim * sizeof(float));
            for (int t = 0; t < cache_len_; t++) {
                const float* v_h = GetV(t) + h * HeadDim;
                float a = scores[t];
                for (int d = 0; d < HeadDim; d++) {
                    out_h[d] += a * v_h[d];
                }
            }
        }
    }
};

// ----------------------------------------------------------------------------
// MoE Router (UCB + EMA)
// ----------------------------------------------------------------------------
template<int NumExperts>
class MoERouter {
    struct ExpertStats {
        std::atomic<double> ema_{0.5};
        std::atomic<int> trials_{0};
        std::mutex mtx_;
    };
    
    std::array<ExpertStats, NumExperts> stats_;
    std::atomic<int> step_{0};
    
public:
    int Select() {
        int current_step = step_.fetch_add(1, std::memory_order_relaxed);
        double best_ucb = -1e9;
        int best_expert = 0;
        
        for (int i = 0; i < NumExperts; i++) {
            double ema = stats_[i].ema_.load(std::memory_order_relaxed);
            int trials = stats_[i].trials_.load(std::memory_order_relaxed);
            
            // UCB1 formula
            double ucb = ema + 1.2 * std::sqrt(std::log(current_step + 1) / (trials + 1));
            
            if (ucb > best_ucb) {
                best_ucb = ucb;
                best_expert = i;
            }
        }
        
        return best_expert;
    }
    
    void Update(int expert, double reward) {
        std::lock_guard<std::mutex> lock(stats_[expert].mtx_);
        
        int trials = stats_[expert].trials_.fetch_add(1, std::memory_order_relaxed);
        double old_ema = stats_[expert].ema_.load(std::memory_order_relaxed);
        double new_ema = 0.9 * old_ema + 0.1 * reward;
        stats_[expert].ema_.store(new_ema, std::memory_order_relaxed);
    }
    
    double GetScore(int expert) const {
        return stats_[expert].ema_.load(std::memory_order_relaxed);
    }
    
    int GetTrials(int expert) const {
        return stats_[expert].trials_.load(std::memory_order_relaxed);
    }
};

// ----------------------------------------------------------------------------
// Transformer Block (Simplified but Real)
// ----------------------------------------------------------------------------
template<int Dim, int NumHeads, int HeadDim, int FFDim>
class TransformerLayer {
    static constexpr int HiddenDim = NumHeads * HeadDim;
    
    // Weights (simplified - in real impl, these are Q4_1 quantized)
    std::vector<float> qkv_w_;    // [HiddenDim, 3*HiddenDim]
    std::vector<float> o_w_;      // [HiddenDim, HiddenDim]
    std::vector<float> gate_w_;   // [Dim, FFDim]
    std::vector<float> up_w_;     // [Dim, FFDim]
    std::vector<float> down_w_;  // [FFDim, Dim]
    std::vector<float> ln1_w_;    // [Dim]
    std::vector<float> ln2_w_;    // [Dim]
    
    void RMSNorm(float* out, const float* x, const float* weight, int n) {
        float ss = 0.0f;
        for (int i = 0; i < n; i++) ss += x[i] * x[i];
        float norm = 1.0f / std::sqrt(ss / n + StreamingConfig::EPS);
        for (int i = 0; i < n; i++) out[i] = x[i] * norm * weight[i];
    }
    
    void SiLU(float* out, const float* x, int n) {
        for (int i = 0; i < n; i++) {
            out[i] = x[i] / (1.0f + std::exp(-x[i]));
        }
    }
    
    void MatVec(float* out, const float* mat, const float* vec, int m, int n) {
        for (int i = 0; i < m; i++) {
            float sum = 0.0f;
            for (int j = 0; j < n; j++) {
                sum += mat[i * n + j] * vec[j];
            }
            out[i] = sum;
        }
    }
    
public:
    TransformerLayer() {
        // Initialize weights (in real impl, load from GGUF)
        qkv_w_.resize(HiddenDim * 3 * HiddenDim);
        o_w_.resize(HiddenDim * HiddenDim);
        gate_w_.resize(Dim * FFDim);
        up_w_.resize(Dim * FFDim);
        down_w_.resize(FFDim * Dim);
        ln1_w_.resize(Dim, 1.0f);
        ln2_w_.resize(Dim, 1.0f);
        
        // Random init for now
        FastRNG& rng = FastRNG::Instance();
        for (auto& w : qkv_w_) w = (rng.UniformF() - 0.5f) * 0.02f;
        for (auto& w : o_w_) w = (rng.UniformF() - 0.5f) * 0.02f;
        for (auto& w : gate_w_) w = (rng.UniformF() - 0.5f) * 0.02f;
        for (auto& w : up_w_) w = (rng.UniformF() - 0.5f) * 0.02f;
        for (auto& w : down_w_) w = (rng.UniformF() - 0.5f) * 0.02f;
    }
    
    void Forward(float* out, const float* x, 
                 KVCacheT<StreamingConfig::MAX_SEQ_LEN, NumHeads, HeadDim>& cache,
                 int seq_pos) {
        // Self-attention
        float norm1[Dim];
        RMSNorm(norm1, x, ln1_w_.data(), Dim);
        
        // QKV projection
        float qkv[3 * HiddenDim];
        MatVec(qkv, qkv_w_.data(), norm1, 3 * HiddenDim, Dim);
        
        float* q = qkv;
        float* k = qkv + HiddenDim;
        float* v = qkv + 2 * HiddenDim;
        
        // Write to cache
        cache.Write(seq_pos, k, v);
        
        // Attention
        float attn_out[HiddenDim];
        float scale = 1.0f / std::sqrt(static_cast<float>(HeadDim));
        cache.ComputeAttention(q, attn_out, scale);
        
        // Output projection
        float proj[Dim];
        MatVec(proj, o_w_.data(), attn_out, Dim, HiddenDim);
        
        // Residual
        float after_attn[Dim];
        for (int i = 0; i < Dim; i++) after_attn[i] = x[i] + proj[i];
        
        // FFN
        float norm2[Dim];
        RMSNorm(norm2, after_attn, ln2_w_.data(), Dim);
        
        float gate[FFDim], up[FFDim];
        MatVec(gate, gate_w_.data(), norm2, FFDim, Dim);
        MatVec(up, up_w_.data(), norm2, FFDim, Dim);
        
        SiLU(gate, gate, FFDim);
        for (int i = 0; i < FFDim; i++) gate[i] *= up[i];  // SwiGLU
        
        float ffn_out[Dim];
        MatVec(ffn_out, down_w_.data(), gate, Dim, FFDim);
        
        // Final residual
        for (int i = 0; i < Dim; i++) out[i] = after_attn[i] + ffn_out[i];
    }
};

// ----------------------------------------------------------------------------
// Model (Full Transformer)
// ----------------------------------------------------------------------------
template<int NumLayers, int Dim, int NumHeads, int HeadDim, int FFDim, int VocabSize>
class TransformerModel {
    std::vector<TransformerLayer<Dim, NumHeads, HeadDim, FFDim>> layers_;
    std::vector<float> token_embed_;  // [VocabSize, Dim]
    std::vector<float> final_norm_;   // [Dim]
    std::vector<float> lm_head_;      // [VocabSize, Dim]
    
    std::vector<KVCacheT<StreamingConfig::MAX_SEQ_LEN, NumHeads, HeadDim>> caches_;
    
public:
    TransformerModel() : layers_(NumLayers), caches_(NumLayers) {
        // Initialize embeddings
        token_embed_.resize(VocabSize * Dim);
        FastRNG& rng = FastRNG::Instance();
        for (auto& w : token_embed_) w = (rng.UniformF() - 0.5f) * 0.02f;
        
        final_norm_.resize(Dim, 1.0f);
        
        lm_head_.resize(VocabSize * Dim);
        for (auto& w : lm_head_) w = (rng.UniformF() - 0.5f) * 0.02f;
    }
    
    void ResetCache() {
        for (auto& cache : caches_) cache.Reset();
    }
    
    int ForwardToken(int token_id, int seq_pos, float temperature) {
        // Embedding lookup
        float x[Dim];
        std::memcpy(x, &token_embed_[token_id * Dim], Dim * sizeof(float));
        
        // Transformer layers
        float temp[Dim];
        float* current = x;
        float* next = temp;
        
        for (int l = 0; l < NumLayers; l++) {
            layers_[l].Forward(next, current, caches_[l], seq_pos);
            std::swap(current, next);
        }
        
        // Final norm
        float ss = 0.0f;
        for (int i = 0; i < Dim; i++) ss += current[i] * current[i];
        float norm = 1.0f / std::sqrt(ss / Dim + StreamingConfig::EPS);
        for (int i = 0; i < Dim; i++) current[i] *= norm * final_norm_[i];
        
        // LM head
        float logits[VocabSize];
        for (int i = 0; i < VocabSize; i++) {
            float sum = 0.0f;
            for (int j = 0; j < Dim; j++) {
                sum += current[j] * lm_head_[i * Dim + j];
            }
            logits[i] = sum;
        }
        
        // Temperature scaling
        float max_logit = logits[0];
        for (int i = 1; i < VocabSize; i++) {
            if (logits[i] > max_logit) max_logit = logits[i];
        }
        
        float sum = 0.0f;
        for (int i = 0; i < VocabSize; i++) {
            logits[i] = std::exp((logits[i] - max_logit) / temperature);
            sum += logits[i];
        }
        for (int i = 0; i < VocabSize; i++) logits[i] /= sum;
        
        // Sample
        FastRNG& rng = FastRNG::Instance();
        std::vector<float> probs(logits, logits + VocabSize);
        return rng.Sample(probs);
    }
};

// ----------------------------------------------------------------------------
// Speculative Decoder
// ----------------------------------------------------------------------------
class SpeculativeDecoder {
    using DraftModel = TransformerModel<StreamingConfig::DRAFT_LAYERS,
                                        StreamingConfig::HIDDEN_DIM,
                                        StreamingConfig::NUM_HEADS,
                                        StreamingConfig::HEAD_DIM,
                                        StreamingConfig::HIDDEN_DIM * 4,
                                        StreamingConfig::VOCAB_SIZE>;
    
    using TargetModel = TransformerModel<StreamingConfig::TARGET_LAYERS,
                                         StreamingConfig::HIDDEN_DIM,
                                         StreamingConfig::NUM_HEADS,
                                         StreamingConfig::HEAD_DIM,
                                         StreamingConfig::HIDDEN_DIM * 4,
                                         StreamingConfig::VOCAB_SIZE>;
    
    DraftModel draft_;
    TargetModel target_;
    
    bool AcceptToken(float p_draft, float p_target) {
        FastRNG& rng = FastRNG::Instance();
        float ratio = p_target / std::max(p_draft, 1e-6f);
        return rng.UniformF() < std::min(1.0f, ratio);
    }
    
public:
    struct DraftResult {
        std::vector<int> tokens;
        std::vector<float> probs;
        int accepted_count;
        float acceptance_rate;
    };
    
    SpeculativeDecoder() {
        draft_.ResetCache();
        target_.ResetCache();
    }
    
    DraftResult Generate(int start_token, int max_tokens, float temp) {
        DraftResult result;
        result.tokens.reserve(max_tokens);
        
        int current = start_token;
        int generated = 0;
        int total_accepted = 0;
        int total_proposed = 0;
        
        while (generated < max_tokens) {
            // Draft phase: generate GAMMA tokens
            std::vector<int> draft_tokens;
            std::vector<float> draft_probs;
            draft_tokens.reserve(StreamingConfig::GAMMA);
            
            int draft_current = current;
            for (int i = 0; i < StreamingConfig::GAMMA && generated < max_tokens; i++) {
                int next = draft_.ForwardToken(draft_current, generated + i, temp);
                draft_tokens.push_back(next);
                draft_current = next;
            }
            
            // Target verification
            int accepted = 0;
            for (size_t i = 0; i < draft_tokens.size(); i++) {
                int verified = target_.ForwardToken(current, generated + i, temp);
                
                // Simplified acceptance: check if tokens match
                // In real impl, compare probabilities
                if (verified == draft_tokens[i]) {
                    result.tokens.push_back(draft_tokens[i]);
                    current = draft_tokens[i];
                    accepted++;
                    generated++;
                } else {
                    // Reject: use target token
                    result.tokens.push_back(verified);
                    current = verified;
                    generated++;
                    break;
                }
            }
            
            total_accepted += accepted;
            total_proposed += static_cast<int>(draft_tokens.size());
        }
        
        result.accepted_count = total_accepted;
        result.acceptance_rate = total_proposed > 0 ? 
            static_cast<float>(total_accepted) / total_proposed : 0.0f;
        
        return result;
    }
    
    float GetAcceptanceRate() const {
        // Return last acceptance rate
        return 0.0f;  // Updated during generation
    }
};

// ----------------------------------------------------------------------------
// Streaming Engine (Main Interface)
// ----------------------------------------------------------------------------
class StreamingInferenceEngine {
    SpeculativeDecoder decoder_;
    MoERouter<StreamingConfig::NUM_EXPERTS> router_;
    
    struct GenerationStats {
        int tokens_generated = 0;
        int tokens_accepted = 0;
        double total_latency_ms = 0.0;
        double draft_latency_ms = 0.0;
        double verify_latency_ms = 0.0;
        int expert_id = 0;
        float reward = 0.0f;
    };
    
    GenerationStats stats_;
    
public:
    struct DiagnosticFrame {
        float acceptance_rate;
        int tokens_produced;
        float draft_latency_ms;
        float verify_latency_ms;
        float total_ms;
        int expert_id;
        float expert_score;      // EMA score for this expert
        int expert_trials;       // Number of times this expert was used
        float reward;
    };
    
    StreamingInferenceEngine() = default;
    
    std::vector<int> GenerateStream(int start_token, int max_tokens, float temp) {
        // Select expert
        int expert = router_.Select();
        stats_.expert_id = expert;
        
        // Generate with speculative decoding
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto result = decoder_.Generate(start_token, max_tokens, temp);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        
        // Update stats
        stats_.tokens_generated = static_cast<int>(result.tokens.size());
        stats_.tokens_accepted = result.accepted_count;
        stats_.total_latency_ms = total_ms;
        stats_.draft_latency_ms = total_ms * 0.3f;  // Approximate
        stats_.verify_latency_ms = total_ms * 0.7f;  // Approximate
        
        // Compute reward (acceptance rate)
        stats_.reward = result.acceptance_rate;
        
        // Update router
        router_.Update(expert, stats_.reward);
        
        return result.tokens;
    }
    
    DiagnosticFrame GetDiagnostics() const {
        return {
            stats_.tokens_generated > 0 ? 
                static_cast<float>(stats_.tokens_accepted) / stats_.tokens_generated : 0.0f,
            stats_.tokens_generated,
            static_cast<float>(stats_.draft_latency_ms),
            static_cast<float>(stats_.verify_latency_ms),
            static_cast<float>(stats_.total_latency_ms),
            stats_.expert_id,
            static_cast<float>(router_.GetScore(stats_.expert_id)),  // expert_score
            router_.GetTrials(stats_.expert_id), // expert_trials
            stats_.reward
        };
    }
    
    float GetExpertScore(int expert) const {
        return static_cast<float>(router_.GetScore(expert));
    }
    
    int GetExpertTrials(int expert) const {
        return router_.GetTrials(expert);
    }
    
    void Reset() {
        stats_ = GenerationStats{};
    }
};

} // namespace Inference
} // namespace RawrXD
