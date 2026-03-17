// ============================================================================
// agentic_composer_ux.h — Agentic Composer UX Polish System
// ============================================================================
// Provides Cursor-like agentic composer UI orchestration:
//   - Step visualization with progress indicators
//   - Approval flow for multi-file changes
//   - Thinking/reasoning display with streaming
//   - Tool call visualization with result previews
//   - Diff overlay with per-hunk accept/reject
//   - Context chip display (files, symbols, @-mentions)
//   - Session timeline with branching
//
// Integrates with:
//   - BoundedAgentLoop (agentic framework)
//   - AgentTranscript (audit trail)
//   - DiffEngine (hunk-level diffs)
//   - UnifiedTelemetryCore (metrics)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Agentic {

// ============================================================================
// Composer step states
// ============================================================================
enum class StepState {
    Pending,        // Not yet started
    InProgress,     // Currently executing
    WaitingApproval,// Needs user approval
    Approved,       // User approved
    Rejected,       // User rejected
    Completed,      // Successfully done
    Failed,         // Error occurred
    Skipped         // Skipped by user or logic
};

// ============================================================================
// Tool call visualization data
// ============================================================================
struct ToolCallDisplay {
    int         stepIndex;
    std::string toolName;
    std::string arguments;      // Serialized JSON
    std::string resultPreview;  // First N chars of result
    std::string fullResult;
    int64_t     durationMs;
    bool        success;
    StepState   state;
};

// ============================================================================
// File change entry for approval flow
// ============================================================================
struct FileChangeEntry {
    std::string filePath;
    std::string diffPreview;        // Unified diff
    int         addedLines;
    int         removedLines;
    int         hunks;
    bool        approved;
    bool        reviewed;
    
    // Per-hunk approval
    struct HunkApproval {
        int     hunkIndex;
        std::string hunkDiff;
        bool    approved;
        bool    reviewed;
    };
    std::vector<HunkApproval> hunkApprovals;
};

// ============================================================================
// Thinking/reasoning display
// ============================================================================
struct ThinkingBlock {
    uint64_t    id;
    std::string title;          // "Analyzing codebase...", "Planning changes..."
    std::string content;        // Streaming content
    int64_t     startMs;
    int64_t     endMs;          // -1 if still streaming
    bool        collapsed;      // UI state
    
    // Token stats
    int         tokensGenerated;
    float       tokensPerSecond;
};

// ============================================================================
// Context chip (displayed in composer header)
// ============================================================================
enum class ContextChipType {
    File,           // @file reference
    Symbol,         // @symbol reference
    Selection,      // Current selection
    Terminal,       // Terminal output
    Web,            // Web search result
    Image,          // Image input
    Diagnostic,     // Error/warning
    Custom          // Plugin-provided
};

struct ContextChip {
    ContextChipType type;
    std::string     label;      // Display text
    std::string     detail;     // Tooltip/hover info
    std::string     iconId;     // Icon identifier
    std::string     data;       // Associated data (file path, symbol name, etc.)
    bool            removable;  // Can user remove this chip?
};

// ============================================================================
// Composer session state
// ============================================================================
struct ComposerSession {
    uint64_t    sessionId;
    std::string title;                  // Auto-generated or user-set
    int64_t     startedMs;
    int64_t     lastActivityMs;
    
    // Steps
    int         currentStep;
    int         totalSteps;
    int         completedSteps;
    
    // Model info
    std::string modelName;
    int         totalTokensIn;
    int         totalTokensOut;
    float       avgLatencyMs;
    
    // Approval state
    int         pendingApprovals;
    int         approvedChanges;
    int         rejectedChanges;
    
    // Context
    std::vector<ContextChip>    contextChips;
    std::vector<ToolCallDisplay> toolCalls;
    std::vector<FileChangeEntry> fileChanges;
    std::vector<ThinkingBlock>   thinkingBlocks;
};

// ============================================================================
// UI Callbacks — Win32 IDE hooks
// ============================================================================
struct ComposerUICallbacks {
    // Render step in the panel
    std::function<void(int stepIndex, StepState state, const std::string& label)>
        onStepUpdate;
    
    // Show thinking block (streaming)
    std::function<void(uint64_t blockId, const std::string& content, bool isComplete)>
        onThinkingUpdate;
    
    // Show tool call result
    std::function<void(const ToolCallDisplay& toolCall)>
        onToolCallComplete;
    
