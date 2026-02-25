// ============================================================================
// reasoning_profile.h — Tunable Reasoning Pipeline Profile System
// ============================================================================
//
// Fully adjustable "P-settings" for the multi-agent reasoning pipeline.
// Controls reasoning depth, visibility, agent participation, adaptive
// behavior, thermal throttling, confidence-based auto-depth, swarm mode,
// and autonomous self-tuning.
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - No std::function in hot paths — raw function pointers for callbacks
//   - Thread-safe via std::mutex
//   - Serializable to/from JSON (manual serializer)
//   - All tunables are bounded with min/max validation
//
// Integration:
//   - ChainOfThoughtEngine (Phase 32A)
//   - ConfidenceGate (Phase 10D)
//   - SwarmCoordinator (Phase 11A)
//   - ConvergenceController
//   - AgenticFailureDetector / AgenticPuppeteer
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_REASONING_PROFILE_H
#define RAWRXD_REASONING_PROFILE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
struct PatchResult;

// ============================================================================
// ENUMS — Reasoning Pipeline Control
// ============================================================================

// ReasoningMode — top-level operating mode
enum class ReasoningMode : uint8_t {
    Fast        = 0,  // Bypass pipeline, direct LLM response
    Normal      = 1,  // Single reasoning pass
    Deep        = 2,  // 2–3 reasoning passes
    Critical    = 3,  // Full critic + auditor + synthesizer chain
    Swarm       = 4,  // Multi-agent distributed reasoning
    Adaptive    = 5,  // Auto-adjusts based on latency/confidence
    DevDebug    = 6,  // Deep mode + exposed CoT + all diagnostics
    Count       = 7
};

// ReasoningVisibility — what the user sees
enum class ReasoningVisibility : uint8_t {
    FinalOnly       = 0,  // Only show final synthesized answer
    ProgressBar     = 1,  // Show "Processing..." with step count
    StepSummary     = 2,  // Show brief per-step summaries
    FullCoT         = 3,  // Expose entire chain-of-thought (dev mode)
    Count           = 4
};

// AdaptiveStrategy — how auto-mode adjusts depth
enum class AdaptiveStrategy : uint8_t {
    LatencyAware    = 0,  // Reduce depth under high latency
    ThermalAware    = 1,  // Reduce depth under thermal load
    ConfidenceBased = 2,  // Increase depth when confidence is low
    Hybrid          = 3,  // Combined latency + thermal + confidence
    Count           = 4
};

// ThermalState — current system thermal pressure
enum class ThermalState : uint8_t {
    Cool        = 0,  // System idle, full reasoning available
    Warm        = 1,  // Moderate load, slight throttle possible
    Hot         = 2,  // High load, reduce reasoning passes
    Critical    = 3,  // Thermal emergency — fast mode forced
    Count       = 4
};

// SwarmReasoningMode — how swarm distributes reasoning
enum class SwarmReasoningMode : uint8_t {
    ParallelVote    = 0,  // All agents reason in parallel, majority vote
    Sequential      = 1,  // Chain agents sequentially (standard CoT)
    Tournament      = 2,  // Bracket-style elimination of worst responses
    Ensemble        = 3,  // Weighted ensemble of all responses
    Count           = 4
};

// SelfTuneObjective — what the self-tuner optimizes for
enum class SelfTuneObjective : uint8_t {
    MinLatency      = 0,  // Minimize response time
    MaxQuality      = 1,  // Maximize answer quality
    BalancedQoS     = 2,  // Balance latency vs quality (Pareto)
    MinCost         = 3,  // Minimize token/compute cost
    MaxThroughput   = 4,  // Maximize requests per second
    Count           = 5
};

// InputComplexity — complexity classification of the input
enum class InputComplexity : uint8_t {
    Trivial     = 0,  // Greetings, single-word, yes/no
    Simple      = 1,  // One-step factual questions
    Moderate    = 2,  // Multi-part questions, code tasks
    Complex     = 3,  // Multi-step reasoning, architecture design
    Expert      = 4,  // Cross-domain synthesis, novel research
    Count       = 5
};

