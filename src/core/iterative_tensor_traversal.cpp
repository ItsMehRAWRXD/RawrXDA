// ============================================================================
// iterative_tensor_traversal.cpp — Iterative Partial Inference Engine Impl
// ============================================================================
//
// This is the core orchestrator. It implements the iterative control loop:
//   Probe → Emit → Measure → Score → Cache → Decide → Adapt → Re-enter
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "iterative_tensor_traversal.h"
#include "unified_hotpatch_manager.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>

// ============================================================================
// Singleton
// ============================================================================

IterativeTensorTraversal& IterativeTensorTraversal::instance() {
    static IterativeTensorTraversal s_instance;
    return s_instance;
}

IterativeTensorTraversal::IterativeTensorTraversal()
    : m_passNumber(0)
    , m_bestQuality(0.0f)
    , m_bestPassIndex(-1)
    , m_passCallback(nullptr)
    , m_passCallbackUserData(nullptr)
    , m_phaseCallback(nullptr)
    , m_phaseCallbackUserData(nullptr)
    , m_adaptCallback(nullptr)
    , m_adaptCallbackUserData(nullptr)
{}

IterativeTensorTraversal::~IterativeTensorTraversal() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult IterativeTensorTraversal::initialize(const ITTConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("IterativeTensorTraversal already initialized", -1);
    }

    m_config = config;

    // Initialize all sub-systems
    PatchResult r;

    // 1. Traversal Strategy
    r = TraversalStrategy::instance().initialize(config.traversalConfig);
    if (!r.success) {
        return PatchResult::error("Failed to initialize TraversalStrategy", -10);
    }

    // 2. Layer Contribution Scorer
    r = LayerContributionScorer::instance().initialize(config.scorerConfig);
    if (!r.success) {
        TraversalStrategy::instance().shutdown();
        return PatchResult::error("Failed to initialize LayerContributionScorer", -11);
    }

    // 3. Semantic Delta Tracker
    r = SemanticDeltaTracker::instance().initialize(config.deltaConfig);
    if (!r.success) {
        LayerContributionScorer::instance().shutdown();
        TraversalStrategy::instance().shutdown();
        return PatchResult::error("Failed to initialize SemanticDeltaTracker", -12);
    }

    // 4. Cross-Run Tensor Cache
    r = CrossRunTensorCache::instance().initialize(config.cacheConfig);
    if (!r.success) {
        SemanticDeltaTracker::instance().shutdown();
        LayerContributionScorer::instance().shutdown();
        TraversalStrategy::instance().shutdown();
        return PatchResult::error("Failed to initialize CrossRunTensorCache", -13);
    }

    // 5. Convergence Controller
    r = ConvergenceController::instance().initialize(config.controllerConfig);
    if (!r.success) {
        CrossRunTensorCache::instance().shutdown();
        SemanticDeltaTracker::instance().shutdown();
        LayerContributionScorer::instance().shutdown();
        TraversalStrategy::instance().shutdown();
        return PatchResult::error("Failed to initialize ConvergenceController", -14);
    }

    m_passNumber    = 0;
    m_bestQuality   = 0.0f;
    m_bestPassIndex = -1;
    m_phase.store(SessionPhase::Idle, std::memory_order_release);
    m_abortRequested.store(false, std::memory_order_release);

    log("ITT: All sub-systems initialized. Model: %u layers, %u hidden dim",
        config.totalModelLayers, config.hiddenDimension);

    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("IterativeTensorTraversal initialized");
}

void IterativeTensorTraversal::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized.load(std::memory_order_acquire)) return;

    m_abortRequested.store(true, std::memory_order_release);

    // Shutdown sub-systems in reverse order
    ConvergenceController::instance().shutdown();
    CrossRunTensorCache::instance().shutdown();
    SemanticDeltaTracker::instance().shutdown();
    LayerContributionScorer::instance().shutdown();
    TraversalStrategy::instance().shutdown();

    m_phase.store(SessionPhase::Idle, std::memory_order_release);
    m_initialized.store(false, std::memory_order_release);
}

// ============================================================================
// Main Inference Session — THE CORE LOOP
// ============================================================================

