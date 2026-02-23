// ============================================================================
// convergence_controller.cpp — Convergence Controller Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "convergence_controller.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>

// ============================================================================
// Singleton
// ============================================================================

ConvergenceController& ConvergenceController::instance() {
    static ConvergenceController s_instance;
    return s_instance;
}

ConvergenceController::ConvergenceController()
    : m_sessionActive(false)
    , m_sessionStartMs(0)
    , m_currentPass(0)
    , m_lastStopReason(StopReason::NotStopped)
    , m_qualityEstimate(0.0f)
    , m_qualityHistoryCount(0)
    , m_callback(nullptr)
    , m_callbackUserData(nullptr)
{
    std::memset(m_qualityHistory, 0, sizeof(m_qualityHistory));
}

ConvergenceController::~ConvergenceController() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult ConvergenceController::initialize(const ControllerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("ConvergenceController already initialized", -1);
    }

    m_config              = config;
    m_sessionActive       = false;
    m_sessionStartMs      = 0;
    m_currentPass         = 0;
    m_lastStopReason      = StopReason::NotStopped;
    m_qualityEstimate     = 0.0f;
    m_qualityHistoryCount = 0;

    m_userInterrupt.store(false, std::memory_order_release);
    m_resourceWarning.store(false, std::memory_order_release);
    m_decodeFailed.store(false, std::memory_order_release);

    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("ConvergenceController initialized");
}

void ConvergenceController::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized.store(false, std::memory_order_release);
    m_sessionActive = false;
    m_callback      = nullptr;
}

// ============================================================================
// Core Decision Making
// ============================================================================

