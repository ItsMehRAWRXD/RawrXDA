#pragma once

// =============================================================================
// Win32IDE_Types.h — POD structs, enums, and event schemas for Win32IDE
//
// Include this header in subsystem files that need to produce or consume
// Win32IDE data types without depending on the full Win32IDE class.
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Required for TerminalPane::ShellType and Win32TerminalManager ptr
#include "Win32TerminalManager.h"

// Required for AgentFailureType used in FailureClassification / FailureIntelligenceRecord
#include "../agent/agentic_failure_detector.hpp"

// Custom window messages (WM_GHOST_TEXT_READY, WM_PLAN_*, WM_AGENT_*)
#include "Win32IDE_Commands.h"

// =============================================================================
// Basic IDE / editor structures
// =============================================================================

struct CodeSnippet {
    std::string name;
    std::string description;
    std::string code;
    std::string trigger;
    std::vector<std::string> placeholders;
};

struct ModuleInfo {
    std::string name;
    std::string version;
    std::string description;
    std::string path;
    bool loaded;
};

struct TerminalPane {
    int id;
    HWND hwnd;
    std::unique_ptr<Win32TerminalManager> manager;
    std::string name;
    Win32TerminalManager::ShellType shellType;
    bool isActive;
    RECT bounds;
};

// =============================================================================
// Git / VCS structures
// =============================================================================

struct GitStatus {
    std::string branch;
    int ahead;
    int behind;
    int modified;
    int added;
    int deleted;
    int untracked;
    bool hasChanges;
    std::string lastCommit;
    std::string lastCommitMessage;
};

struct GitFile {
    std::string path;
    char status;    // M=modified, A=added, D=deleted, ?=untracked
    bool staged;
};

// =============================================================================
// File explorer
// =============================================================================

struct FileExplorerItem {
    std::string name;
    std::string fullPath;
    bool isDirectory;
    bool isModelFile;
    size_t fileSize;
    HTREEITEM hTreeItem;
    std::vector<FileExplorerItem> children;
};

// =============================================================================
// Peek overlay types
// =============================================================================

enum class PeekItemType {
    Definition,
    Reference
};

struct PeekItem {
    std::string title;
    std::string content;
    std::string file;
    int line;
    int column;
    PeekItemType type;
};

// =============================================================================
// Debugger types
// =============================================================================

struct Breakpoint {
    std::string file;
    int line;
    bool enabled;
    std::string condition;
    int hitCount;
};

struct StackFrame {
    std::string function;
    std::string file;
    int line;
    std::map<std::string, std::string> locals;
};

struct Variable {
    std::string name;
    std::string value;
    std::string type;
    bool expanded;
    std::vector<Variable> children;
};

struct WatchItem {
    std::string expression;
    std::string value;
    std::string type;
    bool enabled;
};

// =============================================================================
// Failure intelligence — Phase 4B / 6
// =============================================================================

struct FailureClassification {
    AgentFailureType reason     = AgentFailureType::None;
    float            confidence = 0.0f;   // 0.0–1.0
    std::string      evidence;            // Bounded to 256 chars

    static FailureClassification ok() {
        return { AgentFailureType::None, 1.0f, "" };
    }
    static FailureClassification make(AgentFailureType r, float c, const std::string& e) {
        FailureClassification fc;
        fc.reason     = r;
        fc.confidence = c;
        fc.evidence   = e.size() > 256 ? e.substr(0, 256) : e;
        return fc;
    }
};

struct FailureStats {
    int totalRequests           = 0;
    int totalFailures           = 0;
    int totalRetries            = 0;
    int successAfterRetry       = 0;
    int refusalCount            = 0;
    int hallucinationCount      = 0;
    int formatViolationCount    = 0;
    int infiniteLoopCount       = 0;
    int qualityDegradationCount = 0;
    int emptyResponseCount      = 0;
    int timeoutCount            = 0;
    int toolErrorCount          = 0;
    int invalidOutputCount      = 0;
    int lowConfidenceCount      = 0;
    int safetyViolationCount    = 0;
    int userAbortCount          = 0;
    int retriesDeclined         = 0;
};

// Granular failure sub-reason (finer than AgentFailureType)
enum class FailureReason {
    Unknown             = 0,
    // Refusal
    PolicyRefusal       = 1,
    TaskRefusal         = 2,
    CapabilityRefusal   = 3,
    // Hallucination
    FabricatedAPI       = 10,
    FabricatedFact      = 11,
    SelfContradiction   = 12,
    // Format
    WrongFormat         = 20,
    MissingStructure    = 21,
    ExcessiveVerbosity  = 22,
    // Loop
    TokenRepetition     = 30,
    BlockRepetition     = 31,
    // Quality
    LowDensity          = 40,
    FillerDominant      = 41,
    TooShort            = 42,
    // System
    EmptyOutput         = 50,
    Timeout             = 51,
    ToolError           = 52,
    ContextOverflow     = 53
};

enum class RetryStrategyType {
    None              = 0,
    Rephrase          = 1,
    AddContext        = 2,
    ForceFormat       = 3,
    ReduceScope       = 4,
    AdjustTemperature = 5,
    SplitTask         = 6,
    RetryVerbatim     = 7,
    ToolRetry         = 8
};

struct RetryStrategy {
    RetryStrategyType type            = RetryStrategyType::None;
    int               maxAttempts     = 1;
    float             temperatureOverride = -1.0f;  // -1 = no override
    std::string       promptPrefix;
    std::string       promptSuffix;
    std::string       description;

    std::string typeString() const;
};

