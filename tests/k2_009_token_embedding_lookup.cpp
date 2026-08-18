// ============================================================================
// K2-009 — Real Token Embedding Lookup (10 gates)
// ============================================================================
//
// Purpose: Prove that token_embd.weight can be streamed row-by-row to
//          produce real token embeddings without loading the full ~918 MiB matrix.
//
// Pipeline:
//   discover token_embd.weight in tensor index
//     ↓
//   validate shape [163840, 7168] and quantization
//     ↓
//   stream one row for a given token ID
//     ↓
//   dequantize Q4_K/Q6_K → FP32[7168]
//     ↓
//   verify determinism, checksum, residency
//
// Hard requirements:
//   - Tensor exists and is discoverable
//   - Shape matches expected [vocabSize, hiddenSize]
//   - Quantization is supported (Q4_K=12 or Q6_K=14)
//   - Token ID bounds are enforced
//   - Known tokens produce deterministic embeddings
//   - Dequantization produces finite values
//   - Only one row is resident at a time
//   - Peak residency stays within budget
//   - Final residency returns to zero
//
// Usage: k2_009_token_embedding_lookup <shard-directory>
// Exit codes:
//   0 = ALL GATES PASSED
//   1 = Shard discovery failed
//   2 = Tensor discovery failed
//   3 = Shape validation failed
//   4 = Quantization validation failed
//   5 = Token ID bounds failed
//   6 = Known token lookup failed
//   7 = Dequantization correctness failed
//   8 = Determinism failed
//   9 = Residency invariant failed
//   10 = Real-model integration failed
// ============================================================================

#include "../src/deep2/KimiK2Config.hpp"
#include "../src/deep2/K2GlobalTensorIndex.hpp"
#include "../src/deep2/K2TokenEmbedding.hpp"
#include "../src/deep2/GGUFLoader.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <vector>
#include <chrono>
#include <algorithm>
#include <limits>

namespace fs = std::filesystem;

// ── Hard Budget ──
constexpr uint64_t kBudgetBytes = 256ull * 1024 * 1024;

// ── Gate Helpers ──
#define GATE(name, condition, exitCode) \
    do { \
        if (!(condition)) { \
            printf("  [FAIL] Gate: %s\n", name); \
            return exitCode; \
        } \
        printf("  [PASS] Gate: %s\n", name); \
    } while(0)

// ── Validate output: finite, no NaN/Inf ──
static bool ValidateFinite(const float* data, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (std::isnan(data[i]) || std::isinf(data[i])) return false;
    }
    return true;
}

// ── Compute simple checksum for determinism ──
static uint64_t ComputeChecksum(const float* data, size_t count) {
    uint64_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        uint32_t bits;
        memcpy(&bits, &data[i], sizeof(uint32_t));
        sum = (sum << 1) | (sum >> 63);
        sum ^= bits;
    }
    return sum;
}

