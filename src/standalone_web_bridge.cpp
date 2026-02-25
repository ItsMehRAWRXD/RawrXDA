#include "standalone_web_bridge.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <regex>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSESOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#define CLOSESOCKET close
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#endif

using json = nlohmann::json;

// ============================================================================
// StandaloneWebAPI Implementation
// ============================================================================

StandaloneWebAPI::StandaloneWebAPI(GGUFProxyServer* server) : m_server(server) {}

json StandaloneWebAPI::sendToModel(const std::string& prompt, const json& options) {
    if (!m_server || !m_server->isListening()) {
        return {
            {"success", false},
            {"error", "Server not available"}
        };
    }

    try {
        // Convert options to JSON string for server
        std::string requestData = options.dump();

        // Use server's internal processing
        std::string response = m_server->parseIncomingRequest(
            std::vector<uint8_t>(requestData.begin(), requestData.end())
        );

        return {
            {"success", true},
            {"response", response},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
        };
    } catch (const std::exception& e) {
        return {
            {"success", false},
            {"error", std::string("Processing error: ") + e.what()}
        };
    }
}

json StandaloneWebAPI::getServerStatus() {
    return {
        {"listening", m_server && m_server->isListening()},
        {"port", m_server ? m_server->getPort() : 0},
        {"activeConnections", m_server ? m_server->getActiveConnections() : 0},
        {"ready", isServerReady()}
    };
}

std::string StandaloneWebAPI::getStatistics() {
    if (!m_server) return "{}";
    return m_server->getServerStatistics().dump(2);
}

bool StandaloneWebAPI::isServerReady() {
    return m_server && m_server->isListening();
}

void StandaloneWebAPI::handleWebSocketMessage(const std::string& message,
                                             std::function<void(const std::string&)> sendResponse) {
    try {
        json request = json::parse(message);
        std::string method = request["method"];
        json params = request["params"];
        int id = request["id"];

        json response = {{"jsonrpc", "2.0"}, {"id", id}};

        if (method == "sendToModel") {
            std::string prompt = params["prompt"];
            json options = params.value("options", json::object());
            response["result"] = sendToModel(prompt, options);
        } else if (method == "getServerStatus") {
            response["result"] = getServerStatus();
        } else if (method == "getStatistics") {
            response["result"] = getStatistics();
        } else if (method == "isServerReady") {
            response["result"] = isServerReady();
        } else {
            response["error"] = {{"code", -32601}, {"message", "Method not found"}};
        }

        sendResponse(response.dump());
    } catch (const std::exception& e) {
        json error = {
            {"jsonrpc", "2.0"},
            {"error", {{"code", -32700}, {"message", "Parse error"}}},
            {"id", nullptr}
        };
        sendResponse(error.dump());
    }
}

void StandaloneWebAPI::handleHttpRequest(const std::string& method, const std::string& path,
                                        const std::string& body,
                                        std::function<void(const std::string&)> sendResponse) {
    if (method == "POST" && path == "/api/sendToModel") {
        try {
            json request = json::parse(body);
            std::string prompt = request["prompt"];
            json options = request.value("options", json::object());
            json result = sendToModel(prompt, options);
            sendResponse(result.dump());
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"error", "Invalid request"}};
            sendResponse(error.dump());
        }
    } else if (method == "GET" && path == "/api/status") {
        sendResponse(getServerStatus().dump());
    } else if (method == "GET" && path == "/api/stats") {
        sendResponse(getStatistics());
    } else {
        json error = {{"error", "Endpoint not found"}};
        sendResponse(error.dump());
    }
}

// ============================================================================
// SimpleHttpServer Implementation
// ============================================================================

SimpleHttpServer::SimpleHttpServer(int port) : m_port(port) {}

SimpleHttpServer::~SimpleHttpServer() {
    stop();
}

bool SimpleHttpServer::start() {
    if (m_running) return true;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[HttpServer] WSAStartup failed" << std::endl;
        return false;
    }
#endif

    m_running = true;
    m_serverThread = std::thread(&SimpleHttpServer::run, this);
    return true;
}

