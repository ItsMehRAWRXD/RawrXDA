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
#include <algorithm>

#include <process.h>

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
// DLL Function Pointer Management - Consolidated
// ============================================================================
struct DLLFunctions {
    HMODULE hTitan = nullptr;
    HMODULE hInference = nullptr;
    HMODULE hBridge = nullptr;
    
    Titan_Initialize_t pTitan_Init = nullptr;
    Titan_LoadModelPersistent_t pTitan_Load = nullptr;
    Titan_RunInference_t pTitan_Run = nullptr;
    Titan_Shutdown_t pTitan_Shutdown = nullptr;
    
    LoadModel_t pLoadModel = nullptr;
    UnloadModel_t pUnloadModel = nullptr;
    ForwardPassInfer_t pForwardPass = nullptr;
    SampleNext_t pSampleNext = nullptr;
    
    LoadModelNative_t pLoadNative = nullptr;
    GetTokenEmbedding_t pGetEmbed = nullptr;
    ForwardPassASM_t pForwardASM = nullptr;
    CleanupMathTables_t pCleanup = nullptr;
};

static DLLFunctions g_dll;

// ============================================================================
// Window Handle Management - Consolidated
// ============================================================================
struct WindowHandles {
    HWND main = nullptr;
    HWND editor = nullptr;
    HWND output = nullptr;
    HWND fileTree = nullptr;
    HWND toolbar = nullptr;
    HWND statusBar = nullptr;
    HWND tabControl = nullptr;
    HWND chatInput = nullptr;
    HWND chatHistory = nullptr;
    HWND terminal = nullptr;
    HWND issuesPanel = nullptr;
    HWND issuesList = nullptr;
    HWND issuesCritical = nullptr;
    HWND issuesHigh = nullptr;
    HWND issuesMedium = nullptr;
    HWND issuesLow = nullptr;
    HWND issuesInfo = nullptr;
    HWND chatNew = nullptr;
    HWND chatHistoryBtn = nullptr;
    HWND chatExport = nullptr;
    HWND chatConfig = nullptr;
    HWND chatMCP = nullptr;
    HWND chatLogs = nullptr;
};

static WindowHandles g_hwnd;

static HFONT g_hFontCode = nullptr;
static HFONT g_hFontUI = nullptr;

static std::wstring g_currentFile;
static bool g_isDirty = false;

// Chat server process handle
static HANDLE g_hChatServerProcess = nullptr;
static std::wstring g_workspaceRoot;

// Forward declarations
void AppendWindowText(HWND hwnd, const wchar_t* text);
void HandleChatSend();
void SendChatToServer(const std::wstring& message);
bool StartChatServer();
void StopChatServer();
void ShowAICompletion(const AICompletion& completion);
void HideAICompletion();
void AcceptAICompletion();
void CleanupHTTPClient();
void InitializeHTTPClient();
LRESULT CALLBACK ChatInputSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

// Multi-tab editor support
struct EditorTab {
    HWND hwndEdit;
    std::wstring filePath;
    bool isDirty;
};
static std::vector<EditorTab> g_tabs;
static int g_activeTab = -1;

// ============================================================================
// UI State Management - Consolidated
// ============================================================================
struct UIState {
    bool terminalVisible = false;
    bool outputVisible = true;
    bool chatVisible = false;
    bool issuesVisible = false;
    bool abortInference = false;
    bool showCritical = true;
    bool showHigh = true;
    bool showMedium = true;
    bool showLow = true;
    bool showInfo = true;
};

static UIState g_ui;

// Syntax highlighting colors
static COLORREF g_colorKeyword = RGB(86, 156, 214);    // Blue
static COLORREF g_colorString = RGB(214, 157, 133);    // Orange
static COLORREF g_colorComment = RGB(106, 153, 85);    // Green
static COLORREF g_colorNumber = RGB(181, 206, 168);    // Light green
static COLORREF g_colorPreproc = RGB(155, 155, 155);   // Gray
static COLORREF g_colorDefault = RGB(212, 212, 212);   // White
static COLORREF g_colorBackground = RGB(30, 30, 30);   // Dark

// ============================================================================
// Code Issues System - VS Code/GitHub Copilot Style
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

static std::vector<CodeIssue> g_codeIssues;
static std::mutex g_issuesMutex;

// Forward declarations for issues system
void RefreshCodeIssues();
void AddCodeIssue(const CodeIssue& issue);
void ClearCodeIssues();
void ExportIssues();
void FilterIssuesBySeverity();
void JumpToIssue(int issueIndex);
void ApplyQuickFix(int issueIndex);
void RunCodeAnalysis();
void ShowIssuesContextMenu(POINT pt);

// Chat system enhancements
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

// Forward declarations for enhanced chat
void CreateNewChatSession();
void LoadChatHistory();
void SaveChatSession();
void ExportChatHistory();
void ConfigureMCPServers();
void ShowChatLogs();
void SwitchChatSession(int sessionIndex);

struct AICompletion {
    std::wstring text;           // The completion text
    std::wstring detail;         // Additional context/description
    double confidence;           // 0.0 - 1.0
    std::wstring kind;           // "function", "method", "variable", etc.
    int cursorOffset;            // Where to place cursor after insertion
    bool isMultiLine;            // True if spans multiple lines
};

static bool g_completionVisible = false;
static std::wstring g_completionText;
static int g_completionAnchor = -1;
static COLORREF g_colorCompletionGhost = RGB(100, 100, 100);


    
    if (!g_hwndMain) {
        MessageBox(nullptr, L"Failed to create main window", L"Error", MB_ICONERROR);
        return 1;
    }
    
    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}===================
// Control IDs
// ===============================================================================================================
// Control IDs
// ============================================================================
#define IDC_EDITOR      1001
#define IDC_OUTPUT      1002
#define IDC_FILETREE    1003
#define IDC_TOOLBAR     1004
#define IDC_STATUSBAR   1005
#define IDC_TERMINAL    1006
#define IDC_CHATINPUT   1007
#define IDC_CHATHISTORY 1008
#define IDC_CHAT_GENERATE 1009
#define IDC_CHAT_EXPLAIN  1010
#define IDC_CHAT_REFACTOR 1011
#define IDC_CHAT_FIX      1015
#define IDC_CHAT_SEND     1012
#define IDC_CHAT_CLEAR    1013
#define IDC_CHAT_STOP     1014

// Code Issues Panel Controls
#define IDC_ISSUES_PANEL     1020
#define IDC_ISSUES_LIST      1021
#define IDC_ISSUES_FILTER    1022
#define IDC_ISSUES_CRITICAL  1023
#define IDC_ISSUES_HIGH      1024
#define IDC_ISSUES_MEDIUM    1025
#define IDC_ISSUES_LOW       1026
#define IDC_ISSUES_INFO      1027
#define IDC_ISSUES_REFRESH   1028
#define IDC_ISSUES_EXPORT    1029
#define IDC_ISSUES_CLEAR     1030
#define IDC_ISSUES_SETTINGS  1031

// Chat Panel Extended Controls
#define IDC_CHAT_NEW         1032
#define IDC_CHAT_HISTORY_BTN 1033
#define IDC_CHAT_EXPORT      1034
#define IDC_CHAT_CONFIG      1035
#define IDC_CHAT_MCP         1036
#define IDC_CHAT_LOGS        1037

#define IDM_FILE_NEW    2001
#define IDM_FILE_OPEN   2002
#define IDM_FILE_SAVE   2003
#define IDM_FILE_SAVEAS 2004
#define IDM_FILE_EXIT   2005

#define IDM_EDIT_UNDO   2101
#define IDM_EDIT_REDO   2102
#define IDM_EDIT_CUT    2103
#define IDM_EDIT_COPY   2104
#define IDM_EDIT_PASTE  2105
#define IDM_EDIT_FIND   2106

#define IDM_BUILD_RUN   2201
#define IDM_BUILD_DEBUG 2202
#define IDM_BUILD_BUILD 2203

#define IDM_AI_GENERATE 2301
#define IDM_AI_EXPLAIN  2302
#define IDM_AI_REFACTOR 2303
#define IDM_AI_LOADMODEL 2304
#define IDM_AI_LOADGGUF  2305
#define IDM_AI_UNLOADMODEL 2306

