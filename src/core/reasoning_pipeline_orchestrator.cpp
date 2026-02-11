// ============================================================================
// reasoning_pipeline_orchestrator.cpp — Tunable Reasoning Pipeline Orchestrator
// ============================================================================
//
// Full implementation of the tunable multi-agent reasoning pipeline.
// All stages: routing → chain building → adaptive → thermal → swarm →
// execution → fallback → visibility → telemetry → self-tune.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "../include/reasoning_pipeline_orchestrator.h"
#include "../include/reasoning_profile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <chrono>
#include <numeric>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ============================================================================
// Static Role Definitions (system prompts for each reasoning agent)
// ============================================================================
namespace {

struct RoleDefinition {
    const char* name;
    const char* systemPrompt;
};

static const RoleDefinition kRoles[] = {
    { "thinker",
      "You are a deep thinker. Reason step-by-step through the problem, "
      "consider alternatives, weigh trade-offs, and explain your reasoning clearly." },
    { "critic",
      "You are a harsh critic. Find every flaw, weakness, edge case, and logical gap. "
      "Be thorough and unforgiving in your analysis." },
    { "auditor",
      "You are a security and quality auditor. Check for vulnerabilities, correctness, "
      "compliance issues, and edge cases." },
    { "researcher",
      "You are a research assistant. Gather relevant context, identify patterns, "
      "cross-reference information, and cite sources where applicable." },
    { "debater_for",
      "You argue IN FAVOR of the proposed approach. Present the strongest possible case "
      "for why this is correct, optimal, and should be adopted." },
    { "debater_against",
      "You argue AGAINST the proposed approach. Present the strongest possible "
      "counterarguments, alternatives, and risks." },
    { "verifier",
      "You are a verifier. Check all previous claims for accuracy. Flag anything "
      "unverified, incorrect, or contradictory." },
    { "refiner",
      "You are a refiner. Take the previous output and improve its clarity, "
      "correctness, and completeness without changing its core meaning." },
    { "synthesizer",
      "You are a synthesizer. Combine all previous analyses into a coherent, "
      "actionable final answer. Resolve conflicts and present the best path forward." },
    { "brainstorm",
      "You are a creative brainstormer. Generate multiple diverse approaches "
      "and ideas without filtering. Quantity and novelty over perfection." },
    { "summarizer",
      "You are a summarizer. Distill everything into a concise, actionable summary. "
      "Focus on key decisions, actions, and outcomes." },
    { nullptr, nullptr }
};

const char* getSystemPromptForRole(const char* roleName) {
    for (int i = 0; kRoles[i].name; ++i) {
        if (strcmp(kRoles[i].name, roleName) == 0) return kRoles[i].systemPrompt;
    }
    return "You are a helpful assistant.";
}

// Simple similarity: count shared word tokens between two strings
// Used for swarm voting (how many agents agree)
int wordOverlap(const std::string& a, const std::string& b) {
    auto tokenize = [](const std::string& s) -> std::vector<std::string> {
        std::vector<std::string> tokens;
        std::string word;
        for (char c : s) {
            if (std::isalnum((unsigned char)c)) {
                word += (char)std::tolower((unsigned char)c);
            } else if (!word.empty()) {
                tokens.push_back(word);
                word.clear();
            }
        }
        if (!word.empty()) tokens.push_back(word);
        return tokens;
    };

    auto ta = tokenize(a);
    auto tb = tokenize(b);
    std::unordered_map<std::string, int> counts;
    for (const auto& t : ta) counts[t]++;
    int overlap = 0;
    for (const auto& t : tb) {
        if (counts[t] > 0) { overlap++; counts[t]--; }
    }
    return overlap;
}

// Levenshtein distance for short strings (used for greeting detection edge cases)
int levenshtein(const std::string& a, const std::string& b) {
    int m = (int)a.size(), n = (int)b.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
    for (int i = 0; i <= m; i++) dp[i][0] = i;
    for (int j = 0; j <= n; j++) dp[0][j] = j;
    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            int cost = (std::tolower(a[i-1]) == std::tolower(b[j-1])) ? 0 : 1;
            dp[i][j] = (std::min)({dp[i-1][j]+1, dp[i][j-1]+1, dp[i-1][j-1]+cost});
        }
    }
    return dp[m][n];
}

} // anonymous namespace

// ============================================================================
// Singleton
// ============================================================================

ReasoningPipelineOrchestrator::ReasoningPipelineOrchestrator() {
    memset(&m_stats, 0, sizeof(m_stats));
}

ReasoningPipelineOrchestrator::~ReasoningPipelineOrchestrator() {
    cancel();
    stopThermalMonitor();
}

ReasoningPipelineOrchestrator& ReasoningPipelineOrchestrator::instance() {
    static ReasoningPipelineOrchestrator orch;
    return orch;
}

// ============================================================================
// Configuration
// ============================================================================

void ReasoningPipelineOrchestrator::setInferenceCallback(InferenceCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inferenceCallback = std::move(cb);
}

void ReasoningPipelineOrchestrator::setStreamingCallback(StreamingInferenceCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_streamingCallback = std::move(cb);
}

