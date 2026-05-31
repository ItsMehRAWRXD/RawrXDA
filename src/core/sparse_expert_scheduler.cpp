// ============================================================================
// sparse_expert_scheduler.cpp — Sparse MoE Expert Dispatch Implementation
// ============================================================================
#include "sparse_expert_scheduler.h"

#include <cmath>
#include <numeric>
#include <random>

namespace rawrxd {

// ============================================================================
// Construction / Destruction
// ============================================================================

SparseExpertScheduler::SparseExpertScheduler(const SparseSchedulerConfig& cfg)
    : m_cfg(cfg)
{
    m_loadStats.resize(cfg.numExperts);
    for (uint32_t i = 0; i < cfg.numExperts; ++i) {
        m_loadStats[i].expertIndex = i;
    }
    m_sortBuf.resize(cfg.numExperts);
}

SparseExpertScheduler::~SparseExpertScheduler() = default;

// ============================================================================
// Softmax — numerically stable
// ============================================================================

void SparseExpertScheduler::softmaxInPlace(float* scores, uint32_t n) const
{
    if (n == 0) return;

    // Find max for numerical stability
    float maxVal = scores[0];
    for (uint32_t i = 1; i < n; ++i) {
        if (scores[i] > maxVal) maxVal = scores[i];
    }

    // exp and sum
    float sum = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        scores[i] = std::exp(scores[i] - maxVal);
        sum += scores[i];
    }

    // Normalize
    if (sum > 0.0f) {
        float invSum = 1.0f / sum;
        for (uint32_t i = 0; i < n; ++i) {
            scores[i] *= invSum;
        }
    }
}

// ============================================================================
// Jitter — small noise for load balancing exploration
// ============================================================================

void SparseExpertScheduler::addJitter(float* scores, uint32_t n) const
{
    if (!m_cfg.enableLoadBalance || m_cfg.jitterEpsilon <= 0.0f) return;

    // Use a simple LCG for deterministic jitter (not crypto, just balancing)
    static thread_local uint32_t seed = 42;
    for (uint32_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        float noise = ((float)(seed & 0xFFFF) / 65535.0f - 0.5f) * 2.0f * m_cfg.jitterEpsilon;
        scores[i] += noise;
    }
}

// ============================================================================
// Top-K Selection — per token
// ============================================================================

ExpertSelection SparseExpertScheduler::selectTopK(const float* scores,
                                                    uint32_t tokenIndex) const
{
    ExpertSelection sel;
    sel.tokenIndex = tokenIndex;
    sel.numActive = 0;

    const uint32_t n = m_cfg.numExperts;
    const uint32_t k = std::min(m_cfg.topK, n);

    // Copy scores into sort buffer
    if (m_sortBuf.size() < n)
        m_sortBuf.resize(n);

    for (uint32_t i = 0; i < n; ++i) {
        m_sortBuf[i] = {scores[i], i};
    }

    // Partial sort to get top-K
    std::partial_sort(m_sortBuf.begin(), m_sortBuf.begin() + k, m_sortBuf.begin() + n,
                      [](const auto& a, const auto& b) { return a.first > b.first; });

    // Normalize selected weights (softmax over top-K only)
    float weightSum = 0.0f;
    for (uint32_t i = 0; i < k; ++i) {
        weightSum += m_sortBuf[i].first;
    }

    for (uint32_t i = 0; i < k && i < 8; ++i) {
        sel.expertIds[i] = (int32_t)m_sortBuf[i].second;
        sel.expertWeights[i] = (weightSum > 0.0f)
            ? m_sortBuf[i].first / weightSum
            : 1.0f / (float)k;
        sel.numActive++;
    }

    return sel;
}

// ============================================================================
// Build Dispatch Plan — the main scheduling function
// ============================================================================

SparseDispatchPlan SparseExpertScheduler::buildPlan(const float* routerScores,
                                                     uint32_t batchSize,
                                                     uint32_t hiddenDim)
{
    SparseDispatchPlan plan;
    plan.batchSize = batchSize;
    plan.numExperts = m_cfg.numExperts;
    plan.topK = m_cfg.topK;
    plan.selections.resize(batchSize);
    plan.totalWorkItems = 0;

    // Temporary per-expert work lists
    std::vector<std::vector<ExpertWorkItem>> expertWork(m_cfg.numExperts);

    // Phase 1: Select top-K for each token
    std::vector<float> scoreBuf(m_cfg.numExperts);
    for (uint32_t t = 0; t < batchSize; ++t) {
        // Copy and preprocess router scores for this token
        std::memcpy(scoreBuf.data(), routerScores + t * m_cfg.numExperts,
                    m_cfg.numExperts * sizeof(float));

        // Add jitter for load balancing
        addJitter(scoreBuf.data(), m_cfg.numExperts);

        // Softmax
        softmaxInPlace(scoreBuf.data(), m_cfg.numExperts);

        // Select top-K
        ExpertSelection sel = selectTopK(scoreBuf.data(), t);
        plan.selections[t] = sel;

        // Build work items
        for (uint8_t k = 0; k < sel.numActive; ++k) {
            int32_t eid = sel.expertIds[k];
            if (eid < 0 || eid >= (int32_t)m_cfg.numExperts) continue;

            ExpertWorkItem item;
            item.expertIndex = (uint32_t)eid;
            item.tokenIndex = t;
            item.weight = sel.expertWeights[k];
            item.inputOffset = t * hiddenDim;
            item.outputOffset = 0; // assigned below
            expertWork[(uint32_t)eid].push_back(item);
            plan.totalWorkItems++;
        }
    }

    // Phase 2: Build expert batches (only for active experts)
    plan.activeExperts = 0;
    uint32_t capacity = expertCapacity(batchSize);

    for (uint32_t e = 0; e < m_cfg.numExperts; ++e) {
        auto& work = expertWork[e];
        if (work.empty()) continue;

        // Enforce capacity limit
        if (work.size() > capacity) {
            work.resize(capacity);
        }

        ExpertBatch batch;
        batch.expertIndex = e;
        batch.totalTokens = (uint32_t)work.size();

        // Assign output offsets: sequential within the expert
        for (uint32_t i = 0; i < (uint32_t)work.size(); ++i) {
            work[i].outputOffset = i * hiddenDim;
        }

        batch.items = std::move(work);
        plan.batches.push_back(std::move(batch));
        plan.activeExperts++;
    }

    // Sort batches by size (largest first) for better GPU utilization
    std::sort(plan.batches.begin(), plan.batches.end(),
              [](const ExpertBatch& a, const ExpertBatch& b) {
                  return a.totalTokens > b.totalTokens;
              });

    // Phase 3: Build flat gather/scatter maps for GPU upload
    buildMaps(plan, hiddenDim);

    // Phase 4: Update load statistics
    updateLoadStats(plan);

    return plan;
}

