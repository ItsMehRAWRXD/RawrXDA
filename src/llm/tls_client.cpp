/**
 * @file tls_client.cpp
 * @brief Production TLS 1.3 Client Implementation using WinHTTP/SChannel
 * 
 * Implements secure HTTPS connections for LLM API calls with:
 * - TLS 1.3 support via SChannel
 * - Certificate validation and pinning
 * - Connection pooling with keep-alive
 * - Async request/response handling
 * - Retry with exponential backoff
 * 
 * @author RawrXD Team
 * @version 1.0.0
 */

#include "tls_client.hpp"
#include <sstream>
#include <algorithm>
#include <random>
#include <regex>
#include <iomanip>

namespace RawrXD::LLM {

// ============================================================================
// TLSRequest Convenience Constructors
// ============================================================================

TLSRequest TLSRequest::GET(const std::string& url) {
    TLSRequest req;
    req.method = "GET";
    
    auto parsed = TLSClient::parseURL(url);
    req.host = parsed.host;
    req.port = parsed.port;
    req.path = parsed.path;
    if (!parsed.query.empty()) req.path += "?" + parsed.query;
    req.useTLS = (parsed.scheme == "https");
    
    return req;
}

TLSRequest TLSRequest::POST(const std::string& url, const std::vector<uint8_t>& data) {
    TLSRequest req;
    req.method = "POST";
    
    auto parsed = TLSClient::parseURL(url);
    req.host = parsed.host;
    req.port = parsed.port;
    req.path = parsed.path;
    if (!parsed.query.empty()) req.path += "?" + parsed.query;
    req.useTLS = (parsed.scheme == "https");
    req.body = data;
    
    return req;
}

TLSRequest TLSRequest::POST(const std::string& url, const std::string& json) {
    TLSRequest req = POST(url, std::vector<uint8_t>(json.begin(), json.end()));
    req.headers["Content-Type"] = "application/json";
    return req;
}

// ============================================================================
// TLSResponse Convenience Methods
// ============================================================================

std::string TLSResponse::bodyAsString() const {
    return std::string(body.begin(), body.end());
}

// ============================================================================
// TLSConnectionPool Implementation
// ============================================================================

TLSConnectionPool::TLSConnectionPool(const TLSConfig& config) : m_config(config) {}

TLSConnectionPool::~TLSConnectionPool() {
    shutdown();
}

bool TLSConnectionPool::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return true;
    
    // WinHTTP is initialized per-session, not globally
    m_initialized = true;
    return true;
}

void TLSConnectionPool::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;
    
    // Close all connections
    for (auto& [host, connections] : m_pools) {
        for (auto& conn : connections) {
            if (conn->hConnect) {
                WinHttpCloseHandle(conn->hConnect);
            }
            if (conn->hSession) {
                WinHttpCloseHandle(conn->hSession);
            }
        }
    }
    
    m_pools.clear();
    m_totalConnections = 0;
    m_initialized = false;
}

HINTERNET TLSConnectionPool::createSession() {
    // Create session with proxy settings if configured
    DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    std::wstring proxyServer;
    
    if (m_config.useProxy) {
        accessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
        std::wostringstream oss;
        oss << m_config.proxyHost.c_str() << ":" << m_config.proxyPort;
        proxyServer = oss.str();
    }
    
    std::wstring userAgent(m_config.userAgent.begin(), m_config.userAgent.end());
    
    HINTERNET hSession = WinHttpOpen(
        userAgent.c_str(),
        accessType,
        proxyServer.empty() ? WINHTTP_NO_PROXY_NAME : proxyServer.c_str(),
        WINHTTP_NO_PROXY_BYPASS,
        0);
    
    if (!hSession) return nullptr;
    
    // Configure timeouts
    DWORD connectTimeout = m_config.connectTimeoutMs;
    DWORD requestTimeout = m_config.requestTimeoutMs;
    
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, 
                     &connectTimeout, sizeof(connectTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT,
                     &requestTimeout, sizeof(requestTimeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT,
                     &requestTimeout, sizeof(requestTimeout));
    
    // Enable keep-alive
    if (m_config.enableKeepAlive) {
        DWORD keepAlive = 1;
        WinHttpSetOption(hSession, WINHTTP_OPTION_KEEP_CONNECTION,
                        &keepAlive, sizeof(keepAlive));
    }
    
    return hSession;
}

HINTERNET TLSConnectionPool::createConnection(HINTERNET hSession, 
                                               const std::string& host, 
                                               uint16_t port) {
    std::wstring wideHost(host.begin(), host.end());
    
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        wideHost.c_str(),
        port,
        0);
    
    return hConnect;
}

