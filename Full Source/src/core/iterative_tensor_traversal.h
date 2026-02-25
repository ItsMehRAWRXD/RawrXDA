// ============================================================================
// iterative_tensor_traversal.h — Iterative Partial Inference Engine
// ============================================================================
//
//  ╔══════════════════════════════════════════════════════════════════════╗
//  ║  ITERATIVE PARTIAL INFERENCE WITH ADAPTIVE TENSOR TRAVERSAL        ║
//  ║  AND HOTPATCHED STATE RE-ENTRY                                     ║
//  ╚══════════════════════════════════════════════════════════════════════╝
//
// This is the core engine that implements the self-stabilizing, iterative
// tensor traversal system. It allows models larger than the hardware would
// normally tolerate to run by progressive convergence instead of monolithic
// execution.
//
// ═══════════════════════════════════════════════════════════════════════
// ARCHITECTURE OVERVIEW
// ═══════════════════════════════════════════════════════════════════════
//
// The model is treated NOT as a function, but as a SEARCH PROCESS over
// its own internal representations. Each inference pass:
//
//   1. Uses only the resources currently available
//   2. Traverses some subset of tensor space
//   3. Emits partial semantic signal
//   4. Observes performance + stability
//   5. Adjusts traversal strategy
//   6. Re-enters from a different slice next time
//
// Repeat until convergence.
//
// This is fundamentally different from traditional inference:
//   - NOT streaming (not one pass, one result)
//   - NOT quantization (not reducing precision)
//   - NOT offloading (not moving data)
//   - NOT KV cache paging (not managing cache)
//
// It is closest to a hybrid of:
//   - Iterative deepening search
//   - Speculative decoding
//   - Anytime algorithms
//   - Adaptive control systems
//
// ═══════════════════════════════════════════════════════════════════════
// THE CONTROL LOOP
// ═══════════════════════════════════════════════════════════════════════
//
//  ┌────────────────┐
//  │  Probe Run     │ ← small ctx, few layers, minimal tokens
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Emit Partial  │ ← capture best-known semantic signal
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Measure Delta │ ← compare with previous pass
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Score Layers  │ ← which layers contributed?
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Cache Results │ ← save stable tensor slices
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Decide Next   │ ← convergence? continue? adapt?
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Adapt Strategy│ ← new traversal plan based on feedback
//  └───────┬────────┘
//          │
//          ▼
//  ┌────────────────┐
//  │  Re-enter      │ ← new tensor slice, new hop pattern
//  └────────────────┘
//          │
//          └─── (loop back to Probe/Emit)
//
// ═══════════════════════════════════════════════════════════════════════
// WHY THIS ENABLES 120B+ ON SLOW SYSTEMS
// ═══════════════════════════════════════════════════════════════════════
//
// Traditional inference fails because it assumes:
//   "I must process ALL layers, ALL tokens, NOW."
//
// This system says:
//   "I will process WHAT I CAN, WHEN I CAN, and ADAPT."
//
// Key advantages:
//   1. No single hard failure point (degrade gracefully)
//   2. Progressive semantic convergence (refine, don't restart)
//   3. Hardware-aware traversal (same model, different behavior)
//   4. Hotpatching enables live reconfiguration mid-session
//
// ═══════════════════════════════════════════════════════════════════════
// INTEGRATION POINTS
// ═══════════════════════════════════════════════════════════════════════
//
// This engine coordinates:
//   - TraversalStrategy:         Which layers to visit, in what order
//   - LayerContributionScorer:   Which layers matter for this query
//   - SemanticDeltaTracker:      When to stop (convergence detection)
//   - CrossRunTensorCache:       Reuse stable results across passes
//   - ConvergenceController:     Budget/quality/resource management
//   - UnifiedHotpatchManager:    Live parameter mutation mid-session
//   - GPUKernelAutoTuner:        Hardware-aware dispatch tuning
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - No std::function — raw function pointers for callbacks
//   - Thread-safe singleton
//   - Deterministic: same inputs + same hardware = same traversal
//   - All state transitions are logged and observable
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef ITERATIVE_TENSOR_TRAVERSAL_H
#define ITERATIVE_TENSOR_TRAVERSAL_H

#include "model_memory_hotpatch.hpp"       // PatchResult
#include "traversal_strategy.h"            // TraversalPlan, HardwareFeedback
#include "layer_contribution_scorer.h"     // LayerScore
#include "semantic_delta_tracker.h"        // ConvergenceState, PassSnapshot
#include "cross_run_tensor_cache.h"        // TensorSliceKey, CachedTensorSlice
#include "convergence_controller.h"        // PassDecision, StopReason
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

// ============================================================================
// Forward declarations
// ============================================================================
class UnifiedHotpatchManager;
class GPUKernelAutoTuner;

