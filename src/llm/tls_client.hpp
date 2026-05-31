/**
 * @file tls_client.hpp
 * @brief Production TLS 1.3 Client using WinHTTP/SChannel
 * 
 * Provides secure HTTPS connections for LLM API calls with:
 * - TLS 1.3 support via SChannel
 * - Certificate validation and pinning
 * - Connection pooling with keep-alive
 * - Async request/response handling
 * - Retry with exponential backoff
 * 
 * @author RawrXD Team
 * @version 1.0.0
 */

#pragma once

#include <windows.h>
#include <winhttp.h>
#include <schannel.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <queue>
#include <condition_variable>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "secur32.lib")

namespace RawrXD::LLM {

// ============================================================================
// TLS Configuration
// ============================================================================

struct TLSConfig {
    // Connection settings
    std::string userAgent = "RawrXD-IDE/1.0";
    uint32_t connectTimeoutMs = 30000;      // 30 seconds
    uint32_t requestTimeoutMs = 120000;     // 2 minutes
    uint32_t responseTimeoutMs = 300000;    // 5 minutes
    
    // TLS settings
    bool enableTLS13 = true;
    bool enableTLS12 = true;
    bool enableTLS11 = false;                // Deprecated
    bool enableTLS10 = false;                // Deprecated
    
    // Certificate validation
    bool validateCertificates = true;
    bool allowSelfSigned = false;           // For development only
    std::vector<std::string> pinnedCertificates; // SHA-256 fingerprints
    
    // Connection pooling
    uint32_t maxConnectionsPerHost = 10;
    uint32_t maxTotalConnections = 100;
    uint32_t keepAliveTimeoutMs = 60000;    // 1 minute
    bool enableKeepAlive = true;
    
    // Retry settings
    uint32_t maxRetries = 3;
    uint32_t retryBaseDelayMs = 1000;       // 1 second
    double retryJitterFactor = 0.5;         // Add 0-50% jitter
    uint32_t retryMaxDelayMs = 30000;        // 30 seconds max
    
    // Proxy settings
    bool useProxy = false;
    std::string proxyHost;
    uint16_t proxyPort = 8080;
    std::string proxyUsername;
    std::string proxyPassword;
    bool proxyBypassLocal = true;
};

// ============================================================================
// TLS Certificate Info
// ============================================================================

struct CertificateInfo {
    std::string subject;
    std::string issuer;
    std::string serialNumber;
    std::string fingerprintSHA256;
    std::string fingerprintSHA1;
    std::chrono::system_clock::time_point notBefore;
    std::chrono::system_clock::time_point notAfter;
    bool isValid = false;
    bool isTrusted = false;
    bool isPinned = false;
    std::string validationError;
};

// ============================================================================
// TLS Connection State
// ============================================================================

enum class TLSState {
    Disconnected,
    Connecting,
    Handshaking,
    Connected,
    Sending,
    Receiving,
    Error,
    Closed
};

// ============================================================================
// TLS Request
// ============================================================================

struct TLSRequest {
    std::string method = "GET";
    std::string host;
    uint16_t port = 443;
    std::string path = "/";
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    bool useTLS = true;
    uint32_t timeoutMs = 0;                  // 0 = use config default
    std::string requestId;                   // For tracking
    
    // Convenience constructors
    static TLSRequest GET(const std::string& url);
    static TLSRequest POST(const std::string& url, const std::vector<uint8_t>& data);
    static TLSRequest POST(const std::string& url, const std::string& json);
};

// ============================================================================
// TLS Response
// ============================================================================

struct TLSResponse {
    uint16_t statusCode = 0;
    std::string statusText;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;
    std::string requestId;
    CertificateInfo certificate;
    TLSState finalState = TLSState::Disconnected;
    std::string errorMessage;
    uint64_t totalBytesReceived = 0;
    uint64_t totalBytesSent = 0;
    std::chrono::milliseconds totalTime{0};
    std::chrono::milliseconds connectTime{0};
    std::chrono::milliseconds tlsHandshakeTime{0};
    std::chrono::milliseconds responseTime{0};
    
