/**
 * @file ai_completion_provider.h
 * @brief AI-powered code completion provider (Ollama/local LLM)
 * 
 * Bridges between the text editor keystroke events and the local AI model
 * to provide real-time code suggestions without blocking the UI.
 * 
 * @author RawrXD Team
 * @date 2026-01-07
 */

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace RawrXD {

/**
 * @struct AICompletion
 * @brief Single code completion suggestion from AI model
 */
struct AICompletion {
    std::string text;           ///< The completion text to insert
    std::string type;           ///< "function", "variable", "keyword", etc.
    std::string kind;           ///< Alias for type (test compatibility)
    float confidence;       ///< 0.0 to 1.0 confidence score
    std::string documentation;  ///< Optional docstring
    int ranking;            ///< Sort rank (0 = best)
    
    AICompletion() : confidence(0.0f), ranking(999) {}
};

/**
 * @class AICompletionProvider
 * @brief Async AI completion engine (STL Version)
 */
class AICompletionProvider {
public:
    explicit AICompletionProvider(void* parent = nullptr); // void* parent for compatibility if needed, or remove
    ~AICompletionProvider();

    // Callbacks
    using CompletionCallback = std::function<void(const std::vector<AICompletion>&)>;
    using ErrorCallback = std::function<void(const std::string&)>
    using LatencyCallback = std::function<void(double)>;

    void setCompletionCallback(CompletionCallback cb) { m_completionCallback = cb; }
    void setErrorCallback(ErrorCallback cb) { m_errorCallback = cb; }
    void setLatencyCallback(LatencyCallback cb) { m_latencyCallback = cb; }

    /**
     * Request code completions asynchronously
     */
    void requestCompletions(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& filePath,
        const std::string& fileType,
        const std::vector<std::string>& contextLines
    );

    void cancelPendingRequest();

    void setModelEndpoint(const std::string& endpoint);
    void setModel(const std::string& modelName);
    void setRequestTimeout(int timeoutMs);
    void setTimeoutFallback(bool enabled);
    void setMinConfidence(float threshold);
    void setMaxSuggestions(int count);

    std::string modelEndpoint() const { return m_modelEndpoint; }
    std::string model() const { return m_modelName; }
    double lastLatencyMs() const { return m_lastLatencyMs; }
    bool isPending() const { return m_isPending; }

private:
    void onCompletionRequest(); // Internal worker function

    std::string formatPrompt(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& fileType,
        const std::vector<std::string>& contextLines
    );

    std::vector<AICompletion> parseCompletions(const std::string& response);
    float extractConfidence(const std::string& suggestion);
    std::string callModel(const std::string& prompt);
    std::string generateFallbackCompletion(const std::string& prompt);

    // Configuration
    std::string m_modelEndpoint = "http://localhost:11434";
    std::string m_modelName = "mistral";
    int m_requestTimeoutMs = 5000;
    bool m_useTimeoutFallback = true;
    float m_minConfidence = 0.5f;
    int m_maxSuggestions = 5;

    // State
    bool m_isPending = false;
    double m_lastLatencyMs = 0.0;
    std::string m_lastPrefix;
    std::string m_lastSuffix;
    std::string m_lastFileType;
    std::vector<std::string> m_lastContext; // Added context storage

    // Callbacks
    CompletionCallback m_completionCallback;
    ErrorCallback m_errorCallback;
    LatencyCallback m_latencyCallback;
};

} // namespace RawrXD
