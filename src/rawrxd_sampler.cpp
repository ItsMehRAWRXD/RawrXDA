#include "rawrxd_sampler.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>
#include <iostream>

extern "C" void SoftMax_AVX512(float* x, int size);

// Safety limits
static constexpr int MAX_VOCAB_SIZE = 1000000;  // 1M token vocab max
static constexpr int MIN_VOCAB_SIZE = 256;     // Minimum reasonable vocab
static constexpr float MAX_LOGIT = 100.0f;     // Prevent exp overflow
static constexpr float MIN_LOGIT = -100.0f;    // Prevent exp underflow

RawrXDSampler::RawrXDSampler() : rng(std::random_device{}()) {}

uint32_t RawrXDSampler::Sample(float* logits, int vocab_size, const std::vector<uint32_t>& history) {
    // Validate inputs
    if (!logits) {
        std::cerr << "[Sampler] ERROR: logits pointer is null" << std::endl;
        return 0;  // Default to token 0 (BOS/PAD)
    }
    
    if (vocab_size <= 0 || vocab_size > MAX_VOCAB_SIZE) {
        std::cerr << "[Sampler] ERROR: vocab_size out of range (" << vocab_size 
                  << ", expected 1-" << MAX_VOCAB_SIZE << ")" << std::endl;
        return 0;  // Default fallback
    }
    
    if (vocab_size < MIN_VOCAB_SIZE) {
        std::cerr << "[Sampler] WARNING: vocab_size suspiciously small (" << vocab_size 
                  << "), may produce poor samples" << std::endl;
    }
    
    // 1. Temperature scaling
    if (temperature > 0.0f && !std::isnan(temperature) && !std::isinf(temperature)) {
        for (int i = 0; i < vocab_size; ++i) {
            // Clamp before division to prevent overflow after scaling
            logits[i] = std::max(std::min(logits[i], MAX_LOGIT), MIN_LOGIT);
            logits[i] /= temperature;
        }
    } else {
        std::cerr << "[Sampler] WARNING: temperature is invalid (" << temperature 
                  << "), skipping scaling" << std::endl;
        for (int i = 0; i < vocab_size; ++i) {
            logits[i] = std::max(std::min(logits[i], MAX_LOGIT), MIN_LOGIT);
        }
    }
    
    // 2. Softmax (Call ASM kernel)
    // SoftMax_AVX512(logits, vocab_size);
    // Fallback C++ Softmax with safety checks
    float max_val = -std::numeric_limits<float>::infinity();
    
    // Find max, handling NaN/Inf
    for (int i = 0; i < vocab_size; i++) {
        if (std::isfinite(logits[i]) && logits[i] > max_val) {
            max_val = logits[i];
        }
    }
    
    // If all logits are NaN/Inf, use 0
    if (!std::isfinite(max_val)) {
        max_val = 0.0f;
        std::cerr << "[Sampler] WARNING: all logits are non-finite, using 0 offset" << std::endl;
    }
    
    // Compute exp(logits - max) and sum
    float sum = 0.0f;
    for (int i = 0; i < vocab_size; i++) {
        float val = logits[i] - max_val;
        float exp_val = std::exp(std::max(std::min(val, 100.0f), -100.0f)); // Clamp to prevent over/underflow
        
        if (std::isnan(exp_val) || std::isinf(exp_val)) {
            exp_val = 0.0f;
        }
        
        logits[i] = exp_val;
        sum += exp_val;
    }
    
    // 3. Normalize probabilities
    if (sum <= 0.0f || !std::isfinite(sum)) {
        std::cerr << "[Sampler] ERROR: probability sum is invalid (" << sum 
                  << "), using uniform distribution" << std::endl;
        // Fall back to uniform: set all logits to 1/vocab_size
        for (int i = 0; i < vocab_size; i++) {
            logits[i] = 1.0f / static_cast<float>(vocab_size);
        }
        sum = 1.0f;
    } else {
        for (int i = 0; i < vocab_size; i++) {
            logits[i] /= sum;
        }
    }
    
    // 4. Sampling using discrete distribution
    try {
        std::discrete_distribution<uint32_t> dist(logits, logits + vocab_size);
        return dist(rng);
    } catch (const std::exception& e) {
        std::cerr << "[Sampler] ERROR: discrete_distribution failed: " << e.what() << std::endl;
        // Fallback: return random token
        std::uniform_int_distribution<uint32_t> uniform(0, vocab_size - 1);
        return uniform(rng);
    }
}
