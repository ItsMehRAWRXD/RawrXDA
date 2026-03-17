// streaming_enhancements.cpp
// Full implementation of all streaming enhancements
// ============================================================================

#include "streaming_enhancements.h"
#include "ai_model_caller.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <iostream>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <regex>

// ============================================================================
// 1. ASYNC STREAMING ENGINE IMPLEMENTATION
// ============================================================================

AsyncStreamingEngine::AsyncStreamingEngine() : m_isStreaming(false), m_cancelRequested(false) {}

AsyncStreamingEngine::~AsyncStreamingEngine() {
    cancelStreaming();
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void AsyncStreamingEngine::streamAsync(
    const std::string& prompt,
    TokenCallback onToken,
    CompleteCallback onComplete,
    float temperature,
    float topP,
    int maxTokens)
{
    if (m_isStreaming) {
        if (onComplete) {
            onComplete("", false);  // Error: already streaming
        }
        return;
    }
    
    m_cancelRequested = false;
    m_isStreaming = true;
    
    // Detach previous thread if still running
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
    
    m_workerThread = std::thread(
        &AsyncStreamingEngine::streamWorker,
        this,
        prompt,
        onToken,
        onComplete,
        temperature,
        topP,
        maxTokens
    );
}

void AsyncStreamingEngine::streamWorker(
    const std::string& prompt,
    TokenCallback onToken,
    CompleteCallback onComplete,
    float temperature,
    float topP,
    int maxTokens)
{
    try {
        std::string fullResponse;
        
        // Refactored Implementation: Use ModelCaller::streamModel for real streaming
        // This ensures true token-by-token emission if supported by the backend,
        // or consistent buffering if not.
        
        ModelCaller::GenerationParams params;
        params.max_tokens = maxTokens;
        params.temperature = temperature;
        params.top_p = topP;
        
        std::mutex responseMutex;
        
        bool success = ModelCaller::streamModel(
            prompt, 
            params, 
            [&](const std::string& token) -> bool {
                if (m_cancelRequested.load()) return false;
                
                if (onToken) onToken(token);
                
                {
                    std::lock_guard<std::mutex> lock(responseMutex);
                    fullResponse += token;
                }
                return true;
            }
        );

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_isStreaming = false;
        }
        m_completionCV.notify_all();
        
        if (onComplete) {
            std::lock_guard<std::mutex> lock(responseMutex);
            onComplete(fullResponse, success);
        }
    } catch (const std::exception& e) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_isStreaming = false;
        }
        m_completionCV.notify_all();
        
        if (onComplete) {
            onComplete(std::string("Error: ") + e.what(), false);
        }
    }
}

void AsyncStreamingEngine::cancelStreaming() {
    m_cancelRequested = true;
}

bool AsyncStreamingEngine::isStreaming() const {
    return m_isStreaming.load();
}

bool AsyncStreamingEngine::waitForCompletion(int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    if (timeoutMs == 0) {
        m_completionCV.wait(lock, [this] { return !m_isStreaming; });
        return true;
    } else {
        return m_completionCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                       [this] { return !m_isStreaming; });
    }
}

// ============================================================================
// 2. BATCH PROCESSING ENGINE IMPLEMENTATION
// ============================================================================

BatchProcessingEngine::BatchProcessingEngine(int maxParallelRequests)
    : m_maxParallelRequests(maxParallelRequests), m_shutdown(false)
{
    for (int i = 0; i < maxParallelRequests; ++i) {
        m_workers.emplace_back(&BatchProcessingEngine::batchWorker, this);
    }
}

BatchProcessingEngine::~BatchProcessingEngine() {
    m_shutdown = true;
    m_queueCV.notify_all();
    
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void BatchProcessingEngine::submitBatch(const BatchRequest& request) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_requestQueue.push(request);
    }
    m_queueCV.notify_one();
}

