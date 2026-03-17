// telemetry/metrics_server.hpp
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <string>
#include <unordered_map>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD {

class MetricsServer {
public:
    MetricsServer();
    ~MetricsServer();
    
    bool start(int port);
    void stop();
    
    void setMetric(const std::string& name, double value);
    void incrementMetric(const std::string& name, double delta = 1.0);
    
private:
    void serverThread();
    void handleClient(SOCKET client_socket);
    std::string generatePrometheusOutput();
    
    SOCKET server_socket_;
    std::thread server_thread_;
    std::atomic<bool> running_;
    
    std::unordered_map<std::string, double> metrics_;
    std::mutex metrics_mutex_;
};

} // namespace RawrXD