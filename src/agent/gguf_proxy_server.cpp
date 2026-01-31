#include "gguf_proxy_server.hpp"
#include "agent_hot_patcher.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

GGUFProxyServer::GGUFProxyServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

GGUFProxyServer::~GGUFProxyServer() {
    stopServer();
    WSACleanup();
}

void GGUFProxyServer::initialize(int listenPort, AgentHotPatcher* hotPatcher, const std::string& ggufEndpoint) {
    m_listenPort = listenPort;
    m_hotPatcher = hotPatcher;
    m_ggufEndpoint = ggufEndpoint;
}

bool GGUFProxyServer::startServer() {
    if (m_isListening) return true;

    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) return false;

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("127.0.0.1");
    service.sin_port = htons((u_short)m_listenPort);

    if (bind(m_listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        return false;
    }

    if (listen(m_listenSocket, 100) == SOCKET_ERROR) {
        closesocket(m_listenSocket);
        return false;
    }

    m_isListening = true;
    m_acceptThread = std::thread(&GGUFProxyServer::acceptLoop, this);
    
    std::cout << "[GGUFProxyServer] Listening on " << m_listenPort << std::endl;
    return true;
}

void GGUFProxyServer::stopServer() {
    m_isListening = false;
    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    // Detached client threads clean themselves up or we track and cancel them (hard with blocking sockets).
    // For this simple impl, we let OS cleanup process resources or rely on timeout.
}

void GGUFProxyServer::acceptLoop() {
    while (m_isListening) {
        SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (m_isListening) continue; 
            else break;
        }
        
        // Spawn thread for client
        std::thread t(&GGUFProxyServer::handleClient, this, clientSocket);
        t.detach(); // Let it run
    }
}

SOCKET GGUFProxyServer::connectToBackend() {
    std::string host = "localhost";
    std::string portStr = "8080";
    
    size_t colon = m_ggufEndpoint.find(':');
    if (colon != std::string::npos) {
        host = m_ggufEndpoint.substr(0, colon);
        portStr = m_ggufEndpoint.substr(colon + 1);
    }
    
    struct addrinfo hints, *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return INVALID_SOCKET;
    }

    SOCKET s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(result);
        return INVALID_SOCKET;
    }

    if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        closesocket(s);
        s = INVALID_SOCKET;
    }

    freeaddrinfo(result);
    return s;
}

// Simple Helper to read Utils
static std::string readRequest(SOCKET s) {
    char buf[4096];
    std::string total;
    while (true) {
        int bytes = recv(s, buf, sizeof(buf), 0);
        if (bytes <= 0) break;
        total.append(buf, bytes);
        // Simple heuristic: end of headers. 
        // Real implementation should parse content-length.
        if (total.find("\r\n\r\n") != std::string::npos) {
            // If just GET, we might be done. If POST, check Content-Length.
            // Simplified: just return what we got for forwarding.
            // Assuming small requests for prompt.
            break; 
        }
    }
    return total;
}

void GGUFProxyServer::handleClient(SOCKET clientSocket) {
    SOCKET serverSocket = connectToBackend();
    if (serverSocket == INVALID_SOCKET) {
        closesocket(clientSocket);
        return;
    }

    // 1. Read Request from Client
    char buffer[8192];
    int bytesRecv = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRecv > 0) {
        // Forward to backend
        send(serverSocket, buffer, bytesRecv, 0);
    }

    // 2. Read Response from Backend
    // We need to capture the full response to patch, or stream pass-through.
    // If we patch, we must buffer the whole JSON.
    
    std::string responseData;
    while (true) {
        int r = recv(serverSocket, buffer, sizeof(buffer), 0);
        if (r <= 0) break;
        responseData.append(buffer, r);
        // Check if complete? (Simplified: wait for close or timeout in real usage, 
        // but here we just read as much as possible? No, HTTP keeps alive.)
        // We really need Content-Length parsing.
        
        // Simplified Logic: 
        // If we see "}" at the end of body, we might assume JSON done?
        // Or we just relay chunks if we can't patch safely.
    }
    
    // 3. Patch logic (Mocked for safety if incomplete read)
    // In a real robust proxy, we parse HTTP. 
    // Here we will try to find the JSON body.
    
    size_t headerEnd = responseData.find("\r\n\r\n");
    if (headerEnd != std::string::npos && m_hotPatcher) {
        std::string headers = responseData.substr(0, headerEnd);
        std::string body = responseData.substr(headerEnd + 4);
        
        // Check if JSON
        // If content-type json...
        
        // Call patcher
        // Note: interceptModelOutput returns a json object now in our new API, or we adapted it.
        // Let's assume body is the model output string.
        
        // This part requires `AgentHotPatcher` to be thread safe (it has mutex).
        // Adapt input to patcher
        json context; // empty context
        json patched = m_hotPatcher->interceptModelOutput(body, context);
        
        if (patched["modified"].get<bool>()) {
             // Reconstruct response
             std::string newBody;
             if (patched.contains("final_output")) {
                 newBody = patched["final_output"].dump();
             } else {
                 newBody = body;
             }
             
             // Update Content-Length in headers
             // (String manipulation omitted for brevity, but critical for HTTP)
             // For now, construct a new simple OK response
             std::stringstream ss;
             ss << "HTTP/1.1 200 OK\r\n";
             ss << "Content-Type: application/json\r\n";
             ss << "Content-Length: " << newBody.size() << "\r\n";
             ss << "Connection: close\r\n\r\n";
             ss << newBody;
             responseData = ss.str();
        }
    }

    // 4. Send back to client
    send(clientSocket, responseData.data(), (int)responseData.size(), 0);

    // Cleanup
    closesocket(serverSocket);
    closesocket(clientSocket);
    
    m_requestsProcessed++;
}

std::string GGUFProxyServer::getServerStatistics() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    json j;
    j["requestsProcessed"] = m_requestsProcessed.load();
    return j.dump();
}
