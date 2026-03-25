#pragma once

// Windows / Winsock include order (required for MSVC + MinGW):
//   winsock2.h → ws2tcpip.h → windows.h → commctrl.h → commdlg.h
// winsock2 MUST come before windows.h to avoid winsock1 vs winsock2 conflicts.
// commctrl.h and commdlg.h MUST come after windows.h for CALLBACK, HWND etc.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _Return_type_success_
#define _Return_type_success_(expr)
#endif

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <sal.h>
#include <windows.h>

// Custom window messages
#ifndef WM_AI_BACKEND_STATUS
#define WM_AI_BACKEND_STATUS (WM_USER + 0x500)  // wParam: 1=connected 0=offline
#endif

#ifndef _Return_type_success_
#define _Return_type_success_(expr)
#endif

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

// Keep common-control/dialog headers in implementation files to reduce
// transitive WinSDK surface in this mega-header.



// Undefine Windows macros that conflict with our code
#ifdef ERROR
#undef ERROR
#endif

#include "../../include/editor_engine.h"
#include "../../include/plugin_system/win32_plugin_loader.h"
#include "../full_agentic_ide/FullAgenticIDE.h"
#include "../gguf_loader.h"
#include "../model_source_resolver.h"
#include "../modules/codex_ultimate.h"
#include "../modules/copilot_gap_closer.h"
#include "../modules/crucible_engine.h"
#include "../modules/engine_manager.h"
#include "../modules/game_engine_manager.h"
#include "../streaming_gguf_loader.h"
#include "IDELogger.h"
#include "TransparentRenderer.h"
#include "Win32IDE_AgenticBridge.h"
#include "Win32IDE_Autonomy.h"
#include "Win32IDE_IRCBridge.h"
#include "Win32IDE_SubAgent.h"
#include "Win32IDE_WebView2.h"
#include "Win32TerminalManager.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


#include "../../include/mcp_integration.h"
#include "../core/70b_gguf_hotpatch.h"
#include "../core/governor_throttling.h"
#include "../core/native_inference_pipeline.hpp"
#include "../core/problems_aggregator.hpp"
#include "../modules/ExtensionLoader.hpp"
#include "../modules/vscode_extension_api.h"
#include "../ui/tool_action_status.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
#include <climits>
#include <condition_variable>


#include "Win32IDE_Fwd.h"
// Tier 3: File Watcher — full type needed for unique_ptr destructor
#include "IocpFileWatcher.h"

#include "../../include/agentic/agentic_composer_ux.h"
#include "../agent/agentic_failure_detector.hpp"
#include "../agentic/OllamaProvider.h"
#include "agentic_mode_switcher.hpp"


#include "Win32IDE_Commands.h"
#include "Win32IDE_Types.h"

// Forward declarations for peek overlay (definition in Win32IDE_PeekOverlay.cpp)
class PeekOverlayWindow;
struct PeekOverlayWindowDeleter
{
    void operator()(PeekOverlayWindow*) noexcept;
};

// Forward declarations for plugin managers defined in component translation units
class EnterpriseStressTester;
class SQLite3DatabaseManager;
class TelemetryExportManager;
class RefactoringPluginManager;
class LanguagePluginManager;
class ResourceGeneratorManager;

// Forward declarations for Omega Orchestrator
namespace rawrxd
{
class OmegaOrchestrator;
enum class QualityMode : int;
}  // namespace rawrxd

namespace RawrXD
{
class LayerEvictionManager;
}
struct LayerEvictionManagerDeleter
{
    void operator()(RawrXD::LayerEvictionManager* ptr) noexcept;
};

// Lightweight types used by Language Plugin subsystem
namespace IDEPlugin
{
enum class TokenType
{
    Keyword,
    Identifier,
    String,
    Number,
    Comment,
    Operator,
    Punctuation,
    Type,
    Function,
    Variable,
    Class,
    Unknown
};

struct SyntaxToken
{
    TokenType type;
    int start;
    int length;
    std::string text;
};

enum class CompletionItemKind
{
    Text,
    Method,
    Function,
    Constructor,
    Field,
    Variable,
    Class,
    Interface,
    Module,
    Property,
    Unit,
    Value,
    Enum,
    Keyword,
    Snippet,
    Color,
    File,
    Reference,
    Folder,
    EnumMember,
    Constant,
    Struct,
    Event,
    Operator,
    TypeParameter
};

struct CompletionItem
{
    std::string label;
    std::string detail;
    std::string documentation;
    CompletionItemKind kind;
    std::string insertText;
};

struct Diagnostic
{
    std::string source;
    int line;
    int column;
    std::string message;
    std::string severity;
};
}  // namespace IDEPlugin

struct StressTestResults
{
    uint64_t operationsCompleted = 0;
    uint64_t errorsEncountered = 0;
    double avgResponseTimeUs = 0.0;
    double operationsPerSecond = 0.0;
    bool passed = false;
};

using SQLiteQueryCallback = std::function<int(int, char**, char**)>;

struct RefactoringOption
{
    std::string name;
    std::string description;
};

class Win32IDE
{
    friend class AgenticBridge;
    friend class PeekOverlayWindow;
    friend class vscode::VSCodeExtensionAPI;
    friend void onCreateTrampoline(void* self, HWND hwnd);
    friend void deferredInitTrampoline(void* self);
    friend void bgInitBody(void* self);

  public:
    void runWorkspaceSearchFromDialog(const std::string& query);
    void deferredHeavyInitBody();  // SEH-safe body, called from bg thread via sehRunBgThread

    enum class OutputSeverity
    {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Success = 4
    };

    enum class PanelTab
    {
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
    // ── Parity-audit: Visibility Watchdog ──────────────────────────────────
    void startVisibilityWatchdog();
    void stopVisibilityWatchdog();
    static DWORD WINAPI VisibilityWatchdogThread(LPVOID param);
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

    // Agentic Framework — Full Agentic IDE owns the bridge (single entry point: src/full_agentic_ide/)
    std::unique_ptr<full_agentic_ide::FullAgenticIDE> m_fullAgenticIDE;
    AgenticBridge* m_agenticBridge = nullptr;  // Non-owning; set from m_fullAgenticIDE->getBridge()
    bool m_multiAgentEnabled = false;          // Multi-agent orchestration toggle
    void initializeAgenticBridge();
    bool ensureAgenticBridgeHasModel(const std::string& path);
    void initializeAutonomy();
    void onAgentStartLoop();
    void onAgentExecuteCommand();
    void onAgentConfigureModel();
    void onAgentViewTools();
    void onAgentViewStatus();
    void onAgentStop();

    // Autonomy Framework Controls
    std::unique_ptr<AutonomyManager> m_autonomyManager;  // high-level autonomous orchestrator
    void onAutonomyStart();
    void onAutonomyStop();
    void onAutonomyToggle();
    void onAutonomySetGoal();
    void onAutonomyViewStatus();
    void onAutonomyViewMemory();

    // Autonomous Agentic Pipeline (Task 1) + external AgentCoordinator (Task 2)
    std::unique_ptr<RawrXD::AutonomousAgenticPipelineCoordinator> m_autonomousPipeline;
    void* m_agentCoordinatorForPipeline = nullptr;  // AgentCoordinatorHandle when linked
    void ensureAutonomousPipelineInitialized();
    void onPipelineRun();
    void onPipelineAutonomyStart();
    void onPipelineAutonomyStop();

    // ── Omega Orchestrator — Phase Ω: The Last Tool ──────────────────────
    // Full autonomous software development pipeline with PERCEIVE→PLAN→ARCHITECT→
    // IMPLEMENT→VERIFY→DEPLOY→OBSERVE→EVOLVE capability
    rawrxd::OmegaOrchestrator* m_omegaOrchestrator = nullptr;
    bool m_omegaActive = false;
    void initializeOmegaOrchestrator();
    void onOmegaStart();
    void onOmegaStop();
    void onOmegaSubmitTask();
    void onOmegaRunCycle();
    void onOmegaShowStatus();
    void onOmegaViewPipeline();
    void onOmegaSpawnAgent();
    void onOmegaSetQualityMode(rawrxd::QualityMode mode);
    void onOmegaCancelTask();
    void onOmegaWorldModel();
    void onOmegaExportStats();
    void onOmegaDiagnostics();

    // ── Agentic Planning Orchestrator — Full Approval Gates ──────────────
    void onPlanningStart();
    void onPlanningShowQueue();
    void onPlanningApproveStep();
    void onPlanningRejectStep();
    void onPlanningExecuteStep();
    void onPlanningExecuteAll();
    void onPlanningRollback();
    void onPlanningSetPolicy();
    void onPlanningViewStatus();
    void onPlanningDiagnostics();

    // AI Extended Features Handlers
    void onAIModeMax();
    void onAIModeDeepThink();
    void onAIModeDeepResearch();
    void onAIModeNoRefusal();
    /** Sync main menu + agent chat panel checkboxes from AgenticBridge / NativeAgent (after init or config load). */
    void syncAgentModeUiFromBridge();
    void onAIContextSize(int sizeEnum);

    // Memory Plugin System (Native VSIX Style)
    // Note: loadMemoryPlugin is the legacy single-DLL loader.
    // For the full plugin system, use m_pluginLoader (Phase 43).
    void loadMemoryPlugin(const std::string& path);