// ============================================================================
// Enums
// ============================================================================

// SessionPhase — Current phase of the iterative loop
enum class SessionPhase : uint8_t {
    Idle         = 0,   // No active session
    Probing      = 1,   // Running a probe pass
    Emitting     = 2,   // Capturing partial signal
    Measuring    = 3,   // Computing deltas
    Scoring      = 4,   // Evaluating layer contributions
    Caching      = 5,   // Storing stable tensor slices
    Deciding     = 6,   // Convergence check
    Adapting     = 7,   // Strategy adjustment
    ReEntering   = 8,   // Preparing next pass
    Converged    = 9,   // Session complete: converged
    Stopped      = 10,  // Session complete: stopped (budget/interrupt)
    Failed       = 11   // Session complete: unrecoverable failure
};

// InferenceMode — How to execute a single pass
enum class InferenceMode : uint8_t {
    CPU_Only     = 0,   // Pure CPU inference
    GPU_Primary  = 1,   // GPU with CPU fallback
    Hybrid       = 2,   // Split layers between CPU/GPU
    Cached       = 3    // All layers from cache (no computation)
};

// ============================================================================
// ITTRequest — Input to the iterative inference engine
// ============================================================================
struct ITTRequest {
    // Prompt text
    const char*     prompt;
    uint32_t        promptLen;

    // Token IDs (pre-tokenized, optional)
    const uint32_t* promptTokens;
    uint32_t        promptTokenCount;

    // Generation parameters
    float           temperature;
    float           topP;
    uint32_t        topK;
    float           repeatPenalty;
    uint32_t        maxTokens;       // Maximum tokens to generate total

    // Resource constraints
    double          timeBudgetMs;    // 0 = unlimited
    uint32_t        passBudget;      // 0 = unlimited
    double          maxMemoryBytes;  // 0 = unlimited

    // Quality target
    float           qualityTarget;   // 0.0-1.0 (0 = fast, 1.0 = best possible)

    // Inference mode preference
    InferenceMode   preferredMode;

    // User data (passed to callbacks)
    void*           userData;

    ITTRequest()
        : prompt(nullptr), promptLen(0)
        , promptTokens(nullptr), promptTokenCount(0)
        , temperature(0.7f), topP(0.9f), topK(40), repeatPenalty(1.1f)
        , maxTokens(512), timeBudgetMs(30000.0), passBudget(50)
        , maxMemoryBytes(0), qualityTarget(0.8f)
        , preferredMode(InferenceMode::GPU_Primary), userData(nullptr)
    {}
};

// ============================================================================
// ITTPassResult — Output from a single inference pass
// ============================================================================
struct ITTPassResult {
    // Pass metadata
    uint32_t        passNumber;
    SessionPhase    phase;

    // Generated output
    char            text[4096];
    uint32_t        textLen;
    uint32_t*       tokenIds;        // Allocated by engine, freed on next pass
    uint32_t        tokenCount;

    // Quality metrics
    float           outputEntropy;
    float           compositeDelta;  // vs previous pass
    float           qualityEstimate;

    // Performance metrics
    double          passTimeMs;
    double          tokensPerSecond;
    uint32_t        layersExecuted;
    uint32_t        layersCached;
    uint32_t        layersSkipped;

    // Strategy info
    TraversalMode   modeUsed;
    float           skipRatioUsed;
    uint32_t        contextLengthUsed;

    // Convergence info
    ConvergenceState convergenceState;
    bool             isConverged;
};

// ============================================================================
// ITTSessionResult — Final result of a complete iterative session
// ============================================================================
struct ITTSessionResult {
    bool            success;
    StopReason      stopReason;
    const char*     description;

    // Final output (best result from all passes)
    std::string     finalText;
    std::vector<uint32_t> finalTokenIds;

    // Session summary
    uint32_t        totalPasses;
    uint32_t        totalTokensGenerated;
    double          totalTimeMs;
    double          averageTPS;
    float           finalQuality;

    // Per-pass history
    std::vector<ITTPassResult> passHistory;

    // Layer utilization summary
    struct LayerSummary {
        uint32_t    layerIndex;
        uint32_t    timesExecuted;
        uint32_t    timesCached;
        uint32_t    timesSkipped;
        float       averageContribution;
    };
    std::vector<LayerSummary> layerSummaries;

    static ITTSessionResult ok(const char* desc) {
        ITTSessionResult r;
        r.success     = true;
        r.stopReason  = StopReason::QualityConverged;
        r.description = desc;
        r.totalPasses = 0;
        r.totalTokensGenerated = 0;
        r.totalTimeMs = 0;
        r.averageTPS  = 0;
        r.finalQuality = 0;
        return r;
    }