void SimpleHttpServer::stop() {
    m_running = false;
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void SimpleHttpServer::addRoute(const std::string& path,
                               std::function<std::string(const std::string&)> handler) {
    m_routes[path] = handler;
}

void SimpleHttpServer::serveStaticFiles(const std::string& rootPath) {
    m_staticRoot = rootPath;
}

void SimpleHttpServer::run() {
    socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[HttpServer] Socket creation failed" << std::endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] Bind failed on port " << m_port << std::endl;
        CLOSESOCKET(serverSocket);
        return;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] Listen failed" << std::endl;
        CLOSESOCKET(serverSocket);
        return;
    }

    std::cout << "[HttpServer] Listening on port " << m_port << std::endl;

    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        socket_t clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);

        if (clientSocket == INVALID_SOCKET) {
            if (m_running) std::cerr << "[HttpServer] Accept failed" << std::endl;
            continue;
        }

        // Handle request in a simple way (no threading for simplicity)
        char buffer[4096];
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string response = handleRequest(buffer);
            send(clientSocket, response.c_str(), response.length(), 0);
        }

        CLOSESOCKET(clientSocket);
    }

    CLOSESOCKET(serverSocket);
}

std::string SimpleHttpServer::handleRequest(const std::string& request) {
    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    // Parse headers and body
    std::string line;
    std::getline(iss, line); // Skip rest of first line
    size_t contentLength = 0;
    std::string body;

    while (std::getline(iss, line) && line != "\r") {
        if (line.find("Content-Length:") == 0) {
            contentLength = std::stoul(line.substr(16));
        }
    }

    if (contentLength > 0) {
        body.resize(contentLength);
        iss.read(&body[0], contentLength);
    }

    // Check routes first
    auto routeIt = m_routes.find(path);
    if (routeIt != m_routes.end()) {
        std::string responseBody = routeIt->second(body);
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: application/json\r\n"
               "Content-Length: " + std::to_string(responseBody.length()) + "\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "\r\n" + responseBody;
    }

    // Handle OPTIONS for CORS
    if (method == "OPTIONS") {
        return "HTTP/1.1 200 OK\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "\r\n";
    }

    // Serve static files
    if (!m_staticRoot.empty()) {
        std::string filePath = m_staticRoot + path;
        if (path == "/") filePath = m_staticRoot + "/index.html";

        std::string fileContent = readFile(filePath);
        if (!fileContent.empty()) {
            std::string contentType = "text/html";
            if (filePath.find(".css") != std::string::npos) contentType = "text/css";
            else if (filePath.find(".js") != std::string::npos) contentType = "application/javascript";

            return "HTTP/1.1 200 OK\r\n"
                   "Content-Type: " + contentType + "\r\n"
                   "Content-Length: " + std::to_string(fileContent.length()) + "\r\n"
                   "Access-Control-Allow-Origin: *\r\n"
                   "\r\n" + fileContent;
        }
    }

    // 404
    std::string notFound = "{\"error\": \"Not found\"}";
    return "HTTP/1.1 404 Not Found\r\n"
           "Content-Type: application/json\r\n"
           "Content-Length: " + std::to_string(notFound.length()) + "\r\n"
           "\r\n" + notFound;
}

std::string SimpleHttpServer::readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    return content;
}

// ============================================================================
// SimpleWebSocketServer Implementation
// ============================================================================

SimpleWebSocketServer::SimpleWebSocketServer(int port) : m_port(port) {}

SimpleWebSocketServer::~SimpleWebSocketServer() {
    stop();
}

bool SimpleWebSocketServer::start() {
    if (m_running) return true;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[WebSocketServer] WSAStartup failed" << std::endl;
        return false;
    }
#endif

    m_running = true;
    m_serverThread = std::thread(&SimpleWebSocketServer::run, this);
    return true;
}

