// ═══════════════════════════════════════════════════════════════════════════════
// pocket_lab_turbo.h
// C Header for PocketLab MASM Kernel DLL
// Pure Win32, no dependencies
//
// PocketLabGetStats buffer layout (64 bytes):
//   [tokens_processed:8][sparse_skipped:8][gpu_processed:8][cpu_processed:8][tier:4][spare:28]
// ═══════════════════════════════════════════════════════════════════════════════

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    /// Initialize PocketLab kernel (auto-detects tier, allocates resources)
    /// Returns: 0 on success, -1 on failure
    __declspec(dllimport) int __cdecl PocketLabInit_Export(void);

    /// Fill a ThermalSnapshot struct (see pocket_lab_turbo_dll.asm for layout)
    /// @param snapshot_ptr Pointer to output ThermalSnapshot struct
    __declspec(dllimport) void __cdecl PocketLabGetThermal_Export(void* snapshot_ptr);

    /// Run one inference cycle
    /// @param token_count Number of tokens to process
    /// Returns: number of tokens actually processed
    __declspec(dllimport) int64_t __cdecl PocketLabRunCycle_Export(int64_t token_count);

    /// Get performance counters into a 64-byte buffer
    /// @param stats_buffer Pointer to 64-byte output buffer
    /// Layout: [tokens:8][sparse:8][gpu:8][cpu:8][tier:4][spare:28]
    __declspec(dllimport) void __cdecl PocketLabGetStats_Export(void* stats_buffer);

    /// Shutdown and free resources
    __declspec(dllimport) void __cdecl PocketLabShutdown_Export(void);

#ifdef __cplusplus
}
#endif
