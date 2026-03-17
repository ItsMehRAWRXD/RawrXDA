#include "streaming_engine.h"
#include <chrono>
#include <sstream>
#include <algorithm>

// StreamingEngine Implementation
StreamingEngine::StreamingEngine(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics,
    std::shared_ptr<ResponseParser> parser
) : m_logger(logger), m_metrics(metrics), m_parser(parser) {
    if (m_logger) {
        m_logger->info("StreamingEngine", "Initialized");
    }
}

void StreamingEngine::startStream(
    std::function<void(const ParsedCompletion&)> onCompletion,
    std::function<void(const std::string&)> onError,
    std::function<void()> onStreamEnd
) {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    
    m_onCompletion = onCompletion;
    m_onError = onError;
    m_onStreamEnd = onStreamEnd;
    
    m_streamStartTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    
    m_firstChunkTimeMs = 0;
    m_sequenceCounter = 0;
    m_totalTokensReceived = 0;
    
    if (m_logger) {
        m_logger->info("StreamingEngine", "Stream started");
    }
}

void StreamingEngine::feedChunk(const std::string& chunk) {
    if (chunk.empty()) return;

    // Record time to first chunk
    if (m_firstChunkTimeMs == 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();
        m_firstChunkTimeMs = now - m_streamStartTimeMs;
        
        if (m_metrics) {
            m_metrics->recordHistogram("streaming_ttfc_ms", static_cast<double>(m_firstChunkTimeMs));
        }
    }

    // Wait for buffer space if needed (backpressure)
    while (getBufferDepth() >= m_maxBufferSize) {
        if (!waitForBufferSpace(100)) {
            if (m_logger) {
                m_logger->warn("StreamingEngine", "Buffer backpressure timeout");
            }
            break;
        }
    }

    // Create stream chunk
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();

    StreamChunk streamChunk;
    streamChunk.data = chunk;
    streamChunk.sequenceNumber = m_sequenceCounter++;
    streamChunk.arrivedAtMs = now - m_streamStartTimeMs;
    streamChunk.isComplete = false;

    // Add to buffer
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_chunkBuffer.push(streamChunk);
    }

    // Notify waiting threads
    m_bufferCV.notify_one();

    // Process chunk immediately
    processChunk(streamChunk);
}

void StreamingEngine::endStream() {
    {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        if (!m_chunkBuffer.empty()) {
            auto lastChunk = m_chunkBuffer.back();
            lastChunk.isComplete = true;
        }
    }

    if (m_parser) {
        auto finalCompletions = m_parser->flush();
        for (const auto& completion : finalCompletions) {
            if (m_onCompletion) {
                m_onCompletion(completion);
            }
            m_totalTokensReceived += completion.tokenCount;
        }
    }

    // Record final metrics
    auto endTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    int64_t totalStreamMs = endTime - m_streamStartTimeMs;

    if (m_metrics) {
        m_metrics->recordHistogram("streaming_total_time_ms", static_cast<double>(totalStreamMs));
        m_metrics->recordHistogram("streaming_total_tokens", static_cast<double>(m_totalTokensReceived));
        m_metrics->incrementCounter("streaming_sessions_completed", 1);
    }

    if (m_logger) {
        m_logger->info("StreamingEngine", 
            "Stream ended: " + std::to_string(totalStreamMs) + "ms, " +
            std::to_string(m_totalTokensReceived) + " tokens, " +
            std::to_string(m_sequenceCounter) + " chunks");
    }

    if (m_onStreamEnd) {
        m_onStreamEnd();
    }
}

StreamMetrics StreamingEngine::getMetrics() const {
    StreamMetrics metrics;
    metrics.timeToFirstChunkMs = m_firstChunkTimeMs;
    metrics.totalChunks = m_sequenceCounter;
    metrics.totalTokens = m_totalTokensReceived;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    metrics.totalStreamTimeMs = now - m_streamStartTimeMs;

    if (metrics.totalStreamTimeMs > 0 && m_totalTokensReceived > 0) {
        metrics.throughputTokensPerSec = (m_totalTokensReceived * 1000.0) / metrics.totalStreamTimeMs;
    }

    return metrics;
}

bool StreamingEngine::isStreaming() const {
    return m_streamStartTimeMs > 0;
}

bool StreamingEngine::waitForBufferSpace(int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_bufferMutex);
    return m_bufferCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this]() { return m_chunkBuffer.size() < m_maxBufferSize; });
}

