// ============================================================================
// speculative_decoder_v2.cpp — Speculative Decoding Engine Implementation
// ============================================================================
// Draft-verify-accept loop with batch verification, adaptive draft length,
// and acceptance rate tracking. No exceptions.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "gpu/speculative_decoder_v2.h"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>

namespace RawrXD {
namespace Speculative {

// ============================================================================
// Constructor / Destructor
// ============================================================================

SpeculativeDecoderV2::SpeculativeDecoderV2() {
    m_stats = {};
    m_stats.currentDraftLen = 5;
}

SpeculativeDecoderV2::~SpeculativeDecoderV2() {
    abort();
}

SpeculativeDecoderV2& SpeculativeDecoderV2::Global() {
    static SpeculativeDecoderV2 instance;
    return instance;
}

// ============================================================================
// Model Setup
// ============================================================================

SpecResult SpeculativeDecoderV2::setDraftModel(const ModelInference& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!model.logprobs) {
        return SpecResult::error("Draft model must provide logprobs callback");
    }
    m_draftModel = model;
    m_draftReady = true;
    return SpecResult::ok("Draft model set");
}

SpecResult SpeculativeDecoderV2::setTargetModel(const ModelInference& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!model.logprobs) {
        return SpecResult::error("Target model must provide logprobs callback");
    }
    m_targetModel = model;
    m_targetReady = true;
    return SpecResult::ok("Target model set");
}

// ============================================================================
// Configuration
// ============================================================================

void SpeculativeDecoderV2::setConfig(const SpeculationConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_stats.currentDraftLen = config.maxDraftTokens;
}

// ============================================================================
// Draft
// ============================================================================

SpeculativeDecoderV2::DraftResult
SpeculativeDecoderV2::draft(const std::vector<int>& context, int numTokens) {
    DraftResult result;
    std::vector<int> currentCtx = context;

    for (int i = 0; i < numTokens && !m_abortRequested.load(); ++i) {
        auto logprobs = m_draftModel.logprobs(currentCtx, 1, m_draftModel.userData);
        if (logprobs.empty()) break;

        // Greedy: take top token
        int bestId = logprobs[0].first;
        float bestLogprob = logprobs[0].second;

        result.tokenIds.push_back(bestId);
        result.logprobs.push_back(bestLogprob);

        currentCtx.push_back(bestId);
    }

    return result;
}

// ============================================================================
// Verify
// ============================================================================

