// streaming_enhancements.h
// Comprehensive async streaming, batch processing, advanced tokenizers, and web server support
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================================================
// 1. ASYNC STREAMING ENGINE - Non-blocking token generation with callbacks
// ============================================================================

class AsyncStreamingEngine {
public:
    using TokenCallback = std::function<void(const std::string&)>;
    using CompleteCallback = std::function<void(const std::string&, bool)>;  // full_response, success
    
    AsyncStreamingEngine();
    ~AsyncStreamingEngine();
    
    // Start async streaming with callbacks (non-blocking)
    void streamAsync(
        const std::string& prompt,
        TokenCallback onToken,
        CompleteCallback onComplete,
        float temperature = 0.7f,
        float topP = 0.9f,
        int maxTokens = 128
    );
    
    // Cancel ongoing streaming
    void cancelStreaming();
    
    // Check if streaming is active
    bool isStreaming() const;
    
    // Wait for current stream to complete (timeout in ms, 0 = infinite)
    bool waitForCompletion(int timeoutMs = 0);
    
private:
    void streamWorker(
        const std::string& prompt,
        TokenCallback onToken,
        CompleteCallback onComplete,
        float temperature,
        float topP,
        int maxTokens
    );
    
    std::thread m_workerThread;
    std::atomic<bool> m_isStreaming{false};
    std::atomic<bool> m_cancelRequested{false};
    std::mutex m_mutex;
    std::condition_variable m_completionCV;
};

// ============================================================================
// 2. BATCH PROCESSING ENGINE - Parallel generation of multiple prompts
// ============================================================================

struct BatchRequest {
    std::string id;
    std::string prompt;
    float temperature = 0.7f;
    float topP = 0.9f;
    int maxTokens = 128;
    TokenCallback onToken;
    CompleteCallback onComplete;
};

struct BatchResult {
    std::string requestId;
    std::string response;
    bool success;
    std::chrono::milliseconds duration;
    int tokensGenerated;
};

class BatchProcessingEngine {
public:
    BatchProcessingEngine(int maxParallelRequests = 4);
    ~BatchProcessingEngine();
    
    // Submit request to batch queue
    void submitBatch(const BatchRequest& request);
    
    // Submit multiple requests at once
    void submitBatchMultiple(const std::vector<BatchRequest>& requests);
    
    // Get result (blocking until available or timeout)
    BatchResult getBatchResult(const std::string& requestId, int timeoutMs = 0);
    
    // Check if batch is complete
    bool isBatchComplete(const std::string& requestId) const;
    
    // Get batch statistics
    json getBatchStats() const;
    
    // Set max parallel workers
    void setMaxParallelRequests(int maxRequests);
    
private:
    void batchWorker();
    
    std::queue<BatchRequest> m_requestQueue;
    std::map<std::string, BatchResult> m_results;
    std::vector<std::thread> m_workers;
    int m_maxParallelRequests;
    std::mutex m_queueMutex;
    std::mutex m_resultsMutex;
    std::condition_variable m_queueCV;
    std::atomic<bool> m_shutdown{false};
};

// ============================================================================
// 3. ADVANCED TOKENIZER FACTORY - BPE and SentencePiece support
// ============================================================================

enum class TokenizerType {
    FALLBACK,
    BPE,
    SENTENCEPIECE
};

class AdvancedTokenizer {
public:
    virtual ~AdvancedTokenizer() = default;
    
    virtual std::vector<int32_t> tokenize(const std::string& text) = 0;
    virtual std::string detokenize(const std::vector<int32_t>& tokens) = 0;
    virtual TokenizerType getType() const = 0;
};

class BPETokenizer : public AdvancedTokenizer {
public:
    BPETokenizer();
    bool loadMerges(const std::string& mergesFile);
    
    std::vector<int32_t> tokenize(const std::string& text) override;
    std::string detokenize(const std::vector<int32_t>& tokens) override;
    TokenizerType getType() const override { return TokenizerType::BPE; }
    
private:
    std::map<std::pair<std::string, std::string>, int> m_mergeRanks;
    std::map<std::string, int> m_vocab;
    std::vector<std::string> m_inverseVocab;
    
    std::pair<std::vector<std::string>, std::vector<int>> bpeEncode(const std::string& token);
};

class SentencePieceTokenizer : public AdvancedTokenizer {
public:
    SentencePieceTokenizer();
    bool loadModel(const std::string& modelPath);
    
    std::vector<int32_t> tokenize(const std::string& text) override;
    std::string detokenize(const std::vector<int32_t>& tokens) override;
    TokenizerType getType() const override { return TokenizerType::SENTENCEPIECE; }
    
private:
    std::map<std::string, int> m_pieceToId;
    std::vector<std::string> m_idToPiece;
    int m_unknownId = -1;
};

class TokenizerFactory {
public:
    static std::unique_ptr<AdvancedTokenizer> createBPETokenizer(const std::string& mergesFile);
    static std::unique_ptr<AdvancedTokenizer> createSentencePieceTokenizer(const std::string& modelPath);
    static std::unique_ptr<AdvancedTokenizer> createAutoTokenizer(const std::string& modelPath);
};

