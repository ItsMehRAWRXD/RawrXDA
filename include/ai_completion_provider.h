/**
 * @file ai_completion_provider.h
 * @brief AI-powered code completion provider (Ollama/local LLM) — C++20, no Qt
 *
 * Bridges between the text editor keystroke events and the local AI model
 * to provide real-time code suggestions without blocking the UI.
 *
 * Architecture: C++20 | Win32/STL | No Qt
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {

/**
 * @struct AICompletion
 * @brief Single code completion suggestion from AI model
 */
struct AICompletion {
    std::string text;           ///< The completion text to insert
    std::string type;           ///< "function", "variable", "keyword", etc.
    float confidence = 0.0f;    ///< 0.0 to 1.0 confidence score
    std::string documentation;  ///< Optional docstring
    int ranking = 999;          ///< Sort rank (0 = best)
};

/**
 * @class AICompletionProvider
 * @brief Async AI completion engine (callback-based, no Qt signals)
 *
 * Features:
 * - Async background completion fetching (doesn't block UI)
 * - Confidence ranking and sorting
 * - Context-aware suggestions using code context
 * - Fallback when model unavailable
 * - Latency metrics for monitoring
 */
class AICompletionProvider {
public:
    using CompletionsReadyFn = std::function<void(const std::vector<AICompletion>&)>;
    using ErrorFn = std::function<void(const std::string&)>;
    using LatencyFn = std::function<void(double)>;

    explicit AICompletionProvider() = default;
    ~AICompletionProvider() = default;

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

    /** Callbacks (replace Qt signals). Set before requestCompletions. */
    void setOnCompletionsReady(CompletionsReadyFn fn) { m_onCompletionsReady = std::move(fn); }
    void setOnError(ErrorFn fn) { m_onError = std::move(fn); }
    void setOnLatencyReported(LatencyFn fn) { m_onLatencyReported = std::move(fn); }

private:
    void onCompletionRequest();

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

    std::string m_modelEndpoint = "http://localhost:11434";
    std::string m_modelName = "mistral";
    int m_requestTimeoutMs = 5000;
    bool m_useTimeoutFallback = true;
    float m_minConfidence = 0.5f;
    int m_maxSuggestions = 5;

    bool m_isPending = false;
    double m_lastLatencyMs = 0.0;
    std::string m_lastPrefix;
    std::string m_lastSuffix;
    std::string m_lastFileType;
    std::vector<std::string> m_lastContextLines;

    CompletionsReadyFn m_onCompletionsReady;
    ErrorFn m_onError;
    LatencyFn m_onLatencyReported;
};

} // namespace RawrXD
