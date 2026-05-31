#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

enum class TRDMPrecision : uint8_t {
    FP16 = 0,
    INT8 = 1,
    BITMASK = 2
};

struct TRDMTokenState {
    uint64_t tokenId = 0;
    float decayScore = 1.0f;
    float attentionScore = 0.0f;
    float lambda = 0.05f;
    uint64_t lastTouchStep = 0;
    TRDMPrecision precision = TRDMPrecision::FP16;
    size_t bytes = 0;
};

class TemporalRelevanceDecayMemory {
public:
    explicit TemporalRelevanceDecayMemory(float downgradeThreshold = 0.25f);

    void upsert(uint64_t tokenId, float attentionScore, float lambda, size_t bytes, uint64_t step);
    void touch(uint64_t tokenId, float attentionScore, uint64_t step);

    // Applies decay = exp(-lambda * dt) * attention_score and updates precision ladder.
    std::vector<uint64_t> decayStep(uint64_t step);

    bool get(uint64_t tokenId, TRDMTokenState& out) const;
    void setDowngradeThreshold(float threshold);

private:
    TRDMPrecision nextPrecision(float score) const;

    mutable std::mutex m_mutex;
    float m_downgradeThreshold;
    std::unordered_map<uint64_t, TRDMTokenState> m_tokens;
};

} // namespace RawrXD::Memory
