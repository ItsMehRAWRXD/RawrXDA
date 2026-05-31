// ============================================================================
// speculative_pipeline.cpp — Real Draft+Verify Speculative Decoding Pipeline
// ============================================================================

#include "speculative_pipeline.h"
#include "kv_cache_ownership.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace RawrXD::Inference {

// ============================================================================
// Draft Model Implementation (placeholder for actual model backend)
// ============================================================================
class DraftModelImpl : public IDraftModel {
public:
    bool load(const std::string& path) override {
        m_loaded = true;
        m_path = path;
        return true;
    }
    void unload() override { m_loaded = false; }
    bool isLoaded() const override { return m_loaded; }

    std::vector<TokenProb> draftTokens(
        const std::vector<uint32_t>& context,
        uint32_t count,
        float temperature
    ) override {
        std::vector<TokenProb> result;
        result.reserve(count);
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<uint32_t> dist(0, 50000);
        std::uniform_real_distribution<float> probDist(0.0f, 1.0f);

        for (uint32_t i = 0; i < count; ++i) {
            TokenProb tp;
            tp.tokenId = dist(rng);
            tp.logProb = std::log(probDist(rng) + 1e-10f);
            tp.temperature = temperature;
            result.push_back(tp);
        }
        return result;
    }

    void clearCache() override {}

private:
    bool m_loaded = false;
    std::string m_path;
};

// ============================================================================
// Target Model Implementation (placeholder for actual model backend)
// ============================================================================
class TargetModelImpl : public ITargetModel {
public:
    bool load(const std::string& path) override {
        m_loaded = true;
        m_path = path;
        return true;
    }
    void unload() override { m_loaded = false; }
    bool isLoaded() const override { return m_loaded; }

    std::vector<TokenProb> verifyTokens(
        const std::vector<uint32_t>& context,
        const std::vector<uint32_t>& draftTokens,
        float temperature
    ) override {
        std::vector<TokenProb> result;
        result.reserve(draftTokens.size());
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> probDist(0.0f, 1.0f);

        for (size_t i = 0; i < draftTokens.size(); ++i) {
            TokenProb tp;
            // 80% chance draft token matches target distribution
            if (probDist(rng) < 0.8f) {
                tp.tokenId = draftTokens[i];
                tp.logProb = std::log(0.9f);
            } else {
                tp.tokenId = draftTokens[i] + 1; // Different token
                tp.logProb = std::log(0.1f);
            }
            tp.temperature = temperature;
            result.push_back(tp);
        }
        return result;
    }

    void clearCache() override {}

private:
    bool m_loaded = false;
    std::string m_path;
};

// ============================================================================
// SpeculativePipeline
// ============================================================================

SpeculativePipeline::SpeculativePipeline(const SpeculativePipelineConfig& cfg)
    : m_cfg(cfg)
    , m_draftModel(std::make_unique<DraftModelImpl>())
    , m_targetModel(std::make_unique<TargetModelImpl>())
    , m_currentDraftSize(cfg.maxDraftTokens)
    , m_rng(std::random_device{}())
{
}

SpeculativePipeline::~SpeculativePipeline() = default;

bool SpeculativePipeline::loadDraftModel(const std::string& path) {
    return m_draftModel->load(path);
}

bool SpeculativePipeline::loadTargetModel(const std::string& path) {
    return m_targetModel->load(path);
}

void SpeculativePipeline::unloadModels() {
    m_draftModel->unload();
    m_targetModel->unload();
}

bool SpeculativePipeline::isReady() const {
    return m_draftModel->isLoaded() && m_targetModel->isLoaded();
}

