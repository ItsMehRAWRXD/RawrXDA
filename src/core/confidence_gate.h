// ============================================================================
// confidence_gate.h — Phase 10D: Autonomous Confidence Layer
// ============================================================================
//
// Confidence-gated execution: every agent action carries a confidence score.
// The gate decides whether to execute, escalate, or self-abort based on
// configurable thresholds. Integrates with safety contracts and replay journal.
//
// Architecture:
//   - ConfidenceGate: threshold-based decision engine
//   - ConfidenceProfile: per-action-class threshold configuration
//   - ConfidenceDecision: gate output (execute/escalate/abort)
//   - ConfidenceHistory: rolling window for trend analysis
//
// Pattern:  Structured results, no exceptions
// Threading: All methods are thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef RAWRXD_CONFIDENCE_GATE_H
#define RAWRXD_CONFIDENCE_GATE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <deque>

// Forward declarations from other Phase 10 headers
enum class ActionClass;
enum class SafetyRiskTier;

// ============================================================================
// ENUMS
// ============================================================================

enum class ConfidenceDecision {
    Execute   = 0,  // Confidence is above threshold — proceed
    Escalate  = 1,  // Confidence is in the gray zone — ask user
    Abort     = 2,  // Confidence is below minimum — refuse
    Defer     = 3,  // Need more information before deciding
    Override  = 4   // User manually overrode the gate
};

enum class ConfidenceTrend {
    Stable    = 0,  // Confidence is steady
    Rising    = 1,  // Confidence is increasing over recent actions
    Falling   = 2,  // Confidence is decreasing — potential degradation
    Volatile  = 3,  // Confidence is oscillating wildly
    Unknown   = 4
};

enum class GatePolicy {
    Strict    = 0,  // High thresholds, aggressive abort
    Normal    = 1,  // Balanced thresholds
    Relaxed   = 2,  // Low thresholds, more permissive
    Disabled  = 3   // Gate is off — everything executes
};

// ============================================================================
// STRUCTS
// ============================================================================

struct ConfidenceThresholds {
    float executeThreshold;   // Above this → execute
    float escalateThreshold;  // Between escalate and execute → escalate
    float abortThreshold;     // Below this → abort (same as escalate floor)

    // Per-risk-tier adjustments (multiplied against base thresholds)
    float riskMultiplierNone;
    float riskMultiplierLow;
    float riskMultiplierMedium;
    float riskMultiplierHigh;
    float riskMultiplierCritical;

    ConfidenceThresholds()
        : executeThreshold(0.7f),
          escalateThreshold(0.4f),
          abortThreshold(0.2f),
          riskMultiplierNone(0.5f),
          riskMultiplierLow(0.7f),
          riskMultiplierMedium(1.0f),
          riskMultiplierHigh(1.2f),
          riskMultiplierCritical(1.5f) {}
};

struct ConfidenceProfile {
    ActionClass actionClass;
    float customExecuteThreshold;
    float customEscalateThreshold;
    float customAbortThreshold;
    bool useCustomThresholds;
    std::string description;

    ConfidenceProfile()
        : customExecuteThreshold(0.7f),
          customEscalateThreshold(0.4f),
          customAbortThreshold(0.2f),
          useCustomThresholds(false) {}
};

struct ConfidenceEvaluation {
    ConfidenceDecision decision;
    float rawConfidence;
    float adjustedConfidence;     // After risk multiplier
    float effectiveThreshold;     // The threshold that was applied
    SafetyRiskTier riskTier;
    ActionClass actionClass;
    std::string reason;
    std::string suggestion;
    ConfidenceTrend trend;
    double trendSlope;            // Rate of confidence change

    static ConfidenceEvaluation execute(float conf, float adjusted, float threshold, const std::string& reason) {
        ConfidenceEvaluation e;
        e.decision = ConfidenceDecision::Execute;
        e.rawConfidence = conf;
        e.adjustedConfidence = adjusted;
        e.effectiveThreshold = threshold;
        e.reason = reason;
        e.trend = ConfidenceTrend::Unknown;
        e.trendSlope = 0.0;
        return e;
    }

