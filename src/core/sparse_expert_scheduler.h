// ============================================================================
// sparse_expert_scheduler.h — Sparse MoE Expert Dispatch Scheduler
// ============================================================================
// Only computes the top-K selected experts per token, skipping inactive
// experts entirely.  Builds sparse dispatch indices + gather maps for
// efficient GPU execution.
//
// Typical MoE: 64 experts, top-2 active → 97% computation saved.
//
// Architecture:
//   ExpertSelection   — per-token routing decision (expert IDs + weights)
//   SparseDispatchPlan — precomputed gather/scatter indices for the batch
//   ExpertLoadStats    — utilization tracking for load balancing
//   SparseExpertScheduler — the main scheduler
//
// Integration:
//   1. Router network produces expert scores per token
//   2. SparseExpertScheduler selects top-K and builds dispatch plan
//   3. Dispatch plan used by GPU kernel to gather inputs → experts → scatter
//   4. Load stats fed back into auxiliary loss for training
//
// GPU dispatch model:
//   Instead of launching all 64 experts with masking, we:
//   - Build a sorted list of (expert, token) pairs
//   - Launch one kernel per active expert, only for its assigned tokens
//   - Scatter-add results back using the gather map
//
// Thread safety: immutable dispatch plan; scheduler is single-threaded per batch.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// ExpertSelection — per-token routing decision
// ---------------------------------------------------------------------------
struct ExpertSelection {
    uint32_t tokenIndex;
    int32_t  expertIds[8];    // up to 8 active experts (typical: 2)
    float    expertWeights[8];
    uint8_t  numActive;       // how many experts are active for this token

    ExpertSelection() {
        tokenIndex = 0;
        numActive = 0;
        std::memset(expertIds, 0xFF, sizeof(expertIds));
        std::memset(expertWeights, 0, sizeof(expertWeights));
    }
};

// ---------------------------------------------------------------------------
// ExpertWorkItem — one (expert, token) work unit
// ---------------------------------------------------------------------------
struct ExpertWorkItem {
    uint32_t expertIndex;
    uint32_t tokenIndex;
    float    weight;         // routing weight for scatter-add
    uint32_t inputOffset;    // offset into the batched input tensor
    uint32_t outputOffset;   // offset into the batched output tensor
};

// ---------------------------------------------------------------------------
// ExpertBatch — all tokens assigned to one expert
// ---------------------------------------------------------------------------
struct ExpertBatch {
    uint32_t                  expertIndex;
    std::vector<ExpertWorkItem> items;
    uint32_t                  totalTokens;    // == items.size()
};

// ---------------------------------------------------------------------------
// SparseDispatchPlan — precomputed indices for the entire batch
// ---------------------------------------------------------------------------
struct SparseDispatchPlan {
    uint32_t                    batchSize;      // total tokens in batch
    uint32_t                    numExperts;      // total experts in model
    uint32_t                    topK;            // experts per token
    uint32_t                    activeExperts;   // experts that have ≥1 token
    std::vector<ExpertBatch>    batches;         // one per active expert
    std::vector<ExpertSelection> selections;     // per-token routing

    // Flat gather/scatter maps for GPU upload
    std::vector<uint32_t>       gatherMap;       // tokenIndex → expert input slot
    std::vector<uint32_t>       scatterMap;      // expert output slot → tokenIndex
    std::vector<float>          scatterWeights;  // weights for scatter-add

    // Total work items across all experts
    uint32_t totalWorkItems = 0;
};

// ---------------------------------------------------------------------------
// ExpertLoadStats — utilization tracking for load balancing
// ---------------------------------------------------------------------------
struct ExpertLoadStats {
    uint32_t expertIndex;
    uint64_t totalTokensRouted    = 0;
    uint64_t totalBatchesActive   = 0;
    double   avgTokensPerBatch = 0;
    double   utilizationPercent = 0;  // vs uniform distribution

    // Auxiliary loss components
    double   loadFraction = 0;        // fraction of tokens routed here
    double   auxiliaryLoss = 0;       // L_aux = N * sum(f_i * P_i)
};

// ---------------------------------------------------------------------------
// SparseSchedulerConfig
// ---------------------------------------------------------------------------
struct SparseSchedulerConfig {
    uint32_t numExperts    = 64;     // total experts in the MoE layer
    uint32_t topK          = 2;      // experts selected per token
    uint32_t maxBatchSize  = 512;    // max tokens per batch
    float    jitterEpsilon = 0.01f;  // noise for load balancing
    bool     enableLoadBalance = true;
    float    auxiliaryLossWeight = 0.01f;
};

// ---------------------------------------------------------------------------
// SparseExpertScheduler
// ---------------------------------------------------------------------------
class SparseExpertScheduler {
public:
    explicit SparseExpertScheduler(const SparseSchedulerConfig& cfg = {});
    ~SparseExpertScheduler();

    // ── Core: Build dispatch plan from router scores ────────────────────

    // routerScores: [batchSize × numExperts] row-major — raw router logits
    // Returns a dispatch plan with sorted work items.
    SparseDispatchPlan buildPlan(const float* routerScores,
                                 uint32_t batchSize,
                                 uint32_t hiddenDim);

    // ── Top-K Selection ─────────────────────────────────────────────────

    // Select top-K experts for a single token's router scores.
    // scores: [numExperts] — raw logits (softmax applied internally).
    ExpertSelection selectTopK(const float* scores, uint32_t tokenIndex) const;

    // ── Load Balancing ──────────────────────────────────────────────────

    // Compute auxiliary loss for load balancing.
    // Returns the loss value that should be added to the training loss.
    double computeAuxiliaryLoss(const SparseDispatchPlan& plan) const;

    // Get load stats for all experts
    const std::vector<ExpertLoadStats>& loadStats() const { return m_loadStats; }

    // Reset load statistics
    void resetLoadStats();

    // ── Expert Capacity ─────────────────────────────────────────────────

    // Get the capacity factor: max tokens any expert can receive.
    // Tokens beyond capacity are dropped (overflow → no expert processes them).
    uint32_t expertCapacity(uint32_t batchSize) const;

    // ── Config ──────────────────────────────────────────────────────────
    const SparseSchedulerConfig& config() const { return m_cfg; }

private:
    // Fast path for common MoE settings (k <= 2): linear scan with no partial sort.
    ExpertSelection selectTopKFast(const float* scores, uint32_t tokenIndex) const;

    // Softmax over expert scores for one token
    void softmaxInPlace(float* scores, uint32_t n) const;

    // Add jitter noise for load balancing
    void addJitter(float* scores, uint32_t n) const;

    // Build gather/scatter maps from sorted batches
    void buildMaps(SparseDispatchPlan& plan, uint32_t hiddenDim) const;

    // Update load statistics from a completed plan
    void updateLoadStats(const SparseDispatchPlan& plan);

    SparseSchedulerConfig          m_cfg;
    std::vector<ExpertLoadStats>   m_loadStats;

    // Pre-allocated buffers for top-K selection (avoid alloc per token)
    mutable std::vector<std::pair<float, uint32_t>> m_sortBuf;
};

} // namespace rawrxd