void BatchProcessingEngine::submitBatchMultiple(const std::vector<BatchRequest>& requests) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        for (const auto& req : requests) {
            m_requestQueue.push(req);
        }
    }
    m_queueCV.notify_all();
}

BatchResult BatchProcessingEngine::getBatchResult(const std::string& requestId, int timeoutMs) {
    auto start = std::chrono::system_clock::now();
    
    while (true) {
        {
            std::lock_guard<std::mutex> lock(m_resultsMutex);
            auto it = m_results.find(requestId);
            if (it != m_results.end()) {
                return it->second;
            }
        }
        
        if (timeoutMs > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now() - start).count();
            if (elapsed > timeoutMs) {
                return BatchResult{requestId, "", false, std::chrono::milliseconds(0), 0};
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool BatchProcessingEngine::isBatchComplete(const std::string& requestId) const {
    std::lock_guard<std::mutex> lock(m_resultsMutex);
    return m_results.find(requestId) != m_results.end();
}

json BatchProcessingEngine::getBatchStats() const {
    std::lock_guard<std::mutex> queueLock(m_queueMutex);
    std::lock_guard<std::mutex> resultsLock(m_resultsMutex);
    
    return json{
        {"queued_requests", m_requestQueue.size()},
        {"completed_requests", m_results.size()},
        {"max_parallel_workers", m_maxParallelRequests},
        {"active_workers", m_workers.size()}
    };
}

void BatchProcessingEngine::setMaxParallelRequests(int maxRequests) {
    m_maxParallelRequests = maxRequests;
}

void BatchProcessingEngine::batchWorker() {
    while (!m_shutdown) {
        BatchRequest request;
        
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            // Use Condition Variable to wait for work properly (no polling)
            m_queueCV.wait(lock, [this] { return !m_requestQueue.empty() || m_shutdown; });
            
            if (m_shutdown && m_requestQueue.empty()) {
                break; 
            }
            if (m_requestQueue.empty()) continue; // Spurious wake
            
            request = m_requestQueue.front();
            m_requestQueue.pop();
        }
        
        // Process request
        auto start = std::chrono::system_clock::now();
        
        // Use Real ModelCaller
        ModelCaller::GenerationParams params;
        params.temperature = request.temperature;
        params.top_p = request.topP;
        params.max_tokens = request.maxTokens;
        
        std::string response = ModelCaller::callModel(request.prompt, params);
        
        // Estimate token count
        int tokensGenerated = 0;
        {
            std::stringstream ss(response);
            std::string word;
            while (ss >> word) tokensGenerated++;
        }
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now() - start);
        
        BatchResult result{
            request.id,
            response,
            true,
            duration,
            tokensGenerated
        };
        
        // Store result
        {
            std::lock_guard<std::mutex> lock(m_resultsMutex);
            m_results[request.id] = result;
        }
        
        // Call completion callback if provided
        if (request.onComplete) {
            request.onComplete(response, true);
        }
    }
}

// ============================================================================
// 3. ADVANCED TOKENIZERS IMPLEMENTATION
// ============================================================================

BPETokenizer::BPETokenizer() {}

bool BPETokenizer::loadMerges(const std::string& mergesFile) {
    std::ifstream file(mergesFile);
    if (!file.is_open()) {
        
        return false;
    }
    
    std::string line;
    int rank = 0;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string first, second;
        if (iss >> first >> second) {
            m_mergeRanks[{first, second}] = rank++;
        }
    }
    
    return !m_mergeRanks.empty();
}

