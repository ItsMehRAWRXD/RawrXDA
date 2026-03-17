// ============================================================================
// Detailed Authentication Manager for LLM APIs
// ============================================================================
// Handles secure credential storage, OAuth2 token refresh, and API key rotation

#include "llm_http_client.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <openssl/rand.h>
#include <openssl/evp.h>

namespace {
    // Simple Base64 encoder (production would use robust library)
    std::string base64_encode(const std::string& input) {
        static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        
        std::string ret;
        int i = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        for (unsigned char c : input) {
            char_array_3[i++] = c;
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; i < 4; i++) {
                    ret += base64_chars[char_array_4[i]];
                }
                i = 0;
            }
        }

        if (i > 0) {
            for (int j = i; j < 3; j++) {
                char_array_3[j] = '\0';
            }

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (int j = 0; j <= i; j++) {
                ret += base64_chars[char_array_4[j]];
            }

            while (i++ < 3) {
                ret += '=';
            }
        }

        return ret;
    }
}

/**
 * AuthenticationManager: Production-ready auth handling
 * 
 * Features:
 * - Secure credential storage with encryption
 * - OAuth2 token refresh with expiry tracking
 * - API key rotation with audit logging
 * - Basic auth encoding
 * - Custom header support for proprietary APIs
 * - Credential validation and health checks
 */
class AuthenticationManager {
public:
    AuthenticationManager();
    ~AuthenticationManager() = default;

    /**
     * Load credentials from secure storage
     * @param backend Target backend
     * @param credentialPath Path to credential file
     * @return True if credentials loaded successfully
     */
    bool loadCredentials(LLMBackend backend, const std::string& credentialPath);

    /**
     * Save credentials securely
     * @param backend Target backend
     * @param credentials Credentials to save
     * @param credentialPath Path to save to
     * @return True if save successful
     */
    bool saveCredentials(
        LLMBackend backend,
        const AuthCredentials& credentials,
        const std::string& credentialPath
    );

    /**
     * Validate credentials before use
     * @param credentials Credentials to validate
     * @return True if credentials are valid
     */
    bool validateCredentials(const AuthCredentials& credentials);

    /**
     * Encrypt sensitive data
     * @param plaintext Data to encrypt
     * @return Encrypted data (hex-encoded)
     */
    std::string encryptSensitive(const std::string& plaintext);

    /**
     * Decrypt sensitive data
     * @param encrypted Encrypted data (hex-encoded)
     * @return Decrypted plaintext
     */
    std::string decryptSensitive(const std::string& encrypted);

    /**
     * Refresh OAuth2 token
     * @param credentials Current credentials with refresh token
     * @param client HTTP client for token refresh request
     * @return Updated credentials with new token
     */
    AuthCredentials refreshOAuth2Token(
        const AuthCredentials& credentials,
        LLMHttpClient& client
    );

    /**
     * Rotate API key
     * @param oldKey Old API key
     * @param backend Backend service
     * @return New API key (would call backend API)
     */
    std::string rotateAPIKey(const std::string& oldKey, LLMBackend backend);

    /**
     * Log credential usage for audit trail
     * @param backend Backend service
     * @param action Action performed (e.g., "authenticate", "refresh")
     * @param success Whether action succeeded
     */
    void logCredentialUsage(
        LLMBackend backend,
        const std::string& action,
        bool success
    );

    /**
     * Get last authentication time
     * @return Timestamp of last successful auth
     */
    int64_t getLastAuthTime(LLMBackend backend);

private:
    // Encryption context
    unsigned char m_encryptionKey[32];  // 256-bit key
    unsigned char m_iv[16];              // 128-bit IV

    // Audit log
    std::map<std::string, int64_t> m_lastAuthTime;
    std::vector<std::string> m_auditLog;

    // Helper methods
    void initializeEncryption();
    std::string generateMasterKey();
    bool encryptAES256GCM(const std::string& plaintext, std::string& ciphertext);
    bool decryptAES256GCM(const std::string& ciphertext, std::string& plaintext);
    std::string getBackendName(LLMBackend backend);
};

// ============================================================================
// Advanced Retry and Error Recovery System
// ============================================================================

/**
 * RetryPolicy: Intelligent retry with circuit breaker pattern
 * 
 * Features:
 * - Exponential backoff with jitter
 * - Circuit breaker to prevent cascading failures
 * - Adaptive retry based on error type
 * - Request deadline enforcement
 * - Error classification and recovery strategies
 */
class RetryPolicy {
public:
    enum class ErrorCategory {
        TRANSIENT,      // Temporary error, safe to retry (429, 503, timeout)
        AUTH_ERROR,     // Authentication/authorization (401, 403) - don't retry
        INVALID_REQUEST, // Bad request (400) - don't retry
        NOT_FOUND,      // 404 - don't retry unless it's a timing issue
        SERVER_ERROR,   // 500+ - retry with caution
        NETWORK_ERROR   // Connection issues - retry aggressively
    };

