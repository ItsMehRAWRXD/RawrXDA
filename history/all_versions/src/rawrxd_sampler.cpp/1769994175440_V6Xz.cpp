#include "rawrxd_sampler.h"
#include <random>
#include <numeric>

uint32_t RawrXDSampler::Sample(float* logits, size_t vocabSize, 
                               const std::vector<uint32_t>& promptTokens) {
    // 0. Pre-processing: Copy logits to a local vector to modify
    static std::vector<float> localLogits;
    if (localLogits.size() < vocabSize) localLogits.resize(vocabSize);
    std::copy(logits, logits + vocabSize, localLogits.begin());
    
    float* pLogits = localLogits.data();

    // 1. Repetition Penalty
    ApplyRepetitionPenalty(pLogits, vocabSize, promptTokens);
    
    // 2. Temperature scaling (Apply before Softmax)
    if (config.temperature > 0.0f) {
        float invTemp = 1.0f / config.temperature;
        for (size_t i = 0; i < vocabSize; i++) {
            pLogits[i] *= invTemp;
        }
    }

    // 3. Softmax
    SoftMax(pLogits, vocabSize);
    
    // 4. Sampling Filters (Top K / Top P)
    // We handle TopK/TopP by zeroing out probabilities or sorting.
    // For efficiency, we usually sort a parallel index array.
    
    // Simple greedy max if temp <= 0
    if (config.temperature <= 0.0f) {
        return (uint32_t)(std::max_element(pLogits, pLogits + vocabSize) - pLogits);
    }
    
    // 5. Multinomial Sample
    // Ideally we should apply TopK/TopP first
    if (config.topK > 0 && config.topK < (int)vocabSize) {
        // Zero out everything not in Top K
        // This is expensive O(N log N) with full sort. O(N) with quickselect.
        // Simplified:
        TopKFilter(pLogits, vocabSize, config.topK);
    }
    
    if (config.topP < 1.0f && config.topP > 0.0f) {
        TopPFilter(pLogits, vocabSize, config.topP);
    }
    
    // Renormalize after filtering?
    // Usually Multinomial handles unnormalized weights if using std::discrete_distribution
    return MultinomialSample(pLogits, vocabSize);
}

void RawrXDSampler::ApplyRepetitionPenalty(float* logits, size_t n, 
                            const std::vector<uint32_t>& tokens) {
    if (config.repetitionPenalty == 1.0f) return;
    
    // Scan recent tokens
    size_t start = (tokens.size() > maxRecent) ? (tokens.size() - maxRecent) : 0;
    
    for (size_t i = start; i < tokens.size(); i++) {
        uint32_t id = tokens[i];
        if (id < n) {
            // Apply penalty
            // Logits are log-probs usually? Or pre-softmax scores.
            // If negative score, multiply -> makes it more negative (good).
            // If positive score, divide -> makes it less positive (good).
            
            // Standard HF implementation:
            // if score < 0: score * penalty
            // else: score / penalty
            // Wait, usually penalty > 1.0 reduces probability.
            
            if (logits[id] < 0) logits[id] *= config.repetitionPenalty;
            else logits[id] /= config.repetitionPenalty;
        }
    }
}

void RawrXDSampler::SoftMax(float* x, size_t n) {
    float maxVal = *std::max_element(x, x + n);
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        x[i] = expf(x[i] - maxVal);
        sum += x[i];
    }
    float invSum = 1.0f / sum;
    for (size_t i = 0; i < n; i++) {
        x[i] *= invSum;
    }
}

void RawrXDSampler::TopKFilter(float* probs, size_t n, int k) {
    // Indices and probs pairs
    std::vector<std::pair<float, size_t>> candidates;
    candidates.reserve(n);
    for (size_t i=0; i<n; i++) candidates.push_back({probs[i], i});
    
    // Partial sort to find top K
    std::partial_sort(candidates.begin(), candidates.begin() + k, candidates.end(),
                      [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Zero out the rest
    float cutoff = candidates[k-1].first;
    for (size_t i=0; i<n; i++) {
        if (probs[i] < cutoff) probs[i] = 0.0f;
    }
}

void RawrXDSampler::TopPFilter(float* probs, size_t n, float p) {
    // Need sorted order to accumulate P
    std::vector<std::pair<float, size_t>> candidates;
    candidates.reserve(n);
    for (size_t i=0; i<n; i++) candidates.push_back({probs[i], i});
    
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
              
    float cumSum = 0.0f;
    size_t cutoffIdx = n;
    
    for (size_t i=0; i<n; i++) {
        cumSum += candidates[i].first;
        if (cumSum >= p) {
            cutoffIdx = i + 1;
            break;
        }
    }
    
    // Zero out everything after cutoff
    // This requires mapping back to original indices, which is slow.
    // Optimized way: find threshold value
    if (cutoffIdx < n) {
        float threshold = candidates[cutoffIdx-1].first;
        for (size_t i=0; i<n; i++) {
            if (probs[i] < threshold) probs[i] = 0.0f;
        }
    }
}

uint32_t RawrXDSampler::MultinomialSample(float* probs, size_t n) {
    // std::discrete_distribution
    static std::mt19937 gen(config.seed);
    std::discrete_distribution<> d(probs, probs + n);
    return (uint32_t)d(gen);
}
