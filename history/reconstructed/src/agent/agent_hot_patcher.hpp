// ============================================================================
// File: src/agent/agent_hot_patcher.hpp
//
// Purpose: Real-time hallucination and navigation correction interface
// This system intercepts model outputs and corrects them in real-time
//
// Architecture: C++20, no Qt, no exceptions
// Error model: PatchResult pattern where applicable
// Threading: std::mutex + std::lock_guard + std::atomic
//
// License: Production Grade - Enterprise Ready
// ============================================================================

#pragma once

#include "simple_json.hpp"

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <cstdint>

/**
 * @struct HallucinationDetection
 * @brief Information about detected hallucination
 */
struct HallucinationDetection {
    std::string detectionId;              ///< Unique detection ID
    std::string hallucinationType;        ///< Type: invalid_path, fabricated_path, logic_contradiction, etc.
    double confidence = 0.0;              ///< Confidence (0.0-1.0)
    std::string detectedContent;          ///< What was detected
    std::string expectedContent;          ///< What it should be
    std::string correctionStrategy;       ///< How to fix it
    std::string detectedAt;               ///< ISO timestamp when detected
    bool correctionApplied = false;       ///< Was correction applied
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
    double effectiveness = 0.0;           ///< How effective (0.0-1.0)
    std::string lastApplied;              ///< ISO timestamp when last applied
    int timesCorrected = 0;               ///< How many times corrected
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
    std::vector<std::string> affectedModels; ///< Models to apply to
    double successRate = 0.0;             ///< Success rate
    bool enabled = true;                  ///< Is patch enabled
    std::string createdAt;                ///< ISO timestamp when created
};

// ═══════════════════════════════════════════════════════════════════════════
// Callback types (replace Qt signals)
// ═══════════════════════════════════════════════════════════════════════════

typedef void (*HallucinationDetectedCallback)(const HallucinationDetection& detection, void* userData);
typedef void (*HallucinationCorrectedCallback)(const HallucinationDetection& detection,
                                                const std::string& corrected, void* userData);
typedef void (*NavigationErrorFixedCallback)(const NavigationFix& fix, void* userData);
typedef void (*BehaviorPatchAppliedCallback)(const BehaviorPatch& patch, void* userData);
typedef void (*StatisticsUpdatedCallback)(const JsonValue& stats, void* userData);

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
 * - Non-copyable
 */
class AgentHotPatcher {
public:
    AgentHotPatcher(const AgentHotPatcher&) = delete;
    AgentHotPatcher& operator=(const AgentHotPatcher&) = delete;

    AgentHotPatcher();
    ~AgentHotPatcher();

    /**
     * Initialize hot patcher
     * @param ggufLoaderPath Path to GGUF loader executable
     * @param interceptionPort Port for interception server (0 = disabled)
     * @return true on success
     */
    bool initialize(const std::string& ggufLoaderPath, int interceptionPort);

    /**
     * Intercept and patch model output
     * @param modelOutput Raw model output (JSON string)
     * @param context Execution context
     * @return Corrected output as JsonValue
     */
    JsonValue interceptModelOutput(const std::string& modelOutput, const JsonValue& context);

    /**
     * Detect hallucinations in content
     * @param content Content to analyze
     * @param context Execution context
     * @return Detection info (empty detectionId if no hallucination)
     */
    HallucinationDetection detectHallucination(const std::string& content, const JsonValue& context);

    /**
     * Apply correction to detected hallucination
     * @param detection Hallucination detection
     * @return Corrected content (empty if no correction)
     */
    std::string correctHallucination(const HallucinationDetection& detection);

    /**
     * Fix navigation error in path
     * @param navigationPath Incorrect path
     * @param projectContext Execution context
     * @return Navigation fix info (empty fixId if no fix needed)
     */
    NavigationFix fixNavigationError(const std::string& navigationPath, const JsonValue& projectContext);

    /**
     * Apply behavior patches to output object
     * @param output Original output (JsonValue Object)
     * @param context Execution context
     * @return Patched output
     */
    JsonValue applyBehaviorPatches(const JsonValue& output, const JsonValue& context);

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
     * @return Statistics as JsonValue
     */
    JsonValue getCorrectionStatistics() const;

