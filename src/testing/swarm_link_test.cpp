#include <iostream>
#include <vector>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../headers/rawrxd_swarm_protocol.h"

#pragma comment(lib, "ws2_32.lib")

extern "C" {
    HANDLE RawrXD_Swarm_InitializeNode(uint32_t address, uint16_t port);
    int64_t RawrXD_Swarm_SyncTensorShard(SOCKET s, SwarmTensorShard* header, void* buffer, int direction);
    int RawrXD_Swarm_SendBuffer(SOCKET s, void* buf, int len, int flags);
    int RawrXD_Swarm_RecvBuffer(SOCKET s, void* buf, int len, int flags);
}

bool run_loopback_test() {
    std::cout << "[Test 1] Loopback Handshake Initializing..." << std::endl;
    
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 1. Create Listener (Server)
    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1337);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    bind(listenSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(listenSock, 1);

    // 2. Client Handshake (using MASM kernel)
    uint16_t port = 1337;
    sockaddr_in clientAddr = {0};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &clientAddr.sin_addr);

    // Give server a moment to start listening
    Sleep(100);

    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connect(clientSock, (sockaddr*)&clientAddr, sizeof(clientAddr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        std::cerr << "Client connection failed! Error: " << err << std::endl;
        closesocket(listenSock);
        WSACleanup();
        return false;
    }

    SOCKET acceptSock = accept(listenSock, NULL, NULL);
    if (acceptSock == INVALID_SOCKET) {
        std::cerr << "Accept failed!" << std::endl;
        return false;
    }

    // 3. Exchange Handshake
    SwarmHandshake hsStart = {0};
    hsStart.Header.Magic = 0x4D525753; // 'SWRM'
    hsStart.Header.Version = SW_PROTOCOL_VERSION;
    hsStart.NodeID = 101;
    hsStart.TotalNodes = 2;

    RawrXD_Swarm_SendBuffer(clientSock, &hsStart, sizeof(hsStart), 0);
    
    SwarmHandshake hsRecv = {0};
    RawrXD_Swarm_RecvBuffer(acceptSock, &hsRecv, sizeof(hsRecv), 0);

    if (hsRecv.Header.Magic == hsStart.Header.Magic && hsRecv.NodeID == 101) {
        std::cout << "[SUCCESS] Loopback Handshake Verified (Magic: " << std::hex << hsRecv.Header.Magic << ")" << std::endl;
    } else {
        std::cerr << "[FAILURE] Handshake Mismatch!" << std::endl;
        return false;
    }

    // 4. Test Tensor Sync (MASM Kernel)
    std::vector<uint8_t> dummyData(1024, 0xAA);
    SwarmTensorShard shardHeader = {0};
    shardHeader.Header.Magic = 0x4D525753;
    shardHeader.Size = dummyData.size();
    shardHeader.TensorID = 999;

    std::cout << "[Test 2] Shard Transfer (MASM Kernel)..." << std::endl;
    std::cout << "Pushing Shard Header..." << std::endl;
    int64_t pushRes = RawrXD_Swarm_SyncTensorShard(clientSock, &shardHeader, dummyData.data(), 0); // Push
    std::cout << "Push Result: " << pushRes << std::endl;

    std::vector<uint8_t> recvData(1024, 0);
    // Receive header first in standard way
    SwarmTensorShard recvHeader = {0};
    
    // DEBUG: Print local struct sizes
    std::cout << "Local sizeof(SwarmHeader): " << sizeof(SwarmHeader) << std::endl;
    std::cout << "Local sizeof(SwarmTensorShard): " << sizeof(SwarmTensorShard) << std::endl;
    std::cout << "Local offsetof(Size): " << offsetof(SwarmTensorShard, Size) << std::endl;

    std::cout << "Waiting for Header (Expected " << sizeof(SwarmTensorShard) << " bytes)..." << std::endl;
    
    char* hdrBuf = (char*)&recvHeader;
    int hdrToRecv = sizeof(recvHeader);
    int hdrRecvd = 0;
    while(hdrRecvd < hdrToRecv) {
        int r = recv(acceptSock, hdrBuf + hdrRecvd, hdrToRecv - hdrRecvd, 0);
        if (r <= 0) {
            std::cerr << "Header recv failed or closed! r=" << r << " err=" << WSAGetLastError() << std::endl;
            break;
        }
        hdrRecvd += r;
        std::cout << "  Received " << r << " header bytes..." << std::endl;
    }

    std::cout << "Header Recv Bytes: " << hdrRecvd << " Magic: " << std::hex << recvHeader.Header.Magic << std::dec << " Size: " << recvHeader.Size << std::endl;
    
    // Use MASM to pull data
    std::cout << "Pulling Shard Data..." << std::endl;
    int64_t pullRes = RawrXD_Swarm_SyncTensorShard(acceptSock, &recvHeader, recvData.data(), 1); // Pull
    std::cout << "Pull Result: " << pullRes << std::endl;

    if (recvData == dummyData) {
        std::cout << "[SUCCESS] 1KB Shard Integrity Verified" << std::endl;
    } else {
        std::cerr << "[FAILURE] Data Corruption in MASM Kernel Transfer!" << std::endl;
        for(int i=0; i<16; i++) {
            printf("Sent[%d]: %02X Recv[%d]: %02X\n", i, dummyData[i], i, recvData[i]);
        }
        return false;
    }

    closesocket(clientSock);
    closesocket(acceptSock);
    closesocket(listenSock);
    WSACleanup();
    return true;
}

int main(int argc, char** argv) {
    if (run_loopback_test()) return 0;
    return 1;
}