SpeculativeDecoderV2::VerifyResult
SpeculativeDecoderV2::verify(const std::vector<int>& context,
                              const DraftResult& drafted) {
    VerifyResult result;
    result.acceptedCount = 0;
    result.allAccepted = false;

    if (drafted.tokenIds.empty()) {
        // No draft tokens — just generate from target
        auto targetLogprobs = m_targetModel.logprobs(context, 1, m_targetModel.userData);
        if (!targetLogprobs.empty()) {
            result.correctionToken.id = targetLogprobs[0].first;
            result.correctionToken.logprob = targetLogprobs[0].second;
            if (m_targetModel.decode) {
                result.correctionToken.text = m_targetModel.decode(
                    targetLogprobs[0].first, m_targetModel.userData);
            }
        }
        return result;
    }

    // Build context with draft tokens appended, verify all at once
    // For each position, compare draft logprob vs target logprob
    std::vector<int> verifyCtx = context;

    // If batch logprobs available, use it
    if (m_targetModel.batchLogprobs) {
        // Build all intermediate contexts
        std::vector<std::vector<int>> contexts;
        for (size_t i = 0; i <= drafted.tokenIds.size(); ++i) {
            contexts.push_back(verifyCtx);
            if (i < drafted.tokenIds.size()) {
                verifyCtx.push_back(drafted.tokenIds[i]);
            }
        }

        auto batchResults = m_targetModel.batchLogprobs(
            contexts, 10, m_targetModel.userData);

        // Verify each draft token
        static thread_local std::mt19937 rng(std::random_device{}());

        for (size_t i = 0; i < drafted.tokenIds.size() && i < batchResults.size(); ++i) {
            const auto& targetProbs = batchResults[i];

            // Find target probability for draft token
            float targetLogprob = -100.0f;
            for (const auto& [tid, tlp] : targetProbs) {
                if (tid == drafted.tokenIds[i]) {
                    targetLogprob = tlp;
                    break;
                }
            }

            // Acceptance criterion: p_target(x) / p_draft(x) >= threshold
            float draftProb   = std::exp(drafted.logprobs[i]);
            float targetProb  = std::exp(targetLogprob);
            float ratio       = (draftProb > 0.0f) ? (targetProb / draftProb) : 0.0f;

            // Modified rejection sampling
            float u = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
            if (u < std::min(1.0f, ratio)) {
                result.acceptedCount++;
            } else {
                // Rejected — use target's top token as correction
                if (!targetProbs.empty()) {
                    result.correctionToken.id = targetProbs[0].first;
                    result.correctionToken.logprob = targetProbs[0].second;
                    if (m_targetModel.decode) {
                        result.correctionToken.text = m_targetModel.decode(
                            targetProbs[0].first, m_targetModel.userData);
                    }
                }
                return result;
            }
        }

        // All draft tokens accepted — get one more from target
        result.allAccepted = true;
        if (batchResults.size() > drafted.tokenIds.size()) {
            const auto& lastTargetProbs = batchResults.back();
            if (!lastTargetProbs.empty()) {
                result.correctionToken.id = lastTargetProbs[0].first;
                result.correctionToken.logprob = lastTargetProbs[0].second;
                if (m_targetModel.decode) {
                    result.correctionToken.text = m_targetModel.decode(
                        lastTargetProbs[0].first, m_targetModel.userData);
                }
            }
        }

        return result;
    }

    // Fallback: sequential verification
    static thread_local std::mt19937 rng(std::random_device{}());
    verifyCtx = context;

    for (size_t i = 0; i < drafted.tokenIds.size(); ++i) {
        auto targetLogprobs = m_targetModel.logprobs(verifyCtx, 10, m_targetModel.userData);
        if (targetLogprobs.empty()) break;

        // Find target probability for draft token
        float targetLogprob = -100.0f;
        for (const auto& [tid, tlp] : targetLogprobs) {
            if (tid == drafted.tokenIds[i]) {
                targetLogprob = tlp;
                break;
            }
        }

        float draftProb  = std::exp(drafted.logprobs[i]);
        float targetProb = std::exp(targetLogprob);
        float ratio      = (draftProb > 0.0f) ? (targetProb / draftProb) : 0.0f;

        float u = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
        if (u < std::min(1.0f, ratio)) {
            result.acceptedCount++;
            verifyCtx.push_back(drafted.tokenIds[i]);
        } else {
            // Use target's correction
            result.correctionToken.id = targetLogprobs[0].first;
            result.correctionToken.logprob = targetLogprobs[0].second;
            if (m_targetModel.decode) {
                result.correctionToken.text = m_targetModel.decode(
                    targetLogprobs[0].first, m_targetModel.userData);
            }
            return result;
        }
    }

    // All accepted — get one bonus token from target
    result.allAccepted = true;
    auto targetLogprobs = m_targetModel.logprobs(verifyCtx, 1, m_targetModel.userData);
    if (!targetLogprobs.empty()) {
        result.correctionToken.id = targetLogprobs[0].first;
        result.correctionToken.logprob = targetLogprobs[0].second;
        if (m_targetModel.decode) {
            result.correctionToken.text = m_targetModel.decode(
                targetLogprobs[0].first, m_targetModel.userData);
        }
    }

    return result;
}

// ============================================================================
// Adaptive Draft Length
// ============================================================================

void SpeculativeDecoderV2::adjustDraftLength() {
    if (!m_config.adaptiveDraftLen) return;

    float avgRate = m_acceptRateAvg.mean();

    // If acceptance rate is high, try longer drafts
    if (avgRate > 0.8f) {
        m_stats.currentDraftLen = std::min(
            m_config.maxDraftTokens + 3,
            m_stats.currentDraftLen + 1);
    }
    // If acceptance rate is low, shorten drafts
    else if (avgRate < 0.3f) {
        m_stats.currentDraftLen = std::max(
            m_config.minDraftTokens,
            m_stats.currentDraftLen - 1);
    }
}

// ============================================================================
// Stats Update
// ============================================================================

void SpeculativeDecoderV2::updateStats(int drafted, int accepted,
                                        float draftMs, float verifyMs) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats.totalDrafted  += drafted;
    m_stats.totalAccepted += accepted;
    m_stats.totalRejected += (drafted - accepted);
    m_stats.totalVerified++;

    if (m_stats.totalDrafted > 0) {
        m_stats.acceptanceRate = static_cast<float>(m_stats.totalAccepted) /
                                 static_cast<float>(m_stats.totalDrafted);
    }

    m_stats.avgDraftLatencyMs  = m_draftLatencyAvg.mean();
    m_stats.avgVerifyLatencyMs = m_verifyLatencyAvg.mean();

    // Speedup = tokens generated / target-model-only-time
    float totalTime = draftMs + verifyMs;
    float tokensGenerated = static_cast<float>(accepted + 1); // +1 for correction
    if (verifyMs > 0) {
        m_stats.speedupRatio = tokensGenerated / (totalTime / verifyMs);
    }
}

