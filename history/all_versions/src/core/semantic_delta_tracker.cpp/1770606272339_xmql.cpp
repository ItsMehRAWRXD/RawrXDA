// ============================================================================
// semantic_delta_tracker.cpp — Semantic Delta Convergence Detection Impl
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "semantic_delta_tracker.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>

// ============================================================================
// Singleton
// ============================================================================

SemanticDeltaTracker& SemanticDeltaTracker::instance() {
    static SemanticDeltaTracker s_instance;
    return s_instance;
}

SemanticDeltaTracker::SemanticDeltaTracker()
    : m_state(ConvergenceState::NotStarted)
    , m_lastSignal(ConvergenceSignal::None)
    , m_lastCompositeDelta(1.0f)
    , m_snapshotCount(0)
    , m_snapshotHead(0)
    , m_deltaCount(0)
    , m_deltaHead(0)
    , m_consecutiveConverged(0)
    , m_increasingDeltaCount(0)
    , m_callback(nullptr)
    , m_callbackUserData(nullptr)
{
    std::memset(m_snapshots, 0, sizeof(m_snapshots));
    std::memset(m_deltas, 0, sizeof(m_deltas));
}

SemanticDeltaTracker::~SemanticDeltaTracker() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult SemanticDeltaTracker::initialize(const DeltaTrackerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("SemanticDeltaTracker already initialized", -1);
    }

    m_config               = config;
    m_state                = ConvergenceState::NotStarted;
    m_lastSignal           = ConvergenceSignal::None;
    m_lastCompositeDelta   = 1.0f;
    m_snapshotCount        = 0;
    m_snapshotHead         = 0;
    m_deltaCount           = 0;
    m_deltaHead            = 0;
    m_consecutiveConverged = 0;
    m_increasingDeltaCount = 0;

    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("SemanticDeltaTracker initialized");
}

void SemanticDeltaTracker::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized.store(false, std::memory_order_release);
    m_state    = ConvergenceState::NotStarted;
    m_callback = nullptr;
}

// ============================================================================
// Pass Recording
// ============================================================================

PatchResult SemanticDeltaTracker::recordPass(const PassSnapshot& snapshot) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Store snapshot in ring buffer
    uint32_t idx = m_snapshotHead;
    m_snapshots[idx] = snapshot;
    m_snapshotHead = (m_snapshotHead + 1) & SNAPSHOT_MASK;
    if (m_snapshotCount < MAX_SNAPSHOTS) m_snapshotCount++;

    m_stats.passesRecorded.fetch_add(1, std::memory_order_relaxed);

    // If we have at least 2 snapshots, compute delta
    if (m_snapshotCount >= 2) {
        uint32_t prevIdx = (idx == 0) ? (MAX_SNAPSHOTS - 1) : (idx - 1);
        // Only compute if prev is actually filled
        if (m_snapshotCount >= 2) {
            DeltaMeasurement delta = computeDelta(m_snapshots[prevIdx], m_snapshots[idx]);

            // Store delta in ring buffer
            uint32_t dIdx = m_deltaHead;
            m_deltas[dIdx] = delta;
            m_deltaHead = (m_deltaHead + 1) & DELTA_MASK;
            if (m_deltaCount < MAX_DELTAS) m_deltaCount++;

            m_lastCompositeDelta = delta.compositeDelta;
            m_stats.deltasComputed.fetch_add(1, std::memory_order_relaxed);

            // Evaluate convergence
            evaluateConvergence(delta);
            detectOscillation();
        }
    }

    // First pass: move to Diverging state
    if (m_state == ConvergenceState::NotStarted) {
        m_state = ConvergenceState::Diverging;
    }

    // Check max passes
    if (m_snapshotCount >= m_config.maxPasses) {
        m_state = ConvergenceState::ForceStopped;
        m_lastSignal = ConvergenceSignal::Timeout;
        m_stats.forcedStops.fetch_add(1, std::memory_order_relaxed);
        notifyCallback(ConvergenceSignal::Timeout, m_lastCompositeDelta, snapshot.passNumber);
    }

    return PatchResult::ok("Pass recorded");
}