struct FailureIntelligenceRecord {
    uint64_t         timestampMs     = 0;
    std::string      promptHash;
    std::string      promptSnippet;  // First 256 chars
    AgentFailureType failureType     = AgentFailureType::EmptyResponse;
    FailureReason    reason          = FailureReason::Unknown;
    RetryStrategyType strategyUsed   = RetryStrategyType::None;
    int              attemptNumber   = 0;
    bool             retrySucceeded  = false;
    std::string      failureDetail;
    std::string      sessionId;

    std::string toMetadataJSON() const;
};

struct FailureReasonStats {
    int               occurrences     = 0;
    int               retriesAttempted = 0;
    int               retriesSucceeded = 0;
    float             avgRetryAttempts = 0.0f;
    RetryStrategyType bestStrategy    = RetryStrategyType::None;
};

// =============================================================================
// Plan Executor types
// =============================================================================

enum class PlanStepType {
    CodeEdit, FileCreate, FileDelete, ShellCommand,
    Analysis, Verification, General
};

enum class PlanStepStatus {
    Pending, Running, Completed, Failed, Skipped
};

enum class PlanStatus {
    None, Generating, AwaitingApproval, Approved, Rejected,
    Executing, Completed, Failed
};

struct PlanStep {
    int            id               = 0;
    std::string    title;
    std::string    description;
    PlanStepType   type             = PlanStepType::General;
    std::string    targetFile;
    int            estimatedMinutes = 0;
    float          confidence       = 0.5f;
    std::string    risk;
    PlanStepStatus status           = PlanStepStatus::Pending;
    std::string    output;
};

struct AgentPlan {
    std::string           goal;
    std::vector<PlanStep> steps;
    PlanStatus            status            = PlanStatus::None;
    int                   currentStepIndex  = -1;
    float                 overallConfidence = 0.0f;
};

// =============================================================================
// IDE settings (persisted to JSON)
// =============================================================================

struct IDESettings {
    // General
    bool        autoSaveEnabled     = false;
    bool        lineNumbersVisible  = true;
    bool        wordWrapEnabled     = false;
    int         fontSize            = 14;
    std::string fontName            = "Consolas";
    std::string workingDirectory;
    int         autoSaveIntervalSec = 60;

    // AI / Model
    float       aiTemperature       = 0.7f;
    float       aiTopP              = 0.9f;
    int         aiTopK              = 40;
    int         aiMaxTokens         = 512;
    int         aiContextWindow     = 4096;
    std::string aiModelPath;
    std::string aiOllamaUrl         = "http://localhost:11434";
    bool        ghostTextEnabled    = true;
    bool        failureDetectorEnabled = true;
    int         failureMaxRetries   = 3;

    // Editor
    int         tabSize             = 4;
    bool        useSpaces           = true;
    std::string encoding            = "UTF-8";
    std::string eolStyle            = "LF";
    bool        syntaxColoringEnabled = true;
    bool        minimapEnabled      = true;
    bool        breadcrumbsEnabled  = true;
    bool        smoothScrollEnabled = true;

    // Theme (IDM_THEME_DARK_PLUS = 3101)
    int         themeId             = 3101;
    BYTE        windowAlpha         = 255;

    // Display scaling (0 = auto/system DPI; 100/125/150 = explicit %)
    int         uiScalePercent      = 0;

    // Local server
    bool        localServerEnabled  = false;
    int         localServerPort     = 11435;

    // Local parity mode (full feature set via local GGUF, no API key)
    bool        localParityEnabled  = false;
    std::string localParityModelPath;
    std::string updateManifestUrl;

    // Agent terminal isolation — when true, agent commands use a
    // dedicated terminal so user sessions are never interrupted.
    bool        agentTerminalIsolated = true;

    // Settings GUI fields
    bool        caretAnimationEnabled  = true;
    bool        autoUpdateCheckEnabled = true;
    bool        showWelcomeOnStartup   = true;
    std::string fileIconTheme          = "seti";
    std::string updateChannel          = "stable";
};

struct LocalServerStats {
    int totalRequests = 0;
    int totalTokens   = 0;
};

// =============================================================================
// Agent History — persisted JSONL event log
// =============================================================================

enum class AgentEventType {
    AgentStarted,
    AgentCompleted,
    AgentFailed,
    SubAgentSpawned,
    SubAgentResult,
    ChainStepStarted,
    ChainStepCompleted,
    SwarmStarted,
    SwarmTaskCompleted,
    SwarmMerged,
    ToolInvoked,
    TodoUpdated,
    PlanGenerated,
    PlanStepExecuted,
    FailureDetected,
    FailureCorrected,
    FailureFailed,
    FailureRetryDeclined,
    GhostTextRequested,
    GhostTextAccepted,
    SettingsChanged,
    SessionEvent
};

struct AgentEvent {
    AgentEventType type;
    uint64_t       timestampMs;   // Epoch ms
    std::string    sessionId;
    std::string    parentId;
    std::string    agentId;
    std::string    prompt;        // May be truncated
    std::string    result;        // May be truncated
    std::string    metadata;      // JSON blob
    int            durationMs;
    bool           success;

    std::string typeString() const;
    std::string toJSONL() const;
    static AgentEvent fromJSONL(const std::string& line);
};

struct AgentHistoryStats {
    int totalEvents         = 0;
    int agentStarted        = 0;
    int agentCompleted      = 0;
    int agentFailed         = 0;
    int subAgentSpawned     = 0;
    int chainSteps          = 0;
    int swarmTasks          = 0;
    int toolInvocations     = 0;
    int failuresDetected    = 0;
    int failuresCorrected   = 0;
    int ghostTextAccepted   = 0;
    int totalDurationMs     = 0;
};