#define IDM_HELP_ABOUT  2401

#define IDM_VIEW_TERMINAL 2501
#define IDM_VIEW_CHAT     2502
#define IDM_VIEW_OUTPUT   2503
#define IDM_VIEW_ISSUES   2504

#include <unordered_set>

// C/C++ keywords for syntax highlighting - lazy initialized for better startup performance
static std::unordered_set<std::wstring> g_cppKeywords;

void InitializeCppKeywords() {
    if (g_cppKeywords.empty()) {
        g_cppKeywords = {
            L"auto", L"break", L"case", L"catch", L"char", L"class", L"const", L"continue",
            L"default", L"delete", L"do", L"double", L"else", L"enum", L"extern", L"false",
            L"float", L"for", L"friend", L"goto", L"if", L"inline", L"int", L"long",
            L"namespace", L"new", L"nullptr", L"operator", L"private", L"protected", L"public",
            L"register", L"return", L"short", L"signed", L"sizeof", L"static", L"struct",
            L"switch", L"template", L"this", L"throw", L"true", L"try", L"typedef", L"typename",
            L"union", L"unsigned", L"using", L"virtual", L"void", L"volatile", L"while",
            L"bool", L"constexpr", L"override", L"final", L"noexcept", L"decltype", L"thread_local",
            L"alignas", L"alignof", L"char16_t", L"char32_t", L"static_assert", L"static_cast",
            L"dynamic_cast", L"reinterpret_cast", L"const_cast", L"explicit", L"export", L"mutable",
            L"#include", L"#define", L"#ifdef", L"#ifndef", L"#endif", L"#pragma", L"#if", L"#else"
        };
    }
}

// ============================================================================
// Optimized Syntax Highlighting with caching
// ============================================================================
static std::wstring g_lastHighlightedText;
static bool g_highlightingInProgress = false;

inline bool IsKeyword(const std::wstring& word) {
    InitializeCppKeywords();
    return g_cppKeywords.find(word) != g_cppKeywords.end();
}

