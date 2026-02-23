#pragma once
#include <vector>
#include <random>
#include <cstdint>
#include <string>

// ============================================================================
// PCG32 — Fast, high-quality PRNG (O'Neill). 10-20x faster than mt19937
// ============================================================================
struct PCG32 {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc   = 0xda3e39cb94b95bdbULL;

    void seed(uint64_t s) {
        state = 0; inc = (s << 1) | 1;
        next(); state += s; next();
    }

    uint32_t next() {
        uint64_t old = state;
        state = old * 6364136223846793005ULL + inc;
        uint32_t xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27);
        uint32_t rot = (uint32_t)(old >> 59);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    // Uniform float in [0, 1)
    float nextf() {
        return (float)(next() >> 8) * (1.0f / 16777216.0f);
    }
};

class Sampler {
public:
    float temp = 0.8f;
    int top_k = 40;
    float top_p = 0.9f;
    float repeat_penalty = 1.1f;
    
    std::vector<int> last_tokens;
    
    // Legacy RNG kept for API compat — PCG32 used in hot path
    std::mt19937 rng;
    
    // Fast RNG for sampling
    PCG32 fast_rng;
    
    // Pre-allocated buffers — NEVER allocate during sample()
    std::vector<float> prob_buf;          // reusable probability buffer
    std::vector<std::pair<float,int>> sorted_buf;  // reusable sort buffer
    int last_vocab_size = 0;             // track resize
    
    Sampler();
    int sample(float* logits, int n_vocab);

    bool setCustomStopSequences(const std::vector<std::string>& sequences);
    bool setGrammarConstraints(const std::string& grammar);

private:
    void ensureBuffers(int n_vocab);
    int sampleFromProbs(float* probs, int n);

    std::vector<std::string> custom_stop_sequences_;
    std::string grammar_constraints_;
};
