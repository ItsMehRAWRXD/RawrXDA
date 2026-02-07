#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

// Undefine Windows macros that conflict with our code
#ifdef ERROR
#undef ERROR
#endif

#include <commctrl.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "IDELogger.h"
#include "Win32TerminalManager.h"
#include "TransparentRenderer.h"
#include "../gguf_loader.h"
#include "../streaming_gguf_loader.h"
#include "../model_source_resolver.h"
#include "Win32IDE_AgenticBridge.h"
#include "Win32IDE_Autonomy.h"
#include "Win32IDE_SubAgent.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"

#include "../modules/ExtensionLoader.hpp"
#include <nlohmann/json.hpp>

// Agent and AI IDs
#define IDM_AGENT_START_LOOP 4100
#define IDM_AGENT_EXECUTE_CMD 4101
#define IDM_AGENT_CONFIGURE_MODEL 4102
#define IDM_AGENT_VIEW_TOOLS 4103
#define IDM_AGENT_VIEW_STATUS 4104
#define IDM_AGENT_STOP 4105

// Autonomy IDs
#define IDM_AUTONOMY_TOGGLE 4150
#define IDM_AUTONOMY_START 4151
#define IDM_AUTONOMY_STOP 4152
#define IDM_AUTONOMY_SET_GOAL 4153
#define IDM_AUTONOMY_STATUS 4154
#define IDM_AUTONOMY_MEMORY 4155

// AI Mode IDs
#define IDM_AI_MODE_MAX 4200
#define IDM_AI_MODE_DEEP_THINK 4201
#define IDM_AI_MODE_DEEP_RESEARCH 4202
#define IDM_AI_MODE_NO_REFUSAL 4203

// AI Mode Toggle IDs (Controls)
#define IDC_AI_MAX_MODE 5001
#define IDC_AI_DEEP_THINK 5002
#define IDC_AI_DEEP_RESEARCH 5003
#define IDC_AI_NO_REFUSAL 5004

// Plan Approval Dialog Controls
#define IDC_PLAN_LIST          7001
#define IDC_PLAN_DETAIL        7002
#define IDC_PLAN_GOAL_LABEL    7003
#define IDC_PLAN_SUMMARY_LABEL 7004
#define IDC_PLAN_BTN_APPROVE   7010
#define IDC_PLAN_BTN_EDIT      7011
#define IDC_PLAN_BTN_REJECT    7012
#define IDC_PLAN_BTN_PAUSE     7013
#define IDC_PLAN_BTN_CANCEL    7014
#define IDC_PLAN_PROGRESS      7020
#define IDC_PLAN_PROGRESS_LABEL 7021

// Retry Approval Controls (inside Plan Dialog)
#define IDC_PLAN_BTN_RETRY_YES 7030
#define IDC_PLAN_BTN_RETRY_NO  7031
#define IDC_PLAN_RETRY_LABEL   7032

// Context Window IDs
#define IDM_AI_CONTEXT_4K 4210
#define IDM_AI_CONTEXT_32K 4211
#define IDM_AI_CONTEXT_64K 4212
#define IDM_AI_CONTEXT_128K 4213
#define IDM_AI_CONTEXT_256K 4214
#define IDM_AI_CONTEXT_512K 4215
#define IDM_AI_CONTEXT_1M 4216

// Reverse Engineering IDs
#define IDM_REVENG_ANALYZE 4300
#define IDM_REVENG_DISASM 4301
#define IDM_REVENG_DUMPBIN 4302
#define IDM_REVENG_COMPILE 4303
#define IDM_REVENG_COMPARE 4304
#define IDM_REVENG_DETECT_VULNS 4305
#define IDM_REVENG_EXPORT_IDA 4306
#define IDM_REVENG_EXPORT_GHIDRA 4307

// Define LOG_FUNCTION macro if not already defined
#ifndef LOG_FUNCTION
#define LOG_FUNCTION() LOG_DEBUG(std::string("ENTER ") + __FUNCTION__)
#endif

// Theme and customization structures
struct IDETheme {
    std::string name;                   // Display name ("Dracula", "Nord", etc.)
    bool darkMode;

    // Core editor
    COLORREF backgroundColor;            // Editor pane background
    COLORREF textColor;                  // Default text / foreground
    COLORREF keywordColor;
    COLORREF commentColor;
    COLORREF stringColor;
    COLORREF numberColor;
    COLORREF operatorColor;
    COLORREF preprocessorColor;
    COLORREF functionColor;
    COLORREF typeColor;                  // Built-in types / classes
    COLORREF selectionColor;             // Selection highlight
    COLORREF selectionTextColor;         // Text on selected background
    COLORREF lineNumberColor;            // Gutter line numbers
    COLORREF lineNumberBg;               // Gutter background
    COLORREF currentLineBg;              // Active line highlight
    COLORREF cursorColor;

    // Sidebar / Activity bar
    COLORREF sidebarBg;
    COLORREF sidebarFg;
    COLORREF sidebarHeaderBg;
    COLORREF activityBarBg;
    COLORREF activityBarFg;
    COLORREF activityBarIndicator;       // Active-tab accent stripe
    COLORREF activityBarHoverBg;

    // Tab bar
    COLORREF tabBarBg;
    COLORREF tabActiveBg;
    COLORREF tabActiveFg;
    COLORREF tabInactiveBg;
    COLORREF tabInactiveFg;
    COLORREF tabBorder;

    // Status bar
    COLORREF statusBarBg;
    COLORREF statusBarFg;
    COLORREF statusBarAccent;            // Remote / debug indicator

    // Terminal / Output panel
    COLORREF panelBg;
    COLORREF panelFg;
    COLORREF panelBorder;
    COLORREF panelHeaderBg;

    // Title bar
    COLORREF titleBarBg;
    COLORREF titleBarFg;

    // Scrollbar
    COLORREF scrollbarBg;
    COLORREF scrollbarThumb;
    COLORREF scrollbarThumbHover;

    // Bracket matching / indent guides
    COLORREF bracketMatchBg;
    COLORREF indentGuideColor;

    // Accent / brand
    COLORREF accentColor;                // Primary brand accent
    COLORREF errorColor;                 // Squiggle / diagnostic
    COLORREF warningColor;
    COLORREF infoColor;

    // Font
    std::string fontName;
    int fontSize;

    // Transparency (0 = fully transparent, 255 = opaque)
    BYTE windowAlpha;

    // Per-language syntax palette overrides (optional)
    // When a language key is present, its non-zero fields override the
    // theme-global keyword/comment/string/etc colors in getTokenColor().
    struct LanguageTokenPalette {
        COLORREF keywordColor;       // 0 = use theme global
        COLORREF commentColor;
        COLORREF stringColor;
        COLORREF numberColor;
        COLORREF operatorColor;
        COLORREF preprocessorColor;
        COLORREF functionColor;
        COLORREF typeColor;
        COLORREF bracketColor;
        COLORREF textColor;          // Default text
    };
    std::map<int, LanguageTokenPalette> languagePalettes; // Keyed by SyntaxLanguage enum cast to int
};

// ============================================================================
// THEME COMMAND IDS (3100 range — routed via handleViewCommand)
// ============================================================================
#define IDM_THEME_BASE              3100
#define IDM_THEME_DARK_PLUS         3101
#define IDM_THEME_LIGHT_PLUS        3102
#define IDM_THEME_MONOKAI           3103
#define IDM_THEME_DRACULA           3104
#define IDM_THEME_NORD              3105
#define IDM_THEME_SOLARIZED_DARK    3106
#define IDM_THEME_SOLARIZED_LIGHT   3107
#define IDM_THEME_CYBERPUNK_NEON    3108
#define IDM_THEME_GRUVBOX_DARK      3109
#define IDM_THEME_CATPPUCCIN_MOCHA  3110
#define IDM_THEME_TOKYO_NIGHT       3111
#define IDM_THEME_RAWRXD_CRIMSON    3112
#define IDM_THEME_HIGH_CONTRAST     3113
#define IDM_THEME_ONE_DARK_PRO      3114
#define IDM_THEME_SYNTHWAVE84       3115
#define IDM_THEME_ABYSS             3116
#define IDM_THEME_END               3117

// Transparency commands (3200 range — routed via handleViewCommand)
#define IDM_TRANSPARENCY_100        3200
#define IDM_TRANSPARENCY_90         3201
#define IDM_TRANSPARENCY_80         3202
#define IDM_TRANSPARENCY_70         3203
#define IDM_TRANSPARENCY_60         3204
#define IDM_TRANSPARENCY_50         3205
#define IDM_TRANSPARENCY_40         3206
#define IDM_TRANSPARENCY_CUSTOM     3210
#define IDM_TRANSPARENCY_TOGGLE     3211

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
    char status;  // M=modified, A=added, D=deleted, ?=untracked
    bool staged;
};

struct FileExplorerItem {
    std::string name;
    std::string fullPath;
    bool isDirectory;
    bool isModelFile;
    size_t fileSize;
    HTREEITEM hTreeItem;
    std::vector<FileExplorerItem> children;
};

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

// ============================================================================
// Ghost Text / Inline Completions — WM_APP message
// ============================================================================
#define WM_GHOST_TEXT_READY (WM_APP + 400)

// ============================================================================
// Agent Failure Detection Enums & Structures
// ============================================================================
enum class AgentFailureType {
    None = 0,
    Refusal,
    Hallucination,
    FormatViolation,
    InfiniteLoop,
    QualityDegradation,
    EmptyResponse,
    Timeout,
    ToolError,
    InvalidOutput,
    LowConfidence,
    SafetyViolation,
    UserAbort
};

// ── Phase 4B: Failure Classification with confidence + evidence ──
struct FailureClassification {
    AgentFailureType reason    = AgentFailureType::None;
    float            confidence = 0.0f;   // 0.0–1.0
    std::string      evidence;             // Bounded short string

    static FailureClassification ok() { return { AgentFailureType::None, 1.0f, "" }; }
    static FailureClassification make(AgentFailureType r, float c, const std::string& e) {
        FailureClassification fc;
        fc.reason     = r;
        fc.confidence = c;
        fc.evidence   = e.size() > 256 ? e.substr(0, 256) : e;
        return fc;
    }
};

struct FailureStats {
    int totalRequests       = 0;
    int totalFailures       = 0;
    int totalRetries        = 0;
    int successAfterRetry   = 0;
    int refusalCount        = 0;
    int hallucinationCount  = 0;
    int formatViolationCount = 0;
    int infiniteLoopCount   = 0;
    int qualityDegradationCount = 0;
    int emptyResponseCount  = 0;
    int timeoutCount        = 0;
    int toolErrorCount      = 0;
    int invalidOutputCount  = 0;
    int lowConfidenceCount  = 0;
    int safetyViolationCount = 0;
    int userAbortCount      = 0;
    int retriesDeclined     = 0;
};

// ============================================================================
// Failure Intelligence — Phase 6: Granular Classification + Retry Strategies
// ============================================================================

