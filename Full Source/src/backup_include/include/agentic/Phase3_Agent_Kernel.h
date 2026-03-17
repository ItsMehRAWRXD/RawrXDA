// ═══════════════════════════════════════════════════════════════════════════════
// Phase3_Agent_Kernel.h
// C Header for Phase3 MASM Agent Kernel DLL
// Pure Win32, no dependencies
// ═══════════════════════════════════════════════════════════════════════════════

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    /// Initialize Phase3 Agent Kernel
    /// @param phase1_ctx Context from Phase 1 (can be NULL)
    /// @param phase2_ctx Context from Phase 2 (can be NULL)
    /// Returns: AGENT_CONTEXT pointer on success, NULL on error
    __declspec(dllimport) void* __cdecl Phase3Initialize_Export(void* phase1_ctx, void* phase2_ctx);

    /// Generate tokens using Phase3 kernel
    /// @param context AGENT_CONTEXT from Phase3Initialize_Export
    /// @param prompt Input prompt string
    /// @param params GENERATION_PARAMS structure (can be NULL for defaults)
    /// Returns: 1 on success, 0 on error
    __declspec(dllimport) int __cdecl GenerateTokens_Export(
        void* context,
        const char* prompt,
        void* params
    );

    /// Shutdown Phase3 kernel and free resources
    /// @param context AGENT_CONTEXT to free
    __declspec(dllimport) void __cdecl Phase3Shutdown_Export(void* context);

#ifdef __cplusplus
}
#endif