PatchResult SemanticDeltaTracker::recordPassFromText(uint32_t passNumber,
                                                      const char* text, uint32_t textLen,
                                                      const uint32_t* tokenIds, uint32_t tokenCount,
                                                      float outputEntropy) {
    if (!text) return PatchResult::error("Null text pointer", -2);

    PassSnapshot snapshot;
    std::memset(&snapshot, 0, sizeof(snapshot));

    snapshot.passNumber   = passNumber;
    snapshot.timestampMs  = GetTickCount64();
    snapshot.outputEntropy = outputEntropy;
    snapshot.tokenCount    = tokenCount;

    // Copy token IDs
    if (tokenIds && tokenCount > 0) {
        snapshot.tokenIds.assign(tokenIds, tokenIds + tokenCount);
    }

    // Copy text (truncated)
    uint32_t copyLen = std::min(textLen, m_config.maxSnapshotTextLen - 1);
    std::memcpy(snapshot.text, text, copyLen);
    snapshot.text[copyLen] = '\0';
    snapshot.textLen = copyLen;

    // Compute structural features
    snapshot.outputLength = textLen;
    snapshot.lineCount    = 1;
    snapshot.sentenceCount = 1;
    for (uint32_t i = 0; i < copyLen; ++i) {
        if (text[i] == '\n') snapshot.lineCount++;
        if (text[i] == '.' || text[i] == '!' || text[i] == '?') snapshot.sentenceCount++;
    }

    // Compute semantic hash
    snapshot.semanticHash = computeSemanticHash(text, copyLen);

    // Compute embedding fingerprint
    snapshot.fingerprintDim = std::min(m_config.fingerprintDim, 64u);
    if (tokenIds && tokenCount > 0) {
        computeFingerprint(tokenIds, tokenCount,
                            snapshot.embeddingFingerprint, snapshot.fingerprintDim);
    }

    return recordPass(snapshot);
}

// ============================================================================
// Convergence Query
// ============================================================================

ConvergenceState SemanticDeltaTracker::getState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

ConvergenceSignal SemanticDeltaTracker::getLastSignal() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastSignal;
}

float SemanticDeltaTracker::getLastCompositeDelta() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastCompositeDelta;
}

uint32_t SemanticDeltaTracker::getPassCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshotCount;
}

bool SemanticDeltaTracker::isConverged() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_state == ConvergenceState::Converged || m_state == ConvergenceState::ForceStopped);
}

PatchResult SemanticDeltaTracker::getLastDelta(DeltaMeasurement* outDelta) const {
    if (!outDelta) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_deltaCount == 0) {
        return PatchResult::error("No deltas computed yet", -3);
    }

    uint32_t lastIdx = (m_deltaHead == 0) ? (MAX_DELTAS - 1) : (m_deltaHead - 1);
    *outDelta = m_deltas[lastIdx];
    return PatchResult::ok("Last delta retrieved");
}

PatchResult SemanticDeltaTracker::getDeltaHistory(DeltaMeasurement* outBuf, uint32_t* outCount,
                                                    uint32_t maxCount) const {
    if (!outBuf || !outCount) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t available = std::min(m_deltaCount, maxCount);
    *outCount = available;

    for (uint32_t i = 0; i < available; ++i) {
        uint32_t idx = (m_deltaHead - 1 - i) & DELTA_MASK;
        outBuf[i] = m_deltas[idx];
    }
    return PatchResult::ok("Delta history retrieved");
}

// ============================================================================
// Force States
// ============================================================================

PatchResult SemanticDeltaTracker::forceConverged(ConvergenceSignal reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state      = ConvergenceState::Converged;
    m_lastSignal = reason;
    m_stats.forcedStops.fetch_add(1, std::memory_order_relaxed);
    notifyCallback(reason, m_lastCompositeDelta, m_snapshotCount);
    return PatchResult::ok("Forced converged");
}