void ApplySyntaxHighlighting(HWND hwndEdit) {
    if (g_highlightingInProgress) return;
    g_highlightingInProgress = true;
    
    // Get text content with size check
    int len = GetWindowTextLengthW(hwndEdit);
    if (len <= 0 || len > 100000) { // Skip very large files for performance
        g_highlightingInProgress = false;
        return;
    }
    
    std::wstring text;
    text.resize(len);
    GetWindowTextW(hwndEdit, &text[0], len + 1);
    
    // Skip if text hasn't changed
    if (text == g_lastHighlightedText) {
        g_highlightingInProgress = false;
        return;
    }
    g_lastHighlightedText = text;
    
    // Set background color once
    SendMessage(hwndEdit, EM_SETBKGNDCOLOR, 0, g_colorBackground);
    
    // Apply default color to all
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = g_colorDefault;
    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
    SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // Optimized parsing with reduced string operations
    const wchar_t* ptr = text.c_str();
    const wchar_t* end = ptr + text.length();
    
    while (ptr < end) {
        // Skip whitespace
        while (ptr < end && iswspace(*ptr)) ptr++;
        if (ptr >= end) break;
        
        const wchar_t* start = ptr;
        
        // Check for comment
        if (ptr + 1 < end && *ptr == L'/' && *(ptr+1) == L'/') {
            while (ptr < end && *ptr != L'\n') ptr++;
            cf.crTextColor = g_colorComment;
            SendMessage(hwndEdit, EM_SETSEL, start - text.c_str(), ptr - text.c_str());
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for string
        if (*ptr == L'"' || *ptr == L'\'') {
            wchar_t quote = *ptr++;
            while (ptr < end && *ptr != quote) {
                if (*ptr == L'\\' && ptr + 1 < end) ptr++;
                ptr++;
            }
            if (ptr < end) ptr++;
            cf.crTextColor = g_colorString;
            SendMessage(hwndEdit, EM_SETSEL, start - text.c_str(), ptr - text.c_str());
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for preprocessor
        if (*ptr == L'#') {
            while (ptr < end && (iswalnum(*ptr) || *ptr == L'#' || *ptr == L'_')) ptr++;
            cf.crTextColor = g_colorPreproc;
            SendMessage(hwndEdit, EM_SETSEL, start - text.c_str(), ptr - text.c_str());
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for number
        if (iswdigit(*ptr)) {
            while (ptr < end && (iswalnum(*ptr) || *ptr == L'.' || *ptr == L'x')) ptr++;
            cf.crTextColor = g_colorNumber;
            SendMessage(hwndEdit, EM_SETSEL, start - text.c_str(), ptr - text.c_str());
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for identifier/keyword
        if (iswalpha(*ptr) || *ptr == L'_') {
            while (ptr < end && (iswalnum(*ptr) || *ptr == L'_')) ptr++;
            std::wstring word(start, ptr - start);
            if (IsKeyword(word)) {
                cf.crTextColor = g_colorKeyword;
                SendMessage(hwndEdit, EM_SETSEL, start - text.c_str(), ptr - text.c_str());
                SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
            continue;
        }
        
        ptr++;
    }
    
    // Reset selection
    SendMessage(hwndEdit, EM_SETSEL, 0, 0);
    g_highlightingInProgress = false;
}

// ============================================================================
// Forward declarations for utility functions used below
// ============================================================================
std::string WideToUtf8(const std::wstring& wstr);
std::wstring Utf8ToWide(const std::string& str);

// ============================================================================
// Optimized Terminal with buffered I/O
// ============================================================================
static HANDLE g_hCmdProcess = nullptr;
static HANDLE g_hCmdStdoutRead = nullptr;
static HANDLE g_hCmdStdinWrite = nullptr;
static std::string g_terminalBuffer;
static CRITICAL_SECTION g_terminalCS;
static bool g_terminalCSInitialized = false;

bool StartTerminal() {
    if (!g_terminalCSInitialized) {
        InitializeCriticalSection(&g_terminalCS);
        g_terminalCSInitialized = true;
    }
    
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hStdoutWrite, hStdinRead;
    
    if (!CreatePipe(&g_hCmdStdoutRead, &hStdoutWrite, &sa, 8192)) return false;
    if (!CreatePipe(&hStdinRead, &g_hCmdStdinWrite, &sa, 4096)) {
        CloseHandle(g_hCmdStdoutRead);
        CloseHandle(hStdoutWrite);
        return false;
    }
    
    SetHandleInformation(g_hCmdStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(g_hCmdStdinWrite, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.hStdInput = hStdinRead;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    wchar_t cmd[] = L"powershell.exe -NoLogo -NoProfile";
    
    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(g_hCmdStdoutRead);
        CloseHandle(g_hCmdStdinWrite);
        CloseHandle(hStdoutWrite);
        CloseHandle(hStdinRead);
        g_hCmdStdoutRead = nullptr;
        g_hCmdStdinWrite = nullptr;
        return false;
    }
    
    g_hCmdProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);
    return true;
}

void SendTerminalCommand(const std::wstring& cmd) {
    if (!g_hCmdStdinWrite) return;
    std::string utf8 = WideToUtf8(cmd + L"\r\n");
    DWORD written;
    if (!WriteFile(g_hCmdStdinWrite, utf8.c_str(), (DWORD)utf8.length(), &written, nullptr)) {
        // Handle write error
        CloseHandle(g_hCmdStdinWrite);
        g_hCmdStdinWrite = nullptr;
    }
}

std::wstring ReadTerminalOutput() {
    if (!g_hCmdStdoutRead) return L"";
    
    EnterCriticalSection(&g_terminalCS);
    
    char buffer[8192];
    DWORD available = 0, bytesRead = 0;
    
    if (PeekNamedPipe(g_hCmdStdoutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
        DWORD toRead = (available < sizeof(buffer) - 1) ? available : (DWORD)(sizeof(buffer) - 1);
        if (ReadFile(g_hCmdStdoutRead, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            g_terminalBuffer.append(buffer, bytesRead);
        }
    }
    
    std::wstring result = Utf8ToWide(g_terminalBuffer);
    g_terminalBuffer.clear();
    
    LeaveCriticalSection(&g_terminalCS);
    return result;
}

// ============================================================================
// Optimized DLL Management with lazy loading and caching
// ============================================================================
static bool g_dllsInitialized = false;
static std::wstring g_exeDirectory;

const std::wstring& GetExeDirectory() {
    if (g_exeDirectory.empty()) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        PathRemoveFileSpecW(path);
        g_exeDirectory = path;
    }
    return g_exeDirectory;
}

bool LoadTitanKernel() {
    if (g_hTitanDll) return true;
    
    // Try current directory first, then exe directory
    g_hTitanDll = LoadLibraryW(L"RawrXD_Titan_Kernel.dll");
    if (!g_hTitanDll) {
        std::wstring dllPath = GetExeDirectory() + L"\\RawrXD_Titan_Kernel.dll";
        g_hTitanDll = LoadLibraryW(dllPath.c_str());
    }
    
    if (g_hTitanDll) {
        pTitan_Initialize = (Titan_Initialize_t)GetProcAddress(g_hTitanDll, "Titan_Initialize");
        pTitan_LoadModel = (Titan_LoadModelPersistent_t)GetProcAddress(g_hTitanDll, "Titan_LoadModelPersistent");
        pTitan_RunInference = (Titan_RunInference_t)GetProcAddress(g_hTitanDll, "Titan_RunInference");
        pTitan_Shutdown = (Titan_Shutdown_t)GetProcAddress(g_hTitanDll, "Titan_Shutdown");
        
        if (!pTitan_Initialize || !pTitan_LoadModel || !pTitan_RunInference || !pTitan_Shutdown) {
            FreeLibrary(g_hTitanDll);
            g_hTitanDll = nullptr;
            return false;
        }
    }
    return g_hTitanDll != nullptr;
}

bool LoadModelBridge() {
    if (g_hModelBridgeDll) return true;
    
    g_hModelBridgeDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    if (!g_hModelBridgeDll) {
        std::wstring dllPath = GetExeDirectory() + L"\\RawrXD_NativeModelBridge.dll";
        g_hModelBridgeDll = LoadLibraryW(dllPath.c_str());
    }
        
    if (g_hModelBridgeDll) {
        pLoadModelNative = (LoadModelNative_t)GetProcAddress(g_hModelBridgeDll, "LoadModelNative");
        pGetTokenEmbedding = (GetTokenEmbedding_t)GetProcAddress(g_hModelBridgeDll, "GetTokenEmbedding");
        pForwardPassASM = (ForwardPassASM_t)GetProcAddress(g_hModelBridgeDll, "ForwardPass");
        pCleanupMathTables = (CleanupMathTables_t)GetProcAddress(g_hModelBridgeDll, "CleanupMathTables");
        
        if (!pLoadModelNative || !pGetTokenEmbedding || !pForwardPassASM || !pCleanupMathTables) {
            FreeLibrary(g_hModelBridgeDll);
            g_hModelBridgeDll = nullptr;
            return false;
        }
    }
    return g_hModelBridgeDll != nullptr && pLoadModelNative != nullptr;
}

bool LoadInferenceEngine() {
    if (g_hInferenceEngine) return true;
    
    // Try Win32-specific version first
    g_hInferenceEngine = LoadLibraryW(L"RawrXD_InferenceEngine_Win32.dll");
    if (!g_hInferenceEngine) {
        g_hInferenceEngine = LoadLibraryW(L"RawrXD_InferenceEngine.dll");
    }
    if (!g_hInferenceEngine) {
        std::wstring dllPath = GetExeDirectory() + L"\\RawrXD_InferenceEngine.dll";
        g_hInferenceEngine = LoadLibraryW(dllPath.c_str());
    }
    
    if (g_hInferenceEngine) {
        pLoadModel = (LoadModel_t)GetProcAddress(g_hInferenceEngine, "LoadModel");
        pUnloadModel = (UnloadModel_t)GetProcAddress(g_hInferenceEngine, "UnloadModel");
        pForwardPassInfer = (ForwardPassInfer_t)GetProcAddress(g_hInferenceEngine, "ForwardPass");
        pSampleNext = (SampleNext_t)GetProcAddress(g_hInferenceEngine, "SampleNext");
        
        if (!pLoadModel || !pUnloadModel || !pForwardPassInfer || !pSampleNext) {
            FreeLibrary(g_hInferenceEngine);
            g_hInferenceEngine = nullptr;
            return false;
        }
    }
    return g_hInferenceEngine != nullptr && pLoadModel != nullptr;
}

bool LoadModelWithInferenceEngine(const std::wstring& filepath) {
    if (!g_hInferenceEngine || !pLoadModel) {
        if (!LoadInferenceEngine()) return false;
    }
    
    if (g_modelContext && pUnloadModel) {
        pUnloadModel(g_modelContext);
        g_modelContext = nullptr;
    }
    
    g_modelContext = pLoadModel(filepath.c_str());
    if (g_modelContext) {
        g_modelLoaded = true;
        g_loadedModelPath = filepath;
        return true;
    }
    return false;
}

void UnloadInferenceModel() {
    if (g_modelContext && pUnloadModel) {
        pUnloadModel(g_modelContext);
        g_modelContext = nullptr;
    }
    g_modelLoaded = false;
    g_loadedModelPath.clear();
}

// ============================================================================
// Optimized String Conversion with caching
// ======================================== and thread safety
// ============================================================================
static thread_local std::map<std::wstring, std::string> g_wideToUtf8Cache;
static thread_local std::map<std::string, std::wstring> g_utf8ToWideCache;
static const size_t MAX_CACHE_SIZE = 100;

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    
    // Check cache first
    auto it = g_wideToUtf8Cache.find(wstr);
    if (it != g_wideToUtf8Cache.end()) {
        return it->second;
    }
    
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    
    std::string str(size - 1, '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr) == 0) {
        return "";
    }
    
    // Cache result if not too large
    if (g_wideToUtf8Cache.size() < MAX_CACHE_SIZE) {
        g_wideToUtf8Cache[wstr] = str;
    }
    
    return str;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    
    // Check cache first
    auto it = g_utf8ToWideCache.find(str);
    if (it != g_utf8ToWideCache.end()) {
        return it->second;
    }
    
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    
    std::wstring wstr(size - 1, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size) == 0) {
        return L"";
    }
    
    // Cache result if not too large
    if (g_utf8ToWideCache.size() < MAX_CACHE_SIZE) {
        g_utf8ToWideCache[str] = wstr;
    }
    
    return wstr;
}

// Forward declaration
void AppendWindowText(HWND hwnd, const wchar_t* text);

// ============================================================================
// Optimized File operations with better error handling
// ============================================================================
bool LoadFile(const std::wstring& path) {
    std::string utf8Path = WideToUtf8(path);
    std::ifstream f(utf8Path, std::ios::binary);
    if (!f.is_open()) {
        MessageBoxW(g_hwndMain, L"Failed to open file for reading.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    // Get file size for efficient allocation
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    
    if (size > 50 * 1024 * 1024) { // 50MB limit
        MessageBoxW(g_hwndMain, L"File too large (>50MB)", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    std::string content;
    content.reserve(size);
    content.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    
    SetWindowTextA(g_hwndEditor, content.c_str());
    g_currentFile = path;
    g_isDirty = false;
    
    // Update title
    std::wstring title = L"RawrXD IDE - " + path;
    SetWindowTextW(g_hwndMain, title.c_str());
    
    // Update status
    std::wstring status = L"Loaded: " + path + L" (" + std::to_wstring(size) + L" bytes)";
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    return true;
}

bool SaveFile(const std::wstring& path) {
    int len = GetWindowTextLengthA(g_hwndEditor);
    if (len <= 0) return false;
    
    std::string content;
    content.resize(len);
    GetWindowTextA(g_hwndEditor, &content[0], len + 1);
    
    std::string utf8Path = WideToUtf8(path);
    std::ofstream f(utf8Path, std::ios::binary);
    if (!f.is_open()) {
        MessageBoxW(g_hwndMain, L"Failed to open file for writing.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    f.write(content.c_str(), content.size());
    if (f.fail()) {
        MessageBoxW(g_hwndMain, L"Failed to write to file.", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    g_currentFile = path;
    g_isDirty = false;
    
    std::wstring status = L"Saved: " + path + L" (" + std::to_wstring(content.size()) + L" bytes)";
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    return true;
}

// ============================================================================
// Open/Save dialogs
// ============================================================================
std::wstring OpenFileDialog(HWND hwnd) {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"All Files\0*.*\0C/C++ Files\0*.c;*.cpp;*.h;*.hpp\0Python Files\0*.py\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) return filename;
    return L"";
}

std::wstring SaveFileDialog(HWND hwnd) {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) return filename;
    return L"";
}

// ============================================================================
// Optimized AI Actions with reduced allocations
// ============================================================================
void AI_GenerateCode() {
    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
        // Use loaded model for real inference
        AppendWindowText(g_hwndOutput, L"[AI] Generating code with loaded model...\r\n");
        
        // Get context: last 256 chars before cursor
        int contextStart = (start > 256) ? start - 256 : 0;
        std::wstring context(start - contextStart + 1, 0);
        SendMessage(g_hwndEditor, EM_SETSEL, contextStart, start);
        SendMessage(g_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)context.c_str());
        context.resize(wcslen(context.c_str()));
        SendMessage(g_hwndEditor, EM_SETSEL, start, end); // Restore selection
        
        std::string ctx = WideToUtf8(context);
        std::vector<int> tokens;
        for (char c : ctx) tokens.push_back((unsigned char)c);
        
        std::vector<float> logits(32000, 0.0f);
        std::string generated;
        if (pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data()) == 0) {
            for (int i = 0; i < 128; i++) {
                int next = pSampleNext(logits.data(), 32000, 0.8f, 0.9f, 40);
                if (next <= 0) break;
                generated += (char)next;
                tokens.push_back(next);
                pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data());
            }
        }
        
        if (!generated.empty()) {
            std::wstring wGenerated = Utf8ToWide(generated);
            AICompletion completion = {};
            completion.text = wGenerated;
            completion.detail = L"AI generated code";
            completion.confidence = 0.85;
            completion.kind = L"ai-generated";
            completion.cursorOffset = (int)completion.text.size();
            completion.isMultiLine = (generated.find('\n') != std::string::npos);
            ShowAICompletion(completion);
            AppendWindowText(g_hwndOutput, L"[AI] Code generated. Press Tab to accept.\r\n");
            return;
        }
    }
    
    // Fallback: context-aware completion
    TriggerAICompletion();
    AppendWindowText(g_hwndOutput, L"[AI] Code completion ready. Press Tab to accept.\r\n");
}

void AI_ExplainCode() {
    ExplainSelectedCode();
}

void AI_RefactorCode() {
    RefactorSelectedCode();
}

void AI_FixCode() {
    if (!g_hwndEditor) return;
    AppendWindowText(g_hwndOutput, L"[AI] Running code fix analysis...\r\n");
    
    // Run code analysis first
    RunCodeAnalysis();
    
    // Auto-apply fixable issues
    int fixedCount = 0;
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    for (int i = (int)g_codeIssues.size() - 1; i >= 0; i--) {
        if (g_codeIssues[i].isFixable && !g_codeIssues[i].quickFix.empty()) {
            // Jump and fix
            int lineIndex = (int)SendMessage(g_hwndEditor, EM_LINEINDEX, g_codeIssues[i].line - 1, 0);
            int lineLen = (int)SendMessage(g_hwndEditor, EM_LINELENGTH, lineIndex, 0);
            SendMessage(g_hwndEditor, EM_SETSEL, lineIndex, lineIndex + lineLen);
            
            // Get the line text and apply fix
            std::wstring lineText(lineLen + 1, 0);
            *(WORD*)&lineText[0] = (WORD)(lineLen + 1);
            SendMessage(g_hwndEditor, EM_GETLINE, g_codeIssues[i].line - 1, (LPARAM)lineText.c_str());
            lineText.resize(lineLen);
            
            // Append semicolon if that's the fix
            if (g_codeIssues[i].rule == L"syntax-error") {
                lineText += L";";
                SendMessage(g_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)lineText.c_str());
                fixedCount++;
            }
            g_codeIssues.erase(g_codeIssues.begin() + i);
        }
    }
    
    if (fixedCount > 0) {
        g_isDirty = true;
        wchar_t msg[128];
        swprintf_s(msg, L"[AI] Auto-fixed %d issue(s).\r\n", fixedCount);
        AppendWindowText(g_hwndOutput, msg);
    } else {
        AppendWindowText(g_hwndOutput, L"[AI] No auto-fixable issues found.\r\n");
    }
}

// ============================================================================
// Optimized AI Completion with minimal RichEdit operations
// ============================================================================
void ShowAICompletion(const AICompletion& completion) {
    if (!g_hwndEditor || completion.text.empty()) return;
    HideAICompletion();

    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    g_completionAnchor = (int)((start > end) ? start : end);
    g_completionText = completion.text;

    // Insert completion text
    SendMessage(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor);
    SendMessageW(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)g_completionText.c_str());
    
    // Apply ghost formatting in one operation
    int endPos = g_completionAnchor + (int)g_completionText.size();
    CHARFORMAT2W format = { sizeof(format), CFM_COLOR, 0, 0, 0, g_colorCompletionGhost };
    SendMessage(g_hwndEditor, EM_SETSEL, g_completionAnchor, endPos);
    SendMessage(g_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    SendMessage(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor);

    g_completionVisible = true;
}

void HideAICompletion() {
    if (!g_completionVisible || g_completionText.empty()) return;
    SendMessage(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor + (int)g_completionText.size());
    SendMessageW(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)L"");
    g_completionVisible = false;
    g_completionText.clear();
    g_completionAnchor = -1;
}

void AcceptAICompletion() {
    if (!g_completionVisible || g_completionText.empty()) return;
    int endPos = g_completionAnchor + (int)g_completionText.size();
    
    // Reset formatting to default in one operation
    CHARFORMAT2W format = { sizeof(format), CFM_COLOR, 0, 0, 0, g_colorDefault };
    SendMessage(g_hwndEditor, EM_SETSEL, g_completionAnchor, endPos);
    SendMessage(g_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    SendMessage(g_hwndEditor, EM_SETSEL, endPos, endPos);
    
    g_completionVisible = false;
    g_completionText.clear();
    g_completionAnchor = -1;
}

void AI_LoadModel() {
    std::wstring path = OpenFileDialog(g_hwndMain);
    if (path.empty()) return;
    
    AppendWindowText(g_hwndOutput, (L"[AI] Loading model: " + path + L"\r\n").c_str());
    
    if (g_hTitanDll && pTitan_LoadModel) {
        std::string utf8Path = WideToUtf8(path);
        int slot = pTitan_LoadModel(utf8Path.c_str(), "custom_model");
        if (slot >= 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Model loaded successfully!\r\n");
        } else {
            AppendWindowText(g_hwndOutput, L"[AI] Failed to load model.\r\n");
        }
    } else {
        AppendWindowText(g_hwndOutput, L"[AI] Titan Kernel not loaded.\r\n");
    }
}

void AppendWindowText(HWND hwnd, const wchar_t* text) {
    int len = GetWindowTextLengthW(hwnd);
    SendMessageW(hwnd, EM_SETSEL, len, len);
    SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)text);
}

// ============================================================================
// Build Actions
// ============================================================================
void Build_Run() {
    if (g_currentFile.empty()) {
        AppendWindowText(g_hwndOutput, L"[Build] No file open.\r\n");
        return;
    }
    
    AppendWindowText(g_hwndOutput, L"[Build] Building and running...\r\n");
    SaveCurrentFile();
    CompileCurrentFile();
    RunCurrentFile();
}

void Build_Build() {
    if (g_currentFile.empty()) {
        AppendWindowText(g_hwndOutput, L"[Build] No file open.\r\n");
        return;
    }
    AppendWindowText(g_hwndOutput, L"[Build] Building project...\r\n");
    SaveCurrentFile();
    CompileCurrentFile();
}

// ============================================================================
// Create Menu
// ============================================================================
HMENU CreateMainMenu() {
    HMENU hMenu = CreateMenu();
    HMENU hFileMenu = CreatePopupMenu();
    HMENU hEditMenu = CreatePopupMenu();
    HMENU hBuildMenu = CreatePopupMenu();
    HMENU hAIMenu = CreatePopupMenu();
    HMENU hHelpMenu = CreatePopupMenu();
    
    // File
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");
    
    // Edit
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REDO, L"&Redo\tCtrl+Y");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND, L"&Find\tCtrl+F");
    
    // Build
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_BUILD, L"&Build\tF7");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_RUN, L"&Run\tF5");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_DEBUG, L"&Debug\tF9");
    
    // View
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuW(hViewMenu, MF_STRING | MF_CHECKED, IDM_VIEW_OUTPUT, L"&Output Panel\tCtrl+`");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_TERMINAL, L"&Terminal\tCtrl+Shift+`");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_CHAT, L"&AI Chat Panel\tCtrl+Shift+C");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_ISSUES, L"Code &Issues Panel\tCtrl+Shift+I");
    
    // AI
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_GENERATE, L"&Generate Code\tCtrl+G");
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_EXPLAIN, L"&Explain Code\tCtrl+E");
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_REFACTOR, L"&Refactor Code\tCtrl+R");
    AppendMenuW(hAIMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_LOADGGUF, L"Load &GGUF Model...");
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_UNLOADMODEL, L"&Unload Model");
    AppendMenuW(hAIMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_LOADMODEL, L"Load &Titan Model...");
    
    // Help
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBuildMenu, L"&Build");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAIMenu, L"&AI");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");
    
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
        
        // Create file tree (placeholder list)
        g_hwndFileTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
            0, 0, 200, 700, hwnd, (HMENU)IDC_FILETREE, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndFileTree, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        
        // Create terminal panel (initially hidden)
        g_hwndTerminal = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            200, 500, 800, 200, hwnd, (HMENU)IDC_TERMINAL, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndTerminal, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        SendMessageW(g_hwndTerminal, EM_SETBKGNDCOLOR, 0, RGB(12, 12, 12));
        
        // Create AI chat history (initially hidden)
        g_hwndChatHistory = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 300, 500, hwnd, (HMENU)IDC_CHATHISTORY, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatHistory, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        
        // Create AI chat input (initially hidden)
        g_hwndChatInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 500, 300, 100, hwnd, (HMENU)IDC_CHATINPUT, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatInput, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SetWindowSubclass(g_hwndChatInput, ChatInputSubclassProc, 0, 0);
        
        // Create Code Issues Panel (initially hidden)
        g_hwndIssuesPanel = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
            WS_CHILD | SS_BLACKFRAME,
            0, 0, 400, 600, hwnd, (HMENU)IDC_ISSUES_PANEL, GetModuleHandle(nullptr), nullptr);
        
        // Issues list view
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
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 4, (LPARAM)&col);
        
        // Issues filter buttons
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
        
        // Issues status bar
        CreateWindowExW(0, L"STATIC", L"No issues found", WS_CHILD | WS_VISIBLE | SS_LEFT,
            5, 540, 300, 20, g_hwndIssuesPanel, nullptr, GetModuleHandle(nullptr), nullptr);
        
        // Create enhanced chat action buttons
        g_hwndChatNew = CreateWindowExW(0, L"BUTTON", L"New", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 40, 25, hwnd, (HMENU)IDC_CHAT_NEW, GetModuleHandle(nullptr), nullptr);
        g_hwndChatHistoryBtn = CreateWindowExW(0, L"BUTTON", L"History", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_HISTORY_BTN, GetModuleHandle(nullptr), nullptr);
        g_hwndChatExport = CreateWindowExW(0, L"BUTTON", L"Export", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_EXPORT, GetModuleHandle(nullptr), nullptr);
        g_hwndChatConfig = CreateWindowExW(0, L"BUTTON", L"Config", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_CONFIG, GetModuleHandle(nullptr), nullptr);
        g_hwndChatMCP = CreateWindowExW(0, L"BUTTON", L"MCP", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 40, 25, hwnd, (HMENU)IDC_CHAT_MCP, GetModuleHandle(nullptr), nullptr);
        g_hwndChatLogs = CreateWindowExW(0, L"BUTTON", L"Logs", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 40, 25, hwnd, (HMENU)IDC_CHAT_LOGS, GetModuleHandle(nullptr), nullptr);
        
        // Create original chat action buttons
        CreateWindowExW(0, L"BUTTON", L"Gen", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_GENERATE, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Exp", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_EXPLAIN, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Ref", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_REFACTOR, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Fix", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_FIX, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Stop", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_STOP, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Clr", WS_CHILD | BS_PUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_CLEAR, GetModuleHandle(nullptr), nullptr);
        CreateWindowExW(0, L"BUTTON", L"Send", WS_CHILD | BS_DEFPUSHBUTTON,
            0, 0, 50, 25, hwnd, (HMENU)IDC_CHAT_SEND, GetModuleHandle(nullptr), nullptr);
        
        // Create status bar
        g_hwndStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, GetModuleHandle(nullptr), nullptr);
        
        // Set menu
        SetMenu(hwnd, CreateMainMenu());
        
        // Load Titan Kernel
        if (LoadTitanKernel()) {
            AppendWindowText(g_hwndOutput, L"[System] Titan Kernel loaded.\r\n");
        } else {
            AppendWindowText(g_hwndOutput, L"[System] Titan Kernel not found (AI features limited).\r\n");
        }
        
        // Load Native Model Bridge (GGUF Inference)
        if (LoadModelBridge()) {
            AppendWindowText(g_hwndOutput, L"[System] Native Model Bridge loaded (GGUF inference ready).\r\n");
        } else {
            AppendWindowText(g_hwndOutput, L"[System] Native Model Bridge not found.\r\n");
        }
        
        AppendWindowText(g_hwndOutput, L"[System] RawrXD IDE started. Use File > Open to load a file.\r\n");
        AppendWindowText(g_hwndOutput, L"[System] Use AI > Load GGUF Model to load a language model.\r\n");
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
        int chatWidth = g_bChatVisible ? 300 : 0;
        int issuesWidth = g_bIssuesVisible ? 400 : 0;
        int bottomHeight = 0;
        if (g_bOutputVisible || g_bTerminalVisible) bottomHeight = 150;
        
        // Layout: file tree on left, editor center, issues right, chat far right, output/terminal bottom
        int treeWidth = 200;
        int mainWidth = width - treeWidth - chatWidth - issuesWidth;
        int editorHeight = height - sbHeight - bottomHeight;
        
        // File tree
        MoveWindow(g_hwndFileTree, 0, 0, treeWidth, height - sbHeight, TRUE);
        
        // Editor
        MoveWindow(g_hwndEditor, treeWidth, 0, mainWidth, editorHeight, TRUE);
        
        // Output panel
        if (g_bOutputVisible && !g_bTerminalVisible) {
            MoveWindow(g_hwndOutput, treeWidth, editorHeight, mainWidth, bottomHeight, TRUE);
            ShowWindow(g_hwndOutput, SW_SHOW);
        } else {
            ShowWindow(g_hwndOutput, SW_HIDE);
        }
        
        // Terminal panel
        if (g_bTerminalVisible) {
            MoveWindow(g_hwndTerminal, treeWidth, editorHeight, mainWidth, bottomHeight, TRUE);
            ShowWindow(g_hwndTerminal, SW_SHOW);
        } else {
            ShowWindow(g_hwndTerminal, SW_HIDE);
        }
        
        // Issues panel
        if (g_bIssuesVisible) {
            int issuesX = treeWidth + mainWidth;
            MoveWindow(g_hwndIssuesPanel, issuesX, 0, issuesWidth, height - sbHeight, TRUE);
            ShowWindow(g_hwndIssuesPanel, SW_SHOW);
        } else {
            ShowWindow(g_hwndIssuesPanel, SW_HIDE);
        }
        
        // Chat panels
        if (g_bChatVisible) {
            int chatX = width - chatWidth;
            int chatInputHeight = 100;
            int btnHeight = 25;
            int topBtnWidth = chatWidth / 6;  // Top row buttons
            int bottomBtnWidth = chatWidth / 7;  // Bottom row buttons

            MoveWindow(g_hwndChatHistory, chatX, btnHeight + 5, chatWidth, height - sbHeight - chatInputHeight - btnHeight * 2 - 10, TRUE);
            
            // Top buttons row (New, History, Export, Config, MCP, Logs)
            int topBtnY = 0;
            MoveWindow(g_hwndChatNew, chatX, topBtnY, topBtnWidth, btnHeight, TRUE);
            MoveWindow(g_hwndChatHistoryBtn, chatX + topBtnWidth, topBtnY, topBtnWidth, btnHeight, TRUE);
            MoveWindow(g_hwndChatExport, chatX + topBtnWidth * 2, topBtnY, topBtnWidth, btnHeight, TRUE);
            MoveWindow(g_hwndChatConfig, chatX + topBtnWidth * 3, topBtnY, topBtnWidth, btnHeight, TRUE);
            MoveWindow(g_hwndChatMCP, chatX + topBtnWidth * 4, topBtnY, topBtnWidth, btnHeight, TRUE);
            MoveWindow(g_hwndChatLogs, chatX + topBtnWidth * 5, topBtnY, topBtnWidth, btnHeight, TRUE);
            
            // Bottom buttons row (Gen, Exp, Ref, Fix, Stop, Clr, Send)
            int bottomBtnY = height - sbHeight - chatInputHeight - btnHeight;
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_GENERATE), chatX, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_EXPLAIN), chatX + bottomBtnWidth, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_REFACTOR), chatX + bottomBtnWidth * 2, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_FIX), chatX + bottomBtnWidth * 3, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_STOP), chatX + bottomBtnWidth * 4, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_CLEAR), chatX + bottomBtnWidth * 5, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_SEND), chatX + bottomBtnWidth * 6, bottomBtnY, bottomBtnWidth, btnHeight, TRUE);

            MoveWindow(g_hwndChatInput, chatX, height - sbHeight - chatInputHeight, chatWidth, chatInputHeight, TRUE);
            
            ShowWindow(g_hwndChatHistory, SW_SHOW);
            ShowWindow(g_hwndChatInput, SW_SHOW);
            
            // Show top row buttons
            ShowWindow(g_hwndChatNew, SW_SHOW);
            ShowWindow(g_hwndChatHistoryBtn, SW_SHOW);
            ShowWindow(g_hwndChatExport, SW_SHOW);
            ShowWindow(g_hwndChatConfig, SW_SHOW);
            ShowWindow(g_hwndChatMCP, SW_SHOW);
            ShowWindow(g_hwndChatLogs, SW_SHOW);
            
            // Show bottom row buttons
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_GENERATE), SW_SHOW);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_EXPLAIN), SW_SHOW);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_REFACTOR), SW_SHOW);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_FIX), SW_SHOW);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_STOP), SW_SHOW);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_CLEAR), SW_SHOW);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_SEND), SW_SHOW);
        } else {
            ShowWindow(g_hwndChatHistory, SW_HIDE);
            ShowWindow(g_hwndChatInput, SW_HIDE);
            
            // Hide top row buttons
            ShowWindow(g_hwndChatNew, SW_HIDE);
            ShowWindow(g_hwndChatHistoryBtn, SW_HIDE);
            ShowWindow(g_hwndChatExport, SW_HIDE);
            ShowWindow(g_hwndChatConfig, SW_HIDE);
            ShowWindow(g_hwndChatMCP, SW_HIDE);
            ShowWindow(g_hwndChatLogs, SW_HIDE);
            
            // Hide bottom row buttons
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_GENERATE), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_EXPLAIN), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_REFACTOR), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_FIX), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_STOP), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_CLEAR), SW_HIDE);
            ShowWindow(GetDlgItem(hwnd, IDC_CHAT_SEND), SW_HIDE);
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

        case IDC_CHAT_GENERATE:
            AI_GenerateCode();
            break;
        case IDC_CHAT_EXPLAIN:
            AI_ExplainCode();
            break;
        case IDC_CHAT_REFACTOR:
            AI_RefactorCode();
            break;
        case IDC_CHAT_FIX:
            AI_FixCode();
            break;
        case IDC_CHAT_STOP:
            g_bAbortInference = true;
            break;
        case IDC_CHAT_CLEAR:
            SetWindowTextW(g_hwndChatHistory, L"");
            break;
        case IDC_CHAT_SEND:
            HandleChatSend();
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
            
        // Enhanced chat controls
        case IDC_CHAT_NEW:
            CreateNewChatSession();
            break;
        case IDC_CHAT_HISTORY_BTN:
            LoadChatHistory();
            break;
        case IDC_CHAT_EXPORT:
            ExportChatHistory();
            break;
        case IDC_CHAT_CONFIG:
            ConfigureMCPServers();
            break;
        case IDC_CHAT_MCP:
            // Toggle MCP server connection
            AppendWindowText(g_hwndChatHistory, L"[MCP] Toggling MCP server connection...\r\n");
            break;
        case IDC_CHAT_LOGS:
            ShowChatLogs();
            break;

        case IDM_AI_LOADMODEL:
            AI_LoadModel();
            break;
            
        case IDM_AI_LOADGGUF: {
            // Open GGUF file dialog
            wchar_t filename[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
            ofn.lpstrTitle = L"Load GGUF Model";
            if (GetOpenFileNameW(&ofn)) {
                AppendWindowText(g_hwndOutput, (L"[AI] Loading GGUF model: " + std::wstring(filename) + L"\r\n").c_str());
                if (LoadModelWithInferenceEngine(filename)) {
                    AppendWindowText(g_hwndOutput, L"[AI] GGUF model loaded via Inference Engine!\r\n");
                    // Update status bar
                    std::wstring status = L"Model: " + g_loadedModelPath;
                    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 1, (LPARAM)status.c_str());
                } else {
                    AppendWindowText(g_hwndOutput, L"[AI] Failed to load GGUF model.\r\n");
                }
            }
            break;
        }
        
        case IDM_AI_UNLOADMODEL:
            if (g_modelLoaded) {
                UnloadInferenceModel();
                AppendWindowText(g_hwndOutput, L"[AI] Model unloaded.\r\n");
                SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 1, (LPARAM)L"No model");
            }
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
// Code Issues Panel Implementation
// ============================================================================
void RefreshCodeIssues() {
    if (!g_hwndIssuesList) return;
    
    // Clear existing items
    SendMessageW(g_hwndIssuesList, LVM_DELETEALLITEMS, 0, 0);
    
    // Run code analysis
    RunCodeAnalysis();
    
    // Populate list with filtered issues
    FilterIssuesBySeverity();
    
    // Update status
    std::wstring status = L"Found " + std::to_wstring(g_codeIssues.size()) + L" issues";
    SetWindowTextW(GetDlgItem(g_hwndIssuesPanel, -1), status.c_str());
}

