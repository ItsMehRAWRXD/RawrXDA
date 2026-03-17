#ifndef SWARM_ORCHESTRATOR_H
#define SWARM_ORCHESTRATOR_H

#include <vector>
#include <string>
#include <map>
#include "rawrxd_swarm_protocol.h"

// Forward-declare for C link
extern "C" {
    uintptr_t RawrXD_Swarm_InitializeNode(uint32_t ip, uint16_t port);
    int RawrXD_Swarm_SendBuffer(uintptr_t sock, const void* buf, size_t len, int flags);
    int RawrXD_Swarm_RecvBuffer(uintptr_t sock, void* buf, size_t len, int flags);
    void RawrXD_Swarm_CloseNode(uintptr_t sock);
    
    // Distributor kernels
    void RawrXD_AVX512_TensorShard(const void* src, void* dst, size_t blocks_64b, uint32_t shard_idx);
    void RawrXD_AVX2_TensorShard(const void* src, void* dst, size_t blocks_32b, uint32_t shard_idx);
}

class SwarmNode {
public:
    SWARM_NODE_ID id;
    uintptr_t socket;
    bool isActive;
    
    SwarmNode(uint32_t ip, uint16_t port) : isActive(true) {
        id.IPv4 = ip;
        id.Port = port;
        socket = RawrXD_Swarm_InitializeNode(ip, port);
    }
    
    ~SwarmNode() {
        if (socket != (uintptr_t)-1) {
            RawrXD_Swarm_CloseNode(socket);
        }
    }
};

class SwarmOrchestrator {
private:
    std::vector<SwarmNode*> activeNodes;
    std::map<uint64_t, std::vector<uint8_t>> localTensorCache;
    
public:
    SwarmOrchestrator() {}
    ~SwarmOrchestrator();

    bool RegisterNode(uint32_t ip, uint16_t port);
    bool SyncCluster();
    bool DistributeTensor(uint64_t tensorId, const void* data, size_t size);
    bool ExecuteDistributedInference(uint64_t tensorId, const std::string& inputPrompt);

    size_t GetNodeCount() const { return activeNodes.size(); }
};

#endif // SWARM_ORCHESTRATOR_H