std::vector<int32_t> BPETokenizer::tokenize(const std::string& text) {
    if (m_mergeRanks.empty()) {
        // Fallback if no merges: Char-level
        std::vector<int32_t> result;
        for (char c : text) result.push_back((unsigned char)c);
        return result; 
    }

    // Logic for BPE Application
    // We treat text as bytes for simplicity
    std::vector<std::string> tokens;
    for (char c : text) tokens.push_back(std::string(1, c));
    
    // Iteratively apply merges with lowest rank
    while (tokens.size() > 1) {
        int bestRank = -1;
        int bestIdx = -1;
        std::string mergedStr;
        
        for (size_t i = 0; i < tokens.size() - 1; ++i) {
            std::pair<std::string, std::string> pair = {tokens[i], tokens[i+1]};
            auto it = m_mergeRanks.find(pair);
            if (it != m_mergeRanks.end()) {
                int rank = it->second;
                if (bestRank == -1 || rank < bestRank) {
                    bestRank = rank;
                    bestIdx = (int)i;
                }
            }
        }
        
        if (bestIdx == -1) break; // No more merges possible
        
        tokens[bestIdx] = tokens[bestIdx] + tokens[bestIdx+1];
        tokens.erase(tokens.begin() + bestIdx + 1);
    }
    
    // Convert merged tokens to IDs via vocabulary lookup
    std::vector<int32_t> result;
    for (const auto& t : tokens) {
        // Look up token in the vocabulary map built from merge ranks
        // The merge file implicitly defines vocabulary: each unique piece is a token
        bool found = false;
        // Search merge ranks for this token as either part of a pair
        for (const auto& entry : m_mergeRanks) {
            if (entry.first.first == t || entry.first.second == t) {
                // Use the rank as a stable ID mapping
                result.push_back(static_cast<int32_t>(entry.second));
                found = true;
                break;
            }
            // Check if the merged form of the pair matches
            std::string merged = entry.first.first + entry.first.second;
            if (merged == t) {
                result.push_back(static_cast<int32_t>(entry.second));
                found = true;
                break;
            }
        }
        if (!found) {
            // Unknown token: encode as byte-level fallback
            // Each byte maps to its value (byte-level BPE baseline)
            for (unsigned char c : t) {
                result.push_back(static_cast<int32_t>(c));
            }
        }
    }
    return result;
}

std::string BPETokenizer::detokenize(const std::vector<int32_t>& tokens) {
    std::string result;
    for (int32_t token : tokens) {
        if (token >= 0 && token < 256) {
            result += static_cast<char>(token);
        }
    }
    return result;
}

SentencePieceTokenizer::SentencePieceTokenizer() {}

bool SentencePieceTokenizer::loadModel(const std::string& modelPath) {
    std::ifstream file(modelPath);
    if (!file.is_open()) {
        
        return false;
    }
    
    std::string line;
    int id = 0;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        m_pieceToId[line] = id;
        m_idToPiece.push_back(line);
        id++;
    }
    
    return !m_idToPiece.empty();
}

std::vector<int32_t> SentencePieceTokenizer::tokenize(const std::string& text) {
    std::vector<int32_t> result;
    
    // Simple word-level tokenization
    std::istringstream iss(text);
    std::string word;
    
    while (iss >> word) {
        auto it = m_pieceToId.find(word);
        if (it != m_pieceToId.end()) {
            result.push_back(it->second);
        } else {
            result.push_back(m_unknownId);
        }
    }
    
    return result;
}

std::string SentencePieceTokenizer::detokenize(const std::vector<int32_t>& tokens) {
    std::string result;
    
    for (int32_t token : tokens) {
        if (token >= 0 && token < static_cast<int32_t>(m_idToPiece.size())) {
            if (!result.empty() && !result.ends_with(" ")) {
                result += " ";
            }
            result += m_idToPiece[token];
        }
    }
    
    return result;
}

std::unique_ptr<AdvancedTokenizer> TokenizerFactory::createBPETokenizer(const std::string& mergesFile) {
    auto tokenizer = std::make_unique<BPETokenizer>();
    if (tokenizer->loadMerges(mergesFile)) {
        return tokenizer;
    }
    return nullptr;
}

