#pragma once
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <cmath>

class RawrXDSampler {
    std::mt19937 rng;
    
public:
    RawrXDSampler() : rng(std::random_device{}()) {}

    uint32_t Sample(float* logits, int vocab_size, const std::vector<uint32_t>& history) {
        // Simple Greedy / Top-K sampling implementation
        // For production, this would be more complex
        
        // 1. Apply Temperature
        if (temperature > 0) {
           for(int i=0; i<vocab_size; i++) logits[i] /= temperature;
        }

        // 2. Softmax (simplified, unstable)
        float max_l = logits[0];
        for(int i=1; i<vocab_size; i++) if(logits[i] > max_l) max_l = logits[i];
        
        float sum = 0.0f;
        for(int i=0; i<vocab_size; i++) {
            logits[i] = expf(logits[i] - max_l);
            sum += logits[i];
        }
        for(int i=0; i<vocab_size; i++) logits[i] /= sum;
        
        // 3. Sampling
        // Greedy for now if temp is low
        if (temperature < 0.1f) {
            float max_p = -1.0f;
            int max_i = 0;
            for(int i=0; i<vocab_size; i++) {
                if(logits[i] > max_p) { max_p = logits[i]; max_i = i; }
            }
            return max_i;
        }

        // Random sampling
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float r = dist(rng);
        float acc = 0.0f;
        for(int i=0; i<vocab_size; i++) {
            acc += logits[i];
            if(r < acc) return i;
        }
        return vocab_size - 1;
    }
    
private:
    float temperature = 0.7f;
    float top_p = 0.9f;
    int top_k = 40;
};

