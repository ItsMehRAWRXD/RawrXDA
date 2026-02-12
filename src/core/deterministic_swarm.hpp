// ============================================================================
// deterministic_swarm.hpp — Deterministic Swarm Reproducibility Engine
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Guarantees that: same input + seed = same result
//
// This is achieved by:
//   1. Seeding all RNG sources (model sampling, agent dispatch order)
//   2. Deterministic task ordering (topological sort, no racy dispatch)
//   3. Snapshot + replay of swarm execution traces
//   4. Hash-based result verification for regression detection
//
// Integration:
//   - SwarmCoordinator (core/swarm_coordinator.h)
//   - SwarmDecisionBridge (core/swarm_decision_bridge.h)
//   - ReasoningPipelineOrchestrator
//   - ChainOfThoughtEngine
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <functional>
#include "../core/model_memory_hotpatch.hpp"

// ============================================================================
// SwarmSeed — Reproducibility seed configuration
// ============================================================================
struct SwarmSeed {
    uint64_t    masterSeed;             // Master RNG seed
    uint64_t    samplerSeed;            // Seed for model temperature sampling
    uint64_t    dispatchSeed;           // Seed for agent dispatch ordering
    uint64_t    voteSeed;               // Seed for tie-breaking in vote modes
    bool        enforceOrdering;        // Force deterministic task dispatch order
    bool        disableParallelism;     // Force sequential execution for reproducibility
    bool        fixedTimestamps;        // Use synthetic timestamps (not wall clock)
    bool        isStrict = false;       // Compatibility: strict reproducibility mode

    SwarmSeed()
        : masterSeed(0),
          samplerSeed(0),
          dispatchSeed(0),
          voteSeed(0),
          enforceOrdering(true),
          disableParallelism(false),
          fixedTimestamps(false) {}

    /// Create a fully deterministic seed config from a single master seed
    static SwarmSeed fromMaster(uint64_t master) {
        SwarmSeed s;
        s.masterSeed = master;
        // Derive sub-seeds via simple hash mixing
        s.samplerSeed = master ^ 0x9E3779B97F4A7C15ULL;
        s.dispatchSeed = master ^ 0x6C62272E07BB0142ULL;
        s.voteSeed = master ^ 0xBF58476D1CE4E5B9ULL;
        s.enforceOrdering = true;
        s.disableParallelism = false;
        s.fixedTimestamps = false;
        return s;
    }

    /// Create a strict reproducibility mode (sequential, fixed timestamps)
    static SwarmSeed strict(uint64_t master) {
        SwarmSeed s = fromMaster(master);
        s.disableParallelism = true;
        s.fixedTimestamps = true;
        return s;
    }
};

// ============================================================================
// SwarmTraceEntry — One step in a swarm execution trace
// ============================================================================
struct SwarmTraceEntry {
    uint64_t    sequenceId;             // Monotonic sequence number
    uint64_t    timestamp;              // Wall clock or synthetic timestamp
    std::string agentId;                // Which agent produced this step
    std::string roleName;               // Reasoning role (thinker, critic, etc.)
    std::string input;                  // Input to the agent at this step
    std::string output;                 // Output from the agent
    uint64_t    inputHash;              // FNV-1a hash of input
    uint64_t    outputHash;             // FNV-1a hash of output
    double      durationMs;             // Execution time
    float       confidence;             // Agent's confidence in output
    int         depthUsed;              // Reasoning depth at this step
};

// ============================================================================
// SwarmTrace — Complete execution trace for replay/verification
// ============================================================================
struct SwarmTrace {
    std::string             traceId;
    uint64_t                id = 0;             // Compatibility: hash of traceId for display
    SwarmSeed               seed;
    std::string             originalInput;
    uint64_t                originalInputHash;
    std::string             finalOutput;
    uint64_t                finalOutputHash;
    std::vector<SwarmTraceEntry> entries;
    std::vector<SwarmTraceEntry> steps;         // Compatibility: alias/sync with entries
    double                  totalDurationMs;
    int                     agentCount;
    std::string             profileName;        // Reasoning profile used

    /// Compute a combined hash of all entries for integrity check
    uint64_t computeTraceHash() const;

    /// Check if this trace matches another (same seed → same result)
    bool matchesOutput(const SwarmTrace& other) const;
};

