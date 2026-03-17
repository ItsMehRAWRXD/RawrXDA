// WebSocketHub — Win32 Winsock implementation. No Qt.

#include "collab/websocket_hub.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sstream>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

namespace {

static bool s_ws2Started = false;
static void ensureWinsock() {
    if (s_ws2Started) return;
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0)
        s_ws2Started = true;
}

// Simple base64 for Sec-WebSocket-Accept (SHA-1 of key + GUID, then base64)
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")

static std::string base64Encode(const unsigned char* data, size_t len) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = (unsigned int)data[i] << 16;
        if (i + 1 < len) n |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out += tbl[(n >> 18) & 63];
        out += tbl[(n >> 12) & 63];
        out += (i + 1 < len) ? tbl[(n >> 6) & 63] : '=';
        out += (i + 2 < len) ? tbl[n & 63] : '=';
    }
    return out;
}

std::string sha1Base64(const std::string& in) {
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return {};
    if (!CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return {};
    }
    if (!CryptHashData(hash, (const BYTE*)in.data(), (DWORD)in.size(), 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return {};
    }
    DWORD hashLen = 20;
    BYTE buf[20];
    if (!CryptGetHashParam(hash, HP_HASHVAL, buf, &hashLen, 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        return {};
    }
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    return base64Encode(buf, hashLen);
}

} // namespace

WebSocketHub::~WebSocketHub() {
    stopServer();
}

std::string WebSocketHub::makeWebSocketAcceptKey(const std::string& key) {
    const char guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string concat = key + guid;
    return sha1Base64(concat);
}

void WebSocketHub::serverThreadFn(WebSocketHub* self) {
    if (self) self->serverLoop();
}

void WebSocketHub::serverLoop() {
    while (m_running.load()) {
        acceptOne();
        Sleep(10);
    }
}

void WebSocketHub::acceptOne() {
    SOCKET listenSock = (SOCKET)(uintptr_t)m_listenSocket;
    if (listenSock == INVALID_SOCKET) return;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(listenSock, &readSet);
    timeval tv = { 0, 100000 };  // 100 ms
    if (select(0, &readSet, nullptr, nullptr, &tv) <= 0)
        return;

    SOCKET clientSock = accept(listenSock, nullptr, nullptr);
    if (clientSock == INVALID_SOCKET) return;

    // Spawn handler for this client (so we don't block accept)
    void* clientCtx = (void*)(uintptr_t)clientSock;
    std::thread([this, clientCtx]() { handleClient(clientCtx); }).detach();
}

void WebSocketHub::handleClient(void* clientContext) {
    SOCKET sock = (SOCKET)(uintptr_t)clientContext;
    char buf[4096];
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        closesocket(sock);
        return;
    }
    buf[n] = '\0';
    std::string request(buf);

    // Parse Sec-WebSocket-Key
    std::string key;
    size_t keyPos = request.find("Sec-WebSocket-Key:");
    if (keyPos != std::string::npos) {
        keyPos = request.find(' ', keyPos);
        if (keyPos != std::string::npos) {
            size_t end = request.find("\r\n", keyPos);
            if (end != std::string::npos) {
                key = request.substr(keyPos + 1, end - keyPos - 1);
                while (!key.empty() && (key.back() == ' ' || key.back() == '\r')) key.pop_back();
            }
        }
    }

    std::string acceptKey = makeWebSocketAcceptKey(key);
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptKey << "\r\n\r\n";

    std::string responseStr = response.str();
    if (send(sock, responseStr.data(), (int)responseStr.size(), 0) == SOCKET_ERROR) {
        closesocket(sock);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.push_back(clientContext);
    }

    // Read WebSocket frames
    while (m_running.load()) {
        char header[2];
        if (recv(sock, header, 2, 0) != 2) break;
        unsigned char opcode = header[0] & 0x0Fu;
        bool masked = (header[1] & 0x80) != 0;
        uint64_t payloadLen = header[1] & 0x7Fu;
        if (payloadLen == 126) {
            char ext[2];
            if (recv(sock, ext, 2, 0) != 2) break;
            payloadLen = (unsigned char)ext[0] << 8 | (unsigned char)ext[1];
        } else if (payloadLen == 127) {
            char ext[8];
            if (recv(sock, ext, 8, 0) != 8) break;
            payloadLen = 0;
            for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | (unsigned char)ext[i];
        }
        char maskKey[4];
        if (masked) {
            if (recv(sock, maskKey, 4, 0) != 4) break;
        }
        if (payloadLen > 1024 * 1024) break;  // cap 1MB
        std::string payload((size_t)payloadLen, '\0');
        size_t received = 0;
        while (received < (size_t)payloadLen) {
            int r = recv(sock, &payload[received], (int)((size_t)payloadLen - received), 0);
            if (r <= 0) goto disconnect;
            received += (size_t)r;
        }
        if (masked) {
            for (size_t i = 0; i < payload.size(); i++)
                payload[i] ^= maskKey[i % 4];
        }
        if (opcode == 0x1 && m_onMessageReceived)
            m_onMessageReceived(payload, clientContext);
        if (opcode == 0x8) break;  // close
    }
disconnect:
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        auto it = std::find(m_clients.begin(), m_clients.end(), clientContext);
        if (it != m_clients.end()) m_clients.erase(it);
    }
    closesocket(sock);
}

bool WebSocketHub::sendTextToClient(void* clientContext, const std::string& text) {
    SOCKET sock = (SOCKET)(uintptr_t)clientContext;
    if (sock == INVALID_SOCKET) return false;
    size_t len = text.size();
    if (len > 0xFFFF) return false;
    char header[2];
    header[0] = 0x81;  // fin + text
    header[1] = (len < 126) ? (char)len : (char)126;
    if (send(sock, header, 2, 0) != 2) return false;
    if (len >= 126) {
        char ext[2] = { (char)(len >> 8), (char)(len & 0xFF) };
        if (send(sock, ext, 2, 0) != 2) return false;
    }
    if (send(sock, text.data(), (int)len, 0) != (int)len) return false;
    return true;
}

bool WebSocketHub::startServer(uint16_t port) {
    if (m_running.load()) return true;
    ensureWinsock();
    if (!s_ws2Started) return false;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(s);
        return false;
    }
    if (listen(s, SOMAXCONN) != 0) {
        closesocket(s);
        return false;
    }

    m_listenSocket = (void*)(uintptr_t)s;
    m_port = port;
    m_running.store(true);
    m_serverThread = std::thread(serverThreadFn, this);
    return true;
}

void WebSocketHub::stopServer() {
    m_running.store(false);
    if (m_listenSocket) {
        SOCKET s = (SOCKET)(uintptr_t)m_listenSocket;
        closesocket(s);
        m_listenSocket = nullptr;
    }
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (void* c : m_clients) {
            SOCKET cs = (SOCKET)(uintptr_t)c;
            closesocket(cs);
        }
        m_clients.clear();
    }
    if (m_serverThread.joinable())
        m_serverThread.join();
}

void WebSocketHub::broadcastMessage(const std::string& messageJson) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (void* c : m_clients)
        sendTextToClient(c, messageJson);
}
