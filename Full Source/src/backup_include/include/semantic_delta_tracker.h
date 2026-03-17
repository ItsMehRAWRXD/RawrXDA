// ============================================================================
// semantic_delta_tracker.h — Semantic Delta Convergence Detection
// ============================================================================
// Measures when iterative inference passes stop adding meaningful information.
// This is the convergence detector: it tells the iterative engine "you've
// extracted enough signal — stop traversing."
//
// Convergence is measured via multiple /* signals */ public:
//   1. Token overlap: Jaccard similarity between successive pass outputs
//   2. Embedding distance: cosine similarity of output token embeddings
//   3. Entropy delta: change in output token distribution entropy
//   4. Structural stability: whether output structure (length, formatting) stabilized
//   5. Semantic hash: locality-sensitive hash of output for fast comparison
//
// The tracker maintains a sliding window of recent passes and detects
// convergence when the deltas fall below configurable thresholds.
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - No std::function — raw function pointers for callbacks
//   - Thread-safe singleton
//   - Fixed-size ring buffers (no unbounded allocation)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef SEMANTIC_DELTA_TRACKER_H
#define SEMANTIC_DELTA_TRACKER_H

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

// ============================================================================
// Enums
// ============================================================================

// ConvergenceSignal — Which convergence metric triggered
enum class ConvergenceSignal : uint8_t {
    None                = 0,
    TokenOverlap        = 1,   // Outputs are producing the same tokens
    EmbeddingDistance    = 2,   // Embedding vectors stopped moving
    EntropyStabilized   = 3,   // Output entropy is stable
    StructuralStable    = 4,   // Length/format stopped changing
    SemanticHashMatch   = 5,   // LSH fingerprints match
    AllConverged        = 6,   // All signals agree: converged
    Timeout             = 7,   // Time budget expired (forced convergence)
    UserInterrupt       = 8    // User requested stop
};

// ConvergenceState — Overall state of the convergence tracker
enum class ConvergenceState : uint8_t {
    NotStarted    = 0,   // No passes recorded yet
    Diverging     = 1,   // Passes are still producing different outputs
    Converging    = 2,   // Deltas are shrinking but not yet below threshold
    Converged     = 3,   // All signals agree: converged
    Oscillating   = 4,   // Deltas are bouncing up and down
    ForceStopped  = 5    // Stopped by timeout or user
};

// ============================================================================
// PassSnapshot — Captured output from one inference pass
// ============================================================================
struct PassSnapshot {
    uint32_t         passNumber;
    uint64_t         timestampMs;

    // Token-level output
    std::vector<uint32_t> tokenIds;      // Output token IDs
    uint32_t              tokenCount;     // Number of tokens

    // Embedding fingerprint (reduced dimensionality)
    // We store a small fingerprint, not the full embedding matrix
    float                 embeddingFingerprint[64];
    uint32_t              fingerprintDim;

    // Output entropy (from sampling distribution)
    float                 outputEntropy;

    // Structural features
    uint32_t              outputLength;    // Character count
    uint32_t              lineCount;       // Line count
    uint32_t              sentenceCount;   // Approximate sentence count

    // Semantic hash (64-bit LSH)
    uint64_t              semanticHash;

    // Raw text (truncated to maxSnapshotTextLen)
    char                  text[2048];
    uint32_t              textLen;
};

// ============================================================================
// DeltaMeasurement — Delta between two consecutive passes
// ============================================================================
struct DeltaMeasurement {
    uint32_t    passA;
    uint32_t    passB;

    // Per-signal deltas (0.0 = identical, 1.0 = completely different)
    float       tokenOverlap;        // 1.0 - Jaccard(tokensA, tokensB)
    float       embeddingDistance;    // 1.0 - cosine_sim(embA, embB)
    float       entropyDelta;        // |entropyA - entropyB| normalized
    float       structuralDelta;     // Combined length/line/sentence delta
    float       semanticHashDelta;   // Hamming distance of LSH / 64

    // Composite delta (weighted blend)
    float       compositeDelta;

    // Whether this delta represents convergence
    bool        isConverged;

    uint64_t    timestampMs;
};

// ============================================================================
// DeltaTrackerConfig — Configuration
// ============================================================================
struct DeltaTrackerConfig {
    // Convergence thresholds (below these = converged)
    float       tokenOverlapThreshold     = 0.10f;  // 90% token overlap
    float       embeddingDistThreshold    = 0.05f;  // 95% cosine similarity
    float       entropyDeltaThreshold     = 0.02f;  // 2% entropy change
    float       structuralDeltaThreshold  = 0.05f;  // 5% structural change
    float       semanticHashThreshold     = 0.10f;  // 10% hash distance

    // Composite threshold (if compositeDelta < this, converged)
    float       compositeThreshold        = 0.08f;

    // Weights for composite delta
    float       weightTokenOverlap        = 0.30f;
    float       weightEmbeddingDist       = 0.25f;
    float       weightEntropyDelta        = 0.15f;
    float       weightStructuralDelta     = 0.15f;
    float       weightSemanticHash        = 0.15f;

    // Minimum consecutive converged passes before declaring full convergence
    uint32_t    minConsecutiveConverged   = 2;

