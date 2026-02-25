#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "logging/logger.h"
#include "metrics/metrics.h"

/**
 * LibraryIntegration: Unified interface for external libraries
 * 
 * Supported libraries:
 * - libcurl: HTTP requests (Ollama, HuggingFace API, etc.)
 * - zstd: Compression/decompression for model files
 * - nlohmann/json: JSON parsing and generation
 */

// ============================================================================
// CURL Integration (HTTP Requests)
// ============================================================================

struct HTTPRequest {
    std::string method;        // GET, POST, etc.
    std::string url;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    int timeoutSeconds = 30;
};

struct HTTPResponse {
    int statusCode;
    std::string body;
    std::vector<std::pair<std::string, std::string>> headers;
    bool success;
    std::string errorMessage;
};

class HTTPClient {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

public:
    HTTPClient(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    /**
     * Send HTTP request
     * @param request HTTP request details
     * @return Response with status code and body
     */
    HTTPResponse sendRequest(const HTTPRequest& request);

    /**
     * Convenience method: GET request
     * @param url Target URL
     * @return Response
     */
    HTTPResponse get(const std::string& url);

    /**
     * Convenience method: POST request with JSON
     * @param url Target URL
     * @param jsonBody JSON request body
     * @return Response
     */
    HTTPResponse postJSON(const std::string& url, const std::string& jsonBody);

    /**
     * Stream response with callback
     * @param request HTTP request
     * @param callback Function called for each chunk received
     * @return True if successful
     */
    bool streamRequest(
        const HTTPRequest& request,
        std::function<void(const std::string& chunk)> callback
    );

    /**
     * Download file
     * @param url File URL
     * @param outputPath Local file path
     * @return True if successful
     */
    bool downloadFile(const std::string& url, const std::string& outputPath);

    /**
     * Get metrics about HTTP client
     * @return Vector of metric names and values
     */
    std::vector<std::pair<std::string, double>> getMetrics() const;
};

// ============================================================================
// ZSTD Integration (Compression)
// ============================================================================

class CompressionHandler {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;

public:
    CompressionHandler(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    /**
     * Compress data using Zstandard
     * @param data Input data
     * @param compressionLevel 1-22 (higher = better compression, slower)
     * @return Compressed data
     */
    std::vector<uint8_t> compress(
        const std::vector<uint8_t>& data,
        int compressionLevel = 3
    );

    /**
     * Decompress Zstandard data
     * @param compressedData Compressed data
     * @return Decompressed data
     */
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressedData);

    /**
     * Compress file
     * @param inputPath Input file path
     * @param outputPath Output compressed file
     * @return True if successful
     */
    bool compressFile(const std::string& inputPath, const std::string& outputPath);

    /**
     * Decompress file
     * @param inputPath Input compressed file
     * @param outputPath Output file path
     * @return True if successful
     */
    bool decompressFile(const std::string& inputPath, const std::string& outputPath);

    /**
     * Get compression statistics
     * @return Map of metric names to values
     */
    std::vector<std::pair<std::string, double>> getStatistics() const;

private:
    // Statistics tracking
    size_t m_totalCompressed = 0;
    size_t m_totalDecompressed = 0;
    size_t m_compressionSaved = 0;
};

// ============================================================================
// JSON Integration (nlohmann/json)
// ============================================================================

class JSONHandler {
private:
    std::shared_ptr<Logger> m_logger;

public:
    JSONHandler(std::shared_ptr<Logger> logger);

    /**
     * Parse JSON string
     * @param jsonString JSON text
     * @return Parsed object (simplified representation)
     */
    bool parseJSON(const std::string& jsonString);

    /**
     * Generate JSON from map
     * @param data Key-value pairs
     * @return JSON string
     */
    std::string generateJSON(const std::vector<std::pair<std::string, std::string>>& data);

    /**
     * Extract value from JSON
     * @param jsonString JSON text
     * @param key Field to extract
     * @return Field value or empty string
     */
    std::string extractValue(const std::string& jsonString, const std::string& key);

    /**
     * Validate JSON structure
     * @param jsonString JSON text
     * @return True if valid JSON
     */
    bool validateJSON(const std::string& jsonString);

    /**
     * Pretty-print JSON
     * @param jsonString Raw JSON
     * @return Formatted JSON with indentation
     */
    std::string prettyPrint(const std::string& jsonString);

    /**
     * Minify JSON
     * @param jsonString Raw JSON
     * @return Compact JSON
     */
    std::string minify(const std::string& jsonString);
};

// ============================================================================
// Unified Library Manager
// ============================================================================

class LibraryIntegration {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<HTTPClient> m_httpClient;
    std::shared_ptr<CompressionHandler> m_compressionHandler;
    std::shared_ptr<JSONHandler> m_jsonHandler;

public:
    LibraryIntegration(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics
    );

    /**
     * Get HTTP client instance
     */
    std::shared_ptr<HTTPClient> getHTTPClient() { return m_httpClient; }

    /**
     * Get compression handler instance
     */
    std::shared_ptr<CompressionHandler> getCompressionHandler() { return m_compressionHandler; }

    /**
     * Get JSON handler instance
     */
    std::shared_ptr<JSONHandler> getJSONHandler() { return m_jsonHandler; }

    /**
     * Check if library is available/linked
     * @param libraryName Name of library (curl, zstd, json)
     * @return True if library is available
     */
    static bool isLibraryAvailable(const std::string& libraryName);

    /**
     * Get library version information
     * @param libraryName Name of library
     * @return Version string
     */
    static std::string getLibraryVersion(const std::string& libraryName);

    /**
     * Initialize all libraries
     * @return True if initialization successful
     */
    bool initializeAll();

    /**
     * Get status of all integrated libraries
     * @return Human-readable status string
     */
    std::string getStatus() const;
};
