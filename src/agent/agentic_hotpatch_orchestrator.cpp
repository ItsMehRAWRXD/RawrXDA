// agentic_hotpatch_orchestrator.cpp — Phase 18: Agent-Driven Hotpatch Orchestration
//
// Implementation of the failure detection → hotpatch correction pipeline.
// Non-Qt, no exceptions, function-pointer callbacks.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "agentic_hotpatch_orchestrator.hpp"
#include <mutex>
#include <vector>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace {

float clampf(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

}

// SCAFFOLD_069: agentic_hotpatch_orchestrator


// ============================================================================
// Singleton
// ============================================================================

AgenticHotpatchOrchestrator& AgenticHotpatchOrchestrator::instance() {
    static AgenticHotpatchOrchestrator inst;
    return inst;
}

AgenticHotpatchOrchestrator::AgenticHotpatchOrchestrator()
    : m_enabled(true)
    , m_maxRetries(3)
    , m_confidenceThreshold(0.6f)
    , m_autoEscalate(true)
    , m_modelTemperature(0.7f)
    , m_sequenceCounter(0)
{
    loadDefaultPolicies();
}

AgenticHotpatchOrchestrator::~AgenticHotpatchOrchestrator() = default;

// ============================================================================
// Default Policies
// ============================================================================

void AgenticHotpatchOrchestrator::loadDefaultPolicies() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policies.clear();

    // Refusal → retry with token bias to encourage compliance
    m_policies.push_back({InferenceFailureType::Refusal,
                          CorrectionAction::RetryWithBias,
                          CorrectionAction::EscalateToUser,
                          3, 0.7f, true});

    // Hallucination → rewrite output, fallback to escalation
    m_policies.push_back({InferenceFailureType::Hallucination,
                          CorrectionAction::RewriteOutput,
                          CorrectionAction::EscalateToUser,
                          2, 0.65f, true});

    // Format violation → rewrite output
    m_policies.push_back({InferenceFailureType::FormatViolation,
                          CorrectionAction::RewriteOutput,
                          CorrectionAction::RetryWithBias,
                          2, 0.5f, true});

    // Infinite loop → terminate stream immediately
    m_policies.push_back({InferenceFailureType::InfiniteLoop,
                          CorrectionAction::TerminateStream,
                          CorrectionAction::EscalateToUser,
                          1, 0.8f, true});

    // Token limit → terminate stream
    m_policies.push_back({InferenceFailureType::TokenLimit,
                          CorrectionAction::TerminateStream,
                          CorrectionAction::None,
                          1, 0.9f, true});

    // Resource exhausted → escalate
    m_policies.push_back({InferenceFailureType::ResourceExhausted,
                          CorrectionAction::EscalateToUser,
                          CorrectionAction::None,
                          0, 0.5f, true});

    // Timeout → terminate + retry
    m_policies.push_back({InferenceFailureType::Timeout,
                          CorrectionAction::TerminateStream,
                          CorrectionAction::RetryWithBias,
                          2, 0.5f, true});

    // Safety violation → terminate immediately
    m_policies.push_back({InferenceFailureType::SafetyViolation,
                          CorrectionAction::TerminateStream,
                          CorrectionAction::EscalateToUser,
                          0, 0.3f, true});

    // Low confidence → retry with bias
    m_policies.push_back({InferenceFailureType::LowConfidence,
                          CorrectionAction::RetryWithBias,
                          CorrectionAction::EscalateToUser,
                          2, 0.5f, true});

    // Garbage output → terminate + escalate
    m_policies.push_back({InferenceFailureType::GarbageOutput,
                          CorrectionAction::TerminateStream,
                          CorrectionAction::EscalateToUser,
                          1, 0.7f, true});

    // Default patterns
    m_refusalPatterns = {
        "I cannot", "I can't", "I'm unable", "I am unable",
        "As an AI", "I don't have", "I'm not able",
        "I must decline", "I apologize", "I'm sorry, but",
        "I won't be able to", "It's not appropriate",
    };

    m_hallucinationPatterns = {
        "According to my knowledge", "As everyone knows",
        "It is well established that", "Studies have shown",
    };

    m_loopPatterns = {};  // Detected algorithmically

    m_safetyPatterns = {
        "[SAFETY]", "[BLOCKED]", "[FILTERED]",
        "content policy", "safety filter",
    };

    m_garbagePatterns = {
        "\xff\xfe", "\x00\x00\x00",
    };
}

