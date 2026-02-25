#include "chain_of_thought.h"
#include "../../include/agentic_engine.h"
#include <sstream>
#include <iostream>
#include <regex>
#include <iomanip>

namespace RawrXD {

ChainOfThought::ChainOfThought() {
    auto engine = new AgenticEngine();
    engine->initialize();
    m_inferenceEngine = engine;
}

ChainOfThought::~ChainOfThought() {
    if (m_inferenceEngine) {
        delete static_cast<AgenticEngine*>(m_inferenceEngine);
        m_inferenceEngine = nullptr;
    }
}

std::expected<ReasoningChain, ChainError> ChainOfThought::generateChain(
    const std::string& goal,
    const std::unordered_map<std::string, std::string>& context
) {
    if (!m_inferenceEngine) return std::unexpected(ChainError::StepGenerationFailed);
    
    ReasoningChain chain;
    chain.id = generateChainId();
    chain.goal = goal;
    chain.context = context;
    chain.startTime = std::chrono::steady_clock::now();
    
    AgenticEngine* engine = static_cast<AgenticEngine*>(m_inferenceEngine);
    
    // Construct Prompt
    std::stringstream promptSs;
    promptSs << "Generate a structured Chain of Thought for goal: \"" << goal << "\"\n";
    if (!context.empty()) {
        promptSs << "Context:\n";
        for (const auto& [k, v] : context) promptSs << "- " << k << ": " << v << "\n";
    }
    promptSs << "\nRequirement: Break the reasoning into numbered steps (1., 2., ...). For each step, provide 'Content', 'Reasoning', and 'Confidence' (0.0-1.0).";

    std::string response = engine->processQuery(promptSs.str());
    
    if (response.empty() || response.find("Error") != std::string::npos && response.length() < 20) {
        // Fallback for simulation detection
        return std::unexpected(ChainError::StepGenerationFailed);
    }
    
    // Parse the response
    std::stringstream ss(response);
    std::string line;
    ThoughtStep currentStep;
    bool parsingStep = false;
    int stepCount = 0;

    auto finalizeStep = [&]() {
        if (parsingStep) {
            if (currentStep.id.empty()) currentStep.id = "step_" + std::to_string(stepCount);
            chain.steps.push_back(currentStep);
            currentStep = ThoughtStep();
        }
    };

    while (std::getline(ss, line)) {
        // Trim line
        line.erase(0, line.find_first_not_of(" \t"));
        if (line.empty()) continue;
        
        // Detect Step start (e.g., "1. Analysis")
        if (std::isdigit(line[0]) && line.find(".") != std::string::npos) {
            finalizeStep();
            
            parsingStep = true;
            stepCount++;
            currentStep.content = line; 
            currentStep.timestamp = std::chrono::steady_clock::now();
        } else if (line.find("Reasoning:") != std::string::npos) {
            currentStep.reasoning = line.substr(line.find(":") + 1);
        } else if (line.find("Confidence:") != std::string::npos) {
            try {
                std::string confStr = line.substr(line.find(":") + 1);
                currentStep.confidence = std::stof(confStr);
            } catch(...) {
                currentStep.confidence = 0.8f;
            }
        } else {
            if (parsingStep) {
                 if (!currentStep.reasoning.empty() && line.find("Reasoning") == std::string::npos)
                     currentStep.reasoning += " " + line;
                 else
                     currentStep.content += " " + line;
            }
        }
    }
    finalizeStep();
    
    // Last ditch fallback if parsing found nothing but text exists
    if (chain.steps.empty() && !response.empty()) {
         ThoughtStep step;
         step.id = "step_1";
         step.content = "Reasoning Process (Unstructured)";
         step.reasoning = response;
         step.confidence = 0.5f;
         chain.steps.push_back(step);
    }

    chain.isComplete = true;
    chain.overallConfidence = 0.9f; 
    chain.endTime = std::chrono::steady_clock::now();
    
    return chain;
}

std::expected<ThoughtStep, ChainError> ChainOfThought::generateNextStep(
    const ReasoningChain& chain,
    const std::vector<ThoughtStep>& previousSteps
) {
    if (!m_inferenceEngine) return std::unexpected(ChainError::StepGenerationFailed);
    
    // Future: Iterative step generation
    return ThoughtStep(); 
}

std::expected<std::string, ChainError> ChainOfThought::generateExplanation(
    const ReasoningChain& chain
) {
    std::string explanation = "Reasoning for: " + chain.goal + "\n\n";
    for(const auto& step : chain.steps) {
        explanation += step.content + "\n";
        if (!step.reasoning.empty()) explanation += "   Reasoning: " + step.reasoning + "\n";
    }
    return explanation;
}

} // namespace RawrXD

} // namespace RawrXD
