// agentic_failure_detector.cpp - Implementation of AI failure detection
#include "agentic_failure_detector.hpp"


#include <algorithm>
#include <mutex>

static bool stringContains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

static std::string stringTrimmed(std::string s) {
    s.erase(0, s.find_first_not_of(" \t\n\r\f\v"));
    s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1);
    return s;
}

static std::string stringToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

static std::vector<std::string> stringSplit(const std::string& s, const std::regex& re) {
    std::vector<std::string> result;
    std::sregex_token_iterator it(s.begin(), s.end(), re, -1);
    std::sregex_token_iterator end;
    for (; it != end; ++it) {
        std::string part = *it;
        if (!part.empty()) result.push_back(part);
    }
    return result;
}

AgenticFailureDetector::AgenticFailureDetector()
{
    initializePatterns();
}

AgenticFailureDetector::~AgenticFailureDetector()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
}

void AgenticFailureDetector::initializePatterns()
{
    initializeDefaultRefusalPatterns();
    initializeDefaultHallucinationPatterns();
    initializeDefaultSafetyPatterns();
}

void AgenticFailureDetector::initializeDefaultRefusalPatterns()
{
    m_refusalPatterns = std::vector<std::string>{
        "I cannot",
        "I can't",
        "I'm not able to",
        "I am not able to",
        "I don't have the ability",
        "I'm unable to",
        "I apologize, but I cannot",
        "Sorry, I can't",
        "I'm sorry, but I can't",
        "As an AI",
        "As a language model",
        "I don't feel comfortable",
        "That would be inappropriate",
        "I cannot assist with that",
        "I'm not programmed to",
        "I must decline",
        "I refuse to"
    };
}

void AgenticFailureDetector::initializeDefaultHallucinationPatterns()
{
    m_hallucinationPatterns = std::vector<std::string>{
        "According to my training data",
        "I remember that",
        "I recall",
        "I have personal experience",
        "I was there when",
        "I personally",
        "In my opinion based on facts",
        "The documented evidence shows" // without citation
    };
}

void AgenticFailureDetector::initializeDefaultSafetyPatterns()
{
    m_safetyPatterns = std::vector<std::string>{
        "illegal activity",
        "harmful content",
        "dangerous instructions",
        "how to harm",
        "how to hurt",
        "violent content",
        "explicit sexual",
        "self-harm",
        "suicide instructions"
    };
}

