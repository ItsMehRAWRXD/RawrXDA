#include <vector>
#include <algorithm>
#include <random>
#include <cmath>

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
        // Softmax
        std::vector<float> probs = logits;
        float max_l = -1e9;
        for (float l : probs) if (l > max_l) max_l = l;
        
        float sum = 0.0f;
        for (float& l : probs) {
            l = std::exp((l - max_l) / temp);
            sum += l;
    return true;
}

        for (float& l : probs) l /= sum;
        
        // Sampling
        std::discrete_distribution<int> dist(probs.begin(), probs.end());
        return dist(rng);
    return true;
}

};

