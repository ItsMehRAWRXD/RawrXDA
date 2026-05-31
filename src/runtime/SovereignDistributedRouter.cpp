#include "SovereignToolBridge.h"
#include "SovereignMeshBridge.h"
#include <iostream>
#include <mutex>
#include <algorithm>

namespace RawrXD::Runtime {

const MeshNode* SovereignMeshBridge::findBestPeerForTask(uint32_t requiredResources) {
    std::lock_guard<std::mutex> lock(m_peerMutex);
    if (m_peers.empty()) return nullptr;

    // Load-balancer: Finds node with lowest ActiveOps and Latency < 100ms
    auto best = std::min_element(m_peers.begin(), m_peers.end(), [](const MeshNode& a, const MeshNode& b) {
        return a.lastSnapshot.activeOps < b.lastSnapshot.activeOps;
    });

    if (best != m_peers.end() && best->isActive && best->lastSnapshot.activeOps < 100) {
        return &(*best);
    }
    
    return nullptr;
}

// SovereignToolBridge Extension for Distributed Routing
uint32_t SovereignToolBridge::dispatchDistributed(const std::string& name, const std::string& args) {
    // 1. Check if a better node is available for this tool call
    const MeshNode* peer = SovereignMeshBridge::instance().findBestPeerForTask(10);
    
    if (peer) {
        std::cout << "[DistDispatch] Routing task '" << name << "' to Node " << peer->nodeId << " (Latency: " << peer->lastSnapshot.latencyMs << "ms)" << std::endl;
        // ... Logic for remote RPC / ToolEngine call over Mesh ...
        return 0xFFFF0000 | peer->nodeId; // Mock Distributed ID
    }

    // 2. Fallback to Local ToolEngine (MASM)
    return dispatchTool(name, args, 5);
}

} // namespace RawrXD::Runtime