    // Maximum passes before forced convergence
    uint32_t    maxPasses                 = 50;

    // Oscillation detection: if delta increases N times in a row, flag it
    uint32_t    oscillationWindow         = 4;

    // Maximum snapshot text length
    uint32_t    maxSnapshotTextLen        = 2048;

    // Embedding fingerprint dimension (for fast comparison)
    uint32_t    fingerprintDim            = 64;
};

// ============================================================================
// Convergence callback (function pointer, NOT std::function)
// ============================================================================
typedef void (*ConvergenceCallback)(
    ConvergenceState  state,
    ConvergenceSignal signal,
    float             compositeDelta,
    uint32_t          passNumber,
    void*             userData
);

// ============================================================================
// SemanticDeltaTracker — Main class (singleton)
// ============================================================================
class SemanticDeltaTracker {
public:
    static SemanticDeltaTracker& instance();

    // ----- Lifecycle -----
    PatchResult initialize(const DeltaTrackerConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- Pass Recording -----
    // Record a new pass snapshot for delta computation
    PatchResult recordPass(const PassSnapshot& snapshot);

    // Convenience: record from raw text and token IDs
    PatchResult recordPassFromText(uint32_t passNumber,
                                    const char* text, uint32_t textLen,
                                    const uint32_t* tokenIds, uint32_t tokenCount,
                                    float outputEntropy);

    // ----- Convergence Query -----
    ConvergenceState   getState() const;
    ConvergenceSignal  getLastSignal() const;
    float              getLastCompositeDelta() const;
    uint32_t           getPassCount() const;
    bool               isConverged() const;

    // Get the latest delta measurement
    PatchResult getLastDelta(DeltaMeasurement* outDelta) const;

    // Get delta history (most recent first)
    PatchResult getDeltaHistory(DeltaMeasurement* outBuf, uint32_t* outCount,
                                 uint32_t maxCount) const;

    // ----- Force States -----
    PatchResult forceConverged(ConvergenceSignal reason);
    PatchResult reset();

    // ----- Callback -----
    PatchResult registerCallback(ConvergenceCallback cb, void* userData);
    PatchResult clearCallback();

    // ----- Statistics -----
    struct Stats {
        std::atomic<uint64_t> passesRecorded{0};
        std::atomic<uint64_t> deltasComputed{0};
        std::atomic<uint64_t> convergenceDetections{0};
        std::atomic<uint64_t> oscillationDetections{0};
        std::atomic<uint64_t> forcedStops{0};
    };

    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    SemanticDeltaTracker();
    ~SemanticDeltaTracker();
    SemanticDeltaTracker(const SemanticDeltaTracker&) = delete;
    SemanticDeltaTracker& operator=(const SemanticDeltaTracker&) = delete;

    // ----- Internal delta computation -----
    float computeTokenOverlap(const PassSnapshot& a, const PassSnapshot& b) const;
    float computeEmbeddingDistance(const PassSnapshot& a, const PassSnapshot& b) const;
    float computeEntropyDelta(const PassSnapshot& a, const PassSnapshot& b) const;
    float computeStructuralDelta(const PassSnapshot& a, const PassSnapshot& b) const;
    float computeSemanticHashDelta(const PassSnapshot& a, const PassSnapshot& b) const;
    float computeCompositeDelta(const DeltaMeasurement& dm) const;

    DeltaMeasurement computeDelta(const PassSnapshot& a, const PassSnapshot& b) const;

    // ----- LSH hashing -----
    uint64_t computeSemanticHash(const char* text, uint32_t len) const;

    // ----- Embedding fingerprint -----
    void computeFingerprint(const uint32_t* tokenIds, uint32_t count,
                             float* outFingerprint, uint32_t dim) const;

    // ----- Convergence evaluation -----
    void evaluateConvergence(const DeltaMeasurement& delta);
    void detectOscillation();
    void notifyCallback(ConvergenceSignal signal, float delta, uint32_t pass);

    // ----- Members -----
    std::atomic<bool>              m_initialized{false};
    mutable std::mutex             m_mutex;

    DeltaTrackerConfig             m_config;
    ConvergenceState               m_state;
    ConvergenceSignal              m_lastSignal;
    float                          m_lastCompositeDelta;

    // Snapshot ring buffer
    static constexpr size_t MAX_SNAPSHOTS = 64;
    static constexpr size_t SNAPSHOT_MASK = MAX_SNAPSHOTS - 1;
    PassSnapshot                   m_snapshots[MAX_SNAPSHOTS];
    uint32_t                       m_snapshotCount;
    uint32_t                       m_snapshotHead;

    // Delta history
    static constexpr size_t MAX_DELTAS = 128;
    static constexpr size_t DELTA_MASK = MAX_DELTAS - 1;
    DeltaMeasurement               m_deltas[MAX_DELTAS];
    uint32_t                       m_deltaCount;
    uint32_t                       m_deltaHead;

    // Consecutive convergence counter
    uint32_t                       m_consecutiveConverged;

    // Oscillation detection
    uint32_t                       m_increasingDeltaCount;

    // Callback
    ConvergenceCallback            m_callback;
    void*                          m_callbackUserData;

    Stats                          m_stats;
};

#endif // SEMANTIC_DELTA_TRACKER_H