void SimpleWebSocketServer::stop() {
    m_running = false;
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void SimpleWebSocketServer::setMessageHandler(std::function<void(int, const std::string&)> handler) {
    m_messageHandler = handler;
}

void SimpleWebSocketServer::run() {
    socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "[WebSocketServer] Socket creation failed" << std::endl;
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(m_port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[WebSocketServer] Bind failed on port " << m_port << std::endl;
        CLOSESOCKET(serverSocket);
        return;
    }

    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        std::cerr << "[WebSocketServer] Listen failed" << std::endl;
        CLOSESOCKET(serverSocket);
        return;
    }

    std::cout << "[WebSocketServer] Listening on port " << m_port << std::endl;

    int clientId = 0;
    while (m_running) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        socket_t clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);

        if (clientSocket == INVALID_SOCKET) {
            if (m_running) std::cerr << "[WebSocketServer] Accept failed" << std::endl;
            continue;
        }

        std::thread(&SimpleWebSocketServer::handleClient, this, clientId++, clientSocket).detach();
    }

    CLOSESOCKET(serverSocket);
}

void SimpleWebSocketServer::handleClient(int clientId, socket_t clientSocket) {
    char buffer[4096];
    bool handshakeDone = false;

    while (m_running) {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) break;

        buffer[bytesRead] = '\0';
        std::string request(buffer, bytesRead);

        if (!handshakeDone) {
            // Perform WebSocket handshake
            std::string response = performHandshake(request);
            if (!response.empty()) {
                send(clientSocket, response.c_str(), response.length(), 0);
                handshakeDone = true;
                std::cout << "[WebSocketServer] Client " << clientId << " connected" << std::endl;
            }
        } else {
            // Handle WebSocket frames
            std::string message = decodeFrame(request);
            if (!message.empty() && m_messageHandler) {
                m_messageHandler(clientId, message);
            }
        }
    }

    CLOSESOCKET(clientSocket);
    std::cout << "[WebSocketServer] Client " << clientId << " disconnected" << std::endl;
}

std::string SimpleWebSocketServer::performHandshake(const std::string& request) {
    // Simple WebSocket handshake implementation
    std::regex keyRegex("Sec-WebSocket-Key: ([^\r\n]+)");
    std::smatch match;

    if (std::regex_search(request, match, keyRegex)) {
        std::string key = match[1];
        // Compute accept key (simplified - in real implementation use proper SHA-1)
        std::string acceptKey = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        // For demo purposes, using a simple hash
        size_t hash = std::hash<std::string>{}(acceptKey);
        char hashStr[17];
        sprintf(hashStr, "%016zx", hash);

        return "HTTP/1.1 101 Switching Protocols\r\n"
               "Upgrade: websocket\r\n"
               "Connection: Upgrade\r\n"
               "Sec-WebSocket-Accept: " + std::string(hashStr) + "\r\n"
               "\r\n";
    }

    return "";
}

std::string SimpleWebSocketServer::decodeFrame(const std::string& frame) {
    if (frame.length() < 2) return "";

    unsigned char opcode = frame[0] & 0x0F;
    if (opcode != 1) return ""; // Text frame only

    unsigned char payloadLen = frame[1] & 0x7F;
    size_t offset = 2;

    if (payloadLen == 126) {
        if (frame.length() < 4) return "";
        payloadLen = (frame[2] << 8) | frame[3];
        offset = 4;
    } else if (payloadLen == 127) {
        // Skip 127 handling for simplicity
        return "";
    }

    if (frame.length() < offset + payloadLen) return "";

    return frame.substr(offset, payloadLen);
}

std::string SimpleWebSocketServer::encodeFrame(const std::string& message) {
    std::string frame;
    frame.push_back(0x81); // Text frame

    if (message.length() <= 125) {
        frame.push_back(message.length());
    } else {
        frame.push_back(126);
        frame.push_back((message.length() >> 8) & 0xFF);
        frame.push_back(message.length() & 0xFF);
    }

    frame += message;
    return frame;
}

// ============================================================================
// StandaloneWebBridgeServer Implementation
// ============================================================================