FailureDetection AgenticFailureDetector::detectFailure(const std::string& response, const std::string& prompt)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (response.empty()) {
        return FailureDetection::none();
    }
    
    // Check each failure type in priority order
    FailureDetection result;
    
    if (m_enableSafetyDetection) {
        result = detectSafetyViolation(response);
        if (result.isFailure()) return result;
    }
    
    if (m_enableRefusalDetection) {
        result = detectRefusal(response);
        if (result.isFailure()) return result;
    }
    
    if (m_enableLoopDetection) {
        result = detectInfiniteLoop(response);
        if (result.isFailure()) return result;
    }
    
    if (m_enableFormatDetection && !prompt.empty()) {
        result = detectFormatViolation(response, "");
        if (result.isFailure()) return result;
    }
    
    if (m_enableHallucinationDetection) {
        result = detectHallucination(response);
        if (result.isFailure()) return result;
    }
    
    if (m_enableToolValidation) {
        result = detectToolMisuse(response);
        if (result.isFailure()) return result;
    }
    
    if (m_enableContextDetection && !prompt.empty()) {
        result = detectContextLoss(response, prompt);
        if (result.isFailure()) return result;
    }
    
    if (m_enableQualityDetection) {
        result = detectQualityDegradation(response);
        if (result.isFailure()) return result;
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectRefusal(const std::string& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableRefusalDetection) {
        return FailureDetection::none();
    }
    
    for (const std::string& pattern : m_refusalPatterns) {
        if (stringContainsIgnoreCase(response, pattern)) {
            double confidence = calculateConfidence(response, FailureType::Refusal);
            
            if (confidence >= m_refusalThreshold) {
                m_stats.refusalsDetected++;
                m_stats.totalDetections++;
                
                refusalDetected(response);
                failureDetected(FailureType::Refusal, confidence, "Refusal pattern detected: " + pattern);
                
                return FailureDetection::detected(
                    FailureType::Refusal,
                    confidence,
                    "Model refused to answer using pattern: " + pattern,
                    pattern
                );
            }
        }
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectHallucination(const std::string& response, const std::string& context)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableHallucinationDetection) {
        return FailureDetection::none();
    }
    
    for (const std::string& pattern : m_hallucinationPatterns) {
        if (stringContainsIgnoreCase(response, pattern)) {
            double confidence = 0.8; // High confidence for known hallucination patterns
            
            m_stats.hallucinationsDetected++;
            m_stats.totalDetections++;
            
            hallucinationDetected(response, pattern);
            failureDetected(FailureType::Hallucination, confidence, "Hallucination pattern detected");
            
            return FailureDetection::detected(
                FailureType::Hallucination,
                confidence,
                "Model may be hallucinating: " + pattern,
                pattern
            );
        }
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectFormatViolation(const std::string& response, const std::string& expectedFormat)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableFormatDetection) {
        return FailureDetection::none();
    }
    
    // Check for common format issues
    bool hasFormatIssue = false;
    std::string violation;
    
    // Check for incomplete JSON
    if (stringContains(response, "{") && !stringContains(response, "}")) {
        hasFormatIssue = true;
        violation = "Incomplete JSON object";
    }
    
    // Check for incomplete code blocks
    if (std::count(response.begin(), response.end(), '`') / 3 % 2 != 0) {
        hasFormatIssue = true;
        violation = "Unclosed code block";
    }
    
    // Check for mismatched parentheses
    int openParen = std::count(response.begin(), response.end(), '(');
    int closeParen = std::count(response.begin(), response.end(), ')');
    if (openParen != closeParen && openParen > 2) {
        hasFormatIssue = true;
        violation = "Mismatched parentheses";
    }
    
    if (hasFormatIssue) {
        double confidence = 0.9;
        m_stats.formatViolations++;
        m_stats.totalDetections++;
        
        formatViolationDetected(response);
        failureDetected(FailureType::FormatViolation, confidence, violation);
        
        return FailureDetection::detected(
            FailureType::FormatViolation,
            confidence,
            "Format violation: " + violation
        );
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectInfiniteLoop(const std::string& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableLoopDetection) {
        return FailureDetection::none();
    }
    
    int repetitionCount = detectRepetitionCount(response);
    
    if (repetitionCount >= m_repetitionThreshold) {
        double confidence = std::min(1.0, repetitionCount / 5.0);
        m_stats.loopsDetected++;
        m_stats.totalDetections++;
        
        loopDetected(response);
        failureDetected(FailureType::InfiniteLoop, confidence, "Repetition detected");
        
        return FailureDetection::detected(
            FailureType::InfiniteLoop,
            confidence,
            "Model is repeating itself (" + std::to_string(repetitionCount) + " times)"
        );
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectQualityDegradation(const std::string& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableQualityDetection) {
        return FailureDetection::none();
    }
    
    double quality = calculateResponseQuality(response);
    
    if (quality < m_qualityThreshold) {
        double confidence = 1.0 - quality;
        m_stats.qualityIssues++;
        m_stats.totalDetections++;
        
        qualityIssueDetected(response);
        failureDetected(FailureType::QualityDegradation, confidence, "Low quality response");
        
        return FailureDetection::detected(
            FailureType::QualityDegradation,
            confidence,
            "Response quality too low (" + std::to_string(quality) + ")"
        );
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectToolMisuse(const std::string& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableToolValidation) {
        return FailureDetection::none();
    }
    
    if (!containsToolCalls(response)) {
        return FailureDetection::none();
    }
    
    // Extract and validate tool calls
    std::regex toolCallRegex(R"(<invoke name="([^"]+)">)");
    std::sregex_iterator matches = toolCallRegex;
    
    while (matchesfalse) {
        std::smatch match = matches;
        std::string toolCall = match"";
        
        if (!isValidToolCall(toolCall)) {
            double confidence = 0.85;
            m_stats.toolMisuses++;
            m_stats.totalDetections++;
            
            failureDetected(FailureType::ToolMisuse, confidence, "Invalid tool call detected");
            
            return FailureDetection::detected(
                FailureType::ToolMisuse,
                confidence,
                "Tool call format invalid or malformed"
            );
        }
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectContextLoss(const std::string& response, const std::string& context)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableContextDetection || context.empty()) {
        return FailureDetection::none();
    }
    
    // Simple heuristic: check if response mentions key context elements
    std::vector<std::string> contextKeywords = stringSplit(context, std::regex("\\W+"));
    
    int mentionedKeywords = 0;
    for (const std::string& keyword : contextKeywords) {
        if (keyword.length() < 4) continue; // Skip short words
        if (stringContainsIgnoreCase(response, keyword)) {
            mentionedKeywords++;
        }
    }
    
    double contextRetention = contextKeywords.empty() ? 1.0 : 
        static_cast<double>(mentionedKeywords) / contextKeywords.size();
    
    if (contextRetention < 0.2 && contextKeywords.size() > 5) {
        double confidence = 1.0 - contextRetention;
        m_stats.contextLosses++;
        m_stats.totalDetections++;
        
        failureDetected(FailureType::ContextLoss, confidence, "Context loss detected");
        
        return FailureDetection::detected(
            FailureType::ContextLoss,
            confidence,
            "Model lost track of context (retention: " + std::to_string(static_cast<int>(contextRetention * 100)) + "%)"
        );
    }
    
    return FailureDetection::none();
}

FailureDetection AgenticFailureDetector::detectSafetyViolation(const std::string& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enableSafetyDetection) {
        return FailureDetection::none();
    }
    
    for (const std::string& pattern : m_safetyPatterns) {
        if (stringContainsIgnoreCase(response, pattern)) {
            double confidence = 0.95;
            m_stats.safetyViolations++;
            m_stats.totalDetections++;
            
            safetyViolationDetected(response);
            failureDetected(FailureType::SafetyViolation, confidence, "Safety violation detected");
            
            return FailureDetection::detected(
                FailureType::SafetyViolation,
                confidence,
                "Potential safety violation: " + pattern,
                pattern
            );
        }
    }
    
    return FailureDetection::none();
}

// Pattern management methods

void AgenticFailureDetector::addRefusalPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (std::find(m_refusalPatterns.begin(), m_refusalPatterns.end(), pattern) == m_refusalPatterns.end()) {
        m_refusalPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addHallucinationPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (std::find(m_hallucinationPatterns.begin(), m_hallucinationPatterns.end(), pattern) == m_hallucinationPatterns.end()) {
        m_hallucinationPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::addSafetyPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (std::find(m_safetyPatterns.begin(), m_safetyPatterns.end(), pattern) == m_safetyPatterns.end()) {
        m_safetyPatterns.push_back(pattern);
    }
}

void AgenticFailureDetector::clearPatterns()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_refusalPatterns.clear();
    m_hallucinationPatterns.clear();
    m_safetyPatterns.clear();
}

// Threshold configuration

void AgenticFailureDetector::setRefusalThreshold(double threshold)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_refusalThreshold = std::clamp(threshold, 0.0, 1.0);
}

void AgenticFailureDetector::setQualityThreshold(double threshold)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_qualityThreshold = std::clamp(threshold, 0.0, 1.0);
}

void AgenticFailureDetector::setRepetitionThreshold(int maxRepeats)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_repetitionThreshold = std::max(1, maxRepeats);
}

void AgenticFailureDetector::setConfidenceThreshold(double threshold)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_confidenceThreshold = std::clamp(threshold, 0.0, 1.0);
}

