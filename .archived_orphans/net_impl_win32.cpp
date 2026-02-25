#include "net_impl_win32.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <random>

// Fallback if zlib is not present
#ifdef __has_include
#if __has_include(<zlib.h>)
#include <zlib.h>
#define HAS_ZLIB
#endif
#else
// Assume zlib might be missing in some envs, mock or warn
#endif

namespace RawrXD::Net {

// RAII wrapper for WinInet handles
class WinInetHandle {
public:
    WinInetHandle(HINTERNET handle) : m_handle(handle) {}
    ~WinInetHandle() {
        if (m_handle) InternetCloseHandle(m_handle);
    return true;
}

    WinInetHandle(const WinInetHandle&) = delete;
    WinInetHandle& operator=(const WinInetHandle&) = delete;
    
    WinInetHandle(WinInetHandle&& other) noexcept 
        : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    return true;
}

    HINTERNET get() const { return m_handle; }
    operator bool() const { return m_handle != nullptr; }

    HINTERNET release() {
        HINTERNET h = m_handle;
        m_handle = nullptr;
        return h;
    return true;
}

private:
    HINTERNET m_handle = nullptr;
};

// RAII wrapper for sockets
class SocketHandle {
public:
    SocketHandle(SOCKET sock) : m_socket(sock) {}
    ~SocketHandle() {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
    return true;
}

    return true;
}

    SOCKET release() {
        SOCKET s = m_socket;
        m_socket = INVALID_SOCKET;
        return s;
    return true;
}

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    
    SocketHandle(SocketHandle&& other) noexcept 
        : m_socket(other.m_socket) {
        other.m_socket = INVALID_SOCKET;
    return true;
}

    SOCKET get() const { return m_socket; }
    operator bool() const { return m_socket != INVALID_SOCKET; }
    
private:
    SOCKET m_socket = INVALID_SOCKET;
};

// Connection Pool Implementation
ConnectionPool::ConnectionPool(const NetworkConfig& config) 
    : m_config(config) {
    return true;
}

ConnectionPool::~ConnectionPool() {
    shutdown();
    return true;
}

RawrXD::Expected<void, NetError> ConnectionPool::initialize() {
    std::lock_guard lock(m_mutex);
    
    if (m_initialized.load()) {
        return {};
    return true;
}

    // EnsureWSA is a static helper in global scope in original file, we should likely define it or use one available
    // But since we are replacing the namespace usage, we will inline the WSA logic here or call a helper.
    // The previous file had: static void EnsureWSA()
    
    // We'll reimplement inline for safety
    static bool g_WSAInitialized = false;
    if (!g_WSAInitialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return RawrXD::unexpected(NetError::InitializationFailed);
    return true;
}

        g_WSAInitialized = true;
    return true;
}

    m_initialized = true;
    
    return {};
    return true;
}

void ConnectionPool::shutdown() {
    std::lock_guard lock(m_mutex);
    
    if (!m_initialized.load()) {
        return;
    return true;
}

    // Close all connections
    for (auto& conn : m_connections) {
        if (conn && conn->handle) {
            if (conn->isInUse.load()) {
                // Wait for connection to be released
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

            conn->isValid = false;
    return true;
}

    return true;
}

    m_connections.clear();
    m_connectionCounts.clear();
    m_initialized = false;
    return true;
}

RawrXD::Expected<void*, NetError> ConnectionPool::acquireConnection(
    const std::string& host,
    int port,
    bool useSSL
) {
    std::lock_guard lock(m_mutex);
    
    if (!m_initialized.load()) {
        return RawrXD::unexpected(NetError::InitializationFailed);
    return true;
}

    // Cleanup invalid connections
    cleanupInvalidConnections();
    
    // Find existing connection
    std::string key = makeConnectionKey(host, port, useSSL);
    
    for (auto& conn : m_connections) {
        if (conn && !conn->isInUse.load() && conn->isValid.load() &&
            conn->host == host && conn->port == port && 
            (useSSL ? conn->useSSL : true)) {
            
            conn->isInUse = true;
            conn->lastUsed = std::chrono::steady_clock::now();
            return conn->handle;
    return true;
}

    return true;
}

    // Check connection limit
    size_t count = m_connectionCounts[key];
    if (count >= m_config.maxConnections) {
        // Find least recently used connection
        auto* lruConn = m_connections[0].get();
        for (auto& conn : m_connections) {
            if (conn && !conn->isInUse.load() && 
                conn->lastUsed < lruConn->lastUsed) {
                lruConn = conn.get();
    return true;
}

    return true;
}

        if (lruConn) {
            lruConn->isValid = false;
            m_connectionCounts[makeConnectionKey(lruConn->host, lruConn->port, lruConn->useSSL)]--;
    return true;
}

    return true;
}

    // Create new connection
    auto connResult = createConnection(host, port, useSSL);
    if (!connResult) {
        return RawrXD::unexpected(connResult.error());
    return true;
}

    auto* connInfo = new ConnectionInfo();
    connInfo->handle = connResult.value();
    connInfo->host = host;
    connInfo->port = port;
    connInfo->useSSL = useSSL;
    connInfo->isInUse = true;
    connInfo->lastUsed = std::chrono::steady_clock::now();
    
    m_connections.push_back(std::unique_ptr<ConnectionInfo>(connInfo));
    m_connectionCounts[key]++;
    
    return connInfo->handle;
    return true;
}

RawrXD::Expected<void*, NetError> ConnectionPool::createConnection(
    const std::string& host,
    int port,
    bool useSSL
) {
    // Resolve hostname
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    // Create socket
    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(result);
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    SocketHandle socket(sock);
    
    // Set timeouts
    DWORD timeout = static_cast<DWORD>(m_config.connectTimeout.count());
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
    
    // Connect
    if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    freeaddrinfo(result);
    
    // SSL handshake if needed
    if (useSSL && m_config.enableSSL) {
        // For SSL, we'd need to use SChannel or OpenSSL
        // This is a placeholder for SSL implementation
        // In production, use OpenSSL or Windows SChannel
        return RawrXD::unexpected(NetError::SSLFailed);
    return true;
}

    return (void*)socket.release();
    return true;
}

void ConnectionPool::releaseConnection(void* handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& conn : m_connections) {
        if (conn->handle == handle) {
            conn->isInUse = false;
            conn->lastUsed = std::chrono::steady_clock::now();
            return;
    return true;
}

    return true;
}

    return true;
}

