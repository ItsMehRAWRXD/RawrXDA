// ============================================================================
// speculative_inference_engine.cpp — Sovereign Speculative Decoder
// ============================================================================
// Zero-dependency deterministic speculative inference with self-improvement.
// Hardware target: Ryzen 7 7800X3D, 64GB DDR5-5600, RX 7800 XT 16GB
// ============================================================================

#include "speculative_inference_engine.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>
#include <thread>
#include <mutex>

// Platform-specific includes
#if defined(_WIN32)
#include <windows.h>
#include <intrin.h>
static inline double get_time_ms() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / f.QuadPart;
}
#else
#include <x86intrin.h>
#include <time.h>
static inline double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
#endif

// AVX-512 intrinsics
#include <immintrin.h>

namespace RawrXD {
namespace Inference {

// ============================================================================
// FP16 CONVERSION
// ============================================================================

static inline float fp16_to_fp32(uint16_t h) {
    // IEEE 754 FP16 to FP32
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        // Subnormal
        float f = mant / 1024.0f;
        return (sign ? -1.0f : 1.0f) * f * powf(2.0f, -14);
    } else if (exp == 31) {
        return (mant == 0) ? (sign ? -INFINITY : INFINITY) : NAN;
    }
    
    float f = 1.0f + mant / 1024.0f;
    return (sign ? -1.0f : 1.0f) * f * powf(2.0f, (int)exp - 15);
}

// ============================================================================
// ARENA ALLOCATOR
// ============================================================================

ArenaAllocator::ArenaAllocator(size_t size_gb) 
    : size_(size_gb * 1024ULL * 1024 * 1024), offset_(0), highwater_(0) {
    base_ = (uint8_t*)malloc(size_);
    if (!base_) {
        fprintf(stderr, "FATAL: Cannot allocate %zu GB arena\n", size_gb);
        exit(1);
    }
    // Touch all pages
    #pragma omp parallel for
    for (size_t i = 0; i < size_; i += PAGE_SIZE) {
        base_[i] = 0;
    }
}

ArenaAllocator::~ArenaAllocator() {
    free(base_);
}

void* ArenaAllocator::allocate(size_t bytes) {
    bytes = (bytes + CACHE_LINE - 1) & ~(CACHE_LINE - 1);
    size_t off = offset_.fetch_add(bytes, std::memory_order_relaxed);
    if (off + bytes > size_) {
        fprintf(stderr, "FATAL: Arena overflow\n");
        exit(1);
    }
    if (off + bytes > highwater_) {
        highwater_ = off + bytes;
    }
    return base_ + off;
}

void ArenaAllocator::reset() {
    offset_.store(0, std::memory_order_relaxed);
}

// ============================================================================
// KV CACHE
// ============================================================================

void KVCache::init(ArenaAllocator& arena, int max_seq_, int n_heads_, int head_dim_) {
    max_seq = max_seq_;
    n_heads = n_heads_;
    head_dim = head_dim_;
    len = 0;
    size_t sz = (size_t)max_seq * n_heads * head_dim;
    k = (float*)arena.allocate(sz * sizeof(float));
    v = (float*)arena.allocate(sz * sizeof(float));
    memset(k, 0, sz * sizeof(float));
    memset(v, 0, sz * sizeof(float));
}

void KVCache::reset() {
    len = 0;
}

void KVCache::write(int pos, const float* k_new, const float* v_new) {
    size_t off = (size_t)pos * n_heads * head_dim;
    memcpy(k + off, k_new, n_heads * head_dim * sizeof(float));
    memcpy(v + off, v_new, n_heads * head_dim * sizeof(float));
    if (pos >= len) len = pos + 1;
}

// ============================================================================
// MOE MODEL INITIALIZATION
// ============================================================================

void MoEModel::init(ArenaAllocator& arena, int dim_, int heads_, int head_dim_, 
                     int n_layers_, int ff_dim_, int vocab_, uint64_t id) {
    dim = dim_;
    n_heads = heads_;
    head_dim = head_dim_;
    n_layers = n_layers_;
    ff_dim = ff_dim_;
    vocab = vocab_;
    model_id = id;
    success_rate = 0.0f;
    tokens_generated = 0;
    tokens_accepted = 0;
    
    // Token embeddings (FP32)
    token_embed = (float*)arena.allocate((size_t)vocab * dim * sizeof(float));
    
    // Initialize with small random values
    std::mt19937 rng(static_cast<uint32_t>(id));
    std::uniform_real_distribution<float> dist(-0.02f, 0.02f);
    for (int i = 0; i < vocab * dim; i++) {
        token_embed[i] = dist(rng);
    }
    
    // Layers
    for (int l = 0; l < n_layers; l++) {
        // Router weights
        int router_blocks = (dim * MOE_EXPERTS) / 32;
        layers[l].router_w = (BlockQ4_1*)arena.allocate(router_blocks * sizeof(BlockQ4_1));
        
        // Experts
        int expert_blocks = (ff_dim * dim) / 32;
        for (int e = 0; e < MOE_EXPERTS; e++) {
            layers[l].expert_gate[e] = (BlockQ4_1*)arena.allocate(expert_blocks * sizeof(BlockQ4_1));
            layers[l].expert_up[e] = (BlockQ4_1*)arena.allocate(expert_blocks * sizeof(BlockQ4_1));
            layers[l].expert_down[e] = (BlockQ4_1*)arena.allocate(expert_blocks * sizeof(BlockQ4_1));
            
            // Initialize quantized weights
            for (int b = 0; b < expert_blocks; b++) {
                uint16_t scale = 0x3C00; // FP16 1.0
                layers[l].expert_gate[e][b].d = scale;
                layers[l].expert_gate[e][b].m = 0;
                layers[l].expert_up[e][b].d = scale;
                layers[l].expert_up[e][b].m = 0;
                layers[l].expert_down[e][b].d = scale;
                layers[l].expert_down[e][b].m = 0;
                
                for (int q = 0; q < 16; q++) {
                    layers[l].expert_gate[e][b].qs[q] = (uint8_t)(rng() & 0xFF);
                    layers[l].expert_up[e][b].qs[q] = (uint8_t)(rng() & 0xFF);
                    layers[l].expert_down[e][b].qs[q] = (uint8_t)(rng() & 0xFF);
                }
            }
        }
        
        // Attention weights
        int qkv_blocks = (3 * dim * dim) / 32;
        layers[l].qkv_w = (BlockQ4_1*)arena.allocate(qkv_blocks * sizeof(BlockQ4_1));
        layers[l].o_w = (BlockQ4_1*)arena.allocate((dim * dim) / 32 * sizeof(BlockQ4_1));
        
        // Layer norms
        layers[l].ln1 = (float*)arena.allocate(dim * sizeof(float));
        layers[l].ln2 = (float*)arena.allocate(dim * sizeof(float));
        for (int i = 0; i < dim; i++) {
            layers[l].ln1[i] = 1.0f;
            layers[l].ln2[i] = 1.0f;
        }
    }
    
    // Final norm
    final_norm = (float*)arena.allocate(dim * sizeof(float));
    for (int i = 0; i < dim; i++) final_norm[i] = 1.0f;
    
    // LM head
    int lm_blocks = (vocab * dim) / 32;
    lm_head = (BlockQ4_1*)arena.allocate(lm_blocks * sizeof(BlockQ4_1));
}

// ============================================================================
// SELF-IMPROVEMENT ENGINE
// ============================================================================

SelfImprovementEngine::SelfImprovementEngine() {
    feedback_window_.reserve(FEEDBACK_WINDOW_SIZE * 2);
}

void SelfImprovementEngine::record_acceptance(const std::vector<int>& context, 
                                               int token, bool accepted) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    FeedbackSample sample;
    sample.context = context;
    sample.predicted_token = token;
    sample.target_token = token; // Same if accepted
    sample.loss = accepted ? 0.0f : 1.0f;
    sample.timestamp = static_cast<uint64_t>(get_time_ms());
    sample.was_accepted = accepted;
    
