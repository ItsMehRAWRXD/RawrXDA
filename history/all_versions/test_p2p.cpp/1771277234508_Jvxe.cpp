#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include "include/P2PRelay.h"

#pragma comment(lib, "ws2_32.lib")

void TestNATDetection() {
    printf("Starting NAT detection test...\n");

    // Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        printf("WSAStartup failed: %d\n", wsaResult);
        return;
    }
    printf("Winsock initialized\n");

    // STUN servers
    sockaddr_in stun1 = {AF_INET, htons(3478), inet_addr("74.125.250.129")}; // Google STUN
    sockaddr_in stun2 = {AF_INET, htons(19302), inet_addr("142.250.27.127")};

    printf("Calling NAT_DetectType...\n");
    int natType = NAT_DetectType(&stun1, &stun2);
    printf("NAT Type detected: %d\n", natType);

    WSACleanup();
}

void TestUDPHolePunch() {
    printf("Starting UDP hole punch test...\n");

    // Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        printf("WSAStartup failed: %d\n", wsaResult);
        return;
    }

    // Peer addresses (example)
    sockaddr_in peerPublic = {AF_INET, htons(3478), inet_addr("1.2.3.4")};
    sockaddr_in peerLocal = {AF_INET, htons(3478), inet_addr("192.168.1.100")};

    printf("Calling UDPHolePunch...\n");
    SOCKET sock = UDPHolePunch(3479, &peerPublic, &peerLocal);

    if (sock != INVALID_SOCKET) {
        printf("Hole punch socket created: %llu\n", (unsigned long long)sock);
        closesocket(sock);
    } else {
        printf("Hole punch failed\n");
    }

    WSACleanup();
}

void TestSTUN() {
    printf("Starting STUN test...\n");

    // Initialize Winsock
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        printf("WSAStartup failed: %d\n", wsaResult);
        return;
    }

    char buffer[512];
    sockaddr_in publicAddr = {0};

    printf("Creating STUN binding request...\n");
    int len = STUN_CreateBindingRequest(buffer, sizeof(buffer), nullptr);
    printf("STUN request created, length: %d\n", len);

    // In a real test, we would send this to a STUN server and parse the response
    // For now, just test the parsing function with dummy data

    WSACleanup();
}

int main() {
    printf("Starting P2P test program...\n");

    P2P_InitContextPool(16);
    printf("P2P context pool initialized\n");

    TestSTUN();
    TestNATDetection();
    TestUDPHolePunch();

    printf("All tests completed.\n");
    return 0;
}
    return 0;
}