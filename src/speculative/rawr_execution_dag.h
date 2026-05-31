// rawr_execution_dag.h - Feature-to-Cost-Model Compiler
// Converts 100-bit capability fingerprint into per-layer execution DAG
// Zero-branch inference scheduling with FLOP/memory/latency weights

#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace rawr {

// ============================================================================
// 100-Bit Capability Mask (from detectors 1-100)
// ============================================================================
enum ArchCaps : uint64_t {
    // Attention Family (bits 0-15)
    CAP_GQA = 1ULL << 0,
    CAP_MQA = 1ULL << 1,
    CAP_MLA = 1ULL << 2,
    CAP_SWA = 1ULL << 3,           // Sliding Window Attention
    CAP_GLOBAL_LOCAL_HYBRID = 1ULL << 4,
    CAP_ALIBI = 1ULL << 5,
    CAP_ROPE = 1ULL << 6,
    CAP_ROPE_SCALED = 1ULL << 7,
    CAP_ROPE_INTERLEAVED = 1ULL << 8,
    CAP_PARTIAL_ROPE = 1ULL << 9,
    CAP_QK_NORM = 1ULL << 10,
    CAP_ATTN_BIAS = 1ULL << 11,
    CAP_ATTN_TEMPERATURE = 1ULL << 12,
    CAP_ATTN_CLIPPING = 1ULL << 13,
    CAP_ATTN_SINK = 1ULL << 14,
    CAP_ATTN_SPARSE = 1ULL << 15,
    
    // MoE Family (bits 16-25)
    CAP_MOE = 1ULL << 16,
    CAP_MOE_TOPK = 1ULL << 17,
    CAP_MOE_ROUTERLESS = 1ULL << 18,
    CAP_MOE_HIERARCHICAL = 1ULL << 19,
    CAP_MOE_SHARED_EXPERT = 1ULL << 20,
    CAP_MOE_DYNAMIC = 1ULL << 21,
    CAP_MOE_TOKEN_ROUTED = 1ULL << 22,
    CAP_MOE_BLOCK_SPARSE = 1ULL << 23,
    CAP_MOE_EXPERT_BIAS = 1ULL << 24,
    CAP_MOE_SELF_MOD = 1ULL << 25,
    
    // FFN Family (bits 26-35)
    CAP_SWIGLU = 1ULL << 26,
    CAP_GLU_VARIANT = 1ULL << 27,
    CAP_FFN_LOW_RANK = 1ULL << 28,
    CAP_FFN_BOTTLENECK = 1ULL << 29,
    CAP_FFN_MULTIPATH = 1ULL << 30,
    CAP_FFN_BYPASS = 1ULL << 31,
    CAP_PARALLEL_RESIDUAL = 1ULL << 32,
    CAP_GATED_RESIDUAL = 1ULL << 33,
    CAP_POST_NORM = 1ULL << 34,
    CAP_PRE_NORM = 1ULL << 35,
    
    // KV Cache Family (bits 36-45)
    CAP_KV_PAGED = 1ULL << 36,
    CAP_KV_COMPRESSED = 1ULL << 37,
    CAP_KV_LORA = 1ULL << 38,
    CAP_KV_QUANTIZED = 1ULL << 39,
    CAP_KV_SHARED = 1ULL << 40,
    CAP_KV_RECYCLED = 1ULL << 41,
    CAP_KV_MULTI_RES = 1ULL << 42,
    CAP_KV_EVICTION_POLICY = 1ULL << 43,
    CAP_KV_ROLLING = 1ULL << 44,
    CAP_KV_DOWNPROJ = 1ULL << 45,
    CAP_KV_SPARSE = 1ULL << 46,
    
    // Vision/Multimodal (bits 46-50)
    CAP_VISION = 1ULL << 46,
    CAP_VLM_INTERLEAVE = 1ULL << 47,
    CAP_CROSSMODAL_FUSION = 1ULL << 48,
    CAP_CONV_PATCH = 1ULL << 49,
    CAP_TOKEN_FUSION = 1ULL << 50,
    
