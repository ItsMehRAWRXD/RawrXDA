#pragma once
#include <cstdint>
#include <memory>

namespace rxd::ai {

struct SingularityV2Config {
    uint32_t vocab_size = 32000;
    uint32_t max_draft_width = 512;         // entropy-driven, up to 512
    uint32_t octo_gram_bits = 22;           // 4M 8-gram slots
    uint32_t sedecim_gram_bits = 20;        // 1M 16-gram slots (long-range)
    uint32_t minhash_slots = 1 << 19;       // 512k semantic fingerprints
    uint32_t mlp_hidden = 512;              // BFloat16 MLP
    uint32_t mlp_embed = 512;
    uint32_t slim_vocab = 2048;             // MLP bottleneck
    uint32_t scope_depth = 128;             // variable predictor scope stack
    uint32_t kv_cache_rows = 8192;          // differentiable KV cache
    uint32_t restless_buffers = 4;          // quadruple-buffered restless drafting
    float temperature = 0.8f;
    float accept_target_high = 0.90f;
    float accept_target_low  = 0.20f;
    float bandit_temperature = 0.3f;        // aggressive Thompson sampling
};

struct SingularityV2Stats {
    uint64_t drafts_total;
    uint64_t tokens_accepted;
    uint64_t tokens_rejected;
    uint64_t cascade_recoveries;
    uint64_t syntax_oracle_hits;
    uint64_t var_predict_hits;
    uint64_t octo_gram_hits;
    uint64_t sedecim_gram_hits;
    uint64_t minhash_hits;
    uint64_t kv_cache_hits;
    uint64_t restless_prefetch_hits;
    float acceptance_ewma;
    uint32_t current_width;
    float head_bandit_weights[12];
};

class SingularitySpecDecoderV2 {
public:
    explicit SingularitySpecDecoderV2(const SingularityV2Config& cfg);
    ~SingularitySpecDecoderV2();

    // Hot path: produce draft tokens. Returns count generated.
    uint32_t Draft(const uint32_t* history, uint32_t hist_len,
                   uint32_t* out_draft, uint32_t max_draft);

    // Validate with target argmax. Returns accepted count.
    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target_argmax);

    // Probabilistic validation with temperature-corrected rejection sampling.
    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float* target_logits,
                                   uint32_t vocab_stride);

    // Feed back accepted sequence for online training.
    void FeedAccepted(const uint32_t* seq, uint32_t len);

    SingularityV2Stats GetStats() const;
    uint32_t GetDraftWidth() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rxd::ai