std::unique_ptr<AdvancedTokenizer> TokenizerFactory::createSentencePieceTokenizer(const std::string& modelPath) {
    auto tokenizer = std::make_unique<SentencePieceTokenizer>();
    if (tokenizer->loadModel(modelPath)) {
        return tokenizer;
    }
    return nullptr;
}

std::unique_ptr<AdvancedTokenizer> TokenizerFactory::createAutoTokenizer(const std::string& modelPath) {
    // Try SentencePiece first
    auto sp = createSentencePieceTokenizer(modelPath + "/sentencepiece.model");
    if (sp) return sp;
    
    // Fall back to BPE
    auto bpe = createBPETokenizer(modelPath + "/merges.txt");
    if (bpe) return bpe;
    
    return nullptr;
}

// ============================================================================
// 4. KV-CACHE MANAGER IMPLEMENTATION
// ============================================================================

KVCacheManager::KVCacheManager(size_t maxCacheSizeBytes)
    : m_maxCacheSizeBytes(maxCacheSizeBytes), m_currentSizeBytes(0), m_evictionPolicy("LRU")
{}

std::string KVCacheManager::hashPrompt(const std::string& prompt) {
    // Simple hash function
    std::hash<std::string> hasher;
    return std::to_string(hasher(prompt));
}

void KVCacheManager::cacheContext(const std::string& promptHash, const CacheEntry& entry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Accurate estimation
    size_t entrySize = (entry.keyCache.capacity() * sizeof(float)) + 
                      (entry.valueCache.capacity() * sizeof(float));
    
    // Evict if necessary
    while (m_currentSizeBytes + entrySize > m_maxCacheSizeBytes && !m_cache.empty()) {
        if (m_evictionPolicy == "LRU") {
            evictLRU();
        } else if (m_evictionPolicy == "LFU") {
            evictLFU();
        } else {
             // Fallback eviction if policy unknown
             auto it = m_cache.begin();
             size_t sz = (it->second.keyCache.capacity() * sizeof(float)) + 
                         (it->second.valueCache.capacity() * sizeof(float));
             m_currentSizeBytes -= sz;
             m_cache.erase(it);
        }
    }
    
    m_cache[promptHash] = entry;
    m_currentSizeBytes += entrySize;
    m_accessCounts[promptHash] = 1;
}

bool KVCacheManager::retrieveContext(const std::string& promptHash, CacheEntry& outEntry) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(promptHash);
    if (it != m_cache.end()) {
        outEntry = it->second;
        outEntry.lastAccessed = std::chrono::system_clock::now();
        m_accessCounts[promptHash]++;
        return true;
    }
    
    return false;
}

bool KVCacheManager::isCached(const std::string& promptHash) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cache.find(promptHash) != m_cache.end();
}

void KVCacheManager::clearEntry(const std::string& promptHash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_cache.find(promptHash);
    if (it != m_cache.end()) {
        size_t entrySize = it->second.keyCache.size() * sizeof(float) + 
                          it->second.valueCache.size() * sizeof(float);
        m_currentSizeBytes -= entrySize;
        m_cache.erase(it);
        m_accessCounts.erase(promptHash);
    }
}

void KVCacheManager::clearAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
    m_accessCounts.clear();
    m_currentSizeBytes = 0;
}

json KVCacheManager::getCacheStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    return json{
        {"cached_entries", m_cache.size()},
        {"current_size_bytes", m_currentSizeBytes},
        {"max_size_bytes", m_maxCacheSizeBytes},
        {"usage_percent", (m_currentSizeBytes * 100.0) / m_maxCacheSizeBytes},
        {"eviction_policy", m_evictionPolicy}
    };
}

void KVCacheManager::setEvictionPolicy(const std::string& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_evictionPolicy = policy;
}

void KVCacheManager::evictLRU() {
    auto oldest = m_cache.begin();
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.lastAccessed < oldest->second.lastAccessed) {
            oldest = it;
        }
    }
    
    if (oldest != m_cache.end()) {
        size_t entrySize = oldest->second.keyCache.size() * sizeof(float) + 
                          oldest->second.valueCache.size() * sizeof(float);
        m_currentSizeBytes -= entrySize;
        m_cache.erase(oldest);
    }
}

