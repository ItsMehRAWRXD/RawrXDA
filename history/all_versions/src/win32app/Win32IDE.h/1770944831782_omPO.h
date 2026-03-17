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
#include <shellapi.h>
#include <string>
#include <vector>
#include <array>
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
#include "Win32IDE_WebView2.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include "../modules/game_engine_manager.h"
#include "../modules/crucible_engine.h"
#include "../modules/copilot_gap_closer.h"
#include "../../include/editor_engine.h"
#include "../../include/plugin_system/win32_plugin_loader.h"

#include "../modules/ExtensionLoader.hpp"
#include "../modules/vscode_extension_api.h"
#include "../../include/mcp_integration.h"
#include "../../include/universal_model_router.h"
#include "../core/instructions_provider.hpp"
#include "../core/native_inference_pipeline.hpp"
#include "../ui/tool_action_status.h"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <climits>

#include "../../include/model_registry.h"
#include "../../include/checkpoint_manager.h"
#include "../../include/multi_file_search.h"
#include "../../include/interpretability_panel.h"
#include "../../include/ci_cd_settings.h"
#include "../../include/ghost_text_renderer.h"
#include "../../include/benchmark_menu_widget.hpp"

// Forward declarations
class MultiResponseEngine;
class SubAgentManager;
namespace RawrXD { namespace LSPServer { class RawrXDLSPServer; } }

// Tier 3: File Watcher + DirectWrite (need full types for unique_ptr destructor)
#include "IocpFileWatcher.h"

// Forward declarations for DirectWrite types (avoid including <dwrite.h> in header)
struct IDWriteFactory;
struct IDWriteTextFormat;
struct IDWriteTextLayout;

#include "../agentic/OllamaProvider.h"
#include "../../include/agentic/agentic_composer_ux.h"

// Agent and AI IDs
#define IDM_AGENT_BOUNDED_LOOP 4120
#define IDM_AGENT_START_LOOP 4100
#define IDM_AGENT_EXECUTE_CMD 4101
#define IDM_AGENT_CONFIGURE_MODEL 4102
#define IDM_AGENT_VIEW_TOOLS 4103
#define IDM_AGENT_VIEW_STATUS 4104
#define IDM_AGENT_STOP 4105
#define IDM_AGENT_MEMORY 4106
#define IDM_AGENT_MEMORY_VIEW 4107
#define IDM_AGENT_MEMORY_CLEAR 4108
#define IDM_AGENT_MEMORY_EXPORT 4109

// SubAgent / Chain / Swarm / Todo command IDs (4110–4119)
#define IDM_SUBAGENT_CHAIN 4110
#define IDM_SUBAGENT_SWARM 4111
#define IDM_SUBAGENT_TODO_LIST 4112
#define IDM_SUBAGENT_TODO_CLEAR 4113
#define IDM_SUBAGENT_STATUS 4114

// Terminal extended IDs (4006–4010)
#define IDM_TERMINAL_KILL 4006
#define IDM_TERMINAL_SPLIT_H 4007
#define IDM_TERMINAL_SPLIT_V 4008
#define IDM_TERMINAL_SPLIT_CODE 4009
#define IDM_TERMINAL_CLEAR 4010

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

// Engine / 800B Dual-Engine & Titan (4220–4239)
#define IDM_ENGINE_UNLOCK_800B 4220
#define IDM_ENGINE_LOAD_800B   4221
#define IDM_TITAN_TOGGLE       4230

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
#define IDM_AI_AGENT_CYCLES_SET 4217
#define IDM_AI_AGENT_MULTI_ENABLE 4218
#define IDM_AI_AGENT_MULTI_DISABLE 4219
#define IDM_AI_AGENT_MULTI_STATUS 4220
#define IDM_AI_TITAN_TOGGLE       4221
#define IDM_AI_800B_STATUS        4222

// Reverse Engineering IDs
#define IDM_REVENG_ANALYZE 4300
#define IDM_REVENG_DISASM 4301
#define IDM_REVENG_DUMPBIN 4302
#define IDM_REVENG_COMPILE 4303
#define IDM_REVENG_COMPARE 4304
#define IDM_REVENG_DETECT_VULNS 4305
#define IDM_REVENG_EXPORT_IDA 4306
#define IDM_REVENG_EXPORT_GHIDRA 4307
#define IDM_REVENG_CFG 4308
#define IDM_REVENG_FUNCTIONS 4309
#define IDM_REVENG_DEMANGLE 4310
#define IDM_REVENG_SSA 4311
#define IDM_REVENG_RECURSIVE_DISASM 4312
#define IDM_REVENG_TYPE_RECOVERY 4313
#define IDM_REVENG_DATA_FLOW 4314
#define IDM_REVENG_LICENSE_INFO 4315

// Decompiler View IDs (Phase 18B)
#define IDM_REVENG_DECOMPILER_VIEW 4316
#define IDM_REVENG_DECOMP_RENAME   4317
#define IDM_REVENG_DECOMP_SYNC     4318
#define IDM_REVENG_DECOMP_CLOSE    4319

// Define LOG_FUNCTION macro if not already defined
#ifndef LOG_FUNCTION
#define LOG_FUNCTION() LOG_DEBUG(std::string("ENTER ") + __FUNCTION__)
#endif

// Theme and customization structures — now in include/IDETheme.h
// IDETheme is already included via editor_engine.h → IDETheme.h

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

// Tier 3: Language Mode Quick Switch (9100 range)
#define IDM_LANGMODE_FIRST          9100
#define IDM_LANGMODE_LAST           9199

// Tier 3: Encoding Selector (9200/9300 range)
#define IDM_ENCODING_REOPEN_FIRST   9200
#define IDM_ENCODING_REOPEN_LAST    9249
#define IDM_ENCODING_SAVE_FIRST     9300
#define IDM_ENCODING_SAVE_LAST      9349

// Tier 3: File changed externally — custom window message
#define WM_FILE_CHANGED_EXTERNAL    (WM_APP + 200)

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

// AI Backend Switcher commands (5037+ range — routed via handleToolsCommand)
#define IDM_BACKEND_SWITCH_LOCAL    5037
#define IDM_BACKEND_SWITCH_OLLAMA   5038
#define IDM_BACKEND_SWITCH_OPENAI   5039
#define IDM_BACKEND_SWITCH_CLAUDE   5040
#define IDM_BACKEND_SWITCH_GEMINI   5041
#define IDM_BACKEND_SHOW_STATUS     5042
#define IDM_BACKEND_SHOW_SWITCHER   5043
#define IDM_BACKEND_CONFIGURE       5044
#define IDM_BACKEND_HEALTH_CHECK    5045
#define IDM_BACKEND_SET_API_KEY     5046
#define IDM_BACKEND_SAVE_CONFIGS    5047

// LLM Router commands (5048+ range — routed via handleToolsCommand)
#define IDM_ROUTER_ENABLE           5048
#define IDM_ROUTER_DISABLE          5049
#define IDM_ROUTER_SHOW_STATUS      5050
#define IDM_ROUTER_SHOW_DECISION    5051
#define IDM_ROUTER_SET_POLICY       5052
#define IDM_ROUTER_SHOW_CAPABILITIES 5053
#define IDM_ROUTER_SHOW_FALLBACKS   5054
#define IDM_ROUTER_SAVE_CONFIG      5055
#define IDM_ROUTER_ROUTE_PROMPT     5056
#define IDM_ROUTER_RESET_STATS      5057

// LSP Client commands (5058+ range — routed via handleToolsCommand)
#define IDM_LSP_START_ALL           5058
#define IDM_LSP_STOP_ALL            5059
#define IDM_LSP_SHOW_STATUS         5060
#define IDM_LSP_GOTO_DEFINITION     5061
#define IDM_LSP_FIND_REFERENCES     5062
#define IDM_LSP_RENAME_SYMBOL       5063
#define IDM_LSP_HOVER_INFO          5064
#define IDM_LSP_SHOW_DIAGNOSTICS    5065
#define IDM_LSP_RESTART_SERVER      5066
#define IDM_LSP_CLEAR_DIAGNOSTICS   5067
#define IDM_LSP_SHOW_SYMBOL_INFO    5068
#define IDM_LSP_CONFIGURE           5069
#define IDM_LSP_SAVE_CONFIG         5070

// UX Enhancements & Research Track commands (5071+ range — routed via handleToolsCommand)
#define IDM_ROUTER_WHY_BACKEND      5071
#define IDM_ROUTER_PIN_TASK         5072
#define IDM_ROUTER_UNPIN_TASK       5073
#define IDM_ROUTER_SHOW_PINS        5074
#define IDM_ROUTER_SHOW_HEATMAP     5075
#define IDM_ROUTER_ENSEMBLE_ENABLE  5076
#define IDM_ROUTER_ENSEMBLE_DISABLE 5077
#define IDM_ROUTER_ENSEMBLE_STATUS  5078
#define IDM_ROUTER_SIMULATE         5079
#define IDM_ROUTER_SIMULATE_LAST    5080
#define IDM_ROUTER_SHOW_COST_STATS  5081

// Plugin System commands (5200+ range — routed via handleToolsCommand)
#define IDM_PLUGIN_SHOW_PANEL       5200
#define IDM_PLUGIN_LOAD             5201
#define IDM_PLUGIN_UNLOAD           5202
#define IDM_PLUGIN_UNLOAD_ALL       5203
#define IDM_PLUGIN_REFRESH          5204
#define IDM_PLUGIN_SCAN_DIR         5205
#define IDM_PLUGIN_SHOW_STATUS      5206
#define IDM_PLUGIN_TOGGLE_HOTLOAD   5207
#define IDM_PLUGIN_CONFIGURE        5208

// Converted AI Subsystem command IDs (5300 range)
#define IDM_AI_MODEL_REGISTRY       5300
#define IDM_AI_CHECKPOINT_MGR       5301
#define IDM_AI_INTERPRET_PANEL      5302
#define IDM_AI_CICD_SETTINGS        5303
#define IDM_AI_MULTI_FILE_SEARCH    5304
#define IDM_AI_BENCHMARK_MENU       5305

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
    bool breadcrumbsEnabled     = true;
    bool smoothScrollEnabled    = true;

    // Theme
    int themeId                 = 3101; // IDM_THEME_DARK_PLUS
    BYTE windowAlpha            = 255;

    // Display Scaling (100 = 100%, 125 = 125%, etc. 0 = auto/system DPI)
    int uiScalePercent          = 0; // 0 = auto (follow system DPI)

    // Server
    bool localServerEnabled     = false;
    int localServerPort         = 11435;

    // Local Parity (no API key) — same Cursor/GitHub parity via local GGUF
    bool localParityEnabled     = false;
    std::string localParityModelPath;
    std::string updateManifestUrl;   // Public URL for update check (no auth)
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

// Forward declaration for friend class
namespace vscode { class VSCodeExtensionAPI; }

class Win32IDE
{
    friend class AgenticBridge;
    friend class vscode::VSCodeExtensionAPI;
    friend void onCreateTrampoline(void* self, HWND hwnd);
    friend void deferredInitTrampoline(void* self);
    friend void bgInitBody(void* self);

public:
    void deferredHeavyInitBody();   // SEH-safe body, called from bg thread via sehRunBgThread

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
    bool m_multiAgentEnabled = false;  // Multi-agent orchestration toggle
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
    // Note: loadMemoryPlugin is the legacy single-DLL loader.
    // For the full plugin system, use m_pluginLoader (Phase 43).
    void loadMemoryPlugin(const std::string& path);

    // ========================================================================
    // AGENT MEMORY — Persistent observation store for agentic iterations
    // Phase 19B: Lets agents/models/swarms/end-users store & recall context
    // ========================================================================
    struct AgentMemoryEntry {
        std::string key;
        std::string value;
        std::string source;       // "agent", "user", "swarm", etc.
        uint64_t    timestampMs;
    };
    void onAgentMemoryStore(const std::string& key, const std::string& value,
                            const std::string& source = "agent");
    void onAgentMemoryRecall(const std::string& key);
    void onAgentMemoryView();
    void onAgentMemoryClear();
    void onAgentMemoryExport();
    std::string agentMemoryToJSON() const;
    bool agentMemoryHas(const std::string& key) const;
    std::string agentMemoryGet(const std::string& key) const;

    // ========================================================================
    // SUBAGENT WIN32 COMMAND HANDLERS — Wire SubAgentManager to Win32IDE
    // Phase 19B: Expose chain/swarm/todo via menu & command palette
    // ========================================================================
    void onSubAgentChain();
    void onSubAgentSwarm();
    void onSubAgentTodoList();
    void onSubAgentTodoClear();
    void onSubAgentStatus();

    // ========================================================================
    // TOKEN-BY-TOKEN STREAMING — Live output to editor/output panel
    // Phase 19B: appendStreamingToken writes tokens as they arrive
    // ========================================================================
    void appendStreamingToken(const std::string& token);
    void clearStreamingOutput();
    std::string getStreamingOutput() const;

    // ========================================================================
    // KILL TERMINAL — Force-kill stuck/frozen terminals with timeout
    // Phase 19B: Configurable time limiter, used by agents/models/end-users
    // ========================================================================
    void killTerminal(int paneId = -1);
    void killTerminalWithTimeout(int paneId, int timeoutMs);
    void setTerminalKillTimeout(int timeoutMs);
    int  getTerminalKillTimeout() const;

    // ========================================================================
    // SPLIT CODE VIEWER — Split editor into two side-by-side code panes
    // Phase 19B: Independent of terminal split
    // ========================================================================
    void splitCodeViewerHorizontal();
    void splitCodeViewerVertical();
    bool isCodeViewerSplit() const;
    void closeSplitCodeViewer();

    // ========================================================================
    // CROSS-IDE FINDPATTERN RVA PARITY TEST
    // Phase 19B: Ensures CLI & Win32IDE produce identical FindPattern results
    // ========================================================================
    bool testFindPatternRVAParity(const std::string& binaryPath,
                                   const std::string& pattern,
                                   uint64_t& outRVA);

    // ========================================================================
    // LICENSE → MANIFEST WIRING
    // Phase 19B: Connect RawrLicense_CheckFeature (Phase 17) to FeatureManifest
    // ========================================================================
    bool checkFeatureLicense(const std::string& featureId) const;
    void syncLicenseWithManifest();

    // ========================================================================
    // TRANSCENDENCE ARCHITECTURE PANEL (E → Ω)
    // ========================================================================
    void initTranscendence();
    void shutdownTranscendence();
    bool handleTranscendenceCommand(int cmdId);
    std::string handleTranscendenceEndpoint(
        const std::string& method, const std::string& path, const std::string& body);

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

    // ---- Public Accessors (for VSCodeExtensionAPI bridge) ----
    const IDESettings& getSettings() const { return m_settings; }
    IDESettings& getSettingsMut() { return m_settings; }
    bool isDebugActive() const { return m_debuggingActive; }
    const std::string& getDebugCurrentFile() const { return m_debuggerCurrentFile; }
    int createTerminalPanePublic(Win32TerminalManager::ShellType type, const std::string& name) {
        return createTerminalPane(type, name);
    }
    void sendToAllTerminalsPublic(const std::string& cmd) { sendToAllTerminals(cmd); }

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

    // DPI scaling
    UINT getDpi() const;
    int dpiScale(int basePixels) const;
    void recreateFonts();

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

    // ---- WebView2 + Monaco Editor (Phase 26) ----
    void createMonacoEditor(HWND hwnd);
    void destroyMonacoEditor();
    void toggleMonacoEditor();
    void syncRichEditToMonaco();
    void syncMonacoToRichEdit();
    void syncThemeToMonaco();
    void handleMonacoCommand(int commandId);

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

    // (AI Backend Abstraction — Phase 8B: see full declaration block below Settings section)

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

    // ── Context Window Token Usage Tracking ──
    struct ContextWindowUsage {
        int maxTokens            = 128000;
        int systemTokens         = 0;    // System instructions
        int toolDefTokens        = 0;    // Tool definitions
        int userContextTokens    = 0;    // User context / workspace info
        int messageTokens        = 0;    // Conversation messages
        int toolResultTokens     = 0;    // Tool call results

        int totalUsed() const {
            return systemTokens + toolDefTokens + userContextTokens +
                   messageTokens + toolResultTokens;
        }
        float percentage() const {
            return maxTokens > 0 ? (float(totalUsed()) / float(maxTokens)) * 100.0f : 0.0f;
        }
        bool isWarning() const { return percentage() >= 75.0f; }
        bool isDanger()  const { return percentage() >= 90.0f; }
    };

    ContextWindowUsage m_contextUsage;
    void updateContextWindowDisplay();
    void setContextWindowMax(int maxTokens);
    void addContextTokens(const std::string& category, int tokens);
    std::string formatTokenCount(int tokens) const;
    
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

    // ── Native Inference Pipeline (zero-dependency local AI) ──
    bool initNativePipeline();
    void shutdownNativePipeline();
    bool loadNativeModel(const std::string& ggufPath);
    std::string generateNativeResponse(const std::string& prompt);
    void onNativeAIToken(WPARAM wParam, LPARAM lParam);
    void onNativeAIComplete(WPARAM wParam, LPARAM lParam);
    void onNativeAIError();
    void onNativeAIProgress();

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
    void handleReverseEngineeringCFG();
    void handleReverseEngineeringFunctions();
    void handleReverseEngineeringDemangle();
    void handleReverseEngineeringSSA();
    void handleReverseEngineeringRecursiveDisasm();
    void handleReverseEngineeringTypeRecovery();
    void handleReverseEngineeringDataFlow();
    void handleReverseEngineeringLicenseInfo();
    void handleReverseEngineeringDecompilerView();
    
    HMENU createReverseEngineeringMenu(); // Helper to add the menu

    // ========================================================================
    // DECOMPILER VIEW — Direct2D Split View (Phase 18B)
    // Direct2D-rendered decompiler (left) + disassembly (right) with:
    //   1. Syntax coloring via tokenizeLine() / getTokenColor()
    //   2. Bidirectional synchronized selection (click ↔ highlight)
    //   3. Right-click variable rename with SSA graph propagation
    // ========================================================================
    void initDecompilerView();
    void showDecompilerView(const std::string& decompCode,
                            const std::string& disasmText,
                            const std::string& binaryName);
    void destroyDecompilerView();
    void decompViewRenameVariable(const std::string& oldName, const std::string& newName);
    std::map<std::string, std::string> decompViewGetRenameMap() const;
    void decompViewSyncToAddress(uint64_t address);
    bool isDecompilerViewActive() const;

public:
    // ========================================================================
    // FEATURE MANIFEST & SELF-TEST (Phase 19)
    // Auto-introspects all IDE features across Win32/CLI/React/PowerShell
    // ========================================================================
    int runFeatureSelfTests(std::vector<std::string>& results);
    std::string generateFeatureManifestMarkdown();
    std::string generateFeatureManifestJSON();
    void exportFeatureManifest();
    int getLoadedThemeCount() const { return static_cast<int>(m_themes.size()); }
    bool hasAgenticBridge() const { return m_agenticBridge != nullptr; }

private:

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

    // Grant Decompiler View free functions access to private members (theme, tokenizer)
    friend void DecompView_PaintDecomp(struct DecompViewState* state);
    friend void DecompView_PaintDisasm(struct DecompViewState* state);
    friend LRESULT CALLBACK SplitterBarProc(HWND, UINT, WPARAM, LPARAM);

    // Grant Tier 1 cosmetic callbacks access to private members
    friend LRESULT CALLBACK MinimapWndProc(HWND, UINT, WPARAM, LPARAM);
    friend INT_PTR CALLBACK SettingsGUIProc(HWND, UINT, WPARAM, LPARAM);

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
    static LRESULT CALLBACK FileExplorerContainerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Primary Sidebar (Left) - VS Code Activity Bar + Sidebar
    enum class SidebarView {
        None = 0,
        Explorer = 1,
        Search = 2,
        SourceControl = 3,
        RunDebug = 4,
        Extensions = 5,
        DiskRecovery = 6
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
    void deleteItemInExplorer(const std::string& path);
    void renameItemInExplorer(const std::string& path);
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
    bool m_useTitanKernel = true; // use Titan ASM/DLL inference when available (menu-togglable)
    
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
    
    // (AI Backend state — Phase 8B: see full block near Settings section)

    // Native Agent Integration
    std::unique_ptr<RawrXD::NativeAgent> m_agent;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_nativeEngine;
    std::unique_ptr<RawrXD::UniversalModelRouter> m_modelRouter;
    bool m_nativeEngineLoaded = false;
    std::mutex m_outputMutex;
    
    // Native Inference Pipeline — zero-dependency local AI core
    std::unique_ptr<RawrXD::NativeInferencePipeline> m_nativePipeline;
    bool m_nativePipelineReady = false;
    