void ReasoningPipelineOrchestrator::setDefaultModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultModel = model;
}

// ============================================================================
// PRIMARY EXECUTION — The main pipeline entry point
// ============================================================================

PipelineResult ReasoningPipelineOrchestrator::execute(
    const std::string& userInput, const std::string& context) {

    auto startTime = std::chrono::steady_clock::now();

    // Atomics for cancellation
    m_running.store(true);
    m_cancelled.store(false);
    uint64_t reqId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        reqId = ++m_requestCounter;
    }

    // Get current profile snapshot
    auto& profMgr = ReasoningProfileManager::instance();
    ReasoningProfile profile = profMgr.getProfile();

    // --- Stage 1: Input Classification ---
    InputComplexity complexity = classifyInput(userInput);

    // --- Stage 2: Bypass Check ---
    if (shouldBypass(userInput, complexity)) {
        PipelineResult r = runDirectLLM(userInput, context);
        r.wasBypassed = true;
        r.inputComplexity = complexity;
        r.effectiveDepth = 0;
        r.effectiveMode = ReasoningMode::Fast;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.totalExecutions++;
            m_stats.bypassed++;
        }

        auto endTime = std::chrono::steady_clock::now();
        r.totalLatencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        r.visible = formatVisibleOutput(r, profile);
        emitTelemetry(r, userInput);
        m_running.store(false);
        return r;
    }

    // --- Stage 3: Compute Effective Depth ---
    int baseDepth = computeEffectiveDepth(profile, complexity);

    // --- Stage 4: Adaptive Adjustment ---
    int adaptedDepth = baseDepth;
    bool adaptiveAdjusted = false;
    if (profile.adaptive.enabled) {
        adaptedDepth = applyAdaptiveAdjustment(baseDepth, profile);
        if (adaptedDepth != baseDepth) adaptiveAdjusted = true;
    }

    // --- Stage 5: Thermal Throttling ---
    int thermalDepth = adaptedDepth;
    bool thermalThrottled = false;
    if (profile.thermal.enabled) {
        thermalDepth = applyThermalThrottling(adaptedDepth, profile);
        if (thermalDepth != adaptedDepth) thermalThrottled = true;
    }

    int effectiveDepth = thermalDepth;

    // --- Stage 6: Self-Tune Override ---
    if (profile.selfTune.enabled) {
        SelfTuneState tuneState = profMgr.getSelfTuneState();
        if (tuneState.totalObservations >= (uint64_t)profile.selfTune.minSamplesBeforeTune) {
            int tuneDepth = tuneState.currentOptimalDepth;
            // Blend: take max of self-tune and complexity-adjusted, but not above thermal limit
            effectiveDepth = (std::min)(
                (std::max)(tuneDepth, effectiveDepth),
                thermalDepth
            );
        }
    }

    // Clamp final depth
    effectiveDepth = (std::max)(0, (std::min)(effectiveDepth, 8));

    // --- Stage 7: Resolve Effective Mode ---
    ReasoningMode effectiveMode = resolveEffectiveMode(profile, complexity, effectiveDepth);

    // Check for cancellation
    if (m_cancelled.load()) {
        m_running.store(false);
        return PipelineResult::fail("Pipeline cancelled.");
    }

    // --- Stage 8: Execute based on mode ---
    PipelineResult result;

    if (effectiveDepth == 0 || effectiveMode == ReasoningMode::Fast) {
        result = runDirectLLM(userInput, context);
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.fastExecutions++;
        }
    } else if (effectiveMode == ReasoningMode::Swarm && profile.swarm.enabled) {
        result = runSwarmPipeline(userInput, context, profile);
        result.usedSwarm = true;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.swarmExecutions++;
        }
    } else {
        // Build agent chain
        std::vector<std::string> chain = buildAgentChain(profile, effectiveDepth);
        result = runChainedPipeline(userInput, context, chain, profile);

        std::lock_guard<std::mutex> lock(m_mutex);
        switch (effectiveMode) {
        case ReasoningMode::Normal:   m_stats.normalExecutions++; break;
        case ReasoningMode::Deep:     m_stats.deepExecutions++; break;
        case ReasoningMode::Critical: m_stats.criticalExecutions++; break;
        default: m_stats.normalExecutions++; break;
        }
    }

    // --- Stage 9: Fill metadata ---
    result.effectiveDepth = effectiveDepth;
    result.effectiveMode = effectiveMode;
    result.inputComplexity = complexity;
    result.wasAdaptiveAdjusted = adaptiveAdjusted;
    result.wasThermalThrottled = thermalThrottled;

    // --- Stage 10: Fallback enforcement ---
    if (result.finalAnswer.empty() || result.finalAnswer.find_first_not_of(" \t\r\n") == std::string::npos) {
        result.finalAnswer = enforceFinalAnswer(result.finalAnswer, result.steps, profile);
        result.usedFallback = true;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stats.fallbacksUsed++;
        }
    }

    // --- Stage 11: Confidence estimation ---
    result.finalConfidence = estimateConfidence(result.finalAnswer);

    auto endTime = std::chrono::steady_clock::now();
    result.totalLatencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // --- Stage 12: Format visible output ---
    result.visible = formatVisibleOutput(result, profile);

    // --- Stage 13: Update EWMA latency ---
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_ewmaInitialized) {
            m_ewmaLatency = result.totalLatencyMs;
            m_ewmaInitialized = true;
        } else {
            double alpha = profile.adaptive.latencySmoothingAlpha;
            m_ewmaLatency = alpha * result.totalLatencyMs + (1.0 - alpha) * m_ewmaLatency;
        }

        m_stats.totalExecutions++;
        if (adaptiveAdjusted) m_stats.adaptiveAdjustments++;
        if (thermalThrottled) m_stats.thermalThrottles++;
        if (!result.success) m_stats.errorCount++;

        // Running averages
        uint64_t n = m_stats.totalExecutions;
        m_stats.avgLatencyMs = m_stats.avgLatencyMs * ((double)(n-1)/n) + result.totalLatencyMs / n;
        m_stats.avgConfidence = m_stats.avgConfidence * ((float)(n-1)/(float)n) + result.finalConfidence / (float)n;
    }

    // --- Stage 14: Emit telemetry for self-tuner ---
    emitTelemetry(result, userInput);

    m_running.store(false);
    return result;
}

