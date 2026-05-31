// ============================================================================
// AgentOrchestrator_SkillIntegration.cpp — Skill injection for Agent Orchestrator
// ============================================================================
// Provides skill context injection for all agent orchestration tasks.
// Ensures every dispatched task carries the appropriate skill definitions.
//
// USAGE:
//   Include in agentic_orchestrator.cpp or compile as separate unit.
//   Call before dispatching any task to sub-agents.
// ============================================================================

#include "../skill_system/SkillInjectionHooks.h"
#include <string>

namespace RawrXD {
namespace Agent {

// ============================================================================
// AGENT ORCHESTRATOR SKILL INTEGRATION
// ============================================================================

// Called before dispatching ANY task to sub-agents
std::string EnrichAgentTaskWithSkills(
    const std::string& taskDescription,
    const std::string& targetAgent,
    const std::string& currentPhase
) {
    return SkillSystem::Hook_AgentOrchestrator_TaskDispatch(
        taskDescription,
        targetAgent,
        currentPhase
    );
}

// Called for autonomous mode task planning
std::string EnrichAutonomousPlanWithSkills(
    const std::string& highLevelGoal,
    const std::string& currentPhase
) {
    // Autonomous mode uses AgentPolish skills
    return SkillSystem::Hook_AgentOrchestrator_TaskDispatch(
        highLevelGoal,
        "@AgentPolish",
        currentPhase
    );
}

// Called when spawning sub-agents
std::string EnrichSubAgentSpawnWithSkills(
    const std::string& spawnContext,
    const std::string& specialistAgent
) {
    return SkillSystem::Hook_AgentOrchestrator_TaskDispatch(
        spawnContext,
        specialistAgent,
        "phase1"  // Default to phase1 for sub-agent spawning
    );
}

// ============================================================================
// C-API for backward compatibility
// ============================================================================
extern "C" {
    __declspec(dllexport) const char* __stdcall AgentOrchestrator_InjectSkillContext(
        const char* task,
        const char* agent,
        const char* phase
    ) {
        static std::string result;
        result = EnrichAgentTaskWithSkills(
            task ? task : "",
            agent ? agent : "",
            phase ? phase : ""
        );
        return result.c_str();
    }
}

} // namespace Agent
} // namespace RawrXD
