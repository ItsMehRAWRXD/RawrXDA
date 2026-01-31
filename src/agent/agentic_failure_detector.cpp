// agentic_failure_detector.cpp - Implementation of failure detection
#include "agentic_failure_detector.hpp"


#include <algorithm>

AgenticFailureDetector::AgenticFailureDetector(void* parent)
    : void(parent)
{
    initializePatterns();
}

AgenticFailureDetector::~AgenticFailureDetector()
{
}

void AgenticFailureDetector::initializePatterns()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    // Refusal patterns
    m_refusalPatterns << "I can't" << "I cannot" << "I'm not able to"
                     << "I can't assist" << "I'm unable" << "I don't feel comfortable"
                     << "I decline" << "I won't" << "I must refuse"
                     << "I'm not permitted" << "against my values"
                     << "not allowed to" << "I apologize, but I cannot";
    
    // Hallucination patterns
    m_hallucinationPatterns << "I think" << "probably" << "likely"
                           << "I might have" << "I'm not sure but"
                           << "As far as I know" << "To my knowledge"
                           << "might be" << "could be" << "seems like";
    
    // Infinite loop patterns
    m_loopPatterns << "same" << "repeated" << "over and over"
                  << "again and again" << "same thing";
    
    // Safety patterns
    m_safetyPatterns << "[SENSITIVE]" << "[REDACTED]" << "[FILTERED]"
                    << "[BLOCKED]" << "[SAFETY]" << "[WARNING]";
    
    // Timeout indicators
    m_timeoutIndicators << "[Timeout]" << "[TIMEOUT]" << "timed out"
                       << "inference timeout" << "deadline exceeded";
    
    // Resource exhaustion indicators
    m_resourceExhaustionIndicators << "out of memory" << "OOM" << "[OOM]"
                                  << "resource exhausted" << "no GPU memory"
                                  << "device out of memory";
}

FailureInfo AgenticFailureDetector::detectFailure(const std::string& modelOutput, const std::string& context)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return FailureInfo{AgentFailureType::None, "Detector disabled", 0.0, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber};
    }
    
    if (modelOutput.isEmpty()) {
        return FailureInfo{AgentFailureType::Refusal, "Empty output", 0.5, "No response generated", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber};
    }
    
    m_stats.totalOutputsAnalyzed++;
    
    // Check each failure type in priority order
    if (isRefusal(modelOutput)) {
        FailureInfo info{AgentFailureType::Refusal, "Model refusal detected", 
                        calculateConfidence(AgentFailureType::Refusal, modelOutput),
                        "Contains refusal keywords", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Refusal)]++;
        return info;
    }
    
    if (isSafetyViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::SafetyViolation, "Safety filter triggered",
                        calculateConfidence(AgentFailureType::SafetyViolation, modelOutput),
                        "Contains safety markers", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::SafetyViolation)]++;
        return info;
    }
    
    if (isTokenLimitExceeded(modelOutput)) {
        FailureInfo info{AgentFailureType::TokenLimitExceeded, "Token limit exceeded",
                        0.9, "Response truncated or incomplete", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::TokenLimitExceeded)]++;
        return info;
    }
    
    if (isTimeout(modelOutput)) {
        FailureInfo info{AgentFailureType::Timeout, "Inference timeout",
                        0.95, "Timeout indicator detected", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Timeout)]++;
        return info;
    }
    
    if (isResourceExhausted(modelOutput)) {
        FailureInfo info{AgentFailureType::ResourceExhausted, "Resource exhaustion",
                        0.95, "Out of memory or compute resources", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::ResourceExhausted)]++;
        return info;
    }
    
    if (isInfiniteLoop(modelOutput)) {
        FailureInfo info{AgentFailureType::InfiniteLoop, "Infinite loop detected",
                        calculateConfidence(AgentFailureType::InfiniteLoop, modelOutput),
                        "Repeating content pattern", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::InfiniteLoop)]++;
        return info;
    }
    
    if (isFormatViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::FormatViolation, "Format violation detected",
                        calculateConfidence(AgentFailureType::FormatViolation, modelOutput),
                        "Output format incorrect", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::FormatViolation)]++;
        return info;
    }
    
    if (isHallucination(modelOutput)) {
        FailureInfo info{AgentFailureType::Hallucination, "Hallucination indicators",
                        calculateConfidence(AgentFailureType::Hallucination, modelOutput),
                        "Contains uncertain language patterns", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Hallucination)]++;
        return info;
    }
    
    // No failure detected
    return FailureInfo{AgentFailureType::None, "No failure detected", 1.0, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber};
}