    // Dynamic/Adaptive (bits 51-58)
    CAP_ADAPTIVE_DEPTH = 1ULL << 51,
    CAP_MIXTURE_DEPTH = 1ULL << 52,
    CAP_DYNAMIC_PRECISION = 1ULL << 53,
    CAP_CONTEXT_ROUTED = 1ULL << 54,
    CAP_DYNAMIC_HEADS = 1ULL << 55,
    CAP_TOKEN_SPARSIFICATION = 1ULL << 56,
    CAP_HEAD_PRUNING = 1ULL << 57,
    CAP_HEAD_MERGING = 1ULL << 58,
    
    // Advanced (bits 59-63)
    CAP_MTP = 1ULL << 59,           // Multi-Token Prediction
    CAP_SSM = 1ULL << 60,
    CAP_REcurrent_HYBRID = 1ULL << 61,
    CAP_COMPUTE_BUDGET = 1ULL << 62,
    CAP_SELF_MOD_ROUTING = 1ULL << 63
};

// ============================================================================
// Cost Model Weights (empirically tuned)
// ============================================================================
struct CostWeights {
    // Base FLOP costs (relative units)
    static constexpr float FLOP_ATTN_PER_HEAD = 2.0f;      // Q@K^T per head
    static constexpr float FLOP_ATTN_GQA_REDUCTION = 0.6f;  // GQA saves 40%
    static constexpr float FLOP_ATTN_MLA_REDUCTION = 0.4f;  // MLA saves 60%
    static constexpr float FLOP_MOE_ROUTING = 0.5f;         // Router overhead
    static constexpr float FLOP_MOE_EXPERT = 2.0f;          // Expert compute
    static constexpr float FLOP_SWIGLU = 1.5f;              // Extra matmul
    static constexpr float FLOP_QK_NORM = 0.1f;             // Layernorm overhead
    
    // Memory bandwidth costs (bytes per element)
    static constexpr float MEM_KV_CACHE = 4.0f;            // FP32 KV per token
    static constexpr float MEM_KV_PAGED_OVERHEAD = 1.2f;  // Block table overhead
    static constexpr float MEM_KV_COMPRESSED = 0.5f;       // Compressed ratio
    static constexpr float MEM_MOE_EXPERT_SWITCH = 2.0f;   // Expert loading cost
    
    // Latency penalties (microseconds, relative)
    static constexpr float LATENCY_MOE_ROUTING = 5.0f;
    static constexpr float LATENCY_PAGED_ATTN = 3.0f;
    static constexpr float LATENCY_DYNAMIC_DEPTH = 2.0f;
    static constexpr float LATENCY_PRECISION_SWITCH = 1.0f;
    
    // Cache residency penalties (0-1, higher = worse locality)
    static constexpr float CACHE_KV_SPARSE = 0.3f;
    static constexpr float CACHE_MOE_LARGE = 0.4f;
    static constexpr float CACHE_VISION_PARALLEL = 0.2f;
};

// ============================================================================
// Execution Node (per-layer operation)
// ============================================================================
struct ExecNode {
    enum Type {
        ATTENTION_QKV,      // QKV projection
        ATTENTION_SCORE,    // Q@K^T
        ATTENTION_SOFTMAX,  // Softmax + dropout
        ATTENTION_OUT,      // Attention @ V + projection
        FFN_GATE,           // SwiGLU gate
        FFN_UP,             // FFN up projection
        FFN_DOWN,           // FFN down projection
        MOE_ROUTER,         // Expert routing
        MOE_EXPERT,         // Expert compute
        NORM_PRE,           // Pre-attention norm
        NORM_POST,          // Post-attention norm
        RESIDUAL_ADD,       // Residual connection
        KV_CACHE_UPDATE,    // KV cache write
        VISION_ENCODE,      // Vision encoder
        TOKEN_EMBED,        // Token embedding
        OUTPUT_PROJ         // Output projection
    };
    