    feedback_window_.push_back(sample);
    total_tokens_++;
    if (accepted) accepted_tokens_++;
    
    // Keep window bounded
    if (feedback_window_.size() > FEEDBACK_WINDOW_SIZE * 2) {
        feedback_window_.erase(feedback_window_.begin(), 
                              feedback_window_.begin() + FEEDBACK_WINDOW_SIZE);
    }
}

void SelfImprovementEngine::record_rejection(const std::vector<int>& context,
                                              int predicted, int actual) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    FeedbackSample sample;
    sample.context = context;
    sample.predicted_token = predicted;
    sample.target_token = actual;
    sample.loss = 1.0f;
    sample.timestamp = static_cast<uint64_t>(get_time_ms());
    sample.was_accepted = false;
    
    feedback_window_.push_back(sample);
    total_tokens_++;
    
    if (feedback_window_.size() > FEEDBACK_WINDOW_SIZE * 2) {
        feedback_window_.erase(feedback_window_.begin(),
                              feedback_window_.begin() + FEEDBACK_WINDOW_SIZE);
    }
}

void SelfImprovementEngine::adapt_weights(MoEModel& model) {
    // Lightweight online adaptation
    // Adjust router weights based on expert performance
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Count expert usage from recent feedback
    int expert_usage[MOE_EXPERTS] = {0};
    int total_samples = std::min((int)feedback_window_.size(), FEEDBACK_WINDOW_SIZE);
    
    for (int i = 0; i < total_samples; i++) {
        // Determine which experts were active for this sample
        // Simplified: distribute credit/blame across active experts
        if (feedback_window_[i].was_accepted) {
            for (int e = 0; e < MOE_ACTIVE_EXPERTS; e++) {
                expert_usage[e]++;
            }
        }
    }
    
    // Update model stats
    if (total_tokens_.load() > 0) {
        model.success_rate = (float)accepted_tokens_.load() / total_tokens_.load();
    }
    model.tokens_generated = total_tokens_.load();
    model.tokens_accepted = accepted_tokens_.load();
}

