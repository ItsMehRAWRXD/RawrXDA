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

// VRAM pressure monitoring — MASM64 kernels (vram_pressure_monitor.asm)
extern "C" {
    int32_t rawrxd_poll_vram_pressure(uint64_t current_usage_bytes);
    int32_t rawrxd_check_swap_trigger(uint32_t pressure_threshold);
}

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
// DraftTree helpers
// ============================================================================

std::vector<int>
SpeculativeDecoderV2::DraftTree::pathTokenIds(int idx) const {
    std::vector<int> path;
    while (idx >= 0 && nodes[static_cast<size_t>(idx)].depth > 0) {
        path.push_back(nodes[static_cast<size_t>(idx)].tokenId);
        idx = nodes[static_cast<size_t>(idx)].parentIdx;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<float>
SpeculativeDecoderV2::DraftTree::pathLogprobs(int idx) const {
    std::vector<float> path;
    while (idx >= 0 && nodes[static_cast<size_t>(idx)].depth > 0) {
        path.push_back(nodes[static_cast<size_t>(idx)].logprob);
        idx = nodes[static_cast<size_t>(idx)].parentIdx;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ============================================================================
// draftTree — BFS tree generation
//
// Each node at depth d expands to max(1, branchFactor >> (d-1)) children.
// The total tree width thus decays geometrically, bounding VRAM usage while
// still providing many candidate paths for the verifier to pick from.
// ============================================================================
SpeculativeDecoderV2::DraftTree
SpeculativeDecoderV2::draftTree(const std::vector<int>& context,
                                 int depth, int branchFactor) {
    DraftTree tree;

    // Virtual root at index 0 (depth 0, no token)
    DraftTreeNode root;
    root.depth    = 0;
    root.parentIdx = -1;
    tree.nodes.push_back(root);

    // BFS frontier: list of (nodeIdx, context-for-that-node)
    struct Frame {
        int nodeIdx;
        std::vector<int> ctx;
    };
    std::vector<Frame> frontier;
    frontier.push_back({0, context});

    for (int d = 1; d <= depth && !m_abortRequested.load(); ++d) {
        // Shrink branching factor with depth: depth 1 = branchFactor,
        // depth k = max(1, branchFactor >> (k-1))
        int kBranch = std::max(1, branchFactor >> (d - 1));

        std::vector<Frame> nextFrontier;
        nextFrontier.reserve(frontier.size() * static_cast<size_t>(kBranch));

        for (auto& frame : frontier) {
            auto logprobs = m_draftModel.logprobs(
                frame.ctx, kBranch, m_draftModel.userData);
            if (logprobs.empty()) continue;

            // Record first child index in parent node
            int firstChildIdx = static_cast<int>(tree.nodes.size());
            tree.nodes[static_cast<size_t>(frame.nodeIdx)].firstChild  = firstChildIdx;
            tree.nodes[static_cast<size_t>(frame.nodeIdx)].childCount  = 0;

            int spawned = 0;
            for (int k = 0; k < kBranch && k < static_cast<int>(logprobs.size()); ++k) {
                DraftTreeNode child;
                child.tokenId  = logprobs[static_cast<size_t>(k)].first;
                child.logprob  = logprobs[static_cast<size_t>(k)].second;
                child.parentIdx = frame.nodeIdx;
                child.depth    = d;

                int childIdx = static_cast<int>(tree.nodes.size());
                tree.nodes.push_back(child);
                tree.nodes[static_cast<size_t>(frame.nodeIdx)].childCount++;
                ++spawned;

                // Only extend leaves deeper if below max depth
                if (d < depth) {
                    std::vector<int> childCtx = frame.ctx;
                    childCtx.push_back(child.tokenId);
                    nextFrontier.push_back({childIdx, std::move(childCtx)});
                } else {
                    tree.leafIndices.push_back(childIdx);
                }
            }
            (void)spawned;
        }
        frontier = std::move(nextFrontier);
    }

    // Any nodes with no children are leaves (e.g., if the model returned
    // fewer candidates than requested)
    for (int i = 1; i < static_cast<int>(tree.nodes.size()); ++i) {
        if (tree.nodes[static_cast<size_t>(i)].childCount == 0 &&
            tree.nodes[static_cast<size_t>(i)].firstChild == -1) {
            // Only add to leafIndices if not already there (from depth-exact leaves)
            bool alreadyLeaf = false;
            for (int li : tree.leafIndices)
                if (li == i) { alreadyLeaf = true; break; }
            if (!alreadyLeaf)
                tree.leafIndices.push_back(i);
        }
    }

    return tree;
}

// ============================================================================
// draftTreeSeeded — ensemble variant: perturbs candidate selection with seed
//
// When ensembleDrafts > 1, each fan-out call uses a different branchSeed.
// The seed is XORed into the token selection offset, rotating which top-K
// logprob candidates are chosen at each node — yielding diverse draft paths
// without requiring multiple draft models.
// ============================================================================
SpeculativeDecoderV2::DraftTree
SpeculativeDecoderV2::draftTreeSeeded(const std::vector<int>& context,
                                       int depth, int branchFactor,
                                       uint32_t branchSeed) {
    DraftTree tree;

    DraftTreeNode root;
    root.depth     = 0;
    root.parentIdx = -1;
    tree.nodes.push_back(root);

    struct Frame {
        int              nodeIdx;
        std::vector<int> ctx;
        uint32_t         seed;   // per-node seed carried forward
    };
    std::vector<Frame> frontier;
    frontier.push_back({0, context, branchSeed});

    for (int d = 1; d <= depth && !m_abortRequested.load(); ++d) {
        int kBranch   = std::max(1, branchFactor >> (d - 1));
        // Request extra candidates so the seed can pick different subsets
        int kRequest  = std::min(kBranch * 3, 32);

        std::vector<Frame> nextFrontier;
        nextFrontier.reserve(frontier.size() * static_cast<size_t>(kBranch));

        for (auto& frame : frontier) {
            auto logprobs = m_draftModel.logprobs(
                frame.ctx, kRequest, m_draftModel.userData);
            if (logprobs.empty()) continue;

            // Rotate the selection start by seed so different ensemble members
            // pick different candidate subsets from the same logprob list.
            size_t startOff = frame.seed % std::max<size_t>(1, logprobs.size());
            uint32_t childSeed = frame.seed * 2654435761u; // LCG advance

            int firstChildIdx = static_cast<int>(tree.nodes.size());
            tree.nodes[static_cast<size_t>(frame.nodeIdx)].firstChild = firstChildIdx;
            tree.nodes[static_cast<size_t>(frame.nodeIdx)].childCount = 0;

            for (int k = 0; k < kBranch; ++k) {
                size_t sourceIdx = (startOff + static_cast<size_t>(k))
                                   % logprobs.size();
                DraftTreeNode child;
                child.tokenId   = logprobs[sourceIdx].first;
                child.logprob   = logprobs[sourceIdx].second;
                child.parentIdx = frame.nodeIdx;
                child.depth     = d;

                int childIdx = static_cast<int>(tree.nodes.size());
                tree.nodes.push_back(child);
                tree.nodes[static_cast<size_t>(frame.nodeIdx)].childCount++;

                if (d < depth) {
                    std::vector<int> childCtx = frame.ctx;
                    childCtx.push_back(child.tokenId);
                    nextFrontier.push_back({childIdx, std::move(childCtx), childSeed});
                } else {
                    tree.leafIndices.push_back(childIdx);
                }
                childSeed = childSeed * 1664525u + 1013904223u;
            }
        }
        frontier = std::move(nextFrontier);
    }

    // Nodes with childCount == 0 and not in leafIndices are also leaves
    for (int i = 1; i < static_cast<int>(tree.nodes.size()); ++i) {
        auto& n = tree.nodes[static_cast<size_t>(i)];
        if (n.childCount == 0 && n.firstChild == -1) {
            bool already = false;
            for (int li : tree.leafIndices)
                if (li == i) { already = true; break; }
            if (!already) tree.leafIndices.push_back(i);
        }
    }

    return tree;
}

// ============================================================================
// verifyTree — find the path with the longest accepted prefix
// ============================================================================
SpeculativeDecoderV2::VerifyResult
SpeculativeDecoderV2::verifyTree(const std::vector<int>& context,
                                  const DraftTree& tree) {
    static thread_local std::mt19937 rng(std::random_device{}());

    VerifyResult bestResult;
    bestResult.acceptedCount = -1;  // sentinel: nothing tried yet
    std::vector<int>   bestPathIds;
    std::vector<float> bestPathLprobs;

    for (int leafIdx : tree.leafIndices) {
        // Reconstruct the path sequence
        auto pathIds    = tree.pathTokenIds(leafIdx);
        auto pathLprobs = tree.pathLogprobs(leafIdx);

        if (pathIds.empty()) continue;

        // Build a DraftResult along this path and call verify()
        DraftResult dr;
        dr.tokenIds = pathIds;
        dr.logprobs = pathLprobs;

        VerifyResult vr = verify(context, dr);

        // Accept the path that contributes the most tokens
        if (vr.acceptedCount > bestResult.acceptedCount ||
            (vr.acceptedCount == bestResult.acceptedCount && vr.allAccepted)) {
            bestResult     = vr;
            bestPathIds    = pathIds;
            bestPathLprobs = pathLprobs;
        }

        // Short-circuit: a fully accepted path of maximum depth is optimal
        if (vr.allAccepted &&
            vr.acceptedCount >= m_config.treeDepth) {
            break;
        }
    }

    // Populate the accepted token sequence so generateStreaming()'s accept
    // phase can read them without re-walking the tree.
    if (bestResult.acceptedCount > 0 && !bestPathIds.empty()) {
        int n = std::min(bestResult.acceptedCount,
                         static_cast<int>(bestPathIds.size()));
        bestResult.acceptedTokenIds.assign(bestPathIds.begin(),
                                            bestPathIds.begin() + n);
        bestResult.acceptedLogprobs.assign(bestPathLprobs.begin(),
                                            bestPathLprobs.begin() + n);
    }

    if (bestResult.acceptedCount < 0) {
        // Fallback: generate one token from target directly
        auto logprobs = m_targetModel.logprobs(context, 1, m_targetModel.userData);
        bestResult.acceptedCount = 0;
        bestResult.allAccepted   = false;
        if (!logprobs.empty()) {
            bestResult.correctionToken.id     = logprobs[0].first;
            bestResult.correctionToken.logprob = logprobs[0].second;
            if (m_targetModel.decode)
                bestResult.correctionToken.text =
                    m_targetModel.decode(logprobs[0].first, m_targetModel.userData);
        }
    }

    return bestResult;
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

    // Fallback: sequential verification with KV-delta caching.
    // Each context in the draft window is an extension of the previous one;
    // results computed in prior iterations are reused on cache hit, avoiding
    // redundant re-encoding of already-verified prefixes.
    static thread_local std::mt19937 rng(std::random_device{}());
    verifyCtx = context;

    for (size_t i = 0; i < drafted.tokenIds.size(); ++i) {
        // --- KV-delta cache lookup ---
        const auto* cached = m_kvDelta.lookup(verifyCtx);
        std::vector<std::pair<int, float>> targetLogprobs;
        if (cached) {
            targetLogprobs = *cached;
        } else {
            targetLogprobs = m_targetModel.logprobs(verifyCtx, 10, m_targetModel.userData);
            if (!targetLogprobs.empty()) {
                m_kvDelta.insert(verifyCtx, targetLogprobs);
            }
        }

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

    // All accepted — get one bonus token from target (check cache first)
    result.allAccepted = true;
    {
        const auto* cached = m_kvDelta.lookup(verifyCtx);
        std::vector<std::pair<int, float>> targetLogprobs;
        if (cached) {
            targetLogprobs = *cached;
        } else {
            targetLogprobs = m_targetModel.logprobs(verifyCtx, 1, m_targetModel.userData);
            if (!targetLogprobs.empty()) {
                m_kvDelta.insert(verifyCtx, targetLogprobs);
            }
        }
        if (!targetLogprobs.empty()) {
            result.correctionToken.id = targetLogprobs[0].first;
            result.correctionToken.logprob = targetLogprobs[0].second;
            if (m_targetModel.decode) {
                result.correctionToken.text = m_targetModel.decode(
                    targetLogprobs[0].first, m_targetModel.userData);
            }
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

    // VRAM pressure: when memory is tight, shorten drafts to avoid OOM;
    // when memory is plentiful and vram budget is known, allow longer drafts.
    constexpr uint64_t kVramHi = 40ULL * 1024ULL * 1024ULL * 1024ULL; // 40 GB
    constexpr uint64_t kVramLo = 32ULL * 1024ULL * 1024ULL * 1024ULL; // 32 GB
    uint64_t vram = m_vramUsageBytes.load(std::memory_order_relaxed);
    if (vram > kVramHi) {
        m_stats.currentDraftLen =
            std::max(m_config.minDraftTokens, m_stats.currentDraftLen - 1);
    } else if (vram > 0 && vram < kVramLo &&
               m_stats.currentDraftLen < m_config.maxDraftTokens) {
        m_stats.currentDraftLen =
            std::min(m_config.maxDraftTokens, m_stats.currentDraftLen + 1);
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

    // Cross-generation KV-prefix reuse: if this generation's prompt is a
    // strict prefix extension of the previous call's context, the cached
    // KV-delta entries for the shared prefix are still valid — skip the reset.
    {
        bool prefixReuse = !m_lastContext.empty() &&
            promptTokens.size() >= m_lastContext.size() &&
            std::equal(m_lastContext.begin(), m_lastContext.end(),
                       promptTokens.begin());
        if (!prefixReuse) m_kvDelta.reset();
    }

    std::vector<Token> outputTokens;
    std::vector<int> context = promptTokens;
    int generated = 0;

    while (generated < maxNewTokens && !m_abortRequested.load()) {
        int draftLen = m_stats.currentDraftLen;

        DraftResult  drafted;
        VerifyResult verified;
        float draftMs = 0.0f, verifyMs = 0.0f;
        int   draftedCount = 0;

        if (m_config.treeSpeculation) {
            // ---- Tree (Ensemble) Draft Phase ----
            auto t0 = std::chrono::high_resolution_clock::now();

            // Fan-out: generate ensembleDrafts independent trees and pick the
            // one whose best path has the highest accepted-token count.
            const int nEnsemble = std::max(1, m_config.ensembleDrafts);
            VerifyResult bestVerified;
            bestVerified.acceptedCount = -1;
            int bestDraftedCount = 0;

            for (int e = 0; e < nEnsemble && !m_abortRequested.load(); ++e) {
                DraftTree tree;
                if (e == 0) {
                    // Canonical tree — same deterministic BFS as before
                    tree = draftTree(context, m_config.treeDepth,
                                     m_config.treeBranching);
                } else {
                    // Seeded variant: rotates candidate selection by member index
                    uint32_t seed = static_cast<uint32_t>(e) * 2654435761u;
                    tree = draftTreeSeeded(context, m_config.treeDepth,
                                           m_config.treeBranching, seed);
                }

                int treeDraftCount = static_cast<int>(tree.nodes.size()) - 1;

                auto vr = verifyTree(context, tree);
                if (vr.acceptedCount > bestVerified.acceptedCount ||
                    (vr.acceptedCount == bestVerified.acceptedCount && vr.allAccepted)) {
                    bestVerified   = vr;
                    bestDraftedCount = treeDraftCount;
                }

                // Short-circuit: can't do better than a fully accepted max-depth tree
                if (bestVerified.allAccepted &&
                    bestVerified.acceptedCount >= m_config.treeDepth) break;
            }

            auto t1 = std::chrono::high_resolution_clock::now();
            draftMs      = std::chrono::duration<float, std::milli>(t1 - t0).count();
            verifyMs     = 0.0f;   // verify cost is folded into the above loop
            draftedCount = bestDraftedCount;
            verified     = bestVerified;
        } else {
            // ---- Linear Draft Phase ----
            auto t0  = std::chrono::high_resolution_clock::now();
            drafted  = draft(context, draftLen);
            auto t1  = std::chrono::high_resolution_clock::now();
            draftMs      = std::chrono::duration<float, std::milli>(t1 - t0).count();
            draftedCount = static_cast<int>(drafted.tokenIds.size());

            // ---- Linear Verify Phase ----
            auto t2  = std::chrono::high_resolution_clock::now();
            verified = verify(context, drafted);
            auto t3  = std::chrono::high_resolution_clock::now();
            verifyMs = std::chrono::duration<float, std::milli>(t3 - t2).count();
        }

        m_draftLatencyAvg.push(draftMs);
        m_verifyLatencyAvg.push(verifyMs);

        // ---- Accept Phase ----
        // For tree speculation, verifyTree() stores the accepted token IDs in
        // verified.acceptedTokenIds; for the linear path, fall back to drafted.
        const auto& acceptedIds    = verified.acceptedTokenIds.empty()
            ? drafted.tokenIds : verified.acceptedTokenIds;
        const auto& acceptedLprobs = verified.acceptedLogprobs.empty()
            ? drafted.logprobs : verified.acceptedLogprobs;

        for (int i = 0; i < verified.acceptedCount && generated < maxNewTokens; ++i) {
            Token tok;
            tok.id     = acceptedIds[i];
            tok.logprob = acceptedLprobs.size() > static_cast<size_t>(i)
                          ? acceptedLprobs[i] : 0.0f;
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
        m_acceptRateAvg.push(draftedCount == 0 ? 0.0f :
            static_cast<float>(verified.acceptedCount) /
            static_cast<float>(draftedCount));

        updateStats(draftedCount, verified.acceptedCount, draftMs, verifyMs);

        // Adjust draft length adaptively (also consults VRAM pressure)
        adjustDraftLength();

        // Check for EOS (token id 0 or special stop tokens)
        if (!outputTokens.empty() && outputTokens.back().id == 0) {
            break;
        }
    }

    // Record end-of-generation context for cross-generation KV-prefix reuse.
    m_lastContext = context;
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
