#include "swarm_orchestrator.h"
#include <iostream>
#include <vector>
#include <memory>
#include <cstring>

SwarmOrchestrator::~SwarmOrchestrator() {
    for(auto node : activeNodes) {
        delete node;
    }
    activeNodes.clear();
}

bool SwarmOrchestrator::RegisterNode(uint32_t ip, uint16_t port) {
    auto node = new SwarmNode(ip, port);
    if (node->socket != (uintptr_t)-1) {
        activeNodes.push_back(node);
        std::cout << "[Swarm] Registered Node " << (ip & 0xFF) << "." << ((ip >> 8) & 0xFF) << " Port: " << port << std::endl;
        return true;
    }
    delete node;
    return false;
}

bool SwarmOrchestrator::SyncCluster() {
    if (activeNodes.empty()) return false;
    
    SWARM_HEADER header;
    header.Magic = SWARM_MAGIC;
    header.MessageType = REQ_SWARM_SYNC;
    header.PayloadSize = 0;
    header.SequenceId = 1;
    
    for (auto node : activeNodes) {
        int res = RawrXD_Swarm_SendBuffer(node->socket, &header, sizeof(header), 0);
        if (res <= 0) {
            node->isActive = false;
        }
    }
    return true;
}

bool SwarmOrchestrator::DistributeTensor(uint64_t tensorId, const void* data, size_t size) {
    if (activeNodes.empty() || !data || size == 0) return false;
    
    size_t numNodes = activeNodes.size();
    size_t shardSize = size / numNodes;
    
    for (size_t i = 0; i < numNodes; ++i) {
        if (!activeNodes[i]->isActive) continue;
        
        SWARM_HEADER header;
        header.Magic = SWARM_MAGIC;
        header.MessageType = MSG_TENSOR_CHUNK;
        header.PayloadSize = (uint32_t)(sizeof(TENSOR_CHUNK_INFO) + shardSize);
        header.SequenceId = i + 100;
        
        TENSOR_CHUNK_INFO chunkInfo;
        chunkInfo.TensorId = tensorId;
        chunkInfo.Offset = i * shardSize;
        chunkInfo.TotalSize = size;
        chunkInfo.ElementType = 0; // Default F32
        
        // Prepare shard buffer
        std::vector<uint8_t> shardData(shardSize);
        
        // Call ASM kernel for fast chunking (simulating copy/transformation)
        // Here we assume AVX2 for broad compatibility fallback if not detected
        size_t blocks32b = shardSize / 32;
        RawrXD_AVX2_TensorShard((const uint8_t*)data + (i * shardSize), shardData.data(), blocks32b, (uint32_t)i);
        
        // Send Header + Info + Shard
        RawrXD_Swarm_SendBuffer(activeNodes[i]->socket, &header, sizeof(header), 0);
        RawrXD_Swarm_SendBuffer(activeNodes[i]->socket, &chunkInfo, sizeof(chunkInfo), 0);
        RawrXD_Swarm_SendBuffer(activeNodes[i]->socket, shardData.data(), shardSize, 0);
    }
    
    return true;
}

bool SwarmOrchestrator::ExecuteDistributedInference(uint64_t tensorId, const std::string& inputPrompt) {
    // This would send MSG_INFERENCE_EXEC to all nodes and wait for partial outputs
    // Implementation of partial result collection skipped for concise demonstration
    return true;
}
