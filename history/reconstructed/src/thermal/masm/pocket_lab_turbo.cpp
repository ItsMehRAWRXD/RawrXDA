// ═══════════════════════════════════════════════════════════════════════════════
// pocket_lab_turbo.cpp
// C++ Bridge Layer for PocketLab MASM Kernel DLL
// Exports wrappers that match the actual ASM API in pocket_lab_turbo_dll.asm
//
// IMPORTANT: pocket_lab_turbo_dll.asm defines its own _DllMainCRTStartup
//            which auto-inits on DLL_PROCESS_ATTACH and cleans up on DETACH.
//            We do NOT define DllMain here — the ASM entry point handles it.
// ═══════════════════════════════════════════════════════════════════════════════

#include <windows.h>
#include <cstdint>

// ─── Forward declarations matching actual ASM signatures ────────────────────
// See pocket_lab_turbo_dll.asm PUBLIC exports
extern "C" {
    // PocketLabInit: no args, returns 0 on success, -1 on failure
    int __cdecl PocketLabInit(void);

    // PocketLabGetThermal: RCX = pointer to ThermalSnapshot struct
    void __cdecl PocketLabGetThermal(void* snapshot_ptr);

    // PocketLabRunCycle: RCX = token count to process, returns tokens processed
    int64_t __cdecl PocketLabRunCycle(int64_t token_count);

    // PocketLabGetStats: RCX = pointer to 64-byte buffer
    // Layout: [tokens:8][sparse:8][gpu:8][cpu:8][tier:4][spare:28]
    void __cdecl PocketLabGetStats(void* stats_buffer);

    // InternalCleanup: called by _DllMainCRTStartup on DLL_PROCESS_DETACH
    void __cdecl InternalCleanup(void);
}

// ─── C-callable export wrappers ─────────────────────────────────────────────
// These give KernelDispatcher clean typed entry points via GetProcAddress

extern "C" __declspec(dllexport) int __cdecl PocketLabInit_Export(void) {
    return PocketLabInit();
}

extern "C" __declspec(dllexport) void __cdecl PocketLabGetThermal_Export(void* snapshot_ptr) {
    PocketLabGetThermal(snapshot_ptr);
}

extern "C" __declspec(dllexport) int64_t __cdecl PocketLabRunCycle_Export(int64_t token_count) {
    return PocketLabRunCycle(token_count);
}

extern "C" __declspec(dllexport) void __cdecl PocketLabGetStats_Export(void* stats_buffer) {
    PocketLabGetStats(stats_buffer);
}

extern "C" __declspec(dllexport) void __cdecl PocketLabShutdown_Export(void) {
    // Delegates to InternalCleanup which is what _DllMainCRTStartup calls on detach
    InternalCleanup();
}