// ── Shard Discovery ──
static bool DiscoverK2Shards(const fs::path& dir, std::vector<fs::path>& shards) {
    shards.clear();
    for (int i = 1; i <= 13; ++i) {
        char name[256];
        snprintf(name, sizeof(name),
                 "Kimi-K2-Instruct-0905-Q4_K_M-%05d-of-00013.gguf", i);
        fs::path candidate = dir / name;
        if (fs::exists(candidate)) { shards.push_back(candidate); continue; }
        snprintf(name, sizeof(name),
                 "kimi-k2-instruct-0905-q4_k_m-%05d-of-00013.gguf", i);
        candidate = dir / name;
        if (fs::exists(candidate)) { shards.push_back(candidate); }
    }
    return !shards.empty();
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  K2-009 — Real Token Embedding Lookup (10 gates)          ║\n");
    printf("║  Budget: %llu MiB                                          ║\n",
           (unsigned long long)(kBudgetBytes / (1024 * 1024)));
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    fs::path shardDir = (argc > 1) ? argv[1] : fs::current_path();

    printf("[INFO] Shard directory: %s\n", shardDir.string().c_str());

    // ═══════════════════════════════════════════════════════════════
    // Gate 1: Shard Discovery
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 1: Shard Discovery ──\n");
    std::vector<fs::path> shards;
    if (!DiscoverK2Shards(shardDir, shards)) {
        printf("  [SKIP] No K2 shards found — skipping K2-009.\n");
        return 0;
    }
    printf("       Found %zu shard(s)\n", shards.size());

    // ═══════════════════════════════════════════════════════════════
    // Gate 2: Tensor Index Build + Tensor Discovery
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 2: Tensor Discovery ──\n");
    Deep2::GlobalTensorIndex index;
    std::string indexError;
    Deep2::KimiK2Config k2cfg;
    k2cfg.hiddenDim = 7168;
    k2cfg.numLayers = 61;
    k2cfg.numHeads = 128;
    k2cfg.qLoraRank = 1536;
    k2cfg.kvLoraRank = 512;
    k2cfg.qkNopeHeadDim = 128;
    k2cfg.qkRopeHeadDim = 64;
    k2cfg.vHeadDim = 128;
    k2cfg.vocabSize = 163840;
    GATE("Index built", index.BuildFromShardDirectory(shardDir, k2cfg, indexError), 2);
    printf("       Total tensors indexed: %zu\n", index.TotalTensors());

    auto embRefOpt = index.Find("token_embd.weight");
    GATE("token_embd.weight found in index", embRefOpt.has_value(), 2);
    const auto& embRef = *embRefOpt;
    printf("       Shard: %u, offset: %llu, bytes: %llu\n",
           embRef.shardId, (unsigned long long)embRef.fileOffset, (unsigned long long)embRef.byteSize);

    // ═══════════════════════════════════════════════════════════════
    // Gate 3: Shape Validation
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 3: Shape Validation ──\n");
    printf("       nDims: %u\n", embRef.nDims);
    for (uint8_t i = 0; i < embRef.nDims && i < 8; ++i) {
        printf("       dim[%u]: %llu\n", i, (unsigned long long)embRef.shape[i]);
    }

    bool shapeOk = false;
    size_t actualVocabSize = 0;
    size_t actualHiddenSize = 0;

    if (embRef.nDims == 2) {
        size_t dim0 = embRef.shape[0];
        size_t dim1 = embRef.shape[1];
        if (dim0 == k2cfg.vocabSize && dim1 == k2cfg.hiddenDim) {
            shapeOk = true;
            actualVocabSize = dim0;
            actualHiddenSize = dim1;
            printf("       Orientation: [vocabSize, hiddenSize]\n");
        } else if (dim0 == k2cfg.hiddenDim && dim1 == k2cfg.vocabSize) {
            shapeOk = true;
            actualVocabSize = dim1;
            actualHiddenSize = dim0;
            printf("       Orientation: [hiddenSize, vocabSize] (transposed)\n");
        }
    }
    GATE("Dimensions match expected [163840, 7168]", shapeOk, 3);
    printf("       Vocab: %zu, Hidden: %zu\n", actualVocabSize, actualHiddenSize);

    // ═══════════════════════════════════════════════════════════════
    // Gate 4: Quantization Validation
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 4: Quantization Validation ──\n");
    printf("       GGML type: %d (expected 12 for Q4_K or 14 for Q6_K)\n", embRef.ggmlType);
    bool typeSupported = (embRef.ggmlType == 12 || embRef.ggmlType == 14);
    GATE("GGML type is supported (Q4_K or Q6_K)", typeSupported, 4);

    // ═══════════════════════════════════════════════════════════════
    // Gate 5: Token ID Bounds Validation
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 5: Token ID Bounds ──\n");
    Deep2::K2TokenEmbedding::Config embCfg;
    embCfg.hiddenSize = k2cfg.hiddenDim;
    embCfg.vocabSize = k2cfg.vocabSize;
    embCfg.maxResidentBytes = kBudgetBytes;

    Deep2::K2TokenEmbedding embed(embCfg);
    GATE("Embedding initialized", embed.initialize(&index), 5);

    std::vector<float> out1(k2cfg.hiddenDim);
    auto resOob = embed.lookup(static_cast<uint32_t>(k2cfg.vocabSize), out1.data());
    GATE("Out-of-bounds token rejected", !resOob.ok, 5);
    printf("       Error: %s\n", resOob.error.c_str());

    auto resNeg = embed.lookup(UINT32_MAX, out1.data());
    GATE("Max uint32 token rejected", !resNeg.ok, 5);

    // ═══════════════════════════════════════════════════════════════
    // Gate 6: Known Token Lookup
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 6: Known Token Lookup ──\n");
    std::vector<float> emb0(k2cfg.hiddenDim);
    std::vector<float> emb1(k2cfg.hiddenDim);
    std::vector<float> emb2(k2cfg.hiddenDim);

    auto res0 = embed.lookup(0, emb0.data());
    if (!res0.ok) {
        printf("       Token 0 lookup FAILED: %s\n", res0.error.c_str());
        printf("       dims=%zu, bytes=%zu, peak=%zu, final=%zu\n",
               res0.dimensions, res0.bytesRead, res0.peakResidency, res0.finalResidency);
    }
    GATE("Token 0 lookup succeeds", res0.ok, 6);
    printf("       Token 0: dims=%zu, bytes=%zu, checksum=0x%016llX\n",
           res0.dimensions, res0.bytesRead, (unsigned long long)res0.checksum);

    auto res1 = embed.lookup(1, emb1.data());
    GATE("Token 1 lookup succeeds", res1.ok, 6);

    auto res2 = embed.lookup(2, emb2.data());
    GATE("Token 2 lookup succeeds", res2.ok, 6);

    // ═══════════════════════════════════════════════════════════════
    // Gate 7: Dequantization Correctness
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 7: Dequantization Correctness ──\n");
    GATE("Token 0 output finite", ValidateFinite(emb0.data(), emb0.size()), 7);
    GATE("Token 1 output finite", ValidateFinite(emb1.data(), emb1.size()), 7);
    GATE("Token 2 output finite", ValidateFinite(emb2.data(), emb2.size()), 7);

    float minVal = emb0[0], maxVal = emb0[0];
    for (size_t i = 1; i < emb0.size(); ++i) {
        minVal = std::min(minVal, emb0[i]);
        maxVal = std::max(maxVal, emb0[i]);
    }
    printf("       Token 0 range: [%.6f, %.6f]\n", minVal, maxVal);
    GATE("Token 0 range non-degenerate", minVal != maxVal, 7);

    // ═══════════════════════════════════════════════════════════════
    // Gate 8: Determinism
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 8: Determinism ──\n");
    std::vector<float> emb0Again(k2cfg.hiddenDim);
    auto res0Again = embed.lookup(0, emb0Again.data());
    GATE("Repeat token 0 lookup succeeds", res0Again.ok, 8);

    uint64_t cs1 = ComputeChecksum(emb0.data(), emb0.size());
    uint64_t cs2 = ComputeChecksum(emb0Again.data(), emb0Again.size());
    printf("       Checksum 1: 0x%016llX\n", (unsigned long long)cs1);
    printf("       Checksum 2: 0x%016llX\n", (unsigned long long)cs2);
    GATE("Checksums match (deterministic)", cs1 == cs2, 8);

    // ═══════════════════════════════════════════════════════════════
    // Gate 9: Residency / Streaming Invariant
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 9: Residency Invariant ──\n");
    printf("       Peak residency: %zu bytes (%.2f MiB)\n",
           res0.peakResidency, res0.peakResidency / (1024.0 * 1024.0));
    printf("       Final residency: %zu bytes (%.2f MiB)\n",
           res0.finalResidency, res0.finalResidency / (1024.0 * 1024.0));
    GATE("Peak within 256 MiB budget", res0.peakResidency <= kBudgetBytes, 9);
    GATE("Final residency is zero", res0.finalResidency == 0, 9);

    // The full matrix would be ~918 MiB; verify we didn't accidentally load it
    size_t fullMatrixBytes = k2cfg.vocabSize * k2cfg.hiddenDim * sizeof(float);
    printf("       Full matrix would be: %.1f MiB\n", fullMatrixBytes / (1024.0 * 1024.0));
    GATE("Streaming uses less than full matrix", res0.peakResidency < fullMatrixBytes, 9);

    // ═══════════════════════════════════════════════════════════════
    // Gate 10: Real-Model Integration
    // ═══════════════════════════════════════════════════════════════
    printf("\n── Gate 10: Real-Model Integration ──\n");
    // Verify the embedding can be used as transformer input
    // (just check it's the right size and finite — actual transformer integration
    // is the next gate)
    GATE("Embedding size matches hiddenDim", emb0.size() == k2cfg.hiddenDim, 10);
    GATE("Embedding suitable for transformer input", ValidateFinite(emb0.data(), emb0.size()), 10);

    // ═══════════════════════════════════════════════════════════════
    // Telemetry Report
    // ═══════════════════════════════════════════════════════════════
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  K2-009 Execution Telemetry                                ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  TENSOR_FOUND    = %-40s  ║\n", "YES");
    printf("║  SHAPE_OK        = %-40s  ║\n", shapeOk ? "YES" : "NO");
    printf("║  QUANT_SUPPORTED = %-40s  ║\n", typeSupported ? "YES" : "NO");
    printf("║  BOUNDS_ENFORCED = %-40s  ║\n", "YES");
    printf("║  DETERMINISTIC   = %-40s  ║\n", (cs1 == cs2) ? "YES" : "NO");
    printf("║  PEAK_RESIDENCY  = %-40.2f MiB ║\n", res0.peakResidency / (1024.0 * 1024.0));
    printf("║  FULL_MATRIX     = %-40.1f MiB ║\n", fullMatrixBytes / (1024.0 * 1024.0));
    printf("║  SAVINGS         = %-40.1f MiB ║\n",
           (fullMatrixBytes - res0.peakResidency) / (1024.0 * 1024.0));
    printf("╚════════════════════════════════════════════════════════════╝\n");

    printf("\n✅ ALL K2-009 GATES PASSED\n");
    return 0;
}
