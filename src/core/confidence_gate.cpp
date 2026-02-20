// ============================================================================
// confidence_gate.cpp — Phase 10D: Autonomous Confidence Layer
// ============================================================================
//
// Full implementation of confidence-gated execution with threshold tuning,
// per-action profiles, risk multipliers, trend analysis, and self-abort.
//
// Pattern:  Structured results, no exceptions
// Threading: All public methods are mutex-guarded
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "confidence_gate.h"
#include "agent_safety_contract.h" // For ActionClass and SafetyRiskTier enums
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

// ============================================================================
// SINGLETON
// ============================================================================

ConfidenceGate& ConfidenceGate::instance() {
    static ConfidenceGate inst;
    return inst;
}

ConfidenceGate::ConfidenceGate()
    : m_initialized(false),
      m_enabled(true),
      m_policy(GatePolicy::Normal),
      m_maxHistorySize(DEFAULT_MAX_HISTORY),
      m_nextHistoryId(1),
      m_consecutiveLowCount(0),
      m_selfAbortThreshold(DEFAULT_SELF_ABORT_THRESHOLD),
      m_selfAbortTriggered(false),
      m_autoEscalate(false),
      m_confidenceSum(0.0),
      m_confidenceCount(0) {}

ConfidenceGate::~ConfidenceGate() {
    shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool ConfidenceGate::init() {
    if (m_initialized.load()) return true;

    std::lock_guard<std::mutex> lock(m_mutex);

    m_thresholds = ConfidenceThresholds();
    m_profiles.clear();
    m_history.clear();
    m_stats = ConfidenceGateStats();
    m_consecutiveLowCount = 0;
    m_selfAbortTriggered.store(false);
    m_confidenceSum = 0.0;
    m_confidenceCount = 0;

    m_initialized.store(true);
    return true;
}

void ConfidenceGate::shutdown() {
    if (!m_initialized.load()) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized.store(false);
}

void ConfidenceGate::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_stats = ConfidenceGateStats();
    m_consecutiveLowCount = 0;
    m_selfAbortTriggered.store(false);
    m_confidenceSum = 0.0;
    m_confidenceCount = 0;
    m_nextHistoryId = 1;
}

// ============================================================================
// CORE GATE EVALUATION
// ============================================================================

