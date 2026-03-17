// agentic_hotpatch_orchestrator.hpp — Phase 18: Agent-Driven Hotpatch Orchestration
//
// Bridges the agentic failure detection pipeline with the three-layer
// hotpatch system. When the agent detects inference failures (refusal,
// hallucination, timeout, safety violation, etc.), it can automatically
// apply corrective hotpatches: memory patches, byte-level GGUF edits,
// server-layer request/response transforms, and proxy rewrites.
//
// This is the Win32IDE version. Uses function-pointer callbacks and poll-based notification.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once
#include <functional>
#include "../core/model_memory_hotpatch.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "../core/proxy_hotpatcher.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// FailureType — Detectable inference failure categories
// ---------------------------------------------------------------------------
enum class InferenceFailureType : uint8_t {
    None              = 0,
    Refusal           = 1,      // Model refuses to respond
    Hallucination     = 2,      // False / fabricated information
    FormatViolation   = 3,      // Output doesn't match expected format
    InfiniteLoop      = 4,      // Repeating content
    TokenLimit        = 5,      // Truncated response
    ResourceExhausted = 6,      // Out of memory / compute
    Timeout           = 7,      // Inference took too long
    SafetyViolation   = 8,      // Triggered safety filters
    LowConfidence     = 9,      // Below confidence threshold
    GarbageOutput     = 10,     // Nonsensical / corrupted output
};

// ---------------------------------------------------------------------------
// FailureEvent — Describes a detected failure
// ---------------------------------------------------------------------------
struct InferenceFailureEvent {
    InferenceFailureType    type;
    float                   confidence;     // 0.0 – 1.0
    const char*             description;
    uint64_t                timestamp;      // GetTickCount64()
    uint64_t                sequenceId;
};

// ---------------------------------------------------------------------------
// CorrectionAction — What the orchestrator does in response to a failure
// ---------------------------------------------------------------------------
enum class CorrectionAction : uint8_t {
    None              = 0,
    RetryWithBias     = 1,      // Apply token bias and retry
    RewriteOutput     = 2,      // Pattern-replace in output
    TerminateStream   = 3,      // Force-stop the stream
    PatchMemory       = 4,      // Apply memory-layer patch
    PatchBytes        = 5,      // Apply byte-layer GGUF patch
    InjectServerPatch = 6,      // Add server-layer transform
    EscalateToUser    = 7,      // Cannot auto-correct, notify user
    SwitchModel       = 8,      // Try a different model/backend
};

// ---------------------------------------------------------------------------
// CorrectionPolicy — Maps failure types to correction actions
// ---------------------------------------------------------------------------
struct CorrectionPolicy {
    InferenceFailureType    failureType;
    CorrectionAction        primaryAction;
    CorrectionAction        fallbackAction;
    int                     maxRetries;
    float                   confidenceThreshold;    // Only act above this
    bool                    enabled;
};

// ---------------------------------------------------------------------------
// CorrectionResult — Outcome of an orchestrated correction
// ---------------------------------------------------------------------------
struct CorrectionOutcome {
    bool                    success;
    CorrectionAction        actionTaken;
    const char*             detail;
    int                     retriesUsed;
    uint64_t                durationMs;

    static CorrectionOutcome ok(CorrectionAction action, const char* msg, int retries = 0) {
        CorrectionOutcome r;
        r.success    = true;
        r.actionTaken = action;
        r.detail     = msg;
        r.retriesUsed = retries;
        r.durationMs = 0;
        return r;
    }

    static CorrectionOutcome error(CorrectionAction action, const char* msg) {
        CorrectionOutcome r;
        r.success    = false;
        r.actionTaken = action;
        r.detail     = msg;
        r.retriesUsed = 0;
        r.durationMs = 0;
        return r;
    }
};

// ---------------------------------------------------------------------------
// Callback types (no std::function)
// ---------------------------------------------------------------------------
typedef void (*FailureDetectedCallback)(const InferenceFailureEvent* evt, void* userData);
typedef void (*CorrectionAppliedCallback)(const CorrectionOutcome* outcome, void* userData);
typedef bool (*OutputValidatorFn)(const char* output, size_t len, void* userData);

// ---------------------------------------------------------------------------
// OrchestrationStats
// ---------------------------------------------------------------------------
struct OrchestrationStats {
    std::atomic<uint64_t> failuresDetected{0};
    std::atomic<uint64_t> correctionsAttempted{0};
    std::atomic<uint64_t> correctionsSucceeded{0};
    std::atomic<uint64_t> correctionsFailed{0};
    std::atomic<uint64_t> escalationsToUser{0};
    std::atomic<uint64_t> retryCount{0};
    std::atomic<uint64_t> biasInjections{0};
    std::atomic<uint64_t> outputRewrites{0};
    std::atomic<uint64_t> streamTerminations{0};
};