PatchResult ConvergenceController::decide(
    const HardwareFeedback&   hardware,
    const DeltaMeasurement*   lastDelta,
    ConvergenceState          convergenceState,
    uint32_t                  currentPass,
    uint32_t                  totalTokensGenerated,
    PassDecision*             outDecision)
{
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outDecision) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    m_currentPass = currentPass;

    // -----------------------------------------------------------------------
    // 1. Check hard stop conditions
    // -----------------------------------------------------------------------
    StopReason stopReason = checkStopConditions(hardware, convergenceState,
                                                 currentPass, totalTokensGenerated);

    if (stopReason != StopReason::NotStopped) {
        float quality = computeQualityEstimate(convergenceState, lastDelta);
        *outDecision = PassDecision::stop(stopReason, quality, "Stop condition met");
        outDecision->forPassNumber = currentPass;
        m_lastStopReason   = stopReason;
        m_qualityEstimate  = quality;
        m_stats.stopDecisions.fetch_add(1, std::memory_order_relaxed);
        m_stats.passDecisions.fetch_add(1, std::memory_order_relaxed);

        // Track specific stop reasons
        switch (stopReason) {
            case StopReason::TimeBudgetExpired:
                m_stats.timeBudgetExpiries.fetch_add(1, std::memory_order_relaxed); break;
            case StopReason::UserInterrupt:
                m_stats.userInterrupts.fetch_add(1, std::memory_order_relaxed); break;
            case StopReason::ResourceExhausted:
                m_stats.resourceExhaustions.fetch_add(1, std::memory_order_relaxed); break;
            default: break;
        }

        notifyCallback(*outDecision);
        return PatchResult::ok("Decision: stop");
    }

    // -----------------------------------------------------------------------
    // 2. Record quality for diminishing returns detection
    // -----------------------------------------------------------------------
    float currentQuality = computeQualityEstimate(convergenceState, lastDelta);
    m_qualityEstimate    = currentQuality;

    if (m_qualityHistoryCount < QUALITY_HISTORY_SIZE) {
        m_qualityHistory[m_qualityHistoryCount++] = currentQuality;
    } else {
        // Shift left and append
        std::memmove(m_qualityHistory, m_qualityHistory + 1,
                      (QUALITY_HISTORY_SIZE - 1) * sizeof(float));
        m_qualityHistory[QUALITY_HISTORY_SIZE - 1] = currentQuality;
    }

    // -----------------------------------------------------------------------
    // 3. Check diminishing returns
    // -----------------------------------------------------------------------
    if (checkDiminishingReturns()) {
        *outDecision = PassDecision::stop(StopReason::DiminishingReturns, currentQuality,
                                           "Improvement rate below threshold");
        outDecision->forPassNumber = currentPass;
        m_lastStopReason = StopReason::DiminishingReturns;
        m_stats.stopDecisions.fetch_add(1, std::memory_order_relaxed);
        m_stats.passDecisions.fetch_add(1, std::memory_order_relaxed);
        notifyCallback(*outDecision);
        return PatchResult::ok("Decision: stop (diminishing returns)");
    }

    // -----------------------------------------------------------------------
    // 4. Determine next pass intensity
    // -----------------------------------------------------------------------
    float convergenceRate = computeConvergenceRate();
    PassIntensity intensity = computeIntensity(currentPass, convergenceRate, hardware);

    // -----------------------------------------------------------------------
    // 5. Build continue decision
    // -----------------------------------------------------------------------
    *outDecision = PassDecision::continueWith(intensity, "Continuing iterative inference");
    outDecision->forPassNumber = currentPass + 1;
    outDecision->qualityEstimate = currentQuality;
    outDecision->confidence = std::min(currentQuality + 0.2f, 1.0f);
    outDecision->estimatedRemainingPasses = estimateRemainingPasses(convergenceRate);

    // Fill recommended parameters from intensity profile
    if (static_cast<int>(intensity) < 5) {
        const auto& profile = m_config.profiles[static_cast<int>(intensity)];
        outDecision->recommendedSkipRatio     = profile.skipRatio;
        outDecision->recommendedContextLength = profile.contextLength;
        outDecision->recommendedTokenBudget   = profile.tokenBudget;
    }

    // Determine recommended mode based on convergence state
    switch (convergenceState) {
        case ConvergenceState::Diverging:
            outDecision->recommendedMode = TraversalMode::IterDeepening;
            break;
        case ConvergenceState::Converging:
            outDecision->recommendedMode = TraversalMode::SkipAdaptive;
            break;
        case ConvergenceState::Oscillating:
            outDecision->recommendedMode = TraversalMode::BunnyHop;
            break;
        default:
            outDecision->recommendedMode = TraversalMode::BunnyHop;
            break;
    }

    m_stats.continueDecisions.fetch_add(1, std::memory_order_relaxed);
    m_stats.passDecisions.fetch_add(1, std::memory_order_relaxed);

    if (intensity == PassIntensity::Probe) {
        m_stats.probeDecisions.fetch_add(1, std::memory_order_relaxed);
    } else if (intensity == PassIntensity::Deep || intensity == PassIntensity::Full) {
        m_stats.deepDecisions.fetch_add(1, std::memory_order_relaxed);
    }

    notifyCallback(*outDecision);
    return PatchResult::ok("Decision: continue");
}

// ============================================================================
// Internal Decision Logic
// ============================================================================

StopReason ConvergenceController::checkStopConditions(
    const HardwareFeedback& hw, ConvergenceState state,
    uint32_t pass, uint32_t tokens) const
{
    // User interrupt (highest priority)
    if (m_userInterrupt.load(std::memory_order_relaxed)) {
        return StopReason::UserInterrupt;
    }

    // Quality converged
    if (state == ConvergenceState::Converged) {
        return StopReason::QualityConverged;
    }

    // Time budget
    if (m_sessionActive && m_config.timeBudgetMs > 0) {
        uint64_t elapsed = GetTickCount64() - m_sessionStartMs;
        if (static_cast<double>(elapsed) >= m_config.timeBudgetMs) {
            return StopReason::TimeBudgetExpired;
        }
    }

    // Token budget
    if (m_config.maxTotalTokens > 0 && tokens >= m_config.maxTotalTokens) {
        return StopReason::TokenBudgetExpired;
    }

    // Pass budget
    if (m_config.maxPasses > 0 && pass >= m_config.maxPasses) {
        return StopReason::PassBudgetExpired;
    }

    // Resource exhaustion
    if (m_config.memoryPressureLimit > 0 &&
        hw.memoryPressure() >= m_config.memoryPressureLimit) {
        return StopReason::ResourceExhausted;
    }

    // Decode failure flag
    if (m_decodeFailed.load(std::memory_order_relaxed)) {
        return StopReason::CriticalFailure;
    }

    return StopReason::NotStopped;
}

