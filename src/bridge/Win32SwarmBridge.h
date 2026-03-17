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