// ============================================================================
// STRUCTS — Tunable Parameters
// ============================================================================

// Core reasoning parameters (the "P-settings" dial)
struct ReasoningParams {
    // --- Depth Control ---
    int             reasoningDepth;         // 0–8: number of reasoning passes
    ReasoningMode   mode;                   // Operating mode
    ReasoningVisibility visibility;         // What user sees

    // --- Agent Selection ---
    bool            enableCritic;           // Use Critic agent
    bool            enableAuditor;          // Use Auditor agent
    bool            enableThinker;          // Use Thinker agent
    bool            enableResearcher;       // Use Researcher agent
    bool            enableDebaters;         // Use Debater pair
    bool            enableVerifier;         // Use Verifier agent
    bool            enableRefiner;          // Use Refiner agent
    bool            enableSynthesizer;      // Use Synthesizer (always recommended)
    bool            enableBrainstorm;       // Use Brainstormer
    bool            enableSummarizer;       // Use Summarizer

    // --- Quality Control ---
    float           confidenceThreshold;    // 0.0–1.0: min confidence for auto-accept
    float           qualityFloor;           // 0.0–1.0: reject below this quality
    bool            requireFinalAnswer;     // Enforce non-empty final response
    bool            allowFallback;          // Fallback to last step if final empty

    // --- Routing ---
    bool            autoBypassSimple;       // Auto-bypass pipeline for trivial input
    int             simpleInputMaxLen;      // Max char length for "simple" classification

    // --- Visibility ---
    bool            exposeChainOfThought;   // Show internal reasoning to user
    bool            exposeStepTimings;      // Show per-step latency
    bool            exposeConfidence;       // Show confidence scores

    // Factory defaults
    ReasoningParams()
        : reasoningDepth(1),
          mode(ReasoningMode::Normal),
          visibility(ReasoningVisibility::ProgressBar),
          enableCritic(true),
          enableAuditor(false),
          enableThinker(true),
          enableResearcher(false),
          enableDebaters(false),
          enableVerifier(false),
          enableRefiner(false),
          enableSynthesizer(true),
          enableBrainstorm(false),
          enableSummarizer(false),
          confidenceThreshold(0.7f),
          qualityFloor(0.3f),
          requireFinalAnswer(true),
          allowFallback(true),
          autoBypassSimple(true),
          simpleInputMaxLen(12),
          exposeChainOfThought(false),
          exposeStepTimings(false),
          exposeConfidence(false) {}
};

// Adaptive mode parameters
struct AdaptiveParams {
    bool                enabled;                // Master switch for adaptive mode
    AdaptiveStrategy    strategy;               // Which strategy to use

    // Latency-aware settings
    double              latencyTargetMs;        // Target response latency
    double              latencyMaxMs;           // Hard ceiling — reduce depth above this
    double              latencySmoothingAlpha;   // EWMA alpha (0.0–1.0)
    int                 depthReductionPerStep;  // How many steps to cut when over budget

    // Confidence auto-depth settings
    float               lowConfidenceThreshold; // Below this → add more passes
    float               highConfidenceThreshold;// Above this → reduce passes
    int                 minAdaptiveDepth;       // Floor for auto-adjusted depth
    int                 maxAdaptiveDepth;       // Ceiling for auto-adjusted depth

    AdaptiveParams()
        : enabled(false),
          strategy(AdaptiveStrategy::Hybrid),
          latencyTargetMs(2000.0),
          latencyMaxMs(10000.0),
          latencySmoothingAlpha(0.3),
          depthReductionPerStep(1),
          lowConfidenceThreshold(0.4f),
          highConfidenceThreshold(0.85f),
          minAdaptiveDepth(0),
          maxAdaptiveDepth(6) {}
};

