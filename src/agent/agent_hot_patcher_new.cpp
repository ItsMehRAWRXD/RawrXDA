#include "agent_hot_patcher.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

AgentHotPatcher::AgentHotPatcher() {
    m_enabled = false;
    m_debugLogging = false;
    m_idCounter = 0;
}

AgentHotPatcher::~AgentHotPatcher() = default;

bool AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int interceptionPort) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_ggufLoaderPath = ggufLoaderPath;
    m_interceptionPort = interceptionPort;
    
JsonValue AgentHotPatcher::applyBehaviorPatches(const JsonValue& output, const JsonValue& context) {
    if (!m_enabled) {
        return output;
    }

    std::lock_guard<std::mutex> locker(m_mutex);
    JsonValue patched = output;
    
    // Implementation for applyBehaviorPatches...

    return patched;
}
    if (!m_enabled) {
        return JsonValue::parse(modelOutputStr);
    }

    std::lock_guard<std::mutex> locker(m_mutex);
    JsonValue output = JsonValue::parse(modelOutputStr);
    
    // Hallucination Check
    if (output.contains("reasoning") || output.contains("thinking")) {
        std::string reasoning = output.contains("reasoning") ? 
                            output.value("reasoning").toString("") : 
                            output.value("thinking").toString("");

        HallucinationDetection h = detectHallucination(reasoning, context);
    m_totalHallucinationsDetected++;
    std::string corrected = correctHallucination(h);
    if (!corrected.empty()) {
        if (output.contains("reasoning")) output["reasoning"] = corrected;
        else if (output.contains("thinking")) output["thinking"] = corrected;
    }
}
                 m_hallucinationsCorrected++;
             }
        }
    }

    // Navigation Check
    if (output.contains("navigationPath")) {
        std::string navPath = output.value("navigationPath").toString("");
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty()) {
            output["navigationPath"] = fix.correctPath;
            m_navigationFixesApplied++;
        }
    }

    return output;
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled) {
    m_enabled = enabled;
}

bool AgentHotPatcher::isHotPatchingEnabled() const {
    return m_enabled;
}

void AgentHotPatcher::setDebugLogging(bool enabled) {
    m_debugLogging = enabled;
}

JsonValue AgentHotPatcher::getCorrectionStatistics() const {
    JsonValue stats = JsonValue::object();
    stats["total_hallucinations"] = (int)m_totalHallucinationsDetected.load();
    stats["total_corrected"] = (int)m_hallucinationsCorrected.load();
    stats["total_nav_fixes"] = (int)m_navigationFixesApplied.load();
    return stats;
}

int AgentHotPatcher::getCorrectionPatternCount() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return (int)m_hallucationPatterns.size() + (int)m_navigationPatterns.size();
}

bool AgentHotPatcher::isValidPath(const std::string& path) const {
    try { return fs::exists(path); } catch(...) { return false; }
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const JsonValue& context) {
    if (content.find("/nonexistent/") != std::string::npos) {
        return detectPathHallucination(content);
    }
    return {};
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {
    auto it = m_hallucationPatterns.find(detection.detectedContent);
    if (it != m_hallucationPatterns.end()) {
        return it->second;
    }
    return "";
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const JsonValue& context) {
    NavigationFix fix;
    auto it = m_navigationPatterns.find(path);
    if (it != m_navigationPatterns.end()) {
        fix.fixId = "fix-" + generateUniqueId();
        fix.incorrectPath = path;
        fix.correctPath = it->second;
    }
    return fix;
}

JsonValue AgentHotPatcher::applyBehaviorPatches(const JsonValue& output, const JsonValue& context) {
    return output;
}

void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_hallucationPatterns[pattern.detectedContent] = pattern.expectedContent;
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& fix) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_navigationPatterns[fix.incorrectPath] = fix.correctPath;
}

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& patch) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_behaviorPatches.push_back(patch);
}

