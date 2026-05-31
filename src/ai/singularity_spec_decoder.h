#pragma once
#include <cstdint>
#include <memory>

namespace rxd::ai {

struct SingularityConfig {
    uint32_t vocab_size       = 32000;
    uint32_t max_draft_width  = 256;
    uint32_t octo_gram_bits   = 21;        // 2M 8-gram slots
    uint32_t minhash_slots    = 1 << 18;   // 256k semantic rows (direct count)
    uint32_t mlp_hidden       = 256;
    uint32_t mlp_embed        = 256;
    uint32_t slim_vocab       = 1024;      // MLP output bottleneck
    uint32_t scope_depth      = 64;
    float    temperature      = 0.8f;
    float    accept_target_high = 0.85f;
    float    accept_target_low  = 0.25f;
    float    bandit_temperature = 0.5f;
};

struct SingularityStats {
    uint64_t drafts_total;
    uint64_t tokens_accepted;
    uint64_t tokens_rejected;
    uint64_t cascade_recoveries;
    uint64_t syntax_oracle_hits;
    uint64_t var_predict_hits;
    uint64_t octo_gram_hits;
    uint64_t minhash_hits;
    float    acceptance_ewma;
    uint32_t current_width;
    float    head_bandit_weights[8];
};

class SingularitySpecDecoder {
public:
    explicit SingularitySpecDecoder(const SingularityConfig& cfg);
    ~SingularitySpecDecoder();

    uint32_t Draft(const uint32_t* history, uint32_t hist_len,
                   uint32_t* out_draft, uint32_t max_draft);

    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target_argmax);

    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float* target_logits,
                                   uint32_t vocab_stride);

    void FeedAccepted(const uint32_t* seq, uint32_t len);

    SingularityStats GetStats() const;
    uint32_t GetDraftWidth() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rxd::ai
