// ============================================================================
// context_assembler.h — Ghost Text Context Window (8K+ Tokens)
// ============================================================================
// Priority-based context hierarchy assembly for code completion.
// Manages suffix tokenization, priority sampling, edit distance tracking,
// and git diff integration.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <filesystem>

namespace RawrXD {
namespace Context {

// ============================================================================
// Context Hierarchy — prioritized context chunks
// ============================================================================

struct ContextHierarchy {
    std::string currentFunction;   // ~2K tokens (priority 1)
    std::string imports;           // ~1K tokens (priority 2)
    std::string recentEdits;       // ~3K tokens (priority 3 — from git diff)
    std::string siblingFunctions;  // ~2K tokens (priority 4)
    std::string fileSummary;       // ~0.5K tokens (priority 5 — AI-generated)

    // Additional context
    std::string prefix;            // Text before cursor
    std::string suffix;            // Text after cursor (reverse tokenized)
    std::string languageHint;      // "cpp", "python", etc.
};

// ============================================================================
// Priority Chunk
// ============================================================================

struct PriorityChunk {
    int priority;                  // Lower = higher priority
    std::string content;
    int estimatedTokens;
    const char* label;             // Debug label
};

// ============================================================================
// Edit Event — for recent edit tracking
// ============================================================================

struct EditEvent {
    std::filesystem::path file;
    int line;
    int column;
    std::string oldText;
    std::string newText;
    uint64_t timestamp;            // Epoch ms
};

// ============================================================================
// Context Result
// ============================================================================

struct ContextResult {
    bool success;
    const char* detail;
    std::string assembledContext;
    int totalTokens;
    int contextWindowUsed;

    static ContextResult ok(std::string ctx, int tokens) {
        return {true, "OK", std::move(ctx), tokens, tokens};
    }
    static ContextResult error(const char* msg) {
        return {false, msg, "", 0, 0};
    }
};

// ============================================================================
// Recent Edit Buffer — circular buffer of last N edits
// ============================================================================

class RecentEditBuffer {
public:
    explicit RecentEditBuffer(size_t maxEdits = 50);

    void recordEdit(const EditEvent& edit);
    std::vector<EditEvent> getRecent(size_t count) const;
    std::string formatAsContext(size_t maxTokens = 3000) const;
    void clear();
    size_t size() const;

private:
    std::deque<EditEvent> m_edits;
    size_t m_maxEdits;
    mutable std::mutex m_mutex;
};

// ============================================================================
// Git Diff Parser — extract recent changes for context
// ============================================================================

struct DiffHunk {
    std::string file;
    int oldStart, oldCount;
    int newStart, newCount;
    std::string content;           // The diff text
};

class GitDiffParser {
public:
    // Parse unified diff output (git diff --cached or git diff HEAD)
    static std::vector<DiffHunk> parseUnifiedDiff(const std::string& diffOutput);

    // Execute git diff and parse results
    static std::vector<DiffHunk> getRecentChanges(const std::filesystem::path& repoRoot);

    // Format hunks as context string
    static std::string formatAsContext(const std::vector<DiffHunk>& hunks,
                                        size_t maxTokens = 3000);
};

// ============================================================================
// Context Assembler — main class
// ============================================================================

class ContextAssembler {
public:
    ContextAssembler();
    ~ContextAssembler();

    // Non-copyable
    ContextAssembler(const ContextAssembler&) = delete;
    ContextAssembler& operator=(const ContextAssembler&) = delete;

    // ---- Configuration ----
    void setMaxContextTokens(int tokens) { m_maxTokens = tokens; }
    void setRepoRoot(const std::filesystem::path& root) { m_repoRoot = root; }

    // ---- Assembly ----
    ContextResult assemble(const std::string& fileContent,
                            int cursorLine, int cursorColumn,
                            const std::string& language);

    // Full assembly with all context sources
    ContextResult assembleWithHierarchy(const std::string& fileContent,
                                         int cursorLine, int cursorColumn,
                                         const std::string& language,
                                         const ContextHierarchy& additionalCtx);

    // ---- Context Extraction ----
    std::string extractCurrentFunction(const std::string& fileContent,
                                        int cursorLine) const;
    std::string extractImports(const std::string& fileContent) const;
    std::string extractSiblingFunctions(const std::string& fileContent,
                                         int cursorLine,
                                         int maxFunctions = 3) const;
    std::string extractPrefix(const std::string& fileContent,
                               int cursorLine, int cursorColumn,
                               int maxTokens = 4096) const;
    std::string extractSuffix(const std::string& fileContent,
                               int cursorLine, int cursorColumn,
                               int maxTokens = 4096) const;

    // ---- Priority Sampling ----
    // When total context exceeds maxTokens, drop lowest priority chunks
    std::string prioritySample(const std::vector<PriorityChunk>& chunks,
                                int maxTokens) const;

    // ---- Edit Integration ----
    RecentEditBuffer& editBuffer() { return m_editBuffer; }
    const RecentEditBuffer& editBuffer() const { return m_editBuffer; }

    // ---- FIM Format ----
    // Build Fill-In-Middle prompt (Qwen, CodeLlama, etc.)
    std::string buildFIMPrompt(const std::string& prefix,
                                const std::string& suffix,
                                const std::string& context = "") const;

    // ---- Token Estimation ----
    static int estimateTokens(const std::string& text);

private:
    int m_maxTokens = 8192;
    std::filesystem::path m_repoRoot;
    RecentEditBuffer m_editBuffer;

    // Truncate text to approximately maxTokens
    static std::string truncateToTokens(const std::string& text, int maxTokens);
};

} // namespace Context
} // namespace RawrXD
