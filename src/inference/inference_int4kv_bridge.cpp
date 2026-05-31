// ============================================================================
// inference_int4kv_bridge.cpp
// Phase 2 Integration: Flash Attention v2 + INT4 KV Unpacking + Expert Gating
// ============================================================================

#include "inference/inference_int4kv_bridge.h"
#include <atomic>
#include <cstdio>
#include <algorithm>

namespace RawrXD {
namespace Inference {

// Telemetry counters
struct TelemetryCounters {
    std::atomic<uint64_t> kv_decompressed_tokens{0};
    std::atomic<uint64_t> expert_commits_total{0};
    std::atomic<uint64_t> expert_decommits_total{0};
    std::atomic<uint64_t> resident_set_peak_bytes{0};
    std::atomic<uint64_t> resident_set_current_bytes{0};
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
};

static thread_local TelemetryCounters g_telemetry;

// Constructor
Int4KVFlashAttentionBridge::Int4KVFlashAttentionBridge(
    InferenceKernels* parent_kernels,
    void* res_manager)
    : parent_kernels_(parent_kernels),
      res_manager_(res_manager) {
}

// Process one inference token through the INT4 KV + expert-gated pipeline
void Int4KVFlashAttentionBridge::ProcessToken(
    const float* query_heads,
    const uint8_t* kv_int4_cache,
    float* output_logits,
    const ExpertGatingInput& gating,
    const Int4KVConfig& config) {

    if (!query_heads || !kv_int4_cache || !output_logits || config.seq_len <= 0) {
        return;
    }

    // Phase 2A: Expert Gating
    if (gating.num_active_experts > 0) {
        CommitActiveExperts(gating);
        g_telemetry.expert_commits_total.fetch_add(gating.num_active_experts, std::memory_order_relaxed);
    }

    // Phase 2B: Lazy INT4 KV Decompression window tracking
    const size_t window_start = static_cast<size_t>(std::max(0, config.seq_len - config.context_window));
    const size_t window_size = static_cast<size_t>(std::min(config.context_window, config.seq_len));

    if (cached_kv_start_ == window_start && cached_kv_end_ == window_start + window_size) {
        g_telemetry.cache_hits.fetch_add(1, std::memory_order_relaxed);
    } else {
        cached_kv_start_ = window_start;
        cached_kv_end_ = window_start + window_size;
        g_telemetry.cache_misses.fetch_add(1, std::memory_order_relaxed);
    }

    // Track physical memory usage
    size_t decompressed_bytes = window_size * config.head_dim * sizeof(float) * 2;
    uint64_t peak = g_telemetry.resident_set_peak_bytes.load(std::memory_order_relaxed);
    if (decompressed_bytes > peak) {
        g_telemetry.resident_set_peak_bytes.compare_exchange_strong(
            peak, static_cast<uint64_t>(decompressed_bytes), std::memory_order_release);
    }
    g_telemetry.resident_set_current_bytes.store(
        static_cast<uint64_t>(decompressed_bytes), std::memory_order_release);
    g_telemetry.kv_decompressed_tokens.fetch_add(window_size, std::memory_order_relaxed);

    // Phase 2C: Call flash_attention_v2 (placeholder)
    // In production: InferenceKernels::flash_attention_v2(...)

    // Phase 2D: Soft-eviction (decommit inactive experts)
    if (gating.num_inactive_experts > 0) {
        DecommitInactiveExperts(gating);
        g_telemetry.expert_decommits_total.fetch_add(gating.num_inactive_experts, std::memory_order_relaxed);
    }

    if (config.invalidate_cache_on_token_done) {
        cached_kv_start_ = 0;
        cached_kv_end_ = 0;
    }
}

void Int4KVFlashAttentionBridge::CommitActiveExperts(const ExpertGatingInput& gating) {
    // MASM stub: SovExp_CommitSelectedExperts()
}

void Int4KVFlashAttentionBridge::DecommitInactiveExperts(const ExpertGatingInput& gating) {
    // MASM stub: SovExp_DecommitUnselectedExperts()
}

void Int4KVFlashAttentionBridge::ResetTelemetry() {
    g_telemetry.kv_decompressed_tokens.store(0, std::memory_order_release);
    g_telemetry.expert_commits_total.store(0, std::memory_order_release);
    g_telemetry.expert_decommits_total.store(0, std::memory_order_release);
    g_telemetry.resident_set_peak_bytes.store(0, std::memory_order_release);
    g_telemetry.resident_set_current_bytes.store(0, std::memory_order_release);
    g_telemetry.cache_hits.store(0, std::memory_order_release);
    g_telemetry.cache_misses.store(0, std::memory_order_release);
}

void Int4KVFlashAttentionBridge::PrintTelemetry() {
    uint64_t decompressed = g_telemetry.kv_decompressed_tokens.load();
    uint64_t commits = g_telemetry.expert_commits_total.load();
    uint64_t decommits = g_telemetry.expert_decommits_total.load();
    uint64_t peak = g_telemetry.resident_set_peak_bytes.load();
    uint64_t current = g_telemetry.resident_set_current_bytes.load();
    uint64_t hits = g_telemetry.cache_hits.load();
    uint64_t misses = g_telemetry.cache_misses.load();

    fprintf(stderr, "[Phase2Telemetry]\n");
    fprintf(stderr, "  KV Tokens Decompressed: %llu\n", decompressed);
    fprintf(stderr, "  Expert Commits: %llu | Decommits: %llu\n", commits, decommits);
    fprintf(stderr, "  Resident Set Peak: %.2f GB | Current: %.2f GB\n",
            peak / (1024.0 * 1024.0 * 1024.0),
            current / (1024.0 * 1024.0 * 1024.0));
    fprintf(stderr, "  KV Cache Hits: %llu | Misses: %llu\n", hits, misses);
    if (hits + misses > 0) {
        fprintf(stderr, "  Hit Rate: %.1f%%\n", 100.0 * hits / (hits + misses));
    }
}

}  // namespace Inference
}  // namespace RawrXD
