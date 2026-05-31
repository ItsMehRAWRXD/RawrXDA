// ================================================================
// speculative_decoder.hpp — Speculative Decoding Engine
// Draft (7B) + Target (120B) parallel token verification
// ================================================================

#pragma once
#ifndef RAWRXD_SPECULATIVE_DECODER_HPP
#define RAWRXD_SPECULATIVE_DECODER_HPP

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <random>
#include <vector>

namespace rawrxd {

// ================================================================
// Token probability distribution
// ================================================================
struct TokenProbs {
    std::vector<float>    probs;       // probability distribution over vocab
    std::vector<uint32_t> top_indices; // top-K token indices (sorted)
    uint32_t              vocab_size = 0;
    uint32_t              sampled_id = 0;

    explicit TokenProbs(uint32_t vocab = 32000)
        : probs(vocab, 0.0f), vocab_size(vocab) {}

    // Softmax normalization in-place
    void softmax() {
        float max_val = *std::max_element(probs.begin(), probs.end());
        float sum = 0.0f;
        for (auto& p : probs) {
            p = std::exp(p - max_val);
            sum += p;
        }
        float inv_sum = 1.0f / (sum + 1e-10f);
        for (auto& p : probs) p *= inv_sum;
    }

    // Top-K filtering: zero out all but top K probabilities
    void topK(uint32_t k) {
        if (k >= vocab_size) return;

        // Partial sort to find k-th largest
        std::vector<std::pair<float, uint32_t>> indexed(vocab_size);
        for (uint32_t i = 0; i < vocab_size; i++) {
            indexed[i] = { probs[i], i };
        }
        std::partial_sort(indexed.begin(), indexed.begin() + k, indexed.end(),
            [](auto& a, auto& b) { return a.first > b.first; });

        top_indices.resize(k);
        float threshold = indexed[k - 1].first;
        for (uint32_t i = 0; i < vocab_size; i++) {
            if (probs[i] < threshold) probs[i] = 0.0f;
        }
        for (uint32_t i = 0; i < k; i++) {
            top_indices[i] = indexed[i].second;
        }

        // Re-normalize
        float sum = 0.0f;
        for (auto& p : probs) sum += p;
        float inv = 1.0f / (sum + 1e-10f);
        for (auto& p : probs) p *= inv;
    }

    // Sample from distribution
    uint32_t sample(std::mt19937& rng) {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float r = dist(rng);
        float cumulative = 0.0f;
        for (uint32_t i = 0; i < vocab_size; i++) {
            cumulative += probs[i];
            if (cumulative >= r) {
                sampled_id = i;
                return i;
            }
        }
        sampled_id = vocab_size - 1;
        return sampled_id;
    }

    // Shannon entropy in bits: H = -Σ p·log2(p).
    // Low (1–3 bits)  → peaked distribution → draft likely accepted.
    // High (>5 bits)  → diffuse distribution → draft likely rejected.
    float entropy() const {
        float h = 0.0f;
        for (const float p : probs) {
            if (p > 1e-10f) h -= p * std::log2f(p);
        }
        return h;
    }

    // Top-p (nucleus) sampling: keep smallest set of tokens whose
    // cumulative probability ≥ p, zero out the rest.
    void topP(float p) {
        if (p >= 1.0f) return;
        // Build (prob, index) pairs for the nonzero entries
        std::vector<std::pair<float, uint32_t>> sorted;
        sorted.reserve(vocab_size);
        for (uint32_t i = 0; i < vocab_size; i++) {
            if (probs[i] > 0.0f) sorted.push_back({probs[i], i});
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b) { return a.first > b.first; });

        float cumul = 0.0f;
        size_t cutoff = sorted.size();
        for (size_t i = 0; i < sorted.size(); ++i) {
            cumul += sorted[i].first;
            if (cumul >= p) { cutoff = i + 1; break; }
        }
        // Zero out everything beyond the nucleus
        // Build a set of kept indices
        std::vector<bool> keep(vocab_size, false);
        for (size_t i = 0; i < cutoff; ++i)
            keep[sorted[i].second] = true;
        for (uint32_t i = 0; i < vocab_size; ++i)
            if (!keep[i]) probs[i] = 0.0f;

        // Re-normalize
        float sum = 0.0f;
        for (auto& v : probs) sum += v;
        float inv = 1.0f / (sum + 1e-10f);
        for (auto& v : probs) v *= inv;
    }

