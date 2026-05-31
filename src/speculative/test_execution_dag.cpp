// test_execution_dag.cpp - Demonstrate 100-bit capability → execution DAG conversion
#include "rawr_execution_dag.h"
#include <cstdio>
#include <cstring>

using namespace rawr;

// Simulate capability detection from GGUF
uint64_t detect_capabilities(const char* model_name) {
    uint64_t caps = 0;
    
    if (strstr(model_name, "llama") || strstr(model_name, "mistral")) {
        caps |= CAP_GQA;
        caps |= CAP_ROPE;
        caps |= CAP_SWIGLU;
        caps |= CAP_PRE_NORM;
        caps |= CAP_KV_PAGED;
    }
    
    if (strstr(model_name, "mixtral") || strstr(model_name, "moe")) {
        caps |= CAP_MOE;
        caps |= CAP_MOE_TOPK;
        caps |= CAP_MOE_SHARED_EXPERT;
    }
    
    if (strstr(model_name, "gptoss")) {
        caps |= CAP_GQA;
        caps |= CAP_MOE;
        caps |= CAP_MOE_TOPK;
        caps |= CAP_KV_COMPRESSED;
        caps |= CAP_ADAPTIVE_DEPTH;
    }
    
    if (strstr(model_name, "vision") || strstr(model_name, "pixtral")) {
        caps |= CAP_VISION;
        caps |= CAP_VLM_INTERLEAVE;
        caps |= CAP_CONV_PATCH;
    }
    
    if (strstr(model_name, "deepseek")) {
        caps |= CAP_MLA;
        caps |= CAP_QK_NORM;
        caps |= CAP_MOE;
    }
    
    if (strstr(model_name, "phi")) {
        caps |= CAP_PARALLEL_RESIDUAL;
        caps |= CAP_SWIGLU;
    }
    
    return caps;
}

void print_capability_breakdown(uint64_t caps) {
    printf("\n=== Capability Breakdown ===\n");
    
    // Attention family
    printf("Attention Family:\n");
    if (caps & CAP_GQA) printf("  [✓] GQA (Grouped Query Attention)\n");
    if (caps & CAP_MQA) printf("  [✓] MQA (Multi-Query Attention)\n");
    if (caps & CAP_MLA) printf("  [✓] MLA (Multi-head Latent Attention)\n");
    if (caps & CAP_SWA) printf("  [✓] Sliding Window Attention\n");
    if (caps & CAP_ROPE) printf("  [✓] RoPE (Rotary Position Embedding)\n");
    if (caps & CAP_ROPE_SCALED) printf("  [✓] Scaled RoPE\n");
    if (caps & CAP_QK_NORM) printf("  [✓] QK Normalization\n");
    if (caps & CAP_ATTN_SINK) printf("  [✓] Attention Sink Tokens\n");
    
    // MoE family
    printf("\nMoE Family:\n");
    if (caps & CAP_MOE) printf("  [✓] Mixture of Experts\n");
    if (caps & CAP_MOE_TOPK) printf("  [✓] Top-K Routing\n");
    if (caps & CAP_MOE_SHARED_EXPERT) printf("  [✓] Shared Expert\n");
    if (caps & CAP_MOE_DYNAMIC) printf("  [✓] Dynamic Expert Count\n");
    if (caps & CAP_MOE_HIERARCHICAL) printf("  [✓] Hierarchical MoE\n");
    
    // FFN family
    printf("\nFFN Family:\n");
    if (caps & CAP_SWIGLU) printf("  [✓] SwiGLU Activation\n");
    if (caps & CAP_FFN_LOW_RANK) printf("  [✓] Low-Rank FFN\n");
    if (caps & CAP_FFN_BOTTLENECK) printf("  [✓] Bottleneck FFN\n");
    if (caps & CAP_PARALLEL_RESIDUAL) printf("  [✓] Parallel Residual\n");
    
    // KV Cache family
    printf("\nKV Cache Family:\n");
    if (caps & CAP_KV_PAGED) printf("  [✓] Paged KV Cache\n");
    if (caps & CAP_KV_COMPRESSED) printf("  [✓] Compressed KV\n");
    if (caps & CAP_KV_LORA) printf("  [✓] LoRA-style KV\n");
    if (caps & CAP_KV_QUANTIZED) printf("  [✓] Quantized KV Cache\n");
    
    // Vision/Multimodal
    printf("\nVision/Multimodal:\n");
    if (caps & CAP_VISION) printf("  [✓] Vision Encoder\n");
    if (caps & CAP_VLM_INTERLEAVE) printf("  [✓] VLM Token Interleaving\n");
    
    // Dynamic/Adaptive
    printf("\nDynamic/Adaptive:\n");
    if (caps & CAP_ADAPTIVE_DEPTH) printf("  [✓] Adaptive Depth\n");
    if (caps & CAP_DYNAMIC_PRECISION) printf("  [✓] Dynamic Precision\n");
    if (caps & CAP_TOKEN_SPARSIFICATION) printf("  [✓] Token Sparsification\n");
}