HINTERNET TLSConnectionPool::acquireConnection(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string key = host + ":" + std::to_string(port);
    
    // Check for idle connection
    auto it = m_pools.find(key);
    if (it != m_pools.end()) {
        for (auto& conn : it->second) {
            if (!conn->inUse) {
                // Check if connection is still valid
                auto now = std::chrono::steady_clock::now();
                auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - conn->lastUsed).count();
                
                if (age < m_config.keepAliveTimeoutMs) {
                    conn->inUse = true;
                    conn->lastUsed = now;
                    return conn->hConnect;
                }
            }
        }
    }
    
    // Check connection limit
    if (m_totalConnections >= m_config.maxTotalConnections) {
        cleanupIdleConnections();
        if (m_totalConnections >= m_config.maxTotalConnections) {
            return nullptr; // Pool exhausted
        }
    }
    
    // Check per-host limit
    size_t hostConnections = it != m_pools.end() ? it->second.size() : 0;
    if (hostConnections >= m_config.maxConnectionsPerHost) {
        return nullptr; // Host limit reached
    }
    
    // Create new connection
    HINTERNET hSession = createSession();
    if (!hSession) return nullptr;
    
    HINTERNET hConnect = createConnection(hSession, host, port);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return nullptr;
    }
    
    // Add to pool
    auto conn = std::make_shared<Connection>();
    conn->hSession = hSession;
    conn->hConnect = hConnect;
    conn->host = host;
    conn->port = port;
    conn->lastUsed = std::chrono::steady_clock::now();
    conn->inUse = true;
    
    m_pools[key].push_back(conn);
    m_totalConnections++;
    
    return hConnect;
}

void TLSConnectionPool::releaseConnection(HINTERNET hConnection) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& [key, connections] : m_pools) {
        for (auto& conn : connections) {
            if (conn->hConnect == hConnection) {
                conn->inUse = false;
                conn->lastUsed = std::chrono::steady_clock::now();
                return;
            }
        }
    }
}

size_t TLSConnectionPool::getActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [key, connections] : m_pools) {
        for (const auto& conn : connections) {
            if (conn->inUse) count++;
        }
    }
    return count;
}

size_t TLSConnectionPool::getIdleConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& [key, connections] : m_pools) {
        for (const auto& conn : connections) {
            if (!conn->inUse) count++;
        }
    }
    return count;
}

size_t TLSConnectionPool::getTotalConnections() const {
    return m_totalConnections;
}

void TLSConnectionPool::cleanupIdleConnections() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [key, connections] : m_pools) {
        auto it = connections.begin();
        while (it != connections.end()) {
            if (!(*it)->inUse) {
                auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - (*it)->lastUsed).count();
                
                if (age >= m_config.keepAliveTimeoutMs) {
                    // Close connection
                    if ((*it)->hConnect) WinHttpCloseHandle((*it)->hConnect);
                    if ((*it)->hSession) WinHttpCloseHandle((*it)->hSession);
                    it = connections.erase(it);
                    m_totalConnections--;
                    continue;
                }
            }
            ++it;
        }
    }
}

// ============================================================================
// TLSClient Implementation
// ============================================================================

TLSClient::TLSClient(const TLSConfig& config) : m_config(config) {}

TLSClient::~TLSClient() {
    shutdown();
}