    // Min-p sampling: for each token, zero it if p(token) < min_p * p(max).
    // Dynamically adjusts the cutoff based on the confidence of the best token.
    void minP(float min_p) {
        if (min_p <= 0.0f) return;
        float max_prob = *std::max_element(probs.begin(), probs.end());
        float threshold = min_p * max_prob;
        for (uint32_t i = 0; i < vocab_size; ++i)
            if (probs[i] < threshold) probs[i] = 0.0f;
        // Re-normalize
        float sum = 0.0f;
        for (auto& v : probs) sum += v;
        float inv = 1.0f / (sum + 1e-10f);
        for (auto& v : probs) v *= inv;
    }

    // Repetition penalty: reduce probability of recently seen tokens.
    // penalty > 1.0 penalizes, < 1.0 encourages. Applied to logits (pre-softmax).
    void applyRepetitionPenalty(const uint32_t* context, uint32_t context_len,
                                float penalty) {
        if (penalty == 1.0f || context_len == 0) return;
        for (uint32_t i = 0; i < context_len; ++i) {
            uint32_t tid = context[i];
            if (tid < vocab_size) {
                // Apply multiplicative penalty to logit (pre-softmax prob)
                // For post-softmax: convert, penalize, re-normalize
                if (probs[tid] > 0.0f) {
                    probs[tid] = (probs[tid] > 1e-10f)
                        ? probs[tid] / penalty
                        : 0.0f;
                }
            }
        }
        // Re-normalize
        float sum = 0.0f;
        for (auto& v : probs) sum += v;
        if (sum > 0.0f) {
            float inv = 1.0f / sum;
            for (auto& v : probs) v *= inv;
        }
    }
};

// ================================================================
// Model interface — abstract base for draft and target models
// ================================================================
class ILanguageModel {
public:
    virtual ~ILanguageModel() = default;

    // Run forward pass for a sequence of token IDs
    // Returns logits/probabilities for next token
    virtual void forward(const uint32_t* tokens, uint32_t num_tokens,
                         TokenProbs& output) = 0;

    // Get model parameter count (for identification)
    virtual uint64_t paramCount() const = 0;

    // Get model name
    virtual const char* name() const = 0;
};

// ================================================================
// Speculative Decoding Statistics
// ================================================================
struct SpeculativeStats {
    std::atomic<uint64_t> total_draft_tokens{0};
    std::atomic<uint64_t> accepted_tokens{0};
    std::atomic<uint64_t> rejected_tokens{0};
    std::atomic<uint64_t> total_target_calls{0};
    std::atomic<uint64_t> total_tokens_generated{0};
    std::atomic<uint64_t> total_time_us{0};

    double acceptanceRate() const {
        uint64_t total = total_draft_tokens.load(std::memory_order_relaxed);
        return total > 0
            ? static_cast<double>(accepted_tokens.load(std::memory_order_relaxed)) / total
            : 0.0;
    }

    double tokensPerSecond() const {
        uint64_t time = total_time_us.load(std::memory_order_relaxed);
        return time > 0
            ? static_cast<double>(total_tokens_generated.load(std::memory_order_relaxed))
              * 1e6 / time
            : 0.0;
    }

    void reset() {
        total_draft_tokens.store(0, std::memory_order_relaxed);
        accepted_tokens.store(0, std::memory_order_relaxed);
        rejected_tokens.store(0, std::memory_order_relaxed);
        total_target_calls.store(0, std::memory_order_relaxed);
        total_tokens_generated.store(0, std::memory_order_relaxed);
        total_time_us.store(0, std::memory_order_relaxed);
    }
};

// ================================================================
// SpeculativeDecoder
// ================================================================
// Uses a small draft model (e.g., 7B) to generate K candidate tokens,
// then verifies all K tokens in a single forward pass of the target model
// (e.g., 120B). Accepted tokens are kept, producing multiple tokens per
// target model call.
//
// Algorithm:
// 1. Draft model generates K tokens autoregressively
// 2. Target model evaluates all K+1 positions in one pass
// 3. Accept tokens from left to right while target agrees
// 4. On first rejection, resample from adjusted distribution
// 5. Repeat
// ================================================================
class SpeculativeDecoder {
public:
    struct Config {
        uint32_t draft_lookahead   = 5;     // K: initial number of draft tokens per step
        uint32_t vocab_size        = 32000;  // Vocabulary size
        uint32_t top_k             = 40;     // Top-K sampling parameter
        float    temperature       = 0.8f;   // Sampling temperature
        float    min_accept_prob   = 0.01f;  // Minimum probability to accept draft
        uint32_t max_tokens        = 2048;   // Maximum tokens to generate
        bool     greedy            = false;  // Greedy decoding (no sampling)

