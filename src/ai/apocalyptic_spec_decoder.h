#pragma once
#include <cstdint>
#include <memory>

namespace rxd::ai {

struct ApocalypticConfig {
    uint32_t vocab_size = 32000;
    uint32_t max_draft_width = 256;
    uint32_t beam_width = 4;
    uint32_t ngram_max_order = 8;
    uint32_t ngram_slots_bits = 20;
    uint32_t mlp_hidden = 128;
    uint32_t mlp_embed = 128;
    uint32_t slim_vocab = 1024;
    float temperature = 0.8f;
    float accept_target_high = 0.85f;
    float accept_target_low  = 0.30f;
    float entropy_low_thresh = 0.15f;
    float entropy_high_thresh = 0.8f;
    bool enable_restless = true;
};

struct ApocalypticStats {
    uint64_t drafts_total;
    uint64_t tokens_accepted;
    uint64_t tokens_rejected;
    uint64_t cascade_recoveries;
    uint64_t beam_hits;
    uint64_t restless_prefetch;
    uint64_t octo_gram_hits;
    uint64_t skip_gram_hits;
    float acceptance_ewma;
    uint32_t current_width;
    float head_bandit_weights[6];
};

class ApocalypticSpecDecoder {
public:
    explicit ApocalypticSpecDecoder(const ApocalypticConfig& cfg);
    ~ApocalypticSpecDecoder();

    uint32_t Draft(const uint32_t* history, uint32_t hist_len,
                   uint32_t* out_draft, uint32_t max_draft);

    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target_argmax);

    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float* target_logits,
                                   uint32_t vocab_stride);

    void FeedAccepted(const uint32_t* seq, uint32_t len);

    void DistillLogits(const uint32_t* draft_tokens, uint32_t n,
                       const float* target_logits, uint32_t vocab_stride);

    ApocalypticStats GetStats() const;
    uint32_t GetDraftWidth() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rxd::ai