ConfidenceEvaluation ConfidenceGate::evaluate(
    float confidence,
    ActionClass action,
    SafetyRiskTier risk,
    const std::string& description)
{
    if (!m_initialized.load()) init();

    std::lock_guard<std::mutex> lock(m_mutex);

    m_stats.totalEvaluations++;

    // ── Gate disabled → always execute ──────────────────────────────────
    if (!m_enabled.load() || m_policy == GatePolicy::Disabled) {
        m_stats.totalExecuted++;
        return ConfidenceEvaluation::execute(confidence, confidence, 0.0f,
            "Gate disabled — auto-execute");
    }

    // ── Self-abort check ────────────────────────────────────────────────
    if (m_selfAbortTriggered.load()) {
        m_stats.totalAborted++;
        return ConfidenceEvaluation::abort(confidence, confidence, 0.0f,
            "Self-abort triggered after " + std::to_string(m_selfAbortThreshold) +
            " consecutive low-confidence actions. Call resetSelfAbort() to resume.");
    }

    // ── Clamp confidence to [0.0, 1.0] ─────────────────────────────────
    float clampedConf = std::max(0.0f, std::min(1.0f, confidence));

    // ── Apply risk multiplier ───────────────────────────────────────────
    float riskMult = getRiskMultiplier(risk);
    float adjustedConf = clampedConf; // Confidence stays the same; threshold adjusts

    // ── Get effective thresholds ────────────────────────────────────────
    float execThreshold    = getEffectiveExecuteThreshold(action, risk);
    float escalateThreshold = getEffectiveEscalateThreshold(action, risk);
    float abortThreshold   = getEffectiveAbortThreshold(action, risk);

    // ── Apply policy multiplier ─────────────────────────────────────────
    switch (m_policy) {
        case GatePolicy::Strict:
            execThreshold     *= 1.2f;
            escalateThreshold *= 1.2f;
            abortThreshold    *= 1.2f;
            break;
        case GatePolicy::Relaxed:
            execThreshold     *= 0.7f;
            escalateThreshold *= 0.7f;
            abortThreshold    *= 0.7f;
            break;
        default:
            break;
    }

    // Clamp thresholds
    execThreshold     = std::min(execThreshold, 1.0f);
    escalateThreshold = std::min(escalateThreshold, execThreshold);
    abortThreshold    = std::min(abortThreshold, escalateThreshold);

    // ── Decision logic ──────────────────────────────────────────────────
    ConfidenceEvaluation eval;
    eval.rawConfidence = clampedConf;
    eval.adjustedConfidence = adjustedConf;
    eval.riskTier = risk;
    eval.actionClass = action;

    // Analyze trend
    eval.trend = analyzeTrend();
    eval.trendSlope = getTrendSlope();

    if (adjustedConf >= execThreshold) {
        // ── EXECUTE ─────────────────────────────────────────────────────
        eval.decision = ConfidenceDecision::Execute;
        eval.effectiveThreshold = execThreshold;
        eval.reason = "Confidence " + std::to_string(adjustedConf) +
                      " >= execute threshold " + std::to_string(execThreshold);

        if (eval.trend == ConfidenceTrend::Falling) {
            eval.suggestion = "Warning: confidence trending downward";
        }

        m_stats.totalExecuted++;
    } else if (adjustedConf >= escalateThreshold) {
        // ── ESCALATE ────────────────────────────────────────────────────
        eval.decision = ConfidenceDecision::Escalate;
        eval.effectiveThreshold = escalateThreshold;
        eval.reason = "Confidence " + std::to_string(adjustedConf) +
                      " in escalation zone [" + std::to_string(escalateThreshold) +
                      ", " + std::to_string(execThreshold) + ")";
        eval.suggestion = "User confirmation recommended for '" +
                          std::string(actionClassToString(action)) + "'";

        // Check auto-escalate
        if (m_autoEscalate) {
            eval.decision = ConfidenceDecision::Override;
            eval.reason += " (auto-escalated)";
            m_stats.totalOverridden++;
        } else if (m_escalationCallback) {
            bool approved = m_escalationCallback(eval);
            if (approved) {
                eval.decision = ConfidenceDecision::Override;
                eval.reason += " (user approved)";
                m_stats.totalOverridden++;
            }
        }

        if (eval.decision == ConfidenceDecision::Escalate) {
            m_stats.totalEscalated++;
        }
    } else if (adjustedConf >= abortThreshold) {
        // ── DEFER ───────────────────────────────────────────────────────
        eval.decision = ConfidenceDecision::Defer;
        eval.effectiveThreshold = abortThreshold;
        eval.reason = "Confidence " + std::to_string(adjustedConf) +
                      " in defer zone [" + std::to_string(abortThreshold) +
                      ", " + std::to_string(escalateThreshold) + ")";
        eval.suggestion = "Gather more context before attempting this action";
        m_stats.totalDeferred++;
    } else {
        // ── ABORT ───────────────────────────────────────────────────────
        eval.decision = ConfidenceDecision::Abort;
        eval.effectiveThreshold = abortThreshold;
        eval.reason = "Confidence " + std::to_string(adjustedConf) +
                      " < abort threshold " + std::to_string(abortThreshold);
        eval.suggestion = "Confidence too low — action blocked for safety";
        m_stats.totalAborted++;
    }

    // ── Update running stats ────────────────────────────────────────────
    m_confidenceSum += adjustedConf;
    m_confidenceCount++;
    m_stats.avgConfidence = (float)(m_confidenceSum / m_confidenceCount);
    if (adjustedConf < m_stats.minConfidence) m_stats.minConfidence = adjustedConf;
    if (adjustedConf > m_stats.maxConfidence) m_stats.maxConfidence = adjustedConf;

    return eval;
}

bool ConfidenceGate::wouldExecute(float confidence, ActionClass action, SafetyRiskTier risk) {
    ConfidenceEvaluation eval = evaluate(confidence, action, risk);
    return (eval.decision == ConfidenceDecision::Execute ||
            eval.decision == ConfidenceDecision::Override);
}

ConfidenceEvaluation ConfidenceGate::evaluateAndRecord(
    float confidence,
    ActionClass action,
    SafetyRiskTier risk,
    const std::string& description)
{
    ConfidenceEvaluation eval = evaluate(confidence, action, risk, description);

    // Record in history
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        ConfidenceHistoryEntry entry;
        entry.sequenceId = m_nextHistoryId++;
        entry.confidence = eval.adjustedConfidence;
        entry.decision = eval.decision;
        entry.actionClass = action;
        entry.riskTier = risk;
        entry.timestamp = std::chrono::steady_clock::now();

        m_history.push_back(entry);
        while (m_history.size() > m_maxHistorySize) {
            m_history.pop_front();
        }

        // Check self-abort
        checkSelfAbort(eval.decision);

        // Update trend
        updateTrendAnalysis();
    }

    return eval;
}

