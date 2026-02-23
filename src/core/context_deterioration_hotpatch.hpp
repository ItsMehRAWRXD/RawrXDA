// context_deterioration_hotpatch.hpp — Proactive Context Quality Hotpatch
// ============================================================================
// Prevents model deterioration when context exceeds architecture sweet spots.
// Many LLMs degrade ("lost in the middle", coherence drop) after N tokens.
// This hotpatch keeps quality at 100% by compressing/truncating context
// BEFORE inference, so the model never sees degraded-length input.
//
// Mitigation strategies:
//   - Sliding window: keep last N tokens within sweet spot
//   - Truncation with summary placeholder for dropped content
//   - Model-specific deterioration profiles (LLaMA, Mistral, Qwen, etc.)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// DeteriorationMitigation — What action was taken
// ---------------------------------------------------------------------------
enum class DeteriorationMitigation : uint8_t {
    None           = 0,  // Context within sweet spot, no action
    SlidingWindow  = 1,  // Dropped oldest messages, kept recent
    Truncate       = 2,  // Hard truncation to stay under threshold
    CompressHint   = 3,  // Injected summary placeholder for dropped content
    EmergencyCap   = 4,  // Context was way over, aggressive truncation
};

// ---------------------------------------------------------------------------
// PrepareResult — Output of prepareContextForInference
// ---------------------------------------------------------------------------
struct ContextPrepareResult {
    std::string     modifiedPrompt;       // Context ready for model (possibly truncated)
    int             qualityScore;         // 0-100, 100 = full quality expected
    DeteriorationMitigation mitigation;   // What we did (if anything)
    size_t          originalTokens;       // Estimated tokens before mitigation
    size_t          finalTokens;          // Estimated tokens after mitigation
    size_t          droppedTokens;        // Tokens removed for quality
    const char*     description;          // Human-readable explanation
};

// ---------------------------------------------------------------------------
// ModelDeteriorationProfile — Per-model quality sweet spots
// ---------------------------------------------------------------------------
struct ModelDeteriorationProfile {
    std::string     modelName;            // e.g. "llama", "mistral", "qwen"
    uint32_t        sweetSpotTokens;      // Ideal context for 100% quality
    uint32_t        deteriorationStart;   // Where quality starts dropping (% of max, or 0=use sweetSpot*1.2)
    uint32_t        aggressiveThreshold;  // Apply aggressive mitigation above this
    bool            preferSlidingWindow;  // true = sliding, false = truncate from middle
    bool            enabled;
};

// ---------------------------------------------------------------------------
// ContextDeteriorationHotpatchStats
// ---------------------------------------------------------------------------
struct ContextDeteriorationHotpatchStats {
    std::atomic<uint64_t> preparationsTotal{0};
    std::atomic<uint64_t> mitigationsApplied{0};
    std::atomic<uint64_t> slidingWindowCount{0};
    std::atomic<uint64_t> truncationCount{0};
    std::atomic<uint64_t> tokensSaved{0};
    std::atomic<uint64_t> quality100Count{0};   // Times we kept at 100%
};

// ---------------------------------------------------------------------------
// ContextDeteriorationHotpatch — Main class
// ---------------------------------------------------------------------------
class ContextDeteriorationHotpatch {
public:
    static ContextDeteriorationHotpatch& instance();

    // ---- Core API ----
    // Prepare context for inference. Call BEFORE sending to model.
    // If context exceeds sweet spot, applies mitigation and returns modified prompt.
    ContextPrepareResult prepareContextForInference(
        const std::string& prompt,
        size_t estimatedTokens,
        uint32_t modelMaxContext,
        const char* modelName = nullptr);

    // Simpler overload: pass prompt and let us estimate tokens.
    ContextPrepareResult prepareContextForInference(
        const std::string& prompt,
        uint32_t modelMaxContext,
        const char* modelName = nullptr);

    // Get expected quality score (0-100) without modifying context.
    int getQualityScore(size_t currentTokens, uint32_t modelMaxContext,
                        const char* modelName = nullptr) const;

    // ---- Model Profiles ----
    PatchResult registerProfile(const ModelDeteriorationProfile& profile);
    PatchResult unregisterProfile(const char* modelName);
    const ModelDeteriorationProfile* getProfile(const char* modelName) const;

    // ---- Configuration ----
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    void setTargetQuality(int score);  // Default 100; lower = allow some degradation
    int getTargetQuality() const { return m_targetQuality; }

    // ---- Statistics ----
    const ContextDeteriorationHotpatchStats& getStats() const;
    void resetStats();

    // ---- Token Estimation ----
    static size_t estimateTokens(const std::string& text);

private:
    ContextDeteriorationHotpatch();
    ~ContextDeteriorationHotpatch() = default;
    ContextDeteriorationHotpatch(const ContextDeteriorationHotpatch&) = delete;
    ContextDeteriorationHotpatch& operator=(const ContextDeteriorationHotpatch&) = delete;

    void loadDefaultProfiles();
    const ModelDeteriorationProfile* resolveProfile(const char* modelName,
                                                     uint32_t modelMaxContext) const;
    ContextPrepareResult applySlidingWindow(const std::string& prompt,
                                            size_t tokens,
                                            uint32_t targetTokens,
                                            const ModelDeteriorationProfile* profile);
    ContextPrepareResult applyTruncation(const std::string& prompt,
                                         size_t tokens,
                                         uint32_t targetTokens,
                                         const ModelDeteriorationProfile* profile);
    std::string truncateToTokenBoundary(const std::string& text, size_t maxChars);

    mutable std::mutex                      m_mutex;
    std::vector<ModelDeteriorationProfile>  m_profiles;
    ContextDeteriorationHotpatchStats       m_stats;
    bool                                    m_enabled;
    int                                     m_targetQuality;
};