void print_dag_stats(const ExecutionDAG& dag) {
    printf("\n=== Execution DAG Statistics ===\n");
    printf("Total nodes: %zu\n", dag.nodes.size());
    printf("Layers: %u\n", dag.num_layers);
    printf("Nodes per layer: %.1f\n", (float)dag.nodes.size() / dag.num_layers);
    
    // Count by type
    int attn_nodes = 0, ffn_nodes = 0, moe_nodes = 0, norm_nodes = 0;
    float total_flop = 0, total_mem = 0, total_latency = 0;
    
    for (const auto& node : dag.nodes) {
        switch (node.type) {
            case ExecNode::ATTENTION_QKV:
            case ExecNode::ATTENTION_SCORE:
            case ExecNode::ATTENTION_SOFTMAX:
            case ExecNode::ATTENTION_OUT:
                attn_nodes++;
                break;
            case ExecNode::FFN_GATE:
            case ExecNode::FFN_UP:
            case ExecNode::FFN_DOWN:
                ffn_nodes++;
                break;
            case ExecNode::MOE_ROUTER:
            case ExecNode::MOE_EXPERT:
                moe_nodes++;
                break;
            case ExecNode::NORM_PRE:
            case ExecNode::NORM_POST:
                norm_nodes++;
                break;
            default:
                break;
        }
        total_flop += node.flop_cost;
        total_mem += node.mem_read + node.mem_write;
        total_latency += node.latency_us;
    }
    
    printf("\nNode distribution:\n");
    printf("  Attention: %d\n", attn_nodes);
    printf("  FFN: %d\n", ffn_nodes);
    printf("  MoE: %d\n", moe_nodes);
    printf("  Norm: %d\n", norm_nodes);
    
    printf("\nEstimated costs:\n");
    printf("  Total FLOP: %.2f GFLOPs\n", total_flop / 1e9f);
    printf("  Total memory: %.2f GB\n", total_mem / 1e9f);
    printf("  Total latency: %.2f ms\n", total_latency / 1000.0f);
}

void run_scheduler_analysis(const ExecutionDAG& dag) {
    DAGScheduler scheduler;
    scheduler.dag = dag;
    
    printf("\n=== Scheduler Analysis ===\n");
    printf("Critical path: %.2f ms\n", scheduler.compute_critical_path() / 1000.0f);
    printf("Memory bandwidth: %.2f GB/s\n", scheduler.estimate_memory_bandwidth_gb_s());
    
    auto fusions = scheduler.find_fusion_opportunities();
    printf("Fusion opportunities: %zu\n", fusions.size());
    for (const auto& fusion : fusions) {
        printf("  Nodes %zu + %zu can be fused\n", fusion.first, fusion.second);
    }
}

int main(int argc, char** argv) {
    const char* model = (argc > 1) ? argv[1] : "llama";
    uint32_t n_layers = (argc > 2) ? atoi(argv[2]) : 32;
    uint32_t n_heads = (argc > 3) ? atoi(argv[3]) : 32;
    uint32_t n_kv_heads = (argc > 4) ? atoi(argv[4]) : 8;
    uint32_t head_dim = (argc > 5) ? atoi(argv[5]) : 128;
    uint32_t ffn_dim = (argc > 6) ? atoi(argv[6]) : 14336;
    
    printf("========================================\n");
    printf("Execution DAG Compiler Test\n");
    printf("Model: %s\n", model);
    printf("Config: %u layers, %u heads, %u kv_heads, %u head_dim, %u ffn_dim\n",
           n_layers, n_heads, n_kv_heads, head_dim, ffn_dim);
    printf("========================================\n");
    
    // Detect capabilities
    uint64_t caps = detect_capabilities(model);
    print_capability_breakdown(caps);
    
    // Build execution DAG
    printf("\n=== Building Execution DAG ===\n");
    auto dag = ExecutionDAG::from_caps(caps, n_layers, n_heads, n_kv_heads, 
                                        head_dim, ffn_dim);
    
    // Print DAG statistics
    print_dag_stats(dag);
    
    // Run scheduler analysis
    run_scheduler_analysis(dag);
    
    printf("\n========================================\n");
    printf("Test complete.\n");
    
    return 0;
}
