// ContextFusionEngine.h — Unified semantic state machine for RawrXD IDE
// This is the single source of truth that collapses all IDE intelligence loops.
//
// Architecture:
//   Editor → ContextEvent → ContextFusionEngine → ContextFrame → Subscribers
//   LSP    → ContextEvent →                    →              → Ghost Text
//   AI     → ContextEvent →                    →              → AI Completion
//   Git    → ContextEvent →                    →              → Chat Panel
//                                                   → Agent System
//
// Rule: NO SUBSYSTEM BUILDS ITS OWN CONTEXT. All consume from here.

#pragma once

#include "RawrXD_SignalSlot.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD {

// Forward declarations
class IContextSubscriber;

// ─────────────────────────────────────────────────────────────────────────────
// 1. Core Data Model — ContextFrame (the canonical snapshot)
// ─────────────────────────────────────────────────────────────────────────────

struct EditorPosition {
    int line = 0;
    int column = 0;
    bool operator==(const EditorPosition& other) const {
        return line == other.line && column == other.column;
    }
    bool operator!=(const EditorPosition& other) const {
        return !(*this == other);
    }
};

struct SelectionRange {
    EditorPosition start;
    EditorPosition end;
    std::string selectedText;
    bool isEmpty() const { return start == end; }
};

struct VisibleRange {
    int startLine = 0;
    int endLine = 0;
    int totalLines = 0;
};

struct SymbolInfo {
    std::string name;
    std::string kind;       // "function", "class", "variable", etc.
    int line = 0;
    int column = 0;
    std::string container;  // parent class/namespace
};

struct DiagnosticInfo {
    std::string message;
    std::string severity;   // "error", "warning", "info"
    int line = 0;
    int column = 0;
};

struct AIInteraction {
    std::string prompt;
    std::string response;
    float confidence = 0.0f;
    std::chrono::steady_clock::time_point timestamp;
};

struct GitState {
    std::string branch;
    std::vector<std::string> changedFiles;
    std::vector<std::string> stagedFiles;
    bool hasUncommittedChanges = false;
};

struct ContextFrame {
    // --- Editor State (always source of truth) ---
    std::string filePath;
    std::string bufferText;
    std::string languageId;
    EditorPosition cursor;
    SelectionRange selection;
    VisibleRange visible;
    
    // --- Language Intelligence (LSP enriched) ---
    std::vector<SymbolInfo> symbols;
    std::vector<DiagnosticInfo> diagnostics;
    std::vector<std::string> completions;  // LSP-provided completions
    
    // --- AI State ---
    std::vector<AIInteraction> recentInteractions;
    std::string lastGhostText;
    float lastConfidence = 0.0f;
    
    // --- Git / Project State ---
    GitState git;
    std::string projectRoot;
    
    // --- Runtime Metadata ---
    uint64_t timestamp = 0;
    uint64_t version = 0;
    bool isStale = false;  // true if frame is being rebuilt
    