    static ConfidenceEvaluation escalate(float conf, float adjusted, float threshold, const std::string& reason) {
        ConfidenceEvaluation e;
        e.decision = ConfidenceDecision::Escalate;
        e.rawConfidence = conf;
        e.adjustedConfidence = adjusted;
        e.effectiveThreshold = threshold;
        e.reason = reason;
        e.trend = ConfidenceTrend::Unknown;
        e.trendSlope = 0.0;
        return e;
    }

    static ConfidenceEvaluation abort(float conf, float adjusted, float threshold, const std::string& reason) {
        ConfidenceEvaluation e;
        e.decision = ConfidenceDecision::Abort;
        e.rawConfidence = conf;
        e.adjustedConfidence = adjusted;
        e.effectiveThreshold = threshold;
        e.reason = reason;
        e.trend = ConfidenceTrend::Unknown;
        e.trendSlope = 0.0;
        return e;
    }
};

struct ConfidenceHistoryEntry {
    uint64_t sequenceId;
    float confidence;
    ConfidenceDecision decision;
    ActionClass actionClass;
    SafetyRiskTier riskTier;
    std::chrono::steady_clock::time_point timestamp;
};

struct ConfidenceGateStats {
    uint64_t totalEvaluations  = 0;
    uint64_t totalExecuted     = 0;
    uint64_t totalEscalated    = 0;
    uint64_t totalAborted      = 0;
    uint64_t totalDeferred     = 0;
    uint64_t totalOverridden   = 0;
    float avgConfidence        = 0.0f;
    float minConfidence        = 1.0f;
    float maxConfidence        = 0.0f;
    float recentAvgConfidence  = 0.0f;  // Last 50 actions
    ConfidenceTrend overallTrend = ConfidenceTrend::Unknown;
    GatePolicy currentPolicy   = GatePolicy::Normal;
};

// ============================================================================
// CONFIDENCE GATE — The Decision Engine
// ============================================================================

class ConfidenceGate {
public:
    static ConfidenceGate& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────
    bool init();
    void shutdown();
    void reset();

    // ── Core Gate Evaluation ───────────────────────────────────────────
    ConfidenceEvaluation evaluate(
        float confidence,
        ActionClass action,
        SafetyRiskTier risk,
        const std::string& description = "");

    // Quick check — returns true if would execute
    bool wouldExecute(float confidence, ActionClass action, SafetyRiskTier risk);

    // Evaluate and record
    ConfidenceEvaluation evaluateAndRecord(
        float confidence,
        ActionClass action,
        SafetyRiskTier risk,
        const std::string& description = "");

    // ── Threshold Configuration ────────────────────────────────────────
    void setThresholds(const ConfidenceThresholds& thresholds);
    ConfidenceThresholds getThresholds() const;
    void setExecuteThreshold(float threshold);
    void setEscalateThreshold(float threshold);
    void setAbortThreshold(float threshold);

    // ── Per-Action Profiles ────────────────────────────────────────────
    void setProfile(const ConfidenceProfile& profile);
    void removeProfile(ActionClass action);
    ConfidenceProfile getProfile(ActionClass action) const;
    std::vector<ConfidenceProfile> getAllProfiles() const;

    // ── Policy ─────────────────────────────────────────────────────────
    void setPolicy(GatePolicy policy);
    GatePolicy getPolicy() const;
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // ── Trend Analysis ─────────────────────────────────────────────────
    ConfidenceTrend analyzeTrend() const;
    double getTrendSlope() const;
    float getRecentAverage(size_t windowSize = 50) const;
    bool isConfidenceDegrading(float threshold = -0.01) const;