// Granular failure reason — finer than AgentFailureType
enum class FailureReason {
    Unknown             = 0,
    // Refusal sub-types
    PolicyRefusal       = 1,    // Model's safety policy blocked the output
    TaskRefusal         = 2,    // Model explicitly declined the task
    CapabilityRefusal   = 3,    // Model says it cannot do the task
    // Hallucination sub-types
    FabricatedAPI       = 10,   // Made-up function/library names
    FabricatedFact      = 11,   // Incorrect factual claims
    SelfContradiction   = 12,   // Output contradicts itself
    // Format sub-types
    WrongFormat         = 20,   // JSON expected but got prose, etc.
    MissingStructure    = 21,   // Required fields/sections missing
    ExcessiveVerbosity  = 22,   // Too much explanation when code-only requested
    // Loop sub-types
    TokenRepetition     = 30,   // Same tokens repeated (stuck decoding)
    BlockRepetition     = 31,   // Same paragraphs/blocks repeated
    // Quality sub-types
    LowDensity          = 40,   // Very low information density
    FillerDominant      = 41,   // Output dominated by filler/stop words
    TooShort            = 42,   // Response suspiciously short for the task
    // System-level
    EmptyOutput         = 50,   // Agent returned nothing
    Timeout             = 51,   // Inference timed out
    ToolError           = 52,   // Tool invocation failed
    ContextOverflow     = 53    // Prompt exceeded context window
};

// Retry strategy types — what to do when a failure occurs
enum class RetryStrategyType {
    None                = 0,    // No retry — accept the failure
    Rephrase            = 1,    // Rephrase the prompt to avoid the trigger
    AddContext           = 2,    // Inject grounding context / examples
    ForceFormat         = 3,    // Add explicit format instructions
    ReduceScope         = 4,    // Shorten/simplify the prompt
    AdjustTemperature   = 5,    // Lower temperature for more deterministic output
    SplitTask           = 6,    // Break the task into smaller sub-tasks
    RetryVerbatim       = 7,    // Retry the exact same prompt (transient failures)
    ToolRetry           = 8     // Retry only the failed tool call
};

// A single retry strategy definition
struct RetryStrategy {
    RetryStrategyType type      = RetryStrategyType::None;
    int maxAttempts             = 1;        // Max times this strategy can be attempted
    float temperatureOverride   = -1.0f;    // -1 = no override
    std::string promptPrefix;               // Prepended to the prompt on retry
    std::string promptSuffix;               // Appended to the prompt on retry
    std::string description;                // Human-readable description for UI

    std::string typeString() const;
};

// A record of a specific failure + its classification, stored in history
struct FailureIntelligenceRecord {
    uint64_t timestampMs        = 0;
    std::string promptHash;                 // SHA-like hash of the prompt (for matching)
    std::string promptSnippet;              // First 256 chars of prompt
    AgentFailureType failureType = AgentFailureType::EmptyResponse;
    FailureReason reason        = FailureReason::Unknown;
    RetryStrategyType strategyUsed = RetryStrategyType::None;
    int attemptNumber           = 0;
    bool retrySucceeded         = false;
    std::string failureDetail;              // Human-readable detail
    std::string sessionId;

    std::string toMetadataJSON() const;
};

// Per-reason aggregated stats
struct FailureReasonStats {
    int occurrences             = 0;
    int retriesAttempted        = 0;
    int retriesSucceeded        = 0;
    float avgRetryAttempts      = 0.0f;
    RetryStrategyType bestStrategy = RetryStrategyType::None;
};

// ============================================================================
// Plan Executor Enums & Structures
// ============================================================================
#define WM_PLAN_READY     (WM_APP + 500)
#define WM_PLAN_STEP_DONE (WM_APP + 501)
#define WM_PLAN_COMPLETE  (WM_APP + 502)

enum class PlanStepType {
    CodeEdit,
    FileCreate,
    FileDelete,
    ShellCommand,
    Analysis,
    Verification,
    General
};

enum class PlanStepStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Skipped
};

enum class PlanStatus {
    None,
    Generating,
    AwaitingApproval,
    Approved,
    Rejected,
    Executing,
    Completed,
    Failed
};

struct PlanStep {
    int id                 = 0;
    std::string title;
    std::string description;
    PlanStepType type      = PlanStepType::General;
    std::string targetFile;
    int estimatedMinutes   = 0;
    float confidence       = 0.5f;
    std::string risk;
    PlanStepStatus status  = PlanStepStatus::Pending;
    std::string output;
};

struct AgentPlan {
    std::string goal;
    std::vector<PlanStep> steps;
    PlanStatus status      = PlanStatus::None;
    int currentStepIndex   = -1;
    float overallConfidence = 0.0f;
};

// ============================================================================
// IDE Settings Structure
// ============================================================================
struct IDESettings {
    // General
    bool autoSaveEnabled        = false;
    bool lineNumbersVisible     = true;
    bool wordWrapEnabled        = false;
    int fontSize                = 14;
    std::string fontName        = "Consolas";
    std::string workingDirectory;
    int autoSaveIntervalSec     = 60;

    // AI / Model
    float aiTemperature         = 0.7f;
    float aiTopP                = 0.9f;
    int aiTopK                  = 40;
    int aiMaxTokens             = 512;
    int aiContextWindow         = 4096;
    std::string aiModelPath;
    std::string aiOllamaUrl     = "http://localhost:11434";
    bool ghostTextEnabled       = true;
    bool failureDetectorEnabled = true;
    int failureMaxRetries       = 3;

    // Editor
    int tabSize                 = 4;
    bool useSpaces              = true;
    std::string encoding        = "UTF-8";
    std::string eolStyle        = "LF";
    bool syntaxColoringEnabled  = true;
    bool minimapEnabled         = true;

    // Theme
    int themeId                 = 3101; // IDM_THEME_DARK_PLUS
    BYTE windowAlpha            = 255;

    // Server
    bool localServerEnabled     = false;
    int localServerPort         = 11435;
};

// ============================================================================
// Local Server Statistics
// ============================================================================
struct LocalServerStats {
    int totalRequests = 0;
    int totalTokens   = 0;
};

// ============================================================================
// Agent History — Persisted Event Schema (JSONL append-only log)
// ============================================================================
#define WM_AGENT_HISTORY_REPLAY_DONE (WM_APP + 600)

enum class AgentEventType {
    AgentStarted,           // Agent received a prompt
    AgentCompleted,         // Agent returned a result
    AgentFailed,            // Agent encountered an error
    SubAgentSpawned,        // SubAgentManager spawned a child
    SubAgentResult,         // SubAgent returned a result
    ChainStepStarted,       // Chain pipeline step began
    ChainStepCompleted,     // Chain pipeline step finished
    SwarmStarted,           // HexMag swarm fan-out began
    SwarmTaskCompleted,     // One swarm task finished
    SwarmMerged,            // Swarm merge completed
    ToolInvoked,            // Agent invoked a tool (file edit, shell, etc.)
    TodoUpdated,            // Todo list item status changed
    PlanGenerated,          // Plan executor generated a plan
    PlanStepExecuted,       // Plan executor completed a step
    FailureDetected,        // Failure detector flagged an issue
    FailureCorrected,       // Failure detector correction succeeded
    FailureFailed,          // Failure detector correction failed after retry
    FailureRetryDeclined,   // User declined a retry proposal
    GhostTextRequested,     // Ghost text completion was requested
    GhostTextAccepted,      // Ghost text completion was accepted
    SettingsChanged,        // Settings were modified
    SessionEvent            // Generic session-level event (startup, shutdown)
};

struct AgentEvent {
    AgentEventType type;
    uint64_t timestampMs;       // Epoch milliseconds
    std::string sessionId;      // Unique per IDE launch
    std::string parentId;       // Agent/subagent parent ID (empty for root)
    std::string agentId;        // Agent or subagent ID
    std::string prompt;         // Input prompt (may be truncated)
    std::string result;         // Output result (may be truncated)
    std::string metadata;       // JSON string with type-specific extra data
    int durationMs;             // Elapsed time for the event (0 if instantaneous)
    bool success;               // Whether the operation succeeded

    std::string typeString() const;
    std::string toJSONL() const;
    static AgentEvent fromJSONL(const std::string& line);
};

// Agent History statistics for the current session
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

class Win32IDE
{
    friend class AgenticBridge;

public:
    enum class OutputSeverity {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
    };

    enum class PanelTab {
        Terminal = 0,
        Output = 1,
        Problems = 2,
        DebugConsole = 3
    };

    Win32IDE(HINSTANCE hInstance);
    ~Win32IDE();

    void setEngineManager(EngineManager* mgr) { m_engineManager = mgr; }
    void setCodexUltimate(CodexUltimate* codex) { m_codexUltimate = codex; }

    bool createWindow();
    void showWindow();
    int runMessageLoop();
    void openModel();
    bool loadModelForInference(const std::string& filepath);

    // Test agent access
    HWND getMainWindow() const { return m_hwndMain; }
    HWND getToolbar() const { return m_hwndToolbar; }
    HWND getSidebar() const { return m_hwndSidebar; }
    HWND getEditor() const { return m_hwndEditor; }
    HWND getStatusBar() const { return m_hwndStatusBar; }
    HWND getActivityBar() const { return m_hwndActivityBar; }
    HWND getLineNumbers() const { return m_hwndLineNumbers; }
    HWND getTabBar() const { return m_hwndTabBar; }

    // Agentic Framework
    std::unique_ptr<AgenticBridge> m_agenticBridge;
    void initializeAgenticBridge();
    void initializeAutonomy();
    void onAgentStartLoop();
    void onAgentExecuteCommand();
    void onAgentConfigureModel();
    void onAgentViewTools();
    void onAgentViewStatus();
    void onAgentStop();

    // Autonomy Framework Controls
    std::unique_ptr<AutonomyManager> m_autonomyManager; // high-level autonomous orchestrator
    void onAutonomyStart();
    void onAutonomyStop();
    void onAutonomyToggle();
    void onAutonomySetGoal();
    void onAutonomyViewStatus();
    void onAutonomyViewMemory();

    // AI Extended Features Handlers
    void onAIModeMax();
    void onAIModeDeepThink();
    void onAIModeDeepResearch();
    void onAIModeNoRefusal();
    void onAIContextSize(int sizeEnum);
    
    // Memory Plugin System (Native VSIX Style)
    void loadMemoryPlugin(const std::string& path);

    // Comprehensive Logging System
    void initializeLogging();
    void shutdownLogging();
    void logMessage(const std::string& category, const std::string& message);
    void logFunction(const std::string& functionName);
    void logError(const std::string& functionName, const std::string& error);
    void logWarning(const std::string& functionName, const std::string& warning);
    void logInfo(const std::string& message);
    void logWindowCreate(const std::string& windowName, HWND hwnd);
    void logWindowDestroy(const std::string& windowName, HWND hwnd);
    void logFileOperation(const std::string& operation, const std::string& filePath, bool success);
    void logUIEvent(const std::string& event, const std::string& details);

private:
    EngineManager* m_engineManager = nullptr;
    CodexUltimate* m_codexUltimate = nullptr;

