#include "streaming_engine.h"
#include "license_enforcement.h"
#include <chrono>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

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
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::TokenStreaming, __FUNCTION__)) {
        if (onError) onError("[LICENSE] TokenStreaming feature requires Professional license or higher");
        return;
    }
    
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
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::TokenStreaming, __FUNCTION__)) {
        return;
    }
    
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
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::TokenStreaming, __FUNCTION__)) {
        return;
    }
    
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
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::TokenStreaming, __FUNCTION__)) {
        return;
    }
    
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
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::TokenStreaming, __FUNCTION__)) {
        m_lastError = "[LICENSE] TokenStreaming feature requires Professional license or higher";
        return false;
    }
    
    if (m_logger) {
        m_logger->info("HTTPStreamingClient", "Opening stream to: " + url);
    }

    m_currentUrl = url;
    m_currentHeaders = headers;
    m_currentBody = body;

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
    if (!RawrXD::Enforce::LicenseEnforcer::Instance().allow(
            RawrXD::License::FeatureID::TokenStreaming, __FUNCTION__)) {
        return;
    }
    
    if (m_logger) {
        m_logger->info("HTTPStreamingClient", "Closing stream");
    }
    m_isConnected = false;

#ifdef _WIN32
    // Close WinHTTP handles (readChunkedResponse may also close them)
    if (m_hRequest) { WinHttpCloseHandle((HINTERNET)m_hRequest); m_hRequest = nullptr; }
    if (m_hConnect) { WinHttpCloseHandle((HINTERNET)m_hConnect); m_hConnect = nullptr; }
    if (m_hSession) { WinHttpCloseHandle((HINTERNET)m_hSession); m_hSession = nullptr; }
#endif
}

void HTTPStreamingClient::setConnectionTimeout(int timeoutMs) {
    if (m_logger) {
        m_logger->debug("HTTPStreamingClient", "Set timeout to " + std::to_string(timeoutMs) + "ms");
    }
}

