// ============================================================================
// SkillInjectionHooks.h — Integration with RawrXD Agentic/Model Systems
// ============================================================================
// This file provides the CRITICAL guarantee: first 520 lines of skill context
// are ALWAYS injected regardless of model/agent status.
//
// Hook points:
//   1. GhostText completion requests (Win32IDE_GhostText.cpp)
//   2. Chat panel AI backend calls (ChatPanelModelCaller.cpp)
//   3. Agent orchestrator task dispatch (agentic_orchestrator.cpp)
//   4. LSP completion provider (ai_completion_provider.cpp)
//   5. Any external model API call
// ============================================================================

#pragma once

#include "SkillInjectionEngine.h"
#include <string>
#include <memory>

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// INJECTION HOOK — Called from EVERY model/agent invocation site
// ============================================================================

// Ghost Text / Inline Completion Hook
// Called from: Win32IDE_GhostText.cpp before requesting completion
inline std::string Hook_GhostText_CompletionRequest(
    const std::string& originalPrompt,
    const std::string& currentFile,
    int cursorLine,
    int cursorCol
) {
    std::string enrichedPrompt = SkillInjectionEngine::Instance().InjectSkillContext(
        originalPrompt,
        "@AgentPolish",  // Ghost text uses AgentPolish skills
        "phase1"         // Ghost text is Phase 1 concern
    );
    
    // Add ghost-text specific context
    enrichedPrompt += "\\n\\n# Ghost Text Context\\n";
    enrichedPrompt += "# Current File: " + currentFile + "\\n";
    enrichedPrompt += "# Cursor Position: L" + std::to_string(cursorLine) + 
                     ":C" + std::to_string(cursorCol) + "\\n";
    enrichedPrompt += "# Completion Type: inline_ghost_text\\n";
    
    return enrichedPrompt;
}

// Chat Panel / AI Backend Hook
// Called from: ChatPanelModelCaller.cpp before model inference
inline std::string Hook_ChatPanel_ModelInference(
    const std::string& originalPrompt,
    const std::string& conversationHistory,
    const std::string& activeModel
) {
    std::string enrichedPrompt = SkillInjectionEngine::Instance().InjectSkillContext(
        originalPrompt,
        "@AgentPolish",  // Chat uses AgentPolish skills
        "phase1"
    );
    
    // Add chat-specific context
    enrichedPrompt += "\\n\\n# Chat Panel Context\\n";
    enrichedPrompt += "# Active Model: " + activeModel + "\\n";
    enrichedPrompt += "# Conversation Turns: " + 
                     std::to_string(std::count(conversationHistory.begin(), 
                                               conversationHistory.end(), '\\n')) + "\\n";
    enrichedPrompt += "# Mode: conversational_ai\\n";
    
    return enrichedPrompt;
}

// Agent Orchestrator Hook
// Called from: agentic_orchestrator.cpp before task dispatch
inline std::string Hook_AgentOrchestrator_TaskDispatch(
    const std::string& taskDescription,
    const std::string& targetAgent,
    const std::string& currentPhase
) {
    std::string enrichedPrompt = SkillInjectionEngine::Instance().InjectSkillContext(
        taskDescription,
        targetAgent,
        currentPhase
    );
    
    // Add orchestrator-specific context
    enrichedPrompt += "\\n\\n# Agent Orchestrator Context\\n";
    enrichedPrompt += "# Target Agent: " + targetAgent + "\\n";
    enrichedPrompt += "# Current Phase: " + currentPhase + "\\n";
    enrichedPrompt += "# Task Type: autonomous_dispatch\\n";
    
    return enrichedPrompt;
}

// LSP Completion Provider Hook
// Called from: ai_completion_provider.cpp before completion request
inline std::string Hook_LSP_CompletionRequest(
    const std::string& originalPrompt,
    const std::string& languageId,
    const std::string& triggerCharacter
) {
    std::string enrichedPrompt = SkillInjectionEngine::Instance().InjectSkillContext(
        originalPrompt,
        "@LSPComplete",  // LSP uses LSPComplete skills
        "phase3"
    );
    
    // Add LSP-specific context
    enrichedPrompt += "\\n\\n# LSP Completion Context\\n";
    enrichedPrompt += "# Language: " + languageId + "\\n";
    enrichedPrompt += "# Trigger: " + (triggerCharacter.empty() ? "manual" : triggerCharacter) + "\\n";
    enrichedPrompt += "# Completion Type: lsp_intellisense\\n";
    
    return enrichedPrompt;
}

// Extension Host API Hook
// Called from: ExtensionHost_VSCodeAPIs.cpp before extension API call
inline std::string Hook_ExtensionHost_APIRequest(
    const std::string& originalPrompt,
    const std::string& extensionId,
    const std::string& apiMethod
) {
    std::string enrichedPrompt = SkillInjectionEngine::Instance().InjectSkillContext(
        originalPrompt,
        "@ExtensionHost",  // Extension host skills
        "phase2"
    );
    
    // Add extension-specific context
    enrichedPrompt += "\\n\\n# Extension Host Context\\n";
    enrichedPrompt += "# Extension: " + extensionId + "\\n";
    enrichedPrompt += "# API Method: " + apiMethod + "\\n";
    enrichedPrompt += "# Isolation: process_sandboxed\\n";
    
    return enrichedPrompt;
}

// Performance Optimizer Hook
// Called from: SpeculativeOptimizer.cpp before optimization decisions
inline std::string Hook_Performance_OptimizationRequest(
    const std::string& originalPrompt,
    const std::string& optimizationTarget
) {
    std::string enrichedPrompt = SkillInjectionEngine::Instance().InjectSkillContext(
        originalPrompt,
        "@Performance",  // Performance skills
        "phase4"
    );
    
    // Add performance-specific context
    enrichedPrompt += "\\n\\n# Performance Optimization Context\\n";
    enrichedPrompt += "# Target: " + optimizationTarget + "\\n";
    enrichedPrompt += "# Mode: speculative_decoding_optimization\\n";
    
    return enrichedPrompt;
}

// ============================================================================
// UNIVERSAL INJECTION MACRO
// ============================================================================
// Use this macro at EVERY model/agent call site to guarantee skill injection
#define RAWRXD_INJECT_SKILLS(prompt, agent, phase) \
    RawrXD::SkillSystem::SkillInjectionEngine::Instance().InjectSkillContext((prompt), (agent), (phase))

// ============================================================================
// LEGACY C-API BRIDGE
// ============================================================================
// For MASM/ASM modules that can't use C++ inline functions
extern "C" {
    // These are exported from SkillInjectionEngine.cpp
    __declspec(dllimport) const char* __stdcall SkillSystem_InjectContext(const char* prompt);
    __declspec(dllimport) bool __stdcall SkillSystem_IsSkillEnabled(const char* skillName);
    __declspec(dllimport) bool __stdcall SkillSystem_ToggleSkill(const char* skillName);
    __declspec(dllimport) void __stdcall SkillSystem_InvalidateCache();
}

} // namespace SkillSystem
} // namespace RawrXD