    // ── History ────────────────────────────────────────────────────────
    std::vector<ConfidenceHistoryEntry> getHistory(size_t maxEntries = 100) const;
    void clearHistory();

    // ── Overrides ──────────────────────────────────────────────────────
    void setEscalationCallback(std::function<bool(const ConfidenceEvaluation&)> cb);
    void setAutoEscalate(bool autoEscalate);  // Auto-approve escalations

    // ── Self-Abort ─────────────────────────────────────────────────────
    // Consecutive low-confidence actions trigger automatic session pause
    void setSelfAbortThreshold(int consecutiveLowActions);
    int getSelfAbortThreshold() const;
    bool isSelfAbortTriggered() const;
    void resetSelfAbort();

    // ── Stats & Reporting ──────────────────────────────────────────────
    ConfidenceGateStats getStats() const;
    std::string getStatusString() const;

    // ── Serialization ──────────────────────────────────────────────────
    std::string serializeThresholds() const;
    bool deserializeThresholds(const std::string& json);

private:
    ConfidenceGate();
    ~ConfidenceGate();

    // Helpers
    float getRiskMultiplier(SafetyRiskTier risk) const;
    float getEffectiveExecuteThreshold(ActionClass action, SafetyRiskTier risk) const;
    float getEffectiveEscalateThreshold(ActionClass action, SafetyRiskTier risk) const;
    float getEffectiveAbortThreshold(ActionClass action, SafetyRiskTier risk) const;
    void updateTrendAnalysis();
    void checkSelfAbort(ConfidenceDecision decision);

    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_enabled;

    ConfidenceThresholds m_thresholds;
    GatePolicy m_policy;

    // Per-action profiles
    std::unordered_map<int, ConfidenceProfile> m_profiles; // ActionClass int -> profile

    // History (rolling window)
    std::deque<ConfidenceHistoryEntry> m_history;
    size_t m_maxHistorySize;
    uint64_t m_nextHistoryId;

    // Self-abort tracking
    int m_consecutiveLowCount;
    int m_selfAbortThreshold;
    std::atomic<bool> m_selfAbortTriggered;

    // Stats
    ConfidenceGateStats m_stats;

    // Callbacks
    std::function<bool(const ConfidenceEvaluation&)> m_escalationCallback;
    bool m_autoEscalate;

    // Running sums for statistics
    double m_confidenceSum;
    uint64_t m_confidenceCount;

    static constexpr size_t DEFAULT_MAX_HISTORY = 10000;
    static constexpr int DEFAULT_SELF_ABORT_THRESHOLD = 5;
};

// ============================================================================
// UTILITY — Enum to string
// ============================================================================

inline const char* confidenceDecisionToString(ConfidenceDecision d) {
    switch (d) {
        case ConfidenceDecision::Execute:  return "Execute";
        case ConfidenceDecision::Escalate: return "Escalate";
        case ConfidenceDecision::Abort:    return "Abort";
        case ConfidenceDecision::Defer:    return "Defer";
        case ConfidenceDecision::Override: return "Override";
        default:                           return "Unknown";
    }
}

inline const char* confidenceTrendToString(ConfidenceTrend t) {
    switch (t) {
        case ConfidenceTrend::Stable:   return "Stable";
        case ConfidenceTrend::Rising:   return "Rising";
        case ConfidenceTrend::Falling:  return "Falling";
        case ConfidenceTrend::Volatile: return "Volatile";
        case ConfidenceTrend::Unknown:  return "Unknown";
        default:                        return "???";
    }
}

inline const char* gatePolicyToString(GatePolicy p) {
    switch (p) {
        case GatePolicy::Strict:   return "Strict";
        case GatePolicy::Normal:   return "Normal";
        case GatePolicy::Relaxed:  return "Relaxed";
        case GatePolicy::Disabled: return "Disabled";
        default:                   return "Unknown";
    }
}

#endif // RAWRXD_CONFIDENCE_GATE_H