// Thermal-aware parameters
struct ThermalParams {
    bool            enabled;                    // Master switch
    double          pollIntervalMs;             // How often to check thermals
    float           warmThreshold;              // CPU usage % → Warm state
    float           hotThreshold;               // CPU usage % → Hot state
    float           criticalThreshold;          // CPU usage % → Critical state
    int             depthAtWarm;                // Max depth when Warm
    int             depthAtHot;                 // Max depth when Hot
    int             depthAtCritical;            // Max depth when Critical (usually 0)
    bool            forceBypassInCritical;      // Force Fast mode in Critical

    ThermalParams()
        : enabled(false),
          pollIntervalMs(1000.0),
          warmThreshold(60.0f),
          hotThreshold(80.0f),
          criticalThreshold(95.0f),
          depthAtWarm(4),
          depthAtHot(2),
          depthAtCritical(0),
          forceBypassInCritical(true) {}
};

// Swarm reasoning parameters
struct SwarmReasoningParams {
    bool                    enabled;            // Master switch
    SwarmReasoningMode      mode;               // Distribution strategy
    int                     agentCount;         // Number of parallel agents (2–16)
    float                   voteThreshold;      // Majority threshold for ParallelVote
    int                     tournamentRounds;   // Rounds for Tournament mode
    bool                    heterogeneous;      // Use different models per agent
    std::vector<std::string> models;            // Model pool for heterogeneous mode
    float                   ensembleWeightDecay;// Weight decay for Ensemble mode
    int                     timeoutPerAgentMs;  // Per-agent timeout

    SwarmReasoningParams()
        : enabled(false),
          mode(SwarmReasoningMode::ParallelVote),
          agentCount(3),
          voteThreshold(0.5f),
          tournamentRounds(2),
          heterogeneous(false),
          ensembleWeightDecay(0.9f),
          timeoutPerAgentMs(5000) {}
};

// Self-tuning controller parameters
struct SelfTuneParams {
    bool                enabled;                // Master switch
    SelfTuneObjective   objective;              // What to optimize
    int                 windowSize;             // Rolling window of observations
    double              learningRate;           // PID-like learning rate
    double              explorationRate;        // Fraction of requests used to explore
    bool                persistTuning;          // Save learned params to disk
    std::string         persistPath;            // File path for persistence
    int                 minSamplesBeforeTune;   // Minimum observations before adjusting
    double              qualityDecayAlpha;      // EWMA for quality tracking
    double              latencyDecayAlpha;      // EWMA for latency tracking

    SelfTuneParams()
        : enabled(false),
          objective(SelfTuneObjective::BalancedQoS),
          windowSize(100),
          learningRate(0.1),
          explorationRate(0.05),
          persistTuning(true),
          persistPath("reasoning_tuning.json"),
          minSamplesBeforeTune(20),
          qualityDecayAlpha(0.2),
          latencyDecayAlpha(0.3) {}
};

// ============================================================================
// ReasoningProfile — The complete profile (all P-settings combined)
// ============================================================================
struct ReasoningProfile {
    std::string             name;           // Profile name (e.g. "fast", "deep", "dev")
    ReasoningParams         reasoning;      // Core reasoning settings
    AdaptiveParams          adaptive;       // Adaptive mode settings
    ThermalParams           thermal;        // Thermal-aware settings
    SwarmReasoningParams    swarm;          // Swarm mode settings
    SelfTuneParams          selfTune;       // Self-tuning controller settings
};

// ============================================================================
// ReasoningTelemetry — per-request telemetry
// ============================================================================
struct ReasoningTelemetry {
    uint64_t    requestId;
    double      totalLatencyMs;
    int         stepsExecuted;
    int         stepsSkipped;
    float       finalConfidence;
    float       qualityEstimate;
    int         effectiveDepth;         // Actual depth after adaptive adjustment
    ReasoningMode effectiveMode;        // Actual mode after routing
    ThermalState  thermalAtStart;       // Thermal state when request began
    InputComplexity inputComplexity;    // Classified complexity of input
    bool        wasAutoBypass;          // Was the pipeline bypassed?
    bool        wasAdaptiveAdjusted;    // Was depth auto-adjusted?
    bool        wasThermalThrottled;    // Was reasoning throttled by thermals?
    bool        usedSwarm;              // Was swarm reasoning used?
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
};