    // Convenience methods
    std::string bodyAsString() const;
    bool isSuccess() const { return statusCode >= 200 && statusCode < 300; }
    bool isRedirect() const { return statusCode >= 300 && statusCode < 400; }
    bool isClientError() const { return statusCode >= 400 && statusCode < 500; }
    bool isServerError() const { return statusCode >= 500; }
};

// ============================================================================
// TLS Connection Pool
// ============================================================================

class TLSConnectionPool {
public:
    explicit TLSConnectionPool(const TLSConfig& config);
    ~TLSConnectionPool();
    
    // Connection management
    bool initialize();
    void shutdown();
    
    // Get connection from pool
    HINTERNET acquireConnection(const std::string& host, uint16_t port);
    void releaseConnection(HINTERNET hConnection);
    
    // Pool statistics
    size_t getActiveConnections() const;
    size_t getIdleConnections() const;
    size_t getTotalConnections() const;
    
private:
    struct Connection {
        HINTERNET hSession = nullptr;
        HINTERNET hConnect = nullptr;
        std::string host;
        uint16_t port = 443;
        std::chrono::steady_clock::time_point lastUsed;
        bool inUse = false;
    };
    
    TLSConfig m_config;
    mutable std::mutex m_mutex;
    std::map<std::string, std::vector<std::shared_ptr<Connection>>> m_pools;
    std::atomic<bool> m_initialized{false};
    std::atomic<size_t> m_totalConnections{0};
    
    HINTERNET createSession();
    HINTERNET createConnection(HINTERNET hSession, const std::string& host, uint16_t port);
    void cleanupIdleConnections();
};

// ============================================================================
// TLS Client
// ============================================================================

class TLSClient {
public:
    explicit TLSClient(const TLSConfig& config = TLSConfig{});
    ~TLSClient();
    
    // Lifecycle
    bool initialize();
    void shutdown();
    
    // Synchronous requests
    TLSResponse request(const TLSRequest& req);
    TLSResponse get(const std::string& url);
    TLSResponse post(const std::string& url, const std::vector<uint8_t>& data);
    TLSResponse post(const std::string& url, const std::string& json);
    
    // Asynchronous requests
    using ResponseCallback = std::function<void(const TLSResponse& response)>;
    using ProgressCallback = std::function<void(uint64_t bytesTransferred, uint64_t totalBytes)>;
    
    void requestAsync(const TLSRequest& req, ResponseCallback callback, ProgressCallback progress = nullptr);
    void getAsync(const std::string& url, ResponseCallback callback);
    void postAsync(const std::string& url, const std::vector<uint8_t>& data, ResponseCallback callback);
    
    // Streaming requests (for SSE/streaming APIs)
    using ChunkCallback = std::function<void(const std::vector<uint8_t>& chunk, bool isFinal)>;
    void requestStreaming(const TLSRequest& req, ChunkCallback chunkCallback, ProgressCallback progress = nullptr);
    
    // Certificate management
    CertificateInfo getServerCertificate(const std::string& host, uint16_t port = 443);
    bool pinCertificate(const std::string& host, const std::string& fingerprintSHA256);
    bool unpinCertificate(const std::string& host);
    std::vector<CertificateInfo> getTrustedCertificates();
    
    // Configuration
    void setConfig(const TLSConfig& config);
    TLSConfig getConfig() const;
    
    // Statistics
    uint64_t getTotalRequests() const;
    uint64_t getTotalBytesSent() const;
    uint64_t getTotalBytesReceived() const;
    uint64_t getSuccessfulRequests() const;
    uint64_t getFailedRequests() const;
    double getAverageLatencyMs() const;
    
private:
    TLSConfig m_config;
    std::unique_ptr<TLSConnectionPool> m_pool;
    std::atomic<bool> m_initialized{false};
    