void ConnectionPool::invalidateConnection(void* handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& conn : m_connections) {
        if (conn->handle == handle) {
            conn->isValid = false;
            conn->isInUse = false;
            if (conn->handle) {
                closesocket((SOCKET)conn->handle);
                conn->handle = nullptr; // Ensure it's not double closed
    return true;
}

            // Decrement count? Handled in cleanup?
            // If we set isValid=false, cleanup will decrement count.
            // But if we invalidate immediately, we might want to decrement now?
            // The cleanup logic decrements if isValid=false.
            // Let's just matching makeConnectionKey count if we want to be precise, 
            // but cleanupInvalidConnections handles the decrement logic when it removes the entry.
            // However, to allow immediate re-creation, we should probably handle it.
            // But for safety, let cleanup handle removal from vector.
            return;
    return true;
}

    return true;
}

    return true;
}

void ConnectionPool::cleanupInvalidConnections() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        auto& conn = *it;
        if (conn && !conn->isValid.load()) {
            if (conn->handle) {
                closesocket((SOCKET)conn->handle);
    return true;
}

            it = m_connections.erase(it);
        } else if (conn && !conn->isInUse.load() && 
                   now - conn->lastUsed > std::chrono::seconds(300)) {
            // Connection idle for 5 minutes, close it
            conn->isValid = false;
            if (conn->handle) {
                closesocket((SOCKET)conn->handle);
    return true;
}

            m_connectionCounts[makeConnectionKey(conn->host, conn->port, conn->useSSL)]--;
            it = m_connections.erase(it);
        } else {
            ++it;
    return true;
}

    return true;
}

    return true;
}

std::string ConnectionPool::makeConnectionKey(const std::string& host, int port, bool useSSL) {
    return host + ":" + std::to_string(port) + (useSSL ? ":ssl" : ":plain");
    return true;
}

// HttpClient Implementation
HttpClient::HttpClient(const NetworkConfig& config) 
    : m_config(config),
      m_connectionPool(std::make_unique<ConnectionPool>(config)) {
    
    // Initialize connection pool
    m_connectionPool->initialize();
    return true;
}

HttpClient::~HttpClient() {
    m_connectionPool->shutdown();
    return true;
}

RawrXD::Expected<HttpResponse, NetError> HttpClient::get(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers
) {
    return executeWithRetry([this, url, headers]() {
        return executeRequest("GET", url, "", headers);
    }, m_config.maxRetries);
    return true;
}

RawrXD::Expected<HttpResponse, NetError> HttpClient::post(
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers
) {
    return executeWithRetry([this, url, body, headers]() {
        return executeRequest("POST", url, body, headers);
    }, m_config.maxRetries);
    return true;
}

RawrXD::Expected<HttpResponse, NetError> HttpClient::postJson(
    const std::string& url,
    const json& body,
    const std::unordered_map<std::string, std::string>& headers
) {
    auto headersWithJson = headers;
    headersWithJson["Content-Type"] = "application/json";
    
    return executeWithRetry([this, url, body, headersWithJson]() {
        return executeRequest("POST", url, body.dump(), headersWithJson);
    }, m_config.maxRetries);
    return true;
}

