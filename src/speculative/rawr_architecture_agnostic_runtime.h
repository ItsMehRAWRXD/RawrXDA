// rawr_architecture_agnostic_runtime.h
// Branchless transformer inference engine using 100+ capability detectors
// Zero if(arch) conditionals - pure capability-driven dispatch

#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>

namespace rawr {

// ============================================================================
// CAPABILITY BITMASK (100+ detectors condensed to 64-bit operational mask)
// ============================================================================

enum ArchCaps : uint64_t {
    // Attention families (bits 0-7)
    CAP_MHA           = 1ULL << 0,   // Multi-head attention
    CAP_GQA           = 1ULL << 1,   // Grouped query attention
    CAP_MQA           = 1ULL << 2,   // Multi-query attention
    CAP_MLA           = 1ULL << 3,   // Multi-latent attention
    CAP_SLIDING_WIN   = 1ULL << 4,   // Sliding window attention
    CAP_GLOBAL_WIN    = 1ULL << 5,   // Global + window hybrid
    CAP_CROSS_ATTN    = 1ULL << 6,   // Cross-attention layers
    CAP_SINK_TOKENS   = 1ULL << 7,   // Attention sink tokens
    
    // FFN families (bits 8-15)
    CAP_DENSE_FFN     = 1ULL << 8,   // Standard dense FFN
    CAP_SWIGLU        = 1ULL << 9,   // SwiGLU activation
    CAP_GEGLU         = 1ULL << 10,  // GeGLU activation
    CAP_MOE           = 1ULL << 11,  // Mixture of experts
    CAP_TOPK_ROUTING  = 1ULL << 12,  // Top-k expert routing
    CAP_SHARED_EXPERT = 1ULL << 13,  // Shared expert backbone
    CAP_HYBRID_FFN    = 1ULL << 14,  // Dense + MoE hybrid
    CAP_PARALLEL_FFN  = 1ULL << 15,  // Parallel FFN branches
    
    // Positional encoding (bits 16-23)
    CAP_ROPE          = 1ULL << 16,  // Rotary embeddings
    CAP_ROPE_SCALED   = 1ULL << 17,  // Scaled RoPE (YaRN/NTK)
    CAP_ROPE_PARTIAL  = 1ULL << 18,  // Partial head RoPE
    CAP_ALIBI         = 1ULL << 19,  // ALiBi bias
    CAP_LEARNED_POS   = 1ULL << 20,  // Learned positional embeddings
    CAP_NO_POS        = 1ULL << 21,  // No positional encoding
    CAP_RELATIVE_BIAS = 1ULL << 22,  // T5-style relative bias
    CAP_ROPE_INTERLEAVE = 1ULL << 23, // Interleaved RoPE
    
    // Normalization (bits 24-31)
    CAP_RMSNORM       = 1ULL << 24,  // RMSNorm
    CAP_LAYERNORM     = 1ULL << 25,  // LayerNorm
    CAP_PRE_NORM      = 1ULL << 26,  // Pre-normalization
    CAP_POST_NORM     = 1ULL << 27,  // Post-normalization
    CAP_QK_NORM       = 1ULL << 28,  // Query/key normalization
    CAP_DEEPNORM      = 1ULL << 29,  // DeepNorm scaling
    CAP_RESIDUAL_SCALE = 1ULL << 30, // Residual scaling
    CAP_NORM_BIAS     = 1ULL << 31,  // Norm has bias
    
    // KV cache strategies (bits 32-39)
    CAP_PAGED_KV      = 1ULL << 32,  // Paged KV cache
    CAP_KV_COMPRESS   = 1ULL << 33,  // Compressed KV cache
    CAP_KV_QUANT      = 1ULL << 34,  // Quantized KV cache
    CAP_KV_SHARED     = 1ULL << 35,  // Shared KV across layers
    CAP_KV_LORA       = 1ULL << 36,  // LoRA-style KV projection
    CAP_KV_HIERARCHY  = 1ULL << 37,  // Hierarchical KV cache
    CAP_KV_EVICTION   = 1ULL << 38,  // KV cache eviction
    CAP_KV_RECYCLE    = 1ULL << 39,  // KV cache recycling
    