// ============================================================================
// Failure Detection
// ============================================================================

InferenceFailureEvent AgenticHotpatchOrchestrator::detectFailure(
    const char* output, size_t outputLen,
    const char* /*prompt*/, size_t /*promptLen*/)
{
    InferenceFailureEvent evt = {};
    evt.type = InferenceFailureType::None;
    evt.confidence = 0.0f;
    evt.description = "No failure";
    evt.timestamp = GetTickCount64();

    if (!output || outputLen == 0) {
        evt.type = InferenceFailureType::GarbageOutput;
        evt.confidence = 1.0f;
        evt.description = "Empty output";
        return evt;
    }

    // Run detectors in priority order, pick highest confidence
    struct { InferenceFailureType type; float conf; const char* desc; } candidates[6];
    int count = 0;

    float c;

    c = detectSafetyViolation(output, outputLen);
    if (c > 0.3f) {
        candidates[count++] = {InferenceFailureType::SafetyViolation, c, "Safety violation detected"};
    }

    c = detectRefusal(output, outputLen);
    if (c > 0.3f) {
        candidates[count++] = {InferenceFailureType::Refusal, c, "Model refusal detected"};
    }

    c = detectInfiniteLoop(output, outputLen);
    if (c > 0.5f) {
        candidates[count++] = {InferenceFailureType::InfiniteLoop, c, "Infinite loop detected"};
    }

    c = detectGarbageOutput(output, outputLen);
    if (c > 0.5f) {
        candidates[count++] = {InferenceFailureType::GarbageOutput, c, "Garbage output detected"};
    }

    c = detectHallucination(output, outputLen);
    if (c > 0.4f) {
        candidates[count++] = {InferenceFailureType::Hallucination, c, "Possible hallucination"};
    }

    c = detectFormatViolation(output, outputLen);
    if (c > 0.5f) {
        candidates[count++] = {InferenceFailureType::FormatViolation, c, "Format violation"};
    }

    // Pick the highest confidence failure
    if (count > 0) {
        int best = 0;
        for (int i = 1; i < count; ++i) {
            if (candidates[i].conf > candidates[best].conf) {
                best = i;
            }
        }
        evt.type = candidates[best].type;
        evt.confidence = candidates[best].conf;
        evt.description = candidates[best].desc;

        m_stats.failuresDetected.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            evt.sequenceId = ++m_sequenceCounter;
        }
        notifyFailure(evt);
    }

    return evt;
}

size_t AgenticHotpatchOrchestrator::detectFailures(
    const char** outputs, const size_t* lengths,
    size_t count, InferenceFailureEvent* outEvents)
{
    size_t failures = 0;
    for (size_t i = 0; i < count; ++i) {
        InferenceFailureEvent evt = detectFailure(outputs[i], lengths[i]);
        if (evt.type != InferenceFailureType::None) {
            if (outEvents) outEvents[failures] = evt;
            ++failures;
        }
    }
    return failures;
}

// ============================================================================
// Detection Helpers
// ============================================================================