// ============================================================================
// 4. KV-CACHE MANAGER - Efficient context caching for long sessions
// ============================================================================

struct CacheEntry {
    std::vector<float> keyCache;
    std::vector<float> valueCache;
    int sequenceLength;
    std::chrono::system_clock::time_point lastAccessed;
};

class KVCacheManager {
public:
    KVCacheManager(size_t maxCacheSizeBytes = 1024 * 1024 * 512);  // 512MB default
    
    // Store KV-cache for prompt
    void cacheContext(const std::string& promptHash, const CacheEntry& entry);
    
    // Retrieve cached context
    bool retrieveContext(const std::string& promptHash, CacheEntry& outEntry);
    
    // Check if context is cached
    bool isCached(const std::string& promptHash) const;
    
    // Clear specific cache entry
    void clearEntry(const std::string& promptHash);
    
    // Clear all cache
    void clearAll();
    
    // Get cache statistics
    json getCacheStats() const;
    
    // Set eviction policy (LRU, LFU, FIFO)
    void setEvictionPolicy(const std::string& policy);
    
private:
    void evictLRU();
    void evictLFU();
    std::string hashPrompt(const std::string& prompt);
    
    std::map<std::string, CacheEntry> m_cache;
    std::map<std::string, int> m_accessCounts;
    size_t m_maxCacheSizeBytes;
    size_t m_currentSizeBytes;
    std::string m_evictionPolicy;
    mutable std::mutex m_mutex;
};

// ============================================================================
// 5. WEB SERVER MODE - REST API with Server-Sent Events
// ============================================================================

class StreamingWebServer {
public:
    StreamingWebServer(int port = 8080);
    ~StreamingWebServer();
    
    // Start server (blocking)
    void start();
    
    // Start server (non-blocking)
    void startAsync();
    
    // Stop server
    void stop();
    
    // Check if running
    bool isRunning() const;
    
    // Register model loader callback
    void setModelLoaderCallback(std::function<std::vector<int32_t>(const std::string&)> tokenizer,
                               std::function<std::vector<int32_t>(const std::vector<int32_t>&)> infer,
                               std::function<std::string(const std::vector<int32_t>&)> detokenizer);
    
private:
    int m_port;
    std::thread m_serverThread;
    std::atomic<bool> m_running{false};
    
    // HTTP route handlers
    void handleGenerateStream();
    void handleGenerateSSE();
    void handleBatchGenerate();
    void handleTokenize();
    void handleDetokenize();
    void handleStatus();
};

// ============================================================================
// 6. STRUCTURED OUTPUT FORMATTER - JSON/XML streaming support
// ============================================================================

enum class OutputFormat {
    TEXT,
    JSON,
    XML,
    JSONL  // JSON Lines (one JSON object per line)
};

class StructuredOutputFormatter {
public:
    static std::string formatToken(const std::string& token, OutputFormat format, int tokenIndex);
    
    static std::string formatComplete(
        const std::string& fullResponse,
        int tokensGenerated,
        long durationMs,
        OutputFormat format
    );
    
    static std::string formatError(const std::string& errorMsg, OutputFormat format);
    
    static json toJSON(
        const std::string& response,
        int tokensGenerated,
        long durationMs,
        const std::string& model = "",
        const json& metadata = json::object()
    );
    
    static std::string toXML(
        const std::string& response,
        int tokensGenerated,
        long durationMs,
        const std::string& model = "",
        const json& metadata = json::object()
    );
};

// ============================================================================
// INTEGRATION HELPER - All enhancements combined
// ============================================================================

class EnhancedCLIEngine {
public:
    EnhancedCLIEngine();
    
    // Async streaming
    void streamAsync(const std::string& prompt, const std::function<void(const std::string&)>& onToken);
    
    // Batch processing
    void processBatch(const std::vector<std::string>& prompts);
    
    // Web server
    void startWebServer(int port = 8080);
    
    // Structured output
    void setOutputFormat(OutputFormat format);
    
    // Get all statistics
    json getCompleteStats() const;
    
private:
    std::unique_ptr<AsyncStreamingEngine> m_asyncEngine;
    std::unique_ptr<BatchProcessingEngine> m_batchEngine;
    std::unique_ptr<StreamingWebServer> m_webServer;
    std::unique_ptr<KVCacheManager> m_cacheManager;
    std::unique_ptr<AdvancedTokenizer> m_tokenizer;
    OutputFormat m_outputFormat;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

namespace StreamingUtils {
    // Generate unique request ID
    std::string generateRequestId();
    
    // Calculate prompt hash for caching
    std::string hashPrompt(const std::string& prompt);
    
    // Escape JSON strings
    std::string escapeJSON(const std::string& input);
    
    // Escape XML strings
    std::string escapeXML(const std::string& input);
    
    // Get current timestamp
    std::string getCurrentTimestamp();
    
    // Parse command line arguments
    std::map<std::string, std::string> parseArgs(int argc, char* argv[]);
}

#endif // STREAMING_ENHANCEMENTS_H