void AddCodeIssue(const CodeIssue& issue) {
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    g_codeIssues.push_back(issue);
}

void ClearCodeIssues() {
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    g_codeIssues.clear();
    if (g_hwndIssuesList) {
        SendMessageW(g_hwndIssuesList, LVM_DELETEALLITEMS, 0, 0);
    }
    AppendWindowText(g_hwndOutput, L"[Issues] All issues cleared.\r\n");
}

void FilterIssuesBySeverity() {
    if (!g_hwndIssuesList) return;
    
    SendMessageW(g_hwndIssuesList, LVM_DELETEALLITEMS, 0, 0);
    
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    int itemIndex = 0;
    
    for (const auto& issue : g_codeIssues) {
        bool shouldShow = (issue.severity == L"Critical" && g_bShowCritical) ||
                          (issue.severity == L"High" && g_bShowHigh) ||
                          (issue.severity == L"Medium" && g_bShowMedium) ||
                          (issue.severity == L"Low" && g_bShowLow) ||
                          (issue.severity == L"Info" && g_bShowInfo);
        
        if (shouldShow) {
            LVITEMW item = {};
            item.mask = LVIF_TEXT;
            item.iItem = itemIndex;
            
            // Severity
            item.iSubItem = 0;
            item.pszText = (LPWSTR)issue.severity.c_str();
            SendMessageW(g_hwndIssuesList, LVM_INSERTITEMW, 0, (LPARAM)&item);
            
            // Category
            item.iSubItem = 1;
            item.pszText = (LPWSTR)issue.category.c_str();
            SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, itemIndex, (LPARAM)&item);
            
            // Message
            item.iSubItem = 2;
            item.pszText = (LPWSTR)issue.message.c_str();
            SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, itemIndex, (LPARAM)&item);
            
            // File
            item.iSubItem = 3;
            std::wstring fileName = issue.file;
            size_t pos = fileName.find_last_of(L"\\");
            if (pos != std::wstring::npos) fileName = fileName.substr(pos + 1);
            item.pszText = (LPWSTR)fileName.c_str();
            SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, itemIndex, (LPARAM)&item);
            
            // Line
            std::wstring lineStr = std::to_wstring(issue.line);
            item.iSubItem = 4;
            item.pszText = (LPWSTR)lineStr.c_str();
            SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, itemIndex, (LPARAM)&item);
            
            itemIndex++;
        }
    }
}