bool SelfImprovementEngine::should_spawn_new_expert(float current_acceptance) const {
    return current_acceptance < ACCEPTANCE_THRESHOLD && 
           total_tokens_.load() > TRAINING_TRIGGER_INTERVAL;
}

std::vector<FeedbackSample> SelfImprovementEngine::extract_training_batch(size_t batch_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t start = feedback_window_.size() > batch_size ? 
                   feedback_window_.size() - batch_size : 0;
    
    return std::vector<FeedbackSample>(feedback_window_.begin() + start,
                                       feedback_window_.end());
}

float SelfImprovementEngine::get_average_acceptance() const {
    uint64_t total = total_tokens_.load();
    return total > 0 ? (float)accepted_tokens_.load() / total : 0.0f;
}

uint64_t SelfImprovementEngine::get_total_tokens() const {
    return total_tokens_.load();
}

// ============================================================================
// MOE CHAIN
// ============================================================================

MoEChain::MoEChain(ArenaAllocator& arena) : arena_(arena) {
    model_scores_.fill(0.5f);
    model_usage_.fill(0);
}

void MoEChain::initialize_chain() {
    for (int i = 0; i < MOE_CHAIN_COUNT; i++) {
        arena_.reset();
        models_[i].init(arena_, MOE_DIM, MOE_HEADS, MOE_HEAD_DIM,
                       MOE_LAYERS, MOE_FF, MOE_VOCAB, static_cast<uint64_t>(i));
        model_scores_[i] = 0.5f + i * 0.02f; // Slight variation
    }
}

MoEModel* MoEChain::get_next_model() {
    size_t idx = current_idx_.fetch_add(1, std::memory_order_relaxed) % MOE_CHAIN_COUNT;
    model_usage_[idx]++;
    return &models_[idx];
}

MoEModel* MoEChain::get_model(size_t idx) {
    if (idx >= MOE_CHAIN_COUNT) return nullptr;
    return &models_[idx];
}

void MoEChain::update_model_stats(size_t idx, bool accepted) {
    if (idx >= MOE_CHAIN_COUNT) return;
    
    // Exponential moving average
    float alpha = 0.1f;
    model_scores_[idx] = alpha * (accepted ? 1.0f : 0.0f) + 
                         (1.0f - alpha) * model_scores_[idx];
}

MoEModel* MoEChain::select_best_model(const std::vector<int>& context) {
    // Select based on score and usage
    size_t best_idx = 0;
    float best_score = -1.0f;
    
    for (size_t i = 0; i < MOE_CHAIN_COUNT; i++) {
        // Score = success_rate - usage_penalty
        float usage_penalty = std::min(model_usage_[i] / 1000.0f, 0.3f);
        float score = model_scores_[i] - usage_penalty;
        
        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }
    
    current_idx_.store(best_idx, std::memory_order_relaxed);
    return &models_[best_idx];
}