    // Window Procedures for Subclassing
    static LRESULT CALLBACK CommandInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SidebarProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam); // Renamed to avoid overload conflict
    WNDPROC m_oldCommandInputProc = nullptr;
    WNDPROC m_oldSidebarProc = nullptr;
    WNDPROC m_oldFileExplorerContainerProc = nullptr;
    
    // Extension Loader
    std::unique_ptr<RawrXD::ExtensionLoader> m_extensionLoader;
    void refreshExtensions();
    void loadExtension(const std::string& name);
    void unloadExtension(const std::string& name);
    void showExtensionHelp(const std::string& name);

    // ========================================================================
    // PLUGIN SYSTEM — Phase 43: Native Win32 plugin loading (C ABI)
    // Hot-load DLLs with plugin_init/plugin_cleanup lifecycle.
    // Hooks: onFileSave, onChatMessage, onCommand, onModelLoad.
    // Config-gated hot-loading via IDM_PLUGIN_TOGGLE_HOTLOAD.
    // ========================================================================
    std::unique_ptr<RawrXD::Win32PluginLoader> m_pluginLoader;
    std::string m_pluginDirectory;  // Default plugin scan directory

    // Plugin system lifecycle
    void initPluginSystem();
    void shutdownPlugins();

    // Plugin UI & command handlers
    void showPluginPanel();
    void onPluginLoad();
    void onPluginUnload();
    void onPluginUnloadAll();
    void onPluginRefresh();
    void onPluginScanDir();
    void onPluginShowStatus();
    void onPluginToggleHotLoad();
    void onPluginConfigure();
    bool handlePluginCommand(int id);

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
        bool modified    = false;
        bool isPinned    = false;  // Tier3C #26: Pinned tabs
        bool isPreview   = false;  // Tier3C #27: Preview tabs
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
    HFONT m_hFontPowerShell = nullptr;
    HFONT m_hFontPowerShellStatus = nullptr;
    UINT  m_currentDpi = 96;

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

    // ========================================================================
    // CONVERTED QT SUBSYSTEMS — Phase 14B (Win32 API Native)
    // ========================================================================
    std::unique_ptr<ModelRegistry>          m_modelRegistry;
    std::unique_ptr<CheckpointManager>      m_checkpointManager;
    std::unique_ptr<MultiFileSearch>        m_multiFileSearch;
    std::unique_ptr<InterpretabilityPanel>  m_interpretabilityPanel;
    std::unique_ptr<CiCdSettings>           m_ciCdSettings;
    std::unique_ptr<GhostTextRenderer>      m_ghostTextRendererOverlay;
    std::unique_ptr<BenchmarkMenu>          m_benchmarkMenu;

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

    // Disk Recovery View
    HWND m_hwndRecoveryTitle     = nullptr;
    HWND m_hwndRecoveryDriveList = nullptr;
    HWND m_hwndRecoveryOutPath   = nullptr;
    HWND m_hwndRecoveryStatus    = nullptr;
    HWND m_hwndRecoveryProgress  = nullptr;
    HWND m_hwndRecoveryLog       = nullptr;
    bool m_recoveryTimerActive   = false;

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
    // Tool action status per chat message (keyed by m_chatHistory index)
    std::map<size_t, std::vector<RawrXD::UI::ToolActionStatus>> m_chatToolActions;
    RawrXD::UI::ToolActionAccumulator m_currentToolActions; // accumulator for current response
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
    /** Resolve UI model name to API model tag (e.g. RawrXD-Native (Win32) -> primary model). */
    std::string getResolvedOllamaModel() const;

    // Ollama config
    std::string m_ollamaBaseUrl;      // e.g., http://localhost:11434
    std::string m_ollamaModelOverride; // if set, use this tag instead of deriving from filename

    // FIM Prediction Provider (OllamaProvider for ghost text)
    std::unique_ptr<RawrXD::Prediction::OllamaProvider> m_predictionProvider;

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
    std::string requestGhostTextCompletion(const std::string& context, const std::string& language,
                                            const std::string& suffix, const std::string& filePath,
                                            int cursorLine, int cursorCol);
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
    // Agent Panel — Multi-File Edit Session (Win32IDE_AgentPanel.cpp)
    // ========================================================================
    void initAgentPanel();
    void startAgentSession(const std::string& userGoal);
    void agentAcceptHunk(int fileIndex, int hunkIndex);
    void agentRejectHunk(int fileIndex, int hunkIndex);
    void agentAcceptAll();
    void agentRejectAll();
    std::string getAgentSessionSummary() const;
    void renderAgentDiffPanel(HDC hdc, RECT panelRect);
    void onBoundedAgentLoop();  // Ctrl+Shift+I → Bounded Agent (FIM tools)

    // ========================================================================
    // Disk Recovery Panel (Win32IDE_DiskRecovery.cpp)
    // ========================================================================
    void createDiskRecoveryView(HWND hwndParent);
    void setDarkThemeForWindow(HWND hwnd);
    void recoveryAppendLog(const char* msg);
    void onRecoveryScan();
    void onRecoveryProbe();
    void onRecoveryStart();
    void onRecoveryPause();
    void onRecoveryAbort();
    void onRecoveryKeyExtract();
    void onRecoveryBadMapExport();
    void onRecoveryBrowse();
    void onRecoveryTimer();
    void handleRecoveryCommand(int commandId);

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
    // AI Backend Switcher — Phase 8B (Win32IDE_BackendSwitcher.cpp)
    // ========================================================================
    enum class AIBackendType {
        LocalGGUF       = 0,   // Native CPU inference via RawrXD engine
        Ollama          = 1,   // Ollama HTTP server (local or remote)
        OpenAI          = 2,   // OpenAI API (gpt-4o, etc.)
        Claude          = 3,   // Anthropic Claude API
        Gemini          = 4,   // Google Gemini API
        ReasoningEngine = 5,   // Local C++20/ASM Expert Reasoning Engine
        Count           = 6
    };

    struct AIBackendConfig {
        AIBackendType type      = AIBackendType::LocalGGUF;
        std::string   name;           // Human-readable label ("Local GGUF", "Ollama", etc.)
        std::string   endpoint;       // Base URL (e.g., "http://localhost:11434", "https://api.openai.com")
        std::string   model;          // Model identifier (e.g., "llama3.2", "gpt-4o", "claude-sonnet-4-20250514")
        std::string   apiKey;         // API key for remote backends (empty for local)
        bool          enabled   = true;
        int           timeoutMs = 30000;   // Request timeout
        int           maxTokens = 2048;    // Default max tokens for this backend
        float         temperature = 0.7f;  // Default temperature for this backend
    };

    struct AIBackendStatus {
        AIBackendType type       = AIBackendType::LocalGGUF;
        bool          connected  = false;
        bool          healthy    = false;
        int           latencyMs  = -1;       // Last measured round-trip latency (-1 = unknown)
        uint64_t      requestCount = 0;      // Total requests sent to this backend
        uint64_t      failureCount = 0;      // Total failed requests
        std::string   lastError;             // Last error message (empty if ok)
        std::string   lastModel;             // Last model actually used
        uint64_t      lastUsedEpochMs = 0;   // Epoch ms of last use
    };

    // Backend Manager — initialization & lifecycle
    void initBackendManager();
    void shutdownBackendManager();
    void loadBackendConfigs();
    void saveBackendConfigs();

    // Backend switching (the core operation)
    bool setActiveBackend(AIBackendType type);
    AIBackendType getActiveBackendType() const;
    const AIBackendConfig& getActiveBackendConfig() const;
    std::string getActiveBackendName() const;

    // Backend listing & inspection
    std::vector<AIBackendConfig> listBackends() const;
    AIBackendConfig getBackendConfig(AIBackendType type) const;
    AIBackendStatus getBackendStatus(AIBackendType type) const;
    std::string getBackendStatusString() const;         // Human-readable summary of all backends

    // Backend configuration mutations
    void setBackendEndpoint(AIBackendType type, const std::string& endpoint);
    void setBackendModel(AIBackendType type, const std::string& model);
    void setBackendApiKey(AIBackendType type, const std::string& apiKey);
    void setBackendEnabled(AIBackendType type, bool enabled);
    void setBackendTimeout(AIBackendType type, int timeoutMs);

    // Backend health probing
    bool probeBackendHealth(AIBackendType type);
    void probeAllBackendsAsync();
    void onBackendHealthResult(AIBackendType type, bool healthy, int latencyMs, const std::string& error);

    // Backend inference routing (delegates to correct engine)
    std::string routeInferenceRequest(const std::string& prompt);
    void routeInferenceRequestAsync(const std::string& prompt,
                                     std::function<void(const std::string&, bool)> callback);
    std::string routeToLocalGGUF(const std::string& prompt);
    std::string routeToOllama(const std::string& prompt);
    std::string routeToOpenAI(const std::string& prompt);
    std::string routeToClaude(const std::string& prompt);
    std::string routeToGemini(const std::string& prompt);

    // HTTP helpers for remote backends
    std::string httpPost(const std::string& url, const std::string& body,
                         const std::vector<std::string>& headers, int timeoutMs);

    // Backend UI helpers
    void showBackendSwitcherDialog();
    void showBackendConfigDialog(AIBackendType type);
    void updateStatusBarBackend();
    std::string backendTypeString(AIBackendType type) const;
    AIBackendType backendTypeFromString(const std::string& name) const;
    std::string getBackendConfigFilePath() const;

    // Streaming Engine Selection — Phase 9
    // Auto-selects the optimal ASM streaming engine for a given model file
    std::string selectStreamingEngine(const std::string& modelPath);
    // Get full diagnostics from the streaming engine registry
    std::string getStreamingEngineDiagnostics() const;
    // List all registered streaming engines
    std::vector<std::string> getAvailableStreamingEngines() const;
    // Manually switch to a specific streaming engine by name
    bool switchStreamingEngine(const std::string& engineName);

    // Backend Switcher state
    AIBackendType m_activeBackend                           = AIBackendType::LocalGGUF;
    std::array<AIBackendConfig, (size_t)AIBackendType::Count>  m_backendConfigs;
    std::array<AIBackendStatus, (size_t)AIBackendType::Count>  m_backendStatuses;
    bool          m_backendManagerInitialized                = false;
    std::mutex    m_backendMutex;

    // ========================================================================
    // LLM Router — Phase 8C (Win32IDE_LLMRouter.cpp)
    // ========================================================================
    // Task-based routing: classifies prompts by intent, selects the optimal
    // backend based on capabilities, failure history, and policy constraints.
    // Fallback is explicit, auditable, never silent.

    // ---- Task classification ----
    enum class LLMTaskType {
        Chat           = 0,   // General conversation / Q&A
        CodeGeneration = 1,   // Write new code from description
        CodeReview     = 2,   // Analyze / review existing code
        CodeEdit       = 3,   // Edit / refactor existing code
        Planning       = 4,   // Multi-step plan generation
        ToolExecution  = 5,   // Tool-call / function-calling tasks
        Research       = 6,   // Summarization / information retrieval
        General        = 7,   // Unclassified — default bucket
        Count          = 8
    };

    // ---- Backend capability profile ----
    struct BackendCapability {
        AIBackendType backend         = AIBackendType::LocalGGUF;
        int           maxContextTokens = 4096;
        bool          supportsToolCalls = false;
        bool          supportsStreaming = true;
        bool          supportsFunctionCalling = false;
        bool          supportsJsonMode = false;
        int           costTier         = 0;     // 0 = free/local, 1 = cheap, 2 = moderate, 3 = expensive
        float         qualityScore     = 0.5f;  // 0.0–1.0 subjective quality rating
        std::string   notes;                    // Human-readable notes
    };

    // ---- Routing decision (the core output of the router) ----
    struct RoutingDecision {
        AIBackendType selectedBackend   = AIBackendType::LocalGGUF;
        AIBackendType fallbackBackend   = AIBackendType::Count;  // Count = no fallback
        LLMTaskType   classifiedTask    = LLMTaskType::General;
        float         confidence        = 0.0f;   // 0.0–1.0 classification confidence
        std::string   reason;                      // Human-readable routing rationale
        bool          policyOverride    = false;   // true if policy engine redirected
        bool          fallbackUsed      = false;   // true if primary failed and fallback served
        uint64_t      decisionEpochMs   = 0;       // When the decision was made
        int           primaryLatencyMs  = -1;      // Latency of primary attempt (-1 = not yet)
        int           fallbackLatencyMs = -1;      // Latency of fallback attempt (-1 = not used)
    };

    // ---- Per-task routing preference (user-configurable) ----
    struct TaskRoutingPreference {
        LLMTaskType   taskType          = LLMTaskType::General;
        AIBackendType preferredBackend  = AIBackendType::LocalGGUF;
        AIBackendType fallbackBackend   = AIBackendType::Count;
        bool          allowFallback     = true;
        int           maxFailuresBeforeSkip = 5;   // Skip a backend after N consecutive failures
    };

    // ---- Router statistics ----
    struct RouterStats {
        uint64_t totalRouted           = 0;
        uint64_t totalFallbacksUsed    = 0;
        uint64_t totalPolicyOverrides  = 0;
        uint64_t taskTypeCounts[(size_t)LLMTaskType::Count] = {};
        uint64_t backendSelections[(size_t)AIBackendType::Count] = {};
        uint64_t backendFallbacks[(size_t)AIBackendType::Count] = {};
    };

    // ---- Cost / latency record (one per routing event) ----
    struct CostLatencyRecord {
        LLMTaskType   task              = LLMTaskType::General;
        AIBackendType backend           = AIBackendType::LocalGGUF;
        int           latencyMs         = 0;
        int           costTier          = 0;     // Snapshot of backend's costTier at routing time
        float         qualityScore      = 0.0f;  // Snapshot of backend's qualityScore
        uint64_t      epochMs           = 0;
        bool          fallbackUsed      = false;
    };

    // ---- Aggregated cost/latency heatmap cell ----
    struct HeatmapCell {
        uint64_t      requestCount      = 0;
        double        totalLatencyMs    = 0.0;
        double        avgLatencyMs      = 0.0;
        int           minLatencyMs      = INT_MAX;
        int           maxLatencyMs      = 0;
        double        avgCostTier       = 0.0;
        double        avgQuality        = 0.0;
    };

    // ---- Per-task backend pin (user override) ----
    struct TaskBackendPin {
        LLMTaskType   task              = LLMTaskType::General;
        AIBackendType pinnedBackend     = AIBackendType::Count;  // Count = not pinned
        bool          active            = false;
        std::string   reason;                                     // "User pinned via palette"
    };

    // ---- Ensemble vote (one backend's response in an ensemble query) ----
    struct EnsembleVote {
        AIBackendType backend           = AIBackendType::LocalGGUF;
        std::string   response;
        int           latencyMs         = -1;
        float         confidenceWeight  = 0.0f;  // Derived from qualityScore * viability
        bool          succeeded         = false;
    };

    // ---- Ensemble routing decision ----
    struct EnsembleDecision {
        LLMTaskType   classifiedTask    = LLMTaskType::General;
        std::vector<EnsembleVote> votes;
        std::string   mergedResponse;            // The winning / merged output
        AIBackendType winnerBackend     = AIBackendType::LocalGGUF;
        float         winnerConfidence  = 0.0f;
        int           totalLatencyMs    = 0;     // Wall-clock time for the whole ensemble
        std::string   strategy;                  // "confidence-weighted", "fastest", "majority"
        uint64_t      decisionEpochMs   = 0;
    };

    // ---- Offline routing simulation input ----
    struct SimulationInput {
        std::string   prompt;
        LLMTaskType   expectedTask      = LLMTaskType::General;  // Ground truth (if known)
    };

    // ---- Offline routing simulation result ----
    struct SimulationResult {
        std::vector<SimulationInput> inputs;
        struct PerInput {
            std::string   prompt;
            LLMTaskType   classifiedTask  = LLMTaskType::General;
            AIBackendType selectedBackend = AIBackendType::LocalGGUF;
            float         confidence      = 0.0f;
            std::string   reason;
            bool          matchedExpected = false;  // Only meaningful when expectedTask is set
        };
        std::vector<PerInput> results;
        int     totalInputs          = 0;
        int     correctClassifications = 0;   // Count of matchedExpected == true
        float   classificationAccuracy = 0.0f;
        std::string summaryText;
    };

    // Router — initialization & lifecycle
    void initLLMRouter();
    void shutdownLLMRouter();
    void loadRouterConfig();
    void saveRouterConfig();

    // Task classification
    LLMTaskType classifyTask(const std::string& prompt) const;
    std::string taskTypeString(LLMTaskType type) const;
    LLMTaskType taskTypeFromString(const std::string& name) const;

    // Capability management
    void initDefaultCapabilities();
    BackendCapability getBackendCapability(AIBackendType type) const;
    void setBackendCapability(AIBackendType type, const BackendCapability& cap);

    // Core routing
    RoutingDecision selectBackendForTask(LLMTaskType task, const std::string& prompt);
    std::string routeWithIntelligence(const std::string& prompt);
    std::string handleRoutingFallback(AIBackendType primary, AIBackendType fallback,
                                       const std::string& prompt, RoutingDecision& decision);

    // Policy integration — failure-informed routing
    AIBackendType getFailureAdjustedBackend(AIBackendType preferred, LLMTaskType task) const;
    bool isBackendViableForTask(AIBackendType type, LLMTaskType task) const;
    int getConsecutiveFailures(AIBackendType type) const;

    // Router status & explainability
    std::string getRouterStatusString() const;
    std::string getRoutingDecisionExplanation(const RoutingDecision& decision) const;
    std::string getRouterStatsString() const;
    std::string getCapabilitiesString() const;
    std::string getFallbackChainString() const;
    RoutingDecision getLastRoutingDecision() const;

    // Router configuration
    void setTaskPreference(LLMTaskType task, AIBackendType preferred, AIBackendType fallback);
    TaskRoutingPreference getTaskPreference(LLMTaskType task) const;
    void setRouterEnabled(bool enabled);
    bool isRouterEnabled() const;
    void resetRouterStats();
    std::string getRouterConfigFilePath() const;

    // UX: "Why this backend?" detailed explanation
    std::string generateWhyExplanation(const RoutingDecision& decision) const;
    std::string generateWhyExplanationForLast() const;

    // UX: Per-task backend pinning
    void pinTaskToBackend(LLMTaskType task, AIBackendType backend, const std::string& reason = "");
    void unpinTask(LLMTaskType task);
    void clearAllPins();
    bool isTaskPinned(LLMTaskType task) const;
    TaskBackendPin getTaskPin(LLMTaskType task) const;
    std::string getPinnedTasksString() const;

    // UX: Cost / latency heatmap tracking
    void recordCostLatency(const RoutingDecision& decision);
    HeatmapCell getHeatmapCell(LLMTaskType task, AIBackendType backend) const;
    std::string getCostLatencyHeatmapString() const;
    std::string getCostStatsString() const;

    // Research: Ensemble routing (multi-backend fan-out)
    void setEnsembleEnabled(bool enabled);
    bool isEnsembleEnabled() const;
    std::string routeWithEnsemble(const std::string& prompt);
    EnsembleDecision buildEnsembleDecision(LLMTaskType task, const std::string& prompt);
    std::string selectEnsembleWinner(EnsembleDecision& decision) const;
    std::string getEnsembleStatusString() const;

    // Research: Offline routing simulation
    SimulationResult simulateRoutingOffline(const std::vector<SimulationInput>& inputs) const;
    SimulationResult simulateFromHistory(int maxEvents = 100) const;
    std::string getSimulationResultString(const SimulationResult& result) const;

    // UX/Research HTTP endpoints
    void handleRouterWhyEndpoint(SOCKET client);
    void handleRouterPinsEndpoint(SOCKET client);
    void handleRouterHeatmapEndpoint(SOCKET client);
    void handleRouterEnsembleEndpoint(SOCKET client, const std::string& body);
    void handleRouterSimulateEndpoint(SOCKET client, const std::string& body);

    // Router state
    bool m_routerEnabled                                            = false;
    bool m_routerInitialized                                        = false;
    std::array<BackendCapability, (size_t)AIBackendType::Count>     m_backendCapabilities;
    std::array<TaskRoutingPreference, (size_t)LLMTaskType::Count>   m_taskPreferences;
    std::array<int, (size_t)AIBackendType::Count>                   m_consecutiveFailures = {};
    RouterStats m_routerStats                                       = {};
    RoutingDecision m_lastRoutingDecision                           = {};
    std::mutex m_routerMutex;

    // UX Enhancement state — per-task pinning
    std::array<TaskBackendPin, (size_t)LLMTaskType::Count>          m_taskPins = {};

    // UX Enhancement state — cost / latency heatmap
    // Indexed as [taskType][backendType]
    std::array<std::array<HeatmapCell, (size_t)AIBackendType::Count>,
               (size_t)LLMTaskType::Count>                          m_heatmap = {};
    std::vector<CostLatencyRecord>                                  m_costLatencyLog;
    static const size_t MAX_COST_LATENCY_LOG = 2000;

