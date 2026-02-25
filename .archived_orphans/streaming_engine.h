#pragma once
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
/* Qt removed */
#include <atomic>
#include "logger.h"
#include "metrics.h"
#include "response_parser.h"

struct StreamChunk {
    std::string data;
    long long sequenceNumber = 0;
    long long arrivedAtMs = 0;
    bool isComplete = false;
};

struct StreamMetrics {
    long long timeToFirstChunkMs = 0;
    long long totalChunks = 0;
    long long totalTokens = 0;
};

class StreamingEngine {
public:
    StreamingEngine(
        std::shared_ptr<Logger> logger,
        std::shared_ptr<Metrics> metrics,
        std::shared_ptr<ResponseParser> parser
    );

    void startStream(
        std::function<void(const ParsedCompletion&)> onCompletion,
        std::function<void(const std::string&)> onError,
        std::function<void()> onStreamEnd
    );

    void feedChunk(const std::string& chunk);
    void endStream();
    StreamMetrics getMetrics() const;

private:
    std::shared_ptr<Logger> m_logger;
    std::shared_ptr<Metrics> m_metrics;
    std::shared_ptr<ResponseParser> m_parser;

    std::mutex m_bufferMutex;
    std::condition_variable m_bufferCV;
    std::queue<StreamChunk> m_chunkBuffer;
    size_t m_maxBufferSize = 1000;

    std::function<void(const ParsedCompletion&)> m_onCompletion;
    std::function<void(const std::string&)> m_onError;
    std::function<void()> m_onStreamEnd;

    long long m_streamStartTimeMs = 0;
    long long m_firstChunkTimeMs = 0;
    long long m_sequenceCounter = 0;
    long long m_totalTokensReceived = 0;

    void processChunk(const StreamChunk& chunk);
    size_t getBufferDepth() const;
    bool waitForBufferSpace(int timeoutMs);
};
