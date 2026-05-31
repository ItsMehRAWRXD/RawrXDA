#pragma once
#include "prometheus_config.h"

namespace Prometheus {

// =============================================================================
// 800B MoE CONFIGURATION (DeepSeek-V3 style)
// Total: ~826B parameters | Active per token: ~67B
// =============================================================================

inline ModelConfig get800BConfig() {
    ModelConfig cfg;
    cfg.vocabSize        = 256000;      // 256K multilingual vocab
    cfg.hiddenDim        = 7168;        // 7K hidden
    cfg.numLayers        = 80;          // 80 transformer layers
    cfg.numHeads         = 56;          // 56 attention heads
    cfg.numKVHeads       = 7;           // 7 KV heads (GQA 8:1)
    cfg.headDim          = 128;         // 128 per head
    cfg.intermediateDim  = 10752;      // 1.5x hidden (MoE)
    cfg.maxPosition      = 262144;      // 256K context (extended RoPE)

    // MoE: 44 experts, 2 active + 1 shared
    cfg.numExperts       = 44;
    cfg.expertsPerToken  = 2;
    cfg.sharedExperts    = 1;
    cfg.expertDropout    = 0.0f;
    cfg.loadBalanceFactor = 0.01f;

    // Quantization
    cfg.weightBits       = 4;           // Q4_K_M weights
    cfg.kvCacheBits      = 4;           // 4-bit KV cache
    cfg.useBlockwiseQuant = true;
    cfg.quantBlockSize   = 256;

    // Attention
    cfg.slidingWindow    = 4096;        // Local window
    cfg.globalStride     = 8;           // Global every 8th
    cfg.useRingAttention = true;
    cfg.useFlashAttention = true;

    // Inference
    cfg.enableSpeculativeDecoding = true;
    cfg.speculativeTokens = 8;
    cfg.draftModelDim     = 2048;
    cfg.enableContinuousBatching = true;
    cfg.maxBatchSize      = 256;

    // Multimodal
    cfg.enableVision      = true;
    cfg.visionPatchSize   = 14;
    cfg.visionDim         = 1024;
    cfg.visionLayers      = 24;
    cfg.visionEncoder     = "siglip";

    // Capabilities
    cfg.enableToolCalling      = true;
    cfg.enableCodeExecution    = true;
    cfg.enableStructuredOutput = true;
    cfg.enableArtifacts        = true;

    // Safety
    cfg.enableRefusalTokens    = true;
    cfg.enableConstitutionalAI = true;

    return cfg;
}

} // namespace Prometheus