    Type type;
    uint32_t layer_idx;
    float flop_cost;        // Estimated FLOPs
    float mem_read;         // Bytes read from memory
    float mem_write;        // Bytes written to memory
    float latency_us;       // Estimated latency
    float cache_pressure;   // Cache locality impact (0-1)
    uint64_t deps;          // Bitmask of node dependencies
    bool is_parallel;       // Can run in parallel with next node
};

// ============================================================================
// Execution DAG Builder
// ============================================================================
struct ExecutionDAG {
    std::vector<ExecNode> nodes;
    uint32_t num_layers;
    uint64_t caps;
    
    // Build DAG from capability mask
    static ExecutionDAG from_caps(uint64_t caps, uint32_t n_layers, 
                                   uint32_t n_heads, uint32_t n_kv_heads,
                                   uint32_t head_dim, uint32_t ffn_dim) {
        ExecutionDAG dag;
        dag.caps = caps;
        dag.num_layers = n_layers;
        
        for (uint32_t layer = 0; layer < n_layers; ++layer) {
            dag.build_layer(layer, n_heads, n_kv_heads, head_dim, ffn_dim);
        }
        
        return dag;
    }
    
private:
    void build_layer(uint32_t layer, uint32_t n_heads, uint32_t n_kv_heads,
                     uint32_t head_dim, uint32_t ffn_dim) {
        // Pre-attention norm
        add_node(ExecNode::NORM_PRE, layer, 1.0f, 0.0f);
        
        // Attention QKV projection
        float qkv_flop = 3.0f * n_heads * head_dim;  // Q, K, V
        if (caps & CAP_GQA) {
            qkv_flop *= CostWeights::FLOP_ATTN_GQA_REDUCTION;
        } else if (caps & CAP_MLA) {
            qkv_flop *= CostWeights::FLOP_ATTN_MLA_REDUCTION;
        }
        add_node(ExecNode::ATTENTION_QKV, layer, qkv_flop, 0.1f);
        
        // QK norm (if present)
        if (caps & CAP_QK_NORM) {
            add_node(ExecNode::NORM_PRE, layer, CostWeights::FLOP_QK_NORM, 0.0f);
        }
        
        // Attention score computation
        float score_flop = CostWeights::FLOP_ATTN_PER_HEAD * n_heads;
        if (caps & CAP_SWA) {
            score_flop *= 0.5f;  // Sliding window reduces compute
        }
        add_node(ExecNode::ATTENTION_SCORE, layer, score_flop, 0.0f);
        
        // Softmax
        add_node(ExecNode::ATTENTION_SOFTMAX, layer, 1.0f, 0.0f);
        
        // Output projection
        add_node(ExecNode::ATTENTION_OUT, layer, n_heads * head_dim, 0.1f);
        
        // Residual
        add_node(ExecNode::RESIDUAL_ADD, layer, 0.0f, 0.0f);
        
        // KV cache update
        float kv_mem = CostWeights::MEM_KV_CACHE * n_kv_heads * head_dim;
        if (caps & CAP_KV_COMPRESSED) {
            kv_mem *= CostWeights::MEM_KV_COMPRESSED;
        }
        if (caps & CAP_KV_PAGED) {
            kv_mem *= CostWeights::MEM_KV_PAGED_OVERHEAD;
        }
        add_node(ExecNode::KV_CACHE_UPDATE, layer, 0.0f, kv_mem);
        
        // Post-attention norm
        add_node(ExecNode::NORM_POST, layer, 1.0f, 0.0f);
        
        // FFN or MoE
        if (caps & CAP_MOE) {
            // Router
            add_node(ExecNode::MOE_ROUTER, layer, CostWeights::FLOP_MOE_ROUTING, 
                     CostWeights::MEM_MOE_EXPERT_SWITCH);
            
            // Expert compute (top-k)
            int top_k = (caps & CAP_MOE_TOPK) ? 2 : 1;
            for (int k = 0; k < top_k; ++k) {
                add_node(ExecNode::MOE_EXPERT, layer, 
                         CostWeights::FLOP_MOE_EXPERT * ffn_dim, 0.2f);
            }
        } else {
            // Standard FFN
            if (caps & CAP_SWIGLU) {
                add_node(ExecNode::FFN_GATE, layer, 
                         CostWeights::FLOP_SWIGLU * ffn_dim, 0.1f);
            }
            add_node(ExecNode::FFN_UP, layer, ffn_dim, 0.1f);
            add_node(ExecNode::FFN_DOWN, layer, ffn_dim, 0.1f);
        }
        
        // Final residual
        add_node(ExecNode::RESIDUAL_ADD, layer, 0.0f, 0.0f);
    }
    