// ============================================================================
// Direct Mode Execution (bypass routing)
// ============================================================================

PipelineResult ReasoningPipelineOrchestrator::executeFast(
    const std::string& input, const std::string& context) {
    return runDirectLLM(input, context);
}

PipelineResult ReasoningPipelineOrchestrator::executeNormal(
    const std::string& input, const std::string& context) {
    auto profile = ReasoningProfileManager::instance().getProfile();
    auto chain = buildAgentChain(profile, 1);
    return runChainedPipeline(input, context, chain, profile);
}

PipelineResult ReasoningPipelineOrchestrator::executeDeep(
    const std::string& input, const std::string& context) {
    auto profile = ReasoningProfileManager::instance().getProfile();
    auto chain = buildAgentChain(profile, 3);
    return runChainedPipeline(input, context, chain, profile);
}

PipelineResult ReasoningPipelineOrchestrator::executeCritical(
    const std::string& input, const std::string& context) {
    auto profile = ReasoningProfileManager::instance().getProfile();
    profile.reasoning.enableCritic = true;
    profile.reasoning.enableAuditor = true;
    profile.reasoning.enableThinker = true;
    profile.reasoning.enableVerifier = true;
    profile.reasoning.enableSynthesizer = true;
    auto chain = buildAgentChain(profile, 5);
    return runChainedPipeline(input, context, chain, profile);
}

PipelineResult ReasoningPipelineOrchestrator::executeSwarm(
    const std::string& input, const std::string& context) {
    auto profile = ReasoningProfileManager::instance().getProfile();
    profile.swarm.enabled = true;
    return runSwarmPipeline(input, context, profile);
}

void ReasoningPipelineOrchestrator::cancel() {
    m_cancelled.store(true);
}

bool ReasoningPipelineOrchestrator::isRunning() const {
    return m_running.load();
}

// ============================================================================
// INPUT CLASSIFICATION
// ============================================================================

InputComplexity ReasoningPipelineOrchestrator::classifyInput(const std::string& input) const {
    return ReasoningProfileManager::instance().classifyInput(input);
}

bool ReasoningPipelineOrchestrator::shouldBypass(
    const std::string& input, InputComplexity complexity) const {
    if (complexity == InputComplexity::Trivial) return true;
    return ReasoningProfileManager::instance().shouldBypassPipeline(input);
}

// ============================================================================
// DEPTH COMPUTATION
// ============================================================================

int ReasoningPipelineOrchestrator::computeEffectiveDepth(
    const ReasoningProfile& profile, InputComplexity complexity) const {

    int baseDepth = profile.reasoning.reasoningDepth;

    // Complexity multiplier
    switch (complexity) {
    case InputComplexity::Trivial:  return 0;
    case InputComplexity::Simple:   return (std::min)(baseDepth, 1);
    case InputComplexity::Moderate: return baseDepth;
    case InputComplexity::Complex:  return (std::max)(baseDepth, (std::min)(baseDepth + 1, 6));
    case InputComplexity::Expert:   return (std::max)(baseDepth, (std::min)(baseDepth + 2, 8));
    default:                        return baseDepth;
    }
}