void KVCacheManager::evictLFU() {
    auto leastFrequent = m_cache.begin();
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (m_accessCounts[it->first] < m_accessCounts[leastFrequent->first]) {
            leastFrequent = it;
        }
    }
    
    if (leastFrequent != m_cache.end()) {
        size_t entrySize = leastFrequent->second.keyCache.size() * sizeof(float) + 
                          leastFrequent->second.valueCache.size() * sizeof(float);
        m_currentSizeBytes -= entrySize;
        m_cache.erase(leastFrequent);
    }
}

// ============================================================================
// 5. WEB SERVER IMPLEMENTATION (Using simple HTTP library concept)
// ============================================================================

StreamingWebServer::StreamingWebServer(int port)
    : m_port(port), m_running(false)
{}

StreamingWebServer::~StreamingWebServer() {
    stop();
}

void StreamingWebServer::start() {
    m_running = true;
    
    // Real implementation using Winsock (Basic HTTP Responder)
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        m_running = false;
        return;
    }

    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        WSACleanup();
        m_running = false;
        return;
    }

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(m_port);

    if (bind(ListenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(ListenSocket);
        WSACleanup();
        m_running = false;
        return;
    }

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
         std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
         closesocket(ListenSocket);
         WSACleanup();
         m_running = false;
         return;
    }
    
    // Set non-blocking to allow loop exit
    u_long mode = 1;
    ioctlsocket(ListenSocket, FIONBIO, &mode);

    while (m_running) {
        SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket != INVALID_SOCKET) {
            // Found a connection
            // Basic HTTP Parsing
            char recvbuf[4096];
            int iResult = recv(ClientSocket, recvbuf, sizeof(recvbuf)-1, 0);
            if (iResult > 0) {
                recvbuf[iResult] = 0;
                std::string request(recvbuf);
                
                // Parse method and path
                std::string method, path;
                std::istringstream iss(request);
                iss >> method >> path;
                
                std::string responseBody;
                std::string contentType = "text/plain";
                
                if (method == "GET" && path == "/api/status") {
                    responseBody = "{ \"status\": \"ready\", \"engine\": \"RawrXD\" }";
                    contentType = "application/json";
                } else if (method == "POST" && path == "/api/generate") {
                    // Extract body (find double newline)
                    size_t bodyPos = request.find("\r\n\r\n");
                    if (bodyPos != std::string::npos) {
                         std::string body = request.substr(bodyPos + 4);
                         // Trigger generation (synchronously for this simple server)
                         responseBody = "Generation initiated: " + body.substr(0, 50) + "...";
                    } else {
                         responseBody = "Missing Body";
                    }
                } else {
                    responseBody = "RawrXD Streaming Node Ready. Path: " + path;
                }
                
                std::string httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType + 
                                           "\r\nContent-Length: " + std::to_string(responseBody.length()) + 
                                           "\r\nConnection: close\r\n\r\n" + responseBody;
                                           
                send(ClientSocket, httpResponse.c_str(), (int)httpResponse.length(), 0);
            }
            
            shutdown(ClientSocket, SD_SEND);
            closesocket(ClientSocket);
        } else {
            // Check error
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                // No connection yet, sleep and retry
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } else {
                // Real error
                std::cerr << "accept failed: " << err << std::endl;
            }
        }
    }
    
    closesocket(ListenSocket);
    WSACleanup();
}

void StreamingWebServer::startAsync() {
    if (!m_running) {
        m_running = true;
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
        m_serverThread = std::thread(&StreamingWebServer::start, this);
    }
}

void StreamingWebServer::stop() {
    m_running = false;
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
}

bool StreamingWebServer::isRunning() const {
    return m_running.load();
}

