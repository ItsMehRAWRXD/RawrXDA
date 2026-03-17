#pragma once

#include <string>
#include <memory>
#include <iostream>

class AgenticEngine;
class AgenticLoopState;
namespace CPUInference { class CPUInferenceEngine; }

struct ReasoningResult {
    bool success;
    std::string result;
    std::string error;
};

class AgenticIterativeReasoning {
public:
    AgenticIterativeReasoning();
    ~AgenticIterativeReasoning();

    void initialize(AgenticEngine* engine, AgenticLoopState* state, CPUInference::CPUInferenceEngine* inference);
    ReasoningResult reason(const std::string& goal);

private:
    AgenticEngine* m_engine = nullptr;
    AgenticLoopState* m_state = nullptr;
    CPUInference::CPUInferenceEngine* m_inference = nullptr;
};
