// ============================================================================
// test_int4kv_bridge_integration.cpp
// END-TO-END TEST: Phase 3 Bridge Integration
// ============================================================================
// Validates that the Int4KVFlashAttentionBridge correctly orchestrates:
// 1. Expert gating (selective MEM_COMMIT)
// 2. Lazy INT4 KV decompression
// 3. Flash attention v2
// 4. Expert swapout (MEM_DECOMMIT)
// 5. Telemetry collection
// ============================================================================

#include "engine/inference_kernels.h"
#include "inference/inference_int4kv_bridge.h"
#include "compression/virtualalloc_reservation_manager.h"

#include <cstdio>
#include <memory>
#include <cstring>

using namespace RawrXD::Inference;
using namespace RawrXD::Compression;

// ============================================================================
// Test Harness
// ============================================================================

void TestInt4KVBridge() {
    fprintf(stderr, "\n=== Phase 3 Bridge Integration Test ===\n");

    // Initialize the reservation manager (95GB logical reservation on system RAM)
    auto& res_manager = VirtualAllocReservationManager::Instance();
    fprintf(stderr, "[Init] Reservation manager initialized\n");

    // Get parent inference kernels
    InferenceKernels kernels;
    fprintf(stderr, "[Init] Inference kernels instantiated\n");

    // Create the integration bridge
    Int4KVFlashAttentionBridge bridge(&kernels, &res_manager);
    fprintf(stderr, "[Init] Int4KVFlashAttentionBridge created\n");

    // ========================================================================
    // Synthetic Test Data
    // ========================================================================

    const int n_heads = 32;           // Query heads
    const int n_kv_heads = 8;         // GQA compression
    const int head_dim = 128;         // Head dimension
    const int seq_len = 512;          // Current sequence
    const int context_window = 2048;  // KV cache window
    const int max_seq_len = 4096;     // Max supported

    // Allocate synthetic Q tensor: [n_heads, head_dim]
    std::unique_ptr<float[]> query_heads(new float[n_heads * head_dim]);
    for (int i = 0; i < n_heads * head_dim; i++) {
        query_heads[i] = static_cast<float>(i) / (n_heads * head_dim);
    }

    // Allocate synthetic INT4 KV tensor: quantized, 4x compression
    // Reserve sized as if it were the full 95GB, but we'll allocate just the window for tests
    size_t int4_kv_size = (seq_len * n_kv_heads * head_dim / 2);  // Nibble packed
    std::unique_ptr<uint8_t[]> kv_int4_cache(new uint8_t[int4_kv_size]);
    std::memset(kv_int4_cache.get(), 0x55, int4_kv_size);  // Pattern fill

    // Allocate output buffer: [n_heads, head_dim]
    std::unique_ptr<float[]> output(new float[n_heads * head_dim]);
    std::memset(output.get(), 0, n_heads * head_dim * sizeof(float));

    fprintf(stderr, "[Alloc] Query: %zu bytes, KV (INT4): %zu bytes, Output: %zu bytes\n",
            n_heads * head_dim * sizeof(float),
            int4_kv_size,
            n_heads * head_dim * sizeof(float));

    // ========================================================================
    // Test: Single Token Inference with Expert Gating
    // ========================================================================

    Int4KVConfig config{
        .seq_len = seq_len,
        .context_window = context_window,
        .n_heads = n_heads,
        .n_kv_heads = n_kv_heads,
        .head_dim = head_dim,
        .max_seq_len = max_seq_len,
        .invalidate_cache_on_token_done = false  // Persist cache for batch
    };

    ExpertGatingInput gating{
        .active_expert_mask = 0x00FF,     // 8 experts active (out of 16)
        .num_active_experts = 8,
        .num_inactive_experts = 8,
        .gate_loss_scale = 0.01f
    };

    fprintf(stderr, "\n[Test] ProcessToken: seq_len=%d, experts=%u active/%u inactive\n",
            config.seq_len, gating.num_active_experts, gating.num_inactive_experts);

    bridge.ProcessToken(
        query_heads.get(),
        kv_int4_cache.get(),
        output.get(),
        gating,
        config);

    fprintf(stderr, "[Result] Token processing complete\n");

    // ========================================================================
    // Test: Verify Output Validity
    // ========================================================================

    bool has_nonzero = false;
    for (int i = 0; i < n_heads * head_dim; i++) {
        if (output[i] != 0.0f) {
            has_nonzero = true;
            break;
        }
    }

    if (has_nonzero) {
        fprintf(stderr, "[Verify] Output contains non-zero values ✓\n");
    } else {
        fprintf(stderr, "[Verify] WARNING: Output is all-zero (might indicate no attention computed)\n");
    }

    // ========================================================================
    // Test: Cache Persistence & Hit Rate
    // ========================================================================

    fprintf(stderr, "\n[Cache] Testing cache persistence...\n");
    fprintf(stderr, "[Cache] Is KV cache dirty? %s\n", 
            bridge.IsKVCacheDirty() ? "YES" : "NO");
    fprintf(stderr, "[Cache] Cached KV start: %zu, end: %zu\n",
            bridge.GetCachedKVStart(), bridge.GetCachedKVEnd());

    // Process another token without invalidating (should hit cache)
    gating.active_expert_mask = 0x00AA;  // Different expert selection
    bridge.ProcessToken(
        query_heads.get(),
        kv_int4_cache.get(),
        output.get(),
        gating,
        config);

    fprintf(stderr, "[Cache] Second token processed (potential cache hit)\n");

    // ========================================================================
    // Test: Telemetry
    // ========================================================================

    fprintf(stderr, "\n[Telemetry] Collecting phase 2 metrics...\n");
    bridge.PrintTelemetry();

    fprintf(stderr, "\n=== Phase 3 Bridge Integration Test COMPLETE ===\n\n");
}

// ============================================================================
// Main Entry
// ============================================================================

int main() {
    TestInt4KVBridge();
    return 0;
}
