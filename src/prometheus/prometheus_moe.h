#pragma once
#include "prometheus_config.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

namespace Prometheus {

// =============================================================================
// EXPERT NETWORK
// =============================================================================

struct ExpertWeights {
    std::vector<float> upWeight;
    std::vector<float> upBias;
    std::vector<float> gateWeight;
    std::vector<float> downWeight;
    std::vector<float> downBias;
};

// =============================================================================
// MOE LAYER
// =============================================================================

class MoELayer {
public:
    static constexpr size_t MAX_EXPERTS = 32;

    MoELayer(uint32_t hiddenDim, uint32_t intermediateDim,
             uint32_t numExperts, uint32_t expertsPerToken, uint32_t sharedExperts = 0);

    void forward(
        const float* input,
        float* output,
        uint32_t batchSize,
        uint32_t* expertIndices,
        float* expertWeights
    );

    ExpertWeights& expert(size_t i) { return experts_[i]; }
    const ExpertWeights& expert(size_t i) const { return experts_[i]; }
    std::vector<float>& gateWeight() { return gateWeight_; }
    const std::vector<float>& gateWeight() const { return gateWeight_; }
    ExpertWeights& sharedExpert(size_t i) { return sharedExpertWeights_[i]; }

    struct Stats {
        std::vector<uint64_t> expertUsage;
        uint64_t totalTokens = 0;
        float loadBalanceLoss = 0.0f;
    };
    Stats getStats() const;
    void resetStats();

private:
    uint32_t hiddenDim_;
    uint32_t intermediateDim_;
    uint32_t numExperts_;
    uint32_t expertsPerToken_;
    uint32_t sharedExperts_;
    std::vector<float> gateWeight_;
    std::vector<ExpertWeights> experts_;
    std::vector<ExpertWeights> sharedExpertWeights_;
    mutable std::vector<uint64_t> expertUsage_;
    mutable uint64_t totalTokens_ = 0;

    float swiglu(float gate, float up) const {
        return gate / (1.0f + std::exp(-gate)) * up;
    }

