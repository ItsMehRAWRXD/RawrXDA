#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

#include "logging/logger.h"
#include "metrics/metrics.h"
#include "response_parser.h"

/**
 * StreamingEngine: Production-grade streaming support
 * 
 * Handles:
 * - HTTP streaming with chunked transfer encoding
 * - Real-time chunk buffering and backpressure
 * - Integration with ResponseParser for boundary detection
 * - Token-level streaming with confidence scoring
 * - Latency tracking and metrics
 */

struct StreamChunk {
    std::string data;
    int sequenceNumber;
    int64_t arrivedAtMs;
    bool isComplete;
};

struct StreamMetrics {
    int64_t timeToFirstChunkMs = 0;
    int64_t totalStreamTimeMs = 0;
    int totalChunks = 0;
    int totalTokens = 0;
    double avgChunkSizeBytes = 0.0;
    double throughputTokensPerSec = 0.0;
};

class StreamingEngine {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<ResponseParser> m_parser;

    // Streaming state
    std::queue<StreamChunk> m_chunkBuffer;
    std::mutex m_bufferMutex;
    std::condition_variable m_bufferCV;
    
    // Metrics
    int64_t m_streamStartTimeMs = 0;
    int64_t m_firstChunkTimeMs = 0;
    int m_sequenceCounter = 0;
    int m_totalTokensReceived = 0;

    // Configuration
    size_t m_maxBufferSize = 10;  // Max chunks in buffer before backpressure
    int m_chunkTimeoutMs = 5000;  // Timeout waiting for chunks

    // Streaming callbacks
    std::function<void(const ParsedCompletion&)> m_onCompletion;
    std::function<void(const std::string&)> m_onError;
    std::function<void()> m_onStreamEnd;

public:
    StreamingEngine(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics,
        std::shared_ptr<ResponseParser> parser
    );

    /**
     * Start a new streaming session
     * @param onCompletion Called when a completion boundary is detected
     * @param onError Called on stream error
     * @param onStreamEnd Called when stream ends
     */
    void startStream(
        std::function<void(const ParsedCompletion&)> onCompletion,
        std::function<void(const std::string&)> onError,
        std::function<void()> onStreamEnd
    );

    /**
     * Feed data chunk into the stream
     * Called by HTTP client as chunks arrive
     * @param chunk Raw data from network
     */
    void feedChunk(const std::string& chunk);

    /**
     * Indicate end of stream
     * Flushes any remaining buffered data
     */
    void endStream();

    /**
     * Get current streaming metrics
     * @return Metrics including latency, throughput, etc.
     */
    StreamMetrics getMetrics() const;

    /**
     * Set maximum buffer size before backpressure
     * @param size Maximum chunks to buffer
     */
    void setMaxBufferSize(size_t size) { m_maxBufferSize = size; }

    /**
     * Check if stream is active
     * @return True if streaming in progress
     */
    bool isStreaming() const;

    /**
     * Wait for buffer to have space (backpressure handling)
     * @param timeoutMs Timeout in milliseconds
     * @return True if space available, false if timeout
     */
    bool waitForBufferSpace(int timeoutMs);

    /**
     * Get buffer depth
     * @return Current number of chunks in buffer
     */
    size_t getBufferDepth() const;

    /**
     * Reset stream state for new session
     */
    void reset();

    /**
     * Handle stream error and notify callbacks
     * @param error Error message
     */
    void handleStreamError(const std::string& error);

private:
    void processChunk(const StreamChunk& chunk);
    ParsedCompletion parseStreamChunk(const std::string& data);
};

/**
 * HTTP Streaming Client - handles connection, reading, and chunk delivery
 */
class HTTPStreamingClient {
private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<StreamingEngine> m_streamingEngine;

    // Connection state
    bool m_isConnected = false;
    std::string m_lastError;

public:
    HTTPStreamingClient(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics,
        std::shared_ptr<StreamingEngine> streamingEngine
    );

    /**
     * Open streaming connection and start receiving
     * @param url Target URL
     * @param headers HTTP headers
     * @param body Request body (POST)
     * @return True if connection successful
     */
    bool openStream(
        const std::string& url,
        const std::vector<std::pair<std::string, std::string>>& headers,
        const std::string& body
    );

    /**
     * Close streaming connection
     */
    void closeStream();

    /**
     * Check if connection is active
     * @return True if connected and receiving
     */
    bool isConnected() const { return m_isConnected; }

    /**
     * Get last error message
     * @return Error description
     */
    std::string getLastError() const { return m_lastError; }

    /**
     * Set timeout for connection
     * @param timeoutMs Timeout in milliseconds
     */
    void setConnectionTimeout(int timeoutMs);

private:
    bool setupConnection(const std::string& url);
    bool readChunkedResponse();
    std::string parseChunkSize(const std::string& line);
};

/**
 * Streaming response builder - accumulates chunks into complete responses
 */
class StreamingResponseBuilder {
private:
    std::string m_accumulator;
    std::vector<ParsedCompletion> m_completions;
    int m_totalTokens = 0;
    int64_t m_startTimeMs = 0;

public:
    StreamingResponseBuilder();

    /**
     * Add a parsed completion from stream
     * @param completion Parsed completion chunk
     */
    void addCompletion(const ParsedCompletion& completion);

    /**
     * Get accumulated completions
     * @return Vector of all parsed completions
     */
    std::vector<ParsedCompletion> getCompletions() const;

    /**
     * Get full accumulated text
     * @return Complete response text
     */
    std::string getFullText() const;

    /**
     * Get total tokens received
     * @return Token count
     */
    int getTotalTokens() const { return m_totalTokens; }

    /**
     * Get elapsed time since start
     * @return Milliseconds
     */
    int64_t getElapsedMs() const;

    /**
     * Reset for new stream
     */
    void reset();

    /**
     * Check if we have any data
     * @return True if non-empty
     */
    bool hasData() const { return !m_accumulator.empty(); }
};