void RunCodeAnalysis() {
    // Clear existing issues
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        g_codeIssues.clear();
    }
    
    if (g_currentFile.empty()) {
        AppendWindowText(g_hwndOutput, L"[Issues] No file open for analysis.\r\n");
        return;
    }
    
    AppendWindowText(g_hwndOutput, L"[Issues] Running code analysis...\r\n");
    
    // Get current file content
    int len = GetWindowTextLengthA(g_hwndEditor);
    if (len <= 0) return;
    
    std::string content;
    content.resize(len);
    GetWindowTextA(g_hwndEditor, &content[0], len + 1);
    
    // Simple static analysis - look for common issues
    std::istringstream stream(content);
    std::string line;
    int lineNum = 1;
    
    while (std::getline(stream, line)) {
        // Check for potential issues
        
        // Missing semicolon (simple heuristic)
        if (line.find("return") != std::string::npos && 
            line.find(";") == std::string::npos && 
            line.find("//") == std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile;
            issue.line = lineNum;
            issue.column = 1;
            issue.severity = L"High";
            issue.category = L"Error";
            issue.message = L"Missing semicolon after return statement";
            issue.description = L"Return statements should end with a semicolon";
            issue.rule = L"syntax-error";
            issue.isFixable = true;
            issue.quickFix = L"Add semicolon at end of line";
            GetSystemTime(&issue.timestamp);
            AddCodeIssue(issue);
        }
        
        // Potential memory leak
        if (line.find("new ") != std::string::npos && 
            content.find("delete") == std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile;
            issue.line = lineNum;
            issue.column = 1;
            issue.severity = L"Medium";
            issue.category = L"Security";
            issue.message = L"Potential memory leak - new without delete";
            issue.description = L"Memory allocated with 'new' should be freed with 'delete'";
            issue.rule = L"memory-leak";
            issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            AddCodeIssue(issue);
        }
        
        // TODO comments
        if (line.find("TODO") != std::string::npos || line.find("FIXME") != std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile;
            issue.line = lineNum;
            issue.column = 1;
            issue.severity = L"Info";
            issue.category = L"Info";
            issue.message = L"TODO/FIXME comment found";
            issue.description = L"Consider addressing this TODO item";
            issue.rule = L"todo-comment";
            issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            AddCodeIssue(issue);
        }
        
        // Long lines
        if (line.length() > 120) {
            CodeIssue issue;
            issue.file = g_currentFile;
            issue.line = lineNum;
            issue.column = 121;
            issue.severity = L"Low";
            issue.category = L"Warning";
            issue.message = L"Line too long (>120 characters)";
            issue.description = L"Consider breaking long lines for better readability";
            issue.rule = L"line-length";
            issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            AddCodeIssue(issue);
        }
        
        lineNum++;
    }
    
    AppendWindowText(g_hwndOutput, L"[Issues] Code analysis complete.\r\n");
}

