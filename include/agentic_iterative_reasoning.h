#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>
#include "../src/cpu_inference_engine.h"

class AgenticLoopState;
class AgenticEngine;

/**
 * @class AgenticIterativeReasoning
 * @brief Full iterative reasoning loop with reflection, adjustment, and learning
 * 
 * Complete reasoning loop lifecycle:
 * 1. ANALYSIS: Understand the problem and gather context
 * 2. PLANNING: Generate multiple strategies and select best
 * 3. EXECUTION: Execute chosen action with monitoring
 * 4. VERIFICATION: Verify results match expectations
 * 5. REFLECTION: Analyze what worked and what didn't
 * 6. ADJUSTMENT: Update strategy based on reflection
 * 7. LOOP: Repeat until goal achieved or max iterations exceeded
 * 
 * Features:
 * - Multi-turn reasoning with memory
 * - Strategy generation and evaluation
 * - Error recovery with backtracking
 * - Learning from past decisions
 * - Explainable reasoning traces
 */
class AgenticIterativeReasoning
{
public:
    struct IterationResult {
        bool success;
        std::string result;
        std::string reasoning;
        nlohmann::json decisionTrace;
        float confidence;
        int iterationCount;
        int totalTime; // milliseconds
    };

    struct Strategy {
        std::string name;
        std::string description;
        float estimatedSuccessProbability;
        std::vector<std::string> requiredTools;
        nlohmann::json parameters;
        int complexity; // 1-10
    };

    // Callback types for signals replacement
    using StringCallback = std::function<void(const std::string&)>;
    using IterationCallback = std::function<void(int)>;
    using PhaseCallback = std::function<void(const std::string&, const std::string&)>;
    using BoolStringCallback = std::function<void(bool, const std::string&)>;
    using ErrorCallback = std::function<void(const std::string&, int)>;
    using ResultCallback = std::function<void(int, bool)>;

public:
    explicit AgenticIterativeReasoning(void* parent = nullptr);
    ~AgenticIterativeReasoning();

    void initialize(AgenticEngine* engine, AgenticLoopState* state, RawrXD::InferenceEngine* inference);

    // Main entry point for iterative reasoning
    IterationResult reason(
        const std::string& goal,
        int maxIterations = 10,
        int timeoutSeconds = 300
    );

    // Phase implementations - callable for testing
    std::string analyzeGoal(const std::string& goal);
    std::vector<Strategy> generateStrategies(const std::string& goal, const std::string& context);
    Strategy selectStrategy(const std::vector<Strategy>& strategies);
    
    nlohmann::json planExecution(const std::string& goal, const Strategy& strategy);
    nlohmann::json executeAction(const nlohmann::json& plan);
    
    bool verifyResult(const std::string& expectedOutcome, const nlohmann::json& actualResult);
    std::string reflectOnIteration(const nlohmann::json& iterationData);
    nlohmann::json adjustStrategy(const std::string& reflection);

    // Error handling within reasoning loop
    bool handleReasoningError(const std::string& error, int& retryCount);
    std::string generateBacktrackingStrategy(const std::string& lastFailure);

    // Get information about reasoning process
    nlohmann::json getReasoningTrace() const { return m_reasoningTrace; }
    std::string getReasoningExplanation() const;
    nlohmann::json getMetrics() const;

    // Configuration
    void setMaxIterations(int max) { m_maxIterations = max; }
    void setTimeoutSeconds(int seconds) { m_timeoutSeconds = seconds; }
    void setReflectionEnabled(bool enabled) { m_enableReflection = enabled; }
    void setVerboseLogging(bool enabled) { m_verboseLogging = enabled; }

    // Callbacks (formerly signals)
    IterationCallback onIterationStarted;
    PhaseCallback onPhaseCompleted;
    StringCallback onStrategySelected;
    StringCallback onExecutionStarted;
    BoolStringCallback onVerificationResult;
    StringCallback onReflectionGenerated;
    StringCallback onAdjustmentApplied;
    ResultCallback onIterationCompleted;
    StringCallback onReasoningCompleted;
    ErrorCallback onErrorOccurred;

private:
    // Phase handlers
    std::string executeAnalysisPhase(const std::string& goal);
    std::vector<Strategy> executePlanningPhase(const std::string& goal, const std::string& analysis);
    nlohmann::json executeExecutionPhase(const Strategy& strategy, const std::string& goal);
    bool executeVerificationPhase(const std::string& goal, const nlohmann::json& result);
    std::string executeReflectionPhase(
        int iterationNumber,
        const std::string& goal,
        const nlohmann::json& iterationData
    );
    
    // Helper methods
    Strategy pickBestStrategy(const std::vector<Strategy>& strategies);
    std::vector<std::string> generateAlternativeApproaches(const std::string& goal);
    nlohmann::json buildIterationContext(int iteration, const std::string& goal);
    bool hasConverged(const std::vector<std::string>& recentResults);
    bool shouldRetry(const std::string& error) const;

    // Model-based reasoning
    std::string callModelForReasoning(const std::string& prompt, const std::string& context = "");
    nlohmann::json extractStructuredResponse(const std::string& modelResponse);
    
    // Metrics collection
    void recordIterationMetrics(
        int iterationNumber,
        const std::string& phase,
        int durationMs
    );

    // Logging
    void log(const std::string& message, const std::string& level = "INFO");

    // Pointers to connected engines
    AgenticEngine* m_engine = nullptr;
    AgenticLoopState* m_state = nullptr;
    RawrXD::InferenceEngine* m_inferenceEngine = nullptr;

    // Configuration
    int m_maxIterations = 10;
    int m_timeoutSeconds = 300;
    bool m_enableReflection = true;
    bool m_verboseLogging = false;

    // Runtime state
    std::chrono::system_clock::time_point m_loopStartTime;
    int m_currentIteration = 0;
    std::vector<std::string> m_recentResults;
    std::vector<Strategy> m_failedStrategies;
    std::string m_currentGoal;

    // Trace and metrics
    nlohmann::json m_reasoningTrace;
    nlohmann::json m_metrics;
    std::vector<std::pair<std::string, int>> m_phaseDurations;
};