    // Request approval for file changes
    std::function<void(const std::vector<FileChangeEntry>& changes)>
        onApprovalRequired;
    
    // Update context chips
    std::function<void(const std::vector<ContextChip>& chips)>
        onContextUpdate;
    
    // Progress bar update
    std::function<void(int current, int total, const std::string& label)>
        onProgressUpdate;
    
    // Session status change
    std::function<void(const std::string& status, const std::string& detail)>
        onStatusChange;
    
    // Error display
    std::function<void(const std::string& error, const std::string& suggestion)>
        onError;
};

// ============================================================================
// AgenticComposerUX — Main orchestrator for composer UX
// ============================================================================
class AgenticComposerUX {
public:
    AgenticComposerUX();
    ~AgenticComposerUX();

    // ---- Lifecycle ----
    void Initialize(const ComposerUICallbacks& callbacks);
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // ---- Session Management ----
    uint64_t StartSession(const std::string& prompt, const std::string& model = "");
    void EndSession(uint64_t sessionId);
    ComposerSession* GetCurrentSession();
    const ComposerSession* GetCurrentSession() const;
    std::vector<ComposerSession> GetSessionHistory(size_t maxCount = 50) const;

    // ---- Step Tracking (called by agent loop) ----
    void BeginStep(int stepIndex, const std::string& label);
    void CompleteStep(int stepIndex, bool success, const std::string& result = "");
    void FailStep(int stepIndex, const std::string& error);
    void SetTotalSteps(int total);

    // ---- Thinking Display ----
    uint64_t BeginThinking(const std::string& title);
    void AppendThinking(uint64_t blockId, const std::string& chunk);
    void EndThinking(uint64_t blockId);
    void SetThinkingCollapsed(uint64_t blockId, bool collapsed);

    // ---- Tool Call Visualization ----
    void RecordToolCall(const ToolCallDisplay& toolCall);
    std::vector<ToolCallDisplay> GetToolCalls() const;

    // ---- File Change Approval ----
    void AddFileChange(const FileChangeEntry& change);
    void ApproveFileChange(const std::string& filePath);
    void RejectFileChange(const std::string& filePath);
    void ApproveAllChanges();
    void RejectAllChanges();
    void ApproveHunk(const std::string& filePath, int hunkIndex);
    void RejectHunk(const std::string& filePath, int hunkIndex);
    bool AllChangesReviewed() const;
    std::vector<FileChangeEntry> GetPendingChanges() const;

    // ---- Context Chips ----
    void AddContextChip(const ContextChip& chip);
    void RemoveContextChip(const std::string& label);
    void ClearContextChips();
    std::vector<ContextChip> GetContextChips() const;

    // ---- Progress ----
    void UpdateProgress(int current, int total, const std::string& label);
    void SetBusy(bool busy);
    bool IsBusy() const { return m_busy.load(); }

    // ---- Status ----
    void SetStatus(const std::string& status, const std::string& detail = "");
    void ShowError(const std::string& error, const std::string& suggestion = "");

    // ---- Rendering Helpers (for Win32 paint) ----
    struct RenderMetrics {
        int stepPanelHeight;
        int thinkingPanelHeight;
        int approvalPanelHeight;
        int contextBarHeight;
        int totalHeight;
    };
    RenderMetrics CalculateRenderMetrics(int availableWidth) const;

    // ---- Serialization ----
    std::string SerializeSession() const;
    bool DeserializeSession(const std::string& json);

private:
    std::atomic<bool>           m_initialized{false};
    std::atomic<bool>           m_busy{false};
    ComposerUICallbacks         m_callbacks;
    
    mutable std::mutex          m_sessionMutex;
    ComposerSession             m_currentSession;
    std::vector<ComposerSession> m_sessionHistory;
    
    std::atomic<uint64_t>       m_nextSessionId{1};
    std::atomic<uint64_t>       m_nextThinkingId{1};

    void notifyStepUpdate(int stepIndex, StepState state, const std::string& label);
    void notifyThinkingUpdate(uint64_t blockId, const std::string& content, bool complete);
    void notifyToolCall(const ToolCallDisplay& tc);
    void notifyApprovalRequired();
    void notifyContextUpdate();
    void notifyProgress(int current, int total, const std::string& label);
};

} // namespace Agentic
} // namespace RawrXD