void ExportIssues() {
    std::wstring path = SaveFileDialog(g_hwndMain);
    if (path.empty()) return;
    
    // Ensure .csv extension
    if (path.find(L".csv") == std::wstring::npos) {
        path += L".csv";
    }
    
    std::string utf8Path = WideToUtf8(path);
    std::ofstream file(utf8Path);
    if (!file.is_open()) {
        MessageBoxW(g_hwndMain, L"Failed to create export file.", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Write CSV header
    file << "Severity,Category,Message,File,Line,Column,Rule,Timestamp\n";
    
    // Write issues
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    for (const auto& issue : g_codeIssues) {
        std::string utf8Message = WideToUtf8(issue.message);
        std::string utf8File = WideToUtf8(issue.file);
        std::string utf8Rule = WideToUtf8(issue.rule);
        file << WideToUtf8(issue.severity) << ","
             << WideToUtf8(issue.category) << ","
             << "\"" << utf8Message << "\","
             << utf8File << ","
             << issue.line << ","
             << issue.column << ","
             << utf8Rule << ","
             << issue.timestamp.wYear << "-" << issue.timestamp.wMonth << "-" << issue.timestamp.wDay
             << "\n";
    }
    
    file.close();
    AppendWindowText(g_hwndOutput, (L"[Issues] Exported " + std::to_wstring(g_codeIssues.size()) + L" issues to " + path + L"\r\n").c_str());
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"RawrXDIDE";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) {
        return 1;
    }
    
    // Create main window
    g_hwndMain = CreateWindowExW(0, L"RawrXDIDE", L"RawrXD IDE - Autonomous Agentic Development",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, hInstance, nullptr);
    
    if (!g_hwndMain) return 1;
    
    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);
    
    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

// Console entry for testing
int main() {
    return wWinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOWDEFAULT);
}

