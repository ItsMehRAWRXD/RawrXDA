// ============================================================================
// convergence_controller.h — Convergence Controller for Iterative Inference
// ============================================================================
// The convergence controller is the "brain" that decides when to stop the
// iterative inference loop. It integrates signals from:
//   - SemanticDeltaTracker (are outputs still changing?)
//   - LayerContributionScorer (are we learning anything new?)
//   - HardwareFeedback (can the hardware keep going?)
//   - User constraints (time budget, quality threshold, interrupts)
//
// The controller implements multiple stopping criteria:
//   1. Quality convergence — outputs stopped improving
//   2. Time budget — wall clock limit reached
//   3. Token budget — maximum tokens generated
//   4. Pass budget — maximum inference passes
//   5. Resource exhaustion — system running out of memory/GPU
//   6. User interrupt — external stop signal
//   7. Diminishing returns — rate of improvement below threshold
//
// The controller also decides the INTENSITY of each pass:
//   - Should next pass be a lightweight probe or a deep traversal?
//   - Should we widen context or deepen layer coverage?
//   - Should we trade precision for speed?
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - No std::function — raw function pointers for callbacks
//   - Thread-safe singleton
//   - Deterministic — same inputs produce same decisions
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef CONVERGENCE_CONTROLLER_H
#define CONVERGENCE_CONTROLLER_H

#include "model_memory_hotpatch.hpp"  // PatchResult
#include "semantic_delta_tracker.h"   // ConvergenceState, DeltaMeasurement
#include "traversal_strategy.h"       // HardwareFeedback, TraversalMode
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>

// ============================================================================
// Enums
// ============================================================================

// StopReason — Why the controller decided to stop
enum class StopReason : uint8_t {
    NotStopped          = 0,   // Still running
    QualityConverged    = 1,   // Outputs converged
    TimeBudgetExpired   = 2,   // Wall clock limit
    TokenBudgetExpired  = 3,   // Max tokens generated
    PassBudgetExpired   = 4,   // Max passes reached
    ResourceExhausted   = 5,   // Out of memory / GPU
    UserInterrupt       = 6,   // External stop signal
    DiminishingReturns  = 7,   // Improvement rate too low
    CriticalFailure     = 8    // Unrecoverable error
};

// PassIntensity — How much work to do on the next pass
enum class PassIntensity : uint8_t {
    Probe       = 0,   // Minimal: small context, few layers, fast check
    Light       = 1,   // Light traversal: partial layers, short context
    Standard    = 2,   // Normal traversal: balanced coverage
    Deep        = 3,   // Deep traversal: most layers, longer context
    Full        = 4    // Full traversal: all layers, max context
};

// ============================================================================
// PassDecision — The controller's recommendation for the next pass
// ============================================================================
struct PassDecision {
    // Should we continue running?
    bool            shouldContinue;

    // If stopping, why?
    StopReason      stopReason;

    // Recommended intensity for next pass
    PassIntensity   intensity;

    // Recommended traversal mode for next pass
    TraversalMode   recommendedMode;

    // Recommended context length for next pass
    uint32_t        recommendedContextLength;

    // Recommended token budget for next pass
    uint32_t        recommendedTokenBudget;

    // Recommended skip ratio for next pass (0.0-1.0)
    float           recommendedSkipRatio;

    // Confidence in this decision (0.0-1.0)
    float           confidence;

    // Pass number this decision is for
    uint32_t        forPassNumber;

    // Estimated remaining passes to convergence (0 if unknown)
    uint32_t        estimatedRemainingPasses;

    // Current quality estimate (0.0 = garbage, 1.0 = converged)
    float           qualityEstimate;

    // Reason text
    const char*     description;

    static PassDecision continueWith(PassIntensity intensity, const char* desc) {
        PassDecision d;
        d.shouldContinue      = true;
        d.stopReason          = StopReason::NotStopped;
        d.intensity           = intensity;
        d.recommendedMode     = TraversalMode::BunnyHop;
        d.recommendedContextLength = 0;
        d.recommendedTokenBudget   = 0;
        d.recommendedSkipRatio     = 0.0f;
        d.confidence          = 0.5f;
        d.forPassNumber       = 0;
        d.estimatedRemainingPasses = 0;
        d.qualityEstimate     = 0.0f;
        d.description         = desc;
        return d;
    }

    static PassDecision stop(StopReason reason, float quality, const char* desc) {
        PassDecision d;
        d.shouldContinue      = false;
        d.stopReason          = reason;
        d.intensity           = PassIntensity::Probe;
        d.recommendedMode     = TraversalMode::Full;
        d.recommendedContextLength = 0;
        d.recommendedTokenBudget   = 0;
        d.recommendedSkipRatio     = 0.0f;
        d.confidence          = 1.0f;
        d.forPassNumber       = 0;
        d.estimatedRemainingPasses = 0;
        d.qualityEstimate     = quality;
        d.description         = desc;
        return d;
    }
};

// ============================================================================
// ControllerConfig — Configuration for the convergence controller
// ============================================================================
struct ControllerConfig {
    // ----- Time Budget -----
    double      timeBudgetMs             = 30000.0;  // 30 seconds default
    double      timeBudgetWarningMs      = 25000.0;  // Warn at 25s

    // ----- Token Budget -----
    uint32_t    maxTotalTokens           = 4096;

    // ----- Pass Budget -----
    uint32_t    maxPasses                = 50;

    // ----- Quality Thresholds -----
    float       convergenceThreshold     = 0.08f;  // Match SemanticDeltaTracker
    float       diminishingReturnRate    = 0.01f;  // < 1% improvement = stop
    uint32_t    diminishingReturnWindow  = 3;      // Check over N passes