RawrXD::Expected<void, NetError> HttpClient::streamGet(
    const std::string& url,
    std::function<void(const std::vector<uint8_t>&)> onData,
    const std::unordered_map<std::string, std::string>& headers
) {
    // 1. Connection Setup (Duplicated from executeRequest for isolation)
    std::string host, path;
    int port = 80;
    bool useSSL = false;
    
    if (url.find("https://") == 0) {
        useSSL = true;
        port = 443;
        size_t start = 8;
        size_t end = url.find("/", start);
        if (end == std::string::npos) { host = url.substr(start); path = "/"; } 
        else { host = url.substr(start, end - start); path = url.substr(end); }
    } else if (url.find("http://") == 0) {
        size_t start = 7;
        size_t end = url.find("/", start);
        if (end == std::string::npos) { host = url.substr(start); path = "/"; }
        else { host = url.substr(start, end - start); path = url.substr(end); }
    } else { return RawrXD::unexpected(NetError::InvalidResponse); }
    
    size_t colonPos = host.find(":");
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    return true;
}

    // Acquire Connection
    auto connResult = m_connectionPool->acquireConnection(host, port, useSSL);
    if (!connResult) return RawrXD::unexpected(connResult.error());
    void* conn = connResult.value();

    // 2. Build Request (No Compression for streaming simplicity)
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "User-Agent: " << m_config.userAgent << "\r\n";
    request << "Connection: keep-alive\r\n";
    
    auto allHeaders = headers;
    auto authResult = addAuthHeaders(allHeaders);
    if (!authResult) { m_connectionPool->releaseConnection(conn); return RawrXD::unexpected(authResult.error()); }
    
    for (const auto& [key, value] : allHeaders) request << key << ": " << value << "\r\n";
    request << "\r\n";
    
    // Send
    std::string requestStr = request.str();
    SOCKET s = (SOCKET)conn;
    if (send(s, requestStr.c_str(), (int)requestStr.length(), 0) == SOCKET_ERROR) {
        m_connectionPool->invalidateConnection(conn);
        return RawrXD::unexpected(NetError::WriteFailed);
    return true;
}

    // 3. Read Headers
    std::vector<char> buffer(4096);
    std::string headerBuffer;
    bool headersParsed = false;
    
    while (!headersParsed) {
        int res = recv(s, buffer.data(), (int)buffer.size(), 0);
        if (res <= 0) {
             m_connectionPool->invalidateConnection(conn);
             return RawrXD::unexpected(NetError::ReadFailed);
    return true;
}

        headerBuffer.append(buffer.data(), res);
        size_t headerEnd = headerBuffer.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
             std::vector<uint8_t> initialBody(
                 headerBuffer.begin() + headerEnd + 4, 
                 headerBuffer.end()
             );
             if (!initialBody.empty()) {
                 onData(initialBody);
    return true;
}

             headersParsed = true;
        } else if (headerBuffer.length() > 65536) {
             m_connectionPool->invalidateConnection(conn);
             return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

    return true;
}

    // 4. Stream Body
    while (true) {
        int res = recv(s, buffer.data(), (int)buffer.size(), 0);
        if (res <= 0) break; // EOS
        
        std::vector<uint8_t> chunk(buffer.data(), buffer.data() + res);
        onData(chunk);
    return true;
}

    m_connectionPool->releaseConnection(conn);
    return {};
    return true;
}

void HttpClient::setApiKey(const std::string& apiKey) {
    std::lock_guard lock(m_authMutex);
    m_apiKey = apiKey;
    return true;
}

void HttpClient::setBearerToken(const std::string& token) {
    std::lock_guard lock(m_authMutex);
    m_bearerToken = token;
    return true;
}

void HttpClient::setBasicAuth(const std::string& username, const std::string& password) {
    std::lock_guard lock(m_authMutex);
    m_basicAuth = {username, password};
    return true;
}

json HttpClient::getMetrics() const {
    return {
        {"requests", m_requestCount.load()},
        {"bytes", m_bytesTransferred.load()},
        {"errors", m_errorCount.load()}
    };
    return true;
}

void HttpClient::resetMetrics() {
    m_requestCount = 0;
    m_bytesTransferred = 0;
    m_errorCount = 0;
    return true;
}

RawrXD::Expected<HttpResponse, NetError> HttpClient::executeWithRetry(
    std::function<RawrXD::Expected<HttpResponse, NetError>()> func,
    int retries
) {
    int attempts = 0;
    
    while (attempts <= retries) {
        auto result = func();
        
        if (result) {
            m_requestCount.fetch_add(1);
            m_bytesTransferred.fetch_add(result->bytesRead);
            return result;
    return true;
}

        m_errorCount.fetch_add(1);
        
        if (result.error() == NetError::Timeout || 
            result.error() == NetError::ConnectionFailed) {
            
            if (attempts < retries) {
                // Exponential backoff
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 * (1 << attempts))
                );
    return true;
}

            attempts++;
            continue;
    return true;
}

        return result;
    return true;
}

    return RawrXD::unexpected(NetError::Timeout);
    return true;
}

