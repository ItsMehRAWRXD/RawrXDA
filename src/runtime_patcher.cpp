/**
 * runtime_patcher.cpp - Runtime Masquerade Patcher
 * 
 * Inject real implementations into running binary via IAT hook table.
 * This allows hot-patching of trampolined methods at runtime when
 * real implementations become available (via plugin DLL or dynamic loading).
 * 
 * Build: cl /EHsc /O2 /c runtime_patcher.cpp
 * Link with: runtime_masquerade.obj
 */

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

//=============================================================================
// External references to ASM-defined symbols
//=============================================================================

extern "C" {
    // From runtime_masquerade.asm
    extern void* __iat_hook_base[];
    extern uint64_t __iat_hook_count;
    extern uint64_t masquerade_context[];
    
    // Hook installation API
    extern void* InstallIATHook(uint64_t slot, void* fn);
    extern void* GetIATHook(uint64_t slot);
    extern uint64_t GetMasqueradeStats();
    
    // Global data from asm_globals.asm
    extern void* g_hHeap;
    extern void* g_hInstance;
    extern int RawrXD_GlobalsInit(HINSTANCE hInst, HANDLE hHeap);
}

//=============================================================================
// IAT Slot Index Constants (must match runtime_masquerade.asm)
//=============================================================================

namespace IAT {
    // Win32IDE methods (0-47)
    constexpr uint64_t Win32IDE_getGitChangedFiles = 0;
    constexpr uint64_t Win32IDE_isGitRepository = 1;
    constexpr uint64_t Win32IDE_gitUnstageFile = 2;
    constexpr uint64_t Win32IDE_gitStageFile = 3;
    constexpr uint64_t Win32IDE_gitPull = 4;
    constexpr uint64_t Win32IDE_gitPush = 5;
    constexpr uint64_t Win32IDE_gitCommit = 6;
    constexpr uint64_t Win32IDE_newFile = 7;
    constexpr uint64_t Win32IDE_generateResponse = 8;
    constexpr uint64_t Win32IDE_isModelLoaded = 9;
    constexpr uint64_t Win32IDE_HandleCopilotStreamUpdate = 10;
    constexpr uint64_t Win32IDE_HandleCopilotClear = 11;
    constexpr uint64_t Win32IDE_HandleCopilotSend = 12;
    constexpr uint64_t Win32IDE_routeCommand = 13;
    constexpr uint64_t Win32IDE_shutdownInference = 14;
    constexpr uint64_t Win32IDE_initializeInference = 15;
    constexpr uint64_t Win32IDE_createStatusBar = 16;
    constexpr uint64_t Win32IDE_createTerminal = 17;
    constexpr uint64_t Win32IDE_createEditor = 18;
    constexpr uint64_t Win32IDE_createSidebar = 19;
    constexpr uint64_t Win32IDE_initializeSwarmSystem = 20;
    constexpr uint64_t Win32IDE_createAcceleratorTable = 21;
    constexpr uint64_t Win32IDE_removeTab = 22;
    
    // AgenticBridge/SubAgent (48-63)
    constexpr uint64_t AgenticBridge_GetSubAgentManager = 48;
    constexpr uint64_t SubAgentManager_getStatusSummary = 49;
    constexpr uint64_t SubAgentManager_getTodoList = 50;
    constexpr uint64_t SubAgentManager_getChainSteps = 51;
    constexpr uint64_t SubAgentManager_getAllSubAgents = 52;
    constexpr uint64_t SubAgentManager_setTodoList = 53;
    constexpr uint64_t SubAgentManager_executeSwarm = 54;
    constexpr uint64_t SubAgentManager_executeChain = 55;
    
    // LSP Client (64-75)
    constexpr uint64_t LSPClient_Initialize = 64;
    constexpr uint64_t LSPClient_Shutdown = 65;
    constexpr uint64_t LSPClient_DidOpen = 66;
    constexpr uint64_t LSPClient_DidChange = 67;
    constexpr uint64_t LSPClient_DidClose = 68;
    constexpr uint64_t LSPClient_Completion = 69;
    
