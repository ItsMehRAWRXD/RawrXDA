#include <vector>
#include <algorithm>
#include <random>
#include <cmath>
#include <iostream>

struct Logit {
    int id;
    float value;
};

class Sampler {
    float temp;
    float top_p;
    int top_k;
    std::mt19937 rng;

public:
    Sampler() : temp(0.8f), top_p(0.9f), top_k(40), rng(std::random_device{}()) {}
    
    int sample(const std::vector<float>& logits) {
        // Upfront validation — fail-fast on invalid inputs
        if (logits.empty()) {
            std::cerr << "[Sampler] ERROR: sample() called with empty logits vector" << std::endl;
            return 0;  // Return first token as safe default
        }
        
        // Validate temperature is positive and reasonable
        if (temp <= 0.0f || temp > 100.0f) {
            std::cerr << "[Sampler] ERROR: Invalid temperature " << temp 
                      << " (must be in (0, 100]), reverting to 0.8" << std::endl;
            temp = 0.8f;  // Reset to safe default
        }

        // Softmax with temperature
        std::vector<float> probs = logits;
        float max_l = -1e9f;
        for (float l : probs) {
            if (l > max_l) max_l = l;
        }
        
        float sum = 0.0f;
        for (float& l : probs) {
            l = std::exp((l - max_l) / temp);
            if (!std::isfinite(l)) {
                std::cerr << "[Sampler] ERROR: Non-finite probability after softmax, may indicate bad logits" << std::endl;
                l = 0.0f;  // Treat as zero probability
            }
            sum += l;
        }
        
        if (sum <= 0.0f) {
            std::cerr << "[Sampler] ERROR: Probability sum is " << sum << " (invalid distribution), returning token 0" << std::endl;
            return 0;
        }
        
        for (float& l : probs) l /= sum;
        
        // Sampling with bounds validation
        std::discrete_distribution<int> dist(probs.begin(), probs.end());
        int sampled_token = dist(rng);
        
        // Defensive bounds check on returned token
        if (sampled_token < 0 || sampled_token >= static_cast<int>(logits.size())) {
            std::cerr << "[Sampler] ERROR: Sampled token " << sampled_token 
                      << " out of bounds [0, " << logits.size() << "), clamping to 0" << std::endl;
            sampled_token = 0;
        }
        
        return sampled_token;
    }
};