    // Research state — ensemble routing
    bool m_ensembleEnabled                                          = false;
    EnsembleDecision m_lastEnsembleDecision                         = {};

    // Research state — last simulation result
    SimulationResult m_lastSimulationResult                         = {};

    // ========================================================================
    // LSP Client Bridge — Phase 9A (Win32IDE_LSPClient.cpp)
    // ========================================================================
    // Minimal LSP integration for C/C++ (clangd), Python (pyright), and
    // TypeScript (typescript-language-server). Provides go-to-definition,
    // find-references, rename, hover, and diagnostics. The LSP servers run as
    // child processes communicating via JSON-RPC over stdin/stdout.

    // ---- Supported language servers ----
    enum class LSPLanguage {
        Cpp        = 0,    // clangd
        Python     = 1,    // pyright-langserver (basedpyright) or pylsp
        TypeScript = 2,    // typescript-language-server
        Count      = 3
    };

    // ---- Per-server configuration ----
    struct LSPServerConfig {
        LSPLanguage language           = LSPLanguage::Cpp;
        std::string name;              // "clangd", "pyright", "typescript-language-server"
        std::string executablePath;    // Full path to the LSP binary
        std::vector<std::string> args; // Command-line arguments
        std::string rootUri;           // Workspace root URI ("file:///D:/rawrxd")
        bool        enabled            = true;
        int         initTimeoutMs      = 10000;
    };

    // ---- Per-server runtime state ----
    enum class LSPServerState {
        Stopped     = 0,
        Starting    = 1,
        Running     = 2,
        ShuttingDown = 3,
        Error       = 4
    };

    struct LSPServerStatus {
        LSPLanguage   language          = LSPLanguage::Cpp;
        LSPServerState state            = LSPServerState::Stopped;
        HANDLE        hProcess          = nullptr;    // Child process handle
        HANDLE        hStdinWrite       = nullptr;    // Write end of stdin pipe
        HANDLE        hStdoutRead       = nullptr;    // Read end of stdout pipe
        DWORD         pid               = 0;
        int           requestIdCounter  = 1;          // Monotonic JSON-RPC id
        bool          initialized       = false;      // Server responded to initialize
        std::string   lastError;
        uint64_t      startedEpochMs    = 0;
        uint64_t      requestCount      = 0;
        uint64_t      notificationCount = 0;
    };

    // ---- LSP protocol types (minimal subset) ----
    struct LSPPosition {
        int line      = 0;    // 0-based
        int character = 0;    // 0-based (UTF-16 offset)
    };

    struct LSPRange {
        LSPPosition start;
        LSPPosition end;
    };

    struct LSPLocation {
        std::string uri;      // "file:///path/to/file.cpp"
        LSPRange    range;
    };

    struct LSPDiagnostic {
        LSPRange    range;
        int         severity    = 1;    // 1=Error, 2=Warning, 3=Information, 4=Hint
        std::string code;               // Diagnostic code (e.g., "-Wunused")
        std::string source;             // "clangd", "pyright", etc.
        std::string message;
    };

    struct LSPHoverInfo {
        std::string contents;           // Markdown hover text
        LSPRange    range;              // Range the hover applies to
        bool        valid = false;
    };

    struct LSPSymbolInfo {
        std::string name;
        int         kind     = 0;       // LSP SymbolKind (1=File ... 26=TypeParameter)
        std::string detail;             // e.g., "void foo(int x)"
        LSPLocation location;
        std::string containerName;      // Enclosing symbol name
    };

    struct LSPWorkspaceEdit {
        // uri → list of {range, newText}
        struct TextEdit {
            LSPRange    range;
            std::string newText;
        };
        std::map<std::string, std::vector<TextEdit>> changes;
    };

    // ---- LSP client statistics ----
    struct LSPStats {
        uint64_t totalDefinitionRequests    = 0;
        uint64_t totalReferenceRequests     = 0;
        uint64_t totalRenameRequests        = 0;
        uint64_t totalHoverRequests         = 0;
        uint64_t totalDiagnosticsReceived   = 0;
        uint64_t totalServerRestarts        = 0;
    };

    // LSP Client — lifecycle
    void initLSPClient();
    void shutdownLSPClient();
    void loadLSPConfig();
    void saveLSPConfig();

    // LSP Server management
    bool startLSPServer(LSPLanguage lang);
    void stopLSPServer(LSPLanguage lang);
    void restartLSPServer(LSPLanguage lang);
    void startAllLSPServers();
    void stopAllLSPServers();
    LSPServerState getLSPServerState(LSPLanguage lang) const;

    // JSON-RPC transport
    int  sendLSPRequest(LSPLanguage lang, const std::string& method, const nlohmann::json& params);
    void sendLSPNotification(LSPLanguage lang, const std::string& method, const nlohmann::json& params);
    nlohmann::json readLSPResponse(LSPLanguage lang, int requestId, int timeoutMs = 5000);
    void lspReaderThread(LSPLanguage lang);

    // LSP initialization handshake
    bool sendInitialize(LSPLanguage lang);
    void sendInitialized(LSPLanguage lang);
    void sendShutdown(LSPLanguage lang);
    void sendExit(LSPLanguage lang);

    // Document sync
    void sendDidOpen(LSPLanguage lang, const std::string& uri, const std::string& languageId,
                     const std::string& content);
    void sendDidChange(LSPLanguage lang, const std::string& uri, const std::string& content);
    void sendDidClose(LSPLanguage lang, const std::string& uri);
    void sendDidSave(LSPLanguage lang, const std::string& uri);

    // Core LSP features (5 required)
    std::vector<LSPLocation> lspGotoDefinition(const std::string& uri, int line, int character);
    std::vector<LSPLocation> lspFindReferences(const std::string& uri, int line, int character);
    LSPWorkspaceEdit lspRenameSymbol(const std::string& uri, int line, int character,
                                      const std::string& newName);
    LSPHoverInfo lspHover(const std::string& uri, int line, int character);
    // Diagnostics arrive as notifications — handled in lspReaderThread

    // Apply edits from LSP
    bool applyWorkspaceEdit(const LSPWorkspaceEdit& edit);

    // Language detection
    LSPLanguage detectLanguageForFile(const std::string& filePath) const;
    std::string lspLanguageId(LSPLanguage lang) const;
    std::string lspLanguageString(LSPLanguage lang) const;
    LSPLanguage lspLanguageFromString(const std::string& name) const;

    // URI helpers
    std::string filePathToUri(const std::string& filePath) const;
    std::string uriToFilePath(const std::string& uri) const;

    // Diagnostics management
    void onDiagnosticsReceived(const std::string& uri, const std::vector<LSPDiagnostic>& diagnostics);
    std::vector<LSPDiagnostic> getDiagnosticsForFile(const std::string& uri) const;
    std::vector<std::pair<std::string, std::vector<LSPDiagnostic>>> getAllDiagnostics() const;
    void clearDiagnostics(const std::string& uri);
    void clearAllDiagnostics();
    void displayDiagnosticsAsAnnotations(const std::string& uri);

    // Status & display
    std::string getLSPStatusString() const;
    std::string getLSPStatsString() const;
    std::string getLSPDiagnosticsSummary() const;
    std::string getLSPConfigFilePath() const;

    // Command handlers (called from handleToolsCommand)
    void cmdLSPGotoDefinition();
    void cmdLSPFindReferences();
    void cmdLSPRenameSymbol();
    void cmdLSPHoverInfo();

    // HTTP endpoints
    void handleLSPStatusEndpoint(SOCKET client);
    void handleLSPDiagnosticsEndpoint(SOCKET client);

    // LSP Client state
    bool m_lspInitialized                                       = false;
    std::array<LSPServerConfig, (size_t)LSPLanguage::Count>     m_lspConfigs;
    std::array<LSPServerStatus, (size_t)LSPLanguage::Count>     m_lspStatuses;
    std::map<std::string, std::vector<LSPDiagnostic>>           m_lspDiagnostics;     // uri → diagnostics
    std::map<int, nlohmann::json>                               m_lspPendingResponses; // requestId → response
    LSPStats    m_lspStats                                      = {};
    std::mutex  m_lspMutex;
    std::mutex  m_lspDiagnosticsMutex;
    std::mutex  m_lspResponseMutex;
    std::condition_variable m_lspResponseCV;
    std::vector<std::thread> m_lspReaderThreads;

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
    void handleReadFileEndpoint(SOCKET client, const std::string& body);
    void handleWriteFileEndpoint(SOCKET client, const std::string& body);
    void handleListDirEndpoint(SOCKET client, const std::string& body);
    void handleDeleteFileEndpoint(SOCKET client, const std::string& body);
    void handleRenameFileEndpoint(SOCKET client, const std::string& body);
    void handleMkdirEndpoint(SOCKET client, const std::string& body);
    void handleSearchFilesEndpoint(SOCKET client, const std::string& body);
    void handleStatFileEndpoint(SOCKET client, const std::string& body);
    void handleCopyFileEndpoint(SOCKET client, const std::string& body);
    void handleMoveFileEndpoint(SOCKET client, const std::string& body);
    void handleToolDispatchEndpoint(SOCKET client, const std::string& body);
    void handleCliEndpoint(SOCKET client, const std::string& body);
    void handleHotpatchEndpoint(SOCKET client, const std::string& path, const std::string& body);

    // Phase 6B: Agentic + Failure visibility endpoints (read-only)
    void handleAgentHistoryEndpoint(SOCKET client, const std::string& path);
    void handleAgentStatusEndpoint(SOCKET client);
    void handleAgentReplayEndpoint(SOCKET client, const std::string& body);
    void handleFailuresEndpoint(SOCKET client, const std::string& path);

    // Phase 8B: Backend Switcher HTTP endpoints
    void handleBackendsListEndpoint(SOCKET client);
    void handleBackendActiveEndpoint(SOCKET client);
    void handleBackendSwitchEndpoint(SOCKET client, const std::string& body);

    // Phase 8C: LLM Router HTTP endpoints
    void handleRouterStatusEndpoint(SOCKET client);
    void handleRouterDecisionEndpoint(SOCKET client);
    void handleRouterCapabilitiesEndpoint(SOCKET client);
    void handleRouterRouteEndpoint(SOCKET client, const std::string& body);

    // Phase 32A: Chain-of-Thought Multi-Model Review HTTP endpoints
    void handleCoTStatusEndpoint(SOCKET client);
    void handleCoTPresetsEndpoint(SOCKET client);
    void handleCoTStepsEndpoint(SOCKET client);
    void handleCoTApplyPresetEndpoint(SOCKET client, const std::string& body);
    void handleCoTSetStepsEndpoint(SOCKET client, const std::string& body);
    void handleCoTExecuteEndpoint(SOCKET client, const std::string& body);
    void handleCoTCancelEndpoint(SOCKET client);
    void handleCoTRolesEndpoint(SOCKET client);

    // Phase 32A: Chain-of-Thought initialization
    void initChainOfThought();

    void toggleLocalServer();
    std::string getLocalServerStatus() const;

    // Global shutdown flag — checked by detached threads before accessing members
    std::atomic<bool> m_shuttingDown{false};
    bool isShuttingDown() const { return m_shuttingDown.load(std::memory_order_acquire); }

    // Active detached-thread counter — destructor waits for this to reach 0
    std::atomic<int> m_activeDetachedThreads{0};

    // RAII guard: increments on construction, decrements on destruction.
    // If IDE is shutting down at construction time, sets 'cancelled' flag.
    struct DetachedThreadGuard {
        std::atomic<int>& counter;
        bool cancelled;
        DetachedThreadGuard(std::atomic<int>& c, const std::atomic<bool>& shutting)
            : counter(c), cancelled(shutting.load(std::memory_order_acquire))
        { counter.fetch_add(1, std::memory_order_acq_rel); }
        ~DetachedThreadGuard() { counter.fetch_sub(1, std::memory_order_acq_rel); }
        DetachedThreadGuard(const DetachedThreadGuard&) = delete;
        DetachedThreadGuard& operator=(const DetachedThreadGuard&) = delete;
    };

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

    // ========================================================================
    // ASM Semantic Support (Win32IDE_AsmSemantic.cpp)
    // ========================================================================

    // ---- IDM defines for ASM commands (5082–5093) ----
#define IDM_ASM_PARSE_SYMBOLS       5082
#define IDM_ASM_GOTO_LABEL          5083
#define IDM_ASM_FIND_LABEL_REFS     5084
#define IDM_ASM_SHOW_SYMBOL_TABLE   5085
#define IDM_ASM_INSTRUCTION_INFO    5086
#define IDM_ASM_REGISTER_INFO       5087
#define IDM_ASM_ANALYZE_BLOCK       5088
#define IDM_ASM_SHOW_CALL_GRAPH     5089
#define IDM_ASM_SHOW_DATA_FLOW      5090
#define IDM_ASM_DETECT_CONVENTION   5091
#define IDM_ASM_SHOW_SECTIONS       5092
#define IDM_ASM_CLEAR_SYMBOLS       5093

    // ---- Symbol kinds for ASM analysis ----
    enum class AsmSymbolKind {
        Label       = 0,
        Procedure   = 1,
        Macro       = 2,
        Equate      = 3,
        DataDef     = 4,
        Section     = 5,
        Struct      = 6,
        Extern      = 7,
        Global      = 8,
        Local       = 9,
        Count       = 10
    };

    // ---- Symbol reference (where a symbol is used) ----
    struct AsmSymbolRef {
        std::string filePath;
        int         line       = 0;
        int         column     = 0;
        bool        isDefinition = false;
    };

    // ---- A single ASM symbol entry ----
    struct AsmSymbol {
        std::string   name;
        AsmSymbolKind kind       = AsmSymbolKind::Label;
        std::string   filePath;
        int           line       = 0;
        int           column     = 0;
        int           endLine    = 0;
        std::string   detail;
        std::string   section;
        std::vector<AsmSymbolRef> references;
    };

    // ---- Instruction information ----
    struct AsmInstructionInfo {
        std::string mnemonic;
        std::string category;
        std::string description;
        std::string operands;
        bool        affectsFlags = false;
    };

    // ---- Register information ----
    struct AsmRegisterInfo {
        std::string name;
        std::string category;
        int         bits = 64;
        std::string description;
        std::string aliases;
    };

    // ---- Section descriptor ----
    struct AsmSectionInfo {
        std::string name;
        std::string filePath;
        int         startLine = 0;
        int         endLine   = 0;
        int         symbolCount = 0;
    };

    // ---- Call graph edge ----
    struct AsmCallEdge {
        std::string caller;
        std::string callee;
        std::string filePath;
        int         line = 0;
    };

    // ---- Data flow reference ----
    struct AsmDataFlowRef {
        std::string symbol;
        int         line = 0;
        bool        isRead  = false;
        bool        isWrite = false;
        std::string instruction;
    };

    // ---- AI analysis result for an ASM block ----
    struct AsmBlockAnalysis {
        std::string summary;
        std::string callingConvention;
        std::vector<std::string> registersUsed;
        std::vector<std::string> registersModified;
        std::vector<std::string> memoryAccesses;
        std::vector<std::string> observations;
        bool        isLeafFunction = false;
        bool        usesStackFrame = false;
        int         estimatedStackUsage = 0;
    };

    // ---- Aggregate statistics ----
    struct AsmSemanticStats {
        uint64_t totalSymbols         = 0;
        uint64_t totalLabels          = 0;
        uint64_t totalProcedures      = 0;
        uint64_t totalMacros          = 0;
        uint64_t totalEquates         = 0;
        uint64_t totalDataDefs        = 0;
        uint64_t totalSections        = 0;
        uint64_t totalExterns         = 0;
        uint64_t totalFiles           = 0;
        uint64_t totalParseTimeMs     = 0;
        uint64_t gotoDefRequests      = 0;
        uint64_t findRefsRequests     = 0;
        uint64_t analyzeBlockRequests = 0;
    };

    // ASM Semantic — lifecycle
    void initAsmSemantic();
    void shutdownAsmSemantic();

    // Symbol table — parsing
    void parseAsmFile(const std::string& filePath);
    void parseAsmDirectory(const std::string& dirPath, bool recursive = true);
    void reparseCurrentAsmFile();
    void clearAsmSymbols();
    void clearAsmSymbolsForFile(const std::string& filePath);

    // Symbol table — queries
    const AsmSymbol* findAsmSymbol(const std::string& name) const;
    std::vector<const AsmSymbol*> findAsmSymbolsByKind(AsmSymbolKind kind) const;
    std::vector<const AsmSymbol*> findAsmSymbolsInFile(const std::string& filePath) const;
    std::vector<const AsmSymbol*> findAsmSymbolsInSection(const std::string& sectionName) const;
    std::vector<AsmSymbolRef> findAsmSymbolReferences(const std::string& symbolName) const;

    // Navigation
    bool asmGotoDefinition(const std::string& symbolName);
    bool asmGotoDefinitionAtCursor();
    std::vector<AsmSymbolRef> asmFindReferencesAtCursor();

    // Instruction & register lookup
    AsmInstructionInfo lookupInstruction(const std::string& mnemonic) const;
    AsmRegisterInfo lookupRegister(const std::string& regName) const;
    std::string getInstructionInfoString(const std::string& mnemonic) const;
    std::string getRegisterInfoString(const std::string& regName) const;

    // Section analysis
    std::vector<AsmSectionInfo> getAsmSections(const std::string& filePath) const;
    std::string getAsmSectionsString(const std::string& filePath) const;

    // Call graph
    std::vector<AsmCallEdge> buildCallGraph(const std::string& filePath) const;
    std::string getCallGraphString(const std::string& filePath) const;

    // Data flow
    std::vector<AsmDataFlowRef> analyzeDataFlow(const std::string& symbolName,
                                                 const std::string& filePath) const;
    std::string getDataFlowString(const std::string& symbolName,
                                   const std::string& filePath) const;

    // AI-assisted block analysis
    AsmBlockAnalysis analyzeAsmBlock(const std::string& filePath,
                                      int startLine, int endLine) const;
    AsmBlockAnalysis analyzeCurrentProcedure() const;
    std::string detectCallingConvention(const std::string& filePath,
                                         int startLine, int endLine) const;
    std::string getAsmBlockAnalysisString(const AsmBlockAnalysis& analysis) const;

    // Editor helpers (ASM)
    void gotoLine(int line);
    std::string getWordAtCursor() const;
    bool asmIsAsmFile(const std::string& filePath) const;

    // Display & status
    std::string getAsmSymbolTableString() const;
    std::string getAsmSemanticStatsString() const;
    std::string asmSymbolKindString(AsmSymbolKind kind) const;

    // Command handlers
    void cmdAsmParseSymbols();
    void cmdAsmGotoLabel();
    void cmdAsmFindLabelRefs();
    void cmdAsmShowSymbolTable();
    void cmdAsmInstructionInfo();
    void cmdAsmRegisterInfo();
    void cmdAsmAnalyzeBlock();
    void cmdAsmShowCallGraph();
    void cmdAsmShowDataFlow();
    void cmdAsmDetectConvention();
    void cmdAsmShowSections();
    void cmdAsmClearSymbols();

    // HTTP endpoints
    void handleAsmSymbolsEndpoint(SOCKET client, const std::string& path);
    void handleAsmNavigateEndpoint(SOCKET client, const std::string& body);
    void handleAsmAnalyzeEndpoint(SOCKET client, const std::string& body);

    // ASM Semantic state
    bool m_asmSemanticInitialized                                = false;
    std::map<std::string, AsmSymbol>     m_asmSymbolTable;
    std::map<std::string, std::vector<std::string>> m_asmFileSymbols;
    mutable AsmSemanticStats m_asmStats                          = {};
    mutable std::mutex m_asmMutex;

    // ========================================================================
    // Phase 9B: LSP-AI Hybrid Integration Bridge
    // (Win32IDE_LSP_AI_Bridge.cpp)
    // ========================================================================

    // ---- IDM defines for Hybrid commands (5094–5109) ----
#define IDM_HYBRID_COMPLETE         5094
#define IDM_HYBRID_DIAGNOSTICS      5095
#define IDM_HYBRID_SMART_RENAME     5096
#define IDM_HYBRID_ANALYZE_FILE     5097
#define IDM_HYBRID_AUTO_PROFILE     5098
#define IDM_HYBRID_STATUS           5099
#define IDM_HYBRID_SYMBOL_USAGE     5100
#define IDM_HYBRID_EXPLAIN_SYMBOL   5101
#define IDM_HYBRID_ANNOTATE_DIAG    5102
#define IDM_HYBRID_STREAM_ANALYZE   5103
#define IDM_HYBRID_SEMANTIC_PREFETCH 5104
#define IDM_HYBRID_CORRECTION_LOOP  5105