int ReasoningPipelineOrchestrator::applyAdaptiveAdjustment(
    int baseDepth, const ReasoningProfile& profile) const {

    const auto& ap = profile.adaptive;
    int depth = baseDepth;

    // Latency-aware: if EWMA latency exceeds target, reduce depth
    if (ap.strategy == AdaptiveStrategy::LatencyAware ||
        ap.strategy == AdaptiveStrategy::Hybrid) {
        double ewma;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ewma = m_ewmaLatency;
        }

        if (ewma > ap.latencyMaxMs) {
            // Hard ceiling exceeded: aggressive reduction
            depth = (std::max)(ap.minAdaptiveDepth, depth - ap.depthReductionPerStep * 2);
        } else if (ewma > ap.latencyTargetMs) {
            // Over target: gentle reduction
            double ratio = (ewma - ap.latencyTargetMs) /
                           (ap.latencyMaxMs - ap.latencyTargetMs);
            int reduction = (int)(ratio * ap.depthReductionPerStep + 0.5);
            depth = (std::max)(ap.minAdaptiveDepth, depth - reduction);
        }
    }

    // Confidence-based: if recent confidence is low, increase depth
    if (ap.strategy == AdaptiveStrategy::ConfidenceBased ||
        ap.strategy == AdaptiveStrategy::Hybrid) {

        auto recentTelemetry = ReasoningProfileManager::instance().getRecentTelemetry(5);
        if (!recentTelemetry.empty()) {
            float avgConf = 0.0f;
            for (const auto& t : recentTelemetry) avgConf += t.finalConfidence;
            avgConf /= (float)recentTelemetry.size();

            if (avgConf < ap.lowConfidenceThreshold) {
                // Low confidence: increase depth
                depth = (std::min)(ap.maxAdaptiveDepth, depth + 1);
            } else if (avgConf > ap.highConfidenceThreshold && depth > 1) {
                // High confidence: okay to reduce
                depth = (std::max)(ap.minAdaptiveDepth, depth - 1);
            }
        }
    }

    return (std::max)(ap.minAdaptiveDepth, (std::min)(depth, ap.maxAdaptiveDepth));
}

int ReasoningPipelineOrchestrator::applyThermalThrottling(
    int depth, const ReasoningProfile& profile) const {

    ThermalState thermal = ReasoningProfileManager::instance().getThermalState();
    const auto& tp = profile.thermal;

    switch (thermal) {
    case ThermalState::Cool:
        return depth; // No throttling
    case ThermalState::Warm:
        return (std::min)(depth, tp.depthAtWarm);
    case ThermalState::Hot:
        return (std::min)(depth, tp.depthAtHot);
    case ThermalState::Critical:
        if (tp.forceBypassInCritical) return 0;
        return (std::min)(depth, tp.depthAtCritical);
    default:
        return depth;
    }
}

ReasoningMode ReasoningPipelineOrchestrator::resolveEffectiveMode(
    const ReasoningProfile& profile, InputComplexity complexity, int effectiveDepth) const {

    if (effectiveDepth == 0) return ReasoningMode::Fast;

    // If mode is explicitly set (not Adaptive), use it
    if (profile.reasoning.mode != ReasoningMode::Adaptive) {
        return profile.reasoning.mode;
    }

    // Adaptive mode resolution based on complexity + depth
    if (effectiveDepth >= 4 && profile.swarm.enabled) return ReasoningMode::Swarm;
    if (effectiveDepth >= 4) return ReasoningMode::Critical;
    if (effectiveDepth >= 2) return ReasoningMode::Deep;
    return ReasoningMode::Normal;
}

// ============================================================================
// CHAIN BUILDING — construct the ordered agent list
// ============================================================================

std::vector<std::string> ReasoningPipelineOrchestrator::buildAgentChain(
    const ReasoningProfile& profile, int effectiveDepth) const {

    std::vector<std::string> chain;
    const auto& r = profile.reasoning;

    // Add agents in recommended order
    if (r.enableBrainstorm)  chain.push_back("brainstorm");
    if (r.enableResearcher)  chain.push_back("researcher");
    if (r.enableThinker)     chain.push_back("thinker");
    if (r.enableDebaters) {
        chain.push_back("debater_for");
        chain.push_back("debater_against");
    }
    if (r.enableCritic)      chain.push_back("critic");
    if (r.enableAuditor)     chain.push_back("auditor");
    if (r.enableVerifier)    chain.push_back("verifier");
    if (r.enableRefiner)     chain.push_back("refiner");
    if (r.enableSummarizer)  chain.push_back("summarizer");
    if (r.enableSynthesizer) chain.push_back("synthesizer");

    // If chain is empty, at least use thinker + synthesizer
    if (chain.empty()) {
        chain.push_back("thinker");
        chain.push_back("synthesizer");
    }

    // Truncate to effective depth (but always keep at least synthesizer if present)
    if ((int)chain.size() > effectiveDepth && effectiveDepth > 0) {
        // Always keep the last element (synthesizer) and trim from middle
        std::string last = chain.back();
        chain.resize(effectiveDepth);
        if (chain.back() != last) {
            if ((int)chain.size() >= effectiveDepth) {
                chain.back() = last; // Replace last with synthesizer
            } else {
                chain.push_back(last);
            }
        }
    }

    return chain;
}

std::string ReasoningPipelineOrchestrator::getSystemPromptForRole(const std::string& role) const {
    return ::getSystemPromptForRole(role.c_str());
}

// ============================================================================
// DIRECT LLM — bypass pipeline, single call
// ============================================================================