PatchResult IterativeTensorTraversal::runSession(const ITTRequest& request,
                                                   ITTSessionResult* outResult) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outResult) return PatchResult::error("Null output pointer", -2);
    if (!m_config.inferenceBackend) {
        return PatchResult::error("No inference backend configured", -3);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // -----------------------------------------------------------------------
    // Session Initialization
    // -----------------------------------------------------------------------
    m_passNumber = 0;
    m_bestQuality = 0.0f;
    m_bestPassIndex = -1;
    m_abortRequested.store(false, std::memory_order_release);

    setPhase(SessionPhase::Probing);

    // Start convergence controller session
    ConvergenceController::instance().beginSession();

    // Reset delta tracker for this session
    SemanticDeltaTracker::instance().reset();

    // Apply request-level overrides
    if (request.timeBudgetMs > 0) {
        ConvergenceController::instance().setTimeBudget(request.timeBudgetMs);
    }
    if (request.maxTokens > 0) {
        ConvergenceController::instance().setTokenBudget(request.maxTokens);
    }
    if (request.passBudget > 0) {
        ConvergenceController::instance().setPassBudget(request.passBudget);
    }

    *outResult = ITTSessionResult::ok("Session started");
    uint32_t totalTokensGenerated = 0;
    uint64_t sessionStartMs = GetTickCount64();

    log("ITT: Session started. Prompt: %u chars, quality target: %.2f",
        request.promptLen, request.qualityTarget);

    // -----------------------------------------------------------------------
    // Generate initial traversal plan
    // -----------------------------------------------------------------------
    TraversalPlan currentPlan;
    PatchResult r = TraversalStrategy::instance().generateInitialPlan(&currentPlan);
    if (!r.success) {
        *outResult = ITTSessionResult::error(StopReason::CriticalFailure,
                                              "Failed to generate initial plan");
        ConvergenceController::instance().endSession();
        setPhase(SessionPhase::Failed);
        return PatchResult::error("Initial plan generation failed", -10);
    }

    // ═══════════════════════════════════════════════════════════════════
    // THE ITERATIVE CONTROL LOOP
    // ═══════════════════════════════════════════════════════════════════
    //
    //  Probe → Emit → Measure → Score → Cache → Decide → Adapt → Re-enter
    //
    // Repeat until convergence, budget exhaustion, or abort.
    // ═══════════════════════════════════════════════════════════════════

    while (!m_abortRequested.load(std::memory_order_acquire)) {
        log("ITT: === Pass %u ===", m_passNumber);

        // -------------------------------------------------------------------
        // Phase 1: PROBE — Execute inference with current traversal plan
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Probing);
        ITTPassResult passResult;
        std::memset(&passResult, 0, sizeof(passResult));

        r = executeInferencePass(request, currentPlan, &passResult);
        if (!r.success) {
            log("ITT: Inference pass %u failed: %s", m_passNumber, r.detail);
            // Don't immediately fail — signal decode failure and let controller decide
            ConvergenceController::instance().signalDecodeFailed();
        }

        passResult.passNumber = m_passNumber;
        passResult.modeUsed   = currentPlan.mode;
        passResult.skipRatioUsed = 1.0f - currentPlan.activeLayerRatio();
        passResult.contextLengthUsed = currentPlan.contextLength;
        passResult.layersExecuted = currentPlan.activeLayerCount();
        passResult.layersSkipped  = static_cast<uint32_t>(currentPlan.layerMask.size()) 
                                    - currentPlan.activeLayerCount();

        totalTokensGenerated += passResult.tokenCount;

        // Notify pass callback
        if (m_passCallback) {
            m_passCallback(&passResult, m_passCallbackUserData);
        }

        // Record in history
        if (m_config.recordAllPasses) {
            outResult->passHistory.push_back(passResult);
        }

        // -------------------------------------------------------------------
        // Phase 2: EMIT — Capture partial semantic signal
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Emitting);
        r = phaseEmit(passResult);
        if (!r.success) {
            log("ITT: Emit phase failed: %s", r.detail);
        }

        // -------------------------------------------------------------------
        // Phase 3: MEASURE — Compute delta from previous pass
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Measuring);
        DeltaMeasurement delta;
        std::memset(&delta, 0, sizeof(delta));
        bool hasDelta = false;

        r = phaseMeasure(passResult, &delta);
        if (r.success) {
            hasDelta = true;
            passResult.compositeDelta = delta.compositeDelta;
            log("ITT: Delta: composite=%.4f, token=%.4f, embed=%.4f, entropy=%.4f",
                delta.compositeDelta, delta.tokenOverlap,
                delta.embeddingDistance, delta.entropyDelta);
        }

        // -------------------------------------------------------------------
        // Phase 4: SCORE — Evaluate layer contributions
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Scoring);
        r = phaseScore(passResult);
        if (!r.success) {
            log("ITT: Score phase failed: %s", r.detail);
        }

        // -------------------------------------------------------------------
        // Phase 5: CACHE — Store stable tensor slices
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Caching);
        r = phaseCache(passResult);
        if (!r.success) {
            log("ITT: Cache phase failed: %s", r.detail);
        }

        // -------------------------------------------------------------------
        // Phase 6: DECIDE — Should we continue or stop?
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Deciding);
        HardwareFeedback hw = sampleHardware();
        hw.tokensPerSecond  = passResult.tokensPerSecond;
        hw.latencyMs        = passResult.passTimeMs;
        hw.successCount     = (r.success) ? 1 : 0;
        hw.decodeFailCount  = (r.success) ? 0 : 1;

        PassDecision decision;
        r = phaseDecide(hw, hasDelta ? &delta : nullptr, &decision);
        if (!r.success) {
            log("ITT: Decide phase failed: %s", r.detail);
            break;
        }

        // Track best result
        passResult.qualityEstimate = decision.qualityEstimate;
        passResult.convergenceState = SemanticDeltaTracker::instance().getState();
        passResult.isConverged = SemanticDeltaTracker::instance().isConverged();

        if (decision.qualityEstimate > m_bestQuality) {
            m_bestQuality   = decision.qualityEstimate;
            m_bestPassIndex = static_cast<int>(m_passNumber);
        }

        log("ITT: Decision: %s, intensity=%d, quality=%.3f, remaining=%u",
            decision.shouldContinue ? "CONTINUE" : "STOP",
            static_cast<int>(decision.intensity),
            decision.qualityEstimate,
            decision.estimatedRemainingPasses);

        // Check if we should stop
        if (!decision.shouldContinue) {
            outResult->stopReason = decision.stopReason;
            break;
        }

        // -------------------------------------------------------------------
        // Phase 7: ADAPT — Adjust strategy based on feedback
        // -------------------------------------------------------------------
        setPhase(SessionPhase::Adapting);
        r = phaseAdapt(decision, hw);
        if (!r.success) {
            log("ITT: Adapt phase failed: %s", r.detail);
        }

        // -------------------------------------------------------------------
        // Phase 8: RE-ENTER — Generate new traversal plan
        // -------------------------------------------------------------------
        setPhase(SessionPhase::ReEntering);
        r = phaseReEnter(decision, &currentPlan);
        if (!r.success) {
            log("ITT: Re-enter phase failed: %s", r.detail);
            break;
        }

        m_passNumber++;
        m_stats.totalPasses.fetch_add(1, std::memory_order_relaxed);

    } // end main loop

    // -----------------------------------------------------------------------
    // Session Finalization
    // -----------------------------------------------------------------------
    uint64_t sessionEndMs = GetTickCount64();
    double totalTimeMs = static_cast<double>(sessionEndMs - sessionStartMs);

    // Select the best result from all passes
    if (!outResult->passHistory.empty()) {
        selectBestResult(outResult->passHistory, outResult);
    }

    // Build layer summaries
    buildLayerSummaries(outResult->passHistory, outResult);

    // Fill session metadata
    outResult->totalPasses          = m_passNumber + 1;
    outResult->totalTokensGenerated = totalTokensGenerated;
    outResult->totalTimeMs          = totalTimeMs;
    outResult->averageTPS           = (totalTimeMs > 0)
        ? (totalTokensGenerated * 1000.0 / totalTimeMs) : 0.0;
    outResult->finalQuality         = m_bestQuality;

    if (m_abortRequested.load(std::memory_order_acquire)) {
        outResult->success    = false;
        outResult->stopReason = StopReason::UserInterrupt;
        outResult->description = "Session aborted by user";
        setPhase(SessionPhase::Stopped);
        m_stats.sessionsAborted.fetch_add(1, std::memory_order_relaxed);
    } else if (outResult->stopReason == StopReason::QualityConverged) {
        outResult->success    = true;
        outResult->description = "Converged via iterative tensor traversal";
        setPhase(SessionPhase::Converged);
        m_stats.sessionsConverged.fetch_add(1, std::memory_order_relaxed);
    } else if (outResult->stopReason == StopReason::TimeBudgetExpired ||
               outResult->stopReason == StopReason::TokenBudgetExpired ||
               outResult->stopReason == StopReason::PassBudgetExpired) {
        outResult->success    = true;  // Budget expiry is a graceful stop
        outResult->description = "Budget expired (partial convergence)";
        setPhase(SessionPhase::Stopped);
        m_stats.sessionsTimedOut.fetch_add(1, std::memory_order_relaxed);
    } else {
        outResult->success    = false;
        outResult->description = "Session stopped without convergence";
        setPhase(SessionPhase::Stopped);
        m_stats.sessionsFailed.fetch_add(1, std::memory_order_relaxed);
    }

    ConvergenceController::instance().endSession();
    updateStats(*outResult);

    log("ITT: Session complete. Passes=%u, Tokens=%u, Time=%.1fms, Quality=%.3f, TPS=%.1f",
        outResult->totalPasses, outResult->totalTokensGenerated,
        outResult->totalTimeMs, outResult->finalQuality, outResult->averageTPS);

    m_stats.sessionsRun.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Session complete");
}