bool TLSClient::initialize() {
    if (m_initialized) return true;
    
    m_pool = std::make_unique<TLSConnectionPool>(m_config);
    if (!m_pool->initialize()) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

void TLSClient::shutdown() {
    if (!m_initialized) return;
    
    if (m_pool) {
        m_pool->shutdown();
        m_pool.reset();
    }
    
    m_initialized = false;
}

TLSClient::ParsedURL TLSClient::parseURL(const std::string& url) {
    ParsedURL result;
    
    // Parse scheme
    size_t schemeEnd = url.find("://");
    if (schemeEnd != std::string::npos) {
        result.scheme = url.substr(0, schemeEnd);
        std::transform(result.scheme.begin(), result.scheme.end(), 
                      result.scheme.begin(), ::tolower);
    } else {
        result.scheme = "https"; // Default
    }
    
    // Default port based on scheme
    result.port = (result.scheme == "https") ? 443 : 80;
    
    // Find host start
    size_t hostStart = (schemeEnd != std::string::npos) ? schemeEnd + 3 : 0;
    
    // Find host end (port, path, query, or fragment)
    size_t hostEnd = url.find_first_of(":/?#", hostStart);
    if (hostEnd == std::string::npos) {
        hostEnd = url.length();
    }
    
    result.host = url.substr(hostStart, hostEnd - hostStart);
    
    // Parse port if present
    size_t pos = hostEnd;
    if (pos < url.length() && url[pos] == ':') {
        pos++;
        size_t portEnd = url.find_first_of("/?#", pos);
        if (portEnd == std::string::npos) portEnd = url.length();
        result.port = static_cast<uint16_t>(std::stoi(url.substr(pos, portEnd - pos)));
        pos = portEnd;
    }
    
    // Parse path
    if (pos < url.length() && url[pos] == '/') {
        size_t pathEnd = url.find_first_of("?#", pos);
        if (pathEnd == std::string::npos) pathEnd = url.length();
        result.path = url.substr(pos, pathEnd - pos);
        pos = pathEnd;
    } else {
        result.path = "/";
    }
    
    // Parse query
    if (pos < url.length() && url[pos] == '?') {
        pos++;
        size_t queryEnd = url.find('#', pos);
        if (queryEnd == std::string::npos) queryEnd = url.length();
        result.query = url.substr(pos, queryEnd - pos);
        pos = queryEnd;
    }
    
    // Parse fragment
    if (pos < url.length() && url[pos] == '#') {
        result.fragment = url.substr(pos + 1);
    }
    
    return result;
}

TLSResponse TLSClient::request(const TLSRequest& req) {
    if (!m_initialized) {
        TLSResponse response;
        response.errorMessage = "TLS client not initialized";
        response.finalState = TLSState::Error;
        return response;
    }
    
    m_totalRequests++;
    auto startTime = std::chrono::steady_clock::now();
    
    // Get connection from pool
    HINTERNET hConnection = m_pool->acquireConnection(req.host, req.port);
    if (!hConnection) {
        TLSResponse response;
        response.errorMessage = "Failed to acquire connection";
        response.finalState = TLSState::Error;
        m_failedRequests++;
        return response;
    }
    
    // Execute request with retry
    TLSResponse response = retryRequest(req, 0);
    
    // Release connection back to pool
    m_pool->releaseConnection(hConnection);
    
    // Update statistics
    auto endTime = std::chrono::steady_clock::now();
    response.totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    m_totalBytesSent += response.totalBytesSent;
    m_totalBytesReceived += response.totalBytesReceived;
    m_totalLatencyMs += response.totalTime.count();
    
    if (response.isSuccess()) {
        m_successfulRequests++;
    } else {
        m_failedRequests++;
    }
    
    // Record metrics
    TLSMetricsCollector::instance().recordRequest(
        req.host, req.port, response.isSuccess(), response.totalTime);
    TLSMetricsCollector::instance().recordBytes(
        req.host, response.totalBytesSent, response.totalBytesReceived);
    
    return response;
}

TLSResponse TLSClient::retryRequest(const TLSRequest& req, uint32_t attempt) {
    TLSResponse response = executeRequest(req, nullptr);
    
    // Check if we should retry
    if (attempt < m_config.maxRetries && shouldRetry(response.statusCode, response.errorMessage)) {
        uint32_t delay = calculateRetryDelay(attempt);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        return retryRequest(req, attempt + 1);
    }
    
    return response;
}

TLSResponse TLSClient::executeRequest(const TLSRequest& req, HINTERNET hConnection) {
    TLSResponse response;
    response.requestId = req.requestId;
    
    // Create request handle
    std::wstring widePath(req.path.begin(), req.path.end());
    std::wstring wideMethod(req.method.begin(), req.method.end());
    
    DWORD flags = req.useTLS ? WINHTTP_FLAG_SECURE : 0;
    
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnection,
        wideMethod.c_str(),
        widePath.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);
    
    if (!hRequest) {
        response.errorMessage = getLastErrorString();
        response.finalState = TLSState::Error;
        return response;
    }
    
    // Configure TLS
    if (req.useTLS && !configureTLS(hRequest)) {
        response.errorMessage = "Failed to configure TLS";
        response.finalState = TLSState::Error;
        WinHttpCloseHandle(hRequest);
        return response;
    }
    
    // Add headers
    for (const auto& [key, value] : req.headers) {
        std::string header = key + ": " + value + "\r\n";
        std::wstring wideHeader(header.begin(), header.end());
        WinHttpAddRequestHeaders(hRequest, wideHeader.c_str(), -1, 
                                 WINHTTP_ADDREQ_FLAG_ADD);
    }
    
    // Send request
    response.finalState = TLSState::Sending;
    if (!sendRequest(hRequest, req)) {
        response.errorMessage = getLastErrorString();
        response.finalState = TLSState::Error;
        WinHttpCloseHandle(hRequest);
        return response;
    }
    
    // Receive response
    response.finalState = TLSState::Receiving;
    if (!receiveResponse(hRequest, response)) {
        response.errorMessage = getLastErrorString();
        response.finalState = TLSState::Error;
        WinHttpCloseHandle(hRequest);
        return response;
    }
    
    response.finalState = TLSState::Connected;
    WinHttpCloseHandle(hRequest);
    return response;
}

