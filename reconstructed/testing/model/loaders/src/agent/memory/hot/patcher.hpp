// ============================================================================
// File: src/agent/memory_hot_patcher.hpp
// Purpose: Memory-level hot patching for hallucination correction and
//          navigation fixes in agentic execution contexts
// Converted from Qt to pure C++17
// ============================================================================
#pragma once

#include "../common/json_types.hpp"
#include "../common/callback_system.hpp"
#include "../common/time_utils.hpp"
#include "../common/string_utils.hpp"
#include "../common/logger.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <regex>
#include <cstdint>
#include <atomic>

// ── Hallucination Detection ─────────────────────────────────────────────────
struct HallucinationPattern {
    std::string patternId;
    std::string patternType;       // "factual_error", "code_error", "logical_error", "context_drift"
    std::string detectionRegex;
    std::string correctionTemplate;
    double confidenceThreshold;
    int hitCount;
    bool active;
    TimePoint lastTriggered;
    std::string description;
    std::vector<std::string> tags;
};

// ── Navigation Correction ───────────────────────────────────────────────────
struct NavigationCorrection {
    std::string correctionId;
    std::string triggerCondition;
    std::string originalPath;
    std::string correctedPath;
    std::string reasoning;
    int priority;
    bool autoApply;
    TimePoint createdAt;
    int applicationCount;
};

// ── Memory Intercept Rule ───────────────────────────────────────────────────
struct MemoryInterceptRule {
    std::string ruleId;
    std::string ruleName;
    std::string interceptPoint;    // "pre_read", "post_read", "pre_write", "post_write"
    std::string condition;
    std::string action;            // "block", "modify", "log", "redirect"
    std::string transformExpression;
    int priority;
    bool enabled;
    int executionCount;
    TimePoint lastExecuted;
    std::vector<std::string> affectedRegions;
};

// ── Corrected Execution Context ─────────────────────────────────────────────
struct CorrectedExecutionContext {
    std::string contextId;
    std::string originalContext;
    std::string correctedContext;
    std::vector<std::string> appliedPatches;
    std::vector<std::string> appliedCorrections;
    std::vector<std::string> interceptedAccesses;
    double originalConfidence;
    double correctedConfidence;
    TimePoint timestamp;
    bool wasModified;
    std::string modificationReason;
};

// ── Memory Region Descriptor ────────────────────────────────────────────────
struct MemoryRegionDescriptor {
    std::string regionId;
    std::string regionName;
    uintptr_t baseAddress;
    size_t size;
    std::string protection;     // "read", "write", "execute", "read_write"
    bool isMonitored;
    int accessCount;
    TimePoint lastAccessed;
};

// ── Patch Application Result ────────────────────────────────────────────────
struct PatchApplicationResult {
    bool success;
    std::string patchId;
    std::string detail;
    double confidenceDelta;
    int regionsAffected;
    TimePoint appliedAt;
};

// ── Memory Hot Patcher Class ────────────────────────────────────────────────
class MemoryHotPatcher {
public:
    MemoryHotPatcher();
    ~MemoryHotPatcher();

    // Hallucination pattern management
    bool registerPattern(const HallucinationPattern& pattern);
    bool removePattern(const std::string& patternId);
    HallucinationPattern* findPattern(const std::string& patternId);
    std::vector<HallucinationPattern> getActivePatterns() const;
    std::vector<HallucinationPattern> getAllPatterns() const;
    int getPatternHitCount(const std::string& patternId) const;

    // Navigation correction management
    bool addNavigationCorrection(const NavigationCorrection& correction);
    bool removeNavigationCorrection(const std::string& correctionId);
    std::vector<NavigationCorrection> getNavigationCorrections() const;
    std::string applyNavigationCorrection(const std::string& originalPath);

    // Memory intercept rules
    bool addInterceptRule(const MemoryInterceptRule& rule);
    bool removeInterceptRule(const std::string& ruleId);
    bool enableInterceptRule(const std::string& ruleId, bool enabled);
    std::vector<MemoryInterceptRule> getInterceptRules() const;

    // Memory region management
    bool registerMemoryRegion(const MemoryRegionDescriptor& region);
    bool unregisterMemoryRegion(const std::string& regionId);
    std::vector<MemoryRegionDescriptor> getMonitoredRegions() const;

    // Core hot patching operations
    CorrectedExecutionContext correctExecution(const std::string& inputContext,
                                                double inputConfidence);
    PatchApplicationResult applyHallucinationPatch(const std::string& patternId,
                                                    const std::string& targetText);
    std::vector<PatchApplicationResult> applyAllMatchingPatches(const std::string& text);

    // Batch operations
    int scanAndPatch(const std::string& text);
    std::string getStatisticsReport() const;

    // Serialization
    JsonObject toJson() const;
    bool fromJson(const JsonObject& json);

    // Callbacks (replacing Qt signals)
    CallbackList<const HallucinationPattern&> onPatternRegistered;
    CallbackList<const std::string&> onPatternRemoved;
    CallbackList<const PatchApplicationResult&> onPatchApplied;
    CallbackList<const CorrectedExecutionContext&> onExecutionCorrected;
    CallbackList<const NavigationCorrection&> onNavigationCorrected;
    CallbackList<const MemoryInterceptRule&, const std::string&> onInterceptTriggered;
    CallbackList<const std::string&> onError;

private:
    std::string generateUniqueId();
    bool evaluateCondition(const std::string& condition, const std::string& context);
    std::string applyTransform(const std::string& expression, const std::string& input);
    void processInterceptRules(const std::string& interceptPoint,
                               CorrectedExecutionContext& ctx);

    mutable std::mutex m_mutex;
    std::vector<HallucinationPattern> m_patterns;
    std::vector<NavigationCorrection> m_corrections;
    std::vector<MemoryInterceptRule> m_interceptRules;
    std::vector<MemoryRegionDescriptor> m_regions;
    std::atomic<int> m_idCounter;
    std::atomic<int> m_totalPatchesApplied;
    std::atomic<int> m_totalCorrections;
};
