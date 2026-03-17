#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include "P2PRelay.h"

#pragma comment(lib, "ws2_32.lib")

void Benchmark_STUN_HolePunch() {
    printf("Starting P2P test...\n");
    
    // Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        printf("WSAStartup failed: %d\n", wsaResult);
        return;
    }
    printf("Winsock initialized\n");

    printf("Calling P2P_InitContextPool...\n");
    P2P_InitContextPool(16);
    printf("P2P_InitContextPool completed\n");
    
    void* ctx = P2P_AcquireContext();
    printf("Context acquired: %p\n", ctx);

    // Setup STUN server (Google)
    sockaddr_in stunAddr = {AF_INET, htons(19302), inet_addr("142.250.82.46")};
    memcpy((char*)ctx + 16, &stunAddr, sizeof(stunAddr));
    printf("STUN server configured\n");

    // Detect NAT type
    printf("Calling NAT_DetectType...\n");
    int nat = NAT_DetectType(ctx);
    printf("NAT Type: %d\n", nat);

    // Attempt hole punch to peer
    sockaddr_in peer = {AF_INET, htons(3478), inet_addr("1.2.3.4")};
    auto start = GetTickCount64();

    int result = UDPHolePunch(ctx, &peer, 5000);

    printf("Hole punch %s in %llums\n",
           result == 0 ? "SUCCESS" : "FAILED",
           GetTickCount64() - start);

    WSACleanup();
}

int main() {
    printf("Starting test program...\n");
    Benchmark_STUN_HolePunch();
    printf("Test completed.\n");
    return 0;
}