bool HTTPStreamingClient::setupConnection(const std::string& url) {
    if (m_logger) {
        m_logger->debug("HTTPStreamingClient", "Setting up WinHTTP connection");
    }

#ifdef _WIN32
    // Parse URL components
    URL_COMPONENTSW urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = (DWORD)-1;
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    std::wstring wUrl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        m_lastError = "Failed to parse URL";
        return false;
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = urlComp.nPort;

    // Create session
    m_hSession = WinHttpOpen(L"RawrXD-StreamingEngine/1.0",
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!m_hSession) {
        m_lastError = "WinHttpOpen failed";
        return false;
    }

    // Connect
    m_hConnect = WinHttpConnect((HINTERNET)m_hSession, host.c_str(), port, 0);
    if (!m_hConnect) {
        WinHttpCloseHandle((HINTERNET)m_hSession);
        m_hSession = nullptr;
        m_lastError = "WinHttpConnect failed";
        return false;
    }

    // Create request
    LPCWSTR verb = m_currentBody.empty() ? L"GET" : L"POST";
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    m_hRequest = WinHttpOpenRequest((HINTERNET)m_hConnect, verb, path.c_str(),
                                     nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!m_hRequest) {
        WinHttpCloseHandle((HINTERNET)m_hConnect);
        WinHttpCloseHandle((HINTERNET)m_hSession);
        m_hConnect = nullptr;
        m_hSession = nullptr;
        m_lastError = "WinHttpOpenRequest failed";
        return false;
    }

    // Add custom headers
    for (const auto& [key, value] : m_currentHeaders) {
        std::string headerLine = key + ": " + value;
        std::wstring wHeader(headerLine.begin(), headerLine.end());
        WinHttpAddRequestHeaders((HINTERNET)m_hRequest, wHeader.c_str(),
                                  (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    BOOL sendOk = WinHttpSendRequest((HINTERNET)m_hRequest,
                                      WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      m_currentBody.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)m_currentBody.c_str(),
                                      (DWORD)m_currentBody.size(),
                                      (DWORD)m_currentBody.size(), 0);
    if (!sendOk) {
        WinHttpCloseHandle((HINTERNET)m_hRequest);
        WinHttpCloseHandle((HINTERNET)m_hConnect);
        WinHttpCloseHandle((HINTERNET)m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        m_lastError = "WinHttpSendRequest failed";
        return false;
    }

    // Receive response
    if (!WinHttpReceiveResponse((HINTERNET)m_hRequest, nullptr)) {
        WinHttpCloseHandle((HINTERNET)m_hRequest);
        WinHttpCloseHandle((HINTERNET)m_hConnect);
        WinHttpCloseHandle((HINTERNET)m_hSession);
        m_hRequest = m_hConnect = m_hSession = nullptr;
        m_lastError = "WinHttpReceiveResponse failed";
        return false;
    }

    return true;
#else
    // POSIX: use raw TCP socket for HTTP streaming
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string portStr = std::to_string(m_port);
    if (getaddrinfo(m_host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        m_lastError = "DNS resolution failed for " + m_host;
        return false;
    }
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0 || connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        freeaddrinfo(res);
        m_lastError = "TCP connection failed";
        return false;
    }
    freeaddrinfo(res);

    std::string httpReq = (m_currentBody.empty() ? "GET " : "POST ") + m_currentPath + " HTTP/1.1\r\n"
        "Host: " + m_host + "\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(m_currentBody.size()) + "\r\n"
        "\r\n" + m_currentBody;
    if (::send(sock, httpReq.c_str(), httpReq.size(), 0) < 0) {
        ::close(sock);
        m_lastError = "Socket send failed";
        return false;
    }

    // Skip HTTP response headers
    std::string hdrBuf;
    char ch;
    while (::recv(sock, &ch, 1, 0) == 1) {
        hdrBuf += ch;
        if (hdrBuf.size() >= 4 && hdrBuf.substr(hdrBuf.size()-4) == "\r\n\r\n") break;
    }

    m_posixSocket = sock;
    m_isConnected = true;
    return true;
#endif
}

bool HTTPStreamingClient::readChunkedResponse() {
    if (m_logger) {
        m_logger->debug("HTTPStreamingClient", "Reading chunked response via WinHTTP");
    }

#ifdef _WIN32
    if (!m_hRequest) {
        m_lastError = "No active request handle";
        return false;
    }

    char buffer[4096];
    DWORD bytesAvailable = 0;
    DWORD bytesRead = 0;

    // Read data in chunks until stream is closed or error
    while (m_isConnected) {
        if (!WinHttpQueryDataAvailable((HINTERNET)m_hRequest, &bytesAvailable)) {
            DWORD err = GetLastError();
            if (err == ERROR_WINHTTP_CONNECTION_ERROR || err == ERROR_WINHTTP_TIMEOUT) {
                break; // Connection closed or timed out
            }
            m_streamingEngine->handleStreamError("WinHttpQueryDataAvailable failed: " + std::to_string(err));
            break;
        }

        if (bytesAvailable == 0) {
            break; // End of response
        }

        // Read available data
        DWORD toRead = std::min(bytesAvailable, (DWORD)sizeof(buffer));
        if (!WinHttpReadData((HINTERNET)m_hRequest, buffer, toRead, &bytesRead)) {
            m_streamingEngine->handleStreamError("WinHttpReadData failed");
            break;
        }

        if (bytesRead == 0) {
            break; // End of data
        }

        // Feed chunk to streaming engine
        std::string chunk(buffer, bytesRead);
        m_streamingEngine->feedChunk(chunk);
    }

    // Cleanup WinHTTP handles
    if (m_hRequest) { WinHttpCloseHandle((HINTERNET)m_hRequest); m_hRequest = nullptr; }
    if (m_hConnect) { WinHttpCloseHandle((HINTERNET)m_hConnect); m_hConnect = nullptr; }
    if (m_hSession) { WinHttpCloseHandle((HINTERNET)m_hSession); m_hSession = nullptr; }

    return true;
#else
    // POSIX: read from raw socket
    if (m_posixSocket < 0) {
        m_lastError = "No active socket";
        return false;
    }

    char buffer[4096];
    ssize_t bytesRead;
    while (m_isConnected && (bytesRead = ::recv(m_posixSocket, buffer, sizeof(buffer), 0)) > 0) {
        std::string chunk(buffer, bytesRead);
        m_streamingEngine->feedChunk(chunk);
    }

    ::close(m_posixSocket);
    m_posixSocket = -1;
    return true;
#endif
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
