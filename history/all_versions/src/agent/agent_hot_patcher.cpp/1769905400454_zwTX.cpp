#include "agent_hot_patcher.hpp"
#include <iostream>
#include <filesystem>
#include <regex>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

AgentHotPatcher::AgentHotPatcher() {
    m_hotPatchingEnabled = true;
}

AgentHotPatcher::~AgentHotPatcher() = default;

void AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int flags) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_ggufLoaderPath = ggufLoaderPath;
    (void)flags;
}

json AgentHotPatcher::interceptModelOutput(const std::string& modelOutputStr, const json& context) {
    json result;
    result["original"] = modelOutputStr;
    result["modified"] = false;
    
    if (!m_hotPatchingEnabled) {
        return result;
    }

    json output;
    bool isJson = false;
    try {
        output = json::parse(modelOutputStr);
        isJson = true;
    } catch (...) {}

    bool modified = false;
    std::string currentContent = modelOutputStr;
    std::string patchedContent = applyBehaviorPatches(currentContent);
    
    if (patchedContent != currentContent) {
        modified = true;
        currentContent = patchedContent;
        if (isJson) {
            try { output = json::parse(currentContent); } catch(...) { isJson = false; }
        }
    }

    std::lock_guard<std::mutex> locker(m_mutex);

    if (isJson) {
        std::string reasoning;
        if (output.contains("reasoning")) reasoning = output["reasoning"].get<std::string>();
        else if (output.contains("thinking")) reasoning = output["thinking"].get<std::string>();

        if (!reasoning.empty()) {
            HallucinationDetection h = detectHallucination(reasoning, context);
            if (h.confidence > 0.6) {
                m_totalHallucinationsDetected++;
                if (onHallucinationDetected) onHallucinationDetected(h);
                std::string corrected = correctHallucination(h);
                if (!corrected.empty() && corrected != reasoning) {
                    h.expectedContent = corrected;
                     if (output.contains("reasoning")) output["reasoning"] = corrected;
                     else output["thinking"] = corrected;
                     modified = true;
                     m_hallucinationsCorrected++;
                     if (onHallucinationCorrected) onHallucinationCorrected(h);
                }
            }
        }
        
        if (output.contains("target")) {
            std::string path = output["target"].get<std::string>();
            NavigationFix fix = fixNavigationError(path, context);
            if (!fix.correctPath.empty() && fix.correctPath != path) {
                output["target"] = fix.correctPath;
                modified = true;
                m_navigationFixesApplied++;
                if (onNavigationErrorFixed) onNavigationErrorFixed(fix);
            }
        }
    }

    if (modified) {
        result["modified"] = true;
        if (isJson) {
            result["final_output"] = output;
        } else {
            result["final_output"] = currentContent;
        }
    }

    return result;
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const json& context) {
    for (const auto& pattern : m_correctionPatterns) {
        if (pattern.hallucinationType == "regex") {
            try {
                if (std::regex_search(content, std::regex(pattern.detectedContent))) {
                     HallucinationDetection h = pattern;
                     h.detectedAt = std::chrono::system_clock::now();
                     h.confidence = 0.95;
                     return h;
                }
            } catch(...) {}
        } else {
            if (content.find(pattern.detectedContent) != std::string::npos) {
                 HallucinationDetection h = pattern;
                 h.detectedAt = std::chrono::system_clock::now();
                 h.confidence = 0.95;
                 return h;
            }
        }
    }
    return HallucinationDetection();
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {
    return detection.expectedContent;
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const json& context) {
    NavigationFix fix;
    fix.incorrectPath = path;
    if (isValidPath(path)) return fix;

    for (const auto& pattern : m_navigationPatterns) {
        if (pattern.incorrectPath == path) return pattern;
    }
    return fix;
}

std::string AgentHotPatcher::applyBehaviorPatches(const std::string& output) {
    std::string result = output;
    std::lock_guard<std::mutex> locker(m_mutex);
    for (const auto& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
        try {
            if (patch.patchType == "replace") {
                size_t pos = 0;
                while((pos = result.find(patch.condition, pos)) != std::string::npos) {
                    result.replace(pos, patch.condition.length(), patch.action);
                    pos += patch.action.length();
                }
            } else if (patch.patchType == "regex_replace") {
                result = std::regex_replace(result, std::regex(patch.condition), patch.action);
            }
        } catch(...) {}
    }
    return result;
}

void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& p) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_correctionPatterns.push_back(p);
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& f) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_navigationPatterns.push_back(f);
}

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& p) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_behaviorPatches.push_back(p);
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled) { m_hotPatchingEnabled = enabled; }
bool AgentHotPatcher::isHotPatchingEnabled() const { return m_hotPatchingEnabled; }
void AgentHotPatcher::setDebugLogging(bool enabled) { m_debugLogging = enabled; }

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

HallucinationDetection AgentHotPatcher::detectLogicContradiction(const std::string& content) { return HallucinationDetection(); }
std::string AgentHotPatcher::normalizePathInContent(const std::string& content, const std::string& validPath) {
    std::string s = content;
    std::replace(s.begin(), s.end(), '\\', '/');
    return s;
}
bool AgentHotPatcher::isValidPath(const std::string& path) const {
    try { return fs::exists(path); } catch(...) { return false; }
}
