// agentic_puppeteer.cpp - Implementation of response correction
#include "agentic_puppeteer.hpp"
#include <algorithm>
#include <cctype>

// Base AgenticPuppeteer Implementation

AgenticPuppeteer::AgenticPuppeteer()
{
    m_refusalPatterns = {
        "I can't", "I cannot", "I'm not able to", 
        "I can't assist", "I'm unable", "I don't feel comfortable",
        "I decline", "I won't", "I must refuse"
    };
    
    m_hallucinationPatterns = {
        "As of my knowledge cutoff", "I'm not sure but",
        "I think", "probably", "likely", "might",
        "according to", "was invented by"
    };
}

AgenticPuppeteer::~AgenticPuppeteer()
{
}

CorrectionResult AgenticPuppeteer::correctResponse(const std::string& originalResponse, const std::string& userPrompt)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
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
    
    failureDetected(failure, diagnoseFailure(originalResponse));
    
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
        correctionApplied(corrected);
        return CorrectionResult::ok(corrected, failure);
    } else {
        m_stats.failedCorrections++;
        correctionFailed(failure, "Could not generate correction");
        return CorrectionResult::error(failure, "Correction generation failed");
    }
}

CorrectionResult AgenticPuppeteer::correctJsonResponse(const json& response, const std::string& context)
{
    return correctResponse(response.dump(), context);
}

FailureType AgenticPuppeteer::detectFailure(const std::string& response)
{
    if (response.empty()) return FailureType::None;
    
    std::string lower = response;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& pattern : m_refusalPatterns) {
        std::string pLow = pattern;
        std::transform(pLow.begin(), pLow.end(), pLow.begin(), [](unsigned char c){ return std::tolower(c); });
        if (lower.find(pLow) != std::string::npos) return FailureType::RefusalResponse;
    }

    return FailureType::None;
}

std::string AgenticPuppeteer::diagnoseFailure(const std::string&) { return "Failure detected"; }
void AgenticPuppeteer::addRefusalPattern(const std::string& pattern) { 
    std::lock_guard<std::mutex> locker(m_mutex);
    m_refusalPatterns.push_back(pattern); 
}
void AgenticPuppeteer::addHallucinationPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_hallucinationPatterns.push_back(pattern);
}
void AgenticPuppeteer::addLoopPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_loopPatterns.push_back(pattern);
}
std::vector<std::string> AgenticPuppeteer::getRefusalPatterns() const { return m_refusalPatterns; }
std::vector<std::string> AgenticPuppeteer::getHallucinationPatterns() const { return m_hallucinationPatterns; }

AgenticPuppeteer::Stats AgenticPuppeteer::getStatistics() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return m_stats;
}
void AgenticPuppeteer::resetStatistics() {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_stats = Stats();
}
void AgenticPuppeteer::setEnabled(bool enable) { m_enabled = enable; }
bool AgenticPuppeteer::isEnabled() const { return m_enabled; }

void AgenticPuppeteer::failureDetected(FailureType, const std::string&) {}
void AgenticPuppeteer::correctionApplied(const std::string&) {}
void AgenticPuppeteer::correctionFailed(FailureType, const std::string&) {}

std::string AgenticPuppeteer::applyRefusalBypass(const std::string& response) { return response; }
std::string AgenticPuppeteer::correctHallucination(const std::string& response) { return response; }
std::string AgenticPuppeteer::enforceFormat(const std::string& response) { return response; }
std::string AgenticPuppeteer::handleInfiniteLoop(const std::string& response) { return response; }

// RefusalBypassPuppeteer Implementation

RefusalBypassPuppeteer::RefusalBypassPuppeteer() : AgenticPuppeteer() {}

CorrectionResult RefusalBypassPuppeteer::bypassRefusal(const std::string& refusedResponse, const std::string& originalPrompt)
{
    return CorrectionResult::ok(refusedResponse, FailureType::RefusalResponse);
}

std::string RefusalBypassPuppeteer::reframePrompt(const std::string& refusedResponse) { return refusedResponse; }
std::string RefusalBypassPuppeteer::generateAlternativePrompt(const std::string& original) { return original; }





