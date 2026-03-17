// ============================================================================
// Win32IDE_FlagshipFeatures.cpp — Flagship Feature Lifecycle Router
// ============================================================================
//
// PURPOSE:
//   Master lifecycle and command router for the three flagship product
//   pillars.  This file provides a single entry point that dispatches
//   commands in the 13000–13299 range to the correct subsystem:
//
//     1. Provable AI Coding Agent        (13000–13019)
//     2. AI-Native Reverse Engineering   (13020–13039)
//     3. Airgapped Enterprise Env        (13040–13059)
//
//   initFlagshipFeatures()      — lazy-inits all three on first use
//   handleFlagshipCommand()     — routes WM_COMMAND IDMs
//   shutdownFlagshipFeatures()  — teardown (if needed)
//
//   This file does NOT duplicate any logic.  Each subsystem is fully
//   self-contained in its own .cpp file:
//     Win32IDE_ProvableAgent.cpp
//     Win32IDE_AIReverseEngineering.cpp
//     Win32IDE_AirgappedEnterprise.cpp
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <sstream>

// ============================================================================
// Initialization — lazy-init all three flagship pillars
// ============================================================================

void Win32IDE::initFlagshipFeatures() {
    // Each init is idempotent — safe to call multiple times
    initProvableAgent();
    initAIReverseEngineering();
    initAirgappedEnterprise();

    OutputDebugStringA("[Flagship] All three flagship product pillars initialized.\n");

    std::ostringstream oss;
    oss << "[Flagship] Product pillars active:\n"
        << "  1. Provable AI Coding Agent   — "
        << (m_provableAgentInitialized ? "READY" : "FAILED") << "\n"
        << "  2. AI-Native Reverse Eng IDE  — "
        << (m_aiReverseEngInitialized ? "READY" : "FAILED") << "\n"
        << "  3. Airgapped Enterprise Env   — "
        << (m_airgappedEnterpriseInitialized ? "READY" : "FAILED") << "\n";
    appendToOutput(oss.str());
}

// ============================================================================
// Command Router — dispatches IDM 13000–13059 to correct subsystem
// ============================================================================

bool Win32IDE::handleFlagshipCommand(int commandId) {
    // Range check: all flagship commands are 13000–13059
    if (commandId < 13000 || commandId > 13059) return false;

    // ── Provable AI Coding Agent (13000–13019) ──
    if (commandId >= IDM_PROVABLE_SHOW && commandId <= IDM_PROVABLE_STATS) {
        return handleProvableAgentCommand(commandId);
    }

    // ── AI-Native Reverse Engineering IDE (13020–13039) ──
    if (commandId >= IDM_AIRE_SHOW && commandId <= IDM_AIRE_STATS) {
        return handleAIReverseEngCommand(commandId);
    }

    // ── Airgapped Enterprise Environment (13040–13059) ──
    if (commandId >= IDM_AIRGAP_SHOW && commandId <= IDM_AIRGAP_STATS) {
        return handleAirgappedCommand(commandId);
    }

    return false;
}

// ============================================================================
// Shutdown — teardown all flagship subsystems
// ============================================================================

void Win32IDE::shutdownFlagshipFeatures() {
    // Each subsystem manages its own cleanup via panel WM_DESTROY.
    // This function exists for future resource cleanup if needed.
    OutputDebugStringA("[Flagship] Flagship features shutdown.\n");
}
