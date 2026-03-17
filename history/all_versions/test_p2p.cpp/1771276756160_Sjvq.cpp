#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include "P2PRelay.h"

#pragma comment(lib, "ws2_32.lib")

void Benchmark_STUN_HolePunch() {
    // Initialize Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    P2P_InitContextPool(16);
    void* ctx = P2P_AcquireContext();

    // Setup STUN server (Google)
    sockaddr_in stunAddr = {AF_INET, htons(19302), inet_addr("142.250.82.46")};
    memcpy((char*)ctx + 16, &stunAddr, sizeof(stunAddr));

    // Detect NAT type
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
    Benchmark_STUN_HolePunch();
    return 0;
}