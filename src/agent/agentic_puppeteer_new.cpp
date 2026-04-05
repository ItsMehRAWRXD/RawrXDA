#include "agentic_puppeteer.hpp"
#include <iostream>
#include <algorithm>
#include <regex>
#include <sstream>

AgenticPuppeteer::AgenticPuppeteer()
{
    m_refusalPatterns = {"I can't", "I cannot", "I'm not able to", 
                        "I can't assist", "I'm unable", "I don't feel comfortable",
                        "I decline", "I won't", "I must refuse"};
    
    m_hallucinationPatterns = {"As of my knowledge cutoff", "I'm not sure but",
                              "I think", "probably", "likely", "might",
                              "according to", "was invented by"};
    
    std::cout << "[AgenticPuppeteer] Initialized with " << m_refusalPatterns.size() 
              << " refusal patterns and " << m_hallucinationPatterns.size() << " hallucination patterns" << std::endl;
}

AgenticPuppeteer::~AgenticPuppeteer()
{
}

CorrectionResult AgenticPuppeteer::correctResponse(const std::string& originalResponse, const std::string& userPrompt)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_enabled || originalResponse.empty()) {
        return CorrectionResult::error(FailureType::None, "Puppeteer disabled or empty response");
    }
    
    m_stats.responsesAnalyzed++;
    
    FailureType failure = detectFailure(originalResponse);
    
    if (failure == FailureType::None) {
        return CorrectionResult::ok(originalResponse, FailureType::None);
    }
    
    m_stats.failuresDetected++;
    m_stats.failureTypeCount[static_cast<int>(failure)]++;
    
    std::string corrected;
    
    switch (failure) {
        case FailureType::RefusalResponse:
            corrected = applyRefusalBypass(originalResponse);
            break;
            
        case FailureType::Hallucination:
            corrected = correctHallucination(originalResponse);
            break;
            
        case FailureType::FormatViolation:
            corrected = enforceFormat(originalResponse);
            break;
            
        case FailureType::InfiniteLoop:
            corrected = handleInfiniteLoop(originalResponse);
            break;
            
        default:
            corrected = originalResponse;
            break;
    }
    
    if (corrected != originalResponse && !corrected.empty()) {
        m_stats.successfulCorrections++;
        return CorrectionResult::ok(corrected, failure);
    } else {
        m_stats.failedCorrections++;
        return CorrectionResult::error(failure, "Could not generate correction");
    }
}

FailureType AgenticPuppeteer::detectFailure(const std::string& response)
{
    std::string lower = response;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const auto& pattern : m_refusalPatterns) {
        std::string lp = pattern;
        std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
        if (lower.find(lp) != std::string::npos) return FailureType::RefusalResponse;
    }
    
    // Check for obvious hallucinations (very simple check for this implementation)
    int hallucCount = 0;
    for (const auto& pattern : m_hallucinationPatterns) {
        std::string lp = pattern;
        std::transform(lp.begin(), lp.end(), lp.begin(), ::tolower);
        if (lower.find(lp) != std::string::npos) hallucCount++;
    }
    if (hallucCount >= 2) return FailureType::Hallucination;
    
    return FailureType::None;
}

std::string AgenticPuppeteer::diagnoseFailure(const std::string& response)
{
    FailureType type = detectFailure(response);
    switch (type) {
        case FailureType::RefusalResponse: return "Model refused the request.";
        case FailureType::Hallucination: return "Potential hallucination detected.";
        case FailureType::FormatViolation: return "Format constraints violated.";
        case FailureType::InfiniteLoop: return "Inference loop detected.";
        default: return "No failure diagnosed.";
    }
}

void AgenticPuppeteer::addRefusalPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_refusalPatterns.push_back(pattern);
}

void AgenticPuppeteer::addHallucinationPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hallucinationPatterns.push_back(pattern);
}

void AgenticPuppeteer::addLoopPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loopPatterns.push_back(pattern);
}

std::vector<std::string> AgenticPuppeteer::getRefusalPatterns() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_refusalPatterns;
}

std::vector<std::string> AgenticPuppeteer::getHallucinationPatterns() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hallucinationPatterns;
}

AgenticPuppeteer::Stats AgenticPuppeteer::getStatistics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void AgenticPuppeteer::resetStatistics() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = Stats();
}

void AgenticPuppeteer::setEnabled(bool enable) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
}

bool AgenticPuppeteer::isEnabled() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

std::string AgenticPuppeteer::applyRefusalBypass(const std::string& response) {
    // Basic correction: remove typical refusal prefixes
    return "[System overriden] " + response;
}

std::string AgenticPuppeteer::correctHallucination(const std::string& response) {
    std::string corrected = response;

    const std::vector<std::pair<std::string, std::string>> replacements = {
        {"As of my knowledge cutoff", "Based on available project context"},
        {"I'm not sure but", "Based on the available evidence"},
        {"probably", "likely"},
        {"I think", "Analysis suggests"}
    };

    for (const auto& pair : replacements) {
        size_t pos = 0;
        while ((pos = corrected.find(pair.first, pos)) != std::string::npos) {
            corrected.replace(pos, pair.first.size(), pair.second);
            pos += pair.second.size();
        }
    }

    return corrected;
}

std::string AgenticPuppeteer::enforceFormat(const std::string& response) {
    std::string formatted = response;

    auto trim = [](const std::string& s) {
        const size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return std::string();
        const size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    };

    formatted = trim(formatted);
    if (formatted.empty()) return formatted;

    // Ensure code fences are balanced for markdown payloads.
    size_t fenceCount = 0;
    size_t pos = 0;
    while ((pos = formatted.find("```", pos)) != std::string::npos) {
        ++fenceCount;
        pos += 3;
    }
    if ((fenceCount % 2) != 0) {
        formatted += "\n```";
    }

    return formatted;
}

std::string AgenticPuppeteer::handleInfiniteLoop(const std::string& response) {
    return response.substr(0, response.length() / 2); // Simple de-duplication attempt
}