        // ---- Adaptive lookahead control ----
        // Automatically tunes K based on a rolling acceptance-rate window.
        //
        // Policy (evaluated every adapt_window target-model calls):
        //   accept_rate > high_threshold  → K = min(K+1, max_lookahead)
        //   accept_rate < low_threshold   → K = max(K-1, min_lookahead)
        //
        // When draft model is well-aligned (acceptance ~0.8+), growing K to 7–8
        // amortizes more target-model calls per step, delivering the promised 2–3×
        // throughput gain.  When diverging (acceptance ~0.4–), shrinking K to 1–2
        // reduces wasted draft computation and corrects faster.
        bool     adaptive_lookahead  = true;
        uint32_t min_lookahead       = 1;
        uint32_t max_lookahead       = 8;
        uint32_t adapt_window        = 20;   // rolling window in target-model calls
        float    high_threshold      = 0.80f; // acceptance rate → grow K
        float    low_threshold       = 0.45f; // acceptance rate → shrink K

        // ---- Token-budget draft pruner ----
        // When remaining token budget falls to budget_margin or below, K is
        // forced to 1 to avoid generating excess draft tokens that will be
        // discarded anyway.  When budget < current K (but > budget_margin),
        // K is clamped to the remaining count, preventing over-shoot waste.
        uint32_t budget_margin       = 4;    // force K=1 when budget ≤ this

        // ---- Entropy-based draft pruner ----
        // Stops drafting early when the draft model's probability distribution
        // is too diffuse (high Shannon entropy in bits).  A flat distribution
        // means many tokens have similar probability → the target model is
        // unlikely to agree with the draft → wasted target forward calls.
        //
        //   Low entropy (1–3 bits) → well-peaked → good draft quality.
        //   High entropy (>5 bits) → ~32 equiprobable tokens → poor quality.
        //
        // Default 5.5 bits ≈ 46 effective equiprobable tokens.  Disable by
        // setting entropy_pruning = false or threshold = FLT_MAX.
        bool     entropy_pruning         = true;
        float    draft_entropy_threshold = 5.5f;  // bits

        // ---- Sampling pipeline ----
        // Applied in order: temperature → topK → topP → minP → repetition penalty.
        float    top_p              = 0.95f;  // Nucleus (top-p) threshold. 1.0 = disabled.
        float    min_p              = 0.0f;   // Min-p dynamic cutoff.      0.0 = disabled.
        float    repetition_penalty = 1.0f;   // >1.0 penalizes repeats.   1.0 = disabled.
        uint32_t rep_penalty_window = 64;     // look-back window for rep penalty
    };

    SpeculativeDecoder(ILanguageModel* draft, ILanguageModel* target, Config cfg = {})
        : draft_(draft), target_(target), config_(cfg)
        , current_lookahead_(cfg.draft_lookahead)
        , rolling_accepted_(0), rolling_total_(0), rolling_window_steps_(0)
    {
        rng_.seed(std::random_device{}());

        // Pre-allocate the hot-path probability pool once — eliminates ~2 MB of
        // malloc/free per decode step (K=8, vocab=32 K).  Layout:
        //   slots [0 .. max_lookahead-1]         → draft positions
        //   slots [max_lookahead .. 2*max_lookahead] → target positions (+1 for correction)
        const uint32_t pool_size = 2u * cfg.max_lookahead + 2u;
        probs_pool_.reserve(pool_size);
        for (uint32_t i = 0; i < pool_size; ++i)
            probs_pool_.emplace_back(cfg.vocab_size);

        draft_tokens_.reserve(cfg.max_lookahead);
        seq_copy_.reserve(cfg.max_tokens + cfg.max_lookahead + 1u);
        verify_seq_.reserve(cfg.max_tokens + cfg.max_lookahead + 1u);
    }

    // Current active lookahead (changes dynamically when adaptive_lookahead=true).
    uint32_t currentLookahead() const { return current_lookahead_; }

