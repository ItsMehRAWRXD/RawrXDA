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
 * @brief Masqueraded C-export for IAT Slot 20.
 * 
 * Defined in Win32SwarmBridge.cpp and mapped via RuntimePatcher.
 */
extern "C" {
    __declspec(dllexport) int Win32IDE_initializeSwarmSystem(void* config);
}