    // ----- Resource Thresholds -----
    double      maxMemoryBytes           = 0;      // 0 = no limit
    double      maxVramBytes             = 0;      // 0 = no limit
    double      memoryPressureLimit      = 0.95;   // 95% = stop

    // ----- Intensity Scaling -----
    // How aggressively to scale intensity based on convergence rate
    float       intensityScalingFactor   = 1.0f;

    // Start with probe passes for the first N passes
    uint32_t    probePassCount           = 2;

    // ----- Pass Intensity Profiles -----
    // These define what each intensity level means in concrete terms
    struct IntensityProfile {
        float    skipRatio;       // Layer skip ratio
        uint32_t contextLength;   // Context window
        uint32_t tokenBudget;     // Max tokens this pass
    };

    IntensityProfile profiles[5] = {
        { 0.7f,  256,  64 },   // Probe
        { 0.5f,  512, 128 },   // Light
        { 0.3f, 1024, 256 },   // Standard
        { 0.1f, 2048, 512 },   // Deep
        { 0.0f, 4096, 1024 }   // Full
    };
};

// ============================================================================
// Convergence controller callback (function pointer, NOT std::function)
// ============================================================================
typedef void (*ControllerCallback)(
    const PassDecision* decision,
    void*               userData
);

// ============================================================================
// ConvergenceController — Main class (singleton)
// ============================================================================
class ConvergenceController {
public:
    static ConvergenceController& instance();

    // ----- Lifecycle -----
    PatchResult initialize(const ControllerConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- Core Decision Making -----
    // Given current state, decide what to do next.
    // This is the main entry point called by IterativeTensorTraversal each pass.
    PatchResult decide(
        const HardwareFeedback&   hardware,
        const DeltaMeasurement*   lastDelta,    // null if first pass
        ConvergenceState          convergenceState,
        uint32_t                  currentPass,
        uint32_t                  totalTokensGenerated,
        PassDecision*             outDecision
    );

    // ----- Signal Injection -----
    // External signals that influence the next decision
    PatchResult signalUserInterrupt();
    PatchResult signalResourceWarning(double memUsed, double memTotal);
    PatchResult signalDecodeFailed();
    PatchResult signalQualityDrop(float delta);

    // ----- Budget Management -----
    PatchResult setTimeBudget(double ms);
    PatchResult setTokenBudget(uint32_t tokens);
    PatchResult setPassBudget(uint32_t passes);
    double      remainingTimeMs() const;
    uint32_t    remainingTokens(uint32_t generated) const;
    uint32_t    remainingPasses(uint32_t current) const;

    // ----- Session Management -----
    // Start a new inference session (resets timers and counters)
    PatchResult beginSession();

    // End the current session
    PatchResult endSession();

    // ----- Queries -----
    bool        isSessionActive() const;
    uint32_t    currentPass() const;
    StopReason  lastStopReason() const;
    float       currentQualityEstimate() const;

    // ----- Callback -----
    PatchResult registerCallback(ControllerCallback cb, void* userData);
    PatchResult clearCallback();

    // ----- Statistics -----
    struct Stats {
        std::atomic<uint64_t> sessionsStarted{0};
        std::atomic<uint64_t> sessionsCompleted{0};
        std::atomic<uint64_t> passDecisions{0};
        std::atomic<uint64_t> continueDecisions{0};
        std::atomic<uint64_t> stopDecisions{0};
        std::atomic<uint64_t> probeDecisions{0};
        std::atomic<uint64_t> deepDecisions{0};
        std::atomic<uint64_t> userInterrupts{0};
        std::atomic<uint64_t> timeBudgetExpiries{0};
        std::atomic<uint64_t> resourceExhaustions{0};
    };

    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    ConvergenceController();
    ~ConvergenceController();
    ConvergenceController(const ConvergenceController&) = delete;
    ConvergenceController& operator=(const ConvergenceController&) = delete;

    // ----- Internal decision logic -----
    PassIntensity computeIntensity(uint32_t pass, float convergenceRate,
                                    const HardwareFeedback& hw) const;
    float computeQualityEstimate(ConvergenceState state,
                                  const DeltaMeasurement* delta) const;
    float computeConvergenceRate() const;
    bool checkDiminishingReturns() const;
    StopReason checkStopConditions(
        const HardwareFeedback& hw, ConvergenceState state,
        uint32_t pass, uint32_t tokens) const;
    uint32_t estimateRemainingPasses(float convergenceRate) const;
    void notifyCallback(const PassDecision& decision);

    // ----- Members -----
    std::atomic<bool>              m_initialized{false};
    mutable std::mutex             m_mutex;

    ControllerConfig               m_config;

    // Session state
    bool                           m_sessionActive;
    uint64_t                       m_sessionStartMs;
    uint32_t                       m_currentPass;
    StopReason                     m_lastStopReason;
    float                          m_qualityEstimate;

    // Signal flags
    std::atomic<bool>              m_userInterrupt{false};
    std::atomic<bool>              m_resourceWarning{false};
    std::atomic<bool>              m_decodeFailed{false};

    // Quality history (for diminishing returns detection)
    static constexpr size_t QUALITY_HISTORY_SIZE = 64;
    float                          m_qualityHistory[QUALITY_HISTORY_SIZE];
    uint32_t                       m_qualityHistoryCount;

    // Callback
    ControllerCallback             m_callback;
    void*                          m_callbackUserData;

    Stats                          m_stats;
};

#endif // CONVERGENCE_CONTROLLER_H