    enum class CircuitState {
        CLOSED,   // Operating normally
        OPEN,     // Too many failures, reject requests
        HALF_OPEN // Testing if service recovered
    };

    struct RetryConfig {
        int maxRetries = 3;
        int initialDelayMs = 100;
        int maxDelayMs = 30000;
        double backoffMultiplier = 2.0;
        bool useJitter = true;
        int deadlineMs = 60000;  // Max total time for request
        int circuitBreakerThreshold = 5;  // Failures before opening circuit
        int circuitBreakerResetMs = 60000; // Time before half-open attempt
    };

    RetryPolicy(const RetryConfig& config);

    /**
     * Classify error for retry decision
     * @param statusCode HTTP status code
     * @param error Error message
     * @return Error category
     */
    ErrorCategory classifyError(int statusCode, const std::string& error);

    /**
     * Determine if request should be retried
     * @param statusCode HTTP status code
     * @param retryCount Current retry count
     * @param errorCategory Classified error
     * @return True if should retry
     */
    bool shouldRetry(int statusCode, int retryCount, ErrorCategory errorCategory);

    /**
     * Calculate delay for next retry
     * @param retryCount Current retry count
     * @return Delay in milliseconds
     */
    int getRetryDelay(int retryCount);

    /**
     * Check if request deadline exceeded
     * @param startTimeMs Request start time
     * @return True if deadline exceeded
     */
    bool isDeadlineExceeded(int64_t startTimeMs);

    /**
     * Record request outcome for circuit breaker
     * @param success Whether request succeeded
     */
    void recordOutcome(bool success);

    /**
     * Check circuit breaker state
     * @return Current circuit state
     */
    CircuitState getCircuitState();

    /**
     * Get recommended recovery action
     * @param error Error that occurred
     * @return Recovery action (e.g., "retry", "fail", "use_fallback")
     */
    std::string getRecoveryAction(const std::string& error);

private:
    RetryConfig m_config;
    CircuitState m_circuitState = CircuitState::CLOSED;
    int m_failureCount = 0;
    int64_t m_lastFailureTime = 0;
    std::mt19937 m_randomGenerator;
};

// ============================================================================
// Request/Response Validation and Sanitization
// ============================================================================

/**
 * RequestValidator: Validates and sanitizes API requests
 * 
 * Features:
 * - JSON schema validation
 * - Parameter bounds checking
 * - Prompt injection prevention
 * - Token count estimation
 * - Malformed response detection
 */
class RequestValidator {
public:
    struct ValidationResult {
        bool valid = false;
        std::string errorMessage;
        json sanitizedRequest;
        int estimatedTokens = 0;
    };

    /**
     * Validate Ollama request
     * @param request Request to validate
     * @return Validation result
     */
    static ValidationResult validateOllamaRequest(const json& request);

    /**
     * Validate OpenAI request
     * @param request Request to validate
     * @return Validation result
     */
    static ValidationResult validateOpenAIRequest(const json& request);

    /**
     * Validate Anthropic request
     * @param request Request to validate
     * @return Validation result
     */
    static ValidationResult validateAnthropicRequest(const json& request);

    /**
     * Estimate token count for text
     * @param text Input text
     * @param backend Target backend
     * @return Estimated token count
     */
    static int estimateTokenCount(const std::string& text, LLMBackend backend);

    /**
     * Sanitize user input to prevent injection
     * @param input Raw user input
     * @return Sanitized input
     */
    static std::string sanitizeInput(const std::string& input);

    /**
     * Validate response structure
     * @param response Raw response
     * @param backend Target backend
     * @return True if response is valid
     */
    static bool validateResponse(const std::string& response, LLMBackend backend);

private:
    static const std::vector<std::string> INJECTION_PATTERNS;
    static const int AVG_TOKENS_PER_WORD;
};

// ============================================================================
// Observability and Metrics
// ============================================================================

/**
 * LLMMetrics: Detailed metrics collection for production monitoring
 * 
 * Collects:
 * - Request latency (p50, p95, p99)
 * - Token throughput
 * - Error rates by type
 * - API usage by model
 * - Cost tracking
 * - Connection pool statistics
 */
class LLMMetrics {
public:
    struct MetricsSnapshot {
        int totalRequests = 0;
        int successfulRequests = 0;
        int failedRequests = 0;
        int64_t totalLatencyMs = 0;
        int64_t totalTokensProcessed = 0;
        int activeConnections = 0;
        double errorRate = 0.0;
        int64_t timestamp = 0;

