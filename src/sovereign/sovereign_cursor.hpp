// ============================================================================
// sovereign_cursor.hpp — The "Digital Cortex" Integration Layer
// ============================================================================
// Connects: TextBuffer (Rope) → EmbeddingEngine (RAG) → SovereignInferenceClient
//           → DiffEngine → TextBuffer (Apply)
//
// This is the "Brain" that makes RawrXD a Cursor-like AI IDE.
// Zero HTTP. Zero serialization. Direct pointer access throughout.
// ============================================================================
#pragma once

#include "../RawrXD_TextBuffer.h"
#include "../core/embedding_engine.hpp"
#include "../core/diff_engine.h"
#include "../agentic/SovereignInferenceClient.h"
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace RawrXD {
namespace Sovereign {

// ============================================================================
// Configuration
// ============================================================================
struct CursorAIConfig {
    // Trigger behavior
    uint32_t debounceMs = 300;           // Delay before AI triggers after typing stops
    uint32_t minContextChars = 50;       // Minimum buffer size before AI activates
    uint32_t maxContextChars = 8192;     // Maximum context window for inference
    
    // RAG
    uint32_t topKChunks = 5;           // Number of code chunks to retrieve
    float similarityThreshold = 0.75f;   // Minimum cosine similarity for context
    
    // Inference
    float temperature = 0.1f;            // Low temp for deterministic code
    uint32_t maxTokens = 256;            // Max completion length
    bool useSpeculative = true;          // Use speculative decoding
    
    // Diff
    bool autoApplySingleLine = false;    // Auto-apply single-line suggestions
    bool showDiffPreview = true;         // Show diff before applying
    
    // Performance
    bool backgroundIndexing = true;      // Index files in background
    uint32_t inferenceThreads = 4;       // Thread pool size
};

// ============================================================================
// AI Suggestion Result
// ============================================================================
struct AISuggestion {
    enum class Type : uint8_t {
        Completion = 0,   // Continue typing
        Edit = 1,           // Modify existing code
        Diff = 2,           // Multi-line patch
        None = 3            // No suggestion
    };
    
    Type type = Type::None;
    std::string text;                  // The suggested text
    std::string explanation;           // Why this suggestion
    float confidence = 0.0f;           // 0.0-1.0
    
    // For Diff type
    Core::Diff::DiffResult diff;       // Structured diff
    
    // Positioning
    int insertPos = 0;                 // Where to insert
    int replaceLen = 0;                // How much to replace (0 = pure insert)
    
    // Metadata
    uint64_t latencyUs = 0;            // Generation latency
    uint32_t tokensGenerated = 0;        // Token count
};

// ============================================================================
// The Sovereign Cursor — AI-Powered Editor Brain
// ============================================================================
class SovereignCursor {
public:
    explicit SovereignCursor(const CursorAIConfig& cfg = {});
    ~SovereignCursor();
    
    // Lifecycle
    bool Initialize(
        const std::string& modelPath,           // Path to .gguf
        const std::string& embeddingModelPath,  // Path to embedding .gguf (optional)
        uint32_t contextSize = 8192
    );
    void Shutdown();
    bool IsReady() const;
    
    // Editor binding
    void AttachBuffer(TextBuffer* buffer);
    void DetachBuffer();
    
    // Cursor position tracking
    void SetCursorPosition(int pos);
    int GetCursorPosition() const;
    
    // Main AI loop triggers
    void OnTyping(int pos, const std::wstring& text);     // User typed something
    void OnSelection(int start, int end);                  // User selected text
    void OnIdle();                                          // Editor is idle
    
    // Explicit AI requests
    AISuggestion RequestCompletion();                        // "Complete this"
    AISuggestion RequestEdit(const std::wstring& instruction); // "Fix this bug"
    std::vector<AISuggestion> RequestAlternatives();       // "Show me options"
    
    // Suggestion management
    bool AcceptSuggestion(const AISuggestion& sug);
    bool RejectSuggestion(const AISuggestion& sug);
    bool PreviewSuggestion(const AISuggestion& sug);       // Show ghost text
    void ClearSuggestions();
    
    // Context management
    void IndexWorkspace(const std::string& path);            // Build RAG index
    void IndexCurrentFile();                                 // Re-index active file
    void ClearContext();                                     // Reset KV cache + embeddings
    
    // Callbacks
    using SuggestionCallback = std::function<void(const AISuggestion&)>;
    using ProgressCallback = std::function<void(const std::string& status)>;
    
    void SetSuggestionCallback(SuggestionCallback cb);
    void SetProgressCallback(ProgressCallback cb);
    
    // Statistics
    struct Stats {
        uint64_t totalCompletions = 0;
        uint64_t totalEdits = 0;
        uint64_t totalTokensGenerated = 0;
        double avgLatencyMs = 0.0;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
        uint64_t suggestionsAccepted = 0;
        uint64_t suggestionsRejected = 0;
    };
    Stats GetStats() const;

private:
    // Core components
    std::unique_ptr<Agent::SovereignInferenceClient> inference_;
    std::unique_ptr<Embeddings::EmbeddingEngine> embeddings_;
    Core::Diff::DiffEngine diffEngine_;
    
    // Editor state
    TextBuffer* buffer_ = nullptr;
    std::atomic<int> cursorPos_{0};
    std::atomic<int> selectionStart_{0};
    std::atomic<int> selectionEnd_{0};
    
    // Config
    CursorAIConfig config_;
    
    // Threading
    std::thread workerThread_;
    std::atomic<bool> running_{false};
    
    struct WorkItem {
        enum Type { Complete, Edit, Index, Idle } type;
        std::wstring instruction;  // For Edit type
        int cursorPos;
    };
    
    std::queue<WorkItem> workQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    
    // Debounce
    std::chrono::steady_clock::time_point lastTyping_;
    std::atomic<bool> debounceActive_{false};
    
    // Suggestions
    std::vector<AISuggestion> activeSuggestions_;
    std::mutex suggestionMutex_;
    
    // Callbacks
    SuggestionCallback onSuggestion_;
    ProgressCallback onProgress_;
    
    // Stats
    mutable std::mutex statsMutex_;
    Stats stats_;
    
    // Worker loop
    void WorkerLoop();
    void ProcessWorkItem(const WorkItem& item);
    
    // Context assembly
    std::string BuildContextPrompt(int cursorPos);
    std::string RetrieveRelevantContext(const std::string& query);
    std::string GetBufferContext(int cursorPos, int maxChars);
    
    // Suggestion generation
    AISuggestion GenerateCompletion(int cursorPos);
    AISuggestion GenerateEdit(int cursorPos, const std::wstring& instruction);
    AISuggestion ParseDiffSuggestion(const std::string& raw, int cursorPos);
    
    // Diff application
    bool ApplyDiffToBuffer(const Core::Diff::DiffResult& diff);
    
    // Helpers
    std::string WstringToUtf8(const std::wstring& ws);
    std::wstring Utf8ToWstring(const std::string& s);
    void UpdateStats(const AISuggestion& sug);
    void ReportProgress(const std::string& msg);
};

} // namespace Sovereign
} // namespace RawrXD