    // Streaming/Inference (76-83)
    constexpr uint64_t Stream_Init = 76;
    constexpr uint64_t Stream_Write = 77;
    constexpr uint64_t Stream_Read = 78;
    constexpr uint64_t Inference_LoadModel = 81;
    constexpr uint64_t Inference_RunInference = 82;
    constexpr uint64_t Inference_UnloadModel = 83;
}

//=============================================================================
// Symbol Name to IAT Index Mapping
//=============================================================================

struct SymbolMapping {
    const char* exportName;
    uint64_t iatSlot;
};

static const SymbolMapping g_SymbolMappings[] = {
    // Win32IDE methods
    {"Win32IDE_getGitChangedFiles", IAT::Win32IDE_getGitChangedFiles},
    {"Win32IDE_isGitRepository", IAT::Win32IDE_isGitRepository},
    {"Win32IDE_gitUnstageFile", IAT::Win32IDE_gitUnstageFile},
    {"Win32IDE_gitStageFile", IAT::Win32IDE_gitStageFile},
    {"Win32IDE_gitPull", IAT::Win32IDE_gitPull},
    {"Win32IDE_gitPush", IAT::Win32IDE_gitPush},
    {"Win32IDE_gitCommit", IAT::Win32IDE_gitCommit},
    {"Win32IDE_newFile", IAT::Win32IDE_newFile},
    {"Win32IDE_generateResponse", IAT::Win32IDE_generateResponse},
    {"Win32IDE_isModelLoaded", IAT::Win32IDE_isModelLoaded},
    {"Win32IDE_HandleCopilotStreamUpdate", IAT::Win32IDE_HandleCopilotStreamUpdate},
    {"Win32IDE_HandleCopilotClear", IAT::Win32IDE_HandleCopilotClear},
    {"Win32IDE_HandleCopilotSend", IAT::Win32IDE_HandleCopilotSend},
    {"Win32IDE_routeCommand", IAT::Win32IDE_routeCommand},
    {"Win32IDE_shutdownInference", IAT::Win32IDE_shutdownInference},
    {"Win32IDE_initializeInference", IAT::Win32IDE_initializeInference},
    {"Win32IDE_createStatusBar", IAT::Win32IDE_createStatusBar},
    {"Win32IDE_createTerminal", IAT::Win32IDE_createTerminal},
    {"Win32IDE_createEditor", IAT::Win32IDE_createEditor},
    {"Win32IDE_createSidebar", IAT::Win32IDE_createSidebar},
    {"Win32IDE_initializeSwarmSystem", IAT::Win32IDE_initializeSwarmSystem},
    {"Win32IDE_createAcceleratorTable", IAT::Win32IDE_createAcceleratorTable},
    
    // AgenticBridge methods
    {"AgenticBridge_GetSubAgentManager", IAT::AgenticBridge_GetSubAgentManager},
    {"SubAgentManager_getStatusSummary", IAT::SubAgentManager_getStatusSummary},
    {"SubAgentManager_getTodoList", IAT::SubAgentManager_getTodoList},
    {"SubAgentManager_executeSwarm", IAT::SubAgentManager_executeSwarm},
    {"SubAgentManager_executeChain", IAT::SubAgentManager_executeChain},
    
    // LSP Client
    {"LSPClient_Initialize", IAT::LSPClient_Initialize},
    {"LSPClient_Shutdown", IAT::LSPClient_Shutdown},
    {"LSPClient_Completion", IAT::LSPClient_Completion},
    
    // Inference
    {"Inference_LoadModel", IAT::Inference_LoadModel},
    {"Inference_RunInference", IAT::Inference_RunInference},
    {"Inference_UnloadModel", IAT::Inference_UnloadModel},
    
    {nullptr, 0}  // Sentinel
};

//=============================================================================
// RuntimePatcher Class
//=============================================================================

