#include "Win32SwarmBridge.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>

namespace RawrXD::Bridge {

    // IAT slot indices from audit
constexpr int IAT_SLOT_INITIALIZE_SWARM = 20;
constexpr int IAT_SLOT_CREATE_ACCEL = 21;
constexpr int IAT_SLOT_REMOVE_TAB = 22;
constexpr int IAT_SLOT_ADD_TAB = 23;

constexpr int IAT_SLOT_AGENT_BRIDGE_GET = 48;
constexpr int IAT_SLOT_SA_MGR_SUMMARY = 49;
constexpr int IAT_SLOT_SA_MGR_COUNT = 50;
constexpr int IAT_SLOT_SA_MGR_HEALTHY = 51;

constexpr int IAT_SLOT_EXECUTE_SWARM = 54;
constexpr int IAT_SLOT_SHUTDOWN_SWARM = 55;

bool RegisterSwarmBridgeWithIAT() {
    // Resolve runtime patcher exports dynamically to avoid hard link dependencies.
    using InstallHookFn = int(__cdecl*)(const char*, void*);
    using GetHookFn = void*(__cdecl*)(uint64_t);

    HMODULE self = GetModuleHandleA(nullptr);
    if (!self) return false;

    auto installHook = reinterpret_cast<InstallHookFn>(GetProcAddress(self, "RAWRXD_InstallHook"));
    auto getHook = reinterpret_cast<GetHookFn>(GetProcAddress(self, "RAWRXD_GetHook"));

    if (!installHook) {
        OutputDebugStringA("[SwarmIAT] runtime_patcher not present - skipping IAT bind");
        return false;
    }

    auto* initProc = reinterpret_cast<void*>(&Win32IDE_initializeSwarmSystem);
    bool success = installHook("Win32IDE_initializeSwarmSystem", initProc) != 0;

    if (success) {
        OutputDebugStringA("[SwarmIAT] Slot 20 bound to Win32IDE_initializeSwarmSystem\n");

        if (getHook) {
            // --- UI Slots 21-23 ---
            installHook("Win32IDE_createAcceleratorTable", (void*)&Win32IDE_createAcceleratorTable);
            installHook("Win32IDE_removeTab", (void*)&Win32IDE_removeTab);
            installHook("Win32IDE_addTab", (void*)&Win32IDE_addTab);

            // --- Sidebar Slots 24-27 ---
            installHook("Win32IDE_addSidebarPanel", (void*)&Win32IDE_addSidebarPanel);
            installHook("Win32IDE_removeSidebarPanel", (void*)&Win32IDE_removeSidebarPanel);
            installHook("Win32IDE_showSidebarPanel", (void*)&Win32IDE_showSidebarPanel);
            installHook("Win32IDE_hideSidebarPanel", (void*)&Win32IDE_hideSidebarPanel);

            // --- AgenticBridge Slots 48-51 ---
            installHook("AgenticBridge_GetSubAgentManager", (void*)&AgenticBridge_GetSubAgentManager);
            installHook("SubAgentManager_getStatusSummary", (void*)&SubAgentManager_getStatusSummary);
            installHook("SubAgentManager_getAgentCount", (void*)&SubAgentManager_getAgentCount);
            installHook("SubAgentManager_isHealthy", (void*)&SubAgentManager_isHealthy);

            // --- Slots 52-53 ---
            installHook("SubAgentManager_getLoadAverage", (void*)&SubAgentManager_getLoadAverage);
            installHook("SubAgentManager_broadcastCommand", (void*)&SubAgentManager_broadcastCommand);

            // --- Slots 56-60 ---
            installHook("AgenticBridge_SetModelPath", (void*)&AgenticBridge_SetModelPath);
            installHook("AgenticBridge_GetModelPath", (void*)&AgenticBridge_GetModelPath);
            installHook("AgenticBridge_UpdateStatus", (void*)&AgenticBridge_UpdateStatus);
            installHook("AgenticBridge_GetAPIKey", (void*)&AgenticBridge_GetAPIKey);
            installHook("AgenticBridge_SetAPIKey", (void*)&AgenticBridge_SetAPIKey);

            // --- AgenticBridge Context Slots 61-63 ---
            installHook("AgenticBridge_GetContext", (void*)&AgenticBridge_GetContext);
            installHook("AgenticBridge_SetContext", (void*)&AgenticBridge_SetContext);
            installHook("AgenticBridge_ResetContext", (void*)&AgenticBridge_ResetContext);

            // --- Slot 54: Swarm Execution ---
            auto* execProc = reinterpret_cast<void*>(&Win32IDE_executeSwarmTask);
            if (installHook("Win32IDE_executeSwarmTask", execProc)) {
                OutputDebugStringA("[SwarmIAT] Slot 54 bound to Win32IDE_executeSwarmTask\n");
            }
            
            // --- Slot 55: Swarm Shutdown ---
            auto* shutProc = reinterpret_cast<void*>(&Win32IDE_shutdownSwarmSystem);
            if (installHook("Win32IDE_shutdownSwarmSystem", shutProc)) {
                OutputDebugStringA("[SwarmIAT] Slot 55 bound to Win32IDE_shutdownSwarmSystem\n");
            }
        }
    } else {
        OutputDebugStringA("[SwarmIAT] FAILED to bind slot 20\n");
    }

    return success;
}

} // namespace RawrXD::Bridge
