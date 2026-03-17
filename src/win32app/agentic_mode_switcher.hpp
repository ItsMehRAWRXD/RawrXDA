#pragma once
// ============================================================================
// agentic_mode_switcher.hpp — Plan / Agent / Ask Mode Architecture
// ============================================================================
// Three-mode agentic UI: Plan (stabilize → checklist → approve), Agent
// (autonomous todo + subagent), Ask (Q&A with verification).
// ============================================================================

#include <string>

namespace RawrXD {

enum class AgenticMode {
    Ask         = 0,  // Question answering, verification, citations
    Plan        = 1,  // Research via subagent → structured plan → user approval
    Agent       = 2,  // Autonomous execution: manage_todo_list + runSubagent, streaming
    DeepThink   = 3,  // Extended reasoning: multi-pass chain-of-thought before response
    DeepResearch = 4  // Long-horizon research: iterative web/codebase search + synthesis
};

inline const char* AgenticModeToString(AgenticMode m) {
    switch (m) {
        case AgenticMode::Ask:          return "Ask";
        case AgenticMode::Plan:         return "Plan";
        case AgenticMode::Agent:        return "Agent";
        case AgenticMode::DeepThink:    return "DeepThink";
        case AgenticMode::DeepResearch: return "DeepResearch";
        default:                        return "Ask";
    }
}

inline AgenticMode AgenticModeFromInt(int i) {
    switch (i) {
        case 1: return AgenticMode::Plan;
        case 2: return AgenticMode::Agent;
        case 3: return AgenticMode::DeepThink;
        case 4: return AgenticMode::DeepResearch;
        default: return AgenticMode::Ask;
    }
}

inline int AgenticModeToInt(AgenticMode m) {
    return static_cast<int>(m);
}

} // namespace RawrXD