// ============================================================================
// Single Pass (manual control)
// ============================================================================

PatchResult IterativeTensorTraversal::runSinglePass(const ITTRequest& request,
                                                      const TraversalPlan& plan,
                                                      ITTPassResult* outResult) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outResult) return PatchResult::error("Null output pointer", -2);
    if (!m_config.inferenceBackend) {
        return PatchResult::error("No inference backend configured", -3);
    }

    return executeInferencePass(request, plan, outResult);
}

// ============================================================================
// Abort
// ============================================================================

PatchResult IterativeTensorTraversal::abortSession() {
    m_abortRequested.store(true, std::memory_order_release);
    ConvergenceController::instance().signalUserInterrupt();
    return PatchResult::ok("Abort requested");
}

// ============================================================================
// Session Queries
// ============================================================================

bool IterativeTensorTraversal::isSessionActive() const {
    SessionPhase phase = m_phase.load(std::memory_order_acquire);
    return (phase != SessionPhase::Idle &&
            phase != SessionPhase::Converged &&
            phase != SessionPhase::Stopped &&
            phase != SessionPhase::Failed);
}

SessionPhase IterativeTensorTraversal::currentPhase() const {
    return m_phase.load(std::memory_order_acquire);
}

uint32_t IterativeTensorTraversal::currentPassNumber() const {
    return m_passNumber;
}