PassIntensity ConvergenceController::computeIntensity(uint32_t pass, float convergenceRate,
                                                        const HardwareFeedback& hw) const {
    // First N passes are always probes
    if (pass < m_config.probePassCount) {
        return PassIntensity::Probe;
    }

    // If hardware is struggling, use lighter intensity
    if (hw.tokensPerSecond > 0 && hw.tokensPerSecond < 2.0) {
        return PassIntensity::Light;
    }

    // If memory pressure is high, use light
    if (hw.memoryPressure() > 0.8) {
        return PassIntensity::Light;
    }

    // If converging quickly, use deeper passes to refine
    if (convergenceRate > 0.1f) {
        return PassIntensity::Deep;
    }

    // If convergence rate is moderate, standard
    if (convergenceRate > 0.02f) {
        return PassIntensity::Standard;
    }

    // If convergence is stalling, try a full pass
    if (convergenceRate < 0.005f && pass > 5) {
        return PassIntensity::Full;
    }

    // Default: standard
    return PassIntensity::Standard;
}

float ConvergenceController::computeQualityEstimate(ConvergenceState state,
                                                      const DeltaMeasurement* delta) const {
    float base = 0.0f;

    switch (state) {
        case ConvergenceState::Converged:    base = 1.0f; break;
        case ConvergenceState::Converging:   base = 0.75f; break;
        case ConvergenceState::Diverging:    base = 0.25f; break;
        case ConvergenceState::Oscillating:  base = 0.4f; break;
        case ConvergenceState::ForceStopped: base = 0.6f; break;
        case ConvergenceState::NotStarted:   base = 0.0f; break;
    }

    // Refine with delta if available
    if (delta) {
        // Lower composite delta = higher quality
        float deltaBonus = (1.0f - delta->compositeDelta) * 0.3f;
        base = std::min(base + deltaBonus, 1.0f);
    }

    return base;
}

float ConvergenceController::computeConvergenceRate() const {
    if (m_qualityHistoryCount < 3) return 0.0f;

    // Linear regression slope over last N quality measurements
    uint32_t window = std::min(m_qualityHistoryCount, m_config.diminishingReturnWindow + 2);
    uint32_t start  = m_qualityHistoryCount - window;

    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    for (uint32_t i = 0; i < window; ++i) {
        double x = static_cast<double>(i);
        double y = static_cast<double>(m_qualityHistory[start + i]);
        sumX  += x;
        sumY  += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double n    = static_cast<double>(window);
    double denom = n * sumX2 - sumX * sumX;
    if (std::abs(denom) < 1e-10) return 0.0f;

    double slope = (n * sumXY - sumX * sumY) / denom;
    return static_cast<float>(slope);
}

bool ConvergenceController::checkDiminishingReturns() const {
    if (m_qualityHistoryCount < m_config.diminishingReturnWindow + 1) return false;

    // Check if improvement over the window is below threshold
    uint32_t window = m_config.diminishingReturnWindow;
    float latest = m_qualityHistory[m_qualityHistoryCount - 1];
    float earlier = m_qualityHistory[m_qualityHistoryCount - 1 - window];

    float improvement = latest - earlier;
    return (improvement >= 0 && improvement < m_config.diminishingReturnRate);
}

uint32_t ConvergenceController::estimateRemainingPasses(float convergenceRate) const {
    if (convergenceRate <= 0.0f) return 0;  // Unknown

    // Estimate passes to reach quality 1.0 at current rate
    float remaining = 1.0f - m_qualityEstimate;
    if (remaining <= 0.0f) return 0;

    uint32_t estimated = static_cast<uint32_t>(remaining / convergenceRate);
    return std::min(estimated, m_config.maxPasses);
}

// ============================================================================
// Signal Injection
// ============================================================================

PatchResult ConvergenceController::signalUserInterrupt() {
    m_userInterrupt.store(true, std::memory_order_release);
    return PatchResult::ok("User interrupt signaled");
}

PatchResult ConvergenceController::signalResourceWarning(double memUsed, double memTotal) {
    m_resourceWarning.store(true, std::memory_order_release);
    return PatchResult::ok("Resource warning signaled");
}

PatchResult ConvergenceController::signalDecodeFailed() {
    m_decodeFailed.store(true, std::memory_order_release);
    return PatchResult::ok("Decode failure signaled");
}

PatchResult ConvergenceController::signalQualityDrop(float delta) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Record the quality drop in history
    if (m_qualityHistoryCount < QUALITY_HISTORY_SIZE) {
        m_qualityHistory[m_qualityHistoryCount++] = m_qualityEstimate - delta;
    }
    return PatchResult::ok("Quality drop signaled");
}

