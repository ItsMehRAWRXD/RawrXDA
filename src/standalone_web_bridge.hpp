/**
 * @file standalone_web_bridge.hpp
 * @brief Qt-free web bridge server using HTTP/WebSocket
 * Reverse-engineered from Qt IPC to standalone implementation
 */
#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "gguf_proxy_server.hpp"

class AgentHotPatcher;

/**
 * @brief Standalone web bridge API (Qt-free version)
 * Provides the same interface as WebBridgeAPI but without Qt dependencies
 */
class StandaloneWebAPI {
public:
    StandaloneWebAPI(GGUFProxyServer* server);
    ~StandaloneWebAPI() = default;

    // API methods (match Qt version)
    nlohmann::json sendToModel(const std::string& prompt, const nlohmann::json& options = {});
    nlohmann::json getServerStatus();
    std::string getStatistics();
    bool isServerReady();

    // WebSocket event handlers
    void handleWebSocketMessage(const std::string& message, std::function<void(const std::string&)> sendResponse);
    void handleHttpRequest(const std::string& method, const std::string& path,
                          const std::string& body, std::function<void(const std::string&)> sendResponse);

private:
    GGUFProxyServer* m_server;
};

/**
 * @brief Standalone HTTP/WebSocket server (Qt-free)
 * Serves HTML interface and provides IPC via HTTP/WebSocket instead of Qt WebChannel
 */
class StandaloneWebBridgeServer {
public:
    StandaloneWebBridgeServer(int httpPort, int wsPort, AgentHotPatcher* hotPatcher);
    ~StandaloneWebBridgeServer();

    bool initialize();
    bool start();
    void stop();

    bool isRunning() const { return m_running; }
    std::string getServerUrl() const;

    // File serving
    void serveStaticFiles(const std::string& webRootPath);
    void addRoute(const std::string& path, std::function<std::string(const std::string&)> handler);

private:
    void runHttpServer();
    void runWebSocketServer();
    void handleWebSocketConnection(int clientId, const std::string& message);
    std::string handleHttpRequest(const std::string& method, const std::string& path, const std::string& body);

    // Server components
    int m_httpPort;
    int m_wsPort;
    AgentHotPatcher* m_hotPatcher;
    std::unique_ptr<GGUFProxyServer> m_proxyServer;
    std::unique_ptr<StandaloneWebAPI> m_webAPI;

    // Server state
    std::atomic<bool> m_running{false};
    std::thread m_httpThread;
    std::thread m_wsThread;

    // HTTP routing
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> m_routes;
    std::string m_webRootPath;

    // WebSocket clients
    std::unordered_map<int, std::function<void(const std::string&)>> m_wsClients;
    std::mutex m_clientsMutex;
};

/**
 * @brief Simple HTTP server implementation (no external deps)
 */
class SimpleHttpServer {
public:
    SimpleHttpServer(int port);
    ~SimpleHttpServer();

    bool start();
    void stop();
    void addRoute(const std::string& path, std::function<std::string(const std::string&)> handler);
    void serveStaticFiles(const std::string& rootPath);

private:
    void run();
    std::string handleRequest(const std::string& request);
    std::string readFile(const std::string& path);

    int m_port;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> m_routes;
    std::string m_staticRoot;
};

/**
 * @brief Simple WebSocket server implementation (no external deps)
 */
class SimpleWebSocketServer {
public:
    SimpleWebSocketServer(int port);
    ~SimpleWebSocketServer();

    bool start();
    void stop();
    void setMessageHandler(std::function<void(int, const std::string&)> handler);

private:
    void run();
    void handleClient(int clientSocket);
    std::string performHandshake(const std::string& request);
    std::string decodeFrame(const std::string& frame);
    std::string encodeFrame(const std::string& message);

    int m_port;
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    std::function<void(int, const std::string&)> m_messageHandler;
};