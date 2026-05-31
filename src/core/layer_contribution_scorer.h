// ============================================================================
// layer_contribution_scorer.h — Layer Contribution Scoring System
// ============================================================================
// Tracks which transformer layers actually affect output deltas for a given
// query. This is the "intelligence" behind adaptive layer skipping: instead of
// uniformly skipping layers, we measure each layer's actual contribution to
// the output and skip only the ones that don't matter for THIS specific query.
//
// Scoring methods:
//   1. Activation magnitude: L2 norm of layer output delta
//   2. Gradient proxy: approximate layer sensitivity via perturbation
//   3. Attention entropy: high entropy = low information = safe to skip
//   4. Output delta: compare output with/without layer
//   5. Historical: EMA of past contribution for similar query types
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - No std::function — raw function pointers
//   - Thread-safe singleton
//   - Integrates with TraversalStrategy for real-time skip decisions
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef LAYER_CONTRIBUTION_SCORER_H
#define LAYER_CONTRIBUTION_SCORER_H

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>
#include <cmath>

// ============================================================================
// Enums
// ============================================================================

// ScoringMethod — How to evaluate a layer's contribution
enum class ScoringMethod : uint8_t {
    ActivationMagnitude = 0,  // L2 norm of activation delta
    GradientProxy       = 1,  // Perturbation-based sensitivity
    AttentionEntropy    = 2,  // Entropy of attention distribution
    OutputDelta         = 3,  // Direct output comparison
    Historical          = 4,  // EMA from past runs
    Composite           = 5   // Weighted blend of all methods
};

// LayerRole — Functional role of a layer in the transformer
enum class LayerRole : uint8_t {
    Embedding       = 0,   // Token embedding layer
    EarlyAttention  = 1,   // First quarter attention layers
    MidAttention    = 2,   // Middle attention layers
    LateAttention   = 3,   // Final quarter attention layers
    FFN             = 4,   // Feed-forward network
    Normalization   = 5,   // RMSNorm / LayerNorm
    OutputHead      = 6,   // Final output projection
    Unknown         = 7
};

// ============================================================================
// LayerScore — Score for a single layer
// ============================================================================
struct LayerScore {
    uint32_t    layerIndex;
    LayerRole   role;

    // Individual method scores (0.0 = no contribution, 1.0 = maximum)
    float       activationScore;
    float       gradientScore;
    float       entropyScore;
    float       outputDeltaScore;
    float       historicalScore;

    // Composite score (weighted blend)
    float       compositeScore;

    // How many times this layer has been evaluated
    uint32_t    evalCount;

    // Exponential moving average of composite score
    float       emaScore;

    // Variance of score (for confidence estimation)
    float       scoreVariance;

    // Whether this layer should be skipped at current thresholds
    bool        skipRecommended;

    static LayerScore empty(uint32_t idx) {
        LayerScore s;
        s.layerIndex       = idx;
        s.role             = LayerRole::Unknown;
        s.activationScore  = 0.0f;
        s.gradientScore    = 0.0f;
        s.entropyScore     = 0.0f;
        s.outputDeltaScore = 0.0f;
        s.historicalScore  = 0.0f;
        s.compositeScore   = 0.0f;
        s.evalCount        = 0;
        s.emaScore         = 0.0f;
        s.scoreVariance    = 0.0f;
        s.skipRecommended  = false;
        return s;
    }
};

// ============================================================================
// ActivationSample — Raw data from a layer activation
// ============================================================================
struct ActivationSample {
    uint32_t    layerIndex;
    const float* activations;        // Pointer to activation tensor (not owned)
    uint32_t    activationCount;     // Number of float elements
    uint64_t    timestampMs;

    // Compute L2 norm without owning memory
    double l2Norm() const {
        if (!activations || activationCount == 0) return 0.0;
        double sum = 0.0;
        for (uint32_t i = 0; i < activationCount; ++i) {
            double v = static_cast<double>(activations[i]);
            sum += v * v;
        }
        return std::sqrt(sum);
    }

    // Compute mean activation
    double mean() const {
        if (!activations || activationCount == 0) return 0.0;
        double sum = 0.0;
        for (uint32_t i = 0; i < activationCount; ++i) {
            sum += static_cast<double>(activations[i]);
        }
        return sum / activationCount;
    }
};

// ============================================================================
// ScorerConfig — Configuration for the scoring system
// ============================================================================
struct ScorerConfig {
    // Total number of layers in the model
    uint32_t    totalLayers          = 80;

    // Scoring method to use
    ScoringMethod method             = ScoringMethod::Composite;