PipelineResult ReasoningPipelineOrchestrator::runDirectLLM(
    const std::string& input, const std::string& context) const {

    InferenceCallback cb;
    std::string model;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_inferenceCallback;
        model = m_defaultModel;
    }

    if (!cb) {
        return PipelineResult::fail("No inference callback configured.");
    }

    auto start = std::chrono::steady_clock::now();

    std::string systemPrompt = "You are a helpful assistant.";
    if (!context.empty()) {
        systemPrompt += "\n\nContext:\n" + context;
    }

    std::string response;
    try {
        response = cb(systemPrompt, input, model);
    } catch (...) {
        return PipelineResult::fail("Inference callback threw an exception.");
    }

    auto end = std::chrono::steady_clock::now();
    double latency = std::chrono::duration<double, std::milli>(end - start).count();

    PipelineResult r = PipelineResult::ok(response);
    r.totalLatencyMs = latency;
    r.effectiveDepth = 0;
    r.effectiveMode = ReasoningMode::Fast;
    r.finalConfidence = estimateConfidence(response);
    return r;
}

// ============================================================================
// CHAINED PIPELINE — sequential multi-agent chain
// ============================================================================

PipelineResult ReasoningPipelineOrchestrator::runChainedPipeline(
    const std::string& input, const std::string& context,
    const std::vector<std::string>& chain,
    const ReasoningProfile& profile) {

    InferenceCallback cb;
    std::string model;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_inferenceCallback;
        model = m_defaultModel;
    }

    if (!cb) {
        return PipelineResult::fail("No inference callback configured.");
    }

    PipelineResult result;
    result.success = true;

    std::string accumulatedContext;
    if (!context.empty()) {
        accumulatedContext = "Context:\n" + context + "\n\n";
    }
    accumulatedContext += "User Query:\n" + input;

    // Notify step progress
    auto& profMgr = ReasoningProfileManager::instance();

    for (int i = 0; i < (int)chain.size(); ++i) {
        if (m_cancelled.load()) {
            result.errorMsg = "Pipeline cancelled at step " + std::to_string(i);
            break;
        }

        const std::string& roleName = chain[i];
        std::string systemPrompt = getSystemPromptForRole(roleName);

        // Build user message: original query + all prior outputs
        std::string userMessage = accumulatedContext;
        if (i > 0 && !result.steps.empty()) {
            userMessage += "\n\n--- Prior Analysis ---\n";
            for (const auto& step : result.steps) {
                if (step.success && !step.content.empty()) {
                    userMessage += "\n[" + step.agentRole + "]:\n" + step.content + "\n";
                }
            }
        }

        // Fire progress callback
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            // Access progress callback through profile manager
        }

        auto stepStart = std::chrono::steady_clock::now();

        PipelineStepResult stepResult;
        stepResult.stepIndex = i;
        stepResult.agentRole = roleName;

        std::string response;
        try {
            response = cb(systemPrompt, userMessage, model);
            stepResult.success = true;
            stepResult.content = response;
        } catch (...) {
            stepResult.success = false;
            stepResult.errorMsg = "Inference failed for role: " + roleName;
        }

        auto stepEnd = std::chrono::steady_clock::now();
        stepResult.latencyMs = std::chrono::duration<double, std::milli>(stepEnd - stepStart).count();
        stepResult.confidence = estimateConfidence(response);

        // Rough token count estimate (4 chars per token)
        stepResult.tokenCount = (int)(response.size() / 4);

        result.steps.push_back(stepResult);
    }

    // Final answer is the last successful step's output
    for (int i = (int)result.steps.size() - 1; i >= 0; --i) {
        if (result.steps[i].success && !result.steps[i].content.empty()) {
            result.finalAnswer = result.steps[i].content;
            break;
        }
    }

    // Enforce final answer if needed
    if (result.finalAnswer.empty() && profile.reasoning.requireFinalAnswer) {
        result.finalAnswer = enforceFinalAnswer(result.finalAnswer, result.steps, profile);
        result.usedFallback = true;
    }

    result.finalConfidence = estimateConfidence(result.finalAnswer);
    return result;
}

// ============================================================================
// SWARM PIPELINE — parallel multi-agent reasoning
// ============================================================================

