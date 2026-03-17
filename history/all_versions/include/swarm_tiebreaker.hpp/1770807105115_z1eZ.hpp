// ============================================================================
// swarm_tiebreaker.hpp — Deterministic Tie-Breaking + Reproducibility
// ============================================================================
//
// Action Item #15: Seed control + deterministic tie-break policy.
//   - Stable sort by confidence then agent id
//   - Same input + same seed = same result across runs
//
// Integrates with:
//   - DeterministicSwarmEngine (core/deterministic_swarm.hpp)
//   - SwarmCoordinator (core/swarm_coordinator.h)
//   - ReasoningPipelineOrchestrator
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_SWARM_TIEBREAKER_H
#define RAWRXD_SWARM_TIEBREAKER_H

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>

// ============================================================================
// SwarmCandidate — one agent's output for tie-breaking
// ============================================================================
struct SwarmCandidate {
    std::string agentId;        // Unique agent identifier
    std::string output;         // Agent's response text
    float       confidence;     // Agent's self-reported confidence [0.0, 1.0]
    double      latencyMs;      // Response time
    int         votesReceived;  // Cross-agent votes (for ParallelVote mode)
    uint64_t    outputHash;     // FNV-1a hash of output for dedup
};

// ============================================================================
// TieBreakerPolicy — configurable deterministic resolution
// ============================================================================
enum class TieBreakerPolicy : uint8_t {
    ConfidenceThenId    = 0,  // Sort by confidence desc, then agent id asc (default)
    VotesThenConfidence = 1,  // Sort by votes desc, then confidence desc
    LatencyFirst        = 2,  // Fastest response wins ties
    LongestOutput       = 3,  // Longest (most detailed) output wins ties
    SeededRandom        = 4,  // Deterministic random using provided seed
};

// ============================================================================
// SwarmTieBreaker — stateless, deterministic
// ============================================================================
class SwarmTieBreaker {
public:
    /// Select the winning candidate from a set of tied candidates.
    /// Returns index into the candidates vector.
    /// Guarantees: same input order + same seed = same winner.
    static int resolve(std::vector<SwarmCandidate>& candidates,
                       TieBreakerPolicy policy = TieBreakerPolicy::ConfidenceThenId,
                       uint64_t seed = 0) {
        if (candidates.empty()) return -1;
        if (candidates.size() == 1) return 0;

        switch (policy) {
            case TieBreakerPolicy::ConfidenceThenId:
                return resolveByConfidenceThenId(candidates);

            case TieBreakerPolicy::VotesThenConfidence:
                return resolveByVotesThenConfidence(candidates);

            case TieBreakerPolicy::LatencyFirst:
                return resolveByLatency(candidates);

            case TieBreakerPolicy::LongestOutput:
                return resolveByLength(candidates);

            case TieBreakerPolicy::SeededRandom:
                return resolveBySeededRandom(candidates, seed);

            default:
                return resolveByConfidenceThenId(candidates);
        }
    }

    /// Deduplicate candidates by output hash (keep highest confidence per hash)
    static void dedup(std::vector<SwarmCandidate>& candidates) {
        if (candidates.size() < 2) return;

        // Sort by outputHash, then confidence desc for stable dedup
        std::stable_sort(candidates.begin(), candidates.end(),
            [](const SwarmCandidate& a, const SwarmCandidate& b) {
                if (a.outputHash != b.outputHash) return a.outputHash < b.outputHash;
                return a.confidence > b.confidence;
            });

        // Remove duplicates (keep first = highest confidence)
        auto it = std::unique(candidates.begin(), candidates.end(),
            [](const SwarmCandidate& a, const SwarmCandidate& b) {
                return a.outputHash == b.outputHash;
            });
        candidates.erase(it, candidates.end());
    }

    /// Rank all candidates by the given policy (returns sorted copy)
    static std::vector<int> rank(const std::vector<SwarmCandidate>& candidates,
                                  TieBreakerPolicy policy = TieBreakerPolicy::ConfidenceThenId) {
        std::vector<int> indices(candidates.size());
        for (size_t i = 0; i < indices.size(); ++i) indices[i] = static_cast<int>(i);

        std::stable_sort(indices.begin(), indices.end(),
            [&candidates, policy](int a, int b) {
                return compareCandidates(candidates[a], candidates[b], policy);
            });

        return indices;
    }

private:
    // ---- Policy implementations ----

    static int resolveByConfidenceThenId(std::vector<SwarmCandidate>& c) {
        std::stable_sort(c.begin(), c.end(),
            [](const SwarmCandidate& a, const SwarmCandidate& b) {
                if (a.confidence != b.confidence) return a.confidence > b.confidence;
                return a.agentId < b.agentId; // Lexicographic for determinism
            });
        return 0;
    }

    static int resolveByVotesThenConfidence(std::vector<SwarmCandidate>& c) {
        std::stable_sort(c.begin(), c.end(),
            [](const SwarmCandidate& a, const SwarmCandidate& b) {
                if (a.votesReceived != b.votesReceived) return a.votesReceived > b.votesReceived;
                if (a.confidence != b.confidence) return a.confidence > b.confidence;
                return a.agentId < b.agentId;
            });
        return 0;
    }

    static int resolveByLatency(std::vector<SwarmCandidate>& c) {
        std::stable_sort(c.begin(), c.end(),
            [](const SwarmCandidate& a, const SwarmCandidate& b) {
                if (a.latencyMs != b.latencyMs) return a.latencyMs < b.latencyMs;
                return a.agentId < b.agentId;
            });
        return 0;
    }

    static int resolveByLength(std::vector<SwarmCandidate>& c) {
        std::stable_sort(c.begin(), c.end(),
            [](const SwarmCandidate& a, const SwarmCandidate& b) {
                if (a.output.size() != b.output.size()) return a.output.size() > b.output.size();
                return a.agentId < b.agentId;
            });
        return 0;
    }

    static int resolveBySeededRandom(std::vector<SwarmCandidate>& c, uint64_t seed) {
        // Deterministic: hash seed with each agent id, pick lowest hash
        uint64_t bestHash = UINT64_MAX;
        int bestIdx = 0;
        for (size_t i = 0; i < c.size(); ++i) {
            uint64_t h = seed ^ 0x9E3779B97F4A7C15ULL;
            for (char ch : c[i].agentId) {
                h ^= static_cast<uint64_t>(ch);
                h *= 0x100000001b3ULL;
            }
            if (h < bestHash) {
                bestHash = h;
                bestIdx = static_cast<int>(i);
            }
        }
        return bestIdx;
    }

    static bool compareCandidates(const SwarmCandidate& a, const SwarmCandidate& b,
                                   TieBreakerPolicy policy) {
        switch (policy) {
            case TieBreakerPolicy::ConfidenceThenId:
                if (a.confidence != b.confidence) return a.confidence > b.confidence;
                return a.agentId < b.agentId;
            case TieBreakerPolicy::VotesThenConfidence:
                if (a.votesReceived != b.votesReceived) return a.votesReceived > b.votesReceived;
                if (a.confidence != b.confidence) return a.confidence > b.confidence;
                return a.agentId < b.agentId;
            case TieBreakerPolicy::LatencyFirst:
                if (a.latencyMs != b.latencyMs) return a.latencyMs < b.latencyMs;
                return a.agentId < b.agentId;
            case TieBreakerPolicy::LongestOutput:
                if (a.output.size() != b.output.size()) return a.output.size() > b.output.size();
                return a.agentId < b.agentId;
            default:
                return a.agentId < b.agentId;
        }
    }
};

#endif // RAWRXD_SWARM_TIEBREAKER_H