HallucinationDetection AgentHotPatcher::analyzeForHallucinations(const std::string& content) {
    // Run all sub-detectors, return highest-confidence result
    HallucinationDetection best{};
    
    auto pathResult = detectPathHallucination(content);
    if (pathResult.confidence > best.confidence) best = pathResult;
    
    auto logicResult = detectLogicContradiction(content);
    if (logicResult.confidence > best.confidence) best = logicResult;
    
    auto incompleteResult = detectIncompleteReasoning(content);
    if (incompleteResult.confidence > best.confidence) best = incompleteResult;
    
    return best;
}
bool AgentHotPatcher::validateNavigationPath(const std::string& path) { return isValidPath(path); }

std::string AgentHotPatcher::extractReasoningChain(const JsonValue& output) {
    // Extract reasoning/thinking field from model output JSON
    if (output.contains("reasoning") && output["reasoning"].is_string())
        return output["reasoning"].get<std::string>();
    if (output.contains("thinking") && output["thinking"].is_string())
        return output["thinking"].get<std::string>();
    if (output.contains("chain_of_thought") && output["chain_of_thought"].is_string())
        return output["chain_of_thought"].get<std::string>();
    return "";
}

std::vector<std::string> AgentHotPatcher::validateReasoningLogic(const std::string& reasoning) {
    std::vector<std::string> issues;
    if (reasoning.empty()) {
        issues.push_back("Empty reasoning chain");
        return issues;
    }
    // Check for self-contradictions
    if (reasoning.find("however") != std::string::npos && reasoning.find("therefore") != std::string::npos) {
        // Look for "X is true ... however X is false" patterns
        if (reasoning.find("is not") != std::string::npos || reasoning.find("is false") != std::string::npos) {
            issues.push_back("Potential self-contradiction detected");
        }
    }
    // Check for unsupported claims
    static const char* unsupported[] = {"obviously", "clearly", "everyone knows", "it is well known"};
    for (const char* phrase : unsupported) {
        if (reasoning.find(phrase) != std::string::npos) {
            issues.push_back(std::string("Unsupported claim marker: '") + phrase + "'");
        }
    }
    // Check for incomplete reasoning (ends abruptly)
    if (reasoning.size() > 20 && reasoning.back() != '.' && reasoning.back() != '!' && reasoning.back() != '?') {
        issues.push_back("Reasoning chain may be truncated");
    }
    return issues;
}
std::string AgentHotPatcher::generateUniqueId() { return std::to_string(++m_idCounter); }
bool AgentHotPatcher::loadCorrectionPatterns() { return true; }
bool AgentHotPatcher::saveCorrectionPatterns() { return true; }
bool AgentHotPatcher::startInterceptorServer(int port) { return true; }
JsonValue AgentHotPatcher::processInterceptedResponse(const JsonValue& response) { return response; }

HallucinationDetection AgentHotPatcher::detectPathHallucination(const std::string& content) {
    HallucinationDetection h;
    h.detectionId = "path-" + generateUniqueId();
    h.hallucinationType = "invalid_path";
    h.detectedContent = content;
    h.confidence = 0.8;
    return h;
}

HallucinationDetection AgentHotPatcher::detectLogicContradiction(const std::string& content) {
    HallucinationDetection h;
    // Detect contradictory statements in model reasoning
    static const std::pair<const char*, const char*> contradictions[] = {
        {"is true", "is false"}, {"is correct", "is incorrect"},
        {"will work", "will not work"}, {"can be done", "cannot be done"},
        {"exists", "does not exist"}
    };
    for (const auto& [pos_phrase, neg_phrase] : contradictions) {
        if (content.find(pos_phrase) != std::string::npos &&
            content.find(neg_phrase) != std::string::npos) {
            h.detectionId = "logic-" + generateUniqueId();
            h.hallucinationType = "logic_contradiction";
            h.detectedContent = content;
            h.confidence = 0.7;
            h.correctionStrategy = "rewrite_reasoning";
            h.detectedAt = std::chrono::system_clock::now();
            return h;
        }
    }
    return h;
}