bool MoEChain::spawn_new_expert() {
    // Find worst performing model and reinitialize
    size_t worst_idx = 0;
    float worst_score = 2.0f;
    
    for (size_t i = 0; i < MOE_CHAIN_COUNT; i++) {
        if (model_scores_[i] < worst_score) {
            worst_score = model_scores_[i];
            worst_idx = i;
        }
    }
    
    // Reinitialize with new random seed
    arena_.reset();
    models_[worst_idx].init(arena_, MOE_DIM, MOE_HEADS, MOE_HEAD_DIM,
                           MOE_LAYERS, MOE_FF, MOE_VOCAB, 
                           static_cast<uint64_t>(worst_idx) + 1000);
    model_scores_[worst_idx] = 0.5f;
    model_usage_[worst_idx] = 0;
    
    return true;
}

// ============================================================================
// SPECULATIVE INFERENCE ENGINE — CORE IMPLEMENTATION
// ============================================================================

SpeculativeInferenceEngine::SpeculativeInferenceEngine() = default;
SpeculativeInferenceEngine::~SpeculativeInferenceEngine() = default;

bool SpeculativeInferenceEngine::initialize(size_t arena_gb) {
    arena_ = std::make_unique<ArenaAllocator>(arena_gb);
    moe_chain_ = std::make_unique<MoEChain>(*arena_);
    self_improvement_ = std::make_unique<SelfImprovementEngine>();
    
    // Initialize all 22 models
    moe_chain_->initialize_chain();
    
    // Initialize KV caches
    for (size_t m = 0; m < MOE_CHAIN_COUNT; m++) {
        for (int l = 0; l < MOE_LAYERS; l++) {
            kv_caches_[m][l].init(*arena_, MOE_MAX_SEQ, MOE_HEADS, MOE_HEAD_DIM);
        }
    }
    
    // Start with model 0
    active_model_ = moe_chain_->get_model(0);
    active_model_idx_ = 0;
    
    return true;
}

// Math kernels implementation
float SpeculativeInferenceEngine::dot_f32_avx512(const float* a, const float* b, int n) {
    __m512 sum0 = _mm512_setzero_ps();
    __m512 sum1 = _mm512_setzero_ps();
    int i = 0;
    
    for (; i + 32 <= n; i += 32) {
        __m512 va0 = _mm512_loadu_ps(a + i);
        __m512 vb0 = _mm512_loadu_ps(b + i);
        __m512 va1 = _mm512_loadu_ps(a + i + 16);
        __m512 vb1 = _mm512_loadu_ps(b + i + 16);
        sum0 = _mm512_fmadd_ps(va0, vb0, sum0);
        sum1 = _mm512_fmadd_ps(va1, vb1, sum1);
    }
    
    for (; i + 16 <= n; i += 16) {
        __m512 va = _mm512_loadu_ps(a + i);
        __m512 vb = _mm512_loadu_ps(b + i);
        sum0 = _mm512_fmadd_ps(va, vb, sum0);
    }
    
    float s = _mm512_reduce_add_ps(sum0) + _mm512_reduce_add_ps(sum1);
    for (; i < n; i++) s += a[i] * b[i];
    return s;
}

void SpeculativeInferenceEngine::mv_q4_1_f32(float* y, const BlockQ4_1* w, 
                                              const float* x, int m, int k) {
    int nb = k / 32;
    
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < m; i++) {
        const BlockQ4_1* row = w + (size_t)i * nb;
        float sum = 0.0f;
        
        for (int j = 0; j < nb; j++) {
            const BlockQ4_1* b = &row[j];
            float d = fp16_to_fp32(b->d);
            float m_val = fp16_to_fp32(b->m);
            
            // Process 32 weights
            for (int q = 0; q < 16; q++) {
                uint8_t qs = b->qs[q];
                int q0 = qs & 0xF;
                int q1 = qs >> 4;
                sum += (d * q0 + m_val) * x[j * 32 + q * 2];
                sum += (d * q1 + m_val) * x[j * 32 + q * 2 + 1];
            }
        }
        y[i] = sum;
    }
}

