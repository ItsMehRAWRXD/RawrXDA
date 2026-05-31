// ============================================================================
// SkillSystemBuildIntegration.cpp — Build system integration for skill injection
// ============================================================================

#include "SkillSystemBuildIntegration.h"
#include "PromptWarmingEngine.h"
#include <windows.h>

namespace RawrXD {
namespace SkillSystem {

// ============================================================================
// INITIALIZATION SEQUENCE — Called from WinMain or IDE startup
// ============================================================================
bool InitializeSkillSystem()
{
    try
    {
        auto& engine = SkillInjectionEngine::Instance();
        
        // Load all skill definitions from .github/skills/
        if (!engine.LoadAllSkills()) {
            OutputDebugStringA("[SkillSystem] Warning: No skills loaded from registry\n");
        }
        
        // Verify the 520-line guarantee
        const size_t injectionLines = engine.GetTotalInjectionLines();
        if (injectionLines > 520) {
            OutputDebugStringA("[SkillSystem] ERROR: Injection exceeds 520-line guarantee\n");
            engine.InvalidateCache();
        }
        
        // Initialize prompt warming engine
        auto& warmer = PromptWarmingEngine::Instance();
        warmer.Initialize();
        
        // Pre-seed with active skill context
        std::string skillContext = engine.InjectSkillContext("", "", "");
        if (!skillContext.empty()) {
            warmer.PreseedSkillContext(skillContext);
        }
        
        OutputDebugStringA("[SkillSystem] Initialized successfully\n");
        return true;
    }
    catch (const std::exception& e)
    {
        OutputDebugStringA("[SkillSystem] Exception during initialization: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        return false;
    }
    catch (...)
    {
        OutputDebugStringA("[SkillSystem] Unknown exception during initialization\n");
        return false;
    }
}

// ============================================================================
// SHUTDOWN SEQUENCE — Called from IDE cleanup
// ============================================================================
void ShutdownSkillSystem()
{
    auto& engine = SkillInjectionEngine::Instance();
    engine.SaveToggleState();
    
    auto& warmer = PromptWarmingEngine::Instance();
    warmer.Shutdown();
    
    OutputDebugStringA("[SkillSystem] Shutdown complete\n");
}

} // namespace SkillSystem
} // namespace RawrXD
