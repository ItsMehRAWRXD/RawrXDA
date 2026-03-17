// ============================================================================
// reasoning_pipeline_orchestrator.h — Tunable Reasoning Pipeline Orchestrator
// ============================================================================
//
// The central orchestration engine that routes user input through the
// configurable multi-agent reasoning pipeline. All behavior is governed
// by the active ReasoningProfile ("P-settings").
//
// Capabilities:
//   1. Input classification & smart routing
//   2. Configurable multi-step agent chain
//   3. Adaptive depth adjustment (latency-aware)
//   4. Thermal-aware throttling
//   5. Confidence-based auto-depth
//   6. Swarm/parallel reasoning mode
//   7. Self-tuning PID controller
//   8. Fallback enforcement (never empty responses)
//   9. Full telemetry & observability
//  10. Safe CoT visibility control
//
// Pattern:  Structured results, no exceptions
// Threading: All methods are thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef RAWRXD_REASONING_PIPELINE_ORCHESTRATOR_H
#define RAWRXD_REASONING_PIPELINE_ORCHESTRATOR_H

#include "reasoning_profile.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <thread>

// Forward declarations
struct CoTChainResult;
struct CoTStep;
struct CoTStepResult;
enum class CoTRoleId;

// ============================================================================
// PipelineStepResult — result from one reasoning step in the pipeline
// ============================================================================
struct PipelineStepResult {
    int             stepIndex       = -1;
    std::string     agentRole;              // Name of the agent that produced this
    std::string     content;                // The agent's output text
    float           confidence      = 0.0f; // Confidence in this step's output
    double          latencyMs       = 0.0;
    int             tokenCount      = 0;
    bool            success         = false;
    bool            skipped         = false;
    std::string     error;
};

// ============================================================================
// PipelineResult — full result of a pipeline execution
// ============================================================================
struct PipelineResult {
    bool            success             = false;
    std::string     finalAnswer;                    // The answer shown to the user
    std::vector<PipelineStepResult> steps;          // All intermediate steps
    float           finalConfidence     = 0.0f;
    double          totalLatencyMs      = 0.0;
    int             effectiveDepth      = 0;
    ReasoningMode   effectiveMode       = ReasoningMode::Normal;
    InputComplexity inputComplexity     = InputComplexity::Moderate;
    bool            wasBypassed         = false;    // Skipped pipeline entirely
    bool            wasAdaptiveAdjusted = false;    // Depth was auto-adjusted
    bool            wasThermalThrottled = false;     // Thermal forced reduction
    bool            usedSwarm           = false;     // Swarm reasoning was used
    bool            usedFallback        = false;     // Fell back from empty final
    std::string     error;

    // Visible output (respects visibility settings)
    struct VisibleOutput {
        std::string     finalAnswer;
        std::vector<std::string> stepSummaries;     // Only if visibility >= StepSummary
        std::vector<std::string> fullCoTSteps;      // Only if visibility == FullCoT
        std::vector<double>      stepTimings;        // Only if exposeStepTimings
        std::vector<float>       stepConfidences;    // Only if exposeConfidence
        bool            showProgress    = false;
        int             totalSteps      = 0;
    } visible;

    static PipelineResult ok(const std::string& answer) {
        PipelineResult r;
        r.success = true;
        r.finalAnswer = answer;
        r.visible.finalAnswer = answer;
        return r;
    }

    static PipelineResult error(const std::string& msg) {
        PipelineResult r;
        r.success = false;
        r.error = msg;
        r.finalAnswer = "Response unavailable.";
        r.visible.finalAnswer = r.finalAnswer;
        return r;
    }
};

// ============================================================================
// SwarmAgentResult — result from one agent in a swarm
// ============================================================================
struct SwarmAgentResult {
    int             agentIndex;
    std::string     model;
    std::string     output;
    float           confidence;
    double          latencyMs;
    bool            success;
    std::string     error;
};

// ============================================================================
// Inference callback types
// ============================================================================

// Standard inference: system prompt + user message + model → response text
using InferenceCallback = std::function<std::string(
    const std::string& systemPrompt,
    const std::string& userMessage,
    const std::string& model
)>;

// Streaming inference callback (optional)
using StreamingInferenceCallback = std::function<void(
    const std::string& systemPrompt,
    const std::string& userMessage,
    const std::string& model,
    std::function<void(const std::string& chunk)> onChunk,
    std::function<void()> onComplete,
    std::function<void(const std::string& error)> onError
)>;

// ============================================================================
// ReasoningPipelineOrchestrator — the main orchestration engine
// ============================================================================
class ReasoningPipelineOrchestrator {
public:
    // Singleton access
    static ReasoningPipelineOrchestrator& instance();

    // --- Configuration ---
    void setInferenceCallback(InferenceCallback cb);
    void setStreamingCallback(StreamingInferenceCallback cb);
    void setDefaultModel(const std::string& model);

    // --- Primary API ---

    // Execute the full pipeline for a user input.
    // Respects all P-settings: mode, depth, routing, adaptive, thermal, swarm.
    PipelineResult execute(const std::string& userInput,
                           const std::string& context = "");

