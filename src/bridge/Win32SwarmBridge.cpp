#include "Win32SwarmBridge.h"
#include "../agentic/SubAgentManager.h"
#include <windows.h>
#include <memory>
#include <atomic>
#include <cstdio>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward-declare the IAT slot 20 export (defined below)
// The definition at the bottom of this file provides the implementation.

namespace RawrXD::Bridge {

static std::unique_ptr<SwarmContext> g_swarmContext;
static std::atomic<bool> g_swarmActive{false};

int WINAPI InitializeSwarmSystemImpl(void* rawConfig) {
    if (!rawConfig) {
        return E_INVALIDARG;
    }

    if (g_swarmActive.exchange(true)) {
        return S_OK; // Idempotent success
    }

    auto* config = static_cast<SwarmInitConfig*>(rawConfig);
    
    // Validate config
    if (config->structSize != sizeof(SwarmInitConfig)) {
        g_swarmActive = false;
        return E_INVALIDARG;
    }

    if (config->maxSubAgents == 0 || config->maxSubAgents > 64) {
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
            g_swarmActive = false;
            return E_FAIL;
        }

        g_swarmContext = std::make_unique<SwarmContext>();
        g_swarmContext->creationTime = GetTickCount64();

        return S_OK;
    }
    catch (...) {
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
}

extern "C" __declspec(dllexport) bool AgenticBridge_GetModelPath(char* buffer, uint32_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;
    strcpy_s(buffer, bufferSize, g_ModelPath);
    return true;
}

extern "C" __declspec(dllexport) void AgenticBridge_UpdateStatus(const char* status) {
    // Status update processed
    (void)status;
}

extern "C" __declspec(dllexport) bool AgenticBridge_GetAPIKey(char* buffer, uint32_t bufferSize) {
    if (!buffer || bufferSize == 0) return false;
    strcpy_s(buffer, bufferSize, g_APIKey);
    return true;
}

extern "C" __declspec(dllexport) void AgenticBridge_SetAPIKey(const char* key) {
    if (key) strcpy_s(g_APIKey, key);
}

// AgenticBridge Context (Slots 61-63)
static void* g_pAgenticContext = nullptr;

namespace {

struct TabEntry {
    std::string title;
    void* content;
};

struct SidebarPanelEntry {
    std::string title;
    void* content;
    bool visible;
};

std::mutex g_uiBridgeStateMutex;
std::vector<TabEntry> g_tabs;
std::unordered_map<std::string, SidebarPanelEntry> g_sidebarPanels;

} // namespace

extern "C" __declspec(dllexport) void* AgenticBridge_GetContext() {
    return g_pAgenticContext;
}

extern "C" __declspec(dllexport) void AgenticBridge_SetContext(void* pContext) {
    g_pAgenticContext = pContext;
}

extern "C" __declspec(dllexport) void AgenticBridge_ResetContext() {
    g_pAgenticContext = nullptr;
}

// Win32IDE UI Components (Slots 21-23)
extern "C" __declspec(dllexport) void* Win32IDE_createAcceleratorTable(void* pTableData, int count) {
    if (!pTableData || count <= 0) {
        return nullptr;
    }

    HACCEL haccel = CreateAcceleratorTableA(static_cast<LPACCEL>(pTableData), count);
    if (!haccel) {
        return nullptr;
    }

    return haccel;
}

extern "C" __declspec(dllexport) bool Win32IDE_removeTab(int tabIndex) {
    if (tabIndex < 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_uiBridgeStateMutex);
    if (static_cast<size_t>(tabIndex) >= g_tabs.size()) {
        char outOfRange[96];
        sprintf_s(outOfRange, "[Win32IDE] removeTab index out of range=%d\n", tabIndex);
        OutputDebugStringA(outOfRange);
        return false;
    }

    g_tabs.erase(g_tabs.begin() + tabIndex);
    char buf[64];
    sprintf_s(buf, "[Win32IDE] removeTab index=%d remaining=%zu\n", tabIndex, g_tabs.size());
    OutputDebugStringA(buf);
    return true;
}

extern "C" __declspec(dllexport) bool Win32IDE_addTab(const char* title, void* pContent) {
    if (!title || title[0] == '\0') {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_uiBridgeStateMutex);
    g_tabs.push_back(TabEntry{title, pContent});
    return true;
}

// Sidebar Implementation (Slots 24-27)
extern "C" __declspec(dllexport) bool Win32IDE_addSidebarPanel(const char* id, const char* title, void* pContent) {
    if (!id || !title) return false;

    std::lock_guard<std::mutex> lock(g_uiBridgeStateMutex);
    g_sidebarPanels[std::string(id)] = SidebarPanelEntry{title, pContent, true};

    char buf[256];
    sprintf_s(buf, "[Win32IDE] addSidebarPanel ID=%s Title=%s count=%zu\n", id, title, g_sidebarPanels.size());
    OutputDebugStringA(buf);
    return true;
}

extern "C" __declspec(dllexport) bool Win32IDE_removeSidebarPanel(const char* id) {
    if (!id) return false;

    std::lock_guard<std::mutex> lock(g_uiBridgeStateMutex);
    const size_t erased = g_sidebarPanels.erase(std::string(id));

    char buf[128];
    sprintf_s(buf, "[Win32IDE] removeSidebarPanel ID=%s removed=%zu\n", id, erased);
    OutputDebugStringA(buf);
    return erased > 0;
}

extern "C" __declspec(dllexport) void Win32IDE_showSidebarPanel(const char* id) {
    if (!id) return;

    std::lock_guard<std::mutex> lock(g_uiBridgeStateMutex);
    auto it = g_sidebarPanels.find(std::string(id));
    if (it == g_sidebarPanels.end()) {
        char missing[160];
        sprintf_s(missing, "[Win32IDE] showSidebarPanel missing ID=%s\n", id);
        OutputDebugStringA(missing);
        return;
    }

    it->second.visible = true;

    char buf[128];
    sprintf_s(buf, "[Win32IDE] showSidebarPanel ID=%s\n", id);
    OutputDebugStringA(buf);
}

extern "C" __declspec(dllexport) void Win32IDE_hideSidebarPanel(const char* id) {
    if (!id) return;

    std::lock_guard<std::mutex> lock(g_uiBridgeStateMutex);
    auto it = g_sidebarPanels.find(std::string(id));
    if (it == g_sidebarPanels.end()) {
        char missing[160];
        sprintf_s(missing, "[Win32IDE] hideSidebarPanel missing ID=%s\n", id);
        OutputDebugStringA(missing);
        return;
    }

    it->second.visible = false;

    char buf[128];
    sprintf_s(buf, "[Win32IDE] hideSidebarPanel ID=%s\n", id);
    OutputDebugStringA(buf);
}

extern "C" __declspec(dllexport) uint32_t Win32IDE_executeSwarmTask(const char* taskDesc) {
    if (!taskDesc) return 0;
    return RawrXD::Agentic::SubAgentManager::instance().executeSwarmTask(taskDesc);
}

extern "C" __declspec(dllexport) void Win32IDE_shutdownSwarmSystem() {
    RawrXD::Bridge::ShutdownSwarmSystem();
}
