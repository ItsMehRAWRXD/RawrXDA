#include "rawrxd_sampler.h"
#include <cmath>
#include <algorithm>
#include <numeric>

extern "C" void SoftMax_AVX512(float* x, int size);

RawrXDSampler::RawrXDSampler() : rng(std::random_device{}()) {}

uint32_t RawrXDSampler::Sample(float* logits, int vocab_size, const std::vector<uint32_t>& history) {
    // 1. Temperature scaling
    if (temperature > 0) {
        for (int i = 0; i < vocab_size; ++i) logits[i] /= temperature;
    }

    // 2. Softmax
    float max_val = -1e9f;
    for (int i = 0; i < vocab_size; i++) {
        if (logits[i] > max_val) max_val = logits[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < vocab_size; i++) {
        logits[i] = expf(logits[i] - max_val);
        sum += logits[i];
    }
    for (int i = 0; i < vocab_size; i++) logits[i] /= sum;

    // 3. Top-K filtering: keep only top_k highest-probability tokens
    if (top_k > 0 && top_k < vocab_size) {
        // Build index-probability pairs
        std::vector<std::pair<float, int>> scored(vocab_size);
        for (int i = 0; i < vocab_size; i++) scored[i] = {logits[i], i};

        // Partial sort to find top-k threshold
        std::nth_element(scored.begin(), scored.begin() + top_k, scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        float threshold = scored[top_k - 1].first;
        for (int i = 0; i < vocab_size; i++) {
            if (logits[i] < threshold) logits[i] = 0.0f;
        }
    }

    // 4. Top-P (nucleus) filtering: keep smallest set whose cumulative prob >= top_p
    if (top_p > 0.0f && top_p < 1.0f) {
        std::vector<std::pair<float, int>> sorted_probs(vocab_size);
        for (int i = 0; i < vocab_size; i++) sorted_probs[i] = {logits[i], i};
        std::sort(sorted_probs.begin(), sorted_probs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        float cumsum = 0.0f;
        int cutoff = vocab_size;
        for (int i = 0; i < vocab_size; i++) {
            cumsum += sorted_probs[i].first;
            if (cumsum >= top_p) {
                cutoff = i + 1;
                break;
            }
        }
        // Zero out everything below the nucleus
        for (int i = cutoff; i < vocab_size; i++) {
            logits[sorted_probs[i].second] = 0.0f;
        }
    }

    // 5. Re-normalize after filtering
    sum = 0.0f;
    for (int i = 0; i < vocab_size; i++) sum += logits[i];
    if (sum > 0.0f) {
        for (int i = 0; i < vocab_size; i++) logits[i] /= sum;
    }

    // 6. Sample from the filtered distribution
    std::discrete_distribution<uint32_t> dist(logits, logits + vocab_size);
    return dist(rng);
}