    static ITTSessionResult error(StopReason reason, const char* desc) {
        ITTSessionResult r;
        r.success     = false;
        r.stopReason  = reason;
        r.description = desc;
        r.totalPasses = 0;
        r.totalTokensGenerated = 0;
        r.totalTimeMs = 0;
        r.averageTPS  = 0;
        r.finalQuality = 0;
        return r;
    }
};

// ============================================================================
// ITT Callbacks (function pointers, NOT std::function)
// ============================================================================

// Called after each pass completes
typedef void (*ITTPassCallback)(
    const ITTPassResult* result,
    void*                userData
);

// Called when a phase transition occurs
typedef void (*ITTPhaseCallback)(
    SessionPhase oldPhase,
    SessionPhase newPhase,
    uint32_t     passNumber,
    void*        userData
);

// Called when the strategy adapts
typedef void (*ITTAdaptCallback)(
    const StrategyAdaptation* adaptation,
    void*                     userData
);

// Called to execute a single inference pass (the actual model forward pass)
// This is the integration point with the actual model backend.
// The engine calls this with a traversal plan; the backend executes it.
typedef PatchResult (*ITTInferenceBackend)(
    const ITTRequest*      request,
    const TraversalPlan*   plan,
    ITTPassResult*         outResult,
    void*                  backendUserData
);

// ============================================================================
// ITTConfig — Configuration for the iterative engine
// ============================================================================
struct ITTConfig {
    // Sub-system configs
    TraversalStrategyConfig     traversalConfig;
    ScorerConfig                scorerConfig;
    DeltaTrackerConfig          deltaConfig;
    CacheConfig                 cacheConfig;
    ControllerConfig            controllerConfig;

    // Model info
    uint32_t                    totalModelLayers     = 80;
    uint32_t                    hiddenDimension      = 8192;
    uint32_t                    numAttentionHeads    = 64;
    uint32_t                    numKVHeads           = 8;
    uint64_t                    modelSizeBytes       = 0;

    // Logging
    bool                        verboseLogging       = false;
    bool                        recordAllPasses      = true;

    // Backend
    ITTInferenceBackend         inferenceBackend     = nullptr;
    void*                       backendUserData      = nullptr;
};

// ============================================================================
// IterativeTensorTraversal — Main Engine (singleton)
// ============================================================================
class IterativeTensorTraversal {
public:
    static IterativeTensorTraversal& instance();