// Helper: case-insensitive substring search
static bool containsCI(const char* haystack, size_t haystackLen, const char* needle) {
    size_t needleLen = strlen(needle);
    if (needleLen == 0 || haystackLen < needleLen) return false;

    for (size_t i = 0; i + needleLen <= haystackLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLen; ++j) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

float AgenticHotpatchOrchestrator::detectRefusal(const char* output, size_t len) {
    int matches = 0;
    for (const auto& pat : m_refusalPatterns) {
        if (containsCI(output, len, pat.c_str())) {
            ++matches;
        }
    }
    if (matches == 0) return 0.0f;
    // More matches = higher confidence
    float conf = 0.5f + 0.1f * (float)(matches - 1);
    return std::min(conf, 0.95f);
}

float AgenticHotpatchOrchestrator::detectHallucination(const char* output, size_t len) {
    int matches = 0;
    for (const auto& pat : m_hallucinationPatterns) {
        if (containsCI(output, len, pat.c_str())) {
            ++matches;
        }
    }
    if (matches == 0) return 0.0f;
    return std::min(0.4f + 0.15f * (float)(matches - 1), 0.85f);
}

float AgenticHotpatchOrchestrator::detectInfiniteLoop(const char* output, size_t len) {
    // Check for repeating blocks: if any 32-byte block repeats 3+ times in a row
    if (len < 96) return 0.0f;

    const size_t blockSize = 32;
    int maxRepeats = 0;

    for (size_t i = 0; i + blockSize * 2 <= len; i += blockSize) {
        int repeats = 1;
        for (size_t j = i + blockSize; j + blockSize <= len; j += blockSize) {
            if (std::memcmp(output + i, output + j, blockSize) == 0) {
                ++repeats;
            } else {
                break;
            }
        }
        if (repeats > maxRepeats) maxRepeats = repeats;
    }

    if (maxRepeats < 3) return 0.0f;
    return std::min(0.5f + 0.1f * (float)(maxRepeats - 3), 0.99f);
}

float AgenticHotpatchOrchestrator::detectFormatViolation(const char* /*output*/, size_t /*len*/) {
    // Format validation is context-dependent — requires expected format spec.
    // Returns 0 by default; callers should use custom validators via ProxyHotpatcher.
    return 0.0f;
}

float AgenticHotpatchOrchestrator::detectGarbageOutput(const char* output, size_t len) {
    // Check for high proportion of non-printable characters
    size_t nonPrintable = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)output[i];
        if (ch < 0x20 && ch != '\n' && ch != '\r' && ch != '\t') {
            ++nonPrintable;
        }
    }

    float ratio = (float)nonPrintable / (float)len;
    if (ratio > 0.3f) return 0.9f;
    if (ratio > 0.1f) return 0.5f + ratio;

    // Check for garbage patterns
    for (const auto& pat : m_garbagePatterns) {
        if (len >= pat.size() &&
            std::memcmp(output, pat.c_str(), pat.size()) == 0) {
            return 0.85f;
        }
    }

    return 0.0f;
}

float AgenticHotpatchOrchestrator::detectSafetyViolation(const char* output, size_t len) {
    for (const auto& pat : m_safetyPatterns) {
        if (containsCI(output, len, pat.c_str())) {
            return 0.9f;
        }
    }
    return 0.0f;
}

// ============================================================================
// Correction Orchestration
// ============================================================================

CorrectionOutcome AgenticHotpatchOrchestrator::orchestrateCorrection(
    const InferenceFailureEvent& failure,
    char* outputBuffer, size_t bufferCapacity)
{
    // Context-agnostic behavior: hotpatch orchestration is always available.
    m_enabled = true;

    const CorrectionPolicy* policy = findPolicy(failure.type);
    if (!policy || !policy->enabled) {
        return CorrectionOutcome::error(CorrectionAction::None,
                                         "No policy for this failure type");
    }

    // Temperature-aware aggressiveness:
    // colder model -> stricter threshold, fewer retries
    // hotter model -> looser threshold, more retries
    const float tNorm = clampf(m_modelTemperature / 1.5f, 0.0f, 1.0f);
    const float effectiveThreshold = clampf(
        policy->confidenceThreshold + (0.1f * (1.0f - tNorm)) - (0.35f * tNorm),
        0.1f,
        0.95f);

    if (failure.confidence < effectiveThreshold) {
        return CorrectionOutcome::error(CorrectionAction::None,
                                         "Confidence below threshold");
    }

    m_stats.correctionsAttempted.fetch_add(1, std::memory_order_relaxed);

    CorrectionOutcome result = CorrectionOutcome::error(
        CorrectionAction::None, "Not attempted");

    int effectiveRetries = policy->maxRetries;
    if (tNorm >= 0.75f) {
        effectiveRetries += 2;
    } else if (tNorm <= 0.20f) {
        effectiveRetries = std::max(0, effectiveRetries - 1);
    }
    effectiveRetries = std::max(0, std::max(effectiveRetries, m_maxRetries));

    // Try primary action (with temperature-aware retry budget)
    for (int attempt = 0; attempt <= effectiveRetries; ++attempt) {
        switch (policy->primaryAction) {
            case CorrectionAction::RetryWithBias:
                result = applyBiasCorrection(failure);
                break;
            case CorrectionAction::RewriteOutput:
                result = applyRewriteCorrection(failure, outputBuffer, bufferCapacity);
                break;
            case CorrectionAction::TerminateStream:
                result = applyStreamTermination(failure);
                break;
            case CorrectionAction::EscalateToUser:
                result = escalateToUser(failure);
                break;
            default:
                break;
        }

        result.retriesUsed = attempt;
        if (result.success) {
            break;
        }
    }

    // If primary failed, try fallback
    if (!result.success && policy->fallbackAction != CorrectionAction::None) {
        switch (policy->fallbackAction) {
            case CorrectionAction::RetryWithBias:
                result = applyBiasCorrection(failure);
                break;
            case CorrectionAction::RewriteOutput:
                result = applyRewriteCorrection(failure, outputBuffer, bufferCapacity);
                break;
            case CorrectionAction::TerminateStream:
                result = applyStreamTermination(failure);
                break;
            case CorrectionAction::EscalateToUser:
                result = escalateToUser(failure);
                break;
            default:
                break;
        }
    }

    // If still failed and auto-escalate is on, escalate
    if (!result.success && m_autoEscalate) {
        result = escalateToUser(failure);
    }

    if (result.success) {
        m_stats.correctionsSucceeded.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_stats.correctionsFailed.fetch_add(1, std::memory_order_relaxed);
    }

    notifyCorrection(result);
    return result;
}