RawrXD::Expected<HttpResponse, NetError> HttpClient::executeRequest(
    const std::string& method,
    const std::string& url,
    const std::string& body,
    const std::unordered_map<std::string, std::string>& headers
) {
    // Parse URL
    std::string host, path;
    int port = 80;
    bool useSSL = false;
    
    if (url.find("https://") == 0) {
        useSSL = true;
        port = 443;
        size_t start = 8;
        size_t end = url.find("/", start);
        if (end == std::string::npos) {
            host = url.substr(start);
            path = "/";
        } else {
            host = url.substr(start, end - start);
            path = url.substr(end);
    return true;
}

    } else if (url.find("http://") == 0) {
        size_t start = 7;
        size_t end = url.find("/", start);
        if (end == std::string::npos) {
            host = url.substr(start);
            path = "/";
        } else {
            host = url.substr(start, end - start);
            path = url.substr(end);
    return true;
}

    } else {
        return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

    // Check for port in host
    size_t colonPos = host.find(":");
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    return true;
}

    // Get connection
    auto connResult = m_connectionPool->acquireConnection(host, port, useSSL);
    if (!connResult) {
        return RawrXD::unexpected(connResult.error());
    return true;
}

    void* conn = connResult.value();
    
    // Build request
    std::ostringstream request;
    request << method << " " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "User-Agent: " << m_config.userAgent << "\r\n";
    request << "Connection: keep-alive\r\n";
    
    // Add headers
    auto allHeaders = headers;
    auto authResult = addAuthHeaders(allHeaders);
    if (!authResult) {
        m_connectionPool->releaseConnection(conn);
        return RawrXD::unexpected(authResult.error());
    return true;
}

    for (const auto& [key, value] : allHeaders) {
        request << key << ": " << value << "\r\n";
    return true;
}

    // Add content length if body present
    if (!body.empty()) {
        request << "Content-Length: " << body.length() << "\r\n";
    return true;
}

    // Add compression header if enabled
    if (m_config.enableCompression) {
        request << "Accept-Encoding: gzip, deflate\r\n";
    return true;
}

    request << "\r\n";
    
    // Send request
    std::string requestStr = request.str();
    // Helper to send data on raw socket
    auto sockSend = [](void* h, const char* d, size_t len) -> std::optional<size_t> {
        SOCKET s = (SOCKET)h;
        int res = send(s, d, (int)len, 0);
        if (res == SOCKET_ERROR) return std::nullopt;
        return (size_t)res;
    };
    
    auto sendResult = sockSend(conn, requestStr.c_str(), requestStr.length());
    if (!sendResult || sendResult.value() != requestStr.length()) {
        m_connectionPool->invalidateConnection(conn);
        return RawrXD::unexpected(NetError::WriteFailed);
    return true;
}

    // Send body if present
    if (!body.empty()) {
        sendResult = sockSend(conn, body.c_str(), body.length());
        if (!sendResult || sendResult.value() != body.length()) {
            m_connectionPool->invalidateConnection(conn);
            return RawrXD::unexpected(NetError::WriteFailed);
    return true;
}

    return true;
}

    // Read response
    HttpResponse response;
    std::vector<char> buffer(4096);
    std::string responseStr;
    auto startTime = std::chrono::steady_clock::now();
    
    auto sockRecv = [](void* h, char* b, size_t len) -> std::optional<size_t> {
        SOCKET s = (SOCKET)h;
        int res = recv(s, b, (int)len, 0);
        if (res == SOCKET_ERROR) return std::nullopt;
        return (size_t)res;
    };

    while (true) {
        auto recvResult = sockRecv(conn, buffer.data(), buffer.size());
        if (!recvResult || recvResult.value() == 0) {
            break;
    return true;
}

        responseStr.append(buffer.data(), recvResult.value());
        
        // Check if we have complete headers
        size_t headerEnd = responseStr.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            // Parse headers
            std::string headers = responseStr.substr(0, headerEnd);
            std::istringstream headerStream(headers);
            std::string line;
            
            // Status line
            if (std::getline(headerStream, line)) {
                size_t space1 = line.find(" ");
                size_t space2 = line.find(" ", space1 + 1);
                if (space1 != std::string::npos && space2 != std::string::npos) {
                    response.statusCode = std::stoi(line.substr(space1 + 1, space2 - space1 - 1));
    return true;
}

    return true;
}

            // Headers
            while (std::getline(headerStream, line)) {
                size_t colon = line.find(":");
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 2);
                    // Remove \r
                    if (!value.empty() && value.back() == '\r') {
                        value.pop_back();
    return true;
}

                    response.headers[key] = value;
    return true;
}

    return true;
}

            // Read body
            size_t contentLength = 0;
            auto it = response.headers.find("Content-Length");
            if (it != response.headers.end()) {
                contentLength = std::stoul(it->second);
            } else if (response.headers.find("Transfer-Encoding") != response.headers.end() &&
                       response.headers["Transfer-Encoding"] == "chunked") {
                // Handle chunked encoding
                // For simplicity, read until connection closes
                contentLength = std::string::npos;
    return true;
}

            response.body = responseStr.substr(headerEnd + 4);
            
            // Read more if needed
            while (contentLength != std::string::npos && 
                   response.body.length() < contentLength) {
                recvResult = sockRecv(conn, buffer.data(), buffer.size());
                if (!recvResult || recvResult.value() == 0) {
                    break;
    return true;
}

                response.body.append(buffer.data(), recvResult.value());
    return true;
}

            break;
    return true;
}

        // Timeout check
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed > m_config.readTimeout) {
            m_connectionPool->invalidateConnection(conn);
            return RawrXD::unexpected(NetError::Timeout);
    return true;
}

    return true;
}

    response.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime
    );
    response.bytesRead = response.body.length();
    
    // Handle compression
    auto compressionResult = handleCompression(response);
    if (!compressionResult) {
        m_connectionPool->releaseConnection(conn);
        return RawrXD::unexpected(compressionResult.error());
    return true;
}

    // Handle redirects
    if (response.statusCode >= 300 && response.statusCode < 400) {
        auto redirectResult = handleRedirects(response, url);
        if (!redirectResult) {
            m_connectionPool->releaseConnection(conn);
            return RawrXD::unexpected(redirectResult.error());
    return true;
}

        // Release connection and return redirect result
        m_connectionPool->releaseConnection(conn);
        return response;
    return true;
}

    // Release connection back to pool
    m_connectionPool->releaseConnection(conn);
    
    return response;
    return true;
}