// ============================================================================
// Chat Input Subclassing
// ============================================================================
LRESULT CALLBACK ChatInputSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_CHAR && wParam == VK_RETURN) {
        if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
            SendMessage(GetParent(hwnd), WM_COMMAND, IDC_CHAT_SEND, 0);
            return 0;
        }
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

void HandleChatSend() {
    int len = GetWindowTextLengthW(g_hwndChatInput);
    if (len <= 0) return;

    std::wstring input(len + 1, L'\0');
    GetWindowTextW(g_hwndChatInput, &input[0], len + 1);
    input.resize(len);

    // Clear input
    SetWindowTextW(g_hwndChatInput, L"");

    // Display in history
    AppendWindowText(g_hwndChatHistory, L"\r\n> ");
    AppendWindowText(g_hwndChatHistory, input.c_str());
    AppendWindowText(g_hwndChatHistory, L"\r\n");

    // Send to Python HTTP server
    SendChatToServer(input);
}

// ============================================================================
// Optimized HTTP Client with connection pooling
// ============================================================================
static HINTERNET g_hInternet = nullptr;
static HINTERNET g_hConnect = nullptr;

void InitializeHTTPClient() {
    if (!g_hInternet) {
        g_hInternet = InternetOpenW(L"RawrXD IDE/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
        if (g_hInternet) {
            g_hConnect = InternetConnectW(g_hInternet, L"localhost", 23959, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
        }
    }
}

void CleanupHTTPClient() {
    if (g_hConnect) {
        InternetCloseHandle(g_hConnect);
        g_hConnect = nullptr;
    }
    if (g_hInternet) {
        InternetCloseHandle(g_hInternet);
        g_hInternet = nullptr;
    }
}

void SendChatToServer(const std::wstring& message) {
    InitializeHTTPClient();
    if (!g_hConnect) {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to connect to server.\r\n");
        return;
    }
    
    std::string utf8Message = WideToUtf8(message);
    std::string jsonData;
    jsonData.reserve(utf8Message.length() + 32);
    jsonData = "{\"message\": \"" + utf8Message + "\"}";
    
    HINTERNET hRequest = HttpOpenRequestW(g_hConnect, L"POST", L"/api/chat", nullptr, nullptr, nullptr, 0, 0);
    if (!hRequest) {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to create HTTP request.\r\n");
        return;
    }
    
    const wchar_t* headers = L"Content-Type: application/json\r\n";
    BOOL result = HttpSendRequestW(hRequest, headers, -1, (LPVOID)jsonData.c_str(), (DWORD)jsonData.length());
    
    if (result) {
        std::string response;
        response.reserve(4096);
        char buffer[4096];
        DWORD bytesRead;
        
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0'; // Ensure null termination
            response.append(buffer, bytesRead);
        }
        
        // Simple JSON parsing
        const std::string replyKey = "\"reply\":\"";
        size_t pos = response.find(replyKey);
        if (pos != std::string::npos) {
            pos += replyKey.length();
            size_t start = response.find('\"', pos);
            if (start != std::string::npos) {
                size_t end = response.find('\"', start + 1);
                if (end != std::string::npos) {
                    std::string reply = response.substr(start + 1, end - start - 1);
                    std::wstring wReply = Utf8ToWide(reply);
                    AppendWindowText(g_hwndChatHistory, L"Assistant: ");
                    AppendWindowText(g_hwndChatHistory, wReply.c_str());
                    AppendWindowText(g_hwndChatHistory, L"\r\n");
                }
            }
        } else {
            AppendWindowText(g_hwndChatHistory, L"[AI Chat] Received empty response.\r\n");
        }
    } else {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to send request.\r\n");
    }
    
    InternetCloseHandle(hRequest);
}

LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    (void)uIdSubclass;
    (void)dwRefData;
    if (uMsg == WM_KEYDOWN && g_completionVisible) {
        if (wParam == VK_TAB) {
            AcceptAICompletion();
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            HideAICompletion();
            return 0;
        }
    }
    if (uMsg == WM_CHAR && g_completionVisible) {
        if (wParam != VK_TAB && wParam != VK_ESCAPE) {
            HideAICompletion();
        }
    }
    if ((uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MOUSEWHEEL) && g_completionVisible) {
        HideAICompletion();
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Chat Server Management
// ============================================================================
bool StartChatServer() {
    // Get the path to the Python executable
    wchar_t pythonPath[MAX_PATH];
    if (!SearchPathW(nullptr, L"python.exe", nullptr, MAX_PATH, pythonPath, nullptr)) {
        // Try python3.exe
        if (!SearchPathW(nullptr, L"python3.exe", nullptr, MAX_PATH, pythonPath, nullptr)) {
            AppendWindowText(g_hwndChatHistory, L"[AI Chat] Python not found in PATH.\r\n");
            return false;
        }
    }
    
    // Get the path to the chat server script
    wchar_t scriptPath[MAX_PATH];
    GetModuleFileNameW(nullptr, scriptPath, MAX_PATH);
    PathRemoveFileSpecW(scriptPath);
    PathAppendW(scriptPath, L"chat_server.py");
    
    // Check if the script exists
    if (GetFileAttributesW(scriptPath) == INVALID_FILE_ATTRIBUTES) {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] chat_server.py not found.\r\n");
        return false;
    }
    
    // Build command line
    wchar_t cmdLine[MAX_PATH * 2];
    swprintf_s(cmdLine, L"\"%s\" \"%s\"", pythonPath, scriptPath);
    
    // Start the process
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // Hide the console window
    
    PROCESS_INFORMATION pi = {};
    
    if (CreateProcessW(
        nullptr,           // Application name
        cmdLine,           // Command line
        nullptr,           // Process attributes
        nullptr,           // Thread attributes
        FALSE,             // Inherit handles
        CREATE_NO_WINDOW,  // Creation flags (no console window)
        nullptr,           // Environment
        nullptr,           // Current directory
        &si,               // Startup info
        &pi                // Process info
    )) {
        // Store the process handle
        g_hChatServerProcess = pi.hProcess;
        CloseHandle(pi.hThread);  // We don't need the thread handle
        
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Python server process started.\r\n");
        return true;
    } else {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to start Python server process.\r\n");
        return false;
    }
}

void StopChatServer() {
    if (g_hChatServerProcess) {
        TerminateProcess(g_hChatServerProcess, 0);
        CloseHandle(g_hChatServerProcess);
        g_hChatServerProcess = nullptr;
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Python server stopped.\r\n");
    }
}

// ============================================================================
// Chat & MCP Management
// ============================================================================

void CreateNewChatSession() {
    if (g_hwndChatHistory) {
        SetWindowTextW(g_hwndChatHistory, L"");
        AppendWindowText(g_hwndChatHistory, L"[System] New chat session started.\r\n");
        AppendWindowText(g_hwndChatHistory, L"[System] Type a message and press Send or Enter.\r\n");
    }
    if (g_hwndChatInput) {
        SetWindowTextW(g_hwndChatInput, L"");
        SetFocus(g_hwndChatInput);
    }
}

void LoadChatHistory() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"Chat Logs (*.txt;*.log)\0*.txt;*.log\0All Files\0*.*\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    ofn.lpstrTitle = L"Load Chat History";

    if (GetOpenFileNameW(&ofn)) {
        HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD fileSize = GetFileSize(hFile, nullptr);
            if (fileSize > 0 && fileSize < 10 * 1024 * 1024) {
                char* buf = new char[fileSize + 1];
                DWORD bytesRead = 0;
                if (ReadFile(hFile, buf, fileSize, &bytesRead, nullptr)) {
                    buf[bytesRead] = '\0';

                    int wLen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
                    if (wLen > 0) {
                        wchar_t* wBuf = new wchar_t[wLen];
                        MultiByteToWideChar(CP_UTF8, 0, buf, -1, wBuf, wLen);

                        if (g_hwndChatHistory) {
                            SetWindowTextW(g_hwndChatHistory, L"");
                            AppendWindowText(g_hwndChatHistory, L"[System] Loaded chat history:\r\n");
                            AppendWindowText(g_hwndChatHistory, wBuf);
                        }
                        delete[] wBuf;
                    }
                }
                delete[] buf; new wchar_t[wLen];
                MultiByteToWideChar(CP_UTF8, 0, buf, -1, wBuf, wLen);

                if (g_hwndChatHistory) {
                    SetWindowTextW(g_hwndChatHistory, L"");
                    AppendWindowText(g_hwndChatHistory, L"[System] Loaded chat history:\r\n");
                    AppendWindowText(g_hwndChatHistory, wBuf);
                }
                delete[] wBuf;
                delete[] buf;
            }
            CloseHandle(hFile);
        }
    }
}