    // Window procedure
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Message handlers
    LRESULT handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void onCreate(HWND hwnd);
    void deferredHeavyInit();
    void onDestroy();
    void onSize(int width, int height);
    void onCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify);
    void onTerminalOutput(int paneId, const std::string& output);
    void onTerminalError(int paneId, const std::string& error);

    // UI creation
    void createMenuBar(HWND hwnd);
    void createToolbar(HWND hwnd);
    void createSidebar(HWND hwnd);
    void createTitleBarControls();
    void layoutTitleBar(int width);
    void updateTitleBarText();
    std::string extractLeafName(const std::string& path) const;
    void setCurrentDirectoryFromFile(const std::string& filePath);
    void createEditor(HWND hwnd);
    void createTerminal(HWND hwnd);
    void createStatusBar(HWND hwnd);

    // File operations (9 features)
    void newFile();
    void openFile();
    void openFile(const std::string& filePath);
    void openFileDialog();
    void openRecentFile(int index);
    bool saveFile();
    bool saveFileAs();
    void saveAll();
    void closeFile();
    bool promptSaveChanges();
    void updateRecentFiles(const std::string& filePath);
    void loadRecentFiles();
    void saveRecentFiles();
    void clearRecentFiles();
    std::string getFileDialogPath(bool isSave = false);
    
    // GGUF Model operations
    bool loadGGUFModel(const std::string& filepath);
    std::string getModelInfo() const;
    bool loadTensorData(const std::string& tensorName, std::vector<uint8_t>& data);
    
    // Unified model source resolution (HuggingFace, Ollama blobs, HTTP, local files)
    void openModelFromHuggingFace();
    void openModelFromOllama();
    void openModelFromURL();
    void openModelUnified();
    bool resolveAndLoadModel(const std::string& input);
    
    // Streaming / Model UX — progress, cancellation, status feedback
    void showModelProgressBar(const std::string& operation);
    void updateModelProgress(float percent, const std::string& statusText);
    void hideModelProgressBar();
    void cancelModelOperation();
    bool isModelOperationInProgress() const;
    void showModelStatus(const std::string& text, int durationMs = 5000);
    static LRESULT CALLBACK ModelProgressProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // SubAgent / Swarm UX — progress, status display
    void showSubAgentProgress(const std::string& operation, int totalTasks);
    void updateSubAgentProgress(int completedTasks, int totalTasks, const std::string& currentTask);
    void hideSubAgentProgress();
    void showSwarmStatus();

    // AI Inference Engine - Local GGUF Model Chat
    struct InferenceConfig {
        int maxTokens = 512;
        int contextWindow = 4096; // Added Context
        float temperature = 0.7f;
        float topP = 0.9f;
        int topK = 40;
        float repetitionPenalty = 1.1f;
        std::string systemPrompt;
        bool streamOutput = true;
    };
    
    bool initializeInference();
    void shutdownInference();
    bool isModelLoaded() const;
    std::string generateResponse(const std::string& prompt);
    void generateResponseAsync(const std::string& prompt, std::function<void(const std::string&, bool)> callback);
    void stopInference();
    void setInferenceConfig(const InferenceConfig& config);
    InferenceConfig getInferenceConfig() const;
    void updateContextSliderLabel();
    std::string buildChatPrompt(const std::string& userMessage);
    void onInferenceToken(const std::string& token);
    void onInferenceComplete(const std::string& fullResponse);

    // Editor Operations
    void undo();
    void redo();
    void editCut();
    void editCopy();
    void editPaste();
    
    // View Operations
    void toggleOutputPanel();
    void toggleTerminal();
    void showAbout();

    // Terminal operations (original)
    void startPowerShell();
    void startCommandPrompt();
    void stopTerminal();
    void executeCommand();
    
    // ========================================================================
    // FULL POWERSHELL ACCESS - Complete PowerShell Integration
    // ========================================================================
    
    // PowerShell Execution
    std::string executePowerShellScript(const std::string& scriptPath, const std::vector<std::string>& args = {});
    std::string executePowerShellCommand(const std::string& command, bool async = false);
    std::string invokePowerShellCmdlet(const std::string& cmdlet, const std::map<std::string, std::string>& parameters = {});
    
    // PowerShell Pipeline Support
    std::string executePowerShellPipeline(const std::vector<std::string>& commands);
    std::string pipeToPowerShell(const std::string& input, const std::string& command);
    
    // PowerShell Module Management
    std::vector<std::string> getPowerShellModules();
    bool importPowerShellModule(const std::string& moduleName);
    bool removePowerShellModule(const std::string& moduleName);
    std::string getPowerShellModuleInfo(const std::string& moduleName);
    
    // PowerShell Variable Access
    std::string getPowerShellVariable(const std::string& varName);
    bool setPowerShellVariable(const std::string& varName, const std::string& value);
    std::map<std::string, std::string> getAllPowerShellVariables();
    
    // PowerShell Function Invocation
    std::string invokePowerShellFunction(const std::string& functionName, const std::vector<std::string>& args = {});
    bool definePowerShellFunction(const std::string& functionName, const std::string& functionBody);
    std::vector<std::string> listPowerShellFunctions();
    
    // PowerShell Remoting
    bool enterPowerShellRemoteSession(const std::string& computerName, const std::string& credential = "");
    void exitPowerShellRemoteSession();
    std::string invokePowerShellRemoteCommand(const std::string& computerName, const std::string& command);
    
    // PowerShell Object Manipulation
    std::string convertToPowerShellJson(const std::string& object);
    std::string convertFromPowerShellJson(const std::string& json);
    std::string selectPowerShellObject(const std::string& inputObject, const std::vector<std::string>& properties);
    std::string wherePowerShellObject(const std::string& inputObject, const std::string& filter);
    
    // PowerShell Script Analysis
    struct PSScriptAnalysis {
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<std::string> information;
        bool hasErrors;
        int errorCount;
        int warningCount;
    };
    PSScriptAnalysis analyzePowerShellScript(const std::string& scriptPath);
    std::vector<std::string> getPowerShellCommandSyntax(const std::string& cmdlet);
    
    // PowerShell Provider Access
    std::vector<std::string> getPowerShellProviders();
    std::string getPowerShellDrive(const std::string& driveName);
    std::vector<std::string> listPowerShellDrives();
    bool newPowerShellDrive(const std::string& name, const std::string& root, const std::string& provider = "FileSystem");
    
    // PowerShell Job Management
    int startPowerShellJob(const std::string& scriptBlock, const std::string& name = "");
    std::string getPowerShellJobStatus(int jobId);
    std::string receivePowerShellJob(int jobId);
    bool removePowerShellJob(int jobId);
    std::vector<int> listPowerShellJobs();
    bool waitPowerShellJob(int jobId, int timeoutMs = -1);
    
    // PowerShell Transcription
    bool startPowerShellTranscript(const std::string& path);
    bool stopPowerShellTranscript();
    std::string getPowerShellHistory(int count = 100);
    void clearPowerShellHistory();
    
    // PowerShell Debugger Integration
    bool setPowerShellBreakpoint(const std::string& scriptPath, int line);
    bool removePowerShellBreakpoint(int breakpointId);
    std::vector<int> listPowerShellBreakpoints();
    bool enablePowerShellDebugMode();
    void disablePowerShellDebugMode();
    
    // PowerShell Help System
    std::string getPowerShellHelp(const std::string& cmdlet, bool detailed = false, bool examples = false);
    std::vector<std::string> searchPowerShellHelp(const std::string& query);
    std::string getPowerShellAboutTopic(const std::string& topic);
    
    // PowerShell Configuration
    std::string getPowerShellVersion();
    std::string getPowerShellEdition();
    std::string getPowerShellExecutionPolicy();
    bool setPowerShellExecutionPolicy(const std::string& policy);
    std::map<std::string, std::string> getPowerShellEnvironmentVariables();
    bool setPowerShellEnvironmentVariable(const std::string& name, const std::string& value);
    
    // PowerShell Event Handling
    bool registerPowerShellEvent(const std::string& sourceIdentifier, const std::string& eventName, const std::string& action);
    bool unregisterPowerShellEvent(const std::string& sourceIdentifier);
    std::vector<std::string> getPowerShellEvents();
    
    // PowerShell Profile Management
    std::string getPowerShellProfilePath();
    bool editPowerShellProfile();
    bool reloadPowerShellProfile();
    
    // PowerShell Output Formatting
    std::string formatPowerShellTable(const std::string& data, const std::vector<std::string>& properties = {});
    std::string formatPowerShellList(const std::string& data);
    std::string formatPowerShellWide(const std::string& data, int columns = 2);
    std::string formatPowerShellCustom(const std::string& data, const std::string& formatString);
    
    // PowerShell Workflow Integration
    bool importPowerShellWorkflow(const std::string& workflowPath);
    std::string executePowerShellWorkflow(const std::string& workflowName, const std::map<std::string, std::string>& parameters = {});
    
    // Direct RawrXD.ps1 Integration
    bool loadRawrXDPowerShellModule();
    std::string invokeRawrXDFunction(const std::string& functionName, const std::vector<std::string>& args = {});
    std::string getRawrXDAgentTools();
    bool executeRawrXDAgenticCommand(const std::string& command);
    std::string getRawrXDModelStatus();
    bool loadRawrXDGGUFModel(const std::string& modelPath, int maxZoneMB = 512);
    std::string invokeRawrXDInference(const std::string& prompt, int maxTokens = 100);
    
    // Terminal Integration (5 features - Split panes, multiple terminals)
    int createTerminalPane(Win32TerminalManager::ShellType shellType, const std::string& name = "");
    void splitTerminalHorizontal();
    void splitTerminalVertical();
    void switchTerminalPane(int paneId);
    void closeTerminalPane(int paneId);
    void resizeTerminalPanes();
    void clearAllTerminals();
    void sendToAllTerminals(const std::string& command);
    TerminalPane* getActiveTerminalPane();
    void layoutTerminalPanes(int width, int top, int height);
    
    // Git Integration (7 features - Status, commit, push, pull)
    void updateGitStatus();
    void showGitStatus();
    void gitCommit(const std::string& message);
    void gitPush();
    void gitPull();
    void gitStageFile(const std::string& filePath);
    void gitUnstageFile(const std::string& filePath);
    void showGitPanel();
    void refreshGitPanel();
    bool isGitRepository() const;
    std::string getCurrentGitBranch() const;
    std::vector<GitFile> getGitChangedFiles() const;
    bool executeGitCommand(const std::string& command, std::string& output);
    void showCommitDialog();
    
    // Menu Command System (25 features)
    void handleFileCommand(int commandId);
    void handleEditCommand(int commandId);
    void handleViewCommand(int commandId);
    void handleTerminalCommand(int commandId);
    void handleToolsCommand(int commandId);
    void handleModulesCommand(int commandId);
    void handleHelpCommand(int commandId);
    void handleGitCommand(int commandId);
    void handleAgentCommand(int commandId);
    
    // Reverse Engineering Menu Handlers
    void handleReverseEngineeringAnalyze();
    void handleReverseEngineeringDisassemble();
    void handleReverseEngineeringDumpBin();
    void handleReverseEngineeringCompile();
    void handleReverseEngineeringCompare();
    void handleReverseEngineeringDetectVulns();
    void handleReverseEngineeringExportIDA();
    void handleReverseEngineeringExportGhidra();
    
    HMENU createReverseEngineeringMenu(); // Helper to add the menu

    // Command routing
    bool routeCommand(int commandId);
    std::string getCommandDescription(int commandId) const;
    bool isCommandEnabled(int commandId) const;
    void updateCommandStates();
    void updateMenuEnableStates();

    // Theme and customization
    void loadTheme(const std::string& themeName);
    void saveTheme(const std::string& themeName);
    void applyTheme();
    void applyThemeToAllControls();          // Deep apply: sidebar, activity bar, tabs, status bar, panels
    void showThemeEditor();
    void showThemePicker();                   // Proper picker dialog with preview
    void logThemeDiff(const IDETheme& before, const IDETheme& candidate) const;  // Debug diff during preview
    void resetToDefaultTheme();
    void populateBuiltinThemes();             // Register all 16 built-in themes
    IDETheme getBuiltinTheme(int themeId) const;  // Factory method
    void applyThemeById(int themeId);         // IDM_THEME_xxx → apply
    void setWindowTransparency(BYTE alpha);   // WS_EX_LAYERED alpha
    void showTransparencySlider();            // Custom slider dialog
    void buildThemeMenu(HMENU hParentMenu);   // Build Appearance submenu
    COLORREF blendColor(COLORREF base, COLORREF overlay, float t) const;  // Utility

    // Grant dialog procs access to private members
    friend INT_PTR CALLBACK TransparencyDlgProc(HWND, UINT, WPARAM, LPARAM);
    friend INT_PTR CALLBACK ThemePickerDlgProc(HWND, UINT, WPARAM, LPARAM);
    friend class AgenticBridge;  // AgenticBridge needs failure hooks + failureTypeString

    // Theme session persistence
    void saveSessionTheme(nlohmann::json& session);
    void restoreSessionTheme(const nlohmann::json& session);

    // Code snippets
    void loadCodeSnippets();
    void saveCodeSnippets();
    void insertSnippet(const std::string& trigger);
    void showSnippetManager();
    void createSnippet();

    // Integrated help
    void showGetHelp(const std::string& cmdlet = "");
    void showCommandReference();
    void showPowerShellDocs();
    void searchHelp(const std::string& query);

    // Enhanced output panel
    void createOutputTabs();
    void addOutputTab(const std::string& name);
    void appendToOutput(const std::string& text, const std::string& tabName = "General", OutputSeverity severity = OutputSeverity::Info);
    void clearOutput(const std::string& tabName = "General");
    void formatOutput(const std::string& text, COLORREF color, const std::string& tabName = "");

    // Enhanced clipboard
    void copyWithFormatting();
    void pasteWithoutFormatting();
    void copyLineNumbers();
    void showClipboardHistory();
    void clearClipboardHistory();

    // ========================================================================
    // INCREMENTAL SYNTAX COLORING (RichEdit-based)
    // ========================================================================
    enum class TokenType {
        Default = 0,
        Keyword,
        BuiltinType,
        String,
        Comment,
        Number,
        Preprocessor,
        Operator,
        Function,
        Bracket
    };
    struct SyntaxToken {
        int start;          // Character offset in the editor
        int length;         // Token length
        TokenType type;     // Token classification
    };
    enum class SyntaxLanguage {
        None = 0,
        Cpp,
        Python,
        JavaScript,
        PowerShell,
        JSON,
        Markdown,
        Assembly
    };

    // Visible-range descriptor for syntax coloring optimization
    struct VisibleLineRange {
        int firstLine;      // First visible line (0-based)
        int lastLine;       // Last visible line (0-based)
        int lineCount;      // Total visible line count (including margin)
        int lineHeight;     // Pixel height per line
    };

    // Syntax coloring methods
    void initSyntaxColorizer();
    void applySyntaxColoring();
    void applySyntaxColoringForRange(int startChar, int endChar);
    void applySyntaxColoringForVisibleRange();  // Named wrapper — colors only the visible range
    VisibleLineRange getVisibleEditorLines() const;  // Accessor for the visible line range
    SyntaxLanguage getCurrentSyntaxLanguage() const;  // Accessor for the detected language enum
    std::vector<SyntaxToken> tokenizeLine(const std::string& line, int lineStartOffset, SyntaxLanguage lang);
    std::vector<SyntaxToken> tokenizeDocument(const std::string& text, SyntaxLanguage lang);
    COLORREF getTokenColor(TokenType type) const;
    SyntaxLanguage detectLanguageFromExtension(const std::string& filePath) const;
    void onEditorContentChanged();  // Debounced EN_CHANGE handler for syntax coloring
    bool isKeyword(const std::string& word, SyntaxLanguage lang) const;
    bool isBuiltinType(const std::string& word, SyntaxLanguage lang) const;
    static void CALLBACK SyntaxColorTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);

    // Syntax coloring state
    static const UINT_PTR SYNTAX_COLOR_TIMER_ID = 7777;
    static const UINT SYNTAX_COLOR_DELAY_MS = 80;
    bool m_syntaxColoringEnabled;
    bool m_syntaxDirty;
    SyntaxLanguage m_syntaxLanguage;
    bool m_inBlockComment;  // Tracks multi-line comment state across lines

    // Line Number Gutter
    void createLineNumberGutter(HWND hwndParent);
    void updateLineNumbers();
    static LRESULT CALLBACK LineNumberProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void paintLineNumbers(HDC hdc, RECT& rc);

    // Editor Tab Bar
    void createTabBar(HWND hwndParent);
    void addTab(const std::string& filePath, const std::string& displayName);
    void removeTab(int index);
    void setActiveTab(int index);
    void onTabChanged();
    int findTabByPath(const std::string& filePath) const;

    // Session Persistence
    void saveSession();
    void restoreSession();
    void saveSessionTabs(nlohmann::json& session);
    void restoreSessionTabs(const nlohmann::json& session);
    void saveSessionPanelState(nlohmann::json& session);
    void restoreSessionPanelState(const nlohmann::json& session);
    void saveSessionEditorState(nlohmann::json& session);
    void restoreSessionEditorState(const nlohmann::json& session);
    std::string getSessionFilePath() const;

    // Agent Inline Annotations
    enum class AnnotationSeverity {
        Hint = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Suggestion = 4
    };
    // Annotation Action Types — what happens when the user clicks an annotation
    enum class AnnotationActionType {
        None = 0,           // No action (display only)
        JumpToLine,         // Navigate to the referenced line
        ApplyFix,           // Apply a suggested code fix
        AskAgent,           // Send the annotation context to the agent for elaboration
        OpenFile,           // Open a referenced file
        RunCommand,         // Execute a command palette command
        Suppress            // Dismiss/suppress the annotation permanently
    };

    // Annotation Action — attached to an annotation for click-to-act
    struct AnnotationAction {
        AnnotationActionType type;
        std::string label;          // Human-readable label for context menus
        int targetLine;             // For JumpToLine: destination line (1-based)
        std::string targetFile;     // For OpenFile / JumpToLine in another file
        std::string fixContent;     // For ApplyFix: the replacement text
        int fixStartLine;           // For ApplyFix: start line of replacement range
        int fixEndLine;             // For ApplyFix: end line of replacement range
        std::string agentPrompt;    // For AskAgent: pre-populated prompt
        std::string commandId;      // For RunCommand: command to execute
    };

    struct InlineAnnotation {
        int line;                   // 1-based line number
        int column;                 // 0-based column (for inline placement)
        AnnotationSeverity severity;
        std::string text;           // Annotation message
        std::string source;         // "agent", "linter", "diagnostics", etc.
        COLORREF color;             // Computed from severity
        bool visible;               // Can be toggled
        std::vector<AnnotationAction> actions; // Click actions (may have multiple per annotation)
    };
    void addAnnotation(int line, AnnotationSeverity severity, const std::string& text, const std::string& source = "agent");
    void addAnnotationWithAction(int line, AnnotationSeverity severity, const std::string& text,
                                  const AnnotationAction& action, const std::string& source = "agent");
    void removeAnnotations(int line, const std::string& source = "");
    void clearAllAnnotations(const std::string& source = "");
    void paintAnnotations(HDC hdc, RECT& rc);
    void paintAnnotationGutterIcons(HDC hdc, RECT& gutterRC);
    void updateAnnotationPositions(int fromLine, int lineDelta);
    std::vector<InlineAnnotation> getAnnotationsForLine(int line) const;
    std::string getAnnotationTooltip(int line) const;
    void toggleAnnotationVisibility(const std::string& source);
    COLORREF getAnnotationColor(AnnotationSeverity severity) const;
    int getAnnotationGutterIconWidth() const;
    static LRESULT CALLBACK AnnotationOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Annotation lifecycle — tab switch / file close
    void clearAnnotationsForCurrentFile();  // Clear annotations when closing a tab
    void storeAnnotationsForTab();          // Stash annotations before tab switch
    void restoreAnnotationsForTab();        // Restore stashed annotations on tab switch

    // Annotation → Action Bridge
    void executeAnnotationAction(const InlineAnnotation& annotation);
    void onAnnotationClicked(int line, int clickX, int clickY);
    void showAnnotationActionMenu(HWND hwndParent, int x, int y, const std::vector<InlineAnnotation>& annotations);

    // Session-aware Annotations
    void saveSessionAnnotations(nlohmann::json& session);
    void restoreSessionAnnotations(const nlohmann::json& session);

    // Language-aware Agent Prompts
    std::string getSyntaxLanguageName() const;
    std::string buildLanguageAwarePrompt(const std::string& basePrompt) const;

    // Minimap
    void createMinimap();
    void updateMinimap();
    void scrollToMinimapPosition(int y);
    void toggleMinimap();

    // Performance profiling
    void startProfiling();
    void stopProfiling();
    void showProfileResults();
    void analyzeScript();
    void measureExecutionTime();

    // Module management
    void refreshModuleList();
    void loadModule(const std::string& moduleName);
    void unloadModule(const std::string& moduleName);
    void showModuleBrowser();
    void importModule();
    void exportModule();

    // Command Palette (Ctrl+Shift+P)
    struct CommandPaletteItem {
        int id;
        std::string name;
        std::string shortcut;
        std::string category;
    };
    void showCommandPalette();
    void hideCommandPalette();
    void filterCommandPalette(const std::string& query);
    void executeCommandFromPalette(int index);
    void buildCommandRegistry();
    static LRESULT CALLBACK CommandPaletteProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CommandPaletteInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    // Command palette UI handles and data
    HWND m_hwndCommandPalette;
    HWND m_hwndCommandPaletteInput;
    HWND m_hwndCommandPaletteList;
    bool m_commandPaletteVisible;
    WNDPROC m_oldCommandPaletteInputProc;
    std::vector<CommandPaletteItem> m_commandRegistry;
    std::vector<CommandPaletteItem> m_filteredCommands;
    std::vector<std::vector<int>> m_fuzzyMatchPositions; // per-item match highlight positions
    std::unordered_map<int, int> m_commandMRU; // commandId -> usage count (session-only, no disk)

    // Utility
    std::string getWindowText(HWND hwnd);
    void setWindowText(HWND hwnd, const std::string& text);
    void appendText(HWND hwnd, const std::string& text);
    void syncEditorToGpuSurface();
    void initializeEditorSurface();
    static LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static constexpr const wchar_t* kEditorWndProp = L"RawrXD_IDE_PTR";
    static constexpr const wchar_t* kEditorProcProp = L"RawrXD_EDITOR_PROC";
    TerminalPane* findTerminalPane(int paneId);
    void setActiveTerminalPane(int paneId);

    // Search and Replace
    void showFindDialog();
    void showReplaceDialog();
    void findNext();
    void findPrevious();
    void replaceNext();
    void replaceAll();
    bool findText(const std::string& searchText, bool forward, bool caseSensitive, bool wholeWord, bool useRegex);
    int replaceText(const std::string& searchText, const std::string& replaceText, bool all, bool caseSensitive, bool wholeWord, bool useRegex);
    static INT_PTR CALLBACK FindDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ReplaceDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Floating Panel
    void createFloatingPanel();
    void showFloatingPanel();
    void hideFloatingPanel();
    void toggleFloatingPanel();
    void updateFloatingPanelContent(const std::string& content);
    void setFloatingPanelTab(int tabIndex);
    static LRESULT CALLBACK FloatingPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Git/Module panel helpers
    int getPanelAreaWidth() const;
    static LRESULT CALLBACK GitPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ModulePanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK CommitDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // File Explorer Sidebar
    void createFileExplorer(HWND hwndParent);
    void populateFileTree(HTREEITEM parentItem, const std::string& path);
    void onFileTreeExpand(HTREEITEM item, const std::string& path);
    void onFileTreeSelect(HTREEITEM item);
    void onFileTreeDoubleClick(HTREEITEM item);
    std::string getTreeItemPath(HTREEITEM item) const;
    void loadModelFromPath(const std::string& filepath);
    static LRESULT CALLBACK FileExplorerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Primary Sidebar (Left) - VS Code Activity Bar + Sidebar
    enum class SidebarView {
        None = 0,
        Explorer = 1,
        Search = 2,
        SourceControl = 3,
        RunDebug = 4,
        Extensions = 5
    };
    
    void createActivityBar(HWND hwndParent);
    void createPrimarySidebar(HWND hwndParent);
    void toggleSidebar();
    void setSidebarView(SidebarView view);
    void updateSidebarContent();
    void resizeSidebar(int width, int height);
    static LRESULT CALLBACK ActivityBarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Explorer View
    void createExplorerView(HWND hwndParent);
    void refreshFileTree();
    void expandFolder(const std::string& path);
    void collapseAllFolders();
    void newFileInExplorer();
    void newFolderInExplorer();
    void deleteItemInExplorer();
    void renameItemInExplorer();
    void revealInExplorer(const std::string& filePath);
    void handleExplorerContextMenu(POINT pt);
    static LRESULT CALLBACK ExplorerTreeProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Search View
    void createSearchView(HWND hwndParent);
    void performWorkspaceSearch(const std::string& query, bool useRegex, bool caseSensitive, bool wholeWord);
    void updateSearchResults(const std::vector<std::string>& results);
    void applySearchFilters(const std::string& includePattern, const std::string& excludePattern);
    void searchInFiles(const std::string& query);
    void replaceInFiles(const std::string& searchText, const std::string& replaceText);
    void clearSearchResults();
    
    // Source Control View
    void createSourceControlView(HWND hwndParent);
    void refreshSourceControlView();
    void stageSelectedFiles();
    void unstageSelectedFiles();
    void discardChanges();
    void commitChangesFromSidebar();
    void syncRepository();
    void showSCMContextMenu(POINT pt);
    
    // Run and Debug View
    void createRunDebugView(HWND hwndParent);
    void createLaunchConfiguration();
    void startDebugging();
    void stopDebugging();
    void setBreakpoint(const std::string& file, int line);
    void removeBreakpoint(const std::string& file, int line);
    void stepOver();
    void stepInto();
    void stepOut();
    void continueExecution();
    void showDebugConsole();
    void updateDebugVariables();
    
    // Extensions View
    void createExtensionsView(HWND hwndParent);
    void searchExtensions(const std::string& query);
    void installExtension(const std::string& extensionId);
    void uninstallExtension(const std::string& extensionId);
    void enableExtension(const std::string& extensionId);
    void disableExtension(const std::string& extensionId);
    void updateExtension(const std::string& extensionId);
    void showExtensionDetails(const std::string& extensionId);
    void loadInstalledExtensions();