// ============================================================================
// THRESHOLD CONFIGURATION
// ============================================================================

void ConfidenceGate::setThresholds(const ConfidenceThresholds& thresholds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds = thresholds;
}

ConfidenceThresholds ConfidenceGate::getThresholds() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_thresholds;
}

void ConfidenceGate::setExecuteThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds.executeThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void ConfidenceGate::setEscalateThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds.escalateThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

void ConfidenceGate::setAbortThreshold(float threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_thresholds.abortThreshold = std::max(0.0f, std::min(1.0f, threshold));
}

// ============================================================================
// PER-ACTION PROFILES
// ============================================================================

void ConfidenceGate::setProfile(const ConfidenceProfile& profile) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profiles[(int)profile.actionClass] = profile;
}

void ConfidenceGate::removeProfile(ActionClass action) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_profiles.erase((int)action);
}

ConfidenceProfile ConfidenceGate::getProfile(ActionClass action) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_profiles.find((int)action);
    if (it != m_profiles.end()) return it->second;
    return ConfidenceProfile();
}

std::vector<ConfidenceProfile> ConfidenceGate::getAllProfiles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConfidenceProfile> result;
    for (const auto& kv : m_profiles) {
        result.push_back(kv.second);
    }
    return result;
}

// ============================================================================
// POLICY
// ============================================================================

void ConfidenceGate::setPolicy(GatePolicy policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policy = policy;
    m_stats.currentPolicy = policy;
}

GatePolicy ConfidenceGate::getPolicy() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_policy;
}

void ConfidenceGate::setEnabled(bool enabled) {
    m_enabled.store(enabled);
}

bool ConfidenceGate::isEnabled() const {
    return m_enabled.load();
}

// ============================================================================
// TREND ANALYSIS
// ============================================================================

ConfidenceTrend ConfidenceGate::analyzeTrend() const {
    // Already under lock in evaluate()
    if (m_history.size() < 10) return ConfidenceTrend::Unknown;

    // Look at last 20 entries
    size_t window = std::min((size_t)20, m_history.size());
    size_t start = m_history.size() - window;

    // Simple linear regression over the window
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (size_t i = 0; i < window; i++) {
        double x = (double)i;
        double y = (double)m_history[start + i].confidence;
        sumX  += x;
        sumY  += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double n = (double)window;
    double denom = n * sumX2 - sumX * sumX;
    if (std::abs(denom) < 1e-10) return ConfidenceTrend::Stable;

    double slope = (n * sumXY - sumX * sumY) / denom;

    // Also check variance for volatility
    double mean = sumY / n;
    double variance = 0;
    for (size_t i = 0; i < window; i++) {
        double diff = (double)m_history[start + i].confidence - mean;
        variance += diff * diff;
    }
    variance /= n;

    if (variance > 0.05) return ConfidenceTrend::Volatile;
    if (slope > 0.01)    return ConfidenceTrend::Rising;
    if (slope < -0.01)   return ConfidenceTrend::Falling;
    return ConfidenceTrend::Stable;
}

double ConfidenceGate::getTrendSlope() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.size() < 10) return 0.0;

    size_t window = std::min((size_t)20, m_history.size());
    size_t start = m_history.size() - window;

    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (size_t i = 0; i < window; i++) {
        double x = (double)i;
        double y = (double)m_history[start + i].confidence;
        sumX  += x;
        sumY  += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double n = (double)window;
    double denom = n * sumX2 - sumX * sumX;
    if (std::abs(denom) < 1e-10) return 0.0;

    return (n * sumXY - sumX * sumY) / denom;
}

float ConfidenceGate::getRecentAverage(size_t windowSize) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_history.empty()) return 0.0f;

    size_t count = std::min(windowSize, m_history.size());
    size_t start = m_history.size() - count;
    double sum = 0;
    for (size_t i = start; i < m_history.size(); i++) {
        sum += m_history[i].confidence;
    }
    return (float)(sum / count);
}

bool ConfidenceGate::isConfidenceDegrading(float threshold) const {
    double slope = getTrendSlope();
    return (slope < threshold);
}

// ============================================================================
// HISTORY
// ============================================================================