// ============================================================================
// Expert Capacity
// ============================================================================

uint32_t SparseExpertScheduler::expertCapacity(uint32_t batchSize) const
{
    // capacity = ceil(batchSize * topK / numExperts) * capacityFactor
    // Standard factor is 1.25 to handle slight imbalance
    if (m_cfg.numExperts == 0) return batchSize;
    double uniform = (double)batchSize * (double)m_cfg.topK / (double)m_cfg.numExperts;
    return (uint32_t)std::ceil(uniform * 1.25);
}

// ============================================================================
// Build Gather/Scatter Maps
// ============================================================================

void SparseExpertScheduler::buildMaps(SparseDispatchPlan& plan,
                                       uint32_t hiddenDim) const
{
    plan.gatherMap.clear();
    plan.scatterMap.clear();
    plan.scatterWeights.clear();

    plan.gatherMap.reserve(plan.totalWorkItems);
    plan.scatterMap.reserve(plan.totalWorkItems);
    plan.scatterWeights.reserve(plan.totalWorkItems);

    for (const auto& batch : plan.batches) {
        for (const auto& item : batch.items) {
            plan.gatherMap.push_back(item.tokenIndex);
            plan.scatterMap.push_back(item.tokenIndex);
            plan.scatterWeights.push_back(item.weight);
        }
    }
}

// ============================================================================
// Load Statistics
// ============================================================================

void SparseExpertScheduler::updateLoadStats(const SparseDispatchPlan& plan)
{
    for (const auto& batch : plan.batches) {
        uint32_t e = batch.expertIndex;
        if (e >= m_loadStats.size()) continue;
        m_loadStats[e].totalTokensRouted += batch.totalTokens;
        m_loadStats[e].totalBatchesActive += 1;
    }

    // Update derived stats
    for (auto& ls : m_loadStats) {
        uint64_t batches = ls.totalBatchesActive;
        uint64_t tokens = ls.totalTokensRouted;
        ls.avgTokensPerBatch = (batches > 0) ? (double)tokens / (double)batches : 0;

        // Utilization vs uniform: if all experts get equal tokens, each gets
        // totalTokens * topK / numExperts
        double uniformTokens = (double)plan.batchSize * (double)plan.topK /
                                (double)m_cfg.numExperts;
        ls.loadFraction = (uniformTokens > 0)
            ? ls.avgTokensPerBatch / uniformTokens : 0;
        ls.utilizationPercent = ls.loadFraction * 100.0;
    }
}

double SparseExpertScheduler::computeAuxiliaryLoss(
    const SparseDispatchPlan& plan) const
{
    // Switch Transformer auxiliary loss:
    // L_aux = N * sum_i(f_i * P_i)
    // f_i = fraction of tokens routed to expert i
    // P_i = average routing probability for expert i
    double loss = 0.0;
    uint32_t totalTokens = plan.batchSize * plan.topK;
    if (totalTokens == 0) return 0.0;

    for (const auto& batch : plan.batches) {
        double fi = (double)batch.totalTokens / (double)totalTokens;

        // Compute average routing probability for this expert across all tokens
        double avgProb = 0.0;
        for (const auto& item : batch.items) {
            avgProb += item.weight;
        }
        if (!batch.items.empty())
            avgProb /= (double)batch.items.size();

        loss += fi * avgProb;
    }

    return (double)m_cfg.numExperts * loss * m_cfg.auxiliaryLossWeight;
}

void SparseExpertScheduler::resetLoadStats()
{
    for (auto& ls : m_loadStats) {
        ls.totalTokensRouted = 0;
        ls.totalBatchesActive = 0;
        ls.avgTokensPerBatch = 0;
        ls.utilizationPercent = 0;
        ls.loadFraction = 0;
        ls.auxiliaryLoss = 0;
    }
}

} // namespace rawrxd
