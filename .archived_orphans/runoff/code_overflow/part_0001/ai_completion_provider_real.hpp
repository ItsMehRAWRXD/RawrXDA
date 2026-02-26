#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
/* Qt removed */

/**
 * @class AICompletionProvider
 * @brief Real-time code completion engine with local model inference
 * 
 * Features:
 * - Streaming completion suggestions from GGUF models
 * - Confidence scoring (0.0-1.0)
 * - Syntax-aware context gathering
 * - Multi-language support
 * - Custom vocabulary registry
 */
class AICompletionProvider {
public:
    struct CompletionSuggestion {
        std::string text;
        std::string description;
        float confidence;  // 0.0-1.0
        int priority;      // -100 to 100
        std::string type;  // "function", "keyword", "variable", etc.
        std::string detail;// Additional info
        int sortText;      // Sort priority
    };

    struct CompletionContext {
        std::string currentLine;
        int cursorPosition;
        std::string previousLine;
        std::string nextLine;
        std::vector<std::string> allLines;
        int totalLines;
        std::string language;
        std::string currentFile;
        std::string projectRoot;
    };

    struct InferenceParams {
        int maxNewTokens = 32;
        float temperature = 0.3f;  // Lower for deterministic completions
        float topP = 0.95f;
        int topK = 40;
        bool useBeamSearch = false;
        int beamWidth = 3;
        std::vector<int> stopTokens;
    };

    explicit AICompletionProvider();
    ~AICompletionProvider();

    // Initialization
    bool initialize(const std::string& modelPath, const std::string& tokenizerPath);
    bool isInitialized() const { return m_initialized; }
    bool loadModel(const std::string& modelPath);
    bool loadTokenizer(const std::string& tokenizerPath);

    // Core completion API
    std::vector<CompletionSuggestion> getCompletions(
        const CompletionContext& context,
        const InferenceParams& params = InferenceParams()
    );

    // Streaming API for real-time updates
    void startStreamingCompletion(
        const CompletionContext& context,
        std::function<void(const CompletionSuggestion&)> onSuggestion,
        std::function<void(const std::string&)> onError,
        const InferenceParams& params = InferenceParams()
    );

    void cancelStreaming();
    bool isStreaming() const { return m_streaming.load(); }

    // Configuration
    void setMaxSuggestions(int count) { m_maxSuggestions = count; }
    void setConfidenceThreshold(float threshold) { m_confidenceThreshold = threshold; }
    void setInferenceTimeout(int milliseconds) { m_inferenceTimeout = milliseconds; }

    // Language-specific handling
    void registerLanguage(const std::string& language, 
                         const std::vector<std::string>& keywords,
                         const std::vector<std::string>& builtins);
    
    // Custom vocabulary
    void addCustomWords(const std::vector<std::string>& words);
    void removeCustomWords(const std::vector<std::string>& words);
    std::vector<std::string> getCustomWords() const;

    // Context management
    void setProjectContext(const std::string& projectRoot);
    void updateFileContext(const std::string& filePath);
    std::vector<std::string> getRelevantFiles(int maxFiles = 5) const;

    // Caching
    void enableCache() { m_cacheEnabled = true; }
    void disableCache() { m_cacheEnabled = false; }
    void clearCache();
    size_t getCacheSize() const { return m_completionCache.size(); }

    // Statistics
    struct CompletionStats {
        int totalRequests = 0;
        int successfulResponses = 0;
        int failedResponses = 0;
        float avgResponseTime = 0.0f;
        float avgConfidence = 0.0f;
        int cacheHits = 0;
    };
    CompletionStats getStats() const;
    void resetStats();

private:
    // Model management
    void* m_modelHandle = nullptr;
    void* m_tokenizerHandle = nullptr;
    bool m_initialized = false;

    // Inference
    std::vector<CompletionSuggestion> performInference(
        const std::string& prompt,
        const InferenceParams& params
    );

    // Context building
    std::string buildContextPrompt(const CompletionContext& ctx);
    int estimateTokenCount(const std::string& text) const;
    std::vector<int> tokenizeText(const std::string& text);
    std::string detokenize(const std::vector<int>& tokens);

    // Scoring
    float calculateConfidence(const std::string& suggestion, const CompletionContext& ctx);
    int calculatePriority(const std::string& suggestion, const CompletionContext& ctx);

    // Language support
    std::vector<std::string> getKeywords(const std::string& language) const;
    std::vector<std::string> getBuiltins(const std::string& language) const;
    std::string detectLanguage(const std::string& filePath) const;

    // Caching
    struct CachedCompletion {
        CompletionContext context;
        std::vector<CompletionSuggestion> suggestions;
        std::chrono::system_clock::time_point timestamp;
    };
    std::map<std::string, CachedCompletion> m_completionCache;
    std::string generateCacheKey(const CompletionContext& ctx) const;
    bool getCachedCompletion(const CompletionContext& ctx, std::vector<CompletionSuggestion>& out);

    // Streaming
    std::atomic<bool> m_streaming{false};
    std::thread m_streamingThread;
    std::mutex m_streamingMutex;

    // Settings
    int m_maxSuggestions = 10;
    float m_confidenceThreshold = 0.3f;
    int m_inferenceTimeout = 5000;  // ms
    bool m_cacheEnabled = true;

    // Language registry
    std::map<std::string, std::vector<std::string>> m_keywords;
    std::map<std::string, std::vector<std::string>> m_builtins;
    std::vector<std::string> m_customWords;

    // Context
    std::string m_projectRoot;
    std::string m_currentFile;
    std::vector<std::string> m_fileContexts;

    // Statistics
    CompletionStats m_stats;
    std::mutex m_statsMutex;
};