    // ---- Hybrid completion result ----
    struct HybridCompletionItem {
        std::string label;
        std::string detail;
        std::string insertText;
        std::string source;       // "lsp", "ai", "asm", "merged"
        float       confidence = 0.0f;
        int         sortOrder  = 0;
    };

    // ---- Aggregate diagnostic with AI explanation ----
    struct HybridDiagnostic {
        std::string filePath;
        int         line       = 0;
        int         character  = 0;
        int         severity   = 0;   // 1=Error, 2=Warning, 3=Info, 4=Hint
        std::string message;
        std::string source;           // "lsp", "ai", "asm"
        std::string aiExplanation;    // AI-generated fix suggestion
        std::string suggestedFix;
    };

    // ---- Symbol usage analysis ----
    struct HybridSymbolUsage {
        std::string symbol;
        std::string kind;             // "function", "variable", "procedure", etc.
        int         definitionLine = 0;
        std::string definitionFile;
        int         referenceCount = 0;
        std::vector<std::pair<std::string, int>> references; // (file, line) pairs
        std::string aiSummary;        // AI-generated description of the symbol
    };

    // ---- Streaming analysis result ----
    struct HybridStreamAnalysis {
        std::string filePath;
        int         totalLines     = 0;
        int         symbolCount    = 0;
        int         diagnosticCount = 0;
        int         complexityScore = 0;
        std::string summary;
        std::vector<HybridDiagnostic> diagnostics;
        std::vector<std::string> observations;
        double      analysisTimeMs = 0.0;
    };

    // ---- LSP profile recommendation ----
    struct LSPProfileRecommendation {
        LSPLanguage language;
        std::string serverName;
        std::string reason;
        bool        isInstalled = false;
    };

    // ---- Bridge statistics ----
    struct HybridBridgeStats {
        uint64_t hybridCompletions   = 0;
        uint64_t aggregateDiagRuns   = 0;
        uint64_t smartRenames        = 0;
        uint64_t streamAnalyses      = 0;
        uint64_t autoProfileSelects  = 0;
        uint64_t semanticPrefetches  = 0;
        uint64_t correctionLoops     = 0;
        uint64_t symbolExplains      = 0;
        double   totalBridgeTimeMs   = 0.0;
    };

    // Phase 9B — lifecycle
    void initLSPAIBridge();
    void shutdownLSPAIBridge();

    // Hybrid completion (LSP + AI + ASM merged)
    std::vector<HybridCompletionItem> requestHybridCompletion(
        const std::string& filePath, int line, int character);

    // Aggregate diagnostics (LSP + AI analysis + ASM semantic)
    std::vector<HybridDiagnostic> aggregateDiagnostics(const std::string& filePath);

    // Smart rename (LSP rename + AI verification + ASM cross-ref)
    bool hybridSmartRename(const std::string& filePath, int line, int character,
                           const std::string& newName);

    // Streaming large file analysis (uses StreamingEngineRegistry)
    HybridStreamAnalysis streamLargeFileAnalysis(const std::string& filePath);

    // Auto LSP profile selection based on project analysis
    std::vector<LSPProfileRecommendation> autoSelectLSPProfile();

    // Symbol usage analysis (LSP references + ASM refs + AI summary)
    HybridSymbolUsage analyzeSymbolUsage(const std::string& symbol,
                                          const std::string& filePath);

    // AI-powered symbol explanation
    std::string explainSymbol(const std::string& symbol, const std::string& filePath);

    // Semantic prefetching (pre-warm LSP + ASM caches for open files)
    void semanticPrefetch(const std::string& filePath);

    // Agent-LSP correction loop (detect bad output → re-query with LSP context)
    std::string agentCorrectionLoop(const std::string& prompt,
                                     const std::string& badOutput,
                                     const std::string& filePath);

    // Diagnostic annotation overlay (merge diagnostics → IDE annotations)
    void annotateDiagnostics(const std::string& filePath);

    // Bridge status & statistics
    HybridBridgeStats getHybridBridgeStats() const;
    std::string getHybridBridgeStatusString() const;

    // Command handlers
    void cmdHybridComplete();
    void cmdHybridDiagnostics();
    void cmdHybridSmartRename();
    void cmdHybridAnalyzeFile();
    void cmdHybridAutoProfile();
    void cmdHybridStatus();
    void cmdHybridSymbolUsage();
    void cmdHybridExplainSymbol();
    void cmdHybridAnnotateDiag();
    void cmdHybridStreamAnalyze();
    void cmdHybridSemanticPrefetch();
    void cmdHybridCorrectionLoop();

    // HTTP endpoints
    void handleHybridCompleteEndpoint(SOCKET client, const std::string& body);
    void handleHybridDiagnosticsEndpoint(SOCKET client, const std::string& path);
    void handleHybridSmartRenameEndpoint(SOCKET client, const std::string& body);
    void handleHybridAnalyzeEndpoint(SOCKET client, const std::string& body);
    void handleHybridStatusEndpoint(SOCKET client);
    void handleHybridSymbolUsageEndpoint(SOCKET client, const std::string& body);

    // Phase 9B state
    bool m_hybridBridgeInitialized = false;
    mutable HybridBridgeStats m_hybridStats = {};
    mutable std::mutex m_hybridMutex;

    // ========================================================================
    // Phase 9C: Multi-Response Chain Engine
    // (Win32IDE_MultiResponse.cpp)
    // ========================================================================

    // ---- IDM defines for MultiResponse commands (5106–5117) ----
#define IDM_MULTI_RESP_GENERATE         5106
#define IDM_MULTI_RESP_SET_MAX          5107
#define IDM_MULTI_RESP_SELECT_PREFERRED 5108
#define IDM_MULTI_RESP_COMPARE          5109
#define IDM_MULTI_RESP_SHOW_STATS       5110
#define IDM_MULTI_RESP_SHOW_TEMPLATES   5111
#define IDM_MULTI_RESP_TOGGLE_TEMPLATE  5112
#define IDM_MULTI_RESP_SHOW_PREFS       5113
#define IDM_MULTI_RESP_SHOW_LATEST      5114
#define IDM_MULTI_RESP_SHOW_STATUS      5115
#define IDM_MULTI_RESP_CLEAR_HISTORY    5116
#define IDM_MULTI_RESP_APPLY_PREFERRED  5117

    // Multi-Response lifecycle
    void initMultiResponse();
    void shutdownMultiResponse();

    // Multi-Response command handlers
    void cmdMultiResponseGenerate();
    void cmdMultiResponseSetMax();
    void cmdMultiResponseSelectPreferred();
    void cmdMultiResponseCompare();
    void cmdMultiResponseShowStats();
    void cmdMultiResponseShowTemplates();
    void cmdMultiResponseToggleTemplate();
    void cmdMultiResponseShowPreferences();
    void cmdMultiResponseShowLatest();
    void cmdMultiResponseShowStatus();
    void cmdMultiResponseClearHistory();
    void cmdMultiResponseApplyPreferred();

    // Multi-Response HTTP endpoint handlers
    void handleMultiResponseStatusEndpoint(SOCKET client);
    void handleMultiResponseTemplatesEndpoint(SOCKET client);
    void handleMultiResponseGenerateEndpoint(SOCKET client, const std::string& body);
    void handleMultiResponseResultsEndpoint(SOCKET client, const std::string& sessionId);
    void handleMultiResponsePreferEndpoint(SOCKET client, const std::string& body);
    void handleMultiResponseStatsEndpoint(SOCKET client);
    void handleMultiResponsePreferencesEndpoint(SOCKET client);

    // Multi-Response state
    bool m_multiResponseInitialized = false;
    std::unique_ptr<MultiResponseEngine> m_multiResponseEngine;

    // ════════════════════════════════════════════════════════════════════
    // Phase 10: Autonomous Execution & Trust Hardening
    // ════════════════════════════════════════════════════════════════════

    // ---- IDM defines for Phase 10 commands (5118–5131) ----
#define IDM_GOV_STATUS              5118
#define IDM_GOV_SUBMIT_COMMAND      5119
#define IDM_GOV_KILL_ALL            5120
#define IDM_GOV_TASK_LIST           5121
#define IDM_SAFETY_STATUS           5122
#define IDM_SAFETY_RESET_BUDGET     5123
#define IDM_SAFETY_ROLLBACK_LAST    5124
#define IDM_SAFETY_SHOW_VIOLATIONS  5125
#define IDM_REPLAY_STATUS           5126
#define IDM_REPLAY_SHOW_LAST        5127
#define IDM_REPLAY_EXPORT_SESSION   5128
#define IDM_REPLAY_CHECKPOINT       5129
#define IDM_CONFIDENCE_STATUS       5130
#define IDM_CONFIDENCE_SET_POLICY   5131

    // ---- IDM defines for Phase 11 commands (5132–5155) ----
    // Swarm Control
#define IDM_SWARM_STATUS            5132
#define IDM_SWARM_START_LEADER      5133
#define IDM_SWARM_START_WORKER      5134
#define IDM_SWARM_START_HYBRID      5135
#define IDM_SWARM_STOP              5136
    // Node Management
#define IDM_SWARM_LIST_NODES        5137
#define IDM_SWARM_ADD_NODE          5138
#define IDM_SWARM_REMOVE_NODE       5139
#define IDM_SWARM_BLACKLIST_NODE    5140
    // Build
#define IDM_SWARM_BUILD_SOURCES     5141
#define IDM_SWARM_BUILD_CMAKE       5142
#define IDM_SWARM_START_BUILD       5143
#define IDM_SWARM_CANCEL_BUILD      5144
    // Cache & Config
#define IDM_SWARM_CACHE_STATUS      5145
#define IDM_SWARM_CACHE_CLEAR       5146
#define IDM_SWARM_SHOW_CONFIG       5147
#define IDM_SWARM_TOGGLE_DISCOVERY  5148
    // Task Graph & Events
#define IDM_SWARM_SHOW_TASK_GRAPH   5149
#define IDM_SWARM_SHOW_EVENTS       5150
#define IDM_SWARM_SHOW_STATS        5151
#define IDM_SWARM_RESET_STATS       5152
    // Worker Control
#define IDM_SWARM_WORKER_STATUS     5153
#define IDM_SWARM_WORKER_CONNECT    5154
#define IDM_SWARM_WORKER_DISCONNECT 5155
#define IDM_SWARM_FITNESS_TEST      5156

    // Phase 10 lifecycle
    void initPhase10();
    void shutdownPhase10();

    // Governor command handlers (10A)
    void cmdGovernorStatus();
    void cmdGovernorSubmitCommand();
    void cmdGovernorKillAll();
    void cmdGovernorTaskList();

    // Safety command handlers (10B)
    void cmdSafetyStatus();
    void cmdSafetyResetBudget();
    void cmdSafetyRollbackLast();
    void cmdSafetyShowViolations();

    // Replay command handlers (10C)
    void cmdReplayStatus();
    void cmdReplayShowLast();
    void cmdReplayExportSession();
    void cmdReplayCheckpoint();

    // Confidence command handlers (10D)
    void cmdConfidenceStatus();
    void cmdConfidenceSetPolicy();

    // Governor HTTP endpoint handlers (10A)
    void handleGovernorStatusEndpoint(SOCKET client);
    void handleGovernorSubmitEndpoint(SOCKET client, const std::string& body);
    void handleGovernorKillEndpoint(SOCKET client, const std::string& body);
    void handleGovernorResultEndpoint(SOCKET client, const std::string& body);

    // Safety HTTP endpoint handlers (10B)
    void handleSafetyStatusEndpoint(SOCKET client);
    void handleSafetyCheckEndpoint(SOCKET client, const std::string& body);
    void handleSafetyViolationsEndpoint(SOCKET client);
    void handleSafetyRollbackEndpoint(SOCKET client, const std::string& body);

    // Replay HTTP endpoint handlers (10C)
    void handleReplayStatusEndpoint(SOCKET client);
    void handleReplayRecordsEndpoint(SOCKET client, const std::string& body);
    void handleReplaySessionsEndpoint(SOCKET client);

    // Confidence HTTP endpoint handlers (10D)
    void handleConfidenceStatusEndpoint(SOCKET client);
    void handleConfidenceEvaluateEndpoint(SOCKET client, const std::string& body);
    void handleConfidenceHistoryEndpoint(SOCKET client);

    // Unified Phase 10 status endpoint
    void handlePhase10StatusEndpoint(SOCKET client);

    // Phase 10 state
    bool m_phase10Initialized = false;

    // Phase 11 lifecycle
    void initPhase11();
    void shutdownPhase11();

    // Swarm Control command handlers (11A)
    void cmdSwarmStatus();
    void cmdSwarmStartLeader();
    void cmdSwarmStartWorker();
    void cmdSwarmStartHybrid();
    void cmdSwarmStop();

    // Node Management command handlers (11B)
    void cmdSwarmListNodes();
    void cmdSwarmAddNode();
    void cmdSwarmRemoveNode();
    void cmdSwarmBlacklistNode();

    // Build command handlers (11C)
    void cmdSwarmBuildFromSources();
    void cmdSwarmBuildFromCMake();
    void cmdSwarmStartBuild();
    void cmdSwarmCancelBuild();

    // Cache & Config command handlers (11D)
    void cmdSwarmCacheStatus();
    void cmdSwarmCacheClear();
    void cmdSwarmShowConfig();
    void cmdSwarmToggleDiscovery();

    // Task Graph & Events command handlers (11E)
    void cmdSwarmShowTaskGraph();
    void cmdSwarmShowEvents();
    void cmdSwarmShowStats();
    void cmdSwarmResetStats();

    // Worker Control command handlers (11F)
    void cmdSwarmWorkerStatus();
    void cmdSwarmWorkerConnect();
    void cmdSwarmWorkerDisconnect();
    void cmdSwarmFitnessTest();

    // Swarm HTTP endpoint handlers (11G)
    void handleSwarmStatusEndpoint(SOCKET client);
    void handleSwarmNodesEndpoint(SOCKET client);
    void handleSwarmTaskGraphEndpoint(SOCKET client);
    void handleSwarmStatsEndpoint(SOCKET client);
    void handleSwarmEventsEndpoint(SOCKET client);
    void handleSwarmConfigEndpoint(SOCKET client);
    void handleSwarmWorkerEndpoint(SOCKET client);
    void handleSwarmStartEndpoint(SOCKET client, const std::string& body);
    void handleSwarmStopEndpoint(SOCKET client);
    void handleSwarmAddNodeEndpoint(SOCKET client, const std::string& body);
    void handleSwarmBuildEndpoint(SOCKET client, const std::string& body);
    void handleSwarmCancelEndpoint(SOCKET client);
    void handleSwarmCacheClearEndpoint(SOCKET client);
    void handlePhase11StatusEndpoint(SOCKET client);

    // Phase 11 state
    bool m_phase11Initialized = false;

    // =========================================================================
    //         PHASE 41 — Dual-Agent Orchestrator (Architect + Coder)
    // =========================================================================
    // HTTP endpoint handlers for dual-agent swarm backed by MASM bridge.
    void handleDualAgentInitEndpoint(SOCKET client, const std::string& body);
    void handleDualAgentShutdownEndpoint(SOCKET client);
    void handleDualAgentStatusEndpoint(SOCKET client);
    void handleDualAgentHandoffEndpoint(SOCKET client, const std::string& body);
    void handleDualAgentSubmitEndpoint(SOCKET client, const std::string& body);
    void handlePhase41StatusEndpoint(SOCKET client);

    // Phase 41 state
    bool m_dualAgentInitialized = false;

    // =========================================================================
    //              PHASE 12 — Native Debugger Engine
    // =========================================================================
    // DbgEng COM interop + MASM64 breakpoint injection kernel.
    // IDM range: 5157 – 5184

    // ---- IDM defines for Native Debugger commands ----
    // Session Control (12A)
#define IDM_DBG_LAUNCH              5157
#define IDM_DBG_ATTACH              5158
#define IDM_DBG_DETACH              5159
    // Execution Control (12B)
#define IDM_DBG_GO                  5160
#define IDM_DBG_STEP_OVER           5161
#define IDM_DBG_STEP_INTO           5162
#define IDM_DBG_STEP_OUT            5163
#define IDM_DBG_BREAK               5164
#define IDM_DBG_KILL                5165
    // Breakpoint Management (12C)
#define IDM_DBG_ADD_BP              5166
#define IDM_DBG_REMOVE_BP           5167
#define IDM_DBG_ENABLE_BP           5168
#define IDM_DBG_CLEAR_BPS           5169
#define IDM_DBG_LIST_BPS            5170
    // Watch Management (12D)
#define IDM_DBG_ADD_WATCH           5171
#define IDM_DBG_REMOVE_WATCH        5172
    // Inspection (12E)
#define IDM_DBG_REGISTERS           5173
#define IDM_DBG_STACK               5174
#define IDM_DBG_MEMORY              5175
#define IDM_DBG_DISASM              5176
#define IDM_DBG_MODULES             5177
#define IDM_DBG_THREADS             5178
#define IDM_DBG_SWITCH_THREAD       5179
#define IDM_DBG_EVALUATE            5180
#define IDM_DBG_SET_REGISTER        5181
#define IDM_DBG_SEARCH_MEMORY       5182
#define IDM_DBG_SYMBOL_PATH         5183
#define IDM_DBG_STATUS              5184

    // Phase 12 lifecycle
    void initPhase12();
    void shutdownPhase12();

    // Session Control command handlers (12A)
    void cmdDbgLaunch();
    void cmdDbgAttach();
    void cmdDbgDetach();

    // Execution Control command handlers (12B)
    void cmdDbgGo();
    void cmdDbgStepOver();
    void cmdDbgStepInto();
    void cmdDbgStepOut();
    void cmdDbgBreak();
    void cmdDbgKill();

    // Breakpoint Management command handlers (12C)
    void cmdDbgAddBP();
    void cmdDbgRemoveBP();
    void cmdDbgEnableBP();
    void cmdDbgClearBPs();
    void cmdDbgListBPs();

    // Watch Management command handlers (12D)
    void cmdDbgAddWatch();
    void cmdDbgRemoveWatch();

    // Inspection command handlers (12E)
    void cmdDbgRegisters();
    void cmdDbgStack();
    void cmdDbgMemory();
    void cmdDbgDisasm();
    void cmdDbgModules();
    void cmdDbgThreads();
    void cmdDbgSwitchThread();
    void cmdDbgEvaluate();
    void cmdDbgSetRegister();
    void cmdDbgSearchMemory();
    void cmdDbgSymbolPath();
    void cmdDbgStatus();

    // Debug HTTP endpoint handlers (12F)
    void handleDbgStatusEndpoint(SOCKET client);
    void handleDbgBreakpointsEndpoint(SOCKET client);
    void handleDbgRegistersEndpoint(SOCKET client);
    void handleDbgStackEndpoint(SOCKET client);
    void handleDbgMemoryEndpoint(SOCKET client, const std::string& body);
    void handleDbgDisasmEndpoint(SOCKET client, const std::string& body);
    void handleDbgModulesEndpoint(SOCKET client);
    void handleDbgThreadsEndpoint(SOCKET client);
    void handleDbgEventsEndpoint(SOCKET client);
    void handleDbgWatchesEndpoint(SOCKET client);
    void handleDbgLaunchEndpoint(SOCKET client, const std::string& body);
    void handleDbgAttachEndpoint(SOCKET client, const std::string& body);
    void handleDbgGoEndpoint(SOCKET client);
    void handlePhase12StatusEndpoint(SOCKET client);

    // Phase 12 state
    bool m_phase12Initialized = false;