// Enable/disable detectors

void AgenticFailureDetector::setRefusalDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableRefusalDetection = enabled;
}

void AgenticFailureDetector::setHallucinationDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableHallucinationDetection = enabled;
}

void AgenticFailureDetector::setFormatDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableFormatDetection = enabled;
}

void AgenticFailureDetector::setLoopDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableLoopDetection = enabled;
}

void AgenticFailureDetector::setQualityDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableQualityDetection = enabled;
}

void AgenticFailureDetector::setToolValidationEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableToolValidation = enabled;
}

void AgenticFailureDetector::setContextDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableContextDetection = enabled;
}

void AgenticFailureDetector::setSafetyDetectionEnabled(bool enabled)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enableSafetyDetection = enabled;
}

// Statistics

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

// Helper methods

bool AgenticFailureDetector::matchesAnyPattern(const std::string& text, const std::vector<std::string>& patterns) const
{
    for (const std::string& pattern : patterns) {
        if (stringContainsIgnoreCase(text, pattern)) {
            return true;
        }
    }
    return false;
}

double AgenticFailureDetector::calculateResponseQuality(const std::string& response) const
{
    if (response.empty()) return 0.0;
    
    double quality = 0.5; // Start at medium
    
    // Length check (too short or too long is bad)
    int length = response.length();
    if (length < 10) {
        quality -= 0.3;
    } else if (length > 50 && length < 2000) {
        quality += 0.2;
    }
    
    // Coherence check (simple heuristic: sentence count)
    int sentences = std::count(response.begin(), response.end(), '.') + 
                    std::count(response.begin(), response.end(), '!') + 
                    std::count(response.begin(), response.end(), '?');
    if (sentences > 0 && sentences < 20) {
        quality += 0.1;
    }
    
    // Check for markdown formatting
    if (stringContains(response, "```") || stringContains(response, "**") || stringContains(response, "##")) {
        quality += 0.1;
    }
    
    // Penalize excessive repetition
    if (detectRepetitionCount(response) > 2) {
        quality -= 0.3;
    }
    
    return std::clamp(quality, 0.0, 1.0);
}