PatchResult SemanticDeltaTracker::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_state                = ConvergenceState::NotStarted;
    m_lastSignal           = ConvergenceSignal::None;
    m_lastCompositeDelta   = 1.0f;
    m_snapshotCount        = 0;
    m_snapshotHead         = 0;
    m_deltaCount           = 0;
    m_deltaHead            = 0;
    m_consecutiveConverged = 0;
    m_increasingDeltaCount = 0;
    return PatchResult::ok("Tracker reset");
}

// ============================================================================
// Callback
// ============================================================================

PatchResult SemanticDeltaTracker::registerCallback(ConvergenceCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = cb;
    m_callbackUserData = userData;
    return PatchResult::ok("Callback registered");
}

PatchResult SemanticDeltaTracker::clearCallback() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = nullptr;
    m_callbackUserData = nullptr;
    return PatchResult::ok("Callback cleared");
}

// ============================================================================
// Internal Delta Computation
// ============================================================================

float SemanticDeltaTracker::computeTokenOverlap(const PassSnapshot& a, const PassSnapshot& b) const {
    if (a.tokenIds.empty() && b.tokenIds.empty()) return 0.0f;
    if (a.tokenIds.empty() || b.tokenIds.empty()) return 1.0f;

    // Jaccard distance: 1 - |A ∩ B| / |A ∪ B|
    std::set<uint32_t> setA(a.tokenIds.begin(), a.tokenIds.end());
    std::set<uint32_t> setB(b.tokenIds.begin(), b.tokenIds.end());

    uint32_t intersection = 0;
    for (uint32_t tok : setA) {
        if (setB.count(tok)) intersection++;
    }

    uint32_t unionSize = static_cast<uint32_t>(setA.size() + setB.size()) - intersection;
    if (unionSize == 0) return 0.0f;

    float jaccard = static_cast<float>(intersection) / unionSize;
    return 1.0f - jaccard;  // 0 = identical, 1 = completely different
}

float SemanticDeltaTracker::computeEmbeddingDistance(const PassSnapshot& a, const PassSnapshot& b) const {
    // Cosine distance of fingerprints
    uint32_t dim = std::min(a.fingerprintDim, b.fingerprintDim);
    if (dim == 0) return 1.0f;

    double dotProd = 0.0;
    double normA   = 0.0;
    double normB   = 0.0;

    for (uint32_t i = 0; i < dim; ++i) {
        double va = a.embeddingFingerprint[i];
        double vb = b.embeddingFingerprint[i];
        dotProd += va * vb;
        normA   += va * va;
        normB   += vb * vb;
    }

    double denom = std::sqrt(normA) * std::sqrt(normB);
    if (denom < 1e-10) return 1.0f;

    float cosineSim = static_cast<float>(dotProd / denom);
    return 1.0f - std::max(0.0f, std::min(cosineSim, 1.0f));
}

float SemanticDeltaTracker::computeEntropyDelta(const PassSnapshot& a, const PassSnapshot& b) const {
    float maxEntropy = std::max(a.outputEntropy, b.outputEntropy);
    if (maxEntropy < 1e-6f) return 0.0f;
    return std::abs(a.outputEntropy - b.outputEntropy) / maxEntropy;
}

float SemanticDeltaTracker::computeStructuralDelta(const PassSnapshot& a, const PassSnapshot& b) const {
    // Combine length, line count, and sentence count deltas
    float lengthDelta = 0.0f;
    float lineDelta   = 0.0f;
    float sentDelta   = 0.0f;

    uint32_t maxLen  = std::max(a.outputLength, b.outputLength);
    uint32_t maxLine = std::max(a.lineCount, b.lineCount);
    uint32_t maxSent = std::max(a.sentenceCount, b.sentenceCount);

    if (maxLen > 0) {
        lengthDelta = static_cast<float>(std::abs(static_cast<int>(a.outputLength) - 
                       static_cast<int>(b.outputLength))) / maxLen;
    }
    if (maxLine > 0) {
        lineDelta = static_cast<float>(std::abs(static_cast<int>(a.lineCount) - 
                     static_cast<int>(b.lineCount))) / maxLine;
    }
    if (maxSent > 0) {
        sentDelta = static_cast<float>(std::abs(static_cast<int>(a.sentenceCount) - 
                     static_cast<int>(b.sentenceCount))) / maxSent;
    }

    return (lengthDelta + lineDelta + sentDelta) / 3.0f;
}