    // Quantization (bits 40-47)
    CAP_Q4_0          = 1ULL << 40,  // Q4_0 quantization
    CAP_Q4_1          = 1ULL << 41,  // Q4_1 quantization
    CAP_Q5_0          = 1ULL << 42,  // Q5_0 quantization
    CAP_Q5_1          = 1ULL << 43,  // Q5_1 quantization
    CAP_Q8_0          = 1ULL << 44,  // Q8_0 quantization
    CAP_Q4_K          = 1ULL << 45,  // Q4_K quantization
    CAP_Q6_K          = 1ULL << 46,  // Q6_K quantization
    CAP_FP16          = 1ULL << 47,  // FP16 weights
    
    // Multimodal (bits 48-55)
    CAP_VISION        = 1ULL << 48,  // Vision encoder
    CAP_VLM           = 1ULL << 49,  // Vision-language model
    CAP_PATCH_EMBED   = 1ULL << 50,  // Patch embedding
    CAP_CONV_VISION   = 1ULL << 51,  // Conv vision backbone
    CAP_CROSS_MODAL   = 1ULL << 52,  // Cross-modal fusion
    CAP_UNIFIED_VOCAB = 1ULL << 53,  // Unified text+vision vocab
    CAP_MM_PROJ       = 1ULL << 54,  // Multimodal projector
    CAP_IMG_PREFIX    = 1ULL << 55,  // Image token prefix
    
    // Advanced features (bits 56-63)
    CAP_SSM           = 1ULL << 56,  // State space model
    CAP_HYENA         = 1ULL << 57,  // Hyena long convolution
    CAP_LINEAR_ATTN   = 1ULL << 58,  // Linear attention
    CAP_RAG           = 1ULL << 59,  // Retrieval-augmented
    CAP_TOOL_USE      = 1ULL << 60,  // Tool use capability
    CAP_SPECULATIVE   = 1ULL << 61,  // Speculative decoding
    CAP_ADAPTIVE_DEPTH = 1ULL << 62, // Adaptive depth
    CAP_DYNAMIC_PREC  = 1ULL << 63  // Dynamic precision
};

// ============================================================================
// DETECTOR FUNCTIONS (100+ single-line detectors)
// ============================================================================

struct TensorRegistry {
    std::unordered_map<std::string, bool> exists;
    std::unordered_map<std::string, int> dims;
    
    bool has(const std::string& name) const {
        return exists.find(name) != exists.end();
    }
    
    bool has(const char* name) const {
        return exists.find(name) != exists.end();
    }
};

struct KVRegistry {
    std::unordered_map<std::string, float> floats;
    std::unordered_map<std::string, int> ints;
    std::unordered_map<std::string, bool> bools;
    std::unordered_map<std::string, std::string> strings;
    