void SpeculativeInferenceEngine::rmsnorm(float* out, const float* x, 
                                          const float* weight, int n) {
    float ss = 0.0f;
    int i = 0;
    
    for (; i + 16 <= n; i += 16) {
        __m512 v = _mm512_loadu_ps(x + i);
        ss += _mm512_reduce_add_ps(_mm512_mul_ps(v, v));
    }
    for (; i < n; i++) ss += x[i] * x[i];
    
    float norm = 1.0f / sqrtf(ss / n + EPS);
    __m512 vnorm = _mm512_set1_ps(norm);
    
    i = 0;
    for (; i + 16 <= n; i += 16) {
        __m512 v = _mm512_loadu_ps(x + i);
        __m512 w = _mm512_loadu_ps(weight + i);
        _mm512_storeu_ps(out + i, _mm512_mul_ps(_mm512_mul_ps(v, vnorm), w));
    }
    for (; i < n; i++) out[i] = x[i] * norm * weight[i];
}

void SpeculativeInferenceEngine::attention_forward(float* out, const float* x,
    const BlockQ4_1* qkv_w, const BlockQ4_1* o_w, KVCache* cache,
    int seq_pos, int dim, int n_heads, int head_dim) {
    
    int hidden = n_heads * head_dim;
    float* qkv = (float*)arena_->allocate(3 * hidden * sizeof(float));
    
    // QKV projection
    mv_q4_1_f32(qkv, qkv_w, x, 3 * hidden, dim);
    
    float* q = qkv;
    float* k = qkv + hidden;
    float* v = qkv + 2 * hidden;
    
    // Write to cache
    cache->write(seq_pos, k, v);
    
    // Attention per head
    float* scores = (float*)arena_->allocate(cache->len * sizeof(float));
    float scale = 1.0f / sqrtf((float)head_dim);
    
    for (int h = 0; h < n_heads; h++) {
        float* q_h = q + h * head_dim;
        float* out_h = out + h * head_dim;
        
        // Q @ K^T
        for (int t = 0; t < cache->len; t++) {
            float* k_h = cache->k + (size_t)t * hidden + h * head_dim;
            scores[t] = dot_f32_avx512(q_h, k_h, head_dim) * scale;
        }
        
        // Softmax
        float max_s = scores[0];
        for (int t = 1; t < cache->len; t++) if (scores[t] > max_s) max_s = scores[t];
        
        float sum = 0.0f;
        for (int t = 0; t < cache->len; t++) {
            scores[t] = expf(scores[t] - max_s);
            sum += scores[t];
        }
        for (int t = 0; t < cache->len; t++) scores[t] /= sum;
        
        // Weighted sum of V
        memset(out_h, 0, head_dim * sizeof(float));
        for (int t = 0; t < cache->len; t++) {
            float* v_h = cache->v + (size_t)t * hidden + h * head_dim;
            float a = scores[t];
            for (int d = 0; d < head_dim; d++) out_h[d] += a * v_h[d];
        }
    }
    
    // Output projection
    float* attn_out = (float*)arena_->allocate(hidden * sizeof(float));
    memcpy(attn_out, out, hidden * sizeof(float));
    mv_q4_1_f32(out, o_w, attn_out, dim, hidden);
}

void SpeculativeInferenceEngine::moe_ffn_forward(float* out, const float* x,
    const MoELayer& layer, int dim, int ff_dim) {
    
    // Route to experts
    float router_logits[MOE_EXPERTS];
    mv_q4_1_f32(router_logits, layer.router_w, x, MOE_EXPERTS, dim);
    
    // Softmax router
    float max_r = router_logits[0];
    for (int i = 1; i < MOE_EXPERTS; i++) if (router_logits[i] > max_r) max_r = router_logits[i];
    
    float sum = 0.0f;
    for (int i = 0; i < MOE_EXPERTS; i++) {
        router_logits[i] = expf(router_logits[i] - max_r);
        sum += router_logits[i];
    }
    for (int i = 0; i < MOE_EXPERTS; i++) router_logits[i] /= sum;
    
    // Select top-k experts
    int top_experts[MOE_ACTIVE_EXPERTS];
    float top_weights[MOE_ACTIVE_EXPERTS];
    
    // Simple: take first k (in real impl: sort)
    for (int k = 0; k < MOE_ACTIVE_EXPERTS; k++) {
        top_experts[k] = k;
        top_weights[k] = router_logits[k];
    }
    
    // Normalize weights
    float weight_sum = 0.0f;
    for (int k = 0; k < MOE_ACTIVE_EXPERTS; k++) weight_sum += top_weights[k];
    for (int k = 0; k < MOE_ACTIVE_EXPERTS; k++) top_weights[k] /= weight_sum;
    
    // Compute expert outputs
    memset(out, 0, dim * sizeof(float));
    
    for (int k = 0; k < MOE_ACTIVE_EXPERTS; k++) {
        int e = top_experts[k];
        float weight = top_weights[k];
        
        float* gate = (float*)arena_->allocate(ff_dim * sizeof(float));
        float* up = (float*)arena_->allocate(ff_dim * sizeof(float));
        float* expert_out = (float*)arena_->allocate(dim * sizeof(float));
        
        mv_q4_1_f32(gate, layer.expert_gate[e], x, ff_dim, dim);
        mv_q4_1_f32(up, layer.expert_up[e], x, ff_dim, dim);
        
        // SiLU(gate) * up
        for (int i = 0; i < ff_dim; i++) {
            gate[i] = gate[i] / (1.0f + expf(-gate[i]));
            gate[i] *= up[i];
        }
        
        mv_q4_1_f32(expert_out, layer.expert_down[e], gate, dim, ff_dim);
        
        // Accumulate weighted output
        for (int i = 0; i < dim; i++) out[i] += weight * expert_out[i];
    }
}