// ============================================================================
// Budget Management
// ============================================================================

PatchResult ConvergenceController::setTimeBudget(double ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.timeBudgetMs = ms;
    return PatchResult::ok("Time budget set");
}

PatchResult ConvergenceController::setTokenBudget(uint32_t tokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.maxTotalTokens = tokens;
    return PatchResult::ok("Token budget set");
}

PatchResult ConvergenceController::setPassBudget(uint32_t passes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config.maxPasses = passes;
    return PatchResult::ok("Pass budget set");
}

double ConvergenceController::remainingTimeMs() const {
    if (!m_sessionActive || m_config.timeBudgetMs <= 0) return -1.0;
    uint64_t elapsed = GetTickCount64() - m_sessionStartMs;
    double remaining = m_config.timeBudgetMs - static_cast<double>(elapsed);
    return std::max(remaining, 0.0);
}

uint32_t ConvergenceController::remainingTokens(uint32_t generated) const {
    if (m_config.maxTotalTokens == 0) return UINT32_MAX;
    if (generated >= m_config.maxTotalTokens) return 0;
    return m_config.maxTotalTokens - generated;
}

uint32_t ConvergenceController::remainingPasses(uint32_t current) const {
    if (m_config.maxPasses == 0) return UINT32_MAX;
    if (current >= m_config.maxPasses) return 0;
    return m_config.maxPasses - current;
}

// ============================================================================
// Session Management
// ============================================================================

PatchResult ConvergenceController::beginSession() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_sessionActive       = true;
    m_sessionStartMs      = GetTickCount64();
    m_currentPass         = 0;
    m_lastStopReason      = StopReason::NotStopped;
    m_qualityEstimate     = 0.0f;
    m_qualityHistoryCount = 0;

    m_userInterrupt.store(false, std::memory_order_release);
    m_resourceWarning.store(false, std::memory_order_release);
    m_decodeFailed.store(false, std::memory_order_release);

    m_stats.sessionsStarted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Session started");
}

PatchResult ConvergenceController::endSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessionActive = false;
    m_stats.sessionsCompleted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Session ended");
}

bool ConvergenceController::isSessionActive() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessionActive;
}

uint32_t ConvergenceController::currentPass() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPass;
}

StopReason ConvergenceController::lastStopReason() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastStopReason;
}

float ConvergenceController::currentQualityEstimate() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_qualityEstimate;
}

// ============================================================================
// Callback
// ============================================================================

PatchResult ConvergenceController::registerCallback(ControllerCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = cb;
    m_callbackUserData = userData;
    return PatchResult::ok("Callback registered");
}

PatchResult ConvergenceController::clearCallback() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = nullptr;
    m_callbackUserData = nullptr;
    return PatchResult::ok("Callback cleared");
}

void ConvergenceController::notifyCallback(const PassDecision& decision) {
    if (m_callback) {
        m_callback(&decision, m_callbackUserData);
    }
}

// ============================================================================
// Statistics
// ============================================================================

void ConvergenceController::resetStats() {
    m_stats.sessionsStarted.store(0, std::memory_order_relaxed);
    m_stats.sessionsCompleted.store(0, std::memory_order_relaxed);
    m_stats.passDecisions.store(0, std::memory_order_relaxed);
    m_stats.continueDecisions.store(0, std::memory_order_relaxed);
    m_stats.stopDecisions.store(0, std::memory_order_relaxed);
    m_stats.probeDecisions.store(0, std::memory_order_relaxed);
    m_stats.deepDecisions.store(0, std::memory_order_relaxed);
    m_stats.userInterrupts.store(0, std::memory_order_relaxed);
    m_stats.timeBudgetExpiries.store(0, std::memory_order_relaxed);
    m_stats.resourceExhaustions.store(0, std::memory_order_relaxed);
}