// ---------------------------------------------------------------------------
// AgenticHotpatchOrchestrator — Main class
// ---------------------------------------------------------------------------
class AgenticHotpatchOrchestrator {
public:
    static AgenticHotpatchOrchestrator& instance();

    // ---- Failure Detection ----
    // Analyze model output and detect failure type(s).
    InferenceFailureEvent detectFailure(const char* output, size_t outputLen,
                                         const char* prompt = nullptr,
                                         size_t promptLen = 0);

    // Check a batch of outputs, returns count of failures detected.
    size_t detectFailures(const char** outputs, const size_t* lengths,
                          size_t count, InferenceFailureEvent* outEvents);

    // ---- Correction Orchestration ----
    // Given a failure event, orchestrate the appropriate correction.
    CorrectionOutcome orchestrateCorrection(const InferenceFailureEvent& failure,
                                             char* outputBuffer = nullptr,
                                             size_t bufferCapacity = 0);

    // Full pipeline: detect + correct in one call.
    CorrectionOutcome analyzeAndCorrect(const char* output, size_t outputLen,
                                         const char* prompt = nullptr,
                                         size_t promptLen = 0,
                                         char* correctedOutput = nullptr,
                                         size_t correctedCapacity = 0);

    // ---- Policy Management ----
    PatchResult addPolicy(const CorrectionPolicy& policy);
    PatchResult removePolicy(InferenceFailureType failureType);
    PatchResult clearPolicies();
    const std::vector<CorrectionPolicy>& getPolicies() const;

    // Reset to default policies (sensible defaults for all failure types).
    void loadDefaultPolicies();

    // ---- Pattern Configuration ----
    void addRefusalPattern(const char* pattern);
    void addHallucinationPattern(const char* pattern);
    void addLoopPattern(const char* pattern);
    void addSafetyPattern(const char* pattern);
    void addGarbagePattern(const char* pattern);

    // ---- Callbacks ----
    void registerFailureCallback(FailureDetectedCallback cb, void* userData);
    void registerCorrectionCallback(CorrectionAppliedCallback cb, void* userData);
    void unregisterFailureCallback(FailureDetectedCallback cb);
    void unregisterCorrectionCallback(CorrectionAppliedCallback cb);

    // ---- Statistics ----
    const OrchestrationStats& getStats() const;
    void resetStats();

    // ---- Enable / Disable ----
    void setEnabled(bool enabled);
    bool isEnabled() const;

    // ---- Configuration ----
    void setMaxRetries(int retries);
    void setConfidenceThreshold(float threshold);
    void setAutoEscalate(bool enabled);

private:
    AgenticHotpatchOrchestrator();
    ~AgenticHotpatchOrchestrator();
    AgenticHotpatchOrchestrator(const AgenticHotpatchOrchestrator&) = delete;
    AgenticHotpatchOrchestrator& operator=(const AgenticHotpatchOrchestrator&) = delete;

    // Internal detection helpers
    float detectRefusal(const char* output, size_t len);
    float detectHallucination(const char* output, size_t len);
    float detectInfiniteLoop(const char* output, size_t len);
    float detectFormatViolation(const char* output, size_t len);
    float detectGarbageOutput(const char* output, size_t len);
    float detectSafetyViolation(const char* output, size_t len);

    // Internal correction helpers
    CorrectionOutcome applyBiasCorrection(const InferenceFailureEvent& failure);
    CorrectionOutcome applyRewriteCorrection(const InferenceFailureEvent& failure,
                                              char* buffer, size_t capacity);
    CorrectionOutcome applyStreamTermination(const InferenceFailureEvent& failure);
    CorrectionOutcome escalateToUser(const InferenceFailureEvent& failure);

    // Fire callbacks
    void notifyFailure(const InferenceFailureEvent& evt);
    void notifyCorrection(const CorrectionOutcome& outcome);

    // Policy lookup
    const CorrectionPolicy* findPolicy(InferenceFailureType type) const;

    // State
    std::mutex                                  m_mutex;
    OrchestrationStats                          m_stats;
    std::vector<CorrectionPolicy>               m_policies;
    std::vector<std::string>                    m_refusalPatterns;
    std::vector<std::string>                    m_hallucinationPatterns;
    std::vector<std::string>                    m_loopPatterns;
    std::vector<std::string>                    m_safetyPatterns;
    std::vector<std::string>                    m_garbagePatterns;
    bool                                        m_enabled;
    int                                         m_maxRetries;
    float                                       m_confidenceThreshold;
    bool                                        m_autoEscalate;
    uint64_t                                    m_sequenceCounter;

    // Callback storage
    struct FailureCB { FailureDetectedCallback fn; void* userData; };
    struct CorrectionCB { CorrectionAppliedCallback fn; void* userData; };
    std::vector<FailureCB>                      m_failureCallbacks;
    std::vector<CorrectionCB>                   m_correctionCallbacks;
};
