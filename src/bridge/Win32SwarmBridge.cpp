#include "Win32SwarmBridge.h"
#include "../agentic/SubAgentManager.h"
#include <windows.h>
#include <memory>
#include <atomic>
#include <cstdio>
#include <chrono>

// External reference to the masqueraded IAT slot (populated by RuntimePatcher)
extern "C" {
    // This symbol matches the name expected by runtime_masquerade.asm slot 20
    __declspec(dllexport) int Win32IDE_initializeSwarmSystem(void* config);
}

namespace RawrXD::Bridge {

static std::unique_ptr<SwarmContext> g_swarmContext;
static std::atomic<bool> g_swarmActive{false};

int WINAPI InitializeSwarmSystemImpl(void* rawConfig) {
    if (!rawConfig) {
        OutputDebugStringA("[Win32SwarmBridge] Error: Null configuration (E_INVALIDARG)\n");
        return E_INVALIDARG;
    }

    if (g_swarmActive.exchange(true)) {
        OutputDebugStringA("[Win32SwarmBridge] Swarm already initialized\n");
        return S_OK; // Idempotent success
    }

    auto* config = static_cast<SwarmInitConfig*>(rawConfig);
    
    // Validate config
    if (config->structSize != sizeof(SwarmInitConfig)) {
        OutputDebugStringA("[Win32SwarmBridge] Invalid struct size\n");
        g_swarmActive = false;
        return E_INVALIDARG;
    }

    if (config->maxSubAgents == 0 || config->maxSubAgents > 64) {
        OutputDebugStringA("[Win32SwarmBridge] Invalid swarm configuration\n");
        g_swarmActive = false;
        return E_INVALIDARG;
    }

    try {
        // Bridge to SubAgentManager (IAT slots 54-55)
        auto& manager = RawrXD::Agentic::SubAgentManager::instance();
        
        // Configure swarm topology
        RawrXD::Agentic::SwarmTopology topology;
        topology.workerCount = config->maxSubAgents;
        topology.taskTimeout = std::chrono::milliseconds(config->taskTimeoutMs);
        topology.gpuWorkStealing = (config->enableGPUWorkStealing != 0);
        
        // Initialize coordinator inference model
        if (!manager.initializeSwarm(topology, config->coordinatorModel)) {
            OutputDebugStringA("[Win32SwarmBridge] Failed to initialize coordinator model\n");
            g_swarmActive = false;
            return E_FAIL;
        }

        g_swarmContext = std::make_unique<SwarmContext>();
        g_swarmContext->creationTime = GetTickCount64();

        char msg[256];
        sprintf_s(msg, "[Win32SwarmBridge] Swarm initialized: %u workers, GPU steal=%d, model=%s\n",
                  config->maxSubAgents, (int)config->enableGPUWorkStealing, config->coordinatorModel);
        OutputDebugStringA(msg);

        return S_OK;
    }
    catch (...) {
        OutputDebugStringA("[Win32SwarmBridge] Exception during initialization\n");
        g_swarmActive = false;
        return E_UNEXPECTED;
    }
}

// Cleanup for graceful shutdown
void ShutdownSwarmSystem() {
    if (!g_swarmActive.exchange(false)) return;
    
    if (g_swarmContext) {
        Agentic::SubAgentManager::instance().shutdownSwarm();
        g_swarmContext.reset();
    }
}

int InitializeSwarmSystem(SwarmInitConfig* config) {
    return InitializeSwarmSystemImpl(config);
}

} // namespace RawrXD::Bridge

// C-export for IAT binding (slot 20)
extern "C" __declspec(dllexport) int Win32IDE_initializeSwarmSystem(void* config) {
    return RawrXD::Bridge::InitializeSwarmSystemImpl(config);
}

// AgenticBridge/SubAgent Implementation (Slots 48-51)
extern "C" __declspec(dllexport) void* AgenticBridge_GetSubAgentManager() {
    return &RawrXD::Agentic::SubAgentManager::instance();
}

extern "C" __declspec(dllexport) const char* SubAgentManager_getStatusSummary(void* pMgr) {
    if (!pMgr) return "Invalid Manager";
    // For now, return a static summary to avoid allocation issues in IAT bridge
    static char summary[256];
    auto* mgr = static_cast<RawrXD::Agentic::SubAgentManager*>(pMgr);
    sprintf_s(summary, "Active: %s, Shards: %zu", 
              mgr->isSwarmActive() ? "Yes" : "No", 
              mgr->getActiveShardCount());
    return summary;
}

extern "C" __declspec(dllexport) uint32_t SubAgentManager_getAgentCount(void* pMgr) {
    if (!pMgr) return 0;
    auto* mgr = static_cast<RawrXD::Agentic::SubAgentManager*>(pMgr);
    return (uint32_t)mgr->getActiveShardCount();
}

extern "C" __declspec(dllexport) int SubAgentManager_isHealthy(void* pMgr) {
    if (!pMgr) return 0;
    auto* mgr = static_cast<RawrXD::Agentic::SubAgentManager*>(pMgr);
    return mgr->isSwarmActive() ? 1 : 0;
}

// SLOTS 52-53
extern "C" __declspec(dllexport) float SubAgentManager_getLoadAverage(void* pMgr) {
    if (!pMgr) return 0.0f;
    auto* mgr = static_cast<RawrXD::Agentic::SubAgentManager*>(pMgr);
    return mgr->getSwarmLoadAverage();
}

