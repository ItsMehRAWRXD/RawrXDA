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


#include <memory>
#include "real_time_completion_engine.h"
#include "lsp_client.h"

namespace RawrXD {

/**
 * \brief Completion item for AI suggestions
 */
struct AICompletion {
    std::string text;           // The completion text
    std::string detail;         // Additional context/description
    double confidence;      // 0.0 - 1.0
    std::string kind;          // "function", "method", "variable", etc.
    int cursorOffset;      // Where to place cursor after insertion
    bool isMultiLine;      // True if spans multiple lines
};

/**
 * \brief AI-powered completion provider
 * 
 * Integrates RealTimeCompletionEngine with Qt UI layer
 * Provides ghost text suggestions powered by local GGUF models
 */
class AICompletionProvider : public void
{

public:
    explicit AICompletionProvider(
        RealTimeCompletionEngine* engine,
        std::shared_ptr<Logger> logger,
        void* parent = nullptr
    );

    /**
     * Request completions for current cursor position
     * @param prefix Text before cursor
     * @param suffix Text after cursor
     * @param filePath Current file path (for context)
     * @param fileType File extension/type (cpp, py, js, etc.)
     * @param contextLines Additional context lines around cursor
     */
    void requestCompletions(
        const std::string& prefix,
        const std::string& suffix,
        const std::string& filePath,
        const std::string& fileType,
        const std::vector<std::string>& contextLines = {}
    );

    /**
     * Request inline completions (single line)
     */
    void requestInlineCompletion(
        const std::string& currentLine,
        int cursorColumn,
        const std::string& filePath
    );

    /**
     * Request multi-line completions
     */
    void requestMultiLineCompletion(
        const std::string& prefix,
        const std::string& filePath,
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


    /**
     * Emitted when completions are ready
     * @param completions List of AI-generated suggestions
     */
    void completionsReady(const std::vector<AICompletion>& completions);

    /**
     * Emitted on error
     */
    void error(const std::string& message);

    /**
     * Emitted to report latency
     */
    void latencyReported(double milliseconds);

private:
    /**
     * Convert internal CodeCompletion to AICompletion
     */
    std::vector<AICompletion> convertCompletions(const std::vector<CodeCompletion>& completions);

    /**
     * Build context string from surrounding lines
     */
    std::string buildContext(const std::vector<std::string>& lines) const;

    RealTimeCompletionEngine* m_engine;

    bool m_requestPending{false};
};

} // namespace RawrXD

