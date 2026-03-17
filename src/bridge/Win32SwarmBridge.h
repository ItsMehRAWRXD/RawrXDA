#pragma once
#include <windows.h>
#include <cstdint>

namespace RawrXD::Bridge {

#pragma pack(push, 1)
struct SwarmInitConfig {
    uint32_t structSize;          // sizeof(SwarmInitConfig)
    uint32_t maxSubAgents;        // 1-64 workers
    uint32_t taskTimeoutMs;       // Per-task timeout
    uint8_t  enableGPUWorkStealing;
    uint8_t  reserved[3];
    char     coordinatorModel[64]; // GGUF model name/path
};
#pragma pack(pop)

// Structure for internal swarm context tracking
struct SwarmContext {
    void* topology; // Placeholder for Agentic::SwarmTopology
    void* coordinator; // Placeholder for Inference::RawrXDInference
    uint64_t creationTime;
};

// Internal implementation called by the IAT-bound C-function
int InitializeSwarmSystemImpl(void* rawConfig);

// Called by Win32IDE_Window.cpp (previously commented out)
int InitializeSwarmSystem(SwarmInitConfig* config);

// Runtime registration with RuntimePatcher
bool RegisterSwarmBridgeWithIAT();

// Cleanup
void ShutdownSwarmSystem();

} // namespace RawrXD::Bridge

// Exported IAT target (masquerade slot 20)
extern "C" __declspec(dllexport) int Win32IDE_initializeSwarmSystem(void* config);

// IAT Slot exports — forward declarations for SwarmIATRegistration.cpp
extern "C" {
    // Slots 21-23: UI Tab/Accelerator
    __declspec(dllexport) void* Win32IDE_createAcceleratorTable(void* pTableData, int count);
    __declspec(dllexport) bool  Win32IDE_removeTab(int tabIndex);
    __declspec(dllexport) bool  Win32IDE_addTab(const char* title, void* pContent);

    // Slots 24-27: Sidebar
    __declspec(dllexport) bool Win32IDE_addSidebarPanel(const char* id, const char* title, void* pContent);
    __declspec(dllexport) bool Win32IDE_removeSidebarPanel(const char* id);
    __declspec(dllexport) void Win32IDE_showSidebarPanel(const char* id);
    __declspec(dllexport) void Win32IDE_hideSidebarPanel(const char* id);

    // Slots 48-53: AgenticBridge / SubAgentManager
    __declspec(dllexport) void*    AgenticBridge_GetSubAgentManager();
    __declspec(dllexport) const char* SubAgentManager_getStatusSummary(void* pMgr);
    __declspec(dllexport) uint32_t SubAgentManager_getAgentCount(void* pMgr);
    __declspec(dllexport) int      SubAgentManager_isHealthy(void* pMgr);
    __declspec(dllexport) float    SubAgentManager_getLoadAverage(void* pMgr);
    __declspec(dllexport) void     SubAgentManager_broadcastCommand(void* pMgr, const char* cmd);

    // Slots 54-55: Swarm Execution / Shutdown
    __declspec(dllexport) uint32_t Win32IDE_executeSwarmTask(const char* taskDesc);
    __declspec(dllexport) void     Win32IDE_shutdownSwarmSystem();

    // Slots 56-60: AgenticBridge Config
    __declspec(dllexport) void AgenticBridge_SetModelPath(const char* path);
    __declspec(dllexport) bool AgenticBridge_GetModelPath(char* buffer, uint32_t bufferSize);
    __declspec(dllexport) void AgenticBridge_UpdateStatus(const char* status);
    __declspec(dllexport) bool AgenticBridge_GetAPIKey(char* buffer, uint32_t bufferSize);
    __declspec(dllexport) void AgenticBridge_SetAPIKey(const char* key);

    // Slots 61-63: AgenticBridge Context
    __declspec(dllexport) void* AgenticBridge_GetContext();
    __declspec(dllexport) void  AgenticBridge_SetContext(void* pContext);
    __declspec(dllexport) void  AgenticBridge_ResetContext();
}
