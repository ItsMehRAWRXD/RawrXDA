// ============================================================================
// File: src/agent/agent_hot_patcher.hpp
// Purpose: Real-time hallucination and navigation correction interface
// Converted from Qt to pure C++17
// ============================================================================
#pragma once

#include "../common/json_types.hpp"
#include "../common/callback_system.hpp"
#include "../common/time_utils.hpp"
#include "../common/string_utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>

struct HallucinationDetection {
    std::string detectionId;
    std::string hallucationType;
    double confidence = 0.0;
    std::string detectedContent;
    std::string expectedContent;
    std::string correctionStrategy;
    TimePoint detectedAt;
    bool correctionApplied = false;
};

struct NavigationFix {
    std::string fixId;
    std::string incorrectPath;
    std::string correctPath;
    std::string reasoning;
    double effectiveness = 0.0;
    TimePoint lastApplied;
    int timesCorrected = 0;
};

struct BehaviorPatch {
    std::string patchId;
    std::string patchType;
    std::string condition;
    std::string action;
    std::vector<std::string> affectedModels;
    double successRate = 0.0;
    bool enabled = true;
    TimePoint createdAt;
};

class AgentHotPatcher {
public:
    AgentHotPatcher();
    ~AgentHotPatcher();

    // Non-copyable
    AgentHotPatcher(const AgentHotPatcher&) = delete;
    AgentHotPatcher& operator=(const AgentHotPatcher&) = delete;

    bool initialize(const std::string& ggufLoaderPath, int interceptionPort);
    JsonObject interceptModelOutput(const std::string& modelOutput, const JsonObject& context);
    HallucinationDetection detectHallucination(const std::string& content, const JsonObject& context);
    std::string correctHallucination(const HallucinationDetection& hallucination);
    NavigationFix fixNavigationError(const std::string& path, const JsonObject& context);
    JsonObject applyBehaviorPatches(const JsonObject& output, const JsonObject& context);
    void registerCorrectionPattern(const HallucinationDetection& pattern);
    void registerNavigationFix(const NavigationFix& fix);
    void createBehaviorPatch(const BehaviorPatch& patch);
    void setHotPatchingEnabled(bool enabled);
    bool isHotPatchingEnabled() const;
    JsonObject getCorrectionStatistics() const;
    HallucinationDetection analyzeForHallucinations(const std::string& content);

    // Callbacks (replacing Qt signals)
    CallbackList<const HallucinationDetection&> onHallucinationDetected;
    CallbackList<const HallucinationDetection&, const std::string&> onHallucinationCorrected;
    CallbackList<const NavigationFix&> onNavigationErrorFixed;
    CallbackList<const BehaviorPatch&> onBehaviorPatchApplied;

private:
    std::string generateUniqueId();
    bool validateNavigationPath(const std::string& path);
    std::string extractReasoningChain(const JsonObject& output);
    std::vector<std::string> validateReasoningLogic(const std::string& reasoning);
    bool loadCorrectionPatterns();
    bool saveCorrectionPatterns();
    bool startInterceptorServer(int port);
    JsonObject processInterceptedResponse(const JsonObject& response);

    bool m_enabled = false;
    mutable std::mutex m_mutex;

    std::unordered_map<std::string, std::string> m_hallucationPatterns;
    std::unordered_map<std::string, std::string> m_navigationPatterns;
    std::vector<BehaviorPatch> m_behaviorPatches;

    std::vector<HallucinationDetection> m_detectedHallucinations;
    std::vector<NavigationFix> m_navigationFixes;

    std::string m_ggufLoaderPath;
    int m_interceptionPort = 0;
    std::atomic<int> m_idCounter{0};
};
