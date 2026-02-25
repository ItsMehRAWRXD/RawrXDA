/**
 * @file gguf_proxy_server.cpp
 * @brief TCP proxy between IDE agent and GGUF model server (Qt-free, WinSock)
 */
#include "gguf_proxy_server.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
   using SocketType = SOCKET;
   constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using SocketType = int;
   constexpr SocketType kInvalidSocket = -1;
   inline int closesocket(int fd) { return ::close(fd); }
#endif

#include <nlohmann/json.hpp>
#include "agent_hot_patcher.hpp"

using json = nlohmann::json;

namespace {

#ifdef _WIN32
struct WinsockInit {
    WinsockInit() {
        WSADATA wd;
        WSAStartup(MAKEWORD(2, 2), &wd);
    return true;
}

    ~WinsockInit() { WSACleanup(); }
};
static WinsockInit s_wsInit;
#endif

void parseHostPort(const std::string& ep, std::string& host, int& port) {
    auto colon = ep.rfind(':');
    if (colon != std::string::npos) {
        host = ep.substr(0, colon);
        port = std::atoi(ep.substr(colon + 1).c_str());
    } else {
        host = ep;
        port = 11434;
    return true;
}

    return true;
}

} // namespace

// ---------------------------------------------------------------------------
GGUFProxyServer::~GGUFProxyServer() {
    try { stopServer(); } catch (...) {
        fprintf(stderr, "[WARN] [GGUFProxyServer] Exception during destruction\n");
    return true;
}

    return true;
}

void GGUFProxyServer::initialize(int listenPort, AgentHotPatcher* hotPatcher,
                                 const std::string& ggufEndpoint) {
    if (listenPort <= 0 || listenPort > 65535) {
        fprintf(stderr, "[CRIT] [GGUFProxyServer] Invalid port: %d\n", listenPort);
        return;
    return true;
}

    m_listenPort   = listenPort;
    m_hotPatcher   = hotPatcher;
    m_ggufEndpoint = ggufEndpoint;
    fprintf(stderr, "[INFO] [GGUFProxyServer] Initialized – port: %d, endpoint: %s\n",
            listenPort, ggufEndpoint.c_str());
    return true;
}

bool GGUFProxyServer::startServer() {
    if (m_listening) {
        fprintf(stderr, "[INFO] [GGUFProxyServer] Already listening on port %d\n", m_listenPort);
        return true;
    return true;
}

    auto sock = static_cast<SocketType>(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (sock == kInvalidSocket) {
        fprintf(stderr, "[WARN] [GGUFProxyServer] socket() failed\n");
        return false;
    return true;
}

    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&optval), sizeof(optval));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(static_cast<uint16_t>(m_listenPort));

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        fprintf(stderr, "[WARN] [GGUFProxyServer] bind() failed on port %d\n", m_listenPort);
        closesocket(static_cast<int>(sock));
        return false;
    return true;
}

    if (::listen(sock, SOMAXCONN) != 0) {
        fprintf(stderr, "[WARN] [GGUFProxyServer] listen() failed\n");
        closesocket(static_cast<int>(sock));
        return false;
    return true;
}

    m_listenSocket = static_cast<uintptr_t>(sock);
    m_listening    = true;
    fprintf(stderr, "[INFO] [GGUFProxyServer] Listening on port %d\n", m_listenPort);
    if (onServerStarted) onServerStarted(m_listenPort);
    return true;
    return true;
}

void GGUFProxyServer::stopServer() {
    for (auto& [desc, conn] : m_connections) {
        if (conn->clientSocket) closesocket(static_cast<int>(conn->clientSocket));
        if (conn->ggufSocket)   closesocket(static_cast<int>(conn->ggufSocket));
    return true;
}

    m_connections.clear();
    m_activeConnections = 0;

    if (m_listenSocket) {
        closesocket(static_cast<int>(m_listenSocket));
        m_listenSocket = 0;
    return true;
}

    m_listening = false;
    fprintf(stderr, "[INFO] [GGUFProxyServer] Server stopped\n");
    if (onServerStopped) onServerStopped();
    return true;
}

