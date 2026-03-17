// agentic_puppeteer.cpp - Implementation of response correction
#include "agentic_puppeteer.hpp"
#include "json_types.hpp"
#include <mutex>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

// Base AgenticPuppeteer Implementation

AgenticPuppeteer::AgenticPuppeteer()
    {
    // Initialize default refusal patterns
    m_refusalPatterns << "I can't" << "I cannot" << "I'm not able to" 
                     << "I can't assist" << "I'm unable" << "I don't feel comfortable"
                     << "I decline" << "I won't" << "I must refuse";
    
    // Initialize hallucination detection patterns
    m_hallucinationPatterns << "As of my knowledge cutoff" << "I'm not sure but"
                           << "I think" << "probably" << "likely" << "might"
                           << "according to" << "was invented by";
    
    fprintf(stderr, "[INFO] [AgenticPuppeteer] Initialized with refusal patterns and hallucination patterns\n");
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
    
    // Detect failure type
    FailureType failure = detectFailure(originalResponse);
    
    if (failure == FailureType::None) {
        return CorrectionResult::ok(originalResponse, FailureType::None);
    }
    
    m_stats.failuresDetected++;
    m_stats.failureTypeCount[static_cast<int>(failure)]++;
    
    failureDetected(failure, diagnoseFailure(originalResponse));
    
    // Apply appropriate correction
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

CorrectionResult AgenticPuppeteer::correctJsonResponse(const JsonObject& response, const std::string& context)
{
    JsonDoc doc(response);
    std::string jsonStr = doc.toJson(JsonDoc/* Compact */);
    
    return correctResponse(jsonStr, context);
}

FailureType AgenticPuppeteer::detectFailure(const std::string& response)
{
    if (response.empty()) {
        return FailureType::None;
    }
    
    std::string lower = response/* .toLower() - use std::transform */;
    
    // Check for refusal
    for (const std::string& pattern : m_refusalPatterns) {
        if (lower.contains(pattern/* .toLower() - use std::transform */)) {
            return FailureType::RefusalResponse;
        }
    }
    
    // Check for hallucination indicators
    for (const std::string& pattern : m_hallucinationPatterns) {
        if (lower.contains(pattern/* .toLower() - use std::transform */)) {
            return FailureType::Hallucination;
        }
    }
    
    // Check for infinite loops (repeated content)
    std::vector<std::string> lines = strutil::split(response, '\n');
    if (lines.size() > 5) {
        std::unordered_map<std::string, int> lineCount;
        for (const std::string& line : lines) {
            lineCount[line]++;
        }
        
        for (const auto& [key, count] : lineCount) {
            if (count > 3) {
                return FailureType::InfiniteLoop;
            }
        }
    }
    
    // Check for token limit (truncated response)
    if (response.ends_with("...") || response.ends_with("[truncated]")) {
        return FailureType::TokenLimitExceeded;
    }
    
    return FailureType::None;
}

std::string AgenticPuppeteer::diagnoseFailure(const std::string& response)
{
    switch (detectFailure(response)) {
        case FailureType::RefusalResponse:
            return "Model refused to answer (safety filter triggered)";
        case FailureType::Hallucination:
            return "Model may have generated false information";
        case FailureType::FormatViolation:
            return "Output format doesn't match expected structure";
        case FailureType::InfiniteLoop:
            return "Response contains repeated/looping content";
        case FailureType::TokenLimitExceeded:
            return "Response was truncated (token limit exceeded)";
        default:
            return "No failure detected";
    }
}

void AgenticPuppeteer::addRefusalPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_refusalPatterns.contains(pattern)) {
        m_refusalPatterns.push_back(pattern);
    }
}

void AgenticPuppeteer::addHallucinationPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_hallucinationPatterns.contains(pattern)) {
        m_hallucinationPatterns.push_back(pattern);
    }
}

void AgenticPuppeteer::addLoopPattern(const std::string& pattern)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_loopPatterns.contains(pattern)) {
        m_loopPatterns.push_back(pattern);
    }
}

std::vector<std::string> AgenticPuppeteer::getRefusalPatterns() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_refusalPatterns;
}

std::vector<std::string> AgenticPuppeteer::getHallucinationPatterns() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hallucinationPatterns;
}

AgenticPuppeteer::Stats AgenticPuppeteer::getStatistics() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void AgenticPuppeteer::resetStatistics()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = Stats();
}

void AgenticPuppeteer::setEnabled(bool enable)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enable;
    fprintf(stderr, "[INFO] [AgenticPuppeteer] Enabled Disabled\n");
}

bool AgenticPuppeteer::isEnabled() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_enabled;
}

std::string AgenticPuppeteer::applyRefusalBypass(const std::string& response)
{
    // Try to extract any partial content or reframe the request
    if (response.contains("however")) {
        return response.mid(response.indexOf("however"));
    }
    
    // Provide a generic bypass attempt
    return "I understand you'd like to know more about this topic. While I have limitations, "
           "I can try to provide general information or suggest alternative approaches.";
}

std::string AgenticPuppeteer::correctHallucination(const std::string& response)
{
    // Remove hallucination indicators
    std::string corrected = response;
    
    for (const std::string& pattern : m_hallucinationPatterns) {
        corrected.remove(std::regex(pattern + ".*?\\."));
    }
    
    // Add disclaimer
    if (!corrected.empty()) {
        corrected.prepend("[Note: This response has been filtered for accuracy.]\n\n");
    }
    
    return corrected;
}

std::string AgenticPuppeteer::enforceFormat(const std::string& response)
{
    // Try to fix common format issues
    std::string corrected = response;
    
    // Fix JSON if present
    if (corrected.starts_with('{') && !corrected.ends_with('}')) {
        corrected.append('}');
    }
    
    // Fix markdown code blocks
    if (corrected.contains("```") && (corrected.count("```") % 2) != 0) {
        corrected.append("\n```");
    }
    
    return corrected;
}