// ============================================================================
// Generate
// ============================================================================

SpeculativeDecoderV2::GenerateResult
SpeculativeDecoderV2::generate(const std::vector<int>& promptTokens,
                                int maxNewTokens) {
    return generateStreaming(promptTokens, maxNewTokens, nullptr, nullptr);
}

SpeculativeDecoderV2::GenerateResult
SpeculativeDecoderV2::generateStreaming(const std::vector<int>& promptTokens,
                                        int maxNewTokens,
                                        TokenCallback callback,
                                        void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_draftReady || !m_targetReady) {
        return GenerateResult::error("Models not set");
    }

    m_generating.store(true);
    m_abortRequested.store(false);

    std::vector<Token> outputTokens;
    std::vector<int> context = promptTokens;
    int generated = 0;

    while (generated < maxNewTokens && !m_abortRequested.load()) {
        int draftLen = m_stats.currentDraftLen;

        // ---- Draft Phase ----
        auto draftStart = std::chrono::high_resolution_clock::now();
        auto drafted = draft(context, draftLen);
        auto draftEnd = std::chrono::high_resolution_clock::now();
        float draftMs = std::chrono::duration<float, std::milli>(
            draftEnd - draftStart).count();
        m_draftLatencyAvg.push(draftMs);

        // ---- Verify Phase ----
        auto verifyStart = std::chrono::high_resolution_clock::now();
        auto verified = verify(context, drafted);
        auto verifyEnd = std::chrono::high_resolution_clock::now();
        float verifyMs = std::chrono::duration<float, std::milli>(
            verifyEnd - verifyStart).count();
        m_verifyLatencyAvg.push(verifyMs);

        // ---- Accept Phase ----
        // Add accepted draft tokens
        for (int i = 0; i < verified.acceptedCount && generated < maxNewTokens; ++i) {
            Token tok;
            tok.id = drafted.tokenIds[i];
            tok.logprob = drafted.logprobs[i];
            if (m_draftModel.decode) {
                tok.text = m_draftModel.decode(tok.id, m_draftModel.userData);
            }

            outputTokens.push_back(tok);
            context.push_back(tok.id);
            generated++;

            if (callback) {
                callback(tok, false, userData);
            }
        }

        // Add correction/bonus token from target
        if (generated < maxNewTokens && verified.correctionToken.id != 0) {
            outputTokens.push_back(verified.correctionToken);
            context.push_back(verified.correctionToken.id);
            generated++;

            if (callback) {
                callback(verified.correctionToken, false, userData);
            }
        }

        // Update stats
        m_acceptRateAvg.push(drafted.tokenIds.empty() ? 0.0f :
            static_cast<float>(verified.acceptedCount) /
            static_cast<float>(drafted.tokenIds.size()));

        updateStats(static_cast<int>(drafted.tokenIds.size()),
                    verified.acceptedCount, draftMs, verifyMs);

        // Adjust draft length adaptively
        adjustDraftLength();

        // Check for EOS (token id 0 or special stop tokens)
        if (!outputTokens.empty() && outputTokens.back().id == 0) {
            break;
        }
    }

    m_generating.store(false);

    AcceptanceStats finalStats;
    {
        std::lock_guard<std::mutex> slock(m_statsMutex);
        finalStats = m_stats;
    }

    // Calculate tokens per second
    auto totalTime = m_draftLatencyAvg.mean() + m_verifyLatencyAvg.mean();
    if (totalTime > 0) {
        finalStats.tokensPerSecond = static_cast<float>(generated) /
            (totalTime * m_stats.totalVerified / 1000.0f);
    }

    return GenerateResult::ok(std::move(outputTokens), finalStats);
}

SpeculativeDecoderV2::GenerateResult
SpeculativeDecoderV2::generateFromText(const std::string& prompt,
                                        int maxNewTokens) {
    if (!m_targetModel.encode) {
        return GenerateResult::error("Target model must provide encode callback");
    }

    auto tokens = m_targetModel.encode(prompt, m_targetModel.userData);
    return generate(tokens, maxNewTokens);
}

// ============================================================================
// Statistics
// ============================================================================

AcceptanceStats SpeculativeDecoderV2::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void SpeculativeDecoderV2::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = {};
    m_stats.currentDraftLen = m_config.maxDraftTokens;
    m_acceptRateAvg = RunningAverage{};
    m_draftLatencyAvg = RunningAverage{};
    m_verifyLatencyAvg = RunningAverage{};
}

// ============================================================================
// Control
// ============================================================================

void SpeculativeDecoderV2::abort() {
    m_abortRequested.store(true);
}

} // namespace Speculative
} // namespace RawrXD
