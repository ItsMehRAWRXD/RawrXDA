#include <iostream>
#include <vector>
#include <winsock2.h>
#include "../headers/rawrxd_swarm_protocol.h"

#pragma comment(lib, "ws2_32.lib")

extern "C" {
    int64_t RawrXD_Tensor_SliceAndDistribute(void* tensor, uint64_t totalSize, uint64_t nodeCount, SOCKET* nodes);
}

// Mock of the Sync kernel for integration test if real one is not linked
// In a real build, we link the RawrXD_Swarm_Link.obj
/*
extern "C" int64_t RawrXD_Swarm_SyncTensorShard(SOCKET s, SwarmTensorShard* header, void* buffer, int direction) {
    std::cout << "  [Kernel] Syncing Shard: Offset=" << header->Offset << " Size=" << header->Size << std::endl;
    return 1; 
}
*/

int main() {
    std::cout << "[Phase 14] Scaling Inference Cluster: Tensor Distribution Test" << std::endl;

    const uint64_t TOTAL_SIZE = 1024 * 1024; // 1MB Mock Tensor
    std::vector<uint8_t> mockTensor(TOTAL_SIZE, 0x42); 

    const int NODE_COUNT = 4;
    SOCKET mockSockets[NODE_COUNT] = { (SOCKET)INVALID_SOCKET, (SOCKET)INVALID_SOCKET, (SOCKET)INVALID_SOCKET, (SOCKET)INVALID_SOCKET };

    std::cout << "Slicing " << TOTAL_SIZE << " bytes across " << NODE_COUNT << " nodes..." << std::endl;

    // NOTE: This test will fail if RawrXD_Swarm_SyncTensorShard actually calls 'send()'
    // on these invalid sockets. For a true logic-only test, we would mock the sync kernel.
    // However, since we are linked to the real ASM, we expect it to return -1 (network error).
    int64_t result = RawrXD_Tensor_SliceAndDistribute(mockTensor.data(), TOTAL_SIZE, NODE_COUNT, mockSockets);

    std::cout << "Distribution Result: " << result << " nodes successfully updated." << std::endl;

    if (result == NODE_COUNT) {
        std::cout << "[SUCCESS] Tensor Slicer Kernel Validated!" << std::endl;
    } else {
        std::cout << "[FAILURE] Distribution Mismatch: " << result << "/" << NODE_COUNT << std::endl;
        return 1;
    }

    return 0;
}
