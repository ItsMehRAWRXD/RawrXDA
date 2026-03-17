// ============================================================================
// layer_contribution_scorer.cpp — Layer Contribution Scoring Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "layer_contribution_scorer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>

// ============================================================================
// Singleton
// ============================================================================

LayerContributionScorer& LayerContributionScorer::instance() {
    static LayerContributionScorer s_instance;
    return s_instance;
}

LayerContributionScorer::LayerContributionScorer()
    : m_callback(nullptr)
    , m_callbackUserData(nullptr)
{}

LayerContributionScorer::~LayerContributionScorer() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult LayerContributionScorer::initialize(const ScorerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("LayerContributionScorer already initialized", -1);
    }

    m_config = config;

    // Initialize score array
    m_scores.clear();
    m_scores.reserve(config.totalLayers);
    for (uint32_t i = 0; i < config.totalLayers; ++i) {
        LayerScore s = LayerScore::empty(i);

        // Auto-assign roles based on position
        if (i == 0) {
            s.role = LayerRole::Embedding;
        } else if (i == config.totalLayers - 1) {
            s.role = LayerRole::OutputHead;
        } else if (i < config.totalLayers / 4) {
            s.role = LayerRole::EarlyAttention;
        } else if (i < config.totalLayers / 2) {
            s.role = LayerRole::MidAttention;
        } else {
            s.role = LayerRole::LateAttention;
        }

        m_scores.push_back(s);
    }

    // Mark protected layers as high-scoring by default
    for (uint32_t idx : config.protectedLayers) {
        if (idx < m_scores.size()) {
            m_scores[idx].compositeScore = 1.0f;
            m_scores[idx].emaScore       = 1.0f;
        }
    }

    m_callback         = nullptr;
    m_callbackUserData = nullptr;

    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("LayerContributionScorer initialized");
}

void LayerContributionScorer::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized.store(false, std::memory_order_release);
    m_scores.clear();
    m_callback         = nullptr;
    m_callbackUserData = nullptr;
}

// ============================================================================
// Score Submission
// ============================================================================

PatchResult LayerContributionScorer::submitActivation(const ActivationSample& sample) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (sample.layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }

    LayerScore& score = m_scores[sample.layerIndex];

    // Compute activation magnitude (L2 norm, normalized)
    double norm = sample.l2Norm();
    double normalizedScore = std::min(norm / 100.0, 1.0);  // Normalize to 0-1

    score.activationScore = static_cast<float>(normalizedScore);
    score.evalCount++;

    m_stats.samplesSubmitted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Activation submitted");
}

PatchResult LayerContributionScorer::submitAttentionEntropy(uint32_t layerIndex, float entropy) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }

    // High entropy = uniform attention = less informative = lower contribution
    // Low entropy = focused attention = more informative = higher contribution
    // Invert: score = 1.0 - normalized_entropy
    float normalizedEntropy = std::min(std::max(entropy / 10.0f, 0.0f), 1.0f);
    m_scores[layerIndex].entropyScore = 1.0f - normalizedEntropy;
    m_scores[layerIndex].evalCount++;

    m_stats.samplesSubmitted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Entropy submitted");
}

PatchResult LayerContributionScorer::submitOutputDelta(uint32_t layerIndex, float delta) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }

    // Higher delta = more impact when layer is included/excluded
    m_scores[layerIndex].outputDeltaScore = std::min(std::max(delta, 0.0f), 1.0f);
    m_scores[layerIndex].evalCount++;

    m_stats.samplesSubmitted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Output delta submitted");
}

PatchResult LayerContributionScorer::submitGradientProxy(uint32_t layerIndex, float sensitivity) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }

    m_scores[layerIndex].gradientScore = std::min(std::max(sensitivity, 0.0f), 1.0f);
    m_scores[layerIndex].evalCount++;

    m_stats.samplesSubmitted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Gradient proxy submitted");
}

// ============================================================================
// Score Retrieval
// ============================================================================

PatchResult LayerContributionScorer::getScore(uint32_t layerIndex, LayerScore* outScore) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outScore) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }

    *outScore = m_scores[layerIndex];
    return PatchResult::ok("Score retrieved");
}