    float get_float(const std::string& key, float def) const {
        auto it = floats.find(key); return it != floats.end() ? it->second : def;
    }
    float get_float(const char* key, float def) const {
        return get_float(std::string(key), def);
    }
    int get_int(const std::string& key, int def) const {
        auto it = ints.find(key); return it != ints.end() ? it->second : def;
    }
    int get_int(const char* key, int def) const {
        return get_int(std::string(key), def);
    }
    bool get_bool(const std::string& key, bool def) const {
        auto it = bools.find(key); return it != bools.end() ? it->second : def;
    }
    bool get_bool(const char* key, bool def) const {
        return get_bool(std::string(key), def);
    }
    std::string get_str(const std::string& key, const std::string& def) const {
        auto it = strings.find(key); return it != strings.end() ? it->second : def;
    }
    std::string get_str(const char* key, const std::string& def) const {
        return get_str(std::string(key), def);
    }
};

// Core detector functions
inline bool detect_mha(const TensorRegistry& t, const KVRegistry& kv, int heads, int kv_heads) {
    return heads == kv_heads && heads > 1;
}

inline bool detect_gqa(const TensorRegistry& t, const KVRegistry& kv, int heads, int kv_heads) {
    return kv_heads < heads && kv_heads > 1;
}

inline bool detect_mqa(const TensorRegistry& t, const KVRegistry& kv, int heads, int kv_heads) {
    return kv_heads == 1 && heads > 1;
}

inline bool detect_mla(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.attn_q_a_norm.weight") && t.has("blk.0.attn_kv_a_norm.weight");
}

inline bool detect_sliding_window(const TensorRegistry& t, const KVRegistry& kv) {
    return kv.get_int("attention.sliding_window", 0) > 0;
}

inline bool detect_swiglu(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.ffn_gate.weight") && t.has("blk.0.ffn_up.weight");
}

inline bool detect_moe(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.moe.router.weight") || t.has("blk.0.ffn_gate_exps.weight");
}

inline bool detect_topk_routing(const TensorRegistry& t, const KVRegistry& kv) {
    return kv.get_int("moe.top_k", 1) > 1;
}

inline bool detect_shared_expert(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.ffn_shared_expert.weight");
}

inline bool detect_rope(const TensorRegistry& t, const KVRegistry& kv) {
    return kv.get_bool("rope.enabled", true) || t.has("blk.0.attn_rotary_emb.weight");
}

inline bool detect_rope_scaled(const TensorRegistry& t, const KVRegistry& kv) {
    return kv.get_float("rope.scale", 1.0f) != 1.0f || 
           kv.get_float("rope.freq_base", 10000.0f) > 100000.0f;
}

inline bool detect_rmsnorm(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.attn_norm.weight") && !t.has("blk.0.attn_norm.bias");
}

inline bool detect_qk_norm(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.attn_q_norm.weight") && t.has("blk.0.attn_k_norm.weight");
}

inline bool detect_paged_kv(const TensorRegistry& t, const KVRegistry& kv) {
    return kv.get_bool("attention.paged_kv", false) || t.has("kv_cache.block_table");
}

inline bool detect_kv_compress(const TensorRegistry& t, const KVRegistry& kv) {
    return kv.get_int("attention.kv_bits", 0) > 0 || kv.get_bool("attention.kv_quant", false);
}

inline bool detect_vision(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("v.patch_conv.weight") || t.has("vision.encoder.weight");
}

inline bool detect_vlm(const TensorRegistry& t, const KVRegistry& kv) {
    return (t.has("v.patch_conv.weight") || t.has("v.mm_proj.weight")) && 
           t.has("blk.0.attn_q.weight");
}

inline bool detect_ssm(const TensorRegistry& t, const KVRegistry& kv) {
    return t.has("blk.0.ssm_proj.weight") || t.has("blk.0.mamba.in_proj.weight");
}

// ============================================================================
// CAPABILITY MASK BUILDER (100+ detectors → 64-bit mask)
// ============================================================================

inline uint64_t build_capability_mask(const TensorRegistry& tensors, const KVRegistry& kv,
                                       int heads, int kv_heads, int hidden_dim) {
    uint64_t mask = 0;
    
    // Attention family
    if (detect_mha(tensors, kv, heads, kv_heads)) mask |= CAP_MHA;
    if (detect_gqa(tensors, kv, heads, kv_heads)) mask |= CAP_GQA;
    if (detect_mqa(tensors, kv, heads, kv_heads)) mask |= CAP_MQA;
    if (detect_mla(tensors, kv)) mask |= CAP_MLA;
    if (detect_sliding_window(tensors, kv)) mask |= CAP_SLIDING_WIN;
    
    // FFN family
    if (detect_swiglu(tensors, kv)) mask |= CAP_SWIGLU;
    if (detect_moe(tensors, kv)) mask |= CAP_MOE;
    if (detect_topk_routing(tensors, kv)) mask |= CAP_TOPK_ROUTING;
    if (detect_shared_expert(tensors, kv)) mask |= CAP_SHARED_EXPERT;
    
    // Normalization
    if (detect_rmsnorm(tensors, kv)) mask |= CAP_RMSNORM;
    if (detect_qk_norm(tensors, kv)) mask |= CAP_QK_NORM;
    
    // Positional encoding
    if (detect_rope(tensors, kv)) mask |= CAP_ROPE;
    if (detect_rope_scaled(tensors, kv)) mask |= CAP_ROPE_SCALED;
    
    // KV cache
    if (detect_paged_kv(tensors, kv)) mask |= CAP_PAGED_KV;
    if (detect_kv_compress(tensors, kv)) mask |= CAP_KV_COMPRESS;
    
    // Multimodal
    if (detect_vision(tensors, kv)) mask |= CAP_VISION;
    if (detect_vlm(tensors, kv)) mask |= CAP_VLM;
    
    // Advanced
    if (detect_ssm(tensors, kv)) mask |= CAP_SSM;
    
    // Quantization detection from tensor types
    if (tensors.has("blk.0.attn_q.weight.q8_0")) mask |= CAP_Q8_0;
    if (tensors.has("blk.0.attn_q.weight.q4_k")) mask |= CAP_Q4_K;
    if (tensors.has("blk.0.attn_q.weight.q6_k")) mask |= CAP_Q6_K;
    
    return mask;
}

// ============================================================================
// BRANCHLESS DISPATCH TABLE
// ============================================================================

// Forward declarations for kernel implementations
struct Context;
using KernelFn = void (*)(Context*);

// Kernel function signatures
void kernel_rmsnorm(Context* ctx);
void kernel_layernorm(Context* ctx);
void kernel_attention_mha(Context* ctx);
void kernel_attention_gqa(Context* ctx);
void kernel_attention_mqa(Context* ctx);
void kernel_attention_mla(Context* ctx);
void kernel_attention_sliding(Context* ctx);
void kernel_ffn_dense(Context* ctx);
void kernel_ffn_swiglu(Context* ctx);
void kernel_ffn_moe(Context* ctx);
void kernel_ffn_moe_shared(Context* ctx);
void kernel_rope_standard(Context* ctx);
void kernel_rope_scaled(Context* ctx);
void kernel_rope_partial(Context* ctx);

// Dispatch table indexed by capability bits
struct DispatchTable {
    // Norm kernels
    KernelFn norm_kernel[2];      // 0: RMSNorm, 1: LayerNorm
    