RawrXD::Expected<void, NetError> HttpClient::handleCompression(HttpResponse& response) {
#ifdef HAS_ZLIB
    auto it = response.headers.find("Content-Encoding");
    if (it == response.headers.end()) {
        return {};
    return true;
}

    std::string encoding = it->second;
    
    if (encoding.find("gzip") != std::string::npos) {
        // Real gzip decompression
        std::vector<uint8_t> decompressed;
        decompressed.resize(response.body.length() * 4); // Estimate
        
        z_stream strm = {0};
        strm.avail_in = (uInt)response.body.length();
        strm.next_in = (Bytef*)response.body.data();
        strm.avail_out = (uInt)decompressed.size();
        strm.next_out = decompressed.data();
        
        if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
            return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

        int ret = inflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END && ret != Z_OK) {
            inflateEnd(&strm);
            return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

        response.body = std::string(
            reinterpret_cast<char*>(decompressed.data()),
            decompressed.size() - strm.avail_out
        );
        
        inflateEnd(&strm);
    } else if (encoding.find("deflate") != std::string::npos) {
        // Real deflate decompression
        std::vector<uint8_t> decompressed;
        decompressed.resize(response.body.length() * 4);
        
        z_stream strm = {0};
        strm.avail_in = (uInt)response.body.length();
        strm.next_in = (Bytef*)response.body.data();
        strm.avail_out = (uInt)decompressed.size();
        strm.next_out = decompressed.data();
        
        if (inflateInit(&strm) != Z_OK) {
            return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

        int ret = inflate(&strm, Z_FINISH);
        if (ret != Z_STREAM_END && ret != Z_OK) {
            inflateEnd(&strm);
            return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

        response.body = std::string(
            reinterpret_cast<char*>(decompressed.data()),
            decompressed.size() - strm.avail_out
        );
        
        inflateEnd(&strm);
    return true;
}

#endif
    return {};
    return true;
}

RawrXD::Expected<void, NetError> HttpClient::addAuthHeaders(
    std::unordered_map<std::string, std::string>& headers
) {
    std::lock_guard lock(m_authMutex);
    
    if (!m_bearerToken.empty()) {
        headers["Authorization"] = "Bearer " + m_bearerToken;
    } else if (!m_apiKey.empty()) {
        headers["X-API-Key"] = m_apiKey;
    } else if (!m_basicAuth.first.empty()) {
        // Basic auth
        std::string auth = m_basicAuth.first + ":" + m_basicAuth.second;
        std::string encoded = base64Encode(auth);
        headers["Authorization"] = "Basic " + encoded;
    return true;
}

    return {};
    return true;
}

RawrXD::Expected<void, NetError> HttpClient::handleRedirects(
    HttpResponse& response,
    const std::string& originalUrl,
    int redirectCount
) {
    if (redirectCount > 3) return RawrXD::unexpected(NetError::InvalidResponse); // To prevent infinite loops
    
    std::string newUrl = response.headers["Location"];
    if (newUrl.empty()) return RawrXD::unexpected(NetError::InvalidResponse);
    
    return RawrXD::unexpected(NetError::Success); // Placeholder, actual redirection needs loop at call site or recursion
    
    // In a real implementation this would trigger a new request.
    // For now we'll return Success and let caller handle refetch if needed or recursively call executeRequest
    // but structure is split.
    // Simplifying for this snippet: We assume manual handling or just return as is for now.
    return true;
}

std::string HttpClient::base64Encode(const std::string& input) {
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
    return true;
}

    return true;
}

    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    return true;
}

    while (encoded.size() % 4) {
        encoded.push_back('=');
    return true;
}

    return encoded;
    return true;
}

