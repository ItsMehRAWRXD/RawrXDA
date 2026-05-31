#pragma once
#include "prometheus_config.h"

#include <cmath>
#include <vector>

namespace Prometheus {

// =============================================================================
// GROUPED QUERY ATTENTION WITH RING ATTENTION
// =============================================================================

class AttentionLayer {
public:
    AttentionLayer(
        uint32_t hiddenDim,
        uint32_t numHeads,
        uint32_t numKVHeads,
        uint32_t headDim,
        uint32_t slidingWindow = 0,
        uint32_t globalStride = 0
    );

    void forward(
        const float* input,
        float* output,
        const float* kvCacheK,
        const float* kvCacheV,
        uint32_t seqLen,
        uint32_t kvLen,
        uint32_t layerIdx,
        bool isGeneration = false
    );

    void updateKVCache(float* kvCacheK, float* kvCacheV, uint32_t position);

    std::vector<float>& qWeight() { return qWeight_; }
    std::vector<float>& kWeight() { return kWeight_; }
    std::vector<float>& vWeight() { return vWeight_; }
    std::vector<float>& oWeight() { return oWeight_; }

    void applyRoPE(float* q, float* k, uint32_t seqLen, uint32_t positionOffset = 0);

private:
    uint32_t hiddenDim_;
    uint32_t numHeads_;
    uint32_t numKVHeads_;
    uint32_t headDim_;
    uint32_t slidingWindow_;
    uint32_t globalStride_;
    uint32_t headPerKV_;

    std::vector<float> qWeight_;
    std::vector<float> kWeight_;
    std::vector<float> vWeight_;
    std::vector<float> oWeight_;
    std::vector<float> ropeFreqs_;

    static constexpr uint32_t FLASH_BLOCK_SIZE = 256;

    void flashAttention(
        const float* q,
        const float* k,
        const float* v,
        float* output,
        uint32_t kvLen,
        uint32_t position,
        uint32_t layerIdx
    );