class RuntimePatcher {
public:
    /**
     * Install a hook by function name
     * @param fnName Export name from plugin DLL
     * @param realImplementation Function pointer to real implementation
     * @return true if hook was installed
     */
    static bool InstallHook(const char* fnName, void* realImplementation) {
        if (!fnName || !realImplementation) return false;
        
        // Find the IAT slot for this function name
        for (const SymbolMapping* m = g_SymbolMappings; m->exportName; ++m) {
            if (strcmp(m->exportName, fnName) == 0) {
                void* prev = InstallIATHook(m->iatSlot, realImplementation);
                return true;
            }
        }
        return false;
    }
    
    /**
     * Install a hook by IAT slot index
     * @param slot IAT slot index (see IAT namespace)
     * @param realImplementation Function pointer
     * @return Previous hook value
     */
    static void* InstallHookBySlot(uint64_t slot, void* realImplementation) {
        return InstallIATHook(slot, realImplementation);
    }
    
    /**
     * Get current hook for a slot
     * @param slot IAT slot index
     * @return Current function pointer (nullptr if not hooked)
     */
    static void* GetHook(uint64_t slot) {
        return GetIATHook(slot);
    }
    
    /**
     * Check if a slot is hooked (has real implementation)
     * @param slot IAT slot index
     * @return true if slot has non-null implementation
     */
    static bool IsHooked(uint64_t slot) {
        return GetIATHook(slot) != nullptr;
    }
    
    /**
     * Load implementations from a plugin DLL
     * @param dllPath Path to plugin DLL
     * @return Number of successfully resolved symbols
     */
    static int LoadPluginDLL(const char* dllPath) {
        HMODULE hPlugin = LoadLibraryA(dllPath);
        if (!hPlugin) {
            return -1;  // Failed to load
        }
        
        int resolved = 0;
        
        for (const SymbolMapping* m = g_SymbolMappings; m->exportName; ++m) {
            FARPROC proc = GetProcAddress(hPlugin, m->exportName);
            if (proc) {
                InstallIATHook(m->iatSlot, reinterpret_cast<void*>(proc));
                resolved++;
            }
        }
        
        return resolved;
    }
    
    /**
     * Try to load from multiple candidate DLLs
     * @return Total number of resolved symbols
     */
    static int AutoLoadPlugins() {
        static const char* candidates[] = {
            "RawrXD_Plugin.dll",
            "RawrXD_RealUI.dll",
            "RawrXD_Win32IDE.dll",
            "RawrXD_AgenticBridge.dll",
            nullptr
        };
        
        int totalResolved = 0;
        
        for (const char** c = candidates; *c; ++c) {
            int n = LoadPluginDLL(*c);
            if (n > 0) {
                totalResolved += n;
            }
        }
        
        return totalResolved;
    }
    
    /**
     * Get masquerade statistics
     * @return Number of times stub functions were called
     */
    static uint64_t GetStubCallCount() {
        return GetMasqueradeStats();
    }
    
    /**
     * Print diagnostic information
     */
    static void PrintDiagnostics() {
        printf("[RuntimePatcher] Diagnostics:\n");
        printf("  IAT Hook Table: %p\n", __iat_hook_base);
        printf("  Total Slots: %llu\n", __iat_hook_count);
        printf("  Stub Call Count: %llu\n", GetMasqueradeStats());
        
        int hooked = 0;
        int unhooked = 0;
        
        for (uint64_t i = 0; i < __iat_hook_count && i < 84; ++i) {
            if (__iat_hook_base[i] != nullptr) {
                hooked++;
            } else {
                unhooked++;
            }
        }
        
        printf("  Hooked Slots: %d\n", hooked);
        printf("  Unhooked Slots: %d\n", unhooked);
    }
};

//=============================================================================
// C API for ASM/external access
//=============================================================================

