#pragma once
#include <vector>
#include <string>
#include <memory>
#include <expected>
#include <unordered_map>
#include <chrono>

namespace RawrXD {

class CPUInferenceEngine; 

enum class ChainError {
    Success = 0,
    StepGenerationFailed,
    ValidationFailed,
    BacktrackingFailed,
    MaxDepthExceeded,
    Timeout
};

struct ThoughtStep {
    std::string id;
    std::string content;
    std::string reasoning;
    float confidence;
    std::vector<std::string> premises;
    std::vector<std::string> conclusions;
    std::chrono::steady_clock::time_point timestamp;
    bool isValidated = false;
    bool isRevised = false;
};

struct ReasoningChain {
    std::string id;
    std::string goal;
    std::vector<ThoughtStep> steps;
    std::unordered_map<std::string, std::string> context;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    bool isComplete = false;
    float overallConfidence = 0.0f;
};

class ChainOfThought {
public:
    ChainOfThought();
    ~ChainOfThought() = default;
    
    std::expected<ReasoningChain, ChainError> generateChain(
        const std::string& goal,
        const std::unordered_map<std::string, std::string>& context
    );
    
    std::expected<std::string, ChainError> generateExplanation(
        const ReasoningChain& chain
    );
    
private:
    // CPUInferenceEngine* m_inferenceEngine;
    // Using void* to avoid include cycle if any
    void* m_inferenceEngine = nullptr;

    std::expected<ThoughtStep, ChainError> generateNextStep(
        const ReasoningChain& chain,
        const std::vector<ThoughtStep>& previousSteps
    );
    
    std::string generateChainId() { return "chain_1"; }
    std::string generateStepId() { return "step_1"; }
};

} // namespace RawrXD