PipelineResult ReasoningPipelineOrchestrator::runSwarmPipeline(
    const std::string& input, const std::string& context,
    const ReasoningProfile& profile) {

    InferenceCallback cb;
    std::string model;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cb = m_inferenceCallback;
        model = m_defaultModel;
    }

    if (!cb) {
        return PipelineResult::fail("No inference callback configured.");
    }

    const auto& sp = profile.swarm;
    int agentCount = (std::max)(2, (std::min)(sp.agentCount, 16));

    // Determine models per agent
    std::vector<std::string> agentModels(agentCount, model);
    if (sp.heterogeneous && !sp.models.empty()) {
        for (int i = 0; i < agentCount; ++i) {
            agentModels[i] = sp.models[i % sp.models.size()];
        }
    }

    // Run agents in parallel using thread pool
    std::vector<SwarmAgentResult> agentResults(agentCount);
    std::vector<std::thread> threads;
    threads.reserve(agentCount);

    std::string systemPrompt = "You are a helpful assistant. Answer the following carefully.";
    if (!context.empty()) {
        systemPrompt += "\n\nContext:\n" + context;
    }

    for (int i = 0; i < agentCount; ++i) {
        threads.emplace_back([&, i]() {
            auto start = std::chrono::steady_clock::now();
            agentResults[i].agentIndex = i;
            agentResults[i].model = agentModels[i];

            try {
                agentResults[i].output = cb(systemPrompt, input, agentModels[i]);
                agentResults[i].success = true;
                agentResults[i].confidence = 0.5f; // placeholder
            } catch (...) {
                agentResults[i].success = false;
                agentResults[i].errorMsg = "Agent " + std::to_string(i) + " failed.";
            }

            auto end = std::chrono::steady_clock::now();
            agentResults[i].latencyMs =
                std::chrono::duration<double, std::milli>(end - start).count();
        });
    }

    // Timeout join
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    // Filter successful results
    std::vector<SwarmAgentResult> successResults;
    for (const auto& r : agentResults) {
        if (r.success && !r.output.empty()) {
            successResults.push_back(r);
        }
    }

    if (successResults.empty()) {
        return PipelineResult::fail("All swarm agents failed.");
    }

    // Apply swarm strategy
    std::string finalAnswer;
    switch (sp.mode) {
    case SwarmReasoningMode::ParallelVote:
        finalAnswer = swarmParallelVote(successResults, sp.voteThreshold);
        break;
    case SwarmReasoningMode::Sequential:
        // Sequential: chain them (already just take the last one)
        finalAnswer = successResults.back().output;
        break;
    case SwarmReasoningMode::Tournament:
        finalAnswer = swarmTournament(successResults, sp.tournamentRounds, input);
        break;
    case SwarmReasoningMode::Ensemble:
        finalAnswer = swarmEnsemble(successResults, sp.ensembleWeightDecay);
        break;
    default:
        finalAnswer = successResults[0].output;
        break;
    }

    PipelineResult result = PipelineResult::ok(finalAnswer);
    result.usedSwarm = true;

    // Record individual agent results as pipeline steps
    for (const auto& ar : agentResults) {
        PipelineStepResult step;
        step.stepIndex = ar.agentIndex;
        step.agentRole = "swarm_agent_" + std::to_string(ar.agentIndex);
        step.content = ar.output;
        step.confidence = ar.confidence;
        step.latencyMs = ar.latencyMs;
        step.success = ar.success;
        step.errorMsg = ar.errorMsg;
        result.steps.push_back(step);
    }

    return result;
}

// ============================================================================
// SWARM STRATEGIES
// ============================================================================

std::string ReasoningPipelineOrchestrator::swarmParallelVote(
    const std::vector<SwarmAgentResult>& results, float voteThreshold) const {

    if (results.empty()) return "No results available.";
    if (results.size() == 1) return results[0].output;

    // Find the response with the highest average similarity to all others
    int bestIdx = 0;
    int bestOverlap = -1;

    for (int i = 0; i < (int)results.size(); ++i) {
        int totalOverlap = 0;
        for (int j = 0; j < (int)results.size(); ++j) {
            if (i != j) {
                totalOverlap += wordOverlap(results[i].output, results[j].output);
            }
        }
        if (totalOverlap > bestOverlap) {
            bestOverlap = totalOverlap;
            bestIdx = i;
        }
    }

    return results[bestIdx].output;
}

std::string ReasoningPipelineOrchestrator::swarmTournament(
    std::vector<SwarmAgentResult>& results, int rounds,
    const std::string& input) const {

    if (results.empty()) return "No results available.";
    if (results.size() == 1) return results[0].output;

    // Tournament: in each round, compare pairs and eliminate the loser
    auto remaining = results;

    for (int round = 0; round < rounds && remaining.size() > 1; ++round) {
        std::vector<SwarmAgentResult> winners;
        for (size_t i = 0; i + 1 < remaining.size(); i += 2) {
            // Compare by word overlap with input (proxy for relevance)
            int score_a = wordOverlap(remaining[i].output, input);
            int score_b = wordOverlap(remaining[i+1].output, input);

            // Also factor in length (prefer substantive answers)
            score_a += (int)(remaining[i].output.size() / 100);
            score_b += (int)(remaining[i+1].output.size() / 100);

            winners.push_back(score_a >= score_b ? remaining[i] : remaining[i+1]);
        }
        // If odd number, last one gets a bye
        if (remaining.size() % 2 == 1) {
            winners.push_back(remaining.back());
        }
        remaining = winners;
    }

    return remaining.empty() ? results[0].output : remaining[0].output;
}

