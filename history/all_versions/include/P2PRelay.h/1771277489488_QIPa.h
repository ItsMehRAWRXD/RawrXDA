#pragma once
#include <winsock2.h>

extern "C" {
    // UDP Hole Punching with STUN
    int UDPHolePunch(uint64_t localPort, const char* stunServerIP, uint16_t stunPort, sockaddr_in* outMappedAddr);

    // QUIC Frame Relay
    int QUICFrameRelay(uint64_t socket, void* buffer, size_t len, sockaddr_in* targetAddr);

    // DTLS 1.3 Encryption (placeholder)
    void DTLS13_Encrypt(void* data, size_t len);

    // NAT Type Detection
    int DetectNatType(void* context);
}