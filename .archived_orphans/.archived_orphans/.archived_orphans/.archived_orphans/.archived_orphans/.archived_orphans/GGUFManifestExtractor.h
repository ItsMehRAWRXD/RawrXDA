#pragma once
// GGUFManifestExtractor.h — Extracts LayerManifest from loaded GGUF model
// Reverse-engineers the model's topology for VRAM zone planning
// No Qt. No exceptions. C++20 only.

#include "VRAMHotpatchScaler.h"
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

// ═══════════════════════════════════════════════════════════════════════════════
// ExtractManifestFromGGUF — Read GGUF metadata to build zone plan
//
// Key insight: We don't need the full GGUF parser — we just need:
//   - Number of layers (n_layers tensor key)
//   - Per-layer weight size (sum of tensor sizes per layer group)
//   - Embedding size (token_embd tensor)
//   - Output size (output tensor)
//   - KV cache dimensions (n_head_kv × head_dim × 2 × sizeof(f16))
// ═══════════════════════════════════════════════════════════════════════════════
inline LayerManifest ExtractManifestFromGGUF(
    int totalLayers,
    int hiddenDim,
    int numHeads,
    int numKVHeads,
    int vocabSize,
    int maxContext,
    size_t totalModelBytes,
    int quantBits = 4)   // Q4 = 4, Q8 = 8, F16 = 16
{
    LayerManifest m;
    m.totalLayers = totalLayers;
    m.maxContext = maxContext;
    m.totalModelBytes = totalModelBytes;

    // Head dimension
    int headDim = hiddenDim / numHeads;

    // Embedding table: vocab × dim × bytesPerParam
    float bytesPerParam = (float)quantBits / 8.0f;
    m.embeddingBytes = (size_t)((double)vocabSize * hiddenDim * bytesPerParam);

    // Output projection (LM head): dim × vocab × bytesPerParam
    m.outputBytes = m.embeddingBytes; // Usually tied weights or same size

    // KV cache per token: 2 (K+V) × numKVHeads × headDim × sizeof(float16)
    m.kvCacheBytesPerToken = (size_t)(2 * numKVHeads * headDim * sizeof(uint16_t)) * totalLayers;

    // Per-layer weight size: total - embedding - output, divided by layers
    size_t layerWeightTotal = totalModelBytes - m.embeddingBytes - m.outputBytes;
    m.bytesPerLayer = layerWeightTotal / (totalLayers > 0 ? totalLayers : 1);

    return m;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BuildLayerOffsets — Compute byte offsets for each layer in the weight blob
// Assumes contiguous layout: [embedding][layer0][layer1]...[layerN][output]
// ═══════════════════════════════════════════════════════════════════════════════
inline std::vector<size_t> BuildLayerOffsets(const LayerManifest& m)
{
    std::vector<size_t> offsets(m.totalLayers);
    size_t cursor = m.embeddingBytes; // Skip embedding table

    for (int i = 0; i < m.totalLayers; i++) {
        offsets[i] = cursor;
        cursor += m.bytesPerLayer;
    }

    return offsets;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Common model profiles — pre-computed for quick setup
// ═══════════════════════════════════════════════════════════════════════════════
namespace ModelProfiles {

    // BigDaddyG-God 70B Q4_K_M (~38GB)
    inline LayerManifest BigDaddyG_God_70B() {
        return ExtractManifestFromGGUF(
            /*totalLayers=*/   80,
            /*hiddenDim=*/     8192,
            /*numHeads=*/      64,
            /*numKVHeads=*/    8,
            /*vocabSize=*/     128256,
            /*maxContext=*/    4096,
            /*totalBytes=*/    38ULL * 1024 * 1024 * 1024,
            /*quantBits=*/    4
        );
    }

    // BigDaddyG-16GB (optimized for 7800 XT)
    inline LayerManifest BigDaddyG_16GB() {
        return ExtractManifestFromGGUF(
            /*totalLayers=*/   32,
            /*hiddenDim=*/     5120,
            /*numHeads=*/      40,
            /*numKVHeads=*/    8,
            /*vocabSize=*/     128256,
            /*maxContext=*/    4096,
            /*totalBytes=*/    16ULL * 1024 * 1024 * 1024,
            /*quantBits=*/    4
        );
    }

    // Llama-3 70B Q4
    inline LayerManifest Llama3_70B_Q4() {
        return ExtractManifestFromGGUF(
            /*totalLayers=*/   80,
            /*hiddenDim=*/     8192,
            /*numHeads=*/      64,
            /*numKVHeads=*/    8,
            /*vocabSize=*/     128256,
            /*maxContext=*/    8192,
            /*totalBytes=*/    40ULL * 1024 * 1024 * 1024,
            /*quantBits=*/    4
        );
    }

    // Qwen3-Coder 30B Q4
    inline LayerManifest Qwen3_Coder_30B() {
        return ExtractManifestFromGGUF(
            /*totalLayers=*/   48,
            /*hiddenDim=*/     6144,
            /*numHeads=*/      48,
            /*numKVHeads=*/    8,
            /*vocabSize=*/     152064,
            /*maxContext=*/    4096,
            /*totalBytes=*/    18ULL * 1024 * 1024 * 1024,
            /*quantBits=*/    4
        );
    }

    // Auto-detect from model size and estimate topology
    inline LayerManifest AutoDetect(size_t modelBytes, int maxContext = 4096) {
        // Rough heuristics from common model architectures
        if (modelBytes >= 35ULL * 1024 * 1024 * 1024) {
            // 70B class
            return ExtractManifestFromGGUF(80, 8192, 64, 8, 128256, maxContext, modelBytes, 4);
        } else if (modelBytes >= 14ULL * 1024 * 1024 * 1024) {
            // 30B class
            return ExtractManifestFromGGUF(48, 6144, 48, 8, 128256, maxContext, modelBytes, 4);
        } else if (modelBytes >= 8ULL * 1024 * 1024 * 1024) {
            // 13B class
            return ExtractManifestFromGGUF(40, 5120, 40, 8, 128256, maxContext, modelBytes, 4);
        } else {
            // 7B class
            return ExtractManifestFromGGUF(32, 4096, 32, 8, 128256, maxContext, modelBytes, 4);
        }
    }
}

} // namespace RawrXD