        // Percentiles
        int64_t p50LatencyMs = 0;
        int64_t p95LatencyMs = 0;
        int64_t p99LatencyMs = 0;

        // Cost tracking
        double estimatedCost = 0.0;
    };

    LLMMetrics();

    /**
     * Record request completion
     * @param backend Backend used
     * @param model Model used
     * @param latencyMs Request latency
     * @param tokenCount Tokens processed
     * @param success Whether request succeeded
     */
    void recordRequest(
        LLMBackend backend,
        const std::string& model,
        int64_t latencyMs,
        int tokenCount,
        bool success
    );

    /**
     * Get current metrics snapshot
     * @return Metrics snapshot
     */
    MetricsSnapshot getSnapshot();

    /**
     * Export metrics to Prometheus format
     * @return Prometheus-format metrics
     */
    std::string exportPrometheus();

    /**
     * Get metrics for specific backend
     * @param backend Target backend
     * @return Backend-specific metrics
     */
    json getBackendMetrics(LLMBackend backend);

    /**
     * Get estimated cost for API usage
     * @param backend Backend service
     * @param tokenCount Total tokens used
     * @return Estimated cost in USD
     */
    double calculateEstimatedCost(LLMBackend backend, int64_t tokenCount);

private:
    struct RequestMetric {
        int64_t latencyMs;
        int tokenCount;
        bool success;
        std::string model;
        int64_t timestamp;
    };

    std::deque<RequestMetric> m_metrics;
    std::mutex m_metricsMutex;
    const size_t MAX_METRICS_SIZE = 10000;

    // Pricing (USD per 1M tokens)
    std::map<std::string, double> m_pricing = {
        {"gpt-4", 0.03},
        {"gpt-3.5-turbo", 0.0005},
        {"claude-2", 0.008},
        {"claude-instant", 0.003},
        {"llama2", 0.0}  // Free
    };

    std::vector<int64_t> calculatePercentiles();
};

// ============================================================================
// Full Integration Example - Real API Connectivity Test
// ============================================================================

/**
 * Comprehensive test demonstrating full LLM API connectivity with:
 * - Ollama local inference
 * - OpenAI GPT models
 * - Anthropic Claude models
 * - Error handling and retries
 * - Streaming responses
 * - Metrics collection
 */
class LLMConnectivityTest {
public:
    struct TestResult {
        bool success = false;
        std::string backend;
        std::string model;
        std::string responseText;
        int64_t latencyMs = 0;
        int tokenCount = 0;
        std::string errorMessage;
    };

    /**
     * Test Ollama connectivity
     * @param endpoint Ollama endpoint (e.g., "http://localhost:11434")
     * @param model Model name (e.g., "llama2")
     * @param prompt Test prompt
     * @return Test result
     */
    static TestResult testOllamaConnectivity(
        const std::string& endpoint,
        const std::string& model,
        const std::string& prompt = "Respond briefly with 'Connected!'"
    );

    /**
     * Test OpenAI connectivity
     * @param apiKey OpenAI API key
     * @param model Model identifier (e.g., "gpt-4", "gpt-3.5-turbo")
     * @param prompt Test prompt
     * @return Test result
     */
    static TestResult testOpenAIConnectivity(
        const std::string& apiKey,
        const std::string& model,
        const std::string& prompt = "Respond briefly with 'Connected!'"
    );

    /**
     * Test Anthropic connectivity
     * @param apiKey Anthropic API key
     * @param model Model identifier (e.g., "claude-2")
     * @param prompt Test prompt
     * @return Test result
     */
    static TestResult testAnthropicConnectivity(
        const std::string& apiKey,
        const std::string& model,
        const std::string& prompt = "Respond briefly with 'Connected!'"
    );

    /**
     * Test streaming with real API
     * @param backend Target backend
     * @param config Backend configuration
     * @param prompt Test prompt
     * @return Test result with full response
     */
    static TestResult testStreamingResponse(
        LLMBackend backend,
        const HTTPConfig& config,
        const AuthCredentials& credentials,
        const std::string& model,
        const std::string& prompt
    );

    /**
     * Test error recovery
     * @param backend Target backend
     * @param invalidRequest Request that will fail
     * @return Error handling result
     */
    static TestResult testErrorRecovery(
        LLMBackend backend,
        const APIRequest& invalidRequest
    );

    /**
     * Run comprehensive connectivity test suite
     * @param backends Backends to test
     * @param configs Configuration for each backend
     * @return Results for all tests
     */
    static std::vector<TestResult> runFullTestSuite(
        const std::vector<LLMBackend>& backends,
        const std::map<LLMBackend, HTTPConfig>& configs
    );
};

#endif // LLM_PRODUCTION_UTILITIES_H
