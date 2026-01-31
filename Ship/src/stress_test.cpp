#include <windows.h>
#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

struct StressResult {
    int total, success, failed;
    double totalTimeSec, reqPerSec, avgLatency, p99Latency;
};

class StressTester {
    std::string host;
    int port;
    std::atomic<int> success{0}, failed{0};
    std::vector<double> latencies;
    std::mutex latenciesMutex;
    
    bool HttpPost(const std::string& path, const std::string& body, double& latency) {
        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return false;
        
        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        
        auto start = std::chrono::high_resolution_clock::now();
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != 0) {
            closesocket(sock);
            return false;
        }
        
        std::string req = "POST " + path + " HTTP/1.1\r\nHost: " + host + "\r\n";
        req += "Content-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        
        send(sock, req.c_str(), static_cast<int>(req.size()), 0);
        
        char buf[4096];
        int recvLen = recv(sock, buf, sizeof(buf), 0);
        closesocket(sock);
        
        auto end = std::chrono::high_resolution_clock::now();
        latency = std::chrono::duration<double, std::milli>(end - start).count();
        
        return recvLen > 0 && strstr(buf, "200 OK") != nullptr;
    }
    
public:
    StressTester(const std::string& h, int p) : host(h), port(p) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2,2), &wsa);
    }
    
    ~StressTester() { WSACleanup(); }
    
    void SingleRequest(int id) {
        double latency;
        std::string body = R"({"model":"gpt-oss:120b","prompt":"test","stream":false})";
        if (HttpPost("/api/generate", body, latency)) {
            success++;
        } else {
            failed++;
        }
        {
            std::lock_guard<std::mutex> lock(latenciesMutex);
            latencies.push_back(latency);
        }
    }
    
    StressResult Run(int concurrency, int totalRequests) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        int perThread = totalRequests / concurrency;
        
        for (int i = 0; i < concurrency; i++) {
            threads.emplace_back([this, perThread, i]() {
                for (int j = 0; j < perThread; j++) SingleRequest(i * perThread + j);
            });
        }
        
        for (auto& t : threads) t.join();
        
        auto end = std::chrono::high_resolution_clock::now();
        double totalTime = std::chrono::duration<double>(end - start).count();
        
        std::sort(latencies.begin(), latencies.end());
        double p99 = latencies[(size_t)(latencies.size() * 0.99)];
        double avg = 0;
        for (auto l : latencies) avg += l;
        avg /= latencies.size();
        
        return {totalRequests, success.load(), failed.load(), totalTime, 
                totalRequests/totalTime, avg, p99};
    }
};

int main(int argc, char* argv[]) {
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 11434;
    int concurrency = argc > 3 ? atoi(argv[3]) : 10;
    int requests = argc > 4 ? atoi(argv[4]) : 50;
    
    std::cout << "RawrXD Stress Test: " << host << ":" << port 
              << " | Concurrency: " << concurrency 
              << " | Requests: " << requests << "\n";
    
    StressTester tester(host, port);
    auto r = tester.Run(concurrency, requests);
    
    std::cout << "\nSuccess: " << r.success << "/" << r.total 
              << "\nFailed: " << r.failed
              << "\nTime: " << r.totalTimeSec << "s"
              << "\nThroughput: " << r.reqPerSec << " req/s"
              << "\nAvg Latency: " << r.avgLatency << "ms"
              << "\nP99 Latency: " << r.p99Latency << "ms\n";
    
    return r.failed > 0 ? 1 : 0;
}