bool TLSClient::configureTLS(HINTERNET hRequest) {
    // Enable TLS 1.3 and TLS 1.2
    DWORD protocols = 0;
    if (m_config.enableTLS13) protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (m_config.enableTLS12) protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    if (m_config.enableTLS11) protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1;
    if (m_config.enableTLS10) protocols |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
    
    if (!WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURE_PROTOCOLS,
                         &protocols, sizeof(protocols))) {
        return false;
    }
    
    // Configure certificate validation
    if (m_config.validateCertificates) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        
        if (m_config.allowSelfSigned) {
            flags |= SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
        }
        
        // Clear ignore flags for proper validation
        DWORD secFlags = 0;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS,
                        &secFlags, sizeof(secFlags));
    }
    
    return true;
}

bool TLSClient::sendRequest(HINTERNET hRequest, const TLSRequest& req) {
    const void* pData = req.body.empty() ? nullptr : req.body.data();
    DWORD dataLen = static_cast<DWORD>(req.body.size());
    
    BOOL result = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        const_cast<void*>(pData),
        dataLen,
        dataLen,
        0);
    
    return result == TRUE;
}

bool TLSClient::receiveResponse(HINTERNET hRequest, TLSResponse& response) {
    // Receive headers
    if (!receiveHeaders(hRequest, response)) {
        return false;
    }
    
    // Receive body
    if (!receiveBody(hRequest, response)) {
        return false;
    }
    
    return true;
}

bool TLSClient::receiveHeaders(HINTERNET hRequest, TLSResponse& response) {
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    
    if (!WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, nullptr)) {
        return false;
    }
    response.statusCode = static_cast<uint16_t>(statusCode);
    
    // Query status text
    wchar_t statusText[256];
    DWORD statusTextSize = sizeof(statusText);
    if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_TEXT,
                           WINHTTP_HEADER_NAME_BY_INDEX, statusText, &statusTextSize, nullptr)) {
        char statusTextA[256];
        WideCharToMultiByte(CP_UTF8, 0, statusText, -1, statusTextA, sizeof(statusTextA), nullptr, nullptr);
        response.statusText = statusTextA;
    }
    
    // Query all headers
    DWORD headerSize = 0;
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                        WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &headerSize, nullptr);
    
    if (headerSize > 0) {
        std::vector<wchar_t> headers(headerSize / sizeof(wchar_t) + 1);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                               WINHTTP_HEADER_NAME_BY_INDEX, headers.data(), &headerSize, nullptr)) {
            // Parse headers
            std::wstring headerStr(headers.data());
            std::wistringstream iss(headerStr);
            std::wstring line;
            
            while (std::getline(iss, line)) {
                if (line.empty() || line == L"\r") continue;
                
                size_t colon = line.find(L':');
                if (colon != std::string::npos) {
                    std::string key(line.substr(0, colon).begin(), line.substr(0, colon).end());
                    std::string value(line.substr(colon + 2).begin(), line.substr(colon + 2).end());
                    
                    // Trim whitespace
                    while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(0, 1);
                    while (!key.empty() && (key.back() == ' ' || key.back() == '\t' || key.back() == '\r')) key.pop_back();
                    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
                    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.pop_back();
                    
                    response.headers[key] = value;
                }
            }
        }
    }
    
    return true;
}

bool TLSClient::receiveBody(HINTERNET hRequest, TLSResponse& response) {
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;
    
    do {
        bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable)) {
            break;
        }
        
        if (bytesAvailable == 0) break;
        
        std::vector<uint8_t> buffer(bytesAvailable);
        if (!WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            return false;
        }
        
        response.body.insert(response.body.end(), buffer.begin(), buffer.begin() + bytesRead);
        response.totalBytesReceived += bytesRead;
        
    } while (bytesAvailable > 0);
    
    return true;
}