    // Statistics
    std::atomic<uint64_t> m_totalRequests{0};
    std::atomic<uint64_t> m_totalBytesSent{0};
    std::atomic<uint64_t> m_totalBytesReceived{0};
    std::atomic<uint64_t> m_successfulRequests{0};
    std::atomic<uint64_t> m_failedRequests{0};
    std::atomic<uint64_t> m_totalLatencyMs{0};
    
    // Certificate pinning
    mutable std::mutex m_certMutex;
    std::map<std::string, std::vector<std::string>> m_pinnedCerts;
    
    // Internal helpers
    TLSResponse executeRequest(const TLSRequest& req, HINTERNET hConnection);
    bool configureTLS(HINTERNET hRequest);
    bool validateCertificate(HINTERNET hRequest, const std::string& host);
    bool performHandshake(HINTERNET hRequest);
    bool sendRequest(HINTERNET hRequest, const TLSRequest& req);
    bool receiveResponse(HINTERNET hRequest, TLSResponse& response);
    bool receiveHeaders(HINTERNET hRequest, TLSResponse& response);
    bool receiveBody(HINTERNET hRequest, TLSResponse& response);
    
    // URL parsing
    struct ParsedURL {
        std::string scheme;
        std::string host;
        uint16_t port;
        std::string path;
        std::string query;
        std::string fragment;
    };
    static ParsedURL parseURL(const std::string& url);
    
    // Retry logic
    TLSResponse retryRequest(const TLSRequest& req, uint32_t attempt);
    bool shouldRetry(uint16_t statusCode, const std::string& errorMessage);
    uint32_t calculateRetryDelay(uint32_t attempt);
    
    // Error handling
    std::string getLastErrorString();
    std::string getWinHttpErrorString(DWORD error);
    std::string getSChannelErrorString(SECURITY_STATUS status);
};

// ============================================================================
// TLS Certificate Validator
// ============================================================================

class CertificateValidator {
public:
    explicit CertificateValidator(const TLSConfig& config);
    ~CertificateValidator();
    
    // Validation
    bool validate(PCCERT_CONTEXT pCertContext, const std::string& hostname);
    bool validateChain(PCCERT_CONTEXT pCertContext);
    bool validateExpiration(PCCERT_CONTEXT pCertContext);
    bool validateHostname(PCCERT_CONTEXT pCertContext, const std::string& hostname);
    bool validatePinning(PCCERT_CONTEXT pCertContext, const std::vector<std::string>& pinned);
    
    // Certificate info extraction
    CertificateInfo extractInfo(PCCERT_CONTEXT pCertContext);
    std::string getFingerprintSHA256(PCCERT_CONTEXT pCertContext);
    std::string getFingerprintSHA1(PCCERT_CONTEXT pCertContext);
    std::string getSubject(PCCERT_CONTEXT pCertContext);
    std::string getIssuer(PCCERT_CONTEXT pCertContext);
    
private:
    TLSConfig m_config;
    
    // Certificate store for chain validation
    HCERTSTORE m_trustedStore = nullptr;
    bool initializeTrustedStore();
    void cleanupTrustedStore();
};

// ============================================================================
// TLS Handshake Handler
// ============================================================================

class TLSHandshakeHandler {
public:
    TLSHandshakeHandler();
    ~TLSHandshakeHandler();
    
    // Perform TLS handshake
    bool performHandshake(HINTERNET hRequest, const std::string& hostname);
    
    // Get negotiated protocol
    std::string getNegotiatedProtocol() const;
    
    // Get negotiated cipher suite
    std::string getNegotiatedCipherSuite() const;
    
    // Get peer certificate
    CertificateInfo getPeerCertificate() const;
    
private:
    std::string m_protocol;
    std::string m_cipherSuite;
    CertificateInfo m_peerCert;
    
    // SChannel context
    CredHandle m_credentials;
    CtxtHandle m_context;
    bool m_credentialsInitialized = false;
    