    void expertForward(const float* input, float* output, size_t expertIdx, uint32_t batchSize);
    void route(const float* input, uint32_t* indices, float* weights, uint32_t batchSize);
};

// =============================================================================
// INLINE IMPLEMENTATION
// =============================================================================

inline MoELayer::MoELayer(uint32_t hiddenDim, uint32_t intermediateDim,
                   uint32_t numExperts, uint32_t expertsPerToken, uint32_t sharedExperts)
    : hiddenDim_(hiddenDim)
    , intermediateDim_(intermediateDim)
    , numExperts_(numExperts)
    , expertsPerToken_(expertsPerToken)
    , sharedExperts_(sharedExperts)
{
    gateWeight_.resize((size_t)hiddenDim * numExperts, 0.0f);
    experts_.resize(numExperts);
    for (auto& e : experts_) {
        e.upWeight.resize((size_t)hiddenDim * intermediateDim);
        e.gateWeight.resize((size_t)hiddenDim * intermediateDim);
        e.downWeight.resize((size_t)intermediateDim * hiddenDim);
    }
    sharedExpertWeights_.resize(sharedExperts);
    for (auto& e : sharedExpertWeights_) {
        e.upWeight.resize((size_t)hiddenDim * intermediateDim);
        e.gateWeight.resize((size_t)hiddenDim * intermediateDim);
        e.downWeight.resize((size_t)intermediateDim * hiddenDim);
    }
    expertUsage_.resize(numExperts, 0);
}

inline void MoELayer::forward(
    const float* input,
    float* output,
    uint32_t batchSize,
    uint32_t* expertIndices,
    float* expertWeights
) {
    std::fill(output, output + (size_t)batchSize * hiddenDim_, 0.0f);
    route(input, expertIndices, expertWeights, batchSize);

    for (size_t se = 0; se < sharedExpertWeights_.size(); ++se) {
        expertForward(input, output, se, batchSize);
    }

    for (uint32_t b = 0; b < batchSize; ++b) {
        std::vector<float> expertOut(hiddenDim_);
        for (uint32_t k = 0; k < expertsPerToken_; ++k) {
            uint32_t expertIdx = expertIndices[(size_t)b * expertsPerToken_ + k];
            float weight = expertWeights[(size_t)b * expertsPerToken_ + k];
            const float* in = input + (size_t)b * hiddenDim_;

            std::vector<float> up(intermediateDim_);
            std::vector<float> gate(intermediateDim_);

            for (uint32_t i = 0; i < intermediateDim_; ++i) {
                float upSum = 0.0f, gateSum = 0.0f;
                for (uint32_t j = 0; j < hiddenDim_; ++j) {
                    upSum  += in[j] * experts_[expertIdx].upWeight[(size_t)j * intermediateDim_ + i];
                    gateSum += in[j] * experts_[expertIdx].gateWeight[(size_t)j * intermediateDim_ + i];
                }
                up[i] = upSum;
                gate[i] = gateSum;
            }

            for (uint32_t i = 0; i < intermediateDim_; ++i) {
                gate[i] = swiglu(gate[i], up[i]);
            }

            std::fill(expertOut.begin(), expertOut.end(), 0.0f);
            for (uint32_t i = 0; i < hiddenDim_; ++i) {
                float sum = 0.0f;
                for (uint32_t j = 0; j < intermediateDim_; ++j) {
                    sum += gate[j] * experts_[expertIdx].downWeight[(size_t)j * hiddenDim_ + i];
                }
                expertOut[i] = sum * weight;
            }

            for (uint32_t i = 0; i < hiddenDim_; ++i) {
                output[(size_t)b * hiddenDim_ + i] += expertOut[i];
            }
            expertUsage_[expertIdx]++;
        }
    }
    totalTokens_ += batchSize;
}

inline void MoELayer::route(
    const float* input,
    uint32_t* indices,
    float* weights,
    uint32_t batchSize
) {
    std::vector<float> logits((size_t)batchSize * numExperts_);
    for (uint32_t b = 0; b < batchSize; ++b) {
        for (uint32_t e = 0; e < numExperts_; ++e) {
            float sum = 0.0f;
            for (uint32_t i = 0; i < hiddenDim_; ++i) {
                sum += input[(size_t)b * hiddenDim_ + i] * gateWeight_[(size_t)i * numExperts_ + e];
            }
            logits[(size_t)b * numExperts_ + e] = sum;
        }
    }

    std::mt19937 rng(42);
    for (uint32_t b = 0; b < batchSize; ++b) {
        float* logit = logits.data() + (size_t)b * numExperts_;
        for (uint32_t e = 0; e < numExperts_; ++e) {
            std::normal_distribution<float> noise(0.0f, 1.0f);
            logit[e] += noise(rng) * 0.1f;
        }
        std::vector<std::pair<float, uint32_t>> scored;
        scored.reserve(numExperts_);
        for (uint32_t e = 0; e < numExperts_; ++e) scored.push_back({logit[e], e});
        std::partial_sort(scored.begin(), scored.begin() + expertsPerToken_, scored.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        float maxLogit = scored[0].first;
        float sum = 0.0f;
        for (uint32_t k = 0; k < expertsPerToken_; ++k) {
            float exp = std::exp(scored[k].first - maxLogit);
            weights[(size_t)b * expertsPerToken_ + k] = exp;
            indices[(size_t)b * expertsPerToken_ + k] = scored[k].second;
            sum += exp;
        }
        for (uint32_t k = 0; k < expertsPerToken_; ++k) {
            weights[(size_t)b * expertsPerToken_ + k] /= sum;
        }
    }
}

inline void MoELayer::expertForward(
    const float* input,
    float* output,
    size_t expertIdx,
    uint32_t batchSize
) {
    auto& expert = sharedExpertWeights_[expertIdx];
    for (uint32_t b = 0; b < batchSize; ++b) {
        const float* in = input + (size_t)b * hiddenDim_;
        float* out = output + (size_t)b * hiddenDim_;
        std::vector<float> up(intermediateDim_);
        std::vector<float> gate(intermediateDim_);
        for (uint32_t i = 0; i < intermediateDim_; ++i) {
            float upSum = 0.0f, gateSum = 0.0f;
            for (uint32_t j = 0; j < hiddenDim_; ++j) {
                upSum  += in[j] * expert.upWeight[(size_t)j * intermediateDim_ + i];
                gateSum += in[j] * expert.gateWeight[(size_t)j * intermediateDim_ + i];
            }
            up[i] = upSum;
            gate[i] = gateSum;
        }
        std::vector<float> down(hiddenDim_, 0.0f);
        for (uint32_t i = 0; i < intermediateDim_; ++i) {
            float activated = swiglu(gate[i], up[i]);
            for (uint32_t j = 0; j < hiddenDim_; ++j) {
                down[j] += activated * expert.downWeight[(size_t)i * hiddenDim_ + j];
            }
        }
        for (uint32_t i = 0; i < hiddenDim_; ++i) {
            out[i] += down[i];
        }
    }
}

inline MoELayer::Stats MoELayer::getStats() const {
    Stats s;
    s.expertUsage = expertUsage_;
    s.totalTokens = totalTokens_;
    if (totalTokens_ > 0) {
        float sum = 0.0f;
        for (auto count : expertUsage_) {
            float frac = static_cast<float>(count) / static_cast<float>(totalTokens_);
            sum += frac * frac;
        }
        s.loadBalanceLoss = sum * static_cast<float>(numExperts_);
    }
    return s;
}

inline void MoELayer::resetStats() {
    std::fill(expertUsage_.begin(), expertUsage_.end(), 0);
    totalTokens_ = 0;
}

} // namespace Prometheus