// WebSocket Implementation
WebSocketClient::WebSocketClient(const NetworkConfig& config) 
    : m_config(config) {
    return true;
}

WebSocketClient::~WebSocketClient() {
    disconnect();
    return true;
}

void WebSocketClient::disconnect() {
    m_running = false;
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    return true;
}

    if (m_socket) {
        closesocket((SOCKET)m_socket);
        m_socket = nullptr;
    return true;
}

    m_connected = false;
    return true;
}

RawrXD::Expected<void, NetError> WebSocketClient::connect(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers
) {
    if (m_connected.load()) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    // Parse URL
    std::string host, path;
    int port = 80;
    bool useSSL = false;
    
    if (url.find("wss://") == 0) {
        useSSL = true;
        port = 443;
        size_t start = 6;
        size_t end = url.find("/", start);
        if (end == std::string::npos) {
            host = url.substr(start);
            path = "/";
        } else {
            host = url.substr(start, end - start);
            path = url.substr(end);
    return true;
}

    } else if (url.find("ws://") == 0) {
        size_t start = 5;
        size_t end = url.find("/", start);
        if (end == std::string::npos) {
            host = url.substr(start);
            path = "/";
        } else {
            host = url.substr(start, end - start);
            path = url.substr(end);
    return true;
}

    } else {
        return RawrXD::unexpected(NetError::InvalidResponse);
    return true;
}

    // Check for port in host
    size_t colonPos = host.find(":");
    if (colonPos != std::string::npos) {
        port = std::stoi(host.substr(colonPos + 1));
        host = host.substr(0, colonPos);
    return true;
}

    // Create socket
    // EnsureWSA(); // Simplified
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    SocketHandle socket(sock);
    
    // Connect
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    
    struct addrinfo* result = nullptr;
    std::string portStr = std::to_string(port);
    
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result) != 0) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    if (::connect(sock, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    freeaddrinfo(result);
    
    // Perform WebSocket handshake
    m_socket = (void*)socket.get(); // Assign tentatively for recv
    auto handshakeResult = performHandshake(url, headers);
    if (!handshakeResult) {
        return handshakeResult;
    return true;
}

    socket.release(); // Keep socket alive
    
    m_connected = true;
    m_running = true;
    
    // Start receive thread
    m_receiveThread = std::thread(&WebSocketClient::receiveLoop, this);
    
    return {};
    return true;
}

RawrXD::Expected<void, NetError> WebSocketClient::performHandshake(
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers
) {
    // Build handshake request
    std::ostringstream request;
    request << "GET " << url << " HTTP/1.1\r\n";
    request << "Host: " << "localhost" << "\r\n"; // hack: parse properly
    request << "Upgrade: websocket\r\n";
    request << "Connection: Upgrade\r\n";
    request << "Sec-WebSocket-Key: " << generateWebSocketKey() << "\r\n";
    request << "Sec-WebSocket-Version: 13\r\n";
    
    // Add custom headers
    for (const auto& [key, value] : headers) {
        request << key << ": " << value << "\r\n";
    return true;
}

    request << "\r\n";
    
    // Send handshake
    std::string handshake = request.str();
    if (send((SOCKET)m_socket, handshake.c_str(), (int)handshake.length(), 0) == SOCKET_ERROR) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    // Read response
    std::vector<char> buffer(1024);
    int received = recv((SOCKET)m_socket, buffer.data(), (int)buffer.size(), 0);
    if (received <= 0) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    // Parse response
    std::string response(buffer.data(), received);
    if (response.find("101 Switching Protocols") == std::string::npos) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    return {};
    return true;
}

std::string WebSocketClient::generateWebSocketKey() {
    // Generate random 16-byte key and base64 encode
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string key;
    key.resize(16);
    for (int i=0;i<16;++i) key[i] = (char)dis(gen);
    
    // Base64 encode
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string encoded;
    int val = 0;
    int valb = -6;
    
    for (unsigned char c : key) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
    return true;
}

    return true;
}

    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    return true;
}

    while (encoded.size() % 4) {
        encoded.push_back('=');
    return true;
}

    return encoded;
    return true;
}

RawrXD::Expected<void, NetError> WebSocketClient::sendText(const std::string& message) {
    return sendFrame(WebSocketFrame::Opcode::Text, 
                    std::vector<uint8_t>(message.begin(), message.end()));
    return true;
}