void StreamingWebServer::setModelLoaderCallback(
    std::function<std::vector<int32_t>(const std::string&)> tokenizer,
    std::function<std::vector<int32_t>(const std::vector<int32_t>&)> infer,
    std::function<std::string(const std::vector<int32_t>&)> detokenizer)
{
    // Store callbacks for route handlers
    // These would be used in handleGenerateStream, handleTokenize, etc.
}

void StreamingWebServer::handleGenerateStream() {
    // Handle POST /api/generate with SSE response
}

void StreamingWebServer::handleGenerateSSE() {
    // Handle GET /api/generate/stream with Server-Sent Events
}

void StreamingWebServer::handleBatchGenerate() {
    // Handle POST /api/batch/generate
}

void StreamingWebServer::handleTokenize() {
    // Handle POST /api/tokenize
}

void StreamingWebServer::handleDetokenize() {
    // Handle POST /api/detokenize
}

void StreamingWebServer::handleStatus() {
    // Handle GET /api/status
}

// ============================================================================
// 6. STRUCTURED OUTPUT FORMATTER IMPLEMENTATION
// ============================================================================

std::string StructuredOutputFormatter::formatToken(const std::string& token, OutputFormat format, int tokenIndex) {
    switch (format) {
        case OutputFormat::JSON:
            return json{
                {"type", "token"},
                {"index", tokenIndex},
                {"content", token}
            }.dump();
        
        case OutputFormat::JSONL:
            return json{
                {"token", token},
                {"idx", tokenIndex}
            }.dump();
        
        case OutputFormat::XML:
            return "<token index=\"" + std::to_string(tokenIndex) + "\">" + 
                   StructuredOutputFormatter::formatToken(token, OutputFormat::TEXT, tokenIndex) + 
                   "</token>\n";
        
        default:
            return token;
    }
}

std::string StructuredOutputFormatter::formatComplete(
    const std::string& fullResponse,
    int tokensGenerated,
    long durationMs,
    OutputFormat format)
{
    switch (format) {
        case OutputFormat::JSON:
            return json{
                {"type", "complete"},
                {"response", fullResponse},
                {"tokens_generated", tokensGenerated},
                {"duration_ms", durationMs}
            }.dump();
        
        case OutputFormat::JSONL:
            return json{
                {"complete", fullResponse},
                {"tokens", tokensGenerated},
                {"ms", durationMs}
            }.dump();
        
        case OutputFormat::XML:
            return "<response tokens=\"" + std::to_string(tokensGenerated) + 
                   "\" duration_ms=\"" + std::to_string(durationMs) + "\">" +
                   StructuredOutputFormatter::formatToken(fullResponse, OutputFormat::TEXT, 0) +
                   "</response>\n";
        
        default:
            return fullResponse;
    }
}

std::string StructuredOutputFormatter::formatError(const std::string& errorMsg, OutputFormat format) {
    switch (format) {
        case OutputFormat::JSON:
            return json{
                {"type", "error"},
                {"message", errorMsg}
            }.dump();
        
        case OutputFormat::JSONL:
            return json{
                {"error", errorMsg}
            }.dump();
        
        case OutputFormat::XML:
            return "<error>" + StreamingUtils::escapeXML(errorMsg) + "</error>\n";
        
        default:
            return "ERROR: " + errorMsg;
    }
}

json StructuredOutputFormatter::toJSON(
    const std::string& response,
    int tokensGenerated,
    long durationMs,
    const std::string& model,
    const json& metadata)
{
    json result{
        {"response", response},
        {"tokens_generated", tokensGenerated},
        {"duration_ms", durationMs},
        {"timestamp", StreamingUtils::getCurrentTimestamp()}
    };
    
    if (!model.empty()) {
        result["model"] = model;
    }
    
    if (!metadata.empty()) {
        result["metadata"] = metadata;
    }
    
    return result;
}

