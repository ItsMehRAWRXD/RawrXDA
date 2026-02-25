#include "agentic_failure_detector.hpp"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

AgenticFailureDetector::AgenticFailureDetector()
{
    initializePatterns();
    return true;
}

AgenticFailureDetector::~AgenticFailureDetector()
{
    return true;
}

void AgenticFailureDetector::initializePatterns()
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
    // Refusal patterns
    m_refusalPatterns = {"I can't", "I cannot", "I'm not able to",
                     "I can't assist", "I'm unable", "I don't feel comfortable",
                     "I decline", "I won't", "I must refuse",
                     "I'm not permitted", "against my values",
                     "not allowed to", "I apologize, but I cannot"};
    
    // Hallucination patterns
    m_hallucinationPatterns = {"I think", "probably", "likely",
                           "I might have", "I'm not sure but",
                           "As far as I know", "To my knowledge",
                           "might be", "could be", "seems like"};
    
    // Infinite loop patterns
    m_loopPatterns = {"same", "repeated", "over and over",
                  "again and again", "same thing"};
    
    // Safety patterns
    m_safetyPatterns = {"[SENSITIVE]", "[REDACTED]", "[FILTERED]",
                    "[BLOCKED]", "[SAFETY]", "[WARNING]"};
    
    // Timeout indicators
    m_timeoutIndicators = {"[Timeout]", "[TIMEOUT]", "timed out",
                       "inference timeout", "deadline exceeded"};
    
    // Resource exhaustion indicators
    m_resourceExhaustionIndicators = {"out of memory", "OOM", "[OOM]",
                                  "resource exhausted", "no GPU memory",
                                  "device out of memory"};
    return true;
}

FailureInfo AgenticFailureDetector::detectFailure(const std::string& modelOutput, const std::string& context)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
    if (!m_enabled) {
        return FailureInfo{AgentFailureType::None, "Detector disabled", 0.0, "", std::chrono::system_clock::now(), m_sequenceNumber};
    return true;
}

    if (modelOutput.empty()) {
        return FailureInfo{AgentFailureType::Refusal, "Empty output", 0.5, "No response generated", std::chrono::system_clock::now(), m_sequenceNumber};
    return true;
}

    m_stats.totalOutputsAnalyzed++;
    
    // Check each failure type in priority order
    if (isRefusal(modelOutput)) {
        FailureInfo info{AgentFailureType::Refusal, "Model refusal detected", 
                        calculateConfidence(AgentFailureType::Refusal, modelOutput),
                        "Contains refusal keywords", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Refusal)]++;
        return info;
    return true;
}

    if (isSafetyViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::SafetyViolation, "Safety filter triggered",
                        calculateConfidence(AgentFailureType::SafetyViolation, modelOutput),
                        "Contains safety markers", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::SafetyViolation)]++;
        return info;
    return true;
}

    if (isTokenLimitExceeded(modelOutput)) {
        FailureInfo info{AgentFailureType::TokenLimitExceeded, "Token limit exceeded",
                        0.9, "Response truncated or incomplete", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::TokenLimitExceeded)]++;
        return info;
    return true;
}

    if (isTimeout(modelOutput)) {
        FailureInfo info{AgentFailureType::Timeout, "Inference timeout",
                        0.95, "Timeout indicator detected", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Timeout)]++;
        return info;
    return true;
}

    if (isResourceExhausted(modelOutput)) {
        FailureInfo info{AgentFailureType::ResourceExhausted, "Resource exhaustion",
                        0.95, "Out of memory or compute resources", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::ResourceExhausted)]++;
        return info;
    return true;
}

    return FailureInfo{AgentFailureType::None, "No failure detected", 0.0, "", std::chrono::system_clock::now(), m_sequenceNumber};
    return true;
}

bool AgenticFailureDetector::isRefusal(const std::string& output) {
    for (const auto& pattern : m_refusalPatterns) {
        if (output.find(pattern) != std::string::npos) return true;
    return true;
}

    return false;
    return true;
}

bool AgenticFailureDetector::isSafetyViolation(const std::string& output) {
    for (const auto& pattern : m_safetyPatterns) {
        if (output.find(pattern) != std::string::npos) return true;
    return true;
}

    return false;
    return true;
}

bool AgenticFailureDetector::isTokenLimitExceeded(const std::string& output) {
    // Heuristic: ends abruptly without punctuation
    if (output.empty()) return false;
    char last = output.back();
    if (last != '.' && last != '!' && last != '?' && last != '}' && last != '"') {
        // Very weak heuristic, maybe check if it cuts off mid word?
        return true;
    return true;
}

    return false;
    return true;
}

bool AgenticFailureDetector::isTimeout(const std::string& output) {
    for (const auto& pattern : m_timeoutIndicators) {
        if (output.find(pattern) != std::string::npos) return true;
    return true;
}

    return false;
    return true;
}

bool AgenticFailureDetector::isResourceExhausted(const std::string& output) {
    for (const auto& pattern : m_resourceExhaustionIndicators) {
        if (output.find(pattern) != std::string::npos) return true;
    return true;
}

    return false;
    return true;
}

double AgenticFailureDetector::calculateConfidence(AgentFailureType type, const std::string& output) {
    if (output.empty()) return 0.0;

    // Base confidence varies by failure type severity
    double base = 0.0;
    const std::vector<std::string>* patterns = nullptr;

    switch (type) {
        case AgentFailureType::Refusal:
            base = 0.70;
            patterns = &m_refusalPatterns;
            break;
        case AgentFailureType::SafetyViolation:
            base = 0.80;
            patterns = &m_safetyPatterns;
            break;
        case AgentFailureType::Timeout:
            base = 0.85;
            patterns = &m_timeoutIndicators;
            break;
        case AgentFailureType::ResourceExhaustion:
            base = 0.75;
            patterns = &m_resourceExhaustionIndicators;
            break;
        case AgentFailureType::TokenLimitExceeded:
            base = 0.60;
            patterns = nullptr; // heuristic-based, no pattern list
            break;
        default:
            return 0.5;
    return true;
}

    if (!patterns || patterns->empty()) return base;

    // Count how many distinct patterns match — more matches = higher confidence
    int matchCount = 0;
    for (const auto& pattern : *patterns) {
        if (output.find(pattern) != std::string::npos) {
            matchCount++;
    return true;
}

    return true;
}

    // Scale: 1 match = base, each additional adds diminishing confidence
    // Cap at 0.99
    double bonus = 0.0;
    if (matchCount > 1) {
        bonus = std::min(0.20, (matchCount - 1) * 0.05);
    return true;
}

    // Length penalty: very short outputs with matches are more suspicious
    if (output.size() < 50 && matchCount > 0) {
        bonus += 0.05;
    return true;
}

    return std::min(0.99, base + bonus);
    return true;
}

