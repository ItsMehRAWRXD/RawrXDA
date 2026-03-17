// ═══════════════════════════════════════════════════════════════════════════════
// pocket_lab_turbo.h
// C Header for PocketLab MASM Kernel DLL
// Pure Win32, no dependencies
// ═══════════════════════════════════════════════════════════════════════════════

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    /// Initialize PocketLab kernel (detects system tier, allocates resources)
    /// Returns: 0 on success, non-zero on error
    __declspec(dllimport) int __cdecl PocketLabInit_Export(void);

    /// Get performance statistics from last run
    /// @param out_tokens Pointer to receive token count (can be NULL)
    /// @param out_sparse_skips Pointer to receive sparse skip count
    /// @param out_gpu Pointer to receive GPU-routed token count
    /// @param out_cpu Pointer to receive CPU-routed token count
    __declspec(dllimport) void __cdecl PocketLabGetStats_Export(
        uint64_t* out_tokens,
        uint64_t* out_sparse_skips,
        uint64_t* out_gpu,
        uint64_t* out_cpu
    );

    /// Run one inference cycle (token generation iteration)
    /// Returns: 0 on success, non-zero on error
    __declspec(dllimport) int __cdecl PocketLabRunCycle_Export(void);

    /// Shutdown PocketLab kernel (cleanup resources)
    __declspec(dllimport) void __cdecl PocketLabShutdown_Export(void);

#ifdef __cplusplus
}
#endif
