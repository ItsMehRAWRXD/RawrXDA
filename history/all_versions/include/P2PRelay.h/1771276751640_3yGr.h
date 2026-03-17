#pragma once
#include <winsock2.h>

extern "C" {
    __declspec(dllimport) int UDPHolePunch(void* p2pCtx, sockaddr_in* peerAddr, int timeoutMs);
    __declspec(dllimport) void QUICFrameRelay(void* p2pCtx);
    __declspec(dllimport) int NAT_DetectType(void* p2pCtx);
    __declspec(dllimport) void P2P_InitContextPool(int maxContexts);

    // Context management
    inline void* P2P_AcquireContext() {
        static int idx = 0;
        return (char*)GetProcAddress(GetModuleHandleA("RawrXD_P2P"), "g_P2PContextPool")
               + (InterlockedIncrement((long*)&idx) % 256) * 88;
    }
}