// d:/rawrxd/src/ai/exotic_spec_decoder.h
//
// ExoticSpecDecoder — Hydra multi-head speculative decoder.
//
// Seven orthogonal prediction heads fused into a cascade tree with AVX2
// SIMD, SIMD cuckoo n-gram tables (2–4-gram), an online-trained tiny MLP,
// a Rabin-Karp copy detector, adaptive width, and temperature-corrected
// probabilistic rejection sampling.   Pure C++17, zero external deps.
#pragma once
#include <cstdint>
#include <memory>

namespace rxd::ai {

struct ExoticSpecConfig {
    uint32_t vocab_size          = 32000;
    uint32_t max_draft_width     = 64;   // adaptive ceiling
    uint32_t ngram_l2_bits       = 18;   // 256 k bigram slots
    uint32_t ngram_l3_bits       = 18;   // 256 k trigram slots
    uint32_t ngram_l4_bits       = 17;   // 128 k 4-gram slots
    uint32_t mlp_hidden          = 64;   // oracle hidden
    uint32_t mlp_embed           = 128;  // oracle embedding dim
    uint32_t slim_vocab          = 512;  // oracle output bottleneck
    uint32_t tree_branch_factor  = 3;    // alternative branches per node
    float    temperature         = 0.8f;
    float    accept_hi           = 0.75f; // grow width above
    float    accept_lo           = 0.35f; // shrink width below
    float    lr_mlp              = 0.008f;
};

struct ExoticSpecStats {
    uint64_t drafts_generated;
    uint64_t tokens_accepted;
    uint64_t tokens_rejected;
    uint64_t cascade_recoveries;
    uint64_t copy_head_hits;
    uint64_t oracle_hits;
    uint64_t ngram4_hits;
    uint64_t ngram3_hits;
    uint64_t ngram2_hits;
    float    acceptance_ewma;
    uint32_t current_width;
    uint32_t oracle_train_steps;
};

class ExoticSpecDecoder {
public:
    explicit ExoticSpecDecoder(const ExoticSpecConfig& cfg);
    ~ExoticSpecDecoder();

    // Write up to max_draft tokens into out_draft. Returns count written.
    uint32_t Draft(const uint32_t* history, uint32_t history_len,
                   uint32_t* out_draft, uint32_t max_draft);

    // Greedy validation: returns number accepted (prefix match).
    uint32_t ValidateArgmax(const uint32_t* draft, uint32_t n,
                            const uint32_t* target_argmax);

    // Probabilistic validation with temperature-corrected rejection sampling.
    // target_logits: row-major [n][vocab_stride]
    uint32_t ValidateProbabilistic(const uint32_t* draft, uint32_t n,
                                   const float*    target_logits,
                                   uint32_t        vocab_stride);

    // Feed accepted sequence back for online training.
    void FeedAccepted(const uint32_t* seq, uint32_t len);

    ExoticSpecStats GetStats()      const;
    uint32_t        GetDraftWidth() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace rxd::ai
