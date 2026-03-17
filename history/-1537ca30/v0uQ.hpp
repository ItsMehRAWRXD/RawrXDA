#pragma once
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

class ServiceManager {
public:
    struct ServiceInfo {
        std::string name;
        int port;
        std::string description;
        std::atomic<bool> running{false};
        std::thread serviceThread;
    };

private:
    std::vector<std::unique_ptr<ServiceInfo>> services;
    std::atomic<bool> shutdownRequested{false};
    
public:
    ServiceManager();
    ~ServiceManager();
    
    // Service management
    void startAllServices();
    void stopAllServices();
    bool isRunning() const;
    
    // Individual service control
    void startModelServer();
    void startWebSocketBridge();
    void startCompilerService();
    
    // Health monitoring
    std::string getHealthStatus() const;
    void waitForShutdown();
    
    // Signal handling
    void requestShutdown();
};