// RawrXD Sampler - Production-grade inference sampling
// Top-K, Nucleus (Top-P), Temperature, Repetition Penalty

#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

class RawrXDSampler {
public:
    struct Config {
        float temperature = 0.8f;
        int topK = 40;
        float topP = 0.9f;
        float repetitionPenalty = 1.1f;
        uint32_t seed = 0xDEADBEEF;
    } config;
    
    // Token repetition tracking
    std::vector<uint32_t> recentTokens;
    size_t maxRecent = 512;

    void SetConfig(const Config& cfg) { config = cfg; }
    
    // Main entry: logits in, token ID out
    uint32_t Sample(float* logits, size_t vocabSize, 
                    const std::vector<uint32_t>& promptTokens) {
        // 1. Apply repetition penalty first (on raw logits)
        ApplyRepetitionPenalty(logits, vocabSize, promptTokens);
        
        // 2. Top-K filtering on raw logits (truncate unlikely tokens first)
        TopKFilter(logits, vocabSize, config.topK);
        
        // 3. Apply temperature (on surviving logits only)
        if (config.temperature > 0.0f && config.temperature != 1.0f) {
            float invTemp = 1.0f / config.temperature;
            for (size_t i = 0; i < vocabSize; i++) {
                if (logits[i] > -1e30f)  // skip already-filtered tokens
                    logits[i] *= invTemp;
            }
        }
        
        // 4. Softmax to get probabilities (handles -INFINITY → 0)
        SoftMax(logits, vocabSize);
        
        // 5. Top-P (Nucleus) filtering on probabilities
        TopPFilter(logits, vocabSize, config.topP);
        
        // 6. Renormalize filtered probabilities (simple sum-divide)
        Normalize(logits, vocabSize);
        
        // 7. Sample from remaining distribution
        return MultinomialSample(logits, vocabSize);
    }

private:
    void SoftMax(float* x, size_t n) {
        // Numerically stable softmax
        float maxVal = x[0];
        for (size_t i = 1; i < n; i++) {
            if (x[i] > maxVal) maxVal = x[i];
        }
        
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
    
    void ApplyRepetitionPenalty(float* logits, size_t n, 
                                const std::vector<uint32_t>& tokens) {
        // Penalize tokens that have appeared recently
        for (uint32_t token : tokens) {
            if (token >= n) continue;
            
            if (logits[token] > 0) {
                logits[token] /= config.repetitionPenalty;
            } else {
                logits[token] *= config.repetitionPenalty;
            }
        }
    }
    
    void TopKFilter(float* logits, size_t n, int k) {
        if (k <= 0 || k >= (int)n) return;
        
        // Partial sort to find k-th largest
        std::vector<float> sorted(logits, logits + n);
        std::nth_element(sorted.begin(), sorted.begin() + k, sorted.end(), 
                        std::greater<float>());
        float threshold = sorted[k];
        
        // Zero out everything below threshold
        for (size_t i = 0; i < n; i++) {
            if (logits[i] < threshold) {
                logits[i] = -INFINITY;
            }
        }
    }
    
    void TopPFilter(float* logits, size_t n, float p) {
        if (p >= 1.0f || p <= 0.0f) return;
        
        // Sort descending
        std::vector<size_t> indices(n);
        for (size_t i = 0; i < n; i++) indices[i] = i;
        
        std::sort(indices.begin(), indices.end(),
                 [&](size_t a, size_t b) { return logits[a] > logits[b]; });
        
        // Find cutoff where cumulative prob exceeds p
        float cumSum = 0.0f;
        size_t cutoff = n;
        for (size_t i = 0; i < n; i++) {
            cumSum += logits[indices[i]];
            if (cumSum > p) {
                cutoff = i + 1;
                break;
            }
        }
        
        // Zero out tail
        for (size_t i = cutoff; i < n; i++) {
            logits[indices[i]] = 0.0f;
        }
    }
    
    void Normalize(float* x, size_t n) {
        float sum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            if (x[i] > 0.0f) sum += x[i];
        }
        if (sum > 0.0f) {
            float invSum = 1.0f / sum;
            for (size_t i = 0; i < n; i++) {
                x[i] = x[i] > 0.0f ? x[i] * invSum : 0.0f;
            }
        }
    }
    
    uint32_t MultinomialSample(float* probs, size_t n) {
        // Xorshift RNG (deterministic, fast)
        config.seed ^= config.seed << 13;
        config.seed ^= config.seed >> 17;
        config.seed ^= config.seed << 5;
        
        float u = (config.seed & 0xFFFFFF) / 16777216.0f; // [0,1)
        
        float cum = 0.0f;
        for (size_t i = 0; i < n; i++) {
            if (probs[i] <= 0) continue;
            cum += probs[i];
            if (u < cum) return (uint32_t)i;
        }
        
        return 0; // Fallback
    }
};