private:
    // ========================================================================
    // POWERSHELL ACCESS - Private State
    // ========================================================================
    
    // PowerShell Runtime State
    struct PowerShellState {
        bool initialized;
        bool remoteSessionActive;
        std::string remoteComputerName;
        std::string currentExecutionPolicy;
        bool debugModeEnabled;
        bool transcriptActive;
        std::string transcriptPath;
        std::map<std::string, std::string> sessionVariables;
        std::vector<int> activeJobs;
        std::vector<int> activeBreakpoints;
        std::map<std::string, std::string> loadedModules;
        std::string profilePath;
        std::string version;
        std::string edition;
    };
    PowerShellState m_psState;
    
    // PowerShell Command Queue
    struct PSCommand {
        int id;
        std::string command;
        bool async;
        std::function<void(const std::string&)> callback;
    };
    std::vector<PSCommand> m_psCommandQueue;
    int m_nextPSCommandId;
    
    // PowerShell Job Tracking
    struct PSJob {
        int id;
        std::string name;
        std::string scriptBlock;
        bool completed;
        std::string output;
        std::string error;
    };
    std::map<int, PSJob> m_psJobs;
    int m_nextPSJobId;
    
    // PowerShell Module Cache
    struct PSModule {
        std::string name;
        std::string version;
        std::string path;
        bool loaded;
        std::vector<std::string> exportedCmdlets;
        std::vector<std::string> exportedFunctions;
    };
    std::map<std::string, PSModule> m_psModuleCache;
    
    // PowerShell Function Registry
    std::map<std::string, std::string> m_psFunctions; // name -> body
    
    // PowerShell Event Handlers
    std::map<std::string, std::string> m_psEventHandlers; // sourceId -> action
    
    // RawrXD.ps1 Integration
    bool m_rawrXDModuleLoaded;
    std::string m_rawrXDModulePath;
    std::map<std::string, std::string> m_rawrXDFunctions;
    
    // PowerShell Helper Functions
    std::string escapePowerShellString(const std::string& str);
    std::string buildPowerShellCommand(const std::string& cmdlet, const std::map<std::string, std::string>& params);
    std::string buildPowerShellPipeline(const std::vector<std::string>& commands);
    bool parsePowerShellOutput(const std::string& output, std::vector<std::string>& lines);
    std::string extractPowerShellError(const std::string& output);
    bool isPowerShellCommandAvailable(const std::string& cmdlet);
    void initializePowerShellState();
    void updatePowerShellModuleCache();
    std::string getRawrXDPowerShellPath();
    
    // GGUF Model loader (initialized in constructor) - supports both streaming and standard implementations
    std::unique_ptr<RawrXD::IGGUFLoader> m_ggufLoader;
    std::string m_loadedModelPath;
    RawrXD::GGUFMetadata m_currentModelMetadata;
    std::vector<RawrXD::TensorInfo> m_modelTensors;
    bool m_useStreamingLoader; // preference to use streaming loader to minimize memory
    bool m_useVulkanRenderer; // preference to use Vulkan renderer if enabled
    
    // Unified Model Source Resolver (HuggingFace, Ollama blobs, HTTP, local files)
    std::unique_ptr<RawrXD::ModelSourceResolver> m_modelResolver;
    
    // Streaming / Model UX state
    HWND m_hwndModelProgressBar;       // Progress bar control
    HWND m_hwndModelProgressLabel;     // Status text label
    HWND m_hwndModelProgressContainer; // Container panel
    HWND m_hwndModelCancelBtn;         // Cancel button
    std::atomic<bool> m_modelOperationActive;
    std::atomic<bool> m_modelOperationCancelled;
    std::atomic<float> m_modelProgressPercent;
    std::string m_modelProgressStatus;
    std::mutex m_modelProgressMutex;
    static const UINT_PTR MODEL_PROGRESS_TIMER_ID = 9902;
    static const UINT WM_MODEL_PROGRESS_UPDATE = WM_APP + 300;
    static const UINT WM_MODEL_PROGRESS_DONE = WM_APP + 301;

    // AI Inference State
    InferenceConfig m_inferenceConfig;
    bool m_inferenceRunning;
    bool m_inferenceStopRequested;
    std::string m_currentInferencePrompt;
    std::string m_currentInferenceResponse;
    std::thread m_inferenceThread;
    std::mutex m_inferenceMutex;
    std::function<void(const std::string&, bool)> m_inferenceCallback;
    
    // Native Agent Integration
    std::unique_ptr<RawrXD::NativeAgent> m_agent;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_nativeEngine;
    bool m_nativeEngineLoaded = false;
    std::mutex m_outputMutex;
    
    // Window Procedures for Subclassing
    static LRESULT CALLBACK CommandInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SidebarProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // Renamed to avoid overload conflict
    WNDPROC m_oldCommandInputProc = nullptr;
    WNDPROC m_oldSidebarProc = nullptr;
    
    // Extension Loader
    std::unique_ptr<RawrXD::ExtensionLoader> m_extensionLoader;
    void refreshExtensions();
    void loadExtension(const std::string& name);
    void unloadExtension(const std::string& name);
    void showExtensionHelp(const std::string& name);

    // File Explorer Sidebar - tree view items
    HWND m_hwndFileTree;
    std::map<HTREEITEM, std::string> m_treeItemPaths;

    HINSTANCE m_hInstance;
    HWND m_hwndMain;
    HWND m_hwndEditor;
    HWND m_hwndLineNumbers;     // Line number gutter
    HWND m_hwndTabBar;           // Editor tab bar
    WNDPROC m_oldLineNumberProc;
    int m_lineNumberWidth;       // Width of the gutter in pixels
    int m_currentLine;           // Current cursor line (1-based)

    // Tab tracking
    struct EditorTab {
        std::string filePath;
        std::string displayName;
        std::string content;
        bool modified;
    };
    std::vector<EditorTab> m_editorTabs;
    int m_activeTabIndex;

    HWND m_hwndCommandInput;
    HWND m_hwndStatusBar;
    HWND m_hwndOutputTabs;
    HWND m_hwndMinimap;
    HWND m_hwndHelp;
    HWND m_hwndFloatingPanel;
    HWND m_hwndFloatingContent;

    HMENU m_hMenu;
    HWND m_hwndToolbar;
    HWND m_hwndTitleLabel;
    HWND m_hwndBtnMinimize;
    HWND m_hwndBtnMaximize;
    HWND m_hwndBtnClose;
    HWND m_hwndBtnGitHub;
    HWND m_hwndBtnMicrosoft;
    HWND m_hwndBtnSettings;
    std::string m_lastTitleBarText;

    // Per-pane terminal managers replace the previous single manager
    std::string m_currentFile;
    bool m_fileModified;
    
    // Multi-terminal support
    std::vector<TerminalPane> m_terminalPanes;
    int m_nextTerminalId;
    int m_activeTerminalId;
    bool m_terminalSplitHorizontal;
    
    // Git integration
    GitStatus m_gitStatus;
    HWND m_hwndGitPanel;
    HWND m_hwndGitStatusText;
    HWND m_hwndGitFileList;
    std::vector<GitFile> m_currentGitFiles;
    bool m_gitPanelVisible;
    WNDPROC m_gitPanelProc;
    std::string m_gitRepoPath;
    bool m_gitAutoRefresh;
    HWND m_hwndCommitDialog; // commit dialog handle
    
    // File operations
    std::vector<std::string> m_recentFiles;
    static const size_t MAX_RECENT_FILES = 10;
    std::string m_currentDirectory;
    std::string m_defaultFileExtension;
    bool m_autoSaveEnabled;
    
    // Menu command system
    std::map<int, std::string> m_commandDescriptions;
    std::map<int, bool> m_commandStates;

    // Theme system
    IDETheme m_currentTheme;
    IDETheme m_themeBeforePreview;             // Saved for Cancel in theme picker
    std::map<std::string, IDETheme> m_themes;
    int m_activeThemeId;                      // IDM_THEME_xxx of current theme
    int m_themeIdBeforePreview;               // Saved for Cancel in theme picker
    bool m_transparencyEnabled;
    BYTE m_windowAlpha;                       // 0..255 (current window alpha)
    HBRUSH m_backgroundBrush;
    // Tracked brushes for themed surfaces (deleted + recreated on theme switch)
    HBRUSH m_sidebarBrush;
    HBRUSH m_sidebarContentBrush;
    HBRUSH m_panelBrush;
    HBRUSH m_secondarySidebarBrush;
    HBRUSH m_mainWindowBrush;
    HFONT m_editorFont;
    HFONT m_hFontUI;

    // Code snippets
    std::vector<CodeSnippet> m_codeSnippets;
    std::unordered_map<std::string, size_t> m_snippetTriggers;

    // Output tabs
    std::map<std::string, HWND> m_outputWindows;
    std::string m_activeOutputTab;
    bool m_outputPanelVisible;
    int m_outputTabHeight;
    int m_selectedOutputTab;
    HWND m_hwndSeverityFilter;
    int m_severityFilterLevel; // 0=All, 1=Info+, 2=Warn+, 3=Error only

    // Session Persistence
    bool m_sessionRestored;
    std::string m_sessionFilePath;

    // Agent Inline Annotations State
    std::vector<InlineAnnotation> m_annotations;
    bool m_annotationsVisible;
    HWND m_hwndAnnotationOverlay;    // Transparent overlay for inline annotation rendering
    HFONT m_annotationFont;          // Smaller italic font for annotations
    std::map<std::string, std::vector<InlineAnnotation>> m_annotationCache;  // Per-file annotation stash for tab switching

    // Clipboard history
    std::vector<std::string> m_clipboardHistory;
    static const size_t MAX_CLIPBOARD_HISTORY = 50;

    // Minimap
    bool m_minimapVisible;
    int m_minimapWidth;
    std::vector<std::string> m_minimapLines;
    std::vector<int> m_minimapLineStarts;

    // (Syntax coloring state declared inline with methods above — lines 641-645)

    // Profiling
    bool m_profilingActive;
    std::vector<std::pair<std::string, double>> m_profilingResults;
    LARGE_INTEGER m_profilingStart;
    LARGE_INTEGER m_profilingFreq;

    // Module management
    HWND m_hwndModuleBrowser;
    HWND m_hwndModuleList;
    HWND m_hwndModuleLoadButton;
    HWND m_hwndModuleUnloadButton;
    HWND m_hwndModuleRefreshButton;
    bool m_moduleBrowserVisible;
    WNDPROC m_modulePanelProc;
    std::vector<ModuleInfo> m_modules;
    bool m_moduleListDirty;

    // Window dimensions
    int m_editorHeight;
    int m_terminalHeight;
    int m_minimapX;
    RECT m_editorRect;
    bool m_gpuTextEnabled;
    bool m_editorHooksInstalled;

    // Splitter bar for terminal/output resize
    HWND m_hwndSplitter;
    bool m_splitterDragging;
    int m_splitterY;
    std::unique_ptr<RawrXD::IRenderer> m_renderer;
    bool m_rendererReady;

    // Search and Replace state
    std::string m_lastSearchText;
    std::string m_lastReplaceText;
    bool m_searchCaseSensitive;
    bool m_searchWholeWord;
    bool m_searchUseRegex;
    int m_lastFoundPos;
    HWND m_hwndFindDialog;
    HWND m_hwndReplaceDialog;

    // Primary Sidebar state
    HWND m_hwndActivityBar;
    HWND m_hwndSidebar;
    HWND m_hwndSidebarContent;
    bool m_sidebarVisible;
    int m_sidebarWidth;
    SidebarView m_currentSidebarView;

    // Explorer View
    HWND m_hwndExplorerTree;
    HWND m_hwndExplorerToolbar;
    HIMAGELIST m_hImageListExplorer;
    std::string m_explorerRootPath;

    // Search View
    HWND m_hwndSearchInput;
    HWND m_hwndSearchResults;
    HWND m_hwndSearchOptions;
    HWND m_hwndIncludePattern;
    HWND m_hwndExcludePattern;
    std::vector<std::string> m_searchResults;
    bool m_searchInProgress;

    // Source Control View (extends existing Git)
    HWND m_hwndSCMFileList;
    HWND m_hwndSCMToolbar;
    HWND m_hwndSCMMessageBox;

    // Run and Debug View
    HWND m_hwndDebugConfigs;
    HWND m_hwndDebugToolbar;
    HWND m_hwndDebugVariables;
    HWND m_hwndDebugCallStack;
    HWND m_hwndDebugConsole;
    bool m_debuggingActive;

    // Full Debugger UI HWNDs
    HWND m_hwndDebuggerContainer = nullptr;
    HWND m_hwndDebuggerToolbar = nullptr;
    HWND m_hwndDebuggerStatus = nullptr;
    HWND m_hwndDebuggerTabs = nullptr;
    HWND m_hwndDebuggerBreakpoints = nullptr;
    HWND m_hwndDebuggerWatch = nullptr;
    HWND m_hwndDebuggerVariables = nullptr;
    HWND m_hwndDebuggerStackTrace = nullptr;
    HWND m_hwndDebuggerMemory = nullptr;

    // Debugger State
    bool m_debuggerEnabled = false;
    bool m_debuggerAttached = false;
    bool m_debuggerPaused = false;
    size_t m_debuggerMaxMemory = 0;
    std::string m_debuggerCurrentFile;
    int m_debuggerCurrentLine = -1;

    // Debugger Collections
    std::vector<Breakpoint> m_breakpoints;
    std::vector<StackFrame> m_callStack;
    std::vector<Variable> m_localVariables;
    std::vector<WatchItem> m_watchList;

    // Extensions View
    HWND m_hwndExtensionsList;
    HWND m_hwndExtensionSearch;
    HWND m_hwndExtensionDetails;
    struct Extension {
        std::string id;
        std::string name;
        std::string version;
        std::string description;
        std::string author;
        bool installed;
        bool enabled;
    };
    std::vector<Extension> m_extensions;

    // Outline View (code structure)
    HWND m_hwndOutlineTree;
    struct OutlineItem {
        std::string name;
        std::string type;  // function, class, variable, etc.
        int line;
        int column;
        std::vector<OutlineItem> children;
    };
    std::vector<OutlineItem> m_outlineItems;
    void createOutlineView(HWND hwndParent);
    void updateOutlineView();
    void parseCodeForOutline();
    void goToOutlineItem(int index);

    // Timeline View (file history)
    HWND m_hwndTimelineList;
    struct TimelineEntry {
        std::string message;
        std::string author;
        std::string date;
        std::string commitHash;
        bool isGitCommit;
    };
    std::vector<TimelineEntry> m_timelineEntries;
    void createTimelineView(HWND hwndParent);
    void updateTimelineView();
    void loadGitHistory();
    void goToTimelineEntry(int index);

    // =========================================================================
    // VS Code-like UI Components
    // =========================================================================

    // Activity Bar (Far Left) - vertical icon bar for switching sidebar views
    static const int ACTIVITY_BAR_WIDTH = 48;
    HWND m_activityBarButtons[7];  // Explorer, Search, SCM, Debug, Extensions, Settings, Accounts
    int m_activeActivityBarButton;
    HBRUSH m_actBarBackgroundBrush;
    HBRUSH m_actBarHoverBrush;
    HBRUSH m_actBarActiveBrush;
    static LRESULT CALLBACK ActivityBarButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Secondary Sidebar (Right) - AI Chat / Copilot area
    HWND m_hwndSecondarySidebar;
    HWND m_hwndSecondarySidebarHeader;
    HWND m_hwndCopilotChatInput;
    HWND m_hwndCopilotChatOutput;
    HWND m_hwndCopilotSendBtn;
    HWND m_hwndCopilotClearBtn;
    HWND m_hwndModelSelector;
    HWND m_hwndMaxTokensSlider;
    HWND m_hwndMaxTokensLabel;

    // AI Mode Toggles
    HWND m_hwndChkMaxMode;
    HWND m_hwndChkDeepThink;
    HWND m_hwndChkDeepResearch;
    HWND m_hwndChkNoRefusal;

    // New Context Slider
    HWND m_hwndContextSlider;
    HWND m_hwndContextLabel;
    size_t m_currentContextSize;
    void onContextSizeChanged(int newValue);

    bool m_secondarySidebarVisible;
    int m_secondarySidebarWidth;
    int m_currentMaxTokens;
    std::vector<std::string> m_availableModels;
    std::vector<std::pair<std::string, std::string>> m_chatHistory; // role, message
    static LRESULT CALLBACK SecondarySidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Panel (Bottom) - Terminal, Output, Problems, Debug Console
    HWND m_hwndPanelContainer;
    HWND m_hwndPanelTabs;
    HWND m_hwndPanelToolbar;
    HWND m_hwndPanelNewTerminalBtn;
    HWND m_hwndPanelSplitTerminalBtn;
    HWND m_hwndPanelKillTerminalBtn;
    HWND m_hwndPanelMaximizeBtn;
    HWND m_hwndPanelCloseBtn;
    HWND m_hwndProblemsListView;
    PanelTab m_activePanelTab;
    bool m_panelVisible;
    bool m_panelMaximized;
    int m_panelHeight;

    // Problems tracking
    struct ProblemItem {
        std::string file;
        int line;
        int column;
        std::string message;
        int severity;  // 0=Error, 1=Warning, 2=Info
    };
    std::vector<ProblemItem> m_problems;
    int m_errorCount;
    int m_warningCount;

    // Enhanced Status Bar items
    struct StatusBarInfo {
        std::string remoteName;       // e.g., "WSL: Ubuntu" or empty
        std::string branchName;       // e.g., "main"
        int syncAhead;                // commits ahead
        int syncBehind;               // commits behind
        int errors;
        int warnings;
        int line;
        int column;
        int spacesOrTabWidth;
        bool useSpaces;
        std::string encoding;         // e.g., "UTF-8"
        std::string eolSequence;      // e.g., "LF" or "CRLF"
        std::string languageMode;     // e.g., "C++", "Python"
        bool copilotActive;
        int copilotSuggestions;       // number of available suggestions
    };
    StatusBarInfo m_statusBarInfo;
    HWND m_statusBarParts[12];  // Individual status bar items for custom drawing

    // VS Code UI Creation/Update functions
    void createActivityBarUI(HWND hwndParent);
    void createSecondarySidebar(HWND hwndParent);
    void createPanel(HWND hwndParent);
    void createEnhancedStatusBar(HWND hwndParent);

    void updateActivityBarState();
    void updateSecondarySidebarContent();
    void updatePanelContent();
    void updateEnhancedStatusBar();
    void updateProblemsPanel();

    void toggleSecondarySidebar();
    void togglePanel();
    void maximizePanel();
    void restorePanel();

    void switchPanelTab(PanelTab tab);
    void addProblem(const std::string& file, int line, int col, const std::string& msg, int severity);
    void clearProblems();
    void goToProblem(int index);
    void sendCopilotMessage(const std::string& message);
    void clearCopilotChat();
    void appendCopilotResponse(const std::string& response);
    void updateCursorPosition();
    void updateLanguageMode();
    void detectLanguageFromFile(const std::string& filePath);

    // File Explorer
    void createFileExplorer();
    void populateFileTree();
    void refreshFileExplorer();
    void expandTreeNode(HTREEITEM hItem);
    void onFileExplorerDoubleClick();
    void onFileExplorerRightClick();
    void showFileContextMenu(const std::string& filePath, bool isDirectory);
    void loadModelFromExplorer(const std::string& filePath);
    bool isModelFile(const std::string& filePath);
    HTREEITEM addTreeItem(HTREEITEM hParent, const std::string& text, const std::string& fullPath, bool isDirectory);
    void scanDirectory(const std::string& dirPath, HTREEITEM hParent);
    std::string getSelectedFilePath();

    // Model Chat Interface
    std::string sendMessageToModel(const std::string& message);
    void toggleChatMode();
    void appendChatMessage(const std::string& user, const std::string& message);
    bool trySendToOllama(const std::string& prompt, std::string& outResponse);

    // Ollama config
    std::string m_ollamaBaseUrl;      // e.g., http://localhost:11434
    std::string m_ollamaModelOverride; // if set, use this tag instead of deriving from filename

    // File Explorer members (additional)
    HIMAGELIST m_hImageList;
    std::vector<FileExplorerItem> m_rootItems;
    std::string m_currentExplorerPath;
    HWND m_hwndFileExplorer;  // Primary file explorer window

    // Model Chat state
    bool m_chatMode;

    // ========================================================================
    // DEDICATED POWERSHELL PANEL - Always Available PowerShell Console
    // ========================================================================
    // PowerShell Panel Window Handles
    HWND m_hwndPowerShellPanel;
    HWND m_hwndPowerShellOutput;
    HWND m_hwndPowerShellInput;
    HWND m_hwndPowerShellToolbar;
    HWND m_hwndPowerShellStatusBar;
    // PowerShell Panel Buttons
    HWND m_hwndPSBtnExecute;
    HWND m_hwndPSBtnClear;
    HWND m_hwndPSBtnStop;
    HWND m_hwndPSBtnHistory;
    HWND m_hwndPSBtnRestart;
    HWND m_hwndPSBtnLoadRawrXD;
    HWND m_hwndPSBtnToggle;
    // PowerShell Panel State
    bool m_powerShellPanelVisible;
    bool m_powerShellPanelDocked;
    bool m_powerShellSessionActive;
    bool m_powerShellRawrXDLoaded;
    int m_powerShellPanelHeight;
    int m_powerShellPanelWidth;
    std::string m_powerShellCurrentCommand;
    std::vector<std::string> m_powerShellCommandHistory;
    int m_powerShellHistoryIndex;
    size_t m_maxPowerShellHistory;
    // PowerShell Execution State
    bool m_powerShellExecuting;
    std::string m_powerShellCurrentOutput;
    HANDLE m_powerShellProcessHandle;
    std::unique_ptr<Win32TerminalManager> m_dedicatedPowerShellTerminal;
    // PowerShell Panel Functions
    void createPowerShellPanel();
    void createPowerShellToolbar();
    void showPowerShellPanel();
    void hidePowerShellPanel();
    void togglePowerShellPanel();
    void dockPowerShellPanel();
    void floatPowerShellPanel();
    void resizePowerShellPanel(int width, int height);
    void layoutPowerShellPanel();
    // PowerShell Execution
    void executePowerShellInput();
    void executePowerShellPanelCommand(const std::string& command);
    void stopPowerShellExecution();
    void clearPowerShellConsole();
    void appendPowerShellOutput(const std::string& text, COLORREF color = RGB(200, 200, 200));
    // PowerShell History Management
    void addPowerShellHistory(const std::string& command);
    void navigatePowerShellHistoryUp();
    void navigatePowerShellHistoryDown();
    void showPowerShellHistory();
    // PowerShell Session Management
    void startPowerShellSession();
    void restartPowerShellSession();
    void stopPowerShellSession();
    bool isPowerShellSessionActive() const;
    void updatePowerShellStatus();
    // RawrXD.ps1 Integration
    void loadRawrXDModule();
    void unloadRawrXDModule();
    void executeRawrXDCommand(const std::string& command);
    void quickLoadGGUFModel();
    void quickInference();
    // PowerShell Panel Helpers
    void initializePowerShellPanel();
    void updatePowerShellPanelLayout(int mainWidth, int mainHeight);
    std::string getPowerShellPrompt();
    void scrollPowerShellOutputToBottom();
    static LRESULT CALLBACK PowerShellPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PowerShellInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    // ========================================================================
    // DEBUGGER IMPLEMENTATION
    // ========================================================================
    // Debugger UI Creation & Management
    void createDebuggerUI();
    void updateDebuggerUI();
    void toggleDebugger();
    void attachDebugger();
    void detachDebugger();
    // ========================================================================
    // AI CHAT PANEL IMPLEMENTATION
    // ========================================================================
    void createChatPanel();
    void HandleCopilotSend();
    void HandleCopilotClear();
    void HandleCopilotStreamUpdate(const char* token, size_t length = 0);
    void populateModelSelector();
    void onModelSelectionChanged();
    void onMaxTokensChanged(int newValue);
    // Debugger Execution Control
    void pauseExecution();
    void resumeExecution();
    void stepOverExecution();
    void stepIntoExecution();
    void stepOutExecution();
    void restartDebugger();
    void stopDebugger();
    // Breakpoint Management
    void addBreakpoint(const std::string& file, int line);
    void toggleBreakpoint(const std::string& file, int line);
    void updateBreakpointList();
    void clearAllBreakpoints();
    // Watch Expression Management
    void addWatchExpression(const std::string& expression);
    void removeWatchExpression(const std::string& expression);
    void updateWatchList();
    void evaluateWatch(WatchItem& item);
    // Variable & Stack Inspection
    void updateVariables();
    void updateCallStack();
    void updateMemoryView();
    void expandVariable(const std::string& name);
    void collapseVariable(const std::string& name);
    // Debugger Commands
    void debuggerStepCommand(const std::string& command);
    void debuggerSetVariable(const std::string& name, const std::string& value);
    void debuggerInspectMemory(uint64_t address, size_t bytes);
    void debuggerEvaluateExpression(const std::string& expression);
    // Debugger Callbacks
    void onDebuggerBreakpoint(const std::string& file, int line);
    void onDebuggerException(const std::string& message);
    void onDebuggerOutput(const std::string& text);
    void onDebuggerContinued();
    void onDebuggerTerminated();
    // Helper Methods
    std::string formatDebuggerValue(const std::string& value, const std::string& type);
    bool isBreakpointAtLine(const std::string& file, int line) const;
    void highlightDebuggerLine(const std::string& file, int line);
    void clearDebuggerHighlight();
