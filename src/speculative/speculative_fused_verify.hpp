// ============================================================================
// speculative_fused_verify.hpp — P1: Fused Speculative Verify Kernel
// ============================================================================
// Fused verify-and-accept kernel that operates on draft logits in registers
// without round-tripping to VRAM. Eliminates bandwidth bottleneck in
// speculative decoding verification.
//
// Expected gain: +40-50% spec speedup (from <2x to ~3x effective)
// LOC: ~260 lines
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Speculative {

// Draft token candidate
struct DraftCandidate {
    int32_t token_id;
    float draft_prob;      // Probability from draft model
    float target_logit;    // Logit from target model (computed during verify)
    bool accepted;
};

// Verification result
struct VerifyResult {
    uint32_t num_accepted;
    uint32_t num_rejected;
    int32_t accepted_tokens[8];  // Fixed max draft length
    float acceptance_rate;
};

// Fused kernel configuration
struct FusedVerifyConfig {
    uint32_t max_draft_len = 8;
    float temperature = 1.0f;
    bool use_rejection_sampling = true;
    bool compute_all_logits = false;  // If true, compute all draft positions
};

// KV cache view for verification (avoids full copy)
struct KVCachedView {
    const float* k_cache;  // Pointer to K cache (may be FP8 quantized)
    const float* v_cache;  // Pointer to V cache (may be FP8 quantized)
    uint32_t seq_len;
    uint32_t num_heads;
    uint32_t head_dim;
    bool is_quantized;
    float quant_scale;
};

// ============================================================================
// Fused Speculative Verify Kernel
// ============================================================================
class FusedSpeculativeVerifier {
public:
    FusedSpeculativeVerifier();
    ~FusedSpeculativeVerifier();

    // No copy/move
    FusedSpeculativeVerifier(const FusedSpeculativeVerifier&) = delete;
    FusedSpeculativeVerifier& operator=(const FusedSpeculativeVerifier&) = delete;

    // Initialize with model dimensions
    bool initialize(uint32_t vocab_size, uint32_t num_heads,
                    uint32_t head_dim, uint32_t max_seq_len);

    // Shutdown
    void shutdown();

    // Fused verify-and-accept
    // draft_logits: [draft_len, vocab_size] from draft model
    // target_logits: [draft_len, vocab_size] computed by target for draft positions
    // Returns number of tokens accepted
    VerifyResult verifyAndAccept(const float* draft_logits,
                                  const float* target_logits,
                                  const int32_t* draft_tokens,
                                  uint32_t draft_len,
                                  const FusedVerifyConfig& config);

    // Fused attention forward for verification
    // Computes attention for draft positions only, reusing cached KV
    void fusedAttentionVerify(const float* query,
                               const KVCachedView& kv_cache,
                               float* output_logits,
                               uint32_t draft_start_pos,
                               uint32_t draft_len);

    // Accept/reject with temperature scaling (in registers)
    bool acceptTokenInReg(float draft_logit, float target_logit,
                          float temperature);

    // Batch verify multiple candidates (SIMD-friendly)
    uint32_t batchVerify(const DraftCandidate* candidates,
                         uint32_t num_candidates,
                         VerifyResult* result);

    // Get statistics
    uint64_t getTotalVerifications() const { return total_verifications_.load(); }
    uint64_t getTotalAccepted() const { return total_accepted_.load(); }
    double getAcceptanceRate() const;
    double getAverageVerifyTimeUs() const;

private:
    // Core verification logic (register-only)
    bool verifySingleToken(float draft_prob, float target_prob,
                            float temperature);

    // Temperature-scaled softmax in registers
    void softmaxInReg(const float* logits, float* probs,
                      uint32_t vocab_size, float temperature);

    // Rejection sampling
    bool rejectionSample(float draft_prob, float target_prob);

    // Memory management
    bool allocateWorkspace();
    void freeWorkspace();

    // Configuration
    uint32_t vocab_size_ = 0;
    uint32_t num_heads_ = 0;
    uint32_t head_dim_ = 0;
    uint32_t max_seq_len_ = 0;

    // Workspace
    float* workspace_logits_ = nullptr;
    float* workspace_probs_ = nullptr;
    DraftCandidate* workspace_candidates_ = nullptr;
    size_t workspace_size_ = 0;

    // Statistics
    std::atomic<uint64_t> total_verifications_{0};
    std::atomic<uint64_t> total_accepted_{0};
    std::atomic<uint64_t> total_verify_time_ns_{0};
    std::atomic<uint64_t> total_credits_released_{0};  // Track credits returned to coordinator

    // Credit release callback (called when tokens are accepted)
    std::function<void(uint32_t)> credit_release_callback_;

    // State
    bool initialized_ = false;
};

// ============================================================================
// C API for integration
// ============================================================================
extern "C" {
    typedef void* RawrXD_SpecVerifier;

    RawrXD_SpecVerifier rawrxd_specverifier_create(
        uint32_t vocab_size, uint32_t num_heads,
        uint32_t head_dim, uint32_t max_seq_len);

    void rawrxd_specverifier_destroy(RawrXD_SpecVerifier handle);

    int rawrxd_specverifier_verify(
        RawrXD_SpecVerifier handle,
        const float* draft_logits,
        const float* target_logits,
        const int32_t* draft_tokens,
        uint32_t draft_len,
        int32_t* accepted_tokens,
        uint32_t* num_accepted);

    double rawrxd_specverifier_get_acceptance_rate(RawrXD_SpecVerifier handle);
}

} // namespace Speculative
} // namespace RawrXD
