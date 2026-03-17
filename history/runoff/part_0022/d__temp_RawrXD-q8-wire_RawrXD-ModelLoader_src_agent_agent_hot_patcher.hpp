// ============================================================================
// File: src/agent/agent_hot_patcher.hpp
// 
// Purpose: Real-time hallucination and navigation correction interface
// This system intercepts model outputs and corrects them in real-time
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>

/**
 * @struct HallucinationDetection
 */
struct HallucinationDetection {
    std::string detectionId;
    std::string hallucinationType;
    double confidence = 0.0;
    std::string detectedContent;
    std::string expectedContent;
    std::string correctionStrategy;
    std::chrono::system_clock::time_point detectedAt;
    bool correctionApplied = false;
};

/**
 * @struct NavigationFix
 */
struct NavigationFix {
    std::string fixId;
    std::string incorrectPath;
    std::string correctPath;
    std::string reasoning;
    double effectiveness = 0.0;
};

/**
 * @struct BehaviorPatch
 */
struct BehaviorPatch {
    std::string patchId;
    std::string patchType;
    std::string condition;
    std::string action;
    std::vector<std::string> affectedModels;
    double successRate = 0.0;
    bool enabled = true;
    std::chrono::system_clock::time_point createdAt;
};

/**
 * @class AgentHotPatcher
 */
class AgentHotPatcher {
public:
    explicit AgentHotPatcher();
    ~AgentHotPatcher();

    bool initialize(const std::string& ggufLoaderPath, int interceptionPort);
    std::string patchModelOutput(const std::string& data);
    nlohmann::json interceptModelOutput(const std::string& modelOutput, const nlohmann::json& context);

    using HallucinationDetectedCallback = std::function<void(const HallucinationDetection&)>
    void setHallucinationDetectedCallback(HallucinationDetectedCallback cb) { m_onHallucinationDetected = cb; }

private:
    HallucinationDetection detectHallucination(const std::string& content, const nlohmann::json& context);
    std::string correctHallucination(const HallucinationDetection& h);
    NavigationFix fixNavigationError(const std::string& path, const nlohmann::json& context);
    nlohmann::json applyBehaviorPatches(const nlohmann::json& output, const nlohmann::json& context);
    
    bool loadCorrectionPatterns();
    std::string generateUniqueId();

    std::mutex m_mutex;
    bool m_enabled = false;
    std::string m_ggufLoaderPath;
    int m_interceptionPort = 0;
    std::atomic<uint64_t> m_idCounter{0};

    std::vector<HallucinationDetection> m_detectedHallucinations;
    std::vector<NavigationFix> m_navigationFixes;
    std::vector<BehaviorPatch> m_patches;
    std::map<std::string, std::string> m_halluciationPatterns;

    HallucinationDetectedCallback m_onHallucinationDetected;
};