    // =========================================================================
    //              PHASE 14.2 — Hotpatch UI Integration
    // =========================================================================
    // Wires the three-layer hotpatch system into the Win32IDE command palette
    // and menu bar. IDM range: 9001 – 9030

#define IDM_HOTPATCH_SHOW_STATUS        9001
#define IDM_HOTPATCH_MEMORY_APPLY       9002
#define IDM_HOTPATCH_MEMORY_REVERT      9003
#define IDM_HOTPATCH_BYTE_APPLY         9004
#define IDM_HOTPATCH_BYTE_SEARCH        9005
#define IDM_HOTPATCH_SERVER_ADD         9006
#define IDM_HOTPATCH_SERVER_REMOVE      9007
#define IDM_HOTPATCH_PROXY_BIAS         9008
#define IDM_HOTPATCH_PROXY_REWRITE      9009
#define IDM_HOTPATCH_PROXY_TERMINATE    9010
#define IDM_HOTPATCH_PROXY_VALIDATE     9011
#define IDM_HOTPATCH_PRESET_SAVE        9012
#define IDM_HOTPATCH_PRESET_LOAD        9013
#define IDM_HOTPATCH_SHOW_EVENT_LOG     9014
#define IDM_HOTPATCH_RESET_STATS        9015
#define IDM_HOTPATCH_TOGGLE_ALL         9016
#define IDM_HOTPATCH_SHOW_PROXY_STATS   9017

// ========================================================================
// WEBVIEW2 + MONACO EDITOR COMMANDS — Phase 26 (9100 range)
// ========================================================================
#define IDM_VIEW_TOGGLE_MONACO          9100
#define IDM_VIEW_MONACO_DEVTOOLS        9101
#define IDM_VIEW_MONACO_RELOAD          9102
#define IDM_VIEW_MONACO_ZOOM_IN         9103
#define IDM_VIEW_MONACO_ZOOM_OUT        9104
#define IDM_VIEW_MONACO_SYNC_THEME      9105

// ========================================================================
// LSP SERVER COMMANDS — Phase 27 (9200 range)
// ========================================================================
#define IDM_LSP_SERVER_START            9200
#define IDM_LSP_SERVER_STOP             9201
#define IDM_LSP_SERVER_STATUS           9202
#define IDM_LSP_SERVER_REINDEX          9203
#define IDM_LSP_SERVER_STATS            9204
#define IDM_LSP_SERVER_PUBLISH_DIAG     9205
#define IDM_LSP_SERVER_CONFIG           9206
#define IDM_LSP_SERVER_EXPORT_SYMBOLS   9207
#define IDM_LSP_SERVER_LAUNCH_STDIO     9208

// ========================================================================
// EDITOR ENGINE COMMANDS — Phase 28 (9300 range)
// ========================================================================
#define IDM_EDITOR_ENGINE_RICHEDIT_CMD  9300
#define IDM_EDITOR_ENGINE_WEBVIEW2_CMD  9301
#define IDM_EDITOR_ENGINE_MONACOCORE_CMD 9302
#define IDM_EDITOR_ENGINE_CYCLE_CMD     9303
#define IDM_EDITOR_ENGINE_STATUS_CMD    9304

// ========================================================================
// PDB SYMBOL SERVER COMMANDS — Phase 29 (9400 range)
// ========================================================================
#define IDM_PDB_LOAD                    9400
#define IDM_PDB_FETCH                   9401
#define IDM_PDB_STATUS                  9402
#define IDM_PDB_CACHE_CLEAR             9403
#define IDM_PDB_ENABLE                  9404
#define IDM_PDB_RESOLVE                 9405

// Phase 29.3: PE Import/Export Table Provider
#define IDM_PDB_IMPORTS                 9410
#define IDM_PDB_EXPORTS                 9411
#define IDM_PDB_IAT_STATUS              9412

// ============================================================================
// PHASE 34: TELEMETRY EXPORT — Privacy-respecting IDE metrics (9900 range)
// ============================================================================
#define IDM_TELEMETRY_TOGGLE            9900
#define IDM_TELEMETRY_EXPORT_JSON       9901
#define IDM_TELEMETRY_EXPORT_CSV        9902
#define IDM_TELEMETRY_SHOW_DASHBOARD    9903
#define IDM_TELEMETRY_CLEAR             9904
#define IDM_TELEMETRY_SNAPSHOT          9905

// ============================================================================
// IDE SELF-AUDIT & VERIFICATION COMMANDS — Phase 31 (9500 range)
// ============================================================================
#define IDM_AUDIT_SHOW_DASHBOARD        9500
#define IDM_AUDIT_RUN_FULL              9501
#define IDM_AUDIT_DETECT_STUBS          9502
#define IDM_AUDIT_CHECK_MENUS           9503
#define IDM_AUDIT_RUN_TESTS             9504
#define IDM_AUDIT_EXPORT_REPORT         9505
#define IDM_AUDIT_QUICK_STATS           9506

// ============================================================================
// PHASE 32: FINAL GAUNTLET — Pre-Packaging Runtime Verification (9600 range)
// ============================================================================
#define IDM_GAUNTLET_RUN                9600
#define IDM_GAUNTLET_EXPORT             9601

// ============================================================================
// PHASE 33: VOICE CHAT — Native Win32 Audio Engine (9700 range)
// ============================================================================
#define IDM_VOICE_RECORD                9700
#define IDM_VOICE_PTT                   9701
#define IDM_VOICE_SPEAK                 9702
#define IDM_VOICE_JOIN_ROOM             9703
#define IDM_VOICE_SHOW_DEVICES          9704
#define IDM_VOICE_METRICS               9705
#define IDM_VOICE_TOGGLE_PANEL          9706
#define IDM_VOICE_MODE_PTT              9707
#define IDM_VOICE_MODE_CONTINUOUS       9708
#define IDM_VOICE_MODE_DISABLED         9709

// ============================================================================
// PHASE 33: QUICK-WIN PORTS — Shortcut, Backup, Alerts (9800 range)
// ============================================================================
#define IDM_QW_SHORTCUT_EDITOR          9800
#define IDM_QW_SHORTCUT_RESET           9801
#define IDM_QW_BACKUP_CREATE            9810
#define IDM_QW_BACKUP_RESTORE           9811
#define IDM_QW_BACKUP_AUTO_TOGGLE       9812
#define IDM_QW_BACKUP_LIST              9813
#define IDM_QW_BACKUP_PRUNE             9814
#define IDM_QW_ALERT_TOGGLE_MONITOR     9820
#define IDM_QW_ALERT_SHOW_HISTORY       9821
#define IDM_QW_ALERT_DISMISS_ALL        9822
#define IDM_QW_ALERT_RESOURCE_STATUS    9823
#define IDM_QW_SLO_DASHBOARD            9830

    // Hotpatch command handlers (implemented in Win32IDE_HotpatchPanel.cpp)
    void initHotpatchUI();
    void handleHotpatchCommand(int commandId);
    void cmdHotpatchShowStatus();
    void cmdHotpatchMemoryApply();
    void cmdHotpatchMemoryRevert();
    void cmdHotpatchByteApply();
    void cmdHotpatchByteSearch();
    void cmdHotpatchServerAdd();
    void cmdHotpatchServerRemove();
    void cmdHotpatchProxyBias();
    void cmdHotpatchProxyRewrite();
    void cmdHotpatchProxyTerminate();
    void cmdHotpatchProxyValidate();
    void cmdHotpatchPresetSave();
    void cmdHotpatchPresetLoad();
    void cmdHotpatchShowEventLog();
    void cmdHotpatchResetStats();
    void cmdHotpatchToggleAll();
    void cmdHotpatchShowProxyStats();

    // Hotpatch state
    bool m_hotpatchEnabled = false;
    bool m_hotpatchUIInitialized = false;

    // ========================================================================
    // PDB SYMBOL SERVER — Phase 29: Native PDB Symbol Server
    // Lifecycle, command routing, status, cache management
    // ========================================================================
    void initPDBSymbols();
    bool handlePDBCommand(int commandId);
    void cmdPDBLoad();
    void cmdPDBFetch();
    void cmdPDBStatus();
    void cmdPDBCacheClear();
    void cmdPDBEnable();
    void cmdPDBResolve();

    // Phase 29.3: PE Import/Export Table Provider
    void cmdPDBImports();
    void cmdPDBExports();
    void cmdPDBIATStatus();

    // PDB state
    bool m_pdbInitialized = false;
    bool m_pdbEnabled = true;

    // ========================================================================
    // DECOMPILER VIEW STATE — Direct2D Split View (Phase 18B)
    // ========================================================================
    HWND m_hwndDecompView   = nullptr;  // Split-view container
    HWND m_hwndDecompPane   = nullptr;  // Left pane: decompiler (Direct2D)
    HWND m_hwndDisasmPane   = nullptr;  // Right pane: disassembly (Direct2D)
    bool m_decompViewActive = false;    // True when decompiler view is showing

    // ========================================================================
    // AGENT MEMORY STATE — Phase 19B
    // ========================================================================
    std::vector<AgentMemoryEntry> m_agentMemory;
    mutable std::mutex m_agentMemoryMutex;
    static const size_t MAX_AGENT_MEMORY_ENTRIES = 2000;

    // ========================================================================
    // TOKEN STREAMING STATE — Phase 19B
    // ========================================================================
    std::string m_streamingOutput;
    mutable std::mutex m_streamingOutputMutex;
    bool m_streamingActive = false;

    // ========================================================================
    // TERMINAL KILL TIMEOUT STATE — Phase 19B
    // ========================================================================
    int m_terminalKillTimeoutMs = 5000;  // Default 5s, user-configurable

    // ========================================================================
    // SPLIT CODE VIEWER STATE — Phase 19B
    // ========================================================================
    HWND m_hwndSplitCodeViewer = nullptr;
    HWND m_hwndSplitCodeEditor = nullptr;
    bool m_codeViewerSplit = false;

    // ========================================================================
    // SUBAGENT MANAGER INSTANCE — Phase 19B (owned, used by chain/swarm/todo)
    // ========================================================================
    std::unique_ptr<SubAgentManager> m_subAgentManager;

    // ========================================================================
    // WEBVIEW2 + MONACO EDITOR STATE — Phase 26
    // ========================================================================
    HWND                    m_hwndMonacoContainer = nullptr;
    WebView2Container*      m_webView2 = nullptr;
    bool                    m_monacoEditorActive = false;
    MonacoEditorOptions     m_monacoOptions;
    float                   m_monacoZoomLevel = 1.0f;

    // ========================================================================
    // LSP SERVER — Phase 27 (Embedded Language Server)
    // ========================================================================
    // Lifecycle
    void initLSPServer();
    void shutdownLSPServer();

    // Command handlers (IDM_LSP_SERVER_* range, routed via handleLSPServerCommand)
    void cmdLSPServerStart();
    void cmdLSPServerStop();
    void cmdLSPServerStatus();
    void cmdLSPServerReindex();
    void cmdLSPServerStats();
    void cmdLSPServerPublishDiagnostics();
    void cmdLSPServerConfig();
    void cmdLSPServerExportSymbols();
    void cmdLSPServerLaunchStdio();

    // Command router
    bool handleLSPServerCommand(int commandId);

    // In-process message forwarding (Win32IDE → LSP Server)
    void forwardToLSPServer(const std::string& method, const nlohmann::json& params);
    void notifyLSPServerDidOpen(const std::string& uri, const std::string& languageId,
                                 const std::string& content);
    void notifyLSPServerDidChange(const std::string& uri, const std::string& content,
                                   int version);
    void notifyLSPServerDidClose(const std::string& uri);

    // HTTP endpoint
    void handleLSPServerStatusEndpoint(SOCKET client);

    // State
    std::unique_ptr<RawrXD::LSPServer::RawrXDLSPServer> m_lspServer;

    // ========================================================================
    // EDITOR ENGINE SYSTEM — Phase 28 (MonacoCore / WebView2 / RichEdit Toggle)
    // ========================================================================
    // Lifecycle
    void initEditorEngines();
    void shutdownEditorEngines();

    // Command handlers (IDM_EDITOR_ENGINE_* range)
    void cmdEditorEngineSetRichEdit();
    void cmdEditorEngineSetWebView2();
    void cmdEditorEngineSetMonacoCore();
    void cmdEditorEngineCycle();
    void cmdEditorEngineStatus();

    // Command router
    bool handleEditorEngineCommand(int commandId);

    // State
    bool m_editorEnginesInitialized = false;

    // ========================================================================
    // VS CODE EXTENSION API COMPATIBILITY — Phase 29
    // ========================================================================
    // Lifecycle
    void initVSCodeExtensionAPI();
    void shutdownVSCodeExtensionAPI();

    // Command handlers (IDM_VSCEXT_API_* range)
    void cmdVSCExtAPIStatus();
    void cmdVSCExtAPIReload();
    void cmdVSCExtAPIListCommands();
    void cmdVSCExtAPIListProviders();
    void cmdVSCExtAPIDiagnostics();
    void cmdVSCExtAPIExtensions();
    void cmdVSCExtAPIStats();
    void cmdVSCExtAPILoadNative();
    void cmdVSCExtAPIDeactivateAll();
    void cmdVSCExtAPIExportConfig();

    // Command router
    bool handleVSCExtAPICommand(int commandId);

    // Phase 36: QuickJS Extension Host commands (IDM_QUICKJS_HOST_* range)
    bool handleQuickJSHostCommand(int commandId);

    // State
    bool m_vscExtAPIInitialized = false;

    // ========================================================================
    // IDE SELF-AUDIT & VERIFICATION — Phase 31: CT Scanner / Compliance Auditor
    // ========================================================================
    // Lifecycle
    void initAuditSystem();

    // Command handlers (IDM_AUDIT_* range, 9500)
    void cmdAuditShowDashboard();
    void cmdAuditRunFull();
    void cmdAuditDetectStubs();
    void cmdAuditCheckMenus();
    void cmdAuditRunTests();
    void cmdAuditExportReport();
    void cmdAuditQuickStats();

    // Command router
    bool handleAuditCommand(int commandId);

    // State
    bool m_auditInitialized = false;
    HWND m_hwndAuditDashboard = nullptr;

    // ========================================================================
    // PHASE 32: FINAL GAUNTLET — Pre-Packaging Runtime Verification
    // ========================================================================
    // Command handlers (IDM_GAUNTLET_* range, 9600)
    void cmdGauntletRun();
    void cmdGauntletShowResults(const struct GauntletSummary& summary);
    void cmdGauntletExport();

    // Command router
    bool handleGauntletCommand(int commandId);

    // State
    HWND m_hwndGauntletWindow = nullptr;

    // ========================================================================
    // PHASE 33: VOICE CHAT — Native Win32 Audio Engine
    // ========================================================================
    // Lifecycle
    void initVoiceChat();
    void shutdownVoiceChat();

    // UI creation
    void createVoiceChatPanel(HWND hwndParent);
    void layoutVoiceChatPanel(int panelWidth, int panelHeight);
    void onVoiceChatTimer();

    // Command handlers (IDM_VOICE_* range, 9700)
    void cmdVoiceRecord();
    void cmdVoicePTT();
    void cmdVoiceSpeak();
    void cmdVoiceJoinRoom();
    void cmdVoiceModeChanged();
    void cmdVoiceDeviceChanged();
    void cmdVoiceShowDevices();
    void cmdVoiceShowMetrics();
    void cmdVoiceTogglePanel();

    // Command router
    bool handleVoiceChatCommand(int commandId);

    // Access
    class VoiceChat* getVoiceChatEngine();

    // State
    bool m_voicePanelVisible = false;
    bool m_voiceChatInitialized = false;

    // ========================================================================
    // PHASE 33: QUICK-WIN PORTS — Shortcuts, Backups, Alerts, SLO
    // ========================================================================
    // Lifecycle
    void initQuickWinSystems();
    void shutdownQuickWinSystems();

    // Shortcut Manager
    void cmdShortcutEditor();
    void cmdShortcutReset();
    class ShortcutManager* getShortcutManager();

    // Backup Manager
    void cmdBackupCreate();
    void cmdBackupRestore();
    void cmdBackupAutoToggle();
    void cmdBackupList();
    void cmdBackupPrune();
    class BackupManager* getBackupManager();

    // Alert System
    void cmdAlertToggleMonitor();
    void cmdAlertShowHistory();
    void cmdAlertDismissAll();
    void cmdAlertResourceStatus();
    class AlertSystem* getAlertSystem();

    // SLO Tracker
    void cmdSLODashboard();

    // Command router for 9800-9899 range
    bool handleQuickWinCommand(int commandId);

    // State
    bool m_quickWinInitialized = false;

    // ========================================================================
    // PHASE 34: TELEMETRY EXPORT — Privacy-respecting IDE Metrics
    // Non-Qt, Win32-native, GDPR/CCPA compliant, opt-in only
    // ========================================================================
    void initTelemetry();
    void shutdownTelemetry();

    // Command handlers (IDM_TELEMETRY_* range, 9900)
    void cmdTelemetryToggle();
    void cmdTelemetryExportJSON();
    void cmdTelemetryExportCSV();
    void cmdTelemetryShowDashboard();
    void cmdTelemetryClear();
    void cmdTelemetrySnapshot();

    // Command router
    bool handleTelemetryCommand(int commandId);

    // Inline telemetry recording (call from anywhere in IDE)
    void telemetryTrack(const char* featureName, double value = 1.0);
    void telemetryTrackLatency(const char* operation, double ms);

    // State
    bool m_telemetryInitialized = false;
    bool m_telemetryEnabled     = false;

    // ========================================================================
    // PHASE 33 VOICE UX POLISH — Hotkey, status bar, device hot-swap
    // ========================================================================
    void registerVoiceHotkeys();
    void unregisterVoiceHotkeys();
    void updateVoiceStatusBar();
    void voiceSavePreferences();
    void voiceLoadPreferences();

    // ========================================================================
    // PHASE 44: VOICE AUTOMATION — TTS for AI Responses (Production Wiring)
    // ========================================================================
    bool m_voiceAutomationInitialized = false;

    // ========================================================================
    // PHASE 36: MCP INTEGRATION — Model Context Protocol Server + Client
    // JSON-RPC 2.0 based tool/resource/prompt hosting for agent interactions
    // ========================================================================
    void initMCP();
    void shutdownMCP();
    RawrXD::MCP::MCPServer* getMCPServer() { return m_mcpServer.get(); }

    // State
    std::unique_ptr<RawrXD::MCP::MCPServer> m_mcpServer;
    bool m_mcpInitialized = false;

    // ========================================================================
    // PHASE 36: FLIGHT RECORDER — Memory-mapped binary ring-buffer logger
    // Persistent binary flight recorder at %LOCALAPPDATA%\RawrXD\flight_recorder.bin
    // ========================================================================
    static constexpr int IDM_FR_EXPORT_JSON = 10100;
    static constexpr int IDM_FR_DASHBOARD   = 10101;
    static constexpr int IDM_FR_CLEAR       = 10102;

    void initFlightRecorder();
    void shutdownFlightRecorder();
    void flightRecordEvent(int type, const char* message);
    void flightRecordDebug(const char* message);
    void flightRecordError(const char* message);
    void flightRecordCommand(int commandId);
    void flightRecordPerformance(const char* label, double elapsedMs);
    void flightRecordAgentAction(const char* action, const char* detail);
    void flightRecordCoTStep(int stepIndex, const char* role, const char* summary);
    void flightRecordCrashMarker(const char* reason);
    bool handleFlightRecorderCommand(int commandId);
    void cmdFlightRecorderExportJSON();
    void cmdFlightRecorderDashboard();
    void cmdFlightRecorderClear();

    // State
    bool m_flightRecorderInitialized = false;

    // ========================================================================
    // PHASE 34: INSTRUCTIONS CONTEXT PROVIDER
    // Read tools.instructions.md (all lines) → context for HTTP/CLI/GUI
    // ========================================================================
    void initInstructionsProvider();
    void handleInstructionsEndpoint(SOCKET client, const std::string& mode);
    void handleInstructionsContentEndpoint(SOCKET client);
    void handleInstructionsReloadEndpoint(SOCKET client);
    void showInstructionsDialog();
    std::string getInstructionsContent() const;
    bool m_instructionsInitialized = false;

    static constexpr int IDM_INSTRUCTIONS_VIEW    = 10500;
    static constexpr int IDM_INSTRUCTIONS_RELOAD  = 10501;
    static constexpr int IDM_INSTRUCTIONS_COPY    = 10502;

    // ========================================================================
    // PHASE 45: GAME ENGINE INTEGRATION (Unity + Unreal)
    // Full game-engine project management, build, play, debug, profile
    // ========================================================================
    void initGameEngines();
    void handleGameEngineCommand(int commandId);
    void createGameEngineMenu(HMENU parentMenu);
    void showGameEngineStatusDialog();

    // Command handlers
    void cmdGameEngineDetect();
    void cmdGameEngineOpen();
    void cmdGameEngineClose();
    void cmdGameEngineStatus();
    void cmdGameEngineBuild();
    void cmdGameEnginePlay();
    void cmdGameEngineStop();
    void cmdGameEnginePause();
    void cmdGameEngineCompile();
    void cmdGameEngineProfilerStart();
    void cmdGameEngineProfilerStop();
    void cmdGameEngineProfilerSnapshot();
    void cmdGameEngineDebugStart();
    void cmdGameEngineDebugStop();
    void cmdGameEngineBreakpoint();
    void cmdGameEngineAISummary();
    void cmdGameEngineAIScene();
    void cmdGameEngineInstallations();
    void cmdGameEngineHelp();

    // Unity-specific
    void cmdUnityCreateScript();
    void cmdUnitySceneList();
    void cmdUnityAssetBrowser();
    void cmdUnityPackageManager();

    // Unreal-specific
    void cmdUnrealCreateClass();
    void cmdUnrealCreateModule();
    void cmdUnrealCreatePlugin();
    void cmdUnrealGenProjectFiles();
    void cmdUnrealLiveCoding();
    void cmdUnrealCookContent();
    void cmdUnrealPackageProject();
    void cmdUnrealLevelList();
    void cmdUnrealBlueprintList();