// ============================================================================
// SelfTuneObservation — one datapoint for the self-tuning controller
// ============================================================================
struct SelfTuneObservation {
    uint64_t        requestId;
    double          latencyMs;
    float           qualityScore;       // 0.0–1.0 estimated quality
    float           confidence;         // Final confidence
    int             depthUsed;
    ReasoningMode   modeUsed;
    InputComplexity complexity;
    bool            userAccepted;       // Did user accept the answer?
    bool            userEdited;         // Did user edit the answer?
    double          timestampEpoch;     // Wall clock time
};

// ============================================================================
// SelfTuneState — internal state of the self-tuning controller
// ============================================================================
struct SelfTuneState {
    double          ewmaLatency;        // Exponentially weighted avg latency
    double          ewmaQuality;        // Exponentially weighted avg quality
    int             currentOptimalDepth;// Controller's current depth recommendation
    uint64_t        totalObservations;
    uint64_t        explorationCount;
    float           depthQualityMap[9]; // Estimated quality for depth 0–8
    float           depthLatencyMap[9]; // Estimated latency for depth 0–8
    uint64_t        depthUsageCount[9]; // How many times each depth was used

    SelfTuneState() : ewmaLatency(0), ewmaQuality(0),
                      currentOptimalDepth(1), totalObservations(0),
                      explorationCount(0) {
        for (int i = 0; i < 9; ++i) {
            depthQualityMap[i] = 0.5f;
            depthLatencyMap[i] = 1000.0f * (i + 1);
            depthUsageCount[i] = 0;
        }
    }
};

// ============================================================================
// Callbacks — function pointer types (no std::function in hot paths)
// ============================================================================

// Called when reasoning profile changes
typedef void (*ReasoningProfileChangeCallback)(const ReasoningProfile& profile, void* userData);

// Called when thermal state changes
typedef void (*ThermalStateChangeCallback)(ThermalState newState, ThermalState oldState, void* userData);

// Called when self-tuner adjusts depth
typedef void (*SelfTuneAdjustCallback)(int oldDepth, int newDepth, const char* reason, void* userData);

// Called per reasoning step (for progress/visibility)
typedef void (*ReasoningStepProgressCallback)(int stepIndex, int totalSteps,
                                              const char* roleName, const char* status,
                                              void* userData);

// ============================================================================
// BUILT-IN PROFILE PRESETS
// ============================================================================

// Returns a named preset profile. Returns nullptr if name not found.
const ReasoningProfile* getReasoningPreset(const char* name);

// Get names of all built-in presets
std::vector<std::string> getReasoningPresetNames();

// ============================================================================
// ReasoningProfileManager — singleton that manages the active profile
// ============================================================================
class ReasoningProfileManager {
public:
    static ReasoningProfileManager& instance();

    // --- Profile Management ---
    void setProfile(const ReasoningProfile& profile);
    ReasoningProfile getProfile() const;
    void applyPreset(const char* presetName);
    std::string getActivePresetName() const;

    // --- Quick Adjusters (the "P dial") ---
    void setReasoningDepth(int depth);           // 0–8
    int  getReasoningDepth() const;
    void setMode(ReasoningMode mode);
    ReasoningMode getMode() const;
    void setVisibility(ReasoningVisibility vis);
    ReasoningVisibility getVisibility() const;

    // --- Adaptive Controls ---
    void setAdaptiveEnabled(bool enabled);
    bool isAdaptiveEnabled() const;
    void setAdaptiveStrategy(AdaptiveStrategy strategy);
    void setLatencyTarget(double ms);
    void setLatencyMax(double ms);

    // --- Thermal Controls ---
    void setThermalEnabled(bool enabled);
    bool isThermalEnabled() const;
    void updateThermalState(ThermalState state);
    ThermalState getThermalState() const;