    void add_node(ExecNode::Type type, uint32_t layer, float flop, float mem) {
        ExecNode node;
        node.type = type;
        node.layer_idx = layer;
        node.flop_cost = flop;
        node.mem_read = mem;
        node.mem_write = mem * 0.5f;
        node.latency_us = estimate_latency(type, flop, mem);
        node.cache_pressure = estimate_cache_pressure(type);
        node.deps = 0;
        node.is_parallel = (type == ExecNode::FFN_GATE || type == ExecNode::FFN_UP);
        nodes.push_back(node);
    }
    
    float estimate_latency(ExecNode::Type type, float flop, float mem) {
        float base = flop * 0.01f + mem * 0.001f;
        switch (type) {
            case ExecNode::MOE_ROUTER: 
                return base + CostWeights::LATENCY_MOE_ROUTING;
            case ExecNode::ATTENTION_SCORE:
                if (caps & CAP_KV_PAGED) {
                    return base + CostWeights::LATENCY_PAGED_ATTN;
                }
                return base;
            case ExecNode::NORM_PRE:
                if (caps & CAP_ADAPTIVE_DEPTH) {
                    return base + CostWeights::LATENCY_DYNAMIC_DEPTH;
                }
                return base;
            default:
                return base;
        }
    }
    
    float estimate_cache_pressure(ExecNode::Type type) {
        switch (type) {
            case ExecNode::MOE_EXPERT:
                return CostWeights::CACHE_MOE_LARGE;
            case ExecNode::ATTENTION_SCORE:
                if (caps & CAP_KV_SPARSE) {
                    return CostWeights::CACHE_KV_SPARSE;
                }
                return 0.0f;
            case ExecNode::VISION_ENCODE:
                return CostWeights::CACHE_VISION_PARALLEL;
            default:
                return 0.0f;
        }
    }
};

// ============================================================================
// Scheduler - Optimizes node execution order
// ============================================================================
struct DAGScheduler {
    ExecutionDAG dag;
    
    // Critical path analysis
    float compute_critical_path() {
        float total = 0.0f;
        for (const auto& node : dag.nodes) {
            total += node.latency_us;
        }
        return total;
    }
    
    // Memory bandwidth estimate
    float estimate_memory_bandwidth_gb_s() {
        float total_bytes = 0.0f;
        float total_time = 0.0f;
        for (const auto& node : dag.nodes) {
            total_bytes += node.mem_read + node.mem_write;
            total_time += node.latency_us;
        }
        return (total_bytes / 1e9f) / (total_time / 1e6f);
    }
    
    // Fusion opportunities
    std::vector<std::pair<size_t, size_t>> find_fusion_opportunities() {
        std::vector<std::pair<size_t, size_t>> fusions;
        for (size_t i = 0; i < dag.nodes.size() - 1; ++i) {
            const auto& curr = dag.nodes[i];
            const auto& next = dag.nodes[i + 1];
            
            // Fuse norm + attention
            if (curr.type == ExecNode::NORM_PRE && 
                next.type == ExecNode::ATTENTION_QKV) {
                fusions.push_back({i, i + 1});
            }
            // Fuse gate + up in SwiGLU
            if (curr.type == ExecNode::FFN_GATE && 
                next.type == ExecNode::FFN_UP) {
                fusions.push_back({i, i + 1});
            }
        }
        return fusions;
    }
};

} // namespace rawr
