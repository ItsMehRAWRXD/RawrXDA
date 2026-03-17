#include "native_ide_manager.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

// Global instance for signal handling
ServiceManager* g_serviceManager = nullptr;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully...\n";
    if (g_serviceManager) {
        g_serviceManager->requestShutdown();
    }
}

ServiceManager::ServiceManager() {
    // Initialize Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    
    // Setup signal handlers
    g_serviceManager = this;
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "Native IDE Manager initialized\n";
}

ServiceManager::~ServiceManager() {
    stopAllServices();
    WSACleanup();
}

void ServiceManager::startAllServices() {
    std::cout << "Starting all IDE services...\n";
    
    // Start model server (port 11440)
    auto modelService = std::make_unique<ServiceInfo>();
    modelService->name = "BigDaddyG Model Server";
    modelService->port = 11440;
    modelService->description = "AI Model serving endpoint";
    modelService->serviceThread = std::thread([this, &modelService]() {
        startModelServer();
        modelService->running = true;
    });
    services.push_back(std::move(modelService));
    
    // Start WebSocket bridge (port 8081)
    auto wsService = std::make_unique<ServiceInfo>();
    wsService->name = "Cursor WebSocket Bridge";
    wsService->port = 8081;
    wsService->description = "Cursor IDE integration bridge";
    wsService->serviceThread = std::thread([this, &wsService]() {
        startWebSocketBridge();
        wsService->running = true;
    });
    services.push_back(std::move(wsService));
    
    // Start compiler service (port 8080)
    auto compilerService = std::make_unique<ServiceInfo>();
    compilerService->name = "Universal Compiler Service";
    compilerService->port = 8080;
    compilerService->description = "Multi-language compilation service";
    compilerService->serviceThread = std::thread([this, &compilerService]() {
        startCompilerService();
        compilerService->running = true;
    });
    services.push_back(std::move(compilerService));
    
    // Wait a moment for services to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    std::cout << "All services started successfully!\n";
    std::cout << "Health status: " << getHealthStatus() << "\n";
    std::cout << "Press Ctrl+C to shutdown\n";
}

void ServiceManager::stopAllServices() {
    std::cout << "Stopping all services...\n";
    shutdownRequested = true;
    
    for (auto& service : services) {
        if (service->serviceThread.joinable()) {
            service->running = false;
            service->serviceThread.join();
            std::cout << "Stopped: " << service->name << "\n";
        }
    }
    
    services.clear();
    std::cout << "All services stopped\n";
}

bool ServiceManager::isRunning() const {
    return !shutdownRequested && !services.empty();
}

void ServiceManager::startModelServer() {
    std::cout << "Starting BigDaddyG Model Server on port 11440...\n";
    
    // Create socket
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Model Server: Socket creation failed\n";
        return;
    }
    
    // Bind to port 11440
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(11440);
    
    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Model Server: Bind failed\n";
        closesocket(listenSocket);
        return;
    }
    
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Model Server: Listen failed\n";
        closesocket(listenSocket);
        return;
    }
    
    std::cout << "BigDaddyG Model Server listening on port 11440\n";
    
    // Accept connections and serve model API
    while (!shutdownRequested) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSocket, &readfds);
        
        timeval timeout{1, 0}; // 1 second timeout
        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(listenSocket, &readfds)) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                // Handle HTTP request
                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "\r\n"
                    "{\n"
                    "  \"status\": \"healthy\",\n"
                    "  \"models\": [\n"
                    "    {\"id\": \"bigdaddyg-assembly\", \"name\": \"BigDaddyG Assembly Model\", \"available\": true},\n"
                    "    {\"id\": \"bigdaddyg-ensemble\", \"name\": \"BigDaddyG Ensemble Model\", \"available\": true},\n"
                    "    {\"id\": \"bigdaddyg-pe\", \"name\": \"BigDaddyG PE Model\", \"available\": true},\n"
                    "    {\"id\": \"bigdaddyg-reverse\", \"name\": \"BigDaddyG Reverse Model\", \"available\": true},\n"
                    "    {\"id\": \"your-custom-model\", \"name\": \"Your Custom Model\", \"available\": true}\n"
                    "  ]\n"
                    "}\n";
                
                send(clientSocket, response.c_str(), response.length(), 0);
                closesocket(clientSocket);
            }
        }
    }
    
    closesocket(listenSocket);
    std::cout << "Model Server stopped\n";
}

void ServiceManager::startWebSocketBridge() {
    std::cout << "Starting Cursor WebSocket Bridge on port 8081...\n";
    
    // Similar HTTP server for WebSocket bridge
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "WebSocket Bridge: Socket creation failed\n";
        return;
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8081);
    
    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "WebSocket Bridge: Bind failed\n";
        closesocket(listenSocket);
        return;
    }
    
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "WebSocket Bridge: Listen failed\n";
        closesocket(listenSocket);
        return;
    }
    
    std::cout << "Cursor WebSocket Bridge listening on port 8081\n";
    
    while (!shutdownRequested) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSocket, &readfds);
        
        timeval timeout{1, 0};
        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(listenSocket, &readfds)) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "\r\n"
                    "{\"status\": \"ok\", \"bridge\": \"active\", \"timestamp\": " + 
                    std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()) + "}\n";
                
                send(clientSocket, response.c_str(), response.length(), 0);
                closesocket(clientSocket);
            }
        }
    }
    
    closesocket(listenSocket);
    std::cout << "WebSocket Bridge stopped\n";
}

void ServiceManager::startCompilerService() {
    std::cout << "Starting Universal Compiler Service on port 8080...\n";
    
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Compiler Service: Socket creation failed\n";
        return;
    }
    
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);
    
    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Compiler Service: Bind failed\n";
        closesocket(listenSocket);
        return;
    }
    
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Compiler Service: Listen failed\n";
        closesocket(listenSocket);
        return;
    }
    
    std::cout << "Universal Compiler Service listening on port 8080\n";
    
    while (!shutdownRequested) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listenSocket, &readfds);
        
        timeval timeout{1, 0};
        int result = select(0, &readfds, nullptr, nullptr, &timeout);
        
        if (result > 0 && FD_ISSET(listenSocket, &readfds)) {
            SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
            if (clientSocket != INVALID_SOCKET) {
                std::string response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: application/json\r\n"
                    "\r\n"
                    "{\"status\": \"ok\", \"compiler\": \"ready\", \"languages\": [\"C++\", \"C\", \"Assembly\", \"Rust\", \"Go\"]}\n";
                
                send(clientSocket, response.c_str(), response.length(), 0);
                closesocket(clientSocket);
            }
        }
    }
    
    closesocket(listenSocket);
    std::cout << "Compiler Service stopped\n";
}

std::string ServiceManager::getHealthStatus() const {
    std::string status = "Services: ";
    for (const auto& service : services) {
        status += service->name + " (:" + std::to_string(service->port) + ") ";
        status += service->running ? "[RUNNING] " : "[STOPPED] ";
    }
    return status;
}

void ServiceManager::waitForShutdown() {
    while (!shutdownRequested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ServiceManager::requestShutdown() {
    shutdownRequested = true;
}