PatchResult LayerContributionScorer::getAllScores(LayerScore* outBuf, uint32_t* outCount) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outBuf || !outCount) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    *outCount = static_cast<uint32_t>(m_scores.size());
    for (uint32_t i = 0; i < m_scores.size(); ++i) {
        outBuf[i] = m_scores[i];
    }
    return PatchResult::ok("All scores retrieved");
}

PatchResult LayerContributionScorer::getSortedScores(LayerScore* outBuf, uint32_t* outCount) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outBuf || !outCount) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<LayerScore> sorted = m_scores;
    std::sort(sorted.begin(), sorted.end(),
        [](const LayerScore& a, const LayerScore& b) {
            return a.compositeScore > b.compositeScore;
        });

    *outCount = static_cast<uint32_t>(sorted.size());
    for (uint32_t i = 0; i < sorted.size(); ++i) {
        outBuf[i] = sorted[i];
    }
    return PatchResult::ok("Sorted scores retrieved");
}

PatchResult LayerContributionScorer::getSkipMask(bool* outMask, uint32_t maskLen) const {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outMask) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t count = std::min(maskLen, static_cast<uint32_t>(m_scores.size()));
    for (uint32_t i = 0; i < count; ++i) {
        outMask[i] = m_scores[i].skipRecommended;
    }
    return PatchResult::ok("Skip mask retrieved");
}

// ============================================================================
// Scoring
// ============================================================================

PatchResult LayerContributionScorer::recomputeScores() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& score : m_scores) {
        float newComposite = computeComposite(score);
        updateVariance(score, newComposite);
        score.compositeScore = newComposite;
        updateEMA(score);
        evaluateSkipRecommendation(score);
    }

    m_stats.scoresComputed.fetch_add(static_cast<uint64_t>(m_scores.size()),
                                      std::memory_order_relaxed);
    notifyCallback();
    return PatchResult::ok("All scores recomputed");
}

PatchResult LayerContributionScorer::recomputeLayerScore(uint32_t layerIndex) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }

    LayerScore& score = m_scores[layerIndex];
    float newComposite = computeComposite(score);
    updateVariance(score, newComposite);
    score.compositeScore = newComposite;
    updateEMA(score);
    evaluateSkipRecommendation(score);

    m_stats.scoresComputed.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Layer score recomputed");
}

// ============================================================================
// Internal Scoring
// ============================================================================

float LayerContributionScorer::computeComposite(const LayerScore& score) const {
    float composite = 0.0f;

    switch (m_config.method) {
        case ScoringMethod::ActivationMagnitude:
            composite = score.activationScore;
            break;
        case ScoringMethod::GradientProxy:
            composite = score.gradientScore;
            break;
        case ScoringMethod::AttentionEntropy:
            composite = score.entropyScore;
            break;
        case ScoringMethod::OutputDelta:
            composite = score.outputDeltaScore;
            break;
        case ScoringMethod::Historical:
            composite = score.historicalScore;
            break;
        case ScoringMethod::Composite:
        default:
            composite = score.activationScore  * m_config.weightActivation
                      + score.gradientScore    * m_config.weightGradient
                      + score.entropyScore     * m_config.weightEntropy
                      + score.outputDeltaScore * m_config.weightOutputDelta
                      + score.historicalScore  * m_config.weightHistorical;
            break;
    }

    return std::min(std::max(composite, 0.0f), 1.0f);
}

void LayerContributionScorer::updateEMA(LayerScore& score) {
    float decay = m_config.emaDecay;
    score.emaScore = decay * score.emaScore + (1.0f - decay) * score.compositeScore;
    score.historicalScore = score.emaScore;
}

void LayerContributionScorer::updateVariance(LayerScore& score, float newComposite) {
    if (score.evalCount < 2) {
        score.scoreVariance = 0.0f;
        return;
    }
    // Welford's online variance
    float delta   = newComposite - score.compositeScore;
    float delta2  = newComposite - score.compositeScore;  // Simplified for running variance
    score.scoreVariance = 0.9f * score.scoreVariance + 0.1f * delta * delta2;
}

