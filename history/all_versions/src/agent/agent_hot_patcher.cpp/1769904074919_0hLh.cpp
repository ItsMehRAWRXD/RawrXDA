#include "agent_hot_patcher.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

AgentHotPatcher::AgentHotPatcher() {
}

AgentHotPatcher::~AgentHotPatcher() = default;

void AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int flags) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_ggufLoaderPath = ggufLoaderPath;
    
    if (!fs::exists(ggufLoaderPath)) {
        
        return;
    }
    
    m_hotPatchingEnabled = true; // Default to true after init?
    
}

json AgentHotPatcher::interceptModelOutput(const std::string& modelOutputStr, const json& context) {
    if (!m_hotPatchingEnabled) {
        json result;
        result["original"] = modelOutputStr;
        result["modified"] = false;
        return result;
    }

    std::lock_guard<std::mutex> locker(m_mutex);

    json output;
    try {
        output = json::parse(modelOutputStr);
    } catch (...) {
        // Not JSON?
        json result; 
        result["original"] = modelOutputStr;
        result["modified"] = false;
        return result;
    }

    bool modified = false;

    // Hallucination Check
    if (output.contains("reasoning") || output.contains("thinking")) {
        std::string reasoning = output.contains("reasoning") ? 
                            output["reasoning"].get<std::string>() : 
                            output["thinking"].get<std::string>();

        HallucinationDetection h = detectHallucination(reasoning, context);
        if (h.confidence > 0.6) {
             m_totalHallucinationsDetected++;
             if (onHallucinationDetected) {
                 onHallucinationDetected(h);
             }
             std::string corrected = correctHallucination(h);
             if (!corrected.empty()) {
                 h.expectedContent = corrected;
                 h.correctionApplied = true;

                 if (output.contains("reasoning")) output["reasoning"] = corrected;
                 else output["thinking"] = corrected;
                 
                 modified = true;
                 m_hallucinationsCorrected++;
                 if (onHallucinationCorrected) {
                     onHallucinationCorrected(h);
                 }
             }
        }
    }

    // Navigation Check
    if (output.contains("navigationPath")) {
        std::string navPath = output["navigationPath"].get<std::string>();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty()) {
            output["navigationPath"] = fix.correctPath;
            modified = true;
            m_navigationFixesApplied++;
            if (onNavigationFixApplied) onNavigationFixApplied(fix);
        }
    }

    json result;
    result["original"] = modelOutputStr; // Keep original string? Or maybe we just return the new object.
    result["modified"] = modified;
    result["final_output"] = output;
    return result; // The modifying logic depends on how the caller expects return.
    // The original code returned a void* with 'original' and 'modified'. 
    // Wait, the original code had: returns `void* result` which was a `void*`.
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const json& context) {
    std::lock_guard<std::mutex> locker(m_mutex);

    // 1. Check registered patterns
    for (const auto& pattern : m_correctionPatterns) {
        if (pattern.hallucinationType == "regex") {
             try {
                 std::regex re(pattern.detectedContent);
                 if (std::regex_search(content, re)) {
                     HallucinationDetection h = pattern;
                     h.detectedAt = std::chrono::system_clock::now();
                     h.confidence = 0.95;
                     h.originalText = content; 
                     return h;
                 }
             } catch (...) { }
        } else {
             // Substring
             if (!pattern.detectedContent.empty() && content.find(pattern.detectedContent) != std::string::npos) {
                 HallucinationDetection h = pattern;
                 h.detectedAt = std::chrono::system_clock::now();
                 h.confidence = 0.95;
                 h.originalText = content;
                 return h;
             }
        }
    }

    // 2. Fallbacks
    if (content.find("/nonexistent/") != std::string::npos) {
        auto h = detectPathHallucination(content);
        h.originalText = content;
        return h;
    }

    return HallucinationDetection();
}

HallucinationDetection AgentHotPatcher::detectPathHallucination(const std::string& content) {
    HallucinationDetection h;
    h.detectionId = "auto-id-path"; 
    h.hallucinationType = "invalid_path";
    h.detectedContent = "/nonexistent/"; 
    h.confidence = 0.8; 
    return h;
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {
    if (detection.originalText.empty()) return detection.detectedContent;
    
    std::string text = detection.originalText;
    
    if (detection.hallucinationType == "regex") {
        try {
            std::regex re(detection.detectedContent);
            return std::regex_replace(text, re, detection.expectedContent);
        } catch(...) { return text; }
    } else {
        // Simple Replace Loop
        if (detection.correctionStrategy == "replace_all") {
             return detection.expectedContent; 
        }
        
        size_t pos = 0;
        std::string res = text;
        const std::string& from = detection.detectedContent;
        const std::string& to = detection.expectedContent;
        
        if (from.empty()) return res;

        while ((pos = res.find(from, pos)) != std::string::npos) {
             res.replace(pos, from.length(), to);
             pos += to.length();
        }
        return res;
    }
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const json& context) {
    NavigationFix fix;
    if (!isValidPath(path)) {
        fix.incorrectPath = path;
        
        std::lock_guard<std::mutex> locker(m_mutex);
        for(const auto& pattern : m_navigationPatterns) {
            if (pattern.incorrectPath == path) {
                return pattern;
            }
        }
    }
    return fix;
}

std::string AgentHotPatcher::applyBehaviorPatches(const std::string& output) {
    std::string result = output;
    std::lock_guard<std::mutex> locker(m_mutex);
    
    for (const auto& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
        
        if (patch.patchType == "replace") {
             if (result.find(patch.condition) != std::string::npos) {
                 size_t pos = 0; 
                 while((pos = result.find(patch.condition, pos)) != std::string::npos) {
                      result.replace(pos, patch.condition.length(), patch.action);
                      pos += patch.action.length();
                 }
             }
        } else if (patch.patchType == "regex_replace") {
             try {
                std::regex re(patch.condition);
                result = std::regex_replace(result, re, patch.action);
             } catch (...) {}
        }
    }
    return result;
}

void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& pattern) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_correctionPatterns.push_back(pattern);
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& fix) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_navigationPatterns.push_back(fix);
}

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& patch) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_behaviorPatches.push_back(patch);
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled) {
    m_hotPatchingEnabled = enabled;
}

bool AgentHotPatcher::isHotPatchingEnabled() const {
    return m_hotPatchingEnabled;
}

void AgentHotPatcher::setDebugLogging(bool enabled) {
    m_debugLogging = enabled;
}

json AgentHotPatcher::getCorrectionStatistics() const {
    json stats;
    stats["total_hallucinations"] = m_totalHallucinationsDetected.load();
    stats["total_corrected"] = m_hallucinationsCorrected.load();
    stats["total_nav_fixes"] = m_navigationFixesApplied.load();
    return stats;
}

int AgentHotPatcher::getCorrectionPatternCount() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    return (int)m_correctionPatterns.size();
}

HallucinationDetection AgentHotPatcher::detectLogicContradiction(const std::string& content) {
    return HallucinationDetection();
}

std::string AgentHotPatcher::normalizePathInContent(const std::string& content, const std::string& validPath) {
    // Basic normalization
    std::string s = content;
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}

bool AgentHotPatcher::isValidPath(const std::string& path) const {
    try {
        return fs::exists(path);
    } catch(...) {
        return false;
    }
}
