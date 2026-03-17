// ============================================================================
// File: src/agent/agent_hot_patcher.hpp
// 
// Purpose: Real-time hallucination and navigation correction interface
// This system intercepts model outputs and corrects them in real-time
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include <vector>
#include <atomic>
#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @struct HallucinationDetection
 * @brief Information about detected hallucination
 */
struct HallucinationDetection {
    std::string detectionId;              ///< Unique detection ID
    std::string hallucinationType;        ///< Type: invalid_path, fabricated_path, logic_contradiction, etc.
    double confidence = 0.0;          ///< Confidence (0.0-1.0)
    std::string detectedContent;          ///< What was detected
    std::string expectedContent;          ///< What it should be
    std::string correctionStrategy;       ///< How to fix it
    // DateTime detectedAt;             ///< When detected
    bool correctionApplied = false;   ///< Was correction applied
};

/**
 * @struct NavigationFix
 * @brief Information about fixed navigation error
 */
struct NavigationFix {
    std::string fixId;                    ///< Unique fix ID
    std::string incorrectPath;            ///< Original path
    std::string correctPath;              ///< Normalized path
    std::string reasoning;                ///< Why it was wrong
    double effectiveness = 0.0;       ///< How effective (0.0-1.0)
};

/**
 * @struct BehaviorPatch
 * @brief Behavior modification for model outputs
 */
struct BehaviorPatch {
    std::string patchId;                  ///< Unique patch ID
    std::string patchType;                ///< Type: prompt_modifier, output_filter, validator
    std::string condition;                ///< When to apply
    std::string action;                   ///< What to do
    std::vector<std::string> affectedModels;       ///< Models to apply to
    double successRate = 0.0;         ///< Success rate
    bool enabled = true;              ///< Is patch enabled
    // DateTime createdAt;              ///< When created
};

/**
 * @class AgentHotPatcher
 * @brief Real-time hallucination detection and correction
 * 
 * Features:
 * - Detects 6 types of hallucinations
 * - Applies real-time corrections
 * - Learns from corrections
 * - Thread-safe operation (atomic stats, mutex-protected vectors)
 * - Full statistics tracking
 */
class AgentHotPatcher {
public:
    AgentHotPatcher();
    ~AgentHotPatcher();

    AgentHotPatcher(const AgentHotPatcher&) = delete;
    AgentHotPatcher& operator=(const AgentHotPatcher&) = delete;

    /**
     * Initialize hot patcher
     * @param ggufLoaderPath Path to GGUF loader executable
     * @param flags Reserved for future use
     */
    void initialize(const std::string& ggufLoaderPath, int flags);

    /**
     * Intercept and patch model output
     * @param modelOutput Raw model output
     * @param context Execution context
     * @return Corrected output as JSON
     */
    json interceptModelOutput(const std::string& modelOutput, const json& context);

    /**
     * Detect hallucinations in content
     * @param content Content to analyze
     * @param context Execution context
     * @return Detection info (empty if no hallucination)
     */
    HallucinationDetection detectHallucination(const std::string& content, const json& context);

    /**
     * Apply correction to detected hallucination
     * @param detection Hallucination detection
     * @return Corrected content
     */
    std::string correctHallucination(const HallucinationDetection& detection);

    /**
     * Fix navigation error in path
     * @param path Incorrect path
     * @param context Execution context
     * @return Navigation fix info
     */
    NavigationFix fixNavigationError(const std::string& path, const json& context);

    /**
     * Apply behavior patches to output
     * @param output Original output
     * @return Patched output
     */
    std::string applyBehaviorPatches(const std::string& output);

    /**
     * Register a correction pattern
     * @param pattern Pattern to register
     */
    void registerCorrectionPattern(const HallucinationDetection& pattern);

    /**
     * Register navigation fix pattern
     * @param fix Fix to register
     */
    void registerNavigationFix(const NavigationFix& fix);

    /**
     * Create behavior patch
     * @param patch Patch to create
     */
    void createBehaviorPatch(const BehaviorPatch& patch);

    /**
     * Convenience wrapper: add correction pattern (bridge compatibility)
     */
    inline void addCorrectionPattern(const HallucinationDetection& p) { registerCorrectionPattern(p); }

    /**
     * Convenience wrapper: add navigation fix (bridge compatibility)
     */
    inline void addNavigationFix(const NavigationFix& f) { registerNavigationFix(f); }

    /**
     * Convenience wrapper: add behavior patch (bridge compatibility)
     */
    inline void addBehaviorPatch(const BehaviorPatch& p) { createBehaviorPatch(p); }

    /**
     * Enable/disable hot patching
     * @param enabled True to enable
     */
    void setHotPatchingEnabled(bool enabled);

    /**
     * Check if hot patching is enabled
     */
    bool isHotPatchingEnabled() const;

    /**
     * Enable debug logging
     * @param enabled True to enable
     */
    void setDebugLogging(bool enabled);

    /**
     * Get correction statistics
     * @return Statistics JSON
     */
    json getCorrectionStatistics() const;

    /**
     * Get count of correction patterns
     */
    int getCorrectionPatternCount() const;

    // Events (Callbacks instead of Signals)
    void hallucinationDetected(const HallucinationDetection& detection);
    void hallucinationCorrected(const HallucinationDetection& correction, const std::string& corrected);
    void navigationErrorFixed(const NavigationFix& fix);
    void behaviorPatchApplied(const BehaviorPatch& patch);
    void statisticsUpdated(const json& stats);

private:
    // ====== Statistics (lock-free atomic counters) ======
    std::atomic<int> m_totalHallucinationsDetected{0};
    std::atomic<int> m_hallucinationsCorrected{0};
    std::atomic<int> m_navigationFixesApplied{0};

    // ====== Runtime state (protected by m_mutex) ======
    bool m_hotPatchingEnabled = false;
    bool m_debugLogging = false;
    mutable std::mutex m_mutex;
    
    // Patterns
    std::vector<HallucinationDetection> m_correctionPatterns;
    std::vector<NavigationFix> m_navigationPatterns;
    std::vector<BehaviorPatch> m_behaviorPatches;

    // Runtime tracking
    std::vector<HallucinationDetection> m_detectedHallucinations;
    std::vector<NavigationFix> m_navigationFixes;
    std::map<std::string, std::string> m_hallucinationPatterns;
    std::map<std::string, std::string> m_navigationPatternsMap;

    std::string m_ggufLoaderPath;
    int m_interceptionPort = 0;
    int m_idCounter = 0;
    bool m_enabled = false;
    
    // Helper methods
    HallucinationDetection detectPathHallucination(const std::string& content);
    HallucinationDetection detectLogicContradiction(const std::string& content);
    HallucinationDetection detectIncompleteReasoning(const std::string& content);
    std::string normalizePathInContent(const std::string& content, const std::string& validPath);
    bool isValidPath(const std::string& path) const;
    
    // Additional private helper methods
    std::string extractReasoningChain(const json& output);
    std::vector<std::string> validateReasoningLogic(const std::string& reasoning);
    std::string generateUniqueId();
    bool loadCorrectionPatterns();
    bool saveCorrectionPatterns();
    bool startInterceptorServer(int port);
    json processInterceptedResponse(const json& response);
    bool validateNavigationPath(const std::string& path);
    json applyBehaviorPatches(json output, const json& context);
};

// Qt meta-type registration (must be outside class definition)
// // Q_DECLARE_METATYPE(HallucinationDetection)
// // Q_DECLARE_METATYPE(NavigationFix)
// // Q_DECLARE_METATYPE(BehaviorPatch)