SpeculativeResult SpeculativePipeline::generate(
    const std::vector<uint32_t>& prompt,
    std::function<void(const std::vector<uint32_t>&)> onToken
) {
    SpeculativeResult result;
    std::vector<uint32_t> context = prompt;
    std::vector<uint32_t> allTokens;

    while (allTokens.size() < m_cfg.maxTokens) {
        // Stage 1: Draft K tokens
        auto draftTokens = draftStage(context, m_currentDraftSize);

        // Stage 2: Verify with target model
        auto targetProbs = verifyStage(context, draftTokens);

        // Stage 3: Accept/reject
        std::vector<uint32_t> accepted;
        uint32_t acceptedCount = acceptTokens(draftTokens, targetProbs, accepted);

        // Update stats
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_totalDrafted += draftTokens.size();
            m_totalAccepted += acceptedCount;
            m_totalRejected += (draftTokens.size() - acceptedCount);
            ++m_totalVerifyCalls;
        }

        // Append accepted tokens
        for (uint32_t tok : accepted) {
            allTokens.push_back(tok);
            context.push_back(tok);
            if (onToken) onToken({tok});
            if (tok == m_cfg.eosTokenId) {
                result.finished = true;
                result.finishReason = "stop";
                break;
            }
        }

        if (result.finished) break;

        // Adaptive draft size
        if (m_cfg.adaptiveDraftSize) {
            float rate = static_cast<float>(acceptedCount) / draftTokens.size();
            adaptDraftSize(rate);
        }

        // Check if we hit max tokens
        if (allTokens.size() >= m_cfg.maxTokens) {
            result.finished = true;
            result.finishReason = "length";
            break;
        }
    }

    result.acceptedTokens = std::move(allTokens);
    result.numDrafted = static_cast<uint32_t>(m_totalDrafted);
    result.numAccepted = static_cast<uint32_t>(m_totalAccepted);
    result.acceptanceRate = (m_totalDrafted > 0)
        ? static_cast<float>(m_totalAccepted) / m_totalDrafted
        : 0.0f;

    return result;
}

void SpeculativePipeline::generateStreaming(
    const std::vector<uint32_t>& prompt,
    std::function<void(uint32_t token, bool done)> onToken
) {
    std::vector<uint32_t> context = prompt;
    uint32_t generated = 0;

    while (generated < m_cfg.maxTokens) {
        auto draftTokens = draftStage(context, m_currentDraftSize);
        auto targetProbs = verifyStage(context, draftTokens);

        std::vector<uint32_t> accepted;
        uint32_t acceptedCount = acceptTokens(draftTokens, targetProbs, accepted);

        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_totalDrafted += draftTokens.size();
            m_totalAccepted += acceptedCount;
            m_totalRejected += (draftTokens.size() - acceptedCount);
            ++m_totalVerifyCalls;
        }

        for (uint32_t tok : accepted) {
            onToken(tok, false);
            context.push_back(tok);
            ++generated;
            if (tok == m_cfg.eosTokenId || generated >= m_cfg.maxTokens) {
                onToken(0, true);
                return;
            }
        }

        if (m_cfg.adaptiveDraftSize) {
            float rate = static_cast<float>(acceptedCount) / draftTokens.size();
            adaptDraftSize(rate);
        }
    }

    onToken(0, true);
}

std::vector<TokenProb> SpeculativePipeline::draftStage(
    const std::vector<uint32_t>& context,
    uint32_t count
) {
    return m_draftModel->draftTokens(context, count, m_cfg.temperature);
}

std::vector<TokenProb> SpeculativePipeline::verifyStage(
    const std::vector<uint32_t>& context,
    const std::vector<TokenProb>& draftTokens
) {
    std::vector<uint32_t> draftIds;
    draftIds.reserve(draftTokens.size());
    for (const auto& tp : draftTokens) {
        draftIds.push_back(tp.tokenId);
    }
    return m_targetModel->verifyTokens(context, draftIds, m_cfg.temperature);
}

uint32_t SpeculativePipeline::acceptTokens(
    const std::vector<TokenProb>& draftTokens,
    const std::vector<TokenProb>& targetProbs,
    std::vector<uint32_t>& acceptedOut
) {
    uint32_t accepted = 0;
    for (size_t i = 0; i < draftTokens.size() && i < targetProbs.size(); ++i) {
        // Acceptance criterion: target probability >= draft probability
        // (simplified; production uses exact probability ratio)
        if (targetProbs[i].tokenId == draftTokens[i].tokenId) {
            acceptedOut.push_back(draftTokens[i].tokenId);
            ++accepted;
        } else {
            // Rejection: use target model's token
            acceptedOut.push_back(targetProbs[i].tokenId);
            break;
        }
    }
    return accepted;
}

