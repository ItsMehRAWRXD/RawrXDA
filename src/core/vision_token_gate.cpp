// ============================================================================
// vision_token_gate.cpp — Vision Token Gating Implementation
// ============================================================================
// Full implementation of activation-based vision token gating.
// Prevents vision tokens from exhausting the LLM context window.
//
// Scoring model:
//   composite = 0.4 * normalized_l2 + 0.3 * normalized_variance + 0.3 * entropy
//
// Gating pipeline:
//   1. Compute per-patch activation scores (L2, variance, entropy)
//   2. Filter by minimum activation threshold
//   3. Sort by composite score descending
//   4. Cap at budget (maxVisionTokens / tokensPerPatch)
//   5. Optionally deduplicate spatially redundant patches
//   6. Update statistics
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "vision_token_gate.hpp"
#include <cstring>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <chrono>

namespace RawrXD {
namespace Vision {

// ============================================================================
// Singleton
// ============================================================================

VisionTokenGate& VisionTokenGate::instance() {
    static VisionTokenGate inst;
    return inst;
}

VisionTokenGate::VisionTokenGate()
    : policy_()
{
}

// ============================================================================
// Configuration
// ============================================================================

void VisionTokenGate::setPolicy(const VisionGatePolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_ = policy;
}

const VisionGatePolicy& VisionTokenGate::getPolicy() const {
    return policy_;
}

// ============================================================================
// Activation Scoring — L2 Norm
// ============================================================================

float VisionTokenGate::computeL2Norm(const std::vector<float>& v) const {
    if (v.empty()) return 0.0f;

    float sum = 0.0f;
    for (size_t i = 0; i < v.size(); ++i) {
        sum += v[i] * v[i];
    }
    return sqrtf(sum);
}

// ============================================================================
// Activation Scoring — Variance
// ============================================================================

float VisionTokenGate::computeVariance(const std::vector<float>& v) const {
    if (v.size() < 2) return 0.0f;

    float mean = 0.0f;
    for (size_t i = 0; i < v.size(); ++i) {
        mean += v[i];
    }
    mean /= static_cast<float>(v.size());

    float variance = 0.0f;
    for (size_t i = 0; i < v.size(); ++i) {
        float diff = v[i] - mean;
        variance += diff * diff;
    }
    variance /= static_cast<float>(v.size() - 1);
    return variance;
}

// ============================================================================
// Activation Scoring — Information Entropy Estimate
// ============================================================================
// We discretize the embedding values into bins and compute Shannon entropy.
// Higher entropy = more informative patch (more varied features).

float VisionTokenGate::computeEntropy(const std::vector<float>& v) const {
    if (v.size() < 4) return 0.0f;

    // Find min/max for binning
    float vmin = v[0], vmax = v[0];
    for (size_t i = 1; i < v.size(); ++i) {
        if (v[i] < vmin) vmin = v[i];
        if (v[i] > vmax) vmax = v[i];
    }

    float range = vmax - vmin;
    if (range < 1e-10f) return 0.0f; // Constant vector → zero entropy

    // 32 bins for entropy estimation
    constexpr int NUM_BINS = 32;
    int bins[NUM_BINS] = {};

    for (size_t i = 0; i < v.size(); ++i) {
        int bin = static_cast<int>((v[i] - vmin) / range * (NUM_BINS - 1));
        if (bin < 0) bin = 0;
        if (bin >= NUM_BINS) bin = NUM_BINS - 1;
        bins[bin]++;
    }

    // Shannon entropy: H = -sum(p * log2(p))
    float entropy = 0.0f;
    float n = static_cast<float>(v.size());
    for (int i = 0; i < NUM_BINS; ++i) {
        if (bins[i] > 0) {
            float p = static_cast<float>(bins[i]) / n;
            entropy -= p * log2f(p);
        }
    }

    // Normalize to [0, 1] range (max entropy = log2(NUM_BINS))
    float maxEntropy = log2f(static_cast<float>(NUM_BINS));
    if (maxEntropy > 0.0f) {
        entropy /= maxEntropy;
    }

    return entropy;
}

// ============================================================================
// Composite Score — Weighted combination of L2, variance, entropy
// ============================================================================

float VisionTokenGate::computeCompositeScore(float l2, float variance,
                                              float entropy) const {
    // Weights: L2 magnitude (40%), variance (30%), entropy (30%)
    // Each component should be roughly normalized to [0, 1] range
    // L2: typical range for CLIP embeddings is [0, 50], normalize by /50
    // Variance: typical range [0, 1] for normalized embeddings
    // Entropy: already normalized to [0, 1]

    float normL2 = std::min(l2 / 50.0f, 1.0f);
    float normVar = std::min(variance * 10.0f, 1.0f);  // Scale up small variances
    float normEnt = entropy; // Already [0, 1]

    return 0.4f * normL2 + 0.3f * normVar + 0.3f * normEnt;
}

// ============================================================================
// Cosine Similarity
// ============================================================================

float VisionTokenGate::cosineSimilarity(const std::vector<float>& a,
                                         const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    float dot = 0.0f, normA = 0.0f, normB = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot   += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    float denom = sqrtf(normA) * sqrtf(normB);
    if (denom < 1e-10f) return 0.0f;
    return dot / denom;
}

// ============================================================================
// Compute Single Patch Activation
// ============================================================================

float VisionTokenGate::computePatchActivation(
    const std::vector<float>& patchEmbedding) const
{
    float l2  = computeL2Norm(patchEmbedding);
    float var = computeVariance(patchEmbedding);
    float ent = computeEntropy(patchEmbedding);
    return computeCompositeScore(l2, var, ent);
}

// ============================================================================
// Compute All Patch Activations — sorted by composite score descending
// ============================================================================

void VisionTokenGate::computeAllActivations(
    const VisionEmbedding& emb,
    std::vector<PatchActivation>& activations) const
{
    activations.clear();
    activations.reserve(emb.patchEmbeddings.size());

    for (uint32_t i = 0; i < static_cast<uint32_t>(emb.patchEmbeddings.size()); ++i) {
        const auto& patch = emb.patchEmbeddings[i];

        PatchActivation pa;
        pa.patchIndex = i;
        pa.l2Norm = computeL2Norm(patch);
        pa.variance = computeVariance(patch);
        pa.entropy = computeEntropy(patch);
        pa.compositeScore = computeCompositeScore(pa.l2Norm, pa.variance, pa.entropy);
        pa.gated = false;
        activations.push_back(pa);
    }

    // Sort by composite score descending (best patches first)
    std::sort(activations.begin(), activations.end(),
        [](const PatchActivation& a, const PatchActivation& b) {
            return a.compositeScore > b.compositeScore;
        });
}

// ============================================================================
// Allow Vision Token — Quick check against threshold
// ============================================================================

bool VisionTokenGate::allowVisionToken(float activation) const {
    return activation >= policy_.minActivationThreshold;
}

// ============================================================================
// Compute Available Budget
// ============================================================================

uint32_t VisionTokenGate::computeAvailableBudget(uint32_t textTokensUsed) const {
    if (!policy_.dynamicBudget) {
        return policy_.maxVisionTokens;
    }

    // Dynamic: vision gets whatever's left after text reservation
    uint32_t textReserved = std::max(policy_.reservedForText, textTokensUsed);
    if (textReserved >= policy_.totalContextWindow) {
        return 0; // No room for vision
    }

    uint32_t available = policy_.totalContextWindow - textReserved;
    return std::min(available, policy_.maxVisionTokens);
}

// ============================================================================
// Gate Embedding — Core gating operation
// ============================================================================
// Pipeline:
//   1. Compute per-patch activations
//   2. Filter by minActivationThreshold
//   3. Cap at budget
//   4. Remove dropped patches from embedding
//   5. Update global embedding (mean of kept patches)

VisionResult VisionTokenGate::gateEmbedding(VisionEmbedding& emb,
                                             GateResult& result) {
    std::lock_guard<std::mutex> lock(mutex_);

    result = {};
    result.originalPatches = static_cast<uint32_t>(emb.patchEmbeddings.size());

    if (emb.patchEmbeddings.empty()) {
        result.keptPatches = 0;
        result.droppedPatches = 0;
        result.tokensConsumed = 0;
        result.budgetUtilization = 0.0f;
        return VisionResult::ok("No patches to gate");
    }

    // Step 1: Compute activations
    std::vector<PatchActivation> activations;
    computeAllActivations(emb, activations);

    totalPatchesEvaluated_.fetch_add(activations.size(), std::memory_order_relaxed);

    // Step 2: Apply activation threshold filter
    uint32_t thresholdPassed = 0;
    for (auto& pa : activations) {
        if (pa.compositeScore < policy_.minActivationThreshold) {
            pa.gated = true;
        } else {
            thresholdPassed++;
        }
    }

    // Step 3: Apply token budget cap
    uint32_t maxPatches = policy_.maxVisionTokens / std::max(policy_.tokensPerPatch, 1u);
    maxPatches = std::min(maxPatches, policy_.maxTokensPerImage / std::max(policy_.tokensPerPatch, 1u));

    uint32_t kept = 0;
    float minKeptActivation = 1e30f;
    float maxDroppedActivation = -1e30f;
    double keptActivationSum = 0.0;
    double droppedActivationSum = 0.0;

    for (auto& pa : activations) {
        if (!pa.gated && kept < maxPatches) {
            // Keep this patch
            kept++;
            keptActivationSum += pa.compositeScore;
            if (pa.compositeScore < minKeptActivation) {
                minKeptActivation = pa.compositeScore;
            }
        } else {
            // Drop this patch
            pa.gated = true;
            droppedActivationSum += pa.compositeScore;
            if (pa.compositeScore > maxDroppedActivation) {
                maxDroppedActivation = pa.compositeScore;
            }
        }
    }

    bool budgetExhausted = (thresholdPassed > maxPatches);
    if (budgetExhausted) {
        totalBudgetExhausted_.fetch_add(1, std::memory_order_relaxed);
    }

    // Step 4: Build new patch embeddings (only kept patches, in original order)
    // First, create a set of kept patch indices
    std::vector<uint32_t> keptIndices;
    keptIndices.reserve(kept);
    for (const auto& pa : activations) {
        if (!pa.gated) {
            keptIndices.push_back(pa.patchIndex);
        }
    }
    // Sort indices to maintain spatial ordering
    std::sort(keptIndices.begin(), keptIndices.end());

    // Build filtered patch list
    std::vector<std::vector<float>> filteredPatches;
    filteredPatches.reserve(keptIndices.size());
    for (uint32_t idx : keptIndices) {
        if (idx < emb.patchEmbeddings.size()) {
            filteredPatches.push_back(std::move(emb.patchEmbeddings[idx]));
        }
    }

    uint32_t dropped = result.originalPatches - static_cast<uint32_t>(filteredPatches.size());

    // Step 5: Update the global embedding as mean of kept patches
    if (!filteredPatches.empty() && !filteredPatches[0].empty()) {
        size_t dim = filteredPatches[0].size();
        emb.embedding.assign(dim, 0.0f);

        for (const auto& patch : filteredPatches) {
            for (size_t d = 0; d < std::min(dim, patch.size()); ++d) {
                emb.embedding[d] += patch[d];
            }
        }

        float invN = 1.0f / static_cast<float>(filteredPatches.size());
        for (size_t d = 0; d < dim; ++d) {
            emb.embedding[d] *= invN;
        }

        // L2 normalize the global embedding
        float norm = 0.0f;
        for (size_t d = 0; d < dim; ++d) {
            norm += emb.embedding[d] * emb.embedding[d];
        }
        norm = sqrtf(norm);
        if (norm > 1e-10f) {
            for (size_t d = 0; d < dim; ++d) {
                emb.embedding[d] /= norm;
            }
        }
    }

    // Replace patch embeddings with filtered set
    emb.patchEmbeddings = std::move(filteredPatches);
    emb.numPatches = static_cast<uint32_t>(emb.patchEmbeddings.size());

    // Step 6: Fill result
    result.keptPatches = kept;
    result.droppedPatches = dropped;
    result.tokensConsumed = kept * policy_.tokensPerPatch;
    result.avgActivation = (kept > 0) ? static_cast<float>(keptActivationSum / kept) : 0.0f;
    result.minKeptActivation = (kept > 0) ? minKeptActivation : 0.0f;
    result.maxDroppedActivation = (dropped > 0) ? maxDroppedActivation : 0.0f;
    result.budgetUtilization = (policy_.maxVisionTokens > 0)
        ? static_cast<float>(result.tokensConsumed) / static_cast<float>(policy_.maxVisionTokens)
        : 0.0f;

    // Step 7: Update statistics
    totalGateOps_.fetch_add(1, std::memory_order_relaxed);
    totalPatchesKept_.fetch_add(kept, std::memory_order_relaxed);
    totalPatchesDropped_.fetch_add(dropped, std::memory_order_relaxed);
    activationKeptAccum_ += keptActivationSum;
    activationDroppedAccum_ += droppedActivationSum;
    budgetUtilAccum_ += result.budgetUtilization;

    return VisionResult::ok("Token gating applied");
}

// ============================================================================
// Gate Batch — Distribute token budget across multiple images
// ============================================================================

VisionResult VisionTokenGate::gateBatch(
    std::vector<VisionEmbedding>& embeddings,
    std::vector<GateResult>& results)
{
    results.clear();
    results.resize(embeddings.size());

    if (embeddings.empty()) {
        return VisionResult::ok("Empty batch");
    }

    // Strategy: distribute total budget proportionally to patch count
    // Then gate each image with its allocated share

    uint32_t totalPatches = 0;
    for (const auto& emb : embeddings) {
        totalPatches += static_cast<uint32_t>(emb.patchEmbeddings.size());
    }

    if (totalPatches == 0) {
        return VisionResult::ok("No patches in batch");
    }

    // Save current per-image cap
    uint32_t savedMaxTokensPerImage = policy_.maxTokensPerImage;

    // Compute per-image allocations proportional to richness
    // "Richness" = number of patches * average activation
    std::vector<float> richness(embeddings.size(), 0.0f);
    float totalRichness = 0.0f;

    for (size_t i = 0; i < embeddings.size(); ++i) {
        float avgAct = 0.0f;
        for (const auto& patch : embeddings[i].patchEmbeddings) {
            avgAct += computePatchActivation(patch);
        }
        if (!embeddings[i].patchEmbeddings.empty()) {
            avgAct /= static_cast<float>(embeddings[i].patchEmbeddings.size());
        }
        richness[i] = static_cast<float>(embeddings[i].patchEmbeddings.size()) * (0.5f + avgAct);
        totalRichness += richness[i];
    }

    // Gate each image with proportional budget
    for (size_t i = 0; i < embeddings.size(); ++i) {
        float share = (totalRichness > 0.0f)
            ? richness[i] / totalRichness
            : 1.0f / static_cast<float>(embeddings.size());

        uint32_t imageBudget = static_cast<uint32_t>(
            share * static_cast<float>(policy_.maxVisionTokens));
        imageBudget = std::max(imageBudget, 1u);

        // Temporarily adjust per-image cap
        {
            std::lock_guard<std::mutex> lock(mutex_);
            policy_.maxTokensPerImage = imageBudget;
        }

        gateEmbedding(embeddings[i], results[i]);
    }

    // Restore original per-image cap
    {
        std::lock_guard<std::mutex> lock(mutex_);
        policy_.maxTokensPerImage = savedMaxTokensPerImage;
    }

    return VisionResult::ok("Batch gating applied");
}

// ============================================================================
// Prioritize Patches — Keep top-K by activation
// ============================================================================

VisionResult VisionTokenGate::prioritizePatches(VisionEmbedding& emb,
                                                 uint32_t topK) {
    if (emb.patchEmbeddings.size() <= topK) {
        return VisionResult::ok("Already within budget");
    }

    // Compute activations and sort
    std::vector<PatchActivation> activations;
    computeAllActivations(emb, activations);

    // Keep top-K (activations already sorted descending)
    std::vector<uint32_t> keepIndices;
    keepIndices.reserve(topK);
    for (uint32_t i = 0; i < std::min(topK, static_cast<uint32_t>(activations.size())); ++i) {
        keepIndices.push_back(activations[i].patchIndex);
    }
    std::sort(keepIndices.begin(), keepIndices.end()); // Maintain spatial order

    std::vector<std::vector<float>> filtered;
    filtered.reserve(keepIndices.size());
    for (uint32_t idx : keepIndices) {
        if (idx < emb.patchEmbeddings.size()) {
            filtered.push_back(std::move(emb.patchEmbeddings[idx]));
        }
    }

    emb.patchEmbeddings = std::move(filtered);
    emb.numPatches = static_cast<uint32_t>(emb.patchEmbeddings.size());

    return VisionResult::ok("Patches prioritized");
}

// ============================================================================
// Deduplicate Patches — Remove spatially redundant patches
// ============================================================================

VisionResult VisionTokenGate::deduplicatePatches(VisionEmbedding& emb,
                                                  float similarityThreshold) {
    if (emb.patchEmbeddings.size() < 2) {
        return VisionResult::ok("Nothing to deduplicate");
    }

    // Greedy deduplication: iterate patches, mark duplicates of already-seen patches
    std::vector<bool> keep(emb.patchEmbeddings.size(), true);
    uint32_t removed = 0;

    for (size_t i = 0; i < emb.patchEmbeddings.size(); ++i) {
        if (!keep[i]) continue;

        for (size_t j = i + 1; j < emb.patchEmbeddings.size(); ++j) {
            if (!keep[j]) continue;

            float sim = cosineSimilarity(emb.patchEmbeddings[i], emb.patchEmbeddings[j]);
            if (sim >= similarityThreshold) {
                keep[j] = false; // Mark duplicate
                removed++;
            }
        }
    }

    if (removed == 0) {
        return VisionResult::ok("No duplicates found");
    }

    // Rebuild patch list
    std::vector<std::vector<float>> filtered;
    filtered.reserve(emb.patchEmbeddings.size() - removed);
    for (size_t i = 0; i < emb.patchEmbeddings.size(); ++i) {
        if (keep[i]) {
            filtered.push_back(std::move(emb.patchEmbeddings[i]));
        }
    }

    emb.patchEmbeddings = std::move(filtered);
    emb.numPatches = static_cast<uint32_t>(emb.patchEmbeddings.size());

    return VisionResult::ok("Deduplication complete");
}

// ============================================================================
// Apply Attention Weights — Re-weight patches by cross-attention scores
// ============================================================================

VisionResult VisionTokenGate::applyAttentionWeights(
    VisionEmbedding& emb,
    const std::vector<float>& attentionScores)
{
    if (attentionScores.size() != emb.patchEmbeddings.size()) {
        return VisionResult::error("Attention scores size mismatch", 1);
    }

    // Scale each patch embedding by its attention weight
    // Then re-gate with the threshold (low-attention patches get dropped)
    for (size_t i = 0; i < emb.patchEmbeddings.size(); ++i) {
        float weight = attentionScores[i];

        // Clamp to [0, 2] range (allow slight amplification)
        weight = std::max(0.0f, std::min(weight, 2.0f));

        for (size_t d = 0; d < emb.patchEmbeddings[i].size(); ++d) {
            emb.patchEmbeddings[i][d] *= weight;
        }
    }

    // Remove patches that got zeroed out (attention = 0)
    std::vector<std::vector<float>> filtered;
    filtered.reserve(emb.patchEmbeddings.size());
    for (size_t i = 0; i < emb.patchEmbeddings.size(); ++i) {
        if (attentionScores[i] > 0.01f) {
            filtered.push_back(std::move(emb.patchEmbeddings[i]));
        }
    }

    emb.patchEmbeddings = std::move(filtered);
    emb.numPatches = static_cast<uint32_t>(emb.patchEmbeddings.size());

    return VisionResult::ok("Attention weights applied");
}

// ============================================================================
// Statistics
// ============================================================================

GateStats VisionTokenGate::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    GateStats stats = {};
    stats.totalGateOperations = totalGateOps_.load();
    stats.totalPatchesEvaluated = totalPatchesEvaluated_.load();
    stats.totalPatchesKept = totalPatchesKept_.load();
    stats.totalPatchesDropped = totalPatchesDropped_.load();
    stats.totalBudgetExhausted = totalBudgetExhausted_.load();

    if (stats.totalPatchesKept > 0) {
        stats.avgActivationKept = activationKeptAccum_ / stats.totalPatchesKept;
    }
    if (stats.totalPatchesDropped > 0) {
        stats.avgActivationDropped = activationDroppedAccum_ / stats.totalPatchesDropped;
    }
    if (stats.totalGateOperations > 0) {
        stats.avgBudgetUtilization = budgetUtilAccum_ / stats.totalGateOperations;
    }

    return stats;
}

void VisionTokenGate::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    totalGateOps_.store(0);
    totalPatchesEvaluated_.store(0);
    totalPatchesKept_.store(0);
    totalPatchesDropped_.store(0);
    totalBudgetExhausted_.store(0);
    activationKeptAccum_ = 0.0;
    activationDroppedAccum_ = 0.0;
    budgetUtilAccum_ = 0.0;
}

} // namespace Vision
} // namespace RawrXD