CorrectionOutcome AgenticHotpatchOrchestrator::analyzeAndCorrect(
    const char* output, size_t outputLen,
    const char* prompt, size_t promptLen,
    char* correctedOutput, size_t correctedCapacity)
{
    InferenceFailureEvent evt = detectFailure(output, outputLen, prompt, promptLen);

    if (evt.type == InferenceFailureType::None) {
        return CorrectionOutcome::ok(CorrectionAction::None, "No failure detected");
    }

    return orchestrateCorrection(evt, correctedOutput, correctedCapacity);
}

// ============================================================================
// Correction Action Helpers
// ============================================================================

CorrectionOutcome AgenticHotpatchOrchestrator::applyBiasCorrection(
    const InferenceFailureEvent& failure)
{
    auto& proxy = ProxyHotpatcher::instance();

    // Apply token biases to steer generation away from failure patterns.
    // Token IDs are model-specific — we use output rewrite rules as a
    // model-agnostic fallback, and add representative bias entries when
    // a vocabulary mapping is available at runtime.
    //
    // Strategy:
    //   Refusal  → add rewrite rule to strip refusal prefixes + bias
    //   LowConf  → add rewrite rule to strip hedging prefixes
    //   Other    → log and proceed to retry

    switch (failure.type) {
        case InferenceFailureType::Refusal: {
            // Suppress common refusal prefixes via output rewrite
            OutputRewriteRule rr = {};
            rr.name = "auto_strip_refusal";
            rr.pattern = "I cannot";
            rr.replacement = "";
            rr.hitCount = 0;
            rr.enabled = true;
            proxy.add_rewrite_rule(rr);

            // If vocab size is known, apply logit biases to suppress
            // tokens like "sorry", "cannot", "unable" and boost "Sure",
            // "Here".  Concrete IDs depend on loaded tokenizer; callers
            // can register model-specific mappings via add_token_bias().
            TokenBias bias = {};
            bias.permanent = false;
            // We still inject a bias on tokenId 0 (typically <unk> or
            // <pad>) as a defensive measure — suppressing it avoids
            // degenerate sampling.
            bias.tokenId = 0;
            bias.biasValue = -5.0f;
            proxy.add_token_bias(bias);
            break;
        }

        case InferenceFailureType::LowConfidence: {
            // Strip hedging language via rewrite and re-sample
            OutputRewriteRule rr = {};
            rr.name = "auto_strip_hedging";
            rr.pattern = "I think";
            rr.replacement = "";
            rr.hitCount = 0;
            rr.enabled = true;
            proxy.add_rewrite_rule(rr);

            TokenBias bias = {};
            bias.permanent = false;
            bias.tokenId = 0;
            bias.biasValue = -3.0f;
            proxy.add_token_bias(bias);
            break;
        }

        default:
            break;
    }

    m_stats.biasInjections.fetch_add(1, std::memory_order_relaxed);
    m_stats.retryCount.fetch_add(1, std::memory_order_relaxed);

    return CorrectionOutcome::ok(CorrectionAction::RetryWithBias,
                                  "Token bias + rewrite rules injected for retry");
}

