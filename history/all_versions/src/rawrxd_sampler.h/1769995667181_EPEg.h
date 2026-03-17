#pragma once
#include <vector>
#include <string>
#include <random>

class RawrXDSampler {
    std::mt19937 rng;
    
public:
    RawrXDSampler();
    uint32_t Sample(float* logits, int vocab_size, const std::vector<uint32_t>& history);
    
private:
    float temperature = 0.7f;
    float top_p = 0.9f;
    int top_k = 40;
};
