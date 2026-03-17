#include "agent_hot_patcher.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <random>
#include <sstream>
#include <algorithm>

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

// Helper for recursive file search
static std::string findFileRecursively(const std::string& filename, const fs::path& root = fs::current_path()) {
    try {
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_regular_file() && entry.path().filename() == filename) {
                return entry.absolute_path().string();
            }
        }
    } catch (...) {}
    return "";
}

AgentHotPatcher::AgentHotPatcher() {
}

AgentHotPatcher::~AgentHotPatcher() = default;

void AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int flags) {
    std::lock_guard<std::mutex> locker(m_mutex);
    m_ggufLoaderPath = ggufLoaderPath;
    
    if (!fs::exists(ggufLoaderPath)) {
        // Log warning?
    }
    
    m_hotPatchingEnabled = true;
}

json AgentHotPatcher::interceptModelOutput(const std::string& modelOutputStr, const json& context) {
    if (!m_hotPatchingEnabled) {
        json result;
        result["original"] = modelOutputStr;
        result["modified"] = false;
        return result;
    }

    // std::lock_guard<std::mutex> locker(m_mutex); // Lock granularity optimization

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

    // Hallucination Check
    if (output.contains("reasoning") || output.contains("thinking")) {
        std::string reasoning = output.contains("reasoning") ? 
                            output["reasoning"].get<std::string>() : 
                            output["thinking"].get<std::string>();

        HallucinationDetection h = detectHallucination(reasoning, context);
        if (h.confidence > 0.6) {
             m_totalHallucinationsDetected++;
             if (onHallucinationDetected) onHallucinationDetected(h);

             std::string corrected = correctHallucination(h);
             if (!corrected.empty()) {
                 if (output.contains("reasoning")) output["reasoning"] = corrected;
                 else output["thinking"] = corrected;
                 
                 modified = true;
                 m_hallucinationsCorrected++;
                 if (onHallucinationCorrected) {
                     HallucinationDetection hCorrected = h;
                     hCorrected.expectedContent = corrected;
                     hCorrected.correctionApplied = true;
                     onHallucinationCorrected(hCorrected);
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
    result["original"] = modelOutputStr;
    result["modified"] = modified;
    result["final_output"] = output;
    return result;
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
                     h.detectedContent = content; // Store context
                     return h;
                 }
             } catch (...) { }
        } else {
             // Substring
             if (!pattern.detectedContent.empty() && content.find(pattern.detectedContent) != std::string::npos) {
                 HallucinationDetection h = pattern;
                 h.detectedAt = std::chrono::system_clock::now();
                 h.confidence = 0.95;
                 h.detectedContent = content;
                 return h;
             }
        }
    }

    // 2. Fallbacks
    if (content.find("/nonexistent/") != std::string::npos) {
        return detectPathHallucination(content);
    }

    HallucinationDetection hLogic = detectLogicContradiction(content);
    if (hLogic.confidence > 0.0) return hLogic;

    HallucinationDetection hReasoning = detectIncompleteReasoning(content);
    if (hReasoning.confidence > 0.0) return hReasoning;

    return HallucinationDetection();
}

HallucinationDetection AgentHotPatcher::detectPathHallucination(const std::string& content) {
    HallucinationDetection h;
    h.detectionId = generateUUID();
    h.hallucinationType = "invalid_path";
    h.detectedContent = "/nonexistent/"; 
    h.confidence = 0.8; 
    return h;
}

HallucinationDetection AgentHotPatcher::detectLogicContradiction(const std::string& content) {
    HallucinationDetection h;
    if (content.find("exclude include") != std::string::npos || 
        content.find("never always") != std::string::npos ||
        content.find("true is false") != std::string::npos) {
        
        h.detectionId = generateUUID();
        h.hallucinationType = "logic_contradiction";
        h.detectedContent = "Contradictory statement found";
        h.confidence = 0.7;
    }
    return h;
}

HallucinationDetection AgentHotPatcher::detectIncompleteReasoning(const std::string& content) {
    HallucinationDetection h;
    if (content.length() > 20 && (content.ends_with("...") || content.ends_with("Step "))) {
        h.detectionId = generateUUID();
        h.hallucinationType = "incomplete_reasoning";
        h.detectedContent = "Output appears truncated";
        h.confidence = 0.6;
    }
    return h;
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {
    if (detection.detectedContent.empty()) return "";
    return detection.expectedContent;
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const json& context) {
    NavigationFix fix;
    if (isValidPath(path)) return fix;

    fix.fixId = generateUUID();
    fix.incorrectPath = path;
    
    // Check registered patterns first
    {
        std::lock_guard<std::mutex> locker(m_mutex);
        for(const auto& pattern : m_navigationPatterns) {
            if (pattern.incorrectPath == path) {
                return pattern;
            }
        }
    }
    
    // Heuristic 1: Check if missing src/ prefix
    std::string testPath = "src/" + path;
    if (fs::exists(testPath)) {
        fix.correctPath = testPath;
        fix.reasoning = "Missing src/ prefix";
        fix.effectiveness = 1.0;
        return fix;
    }

    // Heuristic 2: Check if leading slash removal helps (absolute to relative)
    if (path.starts_with("/")) {
        testPath = path.substr(1);
        if (fs::exists(testPath)) {
            fix.correctPath = testPath;
            fix.reasoning = "Removed leading slash"; 
            fix.effectiveness = 1.0;
            return fix;
        }
    }
    
    // Heuristic 3: Deep search
    fs::path p(path);
    std::string found = findFileRecursively(p.filename().string());
    if (!found.empty()) {
        fix.correctPath = found;
        fix.reasoning = "Found file via recursive search";
        fix.effectiveness = 0.9;
        return fix;
    }

    // Fail
    fix.fixId = "";
    return fix;
}

std::string AgentHotPatcher::applyBehaviorPatches(const std::string& output) {
    std::lock_guard<std::mutex> locker(m_mutex);
    std::string result = output;
    
    for (const auto& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
        
        bool applied = false;
        
        if (patch.patchType == "replace") {
             size_t pos = 0; 
             while((pos = result.find(patch.condition, pos)) != std::string::npos) {
                  result.replace(pos, patch.condition.length(), patch.action);
                  pos += patch.action.length();
                  applied = true;
             }
        } else if (patch.patchType == "regex_replace") {
             try {
                std::regex re(patch.condition);
                std::string newResult = std::regex_replace(result, re, patch.action);
                if (newResult != result) {
                    result = newResult;
                    applied = true;
                }
             } catch (...) {}
        }
        
        if (applied && onBehaviorPatchApplied) {
            onBehaviorPatchApplied(patch);
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
