#include "streaming_engine.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

// Helper for string conversion
static std::wstring s2ws(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    if (len == 0) return L"";
    std::wstring buf(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &buf[0], len);
    return buf;
}

// StreamingEngine Implementation
StreamingEngine::StreamingEngine(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics,
    std::shared_ptr<ResponseParser> parser
) : m_logger(logger), m_metrics(metrics), m_parser(parser) {
    if (m_logger) {

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

    }
}

bool HTTPStreamingClient::openStream(
    const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body
) {
    if (m_logger) {
        // Log start
    }

    // We implement the FULL WinHttp stack here to avoid "simulated" logic
    // and to bypass the previous stubbed methods.
    
    m_isConnected = true;

    // Start reading chunks in background thread
    std::thread([this, url, headers, body]() {
        HINTERNET hSession = NULL;
        HINTERNET hConnect = NULL;
        HINTERNET hRequest = NULL;
        bool success = false;

        try {
            // 1. Crack URL
            URL_COMPONENTS urlComp;
            ZeroMemory(&urlComp, sizeof(urlComp));
            urlComp.dwStructSize = sizeof(urlComp);
            urlComp.dwSchemeLength    = (DWORD)-1;
            urlComp.dwHostNameLength  = (DWORD)-1;
            urlComp.dwUrlPathLength   = (DWORD)-1;
            urlComp.dwExtraInfoLength = (DWORD)-1;

            std::wstring wUrl = s2ws(url);
            if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
                throw std::runtime_error("Invalid URL");
            }

            // 2. Open Session
            hSession = WinHttpOpen(L"RawrXD-AgenticIDE/1.0",  
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, 
                                   WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hSession) throw std::runtime_error("WinHttpOpen failed");

            // 3. Connect
            std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
            hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
            if (!hConnect) throw std::runtime_error("WinHttpConnect failed");

            // 4. Open Request
            std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
            hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath.c_str(),
                                          NULL, WINHTTP_NO_REFERER, 
                                          WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                          (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
            if (!hRequest) throw std::runtime_error("WinHttpOpenRequest failed");

            // 5. Add Headers
            std::wstring headersStr = L"Content-Type: application/json\r\n";
            for(const auto& h : headers) {
                headersStr += s2ws(h.first) + L": " + s2ws(h.second) + L"\r\n";
            }
            WinHttpAddRequestHeaders(hRequest, headersStr.c_str(), (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

            // 6. Send Request
            if (!WinHttpSendRequest(hRequest, 
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    (LPVOID)body.c_str(), (DWORD)body.length(), 
                                    (DWORD)body.length(), 0)) {
                throw std::runtime_error("WinHttpSendRequest failed");
            }

            if (!WinHttpReceiveResponse(hRequest, NULL)) {
                throw std::runtime_error("WinHttpReceiveResponse failed");
            }

            // 7. Read Data Stream
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                    break; 
                }
                if (dwSize == 0) break; // End of stream

                std::vector<char> buffer(dwSize + 1);
                if (WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded)) {
                    buffer[dwDownloaded] = '\0';
                    std::string chunk(buffer.data(), dwDownloaded);
                    m_streamingEngine->feedChunk(chunk);
                } else {
                    break;
                }
            } while (dwSize > 0 && m_isConnected);

            m_streamingEngine->endStream();
            success = true;

        } catch (const std::exception& e) {
            m_streamingEngine->handleStreamError(std::string("Stream exception: ") + e.what());
            m_streamingEngine->endStream();
        }

        if (hRequest) WinHttpCloseHandle(hRequest);
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
        
        m_isConnected = false;
    }).detach();

    return true;
}

void HTTPStreamingClient::closeStream() {
    if (m_logger) {

    }
    m_isConnected = false;
}

void HTTPStreamingClient::setConnectionTimeout(int timeoutMs) {
    if (m_logger) {

    }
}

bool HTTPStreamingClient::setupConnection(const std::string& url) {
    // Deprecated: implemented directly in openStream thread
    return true;
}

bool HTTPStreamingClient::readChunkedResponse() {
    // Deprecated / Unused in favor of WinHttp async thread in openStream
    return false;
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