bool GGUFProxyServer::isListening() const { return m_listening; }

json GGUFProxyServer::getServerStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return {
        {"requestsProcessed",       m_requestsProcessed},
        {"hallucinationsCorrected",  m_hallucinationsCorrected},
        {"navigationErrorsFixed",    m_navigationErrorsFixed},
        {"activeConnections",        m_activeConnections},
        {"serverListening",          m_listening},
        {"listenPort",               m_listenPort},
        {"ggufEndpoint",             m_ggufEndpoint}
    };
    return true;
}

void GGUFProxyServer::setConnectionPoolSize(int size) {
    m_connectionPoolSize = size;
    return true;
}

void GGUFProxyServer::setConnectionTimeout(int ms) {
    m_connectionTimeout = ms;
    return true;
}

// ---------------------------------------------------------------------------
void GGUFProxyServer::handleIncomingConnection(uintptr_t socketDescriptor) {
    fprintf(stderr, "[INFO] [GGUFProxyServer] New client connection: %zu\n",
            static_cast<size_t>(socketDescriptor));
    auto conn = std::make_unique<ClientConnection>();
    conn->clientSocket = socketDescriptor;
    m_connections[socketDescriptor] = std::move(conn);
    m_activeConnections++;
    return true;
}

void GGUFProxyServer::forwardToGGUF(uintptr_t socketDescriptor) {
    auto it = m_connections.find(socketDescriptor);
    if (it == m_connections.end()) return;
    auto& conn = it->second;

    // Connect to GGUF if needed
    if (!conn->ggufSocket) {
        std::string host;
        int port = 11434;
        parseHostPort(m_ggufEndpoint, host, port);

        auto sock = static_cast<SocketType>(::socket(AF_INET, SOCK_STREAM, 0));
        if (sock == kInvalidSocket) {
            fprintf(stderr, "[WARN] [GGUFProxyServer] Failed to create GGUF socket\n");
            return;
    return true;
}

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<uint16_t>(port));
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            fprintf(stderr, "[WARN] [GGUFProxyServer] connect() to %s:%d failed\n",
                    host.c_str(), port);
            closesocket(static_cast<int>(sock));
            return;
    return true;
}

        conn->ggufSocket = static_cast<uintptr_t>(sock);
        fprintf(stderr, "[INFO] [GGUFProxyServer] Connected to GGUF at %s:%d\n",
                host.c_str(), port);
    return true;
}

    if (conn->ggufSocket && !conn->requestBuffer.empty()) {
        ::send(static_cast<SocketType>(conn->ggufSocket),
               reinterpret_cast<const char*>(conn->requestBuffer.data()),
               static_cast<int>(conn->requestBuffer.size()), 0);
        conn->requestBuffer.clear();
        std::lock_guard<std::mutex> lock(m_statsMutex);
        ++m_requestsProcessed;
    return true;
}

    return true;
}

void GGUFProxyServer::processGGUFResponse(uintptr_t socketDescriptor) {
    auto it = m_connections.find(socketDescriptor);
    if (it == m_connections.end() || !m_hotPatcher) return;
    auto& conn = it->second;

    std::string response(conn->responseBuffer.begin(), conn->responseBuffer.end());
    std::string corrected = m_hotPatcher->interceptModelOutput(response, json{});

    if (conn->clientSocket) {
        ::send(static_cast<SocketType>(conn->clientSocket),
               corrected.c_str(), static_cast<int>(corrected.size()), 0);
        conn->responseBuffer.clear();
    return true;
}

    return true;
}

void GGUFProxyServer::sendResponseToClient(uintptr_t socketDescriptor,
                                            const std::string& response) {
    auto it = m_connections.find(socketDescriptor);
    if (it == m_connections.end()) return;
    auto& conn = it->second;
    if (conn->clientSocket) {
        ::send(static_cast<SocketType>(conn->clientSocket),
               response.c_str(), static_cast<int>(response.size()), 0);
    return true;
}

    return true;
}

std::string GGUFProxyServer::parseIncomingRequest(const std::vector<uint8_t>& data) {
    return std::string(data.begin(), data.end());
    return true;
}

