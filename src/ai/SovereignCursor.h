// ============================================================================
// SovereignCursor.h — AI-Powered Editor Integration
// ============================================================================
// The "Brain" that connects:
//   - SovereignGapBuffer (editor text)
//   - SovereignVectorStore (RAG context)
//   - SovereignInferenceClient (local LLM)
//   - DiffEngine (patch application)
//
// This is the Cursor-like AI loop: read → embed → retrieve → infer → patch.
// ============================================================================

#pragma once

#include "../editor/SovereignGapBuffer.h"
#include "SovereignVectorStore.h"
#include "../agentic/SovereignInferenceClient.h"
#include "../agentic/DiffEngine.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace RawrXD {
namespace AI {

// ============================================================================
// AI suggestion result
// ============================================================================
struct AISuggestion {
    std::string text;           // Suggested code/text
    std::string reasoning;      // Why this suggestion
    float confidence = 0.0f;   // 0.0-1.0
    bool isDiff = false;        // If true, text is a unified diff
    std::vector<Diff::DiffHunk> hunks; // Parsed hunks if isDiff
};

// ============================================================================
// Cursor context for AI
// ============================================================================
struct CursorContext {
    std::string beforeCursor;   // Text before cursor
    std::string afterCursor;    // Text after cursor
    std::string fileName;       // Current file
    std::string language;       // Detected language
    std::string selectedText;   // Currently selected text (if any)
    size_t lineNumber = 0;      // Current line
    size_t columnNumber = 0;    // Current column
};

// ============================================================================
// Configuration
// ============================================================================
struct SovereignCursorConfig {
    // Model
    std::string modelPath = "models/codestral-22b-q4.gguf";
    uint32_t contextSize = 8192;
    float temperature = 0.2f;
    uint32_t maxTokens = 2048;

    // RAG
    uint32_t embeddingDim = 384;
    size_t maxContextChunks = 5;
    size_t maxContextTokens = 1536;
    float minSimilarity = 0.65f;

    // Behavior
    bool autoSuggest = true;
    uint32_t debounceMs = 300;
    bool speculativeDecode = true;
    uint32_t draftTokens = 5;

    // Diff
    int diffContextLines = 3;
};

// ============================================================================
// SovereignCursor — The AI Editor Brain
// ============================================================================
class SovereignCursor {
public:
    explicit SovereignCursor(const SovereignCursorConfig& cfg = {});
    ~SovereignCursor();

    // Non-copyable
    SovereignCursor(const SovereignCursor&) = delete;
    SovereignCursor& operator=(const SovereignCursor&) = delete;

    // ------------------------------------------------------------------------
    // Lifecycle
    // ------------------------------------------------------------------------
    bool Initialize();
    void Shutdown();
    bool IsReady() const;

    // ------------------------------------------------------------------------
    // Editor binding
    // ------------------------------------------------------------------------
    void BindBuffer(Editor::SovereignGapBuffer* buffer);
    void UnbindBuffer();

    // ------------------------------------------------------------------------
    // AI operations (async)
    // ------------------------------------------------------------------------
    void RequestCompletion();           // Complete at cursor
    void RequestInlineSuggestion();     // Ghost text suggestion
    void RequestRefactoring();          // Refactor selected code
    void RequestExplanation();          // Explain selected code
    void RequestFix();                  // Fix errors in selected code
    void RequestDiff(const std::string& instruction); // Apply instruction as diff

    // ------------------------------------------------------------------------
    // Synchronous helpers (for testing)
    // ------------------------------------------------------------------------
    AISuggestion SuggestCompletionSync();
    std::string ExplainSelectionSync();

    // ------------------------------------------------------------------------
    // RAG indexing
    // ------------------------------------------------------------------------
    void IndexWorkspace(const std::string& path);
    void IndexFile(const std::string& path, const std::string& content);
    void ClearIndex();

    // ------------------------------------------------------------------------
    // Callbacks
    // ------------------------------------------------------------------------
    using SuggestionCallback = std::function<void(const AISuggestion&)>;
    using ProgressCallback = std::function<void(const std::string& status)>;

    void SetSuggestionCallback(SuggestionCallback cb);
    void SetProgressCallback(ProgressCallback cb);

    // ------------------------------------------------------------------------
    // Stats
    // ------------------------------------------------------------------------
    struct Stats {
        uint64_t totalRequests = 0;
        uint64_t totalTokensGenerated = 0;
        double avgLatencyMs = 0.0;
        size_t indexChunkCount = 0;
        size_t indexMemoryBytes = 0;
    };
    Stats GetStats() const;

private:
    SovereignCursorConfig config_;

    // Components
    std::unique_ptr<Agent::SovereignInferenceClient> llm_;
    std::unique_ptr<SovereignVectorStore> vectorStore_;
    Editor::SovereignGapBuffer* buffer_ = nullptr;

    // Threading
    std::thread workerThread_;
    std::atomic<bool> running_{false};
    std::mutex queueMutex_;
    std::condition_variable queueCv_;

    enum class TaskType {
        Completion,
        InlineSuggestion,
        Refactoring,
        Explanation,
        Fix,
        Diff
    };

    struct Task {
        TaskType type;
        std::string instruction;
        CursorContext context;
    };

    std::vector<Task> taskQueue_;

    // Callbacks
    SuggestionCallback suggestionCb_;
    ProgressCallback progressCb_;
    mutable std::mutex cbMutex_;

    // Stats
    mutable std::mutex statsMutex_;
    Stats stats_;

    // Internal
    void WorkerLoop();
    void ProcessTask(const Task& task);

    std::string BuildPrompt(const Task& task);
    std::string BuildSystemPrompt();
    CursorContext CaptureContext();

    std::string RetrieveRAGContext(const std::string& query);
    std::vector<float> EmbedText(const std::string& text);

    AISuggestion ParseResponse(const std::string& response, TaskType type);
    bool ApplySuggestion(const AISuggestion& suggestion);

    void UpdateStats(uint64_t tokens, double latencyMs);
    void ReportProgress(const std::string& msg);
};

} // namespace AI
} // namespace RawrXD