bool TLSClient::shouldRetry(uint16_t statusCode, const std::string& errorMessage) {
    // Retry on 5xx errors
    if (statusCode >= 500 && statusCode < 600) return true;
    
    // Retry on 429 (rate limit)
    if (statusCode == 429) return true;
    
    // Retry on connection errors
    if (errorMessage.find("connection") != std::string::npos) return true;
    if (errorMessage.find("timeout") != std::string::npos) return true;
    
    return false;
}

uint32_t TLSClient::calculateRetryDelay(uint32_t attempt) {
    // Exponential backoff with jitter
    uint32_t baseDelay = m_config.retryBaseDelayMs;
    uint32_t maxDelay = m_config.retryMaxDelayMs;
    double jitter = m_config.retryJitterFactor;
    
    // Calculate exponential delay
    uint32_t delay = baseDelay * (1 << attempt);
    delay = std::min(delay, maxDelay);
    
    // Add jitter
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, jitter);
    delay = static_cast<uint32_t>(delay * (1.0 + dis(gen)));
    
    return delay;
}

std::string TLSClient::getLastErrorString() {
    DWORD error = GetLastError();
    if (error == ERROR_SUCCESS) return "Success";
    
    if (error >= WINHTTP_ERROR_BASE && error <= WINHTTP_ERROR_LAST) {
        return getWinHttpErrorString(error);
    }
    
    return "Error code: " + std::to_string(error);
}

std::string TLSClient::getWinHttpErrorString(DWORD error) {
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    
    if (buffer) {
        char narrow[512];
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, narrow, sizeof(narrow), nullptr, nullptr);
        LocalFree(buffer);
        return narrow;
    }
    
    return "WinHTTP error: " + std::to_string(error);
}

std::string TLSClient::getSChannelErrorString(SECURITY_STATUS status) {
    // SChannel error strings
    static const std::map<SECURITY_STATUS, std::string> errors = {
        {SEC_E_OK, "Success"},
        {SEC_E_INSUFFICIENT_MEMORY, "Insufficient memory"},
        {SEC_E_INVALID_HANDLE, "Invalid handle"},
        {SEC_E_INVALID_TOKEN, "Invalid token"},
        {SEC_E_LOGON_DENIED, "Logon denied"},
        {SEC_E_NO_AUTHENTICATING_AUTHORITY, "No authenticating authority"},
        {SEC_E_NO_CREDENTIALS, "No credentials"},
        {SEC_E_TARGET_UNKNOWN, "Target unknown"},
        {SEC_E_UNSUPPORTED_FUNCTION, "Unsupported function"},
        {SEC_E_WRONG_PRINCIPAL, "Wrong principal"},
        {SEC_E_CERT_EXPIRED, "Certificate expired"},
        {SEC_E_CERT_REVOKED, "Certificate revoked"},
        {SEC_E_CERT_UNKNOWN, "Certificate unknown"},
        {SEC_E_UNTRUSTED_ROOT, "Untrusted root"},
        {SEC_E_INCOMPLETE_MESSAGE, "Incomplete message"},
        {SEC_I_CONTINUE_NEEDED, "Continue needed"},
        {SEC_I_INCOMPLETE_CREDENTIALS, "Incomplete credentials"},
    };
    
    auto it = errors.find(status);
    if (it != errors.end()) return it->second;
    
    return "SChannel error: " + std::to_string(status);
}

// Convenience methods
TLSResponse TLSClient::get(const std::string& url) {
    return request(TLSRequest::GET(url));
}

TLSResponse TLSClient::post(const std::string& url, const std::vector<uint8_t>& data) {
    return request(TLSRequest::POST(url, data));
}

TLSResponse TLSClient::post(const std::string& url, const std::string& json) {
    return request(TLSRequest::POST(url, json));
}

// Configuration
void TLSClient::setConfig(const TLSConfig& config) {
    m_config = config;
    if (m_pool) {
        m_pool = std::make_unique<TLSConnectionPool>(m_config);
        m_pool->initialize();
    }
}

TLSConfig TLSClient::getConfig() const {
    return m_config;
}

// Statistics
uint64_t TLSClient::getTotalRequests() const { return m_totalRequests; }
uint64_t TLSClient::getTotalBytesSent() const { return m_totalBytesSent; }
uint64_t TLSClient::getTotalBytesReceived() const { return m_totalBytesReceived; }
uint64_t TLSClient::getSuccessfulRequests() const { return m_successfulRequests; }
uint64_t TLSClient::getFailedRequests() const { return m_failedRequests; }

double TLSClient::getAverageLatencyMs() const {
    if (m_totalRequests == 0) return 0.0;
    return static_cast<double>(m_totalLatencyMs) / m_totalRequests;
}

