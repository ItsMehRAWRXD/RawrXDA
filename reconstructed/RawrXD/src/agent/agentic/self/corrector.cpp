// agentic_self_corrector.cpp — Automated Self-Correction Loop Implementation
// Detects failures then applies puppeteer correction up to N retries.
// PATTERN:   No exceptions. SelfCorrectionResult returns.
// THREADING: Mutex-protected. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
#include "agentic_self_corrector.hpp"
#include "license_enforcement.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// Construction / Destruction
// ============================================================================

AgenticSelfCorrector::AgenticSelfCorrector() = default;
AgenticSelfCorrector::~AgenticSelfCorrector() = default;

// ============================================================================
// Primary API: self-correction loop
// ============================================================================

SelfCorrectionResult AgenticSelfCorrector::correct(
    const std::string& modelOutput,
    const std::string& userPrompt,
    int maxAttempts)
{
    // Enterprise gate
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
        RawrXD::License::FeatureID::AgenticSelfCorrection, __FUNCTION__))
        return SelfCorrectionResult::error(FailureType::None,
            "[LICENSE] Agentic self-correction requires Enterprise license", 0);

    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_enabled) {
        return SelfCorrectionResult::error(FailureType::None,
            "Self-corrector disabled", 0);
    }

    m_stats.totalRequests++;
    int attemptsToUse = (maxAttempts > 0) ? maxAttempts : m_maxAttempts;
    std::string currentOutput = modelOutput;

    for (int attempt = 1; attempt <= attemptsToUse; ++attempt) {
        m_stats.totalAttempts++;

        // Step 1: Detect failure
        FailureInfo info = m_detector.detectFailure(currentOutput, userPrompt);

        // Map AgentFailureType to FailureType for puppeteer compatibility
        FailureType ft = FailureType::None;
        switch (info.type) {
            case AgentFailureType::Refusal:
                ft = FailureType::RefusalResponse; break;
            case AgentFailureType::Hallucination:
                ft = FailureType::Hallucination; break;
            case AgentFailureType::InfiniteLoop:
                ft = FailureType::InfiniteLoop; break;
            case AgentFailureType::SafetyViolation:
                ft = FailureType::FormatViolation; break;
            default:
                ft = FailureType::None; break;
        }

        // No failure detected — success
        if (ft == FailureType::None) {
            m_stats.successfulFixes++;
            if (onCorrectionComplete)
                onCorrectionComplete(true, currentOutput.c_str(), callbackContext);
            return SelfCorrectionResult::ok(currentOutput, attempt);
        }

        // Fire loop callback
        if (onCorrectionLoop)
            onCorrectionLoop(attempt, ft, callbackContext);

        fprintf(stderr, "[INFO] [SelfCorrector] Attempt %d/%d — failure: %s (confidence: %.2f)\n",
                attempt, attemptsToUse, info.description.c_str(), info.confidence);

        // Step 2: Apply puppeteer correction
        CorrectionResult cr = m_puppeteer.correctResponse(currentOutput, userPrompt);

        if (cr.success && !cr.correctedOutput.empty()) {
            currentOutput = cr.correctedOutput;
            // Re-check if the corrected output passes
            FailureInfo recheck = m_detector.detectFailure(currentOutput, userPrompt);
            if (recheck.type == AgentFailureType::None) {
                m_stats.successfulFixes++;
                if (onCorrectionComplete)
                    onCorrectionComplete(true, currentOutput.c_str(), callbackContext);
                return SelfCorrectionResult::ok(currentOutput, attempt);
            }
            // Correction applied but still has issues — continue loop
        } else {
            // Correction failed — log and continue
            fprintf(stderr, "[WARN] [SelfCorrector] Attempt %d correction failed: %s\n",
                    attempt, cr.diagnosticMessage.c_str());
        }
    }

    // Exhausted all retries
    m_stats.failedAfterRetries++;
    if (onCorrectionComplete)
        onCorrectionComplete(false, currentOutput.c_str(), callbackContext);

    return SelfCorrectionResult::error(FailureType::None,
        "Self-correction failed after max retries", attemptsToUse);
}

// ============================================================================
// Configuration
// ============================================================================

void AgenticSelfCorrector::setMaxAttempts(int n) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (n > 0 && n <= 100) m_maxAttempts = n;
}

int AgenticSelfCorrector::getMaxAttempts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxAttempts;
}

void AgenticSelfCorrector::setEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
}

bool AgenticSelfCorrector::isEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

// ============================================================================
// Subsystem access
// ============================================================================

AgenticPuppeteer& AgenticSelfCorrector::puppeteer() { return m_puppeteer; }
AgenticFailureDetector& AgenticSelfCorrector::detector() { return m_detector; }

// ============================================================================
// Statistics
// ============================================================================

AgenticSelfCorrector::Stats AgenticSelfCorrector::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void AgenticSelfCorrector::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = {};
}