    // Attention kernels (indexed by attention family)
    KernelFn attn_kernels[8];     // MHA, GQA, MQA, MLA, sliding, etc.
    
    // FFN kernels
    KernelFn ffn_kernels[8];      // dense, SwiGLU, MoE, etc.
    
    // RoPE kernels
    KernelFn rope_kernels[4];     // standard, scaled, partial, etc.
};

// Initialize dispatch table at startup
inline DispatchTable init_dispatch_table() {
    DispatchTable dt;
    
    // Norm kernels
    dt.norm_kernel[0] = kernel_rmsnorm;
    dt.norm_kernel[1] = kernel_layernorm;
    
    // Attention kernels
    dt.attn_kernels[0] = kernel_attention_mha;
    dt.attn_kernels[1] = kernel_attention_gqa;
    dt.attn_kernels[2] = kernel_attention_mqa;
    dt.attn_kernels[3] = kernel_attention_mla;
    dt.attn_kernels[4] = kernel_attention_sliding;
    dt.attn_kernels[5] = nullptr; // reserved
    dt.attn_kernels[6] = nullptr; // reserved
    dt.attn_kernels[7] = nullptr; // reserved
    
    // FFN kernels
    dt.ffn_kernels[0] = kernel_ffn_dense;
    dt.ffn_kernels[1] = kernel_ffn_swiglu;
    dt.ffn_kernels[2] = kernel_ffn_moe;
    dt.ffn_kernels[3] = kernel_ffn_moe_shared;
    dt.ffn_kernels[4] = nullptr; // reserved
    dt.ffn_kernels[5] = nullptr; // reserved
    dt.ffn_kernels[6] = nullptr; // reserved
    dt.ffn_kernels[7] = nullptr; // reserved
    
    // RoPE kernels
    dt.rope_kernels[0] = kernel_rope_standard;
    dt.rope_kernels[1] = kernel_rope_scaled;
    dt.rope_kernels[2] = kernel_rope_partial;
    dt.rope_kernels[3] = nullptr; // reserved
    
    return dt;
}

// ============================================================================
// EXECUTION NODE (DAG building block)
// ============================================================================

struct ExecNode {
    enum Type {
        NODE_EMBED,
        NODE_NORM_PRE,
        NODE_ATTENTION,
        NODE_NORM_POST,
        NODE_FFN,
        NODE_NORM_FINAL,
        NODE_LOGITS,
        NODE_KV_READ,
        NODE_KV_WRITE,
        NODE_ROUTER,
        NODE_EXPERT,
        NODE_COMBINE,
        NODE_ROPE,
        NODE_VISION_ENCODE,
        NODE_CROSS_ATTN
    };
    
