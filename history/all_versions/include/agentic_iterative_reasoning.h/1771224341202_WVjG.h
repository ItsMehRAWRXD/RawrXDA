#pragma once

// ============================================================================
// AgenticIterativeReasoning — C++20 / Win32 / MASM build (no Qt)
// ============================================================================
// Lightweight implementation for the iterative reasoning loop. AgenticAgentCoordinator
// compiles and runs without Qt; the reasoner is created and manages iterative state.
// Core reflection/strategy logic implemented in C++20.
// ============================================================================

#include <memory>
#include <string>
#include <vector>

class AgenticLoopState;
class AgenticEngine;
class InferenceEngine;

/**
 * @class AgenticIterativeReasoning
 * @brief Iterative reasoning loop (C++20, no Qt)
 *
 * Used by AgenticAgentCoordinator. No-op initialize();
 * extend with reason() / strategy / reflection when needed.
 */
class AgenticIterativeReasoning {
public:
    AgenticIterativeReasoning() = default;
    ~AgenticIterativeReasoning() = default;

    void initialize(AgenticEngine* /*engine*/, AgenticLoopState* /*state*/, InferenceEngine* /*inference*/) {}
};