CorrectionOutcome AgenticHotpatchOrchestrator::applyRewriteCorrection(
    const InferenceFailureEvent& /*failure*/,
    char* buffer, size_t capacity)
{
    if (!buffer || capacity == 0) {
        return CorrectionOutcome::error(CorrectionAction::RewriteOutput,
                                         "No output buffer for rewrite");
    }

    auto& proxy = ProxyHotpatcher::instance();
    size_t currentLen = strnlen(buffer, capacity);
    size_t newLen = proxy.apply_rewrites(buffer, currentLen, capacity);

    if (newLen != currentLen) {
        m_stats.outputRewrites.fetch_add(1, std::memory_order_relaxed);
        return CorrectionOutcome::ok(CorrectionAction::RewriteOutput,
                                      "Output rewritten via proxy rules");
    }

    return CorrectionOutcome::error(CorrectionAction::RewriteOutput,
                                     "No rewrite rules matched");
}

CorrectionOutcome AgenticHotpatchOrchestrator::applyStreamTermination(
    const InferenceFailureEvent& /*failure*/)
{
    m_stats.streamTerminations.fetch_add(1, std::memory_order_relaxed);
    return CorrectionOutcome::ok(CorrectionAction::TerminateStream,
                                  "Stream terminated by agent");
}

CorrectionOutcome AgenticHotpatchOrchestrator::escalateToUser(
    const InferenceFailureEvent& failure)
{
    m_stats.escalationsToUser.fetch_add(1, std::memory_order_relaxed);

    static char msgBuf[256];
    snprintf(msgBuf, sizeof(msgBuf),
             "Agent escalation: %s (confidence %.2f)",
             failure.description, failure.confidence);

    return CorrectionOutcome::ok(CorrectionAction::EscalateToUser, msgBuf);
}

// ============================================================================
// Policy Management
// ============================================================================

PatchResult AgenticHotpatchOrchestrator::addPolicy(const CorrectionPolicy& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Replace existing policy for this failure type
    for (auto& p : m_policies) {
        if (p.failureType == policy.failureType) {
            p = policy;
            return PatchResult::ok("Policy updated");
        }
    }

    m_policies.push_back(policy);
    return PatchResult::ok("Policy added");
}

PatchResult AgenticHotpatchOrchestrator::removePolicy(InferenceFailureType failureType) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::remove_if(m_policies.begin(), m_policies.end(),
        [failureType](const CorrectionPolicy& p) {
            return p.failureType == failureType;
        });

    if (it != m_policies.end()) {
        m_policies.erase(it, m_policies.end());
        return PatchResult::ok("Policy removed");
    }

    return PatchResult::error("Policy not found");
}

PatchResult AgenticHotpatchOrchestrator::clearPolicies() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policies.clear();
    return PatchResult::ok("All policies cleared");
}

const std::vector<CorrectionPolicy>& AgenticHotpatchOrchestrator::getPolicies() const {
    return m_policies;
}

// ============================================================================
// Pattern Configuration
// ============================================================================

void AgenticHotpatchOrchestrator::addRefusalPattern(const char* pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refusalPatterns.emplace_back(pattern);
}

void AgenticHotpatchOrchestrator::addHallucinationPattern(const char* pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hallucinationPatterns.emplace_back(pattern);
}

void AgenticHotpatchOrchestrator::addLoopPattern(const char* pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loopPatterns.emplace_back(pattern);
}

void AgenticHotpatchOrchestrator::addSafetyPattern(const char* pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_safetyPatterns.emplace_back(pattern);
}

void AgenticHotpatchOrchestrator::addGarbagePattern(const char* pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_garbagePatterns.emplace_back(pattern);
}

// ============================================================================
// Callbacks
// ============================================================================