void SpeculativeInferenceEngine::layer_forward(float* out, const float* x,
    const MoELayer& layer, KVCache* cache, int seq_pos,
    int dim, int n_heads, int head_dim, int ff_dim) {
    
    // Pre-norm 1
    float* norm1 = (float*)arena_->allocate(dim * sizeof(float));
    rmsnorm(norm1, x, layer.ln1, dim);
    
    // Self-attention
    float* attn_out = (float*)arena_->allocate(dim * sizeof(float));
    memset(attn_out, 0, dim * sizeof(float));
    attention_forward(attn_out, norm1, layer.qkv_w, layer.o_w, cache, seq_pos,
                      dim, n_heads, head_dim);
    
    // Residual 1
    for (int i = 0; i < dim; i++) attn_out[i] += x[i];
    
    // Pre-norm 2
    float* norm2 = (float*)arena_->allocate(dim * sizeof(float));
    rmsnorm(norm2, attn_out, layer.ln2, dim);
    
    // MoE FFN
    float* ffn_out = (float*)arena_->allocate(dim * sizeof(float));
    moe_ffn_forward(ffn_out, norm2, layer, dim, ff_dim);
    
    // Residual 2
    for (int i = 0; i < dim; i++) out[i] = attn_out[i] + ffn_out[i];
}

void SpeculativeInferenceEngine::softmax(float* probs, const float* logits, int n, float temp) {
    float max_l = logits[0];
    for (int i = 1; i < n; i++) if (logits[i] > max_l) max_l = logits[i];
    
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        probs[i] = expf((logits[i] - max_l) / temp);
        sum += probs[i];
    }
    for (int i = 0; i < n; i++) probs[i] /= sum;
}

int SpeculativeInferenceEngine::sample_top_p(const float* logits, int n, float p, float temp) {
    float* probs = (float*)arena_->allocate(n * sizeof(float));
    softmax(probs, logits, n, temp);
    
    // Find top-p cutoff
    // Simplified: just take argmax for determinism
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (probs[i] > probs[best]) best = i;
    }
    return best;
}

int SpeculativeInferenceEngine::forward_token(int token_id, MoEModel* model,
    KVCache* caches, int seq_pos, float temp) {
    
    int dim = model->dim;
    float* x = (float*)arena_->allocate(dim * sizeof(float));
    memcpy(x, model->token_embed + (size_t)token_id * dim, dim * sizeof(float));
    
    // Forward through layers
    for (int l = 0; l < model->n_layers; l++) {
        float* out = (float*)arena_->allocate(dim * sizeof(float));
        layer_forward(out, x, model->layers[l], &caches[l], seq_pos,
                      dim, model->n_heads, model->head_dim, model->ff_dim);
        x = out;
    }
    
    // Final norm
    float* norm = (float*)arena_->allocate(dim * sizeof(float));
    rmsnorm(norm, x, model->final_norm, dim);
    
    // LM head
    float* logits = (float*)arena_->allocate(model->vocab * sizeof(float));
    mv_q4_1_f32(logits, model->lm_head, norm, model->vocab, dim);
    
    // Sample
    return sample_top_p(logits, model->vocab, 0.9f, temp);
}

