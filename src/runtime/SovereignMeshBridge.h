#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include "SovereignTelemetryHub.h"

#pragma comment(lib, "Ws2_32.lib")

namespace RawrXD::Runtime {

struct MeshNode {
    uint32_t nodeId;
    std::string address;
    MetricSnapshot lastSnapshot;
    bool isActive;
    bool isTrusted;
};

class SovereignMeshBridge {
public:
    static SovereignMeshBridge& instance();

    bool initialize(uint32_t localNodeId);
    void shutdown();

    // 🌐 Cold-Start Bootstrap (Batch 32)
    bool performColdStartJoin(const std::string& peerAddr);

    // Peer-to-Peer Operations
    void broadcastLocalStats(const MetricSnapshot& snap);
    std::vector<MeshNode> getActivePeers();

    // Distributed Load-Balancer logic (Phase 40.2)
    const MeshNode* findBestPeerForTask(uint32_t requiredResources);

private:
    SovereignMeshBridge();
    ~SovereignMeshBridge();

    void listenThreadLoop();

    bool m_active;
    uint32_t m_localNodeId;
    SOCKET m_socket;
    std::vector<MeshNode> m_peers;
    std::mutex m_peerMutex;
    std::thread m_listenThread;
};

} // namespace RawrXD::Runtime
