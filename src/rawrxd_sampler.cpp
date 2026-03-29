#include "rawrxd_sampler.h"
#include <cmath>
#include <algorithm>
#include <numeric>

extern "C" void SoftMax_AVX512(float* x, int size);

RawrXDSampler::RawrXDSampler() : rng(std::random_device{}()) {}

uint32_t RawrXDSampler::Sample(float* logits, int vocab_size, const std::vector<uint32_t>& history) {
    // 1. Temperature
    if (temperature > 0) {
        for (int i=0; i<vocab_size; ++i) logits[i] /= temperature;
    }
    
    // 2. Softmax (Call ASM kernel)
    // SoftMax_AVX512(logits, vocab_size);
    // Fallback C++ Softmax for robustness if ASM crashes/not linked
    float max_val = -1e9;
    for(int i=0; i<vocab_size; i++) if(logits[i] > max_val) max_val = logits[i];
    
    float sum = 0.0f;
    for(int i=0; i<vocab_size; i++) {
        logits[i] = exp(logits[i] - max_val);
        sum += logits[i];
    }
    for(int i=0; i<vocab_size; i++) logits[i] /= sum;

    // 3. Sampling
    std::discrete_distribution<uint32_t> dist(logits, logits + vocab_size);
    return dist(rng);
}