    std::unique_ptr<RawrXD::GameEngine::GameEngineManager> m_gameEngineManager;
    bool m_gameEnginesInitialized = false;

    // ── Phase 48: The Final Crucible ──
    void initCrucible();
    void handleCrucibleCommand(int commandId);
    void createCrucibleMenu(HMENU parentMenu);

    // Crucible command handlers
    void cmdCrucibleRunAll();
    void cmdCrucibleRunShadow();
    void cmdCrucibleRunCluster();
    void cmdCrucibleRunSemantic();
    void cmdCrucibleCancel();
    void cmdCrucibleStatus();
    void cmdCrucibleReport();
    void cmdCrucibleExportJSON();
    void cmdCrucibleConfig();
    void cmdCrucibleHelp();

    std::unique_ptr<RawrXD::Crucible::CrucibleEngine> m_crucibleEngine;
    bool m_crucibleInitialized = false;

    // ── Crucible Command IDs ──
    static constexpr int IDM_CRUCIBLE_RUN_ALL       = 10700;
    static constexpr int IDM_CRUCIBLE_RUN_SHADOW    = 10701;
    static constexpr int IDM_CRUCIBLE_RUN_CLUSTER   = 10702;
    static constexpr int IDM_CRUCIBLE_RUN_SEMANTIC  = 10703;
    static constexpr int IDM_CRUCIBLE_CANCEL        = 10704;
    static constexpr int IDM_CRUCIBLE_STATUS        = 10705;
    static constexpr int IDM_CRUCIBLE_REPORT        = 10706;
    static constexpr int IDM_CRUCIBLE_EXPORT_JSON   = 10707;
    static constexpr int IDM_CRUCIBLE_CONFIG        = 10708;
    static constexpr int IDM_CRUCIBLE_HELP          = 10709;

    // ── Game Engine Command IDs ──
    static constexpr int IDM_GAME_ENGINE_DETECT           = 10600;
    static constexpr int IDM_GAME_ENGINE_OPEN             = 10601;
    static constexpr int IDM_GAME_ENGINE_CLOSE            = 10602;
    static constexpr int IDM_GAME_ENGINE_STATUS           = 10603;
    static constexpr int IDM_GAME_ENGINE_BUILD            = 10604;
    static constexpr int IDM_GAME_ENGINE_PLAY             = 10605;
    static constexpr int IDM_GAME_ENGINE_STOP             = 10606;
    static constexpr int IDM_GAME_ENGINE_PAUSE            = 10607;
    static constexpr int IDM_GAME_ENGINE_COMPILE          = 10608;
    static constexpr int IDM_GAME_ENGINE_PROFILER_START   = 10609;
    static constexpr int IDM_GAME_ENGINE_PROFILER_STOP    = 10610;
    static constexpr int IDM_GAME_ENGINE_PROFILER_SNAP    = 10611;
    static constexpr int IDM_GAME_ENGINE_DEBUG_START      = 10612;
    static constexpr int IDM_GAME_ENGINE_DEBUG_STOP       = 10613;
    static constexpr int IDM_GAME_ENGINE_BREAKPOINT       = 10614;
    static constexpr int IDM_GAME_ENGINE_AI_SUMMARY       = 10615;
    static constexpr int IDM_GAME_ENGINE_AI_SCENE         = 10616;
    static constexpr int IDM_GAME_ENGINE_INSTALLATIONS    = 10617;
    static constexpr int IDM_GAME_ENGINE_HELP             = 10618;

    // Unity-specific IDs
    static constexpr int IDM_UNITY_CREATE_SCRIPT          = 10650;
    static constexpr int IDM_UNITY_SCENE_LIST             = 10651;
    static constexpr int IDM_UNITY_ASSET_BROWSER          = 10652;
    static constexpr int IDM_UNITY_PACKAGE_MANAGER        = 10653;

    // Unreal-specific IDs
    static constexpr int IDM_UNREAL_CREATE_CLASS          = 10670;
    static constexpr int IDM_UNREAL_CREATE_MODULE         = 10671;
    static constexpr int IDM_UNREAL_CREATE_PLUGIN         = 10672;
    static constexpr int IDM_UNREAL_GEN_PROJECT_FILES     = 10673;
    static constexpr int IDM_UNREAL_LIVE_CODING           = 10674;
    static constexpr int IDM_UNREAL_COOK_CONTENT          = 10675;
    static constexpr int IDM_UNREAL_PACKAGE_PROJECT       = 10676;
    static constexpr int IDM_UNREAL_LEVEL_LIST            = 10677;
    static constexpr int IDM_UNREAL_BLUEPRINT_LIST        = 10678;

    // ========================================================================
    // PHASE 13: DISTRIBUTED PIPELINE ORCHESTRATOR
    // DAG-based task scheduling, work-stealing thread pool, compute nodes
    // ========================================================================
    void initPipelinePanel();
    void handlePipelineCommand(int commandId);
    void cmdPipelineStatus();
    void cmdPipelineSubmit();
    void cmdPipelineCancel();
    void cmdPipelineListNodes();
    void cmdPipelineAddNode();
    void cmdPipelineRemoveNode();
    void cmdPipelineDAGView();
    void cmdPipelineShowStats();
    void cmdPipelineShutdown();
    bool m_pipelinePanelInitialized = false;

    static constexpr int IDM_PIPELINE_STATUS       = 11000;
    static constexpr int IDM_PIPELINE_SUBMIT       = 11001;
    static constexpr int IDM_PIPELINE_CANCEL       = 11002;
    static constexpr int IDM_PIPELINE_LIST_NODES   = 11003;
    static constexpr int IDM_PIPELINE_ADD_NODE     = 11004;
    static constexpr int IDM_PIPELINE_REMOVE_NODE  = 11005;
    static constexpr int IDM_PIPELINE_DAG_VIEW     = 11006;
    static constexpr int IDM_PIPELINE_STATS        = 11007;
    static constexpr int IDM_PIPELINE_SHUTDOWN     = 11008;

    // ========================================================================
    // PHASE 14: HOTPATCH CONTROL PLANE
    // Patch lifecycle, version graphs, atomic transactions, rollback chains
    // ========================================================================
    void initHotpatchCtrlPanel();
    void handleHotpatchCtrlCommand(int commandId);
    void cmdHPCtrlListPatches();
    void cmdHPCtrlPatchDetail();
    void cmdHPCtrlValidate();
    void cmdHPCtrlStage();
    void cmdHPCtrlApply();
    void cmdHPCtrlRollback();
    void cmdHPCtrlSuspend();
    void cmdHPCtrlAuditLog();
    void cmdHPCtrlTxnBegin();
    void cmdHPCtrlTxnCommit();
    void cmdHPCtrlTxnRollback();
    void cmdHPCtrlDepGraph();
    void cmdHPCtrlStats();
    bool m_hotpatchCtrlPanelInitialized = false;

    static constexpr int IDM_HPCTRL_LIST_PATCHES   = 11100;
    static constexpr int IDM_HPCTRL_PATCH_DETAIL   = 11101;
    static constexpr int IDM_HPCTRL_VALIDATE       = 11102;
    static constexpr int IDM_HPCTRL_STAGE          = 11103;
    static constexpr int IDM_HPCTRL_APPLY          = 11104;
    static constexpr int IDM_HPCTRL_ROLLBACK       = 11105;
    static constexpr int IDM_HPCTRL_SUSPEND        = 11106;
    static constexpr int IDM_HPCTRL_AUDIT_LOG      = 11107;
    static constexpr int IDM_HPCTRL_TXN_BEGIN      = 11108;
    static constexpr int IDM_HPCTRL_TXN_COMMIT     = 11109;
    static constexpr int IDM_HPCTRL_TXN_ROLLBACK   = 11110;
    static constexpr int IDM_HPCTRL_DEP_GRAPH      = 11111;
    static constexpr int IDM_HPCTRL_STATS          = 11112;

    // ========================================================================
    // PHASE 15: STATIC ANALYSIS ENGINE
    // CFG/SSA analysis, dominator trees, loop detection, optimization passes
    // ========================================================================
    void initStaticAnalysisPanel();
    void handleStaticAnalysisCommand(int commandId);
    void cmdSABuildCFG();
    void cmdSAComputeDominators();
    void cmdSAConvertSSA();
    void cmdSADetectLoops();
    void cmdSAOptimize();
    void cmdSAFullAnalysis();
    void cmdSAExportDOT();
    void cmdSAExportJSON();
    void cmdSAShowStats();
    bool m_staticAnalysisPanelInitialized = false;

    static constexpr int IDM_SA_BUILD_CFG          = 11200;
    static constexpr int IDM_SA_COMPUTE_DOMINATORS = 11201;
    static constexpr int IDM_SA_CONVERT_SSA        = 11202;
    static constexpr int IDM_SA_DETECT_LOOPS       = 11203;
    static constexpr int IDM_SA_OPTIMIZE           = 11204;
    static constexpr int IDM_SA_FULL_ANALYSIS      = 11205;
    static constexpr int IDM_SA_EXPORT_DOT         = 11206;
    static constexpr int IDM_SA_EXPORT_JSON        = 11207;
    static constexpr int IDM_SA_STATS              = 11208;

    // ========================================================================
    // PHASE 16: SEMANTIC CODE INTELLIGENCE
    // Cross-references, type inference, symbol resolution, code navigation
    // ========================================================================
    void initSemanticPanel();
    void handleSemanticCommand(int commandId);
    void cmdSemGoToDefinition();
    void cmdSemFindReferences();
    void cmdSemFindImplementations();
    void cmdSemTypeHierarchy();
    void cmdSemCallGraph();
    void cmdSemSearchSymbols();
    void cmdSemFileSymbols();
    void cmdSemFindUnused();
    void cmdSemIndexFile();
    void cmdSemRebuildIndex();
    void cmdSemSaveIndex();
    void cmdSemLoadIndex();
    void cmdSemShowStats();
    bool getEditorCursorFileLineCol(std::string& outFile, uint32_t& outLine1Based, uint32_t& outCol) const;
    void navigateToFileLine(const std::string& filePath, uint32_t line1Based);
    bool m_semanticPanelInitialized = false;

    static constexpr int IDM_SEM_GO_TO_DEF         = 11300;
    static constexpr int IDM_SEM_FIND_REFS         = 11301;
    static constexpr int IDM_SEM_FIND_IMPLS        = 11302;
    static constexpr int IDM_SEM_TYPE_HIERARCHY    = 11303;
    static constexpr int IDM_SEM_CALL_GRAPH        = 11304;
    static constexpr int IDM_SEM_SEARCH_SYMBOLS    = 11305;
    static constexpr int IDM_SEM_FILE_SYMBOLS      = 11306;
    static constexpr int IDM_SEM_UNUSED            = 11307;
    static constexpr int IDM_SEM_INDEX_FILE        = 11308;
    static constexpr int IDM_SEM_REBUILD_INDEX     = 11309;
    static constexpr int IDM_SEM_SAVE_INDEX        = 11310;
    static constexpr int IDM_SEM_LOAD_INDEX        = 11311;
    static constexpr int IDM_SEM_STATS             = 11312;

    // ========================================================================
    // PHASE 17: ENTERPRISE TELEMETRY & COMPLIANCE
    // OTLP tracing, tamper-evident audit, compliance policies, license, GDPR
    // ========================================================================
    void initTelemetryPanel();
    // handleTelemetryCommand already declared above (bool version)
    void cmdTelTraceStatus();
    void cmdTelStartSpan();
    void cmdTelAuditLog();
    void cmdTelAuditVerify();
    void cmdTelComplianceReport();
    void cmdTelShowViolations();
    void cmdTelLicenseStatus();
    void cmdTelUsageMeter();
    void cmdTelMetricsDashboard();
    void cmdTelMetricsFlush();
    void cmdTelExportAudit();
    void cmdTelExportOTLP();
    void cmdTelGDPRExport();
    void cmdTelGDPRDelete();
    void cmdTelSetLevel();
    void cmdTelShowStats();
    bool m_telemetryPanelInitialized = false;

    static constexpr int IDM_TEL_TRACE_STATUS      = 11400;
    static constexpr int IDM_TEL_START_SPAN        = 11401;
    static constexpr int IDM_TEL_AUDIT_LOG         = 11402;
    static constexpr int IDM_TEL_AUDIT_VERIFY      = 11403;
    static constexpr int IDM_TEL_COMPLIANCE_REPORT = 11404;
    static constexpr int IDM_TEL_VIOLATIONS        = 11405;
    static constexpr int IDM_TEL_LICENSE_STATUS    = 11406;
    static constexpr int IDM_TEL_USAGE_METER       = 11407;
    static constexpr int IDM_TEL_METRICS_DASHBOARD = 11408;
    static constexpr int IDM_TEL_METRICS_FLUSH     = 11409;
    static constexpr int IDM_TEL_EXPORT_AUDIT      = 11410;
    static constexpr int IDM_TEL_EXPORT_OTLP       = 11411;
    static constexpr int IDM_TEL_GDPR_EXPORT       = 11412;
    static constexpr int IDM_TEL_GDPR_DELETE       = 11413;
    static constexpr int IDM_TEL_SET_LEVEL         = 11414;
    static constexpr int IDM_TEL_STATS             = 11415;

    // ════════════════════════════════════════════════════════════════════════
    // Phase 49: Copilot Gap Closer (10800–10899)
    // ════════════════════════════════════════════════════════════════════════

    void initCopilotGap();
    void handleCopilotGapCommand(int commandId);
    void createCopilotGapMenu(HMENU parentMenu);

    // General
    void cmdGapInit();
    void cmdGapStatus();
    void cmdGapPerf();
    void cmdGapHelp();

    // Vector Database
    void cmdGapVecDbInit();
    void cmdGapVecDbInsert();
    void cmdGapVecDbSearch();
    void cmdGapVecDbDelete();
    void cmdGapVecDbStatus();
    void cmdGapVecDbBench();

    // Multi-file Composer
    void cmdGapComposerBegin();
    void cmdGapComposerAdd();
    void cmdGapComposerCommit();
    void cmdGapComposerStatus();

    // CRDT Engine
    void cmdGapCrdtInit();
    void cmdGapCrdtInsert();
    void cmdGapCrdtDelete();
    void cmdGapCrdtStatus();

    // Git Context
    void cmdGapGitContext();
    void cmdGapGitBranch();

    std::unique_ptr<RawrXD::CopilotGapCloser> m_copilotGap;
    bool m_copilotGapInitialized = false;

    // ── Copilot Gap Closer Command IDs ──
    // General
    static constexpr int IDM_GAPCLOSE_INIT              = 10800;
    static constexpr int IDM_GAPCLOSE_STATUS            = 10801;
    static constexpr int IDM_GAPCLOSE_PERF              = 10802;
    static constexpr int IDM_GAPCLOSE_HELP              = 10803;
    // Vector Database
    static constexpr int IDM_GAPCLOSE_VECDB_INIT        = 10810;
    static constexpr int IDM_GAPCLOSE_VECDB_INSERT      = 10811;
    static constexpr int IDM_GAPCLOSE_VECDB_SEARCH      = 10812;
    static constexpr int IDM_GAPCLOSE_VECDB_DELETE      = 10813;
    static constexpr int IDM_GAPCLOSE_VECDB_STATUS      = 10814;
    static constexpr int IDM_GAPCLOSE_VECDB_BENCH       = 10815;
    // Composer
    static constexpr int IDM_GAPCLOSE_COMPOSER_BEGIN    = 10820;
    static constexpr int IDM_GAPCLOSE_COMPOSER_ADD      = 10821;
    static constexpr int IDM_GAPCLOSE_COMPOSER_COMMIT   = 10822;
    static constexpr int IDM_GAPCLOSE_COMPOSER_STATUS   = 10823;
    // CRDT
    static constexpr int IDM_GAPCLOSE_CRDT_INIT         = 10830;
    static constexpr int IDM_GAPCLOSE_CRDT_INSERT       = 10831;
    static constexpr int IDM_GAPCLOSE_CRDT_DELETE       = 10832;
    static constexpr int IDM_GAPCLOSE_CRDT_STATUS       = 10833;
    // Git Context
    static constexpr int IDM_GAPCLOSE_GIT_CONTEXT       = 10840;
    static constexpr int IDM_GAPCLOSE_GIT_BRANCH        = 10841;

    // ════════════════════════════════════════════════════════════════════════
    // CURSOR/JB-PARITY FEATURE MODULES — Pluginable subsystems (11500–11599)
    // ════════════════════════════════════════════════════════════════════════

    // ── Telemetry Export (multi-format, audit chain, OTLP) ──
    void initTelemetryExport();
    void shutdownTelemetryExport();
    bool handleTelemetryExportCommand(int commandId);
    void cmdTelExportJSON();
    void cmdTelExportCSV();
    void cmdTelExportPrometheus();
    void cmdTelExportOTLPMulti();
    void cmdTelExportAuditLog();
    void cmdTelExportVerifyChain();
    void cmdTelExportAutoStart();
    void cmdTelExportAutoStop();
    bool m_telemetryExportInitialized = false;

    static constexpr int IDM_TELEXPORT_JSON         = 11500;
    static constexpr int IDM_TELEXPORT_CSV          = 11501;
    static constexpr int IDM_TELEXPORT_PROMETHEUS   = 11502;
    static constexpr int IDM_TELEXPORT_OTLP         = 11503;
    static constexpr int IDM_TELEXPORT_AUDIT_LOG    = 11504;
    static constexpr int IDM_TELEXPORT_VERIFY_CHAIN = 11505;
    static constexpr int IDM_TELEXPORT_AUTO_START   = 11506;
    static constexpr int IDM_TELEXPORT_AUTO_STOP    = 11507;

    // ── Agentic Composer UX (session lifecycle, thinking, file changes) ──
    void initAgenticComposerUX();
    bool handleComposerUXCommand(int commandId);
    void cmdComposerNewSession();
    void cmdComposerEndSession();
    void cmdComposerApproveAll();
    void cmdComposerRejectAll();
    void cmdComposerShowTranscript();
    void cmdComposerShowMetrics();
    bool m_composerUXInitialized = false;
    RawrXD::Agentic::AgenticComposerUX m_composerUX;

    static constexpr int IDM_COMPOSER_NEW_SESSION     = 11510;
    static constexpr int IDM_COMPOSER_END_SESSION     = 11511;
    static constexpr int IDM_COMPOSER_APPROVE_ALL     = 11512;
    static constexpr int IDM_COMPOSER_REJECT_ALL      = 11513;
    static constexpr int IDM_COMPOSER_SHOW_TRANSCRIPT = 11514;
    static constexpr int IDM_COMPOSER_SHOW_METRICS    = 11515;

    // ── @-Mention Context Parser (inline context assembly) ──
    void initContextMentionParser();
    bool handleMentionParserCommand(int commandId);
    void cmdMentionParse();
    void cmdMentionSuggest();
    void cmdMentionAssembleContext();
    void cmdMentionRegisterCustom();
    bool m_mentionParserInitialized = false;

    static constexpr int IDM_MENTION_PARSE            = 11520;
    static constexpr int IDM_MENTION_SUGGEST          = 11521;
    static constexpr int IDM_MENTION_ASSEMBLE_CTX     = 11522;
    static constexpr int IDM_MENTION_REGISTER_CUSTOM  = 11523;

    // ── Vision Encoder (image input, clipboard paste, drag-drop) ──
    void initVisionEncoderUI();
    bool handleVisionEncoderCommand(int commandId);
    void cmdVisionLoadFile();
    void cmdVisionPasteClipboard();
    void cmdVisionScreenshot();
    void cmdVisionBuildPayload();
    void cmdVisionViewIDEGUIAndHotpatch();  // View IDE GUI + audit/hotpatch layout
    bool m_visionEncoderUIInitialized = false;

    // GUI layout hotpatch (image viewer): audit overlaps/zero-size, auto-correct via onSize
    bool auditIDEGUILayout(std::string& reportOut);
    void hotpatchIDELayout();

    static constexpr int IDM_VISION_LOAD_FILE         = 11530;
    static constexpr int IDM_VISION_PASTE_CLIPBOARD   = 11531;
    static constexpr int IDM_VISION_SCREENSHOT        = 11532;
    static constexpr int IDM_VISION_BUILD_PAYLOAD     = 11533;
    static constexpr int IDM_VISION_VIEW_GUI_HOTPATCH = 11534;

    // ── Refactoring Engine (200+ pluginable refactorings) ──
    void initRefactoringEngine();
    bool handleRefactoringCommand(int commandId);
    void cmdRefactorExtractMethod();
    void cmdRefactorExtractVariable();
    void cmdRefactorRenameSymbol();
    void cmdRefactorOrganizeIncludes();
    void cmdRefactorConvertToAuto();
    void cmdRefactorRemoveDeadCode();
    void cmdRefactorShowAll();
    void cmdRefactorLoadPlugin();
    bool m_refactoringEngineInitialized = false;

