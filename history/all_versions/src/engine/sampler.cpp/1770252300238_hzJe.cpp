#include "sampler.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <queue>
#include <set>

Sampler::Sampler() : rng(std::random_device{}()) {}

// Single-step nucleus/top-k sampling with temperature
int Sampler::sample(float* logits, int n_vocab) {
    std::vector<float> probs(logits, logits + n_vocab);
    
    // Apply temperature scaling
    if (temp != 1.0f) {
        for (auto& p : probs) {
            p /= temp;
        }
    }
    
    // Apply repeat penalty (penalize recently generated tokens)
    for (int token : last_tokens) {
        if (token >= 0 && token < n_vocab) {
            if (probs[token] > 0.0f) {
                probs[token] /= repeat_penalty;
            } else {
                probs[token] *= repeat_penalty;
            }
        }
    }
    
    // Softmax with numerical stability
    float max_logit = *std::max_element(probs.begin(), probs.end());
    float sum_exp = 0.0f;
    
    for (auto& p : probs) {
        p = expf(p - max_logit);
        sum_exp += p;
    }
    
    for (auto& p : probs) {
        p /= sum_exp;
    }
    
    // Top-K filtering: keep only top K tokens
    if (top_k > 0 && top_k < n_vocab) {
        std::vector<std::pair<float, int>> sorted_probs;
        sorted_probs.reserve(n_vocab);
        
        for (int i = 0; i < n_vocab; i++) {
            sorted_probs.push_back({probs[i], i});
        }
        
        // Partial sort to find top K
        std::partial_sort(sorted_probs.begin(), 
                         sorted_probs.begin() + std::min(top_k, (int)sorted_probs.size()),
                         sorted_probs.end(),
                         [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Threshold for top K
        float kth_value = sorted_probs[std::min(top_k - 1, (int)sorted_probs.size() - 1)].first;
        
        for (int i = 0; i < n_vocab; i++) {
            if (probs[i] < kth_value) {
                probs[i] = 0.0f;
            }
        }
        
        // Renormalize
        sum_exp = std::accumulate(probs.begin(), probs.end(), 0.0f);
        if (sum_exp > 1e-8f) {
            for (auto& p : probs) {
                p /= sum_exp;
            }
        }
    }
    
    // Top-P (Nucleus) filtering: keep tokens until cumulative probability exceeds top_p
    if (top_p < 1.0f) {
        std::vector<std::pair<float, int>> sorted_probs;
        sorted_probs.reserve(n_vocab);
        
        for (int i = 0; i < n_vocab; i++) {
            sorted_probs.push_back({probs[i], i});
        }
        
        // Sort by probability (descending)
        std::sort(sorted_probs.begin(), sorted_probs.end(),
                 [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Find cutoff point
        float cumsum = 0.0f;
        float cutoff = 0.0f;
        
        for (const auto& [prob, idx] : sorted_probs) {
            cumsum += prob;
            if (cumsum > top_p) {
                cutoff = prob;
                break;
            }
        }
        
        // Zero out probabilities below cutoff
        for (int i = 0; i < n_vocab; i++) {
            if (probs[i] < cutoff) {
                probs[i] = 0.0f;
            }
        }
        
        // Renormalize
        sum_exp = std::accumulate(probs.begin(), probs.end(), 0.0f);
        if (sum_exp > 1e-8f) {
            for (auto& p : probs) {
                p /= sum_exp;
            }
        }
    }
    
    // Sample from filtered distribution
    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    int sampled_token = dist(rng);
    
    // Update token history for repeat penalty
    last_tokens.push_back(sampled_token);
    if (last_tokens.size() > 1024) {
        last_tokens.erase(last_tokens.begin());
    }
    
    return sampled_token;
}

// Beam search: explore multiple hypotheses in parallel
std::vector<int> Sampler::beam_search(float* logits, int n_vocab, int beam_size, int max_length) {
    // Beam search state: (score, tokens)
    struct Beam {
        float score;
        std::vector<int> tokens;
        
        bool operator<(const Beam& other) const {
            return score > other.score;  // Min-heap by score
        }
    };
    
    std::priority_queue<Beam> beams;
    std::set<std::vector<int>> finished;
    
    // Initialize with top beam_size tokens
    std::vector<float> probs(logits, logits + n_vocab);
    
    // Softmax
    float max_logit = *std::max_element(probs.begin(), probs.end());
    float sum_exp = 0.0f;
    for (auto& p : probs) {
        p = expf(p - max_logit);
        sum_exp += p;
    }
    for (auto& p : probs) {
        p /= sum_exp;
    }
    
    // Get top beam_size candidates
    std::vector<std::pair<float, int>> sorted_probs;
    for (int i = 0; i < n_vocab; i++) {
        sorted_probs.push_back({probs[i], i});
    }
    std::partial_sort(sorted_probs.begin(), 
                     sorted_probs.begin() + std::min(beam_size, n_vocab),
                     sorted_probs.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });
    
    for (int i = 0; i < std::min(beam_size, n_vocab); i++) {
        Beam b;
        b.score = logf(sorted_probs[i].first);  // Use log probability
        b.tokens.push_back(sorted_probs[i].second);
        beams.push(b);
    }
    
    // Expand beams iteratively
    for (int step = 1; step < max_length; step++) {
        std::priority_queue<Beam> new_beams;
        
        while (!beams.empty() && new_beams.size() < beam_size) {
            Beam current = beams.top();
            beams.pop();
            
            // Check if this is a terminal token (e.g., EOS)
            if (!current.tokens.empty() && current.tokens.back() == 2) {  // Assuming 2 is EOS
                finished.insert(current.tokens);
                continue;
            }
            
            // Simulate next token scores (in real implementation, would use model)
            for (int next_token = 0; next_token < std::min(n_vocab, beam_size); next_token++) {
                Beam expanded = current;
                expanded.score += logf(std::max(probs[next_token], 1e-10f));
                expanded.tokens.push_back(next_token);
                
                if (new_beams.size() < beam_size) {
                    new_beams.push(expanded);
                } else if (expanded.score > new_beams.top().score) {
                    new_beams.pop();
                    new_beams.push(expanded);
                }
            }
        }
        
        beams = new_beams;
    }
    
    // Return best beam
    if (!beams.empty()) {
        return beams.top().tokens;
    } else if (!finished.empty()) {
        return *finished.begin();
    }
    
    return {};
}

// Mirostat sampling: dynamic temperature control for consistent perplexity
int Sampler::mirostat_sample(float* logits, int n_vocab, float tau, float eta) {
    std::vector<float> probs(logits, logits + n_vocab);
    
    // Adaptive temperature based on surprise
    static float current_tau = tau;
    
    // Apply dynamic temperature
    for (auto& p : probs) {
        p /= current_tau;
    }
    
    // Softmax
    float max_logit = *std::max_element(probs.begin(), probs.end());
    float sum_exp = 0.0f;
    for (auto& p : probs) {
        p = expf(p - max_logit);
        sum_exp += p;
    }
    for (auto& p : probs) {
        p /= sum_exp;
    }
    
    // Sample
    std::discrete_distribution<int> dist(probs.begin(), probs.end());
    int token = dist(rng);
    
    // Update tau based on surprise (KL divergence)
    float surprise = -logf(std::max(probs[token], 1e-10f));
    current_tau *= expf(eta * (surprise - tau));
    
    return token;
}

// Reset sampler state
void Sampler::reset() {
    last_tokens.clear();