    /**
     * Get count of correction patterns
     */
    int getCorrectionPatternCount() const;

    // ─────────────────────────────────────────────────────────────────
    // Callback Registration (replaces Qt signals)
    // ─────────────────────────────────────────────────────────────────

    void registerHallucinationDetectedCallback(HallucinationDetectedCallback cb, void* userData);
    void registerHallucinationCorrectedCallback(HallucinationCorrectedCallback cb, void* userData);
    void registerNavigationErrorFixedCallback(NavigationErrorFixedCallback cb, void* userData);
    void registerBehaviorPatchAppliedCallback(BehaviorPatchAppliedCallback cb, void* userData);
    void registerStatisticsUpdatedCallback(StatisticsUpdatedCallback cb, void* userData);

private:
    // ====== Statistics (lock-free atomic counters) ======
    std::atomic<int> m_totalHallucinationsDetected{0};
    std::atomic<int> m_hallucinationsCorrected{0};
    std::atomic<int> m_navigationFixesApplied{0};

    // ====== Runtime state (protected by m_mutex) ======
    bool m_enabled = false;
    bool m_debugLogging = false;
    mutable std::mutex m_mutex;

    // Configuration
    std::string m_ggufLoaderPath;
    int m_interceptionPort = 0;
    int m_idCounter = 0;

    // Pattern maps (key -> correction mapping)
    std::map<std::string, std::string> m_hallucationPatterns;  // detectedContent -> expectedContent
    std::map<std::string, std::string> m_navigationPatterns;   // incorrectPath -> correctPath

    // Runtime collections
    std::vector<BehaviorPatch> m_behaviorPatches;
    std::vector<HallucinationDetection> m_detectedHallucinations;
    std::vector<NavigationFix> m_navigationFixes;

    // ====== Private methods ======
    HallucinationDetection analyzeForHallucinations(const std::string& content);
    bool validateNavigationPath(const std::string& path);
    std::string extractReasoningChain(const JsonValue& output);
    std::vector<std::string> validateReasoningLogic(const std::string& reasoning);
    std::string generateUniqueId();
    bool loadCorrectionPatterns();
    bool saveCorrectionPatterns();
    bool startInterceptorServer(int port);
    JsonValue processInterceptedResponse(const JsonValue& response);

    // Detection helpers
    HallucinationDetection detectPathHallucination(const std::string& content);
    HallucinationDetection detectLogicContradiction(const std::string& content);
    HallucinationDetection detectIncompleteReasoning(const std::string& content);
    std::string normalizePathInContent(const std::string& content, const std::string& validPath);
    bool isValidPath(const std::string& path) const;

    // ====== Callback notification helpers ======
    void notifyHallucinationDetected(const HallucinationDetection& detection);
    void notifyHallucinationCorrected(const HallucinationDetection& detection, const std::string& corrected);
    void notifyNavigationErrorFixed(const NavigationFix& fix);
    void notifyBehaviorPatchApplied(const BehaviorPatch& patch);
    void notifyStatisticsUpdated(const JsonValue& stats);

    // ====== Callback storage ======
    struct HallucinationDetectedCB  { HallucinationDetectedCallback fn;  void* userData; };
    struct HallucinationCorrectedCB { HallucinationCorrectedCallback fn; void* userData; };
    struct NavigationErrorFixedCB   { NavigationErrorFixedCallback fn;   void* userData; };
    struct BehaviorPatchAppliedCB   { BehaviorPatchAppliedCallback fn;   void* userData; };
    struct StatisticsUpdatedCB      { StatisticsUpdatedCallback fn;      void* userData; };

    std::vector<HallucinationDetectedCB>  m_hallucinationDetectedCBs;
    std::vector<HallucinationCorrectedCB> m_hallucinationCorrectedCBs;
    std::vector<NavigationErrorFixedCB>   m_navigationErrorFixedCBs;
    std::vector<BehaviorPatchAppliedCB>   m_behaviorPatchAppliedCBs;
    std::vector<StatisticsUpdatedCB>      m_statisticsUpdatedCBs;
};