    // Weights for composite scoring (must sum to ~1.0)
    float       weightActivation     = 0.25f;
    float       weightGradient       = 0.20f;
    float       weightEntropy        = 0.15f;
    float       weightOutputDelta    = 0.25f;
    float       weightHistorical     = 0.15f;

    // EMA decay factor (0.0 = no memory, 1.0 = infinite memory)
    float       emaDecay             = 0.9f;

    // Skip threshold: layers with composite score below this are skip-safe
    float       skipThreshold        = 0.15f;

    // Minimum evaluations before skip recommendation is trusted
    uint32_t    minEvalsForSkip      = 3;

    // Layers that should NEVER be skipped
    std::vector<uint32_t> protectedLayers;  // e.g., {0, 1, totalLayers-1}
};

// ============================================================================
// Scorer callback — invoked when scores are updated
// (function pointer, NOT std::function)
// ============================================================================
typedef void (*ScoreUpdateCallback)(
    const LayerScore* scores,
    uint32_t          scoreCount,
    void*             userData
);

// ============================================================================
// LayerContributionScorer — Main class (singleton)
// ============================================================================
class LayerContributionScorer {
public:
    static LayerContributionScorer& instance();

    // ----- Lifecycle -----
    PatchResult initialize(const ScorerConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- Score Submission -----
    // Submit an activation sample for a layer (called during/after inference)
    PatchResult submitActivation(const ActivationSample& sample);

    // Submit attention entropy for a layer
    PatchResult submitAttentionEntropy(uint32_t layerIndex, float entropy);

    // Submit an output delta observation (with vs without this layer)
    PatchResult submitOutputDelta(uint32_t layerIndex, float delta);

    // Submit a gradient proxy measurement
    PatchResult submitGradientProxy(uint32_t layerIndex, float sensitivity);

    // ----- Score Retrieval -----
    // Get score for a specific layer
    PatchResult getScore(uint32_t layerIndex, LayerScore* outScore) const;

    // Get all scores
    PatchResult getAllScores(LayerScore* outBuf, uint32_t* outCount) const;

    // Get sorted scores (highest contribution first)
    PatchResult getSortedScores(LayerScore* outBuf, uint32_t* outCount) const;

    // Get skip recommendations as a bitmask (true = skip-safe)
    PatchResult getSkipMask(bool* outMask, uint32_t maskLen) const;

    // ----- Scoring -----
    // Recompute all composite scores (call after submitting a batch)
    PatchResult recomputeScores();

    // Recompute a single layer's score
    PatchResult recomputeLayerScore(uint32_t layerIndex);

    // ----- Layer Role Assignment -----
    PatchResult setLayerRole(uint32_t layerIndex, LayerRole role);
    PatchResult autoAssignRoles();  // Heuristic: first N = early, last M = late, etc.

    // ----- Threshold Management -----
    PatchResult setSkipThreshold(float threshold);
    float getSkipThreshold() const;

    // ----- Protected Layers -----
    PatchResult protectLayer(uint32_t layerIndex);
    PatchResult unprotectLayer(uint32_t layerIndex);

    // ----- Callback -----
    PatchResult registerCallback(ScoreUpdateCallback cb, void* userData);
    PatchResult clearCallback();

    // ----- Reset -----
    PatchResult resetScores();
    PatchResult resetLayer(uint32_t layerIndex);

    // ----- Statistics -----
    struct Stats {
        std::atomic<uint64_t> samplesSubmitted{0};
        std::atomic<uint64_t> scoresComputed{0};
        std::atomic<uint64_t> skipRecommendations{0};
        std::atomic<uint64_t> protectedOverrides{0};
    };

    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    LayerContributionScorer();
    ~LayerContributionScorer();
    LayerContributionScorer(const LayerContributionScorer&) = delete;
    LayerContributionScorer& operator=(const LayerContributionScorer&) = delete;

    // Internal scoring
    float computeComposite(const LayerScore& score) const;
    void updateEMA(LayerScore& score);
    void updateVariance(LayerScore& score, float newComposite);
    void evaluateSkipRecommendation(LayerScore& score);
    void notifyCallback();

    // Members
    std::atomic<bool>           m_initialized{false};
    mutable std::mutex          m_mutex;

    ScorerConfig                m_config;
    std::vector<LayerScore>     m_scores;

    ScoreUpdateCallback         m_callback;
    void*                       m_callbackUserData;

    Stats                       m_stats;
};

#endif // LAYER_CONTRIBUTION_SCORER_H
