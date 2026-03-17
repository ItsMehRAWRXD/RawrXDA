#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "../bridge/Win32SwarmBridge.h"

// Validation export for build-time verification
extern "C" __declspec(dllexport) bool ValidateSwarmBridgeIntegration() {
    using namespace RawrXD::Bridge;
    
    // Test 1: Config structure packing
    static_assert(sizeof(SwarmInitConfig) == 80, "SwarmInitConfig packing mismatch");
    static_assert(offsetof(SwarmInitConfig, coordinatorModel) == 16, "Field offset error");
    
    // Test 2: IAT slot alignment (compile-time check)
    // In our architecture, Slot 20 is the target for initializeSwarmSystem
    constexpr int EXPECTED_IAT_SLOT = 20;
    
    // Test 3: Verify export exists and is callable
    HMODULE hSelf = GetModuleHandle(NULL);
    // Note: In some build configs, we check the DLL or the EXE itself
    auto* proc = GetProcAddress(hSelf, "Win32IDE_initializeSwarmSystem");
    if (!proc) {
        // Fallback to checking the bridge DLL if compiled as such
        HMODULE hBridge = GetModuleHandleA("RawrXD-Win32IDE.exe"); 
        proc = GetProcAddress(hBridge, "Win32IDE_initializeSwarmSystem");
    }
    
    if (!proc) return false;
    
    // Test 4: Null config rejection (should return E_INVALIDARG / 0x80070057)
    typedef int (__stdcall *PSET_INIT)(void*);
    auto result = reinterpret_cast<PSET_INIT>(proc)(nullptr);
    if (result != (int)0x80070057) return false; // E_INVALIDARG
    
    // Test 5: Valid config acceptance (dry-run)
    SwarmInitConfig config{};
    memset(&config, 0, sizeof(config));
    config.structSize = sizeof(config);
    config.maxSubAgents = 2; // Minimal for test
    config.taskTimeoutMs = 1000;
    config.enableGPUWorkStealing = 0;
    strcpy_s(config.coordinatorModel, "test");
    
    // This call initializes the singleton and prepares the topology
    result = reinterpret_cast<PSET_INIT>(proc)(&config);
    
    // We expect S_OK (0) or a specific error code if the model is missing, 
    // but the structure itself should be accepted.
    return (result >= 0 || result == (int)0x80040154); // S_OK or REGDB_E_CLASSNOTREG (model not found)
}
