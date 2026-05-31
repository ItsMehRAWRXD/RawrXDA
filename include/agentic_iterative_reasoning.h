/**
 * @file agentic_iterative_reasoning.h
 * @brief Production iterative reasoning loop for multi-step agent reflection
 *
 * C++20 / Win32 / MASM build (no Qt)
 * Provides: problem decomposition, hypothesis generation, evidence evaluation,
 *           strategy selection, and self-correction for autonomous agents.
 */
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>

class AgenticLoopState;
class AgenticEngine;
class InferenceEngine;

/**
 * @struct ReasoningStep
 * @brief Single step in the iterative reasoning chain
 */
struct ReasoningStep {
    int stepNumber = 0;
    std::string type;           // "analyze", "hypothesize", "experiment", "evaluate", "correct"
    std::string description;
    std::string input;
    std::string output;
    float confidence = 0.0f;
    bool successful = false;
    std::vector<std::string> findings;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @struct ReasoningStrategy
 * @brief Strategy configuration for reasoning adaptation
 */
struct ReasoningStrategy {
    std::string name;
    int maxIterations = 5;
    float confidenceThreshold = 0.7f;
    bool enableSelfCorrection = true;
    bool enableHypothesisTesting = true;
    int maxHypotheses = 3;
    float explorationFactor = 0.3f;  // Balance between exploitation and exploration
};

/**
 * @class AgenticIterativeReasoning
 * @brief Production iterative reasoning loop with multi-step reflection
 */
class AgenticIterativeReasoning {
public:
    AgenticIterativeReasoning();
    ~AgenticIterativeReasoning();

    // Initialize with engine dependencies
    void initialize(AgenticEngine* engine, AgenticLoopState* state, InferenceEngine* inference);
    bool isInitialized() const { return m_initialized; }

    // Core reasoning cycle
    std::vector<ReasoningStep> reason(const std::string& problem, const ReasoningStrategy& strategy);
    
    // Strategy selection based on problem characteristics
    ReasoningStrategy selectStrategy(const std::string& problem) const;
    
    // Individual reasoning phases
    ReasoningStep analyzeProblem(const std::string& problem);
    std::vector<ReasoningStep> generateHypotheses(const std::string& problem, int maxHypotheses);
    ReasoningStep testHypothesis(const std::string& hypothesis, const std::string& problem);
    ReasoningStep evaluateEvidence(const std::vector<ReasoningStep>& hypotheses);
    ReasoningStep selfCorrect(const ReasoningStep& failedStep, const std::string& feedback);
    
    // Confidence scoring
    float calculateConfidence(const std::vector<ReasoningStep>& steps) const;
    bool meetsThreshold(const std::vector<ReasoningStep>& steps, float threshold) const;
    
    // State management
    void reset();
    std::vector<ReasoningStep> getHistory() const;
    std::string getLastError() const { return m_lastError; }
    
    // Callbacks for progress monitoring
    std::function<void(const ReasoningStep&)> onStepCompleted;
    std::function<void(float)> onConfidenceUpdated;
    std::function<void(const std::string&)> onError;

private:
    AgenticEngine* m_engine = nullptr;
    AgenticLoopState* m_state = nullptr;
    InferenceEngine* m_inference = nullptr;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_reasoning{false};
    mutable std::mutex m_mutex;
    
    std::vector<ReasoningStep> m_history;
    std::string m_lastError;
    
    // Internal helpers
    std::string queryLLM(const std::string& prompt, int maxTokens = 500);
    std::vector<std::string> extractKeyIssues(const std::string& problem);
    std::string categorizeProblem(const std::string& problem) const;
    bool validateHypothesis(const std::string& hypothesis) const;
    float scoreEvidence(const std::string& evidence, const std::string& problem) const;
};
