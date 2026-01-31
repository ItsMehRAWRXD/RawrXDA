#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <chrono>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

std::atomic<int> success{0}, failed{0};

void TestRequest(const std::string& host, int port, int id) {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { failed++; return; }
    
    sockaddr_in addr = {}; addr.sin_family = AF_INET; 
    addr.sin_port = htons(port); inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
        std::string req = "GET /health HTTP/1.1\r\nHost: " + host + "\r\n\r\n";
        send(sock, req.c_str(), (int)req.size(), 0);
        char buf[1024]; int r = recv(sock, buf, sizeof(buf), 0);
        if (r > 0 && strstr(buf, "200")) success++; else failed++;
    } else {
        failed++;
    }
    closesocket(sock); WSACleanup();
}

int main(int argc, char* argv[]) {
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 11434;
    int threads = argc > 3 ? atoi(argv[3]) : 10;
    
    std::cout << "Stress Test: " << host << ":" << port << " (" << threads << " threads)\n";
    
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> pool;
    for (int i = 0; i < threads; i++) pool.emplace_back(TestRequest, host, port, i);
    for (auto& t : pool) t.join();
    auto end = std::chrono::high_resolution_clock::now();
    
    double sec = std::chrono::duration<double>(end - start).count();
    std::cout << "Success: " << success << " Failed: " << failed << "\n";
    std::cout << "Time: " << sec << "s (" << threads/sec << " req/s)\n";
    return failed > 0 ? 1 : 0;
}
