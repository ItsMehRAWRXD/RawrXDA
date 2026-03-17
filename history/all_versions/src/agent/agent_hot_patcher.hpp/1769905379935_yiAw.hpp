#pragma once

#include <vector>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>

// Forward declare json
using json = nlohmann::json;

struct HallucinationDetection {
    std::string detectionId;
    std::string hallucinationType;
    double confidence = 0.0;
    std::string detectedContent;
    std::string expectedContent;
    std::string correctionStrategy;
    std::string originalText; // Added to store context for correction
    std::chrono::system_clock::time_point detectedAt;
    bool correctionApplied = false;
};

struct NavigationFix {
    std::string fixId;
    std::string incorrectPath;
    std::string correctPath;
    std::string reasoning;
    double effectiveness = 0.0;
};

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

class AgentHotPatcher {
public:
    AgentHotPatcher();
    ~AgentHotPatcher();

    // Delete copy
    AgentHotPatcher(const AgentHotPatcher&) = delete;
    AgentHotPatcher& operator=(const AgentHotPatcher&) = delete;

    void initialize(const std::string& ggufLoaderPath, int flags);

    // Returns json object
    json interceptModelOutput(const std::string& modelOutput, const json& context);

    HallucinationDetection detectHallucination(const std::string& content, const json& context);
    std::string correctHallucination(const HallucinationDetection& detection);
    NavigationFix fixNavigationError(const std::string& path, const json& context);
    std::string applyBehaviorPatches(const std::string& output);

    void registerCorrectionPattern(const HallucinationDetection& pattern);
    void registerNavigationFix(const NavigationFix& fix);
    void createBehaviorPatch(const BehaviorPatch& patch);

    // Compatibility wrappers
    inline void addCorrectionPattern(const HallucinationDetection& p) { registerCorrectionPattern(p); }
    inline void addNavigationFix(const NavigationFix& f) { registerNavigationFix(f); }
    inline void addBehaviorPatch(const BehaviorPatch& p) { createBehaviorPatch(p); }

    void setHotPatchingEnabled(bool enabled);
    bool isHotPatchingEnabled() const;
    void setDebugLogging(bool enabled);

    json getCorrectionStatistics() const;
    int getCorrectionPatternCount() const;

    // Callbacks
    std::function<void(const HallucinationDetection&)> onHallucinationDetected;
    std::function<void(const HallucinationDetection&)> onHallucinationCorrected;
    std::function<void(const NavigationFix&)> onNavigationFixApplied;
    std::function<void(const BehaviorPatch&)> onBehaviorPatchApplied;

private:
    std::atomic<int> m_totalHallucinationsDetected{0};
    std::atomic<int> m_hallucinationsCorrected{0};
    std::atomic<int> m_navigationFixesApplied{0};

    bool m_hotPatchingEnabled = false;
    bool m_debugLogging = false;
    mutable std::mutex m_mutex;
    std::string m_ggufLoaderPath;
    
    std::vector<HallucinationDetection> m_correctionPatterns;
    std::vector<NavigationFix> m_navigationPatterns;
    std::vector<BehaviorPatch> m_behaviorPatches;
    
    HallucinationDetection detectPathHallucination(const std::string& content);
    HallucinationDetection detectLogicContradiction(const std::string& content);
    HallucinationDetection detectIncompleteReasoning(const std::string& content);
    std::string normalizePathInContent(const std::string& content, const std::string& validPath);
    bool isValidPath(const std::string& path) const;
};
