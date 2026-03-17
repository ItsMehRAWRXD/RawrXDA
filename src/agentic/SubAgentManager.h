#pragma once
#include <string>
#include <memory>
#include <chrono>

namespace RawrXD::Agentic {

struct SwarmTopology {
    uint32_t workerCount;
    std::chrono::milliseconds taskTimeout;
    bool gpuWorkStealing;
};

class SubAgentManager {
public:
    static SubAgentManager& instance();
    
    // Slot 20: Called by Win32SwarmBridge
    bool initializeSwarm(const SwarmTopology& topology, 
                        const std::string& coordinatorModel);
    
    // Slot 55: Called by Win32SwarmBridge
    void shutdownSwarm();
    
    // Additional swarm operations
    uint32_t executeSwarmTask(const std::string& taskDescription);
    bool isSwarmActive() const;
    
private:
    SubAgentManager() = default;
    bool m_initialized{false};
    SwarmTopology m_topology;
    std::string m_coordinatorModel;
};

} // namespace RawrXD::Agentic
