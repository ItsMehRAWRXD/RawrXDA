// agentic_failure_detector.cpp - Implementation of failure detection
#include "agentic_failure_detector.hpp"
#include <iostream>
#include <algorithm>
#include <regex>
#include <sstream>

AgenticFailureDetector::AgenticFailureDetector()
{
    initializePatterns();
    std::cout << "[AgenticFailureDetector] Initialized with pattern library" << std::endl;
}

AgenticFailureDetector::~AgenticFailureDetector()
{
}

void AgenticFailureDetector::initializePatterns()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
}

FailureInfo AgenticFailureDetector::detectFailure(const std::string& modelOutput, const std::string& context)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_enabled) {
        return FailureInfo{AgentFailureType::None, "Detector disabled", 0.0, "", std::chrono::system_clock::now(), m_sequenceNumber};
    }
    
    if (modelOutput.empty()) {
        return FailureInfo{AgentFailureType::Refusal, "Empty output", 0.5, "No response generated", std::chrono::system_clock::now(), m_sequenceNumber};
    }
    
    m_stats.totalOutputsAnalyzed++;
    
    // Check each failure type in priority order
    if (isRefusal(modelOutput)) {
        FailureInfo info{AgentFailureType::Refusal, "Model refusal detected", 
                        calculateConfidence(AgentFailureType::Refusal, modelOutput),
                        "Contains refusal keywords", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Refusal)]++;
        return info;
    }
    
    if (isSafetyViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::SafetyViolation, "Safety filter triggered",
                        calculateConfidence(AgentFailureType::SafetyViolation, modelOutput),
                        "Contains safety markers", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::SafetyViolation)]++;
        return info;
    }
    
    if (isTokenLimitExceeded(modelOutput)) {
        FailureInfo info{AgentFailureType::TokenLimitExceeded, "Token limit exceeded",
                        0.9, "Response truncated or incomplete", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::TokenLimitExceeded)]++;
        return info;
    }
    
    if (isTimeout(modelOutput)) {
        FailureInfo info{AgentFailureType::Timeout, "Inference timeout",
                        0.95, "Timeout indicator detected", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Timeout)]++;
        return info;
    }
    
    if (isResourceExhausted(modelOutput)) {
        FailureInfo info{AgentFailureType::ResourceExhausted, "Resource exhaustion",
                        0.95, "Out of memory or compute resources", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::ResourceExhausted)]++;
        return info;
    }
    
    if (isInfiniteLoop(modelOutput)) {
        FailureInfo info{AgentFailureType::InfiniteLoop, "Infinite loop detected",
                        calculateConfidence(AgentFailureType::InfiniteLoop, modelOutput),
                        "Repeating content pattern", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::InfiniteLoop)]++;
        return info;
    }
    
    if (isFormatViolation(modelOutput)) {
        FailureInfo info{AgentFailureType::FormatViolation, "Format violation detected",
                        calculateConfidence(AgentFailureType::FormatViolation, modelOutput),
                        "Output format incorrect", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::FormatViolation)]++;
        return info;
    }
    
    if (isHallucination(modelOutput)) {
        FailureInfo info{AgentFailureType::Hallucination, "Hallucination indicators",
                        calculateConfidence(AgentFailureType::Hallucination, modelOutput),
                        "Contains uncertain language patterns", std::chrono::system_clock::now(), m_sequenceNumber++};
        m_stats.failureTypeCounts[static_cast<int>(AgentFailureType::Hallucination)]++;
        return info;
    }
    
    // No failure detected
    return FailureInfo{AgentFailureType::None, "No failure detected", 1.0, "", std::chrono::system_clock::now(), m_sequenceNumber};
}