#define WM_AGENT_OUTPUT (WM_APP + 101)
#define WM_AGENT_OUTPUT_SAFE (WM_APP + 102)
    void onAgentOutput(const char* text);
    void postAgentOutputSafe(const std::string& text);

    // ========================================================================
    // Ghost Text / Inline Completions (Win32IDE_GhostText.cpp)
    // ========================================================================
    void initGhostText();
    void shutdownGhostText();
    void triggerGhostTextCompletion();
    void onGhostTextTimer();
    std::string requestGhostTextCompletion(const std::string& context, const std::string& language);
    void onGhostTextReady(int requestedCursorPos, const char* completionText);
    void dismissGhostText();
    void acceptGhostText();
    void renderGhostText(HDC hdc);
    bool handleGhostTextKey(UINT vk);
    void toggleGhostText();
    std::string trimGhostText(const std::string& raw);

    // Ghost Text state
    bool m_ghostTextEnabled     = false;
    bool m_ghostTextVisible     = false;
    bool m_ghostTextAccepted    = false;
    bool m_ghostTextPending     = false;
    std::string m_ghostTextContent;
    int m_ghostTextLine         = -1;
    int m_ghostTextColumn       = -1;
    HFONT m_ghostTextFont       = nullptr;

    // ========================================================================
    // Plan Executor — Plan → Approve → Execute (Win32IDE_PlanExecutor.cpp)
    // ========================================================================
    void generateAgentPlan(const std::string& goal);
    void onPlanReady(int stepCount, PlanStep* steps);
    void showPlanApprovalDialog();
    void executePlan();
    void onPlanStepDone(int stepIndex, int result);
    void onPlanComplete(bool success);
    void cancelPlan();
    void pausePlan();
    void resumePlan();
    std::string getPlanStatusString() const;
    std::vector<PlanStep> parsePlanSteps(const std::string& agentOutput);
    std::string planStepTypeString(PlanStepType type) const;

    // Plan Approval Dialog — custom UI
    void populatePlanListView();
    void onPlanListSelChanged();
    void onPlanDialogCommand(int controlId);
    void updatePlanStepInDialog(int stepIndex, PlanStepStatus status);
    void closePlanDialog();
    void editSelectedPlanStep();

    // Plan state
    AgentPlan m_currentPlan;
    std::vector<AgentPlan> m_planHistory;
    std::atomic<bool> m_planExecutionCancelled{false};
    std::atomic<bool> m_planExecutionPaused{false};
    static const size_t MAX_PLAN_HISTORY = 50;

    // Plan Approval Dialog HWNDs
    HWND m_hwndPlanDialog       = nullptr;
    HWND m_hwndPlanList         = nullptr;  // ListView
    HWND m_hwndPlanDetail       = nullptr;  // RichEdit / Static detail panel
    HWND m_hwndPlanGoalLabel    = nullptr;
    HWND m_hwndPlanSummaryLabel = nullptr;
    HWND m_hwndPlanBtnApprove   = nullptr;
    HWND m_hwndPlanBtnEdit      = nullptr;
    HWND m_hwndPlanBtnReject    = nullptr;
    HWND m_hwndPlanBtnPause     = nullptr;
    HWND m_hwndPlanBtnCancel    = nullptr;
    HWND m_hwndPlanProgress     = nullptr;
    HWND m_hwndPlanProgressLabel = nullptr;
    HBRUSH m_planDialogBrush    = nullptr;

    // ========================================================================
    // Failure Detection & Self-Correction (Win32IDE_FailureDetector.cpp)
    // ========================================================================
    void initFailureDetector();
    std::vector<AgentFailureType> detectFailures(const std::string& response,
                                                  const std::string& originalPrompt);
    bool detectRefusal(const std::string& response);
    bool detectHallucination(const std::string& response, const std::string& prompt);
    bool detectFormatViolation(const std::string& response, const std::string& prompt);
    bool detectInfiniteLoop(const std::string& response);
    bool detectQualityDegradation(const std::string& response);
    std::string applyCorrectionStrategy(AgentFailureType failure,
                                        const std::string& originalPrompt,
                                        int retryAttempt);
    AgentResponse executeWithFailureDetection(const std::string& prompt);
    std::string failureTypeString(AgentFailureType type) const;
    std::string getFailureDetectorStats() const;
    void toggleFailureDetector();

    // Phase 4B: Failure Classification (returns structured FailureClassification)
    FailureClassification classifyFailure(const std::string& response,
                                          const std::string& originalPrompt);
    std::vector<FailureClassification> classifyAllFailures(const std::string& response,
                                                           const std::string& originalPrompt);

    // Phase 4B: Detection hooks — wired at exact choke points only
    //   1. Post-generation validation
    //   2. Tool invocation result parsing
    //   3. Plan step output verification
    //   4. Agent command post-processing
    FailureClassification hookPostGeneration(const std::string& response,
                                             const std::string& prompt);
    FailureClassification hookToolResult(const std::string& toolName,
                                         const std::string& toolOutput);
    FailureClassification hookPlanStepOutput(int stepIndex,
                                             const std::string& output);
    FailureClassification hookAgentCommand(const std::string& response,
                                            const std::string& prompt);
    FailureClassification hookSwarmMerge(const std::string& mergedResult,
                                         int taskCount,
                                         const std::string& strategy);

    // Phase 4B: Bounded retry (max 1 retry, approval required)
    std::string buildRetryPrompt(const FailureClassification& failure,
                                 const std::string& originalPrompt);
    bool showRetryApprovalInPlanDialog(int stepIndex,
                                       const FailureClassification& failure);
    std::string getRetryStrategyDescription(AgentFailureType reason) const;

    // Phase 4B: Failure-aware plan execution (replaces blind retry)
    AgentResponse executeWithBoundedRetry(const std::string& prompt);

    // Failure detector state
    bool m_failureDetectorEnabled = false;
    int m_failureMaxRetries       = 1;  // Phase 4B: hard cap at 1
    FailureStats m_failureStats;

    // ========================================================================
    // Failure Intelligence — Phase 6 (Win32IDE_FailureIntelligence.cpp)
    // ========================================================================
    // Phase 6.1 — Granular Classification
    FailureReason classifyFailureReason(AgentFailureType type,
                                         const std::string& response,
                                         const std::string& prompt);
    FailureReason classifyRefusalReason(const std::string& response);
    FailureReason classifyHallucinationReason(const std::string& response);
    FailureReason classifyFormatReason(const std::string& response, const std::string& prompt);
    FailureReason classifyLoopReason(const std::string& response);
    FailureReason classifyQualityReason(const std::string& response);
    std::string failureReasonString(FailureReason reason) const;
    std::string computePromptHash(const std::string& prompt) const;

    // Phase 6.2 — Bounded Retry Strategies
    void initFailureIntelligence();
    RetryStrategy getRetryStrategyForReason(FailureReason reason) const;
    std::string applyRetryStrategy(const RetryStrategy& strategy,
                                    const std::string& originalPrompt);
    AgentResponse executeWithFailureIntelligence(const std::string& prompt);
    void recordFailureIntelligence(const FailureIntelligenceRecord& record);
    void loadFailureIntelligenceHistory();
    void flushFailureIntelligence();

    // Phase 6.3 — History-Aware Suggestion UI
    bool hasMatchingPreviousFailure(const std::string& prompt) const;
    std::vector<FailureIntelligenceRecord> getMatchingFailures(const std::string& prompt) const;
    void showFailureSuggestionDialog(const std::string& prompt,
                                     const std::vector<FailureIntelligenceRecord>& matches);
    void showFailureIntelligencePanel();
    void showFailureIntelligenceStats();
    std::string getFailureIntelligenceStatsString() const;
    void toggleFailureIntelligence();

    // Failure Intelligence state
    bool m_failureIntelligenceEnabled  = true;
    int m_intelligenceMaxRetries       = 2;
    std::vector<FailureIntelligenceRecord> m_failureIntelligenceHistory;
    std::map<int, FailureReasonStats> m_failureReasonStats;  // keyed by (int)FailureReason
    std::mutex m_failureIntelligenceMutex;
    static const size_t MAX_FAILURE_INTELLIGENCE_RECORDS = 500;

    // Failure Intelligence UI
    HWND m_hwndFailureIntelPanel       = nullptr;
    HWND m_hwndFailureIntelList        = nullptr;
    HWND m_hwndFailureIntelDetail      = nullptr;
    HWND m_hwndFailureIntelStats       = nullptr;
    HWND m_hwndFailureSuggestionDlg    = nullptr;

    // ========================================================================
    // Settings Dialog (Win32IDE_Settings.cpp)
    // ========================================================================
    std::string getSettingsFilePath() const;
    void loadSettings();
    void saveSettings();
    void applyDefaultSettings();
    void applySettings();
    void showSettingsDialog();

    // Settings state
    IDESettings m_settings;

    // ========================================================================
    // Local GGUF HTTP Server (Win32IDE_LocalServer.cpp)
    // ========================================================================
    void startLocalServer();
    void stopLocalServer();
    void handleLocalServerClient(SOCKET clientFd);
    void handleOllamaApiTags(SOCKET client);
    void handleOllamaApiGenerate(SOCKET client, const std::string& body);
    void handleOpenAIChatCompletions(SOCKET client, const std::string& body);
    void handleModelsEndpoint(SOCKET client);
    void handleAskEndpoint(SOCKET client, const std::string& body);
    void handleServeGui(SOCKET client);

    // Phase 6B: Agentic + Failure visibility endpoints (read-only)
    void handleAgentHistoryEndpoint(SOCKET client, const std::string& path);
    void handleAgentStatusEndpoint(SOCKET client);
    void handleAgentReplayEndpoint(SOCKET client, const std::string& body);
    void handleFailuresEndpoint(SOCKET client, const std::string& path);

    void toggleLocalServer();
    std::string getLocalServerStatus() const;

    // Local server state
    std::atomic<bool> m_localServerRunning{false};
    std::thread m_localServerThread;
    LocalServerStats m_localServerStats;

    // ========================================================================
    // Persisted Agent History + Replay (Win32IDE_AgentHistory.cpp)
    // ========================================================================
    void initAgentHistory();
    void shutdownAgentHistory();
    void recordEvent(AgentEventType type, const std::string& agentId,
                     const std::string& prompt, const std::string& result,
                     int durationMs, bool success,
                     const std::string& parentId = "",
                     const std::string& metadata = "");
    void recordSimpleEvent(AgentEventType type, const std::string& description);
    void flushEventLog();
    std::string getHistoryFilePath() const;
    std::vector<AgentEvent> loadHistory(int maxEvents = 500) const;
    std::vector<AgentEvent> loadHistoryForSession(const std::string& sessionId) const;
    std::vector<AgentEvent> filterHistory(AgentEventType typeFilter, int maxEvents = 100) const;
    void pruneHistory(int maxAgeDays = 30, int maxFileBytes = 10 * 1024 * 1024);
    void showAgentHistoryPanel();
    void updateAgentHistoryPanel();
    void showAgentReplayDialog();
    void replaySession(const std::string& sessionId);
    void onReplayStepDone(int stepIndex, int totalSteps);
    std::string getAgentHistoryStats() const;
    void toggleAgentHistory();
    std::string generateSessionId() const;
    std::string agentEventTypeString(AgentEventType type) const;
    uint64_t currentEpochMs() const;
    std::string truncateForLog(const std::string& text, size_t maxLen = 512) const;

    // Agent History state
    bool m_agentHistoryEnabled       = true;
    std::string m_currentSessionId;
    std::vector<AgentEvent> m_eventBuffer;      // In-memory ring buffer for current session
    std::mutex m_eventBufferMutex;
    AgentHistoryStats m_historyStats;
    static const size_t MAX_EVENT_BUFFER  = 1000;
    static const size_t MAX_LOG_FIELD_LEN = 512;

    // Agent History UI
    HWND m_hwndHistoryPanel   = nullptr;
    HWND m_hwndHistoryList    = nullptr;
    HWND m_hwndHistoryDetail  = nullptr;
    HWND m_hwndHistoryFilter  = nullptr;
    HWND m_hwndHistoryStats   = nullptr;
    HWND m_hwndHistoryBtnReplay   = nullptr;
    HWND m_hwndHistoryBtnExport   = nullptr;
    HWND m_hwndHistoryBtnClear    = nullptr;
    HWND m_hwndHistoryBtnRefresh  = nullptr;
};