StandaloneWebBridgeServer::StandaloneWebBridgeServer(int httpPort, int wsPort, AgentHotPatcher* hotPatcher)
    : m_httpPort(httpPort), m_wsPort(wsPort), m_hotPatcher(hotPatcher) {}

StandaloneWebBridgeServer::~StandaloneWebBridgeServer() {
    stop();
}

bool StandaloneWebBridgeServer::initialize() {
    try {
        // Create the TCP proxy server
        m_proxyServer = std::make_unique<GGUFProxyServer>();
        m_proxyServer->initialize(11434, m_hotPatcher, "http://localhost:11434");

        // Create web API
        m_webAPI = std::make_unique<StandaloneWebAPI>(m_proxyServer.get());

        std::cout << "[StandaloneWebBridge] Initialized successfully" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[StandaloneWebBridge] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

bool StandaloneWebBridgeServer::start() {
    if (!m_proxyServer && !initialize()) {
        return false;
    }

    // Start the TCP proxy server
    if (!m_proxyServer->startServer()) {
        std::cerr << "[StandaloneWebBridge] Failed to start TCP proxy server" << std::endl;
        return false;
    }

    // Start HTTP and WebSocket servers
    m_running = true;
    m_httpThread = std::thread(&StandaloneWebBridgeServer::runHttpServer, this);
    m_wsThread = std::thread(&StandaloneWebBridgeServer::runWebSocketServer, this);

    std::cout << "[StandaloneWebBridge] Server started - HTTP: " << m_httpPort
              << ", WebSocket: " << m_wsPort << std::endl;
    return true;
}

void StandaloneWebBridgeServer::stop() {
    m_running = false;

    if (m_proxyServer) {
        m_proxyServer->stopServer();
    }

    if (m_httpThread.joinable()) {
        m_httpThread.join();
    }

    if (m_wsThread.joinable()) {
        m_wsThread.join();
    }
}

std::string StandaloneWebBridgeServer::getServerUrl() const {
    return "http://localhost:" + std::to_string(m_httpPort);
}

void StandaloneWebBridgeServer::serveStaticFiles(const std::string& webRootPath) {
    m_webRootPath = webRootPath;
}

void StandaloneWebBridgeServer::addRoute(const std::string& path,
                                       std::function<std::string(const std::string&)> handler) {
    m_routes[path] = handler;
}

void StandaloneWebBridgeServer::runHttpServer() {
    SimpleHttpServer httpServer(m_httpPort);
    httpServer.serveStaticFiles(m_webRootPath);

    // Add API routes
    httpServer.addRoute("/api/sendToModel", [this](const std::string& body) {
        std::string response;
        m_webAPI->handleHttpRequest("POST", "/api/sendToModel", body,
                                   [&response](const std::string& res) { response = res; });
        return response;
    });

    httpServer.addRoute("/api/status", [this](const std::string&) {
        std::string response;
        m_webAPI->handleHttpRequest("GET", "/api/status", "",
                                   [&response](const std::string& res) { response = res; });
        return response;
    });

    httpServer.addRoute("/api/stats", [this](const std::string&) {
        std::string response;
        m_webAPI->handleHttpRequest("GET", "/api/stats", "",
                                   [&response](const std::string& res) { response = res; });
        return response;
    });

    httpServer.start();

    // Keep running until stopped
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    httpServer.stop();
}

void StandaloneWebBridgeServer::runWebSocketServer() {
    SimpleWebSocketServer wsServer(m_wsPort);
    wsServer.setMessageHandler([this](int clientId, const std::string& message) {
        handleWebSocketConnection(clientId, message);
    });

    wsServer.start();

    // Keep running until stopped
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    wsServer.stop();
}

void StandaloneWebBridgeServer::handleWebSocketConnection(int clientId, const std::string& message) {
    // Store client response function
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    m_wsClients[clientId] = [this, clientId](const std::string& response) {
        // In a real implementation, send response back to client
        std::cout << "[WebSocket] Response to client " << clientId << ": " << response << std::endl;
    };

    // Handle the message
    m_webAPI->handleWebSocketMessage(message, m_wsClients[clientId]);
}