    bool acquireCredentials();
    bool initializeSecurityContext(const std::string& hostname);
    bool completeHandshake(HINTERNET hRequest);
    SecPkgContext_ConnectionInfo getConnectionInfo();
};

// ============================================================================
// TLS Stream Handler (for SSE/Streaming)
// ============================================================================

class TLSStreamHandler {
public:
    TLSStreamHandler();
    ~TLSStreamHandler();
    
    // Open streaming connection
    bool open(const std::string& url, const std::map<std::string, std::string>& headers);
    
    // Read chunk
    std::vector<uint8_t> readChunk();
    bool hasMoreData() const;
    
    // Close connection
    void close();
    
    // Get response headers
    std::map<std::string, std::string> getHeaders() const;
    uint16_t getStatusCode() const;
    
private:
    HINTERNET m_hSession = nullptr;
    HINTERNET m_hConnect = nullptr;
    HINTERNET m_hRequest = nullptr;
    bool m_isOpen = false;
    uint16_t m_statusCode = 0;
    std::map<std::string, std::string> m_headers;
    
    // Buffer for partial reads
    std::vector<uint8_t> m_buffer;
    size_t m_bufferPos = 0;
};

// ============================================================================
// TLS Metrics Collector
// ============================================================================

class TLSMetricsCollector {
public:
    static TLSMetricsCollector& instance();
    
    // Record metrics
    void recordRequest(const std::string& host, uint16_t port, bool success, 
                       std::chrono::milliseconds duration);
    void recordBytes(const std::string& host, uint64_t sent, uint64_t received);
    void recordError(const std::string& host, const std::string& error);
    void recordHandshake(const std::string& host, std::chrono::milliseconds duration);
    
    // Get metrics
    struct HostMetrics {
        uint64_t totalRequests = 0;
        uint64_t successfulRequests = 0;
        uint64_t failedRequests = 0;
        uint64_t totalBytesSent = 0;
        uint64_t totalBytesReceived = 0;
        uint64_t totalLatencyMs = 0;
        uint64_t handshakeCount = 0;
        uint64_t totalHandshakeMs = 0;
        std::map<std::string, uint64_t> errorsByType;
    };
    
    HostMetrics getHostMetrics(const std::string& host) const;
    std::map<std::string, HostMetrics> getAllMetrics() const;
    
    // Reset metrics
    void reset();
    
private:
    TLSMetricsCollector() = default;
    mutable std::mutex m_mutex;
    std::map<std::string, HostMetrics> m_metrics;
};

// ============================================================================
// Utility Functions
// ============================================================================

namespace TLSUtil {
    // URL encoding/decoding
    std::string urlEncode(const std::string& str);
    std::string urlDecode(const std::string& str);
    
    // Base64 encoding/decoding
    std::string base64Encode(const std::vector<uint8_t>& data);
    std::vector<uint8_t> base64Decode(const std::string& str);
    
    // Certificate fingerprint calculation
    std::string calculateFingerprintSHA256(PCCERT_CONTEXT pCertContext);
    std::string calculateFingerprintSHA1(PCCERT_CONTEXT pCertContext);
    
    // Hostname validation
    bool isValidHostname(const std::string& hostname);
    bool matchesCertificate(PCCERT_CONTEXT pCertContext, const std::string& hostname);
    
    // Protocol detection
    bool supportsTLS13(HINTERNET hRequest);
    bool supportsTLS12(HINTERNET hRequest);
    std::string getTLSVersion(HINTERNET hRequest);
    
    // Cipher suite info
    std::string getCipherSuiteName(DWORD cipherSuite);
    int getCipherSuiteStrength(DWORD cipherSuite);
    
    // Error handling
    std::string getWinHttpErrorMessage(DWORD error);
    std::string getSChannelErrorMessage(SECURITY_STATUS status);
}

} // namespace RawrXD::LLM