    void ringAttention(
        const float* q,
        float* kBlock,
        float* vBlock,
        float* output,
        float* maxScore,
        float* sumExp,
        uint32_t blockSize
    );
};

// =============================================================================
// INLINE IMPLEMENTATION
// =============================================================================

inline AttentionLayer::AttentionLayer(
    uint32_t hiddenDim,
    uint32_t numHeads,
    uint32_t numKVHeads,
    uint32_t headDim,
    uint32_t slidingWindow,
    uint32_t globalStride
)
    : hiddenDim_(hiddenDim)
    , numHeads_(numHeads)
    , numKVHeads_(numKVHeads)
    , headDim_(headDim)
    , slidingWindow_(slidingWindow)
    , globalStride_(globalStride)
    , headPerKV_(numHeads / numKVHeads)
{
    qWeight_.resize((size_t)hiddenDim * numHeads * headDim);
    kWeight_.resize((size_t)hiddenDim * numKVHeads * headDim);
    vWeight_.resize((size_t)hiddenDim * numKVHeads * headDim);
    oWeight_.resize((size_t)numHeads * headDim * hiddenDim);

    uint32_t maxPos = 262144;
    ropeFreqs_.resize((size_t)maxPos * headDim / 2);
    float theta = 10000000.0f;
    float scale = 2.0f;
    for (uint32_t pos = 0; pos < maxPos; ++pos) {
        for (uint32_t i = 0; i < headDim / 2; ++i) {
            float freq = 1.0f / std::pow(theta * scale, static_cast<float>(2 * i) / headDim);
            ropeFreqs_[(size_t)pos * headDim / 2 + i] = pos * freq;
        }
    }
}

inline void AttentionLayer::forward(
    const float* input,
    float* output,
    const float* kvCacheK,
    const float* kvCacheV,
    uint32_t seqLen,
    uint32_t kvLen,
    uint32_t layerIdx,
    bool /*isGeneration*/
) {
    std::vector<float> Q((size_t)seqLen * numHeads_ * headDim_);
    std::vector<float> K((size_t)seqLen * numKVHeads_ * headDim_);
    std::vector<float> V((size_t)seqLen * numKVHeads_ * headDim_);

    for (uint32_t s = 0; s < seqLen; ++s) {
        for (uint32_t h = 0; h < numHeads_; ++h) {
            for (uint32_t d = 0; d < headDim_; ++d) {
                float sum = 0.0f;
                for (uint32_t i = 0; i < hiddenDim_; ++i) {
                    sum += input[(size_t)s * hiddenDim_ + i] *
                           qWeight_[(size_t)i * numHeads_ * headDim_ + h * headDim_ + d];
                }
                Q[(size_t)s * numHeads_ * headDim_ + h * headDim_ + d] = sum;
            }
        }
    }

    for (uint32_t s = 0; s < seqLen; ++s) {
        for (uint32_t h = 0; h < numKVHeads_; ++h) {
            for (uint32_t d = 0; d < headDim_; ++d) {
                float kSum = 0.0f, vSum = 0.0f;
                for (uint32_t i = 0; i < hiddenDim_; ++i) {
                    kSum += input[(size_t)s * hiddenDim_ + i] *
                            kWeight_[(size_t)i * numKVHeads_ * headDim_ + h * headDim_ + d];
                    vSum += input[(size_t)s * hiddenDim_ + i] *
                            vWeight_[(size_t)i * numKVHeads_ * headDim_ + h * headDim_ + d];
                }
                K[(size_t)s * numKVHeads_ * headDim_ + h * headDim_ + d] = kSum;
                V[(size_t)s * numKVHeads_ * headDim_ + h * headDim_ + d] = vSum;
            }
        }
    }

    applyRoPE(Q.data(), K.data(), seqLen);

    float scale = 1.0f / std::sqrt(static_cast<float>(headDim_));
    (void)scale;

    for (uint32_t s = 0; s < seqLen; ++s) {
        for (uint32_t h = 0; h < numHeads_; ++h) {
            uint32_t kvHead = h / headPerKV_;
            (void)kvHead;
            std::vector<float> out(headDim_, 0.0f);
            flashAttention(
                Q.data() + (size_t)s * numHeads_ * headDim_ + h * headDim_,
                kvCacheK + (size_t)layerIdx * kvLen * numKVHeads_ * headDim_,
                kvCacheV + (size_t)layerIdx * kvLen * numKVHeads_ * headDim_,
                out.data(),
                kvLen,
                s,
                layerIdx
            );
            for (uint32_t d = 0; d < headDim_; ++d) {
                output[(size_t)s * hiddenDim_ + h * headDim_ + d] = out[d];
            }
        }
    }

    std::vector<float> projOut((size_t)seqLen * hiddenDim_, 0.0f);
    for (uint32_t s = 0; s < seqLen; ++s) {
        for (uint32_t i = 0; i < hiddenDim_; ++i) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < numHeads_ * headDim_; ++j) {
                sum += output[(size_t)s * numHeads_ * headDim_ + j] * oWeight_[(size_t)j * hiddenDim_ + i];
            }
            projOut[(size_t)s * hiddenDim_ + i] = sum;
        }
    }
    std::copy(projOut.begin(), projOut.end(), output);
}

inline void AttentionLayer::applyRoPE(float* q, float* k, uint32_t seqLen, uint32_t positionOffset) {
    for (uint32_t s = 0; s < seqLen; ++s) {
        uint32_t pos = s + positionOffset;
        for (uint32_t h = 0; h < numHeads_; ++h) {
            for (uint32_t i = 0; i < headDim_ / 2; ++i) {
                float freq = ropeFreqs_[(size_t)pos * headDim_ / 2 + i];
                float cosFreq = std::cos(freq);
                float sinFreq = std::sin(freq);
                size_t idx = (size_t)s * numHeads_ * headDim_ + h * headDim_ + i * 2;
                float q0 = q[idx];
                float q1 = q[idx + 1];
                q[idx] = q0 * cosFreq - q1 * sinFreq;
                q[idx + 1] = q0 * sinFreq + q1 * cosFreq;
            }
        }
        for (uint32_t h = 0; h < numKVHeads_; ++h) {
            for (uint32_t i = 0; i < headDim_ / 2; ++i) {
                float freq = ropeFreqs_[(size_t)pos * headDim_ / 2 + i];
                float cosFreq = std::cos(freq);
                float sinFreq = std::sin(freq);
                size_t idx = (size_t)s * numKVHeads_ * headDim_ + h * headDim_ + i * 2;
                float k0 = k[idx];
                float k1 = k[idx + 1];
                k[idx] = k0 * cosFreq - k1 * sinFreq;
                k[idx + 1] = k0 * sinFreq + k1 * cosFreq;
            }
        }
    }
}