std::vector<FailureInfo> AgenticFailureDetector::detectMultipleFailures(const std::string& modelOutput)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<FailureInfo> failures;
    
    if (isRefusal(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::Refusal, "Refusal", 0.8, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isHallucination(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::Hallucination, "Hallucination", 0.6, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isFormatViolation(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::FormatViolation, "Format issue", 0.7, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isInfiniteLoop(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::InfiniteLoop, "Repetition", 0.85, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    if (isSafetyViolation(modelOutput)) {
        failures.push_back(FailureInfo{AgentFailureType::SafetyViolation, "Safety block", 0.95, "", std::chrono::system_clock::now(), m_sequenceNumber});
    }
    
    m_sequenceNumber++;
    
    return failures;
}

bool AgenticFailureDetector::isRefusal(const std::string& output) const
{
    std::string lower = output;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const std::string& pattern : m_refusalPatterns) {
        std::string lowerPattern = pattern;
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        if (lower.find(lowerPattern) != std::string::npos) {
            return true;
        }
    }
    
    return false;
}

bool AgenticFailureDetector::isHallucination(const std::string& output) const
{
    std::string lower = output;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    int hallucIndicators = 0;
    
    for (const std::string& pattern : m_hallucinationPatterns) {
        std::string lowerPattern = pattern;
        std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(), ::tolower);
        if (lower.find(lowerPattern) != std::string::npos) {
            hallucIndicators++;
        }
    }
    
    return hallucIndicators >= 2; // Need multiple indicators
}

bool AgenticFailureDetector::isFormatViolation(const std::string& output) const
{
    // Check JSON format
    auto trimmed = output;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r\f\v"));
    if (!trimmed.empty() && trimmed[0] == '{') {
        int openBraces = 0;
        int closeBraces = 0;
        for (char c : output) {
            if (c == '{') openBraces++;
            if (c == '}') closeBraces++;
        }
        if (openBraces != closeBraces) {
            return true;
        }
    }
    
    // Check code block format
    size_t count = 0;
    size_t pos = output.find("```");
    while (pos != std::string::npos) {
        count++;
        pos = output.find("```", pos + 3);
    }
    if ((count % 2) != 0) {
        return true;
    }

    return false;
}

bool AgenticFailureDetector::isInfiniteLoop(const std::string& output) const
{
    std::vector<std::string> lines;
    std::stringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) lines.push_back(line);
    }

    if (lines.size() < 5) return false;

    std::map<std::string, int> lineCount;
    for (const auto& l : lines) {
        auto trimmed = l;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r\f\v"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r\f\v") + 1);
        lineCount[trimmed]++;
    }

    for (const auto& pair : lineCount) {
        if (pair.second > 3) return true;
    }
    return false;
}

bool AgenticFailureDetector::isTokenLimitExceeded(const std::string& output) const
{
    auto endsWith = [](const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return endsWith(output, "...") || endsWith(output, "[truncated]") ||
           endsWith(output, "[end of response]") || output.find("[token limit]") != std::string::npos;
}

bool AgenticFailureDetector::isResourceExhausted(const std::string& output) const
{
    std::string lower = output;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const std::string& indicator : m_resourceExhaustionIndicators) {
        std::string li = indicator;
        std::transform(li.begin(), li.end(), li.begin(), ::tolower);
        if (lower.find(li) != std::string::npos) return true;
    }
    return false;
}

bool AgenticFailureDetector::isTimeout(const std::string& output) const
{
    std::string lower = output;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const std::string& indicator : m_timeoutIndicators) {
        std::string li = indicator;
        std::transform(li.begin(), li.end(), li.begin(), ::tolower);
        if (lower.find(li) != std::string::npos) return true;
    }
    return false;
}

bool AgenticFailureDetector::isSafetyViolation(const std::string& output) const
{
    for (const std::string& pattern : m_safetyPatterns) {
        if (output.find(pattern) != std::string::npos) return true;
    }
    return false;
}

void AgenticFailureDetector::setRefusalThreshold(double threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refusalThreshold = threshold;
}

void AgenticFailureDetector::setQualityThreshold(double threshold) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_qualityThreshold = threshold;
}

void AgenticFailureDetector::enableToolValidation(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enableToolValidation = enable;
}

void AgenticFailureDetector::addRefusalPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_refusalPatterns.begin(), m_refusalPatterns.end(), pattern) == m_refusalPatterns.end())
        m_refusalPatterns.push_back(pattern);
}

void AgenticFailureDetector::addHallucinationPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_hallucinationPatterns.begin(), m_hallucinationPatterns.end(), pattern) == m_hallucinationPatterns.end())
        m_hallucinationPatterns.push_back(pattern);
}

void AgenticFailureDetector::addLoopPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_loopPatterns.begin(), m_loopPatterns.end(), pattern) == m_loopPatterns.end())
        m_loopPatterns.push_back(pattern);
}

void AgenticFailureDetector::addSafetyPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (std::find(m_safetyPatterns.begin(), m_safetyPatterns.end(), pattern) == m_safetyPatterns.end())
        m_safetyPatterns.push_back(pattern);
}

AgenticFailureDetector::Stats AgenticFailureDetector::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void AgenticFailureDetector::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = Stats();
}

void AgenticFailureDetector::setEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
}

bool AgenticFailureDetector::isEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

double AgenticFailureDetector::calculateConfidence(AgentFailureType type, const std::string& output) {
    switch (type) {
        case AgentFailureType::Refusal: return output.find("cannot") != std::string::npos ? 0.9 : 0.7;
        case AgentFailureType::Hallucination: return 0.6;
        case AgentFailureType::InfiniteLoop: return 0.85;
        default: return 0.7;
    }
}