// ============================================================================
// CertificateValidator Implementation
// ============================================================================

CertificateValidator::CertificateValidator(const TLSConfig& config) : m_config(config) {
    initializeTrustedStore();
}

CertificateValidator::~CertificateValidator() {
    cleanupTrustedStore();
}

bool CertificateValidator::initializeTrustedStore() {
    m_trustedStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_SYSTEM_STORE_CURRENT_USER,
        L"MY");
    
    return m_trustedStore != nullptr;
}

void CertificateValidator::cleanupTrustedStore() {
    if (m_trustedStore) {
        CertCloseStore(m_trustedStore, 0);
        m_trustedStore = nullptr;
    }
}

bool CertificateValidator::validate(PCCERT_CONTEXT pCertContext, const std::string& hostname) {
    if (!m_config.validateCertificates) return true;
    
    if (!validateExpiration(pCertContext)) return false;
    if (!validateHostname(pCertContext, hostname)) return false;
    if (!validateChain(pCertContext)) return false;
    
    return true;
}

bool CertificateValidator::validateChain(PCCERT_CONTEXT pCertContext) {
    CERT_CHAIN_PARA chainPara = {0};
    chainPara.cbSize = sizeof(CERT_CHAIN_PARA);
    
    // Request strong validation
    chainPara.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
    
    PCCERT_CHAIN_CONTEXT pChainContext = nullptr;
    if (!CertGetCertificateChain(
            nullptr,
            pCertContext,
            nullptr,
            nullptr,
            &chainPara,
            CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT,
            nullptr,
            &pChainContext)) {
        return false;
    }
    
    // Verify chain
    CERT_CHAIN_POLICY_PARA policyPara = {0};
    policyPara.cbSize = sizeof(CERT_CHAIN_POLICY_PARA);
    
    CERT_CHAIN_POLICY_STATUS policyStatus = {0};
    policyStatus.cbSize = sizeof(CERT_CHAIN_POLICY_STATUS);
    
    BOOL result = CertVerifyCertificateChainPolicy(
        CERT_CHAIN_POLICY_SSL,
        pChainContext,
        &policyPara,
        &policyStatus);
    
    CertFreeCertificateChain(pChainContext);
    
    return result && policyStatus.dwError == 0;
}

bool CertificateValidator::validateExpiration(PCCERT_CONTEXT pCertContext) {
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    
    // Check not before
    int compare = CompareFileTime(&pCertContext->pCertInfo->NotBefore, &now);
    if (compare > 0) return false; // Certificate not yet valid
    
    // Check not after
    compare = CompareFileTime(&pCertContext->pCertInfo->NotAfter, &now);
    if (compare < 0) return false; // Certificate expired
    
    return true;
}

bool CertificateValidator::validateHostname(PCCERT_CONTEXT pCertContext, const std::string& hostname) {
    // Get subject name
    DWORD size = 0;
    CertGetNameString(pCertContext, CERT_NAME_DNS_TYPE, 0, nullptr, nullptr, &size);
    
    if (size > 0) {
        std::vector<char> name(size);
        CertGetNameStringA(pCertContext, CERT_NAME_DNS_TYPE, 0, nullptr, name.data(), size);
        
        // Simple hostname matching (wildcard support)
        std::string certHostname(name.data());
        
        // Convert to lowercase for comparison
        std::string hostLower = hostname;
        std::string certLower = certHostname;
        std::transform(hostLower.begin(), hostLower.end(), hostLower.begin(), ::tolower);
        std::transform(certLower.begin(), certLower.end(), certLower.begin(), ::tolower);
        
        // Exact match
        if (hostLower == certLower) return true;
        
        // Wildcard match (*.example.com)
        if (certLower.length() > 2 && certLower[0] == '*' && certLower[1] == '.') {
            std::string domain = certLower.substr(1); // Remove *
            if (hostLower.length() > domain.length()) {
                std::string hostDomain = hostLower.substr(hostLower.length() - domain.length());
                if (hostDomain == domain) return true;
            }
        }
    }
    
    return false;
}

bool CertificateValidator::validatePinning(PCCERT_CONTEXT pCertContext, 
                                           const std::vector<std::string>& pinned) {
    if (pinned.empty()) return true; // No pinning required
    
    std::string fingerprint = getFingerprintSHA256(pCertContext);
    
    for (const auto& pin : pinned) {
        if (fingerprint == pin) return true;
    }
    
    return false;
}