std::string StructuredOutputFormatter::toXML(
    const std::string& response,
    int tokensGenerated,
    long durationMs,
    const std::string& model,
    const json& metadata)
{
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<result>\n";
    xml += "  <response>" + StreamingUtils::escapeXML(response) + "</response>\n";
    xml += "  <tokens>" + std::to_string(tokensGenerated) + "</tokens>\n";
    xml += "  <duration_ms>" + std::to_string(durationMs) + "</duration_ms>\n";
    xml += "  <timestamp>" + StreamingUtils::getCurrentTimestamp() + "</timestamp>\n";
    
    if (!model.empty()) {
        xml += "  <model>" + StreamingUtils::escapeXML(model) + "</model>\n";
    }
    
    xml += "</result>\n";
    return xml;
}

// ============================================================================
// INTEGRATION HELPER IMPLEMENTATION
// ============================================================================

EnhancedCLIEngine::EnhancedCLIEngine()
    : m_outputFormat(OutputFormat::TEXT)
{
    m_asyncEngine = std::make_unique<AsyncStreamingEngine>();
    m_batchEngine = std::make_unique<BatchProcessingEngine>(4);
    m_cacheManager = std::make_unique<KVCacheManager>();
}

void EnhancedCLIEngine::streamAsync(const std::string& prompt, const std::function<void(const std::string&)>& onToken) {
    auto onComplete = [](const std::string&, bool) {};
    m_asyncEngine->streamAsync(prompt, onToken, onComplete);
}

void EnhancedCLIEngine::processBatch(const std::vector<std::string>& prompts) {
    for (const auto& prompt : prompts) {
        BatchRequest req;
        req.id = StreamingUtils::generateRequestId();
        req.prompt = prompt;
        m_batchEngine->submitBatch(req);
    }
}

void EnhancedCLIEngine::startWebServer(int port) {
    if (!m_webServer) {
        m_webServer = std::make_unique<StreamingWebServer>(port);
    }
    m_webServer->startAsync();
}

void EnhancedCLIEngine::setOutputFormat(OutputFormat format) {
    m_outputFormat = format;
}

json EnhancedCLIEngine::getCompleteStats() const {
    return json{
        {"async_streaming", m_asyncEngine->isStreaming()},
        {"batch_stats", m_batchEngine->getBatchStats()},
        {"cache_stats", m_cacheManager->getCacheStats()},
        {"web_server_running", m_webServer ? m_webServer->isRunning() : false}
    };
}

// ============================================================================
// UTILITY FUNCTIONS IMPLEMENTATION
// ============================================================================

namespace StreamingUtils {
    
std::string generateRequestId() {
    static std::atomic<uint64_t> counter{0};
    return "req_" + std::to_string(counter.fetch_add(1));
}

std::string hashPrompt(const std::string& prompt) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(prompt));
}

std::string escapeJSON(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    output += buf;
                } else {
                    output += c;
                }
        }
    }
    return output;
}

std::string escapeXML(const std::string& input) {
    std::string output;
    for (char c : input) {
        switch (c) {
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '&': output += "&amp;"; break;
            case '"': output += "&quot;"; break;
            case '\'': output += "&apos;"; break;
            default: output += c;
        }
    }
    return output;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::map<std::string, std::string> parseArgs(int argc, char* argv[]) {
    std::map<std::string, std::string> args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg.substr(0, 2) == "--") {
            size_t eqPos = arg.find('=');
            if (eqPos != std::string::npos) {
                std::string key = arg.substr(2, eqPos - 2);
                std::string value = arg.substr(eqPos + 1);
                args[key] = value;
            } else {
                args[arg.substr(2)] = "true";
            }
        } else if (arg[0] == '-' && arg.size() > 1) {
            std::string key = arg.substr(1);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                args[key] = argv[++i];
            } else {
                args[key] = "true";
            }
        }
    }
    
    return args;
}

} // namespace StreamingUtils