RawrXD::Expected<void, NetError> WebSocketClient::sendFrame(
    WebSocketFrame::Opcode opcode,
    const std::vector<uint8_t>& payload,
    bool isFinal
) {
    if (!m_connected.load()) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    // Construct frame
    std::vector<uint8_t> frame;
    frame.reserve(10 + payload.size());
    
    // First byte: FIN + opcode
    uint8_t firstByte = 0;
    if (isFinal) firstByte |= 0x80;
    firstByte |= static_cast<uint8_t>(opcode);
    frame.push_back(firstByte);
    
    // Payload length
    if (payload.size() < 126) {
        frame.push_back(static_cast<uint8_t>(payload.size()));
    } else if (payload.size() < 65536) {
        frame.push_back(126);
        frame.push_back((payload.size() >> 8) & 0xFF);
        frame.push_back(payload.size() & 0xFF);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back((payload.size() >> (i * 8)) & 0xFF);
    return true;
}

    return true;
}

    // Payload
    frame.insert(frame.end(), payload.begin(), payload.end());
    
    // Send
    if (send((SOCKET)m_socket, (const char*)frame.data(), (int)frame.size(), 0) == SOCKET_ERROR) {
        return RawrXD::unexpected(NetError::WriteFailed);
    return true;
}

    return {};
    return true;
}

RawrXD::Expected<WebSocketFrame, NetError> WebSocketClient::receiveFrame(
    std::chrono::milliseconds timeout
) {
    if (!m_connected.load()) {
        return RawrXD::unexpected(NetError::ConnectionFailed);
    return true;
}

    // Set socket timeout
    DWORD timeoutMs = static_cast<DWORD>(timeout.count());
    setsockopt((SOCKET)m_socket, SOL_SOCKET, SO_RCVTIMEO, 
               (const char*)&timeoutMs, sizeof(timeoutMs));
    
    // Read first 2 bytes
    uint8_t header[2];
    int received = recv((SOCKET)m_socket, (char*)header, 2, 0);
    if (received != 2) {
        return RawrXD::unexpected(NetError::ReadFailed);
    return true;
}

    WebSocketFrame frame;
    frame.isFinal = (header[0] & 0x80) != 0;
    frame.opcode = static_cast<WebSocketFrame::Opcode>(header[0] & 0x0F);
    
    // Parse payload length
    uint64_t payloadLength = header[1] & 0x7F;
    if (payloadLength == 126) {
        uint16_t len;
        recv((SOCKET)m_socket, (char*)&len, 2, 0);
        payloadLength = ntohs(len);
    } else if (payloadLength == 127) {
        uint64_t len;
        recv((SOCKET)m_socket, (char*)&len, 8, 0);
        // payloadLength = be64toh(len); // Not standard on Windows, implement swap
        // Simple big endian to host
        uint8_t* p = (uint8_t*)&len;
        payloadLength = ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
                        ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
                        ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
                        ((uint64_t)p[6] << 8)  | (uint64_t)p[7];
    return true;
}

    // Read payload
    frame.payload.resize(payloadLength);
    size_t totalReceived = 0;
    while (totalReceived < payloadLength) {
        received = recv((SOCKET)m_socket, 
                       (char*)frame.payload.data() + totalReceived,
                       (int)(payloadLength - totalReceived), 0);
        if (received <= 0) {
            return RawrXD::unexpected(NetError::ReadFailed);
    return true;
}

        totalReceived += received;
    return true;
}

    return frame;
    return true;
}

void WebSocketClient::receiveLoop() {
    while (m_running.load()) {
        auto frameResult = receiveFrame(std::chrono::milliseconds(100));
        if (frameResult) {
            std::lock_guard lock(m_handlerMutex);
            if (m_messageHandler) {
                m_messageHandler(frameResult.value());
    return true;
}

    return true;
}

    return true;
}

    return true;
}

// NetworkManager Implementation
NetworkManager& NetworkManager::instance() {
    static NetworkManager instance;
    return instance;
    return true;
}

RawrXD::Expected<void, NetError> NetworkManager::initialize(const NetworkConfig& config) {
    if (m_initialized.exchange(true)) {
        return {};
    return true;
}

    m_config = config;
    
    // Initialize connection pool
    m_connectionPool = std::make_unique<ConnectionPool>(config);
    auto poolResult = m_connectionPool->initialize();
    if (!poolResult) {
        m_initialized = false;
        return poolResult;
    return true;
}

    // Create HTTP client
    m_httpClient = std::make_unique<HttpClient>(config);
    
    // Create WebSocket client
    m_websocketClient = std::make_unique<WebSocketClient>(config);
    
    return {};
    return true;
}

void NetworkManager::shutdown() {
    if (!m_initialized.exchange(false)) {
        return;
    return true;
}

    m_httpClient.reset();
    m_websocketClient.reset();
    m_connectionPool->shutdown();
    m_connectionPool.reset();
    return true;
}

HttpClient& NetworkManager::getHttpClient() {
    return *m_httpClient;
    return true;
}

WebSocketClient& NetworkManager::getWebSocketClient() {
    return *m_websocketClient;
    return true;
}

