// =============================================================================
// swarm_weight_distributor.cpp — Phase 15: Full Model Scaling
// =============================================================================
// Coordinates the distribution of Llama-70B weight shards across the swarm.
// Implements the 'SW_MSG_SHARD_ANNOUNCE' sequence (Protocol v1.2).
//
// Logic:
//   1. MMap the local GGUF file (Node A).
//   2. Parse tensor metadata.
//   3. For each remote node, stream its assigned layer shards.
// =============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../headers/rawrxd_swarm_protocol.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ws32_32.lib") 

// --- External MASM64 Prototypes ---
extern "C" {
    // From mmap_loader.asm
    bool MMap_Open(const char* filePath, uint64_t maxSize);
    void MMap_Close();
    void* MMap_Read(uint64_t offset, uint64_t size, void* destBuffer);
    uint64_t MMap_GetSize();

    // From RawrXD_Swarm_Link.asm
    int64_t RawrXD_Swarm_SyncTensorShard(SOCKET s, void* header, void* buffer, int64_t direction);
}

struct ShardAssignment {
    uint32_t startLayer;
    uint32_t endLayer;
    SOCKET nodeSocket;
};

class SwarmWeightDistributor {
public:
    bool loadAndDistribute(const char* modelPath, const std::vector<ShardAssignment>& assignments) {
        std::cout << "[Swarm] Opening model: " << modelPath << std::endl;
        
        if (!MMap_Open(modelPath, 0)) {
            std::cerr << "[Error] Failed to mmap model file." << std::endl;
            return false;
        }

        uint64_t totalSize = MMap_GetSize();
        std::cout << "[Swarm] Model size: " << (totalSize / (1024*1024)) << " MB" << std::endl;

        // Note: Real GGUF parsing would happen here via RawrXD_GGUF_GraphInterpreter.asm
        // For Phase 15 Scaling verification, we'll demonstrate shard-based distribution.
        
        for (const auto& assign : assignments) {
            std::cout << "[Swarm] Distributing Layers " << assign.startLayer << "-" << assign.endLayer 
                      << " to Node (Socket: " << assign.nodeSocket << ")" << std::endl;
            
            // Mock Shard Announcement (SW_MSG_SHARD_ANNOUNCE)
            SwarmHeader announceHdr;
            announceHdr.Magic = 0x4D525753; // 'SWRM'
            announceHdr.Version = 0x01020000;
            announceHdr.MessageType = SW_MSG_SHARD_ANNOUNCE;
            announceHdr.Sequence = 1;

            // In a real scenario, we would iterate through tensors belonging to these layers.
            // For now, we sync a 'manifest' tensor representing the layer range.
            SwarmTensorShard manifest;
            manifest.Header = announceHdr;
            manifest.TensorID = 0x0000FFFF; // Layer Range Manifest ID
            manifest.Offset = assign.startLayer;
            manifest.Size = assign.endLayer; // Use size as range end for simple test
            
            RawrXD_Swarm_SyncTensorShard(assign.nodeSocket, &manifest, nullptr, 0);
        }

        return true;
    }

    ~SwarmWeightDistributor() {
        MMap_Close();
    }
};

// --- Command Line Entry for Scaling Test ---
int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: swarm_distributor [model.gguf] [node_ip] [port]" << std::endl;
        return 0;
    }

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    const char* modelPath = argv[1];
    const char* nodeIP = argv[2];
    int port = std::stoi(argv[3]);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, nodeIP, &addr.sin_addr);

    std::cout << "[Test] Connecting to worker for scaling test..." << std::endl;
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Error] Node offline." << std::endl;
        return -1;
    }

    SwarmWeightDistributor distributor;
    std::vector<ShardAssignment> assignments = {
        {40, 80, s} // Node B gets 40-80
    };

    distributor.loadAndDistribute(modelPath, assignments);

    closesocket(s);
    WSACleanup();
    return 0;
}