// ============================================================================
// SwarmReplayResult — Outcome of a trace replay
// ============================================================================
struct SwarmReplayResult {
    bool        reproducible;           // True if replay produced same output
    bool        partialMatch;           // True if trace diverged mid-way
    int         divergenceStep;         // Step index where divergence occurred (-1 if none)
    std::string originalOutput;
    std::string replayOutput;
    uint64_t    originalHash;
    uint64_t    replayHash;
    std::string detail;

    static SwarmReplayResult match(const std::string& output, uint64_t hash) {
        SwarmReplayResult r;
        r.reproducible = true;
        r.partialMatch = false;
        r.divergenceStep = -1;
        r.replayOutput = output;
        r.originalOutput = output;
        r.originalHash = hash;
        r.replayHash = hash;
        r.detail = "Deterministic replay verified";
        return r;
    }

    static SwarmReplayResult mismatch(int step, const std::string& orig,
                                       const std::string& replay) {
        SwarmReplayResult r;
        r.reproducible = false;
        r.partialMatch = (step > 0);
        r.divergenceStep = step;
        r.originalOutput = orig;
        r.replayOutput = replay;
        r.detail = "Replay diverged at step " + std::to_string(step);
        return r;
    }
};

// ============================================================================
// DeterministicSwarmEngine — Singleton
// ============================================================================
class DeterministicSwarmEngine {
public:
    static DeterministicSwarmEngine& instance();

    // ---- Seed Management ----
    void setSeed(const SwarmSeed& seed);
    SwarmSeed getSeed() const;
    void setMasterSeed(uint64_t seed);
    uint64_t getMasterSeed() const;

    // ---- Deterministic RNG ----
    /// Get next deterministic random value from the sampler stream
    uint64_t nextSamplerRandom();
    /// Get next deterministic random value from the dispatch stream
    uint64_t nextDispatchRandom();
    /// Get next deterministic value for vote tie-breaking
    uint64_t nextVoteRandom();
    /// Get a deterministic float [0.0, 1.0) for temperature scaling
    float nextSamplerFloat();

    // ---- Trace Recording ----
    /// Start recording a new trace
    void beginTrace(const std::string& input, const std::string& profileName);
    /// Record one execution step
    void recordStep(const std::string& agentId, const std::string& roleName,
                    const std::string& input, const std::string& output,
                    double durationMs, float confidence, int depth);
    /// Finalize trace with final output
    SwarmTrace endTrace(const std::string& finalOutput);

    // ---- Trace Replay ----
    /// Replay a recorded trace and verify reproducibility
    SwarmReplayResult replayTrace(const SwarmTrace& trace);

    // ---- Trace Persistence ----
    PatchResult saveTrace(const std::string& path, const SwarmTrace& trace) const;
    PatchResult loadTrace(const std::string& path, SwarmTrace& outTrace) const;

    // ---- Hash Utilities ----
    static uint64_t fnv1a(const std::string& data);
    static uint64_t fnv1a(const void* data, size_t len);

    // ---- Trace Library ----
    void storeTrace(const SwarmTrace& trace);
    bool getTrace(const std::string& traceId, SwarmTrace& out) const;
    std::vector<std::string> listTraceIds() const;
    void clearTraceLibrary();

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalTraces{0};
        std::atomic<uint64_t> totalReplays{0};
        std::atomic<uint64_t> reproducibleReplays{0};
        std::atomic<uint64_t> divergentReplays{0};
    };
    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    DeterministicSwarmEngine();
    ~DeterministicSwarmEngine() = default;
    DeterministicSwarmEngine(const DeterministicSwarmEngine&) = delete;
    DeterministicSwarmEngine& operator=(const DeterministicSwarmEngine&) = delete;

    // xorshift64* PRNG (deterministic, fast)
    struct PRNG {
        uint64_t state;
        uint64_t next() {
            state ^= state >> 12;
            state ^= state << 25;
            state ^= state >> 27;
            return state * 0x2545F4914F6CDD1DULL;
        }
        float nextFloat() {
            return static_cast<float>(next() >> 40) / static_cast<float>(1ULL << 24);
        }
    };

    mutable std::mutex m_mutex;
    SwarmSeed          m_seed;
    PRNG               m_samplerPRNG;
    PRNG               m_dispatchPRNG;
    PRNG               m_votePRNG;
    std::atomic<uint64_t> m_sequence{0};

    // Active trace being recorded
    SwarmTrace         m_activeTrace;
    bool               m_recording = false;

    // Trace library
    std::unordered_map<std::string, SwarmTrace> m_traceLibrary;

    Stats m_stats;
};