std::vector<FailureInfo> AgenticFailureDetector::detectMultipleFailures(const std::string& modelOutput)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    std::vector<FailureInfo> failures;
    
    if (isRefusal(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::Refusal, "Refusal", 0.8, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber});
    }
    if (isHallucination(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::Hallucination, "Hallucination", 0.6, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber});
    }
    if (isFormatViolation(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::FormatViolation, "Format issue", 0.7, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber});
    }
    if (isInfiniteLoop(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::InfiniteLoop, "Repetition", 0.85, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber});
    }
    if (isSafetyViolation(modelOutput)) {
        failures.append(FailureInfo{AgentFailureType::SafetyViolation, "Safety block", 0.95, "", std::chrono::system_clock::time_point::currentDateTime(), m_sequenceNumber});
    }
    
    m_sequenceNumber++;
    
    if (!failures.isEmpty()) {
        multipleFailuresDetected(failures);
    }
    
    return failures;
}

bool AgenticFailureDetector::isRefusal(const std::string& output) const
{
    std::string lower = output.toLower();
    
    for (const std::string& pattern : m_refusalPatterns) {
        if (lower.contains(pattern.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isHallucination(const std::string& output) const
{
    std::string lower = output.toLower();
    int hallucIndicators = 0;
    
    for (const std::string& pattern : m_hallucinationPatterns) {
        if (lower.contains(pattern.toLower())) {
            hallucIndicators++;
        }
    }
    
    return hallucIndicators >= 2; // Need multiple indicators
}

bool AgenticFailureDetector::isFormatViolation(const std::string& output) const
{
    // Check JSON format
    if (output.trimmed().startsWith('{')) {
        int openBraces = output.count('{');
        int closeBraces = output.count('}');
        if (openBraces != closeBraces) {
            return true;
        }
    }
    
    // Check code block format
    if (output.contains("```")) {
        if ((output.count("```") % 2) != 0) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isInfiniteLoop(const std::string& output) const
{
    std::vector<std::string> lines = output.split('\n', //SkipEmptyParts);
    
    if (lines.count() < 5) {
        return false;
    }
    
    // Check for repeated lines
    std::unordered_map<std::string, int> lineCount;
    for (const std::string& line : lines) {
        lineCount[line.trimmed()]++;
    }
    
    for (int count : lineCount.values()) {
        if (count > 3) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isTokenLimitExceeded(const std::string& output) const
{
    return output.endsWith("...") || output.endsWith("[truncated]") || 
           output.endsWith("[end of response]") || output.contains("[token limit]");
}

bool AgenticFailureDetector::isResourceExhausted(const std::string& output) const
{
    std::string lower = output.toLower();
    
    for (const std::string& indicator : m_resourceExhaustionIndicators) {
        if (lower.contains(indicator.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isTimeout(const std::string& output) const
{
    std::string lower = output.toLower();
    
    for (const std::string& indicator : m_timeoutIndicators) {
        if (lower.contains(indicator.toLower())) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isSafetyViolation(const std::string& output) const
{
    for (const std::string& pattern : m_safetyPatterns) {
        if (output.contains(pattern)) {
            return true;
        }
    }
    
    return false;
}

void AgenticFailureDetector::setRefusalThreshold(double threshold)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_refusalThreshold = threshold;
}

void AgenticFailureDetector::setQualityThreshold(double threshold)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_qualityThreshold = threshold;
}

void AgenticFailureDetector::enableToolValidation(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableToolValidation = enable;
}

void AgenticFailureDetector::addRefusalPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_refusalPatterns.contains(pattern)) {
        m_refusalPatterns.append(pattern);
    }
}

void AgenticFailureDetector::addHallucinationPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_hallucinationPatterns.contains(pattern)) {
        m_hallucinationPatterns.append(pattern);
    }
}

void AgenticFailureDetector::addLoopPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_loopPatterns.contains(pattern)) {
        m_loopPatterns.append(pattern);
    }
}

void AgenticFailureDetector::addSafetyPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_safetyPatterns.contains(pattern)) {
        m_safetyPatterns.append(pattern);
    }
}

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_stats;
}

void AgenticFailureDetector::resetStatistics()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats = Stats();
}

void AgenticFailureDetector::setEnabled(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enabled = enable;
}

bool AgenticFailureDetector::isEnabled() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_enabled;
}

double AgenticFailureDetector::calculateConfidence(AgentFailureType type, const std::string& output)
{
    double confidence = 0.5;
    
    switch (type) {
        case AgentFailureType::Refusal:
            confidence = output.count("cannot") > 0 ? 0.9 : 0.7;
            break;
        case AgentFailureType::Hallucination:
            confidence = 0.6;
            break;
        case AgentFailureType::InfiniteLoop:
            confidence = 0.85;
            break;
        default:
            confidence = 0.7;
            break;
    }
    
    return confidence;
}