    // --- Swarm Controls ---
    void setSwarmEnabled(bool enabled);
    bool isSwarmEnabled() const;
    void setSwarmAgentCount(int count);
    void setSwarmMode(SwarmReasoningMode mode);

    // --- Self-Tune Controls ---
    void setSelfTuneEnabled(bool enabled);
    bool isSelfTuneEnabled() const;
    void setSelfTuneObjective(SelfTuneObjective objective);
    SelfTuneState getSelfTuneState() const;
    void feedObservation(const SelfTuneObservation& obs);
    void resetSelfTuneState();

    // --- Input Routing ---
    InputComplexity classifyInput(const std::string& input) const;
    bool shouldBypassPipeline(const std::string& input) const;
    int  computeEffectiveDepth(InputComplexity complexity) const;

    // --- Telemetry ---
    ReasoningTelemetry getLastTelemetry() const;
    std::vector<ReasoningTelemetry> getRecentTelemetry(int count) const;
    void recordTelemetry(const ReasoningTelemetry& t);

    // --- Callbacks ---
    void setProfileChangeCallback(ReasoningProfileChangeCallback cb, void* userData);
    void setThermalChangeCallback(ThermalStateChangeCallback cb, void* userData);
    void setSelfTuneCallback(SelfTuneAdjustCallback cb, void* userData);
    void setStepProgressCallback(ReasoningStepProgressCallback cb, void* userData);

    // --- Serialization ---
    std::string serializeProfileToJSON(const ReasoningProfile& profile) const;
    bool deserializeProfileFromJSON(const std::string& json, ReasoningProfile& out) const;
    bool saveProfileToFile(const std::string& path) const;
    bool loadProfileFromFile(const std::string& path);

    // --- Statistics ---
    struct ProfileStats {
        uint64_t    totalRequests;
        uint64_t    bypassedRequests;
        uint64_t    adaptiveAdjustments;
        uint64_t    thermalThrottles;
        uint64_t    swarmActivations;
        uint64_t    selfTuneAdjustments;
        double      avgLatencyMs;
        double      avgQuality;
        double      avgConfidence;
        int         avgEffectiveDepth;
    };

    ProfileStats getStats() const;
    void resetStats();

    // Allow the pipeline orchestrator's thermal monitor to access CPU/thermal internals
    friend class ReasoningPipelineOrchestrator;

private:
    ReasoningProfileManager();
    ~ReasoningProfileManager() = default;
    ReasoningProfileManager(const ReasoningProfileManager&) = delete;
    ReasoningProfileManager& operator=(const ReasoningProfileManager&) = delete;

    // Self-tuning PID loop
    void runSelfTuneStep(const SelfTuneObservation& obs);
    int  pidComputeOptimalDepth() const;

    // Thermal polling
    float readSystemCpuUsage() const;
    ThermalState classifyThermal(float cpuUsage) const;

    // Internal state
    mutable std::mutex              m_mutex;
    ReasoningProfile                m_profile;
    std::string                     m_activePresetName;

    // Thermal
    std::atomic<ThermalState>       m_thermalState{ThermalState::Cool};

    // Self-tuning
    SelfTuneState                   m_selfTuneState;
    std::vector<SelfTuneObservation> m_observations;

    // Telemetry
    std::vector<ReasoningTelemetry> m_telemetry;
    static constexpr size_t         kMaxTelemetry = 500;

    // Statistics
    ProfileStats                    m_stats{};

    // Callbacks
    ReasoningProfileChangeCallback  m_profileChangeCb  = nullptr;
    void*                           m_profileChangeCtx = nullptr;
    ThermalStateChangeCallback      m_thermalChangeCb  = nullptr;
    void*                           m_thermalChangeCtx = nullptr;
    SelfTuneAdjustCallback          m_selfTuneCb       = nullptr;
    void*                           m_selfTuneCtx      = nullptr;
    ReasoningStepProgressCallback   m_stepProgressCb   = nullptr;
    void*                           m_stepProgressCtx  = nullptr;
};

#endif // RAWRXD_REASONING_PROFILE_H
