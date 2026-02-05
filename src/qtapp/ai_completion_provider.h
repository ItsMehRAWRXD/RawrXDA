/**
 * \file ai_completion_provider.h
 * \brief AI-powered completion provider using local GGUF models
 * \author RawrXD Team
 * \date 2025-12-13
 * 
 * Provides Cursor-style AI completions with:
 * - Real-time GGUF model inference
 * - Multi-line code suggestions
 * - Context-aware completions
 * - <100ms latency target
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>
#include "real_time_completion_engine.h"
#include "lsp_client.h"

namespace RawrXD {

/**
 * \brief Completion item for AI suggestions
 */
struct AICompletion {
    QString text;           // The completion text
    QString detail;         // Additional context/description
    double confidence;      // 0.0 - 1.0
    QString kind;          // "function", "method", "variable", etc.
    int cursorOffset;      // Where to place cursor after insertion
    bool isMultiLine;      // True if spans multiple lines
};

/**
 * \brief AI-powered completion provider
 * 
 * Integrates RealTimeCompletionEngine with Qt UI layer
 * Provides ghost text suggestions powered by local GGUF models
 */
class AICompletionProvider : public QObject
{
    Q_OBJECT

public:
    /**
     * Simple constructor for standalone usage (creates internal engine)
     */
    explicit AICompletionProvider(QObject* parent = nullptr);
    
    /**
     * Full constructor with external engine and logger
     */
    explicit AICompletionProvider(
        RealTimeCompletionEngine* engine,
        std::shared_ptr<Logger> logger,
        QObject* parent = nullptr
    );
    
    /**
     * Set model name for completions (e.g., "mistral", "llama2")
     */
    void setModel(const QString& modelName);
    
    /**
     * Set model endpoint URL (e.g., "http://localhost:11434")
     */
    void setModelEndpoint(const QString& endpoint);
    
    /**
     * Set request timeout in milliseconds
     */
    void setRequestTimeout(int timeoutMs);
    
    /**
     * Set maximum number of suggestions to return
     */
    void setMaxSuggestions(int count);

    /**
     * Request completions for current cursor position
     * @param prefix Text before cursor
     * @param suffix Text after cursor
     * @param filePath Current file path (for context)
     * @param fileType File extension/type (cpp, py, js, etc.)
     * @param contextLines Additional context lines around cursor
     */
    void requestCompletions(
        const QString& prefix,
        const QString& suffix,
        const QString& filePath,
        const QString& fileType,
        const QStringList& contextLines = {}
    );

    /**
     * Request inline completions (single line)
     */
    void requestInlineCompletion(
        const QString& currentLine,
        int cursorColumn,
        const QString& filePath
    );

    /**
     * Request multi-line completions
     */
    void requestMultiLineCompletion(
        const QString& prefix,
        const QString& filePath,
        int maxLines = 5
    );

    /**
     * Cancel any pending completion requests
     */
    void cancelRequest();

    /**
     * Get performance metrics
     */
    PerformanceMetrics getMetrics() const;

    /**
     * Clear completion cache
     */
    void clearCache();

signals:
    /**
     * Emitted when completions are ready
     * @param completions List of AI-generated suggestions
     */
    void completionsReady(const QVector<AICompletion>& completions);

    /**
     * Emitted on error
     */
    void error(const QString& message);

    /**
     * Emitted to report latency
     */
    void latencyReported(double milliseconds);

private:
    /**
     * Convert internal CodeCompletion to AICompletion
     */
    QVector<AICompletion> convertCompletions(const std::vector<CodeCompletion>& completions);

    /**
     * Build context string from surrounding lines
     */
    QString buildContext(const QStringList& lines) const;

    RealTimeCompletionEngine* m_engine;
    std::shared_ptr<Logger> m_logger;
    bool m_requestPending{false};
    bool m_ownsEngine{false};  // True if we created the engine ourselves
    QString m_modelName{"mistral"};
    QString m_modelEndpoint{"http://localhost:11434"};
    int m_requestTimeout{5000};
    int m_maxSuggestions{5};
};

} // namespace RawrXD
