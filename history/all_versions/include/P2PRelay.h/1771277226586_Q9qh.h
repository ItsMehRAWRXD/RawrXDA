#pragma once
#include <winsock2.h>

extern "C" {
    // STUN Functions
    int STUN_CreateBindingRequest(void* buffer, size_t bufSize, void* transactionID);
    int STUN_ParseXorMappedAddress(void* buffer, size_t bufSize, sockaddr_in* outAddr);

    // NAT Detection
    int NAT_DetectType(sockaddr_in* primarySTUN, sockaddr_in* secondarySTUN);

    // UDP Hole Punching
    SOCKET UDPHolePunch(uint16_t localPort, sockaddr_in* peerPublicAddr, sockaddr_in* peerLocalAddr);

    // QUIC Frame Handling
    void QUIC_ParseFrame(void* packet, size_t length, void* callback);

    // Context Management
    void P2P_InitContextPool(int maxContexts);
    void* P2P_AcquireContext();
}