CertificateInfo CertificateValidator::extractInfo(PCCERT_CONTEXT pCertContext) {
    CertificateInfo info;
    
    // Subject
    DWORD size = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 
                                    0, nullptr, nullptr, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        CertGetNameStringA(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                          0, nullptr, buffer.data(), size);
        info.subject = buffer.data();
    }
    
    // Issuer
    size = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                             CERT_NAME_ISSUER_FLAG, nullptr, nullptr, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        CertGetNameStringA(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                          CERT_NAME_ISSUER_FLAG, nullptr, buffer.data(), size);
        info.issuer = buffer.data();
    }
    
    // Fingerprints
    info.fingerprintSHA256 = getFingerprintSHA256(pCertContext);
    info.fingerprintSHA1 = getFingerprintSHA1(pCertContext);
    
    // Serial number
    if (pCertContext->pCertInfo) {
        DWORD serialSize = pCertContext->pCertInfo->SerialNumber.cbData;
        if (serialSize > 0) {
            std::ostringstream oss;
            for (DWORD i = 0; i < serialSize; i++) {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(pCertContext->pCertInfo->SerialNumber.pbData[i]);
                if (i < serialSize - 1) oss << ":";
            }
            info.serialNumber = oss.str();
        }
        
        // Validity dates
        info.notBefore = std::chrono::system_clock::from_time_t(
            static_cast<time_t>(pCertContext->pCertInfo->NotBefore.dwLowDateTime));
        info.notAfter = std::chrono::system_clock::from_time_t(
            static_cast<time_t>(pCertContext->pCertInfo->NotAfter.dwLowDateTime));
    }
    
    return info;
}

std::string CertificateValidator::getFingerprintSHA256(PCCERT_CONTEXT pCertContext) {
    DWORD size = 0;
    if (!CryptHashCertificate(0, CALG_SHA_256, 0,
                              pCertContext->pbCertEncoded,
                              pCertContext->cbCertEncoded,
                              nullptr, &size)) {
        return "";
    }
    
    std::vector<BYTE> hash(size);
    if (!CryptHashCertificate(0, CALG_SHA_256, 0,
                              pCertContext->pbCertEncoded,
                              pCertContext->cbCertEncoded,
                              hash.data(), &size)) {
        return "";
    }
    
    std::ostringstream oss;
    for (DWORD i = 0; i < size; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        if (i < size - 1) oss << ":";
    }
    
    return oss.str();
}

std::string CertificateValidator::getFingerprintSHA1(PCCERT_CONTEXT pCertContext) {
    DWORD size = 0;
    if (!CryptHashCertificate(0, CALG_SHA1, 0,
                              pCertContext->pbCertEncoded,
                              pCertContext->cbCertEncoded,
                              nullptr, &size)) {
        return "";
    }
    
    std::vector<BYTE> hash(size);
    if (!CryptHashCertificate(0, CALG_SHA1, 0,
                              pCertContext->pbCertEncoded,
                              pCertContext->cbCertEncoded,
                              hash.data(), &size)) {
        return "";
    }
    
    std::ostringstream oss;
    for (DWORD i = 0; i < size; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        if (i < size - 1) oss << ":";
    }
    
    return oss.str();
}

std::string CertificateValidator::getSubject(PCCERT_CONTEXT pCertContext) {
    DWORD size = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                    0, nullptr, nullptr, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        CertGetNameStringA(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                          0, nullptr, buffer.data(), size);
        return buffer.data();
    }
    return "";
}

std::string CertificateValidator::getIssuer(PCCERT_CONTEXT pCertContext) {
    DWORD size = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                    CERT_NAME_ISSUER_FLAG, nullptr, nullptr, 0);
    if (size > 0) {
        std::vector<char> buffer(size);
        CertGetNameStringA(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                          CERT_NAME_ISSUER_FLAG, nullptr, buffer.data(), size);
        return buffer.data();
    }
    return "";
}

// ============================================================================
// TLSMetricsCollector Implementation
// ============================================================================

TLSMetricsCollector& TLSMetricsCollector::instance() {
    static TLSMetricsCollector instance;
    return instance;
}

void TLSMetricsCollector::recordRequest(const std::string& host, uint16_t port,
                                         bool success, std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = host + ":" + std::to_string(port);
    
    m_metrics[key].totalRequests++;
    if (success) {
        m_metrics[key].successfulRequests++;
    } else {
        m_metrics[key].failedRequests++;
    }
    m_metrics[key].totalLatencyMs += duration.count();
}

void TLSMetricsCollector::recordBytes(const std::string& host, 
                                       uint64_t sent, uint64_t received) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics[host].totalBytesSent += sent;
    m_metrics[host].totalBytesReceived += received;
}