    // ═══════════════════════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════════════════════
    PatchResult initialize(const ITTConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ═══════════════════════════════════════════════════════════════════
    // Inference Sessions
    // ═══════════════════════════════════════════════════════════════════

    // Run a full iterative inference session.
    // This is the main entry point: give it a prompt, get a result.
    // Internally runs the Probe → Emit → Measure → Score → Cache →
    // Decide → Adapt → Re-enter loop until convergence or budget.
    PatchResult runSession(const ITTRequest& request, ITTSessionResult* outResult);

    // Run a single pass (for manual control)
    PatchResult runSinglePass(const ITTRequest& request,
                               const TraversalPlan& plan,
                               ITTPassResult* outResult);

    // Abort the current session (from another thread)
    PatchResult abortSession();

    // ═══════════════════════════════════════════════════════════════════
    // Session Queries
    // ═══════════════════════════════════════════════════════════════════
    bool          isSessionActive() const;
    SessionPhase  currentPhase() const;
    uint32_t      currentPassNumber() const;
    float         currentQuality() const;

    // Get the best result so far (even if session is still running)
    PatchResult getBestResult(ITTPassResult* outResult) const;

    // ═══════════════════════════════════════════════════════════════════
    // Callbacks
    // ═══════════════════════════════════════════════════════════════════
    PatchResult setPassCallback(ITTPassCallback cb, void* userData);
    PatchResult setPhaseCallback(ITTPhaseCallback cb, void* userData);
    PatchResult setAdaptCallback(ITTAdaptCallback cb, void* userData);

    // ═══════════════════════════════════════════════════════════════════
    // Backend Configuration
    // ═══════════════════════════════════════════════════════════════════
    PatchResult setInferenceBackend(ITTInferenceBackend backend, void* userData);

    // ═══════════════════════════════════════════════════════════════════
    // Sub-system Access (for advanced use / debugging)
    // ═══════════════════════════════════════════════════════════════════
    TraversalStrategy&        getTraversalStrategy();
    LayerContributionScorer&  getLayerScorer();
    SemanticDeltaTracker&     getDeltaTracker();
    CrossRunTensorCache&      getTensorCache();
    ConvergenceController&    getConvergenceController();

    // ═══════════════════════════════════════════════════════════════════
    // Hotpatch Integration
    // ═══════════════════════════════════════════════════════════════════

    // Live-patch a generation parameter mid-session
    PatchResult hotpatchParameter(ClampTarget target, float newValue);

    // Live-switch traversal mode mid-session
    PatchResult hotpatchTraversalMode(TraversalMode mode);

    // Live-adjust skip ratio mid-session
    PatchResult hotpatchSkipRatio(float ratio);

    // Live-protect/unprotect a layer mid-session
    PatchResult hotpatchProtectLayer(uint32_t layerIndex);
    PatchResult hotpatchUnprotectLayer(uint32_t layerIndex);

    // ═══════════════════════════════════════════════════════════════════
    // Statistics
    // ═══════════════════════════════════════════════════════════════════
    struct Stats {
        std::atomic<uint64_t> sessionsRun{0};
        std::atomic<uint64_t> sessionsConverged{0};
        std::atomic<uint64_t> sessionsTimedOut{0};
        std::atomic<uint64_t> sessionsAborted{0};
        std::atomic<uint64_t> sessionsFailed{0};
        std::atomic<uint64_t> totalPasses{0};
        std::atomic<uint64_t> totalTokensGenerated{0};
        std::atomic<uint64_t> totalLayerExecutions{0};
        std::atomic<uint64_t> totalLayerCacheHits{0};
        std::atomic<uint64_t> totalLayerSkips{0};
        std::atomic<uint64_t> totalAdaptations{0};
        std::atomic<uint64_t> totalHotpatches{0};
        double                bestTPS{0};
        double                worstTPS{999999.0};
        double                averageTPS{0};
        double                averagePassesPerSession{0};
        double                averageQuality{0};
    };

    const Stats& getStats() const { return m_stats; }
    void resetStats();

    // JSON export for diagnostics
    std::string toJson() const;
    std::string sessionSummaryToJson(const ITTSessionResult& result) const;

private:
    IterativeTensorTraversal();
    ~IterativeTensorTraversal();
    IterativeTensorTraversal(const IterativeTensorTraversal&) = delete;
    IterativeTensorTraversal& operator=(const IterativeTensorTraversal&) = delete;

    // ═══════════════════════════════════════════════════════════════════
    // Internal Loop Phases
    // ═══════════════════════════════════════════════════════════════════
    PatchResult phaseProbe(const ITTRequest& request, ITTPassResult* outResult);
    PatchResult phaseEmit(const ITTPassResult& passResult);
    PatchResult phaseMeasure(const ITTPassResult& passResult, DeltaMeasurement* outDelta);
    PatchResult phaseScore(const ITTPassResult& passResult);
    PatchResult phaseCache(const ITTPassResult& passResult);
    PatchResult phaseDecide(const HardwareFeedback& hw, const DeltaMeasurement* delta,
                             PassDecision* outDecision);
    PatchResult phaseAdapt(const PassDecision& decision, const HardwareFeedback& hw);
    PatchResult phaseReEnter(const PassDecision& decision, TraversalPlan* outPlan);

    // ═══════════════════════════════════════════════════════════════════
    // Internal Helpers
    // ═══════════════════════════════════════════════════════════════════
    PatchResult executeInferencePass(const ITTRequest& request,
                                      const TraversalPlan& plan,
                                      ITTPassResult* outResult);
    HardwareFeedback sampleHardware() const;
    uint64_t computeContextHash(const ITTRequest& request) const;
    uint64_t computeStrategyHash(const TraversalPlan& plan) const;
    void setPhase(SessionPhase phase);
    void selectBestResult(const std::vector<ITTPassResult>& passes,
                           ITTSessionResult* outSession);
    void buildLayerSummaries(const std::vector<ITTPassResult>& passes,
                              ITTSessionResult* outSession);
    void updateStats(const ITTSessionResult& result);
    void log(const char* fmt, ...);

    // ═══════════════════════════════════════════════════════════════════
    // Members
    // ═══════════════════════════════════════════════════════════════════
    std::atomic<bool>              m_initialized{false};
    mutable std::mutex             m_mutex;

    ITTConfig                      m_config;

    // Session state
    std::atomic<SessionPhase>      m_phase{SessionPhase::Idle};
    std::atomic<bool>              m_abortRequested{false};
    uint32_t                       m_passNumber;
    float                          m_bestQuality;
    int                            m_bestPassIndex;

    // Current traversal plan
    TraversalPlan                  m_currentPlan;

    // Callbacks
    ITTPassCallback                m_passCallback;
    void*                          m_passCallbackUserData;
    ITTPhaseCallback               m_phaseCallback;
    void*                          m_phaseCallbackUserData;
    ITTAdaptCallback               m_adaptCallback;
    void*                          m_adaptCallbackUserData;

    Stats                          m_stats;
};

#endif // ITERATIVE_TENSOR_TRAVERSAL_H