std::string ReasoningPipelineOrchestrator::swarmEnsemble(
    const std::vector<SwarmAgentResult>& results, float weightDecay) const {

    if (results.empty()) return "No results available.";
    if (results.size() == 1) return results[0].output;

    // Ensemble: build a combined answer by weighting each response
    // For text, we use the response with highest combined score
    // (word overlap with others * position weight)
    int bestIdx = 0;
    float bestScore = -1.0f;

    for (int i = 0; i < (int)results.size(); ++i) {
        float weight = std::pow(weightDecay, (float)i); // Earlier agents weighted higher
        float relevance = 0.0f;
        for (int j = 0; j < (int)results.size(); ++j) {
            if (i != j) {
                relevance += (float)wordOverlap(results[i].output, results[j].output);
            }
        }
        float score = weight * relevance + (float)results[i].output.size() * 0.01f;
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    return results[bestIdx].output;
}

// ============================================================================
// FALLBACK & VALIDATION
// ============================================================================

std::string ReasoningPipelineOrchestrator::enforceFinalAnswer(
    const std::string& finalOutput,
    const std::vector<PipelineStepResult>& steps,
    const ReasoningProfile& profile) const {

    // Try to get content from the last successful step
    if (profile.reasoning.allowFallback) {
        for (int i = (int)steps.size() - 1; i >= 0; --i) {
            if (steps[i].success && !steps[i].content.empty()) {
                // Check it's not just whitespace
                if (steps[i].content.find_first_not_of(" \t\r\n") != std::string::npos) {
                    return steps[i].content;
                }
            }
        }
    }

    // Last resort: return a generic message
    return "I processed your request but was unable to generate a complete response. "
           "Please try rephrasing or adjusting the reasoning depth.";
}

float ReasoningPipelineOrchestrator::estimateConfidence(const std::string& output) const {
    if (output.empty()) return 0.0f;

    float confidence = 0.5f; // Base

    // Length heuristic: very short answers are less confident
    if (output.size() < 20)  confidence -= 0.2f;
    if (output.size() > 200) confidence += 0.1f;
    if (output.size() > 500) confidence += 0.1f;

    // Hedging language reduces confidence
    std::string lower;
    lower.resize(output.size());
    std::transform(output.begin(), output.end(), lower.begin(),
                   [](char c) { return (char)std::tolower((unsigned char)c); });

    if (lower.find("i'm not sure") != std::string::npos) confidence -= 0.15f;
    if (lower.find("might be") != std::string::npos)     confidence -= 0.1f;
    if (lower.find("probably") != std::string::npos)      confidence -= 0.05f;
    if (lower.find("i think") != std::string::npos)       confidence -= 0.05f;
    if (lower.find("unclear") != std::string::npos)       confidence -= 0.1f;

    // Structured content increases confidence
    if (lower.find("```") != std::string::npos)   confidence += 0.1f;
    if (lower.find("1.") != std::string::npos &&
        lower.find("2.") != std::string::npos)     confidence += 0.1f;
    if (lower.find("therefore") != std::string::npos) confidence += 0.05f;
    if (lower.find("conclusion") != std::string::npos) confidence += 0.05f;

    return (std::max)(0.0f, (std::min)(confidence, 1.0f));
}

float ReasoningPipelineOrchestrator::estimateQuality(
    const std::string& output, const std::string& input) const {

    if (output.empty()) return 0.0f;

    float quality = 0.5f;

    // Relevance: overlap with input
    int overlap = wordOverlap(output, input);
    quality += (std::min)((float)overlap * 0.02f, 0.2f);

    // Substance: length relative to input
    float ratio = (float)output.size() / (float)((std::max)((size_t)1, input.size()));
    if (ratio > 1.5f) quality += 0.1f;
    if (ratio > 3.0f) quality += 0.1f;

    // Formatting: lists, code blocks, etc.
    if (output.find("```") != std::string::npos) quality += 0.05f;
    if (output.find("\n-") != std::string::npos || output.find("\n*") != std::string::npos)
        quality += 0.05f;

    return (std::max)(0.0f, (std::min)(quality, 1.0f));
}

// ============================================================================
// VISIBILITY FORMATTING
// ============================================================================

PipelineResult::VisibleOutput ReasoningPipelineOrchestrator::formatVisibleOutput(
    const PipelineResult& result, const ReasoningProfile& profile) const {

    PipelineResult::VisibleOutput vis;
    vis.finalAnswer = result.finalAnswer;
    vis.totalSteps = (int)result.steps.size();

    switch (profile.reasoning.visibility) {
    case ReasoningVisibility::FinalOnly:
        // Only the final answer — nothing else
        break;

    case ReasoningVisibility::ProgressBar:
        vis.showProgress = true;
        break;

    case ReasoningVisibility::StepSummary:
        vis.showProgress = true;
        for (const auto& step : result.steps) {
            if (step.success) {
                // First 80 chars + "..."
                std::string summary = step.agentRole + ": ";
                if (step.content.size() > 80) {
                    summary += step.content.substr(0, 80) + "...";
                } else {
                    summary += step.content;
                }
                vis.stepSummaries.push_back(summary);
            }
        }
        break;

    case ReasoningVisibility::FullCoT:
        vis.showProgress = true;
        for (const auto& step : result.steps) {
            std::string full = "[" + step.agentRole + "]\n" + step.content;
            vis.fullCoTSteps.push_back(full);
            vis.stepSummaries.push_back(step.agentRole + ": " + step.content);
        }
        break;

    default:
        break;
    }

    // Optional timing exposure
    if (profile.reasoning.exposeStepTimings) {
        for (const auto& step : result.steps) {
            vis.stepTimings.push_back(step.latencyMs);
        }
    }

    // Optional confidence exposure
    if (profile.reasoning.exposeConfidence) {
        for (const auto& step : result.steps) {
            vis.stepConfidences.push_back(step.confidence);
        }
    }

    return vis;
}

// ============================================================================
// THERMAL MONITOR
// ============================================================================

void ReasoningPipelineOrchestrator::startThermalMonitor() {
    if (m_thermalMonitorRunning.load()) return;
    m_thermalStopFlag.store(false);
    m_thermalMonitorRunning.store(true);

    m_thermalThread = std::thread([this]() {
        thermalMonitorLoop();
    });
}

void ReasoningPipelineOrchestrator::stopThermalMonitor() {
    m_thermalStopFlag.store(true);
    if (m_thermalThread.joinable()) {
        m_thermalThread.join();
    }
    m_thermalMonitorRunning.store(false);
}

bool ReasoningPipelineOrchestrator::isThermalMonitorRunning() const {
    return m_thermalMonitorRunning.load();
}

void ReasoningPipelineOrchestrator::thermalMonitorLoop() {
    auto& profMgr = ReasoningProfileManager::instance();

    while (!m_thermalStopFlag.load()) {
        ReasoningProfile profile = profMgr.getProfile();
        if (!profile.thermal.enabled) {
            Sleep((DWORD)profile.thermal.pollIntervalMs);
            continue;
        }

        float cpuUsage = profMgr.readSystemCpuUsage();
        ThermalState newState = profMgr.classifyThermal(cpuUsage);
        profMgr.updateThermalState(newState);

        Sleep((DWORD)profile.thermal.pollIntervalMs);
    }
}

// ============================================================================
// SELF-TUNE INTEGRATION
// ============================================================================

void ReasoningPipelineOrchestrator::feedUserFeedback(
    uint64_t requestId, bool accepted, bool edited) {

    auto& profMgr = ReasoningProfileManager::instance();
    auto recentTelemetry = profMgr.getRecentTelemetry(1);
    if (recentTelemetry.empty()) return;

    const auto& last = recentTelemetry.back();

    SelfTuneObservation obs;
    obs.requestId = requestId;
    obs.latencyMs = last.totalLatencyMs;
    obs.qualityScore = last.qualityEstimate;
    obs.confidence = last.finalConfidence;
    obs.depthUsed = last.effectiveDepth;
    obs.modeUsed = last.effectiveMode;
    obs.complexity = last.inputComplexity;
    obs.userAccepted = accepted;
    obs.userEdited = edited;

    // Adjust quality score based on user feedback
    if (accepted && !edited) {
        obs.qualityScore = (std::max)(obs.qualityScore, 0.8f);
    } else if (accepted && edited) {
        obs.qualityScore = (std::max)(obs.qualityScore * 0.7f, 0.4f);
    } else {
        obs.qualityScore = (std::min)(obs.qualityScore * 0.4f, 0.3f);
    }

    auto now = std::chrono::system_clock::now();
    obs.timestampEpoch = (double)std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();

    profMgr.feedObservation(obs);
}

SelfTuneState ReasoningPipelineOrchestrator::getSelfTuneState() const {
    return ReasoningProfileManager::instance().getSelfTuneState();
}

void ReasoningPipelineOrchestrator::emitTelemetry(
    const PipelineResult& result, const std::string& input) {

    auto& profMgr = ReasoningProfileManager::instance();

    ReasoningTelemetry t;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        t.requestId = m_requestCounter;
    }
    t.totalLatencyMs = result.totalLatencyMs;
    t.stepsExecuted = (int)result.steps.size();
    t.stepsSkipped = 0;
    for (const auto& s : result.steps) {
        if (s.skipped) t.stepsSkipped++;
    }
    t.finalConfidence = result.finalConfidence;
    t.qualityEstimate = estimateQuality(result.finalAnswer, input);
    t.effectiveDepth = result.effectiveDepth;
    t.effectiveMode = result.effectiveMode;
    t.thermalAtStart = profMgr.getThermalState();
    t.inputComplexity = result.inputComplexity;
    t.wasAutoBypass = result.wasBypassed;
    t.wasAdaptiveAdjusted = result.wasAdaptiveAdjusted;
    t.wasThermalThrottled = result.wasThermalThrottled;
    t.usedSwarm = result.usedSwarm;

    profMgr.recordTelemetry(t);

    // Also feed to self-tuner if enabled
    ReasoningProfile profile = profMgr.getProfile();
    if (profile.selfTune.enabled) {
        SelfTuneObservation obs;
        obs.requestId = t.requestId;
        obs.latencyMs = t.totalLatencyMs;
        obs.qualityScore = t.qualityEstimate;
        obs.confidence = t.finalConfidence;
        obs.depthUsed = t.effectiveDepth;
        obs.modeUsed = t.effectiveMode;
        obs.complexity = t.inputComplexity;
        obs.userAccepted = true;  // Default until feedback received
        obs.userEdited = false;

        auto now = std::chrono::system_clock::now();
        obs.timestampEpoch = (double)std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();

        profMgr.feedObservation(obs);
    }
}

// ============================================================================
// STATISTICS
// ============================================================================

ReasoningPipelineOrchestrator::OrchestratorStats
ReasoningPipelineOrchestrator::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void ReasoningPipelineOrchestrator::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    memset(&m_stats, 0, sizeof(m_stats));
    m_ewmaLatency = 0.0;
    m_ewmaInitialized = false;
}