float SemanticDeltaTracker::computeSemanticHashDelta(const PassSnapshot& a, const PassSnapshot& b) const {
    // Hamming distance of 64-bit semantic hashes, normalized to 0-1
    uint64_t xorResult = a.semanticHash ^ b.semanticHash;
    uint32_t bitDiff = 0;

    // Popcount
    while (xorResult) {
        bitDiff += xorResult & 1;
        xorResult >>= 1;
    }

    return static_cast<float>(bitDiff) / 64.0f;
}

float SemanticDeltaTracker::computeCompositeDelta(const DeltaMeasurement& dm) const {
    return dm.tokenOverlap      * m_config.weightTokenOverlap
         + dm.embeddingDistance  * m_config.weightEmbeddingDist
         + dm.entropyDelta      * m_config.weightEntropyDelta
         + dm.structuralDelta   * m_config.weightStructuralDelta
         + dm.semanticHashDelta * m_config.weightSemanticHash;
}

DeltaMeasurement SemanticDeltaTracker::computeDelta(const PassSnapshot& a, const PassSnapshot& b) const {
    DeltaMeasurement dm;
    dm.passA = a.passNumber;
    dm.passB = b.passNumber;

    dm.tokenOverlap      = computeTokenOverlap(a, b);
    dm.embeddingDistance  = computeEmbeddingDistance(a, b);
    dm.entropyDelta      = computeEntropyDelta(a, b);
    dm.structuralDelta   = computeStructuralDelta(a, b);
    dm.semanticHashDelta = computeSemanticHashDelta(a, b);

    dm.compositeDelta    = computeCompositeDelta(dm);
    dm.isConverged       = (dm.compositeDelta < m_config.compositeThreshold);
    dm.timestampMs       = GetTickCount64();

    return dm;
}

// ============================================================================
// LSH Hashing (Simple SimHash for fast comparison)
// ============================================================================

uint64_t SemanticDeltaTracker::computeSemanticHash(const char* text, uint32_t len) const {
    if (!text || len == 0) return 0;

    // Simple SimHash: for each bit position, accumulate weighted hash
    int64_t v[64];
    std::memset(v, 0, sizeof(v));

    // Use trigram hashing for locality sensitivity
    for (uint32_t i = 0; i + 2 < len; ++i) {
        // FNV-1a hash of trigram
        uint64_t h = 14695981039346656037ULL;
        h ^= static_cast<uint8_t>(text[i]);
        h *= 1099511628211ULL;
        h ^= static_cast<uint8_t>(text[i + 1]);
        h *= 1099511628211ULL;
        h ^= static_cast<uint8_t>(text[i + 2]);
        h *= 1099511628211ULL;

        for (int b = 0; b < 64; ++b) {
            if (h & (1ULL << b)) {
                v[b]++;
            } else {
                v[b]--;
            }
        }
    }

    // Convert to bits
    uint64_t hash = 0;
    for (int b = 0; b < 64; ++b) {
        if (v[b] > 0) hash |= (1ULL << b);
    }

    return hash;
}

// ============================================================================
// Embedding Fingerprint (Hashing trick for dimensionality reduction)
// ============================================================================

