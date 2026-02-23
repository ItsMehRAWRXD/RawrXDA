#include "agent_modes.h"
#include <string>

std::string build_system_prompt(AgentMode mode) {
    switch(mode) {
        case PLAN: return "You are a planning agent. Produce steps.";
        case EDIT: return "You are a code editing agent.";
        case BUGREPORT: return "You are a bug analysis agent.";
        case CODESUGGEST: return "You are a refactoring agent.";
        default: return "You are a helpful assistant.";
    }
}
