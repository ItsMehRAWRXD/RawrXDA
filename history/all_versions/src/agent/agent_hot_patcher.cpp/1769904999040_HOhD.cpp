#include "agent_hot_patcher.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <random>
#include <sstream>
#include <mutex>
#include <chrono>

namespace fs = std::filesystem;

// Helper to generate UUID
static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    std::stringstream ss;
    for (int i = 0; i < 8; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << std::hex << dis(gen);
    ss << "-4"; // UUID version 4
    for (int i = 0; i < 3; i++) ss << std::hex << dis(gen);
    ss << "-";
    ss << "a"; // Variant 10xxxxxx
    for (int i = 0; i < 3; i++) ss << std::hex << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << std::hex << dis(gen);
    return ss.str();
}

// Helper: Recursively find a file in the workspace
static std::string findFileRecursively(const std::string& filename, const fs::path& startDir = fs::current_path(), int maxDepth = 5) {
    try {
        if (!fs::exists(startDir)) return "";

        for (const auto& entry : fs::recursive_directory_iterator(startDir, fs::directory_options::skip_permission_denied)) {
            if (entry.depth() > maxDepth) continue;
            if (entry.is_regular_file() && entry.path().filename().string() == filename) {
                // Return relative path from startDir if possible, else absolute
                return fs::relative(entry.path(), startDir).string();
            }
        }
    } catch (...) {
        // Ignore permission errors etc.
    }
    return "";
}

AgentHotPatcher::AgentHotPatcher() {
}

AgentHotPatcher::~AgentHotPatcher() = default;

void AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int flags) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_ggufLoaderPath = ggufLoaderPath;
    m_hotPatchingEnabled = true; 
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
        json result; 
        result["original"] = modelOutputStr;
        result["modified"] = false;
        return result;
    }

    bool modified = false;

    // 1. Hallucination Check
    if (output.contains("reasoning") || output.contains("thinking")) {
        std::string fieldName = output.contains("reasoning") ? "reasoning" : "thinking";
        std::string reasoning = output[fieldName].get<std::string>();

        HallucinationDetection h = detectHallucination(reasoning, context);
        if (h.confidence > 0.6) {
             m_totalHallucinationsDetected++;
             
             if (onHallucinationDetected) onHallucinationDetected(h);

             std::string corrected = correctHallucination(h);
             if (!corrected.empty() && corrected != reasoning) {
                 h.expectedContent = corrected;
                 h.correctionApplied = true;

                 output[fieldName] = corrected;
                 
                 modified = true;
                 m_hallucinationsCorrected++;
                 if (onHallucinationCorrected) onHallucinationCorrected(h);
             }
        }
    }

    // 2. Navigation Check
    if (output.contains("navigationPath")) {
        std::string navPath = output["navigationPath"].get<std::string>();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty() && !fix.correctPath.empty() && fix.correctPath != navPath) {
            output["navigationPath"] = fix.correctPath;
            modified = true;
            m_navigationFixesApplied++;
            if (onNavigationErrorFixed) onNavigationErrorFixed(fix);
        }
    }

    json result;
    result["final_output"] = output;
    result["modified"] = modified;
    if (modified) {
        result["original"] = output.dump();
    } else {
        result["original"] = modelOutputStr;
    }
    
    return result; 
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const json& context) {
    for (const auto& pattern : m_correctionPatterns) {
        if (pattern.hallucinationType == "regex") {
             try {
                 std::regex re(pattern.detectedContent);
                 if (std::regex_search(content, re)) {
                     HallucinationDetection h = pattern;
                     h.detectedAt = std::chrono::system_clock::now();
                     h.confidence = 0.95;
                     h.detectedContent = content;
                     return h;
                 }
             } catch (...) { }
        } else {
             if (!pattern.detectedContent.empty() && content.find(pattern.detectedContent) != std::string::npos) {
                 HallucinationDetection h = pattern;
                 h.detectedAt = std::chrono::system_clock::now();
                 h.confidence = 0.95;
                 h.detectedContent = pattern.detectedContent;
                 return h;
             }
        }
    }

    if (content.find("/nonexistent/") != std::string::npos || content.find("C:\\invalid\\") != std::string::npos) {
        return detectPathHallucination(content);
    }
    
    auto logicH = detectLogicContradiction(content);
    if (logicH.confidence > 0.0) return logicH;

    auto incH = detectIncompleteReasoning(content);
    if (incH.confidence > 0.0) return incH;

    return HallucinationDetection();
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {
    if (detection.hallucinationType == "invalid_path") {
         if (!detection.expectedContent.empty()) return detection.expectedContent;
         return detection.detectedContent; 
    }
    if (detection.hallucinationType == "logic_contradiction") {
        return detection.expectedContent;
    }
    if (detection.hallucinationType == "incomplete_reasoning") {
        return detection.detectedContent + " [Reasoning continued by HotPatcher]";
    }
    return detection.expectedContent;
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const json& context) {
    std::lock_guard<std::mutex> locker(m_mutex);

    NavigationFix fix;
    fix.fixId = generateUUID();
    fix.incorrectPath = path;

    if (isValidPath(path)) return fix;

    for (const auto& existingFix : m_navigationPatterns) {
        if (existingFix.incorrectPath == path) {
            fix = existingFix;
            return fix;
        }
    }
    return fix;
}

std::string AgentHotPatcher::applyBehaviorPatches(const std::string& output) {
    std::lock_guard<std::mutex> locker(m_mutex);
    std::string result = output;
    
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

HallucinationDetection AgentHotPatcher::detectPathHallucination(const std::string& content) {
    HallucinationDetection h;
    h.detectionId = generateUUID();
    h.hallucinationType = "invalid_path";
    h.detectedContent = content; 
    h.confidence = 0.8; 
    return h;
}

HallucinationDetection AgentHotPatcher::detectLogicContradiction(const std::string& content) {
    HallucinationDetection h;
    std::regex contradictionRegex("(however|wait|actually),? (no|false|incorrect)", std::regex_constants::icase);
    if (std::regex_search(content, contradictionRegex)) {
        h.detectionId = "logic-" + generateUUID();
        h.hallucinationType = "logic_contradiction";
        h.confidence = 0.75;
        h.detectedContent = "Self-correction detected in stream"; 
        h.expectedContent = "Streamlined logic without hesitations"; 
    }
    return h;
}

HallucinationDetection AgentHotPatcher::detectIncompleteReasoning(const std::string& content) {
    HallucinationDetection h;
    if (content.length() > 50 && (content.ends_with("...") || content.ends_with("then "))) {
        h.detectionId = "reasoning-" + generateUUID();
        h.hallucinationType = "incomplete_reasoning";
        h.confidence = 0.9;
        h.detectedContent = "Output cut off";
        h.expectedContent = "...continued reasoning..."; 
    }
    return h;
}

std::string AgentHotPatcher::normalizePathInContent(const std::string& content, const std::string& validPath) {
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