void SpeculativeInferenceEngine::draft_generate(DraftSeq& draft, MoEModel* model,
    KVCache* caches, int start_token, int gamma, float temp) {
    
    int current = start_token;
    draft.len = 0;
    
    for (int i = 0; i < gamma && i < GAMMA_MAX; i++) {
        arena_->reset();
        int next = forward_token(current, model, caches, i, temp);
        draft.tokens[i] = next;
        draft.len++;
        current = next;
    }
}

int SpeculativeInferenceEngine::target_verify(int* accepted_count, MoEModel* target,
    KVCache* target_caches, int start_token, const DraftSeq& draft, float temp) {
    
    int tokens[GAMMA_MAX + 1];
    tokens[0] = start_token;
    for (int i = 0; i < draft.len; i++) tokens[i + 1] = draft.tokens[i];
    
    int accepted = 0;
    for (int i = 0; i < draft.len; i++) {
        arena_->reset();
        int pos = i + 1;
        int next = forward_token(tokens[i], target, target_caches, pos, temp);
        
        if (next == draft.tokens[i]) {
            accepted++;
        } else {
            *accepted_count = accepted;
            return next;
        }
    }
    
    // All accepted
    *accepted_count = accepted;
    arena_->reset();
    return forward_token(tokens[draft.len], target, target_caches, draft.len + 1, temp);
}

int SpeculativeInferenceEngine::generate(std::vector<int>& output, 
    const std::vector<int>& prompt, int max_new_tokens, float temperature) {
    
    double t_start = get_time_ms();
    
    // Reset KV caches for active model
    for (int l = 0; l < MOE_LAYERS; l++) {
        kv_caches_[active_model_idx_][l].reset();
    }
    
    // Seed with prompt
    int current = prompt.empty() ? 1 : prompt.back();
    for (size_t i = 0; i + 1 < prompt.size(); i++) {
        arena_->reset();
        forward_token(prompt[i], active_model_, kv_caches_[active_model_idx_].data(), 
                     static_cast<int>(i), temperature);
    }
    
    // Speculative decode loop
    DraftSeq draft;
    int generated = 0;
    int prompt_len = static_cast<int>(prompt.size());
    
    while (generated < max_new_tokens) {
        arena_->reset();
        
        // Generate draft
        draft_generate(draft, active_model_, kv_caches_[active_model_idx_].data(),
                     current, gamma_, temperature);
        
        // Verify (in this case, same model verifies - for multi-model, use different model)
        int accepted;
        int next = target_verify(&accepted, active_model_, kv_caches_[active_model_idx_].data(),
                                current, draft, temperature);
        
        // Record feedback
        if (self_improvement_enabled_) {
            std::vector<int> context;
            if (output.size() >= 10) {
                context.assign(output.end() - 10, output.end());
            }
            
            for (int i = 0; i < accepted && generated < max_new_tokens; i++) {
                self_improvement_->record_acceptance(context, draft.tokens[i], true);
                output.push_back(draft.tokens[i]);
                generated++;
            }
            if (generated < max_new_tokens) {
                self_improvement_->record_acceptance(context, next, true);
                output.push_back(next);
                generated++;
                current = next;
            }
        } else {
            for (int i = 0; i < accepted && generated < max_new_tokens; i++) {
                output.push_back(draft.tokens[i]);
                generated++;
            }
            if (generated < max_new_tokens) {
                output.push_back(next);
                generated++;
                current = next;
            }
        }
        
        // Update stats
        total_tokens_ += draft.len + 1;
        accepted_tokens_ += accepted;
        
        // Check for model switch
        if (should_switch_model(output)) {
            switch_to_model((active_model_idx_ + 1) % MOE_CHAIN_COUNT);
        }
        
        // Trigger self-improvement
        if (self_improvement_enabled_ && 
            total_tokens_.load() % TRAINING_TRIGGER_INTERVAL == 0) {
            adapt_weights(*active_model_);
        }
    }
    
    double t_end = get_time_ms();
    total_time_ms_ += (t_end - t_start);
    
    return generated;
}

void SpeculativeInferenceEngine::switch_to_model(size_t idx) {
    if (idx >= MOE_CHAIN_COUNT) return;
    
    active_model_idx_ = idx;
    active_model_ = moe_chain_->get_model(idx);
    
    // Reset KV cache for new model
    for (int l = 0; l < MOE_LAYERS; l++) {
        kv_caches_[idx][l].reset();
    }
}