HallucinationDetection AgentHotPatcher::detectIncompleteReasoning(const std::string& content) {
    HallucinationDetection h;
    // Detect reasoning that ends abruptly or has unfinished thoughts
    if (content.size() < 10) return h;  // Too short to evaluate
    
    // Check for endings that suggest truncation
    if (content.back() == ',' || content.back() == ':' || content.back() == '-') {
        h.detectionId = "incomplete-" + generateUniqueId();
        h.hallucinationType = "incomplete_reasoning";
        h.detectedContent = content;
        h.confidence = 0.65;
        h.correctionStrategy = "request_completion";
        h.detectedAt = std::chrono::system_clock::now();
        return h;
    }
    // Check for "..." indicating trailing off
    if (content.size() > 3 && content.substr(content.size()-3) == "...") {
        h.detectionId = "incomplete-" + generateUniqueId();
        h.hallucinationType = "incomplete_reasoning";
        h.detectedContent = content;
        h.confidence = 0.6;
        h.correctionStrategy = "request_completion";
        h.detectedAt = std::chrono::system_clock::now();
    }
    return h;
}

std::string AgentHotPatcher::normalizePathInContent(const std::string& content, const std::string& validPath) {
    std::string result = content;
    // Replace known invalid paths with the valid one
    for (const auto& [wrongPath, correctPath] : m_navigationPatterns) {
        size_t pos = 0;
        while ((pos = result.find(wrongPath, pos)) != std::string::npos) {
            result.replace(pos, wrongPath.length(), correctPath);
            pos += correctPath.length();
        }
    }
    return result;
}

void AgentHotPatcher::registerHallucinationDetectedCallback(HallucinationDetectedCallback cb, void* userData) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_hallucinationDetectedCBs.push_back({cb, userData});
}
void AgentHotPatcher::registerHallucinationCorrectedCallback(HallucinationCorrectedCallback cb, void* userData) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_hallucinationCorrectedCBs.push_back({cb, userData});
}
void AgentHotPatcher::registerNavigationErrorFixedCallback(NavigationErrorFixedCallback cb, void* userData) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_navigationErrorFixedCBs.push_back({cb, userData});
}
void AgentHotPatcher::registerBehaviorPatchAppliedCallback(BehaviorPatchAppliedCallback cb, void* userData) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_behaviorPatchAppliedCBs.push_back({cb, userData});
}
void AgentHotPatcher::registerStatisticsUpdatedCallback(StatisticsUpdatedCallback cb, void* userData) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_statisticsUpdatedCBs.push_back({cb, userData});
}

void AgentHotPatcher::notifyHallucinationDetected(const HallucinationDetection& d) {
    for (const auto& cb : m_hallucinationDetectedCBs) {
        if (cb.fn) cb.fn(d, cb.userData);
    }
}
void AgentHotPatcher::notifyHallucinationCorrected(const HallucinationDetection& d, const std::string& c) {
    for (const auto& cb : m_hallucinationCorrectedCBs) {
        if (cb.fn) cb.fn(d, c, cb.userData);
    }
}
void AgentHotPatcher::notifyNavigationErrorFixed(const NavigationFix& f) {
    for (const auto& cb : m_navigationErrorFixedCBs) {
        if (cb.fn) cb.fn(f, cb.userData);
    }
}
void AgentHotPatcher::notifyBehaviorPatchApplied(const BehaviorPatch& p) {
    for (const auto& cb : m_behaviorPatchAppliedCBs) {
        if (cb.fn) cb.fn(p, cb.userData);
    }
}
void AgentHotPatcher::notifyStatisticsUpdated(const JsonValue& s) {
    for (const auto& cb : m_statisticsUpdatedCBs) {
        if (cb.fn) cb.fn(s, cb.userData);
    }
}
