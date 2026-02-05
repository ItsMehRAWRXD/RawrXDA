#include "sampler.h"
#include <algorithm>
#include <numeric>
#include <cmath>

Sampler::Sampler() : rng(std::random_device{}()) {}

int Sampler::sample(float* logits, int n_vocab) {
    std::vector<float> probs(logits, logits + n_vocab);
    
    // Apply temperature
    if (temp != 1.0f) {
        for (auto& p : probs) p /= temp;
    }
    
    // Apply repeat penalty
    for (int t : last_tokens) {
        if (t < n_vocab) {
            if (probs[t] > 0) probs[t] /= repeat_penalty;
            else probs[t] *= repeat_penalty;
        }
    }
    
    // Softmax
    float max_logit = *std::max_element(probs.begin(), probs.end());
    float sum = 0;
    for (auto& p : probs) {
        p = expf(p - max_logit);
        sum += p;
    }
    for (auto& p : probs) p /= sum;
    
    // Top-K filtering
    if (top_k > 0 && top_k < n_vocab) {
        std::vector<std::pair<float, int>> sorted;
        for (int i = 0; i < n_vocab; i++) sorted.push_back({probs[i], i});
        std::partial_sort(sorted.begin(), sorted.begin() + top_k, sorted.end(),
            std::greater<>());
        
        float kth = sorted[top_k - 1].first;
        for (auto& p : probs) if (p < kth) p = 0;
        
        // Renormalize
        sum = std::accumulate(probs.begin(), probs.end(), 0.0f);
        for (auto& p : probs) p /= sum;
    }
    
    // Top-P (nucleus) filtering
    if (top_p < 1.0f) {
        std::vector<std::pair<float, int>> sorted;
        for (int i = 0; i < n_vocab; i++) sorted.push_back({probs[i], i});
        std::sort(sorted.begin(), sorted.end(), std::greater<>());
        
        float cumsum = 0;
        for (auto& [p, idx] : sorted) {
            cumsum += p;
            if (cumsum > top_p) {
                probs[idx] = 0;
            }
        }
        
        // Renormalize
        sum = std::accumulate(probs.begin(), probs.end(), 0.0f);
        for (auto& p : probs) p /= sum;
    }
    
    // Sample
    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    int token = dist(rng);
    
    // Update history
    last_tokens.push_back(token);
    if (last_tokens.size() > 1024) last_tokens.erase(last_tokens.begin());
    
    return token;
}
