// ============================================================================
// inference_int4kv_bridge.h
// Phase 2 Integration Bridge Header
// ============================================================================

#ifndef RAWRXD_INFERENCE_INT4KV_BRIDGE_H_
#define RAWRXD_INFERENCE_INT4KV_BRIDGE_H_

#include "engine/inference_kernels.h"
#include <cstdint>
#include <cstdio>

namespace RawrXD {
namespace Inference {

struct Int4KVConfig {
    int seq_len;
    int context_window;
    int n_heads;
    int n_kv_heads;
    int head_dim;
    int max_seq_len;
    bool invalidate_cache_on_token_done;
};

struct ExpertGatingInput {
    uint32_t active_expert_mask;
    uint32_t num_active_experts;
    uint32_t num_inactive_experts;
    float gate_loss_scale;
};

class Int4KVFlashAttentionBridge {
 public:
    explicit Int4KVFlashAttentionBridge(
        InferenceKernels* parent_kernels = nullptr,
        void* res_manager = nullptr);

    void ProcessToken(
        const float* query_heads,
        const uint8_t* kv_int4_cache,
        float* output_logits,
        const ExpertGatingInput& gating,
        const Int4KVConfig& config);

    void PrintTelemetry();
    void ResetTelemetry();

    bool IsKVCacheDirty() const { return cached_kv_start_ != cached_kv_end_; }
    size_t GetCachedKVStart() const { return cached_kv_start_; }
    size_t GetCachedKVEnd() const { return cached_kv_end_; }

 private:
    void CommitActiveExperts(const ExpertGatingInput& gating);
    void DecommitInactiveExperts(const ExpertGatingInput& gating);

    InferenceKernels* parent_kernels_;
    void* res_manager_;
    size_t cached_kv_start_{0};
    size_t cached_kv_end_{0};
};

}  // namespace Inference
}  // namespace RawrXD

#endif