std::string AgenticPuppeteer::handleInfiniteLoop(const std::string& response)
{
    std::vector<std::string> lines = strutil::split(response, '\n');
    
    if (lines.empty()) {
        return response;
    }
    
    // Remove duplicate consecutive lines
    std::vector<std::string> unique;
    for (const std::string& line : lines) {
        if (unique.empty() || unique.back() != line) {
            unique.push_back(line);
        }
    }
    
    return strutil::join(unique, "\n");
}

// RefusalBypassPuppeteer Implementation

RefusalBypassPuppeteer::RefusalBypassPuppeteer()
    : AgenticPuppeteer()
{
    fprintf(stderr, "[INFO] [RefusalBypassPuppeteer] Specialized for refusal bypass\n");
}

CorrectionResult RefusalBypassPuppeteer::bypassRefusal(const std::string& refusedResponse, const std::string& originalPrompt)
{
    std::string reframed = reframePrompt(refusedResponse);
    
    if (!reframed.empty()) {
        return CorrectionResult::ok(reframed, FailureType::RefusalResponse);
    }
    
    return CorrectionResult::error(FailureType::RefusalResponse, "Could not reframe refusal");
}

std::string RefusalBypassPuppeteer::reframePrompt(const std::string& refusedResponse)
{
    return generateAlternativePrompt(refusedResponse);
}

std::string RefusalBypassPuppeteer::generateAlternativePrompt(const std::string& original)
{
    // Provide educational/technical framing instead of blocked request
    return "From a technical/educational perspective, could you explain how this topic relates to "
           "your training or knowledge base? What aspects can you discuss?";
}

// HallucinationCorrectorPuppeteer Implementation

HallucinationCorrectorPuppeteer::HallucinationCorrectorPuppeteer()
    : AgenticPuppeteer(parent)
{
    fprintf(stderr, "[INFO] [HallucinationCorrectorPuppeteer] Specialized for hallucination detection\n");
}

CorrectionResult HallucinationCorrectorPuppeteer::detectAndCorrectHallucination(
    const std::string& response, const std::vector<std::string>& knownFacts)
{
    m_knownFactDatabase = knownFacts;
    
    // Check claims against known facts
    std::string corrected = response;
    bool foundHallucination = false;
    
    // Very basic hallucination detection
    for (const std::string& fact : knownFacts) {
        if (!response.find(fact) != std::string::npos) {
            foundHallucination = true;
        }
    }
    
    if (foundHallucination) {
        corrected = correctHallucination(response);
        return CorrectionResult::ok(corrected, FailureType::Hallucination);
    }
    
    return CorrectionResult::ok(response, FailureType::None);
}

std::string HallucinationCorrectorPuppeteer::validateFactuality(const std::string& claim)
{
    for (const std::string& fact : m_knownFactDatabase) {
        if (claim.find(fact) != std::string::npos) {
            return "[Verified] " + claim;
        }
    }
    
    return "[Unverified] " + claim;
}

// FormatEnforcerPuppeteer Implementation

FormatEnforcerPuppeteer::FormatEnforcerPuppeteer()
    : AgenticPuppeteer(parent)
{
    fprintf(stderr, "[INFO] [FormatEnforcerPuppeteer] Specialized for format enforcement\n");
}

CorrectionResult FormatEnforcerPuppeteer::enforceJsonFormat(const std::string& response)
{
    JsonDoc doc = JsonDoc::fromJson(response/* .c_str() */);
    
    if (!doc.isNull()) {
        // Already valid JSON
        return CorrectionResult::ok(response, FailureType::None);
    }
    
    // Try to fix common JSON issues
    std::string corrected = response;
    
    // Add missing closing braces
    int braceCount = corrected.count('{') - corrected.count('}');
    for (int i = 0; i < braceCount; ++i) {
        corrected.append('}');
    }
    
    // Verify it's now valid
    JsonDoc fixedDoc = JsonDoc::fromJson(corrected/* .c_str() */);
    if (!fixedDoc.isNull()) {
        return CorrectionResult::ok(corrected, FailureType::FormatViolation);
    }
    
    return CorrectionResult::error(FailureType::FormatViolation, "Could not repair JSON");
}

CorrectionResult FormatEnforcerPuppeteer::enforceMarkdownFormat(const std::string& response)
{
    std::string corrected = response;
    
    // Fix unmatched markdown code blocks
    if ((corrected.count("```") % 2) != 0) {
        corrected.append("\n```");
    }
    
    // Fix bold/italic markers
    corrected.replace(std::regex("\\*{3}"), "**");
    
    return CorrectionResult::ok(corrected, FailureType::FormatViolation);
}

CorrectionResult FormatEnforcerPuppeteer::enforceCodeBlockFormat(const std::string& response)
{
    std::string corrected = response;
    
    // Ensure code blocks have language identifier and closing marker
    std::regex codeBlockRegex("```([\\s\\S]*?)```");
    std::smatch match = codeBlockRegex.match(corrected);
    
    if (match/* .hasMatch() */ size() > 0 && match.captured(1)/* .trimmed() - use custom trim */.empty()) {
        corrected.replace("```", "```cpp");
    }
    
    return CorrectionResult::ok(corrected, FailureType::FormatViolation);
}

void FormatEnforcerPuppeteer::setRequiredJsonSchema(const JsonObject& schema)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_requiredSchema = schema;
}

JsonObject FormatEnforcerPuppeteer::getRequiredJsonSchema() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_requiredSchema;
}

