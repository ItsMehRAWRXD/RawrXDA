#include "agent_hot_patcher.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

// Forward declare or define helper to generate UUID
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
    h.detectionId = generateUUID();
    h.hallucinationType = "invalid_path";
    h.detectedContent = "/nonexistent/"; 
    h.confidence = 0.8; 
    return h;
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
                result = std::regex_replace(result, re, patch.action);nt;
             } catch (...) {}edContent;
        }
    }rom.empty()) return res;
    return result;
}   while ((pos = res.find(from, pos)) != std::string::npos) {
s.replace(pos, from.length(), to);
void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& pattern) {            pos += to.length();
    std::lock_guard<std::mutex> locker(m_mutex);        }
    m_correctionPatterns.push_back(pattern);
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& fix) {
    std::lock_guard<std::mutex> locker(m_mutex);(const std::string& path, const json& context) {
    m_navigationPatterns.push_back(fix);
}!isValidPath(path)) {

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& patch) {
    std::lock_guard<std::mutex> locker(m_mutex);x> locker(m_mutex);
    m_behaviorPatches.push_back(patch);
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled) {
    m_hotPatchingEnabled = enabled;
}

bool AgentHotPatcher::isHotPatchingEnabled() const {
    return m_hotPatchingEnabled;
}applyBehaviorPatches(const std::string& output) {
string result = output;
void AgentHotPatcher::setDebugLogging(bool enabled) {td::lock_guard<std::mutex> locker(m_mutex);
    m_debugLogging = enabled;
}   for (const auto& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
json AgentHotPatcher::getCorrectionStatistics() const {
    json stats;
    stats["total_hallucinations"] = m_totalHallucinationsDetected.load();) != std::string::npos) {
    stats["total_corrected"] = m_hallucinationsCorrected.load();                size_t pos = 0; 
    stats["total_nav_fixes"] = m_navigationFixesApplied.load();                 while((pos = result.find(patch.condition, pos)) != std::string::npos) {
    return stats;ch.action);
}();

int AgentHotPatcher::getCorrectionPatternCount() const {            }
    std::lock_guard<std::mutex> locker(m_mutex);        } else if (patch.patchType == "regex_replace") {
    return (int)m_correctionPatterns.size();
}
lace(result, re, patch.action);
HallucinationDetection AgentHotPatcher::detectLogicContradiction(const std::string& content) {            } catch (...) {}
    HallucinationDetection h;        }
    // Heuristic: Check for "A is B" and "A is not B" in close proximity
    // Or simpler: "contradiction" or "wait, no"
    std::regex contradictionRegex("(however|wait|actually),? (no|false|incorrect)", std::regex_constants::icase);
    if (std::regex_search(content, contradictionRegex)) {
        h.detectionId = "logic-" + std::to_string(std::rand()); // Placeholder IDt HallucinationDetection& pattern) {
        h.hallucinationType = "logic_contradiction";locker(m_mutex);
        h.confidence = 0.75;   m_correctionPatterns.push_back(pattern);
        h.detectedContent = "Self-correction detected in stream"; }
        h.expectedContent = "Streamlined logic without hesitations"; 
    }rNavigationFix(const NavigationFix& fix) {
    return h;   std::lock_guard<std::mutex> locker(m_mutex);
}    m_navigationPatterns.push_back(fix);

HallucinationDetection AgentHotPatcher::detectIncompleteReasoning(const std::string& content) {
    HallucinationDetection h;
    // Heuristic: Ends abruptly or "..."
        h.detectionId = "reasoning-" + std::to_string(std::rand());ength() > 50 && (content.ends_with("...") || content.ends_with("then "))) {
        h.hallucinationType = "incomplete_reasoning";
        h.confidence = 0.9;
        h.detectedContent = "Output cut off";) {
        h.expectedContent = "...continued reasoning..."; 
    }
    return h;
}bool AgentHotPatcher::isHotPatchingEnabled() const {

std::string AgentHotPatcher::normalizePathInContent(const std::string& content, const std::string& validPath) {
    // Replace the first detected path-like string with validPath
    std::regex pathRegex(R"([a-zA-Z]:\\[\w\\]+|\/[\w\/]+)");
    return std::regex_replace(content, pathRegex, validPath, std::regex_constants::format_first_only);
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {istics() const {
    if (detection.hallucinationType == "invalid_path") {
         // Try to find a similar file in the workspace?ected.load();
         // This is a naive fix: just return empty so it forces regeneration or user intervention] = m_hallucinationsCorrected.load();
         // Better: Search workspace for similar nametats["total_nav_fixes"] = m_navigationFixesApplied.load();
         // For now, let's assume we have a mapping or we just return the detection's expectedContent if generatedats;
         if (!detection.expectedContent.empty()) return detection.expectedContent;
    }
    return detection.detectedContent; 
}x> locker(m_mutex);

std::string AgentHotPatcher::normalizePathInContent(const std::string& content, const std::string& validPath) {
    // Basic normalization
    std::string s = content;ntradiction(const std::string& content) {
    std::replace(s.begin(), s.end(), '\\', '/');
    return s; obvious contradictions
}f (content.find("exclude include") != std::string::npos || 
nt.find("never always") != std::string::npos ||
bool AgentHotPatcher::isValidPath(const std::string& path) const {       content.find("true is false") != std::string::npos) {
    try {        
        return fs::exists(path);nerateUUID();
    } catch(...) {c_contradiction";
        return false;radictory statement found";
    }
}