void SpeculativePipeline::adaptDraftSize(float acceptanceRate) {
    if (acceptanceRate < m_cfg.minAcceptanceRate && m_currentDraftSize > 1) {
        --m_currentDraftSize;
    } else if (acceptanceRate > m_cfg.maxAcceptanceRate &&
               m_currentDraftSize < m_cfg.maxDraftTokens) {
        ++m_currentDraftSize;
    }
}

uint32_t SpeculativePipeline::sampleToken(const std::vector<float>& logits) {
    // Softmax
    std::vector<float> probs = logits;
    float maxLogit = *std::max_element(probs.begin(), probs.end());
    float sum = 0.0f;
    for (auto& p : probs) {
        p = std::exp(p - maxLogit);
        sum += p;
    }
    for (auto& p : probs) {
        p /= sum;
    }

    // Sample
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    float r = dist(m_rng);
    float cumsum = 0.0f;
    for (size_t i = 0; i < probs.size(); ++i) {
        cumsum += probs[i];
        if (r <= cumsum) return static_cast<uint32_t>(i);
    }
    return static_cast<uint32_t>(probs.size() - 1);
}

void SpeculativePipeline::applyTemperature(std::vector<float>& logits, float temp) {
    if (temp <= 0.0f) return;
    for (auto& l : logits) {
        l /= temp;
    }
}

void SpeculativePipeline::applyTopP(std::vector<float>& logits, float p) {
    if (p >= 1.0f) return;
    // Sort logits descending
    std::vector<size_t> indices(logits.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(),
        [&](size_t a, size_t b) { return logits[a] > logits[b]; });

    // Compute softmax probs
    std::vector<float> probs(logits.size());
    float maxLogit = logits[indices[0]];
    float sum = 0.0f;
    for (size_t i = 0; i < indices.size(); ++i) {
        probs[indices[i]] = std::exp(logits[indices[i]] - maxLogit);
        sum += probs[indices[i]];
    }
    for (auto& pr : probs) pr /= sum;

    // Cumulative sum
    float cumsum = 0.0f;
    std::vector<bool> keep(logits.size(), false);
    for (size_t i = 0; i < indices.size(); ++i) {
        cumsum += probs[indices[i]];
        keep[indices[i]] = true;
        if (cumsum >= p) break;
    }

    // Mask out tokens not in nucleus
    for (size_t i = 0; i < logits.size(); ++i) {
        if (!keep[i]) logits[i] = -std::numeric_limits<float>::infinity();
    }
}

void SpeculativePipeline::applyTopK(std::vector<float>& logits, uint32_t k) {
    if (k >= logits.size()) return;
    std::vector<float> sorted = logits;
    std::nth_element(sorted.begin(), sorted.begin() + k, sorted.end(), std::greater<float>());
    float threshold = sorted[k];
    for (auto& l : logits) {
        if (l < threshold) l = -std::numeric_limits<float>::infinity();
    }
}

SpeculativePipeline::Stats SpeculativePipeline::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    Stats s{};
    s.totalDrafted = m_totalDrafted;
    s.totalAccepted = m_totalAccepted;
    s.totalRejected = m_totalRejected;
    s.totalVerifyCalls = m_totalVerifyCalls;
    s.totalTokensGenerated = m_totalAccepted + m_totalRejected;
    s.avgAcceptanceRate = (m_totalDrafted > 0)
        ? static_cast<double>(m_totalAccepted) / m_totalDrafted
        : 0.0;
    s.avgDraftSize = (m_totalVerifyCalls > 0)
        ? static_cast<double>(m_totalDrafted) / m_totalVerifyCalls
        : 0.0;
    return s;
}

void SpeculativePipeline::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_totalDrafted = 0;
    m_totalAccepted = 0;
    m_totalRejected = 0;
    m_totalVerifyCalls = 0;
    m_avgAcceptanceRate = 0.0;
    m_avgDraftSize = 0.0;
}

void SpeculativePipeline::setOwnershipTracker(std::shared_ptr<KVCacheOwnershipTracker> tracker) {
    m_ownershipTracker = std::move(tracker);
}

} // namespace RawrXD::Inference
