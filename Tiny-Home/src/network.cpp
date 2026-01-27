#include "network.h"
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
namespace TinyHome::Network {
    SOCKET sock = INVALID_SOCKET;
    bool connect() {
        WSADATA w;
        WSAStartup(MAKEWORD(2,2), &w);
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in sa{AF_INET, htons(4444), {inet_addr("127.0.0.1")}};
        return connect(sock, (sockaddr*)&sa, sizeof(sa)) == 0;
    }
    void disconnect() { closesocket(sock); WSACleanup(); }
}
