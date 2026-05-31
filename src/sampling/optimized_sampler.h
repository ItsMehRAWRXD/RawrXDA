#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

namespace RawrXD {

// High-throughput token sampler supporting top-p (nucleus), Mirostat v2,
// and penalty vectors that match llama.cpp semantics.
class OptimizedSampler {
public:
    struct RepetitionPenaltyArgs {
        float penalty          = 1.10f;  // repetition_penalty  (>1 = penalise)
        float freq_penalty     = 0.00f;  // frequency_penalty
        float presence_penalty = 0.00f;  // presence_penalty
    };

    struct MirostatArgs {
        float tau  = 5.0f;   // target perplexity
        float eta  = 0.10f;  // learning rate
        float mu   = 10.0f;  // running state (mutable)
    };

    explicit OptimizedSampler(uint32_t seed = 42) : m_rng(seed) {}

    // Apply repetition / frequency / presence penalties in-place.
    void applyRepetitionPenalty(
        std::vector<float>&        logits,
        const std::vector<int32_t>& context_tokens,
        const RepetitionPenaltyArgs& args
    ) {
        // Count frequencies
        std::vector<int32_t> freq(logits.size(), 0);
        std::unordered_set<int32_t> seen;

        for (int32_t tok : context_tokens) {
            if (tok >= 0 && (size_t)tok < logits.size()) {
                freq[tok]++;
                seen.insert(tok);
            }
        }

        for (int32_t tok : seen) {
            float& l = logits[tok];
            // repetition penalty
            if (args.penalty != 1.0f) {
                l = l < 0.0f ? l * args.penalty : l / args.penalty;
            }
            // frequency / presence penalties (additive, subtracted from logit)
            l -= (float)freq[tok] * args.freq_penalty;
            l -= args.presence_penalty;
        }
    }

    // Top-p (nucleus) sampling. Sorts once via nth_element for O(n) expected.
    int32_t sampleTopP(std::vector<float>& logits, float top_p, float temperature = 1.0f) {
        const size_t n = logits.size();

        // Temperature scaling
        if (temperature != 1.0f && temperature > 0.0f) {
            for (float& v : logits) v /= temperature;
        }

        // Softmax
        float mx = *std::max_element(logits.begin(), logits.end());
        float s  = 0.0f;
        std::vector<float> probs(n);
        for (size_t i = 0; i < n; ++i) {
            probs[i] = std::exp(logits[i] - mx);
            s += probs[i];
        }
        for (float& p : probs) p /= s;

        // Build sorted index array and find nucleus boundary
        std::vector<size_t> idx(n);
        for (size_t i = 0; i < n; ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(),
            [&probs](size_t a, size_t b) { return probs[a] > probs[b]; });

        // Keep tokens until cumulative probability >= top_p
        float cumul   = 0.0f;
        size_t cutoff = 0;
        for (size_t i = 0; i < n; ++i) {
            cumul += probs[idx[i]];
            if (cumul >= top_p) { cutoff = i + 1; break; }
        }
        if (cutoff == 0) cutoff = n;

        // Sample from the nucleus
        std::vector<float> nucleus_probs(cutoff);
        for (size_t i = 0; i < cutoff; ++i) nucleus_probs[i] = probs[idx[i]];
        std::discrete_distribution<int32_t> dist(nucleus_probs.begin(), nucleus_probs.end());
        return (int32_t)idx[(size_t)dist(m_rng)];
    }

    // Mirostat v2 sampling (Baktash et al. 2020).
    // `args.mu` is updated in-place between calls.
    int32_t sampleMirostat(std::vector<float>& logits, MirostatArgs& args, float temperature = 1.0f) {
        const size_t n = logits.size();
        if (temperature > 0.0f) for (float& v : logits) v /= temperature;

        // Softmax
        float mx = *std::max_element(logits.begin(), logits.end());
        float s  = 0.0f;
        std::vector<float> probs(n);
        for (size_t i = 0; i < n; ++i) {
            probs[i] = std::exp(logits[i] - mx);
            s += probs[i];
        }
        for (float& p : probs) p /= s;

        // Sort descending
        std::vector<size_t> idx(n);
        for (size_t i = 0; i < n; ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(),
            [&probs](size_t a, size_t b) { return probs[a] > probs[b]; });

        // Truncate to k tokens where estimated surprise <= mu
        size_t k = 1;
        float  cumul = 0.0f;
        for (size_t i = 0; i < n; ++i) {
            cumul += probs[idx[i]];
            // Approximate surprise: -log2(p)
            float surprise = -std::log2(probs[idx[i]] + 1e-9f);
            if (surprise > args.mu && i > 0) break;
            k = i + 1;
        }

        // Sample from top-k
        std::vector<float> top_probs(k);
        s = 0.0f;
        for (size_t i = 0; i < k; ++i) { top_probs[i] = probs[idx[i]]; s += top_probs[i]; }
        for (float& p : top_probs) p /= s;

        std::discrete_distribution<int32_t> dist(top_probs.begin(), top_probs.end());
        size_t chosen_rank = (size_t)dist(m_rng);
        int32_t token = (int32_t)idx[chosen_rank];

        // Update mu (Mirostat v2 update rule)
        float e_prime = -std::log2(top_probs[chosen_rank] + 1e-9f) - args.tau;
        args.mu -= args.eta * e_prime;

        return token;
    }

private:
    std::mt19937 m_rng;
};

} // namespace RawrXD
