#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <functional>

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
                    const std::vector<uint32_t>& promptTokens);

private:
    void SoftMax(float* x, size_t n);
    void ApplyRepetitionPenalty(float* logits, size_t n, 
                                const std::vector<uint32_t>& tokens);
    void TopKFilter(float* logits, size_t n, int k);
    void TopPFilter(float* logits, size_t n, float p);
    uint32_t MultinomialSample(float* probs, size_t n);
};