    // Convenience
    std::string CurrentLine() const;
    std::string CurrentWord() const;
    bool HasSelection() const { return !selection.isEmpty(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// 2. Event Contract — ContextEvent (what subsystems emit)
// ─────────────────────────────────────────────────────────────────────────────

struct ContextEvent {
    enum Type {
        EDITOR_CHANGED,      // buffer text changed
        CURSOR_MOVED,        // cursor position changed
        SELECTION_CHANGED,   // selection range changed
        VISIBLE_RANGE_CHANGED, // scroll/viewport changed
        LSP_UPDATED,         // LSP symbols/diagnostics updated
        AI_RESPONSE,         // AI generated a completion
        AI_ACCEPTED,         // user accepted AI suggestion
        AI_REJECTED,         // user rejected AI suggestion
        GIT_UPDATED,         // git state changed
        FILE_OPENED,         // new file opened
        FILE_SAVED,          // file saved
        AGENT_ACTION,        // agent performed an action
        TOOL_EXECUTED        // tool was executed
    };
    
    Type type;
    uint64_t timestamp;
    std::string source;      // which subsystem emitted this
    void* payload = nullptr; // type-specific data (owned by emitter)
    
    ContextEvent(Type t, const std::string& src, void* p = nullptr)
        : type(t), timestamp(GetTickCount64()), source(src), payload(p) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// 3. Subscriber Interface — IContextSubscriber
// ─────────────────────────────────────────────────────────────────────────────

class IContextSubscriber {
public:
    virtual ~IContextSubscriber() = default;
    
    // Called when context frame changes
    virtual void OnContextUpdate(const ContextFrame& frame) = 0;
    
    // Called when specific event type occurs (optional override)
    virtual void OnContextEvent(const ContextEvent& event) {
        (void)event; // default: ignore
    }
    
    // Priority for dispatch order (lower = earlier)
    virtual int GetPriority() const { return 100; }
    
    // Subscriber name for debugging
    virtual std::string GetName() const { return "UnknownSubscriber"; }
};

// ─────────────────────────────────────────────────────────────────────────────
// 4. Context Fusion Engine — The Brain
// ─────────────────────────────────────────────────────────────────────────────

class ContextFusionEngine {
public:
    static ContextFusionEngine& Get();
    
    // Lifecycle
    void Initialize();
    void Shutdown();
    
    // Event ingestion (called by subsystems)
    void EmitEvent(const ContextEvent& event);
    
    // Frame access
    ContextFrame GetCurrentFrame() const;
    ContextFrame GetFrameCopy() const;  // thread-safe snapshot
    
    // Subscription
    void Subscribe(IContextSubscriber* subscriber);
    void Unsubscribe(IContextSubscriber* subscriber);
    
    // Direct frame mutation (for testing/debugging)
    void InjectFrame(const ContextFrame& frame);
    
    // Statistics
    uint64_t GetFrameVersion() const { return m_currentVersion.load(); }
    size_t GetSubscriberCount() const;
    
    // Validation
    bool ValidateFrameIntegrity(const ContextFrame& frame) const;
    std::string GetFrameDiagnostics(const ContextFrame& frame) const;
    
    // Signals (for reactive wiring)
    Signal<const ContextFrame&> OnFrameUpdated;
    Signal<const ContextEvent&> OnEventReceived;

private:
    ContextFusionEngine() = default;
    ~ContextFusionEngine() = default;
    ContextFusionEngine(const ContextFusionEngine&) = delete;
    ContextFusionEngine& operator=(const ContextFusionEngine&) = delete;
    
    // Merge logic
    void ProcessEvent(const ContextEvent& event);
    void RebuildFrame();
    void BroadcastFrame();
    
    // Priority-ordered subscribers
    std::vector<IContextSubscriber*> GetOrderedSubscribers() const;
    
    mutable std::mutex m_mutex;
    ContextFrame m_currentFrame;
    std::atomic<uint64_t> m_currentVersion{0};
    std::vector<IContextSubscriber*> m_subscribers;
    
    // Event queue for async processing
    std::vector<ContextEvent> m_eventQueue;
    bool m_initialized = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// 5. Helper: ContextFrameBuilder (for subsystems to build partial context)
// ─────────────────────────────────────────────────────────────────────────────

class ContextFrameBuilder {
public:
    ContextFrameBuilder& WithEditorState(
        const std::string& filePath,
        const std::string& bufferText,
        const EditorPosition& cursor,
        const SelectionRange& selection
    );
    
    ContextFrameBuilder& WithLSPData(
        const std::vector<SymbolInfo>& symbols,
        const std::vector<DiagnosticInfo>& diagnostics
    );
    
    ContextFrameBuilder& WithAIState(
        const std::string& lastCompletion,
        float confidence
    );
    
    ContextFrameBuilder& WithGitState(const GitState& git);
    
    ContextFrame Build();
    
private:
    ContextFrame m_frame;
};

} // namespace RawrXD