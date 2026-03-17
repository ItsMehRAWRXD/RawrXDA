#include "agent_hot_patcher.hpp"
#include <iostream>
#include <regex>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

AgentHotPatcher::AgentHotPatcher()
    : m_enabled(false)
    , m_idCounter(0)
    , m_interceptionPort(0)
{
}

AgentHotPatcher::~AgentHotPatcher() = default;

bool AgentHotPatcher::initialize(const std::string& ggufLoaderPath, int interceptionPort)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    
    m_ggufLoaderPath = ggufLoaderPath;
    m_interceptionPort = interceptionPort;
    
    if (!fs::exists(ggufLoaderPath)) {
        return false;
    }
    
    m_enabled = true;
    return true;
}

std::string AgentHotPatcher::patchModelOutput(const std::string& data) {
    try {
        auto j = nlohmann::json::parse(data);
        auto result = interceptModelOutput(data, nlohmann::json::object());
        if (result.contains("modified") && result["modified"].is_object()) {
            return result["modified"].dump();
        }
    } catch (...) {
        // Not JSON, apply some string-based heuristics?
    }
    return data;
}

nlohmann::json AgentHotPatcher::interceptModelOutput(const std::string& modelOutput, const nlohmann::json& context)
{
    if (!m_enabled) {
        return {{"original", modelOutput}, {"modified", false}};
    }
    
    std::lock_guard<std::mutex> locker(m_mutex);
    
    nlohmann::json output;
    try {
        output = nlohmann::json::parse(modelOutput);
    } catch (...) {
        return {{"original", modelOutput}, {"modified", false}};
    }

    nlohmann::json originalOutput = output;
    
    if (output.contains("reasoning") || output.contains("thinking")) {
        std::string reasoning = output.contains("reasoning") ? 
                               output["reasoning"].get<std::string>() : 
                               output["thinking"].get<std::string>();
        
        HallucinationDetection hallucination = detectHallucination(reasoning, context);
        if (hallucination.confidence > 0.6) {
            if (m_onHallucinationDetected) m_onHallucinationDetected(hallucination);
            
            std::string corrected = correctHallucination(hallucination);
            if (!corrected.empty()) {
                if (output.contains("reasoning")) {
                    output["reasoning"] = corrected;
                } else {
                    output["thinking"] = corrected;
                }
                hallucination.correctionApplied = true;
                m_detectedHallucinations.push_back(hallucination);
            }
        }
    }
    
    if (output.contains("navigationPath")) {
        std::string navPath = output["navigationPath"].get<std::string>();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.empty()) {
            output["navigationPath"] = fix.correctPath;
            m_navigationFixes.push_back(fix);
        }
    }
    
    output = applyBehaviorPatches(output, context);
    
    return {
        {"original", modelOutput},
        {"modified", output},
        {"wasModified", (output != originalOutput)},
        {"hallucinationsDetected", m_detectedHallucinations.size()},
        {"navigationFixesApplied", m_navigationFixes.size()}
    };
}

HallucinationDetection AgentHotPatcher::detectHallucination(const std::string& content, const nlohmann::json& context)
{
    HallucinationDetection detection;
    detection.detectionId = generateUniqueId();
    detection.detectedAt = std::chrono::system_clock::now();
    detection.detectedContent = content;
    
    std::regex pathRegex(R"((?:file|path|dir|directory):\s*([^\s,\.]+))");
    auto pathBegin = std::sregex_iterator(content.begin(), content.end(), pathRegex);
    auto pathEnd = std::sregex_iterator();

    for (std::sregex_iterator i = pathBegin; i != pathEnd; ++i) {
        std::smatch match = *i;
        std::string path = match[1].str();
        if (path.find("//") != std::string::npos || path.find("...") != std::string::npos) {
            detection.hallucinationType = "invalid_path";
            detection.confidence = 0.8;
            detection.detectedContent = path;
            return detection;
        }
    }
    
    if (content.find("always succeeds") != std::string::npos && content.find("always fails") != std::string::npos) {
        detection.hallucinationType = "logic_contradiction";
        detection.confidence = 0.95;
        return detection;
    }
    
    return detection;
}

std::string AgentHotPatcher::correctHallucination(const HallucinationDetection& h)
{
    if (h.hallucinationType == "invalid_path") {
        std::string c = h.detectedContent;
        size_t pos = 0;
        while ((pos = c.find("//", pos)) != std::string::npos) c.replace(pos, 2, "/");
        return c;
    }
    return "";
}

NavigationFix AgentHotPatcher::fixNavigationError(const std::string& path, const nlohmann::json& context)
{
    NavigationFix fix;
    fix.fixId = generateUniqueId();
    if (path.find("//") != std::string::npos) {
        fix.incorrectPath = path;
        fix.correctPath = path;
        size_t pos = 0;
        while ((pos = fix.correctPath.find("//", pos)) != std::string::npos) fix.correctPath.replace(pos, 2, "/");
    }
    return fix;
}

nlohmann::json AgentHotPatcher::applyBehaviorPatches(const nlohmann::json& output, const nlohmann::json& context)
{
    return output;
}

bool AgentHotPatcher::loadCorrectionPatterns() { return true; }

std::string AgentHotPatcher::generateUniqueId()
{
    return std::to_string(m_idCounter++);
}