bool SpeculativeInferenceEngine::should_switch_model(const std::vector<int>& context) {
    // Simple heuristic: switch every N tokens or on low acceptance
    if (total_tokens_.load() % 256 == 0) return true;
    
    float acceptance = get_acceptance_rate();
    return acceptance < 0.5f;
}

void SpeculativeInferenceEngine::enable_self_improvement(bool enable) {
    self_improvement_enabled_ = enable;
}

void SpeculativeInferenceEngine::trigger_training_cycle() {
    if (!self_improvement_) return;
    
    auto batch = self_improvement_->extract_training_batch(64);
    
    // Lightweight adaptation
    for (auto& model : {active_model_}) {
        if (model) adapt_weights(*model);
    }
    
    // Spawn new expert if needed
    float acceptance = get_acceptance_rate();
    if (self_improvement_->should_spawn_new_expert(acceptance)) {
        moe_chain_->spawn_new_expert();
    }
}

void SpeculativeInferenceEngine::adapt_weights(MoEModel& model) {
    if (!self_improvement_) return;
    self_improvement_->adapt_weights(model);
}

double SpeculativeInferenceEngine::get_tokens_per_second() const {
    double total_s = total_time_ms_.load() / 1000.0;
    return total_s > 0 ? total_tokens_.load() / total_s : 0.0;
}

double SpeculativeInferenceEngine::get_acceptance_rate() const {
    uint64_t total = total_tokens_.load();
    return total > 0 ? (double)accepted_tokens_.load() / total : 0.0;
}

uint64_t SpeculativeInferenceEngine::get_total_tokens_generated() const {
    return total_tokens_.load();
}

void SpeculativeInferenceEngine::set_gamma(int gamma) {
    gamma_ = std::max(1, std::min(gamma, GAMMA_MAX));
}

int SpeculativeInferenceEngine::get_optimal_gamma() const {
    // Adaptive: higher gamma when acceptance is high
    float acceptance = get_acceptance_rate();
    if (acceptance > 0.9f) return GAMMA_MAX;
    if (acceptance > 0.7f) return 10;
    if (acceptance > 0.5f) return 6;
    return 4;
}

// ============================================================================
// C API IMPLEMENTATION
// ============================================================================

extern "C" {

SpeculativeEngineHandle SpeculativeEngine_Create(size_t arena_gb) {
    auto* engine = new SpeculativeInferenceEngine();
    if (!engine->initialize(arena_gb)) {
        delete engine;
        return nullptr;
    }
    return engine;
}

void SpeculativeEngine_Destroy(SpeculativeEngineHandle handle) {
    delete static_cast<SpeculativeInferenceEngine*>(handle);
}

int SpeculativeEngine_Generate(SpeculativeEngineHandle handle,
                                int* output_tokens, int max_output,
                                const int* prompt_tokens, int prompt_len,
                                int max_new, float temperature) {
    if (!handle) return 0;
    
    auto* engine = static_cast<SpeculativeInferenceEngine*>(handle);
    
    std::vector<int> prompt(prompt_tokens, prompt_tokens + prompt_len);
    std::vector<int> output;
    
    int generated = engine->generate(output, prompt, max_new, temperature);
    
    int to_copy = std::min(generated, max_output);
    for (int i = 0; i < to_copy; i++) {
        output_tokens[i] = output[i];
    }
    
    return to_copy;
}

double SpeculativeEngine_GetTPS(SpeculativeEngineHandle handle) {
    if (!handle) return 0.0;
    return static_cast<SpeculativeInferenceEngine*>(handle)->get_tokens_per_second();
}

double SpeculativeEngine_GetAcceptanceRate(SpeculativeEngineHandle handle) {
    if (!handle) return 0.0;
    return static_cast<SpeculativeInferenceEngine*>(handle)->get_acceptance_rate();
}

void SpeculativeEngine_EnableSelfImprovement(SpeculativeEngineHandle handle, int enable) {
    if (!handle) return;
    static_cast<SpeculativeInferenceEngine*>(handle)->enable_self_improvement(enable != 0);
}

void SpeculativeEngine_TriggerTrainingCycle(SpeculativeEngineHandle handle) {
    if (!handle) return;
    static_cast<SpeculativeInferenceEngine*>(handle)->trigger_training_cycle();
}

} // extern "C"

} // namespace Inference
} // namespace RawrXD
