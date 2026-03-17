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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

namespace RawrXD {

/**
 * @struct AICompletion
 * @brief Single code completion suggestion from AI model
 */
struct AICompletion {
    QString text;           ///< The completion text to insert
    QString type;           ///< "function", "variable", "keyword", etc.
    float confidence;       ///< 0.0 to 1.0 confidence score
    QString documentation;  ///< Optional docstring
    int ranking;            ///< Sort rank (0 = best)
    
    AICompletion() : confidence(0.0f), ranking(999) {}
};

/**
 * @class AICompletionProvider
 * @brief Async AI completion engine
 * 
 * Features:
 * - Async background completion fetching (doesn't block UI)
 * - Confidence ranking and sorting
 * - Context-aware suggestions using code context
 * - Fallback when model unavailable
 * - Latency metrics for monitoring
 */
class AICompletionProvider : public QObject {
    Q_OBJECT

public:
    explicit AICompletionProvider(QObject* parent = nullptr);
    ~AICompletionProvider() override;

    /**
     * Request code completions asynchronously
     * 
     * @param prefix Text before cursor on current line
     * @param suffix Text after cursor on current line
     * @param filePath Current file being edited
     * @param fileType Language/file type (cpp, python, js, etc.)
     * @param contextLines Previous lines for context (5 lines recommended)
     */
    void requestCompletions(
        const QString& prefix,
        const QString& suffix,
        const QString& filePath,
        const QString& fileType,
        const QStringList& contextLines
    );

    /**
     * Cancel any pending completion request
     */
    void cancelPendingRequest();

    /**
     * Set model endpoint (Ollama, vLLM, etc.)
     * Format: "http://localhost:11434" or similar
     */
    void setModelEndpoint(const QString& endpoint);

    /**
     * Set model name to use for completions
     * Examples: "llama2", "mistral", "neural-chat", etc.
     */
    void setModel(const QString& modelName);

    /**
     * Set timeout for completion requests (milliseconds)
     * Default: 5000ms
     */
    void setRequestTimeout(int timeoutMs);

    /**
     * Enable/disable timeout fallback
     * When enabled, failed requests fall back to simpler suggestions
     */
    void setTimeoutFallback(bool enabled);

    /**
     * Set minimum confidence threshold for suggestions
     * Range: 0.0 to 1.0 (default 0.5)
     * Suggestions below threshold are filtered out
     */
    void setMinConfidence(float threshold);

    /**
     * Set maximum number of suggestions to return
     */
    void setMaxSuggestions(int count);

    /**
     * Get current model endpoint
     */
    QString modelEndpoint() const { return m_modelEndpoint; }

    /**
     * Get current model name
     */
    QString model() const { return m_modelName; }

    /**
     * Get last recorded latency in milliseconds
     */
    double lastLatencyMs() const { return m_lastLatencyMs; }

    /**
     * Check if a completion request is currently pending
     */
    bool isPending() const { return m_isPending; }

signals:
    /**
     * Emitted when completions are ready
     * @param completions Vector of suggestions sorted by confidence
     */
    void completionsReady(const QVector<AICompletion>& completions);

    /**
     * Emitted if completion request fails
     * @param error Description of error
     */
    void error(const QString& error);

    /**
     * Emitted after request completes to report latency
     * @param milliseconds Time taken for the request
     */
    void latencyReported(double milliseconds);

private slots:
    /**
     * Internal: Handle completion request in background thread
     */
    void onCompletionRequest();

private:
    /**
     * Format prompt for the LLM
     */
    QString formatPrompt(
        const QString& prefix,
        const QString& suffix,
        const QString& fileType,
        const QStringList& contextLines
    );

    /**
     * Parse LLM response into structured completions
     */
    QVector<AICompletion> parseCompletions(const QString& response);

    /**
     * Extract confidence scores from model response
     */
    float extractConfidence(const QString& suggestion);

    /**
     * Call the AI model via HTTP (Ollama/vLLM API)
     */
    QString callModel(const QString& prompt);

    /**
     * Generate simple fallback completion when model unavailable
     */
    QString generateFallbackCompletion(const QString& prompt);

    // Configuration
    QString m_modelEndpoint = "http://localhost:11434";
    QString m_modelName = "mistral";
    int m_requestTimeoutMs = 5000;
    bool m_useTimeoutFallback = true;
    float m_minConfidence = 0.5f;
    int m_maxSuggestions = 5;

    // State
    bool m_isPending = false;
    double m_lastLatencyMs = 0.0;
    QString m_lastPrefix;
    QString m_lastSuffix;
    QString m_lastFileType;
};

} // namespace RawrXD