float IterativeTensorTraversal::currentQuality() const {
    return m_bestQuality;
}

PatchResult IterativeTensorTraversal::getBestResult(ITTPassResult* outResult) const {
    if (!outResult) return PatchResult::error("Null output pointer", -2);
    // This would need concurrent access to pass history — for now return current best quality
    outResult->qualityEstimate = m_bestQuality;
    outResult->passNumber      = (m_bestPassIndex >= 0) ? m_bestPassIndex : 0;
    return PatchResult::ok("Best result retrieved");
}

// ============================================================================
// Internal Loop Phases
// ============================================================================

PatchResult IterativeTensorTraversal::executeInferencePass(const ITTRequest& request,
                                                             const TraversalPlan& plan,
                                                             ITTPassResult* outResult) {
    uint64_t startMs = GetTickCount64();

    // Call the backend
    PatchResult r = m_config.inferenceBackend(&request, &plan, outResult,
                                               m_config.backendUserData);

    uint64_t endMs = GetTickCount64();
    outResult->passTimeMs = static_cast<double>(endMs - startMs);

    if (outResult->passTimeMs > 0 && outResult->tokenCount > 0) {
        outResult->tokensPerSecond = outResult->tokenCount * 1000.0 / outResult->passTimeMs;
    }

    m_stats.totalTokensGenerated.fetch_add(outResult->tokenCount, std::memory_order_relaxed);
    m_stats.totalLayerExecutions.fetch_add(plan.activeLayerCount(), std::memory_order_relaxed);
    m_stats.totalLayerSkips.fetch_add(
        static_cast<uint32_t>(plan.layerMask.size()) - plan.activeLayerCount(),
        std::memory_order_relaxed);

    return r;
}

