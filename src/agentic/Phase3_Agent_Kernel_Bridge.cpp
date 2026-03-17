// ═══════════════════════════════════════════════════════════════════════════════
// Phase3_Agent_Kernel_Bridge.cpp
// C++ Bridge Layer for Phase3 MASM Kernel
// Exports phase3 functions with proper C calling convention
// ═══════════════════════════════════════════════════════════════════════════════

#include <windows.h>
#include <cstdint>

// Forward declarations to MASM functions
extern "C" {
    void* __cdecl Phase3Initialize(void* phase1_ctx, void* phase2_ctx);
    int __cdecl GenerateTokens(void* context, const char* prompt, void* params);
    void __cdecl Phase3Shutdown(void* context);
}

// Export stubs that call the MASM implementations
extern "C" __declspec(dllexport) void* __cdecl Phase3Initialize_Export(void* phase1_ctx, void* phase2_ctx) {
    return Phase3Initialize(phase1_ctx, phase2_ctx);
}

extern "C" __declspec(dllexport) int __cdecl GenerateTokens_Export(void* context, const char* prompt, void* params) {
    return GenerateTokens(context, prompt, params);
}

extern "C" __declspec(dllexport) void __cdecl Phase3Shutdown_Export(void* context) {
    Phase3Shutdown(context);
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