    // Cancel a running pipeline
    void cancel();
    bool isRunning() const;

    // --- Direct Mode Execution (bypass routing) ---
    PipelineResult executeFast(const std::string& input, const std::string& context = "");
    PipelineResult executeNormal(const std::string& input, const std::string& context = "");
    PipelineResult executeDeep(const std::string& input, const std::string& context = "");
    PipelineResult executeCritical(const std::string& input, const std::string& context = "");
    PipelineResult executeSwarm(const std::string& input, const std::string& context = "");

    // --- Thermal Monitoring ---
    void startThermalMonitor();
    void stopThermalMonitor();
    bool isThermalMonitorRunning() const;

    // --- Self-Tuning ---
    void feedUserFeedback(uint64_t requestId, bool accepted, bool edited);
    SelfTuneState getSelfTuneState() const;

    // --- Statistics ---
    struct OrchestratorStats {
        uint64_t    totalExecutions;
        uint64_t    fastExecutions;
        uint64_t    normalExecutions;
        uint64_t    deepExecutions;
        uint64_t    criticalExecutions;
        uint64_t    swarmExecutions;
        uint64_t    bypassed;
        uint64_t    adaptiveAdjustments;
        uint64_t    thermalThrottles;
        uint64_t    fallbacksUsed;
        uint64_t    errorCount;
        double      avgLatencyMs;
        float       avgConfidence;
    };

    OrchestratorStats getStats() const;
    void resetStats();

private:
    ReasoningPipelineOrchestrator();
    ~ReasoningPipelineOrchestrator();
    ReasoningPipelineOrchestrator(const ReasoningPipelineOrchestrator&) = delete;
    ReasoningPipelineOrchestrator& operator=(const ReasoningPipelineOrchestrator&) = delete;

    // --- Internal Pipeline Stages ---
    InputComplexity classifyInput(const std::string& input) const;
    bool shouldBypass(const std::string& input, InputComplexity complexity) const;
    int  computeEffectiveDepth(const ReasoningProfile& profile,
                               InputComplexity complexity) const;
    int  applyAdaptiveAdjustment(int baseDepth, const ReasoningProfile& profile) const;
    int  applyThermalThrottling(int depth, const ReasoningProfile& profile) const;
    ReasoningMode resolveEffectiveMode(const ReasoningProfile& profile,
                                       InputComplexity complexity,
                                       int effectiveDepth) const;

    // --- Chain Building ---
    std::vector<std::string> buildAgentChain(const ReasoningProfile& profile,
                                              int effectiveDepth) const;
    std::string getSystemPromptForRole(const std::string& role) const;

    // --- Execution Engines ---
    PipelineResult runDirectLLM(const std::string& input,
                                const std::string& context) const;
    PipelineResult runChainedPipeline(const std::string& input,
                                      const std::string& context,
                                      const std::vector<std::string>& chain,
                                      const ReasoningProfile& profile);
    PipelineResult runSwarmPipeline(const std::string& input,
                                    const std::string& context,
                                    const ReasoningProfile& profile);

    // --- Swarm Strategies ---
    std::string swarmParallelVote(const std::vector<SwarmAgentResult>& results,
                                  float voteThreshold) const;
    std::string swarmTournament(std::vector<SwarmAgentResult>& results,
                                int rounds, const std::string& input) const;
    std::string swarmEnsemble(const std::vector<SwarmAgentResult>& results,
                               float weightDecay) const;

    // --- Fallback & Validation ---
    std::string enforceFinalAnswer(const std::string& finalOutput,
                                   const std::vector<PipelineStepResult>& steps,
                                   const ReasoningProfile& profile) const;
    float estimateConfidence(const std::string& output) const;
    float estimateQuality(const std::string& output, const std::string& input) const;

    // --- Visibility Formatting ---
    PipelineResult::VisibleOutput formatVisibleOutput(
        const PipelineResult& result,
        const ReasoningProfile& profile) const;

    // --- Thermal Monitor Thread ---
    void thermalMonitorLoop();

    // --- Self-Tune Integration ---
    void emitTelemetry(const PipelineResult& result, const std::string& input);

    // State
    mutable std::mutex              m_mutex;
    InferenceCallback               m_inferenceCallback;
    StreamingInferenceCallback      m_streamingCallback;
    std::string                     m_defaultModel;
    std::atomic<bool>               m_running{false};
    std::atomic<bool>               m_cancelled{false};
    uint64_t                        m_requestCounter = 0;

    // Thermal monitor
    std::thread                     m_thermalThread;
    std::atomic<bool>               m_thermalMonitorRunning{false};
    std::atomic<bool>               m_thermalStopFlag{false};

    // Latency tracking (EWMA)
    double                          m_ewmaLatency = 0.0;
    bool                            m_ewmaInitialized = false;

    // Stats
    OrchestratorStats               m_stats{};
};

#endif // RAWRXD_REASONING_PIPELINE_ORCHESTRATOR_H