std::vector<ConfidenceHistoryEntry> ConfidenceGate::getHistory(size_t maxEntries) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ConfidenceHistoryEntry> result;

    size_t count = std::min(maxEntries, m_history.size());
    size_t start = m_history.size() - count;
    for (size_t i = start; i < m_history.size(); i++) {
        result.push_back(m_history[i]);
    }
    return result;
}

void ConfidenceGate::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    m_nextHistoryId = 1;
}

// ============================================================================
// OVERRIDES
// ============================================================================

void ConfidenceGate::setEscalationCallback(std::function<bool(const ConfidenceEvaluation&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_escalationCallback = cb;
}

void ConfidenceGate::setAutoEscalate(bool autoEscalate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_autoEscalate = autoEscalate;
}

// ============================================================================
// SELF-ABORT
// ============================================================================

void ConfidenceGate::setSelfAbortThreshold(int consecutiveLowActions) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_selfAbortThreshold = std::max(1, consecutiveLowActions);
}

int ConfidenceGate::getSelfAbortThreshold() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_selfAbortThreshold;
}

bool ConfidenceGate::isSelfAbortTriggered() const {
    return m_selfAbortTriggered.load();
}

void ConfidenceGate::resetSelfAbort() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consecutiveLowCount = 0;
    m_selfAbortTriggered.store(false);
}

// ============================================================================
// STATS & REPORTING
// ============================================================================

ConfidenceGateStats ConfidenceGate::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    ConfidenceGateStats s = m_stats;
    s.recentAvgConfidence = 0.0f;

    // Calculate recent average
    if (!m_history.empty()) {
        size_t window = std::min((size_t)50, m_history.size());
        size_t start = m_history.size() - window;
        double sum = 0;
        for (size_t i = start; i < m_history.size(); i++) {
            sum += m_history[i].confidence;
        }
        s.recentAvgConfidence = (float)(sum / window);
    }

    return s;
}

std::string ConfidenceGate::getStatusString() const {
    auto s = getStats();
    auto t = getThresholds();

    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Confidence Gate Status (Phase 10D)\n"
        << "════════════════════════════════════════════\n"
        << "  Enabled:            " << (m_enabled.load() ? "YES" : "NO") << "\n"
        << "  Policy:             " << gatePolicyToString(s.currentPolicy) << "\n"
        << "  Self-Abort:         " << (m_selfAbortTriggered.load() ? "TRIGGERED" : "OK") << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Thresholds:\n"
        << "    Execute:          " << std::fixed << std::setprecision(2) << t.executeThreshold << "\n"
        << "    Escalate:         " << t.escalateThreshold << "\n"
        << "    Abort:            " << t.abortThreshold << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Evaluations:        " << s.totalEvaluations << "\n"
        << "  Executed:           " << s.totalExecuted << "\n"
        << "  Escalated:          " << s.totalEscalated << "\n"
        << "  Aborted:            " << s.totalAborted << "\n"
        << "  Deferred:           " << s.totalDeferred << "\n"
        << "  Overridden:         " << s.totalOverridden << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Confidence:\n"
        << "    Average:          " << std::fixed << std::setprecision(3) << s.avgConfidence << "\n"
        << "    Recent (50):      " << s.recentAvgConfidence << "\n"
        << "    Min:              " << s.minConfidence << "\n"
        << "    Max:              " << s.maxConfidence << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Trend:              " << confidenceTrendToString(s.overallTrend) << "\n"
        << "  History Size:       " << m_history.size() << "\n"
        << "  Profiles:           " << m_profiles.size() << "\n"
        << "  Consec. Low:        " << m_consecutiveLowCount << " / " << m_selfAbortThreshold << "\n"
        << "════════════════════════════════════════════";
    return oss.str();
}

// ============================================================================
// SERIALIZATION
// ============================================================================

std::string ConfidenceGate::serializeThresholds() const {
    auto t = getThresholds();
    std::ostringstream oss;
    oss << "{"
        << "\"executeThreshold\":" << std::fixed << std::setprecision(3) << t.executeThreshold << ","
        << "\"escalateThreshold\":" << t.escalateThreshold << ","
        << "\"abortThreshold\":" << t.abortThreshold << ","
        << "\"riskMultiplierNone\":" << t.riskMultiplierNone << ","
        << "\"riskMultiplierLow\":" << t.riskMultiplierLow << ","
        << "\"riskMultiplierMedium\":" << t.riskMultiplierMedium << ","
        << "\"riskMultiplierHigh\":" << t.riskMultiplierHigh << ","
        << "\"riskMultiplierCritical\":" << t.riskMultiplierCritical
        << "}";
    return oss.str();
}

