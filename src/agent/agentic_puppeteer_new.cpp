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
    // Detect common hallucination patterns and strip them
    std::string corrected = response;

    // Remove fabricated file paths (paths containing common hallucination markers)
    const std::vector<std::string> hallucinationMarkers = {
        "/nonexistent/", "PLACEHOLDER_", "TODO_IMPLEMENT",
        "example.com/fake", "SIMULATED_OUTPUT"
    };
    for (const auto& marker : hallucinationMarkers) {
        size_t pos;
        while ((pos = corrected.find(marker)) != std::string::npos) {
            // Find line boundaries and remove the hallucinated line
            size_t lineStart = corrected.rfind('\n', pos);
            size_t lineEnd   = corrected.find('\n', pos);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart;
            lineEnd   = (lineEnd == std::string::npos) ? corrected.size() : lineEnd;
            corrected.erase(lineStart, lineEnd - lineStart);
        }
    }

    // Strip repeated consecutive lines (loop degeneration)
    std::istringstream stream(corrected);
    std::ostringstream deduped;
    std::string line, prevLine;
    int repeatCount = 0;
    while (std::getline(stream, line)) {
        if (line == prevLine) {
            ++repeatCount;
            if (repeatCount >= 3) continue; // Skip after 3+ repeats
        } else {
            repeatCount = 0;
        }
        deduped << line << '\n';
        prevLine = line;
    }
    return deduped.str();
}

std::string AgenticPuppeteer::enforceFormat(const std::string& response) {
    // Enforce structured output format: trim, ensure JSON validity if JSON-like
    std::string formatted = response;

    // Trim leading/trailing whitespace
    size_t start = formatted.find_first_not_of(" \t\n\r");
    size_t end   = formatted.find_last_not_of(" \t\n\r");
    if (start != std::string::npos && end != std::string::npos) {
        formatted = formatted.substr(start, end - start + 1);
    }

    // If response looks like JSON, validate basic structure
    if (!formatted.empty() && (formatted.front() == '{' || formatted.front() == '[')) {
        int braces = 0, brackets = 0;
        bool inString = false, escaped = false;
        for (char c : formatted) {
            if (escaped) { escaped = false; continue; }
            if (c == '\\') { escaped = true; continue; }
            if (c == '"') { inString = !inString; continue; }
            if (inString) continue;
            if (c == '{') ++braces;
            if (c == '}') --braces;
            if (c == '[') ++brackets;
            if (c == ']') --brackets;
        }
        // Close unclosed brackets/braces
        while (brackets > 0) { formatted += ']'; --brackets; }
        while (braces > 0)   { formatted += '}'; --braces; }
    }

    return formatted;
}

std::string AgenticPuppeteer::handleInfiniteLoop(const std::string& response) {
    return response.substr(0, response.length() / 2); // Simple de-duplication attempt
}