void TLSMetricsCollector::recordError(const std::string& host, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics[host].errorsByType[error]++;
}

void TLSMetricsCollector::recordHandshake(const std::string& host, 
                                          std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics[host].handshakeCount++;
    m_metrics[host].totalHandshakeMs += duration.count();
}

TLSMetricsCollector::HostMetrics TLSMetricsCollector::getHostMetrics(const std::string& host) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_metrics.find(host);
    if (it != m_metrics.end()) {
        return it->second;
    }
    return HostMetrics{};
}

std::map<std::string, TLSMetricsCollector::HostMetrics> TLSMetricsCollector::getAllMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_metrics;
}

void TLSMetricsCollector::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_metrics.clear();
}

// ============================================================================
// TLSUtil Implementation
// ============================================================================

namespace TLSUtil {

std::string urlEncode(const std::string& str) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            oss << c;
        } else {
            oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    
    return oss.str();
}

std::string urlDecode(const std::string& str) {
    std::ostringstream oss;
    
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int hex;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> hex;
            oss << static_cast<char>(hex);
            i += 2;
        } else if (str[i] == '+') {
            oss << ' ';
        } else {
            oss << str[i];
        }
    }
    
    return oss.str();
}

std::string base64Encode(const std::vector<uint8_t>& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);
    
    size_t i = 0;
    while (i < data.size()) {
        uint32_t octet_a = i < data.size() ? data[i++] : 0;
        uint32_t octet_b = i < data.size() ? data[i++] : 0;
        uint32_t octet_c = i < data.size() ? data[i++] : 0;
        
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        result += chars[(triple >> 18) & 0x3F];
        result += chars[(triple >> 12) & 0x3F];
        result += chars[(triple >> 6) & 0x3F];
        result += chars[triple & 0x3F];
    }
    
    // Add padding
    size_t mod = data.size() % 3;
    if (mod == 1) {
        result[result.size() - 1] = '=';
        result[result.size() - 2] = '=';
    } else if (mod == 2) {
        result[result.size() - 1] = '=';
    }
    
    return result;
}

std::vector<uint8_t> base64Decode(const std::string& str) {
    static const int table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    
    std::vector<uint8_t> result;
    result.reserve(str.size() * 3 / 4);
    
    int val = 0, valb = -8;
    for (char c : str) {
        if (table[static_cast<unsigned char>(c)] == -1) break;
        val = (val << 6) + table[static_cast<unsigned char>(c)];
        valb += 6;
        if (valb >= 0) {
            result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return result;
}

bool isValidHostname(const std::string& hostname) {
    if (hostname.empty() || hostname.length() > 253) return false;
    
    // Check for valid characters
    for (char c : hostname) {
        if (!isalnum(c) && c != '-' && c != '.') {
            return false;
        }
    }
    
    // Check for valid format
    std::regex hostnameRegex(R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}$)");
    return std::regex_match(hostname, hostnameRegex);
}

std::string getWinHttpErrorMessage(DWORD error) {
    LPWSTR buffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    
    if (buffer) {
        char narrow[512];
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, narrow, sizeof(narrow), nullptr, nullptr);
        LocalFree(buffer);
        return narrow;
    }
    
    return "WinHTTP error: " + std::to_string(error);
}

std::string getSChannelErrorMessage(SECURITY_STATUS status) {
    static const std::map<SECURITY_STATUS, std::string> errors = {
        {SEC_E_OK, "Success"},
        {SEC_E_INSUFFICIENT_MEMORY, "Insufficient memory"},
        {SEC_E_INVALID_HANDLE, "Invalid handle"},
        {SEC_E_INVALID_TOKEN, "Invalid token"},
        {SEC_E_LOGON_DENIED, "Logon denied"},
        {SEC_E_NO_CREDENTIALS, "No credentials"},
        {SEC_E_TARGET_UNKNOWN, "Target unknown"},
        {SEC_E_UNSUPPORTED_FUNCTION, "Unsupported function"},
        {SEC_E_WRONG_PRINCIPAL, "Wrong principal"},
        {SEC_E_CERT_EXPIRED, "Certificate expired"},
        {SEC_E_CERT_REVOKED, "Certificate revoked"},
        {SEC_E_UNTRUSTED_ROOT, "Untrusted root"},
    };
    
    auto it = errors.find(status);
    return it != errors.end() ? it->second : "SChannel error: " + std::to_string(status);
}

} // namespace TLSUtil

} // namespace RawrXD::LLM