bool ConfidenceGate::deserializeThresholds(const std::string& json) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto extractFloat = [&json](const std::string& key) -> float {
        std::string searchKey = "\"" + key + "\":";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return -1.0f;
        pos += searchKey.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        std::string numStr;
        while (pos < json.length() && (json[pos] >= '0' && json[pos] <= '9' || json[pos] == '.' || json[pos] == '-')) {
            numStr += json[pos++];
        }
        if (numStr.empty()) return -1.0f;
        return std::stof(numStr);
    };

    float val;
    val = extractFloat("executeThreshold");       if (val >= 0) m_thresholds.executeThreshold = val;
    val = extractFloat("escalateThreshold");      if (val >= 0) m_thresholds.escalateThreshold = val;
    val = extractFloat("abortThreshold");         if (val >= 0) m_thresholds.abortThreshold = val;
    val = extractFloat("riskMultiplierNone");     if (val >= 0) m_thresholds.riskMultiplierNone = val;
    val = extractFloat("riskMultiplierLow");      if (val >= 0) m_thresholds.riskMultiplierLow = val;
    val = extractFloat("riskMultiplierMedium");   if (val >= 0) m_thresholds.riskMultiplierMedium = val;
    val = extractFloat("riskMultiplierHigh");     if (val >= 0) m_thresholds.riskMultiplierHigh = val;
    val = extractFloat("riskMultiplierCritical"); if (val >= 0) m_thresholds.riskMultiplierCritical = val;

    return true;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

float ConfidenceGate::getRiskMultiplier(SafetyRiskTier risk) const {
    // Already under lock
    switch (risk) {
        case SafetyRiskTier::None:     return m_thresholds.riskMultiplierNone;
        case SafetyRiskTier::Low:      return m_thresholds.riskMultiplierLow;
        case SafetyRiskTier::Medium:   return m_thresholds.riskMultiplierMedium;
        case SafetyRiskTier::High:     return m_thresholds.riskMultiplierHigh;
        case SafetyRiskTier::Critical: return m_thresholds.riskMultiplierCritical;
        default:                       return 1.0f;
    }
}

float ConfidenceGate::getEffectiveExecuteThreshold(ActionClass action, SafetyRiskTier risk) const {
    // Check for custom profile first
    auto it = m_profiles.find((int)action);
    if (it != m_profiles.end() && it->second.useCustomThresholds) {
        return it->second.customExecuteThreshold * getRiskMultiplier(risk);
    }
    return m_thresholds.executeThreshold * getRiskMultiplier(risk);
}

float ConfidenceGate::getEffectiveEscalateThreshold(ActionClass action, SafetyRiskTier risk) const {
    auto it = m_profiles.find((int)action);
    if (it != m_profiles.end() && it->second.useCustomThresholds) {
        return it->second.customEscalateThreshold * getRiskMultiplier(risk);
    }
    return m_thresholds.escalateThreshold * getRiskMultiplier(risk);
}

float ConfidenceGate::getEffectiveAbortThreshold(ActionClass action, SafetyRiskTier risk) const {
    auto it = m_profiles.find((int)action);
    if (it != m_profiles.end() && it->second.useCustomThresholds) {
        return it->second.customAbortThreshold * getRiskMultiplier(risk);
    }
    return m_thresholds.abortThreshold * getRiskMultiplier(risk);
}

void ConfidenceGate::updateTrendAnalysis() {
    // Already under lock
    m_stats.overallTrend = analyzeTrend();
    m_stats.recentAvgConfidence = 0.0f;
    if (!m_history.empty()) {
        size_t window = std::min((size_t)50, m_history.size());
        size_t start = m_history.size() - window;
        double sum = 0;
        for (size_t i = start; i < m_history.size(); i++) {
            sum += m_history[i].confidence;
        }
        m_stats.recentAvgConfidence = (float)(sum / window);
    }
}

void ConfidenceGate::checkSelfAbort(ConfidenceDecision decision) {
    // Already under lock
    if (decision == ConfidenceDecision::Abort || decision == ConfidenceDecision::Defer) {
        m_consecutiveLowCount++;
        if (m_consecutiveLowCount >= m_selfAbortThreshold) {
            m_selfAbortTriggered.store(true);
        }
    } else {
        // Reset counter on any non-low decision
        m_consecutiveLowCount = 0;
    }
}
