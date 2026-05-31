// ============================================================================
// ChatPanelModelCaller_SkillIntegration.cpp — Skill injection for Chat Panel
// ============================================================================
// This file provides the integration hook for the Chat Panel model caller.
// It enriches every chat inference request with active skill context.
//
// USAGE:
//   Include this file in ChatPanelModelCaller.cpp or add as separate compilation unit.
//   Call RawrXD::SkillSystem::Hook_ChatPanel_ModelInference before sending to backend.
// ============================================================================

#include "../skill_system/SkillInjectionHooks.h"
#include <string>

namespace RawrXD {
namespace Chat {

// ============================================================================
// CHAT PANEL SKILL INTEGRATION
// ============================================================================

// Called before EVERY chat model inference request
std::string EnrichChatPromptWithSkills(
    const std::string& originalPrompt,
    const std::string& conversationHistory,
    const std::string& activeModel,
    const std::string& currentPhase = "phase1"
) {
    return SkillSystem::Hook_ChatPanel_ModelInference(
        originalPrompt,
        conversationHistory,
        activeModel
    );
}

// Called when switching agent modes in chat
std::string EnrichChatPromptForAgentMode(
    const std::string& originalPrompt,
    const std::string& targetAgent,
    const std::string& currentPhase
) {
    return SkillSystem::Hook_AgentOrchestrator_TaskDispatch(
        originalPrompt,
        targetAgent,
        currentPhase
    );
}

// ============================================================================
// C-API for backward compatibility
// ============================================================================
extern "C" {
    __declspec(dllexport) const char* __stdcall ChatPanel_InjectSkillContext(
        const char* prompt,
        const char* history,
        const char* model
    ) {
        static std::string result;
        result = EnrichChatPromptWithSkills(
            prompt ? prompt : "",
            history ? history : "",
            model ? model : ""
        );
        return result.c_str();
    }
}

} // namespace Chat
} // namespace RawrXD