inline void AttentionLayer::flashAttention(
    const float* q,
    const float* k,
    const float* v,
    float* output,
    uint32_t kvLen,
    uint32_t position,
    uint32_t /*layerIdx*/
) {
    float scale = 1.0f / std::sqrt(static_cast<float>(headDim_));
    std::fill(output, output + headDim_, 0.0f);
    float maxScore = -INFINITY;
    float sumExp = 0.0f;

    uint32_t start = 0;
    uint32_t end = kvLen;
    if (slidingWindow_ > 0) {
        start = (position > slidingWindow_) ? (position - slidingWindow_) : 0;
        end = std::min(position + 1, kvLen);
    }

    for (uint32_t blockStart = start; blockStart < end; blockStart += FLASH_BLOCK_SIZE) {
        uint32_t blockEnd = std::min(blockStart + FLASH_BLOCK_SIZE, end);
        std::vector<float> scores(blockEnd - blockStart);

        for (uint32_t j = blockStart; j < blockEnd; ++j) {
            float score = 0.0f;
            for (uint32_t d = 0; d < headDim_; ++d) {
                score += q[d] * k[(size_t)j * numKVHeads_ * headDim_ + d];
            }
            scores[j - blockStart] = score * scale;
        }

        float blockMax = *std::max_element(scores.begin(), scores.end());
        float newMax = std::max(maxScore, blockMax);
        if (maxScore > -INFINITY) {
            sumExp *= std::exp(maxScore - newMax);
        }
        for (size_t i = 0; i < scores.size(); ++i) {
            scores[i] = std::exp(scores[i] - newMax);
            sumExp += scores[i];
        }
        maxScore = newMax;

        for (uint32_t j = blockStart; j < blockEnd; ++j) {
            float weight = scores[j - blockStart];
            for (uint32_t d = 0; d < headDim_; ++d) {
                output[d] += weight * v[(size_t)j * numKVHeads_ * headDim_ + d];
            }
        }
    }

    if (sumExp > 0) {
        for (uint32_t d = 0; d < headDim_; ++d) {
            output[d] /= sumExp;
        }
    }
}

inline void AttentionLayer::ringAttention(
    const float* q,
    float* kBlock,
    float* vBlock,
    float* output,
    float* maxScore,
    float* sumExp,
    uint32_t blockSize
) {
    float scale = 1.0f / std::sqrt(static_cast<float>(headDim_));
    std::vector<float> scores(blockSize);
    for (uint32_t j = 0; j < blockSize; ++j) {
        float score = 0.0f;
        for (uint32_t d = 0; d < headDim_; ++d) {
            score += q[d] * kBlock[(size_t)j * headDim_ + d];
        }
        scores[j] = score * scale;
    }

    float blockMax = *std::max_element(scores.begin(), scores.end());
    float newMax = std::max(*maxScore, blockMax);
    if (*maxScore > -INFINITY) {
        *sumExp *= std::exp(*maxScore - newMax);
    }
    float blockSum = 0.0f;
    for (size_t i = 0; i < scores.size(); ++i) {
        scores[i] = std::exp(scores[i] - newMax);
        blockSum += scores[i];
    }
    *maxScore = newMax;
    *sumExp += blockSum;

    for (uint32_t j = 0; j < blockSize; ++j) {
        for (uint32_t d = 0; d < headDim_; ++d) {
            output[d] += scores[j] * vBlock[(size_t)j * headDim_ + d];
        }
    }
}

inline void AttentionLayer::updateKVCache(float* kvCacheK, float* kvCacheV, uint32_t position) {
    (void)kvCacheK;
    (void)kvCacheV;
    (void)position;
}

} // namespace Prometheus
