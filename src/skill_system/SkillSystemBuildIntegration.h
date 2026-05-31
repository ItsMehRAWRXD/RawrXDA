// ============================================================================
// SkillSystemBuildIntegration.h — Public API for Win32IDE integration
// ============================================================================
#pragma once

#include "SkillInjectionEngine.h"
#include "SkillInjectionHooks.h"
#include "SkillToggleUI.h"
#include "PromptWarmingEngine.h"

namespace RawrXD {
namespace SkillSystem {

// Initialize the skill system (called from WinMain)
bool InitializeSkillSystem();

// Shutdown the skill system (called from IDE cleanup)
void ShutdownSkillSystem();

// Quick access to injection engine
inline SkillInjectionEngine& GetSkillEngine() {
    return SkillInjectionEngine::Instance();
}

// Quick access to prompt warmer
inline PromptWarmingEngine& GetPromptWarmer() {
    return PromptWarmingEngine::Instance();
}

} // namespace SkillSystem
} // namespace RawrXD