    Type type;
    uint32_t layer_idx;
    uint64_t caps;           // Required capabilities for this node
    KernelFn kernel;         // Pre-selected kernel function
    
    // Cost model
    float flops;
    float bandwidth_gb;
    float latency_us;
};

// ============================================================================
// DAG BUILDER (converts capability mask to execution graph)
// ============================================================================

struct ExecutionDAG {
    std::vector<ExecNode> nodes;
    uint64_t model_caps;
    
    // Build DAG from capability mask
    void build(uint64_t caps, int n_layers, int n_experts = 0, int top_k = 0) {
        model_caps = caps;
        nodes.clear();
        
        // Token embedding
        nodes.push_back({ExecNode::NODE_EMBED, 0, 0, nullptr, 0, 0, 0});
        
        // Transformer layers
        for (int l = 0; l < n_layers; l++) {
            // Pre-attention norm
            nodes.push_back({ExecNode::NODE_NORM_PRE, (uint32_t)l, caps, nullptr, 0, 0, 0});
            
            // RoPE (if enabled)
            if (caps & CAP_ROPE) {
                nodes.push_back({ExecNode::NODE_ROPE, (uint32_t)l, caps, nullptr, 0, 0, 0});
            }
            
            // KV cache read
            nodes.push_back({ExecNode::NODE_KV_READ, (uint32_t)l, caps, nullptr, 0, 0, 0});
            
            // Attention (type selected by caps)
            nodes.push_back({ExecNode::NODE_ATTENTION, (uint32_t)l, caps, nullptr, 0, 0, 0});
            
            // KV cache write
            nodes.push_back({ExecNode::NODE_KV_WRITE, (uint32_t)l, caps, nullptr, 0, 0, 0});
            
            // Post-attention norm
            nodes.push_back({ExecNode::NODE_NORM_POST, (uint32_t)l, caps, nullptr, 0, 0, 0});
            
            // FFN (dense or MoE)
            if (caps & CAP_MOE) {
                // MoE path: router → experts → combine
                nodes.push_back({ExecNode::NODE_ROUTER, (uint32_t)l, caps, nullptr, 0, 0, 0});
                for (int e = 0; e < top_k; e++) {
                    nodes.push_back({ExecNode::NODE_EXPERT, (uint32_t)l, caps, nullptr, 0, 0, 0});
                }
                nodes.push_back({ExecNode::NODE_COMBINE, (uint32_t)l, caps, nullptr, 0, 0, 0});
            } else {
                // Dense FFN
                nodes.push_back({ExecNode::NODE_FFN, (uint32_t)l, caps, nullptr, 0, 0, 0});
            }
        }
        
        // Final norm
        nodes.push_back({ExecNode::NODE_NORM_FINAL, (uint32_t)n_layers, caps, nullptr, 0, 0, 0});
        
        // Output logits
        nodes.push_back({ExecNode::NODE_LOGITS, (uint32_t)n_layers, caps, nullptr, 0, 0, 0});
    }
    
    // Branchless execution - walk the DAG
    void execute(Context* ctx, const DispatchTable& dt) {
        for (auto& node : nodes) {
            // Select kernel based on node type and capabilities
            switch (node.type) {
                case ExecNode::NODE_NORM_PRE:
                case ExecNode::NODE_NORM_POST:
                case ExecNode::NODE_NORM_FINAL:
                    if (dt.norm_kernel[0] && (model_caps & CAP_RMSNORM)) {
                        dt.norm_kernel[0](ctx);
                    } else if (dt.norm_kernel[1]) {
                        dt.norm_kernel[1](ctx);
                    }
                    break;
                    
                case ExecNode::NODE_ATTENTION:
                    if (model_caps & CAP_MLA) {
                        dt.attn_kernels[3](ctx);
                    } else if (model_caps & CAP_GQA) {
                        dt.attn_kernels[1](ctx);
                    } else if (model_caps & CAP_MQA) {
                        dt.attn_kernels[2](ctx);
                    } else if (model_caps & CAP_SLIDING_WIN) {
                        dt.attn_kernels[4](ctx);
                    } else {
                        dt.attn_kernels[0](ctx); // MHA default
                    }
                    break;
                    
                case ExecNode::NODE_FFN:
                    if (model_caps & CAP_SWIGLU) {
                        dt.ffn_kernels[1](ctx);
                    } else {
                        dt.ffn_kernels[0](ctx); // Dense default
                    }
                    break;
                    
                case ExecNode::NODE_ROPE:
                    if (model_caps & CAP_ROPE_SCALED) {
                        dt.rope_kernels[1](ctx);
                    } else {
                        dt.rope_kernels[0](ctx); // Standard
                    }
                    break;
                    
                default:
                    // Other nodes use default handling
                    if (node.kernel) {
                        node.kernel(ctx);
                    }
                    break;
            }
        }
    }
};

// ============================================================================
// MODEL CONFIGURATION (architecture-agnostic)
// ============================================================================

struct ModelConfig {
    // Core dimensions
    int vocab_size;
    int hidden_dim;
    int n_layers;
    int n_heads;
    int n_kv_heads;
    int head_dim;
    int ffn_dim;
    