extern "C" __declspec(dllexport) void SubAgentManager_broadcastCommand(void* pMgr, const char* command) {
    if (!pMgr || !command) return;
    auto* mgr = static_cast<RawrXD::Agentic::SubAgentManager*>(pMgr);
    mgr->broadcastCommand(command);
}

// Global state for AgenticBridge properties (could be moved to manager later)
static char g_ModelPath[MAX_PATH] = { 0 };
static char g_APIKey[128] = { 0 };

// SLOTS 56-60
extern "C" __declspec(dllexport) void AgenticBridge_SetModelPath(const char* path) {
    if (path) strcpy_s(g_ModelPath, path);
    char msg[MAX_PATH + 64];
    sprintf_s(msg, "[AgenticBridge] Model Path set: %s\n", g_ModelPath);
    OutputDebugStringA(msg);
}

extern "C" __declspec(dllexport) bool AgenticBridge_GetModelPath(char* buffer, uint32_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;
    strcpy_s(buffer, bufferSize, g_ModelPath);
    return true;
}

extern "C" __declspec(dllexport) void AgenticBridge_UpdateStatus(const char* status) {
    if (!status) return;
    char msg[256];
    sprintf_s(msg, "[AgenticBridge] Status Update: %s\n", status);
    OutputDebugStringA(msg);
}

extern "C" __declspec(dllexport) bool AgenticBridge_GetAPIKey(char* buffer, uint32_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;
    strcpy_s(buffer, bufferSize, g_APIKey);
    return true;
}

extern "C" __declspec(dllexport) void AgenticBridge_SetAPIKey(const char* key) {
    if (key) strcpy_s(g_APIKey, key);
    OutputDebugStringA("[AgenticBridge] API Key updated\n");
}

// AgenticBridge Context (Slots 61-63)
static void* g_pAgenticContext = nullptr;

extern "C" __declspec(dllexport) void* AgenticBridge_GetContext() {
    return g_pAgenticContext;
}

extern "C" __declspec(dllexport) void AgenticBridge_SetContext(void* pContext) {
    g_pAgenticContext = pContext;
    OutputDebugStringA("[AgenticBridge] Context updated\n");
}

extern "C" __declspec(dllexport) void AgenticBridge_ResetContext() {
    g_pAgenticContext = nullptr;
    OutputDebugStringA("[AgenticBridge] Context reset\n");
}

// Win32IDE UI Components (Slots 21-23)
extern "C" __declspec(dllexport) void* Win32IDE_createAcceleratorTable(void* pTableData, int count) {
    // Stub implementation for UI accelerator table
    OutputDebugStringA("[Win32IDE] createAcceleratorTable stub called\n");
    return CreateAcceleratorTableA(static_cast<LPACCEL>(pTableData), count);
}

extern "C" __declspec(dllexport) bool Win32IDE_removeTab(int tabIndex) {
    char buf[64];
    sprintf_s(buf, "[Win32IDE] removeTab index=%d\n", tabIndex);
    OutputDebugStringA(buf);
    return true; // Stub success
}

extern "C" __declspec(dllexport) bool Win32IDE_addTab(const char* title, void* pContent) {
    char buf[128];
    sprintf_s(buf, "[Win32IDE] addTab title=%s\n", title ? title : "NULL");
    OutputDebugStringA(buf);
    return true; // Stub success
}

// Sidebar Implementation (Slots 24-27)
extern "C" __declspec(dllexport) bool Win32IDE_addSidebarPanel(const char* id, const char* title, void* pContent) {
    if (!id || !title) return false;
    char buf[256];
    sprintf_s(buf, "[Win32IDE] addSidebarPanel ID=%s Title=%s\n", id, title);
    OutputDebugStringA(buf);
    return true; // Stub success
}

extern "C" __declspec(dllexport) bool Win32IDE_removeSidebarPanel(const char* id) {
    if (!id) return false;
    char buf[128];
    sprintf_s(buf, "[Win32IDE] removeSidebarPanel ID=%s\n", id);
    OutputDebugStringA(buf);
    return true; // Stub success
}

extern "C" __declspec(dllexport) void Win32IDE_showSidebarPanel(const char* id) {
    if (!id) return;
    char buf[128];
    sprintf_s(buf, "[Win32IDE] showSidebarPanel ID=%s\n", id);
    OutputDebugStringA(buf);
}

extern "C" __declspec(dllexport) void Win32IDE_hideSidebarPanel(const char* id) {
    if (!id) return;
    char buf[128];
    sprintf_s(buf, "[Win32IDE] hideSidebarPanel ID=%s\n", id);
    OutputDebugStringA(buf);
}

extern "C" __declspec(dllexport) uint32_t Win32IDE_executeSwarmTask(const char* taskDesc) {
    if (!taskDesc) return 0;
    OutputDebugStringA("[Win32SwarmBridge] Executing Swarm Task (Slot 54)\n");
    return RawrXD::Agentic::SubAgentManager::instance().executeSwarmTask(taskDesc);
}

extern "C" __declspec(dllexport) void Win32IDE_shutdownSwarmSystem() {
    OutputDebugStringA("[Win32SwarmBridge] Shutting down Swarm System (Slot 55)\n");
    RawrXD::Bridge::ShutdownSwarmSystem();
}
