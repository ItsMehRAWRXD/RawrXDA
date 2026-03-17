#pragma once
#include <winsock2.h>

extern "C" {
    // STUN Functions
    int STUN_CreateBindingRequest(void* buffer, void* transactionID);

    // NAT Detection
    int NAT_DetectType(uint64_t localPort, const char* stunServer1, const char* stunServer2, sockaddr_in* outMappedAddr);

    // UDP Hole Punching with STUN
    int UDPHolePunch(uint64_t localPort, const char* stunServerIP, uint16_t stunPort, sockaddr_in* outMappedAddr);

    // P2P Context Management
    uint64_t P2P_InitContextPool(uint64_t maxContexts);

    // QUIC Frame Relay
    int QUICFrameRelay(uint64_t socket, void* buffer, size_t len, sockaddr_in* targetAddr);

    // DTLS 1.3 Encryption (placeholder)
    void DTLS13_Encrypt(void* data, size_t len);
}