    static constexpr int IDM_REFACTOR_EXTRACT_METHOD     = 11540;
    static constexpr int IDM_REFACTOR_EXTRACT_VARIABLE   = 11541;
    static constexpr int IDM_REFACTOR_RENAME_SYMBOL      = 11542;
    static constexpr int IDM_REFACTOR_ORGANIZE_INCLUDES  = 11543;
    static constexpr int IDM_REFACTOR_CONVERT_AUTO       = 11544;
    static constexpr int IDM_REFACTOR_REMOVE_DEAD_CODE   = 11545;
    static constexpr int IDM_REFACTOR_SHOW_ALL           = 11546;
    static constexpr int IDM_REFACTOR_LOAD_PLUGIN        = 11547;

    // ── Language Plugin (60+ language descriptors, DLL extensible) ──
    void initLanguageRegistry();
    bool handleLanguageCommand(int commandId);
    void cmdLanguageDetect();
    void cmdLanguageListAll();
    void cmdLanguageLoadPlugin();
    void cmdLanguageSetForFile();
    bool m_languageRegistryInitialized = false;

    static constexpr int IDM_LANG_DETECT              = 11550;
    static constexpr int IDM_LANG_LIST_ALL            = 11551;
    static constexpr int IDM_LANG_LOAD_PLUGIN         = 11552;
    static constexpr int IDM_LANG_SET_FOR_FILE        = 11553;

    // ── Semantic Index (cross-language, dependency graph, type hierarchy) ──
    void initSemanticIndex();
    bool handleSemanticIndexCommand(int commandId);
    void cmdSemanticBuildIndex();
    void cmdSemanticFuzzySearch();
    void cmdSemanticFindReferences();
    void cmdSemanticShowDependencies();
    void cmdSemanticShowTypeHierarchy();
    void cmdSemanticShowCallGraph();
    void cmdSemanticFindCycles();
    void cmdSemanticLoadPlugin();
    bool m_semanticIndexInitialized = false;

    static constexpr int IDM_SEMANTIC_BUILD_INDEX     = 11560;
    static constexpr int IDM_SEMANTIC_FUZZY_SEARCH    = 11561;
    static constexpr int IDM_SEMANTIC_FIND_REFS       = 11562;
    static constexpr int IDM_SEMANTIC_SHOW_DEPS       = 11563;
    static constexpr int IDM_SEMANTIC_TYPE_HIERARCHY  = 11564;
    static constexpr int IDM_SEMANTIC_CALL_GRAPH      = 11565;
    static constexpr int IDM_SEMANTIC_FIND_CYCLES     = 11566;
    static constexpr int IDM_SEMANTIC_LOAD_PLUGIN     = 11567;

    // ── Resource Generator (Docker, K8s, Terraform, CI/CD, config) ──
    void initResourceGenerator();
    bool handleResourceGenCommand(int commandId);
    void cmdResourceGenerate();
    void cmdResourceGenerateProject();
    void cmdResourceListTemplates();
    void cmdResourceSearchTemplates();
    void cmdResourceLoadPlugin();
    bool m_resourceGeneratorInitialized = false;

    static constexpr int IDM_RESOURCE_GENERATE        = 11570;
    static constexpr int IDM_RESOURCE_GEN_PROJECT     = 11571;
    static constexpr int IDM_RESOURCE_LIST_TEMPLATES  = 11572;
    static constexpr int IDM_RESOURCE_SEARCH          = 11573;
    static constexpr int IDM_RESOURCE_LOAD_PLUGIN     = 11574;

    // ── Cursor/JB-Parity Menu Builder ──
    void createCursorParityMenu(HMENU parentMenu);
    bool handleCursorParityCommand(int commandId);
    void initAllCursorParityModules();

    // ════════════════════════════════════════════════════════════════════
    // TIER 2: HIGH VISIBILITY (Daily Friction) — Features 11–19
    // ════════════════════════════════════════════════════════════════════

    // Master lifecycle
    void initTier2Cosmetics();
    void shutdownTier2Cosmetics();
    bool handleTier2Command(int commandId);
    void renderTier2Overlays(HDC hdc);

public:
    // ── LSP Symbol Kinds (used by Outline, CodeLens, Inlay) ──
    enum LSPSymbolKind {
        SK_File = 1, SK_Module, SK_Namespace, SK_Package,
        SK_Class, SK_Method, SK_Property, SK_Field,
        SK_Constructor, SK_Enum, SK_Interface, SK_Function,
        SK_Variable, SK_Constant, SK_String, SK_Number,
        SK_Boolean, SK_Array, SK_Object, SK_Key,
        SK_Null, SK_EnumMember, SK_Struct, SK_Event,
        SK_Operator, SK_TypeParameter
    };
private:

    // ── OutlineSymbol (enhanced outline model) ──
    struct OutlineSymbol {
        int kind = 0;          // LSPSymbolKind
        std::string name;
        int line     = 0;
        int column   = 0;
        int endLine  = 0;
        std::string detail;
        bool expanded = true;
        std::vector<OutlineSymbol> children;
    };

    std::vector<OutlineSymbol> m_outlineSymbols;
    std::string m_outlineFilter;
    bool m_outlineSortByName = false;

    // 15. Document Symbols Outline (enhanced)
    void initOutlinePanel();
    void refreshOutlineFromLSP();
    void setOutlineFilter(const std::string& filter);
    void setOutlineSortOrder(bool byName);
    void filterOutlineView(const std::string& filter);
    void sortOutlineView(bool byName);
    void renderOutlineIcons(HDC hdc, int kind, RECT iconRect);
    std::string symbolKindName(int kind) const;
    std::string outlineKindToString(int kind) const;
    COLORREF outlineKindColor(int kind) const;

    // ── DiffLine / DiffHunk (Git Diff Side-by-Side) ──
    enum class DiffLineType { Context, Added, Removed };

    struct DiffLine {
        std::string text;
        DiffLineType type = DiffLineType::Context;
        int lineNumber = -1;
    };

    struct DiffHunk {
        std::string header;
        int oldStart = 0;
        int newStart = 0;
        std::vector<DiffLine> lines;
    };

    // 11. Git Diff Side-by-Side
    void initGitDiffViewer();
    void shutdownGitDiffViewer();
    void showGitDiffSideBySide(const std::string& filePath);
    void closeGitDiffViewer();
    void parseUnifiedDiff(const std::string& diff, const std::string& filePath);
    void markDiffLines();
    void createGitDiffPanel();
    void populateGitDiffPane(HWND hwndRichEdit, const std::vector<DiffLine>& lines,
                              const std::string& header);
    void navigateDiffHunk(int direction);
    static LRESULT CALLBACK GitDiffPanelProc(HWND, UINT, WPARAM, LPARAM);

    bool m_gitDiffVisible = false;
    HWND m_hwndGitDiffPanel = nullptr;
    HWND m_hwndGitDiffLeft  = nullptr;
    HWND m_hwndGitDiffRight = nullptr;
    HFONT m_gitDiffFont     = nullptr;
    std::vector<DiffLine> m_gitDiffLeftLines;
    std::vector<DiffLine> m_gitDiffRightLines;
    std::vector<DiffHunk> m_gitDiffHunks;
    int m_gitDiffCurrentHunk = -1;

    // ── TerminalProfile / TerminalTabInfo (Terminal Tabs) ──
    struct TerminalProfile {
        std::string name;
        std::string shellPath;
        std::string shellArgs;
        std::string icon;
        COLORREF color = RGB(204, 204, 204);
    };

    struct TerminalTabInfo {
        int profileIndex = 0;
        std::string title;
        COLORREF color = RGB(204, 204, 204);
        bool active    = true;
        HWND hwndOutput = nullptr;
        std::unique_ptr<Win32TerminalManager> manager;
    };

    // 12. Integrated Terminal Tabs
    void initTerminalTabs();
    void createTerminalTabBar();
    void addTerminalTab(int profileIndex);
    void closeTerminalTab(int tabIndex);
    void switchTerminalTab(int tabIndex);
    void showTerminalProfileMenu();
    static LRESULT CALLBACK TerminalTabBarProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hwndTerminalTabBar = nullptr;
    int m_activeTerminalTab   = 0;
    std::vector<TerminalProfile> m_terminalTabProfiles;
    std::vector<TerminalTabInfo> m_terminalTabs;

    // 13. Hover Documentation Tooltips
    void initHoverTooltip();
    void shutdownHoverTooltip();
    void showHoverTooltip(int screenX, int screenY, const std::string& content);
    void dismissHoverTooltip();
    void onEditorMouseHover(int charPos);
    static LRESULT CALLBACK HoverTooltipProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hwndHoverPopup   = nullptr;
    bool m_hoverVisible     = false;
    std::string m_hoverContent;
    HFONT m_hoverFont       = nullptr;
    HFONT m_hoverBoldFont   = nullptr;

    // 14. Parameter Hints (Signature Help)
    void initSignatureHelp();
    void shutdownSignatureHelp();
    void triggerSignatureHelp();
    void showSignaturePopup(int screenX, int screenY);
    void dismissSignatureHelp();
    void updateSignatureActiveParam(int paramIndex);
    static LRESULT CALLBACK SignatureHelpProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hwndSignaturePopup = nullptr;
    bool m_signatureVisible   = false;
    std::string m_signatureContent;
    int m_signatureActiveParam = 0;
    int m_signatureParamCount  = 0;
    HFONT m_signatureFont      = nullptr;

    // ── ReferenceResult (Find All References UI) ──
    struct ReferenceResult {
        std::string filePath;
        int line     = 0;
        int column   = 0;
        std::string contextLine;
    };

    // ── RenameChange / RenamePreviewState (Rename Refactoring Preview) ──
    struct RenameChange {
        std::string filePath;
        int line     = 0;
        int column   = 0;
        std::string oldText;
        std::string newText;
        std::string contextLine;
        bool selected = true;
    };

    struct RenamePreviewState {
        std::string oldName;
        std::string newName;
        std::vector<RenameChange> changes;
        bool visible   = false;
        HWND hwndPanel = nullptr;
        HWND hwndList  = nullptr;
        HFONT hFont    = nullptr;
    };

    // 17. Rename Refactoring Preview
    void initRenamePreview();
    void shutdownRenamePreview();
    void showRenamePreview(const std::string& oldName, const std::string& newName,
                            const std::vector<RenameChange>& changes);
    void closeRenamePreview();
    void applyRenameChanges();
    void applySelectedRenames();
    void renderRenamePreviewItem(HDC hdc, RECT itemRect, const RenameChange& change);
    static LRESULT CALLBACK RenamePreviewProc(HWND, UINT, WPARAM, LPARAM);

    RenamePreviewState m_renamePreview;

    static constexpr int IDC_RENAME_PREVIEW     = 11280;
    static constexpr int IDC_RENAME_INPUT       = 11281;
    static constexpr int IDC_RENAME_CHECKLIST   = 11282;
    static constexpr int IDC_RENAME_APPLY_BTN   = 11283;
    static constexpr int IDC_RENAME_CANCEL_BTN  = 11284;

    // 16. Find All References UI
    void initReferencePanel();
    void shutdownReferencePanel();
    void showFindAllReferences(const std::string& symbol,
                                const std::vector<ReferenceResult>& results);
    void closeReferencePanel();
    void navigateToReference(int refIndex);
    void cmdFindAllReferences(const std::string& symbol);
    static LRESULT CALLBACK ReferencePanelProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hwndReferencePanel = nullptr;
    HWND m_hwndReferenceTree  = nullptr;
    bool m_referencePanelVisible = false;
    std::string m_referenceSymbol;
    std::vector<ReferenceResult> m_referenceResults;
    HFONT m_referenceFont     = nullptr;

    // ── CodeLensEntry ──
    struct CodeLensEntry {
        int line            = 0;
        std::string symbol;
        int referenceCount  = 0;
        std::string text;
    };

    // 18. CodeLens (Reference Counts)
    void initCodeLens();
    void shutdownCodeLens();
    void refreshCodeLens();
    int  countSymbolReferences(const std::string& symbol);
    void renderCodeLens(HDC hdc);
    void toggleCodeLens();

    bool m_codeLensEnabled = true;
    std::vector<CodeLensEntry> m_codeLensEntries;
    HFONT m_codeLensFont   = nullptr;

    // ── InlayHintEntry ──
    enum class InlayHintKind { Type, Parameter, Enum };

    struct InlayHintEntry {
        int line     = 0;
        int column   = 0;
        std::string text;
        InlayHintKind kind = InlayHintKind::Type;
    };

    // 19. Inlay Type Hints
    void initInlayHints();
    void shutdownInlayHints();
    void refreshInlayHints();
    std::string inferTypeFromExpression(const std::string& expr);
    void renderInlayHints(HDC hdc);
    void toggleInlayHints();

    bool m_inlayHintsEnabled = true;
    std::vector<InlayHintEntry> m_inlayHintEntries;
    HFONT m_inlayHintFont  = nullptr;

    // ── Tier 2 Command IDs (11700–11799) ──
    static constexpr int IDM_TIER2_GITDIFF           = 11700;
    static constexpr int IDM_TIER2_GITDIFF_CLOSE     = 11701;
    static constexpr int IDM_TIER2_GITDIFF_PREV      = 11702;
    static constexpr int IDM_TIER2_GITDIFF_NEXT      = 11703;
    static constexpr int IDM_TIER2_TERMINAL_NEW      = 11710;
    static constexpr int IDM_TIER2_TERMINAL_CLOSE    = 11711;
    static constexpr int IDM_TIER2_HOVER             = 11720;
    static constexpr int IDM_TIER2_SIGHELP           = 11721;
    static constexpr int IDM_TIER2_OUTLINE_REFRESH   = 11730;
    static constexpr int IDM_TIER2_OUTLINE_FILTER    = 11731;
    static constexpr int IDM_TIER2_OUTLINE_SORT      = 11732;
    static constexpr int IDM_TIER2_FIND_REFS         = 11740;
    static constexpr int IDM_TIER2_RENAME_PREVIEW    = 11750;
    static constexpr int IDM_TIER2_CODELENS_TOGGLE   = 11760;
    static constexpr int IDM_TIER2_CODELENS_REFRESH  = 11761;
    static constexpr int IDM_TIER2_INLAY_TOGGLE      = 11770;
    static constexpr int IDM_TIER2_INLAY_REFRESH     = 11771;

    // ════════════════════════════════════════════════════════════════════
    // TIER 3: POLISH (Quality of Life) — Features 31–39
    // ════════════════════════════════════════════════════════════════════

    // Master init/shutdown
    void initTier3Polish();
    void shutdownTier3Polish();
    bool handleTier3Timer(UINT_PTR timerId);

    // 31. Smooth Caret Animation
    void initSmoothCaret();
    void shutdownSmoothCaret();
    void updateCaretTarget();
    void onCaretAnimationTick();
    void renderSmoothCaret(HDC hdc);

    struct CaretAnimation {
        float currentX   = 0.0f;
        float currentY   = 0.0f;
        float targetX    = 0.0f;
        float targetY    = 0.0f;
        bool  blinkOn    = true;
        bool  animating  = false;
        bool  enabled    = false;
        int   blinkPhase = 0;
    };
    CaretAnimation m_caretAnim;

    // 32. Font Ligatures (DirectWrite)
    void initDirectWriteLigatures();
    void shutdownDirectWriteLigatures();
    void toggleLigatures();
    IDWriteTextLayout* createLigatureLayout(const std::wstring& text, float maxWidth);

    IDWriteFactory*    m_dwFactory     = nullptr;
    IDWriteTextFormat* m_dwTextFormat  = nullptr;
    bool               m_ligaturesEnabled = false;

    // 33. High DPI Polish
    void  onDpiChanged(UINT newDpi, const RECT* suggestedRect);
    int   dpiScaleValue(int basePixels) const;
    float getDpiScaleFactor() const;
    float m_dpiScaleFactor = 1.0f;

    // 34. Theme Toggle Animation
    void beginThemeTransition(int targetThemeId);
    void onThemeAnimationTick();
    void applyThemeByIdAnimated(int themeId);

    struct ThemeTransition {
        IDETheme fromTheme;
        IDETheme toTheme;
        int      targetThemeId = 0;
        UINT     elapsedMs     = 0;
        bool     active        = false;
    };
    ThemeTransition m_themeTransition;

    // 35. File Watcher Indicators
    void initFileWatcher();
    void shutdownFileWatcher();
    void startWatchingFile(const std::string& filePath);
    void stopWatchingFile();
    void onExternalFileChange(const std::string& changedFile);
    void showFileChangedToast();
    void reloadCurrentFile();

    std::unique_ptr<IocpFileWatcher> m_fileWatcher;
    std::string                      m_watchedFilePath;
    bool                             m_fileChangedExternally = false;

    // 36. Save Status Indicator
    void updateSaveStatusIndicator();
    void updateTabModifiedIndicator();
    void markFileModified();
    void markFileSaved();

    // 37. Format on Save Progress
    void showFormatOnSaveProgress();
    void onFormatComplete(bool success);
    void onFormatStatusTimerExpired();
    bool formatAndSave();
    bool requestLSPFormat();
    bool m_formatInProgress  = false;
    bool m_lspFormatEnabled  = false;

    // 38. Language Mode Quick Switch
    void showLanguageModeSelector();

    // 39. Encoding Selector UI
    void showEncodingSelector();
    void reopenWithEncoding(const char* encodingName, int codePage);
    void saveWithEncoding(const char* encodingName, int codePage);
    int  m_currentEncoding = 65001; // CP_UTF8

    // Tier 3: Status bar click routing
    void handleStatusBarClick(int x, int y);

    // ════════════════════════════════════════════════════════════════════
    // TIER 3: COSMETICS — Features 20–30
    // ════════════════════════════════════════════════════════════════════

    // Master lifecycle
    void initTier3Cosmetics();
    void shutdownTier3Cosmetics();
    bool handleTier3CosmeticsCommand(int commandId);
    bool handleTier3CosmeticsTimer(UINT_PTR timerId);

    // Composite paint helpers (called from gutter / editor paint)
    void paintTier3CosmeticsGutter(HDC hdc, const RECT& gutterRect);
    void paintTier3CosmeticsEditor(HDC hdc, const RECT& editorRect);

    // 20. Bracket Pair Colorization
    void initBracketPairColorization();
    void shutdownBracketPairColorization();
    void toggleBracketPairColorization();
    void applyBracketPairColors();
    bool m_bracketPairEnabled = false;

    // 21. Indentation Guides
    void initIndentationGuides();
    void shutdownIndentationGuides();
    void toggleIndentationGuides();
    void paintIndentationGuides(HDC hdc, const RECT& editorRect);
    bool m_indentGuidesEnabled = false;

    // 22. Whitespace Rendering Toggle
    void initWhitespaceRendering();
    void toggleWhitespaceRendering();
    void paintWhitespaceGlyphs(HDC hdc, const RECT& editorRect);
    bool m_whitespaceVisible = false;

    // 23. Word Wrap Indicator
    void initWordWrapIndicator();
    void toggleWordWrapIndicator();
    void paintWordWrapIndicators(HDC hdc, const RECT& gutterRect);
    bool m_wordWrapIndicatorEnabled = false;

    // 24. Relative Line Numbers
    void initRelativeLineNumbers();
    void toggleRelativeLineNumbers();
    void paintRelativeLineNumbers(HDC hdc, RECT& rc);
    bool m_relativeLineNumbers = false;

    // 25. Zen Mode (Distraction Free)
    void initZenMode();
    void toggleZenMode();
    void enterZenMode();
    void exitZenMode();
    bool m_zenModeActive = false;

    struct ZenModePrevState {
        bool sidebarWasVisible     = false;
        bool statusBarWasVisible   = false;
        bool activityBarWasVisible = false;
        bool tabBarWasVisible      = false;
        bool panelWasVisible       = false;
        bool menuWasVisible        = false;
        bool wasMaximized          = false;
    };
    ZenModePrevState m_zenModePrevState;

    // 26. Tab Pinning
    void pinTab(int index);
    void unpinTab(int index);
    void reorderTabsForPinning();
    void rebuildTabBarFromModel();

    // 27. Preview Tabs (Single Click)
    void openPreviewTab(const std::string& filePath, const std::string& displayName);
    void promotePreviewToFull();

    // 28. Search Results in Scrollbar
    void initScrollbarSearchMarkers();
    void updateScrollbarSearchMarkers(const std::string& searchTerm);
    void paintScrollbarSearchMarkers(HDC hdc, const RECT& scrollRect);
    void clearScrollbarSearchMarkers();
    bool m_scrollbarSearchEnabled = false;
    std::vector<int> m_scrollSearchMatches;

