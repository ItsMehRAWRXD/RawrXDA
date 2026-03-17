#pragma once
#include <vector>
#include <random>

class Sampler {
public:
    float temp = 0.8f;
    int top_k = 40;
    float top_p = 0.9f;
    float repeat_penalty = 1.1f;
    
    std::vector<int> last_tokens;
    std::mt19937 rng;
    
    Sampler();
    int sample(float* logits, int n_vocab);
    std::vector<int> beam_search(float* logits, int n_vocab, int beam_size, int max_length);
    int mirostat_sample(float* logits, int n_vocab, float tau, float eta);
    void reset();
};
