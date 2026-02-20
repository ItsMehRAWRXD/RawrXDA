#pragma once

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
#include "gguf_loader.h"
#include "streaming_gguf_loader.h"
#include "Win32IDE_AgenticBridge.h"
#include "Win32IDE_Autonomy.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"

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
#include "Win32TerminalManager.h"
#include "TransparentRenderer.h"
#include "gguf_loader.h"
#include "streaming_gguf_loader.h"

// Theme and customization structures
struct IDETheme {
    COLORREF backgroundColor;
    COLORREF textColor;
    COLORREF keywordColor;
    COLORREF commentColor;
    COLORREF stringColor;
    COLORREF numberColor;
    COLORREF operatorColor;
    COLORREF selectionColor;
    COLORREF lineNumberColor;
    std::string fontName;
    int fontSize;
    bool darkMode;
};

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

class Win32IDE
{
public:
    enum class OutputSeverity {
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3
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

    // Agentic Framework
    std::unique_ptr<AgenticBridge> m_agenticBridge;
    void initializeAgenticBridge();
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
    std::string getPowerShellEdition(); // Core or Desktop
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
    void showCommitDialog(); // simple commit message dialog (Ctrl+Shift+C)
    
    // Menu Command System (25 features)
    void handleFileCommand(int commandId);
    void handleEditCommand(int commandId);
    void handleViewCommand(int commandId);
    void handleTerminalCommand(int commandId);
    void handleToolsCommand(int commandId);
    void handleModulesCommand(int commandId);
    void handleHelpCommand(int commandId);
    void handleAgentCommand(int commandId); // Added Agent handler
    
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
    void registerCommandHandler(int commandId, std::function<void()> handler);
    std::string getCommandDescription(int commandId) const;
    bool isCommandEnabled(int commandId) const;
    void updateCommandStates();
    void updateMenuEnableStates(); // dynamic enable/disable for Git & terminal items

    // Theme and customization
    void loadTheme(const std::string& themeName);
    void saveTheme(const std::string& themeName);
    void applyTheme();
    void showThemeEditor();
    void resetToDefaultTheme();

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
    // Command palette UI handles and data
    HWND m_hwndCommandPalette;
    HWND m_hwndCommandPaletteInput;
    HWND m_hwndCommandPaletteList;
    bool m_commandPaletteVisible;
    std::vector<CommandPaletteItem> m_commandRegistry;
    std::vector<CommandPaletteItem> m_filteredCommands;
    std::vector<int> m_commandPaletteFilteredIndices; // maps listbox index -> registry index
    std::unordered_map<int, std::function<void()>> m_commandHandlers;

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
    void toggleSidebar(); // Ctrl+B
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
    void syncRepository(); // push & pull
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
    std::unique_ptr<IGGUFLoader> m_ggufLoader;
    std::string m_loadedModelPath;
    GGUFMetadata m_currentModelMetadata;
    std::vector<TensorInfo> m_modelTensors;
    bool m_useStreamingLoader; // preference to use streaming loader to minimize memory
    bool m_useVulkanRenderer; // preference to use Vulkan renderer if enabled
    
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
    std::mutex m_outputMutex;
    
    // Window Procedures for Subclassing
    static LRESULT CALLBACK CommandInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
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
    std::map<std::string, IDETheme> m_themes;
    HBRUSH m_backgroundBrush;
    HFONT m_editorFont;

    // Code snippets
    std::vector<CodeSnippet> m_codeSnippets;
    std::unordered_map<std::string, size_t> m_snippetTriggers;

    // Output tabs
    std::map<std::string, HWND> m_outputWindows;
    std::string m_activeOutputTab;
    bool m_outputPanelVisible;
    int m_selectedOutputTab;
    HWND m_hwndSeverityFilter;
    int m_severityFilterLevel; // 0=All, 1=Info+, 2=Warn+, 3=Error only

    // Clipboard history
    std::vector<std::string> m_clipboardHistory;
    static const size_t MAX_CLIPBOARD_HISTORY = 50;

    // Minimap
    bool m_minimapVisible;
    int m_minimapWidth;
    std::vector<std::string> m_minimapLines;
    std::vector<int> m_minimapLineStarts;

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
    int m_outputTabHeight;
    RECT m_editorRect;
    bool m_gpuTextEnabled;
    bool m_editorHooksInstalled;
    
    // Splitter bar for terminal/output resize
    HWND m_hwndSplitter;
    bool m_splitterDragging;
    int m_splitterY;
    
    std::unique_ptr<IRenderer> m_renderer;
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
    // m_breakpoints defined in Debugger State section with Breakpoint struct
    
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
    enum class PanelTab {
        Terminal = 0,
        Output = 1,
        Problems = 2,
        DebugConsole = 3
    };
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
    
    void toggleSecondarySidebar();      // Ctrl+Alt+B
    void togglePanel();                  // Ctrl+J
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
    HWND m_hwndPowerShellPanel;           // Main PowerShell panel container
    HWND m_hwndPowerShellOutput;          // PowerShell output/console area
    HWND m_hwndPowerShellInput;           // PowerShell command input
    HWND m_hwndPowerShellToolbar;         // PowerShell panel toolbar
    HWND m_hwndPowerShellStatusBar;       // PowerShell status (version, execution policy)
    
    // PowerShell Panel Buttons
    HWND m_hwndPSBtnExecute;              // Execute button
    HWND m_hwndPSBtnClear;                // Clear console
    HWND m_hwndPSBtnStop;                 // Stop execution
    HWND m_hwndPSBtnHistory;              // Command history
    HWND m_hwndPSBtnRestart;              // Restart PowerShell session
    HWND m_hwndPSBtnLoadRawrXD;           // Load RawrXD.ps1 module
    HWND m_hwndPSBtnToggle;               // Toggle panel visibility
    
    // PowerShell Panel State
    bool m_powerShellPanelVisible;
    bool m_powerShellPanelDocked;         // true=docked, false=floating
    bool m_powerShellSessionActive;
    bool m_powerShellRawrXDLoaded;
    int m_powerShellPanelHeight;          // Height when docked
    int m_powerShellPanelWidth;           // Width when floating
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
    void togglePowerShellPanel();         // Ctrl+`
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
    void quickLoadGGUFModel();            // Quick model loader dialog
    void quickInference();                 // Quick inference dialog
    
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
};
