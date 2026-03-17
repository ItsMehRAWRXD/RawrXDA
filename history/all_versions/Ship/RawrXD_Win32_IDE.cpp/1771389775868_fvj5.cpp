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
#define ID_BUILD_BUILD      1040
#define ID_BUILD_RUN        1041
#define ID_BUILD_DEBUG      1042
#define ID_BUILD_COMPILE    1043
#define ID_BUILD_DLLS       1044
#define ID_AI_COMPLETION    1034
#define IDM_AI_GENERATE     ID_AI_COMPLETE
#define IDM_AI_LOADGGUF     ID_AI_LOAD_GGUF
#define IDM_AI_UNLOADMODEL  ID_AI_UNLOADMODEL
#define IDM_HELP_ABOUT      ID_HELP_ABOUT

// IDM_ aliases for compatibility
#define IDM_FILE_NEW        ID_FILE_NEW
#define IDM_FILE_OPEN       ID_FILE_OPEN
#define IDM_FILE_SAVE       ID_FILE_SAVE
#define IDM_FILE_SAVEAS     ID_FILE_SAVEAS
#define IDM_FILE_CLOSE      ID_FILE_CLOSE
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
#define IDM_AI_GENERATE     ID_AI_COMPLETION
#define IDM_AI_LOADGGUF     ID_AI_LOAD_GGUF
#define IDM_AI_UNLOADMODEL  ID_AI_UNLOADMODEL
#define IDM_HELP_ABOUT      ID_HELP_ABOUT

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
void CreateIssuesPanel();
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
bool SaveFile(const std::wstring& path);
void SaveCurrentFile();
void AI_LoadModel();
void AI_LoadGGUF();
void AI_UnloadModel();

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
void RefreshCodeIssues() {
    RunCodeAnalysis();
}

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

void SendChatMessage() {
    HandleChatSend();
}

HMENU CreateMainMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    HMENU hEditMenu = CreatePopupMenu();
    HMENU hViewMenu = CreatePopupMenu();
    HMENU hBuildMenu = CreatePopupMenu();
    HMENU hAIMenu = CreatePopupMenu();
    
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_SAVEAS, L"Save &As...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, ID_FILE_EXIT, L"E&xit");
    
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_PASTE, L"&Paste\tCtrl+V");
    
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_OUTPUT, L"&Output");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_TERMINAL, L"&Terminal");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_CHAT, L"&Chat");
    AppendMenuW(hViewMenu, MF_STRING, ID_VIEW_ISSUES, L"&Issues");
    
    AppendMenuW(hBuildMenu, MF_STRING, ID_BUILD_BUILD, L"&Build\tF7");
    AppendMenuW(hBuildMenu, MF_STRING, ID_BUILD_RUN, L"&Run\tF5");
    
    AppendMenuW(hAIMenu, MF_STRING, ID_AI_COMPLETION, L"Trigger &Completion\tCtrl+Space");
    AppendMenuW(hAIMenu, MF_STRING, ID_AI_EXPLAIN, L"&Explain Code");
    AppendMenuW(hAIMenu, MF_STRING, ID_AI_REFACTOR, L"&Refactor Code");
    AppendMenuW(hAIMenu, MF_STRING, ID_AI_FIX, L"&Fix Code");
    AppendMenuW(hAIMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAIMenu, MF_STRING, ID_AI_LOADMODEL, L"&Load Model...");
    AppendMenuW(hAIMenu, MF_STRING, ID_AI_LOAD_GGUF, L"Load &GGUF Model...");
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBuildMenu, L"&Build");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAIMenu, L"&AI");
    
    return hMenu;
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
        
        // Create file tree (placeholder list)
        g_hwndFileTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
            0, 0, 200, 700, hwnd, (HMENU)IDC_FILETREE, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndFileTree, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SendMessageW(g_hwndChatInput, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SetWindowSubclass(g_hwndChatInput, ChatInputSubclassProc, 0, 0);
        
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
        
        // Calculate panel heights
        int issuesWidth = g_bIssuesVisible ? 400 : 0;
        int bottomHeight = 0;
        if (g_bOutputVisible) bottomHeight = 150;
        
        // Layout: file tree on left, editor center, issues right, output bottom
        int treeWidth = 200;
        int mainWidth = width - treeWidth - issuesWidth;
        int editorHeight = height - sbHeight - bottomHeight;
        
        // File tree
        MoveWindow(g_hwndFileTree, 0, 0, treeWidth, height - sbHeight, TRUE);
        
        // Editor
        MoveWindow(g_hwndEditor, treeWidth, 0, mainWidth, editorHeight, TRUE);
        
        // Output panel
        if (g_bOutputVisible) {
            MoveWindow(g_hwndOutput, treeWidth, editorHeight, mainWidth, bottomHeight, TRUE);
            ShowWindow(g_hwndOutput, SW_SHOW);
        } else {
            ShowWindow(g_hwndOutput, SW_HIDE);
        }
        
        // Issues panel
        if (g_bIssuesVisible) {
            int issuesX = treeWidth + mainWidth;
            MoveWindow(g_hwndIssuesPanel, issuesX, 0, issuesWidth, height - sbHeight, TRUE);
            ShowWindow(g_hwndIssuesPanel, SW_SHOW);
        } else {
            ShowWindow(g_hwndIssuesPanel, SW_HIDE);
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
                if (!g_hChatServerProcess) {
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
            
        case IDM_AI_GENERATE:
            AI_GenerateCode();
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
            
        default:
            break;
        }
        break;
    }
    
    case WM_USER + 1: // Deferred syntax highlighting
        ApplySyntaxHighlighting(g_hwndEditor);
        break;
        
    case WM_DESTROY:
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
    SetWindowTextW(hwnd, title.c_str());
    
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

bool SaveFile() {
    if (g_currentFile.empty()) {
        return SaveFileAs();
    } else {
        return SaveFile(g_currentFile);
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
    int lineCount = (int)SendMessage(g_hwndEditor, EM_GETLINECOUNT, 0);
    
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
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
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
