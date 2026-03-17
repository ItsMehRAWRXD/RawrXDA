// ═══════════════════════════════════════════════════════════════════════════════
// pocket_lab_turbo.cpp
// C++ Bridge Layer for PocketLab MASM Kernel
// Exports pocket lab functions with proper C calling convention
// ═══════════════════════════════════════════════════════════════════════════════

#include <windows.h>
#include <cstdint>

// Forward declarations to MASM functions
extern "C" {
    int __cdecl PocketLabInit(void);
    void __cdecl PocketLabGetStats(uint64_t* out_tokens, uint64_t* out_sparse_skips, uint64_t* out_gpu, uint64_t* out_cpu);
    int __cdecl PocketLabRunCycle(void);
    void __cdecl PocketLabShutdown(void);
}

// Export stubs that call the MASM implementations
extern "C" __declspec(dllexport) int __cdecl PocketLabInit_Export(void) {
    return PocketLabInit();
}

extern "C" __declspec(dllexport) void __cdecl PocketLabGetStats_Export(uint64_t* out_tokens, uint64_t* out_sparse_skips, uint64_t* out_gpu, uint64_t* out_cpu) {
    PocketLabGetStats(out_tokens, out_sparse_skips, out_gpu, out_cpu);
}

extern "C" __declspec(dllexport) int __cdecl PocketLabRunCycle_Export(void) {
    return PocketLabRunCycle();
}

extern "C" __declspec(dllexport) void __cdecl PocketLabShutdown_Export(void) {
    PocketLabShutdown();
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