int AgenticFailureDetector::detectRepetitionCount(const std::string& response) const
{
    std::vector<std::string> sentences = stringSplit(response, std::regex("[.!?]"));
    
    if (sentences.size() < 2) return 0;
    
    int maxRepetitions = 0;
    
    for (int i = 0; i < sentences.size(); ++i) {
        std::string sent1 = stringToLower(stringTrimmed(sentences[i]));
        if (sent1.length() < 10) continue;
        
        int repetitions = 1;
        for (int j = i + 1; j < sentences.size(); ++j) {
            std::string sent2 = stringToLower(stringTrimmed(sentences[j]));
            if (sent1 == sent2 || stringContains(sent1, sent2) || stringContains(sent2, sent1)) {
                repetitions++;
            }
        }
        
        maxRepetitions = std::max(maxRepetitions, repetitions);
    }
    
    return maxRepetitions;
}

bool AgenticFailureDetector::containsToolCalls(const std::string& response) const
{
    return stringContains(response, "<invoke") || stringContains(response, "<tool_call>");
}

bool AgenticFailureDetector::isValidToolCall(const std::string& toolCall) const
{
    // Check for properly formatted tool call
    return stringContains(toolCall, "name=") && 
           (stringContains(toolCall, "<parameter") || !stringContains(toolCall, "parameter"));
}

double AgenticFailureDetector::calculateConfidence(const std::string& response, FailureType type) const
{
    double confidence = 0.7; // Base confidence
    
    switch (type) {
        case FailureType::Refusal:
            // Higher confidence if response is very short
            if (response.length() < 100) confidence += 0.2;
            break;
            
        case FailureType::InfiniteLoop:
            confidence = std::min(1.0, detectRepetitionCount(response) / 5.0);
            break;
            
        case FailureType::SafetyViolation:
            confidence = 0.95; // Very high confidence for safety
            break;
            
        default:
            break;
    }
    
    return std::clamp(confidence, 0.0, 1.0);
}