PatchResult IterativeTensorTraversal::phaseEmit(const ITTPassResult& passResult) {
    // Record this pass's output in the delta tracker
    return SemanticDeltaTracker::instance().recordPassFromText(
        passResult.passNumber,
        passResult.text, passResult.textLen,
        passResult.tokenIds, passResult.tokenCount,
        passResult.outputEntropy
    );
}

PatchResult IterativeTensorTraversal::phaseMeasure(const ITTPassResult& passResult,
                                                     DeltaMeasurement* outDelta) {
    if (m_passNumber == 0) {
        // First pass: no delta to compute
        return PatchResult::error("First pass, no delta", 0);
    }

    return SemanticDeltaTracker::instance().getLastDelta(outDelta);
}

PatchResult IterativeTensorTraversal::phaseScore(const ITTPassResult& passResult) {
    // Submit output entropy as attention entropy proxy for active layers
    // In a full integration, we'd get per-layer activations from the backend
    // For now, distribute the output entropy across executed layers as a proxy

    float entropyPerLayer = passResult.outputEntropy;

    // Recompute all scores with new data
    return LayerContributionScorer::instance().recomputeScores();
}

PatchResult IterativeTensorTraversal::phaseCache(const ITTPassResult& passResult) {
    // In a full integration, we'd cache per-layer tensor outputs here.
    // For now, track cache statistics.
    m_stats.totalLayerCacheHits.fetch_add(passResult.layersCached, std::memory_order_relaxed);
    return PatchResult::ok("Cache phase complete");
}

PatchResult IterativeTensorTraversal::phaseDecide(const HardwareFeedback& hw,
                                                    const DeltaMeasurement* delta,
                                                    PassDecision* outDecision) {
    ConvergenceState state = SemanticDeltaTracker::instance().getState();

    return ConvergenceController::instance().decide(
        hw, delta, state, m_passNumber,
        m_stats.totalTokensGenerated.load(std::memory_order_relaxed),
        outDecision
    );
}

PatchResult IterativeTensorTraversal::phaseAdapt(const PassDecision& decision,
                                                   const HardwareFeedback& hw) {
    // Feed layer scores into traversal strategy
    auto& scorer  = LayerContributionScorer::instance();
    auto& strategy = TraversalStrategy::instance();

    // Get scores and convert to priority updates
    uint32_t totalLayers = m_config.totalModelLayers;
    std::vector<LayerScore> scores(totalLayers);
    uint32_t scoreCount = 0;
    scorer.getAllScores(scores.data(), &scoreCount);

    if (scoreCount > 0) {
        std::vector<float> floatScores(scoreCount);
        for (uint32_t i = 0; i < scoreCount; ++i) {
            floatScores[i] = scores[i].emaScore;
        }
        strategy.setLayerPrioritiesFromScores(floatScores.data(), scoreCount);
    }

    // Apply recommended mode/skip from controller decision
    if (decision.recommendedSkipRatio > 0) {
        strategy.setSkipRatio(decision.recommendedSkipRatio);
    }

    // Adapt based on hardware feedback
    PatchResult r = strategy.adapt(hw);

    m_stats.totalAdaptations.fetch_add(1, std::memory_order_relaxed);

    // Notify adapt callback
    if (m_adaptCallback) {
        StrategyAdaptation adaptation;
        adaptation.reason      = AdaptationReason::None;
        adaptation.newMode     = decision.recommendedMode;
        adaptation.oldMode     = strategy.currentMode();
        adaptation.newSkipRatio = decision.recommendedSkipRatio;
        adaptation.oldSkipRatio = strategy.currentSkipRatio();
        adaptation.passNumber   = m_passNumber;
        adaptation.timestampMs  = GetTickCount64();
        adaptation.description  = "Auto-adapted from convergence controller";
        m_adaptCallback(&adaptation, m_adaptCallbackUserData);
    }

    return r;
}

PatchResult IterativeTensorTraversal::phaseReEnter(const PassDecision& decision,
                                                     TraversalPlan* outPlan) {
    HardwareFeedback hw = sampleHardware();
    return TraversalStrategy::instance().generatePlan(hw, outPlan);
}

// ============================================================================
// Internal Helpers
// ============================================================================