    // ========================================================================
    // AGENT MEMORY — Persistent observation store for agentic iterations
    // Phase 19B: Lets agents/models/swarms/end-users store & recall context
    // ========================================================================
    struct AgentMemoryEntry
    {
        std::string key;
        std::string value;
        std::string source;  // "agent", "user", "swarm", etc.
        uint64_t timestampMs;
    };
    void onAgentMemoryStore(const std::string& key, const std::string& value, const std::string& source = "agent");
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
    int getTerminalKillTimeout() const;

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
    bool testFindPatternRVAParity(const std::string& binaryPath, const std::string& pattern, uint64_t& outRVA);

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
    std::string handleTranscendenceEndpoint(const std::string& method, const std::string& path,
                                            const std::string& body);

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
    int createTerminalPanePublic(Win32TerminalManager::ShellType type, const std::string& name)
    {
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
    static DWORD WINAPI deferredHeavyInitThreadProc(LPVOID param);
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
#if defined(RAWR_HAS_MODEL_ANATOMY)
    /// Full tensor manifest JSON for the loaded model (`m_loadedModelPath`); empty if none or parse failed.
    std::string getModelAnatomyJson(bool pretty = true) const;
    /// Neurological diff JSON for two GGUF paths; empty if either file fails to parse.
    std::string getModelDiffJson(const std::string& pathA, const std::string& pathB, bool pretty = true) const;
#endif

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
    void showModelLoadError(const std::string& detail);
    static LRESULT CALLBACK ModelProgressProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // SubAgent / Swarm UX — progress, status display
    void showSubAgentProgress(const std::string& operation, int totalTasks);
    void updateSubAgentProgress(int completedTasks, int totalTasks, const std::string& currentTask);
    void hideSubAgentProgress();
    void showSwarmStatus();

    // (AI Backend Abstraction — Phase 8B: see full declaration block below Settings section)

    // AI Inference Engine - Local GGUF Model Chat
    struct InferenceConfig
    {
        int maxTokens = 512;
        int contextWindow = 4096;  // Added Context
        float temperature = 0.7f;
        float topP = 0.9f;
        int topK = 40;
        float repetitionPenalty = 1.1f;
        std::string systemPrompt;
        bool streamOutput = true;
    };

    // ── Context Window Token Usage Tracking ──
    struct ContextWindowUsage
    {
        int maxTokens = 128000;
        int systemTokens = 0;       // System instructions
        int toolDefTokens = 0;      // Tool definitions
        int userContextTokens = 0;  // User context / workspace info
        int messageTokens = 0;      // Conversation messages
        int toolResultTokens = 0;   // Tool call results

        int totalUsed() const
        {
            return systemTokens + toolDefTokens + userContextTokens + messageTokens + toolResultTokens;
        }
        float percentage() const { return maxTokens > 0 ? (float(totalUsed()) / float(maxTokens)) * 100.0f : 0.0f; }
        bool isWarning() const { return percentage() >= 75.0f; }
        bool isDanger() const { return percentage() >= 90.0f; }
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
    std::string invokePowerShellCmdlet(const std::string& cmdlet,
                                       const std::map<std::string, std::string>& parameters = {});

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
    struct PSScriptAnalysis
    {
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
    bool newPowerShellDrive(const std::string& name, const std::string& root,
                            const std::string& provider = "FileSystem");

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
    bool registerPowerShellEvent(const std::string& sourceIdentifier, const std::string& eventName,
                                 const std::string& action);
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
    std::string executePowerShellWorkflow(const std::string& workflowName,
                                          const std::map<std::string, std::string>& parameters = {});

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

    // Terminal Split Panes (Win32IDE_TerminalSplit.cpp)
    void splitTerminalImpl(bool horizontal);
    void focusNextTerminalPane();

  public:
    // Peek Definition/References Overlay (Win32IDE_PeekView.cpp)
    struct PeekLocation
    {
        std::string filePath;
        int line = 0;
        int col = 0;
        int endCol = 0;
        std::string preview;
        std::vector<std::string> contextLines;
        int contextStartLine = 0;
    };
    void showPeekDefinition();
    void showPeekReferences();
    void closePeekView();
    void navigatePeekLocation(int index);
    void showPeekOverlay(const std::string& symbol, const std::vector<PeekLocation>& locations, bool isDefinition);
    std::vector<PeekLocation> scanForDefinitions(const std::string& symbol);
    std::vector<PeekLocation> scanForReferences(const std::string& symbol);

    // Auto-Save System (Win32IDE_AutoSave.cpp)
    enum class AutoSaveMode
    {
        Off = 0,
        AfterDelay = 1,
        OnFocusChange = 2,
        OnWindowChange = 3
    };
    void initAutoSave();
    void shutdownAutoSave();
    void toggleAutoSave();
    void setAutoSaveMode(AutoSaveMode mode);
    void startAutoSaveTimer();
    void stopAutoSaveTimer();
    void handleAutoSaveTimer(UINT_PTR timerId);
    void autoSaveDirtyFiles();
    void onEditorFocusLost();
    void onWindowDeactivated();
    void checkExternalFileChanges();
    void reloadCurrentFile();

    // Multi-Cursor Editing (Win32IDE_MultiCursor.cpp)
    void initMultiCursor();
    bool isMultiCursorActive() const;
    void clearSecondaryCursors();
    void addCursorAtPosition(int charPos);
    void addCursorAbove();
    void addCursorBelow();
    void selectNextOccurrence();
    void selectAllOccurrences();
    void multiCursorInsertText(const std::string& text);
    void multiCursorBackspace();
    void multiCursorDelete();
    void paintMultiCursorIndicators(HDC hdc);
    bool handleMultiCursorKeyDown(WPARAM vk, bool ctrl, bool shift, bool alt);
    bool handleMultiCursorChar(WPARAM ch);
    bool handleMultiCursorClick(int charPos, bool altHeld);
    void updateMultiCursorStatusBar();
    int getMultiCursorCount() const;

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
    void handleBuildCommand(int commandId);
    void handleTerminalCommand(int commandId);
    void handleToolsCommand(int commandId);
    void handleModulesCommand(int commandId);
    void handleHelpCommand(int commandId);
    void handleGitCommand(int commandId);
    void handleAgentCommand(int commandId);

    // Swarm state
    bool isSwarmRunning() const;

    // Edit helpers
    void pastePlainText();

    // License / Feature dialogs
    void showLicenseCreatorDialog();
    void showFeatureRegistryDialog();
    void showEnterpriseLicenseDialog();

    // Monaco settings / Thermal dashboard
    void showMonacoSettingsDialog();
    void showThermalDashboard();

    // Simple modal input dialog (implemented in Win32IDE_Quantum.cpp)
    bool DialogBoxWithInput(const wchar_t* title, const wchar_t* prompt, wchar_t* buffer, size_t bufferSize);

    // Security scans (Top-50 P0)
    bool handleSecurityCommand(int commandId);
    void RunSecretsScan();
    void RunSastScan();
    void RunDependencyAudit();
    void initSecurityDashboard();
    void ShowSecurityDashboard();
    void ExportSBOM();

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
    void handleReverseEngineeringSetBinaryFromActive();
    void handleReverseEngineeringSetBinaryFromDebugTarget();
    void handleReverseEngineeringSetBinaryFromBuildOutput();
    void handleReverseEngineeringDisassembleAtRIP();

    /** Set the current binary for Reverse Engineering menu (Disassemble, DumpBin, CFG, etc.).
     *  Called automatically when debug is launched; can be called after build to target the built exe. */
    void setCurrentBinaryForReverseEngineering(const std::string& path);

    HMENU createReverseEngineeringMenu();  // Helper to add the menu

    // ========================================================================
    // DECOMPILER VIEW — Direct2D Split View (Phase 18B)
    // Direct2D-rendered decompiler (left) + disassembly (right) with:
    //   1. Syntax coloring via tokenizeLine() / getTokenColor()
    //   2. Bidirectional synchronized selection (click ↔ highlight)
    //   3. Right-click variable rename with SSA graph propagation
    // ========================================================================
    void initDecompilerView();
    void showDecompilerView(const std::string& decompCode, const std::string& disasmText,
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
    void applyThemeToAllControls();  // Deep apply: sidebar, activity bar, tabs, status bar, panels
    void showThemeEditor();
    void showThemePicker();                                                      // Proper picker dialog with preview
    void logThemeDiff(const IDETheme& before, const IDETheme& candidate) const;  // Debug diff during preview
    void resetToDefaultTheme();
    void populateBuiltinThemes();                                         // Register all 16 built-in themes
    IDETheme getBuiltinTheme(int themeId) const;                          // Factory method
    void applyThemeById(int themeId);                                     // IDM_THEME_xxx → apply
    void setWindowTransparency(BYTE alpha);                               // WS_EX_LAYERED alpha
    void showTransparencySlider();                                        // Custom slider dialog
    void buildThemeMenu(HMENU hParentMenu);                               // Build Appearance submenu
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
    friend LRESULT CALLBACK Tier1EnhancedMinimapWndProc(HWND, UINT, WPARAM, LPARAM);
    friend LRESULT CALLBACK MinimapWndProc(HWND, UINT, WPARAM, LPARAM);

    // Theme session persistence
    void saveSessionTheme(nlohmann::json& session);
    void restoreSessionTheme(const nlohmann::json& session);

    // Code snippets
    void loadCodeSnippets();
    void saveCodeSnippets();
    void insertSnippet(const std::string& trigger);
    void showSnippetManager();
    void createSnippet();

    // Snippet Tab-Stop Engine (Win32IDE_SnippetEngine.cpp)
    void insertSnippetWithTabStops(const std::string& snippetBody);
    bool snippetNextField();
    bool snippetPrevField();
    void selectCurrentSnippetField();
    void endSnippetSession();
    bool isSnippetSessionActive() const;
    bool handleSnippetTab(bool shiftHeld);
    void loadSnippetsFromJson(const std::string& filePath, const std::string& language);
    void loadBuiltInSnippets();
    void loadUserSnippetFiles();
    int findSnippetByTrigger(const std::string& trigger, const std::string& language);
    std::vector<std::pair<std::string, std::string>> getSnippetCompletions(const std::string& prefix,
                                                                           const std::string& language);
    bool tryExpandSnippetAtCursor();

    // Integrated help
    void showGetHelp(const std::string& cmdlet = "");
    void showCommandReference();
    void showPowerShellDocs();
    void searchHelp(const std::string& query);

  public:
    // Enhanced output panel (public — accessed by BuildRunner, AgentStreamingBridge, AuditDashboard, etc.)
    void createOutputTabs();
    void addOutputTab(const std::string& name);
    void appendToOutput(const std::string& text, const std::string& tabName = "General",
                        OutputSeverity severity = OutputSeverity::Info);
    void clearOutput(const std::string& tabName = "General");
    void formatOutput(const std::string& text, COLORREF color, const std::string& tabName = "");

  private:
    // Enhanced clipboard
    void copyWithFormatting();
    void pasteWithoutFormatting();
    void copyLineNumbers();
    void showClipboardHistory();
    void clearClipboardHistory();

    // ========================================================================
    // INCREMENTAL SYNTAX COLORING (RichEdit-based)
    // ========================================================================
    enum class TokenType
    {
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
    struct SyntaxToken
    {
        int start;       // Character offset in the editor
        int length;      // Token length
        TokenType type;  // Token classification
    };
    enum class SyntaxLanguage
    {
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
    struct VisibleLineRange
    {
        int firstLine;   // First visible line (0-based)
        int lastLine;    // Last visible line (0-based)
        int lineCount;   // Total visible line count (including margin)
        int lineHeight;  // Pixel height per line
    };

    // Syntax coloring methods
    void initSyntaxColorizer();
    void applySyntaxColoring();
    void applySyntaxColoringForRange(int startChar, int endChar);
    void applySyntaxColoringForVisibleRange();        // Named wrapper — colors only the visible range
    VisibleLineRange getVisibleEditorLines() const;   // Accessor for the visible line range
    SyntaxLanguage getCurrentSyntaxLanguage() const;  // Accessor for the detected language enum
    std::vector<SyntaxToken> tokenizeLine(const std::string& line, int lineStartOffset, SyntaxLanguage lang);
    std::vector<SyntaxToken> tokenizeDocument(const std::string& text, SyntaxLanguage lang);
    COLORREF getTokenColor(TokenType type) const;
    SyntaxLanguage detectLanguageFromExtension(const std::string& filePath) const;
    void onEditorContentChanged();    // Debounced EN_CHANGE handler for syntax coloring
    void toggleSyntaxHighlighting();  // Toggle syntax highlighting on/off
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
    /** Writes `performance.vulkanRenderer` to rawrxd.config.json (cwd, else exe dir). */
    void persistPerformanceVulkanRendererToConfig();

    // Agent Inline Annotations
    enum class AnnotationSeverity
    {
        Hint = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Suggestion = 4
    };
    // Annotation Action Types — what happens when the user clicks an annotation
    enum class AnnotationActionType
    {
        None = 0,    // No action (display only)
        JumpToLine,  // Navigate to the referenced line
        ApplyFix,    // Apply a suggested code fix
        AskAgent,    // Send the annotation context to the agent for elaboration
        OpenFile,    // Open a referenced file
        RunCommand,  // Execute a command palette command
        Suppress     // Dismiss/suppress the annotation permanently
    };

    // Annotation Action — attached to an annotation for click-to-act
    struct AnnotationAction
    {
        AnnotationActionType type;
        std::string label;        // Human-readable label for context menus
        int targetLine;           // For JumpToLine: destination line (1-based)
        std::string targetFile;   // For OpenFile / JumpToLine in another file
        std::string fixContent;   // For ApplyFix: the replacement text
        int fixStartLine;         // For ApplyFix: start line of replacement range
        int fixEndLine;           // For ApplyFix: end line of replacement range
        std::string agentPrompt;  // For AskAgent: pre-populated prompt
        std::string commandId;    // For RunCommand: command to execute
    };

    struct InlineAnnotation
    {
        int line;    // 1-based line number
        int column;  // 0-based column (for inline placement)
        AnnotationSeverity severity;
        std::string text;                       // Annotation message
        std::string source;                     // "agent", "linter", "diagnostics", etc.
        COLORREF color;                         // Computed from severity
        bool visible;                           // Can be toggled
        std::vector<AnnotationAction> actions;  // Click actions (may have multiple per annotation)
    };
    void addAnnotation(int line, AnnotationSeverity severity, const std::string& text,
                       const std::string& source = "agent");
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
    void createAnnotationOverlay(HWND hwndParent);

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
    void paintMinimap(HDC hdc, RECT& rc);
    void minimapHitTest(int mouseY, int& outLine);

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
    struct CommandPaletteItem
    {
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
    std::vector<CommandPaletteItem> m_commandRegistry;    // reserved large upfront to avoid realloc invalidation
    std::vector<CommandPaletteItem> m_filteredCommands;   // mirrors registry; also reserved
    std::vector<std::vector<int>> m_fuzzyMatchPositions;  // per-item match highlight positions
    uint64_t m_commandRegistryVersion = 0;                // bump on rebuild to detect stale filtered indexes
    uint64_t m_filteredVersion = 0;                       // version of current filtered list
    std::unordered_map<int, int> m_commandMRU;            // commandId -> usage count (session-only, no disk)

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
    int replaceText(const std::string& searchText, const std::string& replaceText, bool all, bool caseSensitive,
                    bool wholeWord, bool useRegex);
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
    enum class SidebarView
    {
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
    static LRESULT CALLBACK SidebarContentProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static WNDPROC s_sidebarContentOldProc;

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
    void createSearchPanel();
    void performSearch();
    void searchDirectory(const std::string& dir, const char* query, int depth, bool caseSensitive, bool useRegex,
                         bool wholeWord, const std::string& includeFilter, const std::string& excludeFilter);
    void performSearchReplace(bool replaceAll);

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
    void debugConsoleEvaluate(const std::string& expression);
    void debugConsoleAppend(const std::string& text, COLORREF color);
    std::string debugConsoleHistoryPrev();
    std::string debugConsoleHistoryNext();
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
    void handleExtensionCommand(int commandId);

    // Tooling & Diagnostics
    void ExecuteToolingSmokeTest();  // CRITICAL FIX: Bounds-checked tool validation
  private:
    void installFromVSIXFile();
    void showMultiFileSearchDialog();
    void showCICDSettingsDialog();
    void showModelRegistryDialog();
    friend LRESULT CALLBACK CICDSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK MultiFileSearchDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK ModelRegistryDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK Tier1LegacySettingsGUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK SettingsGUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    friend LRESULT CALLBACK auditDashboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  private:
    // ========================================================================
    // POWERSHELL ACCESS - Private State
    // ========================================================================

    // PowerShell Runtime State
    struct PowerShellState
    {
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
    struct PSCommand
    {
        int id;
        std::string command;
        bool async;
        std::function<void(const std::string&)> callback;
    };
    std::vector<PSCommand> m_psCommandQueue;
    int m_nextPSCommandId;

    // PowerShell Job Tracking
    struct PSJob
    {
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
    struct PSModule
    {
        std::string name;
        std::string version;
        std::string path;
        bool loaded;
        std::vector<std::string> exportedCmdlets;
        std::vector<std::string> exportedFunctions;
    };
    std::map<std::string, PSModule> m_psModuleCache;

    // PowerShell Function Registry
    std::map<std::string, std::string> m_psFunctions;  // name -> body

    // PowerShell Event Handlers
    std::map<std::string, std::string> m_psEventHandlers;  // sourceId -> action

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
    bool m_useStreamingLoader;     // preference to use streaming loader to minimize memory
    bool m_useVulkanRenderer;      // preference to use Vulkan renderer if enabled
    bool m_useTitanKernel = true;  // use Titan ASM/DLL inference when available (menu-togglable)

    // Unified Model Source Resolver (HuggingFace, Ollama blobs, HTTP, local files)
    std::unique_ptr<RawrXD::ModelSourceResolver> m_modelResolver;

    // Streaming / Model UX state
    HWND m_hwndModelProgressBar;        // Progress bar control
    HWND m_hwndModelProgressLabel;      // Status text label
    HWND m_hwndModelProgressContainer;  // Container panel
    HWND m_hwndModelCancelBtn;          // Cancel button
    std::atomic<bool> m_modelOperationActive;
    std::atomic<bool> m_modelOperationCancelled;
    std::atomic<float> m_modelProgressPercent;
    std::string m_modelProgressStatus;
    // =========================================================================
    // LOCK ORDERING - acquire mutexes only in this canonical order to prevent
    // deadlock.  Never hold a lower-ranked lock while acquiring a higher one.
    //
    //  Rank  Member                     Guards
    //  ----  -------------------------  ------------------------------------
    //   1    m_backendMutex             AI backend selection / hot-swap
    //   2    m_inferenceMutex           Inference engine state + thread
    //   3    m_lspMutex                 LSP server lifecycle per language
    //   4    m_lspResponseMutex         JSON-RPC pending response map
    //   5    m_lspDiagnosticsMutex      Per-file diagnostic vectors
    //   6    m_asmMutex                 ASM symbol tables (mutable analysis)
    //   7    m_hybridMutex              Hybrid completion provider cache
    //   8    m_outputMutex              Output panel append queue
    //   9    m_modelProgressMutex       Model load progress % + status string
    //  10    m_routerMutex              Command-routing dispatch table
    //  11    m_failureIntelligenceMutex Failure-mode heuristics state
    //  12    m_ghostTextCacheMutex      Ghost-text / inline-completion cache
    //  13    m_eventBufferMutex         Agent-event ring buffer
    //  14    m_agentMemoryMutex         Key-value agent memory store
    //  15    m_streamingOutputMutex     Streaming token append buffer
    //
    // Watchdog thread touches NO mutexes - it communicates exclusively via
    // PostMessage / InterlockedCompareExchange / m_shuttingDown.load().
    // =========================================================================
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
    bool m_nativeEngineLoaded = false;
    std::mutex m_outputMutex;

    // Native Inference Pipeline — zero-dependency local AI core
    std::unique_ptr<RawrXD::NativeInferencePipeline> m_nativePipeline;
    bool m_nativePipelineReady = false;

    // 70B GGUF Hotpatch System
    std::unique_ptr<RawrXD::GGUFHotpatch> m_ggufHotpatch;

    // Governor/Throttling System
    std::unique_ptr<RawrXD::GovernorThrottling> m_governorThrottling;
    std::unique_ptr<RawrXD::LayerEvictionManager, LayerEvictionManagerDeleter> m_layerEvictionManager;

    // Window Procedures for Subclassing
    static LRESULT CALLBACK CommandInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SidebarProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam,
                                            LPARAM lParam);  // Renamed to avoid overload conflict
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

    // Plugin Signature Verification (Plugin Signature #56)
    void initPluginSignatureVerifier();
    bool verifyPluginBeforeLoad(const wchar_t* pluginPath);
    bool initializeCoreRuntimeSpine();
    void shutdownCoreRuntimeSpine();

    // File Explorer Sidebar - tree view items
    HWND m_hwndFileTree;
    std::map<HTREEITEM, std::string> m_treeItemPaths;

    HINSTANCE m_hInstance;
    HWND m_hwndMain;
    // ── Parity-audit: Visibility Watchdog members ──────────────────────────
    HANDLE m_watchdogThread = nullptr;
    volatile LONG m_watchdogRunning = 0;  // 1 = active, 0 = stop requested
    HWND m_hwndEditor;
    HWND m_hwndLineNumbers;  // Line number gutter
    HWND m_hwndTabBar;       // Editor tab bar
    WNDPROC m_oldLineNumberProc;
    int m_lineNumberWidth;  // Width of the gutter in pixels
    int m_currentLine;      // Current cursor line (1-based)

    // Tab tracking
    struct EditorTab
    {
        std::string filePath;
        std::string displayName;
        std::string content;
        bool modified = false;
        bool isPinned = false;   // Tier3C #26: Pinned tabs
        bool isPreview = false;  // Tier3C #27: Preview tabs
    };
    std::vector<EditorTab> m_editorTabs;
    int m_activeTabIndex;

    HWND m_hwndCommandInput;
    HWND m_hwndStatusBar;
    bool m_aiAvailable = false; // Set by onAIBackendVerified()
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
    HWND m_hwndGitStatus = nullptr;     // Source control status EDIT
    HWND m_hwndGitBranch = nullptr;     // Branch label STATIC
    HWND m_hwndGitDiff = nullptr;       // Diff view EDIT
    HWND m_hwndGitCommitMsg = nullptr;  // Commit message EDIT
    std::vector<GitFile> m_currentGitFiles;
    bool m_gitPanelVisible;
    WNDPROC m_gitPanelProc;
    std::string m_gitRepoPath;
    bool m_gitAutoRefresh;
    HWND m_hwndCommitDialog;  // commit dialog handle

    // Git panel methods (Win32IDE_GitPanel.cpp)
    void createGitPanel();
    void refreshGitStatus();
    void handleGitPanelCommand(WORD cmdId);

    // File operations
    std::vector<std::string> m_recentFiles;
    static const size_t MAX_RECENT_FILES = 10;
    std::string m_currentDirectory;
    std::string m_defaultFileExtension;
    bool m_autoSaveEnabled;
    AutoSaveMode m_autoSaveMode = AutoSaveMode::Off;
    UINT_PTR m_autoSaveTimerId = 0;
    UINT_PTR m_fileWatchTimerId = 0;
    int m_autoSaveIntervalMs = 1000;
    std::filesystem::file_time_type m_lastKnownFileWriteTime;

    // Debug console REPL members
    HWND m_hwndDebugConsole = nullptr;
    HWND m_hwndDebugConsoleOutput = nullptr;
    HWND m_hwndDebugConsoleInput = nullptr;
    HWND m_hwndOutputPanel = nullptr;
    WNDPROC m_debugConsoleOrigInputProc = nullptr;
    std::vector<std::string> m_debugConsoleHistory;
    int m_debugConsoleHistoryIndex = -1;

    // Debug engine handle (nullptr when no debugger attached)
    void* m_debugEngine = nullptr;

    // Peek overlay members
    HWND m_hwndPeekOverlay = nullptr;
    HWND m_hwndPeekFileList = nullptr;
    HWND m_hwndPeekContent = nullptr;
    HWND m_hwndPeekCloseBtn = nullptr;
    std::vector<std::pair<std::string, int>> m_peekLocations;
    int m_peekCurrentIndex = -1;

    // Menu command system
    std::map<int, std::string> m_commandDescriptions;
    std::map<int, bool> m_commandStates;

    // Theme system
    IDETheme m_currentTheme;
    IDETheme m_themeBeforePreview;  // Saved for Cancel in theme picker
    std::map<std::string, IDETheme> m_themes;
    int m_activeThemeId;         // IDM_THEME_xxx of current theme
    int m_themeIdBeforePreview;  // Saved for Cancel in theme picker
    bool m_transparencyEnabled;
    BYTE m_windowAlpha;  // 0..255 (current window alpha)
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
    UINT m_currentDpi = 96;

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
    int m_severityFilterLevel;  // 0=All, 1=Info+, 2=Warn+, 3=Error only

    // Session Persistence
    bool m_sessionRestored;
    std::string m_sessionFilePath;

    // Agent Inline Annotations State
    std::vector<InlineAnnotation> m_annotations;
    bool m_annotationsVisible;
    HWND m_hwndAnnotationOverlay;  // Transparent overlay for inline annotation rendering
    HFONT m_annotationFont;        // Smaller italic font for annotations
    std::map<std::string, std::vector<InlineAnnotation>>
        m_annotationCache;  // Per-file annotation stash for tab switching

    // Clipboard history
    std::vector<std::string> m_clipboardHistory;
    static const size_t MAX_CLIPBOARD_HISTORY = 50;

    // Minimap
    bool m_minimapVisible;
    int m_minimapWidth;
    std::vector<std::string> m_minimapLines;
    std::vector<int> m_minimapLineStarts;
    int m_minimapScrollY = 0;
    int m_minimapTotalLines = 0;
    bool m_minimapDragging = false;
    HFONT m_minimapFont = nullptr;
    RECT m_minimapViewRect = {};

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
    HWND m_hwndSidebarTitle = nullptr;
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
    HWND m_hwndSearchReplace = nullptr;
    HWND m_hwndSearchInclude = nullptr;
    HWND m_hwndSearchExclude = nullptr;
    HWND m_hwndSearchStatus = nullptr;
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
    // m_hwndDebugConsole declared above (line ~2136) with initializer
    bool m_debuggingActive;

    // Disk Recovery View
    HWND m_hwndRecoveryTitle = nullptr;
    HWND m_hwndRecoveryDriveList = nullptr;
    HWND m_hwndRecoveryOutPath = nullptr;
    HWND m_hwndRecoveryStatus = nullptr;
    HWND m_hwndRecoveryProgress = nullptr;
    HWND m_hwndRecoveryLog = nullptr;
    bool m_recoveryTimerActive = false;

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
    int m_selectedStackFrameIndex = 0;

    // Extensions View
    HWND m_hwndExtensionsList;
    HWND m_hwndExtensionSearch;
    HWND m_hwndExtensionDetails;
    struct Extension
    {
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
    struct OutlineItem
    {
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
    struct TimelineEntry
    {
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
    std::vector<std::string> m_userModelDirectories;
    std::vector<std::pair<std::string, std::string>> m_chatHistory;  // role, message
    // Tool action status per chat message (keyed by m_chatHistory index)
    std::map<size_t, std::vector<RawrXD::UI::ToolActionStatus>> m_chatToolActions;
    RawrXD::UI::ToolActionAccumulator m_currentToolActions;  // accumulator for current response
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
    HWND m_hwndProblemsListView = nullptr;  // unified with Problems panel (no duplicate)
    PanelTab m_activePanelTab;
    bool m_panelVisible;
    bool m_panelMaximized;
    int m_panelHeight;

    // Problems tracking
    struct ProblemItem
    {
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
    struct StatusBarInfo
    {
        std::string remoteName;  // e.g., "WSL: Ubuntu" or empty
        std::string branchName;  // e.g., "main"
        int syncAhead;           // commits ahead
        int syncBehind;          // commits behind
        int errors;
        int warnings;
        int line;
        int column;
        int spacesOrTabWidth;
        bool useSpaces;
        std::string encoding;      // e.g., "UTF-8"
        std::string eolSequence;   // e.g., "LF" or "CRLF"
        std::string languageMode;  // e.g., "C++", "Python"
        bool copilotActive;
        int copilotSuggestions;  // number of available suggestions
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
    std::string m_ollamaBaseUrl;        // e.g., http://localhost:11434
    std::string m_ollamaModelOverride;  // if set, use this tag instead of deriving from filename

    // Optional panels (nullptr when feature not used)
    ModelRegistry* m_modelRegistry = nullptr;
    InterpretabilityPanel* m_interpretabilityPanel = nullptr;
    BenchmarkMenu* m_benchmarkMenu = nullptr;
    CheckpointManager* m_checkpointManager = nullptr;
    CICDSettings* m_ciCdSettings = nullptr;
    MultiFileSearchWidget* m_multiFileSearch = nullptr;
    ModelRegistry* getModelRegistry() { return m_modelRegistry; }

    // Project root (for build commands)
    std::string m_projectRoot;

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
    std::vector<std::string> getModelsFromDirectory(const std::string& directory);
    std::string makeHttpRequest(const std::string& url, const std::string& method, const std::string& body, const std::string& contentType);
    void handleModelBrowse();
    std::vector<std::string> getConfiguredModelDirectories() const;
    std::vector<std::string> collectFilesystemModelNames(const std::vector<std::string>& modelDirs,
                                                         size_t maxCount = 2048) const;
    void onModelSelectionChanged();
    void onMaxTokensChanged(int newValue);
    // Debugger Execution Control
    void handleDebuggerToolbarCommand(int commandId);
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
                                           const std::string& suffix, const std::string& filePath, int cursorLine,
                                           int cursorCol, uint64_t expectedSeq = 0);
    void onGhostTextReady(int requestedCursorPos, const char* completionText);
    void dismissGhostText();
    void acceptGhostText();
    void renderGhostText(HDC hdc);
    bool handleGhostTextKey(UINT vk);
    void toggleGhostText();
    std::string trimGhostText(const std::string& raw);

    struct GhostTextCacheEntry
    {
        std::string completion;
        uint64_t createdAtMs = 0;
    };

    struct GhostTextMetrics
    {
        uint64_t requests = 0;
        uint64_t cacheHits = 0;
        uint64_t staleDrops = 0;
        uint64_t localWins = 0;
        uint64_t snippetWins = 0;
        uint64_t lspWins = 0;
        double lastLatencyMs = 0.0;
        double avgLatencyMs = 0.0;
    };

    // Ghost Text state
    RawrXD::GhostTextRenderer* m_ghostTextRendererOverlay = nullptr;
    bool m_ghostTextEnabled = false;
    bool m_ghostTextVisible = false;
    bool m_ghostTextAccepted = false;
    bool m_ghostTextPending = false;
    std::string m_ghostTextContent;
    int m_ghostTextLine = -1;
    int m_ghostTextColumn = -1;
    HFONT m_ghostTextFont = nullptr;
    std::atomic<uint64_t> m_ghostTextRequestSeq{0};
    std::mutex m_ghostTextCacheMutex;
    std::unordered_map<std::string, GhostTextCacheEntry> m_ghostTextCache;
    GhostTextMetrics m_ghostTextMetrics = {};

    // ========================================================================
    // Peek Overlay — Definition/References Overlay (Win32IDE_PeekOverlay.cpp)
    // ========================================================================
    struct LSPLocation;

    void initPeekOverlay();
    void shutdownPeekOverlay();
    void showPeekDefinition(int line, int col);
    void showPeekReferences(int line, int col);
    /** Show peek overlay without re-querying LSP (uses pre-built items). */
    void showPeekOverlayWithItems(const std::vector<PeekItem>& items, int triggerLine, int triggerCol);
    /** Build peek rows from LSP locations by reading source files on disk. */
    std::vector<PeekItem> buildPeekItemsFromLspLocations(const std::vector<LSPLocation>& locations,
                                                         PeekItemType type,
                                                         int contextLinesBefore,
                                                         int contextLinesAfter);
    std::vector<PeekItem> findDefinitionsAt(int line, int col);
    std::vector<PeekItem> findReferencesAt(int line, int col);
    void handlePeekOverlayKey(UINT vk, bool ctrl, bool alt, bool shift);
    bool isPeekOverlayActive() const;
    /// Deferred dismiss / navigate — safe from peek overlay WndProc (posts to main HWND).
    void postPeekFinishFromOverlay(bool navigate, const std::string& filePath, uint32_t line1Based);

    // Peek overlay state
    static constexpr UINT WM_RAWRXD_PEEK_FINISH = WM_USER + 520;
    std::unique_ptr<PeekOverlayWindow, PeekOverlayWindowDeleter> m_peekOverlayWindow;
    bool m_peekOverlayActive = false;
    bool m_peekDeferredNavigate = false;
    std::string m_peekDeferredFile;
    uint32_t m_peekDeferredLine = 1;

    // ========================================================================
    // Agent Panel — Multi-File Edit Session (Win32IDE_AgentPanel.cpp)
    // ========================================================================
    void initAgentPanel();
    void startAgentSession(const std::string& userGoal);
    void agentAcceptHunk(int fileIndex, int hunkIndex);
    void agentRejectHunk(int fileIndex, int hunkIndex);
    void agentAcceptAll();
    void agentRejectAll();
    void refreshAgentDiffDisplay();
    std::string getAgentSessionSummary() const;
    void renderAgentDiffPanel(HDC hdc, RECT panelRect);
    static LRESULT CALLBACK AgentDiffPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    bool m_agentPanelInitialized = false;
    HWND m_hwndAgentStatusLabel = nullptr;
    HWND m_hwndAgentDiffPanel = nullptr;
    HWND m_hwndAgentSummaryEdit = nullptr;
    HWND m_hwndAgentAcceptAllBtn = nullptr;
    HWND m_hwndAgentRejectAllBtn = nullptr;
    HWND m_hwndAgentNewSessionBtn = nullptr;
    void onBoundedAgentLoop();  // Ctrl+Shift+I → Bounded Agent (FIM tools)
    void toggleAgentPanel();    // View > Agent Panel — show/hide agent chat panel

    // Resolved Ollama model (used by GhostText, AI caller)
    std::string getResolvedOllamaModel() const;

    // Agentic mode (Plan/Agent/Ask) — agentic_mode_switcher.hpp
    void setAgenticMode(RawrXD::AgenticMode mode);
    void onAgenticModeChanged(RawrXD::AgenticMode mode);
    void onAgenticModeAsk();
    void onAgenticModePlan();
    void onAgenticModeAgent();
    RawrXD::AgenticMode m_agenticMode = RawrXD::AgenticMode::Ask;
    HWND m_hwndAgenticModeAsk = nullptr;
    HWND m_hwndAgenticModePlan = nullptr;
    HWND m_hwndAgenticModeAgent = nullptr;

    // ========================================================================
    // Security Scans (Win32IDE_SecurityScans.cpp) — Top-50 P0
    // (declarations in earlier section ~line 1264)
    // ========================================================================

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
    /// Refresh Agentic::ExecutionPlan approval rows from m_currentPlan (after dialog approve modes).
    void syncActiveAgenticPlanApprovalsFromUi();
    void rejectPendingStepsInActiveAgenticPlan();

    /// E05 — AgenticPlanningOrchestrator tool lane (JSON payloads from ExecutionPlan::actions).
    std::string executeAgentPlanStepViaBridge(const PlanStep& step);
    void wireAgenticOrchestratorIntegration();

    // Plan state
    AgentPlan m_currentPlan;
    /// Correlates Win32IDE AgentPlan rows with Agentic::ExecutionPlan in OrchestratorIntegration.
    std::string m_activeAgenticPlanId;
    std::vector<AgentPlan> m_planHistory;
    std::atomic<bool> m_planExecutionCancelled{false};
    std::atomic<bool> m_planExecutionPaused{false};
    static const size_t MAX_PLAN_HISTORY = 50;

    // ========================================================================
    // AGENT ENHANCEMENTS — 7 autonomous-agent capabilities
    // (Win32IDE_AgentEnhancements.cpp + Win32IDE_AgentEnhancements.h)
    // ========================================================================
#include "Win32IDE_AgentEnhancements.h"

    // Enhancement 1: Context-window budget tracking
    void initContextBudget(int contextWindowTokens);
    std::string applyContextBudget(const std::string& prompt, const std::string& history);
    std::string getContextBudgetStatus() const;
    ContextBudgetState m_contextBudget;

    // Enhancement 2: Tool-call schema validation
    ToolValidationResult validateToolCall(const std::string& toolName, const std::string& argsJson);
    bool validateAndDispatchToolCall(const std::string& toolName, const std::string& argsJson, std::string& toolResult);

    // Enhancement 3: Plan DAG + parallel batch execution
    std::vector<std::vector<int>> buildPlanExecutionBatches();
    void executePlanBatch(const std::vector<int>& batch, bool dryRun = false);

    // Enhancement 4: Agent scratchpad
    void scratchpadWrite(const std::string& key, const std::string& value, const std::string& stepContext = "");
    std::string scratchpadRead(const std::string& key) const;
    bool scratchpadHas(const std::string& key) const;
    void scratchpadClear();
    std::string scratchpadToJSON() const;
    void persistScratchpad();
    std::map<std::string, ScratchpadEntry> m_scratchpad;
    mutable std::mutex m_scratchpadMutex;

    // Enhancement 5: Streaming plan step output
    void onPlanStreamToken(int stepIndex, const char* token);
    void resetPlanStreamCounters();
    int m_planStreamTokenCount = 0;
    std::chrono::steady_clock::time_point m_planStreamStart;

    // Enhancement 6: Per-task token budget enforcement
    void initPlanTokenBudget(int totalTokens);
    bool checkStepTokenBudget(int stepIndex, const std::string& output);
    std::string getPlanTokenBudgetStatus() const;
    PlanTokenBudgetState m_planTokenBudget;

    // Enhancement 7: Multi-model routing with fallback
    AgentModelRoute routeStepToModel(const PlanStep& step);
    std::string getModelRouterStatus() const;

    // Unified init for all 7 enhancements
    void initAllAgentEnhancements(int contextWindow = 4096, int tokenBudget = 8192);

    // Plan Approval Dialog HWNDs
    HWND m_hwndPlanDialog = nullptr;
    HWND m_hwndPlanList = nullptr;    // ListView
    HWND m_hwndPlanDetail = nullptr;  // RichEdit / Static detail panel
    HWND m_hwndPlanGoalLabel = nullptr;
    HWND m_hwndPlanSummaryLabel = nullptr;
    HWND m_hwndPlanBtnApprove = nullptr;
    HWND m_hwndPlanBtnApproveSafe = nullptr;
    HWND m_hwndPlanBtnEdit = nullptr;
    HWND m_hwndPlanBtnReject = nullptr;
    HWND m_hwndPlanBtnPause = nullptr;
    HWND m_hwndPlanBtnCancel = nullptr;
    HWND m_hwndPlanProgress = nullptr;
    HWND m_hwndPlanProgressLabel = nullptr;
    HBRUSH m_planDialogBrush = nullptr;

    // ========================================================================
    // Failure Detection & Self-Correction (Win32IDE_FailureDetector.cpp)
    // ========================================================================
    void initFailureDetector();
    std::vector<AgentFailureType> detectFailures(const std::string& response, const std::string& originalPrompt);
    bool detectRefusal(const std::string& response);
    bool detectHallucination(const std::string& response, const std::string& prompt);
    bool detectFormatViolation(const std::string& response, const std::string& prompt);
    bool detectInfiniteLoop(const std::string& response);
    bool detectQualityDegradation(const std::string& response);
    std::string applyCorrectionStrategy(AgentFailureType failure, const std::string& originalPrompt, int retryAttempt);
    AgentResponse executeWithFailureDetection(const std::string& prompt);
    std::string failureTypeString(AgentFailureType type) const;
    std::string getFailureDetectorStats() const;
    void toggleFailureDetector();

    // Phase 4B: Failure Classification (returns structured FailureClassification)
    FailureClassification classifyFailure(const std::string& response, const std::string& originalPrompt);
    std::vector<FailureClassification> classifyAllFailures(const std::string& response,
                                                           const std::string& originalPrompt);

    // Phase 4B: Detection hooks — wired at exact choke points only
    //   1. Post-generation validation
    //   2. Tool invocation result parsing
    //   3. Plan step output verification
    //   4. Agent command post-processing
    FailureClassification hookPostGeneration(const std::string& response, const std::string& prompt);
    FailureClassification hookToolResult(const std::string& toolName, const std::string& toolOutput);
    FailureClassification hookPlanStepOutput(int stepIndex, const std::string& output);
    FailureClassification hookAgentCommand(const std::string& response, const std::string& prompt);
    FailureClassification hookSwarmMerge(const std::string& mergedResult, int taskCount, const std::string& strategy);

    // Phase 4B: Bounded retry (max 1 retry, approval required)
    std::string buildRetryPrompt(const FailureClassification& failure, const std::string& originalPrompt);
    bool showRetryApprovalInPlanDialog(int stepIndex, const FailureClassification& failure);
    std::string getRetryStrategyDescription(AgentFailureType reason) const;

    // Phase 4B: Failure-aware plan execution (replaces blind retry)
    AgentResponse executeWithBoundedRetry(const std::string& prompt);

    // Failure detector state
    bool m_failureDetectorEnabled = false;
    int m_failureMaxRetries = 1;  // Phase 4B: hard cap at 1
    FailureStats m_failureStats;

    // ========================================================================
    // Failure Intelligence — Phase 6 (Win32IDE_FailureIntelligence.cpp)
    // ========================================================================
    // Phase 6.1 — Granular Classification
    FailureReason classifyFailureReason(AgentFailureType type, const std::string& response, const std::string& prompt);
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
    std::string applyRetryStrategy(const RetryStrategy& strategy, const std::string& originalPrompt);
    AgentResponse executeWithFailureIntelligence(const std::string& prompt);
    void recordFailureIntelligence(const FailureIntelligenceRecord& record);
    void loadFailureIntelligenceHistory();
    void flushFailureIntelligence();

    // Phase 6.3 — History-Aware Suggestion UI
    bool hasMatchingPreviousFailure(const std::string& prompt) const;
    std::vector<FailureIntelligenceRecord> getMatchingFailures(const std::string& prompt) const;
    void showFailureSuggestionDialog(const std::string& prompt, const std::vector<FailureIntelligenceRecord>& matches);
    void showFailureIntelligencePanel();
    void showFailureIntelligenceStats();
    std::string getFailureIntelligenceStatsString() const;
    void toggleFailureIntelligence();

    // Failure Intelligence state
    bool m_failureIntelligenceEnabled = true;
    int m_intelligenceMaxRetries = 2;
    std::vector<FailureIntelligenceRecord> m_failureIntelligenceHistory;
    std::map<int, FailureReasonStats> m_failureReasonStats;  // keyed by (int)FailureReason
    std::mutex m_failureIntelligenceMutex;
    static const size_t MAX_FAILURE_INTELLIGENCE_RECORDS = 500;

    // Failure Intelligence UI
    HWND m_hwndFailureIntelPanel = nullptr;
    HWND m_hwndFailureIntelList = nullptr;
    HWND m_hwndFailureIntelDetail = nullptr;
    HWND m_hwndFailureIntelStats = nullptr;
    HWND m_hwndFailureSuggestionDlg = nullptr;

    // ========================================================================
    // AI Backend Switcher — Phase 8B (Win32IDE_BackendSwitcher.cpp)
    // ========================================================================
    enum class AIBackendType
    {
        LocalGGUF = 0,        // Native CPU inference via RawrXD engine
        Ollama = 1,           // Ollama HTTP server (local or remote)
        OpenAI = 2,           // OpenAI API (gpt-4o, etc.)
        Claude = 3,           // Anthropic Claude API
        Gemini = 4,           // Google Gemini API
        ReasoningEngine = 5,  // RawrXD local reasoning engine
        GitHubCopilot = 6,    // GitHub Copilot extension
        AmazonQ = 7,          // Amazon Q extension
        Count = 8
    };

    struct AIBackendConfig
    {
        AIBackendType type = AIBackendType::LocalGGUF;
        std::string name;      // Human-readable label ("Local GGUF", "Ollama", etc.)
        std::string endpoint;  // Base URL (e.g., "http://localhost:11434", "https://api.openai.com")
        std::string model;     // Model identifier (e.g., "llama3.2", "gpt-4o", "claude-sonnet-4-20250514")
        std::string apiKey;    // API key for remote backends (empty for local)
        bool enabled = true;
        int timeoutMs = 30000;     // Request timeout
        int maxTokens = 2048;      // Default max tokens for this backend
        float temperature = 0.7f;  // Default temperature for this backend
    };

    struct AIBackendStatus
    {
        AIBackendType type = AIBackendType::LocalGGUF;
        bool connected = false;
        bool healthy = false;
        int latencyMs = -1;            // Last measured round-trip latency (-1 = unknown)
        uint64_t requestCount = 0;     // Total requests sent to this backend
        uint64_t failureCount = 0;     // Total failed requests
        std::string lastError;         // Last error message (empty if ok)
        std::string lastModel;         // Last model actually used
        uint64_t lastUsedEpochMs = 0;  // Epoch ms of last use
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
    std::string getBackendStatusString() const;  // Human-readable summary of all backends

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
    void routeInferenceRequestAsync(const std::string& prompt, std::function<void(const std::string&, bool)> callback);
    std::string routeToLocalGGUF(const std::string& prompt);
    std::string routeToOllama(const std::string& prompt);
    std::string routeToOpenAI(const std::string& prompt);
    std::string routeToClaude(const std::string& prompt);
    std::string routeToGemini(const std::string& prompt);
    std::string routeToReasoningEngine(const std::string& prompt);
    std::string routeToGitHubCopilot(const std::string& prompt);
    std::string routeToAmazonQ(const std::string& prompt);

    // HTTP helpers for remote backends
    std::string httpPost(const std::string& url, const std::string& body, const std::vector<std::string>& headers,
                         int timeoutMs);

    // Backend UI helpers
    void showBackendSwitcherDialog();
    void showBackendConfigDialog(AIBackendType type);
    void updateStatusBarBackend();
    void onAIBackendVerified(bool success);
    void updateToolsMenu();
    void initializeAIBackend();
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
    AIBackendType m_activeBackend = AIBackendType::LocalGGUF;
    std::array<AIBackendConfig, (size_t)AIBackendType::Count> m_backendConfigs;
    std::array<AIBackendStatus, (size_t)AIBackendType::Count> m_backendStatuses;
    bool m_backendManagerInitialized = false;
    std::mutex m_backendMutex;

    // ========================================================================
    // LLM Router — Phase 8C (Win32IDE_LLMRouter.cpp)
    // ========================================================================
    // Task-based routing: classifies prompts by intent, selects the optimal
    // backend based on capabilities, failure history, and policy constraints.
    // Fallback is explicit, auditable, never silent.

    // ---- Task classification ----
    enum class LLMTaskType
    {
        Chat = 0,            // General conversation / Q&A
        CodeGeneration = 1,  // Write new code from description
        CodeReview = 2,      // Analyze / review existing code
        CodeEdit = 3,        // Edit / refactor existing code
        Planning = 4,        // Multi-step plan generation
        ToolExecution = 5,   // Tool-call / function-calling tasks
        Research = 6,        // Summarization / information retrieval
        General = 7,         // Unclassified — default bucket
        Count = 8
    };

    // ---- Backend capability profile ----
    struct BackendCapability
    {
        AIBackendType backend = AIBackendType::LocalGGUF;
        int maxContextTokens = 4096;
        bool supportsToolCalls = false;
        bool supportsStreaming = true;
        bool supportsFunctionCalling = false;
        bool supportsJsonMode = false;
        int costTier = 0;           // 0 = free/local, 1 = cheap, 2 = moderate, 3 = expensive
        float qualityScore = 0.5f;  // 0.0–1.0 subjective quality rating
        std::string notes;          // Human-readable notes
    };

    // ---- Routing decision (the core output of the router) ----
    struct RoutingDecision
    {
        AIBackendType selectedBackend = AIBackendType::LocalGGUF;
        AIBackendType fallbackBackend = AIBackendType::Count;  // Count = no fallback
        LLMTaskType classifiedTask = LLMTaskType::General;
        float confidence = 0.0f;       // 0.0–1.0 classification confidence
        std::string reason;            // Human-readable routing rationale
        bool policyOverride = false;   // true if policy engine redirected
        bool fallbackUsed = false;     // true if primary failed and fallback served
        uint64_t decisionEpochMs = 0;  // When the decision was made
        int primaryLatencyMs = -1;     // Latency of primary attempt (-1 = not yet)
        int fallbackLatencyMs = -1;    // Latency of fallback attempt (-1 = not used)
    };

    // ---- Per-task routing preference (user-configurable) ----
    struct TaskRoutingPreference
    {
        LLMTaskType taskType = LLMTaskType::General;
        AIBackendType preferredBackend = AIBackendType::LocalGGUF;
        AIBackendType fallbackBackend = AIBackendType::Count;
        bool allowFallback = true;
        int maxFailuresBeforeSkip = 5;  // Skip a backend after N consecutive failures
    };

    // ---- Router statistics ----
    struct RouterStats
    {
        uint64_t totalRouted = 0;
        uint64_t totalFallbacksUsed = 0;
        uint64_t totalPolicyOverrides = 0;
        uint64_t taskTypeCounts[(size_t)LLMTaskType::Count] = {};
        uint64_t backendSelections[(size_t)AIBackendType::Count] = {};
        uint64_t backendFallbacks[(size_t)AIBackendType::Count] = {};
    };

    // ---- Cost / latency record (one per routing event) ----
    struct CostLatencyRecord
    {
        LLMTaskType task = LLMTaskType::General;
        AIBackendType backend = AIBackendType::LocalGGUF;
        int latencyMs = 0;
        int costTier = 0;           // Snapshot of backend's costTier at routing time
        float qualityScore = 0.0f;  // Snapshot of backend's qualityScore
        uint64_t epochMs = 0;
        bool fallbackUsed = false;
    };

    // ---- Aggregated cost/latency heatmap cell ----
    struct HeatmapCell
    {
        uint64_t requestCount = 0;
        double totalLatencyMs = 0.0;
        double avgLatencyMs = 0.0;
        int minLatencyMs = INT_MAX;
        int maxLatencyMs = 0;
        double avgCostTier = 0.0;
        double avgQuality = 0.0;
    };

    // ---- Per-task backend pin (user override) ----
    struct TaskBackendPin
    {
        LLMTaskType task = LLMTaskType::General;
        AIBackendType pinnedBackend = AIBackendType::Count;  // Count = not pinned
        bool active = false;
        std::string reason;  // "User pinned via palette"
    };

    // ---- Ensemble vote (one backend's response in an ensemble query) ----
    struct EnsembleVote
    {
        AIBackendType backend = AIBackendType::LocalGGUF;
        std::string response;
        int latencyMs = -1;
        float confidenceWeight = 0.0f;  // Derived from qualityScore * viability
        bool succeeded = false;
    };

    // ---- Ensemble routing decision ----
    struct EnsembleDecision
    {
        LLMTaskType classifiedTask = LLMTaskType::General;
        std::vector<EnsembleVote> votes;
        std::string mergedResponse;  // The winning / merged output
        AIBackendType winnerBackend = AIBackendType::LocalGGUF;
        float winnerConfidence = 0.0f;
        int totalLatencyMs = 0;  // Wall-clock time for the whole ensemble
        std::string strategy;    // "confidence-weighted", "fastest", "majority"
        uint64_t decisionEpochMs = 0;
    };

    // ---- Offline routing simulation input ----
    struct SimulationInput
    {
        std::string prompt;
        LLMTaskType expectedTask = LLMTaskType::General;  // Ground truth (if known)
    };

    // ---- Offline routing simulation result ----
    struct SimulationResult
    {
        std::vector<SimulationInput> inputs;
        struct PerInput
        {
            std::string prompt;
            LLMTaskType classifiedTask = LLMTaskType::General;
            AIBackendType selectedBackend = AIBackendType::LocalGGUF;
            float confidence = 0.0f;
            std::string reason;
            bool matchedExpected = false;  // Only meaningful when expectedTask is set
        };
        std::vector<PerInput> results;
        int totalInputs = 0;
        int correctClassifications = 0;  // Count of matchedExpected == true
        float classificationAccuracy = 0.0f;
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
    std::string handleRoutingFallback(AIBackendType primary, AIBackendType fallback, const std::string& prompt,
                                      RoutingDecision& decision);

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
    bool m_routerEnabled = false;
    bool m_routerInitialized = false;
    std::array<BackendCapability, (size_t)AIBackendType::Count> m_backendCapabilities;
    std::array<TaskRoutingPreference, (size_t)LLMTaskType::Count> m_taskPreferences;
    std::array<int, (size_t)AIBackendType::Count> m_consecutiveFailures = {};
    RouterStats m_routerStats = {};
    RoutingDecision m_lastRoutingDecision = {};
    std::mutex m_routerMutex;

    // UX Enhancement state — per-task pinning
    std::array<TaskBackendPin, (size_t)LLMTaskType::Count> m_taskPins = {};

    // UX Enhancement state — cost / latency heatmap
    // Indexed as [taskType][backendType]
    std::array<std::array<HeatmapCell, (size_t)AIBackendType::Count>, (size_t)LLMTaskType::Count> m_heatmap = {};
    std::vector<CostLatencyRecord> m_costLatencyLog;
    static const size_t MAX_COST_LATENCY_LOG = 2000;

    // Research state — ensemble routing
    bool m_ensembleEnabled = false;
    EnsembleDecision m_lastEnsembleDecision = {};

    // Research state — last simulation result
    SimulationResult m_lastSimulationResult = {};

    // ========================================================================
    // LSP Client Bridge — Phase 9A (Win32IDE_LSPClient.cpp)
    // ========================================================================
    // Minimal LSP integration for C/C++ (clangd), Python (pyright), and
    // TypeScript (typescript-language-server). Provides go-to-definition,
    // find-references, rename, hover, and diagnostics. The LSP servers run as
    // child processes communicating via JSON-RPC over stdin/stdout.

    // ---- Supported language servers ----
    enum class LSPLanguage
    {
        Cpp = 0,         // clangd
        Python = 1,      // pyright-langserver (basedpyright) or pylsp
        TypeScript = 2,  // typescript-language-server
        Count = 3
    };

    // ---- Per-server configuration ----
    struct LSPServerConfig
    {
        LSPLanguage language = LSPLanguage::Cpp;
        std::string name;               // "clangd", "pyright", "typescript-language-server"
        std::string executablePath;     // Full path to the LSP binary
        std::vector<std::string> args;  // Command-line arguments
        std::string rootUri;            // Workspace root URI ("file:///D:/rawrxd")
        bool enabled = true;
        int initTimeoutMs = 10000;
    };

    // ---- Per-server runtime state ----
    enum class LSPServerState
    {
        Stopped = 0,
        Starting = 1,
        Running = 2,
        ShuttingDown = 3,
        Error = 4
    };

    struct LSPServerStatus
    {
        LSPLanguage language = LSPLanguage::Cpp;
        LSPServerState state = LSPServerState::Stopped;
        HANDLE hProcess = nullptr;     // Child process handle
        HANDLE hStdinWrite = nullptr;  // Write end of stdin pipe
        HANDLE hStdoutRead = nullptr;  // Read end of stdout pipe
        DWORD pid = 0;
        int requestIdCounter = 1;  // Monotonic JSON-RPC id
        bool initialized = false;  // Server responded to initialize
        std::string lastError;
        uint64_t startedEpochMs = 0;
        uint64_t requestCount = 0;
        uint64_t notificationCount = 0;
    };

    // ---- LSP protocol types (minimal subset) ----
    struct LSPPosition
    {
        int line = 0;       // 0-based
        int character = 0;  // 0-based (UTF-16 offset)
    };

    struct LSPRange
    {
        LSPPosition start;
        LSPPosition end;
    };

    struct LSPLocation
    {
        std::string uri;  // "file:///path/to/file.cpp"
        LSPRange range;
    };

    struct LSPDiagnostic
    {
        LSPRange range;
        int severity = 1;    // 1=Error, 2=Warning, 3=Information, 4=Hint
        std::string code;    // Diagnostic code (e.g., "-Wunused")
        std::string source;  // "clangd", "pyright", etc.
        std::string message;
    };

    struct LSPHoverInfo
    {
        std::string contents;  // Markdown hover text
        LSPRange range;        // Range the hover applies to
        bool valid = false;
    };

    struct LSPCompletionItem
    {
        std::string label;
        std::string detail;
        std::string insertText;
        int kind = 0;
        bool isSnippet = false;
    };

    struct LSPSignatureHelpInfo
    {
        std::vector<std::string> signatures;
        std::string activeSignatureLabel;
        std::string activeDocumentation;
        int activeSignature = 0;
        int activeParameter = 0;
        bool valid = false;
    };

    struct LSPSymbolInfo
    {
        std::string name;
        int kind = 0;        // LSP SymbolKind (1=File ... 26=TypeParameter)
        std::string detail;  // e.g., "void foo(int x)"
        LSPLocation location;
        std::string containerName;  // Enclosing symbol name
    };

    struct SemanticToken
    {
        int line = 0;
        int startChar = 0;
        int length = 0;
        int tokenType = 0;
        int modifiers = 0;
        std::string typeName;
    };

    struct LSPWorkspaceEdit
    {
        // uri → list of {range, newText}
        struct TextEdit
        {
            LSPRange range;
            std::string newText;
        };
        std::map<std::string, std::vector<TextEdit>> changes;
    };

    // ---- LSP client statistics ----
    struct LSPStats
    {
        uint64_t totalDefinitionRequests = 0;
        uint64_t totalReferenceRequests = 0;
        uint64_t totalRenameRequests = 0;
        uint64_t totalHoverRequests = 0;
        uint64_t totalCompletionRequests = 0;
        uint64_t totalSignatureRequests = 0;
        uint64_t totalDiagnosticsReceived = 0;
        uint64_t totalServerRestarts = 0;
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
    int sendLSPRequest(LSPLanguage lang, const std::string& method, const nlohmann::json& params);
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
    LSPWorkspaceEdit lspRenameSymbol(const std::string& uri, int line, int character, const std::string& newName);
    LSPHoverInfo lspHover(const std::string& uri, int line, int character);
    std::vector<LSPCompletionItem> lspCompletion(const std::string& uri, int line, int character);
    LSPSignatureHelpInfo lspSignatureHelp(const std::string& uri, int line, int character, int triggerKind = 1);
    std::vector<SemanticToken> lspSemanticTokensFull(const std::string& uri);
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
    bool m_lspInitialized = false;
    std::array<LSPServerConfig, (size_t)LSPLanguage::Count> m_lspConfigs;
    std::array<LSPServerStatus, (size_t)LSPLanguage::Count> m_lspStatuses;
    std::map<std::string, std::vector<LSPDiagnostic>> m_lspDiagnostics;  // uri → diagnostics
    std::map<int, nlohmann::json> m_lspPendingResponses;                 // requestId → response
    LSPStats m_lspStats = {};
    std::mutex m_lspMutex;
    std::mutex m_lspDiagnosticsMutex;
    std::mutex m_lspResponseMutex;
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
    void handleV1ModelsEndpoint(SOCKET client);
    void handleReSetBinaryEndpoint(SOCKET client, const std::string& body);
    static std::vector<std::string> getCandidateModelRootPaths();
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
    void handleHotpatchTargetTpsEndpoint(SOCKET client, const std::string& method, const std::string& body);

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
    struct DetachedThreadGuard
    {
        std::atomic<int>& counter;
        bool cancelled;
        DetachedThreadGuard(std::atomic<int>& c, const std::atomic<bool>& shutting)
            : counter(c), cancelled(shutting.load(std::memory_order_acquire))
        {
            counter.fetch_add(1, std::memory_order_acq_rel);
        }
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
    void recordEvent(AgentEventType type, const std::string& agentId, const std::string& prompt,
                     const std::string& result, int durationMs, bool success, const std::string& parentId = "",
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
    bool m_agentHistoryEnabled = true;
    std::string m_currentSessionId;
    std::vector<AgentEvent> m_eventBuffer;  // In-memory ring buffer for current session
    std::mutex m_eventBufferMutex;
    AgentHistoryStats m_historyStats;
    static const size_t MAX_EVENT_BUFFER = 1000;
    static const size_t MAX_LOG_FIELD_LEN = 512;

    // Agent History UI
    HWND m_hwndHistoryPanel = nullptr;
    HWND m_hwndHistoryList = nullptr;
    HWND m_hwndHistoryDetail = nullptr;
    HWND m_hwndHistoryFilter = nullptr;
    HWND m_hwndHistoryStats = nullptr;
    HWND m_hwndHistoryBtnReplay = nullptr;
    HWND m_hwndHistoryBtnExport = nullptr;
    HWND m_hwndHistoryBtnClear = nullptr;
    HWND m_hwndHistoryBtnRefresh = nullptr;

    // ========================================================================
    // ASM Semantic Support (Win32IDE_AsmSemantic.cpp)
    // ========================================================================

    // ---- IDM defines for ASM commands (5082–5093) ----
#define IDM_ASM_PARSE_SYMBOLS 5082
#define IDM_ASM_GOTO_LABEL 5083
#define IDM_ASM_FIND_LABEL_REFS 5084
#define IDM_ASM_SHOW_SYMBOL_TABLE 5085
#define IDM_ASM_INSTRUCTION_INFO 5086
#define IDM_ASM_REGISTER_INFO 5087
#define IDM_ASM_ANALYZE_BLOCK 5088
#define IDM_ASM_SHOW_CALL_GRAPH 5089
#define IDM_ASM_SHOW_DATA_FLOW 5090
#define IDM_ASM_DETECT_CONVENTION 5091
#define IDM_ASM_SHOW_SECTIONS 5092
#define IDM_ASM_CLEAR_SYMBOLS 5093

    // ---- Symbol kinds for ASM analysis ----
    enum class AsmSymbolKind
    {
        Label = 0,
        Procedure = 1,
        Macro = 2,
        Equate = 3,
        DataDef = 4,
        Section = 5,
        Struct = 6,
        Extern = 7,
        Global = 8,
        Local = 9,
        Count = 10
    };

    // ---- Symbol reference (where a symbol is used) ----
    struct AsmSymbolRef
    {
        std::string filePath;
        int line = 0;
        int column = 0;
        bool isDefinition = false;
    };

    // ---- A single ASM symbol entry ----
    struct AsmSymbol
    {
        std::string name;
        AsmSymbolKind kind = AsmSymbolKind::Label;
        std::string filePath;
        int line = 0;
        int column = 0;
        int endLine = 0;
        std::string detail;
        std::string section;
        std::vector<AsmSymbolRef> references;
    };

    // ---- Instruction information ----
    struct AsmInstructionInfo
    {
        std::string mnemonic;
        std::string category;
        std::string description;
        std::string operands;
        bool affectsFlags = false;
    };

    // ---- Register information ----
    struct AsmRegisterInfo
    {
        std::string name;
        std::string category;
        int bits = 64;
        std::string description;
        std::string aliases;
    };

    // ---- Section descriptor ----
    struct AsmSectionInfo
    {
        std::string name;
        std::string filePath;
        int startLine = 0;
        int endLine = 0;
        int symbolCount = 0;
    };

    // ---- Call graph edge ----
    struct AsmCallEdge
    {
        std::string caller;
        std::string callee;
        std::string filePath;
        int line = 0;
    };

    // ---- Data flow reference ----
    struct AsmDataFlowRef
    {
        std::string symbol;
        int line = 0;
        bool isRead = false;
        bool isWrite = false;
        std::string instruction;
    };

    // ---- AI analysis result for an ASM block ----
    struct AsmBlockAnalysis
    {
        std::string summary;
        std::string callingConvention;
        std::vector<std::string> registersUsed;
        std::vector<std::string> registersModified;
        std::vector<std::string> memoryAccesses;
        std::vector<std::string> observations;
        bool isLeafFunction = false;
        bool usesStackFrame = false;
        int estimatedStackUsage = 0;
    };

    // ---- Aggregate statistics ----
    struct AsmSemanticStats
    {
        uint64_t totalSymbols = 0;
        uint64_t totalLabels = 0;
        uint64_t totalProcedures = 0;
        uint64_t totalMacros = 0;
        uint64_t totalEquates = 0;
        uint64_t totalDataDefs = 0;
        uint64_t totalSections = 0;
        uint64_t totalExterns = 0;
        uint64_t totalFiles = 0;
        uint64_t totalParseTimeMs = 0;
        uint64_t gotoDefRequests = 0;
        uint64_t findRefsRequests = 0;
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
    std::vector<AsmDataFlowRef> analyzeDataFlow(const std::string& symbolName, const std::string& filePath) const;
    std::string getDataFlowString(const std::string& symbolName, const std::string& filePath) const;

    // AI-assisted block analysis
    AsmBlockAnalysis analyzeAsmBlock(const std::string& filePath, int startLine, int endLine) const;
    AsmBlockAnalysis analyzeCurrentProcedure() const;
    std::string detectCallingConvention(const std::string& filePath, int startLine, int endLine) const;
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
    bool m_asmSemanticInitialized = false;
    std::map<std::string, AsmSymbol> m_asmSymbolTable;
    std::map<std::string, std::vector<std::string>> m_asmFileSymbols;
    mutable AsmSemanticStats m_asmStats = {};
    mutable std::mutex m_asmMutex;

    // ========================================================================
    // Phase 9B: LSP-AI Hybrid Integration Bridge
    // (Win32IDE_LSP_AI_Bridge.cpp)
    // ========================================================================

    // ---- IDM defines for Hybrid commands (5094–5109) ----
#define IDM_HYBRID_COMPLETE 5094
#define IDM_HYBRID_DIAGNOSTICS 5095
#define IDM_HYBRID_SMART_RENAME 5096
#define IDM_HYBRID_ANALYZE_FILE 5097
#define IDM_HYBRID_AUTO_PROFILE 5098
#define IDM_HYBRID_STATUS 5099
#define IDM_HYBRID_SYMBOL_USAGE 5100
#define IDM_HYBRID_EXPLAIN_SYMBOL 5101
#define IDM_HYBRID_ANNOTATE_DIAG 5102
#define IDM_HYBRID_STREAM_ANALYZE 5103
#define IDM_HYBRID_SEMANTIC_PREFETCH 5104
#define IDM_HYBRID_CORRECTION_LOOP 5105

    // ---- Hybrid completion result ----
    struct HybridCompletionItem
    {
        std::string label;
        std::string detail;
        std::string insertText;
        std::string source;  // "lsp", "ai", "asm", "merged"
        float confidence = 0.0f;
        int sortOrder = 0;
    };

    // ---- Aggregate diagnostic with AI explanation ----
    struct HybridDiagnostic
    {
        std::string filePath;
        int line = 0;
        int character = 0;
        int severity = 0;  // 1=Error, 2=Warning, 3=Info, 4=Hint
        std::string message;
        std::string source;         // "lsp", "ai", "asm"
        std::string aiExplanation;  // AI-generated fix suggestion
        std::string suggestedFix;
    };

    // ---- Symbol usage analysis ----
    struct HybridSymbolUsage
    {
        std::string symbol;
        std::string kind;  // "function", "variable", "procedure", etc.
        int definitionLine = 0;
        std::string definitionFile;
        int referenceCount = 0;
        std::vector<std::pair<std::string, int>> references;  // (file, line) pairs
        std::string aiSummary;                                // AI-generated description of the symbol
    };

    // ---- Streaming analysis result ----
    struct HybridStreamAnalysis
    {
        std::string filePath;
        int totalLines = 0;
        int symbolCount = 0;
        int diagnosticCount = 0;
        int complexityScore = 0;
        std::string summary;
        std::vector<HybridDiagnostic> diagnostics;
        std::vector<std::string> observations;
        double analysisTimeMs = 0.0;
    };

    // ---- LSP profile recommendation ----
    struct LSPProfileRecommendation
    {
        LSPLanguage language;
        std::string serverName;
        std::string reason;
        bool isInstalled = false;
    };

    // ---- Bridge statistics ----
    struct HybridBridgeStats
    {
        uint64_t hybridCompletions = 0;
        uint64_t aggregateDiagRuns = 0;
        uint64_t smartRenames = 0;
        uint64_t streamAnalyses = 0;
        uint64_t autoProfileSelects = 0;
        uint64_t semanticPrefetches = 0;
        uint64_t correctionLoops = 0;
        uint64_t symbolExplains = 0;
        double totalBridgeTimeMs = 0.0;
    };

    // Phase 9B — lifecycle
    void initLSPAIBridge();
    void shutdownLSPAIBridge();

    // Hybrid completion (LSP + AI + ASM merged)
    std::vector<HybridCompletionItem> requestHybridCompletion(const std::string& filePath, int line, int character);

    // Aggregate diagnostics (LSP + AI analysis + ASM semantic)
    std::vector<HybridDiagnostic> aggregateDiagnostics(const std::string& filePath);

    // Smart rename (LSP rename + AI verification + ASM cross-ref)
    bool hybridSmartRename(const std::string& filePath, int line, int character, const std::string& newName);

    // Streaming large file analysis (uses StreamingEngineRegistry)
    HybridStreamAnalysis streamLargeFileAnalysis(const std::string& filePath);

    // Auto LSP profile selection based on project analysis
    std::vector<LSPProfileRecommendation> autoSelectLSPProfile();

    // Symbol usage analysis (LSP references + ASM refs + AI summary)
    HybridSymbolUsage analyzeSymbolUsage(const std::string& symbol, const std::string& filePath);

    // AI-powered symbol explanation
    std::string explainSymbol(const std::string& symbol, const std::string& filePath);

    // Semantic prefetching (pre-warm LSP + ASM caches for open files)
    void semanticPrefetch(const std::string& filePath);

    // Agent-LSP correction loop (detect bad output → re-query with LSP context)
    std::string agentCorrectionLoop(const std::string& prompt, const std::string& badOutput,
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
#define IDM_MULTI_RESP_GENERATE 5106
#define IDM_MULTI_RESP_SET_MAX 5107
#define IDM_MULTI_RESP_SELECT_PREFERRED 5108
#define IDM_MULTI_RESP_COMPARE 5109
#define IDM_MULTI_RESP_SHOW_STATS 5110
#define IDM_MULTI_RESP_SHOW_TEMPLATES 5111
#define IDM_MULTI_RESP_TOGGLE_TEMPLATE 5112
#define IDM_MULTI_RESP_SHOW_PREFS 5113
#define IDM_MULTI_RESP_SHOW_LATEST 5114
#define IDM_MULTI_RESP_SHOW_STATUS 5115
#define IDM_MULTI_RESP_CLEAR_HISTORY 5116
#define IDM_MULTI_RESP_APPLY_PREFERRED 5117

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
#define IDM_GOV_STATUS 5118
#define IDM_GOV_SUBMIT_COMMAND 5119
#define IDM_GOV_KILL_ALL 5120
#define IDM_GOV_TASK_LIST 5121
#define IDM_SAFETY_STATUS 5122
#define IDM_SAFETY_RESET_BUDGET 5123
#define IDM_SAFETY_ROLLBACK_LAST 5124
#define IDM_SAFETY_SHOW_VIOLATIONS 5125
#define IDM_REPLAY_STATUS 5126
#define IDM_REPLAY_SHOW_LAST 5127
#define IDM_REPLAY_EXPORT_SESSION 5128
#define IDM_REPLAY_CHECKPOINT 5129
#define IDM_CONFIDENCE_STATUS 5130
#define IDM_CONFIDENCE_SET_POLICY 5131

    // ---- IDM defines for Phase 11 commands (5132–5155) ----
    // Swarm Control
#define IDM_SWARM_STATUS 5132
#define IDM_SWARM_START_LEADER 5133
#define IDM_SWARM_START_WORKER 5134
#define IDM_SWARM_START_HYBRID 5135
#define IDM_SWARM_STOP 5136
    // Node Management
#define IDM_SWARM_LIST_NODES 5137
#define IDM_SWARM_ADD_NODE 5138
#define IDM_SWARM_REMOVE_NODE 5139
#define IDM_SWARM_BLACKLIST_NODE 5140
    // Build
#define IDM_SWARM_BUILD_SOURCES 5141
#define IDM_SWARM_BUILD_CMAKE 5142
#define IDM_SWARM_START_BUILD 5143
#define IDM_SWARM_CANCEL_BUILD 5144
    // Cache & Config
#define IDM_SWARM_CACHE_STATUS 5145
#define IDM_SWARM_CACHE_CLEAR 5146
#define IDM_SWARM_SHOW_CONFIG 5147
#define IDM_SWARM_TOGGLE_DISCOVERY 5148
    // Task Graph & Events
#define IDM_SWARM_SHOW_TASK_GRAPH 5149
#define IDM_SWARM_SHOW_EVENTS 5150
#define IDM_SWARM_SHOW_STATS 5151
#define IDM_SWARM_RESET_STATS 5152
    // Worker Control
#define IDM_SWARM_WORKER_STATUS 5153
#define IDM_SWARM_WORKER_CONNECT 5154
#define IDM_SWARM_WORKER_DISCONNECT 5155
#define IDM_SWARM_FITNESS_TEST 5156

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

    // Layer eviction lifecycle + diagnostics (P0 #13)
    void initLayerEviction();
    void shutdownLayerEviction();
    bool registerModelLayer(const std::string& layerId, size_t memoryUsage);
    void accessModelLayer(const std::string& layerId);
    void showLayerEvictionStats();

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

    // Model bridge (GUI / ide_chatbot_engine.js) — profiles, load/unload, caps
    void handleModelBridgeProfilesEndpoint(SOCKET client);
    void handleModelBridgeLoadEndpoint(SOCKET client, const std::string& body);
    void handleModelBridgeUnloadEndpoint(SOCKET client);
    void handleEngineCapabilitiesEndpoint(SOCKET client);

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
#define IDM_DBG_LAUNCH 5157
#define IDM_DBG_ATTACH 5158
#define IDM_DBG_DETACH 5159
    // Execution Control (12B)
#define IDM_DBG_GO 5160
#define IDM_DBG_STEP_OVER 5161
#define IDM_DBG_STEP_INTO 5162
#define IDM_DBG_STEP_OUT 5163
#define IDM_DBG_BREAK 5164
#define IDM_DBG_KILL 5165
    // Breakpoint Management (12C)
#define IDM_DBG_ADD_BP 5166
#define IDM_DBG_REMOVE_BP 5167
#define IDM_DBG_ENABLE_BP 5168
#define IDM_DBG_CLEAR_BPS 5169
#define IDM_DBG_LIST_BPS 5170
    // Watch Management (12D)
#define IDM_DBG_ADD_WATCH 5171
#define IDM_DBG_REMOVE_WATCH 5172
    // Inspection (12E)
#define IDM_DBG_REGISTERS 5173
#define IDM_DBG_STACK 5174
#define IDM_DBG_MEMORY 5175
#define IDM_DBG_DISASM 5176
#define IDM_DBG_MODULES 5177
#define IDM_DBG_THREADS 5178
#define IDM_DBG_SWITCH_THREAD 5179
#define IDM_DBG_EVALUATE 5180
#define IDM_DBG_SET_REGISTER 5181
#define IDM_DBG_SEARCH_MEMORY 5182
#define IDM_DBG_SYMBOL_PATH 5183
#define IDM_DBG_STATUS 5184

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

#define IDM_HOTPATCH_SHOW_STATUS 9001
#define IDM_HOTPATCH_MEMORY_APPLY 9002
#define IDM_HOTPATCH_MEMORY_REVERT 9003
#define IDM_HOTPATCH_BYTE_APPLY 9004
#define IDM_HOTPATCH_BYTE_SEARCH 9005
#define IDM_HOTPATCH_SERVER_ADD 9006
#define IDM_HOTPATCH_SERVER_REMOVE 9007
#define IDM_HOTPATCH_PROXY_BIAS 9008
#define IDM_HOTPATCH_PROXY_REWRITE 9009
#define IDM_HOTPATCH_PROXY_TERMINATE 9010
#define IDM_HOTPATCH_PROXY_VALIDATE 9011
#define IDM_HOTPATCH_PRESET_SAVE 9012
#define IDM_HOTPATCH_PRESET_LOAD 9013
#define IDM_HOTPATCH_SHOW_EVENT_LOG 9014
#define IDM_HOTPATCH_RESET_STATS 9015
#define IDM_HOTPATCH_TOGGLE_ALL 9016
#define IDM_HOTPATCH_SHOW_PROXY_STATS 9017
#define IDM_HOTPATCH_SET_TARGET_TPS 9018

// ========================================================================
// WEBVIEW2 + MONACO EDITOR COMMANDS — Phase 26 (9100 range)
// ========================================================================
#define IDM_VIEW_TOGGLE_MONACO 9100
#define IDM_VIEW_MONACO_DEVTOOLS 9101
#define IDM_VIEW_MONACO_RELOAD 9102
#define IDM_VIEW_MONACO_ZOOM_IN 9103
#define IDM_VIEW_MONACO_ZOOM_OUT 9104
#define IDM_VIEW_MONACO_SYNC_THEME 9105

// ========================================================================
// LSP SERVER COMMANDS — Phase 27 (9200 range)
// ========================================================================
#define IDM_LSP_SERVER_START 9200
#define IDM_LSP_SERVER_STOP 9201
#define IDM_LSP_SERVER_STATUS 9202
#define IDM_LSP_SERVER_REINDEX 9203
#define IDM_LSP_SERVER_STATS 9204
#define IDM_LSP_SERVER_PUBLISH_DIAG 9205
#define IDM_LSP_SERVER_CONFIG 9206
#define IDM_LSP_SERVER_EXPORT_SYMBOLS 9207
#define IDM_LSP_SERVER_LAUNCH_STDIO 9208

// ========================================================================
// EDITOR ENGINE COMMANDS — Phase 28 (9300 range)
// ========================================================================
#define IDM_EDITOR_ENGINE_RICHEDIT_CMD 9300
#define IDM_EDITOR_ENGINE_WEBVIEW2_CMD 9301
#define IDM_EDITOR_ENGINE_MONACOCORE_CMD 9302
#define IDM_EDITOR_ENGINE_CYCLE_CMD 9303
#define IDM_EDITOR_ENGINE_STATUS_CMD 9304

// ========================================================================
// PDB SYMBOL SERVER COMMANDS — Phase 29 (9400 range)
// ========================================================================
#define IDM_PDB_LOAD 9400
#define IDM_PDB_FETCH 9401
#define IDM_PDB_STATUS 9402
#define IDM_PDB_CACHE_CLEAR 9403
#define IDM_PDB_ENABLE 9404
#define IDM_PDB_RESOLVE 9405

// Phase 29.3: PE Import/Export Table Provider
#define IDM_PDB_IMPORTS 9410
#define IDM_PDB_EXPORTS 9411
#define IDM_PDB_IAT_STATUS 9412

// ============================================================================
// PHASE 34: TELEMETRY EXPORT — Privacy-respecting IDE metrics (9900 range)
// ============================================================================
#define IDM_TELEMETRY_TOGGLE 9900
#define IDM_TELEMETRY_EXPORT_JSON 9901
#define IDM_TELEMETRY_EXPORT_CSV 9902
#define IDM_TELEMETRY_SHOW_DASHBOARD 9903
#define IDM_TELEMETRY_CLEAR 9904
#define IDM_TELEMETRY_SNAPSHOT 9905

// ============================================================================
// IDE SELF-AUDIT & VERIFICATION COMMANDS — Phase 31 (9500 range)
// ============================================================================
#define IDM_AUDIT_SHOW_DASHBOARD 9500
#define IDM_AUDIT_RUN_FULL 9501
#define IDM_AUDIT_DETECT_STUBS 9502
#define IDM_AUDIT_CHECK_MENUS 9503
#define IDM_AUDIT_RUN_TESTS 9504
#define IDM_AUDIT_EXPORT_REPORT 9505
#define IDM_AUDIT_QUICK_STATS 9506

// ============================================================================
// PHASE 32: FINAL GAUNTLET — Pre-Packaging Runtime Verification (9600 range)
// ============================================================================
#define IDM_GAUNTLET_RUN 9600
#define IDM_GAUNTLET_EXPORT 9601

// ============================================================================
// BUILD COMMANDS — Unified Build Pipeline (9602+)
// ============================================================================
// IDM_BUILD_SOLUTION already defined as 10400 above

// ============================================================================
// PHASE 33: VOICE CHAT — Native Win32 Audio Engine (9700 range)
// ============================================================================
#define IDM_VOICE_RECORD 9700
#define IDM_VOICE_PTT 9701
#define IDM_VOICE_SPEAK 9702
#define IDM_VOICE_JOIN_ROOM 9703
#define IDM_VOICE_SHOW_DEVICES 9704
#define IDM_VOICE_METRICS 9705
#define IDM_VOICE_TOGGLE_PANEL 9706
#define IDM_VOICE_MODE_PTT 9707
#define IDM_VOICE_MODE_CONTINUOUS 9708
#define IDM_VOICE_MODE_DISABLED 9709

// ============================================================================
// PHASE 33: QUICK-WIN PORTS — Shortcut, Backup, Alerts (9800 range)
// ============================================================================
#define IDM_QW_SHORTCUT_EDITOR 9800
#define IDM_QW_SHORTCUT_RESET 9801
#define IDM_QW_BACKUP_CREATE 9810
#define IDM_QW_BACKUP_RESTORE 9811
#define IDM_QW_BACKUP_AUTO_TOGGLE 9812
#define IDM_QW_BACKUP_LIST 9813
#define IDM_QW_BACKUP_PRUNE 9814
#define IDM_QW_ALERT_TOGGLE_MONITOR 9820
#define IDM_QW_ALERT_SHOW_HISTORY 9821
#define IDM_QW_ALERT_DISMISS_ALL 9822
#define IDM_QW_ALERT_RESOURCE_STATUS 9823
#define IDM_QW_SLO_DASHBOARD 9830

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
    void cmdHotpatchSetTargetTps();

    // Hotpatch state
    bool m_hotpatchEnabled = false;
    bool m_hotpatchUIInitialized = false;
    double m_hotpatchSavedTargetTps = 0.0;

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
    HWND m_hwndDecompView = nullptr;  // Split-view container
    HWND m_hwndDecompPane = nullptr;  // Left pane: decompiler (Direct2D)
    HWND m_hwndDisasmPane = nullptr;  // Right pane: disassembly (Direct2D)
    bool m_decompViewActive = false;  // True when decompiler view is showing

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
    HWND m_hwndMonacoContainer = nullptr;
    WebView2Container* m_webView2 = nullptr;
    bool m_monacoEditorActive = false;
    MonacoEditorOptions m_monacoOptions;
    float m_monacoZoomLevel = 1.0f;

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
    void notifyLSPServerDidOpen(const std::string& uri, const std::string& languageId, const std::string& content);
    void notifyLSPServerDidChange(const std::string& uri, const std::string& content, int version);
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
    struct RuntimeValidationCheck
    {
        std::string name;
        bool passed = false;
        std::string detail;
    };

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
    std::vector<RuntimeValidationCheck> runCriticalValidationBatch1();
    std::vector<RuntimeValidationCheck> runCriticalValidationBatch2();

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
    void telemetryDashboardTrack(const char* eventName, const char* category, const char* payload,
                                 double latencyMs = 0.0);

    // State
    bool m_telemetryInitialized = false;
    bool m_telemetryEnabled = false;

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
    static constexpr int IDM_FR_DASHBOARD = 10101;
    static constexpr int IDM_FR_CLEAR = 10102;

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

    static constexpr int IDM_INSTRUCTIONS_VIEW = 10500;
    static constexpr int IDM_INSTRUCTIONS_RELOAD = 10501;
    static constexpr int IDM_INSTRUCTIONS_COPY = 10502;

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
    static constexpr int IDM_CRUCIBLE_RUN_ALL = 10700;
    static constexpr int IDM_CRUCIBLE_RUN_SHADOW = 10701;
    static constexpr int IDM_CRUCIBLE_RUN_CLUSTER = 10702;
    static constexpr int IDM_CRUCIBLE_RUN_SEMANTIC = 10703;
    static constexpr int IDM_CRUCIBLE_CANCEL = 10704;
    static constexpr int IDM_CRUCIBLE_STATUS = 10705;
    static constexpr int IDM_CRUCIBLE_REPORT = 10706;
    static constexpr int IDM_CRUCIBLE_EXPORT_JSON = 10707;
    static constexpr int IDM_CRUCIBLE_CONFIG = 10708;
    static constexpr int IDM_CRUCIBLE_HELP = 10709;

    // ── Game Engine Command IDs ──
    static constexpr int IDM_GAME_ENGINE_DETECT = 10600;
    static constexpr int IDM_GAME_ENGINE_OPEN = 10601;
    static constexpr int IDM_GAME_ENGINE_CLOSE = 10602;
    static constexpr int IDM_GAME_ENGINE_STATUS = 10603;
    static constexpr int IDM_GAME_ENGINE_BUILD = 10604;
    static constexpr int IDM_GAME_ENGINE_PLAY = 10605;
    static constexpr int IDM_GAME_ENGINE_STOP = 10606;
    static constexpr int IDM_GAME_ENGINE_PAUSE = 10607;
    static constexpr int IDM_GAME_ENGINE_COMPILE = 10608;
    static constexpr int IDM_GAME_ENGINE_PROFILER_START = 10609;
    static constexpr int IDM_GAME_ENGINE_PROFILER_STOP = 10610;
    static constexpr int IDM_GAME_ENGINE_PROFILER_SNAP = 10611;
    static constexpr int IDM_GAME_ENGINE_DEBUG_START = 10612;
    static constexpr int IDM_GAME_ENGINE_DEBUG_STOP = 10613;
    static constexpr int IDM_GAME_ENGINE_BREAKPOINT = 10614;
    static constexpr int IDM_GAME_ENGINE_AI_SUMMARY = 10615;
    static constexpr int IDM_GAME_ENGINE_AI_SCENE = 10616;
    static constexpr int IDM_GAME_ENGINE_INSTALLATIONS = 10617;
    static constexpr int IDM_GAME_ENGINE_HELP = 10618;

    // Unity-specific IDs
    static constexpr int IDM_UNITY_CREATE_SCRIPT = 10650;
    static constexpr int IDM_UNITY_SCENE_LIST = 10651;
    static constexpr int IDM_UNITY_ASSET_BROWSER = 10652;
    static constexpr int IDM_UNITY_PACKAGE_MANAGER = 10653;

    // Unreal-specific IDs
    static constexpr int IDM_UNREAL_CREATE_CLASS = 10670;
    static constexpr int IDM_UNREAL_CREATE_MODULE = 10671;
    static constexpr int IDM_UNREAL_CREATE_PLUGIN = 10672;
    static constexpr int IDM_UNREAL_GEN_PROJECT_FILES = 10673;
    static constexpr int IDM_UNREAL_LIVE_CODING = 10674;
    static constexpr int IDM_UNREAL_COOK_CONTENT = 10675;
    static constexpr int IDM_UNREAL_PACKAGE_PROJECT = 10676;
    static constexpr int IDM_UNREAL_LEVEL_LIST = 10677;
    static constexpr int IDM_UNREAL_BLUEPRINT_LIST = 10678;

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

    static constexpr int IDM_PIPELINE_STATUS = 11000;
    static constexpr int IDM_PIPELINE_SUBMIT = 11001;
    static constexpr int IDM_PIPELINE_CANCEL = 11002;
    static constexpr int IDM_PIPELINE_LIST_NODES = 11003;
    static constexpr int IDM_PIPELINE_ADD_NODE = 11004;
    static constexpr int IDM_PIPELINE_REMOVE_NODE = 11005;
    static constexpr int IDM_PIPELINE_DAG_VIEW = 11006;
    static constexpr int IDM_PIPELINE_STATS = 11007;
    static constexpr int IDM_PIPELINE_SHUTDOWN = 11008;

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
    uint64_t m_hotpatchCtrlActiveTransactionId = 0;

    static constexpr int IDM_HPCTRL_LIST_PATCHES = 11100;
    static constexpr int IDM_HPCTRL_PATCH_DETAIL = 11101;
    static constexpr int IDM_HPCTRL_VALIDATE = 11102;
    static constexpr int IDM_HPCTRL_STAGE = 11103;
    static constexpr int IDM_HPCTRL_APPLY = 11104;
    static constexpr int IDM_HPCTRL_ROLLBACK = 11105;
    static constexpr int IDM_HPCTRL_SUSPEND = 11106;
    static constexpr int IDM_HPCTRL_AUDIT_LOG = 11107;
    static constexpr int IDM_HPCTRL_TXN_BEGIN = 11108;
    static constexpr int IDM_HPCTRL_TXN_COMMIT = 11109;
    static constexpr int IDM_HPCTRL_TXN_ROLLBACK = 11110;
    static constexpr int IDM_HPCTRL_DEP_GRAPH = 11111;
    static constexpr int IDM_HPCTRL_STATS = 11112;

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

    static constexpr int IDM_SA_BUILD_CFG = 11200;
    static constexpr int IDM_SA_COMPUTE_DOMINATORS = 11201;
    static constexpr int IDM_SA_CONVERT_SSA = 11202;
    static constexpr int IDM_SA_DETECT_LOOPS = 11203;
    static constexpr int IDM_SA_OPTIMIZE = 11204;
    static constexpr int IDM_SA_FULL_ANALYSIS = 11205;
    static constexpr int IDM_SA_EXPORT_DOT = 11206;
    static constexpr int IDM_SA_EXPORT_JSON = 11207;
    static constexpr int IDM_SA_STATS = 11208;

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

  public:
    // Unified Problems Panel (P0) — accessed by ProblemsListSubclassProc
    void initProblemsPanel();
    void refreshProblemsView();
    void onProblemsItemActivate(int index);
    const std::vector<RawrXD::ProblemEntry>& problemsViewCache() const { return m_problemsViewCache; }

    // Agent Streaming Bridge helpers (extern "C" bridge access)
    void bridgeRecordSimpleEvent(AgentEventType t, const std::string& desc) { recordSimpleEvent(t, desc); }
    bool bridgeIsAgentPanelReady() const { return m_agentPanelInitialized; }
    void bridgeRefreshAgentDiff() { refreshAgentDiffDisplay(); }

  private:
    void handleProblemsCommand(int commandId);
    void runBuildInBackground(const std::string& workingDir, const std::string& buildCommand);
    bool m_problemsPanelInitialized = false;
    bool m_problemsShowErrors = true;
    bool m_problemsShowWarnings = true;
    bool m_problemsShowInfo = false;

    // Tier 5 Cosmetics state
    bool m_lineEndingSelectorInitialized = false;
    int m_lineEndingMode = 0;
    bool m_debugWatchInitialized = false;
    bool m_callStackSymbolsInitialized = false;
    HWND m_hwndProblemsPanel = nullptr;
    HWND m_hwndProblemsFilter = nullptr;
    std::vector<RawrXD::ProblemEntry> m_problemsViewCache;

    bool m_semanticPanelInitialized = false;

    static constexpr int IDM_SEM_GO_TO_DEF = 11300;
    static constexpr int IDM_SEM_FIND_REFS = 11301;
    static constexpr int IDM_SEM_FIND_IMPLS = 11302;
    static constexpr int IDM_SEM_TYPE_HIERARCHY = 11303;
    static constexpr int IDM_SEM_CALL_GRAPH = 11304;
    static constexpr int IDM_SEM_SEARCH_SYMBOLS = 11305;
    static constexpr int IDM_SEM_FILE_SYMBOLS = 11306;
    static constexpr int IDM_SEM_UNUSED = 11307;
    static constexpr int IDM_SEM_INDEX_FILE = 11308;
    static constexpr int IDM_SEM_REBUILD_INDEX = 11309;
    static constexpr int IDM_SEM_SAVE_INDEX = 11310;
    static constexpr int IDM_SEM_LOAD_INDEX = 11311;
    static constexpr int IDM_SEM_STATS = 11312;

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
    bool m_securityDashboardInitialized = false;

    static constexpr int IDM_TEL_TRACE_STATUS = 11400;
    static constexpr int IDM_TEL_START_SPAN = 11401;
    static constexpr int IDM_TEL_AUDIT_LOG = 11402;
    static constexpr int IDM_TEL_AUDIT_VERIFY = 11403;
    static constexpr int IDM_TEL_COMPLIANCE_REPORT = 11404;
    static constexpr int IDM_TEL_VIOLATIONS = 11405;
    static constexpr int IDM_TEL_LICENSE_STATUS = 11406;
    static constexpr int IDM_TEL_USAGE_METER = 11407;
    static constexpr int IDM_TEL_METRICS_DASHBOARD = 11408;
    static constexpr int IDM_TEL_METRICS_FLUSH = 11409;
    static constexpr int IDM_TEL_EXPORT_AUDIT = 11410;
    static constexpr int IDM_TEL_EXPORT_OTLP = 11411;
    static constexpr int IDM_TEL_GDPR_EXPORT = 11412;
    static constexpr int IDM_TEL_GDPR_DELETE = 11413;
    static constexpr int IDM_TEL_SET_LEVEL = 11414;
    static constexpr int IDM_TEL_STATS = 11415;

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
    static constexpr int IDM_GAPCLOSE_INIT = 10800;
    static constexpr int IDM_GAPCLOSE_STATUS = 10801;
    static constexpr int IDM_GAPCLOSE_PERF = 10802;
    static constexpr int IDM_GAPCLOSE_HELP = 10803;
    // Vector Database
    static constexpr int IDM_GAPCLOSE_VECDB_INIT = 10810;
    static constexpr int IDM_GAPCLOSE_VECDB_INSERT = 10811;
    static constexpr int IDM_GAPCLOSE_VECDB_SEARCH = 10812;
    static constexpr int IDM_GAPCLOSE_VECDB_DELETE = 10813;
    static constexpr int IDM_GAPCLOSE_VECDB_STATUS = 10814;
    static constexpr int IDM_GAPCLOSE_VECDB_BENCH = 10815;
    // Composer
    static constexpr int IDM_GAPCLOSE_COMPOSER_BEGIN = 10820;
    static constexpr int IDM_GAPCLOSE_COMPOSER_ADD = 10821;
    static constexpr int IDM_GAPCLOSE_COMPOSER_COMMIT = 10822;
    static constexpr int IDM_GAPCLOSE_COMPOSER_STATUS = 10823;
    // CRDT
    static constexpr int IDM_GAPCLOSE_CRDT_INIT = 10830;
    static constexpr int IDM_GAPCLOSE_CRDT_INSERT = 10831;
    static constexpr int IDM_GAPCLOSE_CRDT_DELETE = 10832;
    static constexpr int IDM_GAPCLOSE_CRDT_STATUS = 10833;
    // Git Context
    static constexpr int IDM_GAPCLOSE_GIT_CONTEXT = 10840;
    static constexpr int IDM_GAPCLOSE_GIT_BRANCH = 10841;

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

    static constexpr int IDM_TELEXPORT_JSON = 11500;
    static constexpr int IDM_TELEXPORT_CSV = 11501;
    static constexpr int IDM_TELEXPORT_PROMETHEUS = 11502;
    static constexpr int IDM_TELEXPORT_OTLP = 11503;
    static constexpr int IDM_TELEXPORT_AUDIT_LOG = 11504;
    static constexpr int IDM_TELEXPORT_VERIFY_CHAIN = 11505;
    static constexpr int IDM_TELEXPORT_AUTO_START = 11506;
    static constexpr int IDM_TELEXPORT_AUTO_STOP = 11507;

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

    static constexpr int IDM_COMPOSER_NEW_SESSION = 11510;
    static constexpr int IDM_COMPOSER_END_SESSION = 11511;
    static constexpr int IDM_COMPOSER_APPROVE_ALL = 11512;
    static constexpr int IDM_COMPOSER_REJECT_ALL = 11513;
    static constexpr int IDM_COMPOSER_SHOW_TRANSCRIPT = 11514;
    static constexpr int IDM_COMPOSER_SHOW_METRICS = 11515;

    // ── @-Mention Context Parser (inline context assembly) ──
    void initContextMentionParser();
    bool handleMentionParserCommand(int commandId);
    void cmdMentionParse();
    void cmdMentionSuggest();
    void cmdMentionAssembleContext();
    void cmdMentionRegisterCustom();
    bool m_mentionParserInitialized = false;

    static constexpr int IDM_MENTION_PARSE = 11520;
    static constexpr int IDM_MENTION_SUGGEST = 11521;
    static constexpr int IDM_MENTION_ASSEMBLE_CTX = 11522;
    static constexpr int IDM_MENTION_REGISTER_CUSTOM = 11523;

    // ── Vision Encoder (image input, clipboard paste, drag-drop) ──
    void initVisionEncoderUI();
    bool handleVisionEncoderCommand(int commandId);
    void cmdVisionLoadFile();
    void cmdVisionPasteClipboard();
    void cmdVisionScreenshot();
    void cmdVisionBuildPayload();
    void cmdVisionViewIDEGUIAndHotpatch();  // View IDE GUI + audit/hotpatch layout
    bool m_visionEncoderUIInitialized = false;
    void showVisionEncoder();
    void hideVisionEncoder();
    void initVisionEncoder();
    void shutdownVisionEncoder();

    // ── Semantic Index (code intelligence, cross-referencing) ──
    void showSemanticIndex();
    void hideSemanticIndex();
    void shutdownSemanticIndex();

    // GUI layout hotpatch (image viewer): audit overlaps/zero-size, auto-correct via onSize
    bool auditIDEGUILayout(std::string& reportOut);
    void hotpatchIDELayout();

    static constexpr int IDM_VISION_LOAD_FILE = 11530;
    static constexpr int IDM_VISION_PASTE_CLIPBOARD = 11531;
    static constexpr int IDM_VISION_SCREENSHOT = 11532;
    static constexpr int IDM_VISION_BUILD_PAYLOAD = 11533;
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

    static constexpr int IDM_REFACTOR_EXTRACT_METHOD = 11540;
    static constexpr int IDM_REFACTOR_EXTRACT_VARIABLE = 11541;
    static constexpr int IDM_REFACTOR_RENAME_SYMBOL = 11542;
    static constexpr int IDM_REFACTOR_ORGANIZE_INCLUDES = 11543;
    static constexpr int IDM_REFACTOR_CONVERT_AUTO = 11544;
    static constexpr int IDM_REFACTOR_REMOVE_DEAD_CODE = 11545;
    static constexpr int IDM_REFACTOR_SHOW_ALL = 11546;
    static constexpr int IDM_REFACTOR_LOAD_PLUGIN = 11547;

    // ── Language Plugin (60+ language descriptors, DLL extensible) ──
    void initLanguageRegistry();
    bool handleLanguageCommand(int commandId);
    void cmdLanguageDetect();
    void cmdLanguageListAll();
    void cmdLanguageLoadPlugin();
    void cmdLanguageSetForFile();
    bool m_languageRegistryInitialized = false;

    static constexpr int IDM_LANG_DETECT = 11550;
    static constexpr int IDM_LANG_LIST_ALL = 11551;
    static constexpr int IDM_LANG_LOAD_PLUGIN = 11552;
    static constexpr int IDM_LANG_SET_FOR_FILE = 11553;

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

    static constexpr int IDM_SEMANTIC_BUILD_INDEX = 11560;
    static constexpr int IDM_SEMANTIC_FUZZY_SEARCH = 11561;
    static constexpr int IDM_SEMANTIC_FIND_REFS = 11562;
    static constexpr int IDM_SEMANTIC_SHOW_DEPS = 11563;
    static constexpr int IDM_SEMANTIC_TYPE_HIERARCHY = 11564;
    static constexpr int IDM_SEMANTIC_CALL_GRAPH = 11565;
    static constexpr int IDM_SEMANTIC_FIND_CYCLES = 11566;
    static constexpr int IDM_SEMANTIC_LOAD_PLUGIN = 11567;

    // ── Resource Generator (Docker, K8s, Terraform, CI/CD, config) ──
    void initResourceGenerator();
    bool handleResourceGenCommand(int commandId);
    void cmdResourceGenerate();
    void cmdResourceGenerateProject();
    void cmdResourceListTemplates();
    void cmdResourceSearchTemplates();
    void cmdResourceLoadPlugin();
    bool m_resourceGeneratorInitialized = false;

    static constexpr int IDM_RESOURCE_GENERATE = 11570;
    static constexpr int IDM_RESOURCE_GEN_PROJECT = 11571;
    static constexpr int IDM_RESOURCE_LIST_TEMPLATES = 11572;
    static constexpr int IDM_RESOURCE_SEARCH = 11573;
    static constexpr int IDM_RESOURCE_LOAD_PLUGIN = 11574;

    // ── Enterprise Stress Tests (load testing, stability, perf validation) ──
    static constexpr int IDM_ENTERPRISE_STRESS_RUN = 11575;
    static constexpr int IDM_ENTERPRISE_STRESS_STOP = 11576;
    static constexpr int IDM_ENTERPRISE_STRESS_SHOW = 11577;

    // ── Cursor/JB-Parity Menu Builder ──
    void createFeaturesMenu(HMENU parentMenu);
    bool handleFeaturesCommand(int commandId);
    void initAllFeatureModules();

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
    enum LSPSymbolKind
    {
        SK_File = 1,
        SK_Module,
        SK_Namespace,
        SK_Package,
        SK_Class,
        SK_Method,
        SK_Property,
        SK_Field,
        SK_Constructor,
        SK_Enum,
        SK_Interface,
        SK_Function,
        SK_Variable,
        SK_Constant,
        SK_String,
        SK_Number,
        SK_Boolean,
        SK_Array,
        SK_Object,
        SK_Key,
        SK_Null,
        SK_EnumMember,
        SK_Struct,
        SK_Event,
        SK_Operator,
        SK_TypeParameter
    };

  private:
    // ── OutlineSymbol (enhanced outline model) ──
    struct OutlineSymbol
    {
        int kind = 0;  // LSPSymbolKind
        std::string name;
        int line = 0;
        int column = 0;
        int endLine = 0;
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
    enum class DiffLineType
    {
        Context,
        Added,
        Removed
    };

    struct DiffLine
    {
        std::string text;
        DiffLineType type = DiffLineType::Context;
        int lineNumber = -1;
    };

    struct DiffHunk
    {
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
    void populateGitDiffPane(HWND hwndRichEdit, const std::vector<DiffLine>& lines, const std::string& header);
    void navigateDiffHunk(int direction);
    static LRESULT CALLBACK GitDiffPanelProc(HWND, UINT, WPARAM, LPARAM);

    bool m_gitDiffVisible = false;
    HWND m_hwndGitDiffPanel = nullptr;
    HWND m_hwndGitDiffLeft = nullptr;
    HWND m_hwndGitDiffRight = nullptr;
    HFONT m_gitDiffFont = nullptr;
    std::vector<DiffLine> m_gitDiffLeftLines;
    std::vector<DiffLine> m_gitDiffRightLines;
    std::vector<DiffHunk> m_gitDiffHunks;
    int m_gitDiffCurrentHunk = -1;

    // ── Inline Diff Viewer (Side-by-Side + Inline mode) ──
    struct DiffViewHunk
    {
        enum HunkType
        {
            Added,
            Removed,
            Modified
        };
        HunkType type = Added;
        int leftStart = 0, leftCount = 0;
        int rightStart = 0, rightCount = 0;
        std::string leftText, rightText;
    };

    struct DiffViewState
    {
        bool visible = false;
        std::string leftTitle, rightTitle;
        std::vector<std::string> leftLines, rightLines;
        std::vector<DiffViewHunk> hunks;
        int scrollPos = 0;
        int currentHunk = -1;
        bool inlineMode = false;
        HFONT hFont = nullptr;
    };

    void initDiffView();
    void shutdownDiffView();
    void openDiffView(const std::string& leftContent, const std::string& rightContent, const std::string& leftTitle,
                      const std::string& rightTitle);
    void closeDiffView();
    void computeDiffHunks();
    void diffNavigateHunk(int direction);
    void toggleDiffInlineMode();
    void renderDiffPanel(HWND hwnd, HDC hdc, bool isRight);
    void openGitDiffForCurrentFile();
    static LRESULT CALLBACK DiffPanelProc(HWND, UINT, WPARAM, LPARAM);

    DiffViewState m_diffState;
    HWND m_hwndDiffPanel = nullptr;
    HWND m_hwndDiffLeft = nullptr;
    HWND m_hwndDiffRight = nullptr;
    HWND m_hwndDiffToolbar = nullptr;

    static constexpr int IDC_DIFF_PANEL = 10800;
    static constexpr int IDC_DIFF_TOOLBAR = 10801;
    static constexpr int IDC_DIFF_LEFT = 10802;
    static constexpr int IDC_DIFF_RIGHT = 10803;
    static constexpr int IDC_DIFF_PREV_BTN = 10804;
    static constexpr int IDC_DIFF_NEXT_BTN = 10805;
    static constexpr int IDC_DIFF_INLINE_BTN = 10806;
    static constexpr int IDC_DIFF_CLOSE_BTN = 10807;

    // ── TerminalProfile / TerminalTabInfo (Terminal Tabs) ──
    struct TerminalProfile
    {
        std::string name;
        std::string shellPath;
        std::string shellArgs;
        std::string icon;
        COLORREF color = RGB(204, 204, 204);
    };

    struct TerminalTabInfo
    {
        int profileIndex = 0;
        std::string title;
        COLORREF color = RGB(204, 204, 204);
        bool active = true;
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
    int m_activeTerminalTab = 0;
    std::vector<TerminalProfile> m_terminalTabProfiles;
    std::vector<TerminalTabInfo> m_terminalTabs;

    // 13. Hover Documentation Tooltips
    struct HoverTooltipState
    {
        HWND hwndPopup = nullptr;
        bool visible = false;
        bool pending = false;
        std::string content;
        HFONT hFont = nullptr;
        HFONT hBoldFont = nullptr;
        int line = 0;
        int column = 0;
        POINT screenPos = {};
    };

    void initHoverTooltip();
    void initHoverTooltips();
    void shutdownHoverTooltip();
    void shutdownHoverTooltips();
    void showHoverTooltip(int screenX, int screenY, const std::string& content);
    void dismissHoverTooltip();
    void onEditorMouseHover(int charPos);
    void onEditorMouseHover(int x, int y);
    void onHoverTimer();
    void onHoverReady(const std::string& content, int line, int col);
    void renderHoverContent(HDC hdc, RECT rc, const std::string& content);
    static LRESULT CALLBACK HoverTooltipProc(HWND, UINT, WPARAM, LPARAM);

    HoverTooltipState m_hoverState;
    HWND m_hwndHoverPopup = nullptr;
    bool m_hoverVisible = false;
    std::string m_hoverContent;
    HFONT m_hoverFont = nullptr;
    HFONT m_hoverBoldFont = nullptr;
    bool m_hoverPending = false;
    int m_hoverLine = 0;
    int m_hoverColumn = 0;
    POINT m_hoverScreenPos = {};
    static constexpr UINT_PTR HOVER_TIMER_ID = 42001;
    static constexpr int HOVER_TIMER_DELAY = 500;
    static constexpr UINT WM_HOVER_READY = WM_USER + 200;

    // 14. Parameter Hints (Signature Help)
    void initSignatureHelp();
    void shutdownSignatureHelp();
    void triggerSignatureHelp();
    void showSignaturePopup(int screenX, int screenY);
    void dismissSignatureHelp();
    void updateSignatureActiveParam(int paramIndex);
    static LRESULT CALLBACK SignatureHelpProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hwndSignaturePopup = nullptr;
    bool m_signatureVisible = false;
    std::string m_signatureContent;
    int m_signatureActiveParam = 0;
    int m_signatureParamCount = 0;
    HFONT m_signatureFont = nullptr;

    // ── ReferenceResult (Find All References UI) ──
    struct ReferenceResult
    {
        std::string filePath;
        int line = 0;
        int column = 0;
        std::string contextLine;
    };

    // ── RenameChange / RenamePreviewState (Rename Refactoring Preview) ──
    struct RenameChange
    {
        std::string filePath;
        int line = 0;
        int column = 0;
        std::string oldText;
        std::string newText;
        std::string contextLine;
        bool selected = true;
    };

    struct RenamePreviewState
    {
        std::string oldName;
        std::string newName;
        std::vector<RenameChange> changes;
        bool visible = false;
        HWND hwndPanel = nullptr;
        HWND hwndList = nullptr;
        HFONT hFont = nullptr;
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

    static constexpr int IDC_RENAME_PREVIEW = 11280;
    static constexpr int IDC_RENAME_INPUT = 11281;
    static constexpr int IDC_RENAME_CHECKLIST = 11282;
    static constexpr int IDC_RENAME_APPLY_BTN = 11283;
    static constexpr int IDC_RENAME_CANCEL_BTN = 11284;

    // 16. Find All References UI
    void initReferencePanel();
    void shutdownReferencePanel();
    void showFindAllReferences(const std::string& symbol, const std::vector<ReferenceResult>& results);
    void closeReferencePanel();
    void navigateToReference(int refIndex);
    void cmdFindAllReferences(const std::string& symbol);
    static LRESULT CALLBACK ReferencePanelProc(HWND, UINT, WPARAM, LPARAM);

    HWND m_hwndReferencePanel = nullptr;
    HWND m_hwndReferenceTree = nullptr;
    bool m_referencePanelVisible = false;
    std::string m_referenceSymbol;
    std::vector<ReferenceResult> m_referenceResults;
    uint32_t m_referenceResultsVersion = 0;  // bump whenever results refresh
    HFONT m_referenceFont = nullptr;

    // ── CodeLensEntry ──
    struct CodeLensEntry
    {
        int line = 0;
        std::string symbol;
        int referenceCount = 0;
        std::string text;
        COLORREF color = RGB(150, 150, 150);
        std::string command;
    };

    // 18. CodeLens (Reference Counts)
    void initCodeLens();
    void shutdownCodeLens();
    void refreshCodeLens();
    int countSymbolReferences(const std::string& symbol);
    void renderCodeLens(HDC hdc);
    void renderCodeLens(HDC hdc, int lineY, int lineNumber);
    void toggleCodeLens();
    void onCodeLensClick(int line);
    void computeCodeLensForFunction(const std::string& funcName, int line);

    bool m_codeLensEnabled = true;
    std::vector<CodeLensEntry> m_codeLensEntries;
    HFONT m_codeLensFont = nullptr;

    // ── InlayHintEntry ──
    enum class InlayHintKind
    {
        Type,
        Parameter,
        Enum
    };

    struct InlayHintEntry
    {
        int line = 0;
        int column = 0;
        std::string text;
        InlayHintKind kind = InlayHintKind::Type;
        COLORREF color = RGB(104, 151, 187);
    };

    // 19. Inlay Type Hints
    void initInlayHints();
    void shutdownInlayHints();
    void refreshInlayHints();
    std::string inferTypeFromExpression(const std::string& expr);
    void renderInlayHints(HDC hdc);
    void renderInlayHints(HDC hdc, int lineY, int lineNumber);
    void toggleInlayHints();
    void parseInlayHintsFromLSP(const std::string& jsonResponse);

    bool m_inlayHintsEnabled = true;
    std::vector<InlayHintEntry> m_inlayHintEntries;
    HFONT m_inlayHintFont = nullptr;

    // ── Tier 2 Command IDs (11700–11799) ──
    static constexpr int IDM_TIER2_GITDIFF = 11700;
    static constexpr int IDM_TIER2_GITDIFF_CLOSE = 11701;
    static constexpr int IDM_TIER2_GITDIFF_PREV = 11702;
    static constexpr int IDM_TIER2_GITDIFF_NEXT = 11703;
    static constexpr int IDM_TIER2_TERMINAL_NEW = 11710;
    static constexpr int IDM_TIER2_TERMINAL_CLOSE = 11711;
    static constexpr int IDM_TIER2_HOVER = 11720;
    static constexpr int IDM_TIER2_SIGHELP = 11721;
    static constexpr int IDM_TIER2_OUTLINE_REFRESH = 11730;
    static constexpr int IDM_TIER2_OUTLINE_FILTER = 11731;
    static constexpr int IDM_TIER2_OUTLINE_SORT = 11732;
    static constexpr int IDM_TIER2_FIND_REFS = 11740;
    static constexpr int IDM_TIER2_RENAME_PREVIEW = 11750;
    static constexpr int IDM_TIER2_CODELENS_TOGGLE = 11760;
    static constexpr int IDM_TIER2_CODELENS_REFRESH = 11761;
    static constexpr int IDM_TIER2_INLAY_TOGGLE = 11770;
    static constexpr int IDM_TIER2_INLAY_REFRESH = 11771;

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

    struct CaretAnimation
    {
        float currentX = 0.0f;
        float currentY = 0.0f;
        float targetX = 0.0f;
        float targetY = 0.0f;
        bool blinkOn = true;
        bool animating = false;
        bool enabled = false;
        int blinkPhase = 0;
    };
    CaretAnimation m_caretAnim;

    // 32. Font Ligatures (DirectWrite)
    void initDirectWriteLigatures();
    void shutdownDirectWriteLigatures();
    void toggleLigatures();
    IDWriteTextLayout* createLigatureLayout(const std::wstring& text, float maxWidth);

    IDWriteFactory* m_dwFactory = nullptr;
    IDWriteTextFormat* m_dwTextFormat = nullptr;
    bool m_ligaturesEnabled = false;

    // 33. High DPI Polish
    void onDpiChanged(UINT newDpi, const RECT* suggestedRect);
    int dpiScaleValue(int basePixels) const;
    float getDpiScaleFactor() const;
    float m_dpiScaleFactor = 1.0f;

    // 34. Theme Toggle Animation
    void beginThemeTransition(int targetThemeId);
    void onThemeAnimationTick();
    void applyThemeByIdAnimated(int themeId);

    struct ThemeTransition
    {
        IDETheme fromTheme;
        IDETheme toTheme;
        int targetThemeId = 0;
        UINT elapsedMs = 0;
        bool active = false;
    };
    ThemeTransition m_themeTransition;

    // 35. File Watcher Indicators
    void initFileWatcher();
    void shutdownFileWatcher();
    void startWatchingFile(const std::string& filePath);
    void stopWatchingFile();
    void onExternalFileChange(const std::string& changedFile);
    void showFileChangedToast();
    // reloadCurrentFile() declared above (line ~1295)

    std::unique_ptr<IocpFileWatcher> m_fileWatcher;
    std::string m_watchedFilePath;
    bool m_fileChangedExternally = false;

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
    bool m_formatInProgress = false;
    bool m_lspFormatEnabled = false;

    // 38. Language Mode Quick Switch
    void showLanguageModeSelector();

    // 39. Encoding Selector UI
    void showEncodingSelector();
    void reopenWithEncoding(const char* encodingName, int codePage);
    void saveWithEncoding(const char* encodingName, int codePage);
    int m_currentEncoding = 65001;  // CP_UTF8

    // Tier 3: Status bar click routing
    void handleStatusBarClick(int partIndex);

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

    struct ZenModePrevState
    {
        bool sidebarWasVisible = false;
        bool statusBarWasVisible = false;
        bool activityBarWasVisible = false;
        bool tabBarWasVisible = false;
        bool panelWasVisible = false;
        bool menuWasVisible = false;
        bool wasMaximized = false;
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
    int m_lightbulbLine = -1;

    struct CodeAction
    {
        std::string title;
        std::string kind;  // "quickfix", "refactor", "refactor.extract", "source.organizeImports", "suppress"
        int diagnosticIndex = -1;
        bool isFromLSP = false;   // true if action came from LSP server
        std::string lspEditJson;  // JSON string of workspace edit (only for LSP actions)
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
    int getFoldRegionAtGutterClick(int y);
    bool m_codeFoldingEnabled = false;

    struct FoldRegion
    {
        int startLine = 0;
        int endLine = 0;
        int depth = 0;
        bool collapsed = false;
        std::string foldedText;
        int foldMarkerStart = 0;
        int foldMarkerLen = 0;
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
    static constexpr int IDM_T1_SMOOTH_SCROLL_TOGGLE = 12000;
    static constexpr int IDM_T1_SMOOTH_SCROLL_SPEED = 12001;
    static constexpr int IDM_T1_MINIMAP_TOGGLE = 12010;
    static constexpr int IDM_T1_MINIMAP_HIGHLIGHT = 12011;
    static constexpr int IDM_T1_BREADCRUMBS_TOGGLE = 12020;
    static constexpr int IDM_T1_FUZZY_PALETTE = 12030;
    static constexpr int IDM_T1_FUZZY_FILES = 12031;
    static constexpr int IDM_T1_FUZZY_SYMBOLS = 12032;
    static constexpr int IDM_T1_SETTINGS_GUI = 12040;
    static constexpr int IDM_T1_SETTINGS_RESET = 12042;
    static constexpr int IDM_T1_WELCOME_SHOW = 12050;
    static constexpr int IDM_T1_WELCOME_CLONE = 12051;
    static constexpr int IDM_T1_WELCOME_OPEN_FOLDER = 12052;
    static constexpr int IDM_T1_WELCOME_NEW_FILE = 12053;
    static constexpr int IDM_T1_ICON_THEME_SET = 12060;
    static constexpr int IDM_T1_ICON_THEME_SETI = 12061;
    static constexpr int IDM_T1_ICON_THEME_MATERIAL = 12062;
    static constexpr int IDM_T1_TAB_DRAG_ENABLE = 12070;
    static constexpr int IDM_T1_SPLIT_VERTICAL = 12080;
    static constexpr int IDM_T1_SPLIT_HORIZONTAL = 12081;
    static constexpr int IDM_T1_SPLIT_GRID_2X2 = 12082;
    static constexpr int IDM_T1_SPLIT_CLOSE = 12083;
    static constexpr int IDM_T1_SPLIT_FOCUS_NEXT = 12084;
    static constexpr int IDM_T1_UPDATE_CHECK = 12090;
    static constexpr int IDM_T1_UPDATE_INSTALL = 12091;
    static constexpr int IDM_T1_UPDATE_DISMISS = 12092;
    static constexpr int IDM_T1_UPDATE_RELEASE_NOTES = 12093;

    // 1. Smooth Scroll Animation
    void initSmoothScroll();
    void shutdownSmoothScroll();
    bool onSmoothMouseWheel(WPARAM wParam, LPARAM lParam);
    void onSmoothScrollTick();

    struct SmoothScrollState
    {
        bool enabled = true;
        float velocityY = 0.0f;
        float currentY = 0.0f;
        float targetY = 0.0f;
        bool animating = false;
    };
    SmoothScrollState m_smoothScroll;

    // 2. Minimap Enhanced (extends existing minimap)
    void initMinimapEnhanced();
    void paintMinimapEnhanced(HDC hdc, const RECT& rect);
    bool m_minimapHighlightCursor = true;

    // 3. Breadcrumbs (main implementation in Win32IDE_Breadcrumbs.cpp)
    static constexpr int IDC_BREADCRUMB_BAR = 9825;  // ESP: control ID for symbol path bar
    struct BreadcrumbItem
    {
        std::string label;
        std::string symbolKind;
        int line = 0;
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
    std::vector<RECT> m_breadcrumbRects;
    int m_breadcrumbHeight = 22;
    HFONT m_breadcrumbFont = nullptr;
    HWND m_hwndBreadcrumbs = nullptr;

    // 4. Command Palette Fuzzy Search
    void initFuzzySearch();
    void showFuzzyCommandPalette();
    void showFuzzyPaletteWindow();
    void showFuzzyFileFinder();
    void showFuzzySymbolSearch();
    int fuzzyMatchScore(const std::string& pattern, const std::string& candidate,
                        std::vector<int>* matchPositions = nullptr);
    void fuzzyFilterCommandPalette(const std::string& query);
    void paintFuzzyHighlights(HDC hdc, RECT itemRect, const std::string& text, const std::vector<int>& matchPositions);

    struct FuzzyCommandEntry
    {
        int commandId = 0;
        const char* label = nullptr;
        const char* category = nullptr;
        const char* cliAlias = nullptr;
    };
    std::vector<FuzzyCommandEntry> m_fuzzyCommandLabels;

    // 5. Settings GUI
    struct SettingsCategory
    {
        std::string name;
        std::vector<std::string> keys;
    };

    void initSettingsGUI();
    void showSettingsGUI();
    void showSettingsGUIDialog();
    void buildSettingsSchema();
    void createSettingsControls(HWND hwndParent);
    void onSettingsCategorySelect(int categoryIndex);
    void onSettingChanged(const std::string& key, const std::string& value);
    void filterSettings(const std::string& query);
    void populateSettingsTree();
    friend LRESULT CALLBACK SettingsSummaryWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    std::vector<SettingsCategory> m_settingsSchema;
    std::string m_settingsSearchQuery;
    HWND m_hwndSettingsGUI = nullptr;
    HWND m_hwndSettingsSearch = nullptr;
    HWND m_hwndSettingsTabs = nullptr;
    HWND m_hwndSettingsPanel = nullptr;

    // 6. Welcome / Onboarding Page
    void initWelcomePage();
    void showWelcomePage();
    void handleWelcomeCloneRepo();
    void handleWelcomeOpenFolder();
    void handleWelcomeNewFile();
    bool m_showWelcomeOnStartup = true;
    bool m_welcomePageShown = false;

    // 7. File Icon Theme Support
    void initFileIconTheme();
    int getFileIconIndex(const std::string& filename) const;
    void setFileIconTheme(const std::string& themeName);
    void showFileIconThemeSelector();
    HIMAGELIST m_fileIconImageList = nullptr;
    std::string m_currentIconTheme = "seti";

    // 8. Drag-and-Drop File Tabs
    void initTabDragDrop();
    void reorderTab(int fromIndex, int toIndex);
    void onTabDragTick();
    static LRESULT CALLBACK TabBarDragProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    WNDPROC m_originalTabBarProc = nullptr;
    bool m_tabDragEnabled = true;
    bool m_tabDragging = false;
    int m_dragTabIndex = -1;
    int m_dragInsertIndex = -1;
    int m_dragStartX = 0;
    int m_dragStartY = 0;

    // 9. Split Editor (Grid Layout)
    void initSplitEditor();
    HWND createEditorPane(HWND parent, const RECT& bounds);
    void splitEditorVertical();
    void splitEditorHorizontal();
    void splitEditorGrid2x2();
    void closeSplitEditor();
    void focusNextSplitPane();
    void layoutSplitPanes();

    struct SplitEditorPane
    {
        HWND hwnd = nullptr;
        int row = 0;
        int col = 0;
        std::string filePath;
    };
    std::vector<SplitEditorPane> m_splitPanes;
    bool m_splitEditorActive = false;
    int m_splitOrientation = 0;  // 0=none, 1=vert, 2=horiz, 3=grid

    // 10. Auto-Update Notification UI
    void initAutoUpdateUI();
    void shutdownAutoUpdateUI();
    void checkForUpdates();
    void showUpdateNotification();
    void installUpdate();
    void dismissUpdateNotification();
    void showReleaseNotes();
    bool m_updateAvailable = false;
    bool m_updateDismissed = false;
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

    // ── Phase Ω: OmegaOrchestrator — Autonomous SDLC (12400–12450) ──
    bool handleOmegaOrchestratorCommand(int commandId);

    // ── Agentic Planning Orchestrator — Multi-step planning with approval gates (4261–4270) ──
    bool handleAgenticPlanningCommand(int commandId);

    // ── FailureIntelligence Orchestrator — Autonomous recovery & root cause analysis (4281–4299) ──
    bool handleFailureIntelligenceCommand(int commandId);

    // ── KnowledgeGraphCore — Cross-session learning + decision archaeology (4271–4280) ──
    bool handleKnowledgeGraphCommand(int commandId);

    // ── Change Impact Analyzer — Pre-commit ripple effect prediction (4350–4370) ──
    bool handleChangeImpactCommand(int commandId);

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
    static constexpr int IDM_PROVABLE_SHOW = 13000;
    static constexpr int IDM_PROVABLE_START = 13001;
    static constexpr int IDM_PROVABLE_RECORD = 13002;
    static constexpr int IDM_PROVABLE_VERIFY = 13003;
    static constexpr int IDM_PROVABLE_REPLAY = 13004;
    static constexpr int IDM_PROVABLE_EXPORT = 13005;
    static constexpr int IDM_PROVABLE_RESET = 13006;
    static constexpr int IDM_PROVABLE_STATS = 13007;

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
    static constexpr int IDM_AIRE_SHOW = 13020;
    static constexpr int IDM_AIRE_LOAD = 13021;
    static constexpr int IDM_AIRE_AI_RENAME = 13022;
    static constexpr int IDM_AIRE_VULNSCAN = 13023;
    static constexpr int IDM_AIRE_ANNOTATE = 13024;
    static constexpr int IDM_AIRE_CALLGRAPH = 13025;
    static constexpr int IDM_AIRE_DIFF = 13026;
    static constexpr int IDM_AIRE_EXPORT = 13027;
    static constexpr int IDM_AIRE_STATS = 13028;

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
    static constexpr int IDM_AIRGAP_SHOW = 13040;
    static constexpr int IDM_AIRGAP_COMPLIANCE = 13041;
    static constexpr int IDM_AIRGAP_MODELS = 13042;
    static constexpr int IDM_AIRGAP_AUDIT = 13043;
    static constexpr int IDM_AIRGAP_DLP = 13044;
    static constexpr int IDM_AIRGAP_FIREWALL = 13045;
    static constexpr int IDM_AIRGAP_LICENSE = 13046;
    static constexpr int IDM_AIRGAP_ENCRYPT = 13047;
    static constexpr int IDM_AIRGAP_EXPORT = 13048;
    static constexpr int IDM_AIRGAP_STATS = 13049;

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
    void cmdCallStackCapture();
    void cmdCallStackShowDialog();
    void cmdCallStackCopy();
    void cmdCallStackResolve();

    // 45. Marketplace
    void initMarketplace();
    bool handleMarketplaceCommand(int commandId);
    void cmdMarketplaceShow();
    void cmdMarketplaceSearch();
    void cmdMarketplaceInstall();
    void cmdMarketplaceUninstall();
    void cmdMarketplaceList();
    void cmdMarketplaceStatus();

    // Collaboration (View > Collaboration) — CRDT/WebSocket live session panel
    void showCollabPanel();

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

    // Caret Animation
    void initCaretAnimation();
    void shutdownCaretAnimation();
    void startCaretBlink();
    void stopCaretBlink();
    void setCaretBlinkRate(int milliseconds);
    void animateCaretToPosition(int line, int column);
    bool isCaretAnimationEnabled() const;
    void toggleCaretAnimation();

    // Agent Ollama Client
    void initAgentOllamaClient();
    void shutdownAgentOllamaClient();
    bool testOllamaConnection();
    bool isOllamaConnected() const;
    std::string getOllamaStatus() const;
    void setOllamaEndpoint(const std::string& endpoint);
    std::string getOllamaEndpoint() const;

    // Model Discovery
    void initModelDiscovery();
    void shutdownModelDiscovery();
    void scanForModels();
    std::vector<std::string> getAvailableModels() const;
    std::vector<std::string> getModelPaths() const;
    bool isModelDiscoveryEnabled() const;
    void setModelDiscoveryPaths(const std::vector<std::string>& paths);
    std::vector<std::string> getModelDiscoveryPaths() const;

    // Enterprise Stress Tests
    void initEnterpriseStressTests();
    bool executeEnterpriseStressTest(int durationSeconds, int threadCount);
    void handleEnterpriseStressTestCommand();
    bool showStressTestDialog();
    StressTestResults getStressTestResults() const { return m_stressTestResults; }

    // SQLite3 Core
    void initSQLite3Core();
    void shutdownSQLite3Core();
    bool saveSetting(const std::string& key, const std::string& value);
    std::string loadSetting(const std::string& key, const std::string& defaultValue = "");
    bool storeTelemetryEvent(const std::string& eventType, const std::string& eventData);
    bool saveAgentState(const std::string& agentId, const std::string& stateData);
    std::string loadAgentState(const std::string& agentId);
    std::vector<std::vector<std::string>> executeDatabaseQuery(const std::string& sql);
    /// E07 — Append-only agentic approval / execution audit (SQLite `agentic_approval_audit`).
    bool recordAgenticApprovalAudit(const std::string& eventKind, const std::string& jsonPayload);

    // Telemetry Export
    bool exportTelemetryData(const std::string& format, const std::string& timeRange, const std::string& filename = "");
    std::vector<std::string> getTelemetryExportFormats();
    std::string getTelemetryExportDirectory();
    void handleTelemetryExportCommand();
    bool showTelemetryExportDialog();

    // Refactoring Plugin
    void initRefactoringPlugin();
    std::vector<std::string> getAvailableRefactorings(int startPos, int endPos);
    bool executeRefactoring(const std::string& operation, int startPos, int endPos, const std::string& param = "");
    bool applyRefactoring(const std::string& refactoredCode);
    std::string getEditorText() const;
    void handleRefactoringCommand();
    void showRefactoringMenu(int startPos, int endPos);

    // Language Plugin
    void initLanguagePlugin();
    void updateSyntaxHighlighting();
    void applySyntaxHighlighting(const std::vector<IDEPlugin::SyntaxToken>& tokens);
    void showCodeCompletions(int position);
    void updateDiagnostics();
    void clearDiagnostics();
    void addDiagnostic(const IDEPlugin::Diagnostic& diagnostic);
    bool initializeLanguageServer(const std::string& filename);
    std::vector<std::string> getSupportedLanguages() const;

    // Resource Generator
    std::vector<std::string> getAvailableResourceGenerators() const;
    bool generateResource(const std::string& type, const std::string& name, const std::string& outputPath,
                          const std::unordered_map<std::string, std::string>& parameters);
    void showResourceGeneratorDialog();

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

    // 51. mIRC Control Bridge
    void initIRCBridge();
    bool handleIRCBridgeCommand(int commandId);
    void cmdIRCConnect();
    void cmdIRCDisconnect();
    void cmdIRCStatus();
    void cmdIRCConfig();
    void cmdIRCSend();
    void dispatchIRCCommand(const std::string& nick,
                            const std::string& cmd,
                            const std::string& args,
                            const std::string& replyTarget,
                            bool isDirectMessage);

  public:
    // Tier 5 Command IDs (11500–11609)
    static constexpr int IDM_LINEENDING_DETECT = 11500;
    static constexpr int IDM_LINEENDING_TO_LF = 11509;

    static constexpr int IDM_NETWORK_SHOW = 11510;
    static constexpr int IDM_NETWORK_ADD_PORT = 11511;
    static constexpr int IDM_NETWORK_REMOVE_PORT = 11512;
    static constexpr int IDM_NETWORK_TOGGLE = 11513;
    static constexpr int IDM_NETWORK_LIST = 11514;
    static constexpr int IDM_NETWORK_STATUS = 11519;

    static constexpr int IDM_TESTEXPLORER_SHOW = 11520;
    static constexpr int IDM_TESTEXPLORER_RUN = 11521;
    static constexpr int IDM_TESTEXPLORER_REFRESH = 11522;
    static constexpr int IDM_TESTEXPLORER_FILTER = 11529;

    static constexpr int IDM_DBGWATCH_SHOW = 11530;
    static constexpr int IDM_DBGWATCH_CLEAR = 11539;

    static constexpr int IDM_CALLSTACK_CAPTURE = 11540;
    static constexpr int IDM_CALLSTACK_SHOW = 11541;
    static constexpr int IDM_CALLSTACK_COPY = 11542;
    static constexpr int IDM_CALLSTACK_RESOLVE = 11549;

    static constexpr int IDM_MARKETPLACE_SHOW = 11550;
    static constexpr int IDM_MARKETPLACE_SEARCH = 11551;
    static constexpr int IDM_MARKETPLACE_INSTALL = 11552;
    static constexpr int IDM_MARKETPLACE_UNINSTALL = 11553;
    static constexpr int IDM_MARKETPLACE_LIST = 11554;
    static constexpr int IDM_MARKETPLACE_STATUS = 11559;

    static constexpr int IDM_TELDASH_LOG = 11565;
    static constexpr int IDM_TELDASH_CLEAR = 11566;
    static constexpr int IDM_TELDASH_EXPORT = 11567;
    static constexpr int IDM_TELDASH_SHOW = 11568;  // avoid conflict with IDM_SEMANTIC_BUILD_INDEX 11560
    static constexpr int IDM_TELDASH_STATS = 11569;

    static constexpr int IDM_SHORTCUT_SHOW = 11570;
    static constexpr int IDM_SHORTCUT_RECORD = 11571;
    static constexpr int IDM_SHORTCUT_RESET = 11572;
    static constexpr int IDM_SHORTCUT_SAVE = 11573;
    static constexpr int IDM_SHORTCUT_LIST = 11579;

    static constexpr int IDM_COLORPICK_SCAN = 11580;
    static constexpr int IDM_COLORPICK_PICK = 11581;
    static constexpr int IDM_COLORPICK_INSERT = 11582;
    static constexpr int IDM_COLORPICK_LIST = 11589;

    static constexpr int IDM_EMOJI_PICKER = 11590;
    static constexpr int IDM_EMOJI_INSERT = 11591;
    static constexpr int IDM_EMOJI_CONFIG = 11592;
    static constexpr int IDM_EMOJI_TEST = 11599;

    static constexpr int IDM_CRASH_SHOW = 11600;
    static constexpr int IDM_CRASH_TEST = 11601;
    static constexpr int IDM_CRASH_LOG = 11602;
    static constexpr int IDM_CRASH_CLEAR = 11603;
    static constexpr int IDM_CRASH_STATS = 11609;

    // Phase 51: mIRC Control Bridge
    static constexpr int IDM_IRC_CONNECT    = 11610;
    static constexpr int IDM_IRC_DISCONNECT = 11611;
    static constexpr int IDM_IRC_STATUS     = 11612;
    static constexpr int IDM_IRC_CONFIG     = 11613;
    static constexpr int IDM_IRC_SEND       = 11614;

    bool m_telemetryDashboardInitialized = false;
    bool m_crashReporterInitialized = false;

    // Phase 51: mIRC Control Bridge
    bool m_ircBridgeInitialized = false;
    std::unique_ptr<RawrXD::IRC::IRCBridge> m_ircBridge;
    RawrXD::IRC::IRCBridgeSettings m_ircSettings;
    bool m_colorPickerInitialized = false;
    bool m_networkPanelInitialized = false;
    bool m_testExplorerInitialized = false;
    bool m_marketplaceInitialized = false;
    bool m_shortcutEditorInitialized = false;
    bool m_emojiSupportInitialized = false;

    // Caret Animation
    bool m_caretAnimationEnabled = false;
    bool m_caretBlinking = false;
    int m_caretBlinkRate = 500;
    UINT_PTR m_caretBlinkTimer = 0;

    // Agent Ollama Client
    bool m_ollamaClientInitialized = false;
    bool m_ollamaConnected = false;
    std::string m_ollamaEndpoint = "http://localhost:11434";
    std::string m_ollamaStatus = "Not connected";
    uint64_t m_ollamaLastConnectedMs = 0;

    // Model Discovery
    bool m_modelDiscoveryEnabled = false;
    std::vector<std::string> m_modelDiscoveryPaths;
    std::vector<std::string> m_modelPaths;

    // Enterprise Stress Tests
    bool m_stressTestsInitialized = false;
    bool m_stressTestRunning = false;
    std::unique_ptr<EnterpriseStressTester> m_enterpriseStressTester;
    StressTestResults m_stressTestResults;

    // SQLite3 Core
    std::unique_ptr<SQLite3DatabaseManager> m_sqliteManager;

    // Refactoring Plugin
    std::unique_ptr<RefactoringPluginManager> m_refactoringManager;

    // Language Plugin
    std::unique_ptr<LanguagePluginManager> m_languageManager;

    // Resource Generator
    std::unique_ptr<ResourceGeneratorManager> m_resourceManager;

    // Telemetry Export
    std::unique_ptr<TelemetryExportManager> m_telemetryExporter;
    bool m_coreRuntimeSpineInitialized = false;

    // Feature Registry / License Creator
    HWND m_hwndFeatureRegistryHost = nullptr;
    std::unique_ptr<FeatureRegistryPanel> m_featureRegistryPanel;
    static LRESULT CALLBACK LicenseCreatorWndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK FeatureRegistryHostProc(HWND, UINT, WPARAM, LPARAM);

  private:
};

// Global IDE instance for C-callable bridges (agent streaming, etc.). Set in initProblemsPanel().
extern Win32IDE* g_pMainIDE;
