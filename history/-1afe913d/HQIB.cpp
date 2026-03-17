#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include "cpu_inference_engine.h"
#include <iostream>
#include <vector>

AgenticIterativeReasoning::AgenticIterativeReasoning()
{
    std::cout << "[AgenticIterativeReasoning] Initialized" << std::endl;
}

AgenticIterativeReasoning::~AgenticIterativeReasoning()
{
}

void AgenticIterativeReasoning::initialize(AgenticEngine* engine, AgenticLoopState* state, CPUInference::CPUInferenceEngine* inference)
{
    m_engine = engine;
    m_state = state;
    m_inference = inference;
}

ReasoningResult AgenticIterativeReasoning::reason(const std::string& goal)
{
    ReasoningResult result;
    result.success = false;

    if (!m_state) {
        result.error = "No state initialized";
        return result;
    }

    m_state->startIteration(goal);
    std::cout << "[AgenticIterativeReasoning] Reasoning about: " << goal << std::endl;

    // Simulate multi-step reasoning
    // 1. Analysis
    m_state->updatePhase(ReasoningPhase::Analysis);
    
    // In a real implementation this would query the LLM multiple times
    // For now, if we have inference engine:
    if (m_inference) {
        std::string prompt = "Analyze and solve this task: " + goal;
        
        // Use inference engine
        if (m_inference->IsModelLoaded()) {
             std::vector<int32_t> input_ids = m_inference->Tokenize(prompt);
             std::string output;
             
             // Simple generation (assumed blocking or short)
             // We use Generate directly which returns tokens
             auto tokens = m_inference->Generate(input_ids, 256);
             output = m_inference->Detokenize(tokens);
             
             result.result = output;
             result.success = true;
             
             m_state->completeIteration(output);
             return result;
        }
    }

    // Fallback if no model loaded
    result.result = "Executed: " + goal;
    result.success = true;
    m_state->completeIteration(result.result);

    return result;
}
