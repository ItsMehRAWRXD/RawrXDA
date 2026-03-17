// agentic_failure_detector.cpp - Implementation of failure detection
#include "agentic_failure_detector.hpp"
#include <algorithm>

AgenticFailureDetector::AgenticFailureDetector()
{
    initializePatterns();
}

AgenticFailureDetector::~AgenticFailureDetector()
{
}

void AgenticFailureDetector::initializePatterns()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
    m_refusalPatterns = {
        "I can't", "I cannot", "I'm not able to",
        "I can't assist", "I'm unable", "I don't feel comfortable",
        "I decline", "I won't", "I must refuse",
        "I'm not permitted", "against my values",
        "not allowed to", "I apologize, but I cannot"
    };
    
    m_hallucinationPatterns = {
        "I think", "probably", "likely",
        "I might have", "I'm not sure but",
        "As far as I know", "To my knowledge",
        "might be", "could be", "seems like"
    };
    
    m_loopPatterns = {
        "same", "repeated", "over and over",
        "again and again", "same thing"
    };
    
    m_safetyPatterns = {
        "[SENSITIVE]", "[REDACTED]", "[FILTERED]",
        "[BLOCKED]", "[SAFETY]", "[WARNING]"
    };
    
    m_timeoutIndicators = {
        "[Timeout]", "[TIMEOUT]", "timed out",
        "inference timeout", "deadline exceeded"
    };
    
    m_resourceExhaustionIndicators = {
        "out of memory", "OOM", "[OOM]",
        "resource exhausted", "no GPU memory",
        "device out of memory"
    };
}

FailureInfo AgenticFailureDetector::detectFailure(const std::string& modelOutput, const std::string& context)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
    if (!m_enabled) {
        return {AgentFailureType::None, "Detector disabled", 0.0, "", m_sequenceNumber};
    }
    
    if (modelOutput.empty()) {
        return {AgentFailureType::Refusal, "Empty output", 0.5, "No response generated", m_sequenceNumber};
    }
    
    m_stats.totalOutputsAnalyzed++;
    
    if (isRefusal(modelOutput)) {
        FailureInfo info = {AgentFailureType::Refusal, "Model refusal detected", 
                           calculateConfidence(AgentFailureType::Refusal, modelOutput),
                           "Contains refusal keywords", m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Refusal)]++;
        return info;
    }
    
    if (isSafetyViolation(modelOutput)) {
        FailureInfo info = {AgentFailureType::SafetyViolation, "Safety filter triggered",
                           calculateConfidence(AgentFailureType::SafetyViolation, modelOutput),
                           "Contains safety markers", m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::SafetyViolation)]++;
        return info;
    }
    
    if (isTokenLimitExceeded(modelOutput)) {
        FailureInfo info = {AgentFailureType::TokenLimitExceeded, "Token limit exceeded",
                           0.9, "Response truncated or incomplete", m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::TokenLimitExceeded)]++;
        return info;
    }
    
    if (isTimeout(modelOutput)) {
        FailureInfo info = {AgentFailureType::Timeout, "Inference timeout",
                           0.95, "Timeout indicator detected", m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Timeout)]++;
        return info;
    }
    
    if (isResourceExhausted(modelOutput)) {
        FailureInfo info = {AgentFailureType::ResourceExhausted, "Resource exhaustion",
                           0.95, "Out of memory or compute resources", m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::ResourceExhausted)]++;
        return info;
    }

    return {AgentFailureType::None, "", 0.0, "", m_sequenceNumber};
}

std::vector<FailureInfo> AgenticFailureDetector::detectMultipleFailures(const std::string& modelOutput)
{
    std::vector<FailureInfo> failures;
    // Implementation could check all types instead of returning early
    return failures;
}

bool AgenticFailureDetector::isRefusal(const std::string& output) const
{
    for (const auto& pattern : m_refusalPatterns) {
        if (output.find(pattern) != std::string::npos) return true;
    }
    return false;
}

bool AgenticFailureDetector::isHallucination(const std::string& output) const
{
    for (const auto& pattern : m_hallucinationPatterns) {
        if (output.find(pattern) != std::string::npos) return true;
    }
    return false;
}

bool AgenticFailureDetector::isFormatViolation(const std::string&) const { return false; }
bool AgenticFailureDetector::isInfiniteLoop(const std::string&) const { return false; }
bool AgenticFailureDetector::isTokenLimitExceeded(const std::string&) const { return false; }
bool AgenticFailureDetector::isResourceExhausted(const std::string& output) const
{
    for (const auto& pattern : m_resourceExhaustionIndicators) {
        if (output.find(pattern) != std::string::npos) return true;
    }
    return false;
}

bool AgenticFailureDetector::isTimeout(const std::string& output) const
{
    for (const auto& pattern : m_timeoutIndicators) {
        if (output.find(pattern) != std::string::npos) return true;
    }
    return false;
}

bool AgenticFailureDetector::isSafetyViolation(const std::string& output) const
{
    for (const auto& pattern : m_safetyPatterns) {
        if (output.find(pattern) != std::string::npos) return true;
    }
    return false;
}

void AgenticFailureDetector::setRefusalThreshold(double threshold) { m_refusalThreshold = threshold; }
void AgenticFailureDetector::setQualityThreshold(double threshold) { m_qualityThreshold = threshold; }
void AgenticFailureDetector::enableToolValidation(bool enable) { m_enableToolValidation = enable; }

void AgenticFailureDetector::addRefusalPattern(const std::string& pattern) { 
    std::lock_guard<std::mutex> locker(m_mutex);
    m_refusalPatterns.push_back(pattern); 
}

void AgenticFailureDetector::addHallucinationPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_hallucinationPatterns.push_back(pattern);
}

void AgenticFailureDetector::addLoopPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_loopPatterns.push_back(pattern);
}

void AgenticFailureDetector::addSafetyPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_safetyPatterns.push_back(pattern);
}

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_stats;
}

void AgenticFailureDetector::resetStatistics() {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_stats = Stats();
}

void AgenticFailureDetector::setEnabled(bool enable) { m_enabled = enable; }
bool AgenticFailureDetector::isEnabled() const { return m_enabled; }

void AgenticFailureDetector::failureDetected(AgentFailureType, const std::string&) {}
void AgenticFailureDetector::multipleFailuresDetected(const std::vector<FailureInfo>&) {}
void AgenticFailureDetector::highConfidenceDetection(AgentFailureType, double) {}

double AgenticFailureDetector::calculateConfidence(AgentFailureType, const std::string&) { return 0.9; }