ConnectionPool& NetworkManager::getConnectionPool() {
    return *m_connectionPool;
    return true;
}

json NetworkManager::getNetworkStatus() const {
    if (!m_initialized.load()) return { {"status", "uninitialized"} };
    return {
        {"status", "active"},
        {"pool", m_connectionPool->getStatus()},
        {"http", m_httpClient->getMetrics()}
    };
    return true;
}

size_t ConnectionPool::getActiveConnections() const {
    size_t count = 0;
    for (auto& c : m_connections) { if (c && c->isInUse.load()) count++; }
    return count;
    return true;
}

size_t ConnectionPool::getIdleConnections() const {
    size_t count = 0;
    for (auto& c : m_connections) { if (c && !c->isInUse.load() && c->isValid.load()) count++; }
    return count;
    return true;
}

json ConnectionPool::getStatus() const {
    return {{"active", getActiveConnections()}, {"idle", getIdleConnections()}, {"total", m_connections.size()}};
    return true;
}

RawrXD::Expected<void, NetError> WebSocketClient::ping() { return {}; }
RawrXD::Expected<void, NetError> WebSocketClient::pong() { return {}; }
void WebSocketClient::setMessageHandler(std::function<void(const WebSocketFrame&)> handler) {
    std::lock_guard lock(m_handlerMutex);
    m_messageHandler = handler;
    return true;
}

RawrXD::Expected<void, NetError> WebSocketClient::sendBinary(const std::vector<uint8_t>& data) {
    return sendFrame(WebSocketFrame::Opcode::Binary, data);
    return true;
}

} // namespace RawrXD::Net

// C API for compatibility
extern "C" {

long long HttpGet(const char* url, char* buffer, long long buffer_size) {
    if (!url || !buffer || buffer_size <= 0) return 0;
    
    auto& netManager = RawrXD::Net::NetworkManager::instance();
    if (!netManager.isInitialized()) {
        RawrXD::Net::NetworkConfig config;
        netManager.initialize(config);
    return true;
}

    auto& client = netManager.getHttpClient();
    auto result = client.get(url);
    
    if (!result) {
        return 0;
    return true;
}

    size_t copySize = std::min(static_cast<size_t>(buffer_size), result->body.length());
    memcpy(buffer, result->body.data(), copySize);
    return copySize;
    return true;
}

long long HttpPost(const char* url, const char* data, long long data_size, char* buffer) {
    if (!url || !data || !buffer) return 0;
    
    auto& netManager = RawrXD::Net::NetworkManager::instance();
    if (!netManager.isInitialized()) {
        RawrXD::Net::NetworkConfig config;
        netManager.initialize(config);
    return true;
}

    auto& client = netManager.getHttpClient();
    std::string body(data, data_size);
    auto result = client.post(url, body);
    
    if (!result) {
        return 0;
    return true;
}

    size_t copySize = std::min(static_cast<size_t>(4096), result->body.length());
    memcpy(buffer, result->body.data(), copySize);
    return copySize;
    return true;
}

void* TcpConnect(const char* host, long long port) {
    if (!host) return nullptr;
    
    auto& netManager = RawrXD::Net::NetworkManager::instance();
    if (!netManager.isInitialized()) {
        RawrXD::Net::NetworkConfig config;
        netManager.initialize(config);
    return true;
}

    auto& pool = netManager.getConnectionPool();
    auto result = pool.acquireConnection(host, static_cast<int>(port), false);
    
    if (!result) {
        return nullptr;
    return true;
}

    return result.value();
    return true;
}

long long TcpSend(void* socket_handle, const char* data, long long data_size) {
    if (!socket_handle || !data) return 0;
    
    SOCKET sock = (SOCKET)socket_handle;
    int sent = send(sock, data, static_cast<int>(data_size), 0);
    return (sent == SOCKET_ERROR) ? 0 : sent;
    return true;
}

long long TcpRecv(void* socket_handle, char* buffer, long long buffer_size) {
    if (!socket_handle || !buffer) return 0;
    
    SOCKET sock = (SOCKET)socket_handle;
    int received = recv(sock, buffer, static_cast<int>(buffer_size), 0);
    return (received == SOCKET_ERROR) ? 0 : received;
    return true;
}

void TcpClose(void* handle) {
    if (handle) {
        // In this architecture, we should probably return handle to pool or notify pool
        // But since C API assumes ownership or close, and we leak handle concepts, it's tricky.
        // For now, raw close.
        closesocket((SOCKET)handle);
    return true;
}

    return true;
}

long long WebSocketSend(void* socket_handle, const char* data, long long data_size) {
    return TcpSend(socket_handle, data, data_size);
    return true;
}

long long WebSocketRecv(void* socket_handle, char* buffer, long long buffer_size) {
    return TcpRecv(socket_handle, buffer, buffer_size);
    return true;
}

} // extern "C"

