// =============================================================================
// swarm_2node_test.cpp — Phase 14.7: First Cross-Node Inference Test
// =============================================================================
// Minimal Viable Swarm Orchestrator for 2-node pipeline sharding.
// Proves tensor synchronization works between Node A and Node B.
//
// Protocol v1.2: 24-byte header ('SWRM' magic) + 56-byte shard descriptor.
// ASM Sync Kernel: RawrXD_Swarm_SyncTensorShard (d:\rawrxd\src\asm\RawrXD_Swarm_Link.asm)
//
// Pipeline:
//   Node A (Layer 0-39) -> Node B (Layer 40-80) -> Node A (Logits)
// =============================================================================

#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// --- External ASM Prototypes (MASM64) ---
extern "C" {
    /**
     * @brief Synchronizes a tensor shard over a socket using v1.2 protocol.
     * @param s Socket handle to the cluster node.
     * @param header Ptr to SwarmTensorShard struct.
     * @param buffer Ptr to local data buffer.
     * @param direction 0 = Send (Push), 1 = Receive (Pull).
     * @return 1 (Success), 0 (Protocol Error), -1 (Network Error).
     */
    int64_t RawrXD_Swarm_SyncTensorShard(SOCKET s, void* header, void* buffer, int64_t direction);
}

// --- Protocol Structures (v1.2) ---
#pragma pack(push, 1)
struct SwarmHeader {
    uint32_t Magic;         // 'SWRM' (0x4D525753)
    uint32_t Version;       // 0x01020000
    uint64_t Sequence;
    uint8_t  MessageType;
    uint8_t  Pad[7];
};

struct SwarmTensorShard {
    SwarmHeader Header;
    uint64_t TensorID;      // Unique ID for the tensor (e.g., hidden_states)
    uint64_t Offset;        // Start offset (for partial sharding)
    uint64_t Size;          // Payload size in bytes
    uint64_t Checksum;      // Optional CRC64
};
#pragma pack(pop)

// --- Test Configuration ---
#define TEST_MAGIC 0x4D525753
#define TEST_VERSION 0x01020000
#define SHARD_SIZE (1024 * 1024) // 1MB Test Shard

void RunNodeA(const char* targetIP, int port) {
    std::cout << "[Node A] Initializing Layer 0-39..." << std::endl;
    
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, targetIP, &addr.sin_addr);

    std::cout << "[Node A] Connecting to Node B (" << targetIP << ":" << port << ")..." << std::endl;
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[Node A] Connection failed: " << WSAGetLastError() << std::endl;
        return;
    }

    // 1. Prepare Hidden States (Output of Layer 39)
    std::vector<float> hidden_states(SHARD_SIZE / sizeof(float), 1.0f);
    
    SwarmTensorShard shard;
    shard.Header.Magic = TEST_MAGIC;
    shard.Header.Version = TEST_VERSION;
    shard.Header.Sequence = 1;
    shard.Header.MessageType = 0x03; // SW_MSG_TENSOR_SYNC
    shard.TensorID = 0x1337;
    shard.Offset = 0;
    shard.Size = SHARD_SIZE;
    shard.Checksum = 0;

    std::cout << "[Node A] Pushing Hidden States to Node B..." << std::endl;
    int64_t result = RawrXD_Swarm_SyncTensorShard(s, &shard, hidden_states.data(), 0);
    
    if (result == 1) {
        std::cout << "[Node A] Successfully pushed " << SHARD_SIZE << " bytes." << std::endl;
        
        // 2. Wait for Logits from Node B (Output of Layer 80)
        std::cout << "[Node A] Waiting for Logits from Node B..." << std::endl;
        result = RawrXD_Swarm_SyncTensorShard(s, &shard, hidden_states.data(), 1);
        
        if (result == 1) {
            std::cout << "[Node A] Received Logits! Sync verified." << std::endl;
        } else {
            std::cerr << "[Node A] Pull failed: " << result << std::endl;
        }
    } else {
        std::cerr << "[Node A] Push failed: " << result << std::endl;
    }

    closesocket(s);
    WSACleanup();
}

void RunNodeB(int port) {
    std::cout << "[Node B] Initializing Layer 40-80..." << std::endl;
    
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    bind(listenSock, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenSock, 1);

    std::cout << "[Node B] Listening for Node A on port " << port << "..." << std::endl;
    SOCKET s = accept(listenSock, NULL, NULL);

    // 1. Receive Hidden States from Node A
    SwarmTensorShard shard;
    std::vector<float> buffer(SHARD_SIZE / sizeof(float));

    std::cout << "[Node B] Pulling Hidden States from Node A..." << std::endl;
    int64_t result = RawrXD_Swarm_SyncTensorShard(s, &shard, buffer.data(), 1);

    if (result == 1) {
        std::cout << "[Node B] Received " << shard.Size << " bytes for TensorID " << std::hex << shard.TensorID << std::dec << std::endl;
        
        // 2. Mock Inference (Layer 40-80)
        std::cout << "[Node B] Processing Layers 40-80..." << std::endl;
        for (auto& f : buffer) f *= 2.0f; // Mock operation

        // 3. Push Logits back to Node A
        shard.Header.Sequence++;
        std::cout << "[Node B] Pushing Logits back to Node A..." << std::endl;
        result = RawrXD_Swarm_SyncTensorShard(s, &shard, buffer.data(), 0);
        
        if (result == 1) {
            std::cout << "[Node B] Successfully pushed Logits." << std::endl;
        }
    } else {
        std::cerr << "[Node B] Pull failed: " << result << std::endl;
    }

    closesocket(s);
    closesocket(listenSock);
    WSACleanup();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: swarm_2node_test [nodeA|nodeB] [targetIP] [port]" << std::endl;
        return 0;
    }

    std::string mode = argv[1];
    int port = (argc > 3) ? std::stoi(argv[3]) : 8888;

    if (mode == "nodeA") {
        const char* target = (argc > 2) ? argv[2] : "127.0.0.1";
        RunNodeA(target, port);
    } else if (mode == "nodeB") {
        RunNodeB(port);
    }

    return 0;
}