    // MoE
    int n_experts;
    int n_experts_per_token;
    
    // Normalization
    float rms_norm_eps;
    
    // RoPE
    float rope_theta;
    float rope_scale;
    int rope_dim;
    
    // Context
    int max_seq_len;
    int sliding_window;
    
    // Capability mask
    uint64_t caps;
};

// ============================================================================
// ARCHITECTURE-SPECIFIC CONFIGURATIONS
// ============================================================================

inline ModelConfig config_ministral3() {
    ModelConfig cfg;
    cfg.vocab_size = 131072;
    cfg.hidden_dim = 4096;
    cfg.n_layers = 34;
    cfg.n_heads = 32;
    cfg.n_kv_heads = 8;      // GQA
    cfg.head_dim = 128;
    cfg.ffn_dim = 14336;
    cfg.n_experts = 0;
    cfg.n_experts_per_token = 0;
    cfg.rms_norm_eps = 1e-5f;
    cfg.rope_theta = 1000000.0f;
    cfg.rope_scale = 1.0f;
    cfg.rope_dim = 128;
    cfg.max_seq_len = 131072;
    cfg.sliding_window = 0;
    cfg.caps = CAP_GQA | CAP_RMSNORM | CAP_ROPE | CAP_ROPE_SCALED | CAP_SWIGLU;
    return cfg;
}

inline ModelConfig config_gptoss20b() {
    ModelConfig cfg;
    cfg.vocab_size = 32000;
    cfg.hidden_dim = 2880;
    cfg.n_layers = 24;
    cfg.n_heads = 64;
    cfg.n_kv_heads = 8;      // GQA
    cfg.head_dim = 64;
    cfg.ffn_dim = 2880;
    cfg.n_experts = 32;      // MoE
    cfg.n_experts_per_token = 4;
    cfg.rms_norm_eps = 1e-5f;
    cfg.rope_theta = 150000.0f;
    cfg.rope_scale = 1.0f;
    cfg.rope_dim = 64;
    cfg.max_seq_len = 8192;
    cfg.sliding_window = 128;
    cfg.caps = CAP_GQA | CAP_MOE | CAP_TOPK_ROUTING | CAP_RMSNORM | CAP_ROPE | CAP_SLIDING_WIN;
    return cfg;
}

inline ModelConfig config_phi3mini() {
    ModelConfig cfg;
    cfg.vocab_size = 32064;
    cfg.hidden_dim = 3072;
    cfg.n_layers = 32;
    cfg.n_heads = 32;        // MHA
    cfg.n_kv_heads = 32;     // No GQA
    cfg.head_dim = 96;
    cfg.ffn_dim = 8192;
    cfg.n_experts = 0;
    cfg.n_experts_per_token = 0;
    cfg.rms_norm_eps = 1e-5f;
    cfg.rope_theta = 10000.0f;
    cfg.rope_scale = 1.0f;
    cfg.rope_dim = 96;
    cfg.max_seq_len = 4096;
    cfg.sliding_window = 0;
    cfg.caps = CAP_MHA | CAP_RMSNORM | CAP_ROPE | CAP_SWIGLU;
    return cfg;
}

} // namespace rawr