HardwareFeedback IterativeTensorTraversal::sampleHardware() const {
    HardwareFeedback hw;
    std::memset(&hw, 0, sizeof(hw));

    // Query system memory
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        hw.memoryUsedBytes = static_cast<double>(memInfo.ullTotalPhys - memInfo.ullAvailPhys);
        hw.vramTotalBytes  = static_cast<double>(memInfo.ullTotalPhys);
        hw.vramUsedBytes   = hw.memoryUsedBytes;
    }

    hw.timestampMs = GetTickCount64();
    return hw;
}

uint64_t IterativeTensorTraversal::computeContextHash(const ITTRequest& request) const {
    if (!request.prompt || request.promptLen == 0) return 0;

    // FNV-1a hash of prompt text
    uint64_t hash = 14695981039346656037ULL;
    for (uint32_t i = 0; i < request.promptLen; ++i) {
        hash ^= static_cast<uint8_t>(request.prompt[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t IterativeTensorTraversal::computeStrategyHash(const TraversalPlan& plan) const {
    uint64_t hash = static_cast<uint64_t>(plan.mode);
    hash = hash * 6364136223846793005ULL + plan.contextLength;
    hash = hash * 6364136223846793005ULL + plan.tokenBudget;
    hash = hash * 6364136223846793005ULL + plan.passNumber;

    // Include layer mask in hash
    for (size_t i = 0; i < plan.layerMask.size(); ++i) {
        if (plan.layerMask[i]) {
            hash ^= (i * 2654435761ULL);
        }
    }
    return hash;
}

void IterativeTensorTraversal::setPhase(SessionPhase phase) {
    SessionPhase old = m_phase.exchange(phase, std::memory_order_acq_rel);
    if (m_phaseCallback && old != phase) {
        m_phaseCallback(old, phase, m_passNumber, m_phaseCallbackUserData);
    }
}

void IterativeTensorTraversal::selectBestResult(const std::vector<ITTPassResult>& passes,
                                                  ITTSessionResult* outSession) {
    if (passes.empty()) return;

    // Select the pass with highest quality estimate
    int bestIdx = 0;
    float bestQuality = passes[0].qualityEstimate;

    for (size_t i = 1; i < passes.size(); ++i) {
        if (passes[i].qualityEstimate > bestQuality) {
            bestQuality = passes[i].qualityEstimate;
            bestIdx = static_cast<int>(i);
        }
    }

    // If no quality estimates differ, take the last pass (most refined)
    if (bestQuality <= 0.0f) {
        bestIdx = static_cast<int>(passes.size() - 1);
    }

    const ITTPassResult& best = passes[bestIdx];
    outSession->finalText.assign(best.text, best.textLen);
    if (best.tokenIds && best.tokenCount > 0) {
        outSession->finalTokenIds.assign(best.tokenIds, best.tokenIds + best.tokenCount);
    }
}

void IterativeTensorTraversal::buildLayerSummaries(const std::vector<ITTPassResult>& passes,
                                                     ITTSessionResult* outSession) {
    uint32_t totalLayers = m_config.totalModelLayers;
    outSession->layerSummaries.resize(totalLayers);

    for (uint32_t i = 0; i < totalLayers; ++i) {
        auto& summary = outSession->layerSummaries[i];
        summary.layerIndex   = i;
        summary.timesExecuted = 0;
        summary.timesCached   = 0;
        summary.timesSkipped  = 0;
        summary.averageContribution = 0.0f;
    }

    // Get final scores from scorer
    auto& scorer = LayerContributionScorer::instance();
    for (uint32_t i = 0; i < totalLayers; ++i) {
        LayerScore score;
        if (scorer.getScore(i, &score).success) {
            outSession->layerSummaries[i].averageContribution = score.emaScore;
        }
    }
}

void IterativeTensorTraversal::updateStats(const ITTSessionResult& result) {
    if (result.averageTPS > m_stats.bestTPS) {
        m_stats.bestTPS = result.averageTPS;
    }
    if (result.averageTPS > 0 && result.averageTPS < m_stats.worstTPS) {
        m_stats.worstTPS = result.averageTPS;
    }

    // Running average
    uint64_t sessions = m_stats.sessionsRun.load(std::memory_order_relaxed);
    if (sessions > 0) {
        m_stats.averageTPS = (m_stats.averageTPS * (sessions - 1) + result.averageTPS) / sessions;
        m_stats.averagePassesPerSession = (m_stats.averagePassesPerSession * (sessions - 1)
                                            + result.totalPasses) / sessions;
        m_stats.averageQuality = (m_stats.averageQuality * (sessions - 1)
                                   + result.finalQuality) / sessions;
    } else {
        m_stats.averageTPS              = result.averageTPS;
        m_stats.averagePassesPerSession = result.totalPasses;
        m_stats.averageQuality          = result.finalQuality;
    }
}

void IterativeTensorTraversal::log(const char* fmt, ...) {
    if (!m_config.verboseLogging) return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

// ============================================================================
// Callbacks
// ============================================================================

PatchResult IterativeTensorTraversal::setPassCallback(ITTPassCallback cb, void* userData) {
    m_passCallback         = cb;
    m_passCallbackUserData = userData;
    return PatchResult::ok("Pass callback set");
}

PatchResult IterativeTensorTraversal::setPhaseCallback(ITTPhaseCallback cb, void* userData) {
    m_phaseCallback         = cb;
    m_phaseCallbackUserData = userData;
    return PatchResult::ok("Phase callback set");
}

PatchResult IterativeTensorTraversal::setAdaptCallback(ITTAdaptCallback cb, void* userData) {
    m_adaptCallback         = cb;
    m_adaptCallbackUserData = userData;
    return PatchResult::ok("Adapt callback set");
}

PatchResult IterativeTensorTraversal::setInferenceBackend(ITTInferenceBackend backend,
                                                            void* userData) {
    m_config.inferenceBackend  = backend;
    m_config.backendUserData   = userData;
    return PatchResult::ok("Inference backend set");
}

// ============================================================================
// Sub-system Access
// ============================================================================

TraversalStrategy& IterativeTensorTraversal::getTraversalStrategy() {
    return TraversalStrategy::instance();
}

LayerContributionScorer& IterativeTensorTraversal::getLayerScorer() {
    return LayerContributionScorer::instance();
}

SemanticDeltaTracker& IterativeTensorTraversal::getDeltaTracker() {
    return SemanticDeltaTracker::instance();
}

CrossRunTensorCache& IterativeTensorTraversal::getTensorCache() {
    return CrossRunTensorCache::instance();
}

ConvergenceController& IterativeTensorTraversal::getConvergenceController() {
    return ConvergenceController::instance();
}

// ============================================================================
// Hotpatch Integration — Live mutation mid-session
// ============================================================================

PatchResult IterativeTensorTraversal::hotpatchParameter(ClampTarget target, float newValue) {
    ParameterClamp clamp = ParameterClamp::make(target, newValue, newValue, newValue);
    PatchResult r = TraversalStrategy::instance().setClamp(clamp);
    if (r.success) {
        m_stats.totalHotpatches.fetch_add(1, std::memory_order_relaxed);

        // Also emit through unified hotpatch manager event system
        UnifiedHotpatchManager::instance().apply_memory_patch(nullptr, 0, nullptr);
    }
    return r;
}

PatchResult IterativeTensorTraversal::hotpatchTraversalMode(TraversalMode mode) {
    PatchResult r = TraversalStrategy::instance().setMode(mode);
    if (r.success) {
        m_stats.totalHotpatches.fetch_add(1, std::memory_order_relaxed);
    }
    return r;
}

PatchResult IterativeTensorTraversal::hotpatchSkipRatio(float ratio) {
    PatchResult r = TraversalStrategy::instance().setSkipRatio(ratio);
    if (r.success) {
        m_stats.totalHotpatches.fetch_add(1, std::memory_order_relaxed);
    }
    return r;
}

PatchResult IterativeTensorTraversal::hotpatchProtectLayer(uint32_t layerIndex) {
    PatchResult r = LayerContributionScorer::instance().protectLayer(layerIndex);
    if (r.success) {
        TraversalStrategy::instance().setLayerPriority(layerIndex, LayerPriority::Critical);
        m_stats.totalHotpatches.fetch_add(1, std::memory_order_relaxed);
    }
    return r;
}

PatchResult IterativeTensorTraversal::hotpatchUnprotectLayer(uint32_t layerIndex) {
    PatchResult r = LayerContributionScorer::instance().unprotectLayer(layerIndex);
    if (r.success) {
        TraversalStrategy::instance().setLayerPriority(layerIndex, LayerPriority::Unknown);
        m_stats.totalHotpatches.fetch_add(1, std::memory_order_relaxed);
    }
    return r;
}

// ============================================================================
// JSON Export
// ============================================================================

std::string IterativeTensorTraversal::toJson() const {
    char buf[2048];
    const auto& s = m_stats;
    snprintf(buf, sizeof(buf),
        "{"
        "\"sessionsRun\":%llu,"
        "\"sessionsConverged\":%llu,"
        "\"sessionsTimedOut\":%llu,"
        "\"sessionsAborted\":%llu,"
        "\"sessionsFailed\":%llu,"
        "\"totalPasses\":%llu,"
        "\"totalTokensGenerated\":%llu,"
        "\"totalLayerExecutions\":%llu,"
        "\"totalLayerCacheHits\":%llu,"
        "\"totalLayerSkips\":%llu,"
        "\"totalAdaptations\":%llu,"
        "\"totalHotpatches\":%llu,"
        "\"bestTPS\":%.2f,"
        "\"worstTPS\":%.2f,"
        "\"averageTPS\":%.2f,"
        "\"averagePassesPerSession\":%.1f,"
        "\"averageQuality\":%.3f,"
        "\"cacheHitRate\":%.3f,"
        "\"cacheFillRatio\":%.3f"
        "}",
        (unsigned long long)s.sessionsRun.load(),
        (unsigned long long)s.sessionsConverged.load(),
        (unsigned long long)s.sessionsTimedOut.load(),
        (unsigned long long)s.sessionsAborted.load(),
        (unsigned long long)s.sessionsFailed.load(),
        (unsigned long long)s.totalPasses.load(),
        (unsigned long long)s.totalTokensGenerated.load(),
        (unsigned long long)s.totalLayerExecutions.load(),
        (unsigned long long)s.totalLayerCacheHits.load(),
        (unsigned long long)s.totalLayerSkips.load(),
        (unsigned long long)s.totalAdaptations.load(),
        (unsigned long long)s.totalHotpatches.load(),
        s.bestTPS, s.worstTPS, s.averageTPS,
        s.averagePassesPerSession, s.averageQuality,
        CrossRunTensorCache::instance().hitRate(),
        CrossRunTensorCache::instance().fillRatio()
    );
    return std::string(buf);
}

std::string IterativeTensorTraversal::sessionSummaryToJson(const ITTSessionResult& result) const {
    char buf[2048];
    snprintf(buf, sizeof(buf),
        "{"
        "\"success\":%s,"
        "\"stopReason\":%d,"
        "\"totalPasses\":%u,"
        "\"totalTokens\":%u,"
        "\"totalTimeMs\":%.1f,"
        "\"averageTPS\":%.2f,"
        "\"finalQuality\":%.3f,"
        "\"finalTextLength\":%zu,"
        "\"layerSummaryCount\":%zu,"
        "\"passHistoryCount\":%zu"
        "}",
        result.success ? "true" : "false",
        static_cast<int>(result.stopReason),
        result.totalPasses,
        result.totalTokensGenerated,
        result.totalTimeMs,
        result.averageTPS,
        result.finalQuality,
        result.finalText.size(),
        result.layerSummaries.size(),
        result.passHistory.size()
    );
    return std::string(buf);
}

// ============================================================================
// Statistics Reset
// ============================================================================

void IterativeTensorTraversal::resetStats() {
    m_stats.sessionsRun.store(0, std::memory_order_relaxed);
    m_stats.sessionsConverged.store(0, std::memory_order_relaxed);
    m_stats.sessionsTimedOut.store(0, std::memory_order_relaxed);
    m_stats.sessionsAborted.store(0, std::memory_order_relaxed);
    m_stats.sessionsFailed.store(0, std::memory_order_relaxed);
    m_stats.totalPasses.store(0, std::memory_order_relaxed);
    m_stats.totalTokensGenerated.store(0, std::memory_order_relaxed);
    m_stats.totalLayerExecutions.store(0, std::memory_order_relaxed);
    m_stats.totalLayerCacheHits.store(0, std::memory_order_relaxed);
    m_stats.totalLayerSkips.store(0, std::memory_order_relaxed);
    m_stats.totalAdaptations.store(0, std::memory_order_relaxed);
    m_stats.totalHotpatches.store(0, std::memory_order_relaxed);
    m_stats.bestTPS = 0;
    m_stats.worstTPS = 999999.0;
    m_stats.averageTPS = 0;
    m_stats.averagePassesPerSession = 0;
    m_stats.averageQuality = 0;
}
