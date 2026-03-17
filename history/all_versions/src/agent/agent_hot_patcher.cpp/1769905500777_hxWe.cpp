// ============================================================================
// File: src/agent/agent_hot_patcher.cpp
// 
// Purpose: Core logic for Hot Patching AI outputs.
// Detects hallucinations, corrects paths, and applies behavioral patches.
// ============================================================================

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

// Helper: Recursively find a file in the workspace to fix bad paths
static std::string findFileRecursively(const std::string& filename, const fs::path& startDir = fs::current_path(), int maxDepth = 5) {
    try {
        if (!fs::exists(startDir)) return "";

        for (const auto& entry : fs::recursive_directory_iterator(startDir, fs::directory_options::skip_permission_denied)) {
            // Check depth manually if needed, or rely on iterator
             // fs::recursive_directory_iterator doesn't easily give depth, checking path components count helps
             // but strictly 'depth' param is harder. We'll trust limit or assume 'maxDepth' is heuristic.
             // Actually, let's just use a simple iteration.
            if (entry.is_regular_file() && entry.path().filename().string() == filename) {
                // Return relative path key
                try {
                    return fs::relative(entry.path(), startDir).string();
                } catch(...) {
                    return entry.path().string();
                }
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
             
             // Notify detection
             if (onHallucinationDetected) onHallucinationDetected(h);

             std::string corrected = correctHallucination(h);
             if (!corrected.empty() && corrected != reasoning) {
                 h.expectedContent = corrected;
                 h.correctionApplied = true;

                 // Apply fix
                 output[fieldName] = corrected;
                 modified = true;
                 m_hallucinationsCorrected++;

                 // Notify correction
                 if (onHallucinationCorrected) onHallucinationCorrected(h);
             }
        }
    }

    // 2. Navigation Check
    if (output.contains("navigationPath")) {
        std::string navPath = output["navigationPath"].get<std::string>();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty() && !fix.correctPath.empty()) {
            output["navigationPath"] = fix.correctPath;
            modified = true;
            m_navigationFixesApplied++;
            
            if (onNavigationErrorFixed) onNavigationErrorFixed(fix);
        }
    }
    
    // 3. Behavior Patches over the whole JSON
    // Note: Behavior patches usually apply to raw string, but here we work on JSON object.
    // We might skip this or apply it to specific fields. 
    // Ideally, we'd serialise, patch, and re-parse, but that's expensive.
    // Let's assume behavior patches are applied by caller on the 'final_output' string if valid.

    json result;
    result["original"] = modelOutputStr;
    result["modified"] = modified;
    result["final_output"] = output;
    return result;
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const json& context) {
    // 1. Check registered patterns (User defined)
    for (const auto& pattern : m_correctionPatterns) {
        if (pattern.hallucinationType == "regex") {
             try {
                 std::regex re(pattern.detectedContent);
                 if (std::regex_search(content, re)) {
                     HallucinationDetection h = pattern;
                     h.detectedAt = std::chrono::system_clock::now();
                     h.confidence = 0.95; // High confidence for regex match
                     h.detectedContent = content; // Or maybe just the match?
                     return h;
                 }
             } catch (...) { }
        } else {
             // Substring
             if (!pattern.detectedContent.empty() && content.find(pattern.detectedContent) != std::string::npos) {
                 HallucinationDetection h = pattern;
                 h.detectedAt = std::chrono::system_clock::now();
                 h.confidence = 0.95;
                 h.detectedContent = pattern.detectedContent; // The trigger
                 return h;
             }
        }
    }

    // 2. Automated Heuristics
    // Path Hallucination
    HallucinationDetection hPath = detectPathHallucination(content);
    if (hPath.confidence > 0.0) return hPath;

    // Logic Contradiction
    HallucinationDetection hLogic = detectLogicContradiction(content);
    if (hLogic.confidence > 0.0) return hLogic;

    // Incomplete Reasoning
    HallucinationDetection hReasoning = detectIncompleteReasoning(content);
    if (hReasoning.confidence > 0.0) return hReasoning;

    return HallucinationDetection();
}

HallucinationDetection AgentHotPatcher::detectPathHallucination(const std::string& content) {
    HallucinationDetection h;
    
    // Regex to find potential file paths with common extensions
    std::regex pathRegex(R"((?:^|[\s"'])((?:[a-zA-Z]:[\\/]|/|\./|src/|[\w_\-\.]+/)?[\w_\-\.]+\.(?:cpp|hpp|h|c|py|js|ts|txt|md|json|xml|html|css|asm|ps1|bat|sh))(?:$|[\s"']))");
    
    std::string correctedContent = content;
    bool foundHallucination = false;
    
    auto words_begin = std::sregex_iterator(content.begin(), content.end(), pathRegex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        std::smatch match = *i;
        std::string potentialPath = match[1].str(); 
        
        if (isValidPath(potentialPath)) continue;

        // Try to fix it to confirm it's a hallucination (i.e., we know real path)
        NavigationFix fix = fixNavigationError(potentialPath, json());
        if (!fix.correctPath.empty() && fix.correctPath != potentialPath) {
            
            // It was a hallucinaton because we found a fix
            size_t pos = correctedContent.find(potentialPath);
            while (pos != std::string::npos) {
                correctedContent.replace(pos, potentialPath.length(), fix.correctPath);
                pos = correctedContent.find(potentialPath, pos + fix.correctPath.length());
            }
            foundHallucination = true;
        }
    }

    if (foundHallucination) {
        h.detectionId = generateUUID();
        h.hallucinationType = "invalid_path";
        h.detectedContent = content;
        h.expectedContent = correctedContent;
        h.correctionStrategy = "path_correction";
        h.confidence = 0.95;
    }
    
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

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& detection) {
    if (!detection.expectedContent.empty()) return detection.expectedContent;
    return detection.detectedContent; 
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const json& context) {
    NavigationFix fix;
    if (isValidPath(path)) return fix; // No error

    fix.fixId = "fix-" + generateUUID();
    fix.incorrectPath = path;
    
    // Approach 1: Check if it's missing "src/" or has extra leading slash
    std::string testPath = "src/" + path;
    if (fs::exists(testPath)) {
        fix.correctPath = testPath;
        fix.reasoning = "Missing src/ prefix";
        fix.effectiveness = 1.0;
        return fix;
    }

    if (path.starts_with("/")) {
        testPath = path.substr(1);
        if (fs::exists(testPath)) {
            fix.correctPath = testPath;
            fix.reasoning = "Removed leading slash";
            fix.effectiveness = 1.0;
            return fix;
        }
    }
    
    // Approach 2: Deep search for filename
    fs::path p(path);
    std::string filename = p.filename().string();
    std::string found = findFileRecursively(filename);
    
    if (!found.empty()) {
        fix.correctPath = found;
        fix.reasoning = "Found file in workspace via recursive search";
        fix.effectiveness = 0.9;
        return fix;
    }

    // Fallback
    fix.correctPath = ""; 
    return fix;
}

std::string AgentHotPatcher::applyBehaviorPatches(const std::string& output) {
    std::string result = output;
    
    for (const auto& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
        
        // Ensure patch applies to this model? (skipped for now)
        
        if (patch.patchType == "replace") {
             size_t pos = 0; 
             if (!patch.condition.empty()) {
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