void ExportChatHistory() {
    wchar_t filename[MAX_PATH] = L"chat_export.txt";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files\0*.*\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"txt";
    ofn.lpstrTitle = L"Export Chat History";

    if (GetSaveFileNameW(&ofn)) {
        int len = GetWindowTextLengthW(g_hwndChatHistory);
        if (len > 0) {
            wchar_t* text = new wchar_t[len + 1];
            GetWindowTextW(g_hwndChatHistory, text, len + 1);

            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            char* utf8 = new char[utf8Len];
            WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, utf8Len, nullptr, nullptr);

            HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hFile, utf8, utf8Len - 1, &written, nullptr);
                CloseHandle(hFile);
                AppendWindowText(g_hwndChatHistory, L"\r\n[System] Chat history exported successfully.\r\n");
            }
            delete[] utf8;
            delete[] text;
        }
    }
}

void ConfigureMCPServers() {
    // MCP (Model Context Protocol) server configuration dialog
    int result = MessageBoxW(g_hwndMain,
        L"MCP Server Configuration\r\n\r\n"
        L"Current servers:\r\n"
        L"  - localhost:8080 (Local inference)\r\n"
        L"  - GGUF direct load (file-based)\r\n\r\n"
        L"Would you like to add a new MCP server endpoint?",
        L"RawrXD - MCP Configuration",
        MB_YESNOCANCEL | MB_ICONINFORMATION);

    if (result == IDYES) {
        AppendWindowText(g_hwndChatHistory,
            L"[MCP] Opening server configuration...\r\n"
            L"[MCP] Use Settings > AI > MCP Servers to configure endpoints.\r\n");
    }
}

void ShowChatLogs() {
    // Find and open the chat logs directory
    wchar_t logPath[MAX_PATH];
    GetModuleFileNameW(nullptr, logPath, MAX_PATH);

    // Navigate to parent directory and append logs folder
    wchar_t* lastSlash = wcsrchr(logPath, L'\\');
    if (lastSlash) {
        wcscpy_s(lastSlash + 1, MAX_PATH - (lastSlash - logPath + 1), L"logs");
        CreateDirectoryW(logPath, nullptr);
        ShellExecuteW(g_hwndMain, L"explore", logPath, nullptr, nullptr, SW_SHOWNORMAL);
    }

    AppendWindowText(g_hwndChatHistory, L"[System] Opening chat logs directory...\r\n");
}