    // ================================================================
    // generate — Run speculative decoding to generate tokens
    // ================================================================
    // prompt: initial token sequence
    // prompt_len: number of prompt tokens
    // output: buffer for generated tokens (at least max_tokens capacity)
    // Returns: number of tokens generated
    // ================================================================
    uint32_t generate(const uint32_t* prompt, uint32_t prompt_len,
                      uint32_t* output) {
        auto t0 = std::chrono::high_resolution_clock::now();

        // Copy prompt to working sequence
        std::vector<uint32_t> sequence(prompt, prompt + prompt_len);
        uint32_t generated = 0;

        while (generated < config_.max_tokens) {
            // Step 1: Generate K draft tokens using the *current adaptive* lookahead,
            // clamped by token-budget pruner to avoid wasted draft computation near EOS.
            const uint32_t remaining = config_.max_tokens - generated;
            const uint32_t K = (remaining <= config_.budget_margin)
                ? 1u
                : std::min(current_lookahead_, remaining);
            // Step 1: Draft tokens — pool slots, zero heap allocations.
            // Entropy gate: stop early when the distribution is too diffuse.
            draft_tokens_.clear();
            seq_copy_.assign(sequence.begin(), sequence.end());
            uint32_t actual_k = 0;   // may be < K when entropy pruning fires

            for (uint32_t k = 0; k < K; ++k) {
                TokenProbs& dp = draftSlot(k);
                resetEntry(dp);
                draft_->forward(seq_copy_.data(),
                    static_cast<uint32_t>(seq_copy_.size()), dp);
                applySampling(dp, seq_copy_);

                // Entropy-based early exit: flat distribution = likely rejection.
                if (config_.entropy_pruning &&
                        dp.entropy() > config_.draft_entropy_threshold) {
                    break;
                }

                ++actual_k;
                const uint32_t token = config_.greedy ? argmax(dp) : dp.sample(rng_);
                draft_tokens_.push_back(token);
                seq_copy_.push_back(token);
            }

            stats_.total_draft_tokens.fetch_add(actual_k, std::memory_order_relaxed);

            // Step 2: Verify actual_k tokens with target (actual_k+1 forward passes).
            verify_seq_.assign(sequence.begin(), sequence.end());
            for (uint32_t k = 0; k <= actual_k; ++k) {
                TokenProbs& tp = targetSlot(k);
                resetEntry(tp);
                target_->forward(verify_seq_.data(),
                    static_cast<uint32_t>(verify_seq_.size()), tp);
                applySampling(tp, verify_seq_);
                if (k < actual_k) verify_seq_.push_back(draft_tokens_[k]);
            }

            stats_.total_target_calls.fetch_add(1, std::memory_order_relaxed);

            // Step 3: Accept/reject from left to right.
            uint32_t accepted = 0;
            for (uint32_t k = 0; k < actual_k; ++k) {
                const uint32_t token  = draft_tokens_[k];
                const float p_target  = targetSlot(k).probs[token];
                const float p_draft   = draftSlot(k).probs[token];

                const float accept_ratio = (p_draft > 1e-10f)
                    ? std::min(1.0f, p_target / p_draft)
                    : 0.0f;

                if (config_.greedy) {
                    if (argmax(targetSlot(k)) == token) {
                        accepted++;
                        sequence.push_back(token);
                        output[generated++] = token;
                        if (generated >= config_.max_tokens) break;
                    } else {
                        break;
                    }
                } else {
                    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                    if (dist(rng_) < accept_ratio) {
                        accepted++;
                        sequence.push_back(token);
                        output[generated++] = token;
                        if (generated >= config_.max_tokens) break;
                    } else {
                        break;
                    }
                }
            }

            stats_.accepted_tokens.fetch_add(accepted, std::memory_order_relaxed);
            stats_.rejected_tokens.fetch_add(actual_k - accepted, std::memory_order_relaxed);

            // Step 4: Resample correction token from adjusted target distribution.
            if (accepted < actual_k && generated < config_.max_tokens) {
                TokenProbs& tp = targetSlot(accepted);
                if (!config_.greedy && accepted < actual_k) {
                    TokenProbs& dp = draftSlot(accepted);
                    for (uint32_t i = 0; i < config_.vocab_size; ++i) {
                        tp.probs[i] = std::max(0.0f, tp.probs[i] - dp.probs[i]);
                    }
                    float sum = 0.0f;
                    for (const auto& p : tp.probs) sum += p;
                    if (sum > 1e-10f) {
                        const float inv = 1.0f / sum;
                        for (auto& p : tp.probs) p *= inv;
                    }
                }
                const uint32_t correction = config_.greedy ? argmax(tp) : tp.sample(rng_);
                sequence.push_back(correction);
                output[generated++] = correction;
            }

            // Adaptive lookahead: update rolling window and adjust K if warranted.
            adaptLookahead(accepted, actual_k);

            // EOS check
            if (generated > 0 && output[generated - 1] == 2) break;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        stats_.total_tokens_generated.fetch_add(generated, std::memory_order_relaxed);
        stats_.total_time_us.fetch_add(elapsed_us, std::memory_order_relaxed);

        return generated;
    }

    const SpeculativeStats& stats() const { return stats_; }
    void resetStats() { stats_.reset(); }

    void setConfig(const Config& cfg) { config_ = cfg; }
    const Config& config() const { return config_; }

private:
    ILanguageModel*  draft_;
    ILanguageModel*  target_;
    Config           config_;
    SpeculativeStats stats_;
    std::mt19937     rng_;

    // Adaptive lookahead state
    uint32_t current_lookahead_;     // live K — may differ from config_.draft_lookahead
    uint32_t rolling_accepted_;      // cumulative accepted tokens in current window
    uint32_t rolling_total_;         // cumulative draft tokens in current window
    uint32_t rolling_window_steps_;  // target-model calls in current window

    // Hot-path pool — pre-allocated at construction, reused every decode step.
    // Eliminates ~2 MB of malloc/free per decode step at K=8, vocab=32 K.
    // Layout: slots [0..max_lookahead-1] = draft; [max_lookahead..2*max_lookahead] = target.
    std::vector<TokenProbs> probs_pool_;
    std::vector<uint32_t>   draft_tokens_;  // reusable per-step draft token list
    std::vector<uint32_t>   seq_copy_;      // draft-phase sequence scratch buffer
    std::vector<uint32_t>   verify_seq_;    // target-phase sequence scratch buffer

    TokenProbs& draftSlot(uint32_t k)  { return probs_pool_[k]; }
    TokenProbs& targetSlot(uint32_t k) { return probs_pool_[config_.max_lookahead + k]; }
    static void resetEntry(TokenProbs& tp) {
        std::fill(tp.probs.begin(), tp.probs.end(), 0.0f);
        tp.top_indices.clear();
        tp.sampled_id = 0;
    }

    // ================================================================
    // adaptLookahead — called after each generation step.
    //
    // Accumulates acceptance statistics over a rolling window of
    // `config_.adapt_window` target-model calls.  At window boundary,
    // evaluates the mean acceptance rate and adjusts current_lookahead_:
    //
    //   rate ≥ high_threshold → K++ (draft is well-aligned, amortise more)
    //   rate ≤ low_threshold  → K-- (draft is diverging, correct faster)
    //   otherwise             → K unchanged
    //
    // K is always clamped to [min_lookahead, max_lookahead].
    // ================================================================
    void adaptLookahead(uint32_t accepted, uint32_t total) {
        if (!config_.adaptive_lookahead || config_.adapt_window == 0) return;

        rolling_accepted_ += accepted;
        rolling_total_    += total;
        rolling_window_steps_++;

        if (rolling_window_steps_ < config_.adapt_window) return;

        // Window boundary: compute rate and adjust K.
        const float rate = (rolling_total_ > 0)
            ? static_cast<float>(rolling_accepted_) / static_cast<float>(rolling_total_)
            : 0.0f;

        if (rate >= config_.high_threshold && current_lookahead_ < config_.max_lookahead) {
            current_lookahead_++;
        } else if (rate <= config_.low_threshold && current_lookahead_ > config_.min_lookahead) {
            current_lookahead_--;
        }

        // Reset window counters.
        rolling_accepted_     = 0;
        rolling_total_        = 0;
        rolling_window_steps_ = 0;
    }

    void applyTemperature(TokenProbs& tp) {
        if (config_.temperature <= 0.0f || config_.temperature == 1.0f) return;
        float inv_t = 1.0f / config_.temperature;
        for (auto& p : tp.probs) p *= inv_t;
        tp.softmax();
    }

    // Full sampling pipeline: temperature → topK → topP → minP → repetition penalty
    void applySampling(TokenProbs& tp, const std::vector<uint32_t>& seq) {
        applyTemperature(tp);
        tp.topK(config_.top_k);
        if (config_.top_p < 1.0f)   tp.topP(config_.top_p);
        if (config_.min_p > 0.0f)   tp.minP(config_.min_p);
        if (config_.repetition_penalty != 1.0f && !seq.empty()) {
            uint32_t window = config_.rep_penalty_window;
            uint32_t start  = (seq.size() > window) ?
                              static_cast<uint32_t>(seq.size() - window) : 0u;
            tp.applyRepetitionPenalty(seq.data() + start,
                                     static_cast<uint32_t>(seq.size() - start),
                                     config_.repetition_penalty);
        }
    }

    uint32_t argmax(const TokenProbs& tp) {
        return static_cast<uint32_t>(
            std::max_element(tp.probs.begin(), tp.probs.end()) - tp.probs.begin()
        );
    }
};

} // namespace rawrxd

#endif // RAWRXD_SPECULATIVE_DECODER_HPP
