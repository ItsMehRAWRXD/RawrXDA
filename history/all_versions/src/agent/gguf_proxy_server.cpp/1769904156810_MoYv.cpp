#include "gguf_proxy_server.hpp"
#include "agent_hot_patcher.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include <regex>

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

// Helper to read full HTTP message
static bool readFullMessage(SOCKET s, std::string& headers, std::string& body) {
    char buf[4096];
    std::string raw;
    int headerEnd = -1;
    
    // Read Headers
    while(true) {
        int r = recv(s, buf, sizeof(buf), 0);
        if (r <= 0) return false;
        raw.append(buf, r);
        
        size_t pos = raw.find("\r\n\r\n");
        if (pos != std::string::npos) {
            headerEnd = (int)pos;
            break;
        }
        if (raw.size() > 16384) return false; 
    }
    
    headers = raw.substr(0, headerEnd + 4);
    body = raw.substr(headerEnd + 4); 
    
    // Parse Content-Length
    int contentLength = 0;
    std::string lowerHeaders = headers;
    std::transform(lowerHeaders.begin(), lowerHeaders.end(), lowerHeaders.begin(), ::tolower);
    
    size_t clPos = lowerHeaders.find("content-length: ");
    if (clPos != std::string::npos) {
        size_t endLine = lowerHeaders.find("\r\n", clPos);
        std::string val = headers.substr(clPos + 16, endLine - (clPos + 16));
        try { contentLength = std::stoi(val); } catch (...) { contentLength = 0; }
    }
    
    // Read remaining body
    while (body.size() < (size_t)contentLength) {
        int r = recv(s, buf, sizeof(buf), 0);
        if (r <= 0) break; 
        body.append(buf, r);
    }
    
    return true;
}

void GGUFProxyServer::handleClient(SOCKET clientSocket) {
    auto closeSockets = [&](SOCKET s1, SOCKET s2) {
        if (s1 != INVALID_SOCKET) closesocket(s1);
        if (s2 != INVALID_SOCKET) closesocket(s2);
    };

    std::string reqHeaders, reqBody;
    if (!readFullMessage(clientSocket, reqHeaders, reqBody)) {
        closeSockets(clientSocket, INVALID_SOCKET);
        return;
    }

    SOCKET serverSocket = connectToBackend();
    if (serverSocket == INVALID_SOCKET) {
        std::string err = "HTTP/1.1 502 Bad Gateway\r\nConnection: close\r\n\r\n";
        send(clientSocket, err.c_str(), (int)err.size(), 0);
        closeSockets(clientSocket, INVALID_SOCKET);
        return;
    }

    // Forward Request
    std::string fullReq = reqHeaders + reqBody;
    if (send(serverSocket, fullReq.data(), (int)fullReq.size(), 0) == SOCKET_ERROR) {
        closeSockets(clientSocket, serverSocket);
        return;
    }

    // Read Response
    std::string respHeaders, respBody;
    if (!readFullMessage(serverSocket, respHeaders, respBody)) {
        closeSockets(clientSocket, serverSocket);
        return;
    }

    // Patch Logic
    std::string finalBody = respBody;
    bool modified = false;

    if (m_hotPatcher) {
        if (!respBody.empty() && (respBody[0] == '{' || respBody[0] == '[')) {
             try {
                json context;
                json patched = m_hotPatcher->interceptModelOutput(respBody, context);
                
                if (patched.contains("modified") && patched["modified"].get<bool>()) {
                    if (patched.contains("final_output")) {
                        finalBody = patched["final_output"].dump();
                        modified = true;
                    }
                }
             } catch (...) {}
        }
    }

    std::string responseData;
    if (modified) {
        std::string newHeaders = respHeaders;
        try {
            std::regex clRegex("Content-Length: [0-9]+\r\n", std::regex_constants::icase);
            newHeaders = std::regex_replace(newHeaders, clRegex, "Content-Length: " + std::to_string(finalBody.size()) + "\r\n");
        } catch(...) {}
        responseData = newHeaders + finalBody;
    } else {
        responseData = respHeaders + respBody;
    }

    send(clientSocket, responseData.data(), (int)responseData.size(), 0);
    
    closeSockets(clientSocket, serverSocket);
    m_requestsProcessed++;
}

std::string GGUFProxyServer::getServerStatistics() const {
    std::lock_guard<std::mutex> locker(m_mutex);
    json j;
    j["requestsProcessed"] = m_requestsProcessed.load();
    return j.dump();
}
