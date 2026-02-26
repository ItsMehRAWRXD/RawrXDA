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
        uint32_t draft_lookahead   = 5;     // K: number of draft tokens per step
        uint32_t vocab_size        = 32000;  // Vocabulary size
        uint32_t top_k             = 40;     // Top-K sampling parameter
        float    temperature       = 0.8f;   // Sampling temperature
        float    min_accept_prob   = 0.01f;  // Minimum probability to accept draft
        uint32_t max_tokens        = 2048;   // Maximum tokens to generate
        bool     greedy            = false;  // Greedy decoding (no sampling)
    };

    SpeculativeDecoder(ILanguageModel* draft, ILanguageModel* target, Config cfg = {})
        : draft_(draft), target_(target), config_(cfg)
    {
        rng_.seed(std::random_device{}());
    }

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
            // Step 1: Generate K draft tokens
            std::vector<uint32_t> draft_tokens;
            std::vector<TokenProbs> draft_probs;
            draft_tokens.reserve(config_.draft_lookahead);
            draft_probs.reserve(config_.draft_lookahead);

            auto seq_copy = sequence;
            for (uint32_t k = 0; k < config_.draft_lookahead; k++) {
                TokenProbs dp(config_.vocab_size);
                draft_->forward(seq_copy.data(),
                    static_cast<uint32_t>(seq_copy.size()), dp);
                applyTemperature(dp);
                dp.topK(config_.top_k);

                uint32_t token = config_.greedy
                    ? argmax(dp)
                    : dp.sample(rng_);

                draft_tokens.push_back(token);
                draft_probs.push_back(std::move(dp));
                seq_copy.push_back(token);
            }

            stats_.total_draft_tokens.fetch_add(config_.draft_lookahead,
                std::memory_order_relaxed);

            // Step 2: Verify all K tokens with target model in one pass
            // Target evaluates sequence + all draft tokens
            std::vector<TokenProbs> target_probs(config_.draft_lookahead + 1,
                TokenProbs(config_.vocab_size));

            // For efficiency, target model should support batch position evaluation
            // Here we simulate with sequential calls (real impl would batch)
            auto verify_seq = sequence;
            for (uint32_t k = 0; k <= config_.draft_lookahead; k++) {
                target_->forward(verify_seq.data(),
                    static_cast<uint32_t>(verify_seq.size()), target_probs[k]);
                applyTemperature(target_probs[k]);
                target_probs[k].topK(config_.top_k);

                if (k < config_.draft_lookahead) {
                    verify_seq.push_back(draft_tokens[k]);
                }
            }

            stats_.total_target_calls.fetch_add(1, std::memory_order_relaxed);

            // Step 3: Accept/reject from left to right
            uint32_t accepted = 0;
            for (uint32_t k = 0; k < config_.draft_lookahead; k++) {
                uint32_t token = draft_tokens[k];
                float p_target = target_probs[k].probs[token];
                float p_draft = draft_probs[k].probs[token];

                // Acceptance criterion: accept with probability min(1, p_target/p_draft)
                float accept_ratio = (p_draft > 1e-10f)
                    ? std::min(1.0f, p_target / p_draft)
                    : 0.0f;

                if (config_.greedy) {
                    // Greedy: accept if target's argmax matches draft
                    if (argmax(target_probs[k]) == token) {
                        accepted++;
                        sequence.push_back(token);
                        output[generated++] = token;
                        if (generated >= config_.max_tokens) break;
                    } else {
                        break;
                    }
                } else {
                    // Stochastic acceptance
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
            stats_.rejected_tokens.fetch_add(
                config_.draft_lookahead - accepted, std::memory_order_relaxed);

            // Step 4: If rejected early, sample correction token from target
            if (accepted < config_.draft_lookahead &&
                generated < config_.max_tokens) {
                // Resample from adjusted distribution:
                // p_adjusted(x) = max(0, p_target(x) - p_draft(x)) / Z
                TokenProbs& tp = target_probs[accepted];
                if (!config_.greedy && accepted < draft_probs.size()) {
                    TokenProbs& dp = draft_probs[accepted];
                    for (uint32_t i = 0; i < config_.vocab_size; i++) {
                        tp.probs[i] = std::max(0.0f, tp.probs[i] - dp.probs[i]);
                    }
                    // Re-normalize
                    float sum = 0.0f;
                    for (auto& p : tp.probs) sum += p;
                    if (sum > 1e-10f) {
                        float inv = 1.0f / sum;
                        for (auto& p : tp.probs) p *= inv;
                    }
                }

                uint32_t correction = config_.greedy
                    ? argmax(tp)
                    : tp.sample(rng_);

                sequence.push_back(correction);
                output[generated++] = correction;
            }

            // Check for EOS
            if (generated > 0 && output[generated - 1] == 2) { // EOS token = 2
                break;
            }
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

    void applyTemperature(TokenProbs& tp) {
        if (config_.temperature <= 0.0f || config_.temperature == 1.0f) return;
        float inv_t = 1.0f / config_.temperature;
        for (auto& p : tp.probs) p *= inv_t;
        tp.softmax();
    }

    uint32_t argmax(const TokenProbs& tp) {
        return static_cast<uint32_t>(
            std::max_element(tp.probs.begin(), tp.probs.end()) - tp.probs.begin()
        );
    }
};

} // namespace rawrxd

#endif // RAWRXD_SPECULATIVE_DECODER_HPP