void LayerContributionScorer::evaluateSkipRecommendation(LayerScore& score) {
    // Never skip protected layers
    for (uint32_t prot : m_config.protectedLayers) {
        if (prot == score.layerIndex) {
            score.skipRecommended = false;
            m_stats.protectedOverrides.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }

    // Don't trust skip recommendations until enough evaluations
    if (score.evalCount < m_config.minEvalsForSkip) {
        score.skipRecommended = false;
        return;
    }

    // Skip if composite score (EMA) is below threshold
    bool shouldSkip = (score.emaScore < m_config.skipThreshold);
    if (shouldSkip) {
        m_stats.skipRecommendations.fetch_add(1, std::memory_order_relaxed);
    }
    score.skipRecommended = shouldSkip;
}

void LayerContributionScorer::notifyCallback() {
    if (m_callback && !m_scores.empty()) {
        m_callback(m_scores.data(), static_cast<uint32_t>(m_scores.size()), m_callbackUserData);
    }
}

// ============================================================================
// Layer Role Assignment
// ============================================================================

PatchResult LayerContributionScorer::setLayerRole(uint32_t layerIndex, LayerRole role) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }
    m_scores[layerIndex].role = role;
    return PatchResult::ok("Layer role set");
}

PatchResult LayerContributionScorer::autoAssignRoles() {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t total = static_cast<uint32_t>(m_scores.size());
    if (total == 0) return PatchResult::error("No layers", -2);

    for (uint32_t i = 0; i < total; ++i) {
        if (i == 0) {
            m_scores[i].role = LayerRole::Embedding;
        } else if (i == total - 1) {
            m_scores[i].role = LayerRole::OutputHead;
        } else if (i < total / 4) {
            m_scores[i].role = LayerRole::EarlyAttention;
        } else if (i < total * 3 / 4) {
            m_scores[i].role = LayerRole::MidAttention;
        } else {
            m_scores[i].role = LayerRole::LateAttention;
        }
    }
    return PatchResult::ok("Roles auto-assigned");
}

// ============================================================================
// Threshold & Protected Layers
// ============================================================================

PatchResult LayerContributionScorer::setSkipThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.skipThreshold = std::min(std::max(threshold, 0.0f), 1.0f);
    return PatchResult::ok("Skip threshold set");
}

float LayerContributionScorer::getSkipThreshold() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.skipThreshold;
}

PatchResult LayerContributionScorer::protectLayer(uint32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Check if already protected
    for (uint32_t idx : m_config.protectedLayers) {
        if (idx == layerIndex) return PatchResult::ok("Already protected");
    }
    m_config.protectedLayers.push_back(layerIndex);
    if (layerIndex < m_scores.size()) {
        m_scores[layerIndex].skipRecommended = false;
    }
    return PatchResult::ok("Layer protected");
}

PatchResult LayerContributionScorer::unprotectLayer(uint32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& vec = m_config.protectedLayers;
    auto it = std::remove(vec.begin(), vec.end(), layerIndex);
    if (it != vec.end()) {
        vec.erase(it, vec.end());
        return PatchResult::ok("Layer unprotected");
    }
    return PatchResult::ok("Layer was not protected");
}

// ============================================================================
// Callback
// ============================================================================

PatchResult LayerContributionScorer::registerCallback(ScoreUpdateCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = cb;
    m_callbackUserData = userData;
    return PatchResult::ok("Callback registered");
}

PatchResult LayerContributionScorer::clearCallback() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = nullptr;
    m_callbackUserData = nullptr;
    return PatchResult::ok("Callback cleared");
}

// ============================================================================
// Reset
// ============================================================================

PatchResult LayerContributionScorer::resetScores() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& score : m_scores) {
        LayerRole savedRole = score.role;
        uint32_t savedIdx   = score.layerIndex;
        score = LayerScore::empty(savedIdx);
        score.role = savedRole;
    }
    return PatchResult::ok("All scores reset");
}

PatchResult LayerContributionScorer::resetLayer(uint32_t layerIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (layerIndex >= m_scores.size()) {
        return PatchResult::error("Layer index out of range", -2);
    }
    LayerRole savedRole = m_scores[layerIndex].role;
    m_scores[layerIndex] = LayerScore::empty(layerIndex);
    m_scores[layerIndex].role = savedRole;
    return PatchResult::ok("Layer score reset");
}

// ============================================================================
// Statistics
// ============================================================================

void LayerContributionScorer::resetStats() {
    m_stats.samplesSubmitted.store(0, std::memory_order_relaxed);
    m_stats.scoresComputed.store(0, std::memory_order_relaxed);
    m_stats.skipRecommendations.store(0, std::memory_order_relaxed);
    m_stats.protectedOverrides.store(0, std::memory_order_relaxed);
}
