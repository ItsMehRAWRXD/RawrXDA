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
    Ask = 0,   // Question answering, verification, citations
    Plan = 1,  // Research via subagent → structured plan → user approval
    Agent = 2  // Autonomous execution: manage_todo_list + runSubagent, streaming
};

inline const char* AgenticModeToString(AgenticMode m) {
    switch (m) {
        case AgenticMode::Ask:   return "Ask";
        case AgenticMode::Plan:  return "Plan";
        case AgenticMode::Agent: return "Agent";
        default: return "Ask";
    }
}

inline AgenticMode AgenticModeFromInt(int i) {
    if (i == 1) return AgenticMode::Plan;
    if (i == 2) return AgenticMode::Agent;
    return AgenticMode::Ask;
}

inline int AgenticModeToInt(AgenticMode m) {
    return static_cast<int>(m);
}

} // namespace RawrXD
