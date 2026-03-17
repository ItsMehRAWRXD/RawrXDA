#pragma once
// ============================================================================
// Win32IDE_SubAgent.h — Win32IDE Wrapper for Portable SubAgent System
// ============================================================================
// Thin wrapper that re-exports subagent_core.h types and provides
// Win32IDE-specific factory with IDELogger / IDEConfig / METRICS wiring.
//
// All types (SubAgent, ChainStep, SwarmTask, SwarmConfig, TodoItem,
// SubAgentManager) come from subagent_core.h — this header adds:
//   - Win32-specific headers (windows.h, IDELogger.h)
//   - Factory: createWin32SubAgentManager() wires IDELogger + METRICS
//
// Integration points:
//   - AgenticBridge::ExecuteAgentCommand detects tool calls in LLM output
//   - NativeAgent::Ask prompt includes tool descriptions
//   - AutonomyManager uses SubAgentManager for complex task decomposition
//   - StreamingUX displays swarm progress
// ============================================================================

#include <windows.h>

#ifdef ERROR
#undef ERROR
#endif

#include "../subagent_core.h"
#include "IDELogger.h"
#include "../cpu_inference_engine.h"

// Forward declarations for Win32IDE-specific types
class AgenticBridge;

// ============================================================================
// Factory: Create a SubAgentManager with IDELogger + METRICS callbacks
// ============================================================================
SubAgentManager* createWin32SubAgentManager(AgenticEngine* engine);
