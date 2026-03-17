#pragma once

#include <windows.h>
#include <string>
#include <vector>

/**
 * @file Win32SwarmBridge.h
 * @brief Bridge for the masqueraded IAT Slot 20 (Win32IDE_initializeSwarmSystem).
 * 
 * This bridge connects the Win32 user interface and the masqueraded assembly 
 * entry points with the C++ Agentic Swarm system (SubAgentManager).
 */

namespace RawrXD::Bridge {

struct SwarmInitConfig {
    uint32_t structSize;      // 0: struct size for versioning
    uint32_t maxSubAgents;   // 4: max sub-agents
    uint32_t taskTimeoutMs;  // 8: task timeout in ms
    int enableGPUWorkStealing; // 12: enable GPU work stealing
    char coordinatorModel[64]; // 16: coordinator model name
};

struct SwarmContext {
    uint64_t creationTime;
};

/**
 * @brief Real C++ implementation of the swarm initialization.
 * 
 * @param config Pointer to initialization configuration.
 * @return int S_OK on success, or error code.
 */
int InitializeSwarmSystem(SwarmInitConfig* config);

/**
 * @brief Binds the C++ implementation to the IAT via RuntimePatcher.
 * 
 * Must be called during Win32IDE early initialization.
 */
bool RegisterSwarmBridgeWithIAT();

} // namespace RawrXD::Bridge

/**
 * @brief Masqueraded C-export for IAT Slot 20, 54, 55.
 * 
 * Defined in Win32SwarmBridge.cpp and mapped via RuntimePatcher.
 */
extern "C" {
    __declspec(dllexport) int Win32IDE_initializeSwarmSystem(void* config);
    __declspec(dllexport) uint32_t Win32IDE_executeSwarmTask(const char* taskDesc);
    __declspec(dllexport) void Win32IDE_shutdownSwarmSystem();

    // AgenticBridge/SubAgent (Slots 48-51)
    __declspec(dllexport) void* AgenticBridge_GetSubAgentManager();
    __declspec(dllexport) const char* SubAgentManager_getStatusSummary(void* pMgr);
    __declspec(dllexport) int SubAgentManager_getAgentCount(void* pMgr);
    __declspec(dllexport) bool SubAgentManager_isHealthy(void* pMgr);

    // --- New Symbols (Slots 52, 53, 56-60) ---
    __declspec(dllexport) float SubAgentManager_getLoadAverage(void* pMgr);
    __declspec(dllexport) void SubAgentManager_broadcastCommand(void* pMgr, const char* command);
    __declspec(dllexport) void AgenticBridge_SetModelPath(const char* path);
    __declspec(dllexport) bool AgenticBridge_GetModelPath(char* buffer, uint32_t bufferSize);
    __declspec(dllexport) void AgenticBridge_UpdateStatus(const char* status);
    __declspec(dllexport) bool AgenticBridge_GetAPIKey(char* buffer, uint32_t bufferSize);
    __declspec(dllexport) void AgenticBridge_SetAPIKey(const char* key);

    // AgenticBridge Context (Slots 61-63)
    __declspec(dllexport) void* AgenticBridge_GetContext();
    __declspec(dllexport) void AgenticBridge_SetContext(void* pContext);
    __declspec(dllexport) void AgenticBridge_ResetContext();

    // Win32IDE UI Components (Slots 21-23)
    __declspec(dllexport) void* Win32IDE_createAcceleratorTable(void* pTableData, int count);
    __declspec(dllexport) bool Win32IDE_removeTab(int tabIndex);
    __declspec(dllexport) bool Win32IDE_addTab(const char* title, void* pContent);
}