extern "C" {
    
/**
 * Initialize the runtime patcher
 * Call this early in main/WinMain
 */
__declspec(dllexport)
int RAWRXD_InitRuntime(HINSTANCE hInstance) {
    // Initialize globals
    RawrXD_GlobalsInit(hInstance, GetProcessHeap());
    
    // Try to auto-load plugins
    int resolved = RuntimePatcher::AutoLoadPlugins();
    
    return resolved;
}

/**
 * Install a hook by name
 */
__declspec(dllexport)
int RAWRXD_InstallHook(const char* fnName, void* impl) {
    return RuntimePatcher::InstallHook(fnName, impl) ? 1 : 0;
}

/**
 * Install a hook by slot index
 */
__declspec(dllexport)
void* RAWRXD_InstallHookBySlot(uint64_t slot, void* impl) {
    return RuntimePatcher::InstallHookBySlot(slot, impl);
}

/**
 * Get current hook for a slot
 */
__declspec(dllexport)
void* RAWRXD_GetHook(uint64_t slot) {
    return RuntimePatcher::GetHook(slot);
}

/**
 * Load implementations from a DLL
 */
__declspec(dllexport)
int RAWRXD_LoadPlugin(const char* dllPath) {
    return RuntimePatcher::LoadPluginDLL(dllPath);
}

/**
 * Get stub call statistics
 */
__declspec(dllexport)
uint64_t RAWRXD_GetStubStats() {
    return RuntimePatcher::GetStubCallCount();
}

/**
 * Patch all methods - called when real implementation loads
 * Can be triggered by plugin loader or manual invocation
 */
__declspec(dllexport)
void RAWRXD_PatchAllMethods() {
    // Try known DLL names
    HMODULE hReal = LoadLibraryA("RawrXD_RealUI.dll");
    if (!hReal) {
        hReal = LoadLibraryA("RawrXD_Plugin.dll");
    }
    if (!hReal) return;
    
    // Resolve all mapped symbols
    for (const SymbolMapping* m = g_SymbolMappings; m->exportName; ++m) {
        FARPROC proc = GetProcAddress(hReal, m->exportName);
        if (proc) {
            __iat_hook_base[m->iatSlot] = reinterpret_cast<void*>(proc);
        }
    }
}

} // extern "C"

//=============================================================================
// Sample Plugin Interface
//=============================================================================

/**
 * This is the interface that plugin DLLs should implement.
 * Each function should be exported with the exact name from g_SymbolMappings.
 * 
 * Example plugin (RawrXD_Plugin.dll):
 * 
 * extern "C" {
 *     __declspec(dllexport) bool Win32IDE_isGitRepository() {
 *         // Real implementation
 *         return true;
 *     }
 *     
 *     __declspec(dllexport) std::string Win32IDE_generateResponse(const std::string& prompt) {
 *         // Real implementation
 *         return "AI response...";
 *     }
 * }
 */

//=============================================================================
// Self-Test (compile with /DRUNTIME_PATCHER_TEST)
//=============================================================================

#ifdef RUNTIME_PATCHER_TEST

// Test stub implementation
static bool test_isGitRepository() {
    return true;
}

int main() {
    printf("Runtime Patcher Self-Test\n");
    printf("=========================\n\n");
    
    // Initialize
    RAWRXD_InitRuntime(GetModuleHandle(nullptr));
    
    // Print initial state
    RuntimePatcher::PrintDiagnostics();
    
    // Test manual hook installation
    printf("\nInstalling test hook for Win32IDE_isGitRepository...\n");
    bool installed = RuntimePatcher::InstallHook("Win32IDE_isGitRepository", (void*)test_isGitRepository);
    printf("  Result: %s\n", installed ? "SUCCESS" : "FAILED");
    
    // Verify hook
    void* hook = RuntimePatcher::GetHook(IAT::Win32IDE_isGitRepository);
    printf("  Hook value: %p\n", hook);
    printf("  Expected:   %p\n", (void*)test_isGitRepository);
    printf("  Match: %s\n", (hook == (void*)test_isGitRepository) ? "YES" : "NO");
    
    // Final diagnostics
    printf("\nFinal state:\n");
    RuntimePatcher::PrintDiagnostics();
    
    return 0;
}

#endif // RUNTIME_PATCHER_TEST
