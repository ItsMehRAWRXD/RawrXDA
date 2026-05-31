#include "temporal_relevance_decay_memory.h"

#include <algorithm>
#include <cmath>

namespace RawrXD::Memory {

TemporalRelevanceDecayMemory::TemporalRelevanceDecayMemory(float downgradeThreshold)
    : m_downgradeThreshold(std::clamp(downgradeThreshold, 0.01f, 0.95f)) {}

void TemporalRelevanceDecayMemory::upsert(
    uint64_t tokenId,
    float attentionScore,
    float lambda,
    size_t bytes,
    uint64_t step) {
    std::lock_guard<std::mutex> lock(m_mutex);

    TRDMTokenState& s = m_tokens[tokenId];
    s.tokenId = tokenId;
    s.attentionScore = std::clamp(attentionScore, 0.0f, 1.0f);
    s.lambda = std::clamp(lambda, 0.0001f, 2.0f);
    s.lastTouchStep = step;
    s.bytes = bytes;

    if (s.decayScore <= 0.0f) {
        s.decayScore = s.attentionScore;
    }

    s.precision = nextPrecision(s.decayScore);
}

void TemporalRelevanceDecayMemory::touch(uint64_t tokenId, float attentionScore, uint64_t step) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tokens.find(tokenId);
    if (it == m_tokens.end()) {
        return;
    }

    it->second.attentionScore = std::clamp(attentionScore, 0.0f, 1.0f);
    it->second.lastTouchStep = step;
    it->second.decayScore = std::max(it->second.decayScore, it->second.attentionScore);
    it->second.precision = nextPrecision(it->second.decayScore);
}

std::vector<uint64_t> TemporalRelevanceDecayMemory::decayStep(uint64_t step) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint64_t> downgraded;
    for (auto& [id, s] : m_tokens) {
        const uint64_t dt = (step > s.lastTouchStep) ? (step - s.lastTouchStep) : 0;
        const float decay = std::exp(-s.lambda * static_cast<float>(dt)) * s.attentionScore;
        s.decayScore = std::clamp(decay, 0.0f, 1.0f);

        const TRDMPrecision oldP = s.precision;
        s.precision = nextPrecision(s.decayScore);
        if (s.precision != oldP) {
            downgraded.push_back(id);
        }
    }
    return downgraded;
}

bool TemporalRelevanceDecayMemory::get(uint64_t tokenId, TRDMTokenState& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tokens.find(tokenId);
    if (it == m_tokens.end()) {
        return false;
    }

    out = it->second;
    return true;
}

void TemporalRelevanceDecayMemory::setDowngradeThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_downgradeThreshold = std::clamp(threshold, 0.01f, 0.95f);
}

TRDMPrecision TemporalRelevanceDecayMemory::nextPrecision(float score) const {
    if (score >= m_downgradeThreshold) {
        return TRDMPrecision::FP16;
    }
    if (score >= m_downgradeThreshold * 0.5f) {
        return TRDMPrecision::INT8;
    }
    return TRDMPrecision::BITMASK;
}

} // namespace RawrXD::Memory
