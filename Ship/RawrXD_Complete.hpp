// RawrXD_Complete.hpp - Master Header for Qt-Free Architecture
// Pure C++20 - No Qt Dependencies Whatsoever
// Includes: All core systems for production RawrXD IDE

#pragma once

// ============================================================================
// Core Infrastructure
// ============================================================================
#include "RawrXD_JSON.hpp"                  // JSON parsing/serialization
#include "RawrXD_SymbolIndex.hpp"           // Code symbol indexing
#include "RawrXD_FileWatcher.hpp"           // Real-time file monitoring

// ============================================================================
// Agentic Systems
// ============================================================================
#include "RawrXD_Planner.hpp"               // Autonomous task planning
#include "RawrXD_ToolRegistry.hpp"          // Tool execution framework
#include "RawrXD_ExtensionMgr.hpp"          // Extension management
#include "RawrXD_SelfPatch.hpp"             // Code generation & patching

// ============================================================================
// Code Intelligence
// ============================================================================
#include "RawrXD_CodebaseEngine.hpp"        // Static analysis & refactoring
#include "LSPClient.hpp"                    // Language Server Protocol

// ============================================================================
// Model & Inference
// ============================================================================
#include "RawrXD_CloudClient.hpp"           // Cloud API client (OpenAI, Anthropic)
#include "RawrXD_HybridCloudMgr.hpp"        // Local/Cloud orchestration
#include "RawrXD_MetaLearn.hpp"             // Performance learning

// ============================================================================
// Release & Operations
// ============================================================================
#include "RawrXD_ReleaseAgent.hpp"          // Autonomous releases
#include "RawrXD_ErrorRecovery.hpp"         // Health monitoring
#include "RawrXD_Overclock.hpp"             // Hardware optimization

// ============================================================================
// UI & Presentation
// ============================================================================
#include "RawrXD_WebView.hpp"               // WebView2 integration
#include "MCPServer.hpp"                    // Model Context Protocol server

// ============================================================================
// Build Status
// ============================================================================
// Compiled DLLs (Pure C/Win32):
//   - RawrXD_InferenceEngine.dll      ✓ GGUF model loading
//   - RawrXD_AgenticEngine.dll        ✓ Autonomous execution
//   - RawrXD_TerminalMgr.dll          ✓ ConPTY terminal sessions
//   - RawrXD_PlanOrchestrator.dll     ✓ Multi-step task coordination
//
// Compiled EXEs (Pure C++20/Win32):
//   - RawrXD_IDE_Production.exe       ✓ Full IDE (no Qt)
//   - RawrXD_Agent.exe                ✓ Standalone agent
//   - RawrXD_CLI.exe                  ✓ Command-line interface
//
// Pre-built (MASM64):
//   - RawrXD_Titan_Kernel.dll         ○ High-perf inference kernel
//   - RawrXD_NativeModelBridge.dll    ○ GGUF integration bridge
// ============================================================================

namespace RawrXD {

// Version information
constexpr const char* VERSION = "2.0.0-NoQt";
constexpr const char* BUILD_DATE = "2026-01-29";
constexpr const char* ARCHITECTURE = "Pure Win32/C++20/MASM64";

// Feature flags
constexpr bool QT_FREE = true;
constexpr bool SUPPORTS_WEBVIEW2 = true;
constexpr bool SUPPORTS_LSP = true;
constexpr bool SUPPORTS_MCP = true;
constexpr bool SUPPORTS_CLOUD_INFERENCE = true;
constexpr bool SUPPORTS_LOCAL_GGUF = true;

} // namespace RawrXD