size_t StreamingEngine::getBufferDepth() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_bufferMutex));
    return m_chunkBuffer.size();
}

void StreamingEngine::reset() {
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    while (!m_chunkBuffer.empty()) {
        m_chunkBuffer.pop();
    }
    m_streamStartTimeMs = 0;
    m_firstChunkTimeMs = 0;
    m_sequenceCounter = 0;
    m_totalTokensReceived = 0;
}

void StreamingEngine::processChunk(const StreamChunk& chunk) {
    if (!m_parser) return;

    // Parse chunk using response parser to detect boundaries
    auto parsed = m_parser->parseChunk(chunk.data);
    
    for (const auto& completion : parsed) {
        if (m_onCompletion) {
            m_onCompletion(completion);
        }
        m_totalTokensReceived += completion.tokenCount;
        
        if (m_metrics) {
            m_metrics->recordHistogram("streaming_chunk_tokens", static_cast<double>(completion.tokenCount));
        }
    }
}

void StreamingEngine::handleStreamError(const std::string& error) {
    if (m_logger) {
        m_logger->error("StreamingEngine", "Stream error: " + error);
    }

    if (m_onError) {
        m_onError(error);
    }

    if (m_metrics) {
        m_metrics->incrementCounter("streaming_errors", 1);
    }
}

// HTTPStreamingClient Implementation
HTTPStreamingClient::HTTPStreamingClient(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics,
    std::shared_ptr<StreamingEngine> streamingEngine
) : m_logger(logger), m_metrics(metrics), m_streamingEngine(streamingEngine) {
    if (m_logger) {
        m_logger->info("HTTPStreamingClient", "Initialized");
    }
}

bool HTTPStreamingClient::openStream(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body
) {
    if (m_logger) {
        m_logger->info("HTTPStreamingClient", "Opening stream to: " + url);
    }

    if (!setupConnection(url)) {
        m_lastError = "Failed to setup connection";
        return false;
    }

    m_isConnected = true;

    // Start reading chunks in background thread
    std::thread([this]() {
        try {
            if (!readChunkedResponse()) {
                m_streamingEngine->endStream();
            }
        } catch (const std::exception& e) {
            m_streamingEngine->handleStreamError(std::string("Read exception: ") + e.what());
            m_streamingEngine->endStream();
        }
        m_isConnected = false;
    }).detach();

    return true;
}

void HTTPStreamingClient::closeStream() {
    if (m_logger) {
        m_logger->info("HTTPStreamingClient", "Closing stream");
    }
    m_isConnected = false;
}

void HTTPStreamingClient::setConnectionTimeout(int timeoutMs) {
    if (m_logger) {
        m_logger->debug("HTTPStreamingClient", "Set timeout to " + std::to_string(timeoutMs) + "ms");
    }
}

bool HTTPStreamingClient::setupConnection(const std::string& url) {
    if (m_logger) {
        m_logger->debug("HTTPStreamingClient", "Setting up connection");
    }
    // Real implementation would use socket/libcurl here
    return true;
}

bool HTTPStreamingClient::readChunkedResponse() {
    if (m_logger) {
        m_logger->debug("HTTPStreamingClient", "Reading chunked response");
    }
    
    // Simulate receiving chunks
    // In real implementation, this would read from socket
    for (int i = 0; i < 5; i++) {
        std::string chunk = "simulated chunk " + std::to_string(i) + "\n";
        m_streamingEngine->feedChunk(chunk);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return true;
}

std::string HTTPStreamingClient::parseChunkSize(const std::string& line) {
    // Parse chunk size from chunked transfer encoding
    // Format: [size in hex] CRLF
    size_t pos = line.find(';');
    if (pos != std::string::npos) {
        return line.substr(0, pos);
    }
    return line;
}

// StreamingResponseBuilder Implementation
StreamingResponseBuilder::StreamingResponseBuilder() {
    m_startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

void StreamingResponseBuilder::addCompletion(const ParsedCompletion& completion) {
    m_accumulator += completion.text;
    m_completions.push_back(completion);
    m_totalTokens += completion.tokenCount;
}

std::vector<ParsedCompletion> StreamingResponseBuilder::getCompletions() const {
    return m_completions;
}

std::string StreamingResponseBuilder::getFullText() const {
    return m_accumulator;
}

int64_t StreamingResponseBuilder::getElapsedMs() const {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    return now - m_startTimeMs;
}

void StreamingResponseBuilder::reset() {
    m_accumulator.clear();
    m_completions.clear();
    m_totalTokens = 0;
    m_startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}
