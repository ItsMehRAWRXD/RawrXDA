#pragma once
#include <winsock2.h>

extern "C" {
    // STUN Functions
    int STUN_CreateBindingRequest(void* buffer, size_t bufferSize, void* transactionID);

    // NAT Detection
    int NAT_DetectType(sockaddr_in* stunServer1, sockaddr_in* stunServer2);

    // UDP Hole Punching with STUN
    SOCKET UDPHolePunch(int localPort, sockaddr_in* peerPublicAddr, sockaddr_in* peerLocalAddr);

    // P2P Context Management
    uint64_t P2P_InitContextPool(uint64_t maxContexts);

    // QUIC Frame Relay
    int QUICFrameRelay(uint64_t socket, void* buffer, size_t len, sockaddr_in* targetAddr);

    // DTLS 1.3 Encryption (placeholder)
    void DTLS13_Encrypt(void* data, size_t len);
}