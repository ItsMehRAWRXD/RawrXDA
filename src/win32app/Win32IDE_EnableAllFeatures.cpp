// ============================================================================
// Win32IDE_EnableAllFeatures.cpp
// 5-tier subsystem enablement orchestrator (core → AI → agent → build → advanced).
// Invoked from Win32IDE::deferredHeavyInitBody() as pipeline batch 1/8.
// ============================================================================

#include "Win32IDE.h"
#include <cassert>

namespace
{
// Tier 1: Core Subsystems (Window, UI, Foundation)
bool enableCoreSubsystems()
{
    // Implicit in Win32IDE constructor: main window, message loop, basic UI.
    return true;
}

// Tier 2: AI Backend Subsystems
bool enableAISubsystems()
{
    // Lazy in deferredHeavyInitBody: GGUF loader, CPU/GPU inference, model resolver.
    return true;
}

// Tier 3: Agent & Autonomy Systems
bool enableAgentSystems()
{
    // Deferred init: NativeAgent, SubAgentManager, agentic bridge, hotpatch hooks.
    return true;
}

// Tier 4: Build & Compilation Systems
bool enableBuildSystems()
{
    // Build / MASM / task providers wired through feature modules and commands.
    return true;
}

// Tier 5: Advanced Features & Extensibility
bool enableAdvancedFeatures()
{
    // Extensions, LSP, MCP, RE suite, sidebar — initialized in later deferredHeavyInitBody phases.
    return true;
}

void wireAllSubsystems()
{
    enableCoreSubsystems();
    enableAISubsystems();
    enableAgentSystems();
    enableBuildSystems();
    enableAdvancedFeatures();
}
}  // namespace

void Win32IDE::enableAllFeaturesAndWire()
{
    wireAllSubsystems();
}

void Win32IDE::wireAllSystems()
{
    wireAllSubsystems();
}
