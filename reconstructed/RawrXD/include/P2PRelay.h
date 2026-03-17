#pragma once
#include <winsock2.h>

extern "C" {
    // UDP Hole Punching with STUN binding
    // Returns NAT_TYPE_* (0-5) on success, -1 on failure
    // Mapped address stored in outMappedAddr
    int UDPHolePunch(uint64_t localPort, const char* stunServerIP, uint16_t stunPort, sockaddr_in* outMappedAddr);

    // QUIC Frame Relay — parse/relay QUIC frames without crypto
    int QUICFrameRelay(uint64_t socket, void* buffer, size_t len, sockaddr_in* targetAddr);

    // DTLS 1.3 Encryption placeholder (ChaCha20-Poly1305)
    void DTLS13_Encrypt(void* data, size_t len);
}