    // 29. Quick Fix Lightbulb
    void initQuickFixLightbulb();
    void shutdownQuickFixLightbulb();
    void updateLightbulbPosition();
    void paintLightbulb(HDC hdc, const RECT& gutterRect);
    void onLightbulbClicked();
    bool m_lightbulbEnabled = false;
    bool m_lightbulbVisible = false;
    int  m_lightbulbLine    = -1;

    struct CodeAction {
        std::string title;
        std::string kind;   // "quickfix", "refactor", "suppress"
        int diagnosticIndex = -1;
    };
    std::vector<CodeAction> requestCodeActions(int line);
    void applyCodeAction(const CodeAction& action);

    // 30. Code Folding Controls
    void initCodeFolding();
    void shutdownCodeFolding();
    void toggleCodeFolding();
    void parseFoldRegions();
    void toggleFoldAtLine(int line);
    void foldAll();
    void unfoldAll();
    void paintFoldingControls(HDC hdc, const RECT& gutterRect);
    int  getFoldRegionAtGutterClick(int y);
    bool m_codeFoldingEnabled = false;

    struct FoldRegion {
        int startLine       = 0;
        int endLine         = 0;
        int depth           = 0;
        bool collapsed      = false;
        std::string foldedText;
        int foldMarkerStart = 0;
        int foldMarkerLen   = 0;
    };
    std::vector<FoldRegion> m_foldRegions;
    void foldRegion(FoldRegion& region);
    void unfoldRegion(FoldRegion& region);

    // ════════════════════════════════════════════════════════════════════
    // TIER 1: CRITICAL COSMETIC (Mainstream Adoption) — Features 1–10
    // ════════════════════════════════════════════════════════════════════

    // Master init/shutdown/dispatch
    void initTier1Cosmetics();
    void shutdownTier1Cosmetics();
    bool handleTier1Command(int commandId);
    bool handleTier1Timer(UINT_PTR timerId);
    bool handleTier1MouseWheel(WPARAM wParam, LPARAM lParam);

    // Tier 1 Command IDs (12000–12099)
    static constexpr int IDM_T1_SMOOTH_SCROLL_TOGGLE  = 12000;
    static constexpr int IDM_T1_SMOOTH_SCROLL_SPEED   = 12001;
    static constexpr int IDM_T1_MINIMAP_TOGGLE        = 12010;
    static constexpr int IDM_T1_MINIMAP_HIGHLIGHT     = 12011;
    static constexpr int IDM_T1_BREADCRUMBS_TOGGLE    = 12020;
    static constexpr int IDM_T1_FUZZY_PALETTE         = 12030;
    static constexpr int IDM_T1_FUZZY_FILES           = 12031;
    static constexpr int IDM_T1_FUZZY_SYMBOLS         = 12032;
    static constexpr int IDM_T1_SETTINGS_GUI          = 12040;
    static constexpr int IDM_T1_SETTINGS_RESET        = 12042;
    static constexpr int IDM_T1_WELCOME_SHOW          = 12050;
    static constexpr int IDM_T1_WELCOME_CLONE         = 12051;
    static constexpr int IDM_T1_WELCOME_OPEN_FOLDER   = 12052;
    static constexpr int IDM_T1_WELCOME_NEW_FILE      = 12053;
    static constexpr int IDM_T1_ICON_THEME_SET        = 12060;
    static constexpr int IDM_T1_ICON_THEME_SETI       = 12061;
    static constexpr int IDM_T1_ICON_THEME_MATERIAL   = 12062;
    static constexpr int IDM_T1_TAB_DRAG_ENABLE       = 12070;
    static constexpr int IDM_T1_SPLIT_VERTICAL        = 12080;
    static constexpr int IDM_T1_SPLIT_HORIZONTAL      = 12081;
    static constexpr int IDM_T1_SPLIT_GRID_2X2        = 12082;
    static constexpr int IDM_T1_SPLIT_CLOSE           = 12083;
    static constexpr int IDM_T1_SPLIT_FOCUS_NEXT      = 12084;
    static constexpr int IDM_T1_UPDATE_CHECK          = 12090;
    static constexpr int IDM_T1_UPDATE_INSTALL        = 12091;
    static constexpr int IDM_T1_UPDATE_DISMISS        = 12092;
    static constexpr int IDM_T1_UPDATE_RELEASE_NOTES  = 12093;

    // 1. Smooth Scroll Animation
    void initSmoothScroll();
    void shutdownSmoothScroll();
    bool onSmoothMouseWheel(WPARAM wParam, LPARAM lParam);
    void onSmoothScrollTick();

    struct SmoothScrollState {
        bool  enabled    = true;
        float velocityY  = 0.0f;
        float currentY   = 0.0f;
        float targetY    = 0.0f;
        bool  animating  = false;
    };
    SmoothScrollState m_smoothScroll;

    // 2. Minimap Enhanced (extends existing minimap)
    void initMinimapEnhanced();
    void paintMinimapEnhanced(HDC hdc, const RECT& rect);
    bool m_minimapHighlightCursor = true;

    // 3. Breadcrumbs (main implementation in Win32IDE_Breadcrumbs.cpp)
    static constexpr int IDC_BREADCRUMB_BAR = 9825;  // ESP: control ID for symbol path bar
    struct BreadcrumbItem {
        std::string label;
        std::string symbolKind;
        int line  = 0;
        int column = 0;
    };
    void createBreadcrumbBar(HWND hwndParent);
    void updateBreadcrumbs();
    void updateBreadcrumbsOnCursorMove();
    void updateBreadcrumbsForCursor(int line, int column);
    void onBreadcrumbClick(int index);
    void paintBreadcrumbs(HDC hdc, RECT& rc);
    static LRESULT CALLBACK BreadcrumbProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    std::vector<BreadcrumbItem> m_breadcrumbPath;
    int  m_breadcrumbHeight = 22;
    HFONT m_breadcrumbFont = nullptr;
    HWND  m_hwndBreadcrumbs = nullptr;

    // 4. Command Palette Fuzzy Search
    void initFuzzySearch();
    void showFuzzyCommandPalette();
    void showFuzzyPaletteWindow();
    void showFuzzyFileFinder();
    void showFuzzySymbolSearch();

    struct FuzzyCommandEntry {
        int         commandId = 0;
        const char* label     = nullptr;
        const char* category  = nullptr;
        const char* cliAlias  = nullptr;
    };
    std::vector<FuzzyCommandEntry> m_fuzzyCommandLabels;

    // 5. Settings GUI
    void initSettingsGUI();
    void showSettingsGUIDialog();

    // 6. Welcome / Onboarding Page
    void initWelcomePage();
    void showWelcomePage();
    void handleWelcomeCloneRepo();
    void handleWelcomeOpenFolder();
    void handleWelcomeNewFile();
    bool m_showWelcomeOnStartup = true;
    bool m_welcomePageShown     = false;

    // 7. File Icon Theme Support
    void initFileIconTheme();
    int  getFileIconIndex(const std::string& filename) const;
    void setFileIconTheme(const std::string& themeName);
    void showFileIconThemeSelector();
    HIMAGELIST  m_fileIconImageList  = nullptr;
    std::string m_currentIconTheme   = "seti";

    // 8. Drag-and-Drop File Tabs
    void initTabDragDrop();
    void reorderTab(int fromIndex, int toIndex);
    void onTabDragTick();
    static LRESULT CALLBACK TabBarDragProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    WNDPROC m_originalTabBarProc = nullptr;
    bool    m_tabDragEnabled     = true;
    bool    m_tabDragging        = false;
    int     m_dragTabIndex       = -1;
    int     m_dragInsertIndex    = -1;
    int     m_dragStartX         = 0;
    int     m_dragStartY         = 0;

    // 9. Split Editor (Grid Layout)
    void initSplitEditor();
    HWND createEditorPane(HWND parent, const RECT& bounds);
    void splitEditorVertical();
    void splitEditorHorizontal();
    void splitEditorGrid2x2();
    void closeSplitEditor();
    void focusNextSplitPane();
    void layoutSplitPanes();

    struct SplitEditorPane {
        HWND        hwnd      = nullptr;
        int         row       = 0;
        int         col       = 0;
        std::string filePath;
    };
    std::vector<SplitEditorPane> m_splitPanes;
    bool m_splitEditorActive  = false;
    int  m_splitOrientation   = 0; // 0=none, 1=vert, 2=horiz, 3=grid

    // 10. Auto-Update Notification UI
    void initAutoUpdateUI();
    void shutdownAutoUpdateUI();
    void checkForUpdates();
    void showUpdateNotification();
    void installUpdate();
    void dismissUpdateNotification();
    void showReleaseNotes();
    bool        m_updateAvailable  = false;
    bool        m_updateDismissed  = false;
    std::string m_updateVersion;
    std::string m_updateUrl;
    NOTIFYICONDATAA m_trayIconData = {};

    // ════════════════════════════════════════════════════════════════════════
    // FLAGSHIP FEATURES — Product Pillars (13000–13299)
    // ════════════════════════════════════════════════════════════════════════
    // Three flagship product pillars that unify existing subsystems into
    // coherent, auditable, enterprise-grade capabilities:
    //   1. Provable AI Coding Agent   — cryptographic proof + chain-of-custody
    //   2. AI-Native Reverse Eng IDE  — AI-powered binary analysis + vuln scan
    //   3. Airgapped Enterprise Env   — offline licensing + compliance + DLP
    // ════════════════════════════════════════════════════════════════════════

    // ── Flagship Lifecycle ──
    void initFlagshipFeatures();
    void shutdownFlagshipFeatures();
    bool handleFlagshipCommand(int commandId);

    // ────────────────────────────────────────────────────────────────────────
    // 1. Provable AI Coding Agent (13000–13019)
    //    Cryptographic attestation chain, Merkle-rooted proof bundles,
    //    replay-deterministic verification, pre/post + invariant obligations.
    //    Wires: DeterministicReplayEngine, AgentTranscript, BoundedAgentLoop.
    // ────────────────────────────────────────────────────────────────────────
    void initProvableAgent();
    bool handleProvableAgentCommand(int commandId);
    void cmdProvableShow();
    void cmdProvableStart();
    void cmdProvableRecord();
    void cmdProvableVerify();
    void cmdProvableReplay();
    void cmdProvableExport();
    void cmdProvableReset();
    void cmdProvableStats();
    bool m_provableAgentInitialized = false;

public:
    static constexpr int IDM_PROVABLE_SHOW     = 13000;
    static constexpr int IDM_PROVABLE_START    = 13001;
    static constexpr int IDM_PROVABLE_RECORD   = 13002;
    static constexpr int IDM_PROVABLE_VERIFY   = 13003;
    static constexpr int IDM_PROVABLE_REPLAY   = 13004;
    static constexpr int IDM_PROVABLE_EXPORT   = 13005;
    static constexpr int IDM_PROVABLE_RESET    = 13006;
    static constexpr int IDM_PROVABLE_STATS    = 13007;
private:

    // ────────────────────────────────────────────────────────────────────────
    // 2. AI-Native Reverse Engineering IDE (13020–13039)
    //    AI-powered symbol renaming, vulnerability scanning, call-graph
    //    analysis, binary diffing, AI annotation with confidence scores.
    //    Wires: RawrCodex, RawrReverseEngine, PDB native parser.
    // ────────────────────────────────────────────────────────────────────────
    void initAIReverseEngineering();
    bool handleAIReverseEngCommand(int commandId);
    void cmdAIREShow();
    void cmdAIRELoad();
    void cmdAIRERename();
    void cmdAIREVulnScan();
    void cmdAIREAnnotate();
    void cmdAIRECallGraph();
    void cmdAIREDiff();
    void cmdAIREExport();
    void cmdAIREStats();
    bool m_aiReverseEngInitialized = false;

public:
    static constexpr int IDM_AIRE_SHOW         = 13020;
    static constexpr int IDM_AIRE_LOAD         = 13021;
    static constexpr int IDM_AIRE_AI_RENAME    = 13022;
    static constexpr int IDM_AIRE_VULNSCAN     = 13023;
    static constexpr int IDM_AIRE_ANNOTATE     = 13024;
    static constexpr int IDM_AIRE_CALLGRAPH    = 13025;
    static constexpr int IDM_AIRE_DIFF         = 13026;
    static constexpr int IDM_AIRE_EXPORT       = 13027;
    static constexpr int IDM_AIRE_STATS        = 13028;
private:

    // ────────────────────────────────────────────────────────────────────────
    // 3. Airgapped Enterprise AI Dev Environment (13040–13059)
    //    Offline model vault, HWID-locked license validation, 9-framework
    //    compliance (GDPR/SOX/HIPAA/PCI/ISO27001/FedRAMP/ITAR/EAR/NIST),
    //    DLP scanning, network firewall verification, workspace encryption.
    //    Wires: EnterpriseLicense, License_Shield, TelemetryCompliance.
    // ────────────────────────────────────────────────────────────────────────
    void initAirgappedEnterprise();
    bool handleAirgappedCommand(int commandId);
    void cmdAirgapShow();
    void cmdAirgapCompliance();
    void cmdAirgapModels();
    void cmdAirgapAudit();
    void cmdAirgapDLP();
    void cmdAirgapFirewall();
    void cmdAirgapLicense();
    void cmdAirgapEncrypt();
    void cmdAirgapExport();
    void cmdAirgapStats();
    bool m_airgappedEnterpriseInitialized = false;

public:
    static constexpr int IDM_AIRGAP_SHOW       = 13040;
    static constexpr int IDM_AIRGAP_COMPLIANCE = 13041;
    static constexpr int IDM_AIRGAP_MODELS     = 13042;
    static constexpr int IDM_AIRGAP_AUDIT      = 13043;
    static constexpr int IDM_AIRGAP_DLP        = 13044;
    static constexpr int IDM_AIRGAP_FIREWALL   = 13045;
    static constexpr int IDM_AIRGAP_LICENSE    = 13046;
    static constexpr int IDM_AIRGAP_ENCRYPT    = 13047;
    static constexpr int IDM_AIRGAP_EXPORT     = 13048;
    static constexpr int IDM_AIRGAP_STATS      = 13049;
private:

    // ════════════════════════════════════════════════════════════════════
    // TIER 5: COSMETIC FEATURES (#40-#50) — Line Ending, Network, Test
    //         Explorer, Debug Watch, Call Stack, Marketplace, Telemetry
    //         Dashboard, Shortcut Editor, Color Picker, Emoji, Crash
    // ════════════════════════════════════════════════════════════════════

    // Master lifecycle
    void initTier5Cosmetics();
    bool handleTier5Command(int commandId);

    // 40. Line Ending Selector
    void initLineEndingSelector();
    bool handleLineEndingCommand(int commandId);

    // 41. Network Panel
    void initNetworkPanel();
    bool handleNetworkCommand(int commandId);
    void cmdNetworkShowPanel();
    void cmdNetworkAddPort();
    void cmdNetworkRemovePort();
    void cmdNetworkTogglePort();
    void cmdNetworkListPorts();
    void cmdNetworkStatus();

    // 42. Test Explorer
    void initTestExplorer();
    bool handleTestExplorerCommand(int commandId);
    void cmdTestExplorerShow();
    void cmdTestExplorerRun();
    void cmdTestExplorerRefresh();
    void cmdTestExplorerFilter();

    // 43. Debug Watch Format
    void initDebugWatchFormat();
    bool handleDebugWatchCommand(int commandId);

    // 44. Call Stack Symbols
    void initCallStackSymbols();
    bool handleCallStackCommand(int commandId);

    // 45. Marketplace
    void initMarketplace();
    bool handleMarketplaceCommand(int commandId);
    void cmdMarketplaceShow();
    void cmdMarketplaceSearch();
    void cmdMarketplaceInstall();
    void cmdMarketplaceUninstall();
    void cmdMarketplaceList();
    void cmdMarketplaceStatus();

    // 46. Telemetry Dashboard
    void initTelemetryDashboard();
    bool handleTelemetryDashboardCommand(int commandId);
    void cmdTelDashShow();
    void cmdTelDashLog();
    void cmdTelDashClear();
    void cmdTelDashExport();
    void cmdTelDashStats();

    // 47. Shortcut Editor Panel
    void initShortcutEditorPanel();
    void initShortcutEditor();  // implementation in Win32IDE_ShortcutEditor.cpp
    bool handleShortcutEditorCommand(int commandId);
    void cmdShortcutEditorShow();
    void cmdShortcutEditorRecord();
    void cmdShortcutEditorReset();
    void cmdShortcutEditorSave();
    void cmdShortcutEditorList();

    // 48. Color Picker
    void initColorPicker();
    bool handleColorPickerCommand(int commandId);
    void cmdColorPickerScan();
    void cmdColorPickerPick();
    void cmdColorPickerInsert();
    void cmdColorPickerList();

    // 49. Emoji Support
    void initEmojiSupport();
    bool handleEmojiCommand(int commandId);
    void cmdEmojiPicker();
    void cmdEmojiConfig();
    void cmdEmojiTest();

    // 50. Crash Reporter
    void initCrashReporter();
    bool handleCrashReporterCommand(int commandId);
    void cmdCrashShow();
    void cmdCrashTest();
    void cmdCrashLog();
    void cmdCrashClear();
    void cmdCrashStats();

public:
    // Tier 5 Command IDs (11500–11609)
    static constexpr int IDM_LINEENDING_DETECT     = 11500;
    static constexpr int IDM_LINEENDING_TO_LF      = 11509;

    static constexpr int IDM_NETWORK_SHOW          = 11510;
    static constexpr int IDM_NETWORK_ADD_PORT      = 11511;
    static constexpr int IDM_NETWORK_REMOVE_PORT   = 11512;
    static constexpr int IDM_NETWORK_TOGGLE        = 11513;
    static constexpr int IDM_NETWORK_LIST          = 11514;
    static constexpr int IDM_NETWORK_STATUS        = 11519;

    static constexpr int IDM_TESTEXPLORER_SHOW     = 11520;
    static constexpr int IDM_TESTEXPLORER_RUN      = 11521;
    static constexpr int IDM_TESTEXPLORER_REFRESH  = 11522;
    static constexpr int IDM_TESTEXPLORER_FILTER   = 11529;

    static constexpr int IDM_DBGWATCH_SHOW         = 11530;
    static constexpr int IDM_DBGWATCH_CLEAR        = 11539;

    static constexpr int IDM_CALLSTACK_CAPTURE     = 11540;
    static constexpr int IDM_CALLSTACK_RESOLVE     = 11549;

    static constexpr int IDM_MARKETPLACE_SHOW      = 11550;
    static constexpr int IDM_MARKETPLACE_SEARCH    = 11551;
    static constexpr int IDM_MARKETPLACE_INSTALL   = 11552;
    static constexpr int IDM_MARKETPLACE_UNINSTALL = 11553;
    static constexpr int IDM_MARKETPLACE_LIST      = 11554;
    static constexpr int IDM_MARKETPLACE_STATUS    = 11559;

    static constexpr int IDM_TELDASH_LOG           = 11565;
    static constexpr int IDM_TELDASH_CLEAR         = 11566;
    static constexpr int IDM_TELDASH_EXPORT        = 11567;
    static constexpr int IDM_TELDASH_SHOW          = 11568;  // avoid conflict with IDM_SEMANTIC_BUILD_INDEX 11560
    static constexpr int IDM_TELDASH_STATS         = 11569;

    static constexpr int IDM_SHORTCUT_SHOW         = 11570;
    static constexpr int IDM_SHORTCUT_RECORD      = 11571;
    static constexpr int IDM_SHORTCUT_RESET       = 11572;
    static constexpr int IDM_SHORTCUT_SAVE        = 11573;
    static constexpr int IDM_SHORTCUT_LIST        = 11579;

    static constexpr int IDM_COLORPICK_SCAN        = 11580;
    static constexpr int IDM_COLORPICK_PICK        = 11581;
    static constexpr int IDM_COLORPICK_INSERT      = 11582;
    static constexpr int IDM_COLORPICK_LIST        = 11589;

    static constexpr int IDM_EMOJI_PICKER          = 11590;
    static constexpr int IDM_EMOJI_INSERT         = 11591;
    static constexpr int IDM_EMOJI_CONFIG         = 11592;
    static constexpr int IDM_EMOJI_TEST            = 11599;

    static constexpr int IDM_CRASH_SHOW            = 11600;
    static constexpr int IDM_CRASH_TEST            = 11601;
    static constexpr int IDM_CRASH_LOG             = 11602;
    static constexpr int IDM_CRASH_CLEAR          = 11603;
    static constexpr int IDM_CRASH_STATS           = 11609;

    bool m_telemetryDashboardInitialized = false;
    bool m_crashReporterInitialized      = false;
    bool m_colorPickerInitialized        = false;
    bool m_networkPanelInitialized       = false;
    bool m_testExplorerInitialized       = false;
    bool m_marketplaceInitialized        = false;
    bool m_shortcutEditorInitialized     = false;
    bool m_emojiSupportInitialized       = false;
private:
};
