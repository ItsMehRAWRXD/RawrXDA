#pragma once
#include <winsock2.h>

// STUN
extern "C" {
    int  STUN_InitContext(int serverIndex, void* ctx);
    void STUN_SendBindingRequest(void* ctx, SOCKET sock);
    int  STUN_ParseResponse(void* ctx, void* buf, int len, sockaddr_in* outAddr);

    // P2P
    int  UDPHolePunch(void* p2pCtx, sockaddr_in* peerAddr);
    int  DetectNATType(void* stunCtx);  // Returns NAT_* constants

    // QUIC
    int  QUIC_ParseFrame(void* buf, int len, uint8_t* frameType, void** payload);
    void ChaCha20_Block(void* state, void* out);
}

struct P2PContext {
    SOCKET localSocket;
    sockaddr_in publicAddr;
    sockaddr_in peerAddr;
    int natType;
    bool holePunched;
    bool quicEnabled;
    void* cryptoCtx;
};

// UI Extension for Protocol Dropdown
// In Win32IDE_NetworkPanel.cpp, modify PortForwardEntry:
struct PortForwardEntry {
    uint16_t localPort;
    uint16_t remotePort;
    std::string label;
    std::string protocol;      // "TCP", "UDP", "QUIC", "STUN"
    // ... rest
};