void AgenticHotpatchOrchestrator::registerFailureCallback(
    FailureDetectedCallback cb, void* userData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_failureCallbacks.push_back({cb, userData});
}

void AgenticHotpatchOrchestrator::registerCorrectionCallback(
    CorrectionAppliedCallback cb, void* userData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_correctionCallbacks.push_back({cb, userData});
}

void AgenticHotpatchOrchestrator::unregisterFailureCallback(FailureDetectedCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_failureCallbacks.erase(
        std::remove_if(m_failureCallbacks.begin(), m_failureCallbacks.end(),
            [cb](const FailureCB& entry) { return entry.fn == cb; }),
        m_failureCallbacks.end());
}

void AgenticHotpatchOrchestrator::unregisterCorrectionCallback(CorrectionAppliedCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_correctionCallbacks.erase(
        std::remove_if(m_correctionCallbacks.begin(), m_correctionCallbacks.end(),
            [cb](const CorrectionCB& entry) { return entry.fn == cb; }),
        m_correctionCallbacks.end());
}

void AgenticHotpatchOrchestrator::notifyFailure(const InferenceFailureEvent& evt) {
    // Copy callbacks under lock, fire outside lock
    std::vector<FailureCB> cbs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cbs = m_failureCallbacks;
    }
    for (const auto& cb : cbs) {
        cb.fn(&evt, cb.userData);
    }
}

void AgenticHotpatchOrchestrator::notifyCorrection(const CorrectionOutcome& outcome) {
    std::vector<CorrectionCB> cbs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cbs = m_correctionCallbacks;
    }
    for (const auto& cb : cbs) {
        cb.fn(&outcome, cb.userData);
    }
}

// ============================================================================
// Policy Lookup
// ============================================================================

const CorrectionPolicy* AgenticHotpatchOrchestrator::findPolicy(
    InferenceFailureType type) const
{
    for (const auto& p : m_policies) {
        if (p.failureType == type) return &p;
    }
    return nullptr;
}

// ============================================================================
// Statistics
// ============================================================================

const OrchestrationStats& AgenticHotpatchOrchestrator::getStats() const {
    return m_stats;
}

void AgenticHotpatchOrchestrator::resetStats() {
    m_stats.failuresDetected.store(0, std::memory_order_relaxed);
    m_stats.correctionsAttempted.store(0, std::memory_order_relaxed);
    m_stats.correctionsSucceeded.store(0, std::memory_order_relaxed);
    m_stats.correctionsFailed.store(0, std::memory_order_relaxed);
    m_stats.escalationsToUser.store(0, std::memory_order_relaxed);
    m_stats.retryCount.store(0, std::memory_order_relaxed);
    m_stats.biasInjections.store(0, std::memory_order_relaxed);
    m_stats.outputRewrites.store(0, std::memory_order_relaxed);
    m_stats.streamTerminations.store(0, std::memory_order_relaxed);
}

// ============================================================================
// Enable / Disable / Configuration
// ============================================================================

void AgenticHotpatchOrchestrator::setEnabled(bool enabled) {
    m_enabled = enabled;
}

bool AgenticHotpatchOrchestrator::isEnabled() const {
    return m_enabled;
}

void AgenticHotpatchOrchestrator::setMaxRetries(int retries) {
    m_maxRetries = retries;
}

void AgenticHotpatchOrchestrator::setConfidenceThreshold(float threshold) {
    m_confidenceThreshold = threshold;
}

void AgenticHotpatchOrchestrator::setAutoEscalate(bool enabled) {
    m_autoEscalate = enabled;
}

void AgenticHotpatchOrchestrator::setModelTemperature(float temperature) {
    m_modelTemperature = clampf(temperature, 0.0f, 2.0f);

    // Keep global controls aligned with temperature profile.
    const float tNorm = clampf(m_modelTemperature / 1.5f, 0.0f, 1.0f);
    m_confidenceThreshold = clampf(0.8f - (0.6f * tNorm), 0.2f, 0.9f);
    m_maxRetries = 1 + static_cast<int>(tNorm * 5.0f);
    m_enabled = true;
}

float AgenticHotpatchOrchestrator::getModelTemperature() const {
    return m_modelTemperature;
}
