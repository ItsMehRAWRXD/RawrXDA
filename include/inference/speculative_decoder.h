#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Inference {

// Speculative decoding configuration
struct SpeculativeConfig {
    // Number of draft tokens to generate per step
    size_t num_draft_tokens = 4;
    
    // Temperature for sampling (0.0 = greedy)
    float temperature = 0.8f;
    
    // Top-p (nucleus) sampling threshold
    float top_p = 0.9f;
    
    // Minimum probability threshold for draft acceptance
    float min_accept_prob = 0.1f;
    
    // Maximum verification batch size
    size_t max_verify_batch = 8;
    
    // Enable register-only verification (no VRAM round-trip)
    bool register_only_verify = true;
    
    // Draft model size ratio (e.g., 0.125 = 1/8th size)
    float draft_model_ratio = 0.125f;
};

// Token with probability
template<typename T = uint32_t>
struct TokenProb {
    T token;
    float prob;
    float logit;
};

// Draft token sequence
struct DraftSequence {
    std::vector<TokenProb<>> tokens;
    uint64_t sequence_id;
    float cumulative_prob;
    bool is_complete;
};

// Verification result
struct VerifyResult {
    // Number of tokens accepted
    size_t accepted_count;
    
    // Index of first rejected token (if any)
    size_t reject_index;
    
    // Corrected token at reject position (if rejected)
    uint32_t corrected_token;
    
    // Whether all tokens were accepted
    bool all_accepted;
    
    // Acceptance rate for this batch
    float acceptance_rate;
};

// Register-only verification buffer
// Keeps logits in registers/GPU shared memory during verification
class RegisterVerifyBuffer {
public:
    static constexpr size_t MAX_BATCH = 8;
    static constexpr size_t MAX_VOCAB = 32000; // Typical vocab size
    
    RegisterVerifyBuffer();
    ~RegisterVerifyBuffer();
    
    // Load draft logits into register buffer
    bool LoadDraftLogits(const float* logits, size_t count, size_t vocab_size);
    
    // Load target logits for verification
    bool LoadTargetLogits(const float* logits, size_t count, size_t vocab_size);
    
    // Perform register-only verification
    VerifyResult Verify(const DraftSequence& draft, float temperature);
    
    // Get accepted tokens
    size_t GetAcceptedTokens(uint32_t* tokens, size_t max_count);
    
    // Clear buffer
    void Clear();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// Fused speculative decoder
class FusedSpeculativeDecoder {
public:
    FusedSpeculativeDecoder();
    ~FusedSpeculativeDecoder();
    
    // Initialize with configuration
    bool Initialize(const SpeculativeConfig& config);
    
    // Generate draft tokens using small model
    DraftSequence GenerateDraft(const uint32_t* context, size_t context_len,
                                 uint64_t sequence_id);
    
    // Verify draft tokens against large model (register-only path)
    VerifyResult VerifyDraft(const DraftSequence& draft,
                              const float* target_logits,
                              size_t target_count);
    
    // Fused generate + verify step
    // Returns number of tokens actually generated
    size_t GenerateFused(const uint32_t* context, size_t context_len,
                         uint32_t* output_tokens, size_t max_output,
                         uint64_t sequence_id);
    
    // Get current acceptance rate
    float GetAcceptanceRate() const;
    
    // Get average tokens per step
    float GetAvgTokensPerStep() const;
    
    // Reset state for new sequence
    void Reset();
    
    // Performance stats
    struct Stats {
        uint64_t total_steps;
        uint64_t total_draft_tokens;
        uint64_t total_accepted_tokens;
        uint64_t total_rejected_tokens;
        float avg_acceptance_rate;
        float speedup_ratio;
        double avg_verify_time_us;
    };
    Stats GetStats() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// C-API for FFI
extern "C" {
    typedef struct RawrXD_SpeculativeDecoder RawrXD_SpeculativeDecoder;
    
    RawrXD_SpeculativeDecoder* RawrXD_speculative_decoder_create(
        size_t num_draft_tokens,
        float temperature,
        float top_p
    );
    
    void RawrXD_speculative_decoder_destroy(RawrXD_SpeculativeDecoder* decoder);
    
    size_t RawrXD_speculative_decoder_generate(
        RawrXD_SpeculativeDecoder* decoder,
        const uint32_t* context,
        size_t context_len,
        uint32_t* output_tokens,
        size_t max_output,
        uint64_t sequence_id
    );
    
    float RawrXD_speculative_decoder_get_acceptance_rate(RawrXD_SpeculativeDecoder* decoder);
    float RawrXD_speculative_decoder_get_speedup(RawrXD_SpeculativeDecoder* decoder);
    void RawrXD_speculative_decoder_reset(RawrXD_SpeculativeDecoder* decoder);
}

} // namespace Inference
} // namespace RawrXD
