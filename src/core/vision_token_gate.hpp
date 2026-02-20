// ============================================================================
// vision_token_gate.hpp — Vision Token Gating Policy for Multi-Modal Pipeline
// ============================================================================
// Controls how many vision tokens are injected into the LLM context window.
// Prevents vision-heavy inputs from exhausting the text token budget.
//
// Features:
//   - Per-patch activation scoring (L2 norm, variance, entropy)
//   - Budget-aware top-K selection (keep highest-activation patches)
//   - Minimum activation threshold filtering
//   - Per-image and per-batch token caps
//   - Attention-weighted gating (prioritize patches the model attends to)
//   - Statistics tracking for observability
//
// Integration: Called by VisionEncoder after encoding, before LLM injection.
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "vision_encoder.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace RawrXD {
namespace Vision {

// ============================================================================
// Gate Policy Configuration
// ============================================================================
struct VisionGatePolicy {
    uint32_t maxVisionTokens;          // Hard cap on total vision tokens injected
    uint32_t maxTokensPerImage;        // Per-image cap (for multi-image batches)
    uint32_t tokensPerPatch;           // How many tokens each patch consumes (usually 1)
    float    minActivationThreshold;   // Patches below this activation are dropped
    float    entropyThreshold;         // Min entropy for a patch to be "informative"
    bool     useAttentionWeighting;    // Weight by cross-attention scores if available
    bool     dynamicBudget;            // Auto-adjust budget based on text token usage

    // Context window integration
    uint32_t totalContextWindow;       // Total LLM context window size
    uint32_t reservedForText;          // Tokens reserved for text (prompt + response)

    VisionGatePolicy()
        : maxVisionTokens(576)
        , maxTokensPerImage(256)
        , tokensPerPatch(1)
        , minActivationThreshold(0.05f)
        , entropyThreshold(0.1f)
        , useAttentionWeighting(false)
        , dynamicBudget(true)
        , totalContextWindow(4096)
        , reservedForText(2048)
    {}
};

// ============================================================================
// Patch Activation Score — computed per patch for gating decisions
// ============================================================================
struct PatchActivation {
    uint32_t patchIndex;
    float    l2Norm;          // L2 norm of patch embedding (magnitude)
    float    variance;        // Variance of embedding dimensions
    float    entropy;         // Information entropy estimate
    float    compositeScore;  // Weighted combination for ranking
    bool     gated;           // true = dropped, false = kept
};

// ============================================================================
// Gate Result — returned after gating a VisionEmbedding
// ============================================================================
struct GateResult {
    uint32_t originalPatches;     // Patches before gating
    uint32_t keptPatches;         // Patches after gating
    uint32_t droppedPatches;      // Patches removed
    uint32_t tokensConsumed;      // Total vision tokens after gating
    float    avgActivation;       // Average activation of kept patches
    float    minKeptActivation;   // Lowest activation that survived
    float    maxDroppedActivation;// Highest activation that was dropped
    float    budgetUtilization;   // tokensConsumed / maxVisionTokens
};

// ============================================================================
// Gate Statistics — observability counters
// ============================================================================
struct GateStats {
    uint64_t totalGateOperations;
    uint64_t totalPatchesEvaluated;
    uint64_t totalPatchesKept;
    uint64_t totalPatchesDropped;
    uint64_t totalBudgetExhausted;   // How many times the budget cap was hit
    double   avgActivationKept;
    double   avgActivationDropped;
    double   avgBudgetUtilization;
};

// ============================================================================
// VisionTokenGate — The main gating engine
// ============================================================================
class VisionTokenGate {
public:
    static VisionTokenGate& instance();

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------
    void setPolicy(const VisionGatePolicy& policy);
    const VisionGatePolicy& getPolicy() const;

    // -----------------------------------------------------------------------
    // Core Gating Operations
    // -----------------------------------------------------------------------

    // Gate a single embedding: drop low-activation patches, enforce token budget.
    // Modifies emb.patchEmbeddings in-place (removes dropped patches).
    // Returns GateResult with statistics about what was kept/dropped.
    VisionResult gateEmbedding(VisionEmbedding& emb, GateResult& result);

    // Gate a batch of embeddings with shared token budget.
    // Distributes tokens across images proportionally to their content richness.
    VisionResult gateBatch(std::vector<VisionEmbedding>& embeddings,
                           std::vector<GateResult>& results);

    // -----------------------------------------------------------------------
    // Activation Scoring
    // -----------------------------------------------------------------------

    // Compute activation score for a single patch embedding.
    float computePatchActivation(const std::vector<float>& patchEmbedding) const;

    // Compute all patch activations for an embedding.
    // Returns sorted by compositeScore descending.
    void computeAllActivations(const VisionEmbedding& emb,
                               std::vector<PatchActivation>& activations) const;

    // -----------------------------------------------------------------------
    // Budget Management
    // -----------------------------------------------------------------------

    // Check if a patch should be allowed given current budget.
    bool allowVisionToken(float activation) const;

    // Compute available vision token budget given current text usage.
    uint32_t computeAvailableBudget(uint32_t textTokensUsed) const;

    // -----------------------------------------------------------------------
    // Selective Gating (for fine-grained control)
    // -----------------------------------------------------------------------

    // Keep only the top-K patches by activation score.
    VisionResult prioritizePatches(VisionEmbedding& emb, uint32_t topK);

    // Drop patches that are spatially redundant (similar to neighbors).
    VisionResult deduplicatePatches(VisionEmbedding& emb,
                                    float similarityThreshold = 0.95f);

    // Apply attention weights from a previous forward pass to re-rank patches.
    VisionResult applyAttentionWeights(VisionEmbedding& emb,
                                       const std::vector<float>& attentionScores);

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    GateStats getStats() const;
    void resetStats();

private:
    VisionTokenGate();
    ~VisionTokenGate() = default;
    VisionTokenGate(const VisionTokenGate&) = delete;
    VisionTokenGate& operator=(const VisionTokenGate&) = delete;

    // Internal: compute individual activation components
    float computeL2Norm(const std::vector<float>& v) const;
    float computeVariance(const std::vector<float>& v) const;
    float computeEntropy(const std::vector<float>& v) const;
    float computeCompositeScore(float l2, float variance, float entropy) const;

    // Internal: cosine similarity between two vectors
    float cosineSimilarity(const std::vector<float>& a,
                           const std::vector<float>& b) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex mutex_;
    VisionGatePolicy policy_;

    // Statistics (atomic for lock-free reads)
    std::atomic<uint64_t> totalGateOps_{0};
    std::atomic<uint64_t> totalPatchesEvaluated_{0};
    std::atomic<uint64_t> totalPatchesKept_{0};
    std::atomic<uint64_t> totalPatchesDropped_{0};
    std::atomic<uint64_t> totalBudgetExhausted_{0};

    // Accumulators (protected by mutex_)
    double activationKeptAccum_ = 0.0;
    double activationDroppedAccum_ = 0.0;
    double budgetUtilAccum_ = 0.0;
};

} // namespace Vision
} // namespace RawrXD
