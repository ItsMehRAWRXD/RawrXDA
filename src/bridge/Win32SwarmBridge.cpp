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