void SemanticDeltaTracker::computeFingerprint(const uint32_t* tokenIds, uint32_t count,
                                                float* outFingerprint, uint32_t dim) const {
    if (!outFingerprint || dim == 0) return;
    std::memset(outFingerprint, 0, dim * sizeof(float));

    // Random projection via hash: hash each token to a dimension and accumulate
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t h = tokenIds[i] * 2654435761ULL;
        uint32_t bucket = static_cast<uint32_t>(h % dim);
        float sign = (h & (1ULL << 32)) ? 1.0f : -1.0f;
        outFingerprint[bucket] += sign;
    }

    // Normalize
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; ++i) {
        norm += outFingerprint[i] * outFingerprint[i];
    }
    norm = std::sqrt(norm);
    if (norm > 1e-6f) {
        for (uint32_t i = 0; i < dim; ++i) {
            outFingerprint[i] /= norm;
        }
    }
}

// ============================================================================
// Convergence Evaluation
// ============================================================================

void SemanticDeltaTracker::evaluateConvergence(const DeltaMeasurement& delta) {
    if (delta.isConverged) {
        m_consecutiveConverged++;

        // Determine which signal triggered
        ConvergenceSignal signal = ConvergenceSignal::None;
        if (delta.tokenOverlap < m_config.tokenOverlapThreshold)
            signal = ConvergenceSignal::TokenOverlap;
        else if (delta.embeddingDistance < m_config.embeddingDistThreshold)
            signal = ConvergenceSignal::EmbeddingDistance;
        else if (delta.entropyDelta < m_config.entropyDeltaThreshold)
            signal = ConvergenceSignal::EntropyStabilized;
        else if (delta.structuralDelta < m_config.structuralDeltaThreshold)
            signal = ConvergenceSignal::StructuralStable;
        else if (delta.semanticHashDelta < m_config.semanticHashThreshold)
            signal = ConvergenceSignal::SemanticHashMatch;
        else
            signal = ConvergenceSignal::AllConverged;

        m_lastSignal = signal;

        if (m_consecutiveConverged >= m_config.minConsecutiveConverged) {
            m_state      = ConvergenceState::Converged;
            m_lastSignal = ConvergenceSignal::AllConverged;
            m_stats.convergenceDetections.fetch_add(1, std::memory_order_relaxed);
            notifyCallback(ConvergenceSignal::AllConverged, delta.compositeDelta, delta.passB);
        } else {
            m_state = ConvergenceState::Converging;
            notifyCallback(signal, delta.compositeDelta, delta.passB);
        }
    } else {
        m_consecutiveConverged = 0;
        m_state = ConvergenceState::Diverging;
        m_lastSignal = ConvergenceSignal::None;
    }
}

void SemanticDeltaTracker::detectOscillation() {
    if (m_deltaCount < 2) return;

    // Check if the last delta was larger than the one before it
    uint32_t lastIdx = (m_deltaHead == 0) ? (MAX_DELTAS - 1) : (m_deltaHead - 1);
    uint32_t prevIdx = (lastIdx == 0) ? (MAX_DELTAS - 1) : (lastIdx - 1);

    if (m_deltas[lastIdx].compositeDelta > m_deltas[prevIdx].compositeDelta) {
        m_increasingDeltaCount++;
    } else {
        m_increasingDeltaCount = 0;
    }

    if (m_increasingDeltaCount >= m_config.oscillationWindow) {
        m_state = ConvergenceState::Oscillating;
        m_stats.oscillationDetections.fetch_add(1, std::memory_order_relaxed);
        m_increasingDeltaCount = 0;
    }
}

void SemanticDeltaTracker::notifyCallback(ConvergenceSignal signal, float delta, uint32_t pass) {
    if (m_callback) {
        m_callback(m_state, signal, delta, pass, m_callbackUserData);
    }
}

// ============================================================================
// Statistics
// ============================================================================

void SemanticDeltaTracker::resetStats() {
    m_stats.passesRecorded.store(0, std::memory_order_relaxed);
    m_stats.deltasComputed.store(0, std::memory_order_relaxed);
    m_stats.convergenceDetections.store(0, std::memory_order_relaxed);
    m_stats.oscillationDetections.store(0, std::memory_order_relaxed);
    m_stats.forcedStops.store(0, std::memory_order_relaxed);
}
