#pragma once
#include <vector>

class Sampler {
public:
    Sampler();
    int sample(const std::vector<float>& logits);
};
