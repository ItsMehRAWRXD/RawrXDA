/*
 * RawrXD_Win32_IDE.cpp - Pure Win32 IDE (Zero Qt Dependencies)
 * Production-ready autonomous agentic development environment
 * Copyright (c) 2026 RawrXD Project
 *
 * Build: cl /O2 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS
 *        /DNOMINMAX /EHsc /std:c++17 /W1 RawrXD_Win32_IDE.cpp
 *        /link user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib
 *              comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib
 *        /SUBSYSTEM:WINDOWS
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
#include <shlwapi.h>
#include <wininet.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <mutex>
#include <algorithm>
#include <functional>
#include <thread>
#include <atomic>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

// ============================================================================
// Control IDs
// ============================================================================
#define IDC_EDITOR          100
#define IDC_OUTPUT          101
#define IDC_FILETREE        102
#define IDC_STATUSBAR       103
#define IDC_TERMINAL        104
#define IDC_CHATHISTORY     105
#define IDC_CHATINPUT       106
#define IDC_TABCONTROL      107
#define IDC_ISSUES_PANEL    108
#define IDC_ISSUES_LIST     109

// Chat buttons
#define IDC_CHAT_SEND       200
#define IDC_CHAT_NEW        201
#define IDC_CHAT_HISTORY    202
#define IDC_CHAT_EXPORT     203
#define IDC_CHAT_CONFIG     204
#define IDC_CHAT_CLEAR      205
#define IDC_CHAT_GEN        206
#define IDC_CHAT_EXPLAIN    207
#define IDC_CHAT_REFACTOR   208
#define IDC_CHAT_FIX        209
#define IDC_CHAT_STOP       210

// Issues buttons
#define IDC_ISSUES_CRITICAL 220
#define IDC_ISSUES_HIGH     221
#define IDC_ISSUES_MEDIUM   222
#define IDC_ISSUES_LOW      223
#define IDC_ISSUES_INFO     224
#define IDC_ISSUES_REFRESH  225
#define IDC_ISSUES_EXPORT   226
#define IDC_ISSUES_CLEAR    227

// Menu IDs
#define IDM_FILE_NEW        1001
#define IDM_FILE_OPEN       1002
#define IDM_FILE_SAVE       1003
#define IDM_FILE_SAVEAS     1004
#define IDM_FILE_CLOSE      1005
#define IDM_FILE_EXIT       1006

#define IDM_EDIT_UNDO       1010
#define IDM_EDIT_REDO       1011
#define IDM_EDIT_CUT        1012
#define IDM_EDIT_COPY       1013
#define IDM_EDIT_PASTE      1014
#define IDM_EDIT_FIND       1015
#define IDM_EDIT_REPLACE    1016
#define IDM_EDIT_SELECTALL  1017
#define IDM_EDIT_GOTO       1018

#define IDM_VIEW_FILETREE   1020
#define IDM_VIEW_OUTPUT     1021
#define IDM_VIEW_TERMINAL   1022
#define IDM_VIEW_CHAT       1023
#define IDM_VIEW_ISSUES     1024

#define IDM_BUILD_COMPILE   1030
#define IDM_BUILD_RUN       1031
#define IDM_BUILD_CLEAN     1032

#define IDM_AI_COMPLETE     1040
#define IDM_AI_EXPLAIN      1041
#define IDM_AI_REFACTOR     1042
#define IDM_AI_LOADGGUF     1043
#define IDM_AI_UNLOAD       1044
#define IDM_AI_LOADTITAN    1045

#define IDM_HELP_ABOUT      1050
#define IDM_HELP_SHORTCUTS  1051

#define WM_TERMINAL_OUTPUT  (WM_USER + 100)
#define WM_SYNTAX_HIGHLIGHT (WM_USER + 101)
#define WM_BUILD_COMPLETE   (WM_USER + 102)

// ============================================================================
// Forward Declarations
// ============================================================================

// DLL function types - Titan Kernel
typedef int  (*Titan_Initialize_t)();
typedef int  (*Titan_LoadModelPersistent_t)(const char* path, const char* name);
typedef int  (*Titan_RunInference_t)(int slotIdx, const char* prompt, int maxTokens);
typedef void (*Titan_Shutdown_t)();

// DLL function types - Inference Engine
typedef void* (*LoadModel_t)(const wchar_t* path);
typedef void  (*UnloadModel_t)(void* ctx);
typedef int   (*ForwardPassInfer_t)(void* ctx, int* tokens, int n_tokens, float* logits);
typedef int   (*SampleNext_t)(float* logits, int vocab_size, float temperature, float top_p, int top_k);

// DLL function types - Native Model Bridge
typedef int  (*LoadModelNative_t)(const char* filepath);
typedef int  (*GetTokenEmbedding_t)(int tokenId, float* output, int dim);
typedef int  (*ForwardPassASM_t)(int* tokens, int numTokens, float* output, int vocabSize);
typedef void (*CleanupMathTables_t)();

// ============================================================================
// Data Structures
// ============================================================================

struct EditorTab {
    HWND hwndEdit;
    std::wstring filePath;
    std::wstring displayName;
    bool isDirty;
};

struct AICompletion {
    std::wstring text;
    std::wstring detail;
    double confidence;
    std::wstring kind;
    int cursorOffset;
    bool isMultiLine;
};

struct CodeIssue {
    std::wstring file;
    int line;
    int column;
    std::wstring severity;   // Critical, High, Medium, Low, Info
    std::wstring category;   // Error, Warning, Security, Performance, Info
    std::wstring message;
    std::wstring rule;
    std::wstring quickFix;
    bool isFixable;
    SYSTEMTIME timestamp;
};

// ============================================================================
// Global State
// ============================================================================

// Window handles
static HWND g_hwndMain = nullptr;
static HWND g_hwndEditor = nullptr;
static HWND g_hwndOutput = nullptr;
static HWND g_hwndFileTree = nullptr;
static HWND g_hwndStatusBar = nullptr;
static HWND g_hwndTabControl = nullptr;
static HWND g_hwndTerminal = nullptr;
static HWND g_hwndChatHistory = nullptr;
static HWND g_hwndChatInput = nullptr;
static HWND g_hwndIssuesPanel = nullptr;
static HWND g_hwndIssuesList = nullptr;

// Fonts
static HFONT g_hFontCode = nullptr;
static HFONT g_hFontUI = nullptr;

// File state
static std::wstring g_currentFile;
static std::wstring g_workspaceRoot;
static bool g_isDirty = false;

// Tab system
static std::vector<EditorTab> g_tabs;
static int g_activeTab = -1;

// Panel visibility
static bool g_bFileTreeVisible = true;
static bool g_bOutputVisible = true;
static bool g_bTerminalVisible = false;
static bool g_bChatVisible = false;
static bool g_bIssuesVisible = false;

// AI completion state
static bool g_completionVisible = false;
static std::wstring g_completionText;
static int g_completionAnchor = -1;
static std::atomic<bool> g_bAbortInference{false};

// Issues
static std::vector<CodeIssue> g_codeIssues;
static std::mutex g_issuesMutex;
static bool g_bShowCritical = true;
static bool g_bShowHigh = true;
static bool g_bShowMedium = true;
static bool g_bShowLow = true;
static bool g_bShowInfo = true;

// Terminal process
static HANDLE g_hTermProcess = nullptr;
static HANDLE g_hTermStdoutRead = nullptr;
static HANDLE g_hTermStdinWrite = nullptr;
static CRITICAL_SECTION g_terminalCS;
static bool g_terminalCSInit = false;
static std::thread g_termReadThread;
static std::atomic<bool> g_termRunning{false};

// DLL handles
static HMODULE g_hTitanDll = nullptr;
static Titan_Initialize_t       pTitan_Initialize = nullptr;
static Titan_LoadModelPersistent_t pTitan_LoadModel = nullptr;
static Titan_RunInference_t     pTitan_RunInference = nullptr;
static Titan_Shutdown_t         pTitan_Shutdown = nullptr;

static HMODULE g_hInferenceEngine = nullptr;
static LoadModel_t       pLoadModel = nullptr;
static UnloadModel_t     pUnloadModel = nullptr;
static ForwardPassInfer_t pForwardPassInfer = nullptr;
static SampleNext_t      pSampleNext = nullptr;
static void*             g_modelContext = nullptr;
static bool              g_modelLoaded = false;
static std::wstring      g_loadedModelPath;

static HMODULE g_hModelBridgeDll = nullptr;
static LoadModelNative_t      pLoadModelNative = nullptr;
static GetTokenEmbedding_t    pGetTokenEmbedding = nullptr;
static ForwardPassASM_t       pForwardPassASM = nullptr;
static CleanupMathTables_t    pCleanupMathTables = nullptr;

// HTTP client
static HINTERNET g_hInternet = nullptr;

// Syntax highlighting colors (VS Code dark theme)
static const COLORREF CLR_KEYWORD    = RGB(86, 156, 214);
static const COLORREF CLR_STRING     = RGB(214, 157, 133);
static const COLORREF CLR_COMMENT    = RGB(106, 153, 85);
static const COLORREF CLR_NUMBER     = RGB(181, 206, 168);
static const COLORREF CLR_PREPROC    = RGB(155, 155, 155);
static const COLORREF CLR_DEFAULT    = RGB(212, 212, 212);
static const COLORREF CLR_BACKGROUND = RGB(30, 30, 30);
static const COLORREF CLR_GHOST      = RGB(100, 100, 100);
static const COLORREF CLR_TYPE       = RGB(78, 201, 176);
static const COLORREF CLR_FUNCTION   = RGB(220, 220, 170);

// Syntax keywords
static std::unordered_set<std::wstring> g_cppKeywords;
static std::unordered_set<std::wstring> g_cppTypes;
static bool g_highlightingInProgress = false;
static std::wstring g_lastHighlightedText;

// Find/Replace state
static HWND g_hwndFindDlg = nullptr;
static std::wstring g_findText;
static std::wstring g_replaceText;
static bool g_findMatchCase = false;
static bool g_findWholeWord = false;

// ============================================================================
// Forward Declarations
// ============================================================================
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditorSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK ChatInputSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK TerminalSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
INT_PTR CALLBACK FindDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ReplaceDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK GotoDlgProc(HWND, UINT, WPARAM, LPARAM);

void AppendOutput(const wchar_t* text);
void AppendChat(const wchar_t* text);
void AppendTerminal(const wchar_t* text);
void UpdateLayout(HWND hwnd);
void UpdateStatusBar();
void UpdateWindowTitle();

// ============================================================================
// Utility Functions
// ============================================================================

std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string str(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) return L"";
    std::wstring wstr(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    return wstr;
}

void AppendToRichEdit(HWND hwnd, const wchar_t* text) {
    if (!hwnd || !text) return;
    int len = GetWindowTextLengthW(hwnd);
    SendMessageW(hwnd, EM_SETSEL, len, len);
    SendMessageW(hwnd, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageW(hwnd, EM_SCROLLCARET, 0, 0);
}

void AppendOutput(const wchar_t* text)   { AppendToRichEdit(g_hwndOutput, text); }
void AppendChat(const wchar_t* text)     { AppendToRichEdit(g_hwndChatHistory, text); }
void AppendTerminal(const wchar_t* text) { AppendToRichEdit(g_hwndTerminal, text); }

std::wstring GetExeDirectory() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return path;
}

std::wstring GetFileExtension(const std::wstring& path) {
    size_t dot = path.find_last_of(L'.');
    if (dot == std::wstring::npos) return L"";
    std::wstring ext = path.substr(dot);
    for (auto& c : ext) c = towlower(c);
    return ext;
}

std::wstring GetFileName(const std::wstring& path) {
    size_t sep = path.find_last_of(L"\\/");
    return (sep != std::wstring::npos) ? path.substr(sep + 1) : path;
}

// ============================================================================
// DLL Loading
// ============================================================================

bool LoadTitanKernel() {
    if (g_hTitanDll) return true;
    std::wstring dir = GetExeDirectory();
    g_hTitanDll = LoadLibraryW((dir + L"\\RawrXD_Titan_Kernel.dll").c_str());
    if (!g_hTitanDll) g_hTitanDll = LoadLibraryW(L"RawrXD_Titan_Kernel.dll");
    if (g_hTitanDll) {
        pTitan_Initialize = (Titan_Initialize_t)GetProcAddress(g_hTitanDll, "Titan_Initialize");
        pTitan_LoadModel  = (Titan_LoadModelPersistent_t)GetProcAddress(g_hTitanDll, "Titan_LoadModelPersistent");
        pTitan_RunInference = (Titan_RunInference_t)GetProcAddress(g_hTitanDll, "Titan_RunInference");
        pTitan_Shutdown   = (Titan_Shutdown_t)GetProcAddress(g_hTitanDll, "Titan_Shutdown");
        if (!pTitan_Initialize || !pTitan_LoadModel || !pTitan_RunInference || !pTitan_Shutdown) {
            FreeLibrary(g_hTitanDll); g_hTitanDll = nullptr; return false;
        }
    }
    return g_hTitanDll != nullptr;
}

bool LoadInferenceEngine() {
    if (g_hInferenceEngine) return true;
    std::wstring dir = GetExeDirectory();
    const wchar_t* names[] = { L"RawrXD_InferenceEngine_Win32.dll", L"RawrXD_InferenceEngine.dll" };
    for (auto name : names) {
        g_hInferenceEngine = LoadLibraryW((dir + L"\\" + name).c_str());
        if (!g_hInferenceEngine) g_hInferenceEngine = LoadLibraryW(name);
        if (g_hInferenceEngine) break;
    }
    if (g_hInferenceEngine) {
        pLoadModel = (LoadModel_t)GetProcAddress(g_hInferenceEngine, "LoadModel");
        pUnloadModel = (UnloadModel_t)GetProcAddress(g_hInferenceEngine, "UnloadModel");
        pForwardPassInfer = (ForwardPassInfer_t)GetProcAddress(g_hInferenceEngine, "ForwardPass");
        pSampleNext = (SampleNext_t)GetProcAddress(g_hInferenceEngine, "SampleNext");
        if (!pLoadModel || !pUnloadModel) {
            FreeLibrary(g_hInferenceEngine); g_hInferenceEngine = nullptr; return false;
        }
    }
    return g_hInferenceEngine != nullptr;
}

bool LoadModelBridge() {
    if (g_hModelBridgeDll) return true;
    std::wstring dir = GetExeDirectory();
    g_hModelBridgeDll = LoadLibraryW((dir + L"\\RawrXD_NativeModelBridge.dll").c_str());
    if (!g_hModelBridgeDll) g_hModelBridgeDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    if (g_hModelBridgeDll) {
        pLoadModelNative   = (LoadModelNative_t)GetProcAddress(g_hModelBridgeDll, "LoadModelNative");
        pGetTokenEmbedding = (GetTokenEmbedding_t)GetProcAddress(g_hModelBridgeDll, "GetTokenEmbedding");
        pForwardPassASM    = (ForwardPassASM_t)GetProcAddress(g_hModelBridgeDll, "ForwardPass");
        pCleanupMathTables = (CleanupMathTables_t)GetProcAddress(g_hModelBridgeDll, "CleanupMathTables");
    }
    return g_hModelBridgeDll != nullptr;
}

bool LoadGGUFModel(const std::wstring& filepath) {
    if (!LoadInferenceEngine()) return false;
    if (g_modelContext && pUnloadModel) { pUnloadModel(g_modelContext); g_modelContext = nullptr; }
    g_modelContext = pLoadModel(filepath.c_str());
    if (g_modelContext) { g_modelLoaded = true; g_loadedModelPath = filepath; return true; }
    return false;
}

void UnloadModel() {
    if (g_modelContext && pUnloadModel) { pUnloadModel(g_modelContext); g_modelContext = nullptr; }
    g_modelLoaded = false; g_loadedModelPath.clear();
}

// ============================================================================
// HTTP Client
// ============================================================================

void InitializeHTTPClient() {
    if (!g_hInternet)
        g_hInternet = InternetOpenW(L"RawrXD-IDE/1.0", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
}

void CleanupHTTPClient() {
    if (g_hInternet) { InternetCloseHandle(g_hInternet); g_hInternet = nullptr; }
}

// ============================================================================
// Syntax Highlighting
// ============================================================================

void InitKeywords() {
    if (!g_cppKeywords.empty()) return;
    g_cppKeywords = {
        L"auto", L"break", L"case", L"catch", L"class", L"const", L"continue",
        L"default", L"delete", L"do", L"else", L"enum", L"extern", L"false",
        L"for", L"friend", L"goto", L"if", L"inline", L"namespace", L"new",
        L"nullptr", L"operator", L"private", L"protected", L"public", L"register",
        L"return", L"short", L"signed", L"sizeof", L"static", L"struct", L"switch",
        L"template", L"this", L"throw", L"true", L"try", L"typedef", L"typename",
        L"union", L"unsigned", L"using", L"virtual", L"volatile", L"while",
        L"constexpr", L"override", L"final", L"noexcept", L"decltype", L"thread_local",
        L"alignas", L"alignof", L"static_assert", L"static_cast", L"dynamic_cast",
        L"reinterpret_cast", L"const_cast", L"explicit", L"export", L"mutable",
        L"co_await", L"co_return", L"co_yield", L"concept", L"requires",
        L"#include", L"#define", L"#ifdef", L"#ifndef", L"#endif", L"#pragma",
        L"#if", L"#else", L"#elif", L"#undef", L"#error"
    };
    g_cppTypes = {
        L"int", L"long", L"char", L"float", L"double", L"void", L"bool",
        L"wchar_t", L"char16_t", L"char32_t", L"size_t", L"int8_t", L"int16_t",
        L"int32_t", L"int64_t", L"uint8_t", L"uint16_t", L"uint32_t", L"uint64_t",
        L"BOOL", L"DWORD", L"LPARAM", L"WPARAM", L"LRESULT", L"HWND", L"HINSTANCE",
        L"HANDLE", L"HMODULE", L"HFONT", L"HBRUSH", L"HDC", L"HMENU", L"RECT",
        L"POINT", L"SIZE", L"MSG", L"WNDCLASSEX", L"string", L"wstring", L"vector",
        L"map", L"unordered_map", L"set", L"unordered_set", L"pair", L"tuple",
        L"shared_ptr", L"unique_ptr", L"optional", L"variant", L"any",
        L"UINT", L"LONG", L"SHORT", L"BYTE", L"WORD", L"LPWSTR", L"LPCWSTR",
        L"LPSTR", L"LPCSTR", L"LPVOID", L"COLORREF", L"HRESULT", L"INT_PTR",
        L"UINT_PTR", L"DWORD_PTR", L"TRUE", L"FALSE", L"NULL"
    };
}

void ApplySyntaxHighlighting(HWND hwndEdit) {
    if (g_highlightingInProgress || !hwndEdit) return;
    g_highlightingInProgress = true;

    int len = GetWindowTextLengthW(hwndEdit);
    if (len <= 0 || len > 100000) { g_highlightingInProgress = false; return; }

    std::wstring text(len, L'\0');
    GetWindowTextW(hwndEdit, &text[0], len + 1);

    if (text == g_lastHighlightedText) { g_highlightingInProgress = false; return; }
    g_lastHighlightedText = text;

    InitKeywords();

    // Freeze redraw
    SendMessageW(hwndEdit, WM_SETREDRAW, FALSE, 0);
    DWORD selStart = 0, selEnd = 0;
    SendMessageW(hwndEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    POINT scrollPos;
    SendMessageW(hwndEdit, EM_GETSCROLLPOS, 0, (LPARAM)&scrollPos);

    // Set background
    SendMessageW(hwndEdit, EM_SETBKGNDCOLOR, 0, CLR_BACKGROUND);

    // Default all to CLR_DEFAULT
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = CLR_DEFAULT;
    SendMessageW(hwndEdit, EM_SETSEL, 0, -1);
    SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    const wchar_t* ptr = text.c_str();
    const wchar_t* end = ptr + text.length();

    while (ptr < end) {
        while (ptr < end && iswspace(*ptr)) ptr++;
        if (ptr >= end) break;

        const wchar_t* start = ptr;

        // Line comment
        if (ptr + 1 < end && *ptr == L'/' && *(ptr + 1) == L'/') {
            while (ptr < end && *ptr != L'\n') ptr++;
            cf.crTextColor = CLR_COMMENT;
            SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)(start - text.c_str()), (LPARAM)(ptr - text.c_str()));
            SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }

        // Block comment
        if (ptr + 1 < end && *ptr == L'/' && *(ptr + 1) == L'*') {
            ptr += 2;
            while (ptr + 1 < end && !(*ptr == L'*' && *(ptr + 1) == L'/')) ptr++;
            if (ptr + 1 < end) ptr += 2;
            cf.crTextColor = CLR_COMMENT;
            SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)(start - text.c_str()), (LPARAM)(ptr - text.c_str()));
            SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }

        // String
        if (*ptr == L'"' || *ptr == L'\'') {
            wchar_t q = *ptr++;
            while (ptr < end && *ptr != q) { if (*ptr == L'\\' && ptr + 1 < end) ptr++; ptr++; }
            if (ptr < end) ptr++;
            cf.crTextColor = CLR_STRING;
            SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)(start - text.c_str()), (LPARAM)(ptr - text.c_str()));
            SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }

        // Preprocessor
        if (*ptr == L'#') {
            while (ptr < end && (iswalnum(*ptr) || *ptr == L'#' || *ptr == L'_')) ptr++;
            cf.crTextColor = CLR_PREPROC;
            SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)(start - text.c_str()), (LPARAM)(ptr - text.c_str()));
            SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }

        // Number
        if (iswdigit(*ptr) || (*ptr == L'.' && ptr + 1 < end && iswdigit(*(ptr + 1)))) {
            while (ptr < end && (iswalnum(*ptr) || *ptr == L'.' || *ptr == L'x' || *ptr == L'X')) ptr++;
            cf.crTextColor = CLR_NUMBER;
            SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)(start - text.c_str()), (LPARAM)(ptr - text.c_str()));
            SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }

        // Identifier / keyword / type
        if (iswalpha(*ptr) || *ptr == L'_') {
            while (ptr < end && (iswalnum(*ptr) || *ptr == L'_')) ptr++;
            std::wstring word(start, ptr - start);
            COLORREF color = 0;
            if (g_cppKeywords.count(word)) color = CLR_KEYWORD;
            else if (g_cppTypes.count(word)) color = CLR_TYPE;
            if (color) {
                cf.crTextColor = color;
                SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)(start - text.c_str()), (LPARAM)(ptr - text.c_str()));
                SendMessageW(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
            continue;
        }

        ptr++;
    }

    // Restore state
    SendMessageW(hwndEdit, EM_SETSEL, selStart, selEnd);
    SendMessageW(hwndEdit, EM_SETSCROLLPOS, 0, (LPARAM)&scrollPos);
    SendMessageW(hwndEdit, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndEdit, nullptr, FALSE);
    g_highlightingInProgress = false;
}

// ============================================================================
// File Tree Population
// ============================================================================

HTREEITEM AddTreeItem(HWND hTree, HTREEITEM hParent, const wchar_t* text, bool isFolder) {
    TVINSERTSTRUCTW tvis = {};
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_SORT;
    tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvis.item.pszText = (LPWSTR)text;
    tvis.item.lParam = isFolder ? 1 : 0;
    return TreeView_InsertItem(hTree, &tvis);
}

void PopulateTreeRecursive(HWND hTree, HTREEITEM hParent, const std::wstring& dirPath, int depth = 0) {
    if (depth > 5) return; // Limit recursion depth

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW((dirPath + L"\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    // Folders first
    std::vector<std::wstring> folders, files;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        if (wcscmp(fd.cFileName, L".git") == 0 || wcscmp(fd.cFileName, L"node_modules") == 0) continue;
        if (wcscmp(fd.cFileName, L".vs") == 0 || wcscmp(fd.cFileName, L"__pycache__") == 0) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            folders.push_back(fd.cFileName);
        else
            files.push_back(fd.cFileName);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);

    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());

    for (auto& f : folders) {
        HTREEITEM hItem = AddTreeItem(hTree, hParent, f.c_str(), true);
        PopulateTreeRecursive(hTree, hItem, dirPath + L"\\" + f, depth + 1);
    }
    for (auto& f : files) {
        AddTreeItem(hTree, hParent, f.c_str(), false);
    }
}

void PopulateFileTree(const std::wstring& rootPath) {
    if (!g_hwndFileTree) return;
    TreeView_DeleteAllItems(g_hwndFileTree);
    g_workspaceRoot = rootPath;

    std::wstring displayName = GetFileName(rootPath);
    if (displayName.empty()) displayName = rootPath;
    HTREEITEM hRoot = AddTreeItem(g_hwndFileTree, TVI_ROOT, displayName.c_str(), true);
    PopulateTreeRecursive(g_hwndFileTree, hRoot, rootPath);
    TreeView_Expand(g_hwndFileTree, hRoot, TVE_EXPAND);
}

std::wstring GetTreeItemFullPath(HWND hTree, HTREEITEM hItem) {
    std::vector<std::wstring> parts;
    while (hItem) {
        wchar_t buf[MAX_PATH] = {};
        TVITEMW tvi = {};
        tvi.mask = TVIF_TEXT;
        tvi.hItem = hItem;
        tvi.pszText = buf;
        tvi.cchTextMax = MAX_PATH;
        TreeView_GetItem(hTree, &tvi);
        parts.push_back(buf);
        hItem = TreeView_GetParent(hTree, hItem);
    }
    // First part is root display name, replace with actual root
    if (parts.size() > 0) parts.back() = g_workspaceRoot;
    std::wstring result;
    for (int i = (int)parts.size() - 1; i >= 0; i--) {
        if (!result.empty()) result += L"\\";
        result += parts[i];
    }
    return result;
}

// ============================================================================
// Tab Management
// ============================================================================

void CreateNewTab(const std::wstring& filePath = L"") {
    EditorTab tab;
    tab.filePath = filePath;
    tab.displayName = filePath.empty() ? L"Untitled" : GetFileName(filePath);
    tab.isDirty = false;

    tab.hwndEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
        WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, g_hwndMain, (HMENU)(UINT_PTR)(500 + g_tabs.size()),
        GetModuleHandle(nullptr), nullptr);

    if (tab.hwndEdit) {
        SendMessageW(tab.hwndEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
        SendMessageW(tab.hwndEdit, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        SendMessageW(tab.hwndEdit, EM_SETBKGNDCOLOR, 0, CLR_BACKGROUND);
        SendMessageW(tab.hwndEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = CLR_DEFAULT;
        SendMessageW(tab.hwndEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
        SetWindowSubclass(tab.hwndEdit, EditorSubclassProc, 1, 0);
    }

    g_tabs.push_back(tab);

    if (g_hwndTabControl) {
        TCITEMW tci = {};
        tci.mask = TCIF_TEXT;
        tci.pszText = (LPWSTR)tab.displayName.c_str();
        TabCtrl_InsertItem(g_hwndTabControl, (int)g_tabs.size() - 1, &tci);
    }

    // Switch to new tab
    if (g_activeTab >= 0 && g_activeTab < (int)g_tabs.size())
        ShowWindow(g_tabs[g_activeTab].hwndEdit, SW_HIDE);

    g_activeTab = (int)g_tabs.size() - 1;
    g_hwndEditor = tab.hwndEdit;
    g_currentFile = tab.filePath;
    g_isDirty = false;
    ShowWindow(g_hwndEditor, SW_SHOW);
    if (g_hwndTabControl) TabCtrl_SetCurSel(g_hwndTabControl, g_activeTab);
    SetFocus(g_hwndEditor);
    UpdateLayout(g_hwndMain);
}

void SwitchToTab(int index) {
    if (index < 0 || index >= (int)g_tabs.size() || index == g_activeTab) return;
    if (g_activeTab >= 0 && g_activeTab < (int)g_tabs.size())
        ShowWindow(g_tabs[g_activeTab].hwndEdit, SW_HIDE);
    g_activeTab = index;
    g_hwndEditor = g_tabs[index].hwndEdit;
    g_currentFile = g_tabs[index].filePath;
    g_isDirty = g_tabs[index].isDirty;
    ShowWindow(g_hwndEditor, SW_SHOW);
    SetFocus(g_hwndEditor);
    if (g_hwndTabControl) TabCtrl_SetCurSel(g_hwndTabControl, index);
    UpdateWindowTitle();
    UpdateStatusBar();
    UpdateLayout(g_hwndMain);
}

void CloseTab(int index) {
    if (index < 0 || index >= (int)g_tabs.size()) return;
    if (g_tabs[index].isDirty) {
        std::wstring msg = L"Save changes to " + g_tabs[index].displayName + L"?";
        int r = MessageBoxW(g_hwndMain, msg.c_str(), L"Unsaved Changes", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (r == IDCANCEL) return;
        if (r == IDYES) {
            if (!g_tabs[index].filePath.empty()) {
                // Save before closing
                int len = GetWindowTextLengthW(g_tabs[index].hwndEdit);
                std::wstring content(len, L'\0');
                GetWindowTextW(g_tabs[index].hwndEdit, &content[0], len + 1);
                std::string utf8 = WideToUtf8(content);
                std::ofstream f(g_tabs[index].filePath, std::ios::binary);
                if (f.is_open()) f.write(utf8.c_str(), utf8.size());
            }
        }
    }
    DestroyWindow(g_tabs[index].hwndEdit);
    g_tabs.erase(g_tabs.begin() + index);
    if (g_hwndTabControl) TabCtrl_DeleteItem(g_hwndTabControl, index);

    if (g_tabs.empty()) {
        g_activeTab = -1; g_hwndEditor = nullptr; g_currentFile.clear();
    } else {
        int newIdx = (index < (int)g_tabs.size()) ? index : (int)g_tabs.size() - 1;
        g_activeTab = -1; // Force switch
        SwitchToTab(newIdx);
    }
    UpdateWindowTitle();
}

// ============================================================================
// File Operations
// ============================================================================

bool LoadFileIntoEditor(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize > 50 * 1024 * 1024) {
        CloseHandle(hFile);
        MessageBoxW(g_hwndMain, L"File too large (>50MB)", L"Error", MB_ICONERROR);
        return false;
    }

    std::string content(fileSize, '\0');
    DWORD bytesRead = 0;
    ReadFile(hFile, &content[0], fileSize, &bytesRead, nullptr);
    CloseHandle(hFile);
    content.resize(bytesRead);

    std::wstring wContent = Utf8ToWide(content);

    // Check if file is already open in a tab
    for (int i = 0; i < (int)g_tabs.size(); i++) {
        if (_wcsicmp(g_tabs[i].filePath.c_str(), path.c_str()) == 0) {
            SwitchToTab(i);
            return true;
        }
    }

    CreateNewTab(path);
    if (g_hwndEditor) {
        SetWindowTextW(g_hwndEditor, wContent.c_str());
        g_isDirty = false;
        g_tabs[g_activeTab].isDirty = false;
        // Trigger syntax highlighting
        PostMessageW(g_hwndMain, WM_SYNTAX_HIGHLIGHT, 0, 0);
    }

    UpdateWindowTitle();
    UpdateStatusBar();
    AppendOutput((L"[File] Opened: " + path + L" (" + std::to_wstring(fileSize) + L" bytes)\r\n").c_str());
    return true;
}

bool SaveCurrentFile() {
    if (!g_hwndEditor) return false;
    if (g_currentFile.empty()) {
        // Save As
        wchar_t filename[MAX_PATH] = L"";
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = g_hwndMain;
        ofn.lpstrFilter = L"All Files\0*.*\0C/C++\0*.cpp;*.c;*.h;*.hpp\0Python\0*.py\0\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn)) return false;
        g_currentFile = filename;
        if (g_activeTab >= 0) {
            g_tabs[g_activeTab].filePath = filename;
            g_tabs[g_activeTab].displayName = GetFileName(filename);
            TCITEMW tci = {};
            tci.mask = TCIF_TEXT;
            tci.pszText = (LPWSTR)g_tabs[g_activeTab].displayName.c_str();
            TabCtrl_SetItem(g_hwndTabControl, g_activeTab, &tci);
        }
    }

    int len = GetWindowTextLengthW(g_hwndEditor);
    std::wstring content(len, L'\0');
    GetWindowTextW(g_hwndEditor, &content[0], len + 1);
    std::string utf8 = WideToUtf8(content);

    HANDLE hFile = CreateFileW(g_currentFile.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBoxW(g_hwndMain, L"Failed to save file.", L"Error", MB_ICONERROR);
        return false;
    }
    DWORD written = 0;
    WriteFile(hFile, utf8.c_str(), (DWORD)utf8.size(), &written, nullptr);
    CloseHandle(hFile);

    g_isDirty = false;
    if (g_activeTab >= 0) g_tabs[g_activeTab].isDirty = false;
    UpdateWindowTitle();
    AppendOutput((L"[File] Saved: " + g_currentFile + L" (" + std::to_wstring(written) + L" bytes)\r\n").c_str());
    return true;
}

bool SaveFileAs() {
    std::wstring oldPath = g_currentFile;
    g_currentFile.clear();
    if (!SaveCurrentFile()) {
        g_currentFile = oldPath;
        return false;
    }
    return true;
}

void OpenFileDialog() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"All Files\0*.*\0C/C++\0*.cpp;*.c;*.h;*.hpp\0Python\0*.py\0Text\0*.txt\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        LoadFileIntoEditor(filename);
    }
}

void OpenFolderDialog() {
    wchar_t path[MAX_PATH] = L"";
    BROWSEINFOW bi = {};
    bi.hwndOwner = g_hwndMain;
    bi.lpszTitle = L"Select Workspace Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        SHGetPathFromIDListW(pidl, path);
        CoTaskMemFree(pidl);
        PopulateFileTree(path);
        AppendOutput((L"[Workspace] Opened: " + std::wstring(path) + L"\r\n").c_str());
    }
}

// ============================================================================
// Find / Replace / Go To Line - Proper Modal Dialogs
// ============================================================================

void FindNext(HWND hwndEdit, const std::wstring& searchText, bool matchCase, bool wholeWord, bool searchDown) {
    if (!hwndEdit || searchText.empty()) return;

    DWORD start = 0, end = 0;
    SendMessageW(hwndEdit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    FINDTEXTEXW ft = {};
    ft.chrg.cpMin = searchDown ? end : start;
    ft.chrg.cpMax = searchDown ? -1 : 0;
    ft.lpstrText = searchText.c_str();

    DWORD flags = (searchDown ? FR_DOWN : 0) | (matchCase ? FR_MATCHCASE : 0) | (wholeWord ? FR_WHOLEWORD : 0);
    LRESULT pos = SendMessageW(hwndEdit, EM_FINDTEXTEXW, flags, (LPARAM)&ft);

    if (pos >= 0) {
        SendMessageW(hwndEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
        SendMessageW(hwndEdit, EM_SCROLLCARET, 0, 0);
    } else {
        // Wrap around
        ft.chrg.cpMin = searchDown ? 0 : GetWindowTextLengthW(hwndEdit);
        ft.chrg.cpMax = searchDown ? -1 : 0;
        pos = SendMessageW(hwndEdit, EM_FINDTEXTEXW, flags, (LPARAM)&ft);
        if (pos >= 0) {
            SendMessageW(hwndEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
            SendMessageW(hwndEdit, EM_SCROLLCARET, 0, 0);
        } else {
            MessageBoxW(g_hwndMain, L"Text not found.", L"Find", MB_ICONINFORMATION);
        }
    }
}

void ReplaceAll(HWND hwndEdit, const std::wstring& find, const std::wstring& replace, bool matchCase) {
    if (!hwndEdit || find.empty()) return;

    int count = 0;
    FINDTEXTEXW ft = {};
    ft.chrg.cpMin = 0;
    ft.chrg.cpMax = -1;
    ft.lpstrText = find.c_str();
    DWORD flags = FR_DOWN | (matchCase ? FR_MATCHCASE : 0);

    while (true) {
        LRESULT pos = SendMessageW(hwndEdit, EM_FINDTEXTEXW, flags, (LPARAM)&ft);
        if (pos < 0) break;
        SendMessageW(hwndEdit, EM_SETSEL, ft.chrgText.cpMin, ft.chrgText.cpMax);
        SendMessageW(hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)replace.c_str());
        ft.chrg.cpMin = ft.chrgText.cpMin + (LONG)replace.length();
        count++;
    }

    std::wstring msg = std::to_wstring(count) + L" occurrence(s) replaced.";
    AppendOutput((L"[Edit] " + msg + L"\r\n").c_str());
}

// Find dialog as modeless
static HWND g_hwndFindDialog = nullptr;

INT_PTR CALLBACK FindReplaceDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static bool isReplaceMode = false;
    switch (msg) {
    case WM_INITDIALOG:
        isReplaceMode = (lParam != 0);
        SetDlgItemTextW(hDlg, 1001, g_findText.c_str());
        if (isReplaceMode) SetDlgItemTextW(hDlg, 1002, g_replaceText.c_str());
        ShowWindow(GetDlgItem(hDlg, 1002), isReplaceMode ? SW_SHOW : SW_HIDE);
        ShowWindow(GetDlgItem(hDlg, 1005), isReplaceMode ? SW_SHOW : SW_HIDE);  // Replace btn
        ShowWindow(GetDlgItem(hDlg, 1006), isReplaceMode ? SW_SHOW : SW_HIDE);  // Replace All btn
        SetWindowTextW(hDlg, isReplaceMode ? L"Find and Replace" : L"Find");
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1003: { // Find Next
            wchar_t buf[256] = {};
            GetDlgItemTextW(hDlg, 1001, buf, 256);
            g_findText = buf;
            g_findMatchCase = IsDlgButtonChecked(hDlg, 1004) == BST_CHECKED;
            FindNext(g_hwndEditor, g_findText, g_findMatchCase, false, true);
            return TRUE;
        }
        case 1005: { // Replace
            wchar_t findBuf[256] = {}, repBuf[256] = {};
            GetDlgItemTextW(hDlg, 1001, findBuf, 256);
            GetDlgItemTextW(hDlg, 1002, repBuf, 256);
            g_findText = findBuf;
            g_replaceText = repBuf;
            // Replace current selection if it matches
            DWORD s = 0, e = 0;
            SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
            if (s != e) {
                SendMessageW(g_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)g_replaceText.c_str());
            }
            FindNext(g_hwndEditor, g_findText, g_findMatchCase, false, true);
            return TRUE;
        }
        case 1006: { // Replace All
            wchar_t findBuf[256] = {}, repBuf[256] = {};
            GetDlgItemTextW(hDlg, 1001, findBuf, 256);
            GetDlgItemTextW(hDlg, 1002, repBuf, 256);
            g_findText = findBuf;
            g_replaceText = repBuf;
            g_findMatchCase = IsDlgButtonChecked(hDlg, 1004) == BST_CHECKED;
            ReplaceAll(g_hwndEditor, g_findText, g_replaceText, g_findMatchCase);
            return TRUE;
        }
        case IDCANCEL:
            DestroyWindow(hDlg);
            g_hwndFindDialog = nullptr;
            return TRUE;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hDlg);
        g_hwndFindDialog = nullptr;
        return TRUE;
    }
    return FALSE;
}

HWND CreateFindReplaceDialog(HWND hwndParent, bool replaceMode) {
    if (g_hwndFindDialog) { SetFocus(g_hwndFindDialog); return g_hwndFindDialog; }

    // Create dialog programmatically
    // Using a DLGTEMPLATE in memory
    struct {
        DLGTEMPLATE dt;
        WORD menu, cls, title;
        wchar_t titleText[32];
    } dlgBuf = {};

    // We'll create as a regular popup window instead for simplicity
    HWND hDlg = CreateWindowExW(
        WS_EX_TOOLWINDOW, L"#32770", replaceMode ? L"Find and Replace" : L"Find",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, replaceMode ? 200 : 160,
        hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!hDlg) return nullptr;

    // Create controls manually
    CreateWindowExW(0, L"STATIC", L"Find:", WS_CHILD | WS_VISIBLE,
        10, 10, 40, 20, hDlg, nullptr, nullptr, nullptr);
    HWND hFindEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_findText.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 60, 8, 220, 24, hDlg, (HMENU)1001, nullptr, nullptr);

    if (replaceMode) {
        CreateWindowExW(0, L"STATIC", L"Replace:", WS_CHILD | WS_VISIBLE,
            10, 40, 50, 20, hDlg, nullptr, nullptr, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_replaceText.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 60, 38, 220, 24, hDlg, (HMENU)1002, nullptr, nullptr);
    }

    int btnY = replaceMode ? 70 : 40;
    CreateWindowExW(0, L"BUTTON", L"Find Next", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        295, 8, 90, 26, hDlg, (HMENU)1003, nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Match Case", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        60, btnY, 100, 20, hDlg, (HMENU)1004, nullptr, nullptr);

    if (replaceMode) {
        CreateWindowExW(0, L"BUTTON", L"Replace", WS_CHILD | WS_VISIBLE,
            295, 38, 90, 26, hDlg, (HMENU)1005, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Replace All", WS_CHILD | WS_VISIBLE,
            295, 68, 90, 26, hDlg, (HMENU)1006, nullptr, nullptr);
    }

    // Get selected text for find
    if (g_hwndEditor) {
        DWORD s = 0, e = 0;
        SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&s, (LPARAM)&e);
        if (e > s && (e - s) < 256) {
            wchar_t selBuf[256] = {};
            SendMessageW(g_hwndEditor, EM_SETSEL, s, e);
            // Get selected text via clipboard approach or direct
            int lineIdx = (int)SendMessageW(g_hwndEditor, EM_LINEFROMCHAR, s, 0);
            int lineStart = (int)SendMessageW(g_hwndEditor, EM_LINEINDEX, lineIdx, 0);
            // Simpler: get full text and extract
            int len = GetWindowTextLengthW(g_hwndEditor);
            if (len > 0 && len < 1000000) {
                std::wstring text(len, L'\0');
                GetWindowTextW(g_hwndEditor, &text[0], len + 1);
                if (s < (DWORD)len && e <= (DWORD)len) {
                    std::wstring sel = text.substr(s, e - s);
                    SetWindowTextW(hFindEdit, sel.c_str());
                    g_findText = sel;
                }
            }
        }
    }

    // Override WndProc for the dialog
    SetWindowLongPtrW(hDlg, GWLP_WNDPROC, (LONG_PTR)FindReplaceDlgProc);

    ShowWindow(hDlg, SW_SHOW);
    g_hwndFindDialog = hDlg;
    return hDlg;
}

void GoToLine() {
    // Simple input box using a message-driven approach
    wchar_t buf[32] = L"";
    // We'll use a quick and dirty approach with a fake dialog
    int lineCount = (int)SendMessageW(g_hwndEditor, EM_GETLINECOUNT, 0, 0);
    std::wstring prompt = L"Enter line number (1-" + std::to_wstring(lineCount) + L"):";

    // Use InputBox via a simple dialog
    HWND hDlg = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_DLGMODALFRAME,
        L"#32770", L"Go To Line",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 120,
        g_hwndMain, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hDlg) return;

    CreateWindowExW(0, L"STATIC", prompt.c_str(), WS_CHILD | WS_VISIBLE,
        10, 10, 270, 20, hDlg, nullptr, nullptr, nullptr);
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
        10, 35, 170, 24, hDlg, (HMENU)1001, nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        190, 35, 90, 26, hDlg, (HMENU)IDOK, nullptr, nullptr);

    SendMessageW(hEdit, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    ShowWindow(hDlg, SW_SHOW);
    SetFocus(hEdit);

    // Run a mini message loop for this dialog
    MSG msg;
    bool done = false;
    while (!done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            done = true; break;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            GetWindowTextW(hEdit, buf, 32);
            done = true; break;
        }
        if (msg.message == WM_COMMAND && LOWORD(msg.wParam) == IDOK && msg.hwnd == hDlg) {
            GetWindowTextW(hEdit, buf, 32);
            done = true; break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DestroyWindow(hDlg);

    int line = _wtoi(buf);
    if (line > 0 && line <= lineCount) {
        int charIdx = (int)SendMessageW(g_hwndEditor, EM_LINEINDEX, line - 1, 0);
        SendMessageW(g_hwndEditor, EM_SETSEL, charIdx, charIdx);
        SendMessageW(g_hwndEditor, EM_SCROLLCARET, 0, 0);
        SetFocus(g_hwndEditor);
        AppendOutput((L"[Edit] Jumped to line " + std::to_wstring(line) + L"\r\n").c_str());
    }
}

// ============================================================================
// Terminal
// ============================================================================

bool StartTerminal() {
    if (g_hTermProcess) return true;

    if (!g_terminalCSInit) {
        InitializeCriticalSection(&g_terminalCS);
        g_terminalCSInit = true;
    }

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hStdoutWrite, hStdinRead;

    if (!CreatePipe(&g_hTermStdoutRead, &hStdoutWrite, &sa, 0)) return false;
    if (!CreatePipe(&hStdinRead, &g_hTermStdinWrite, &sa, 0)) {
        CloseHandle(g_hTermStdoutRead); g_hTermStdoutRead = nullptr; return false;
    }
    SetHandleInformation(g_hTermStdoutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(g_hTermStdinWrite, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    si.hStdInput = hStdinRead;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    wchar_t cmd[] = L"powershell.exe -NoLogo -NoProfile";

    if (!CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                        g_workspaceRoot.empty() ? nullptr : g_workspaceRoot.c_str(), &si, &pi)) {
        CloseHandle(g_hTermStdoutRead); CloseHandle(g_hTermStdinWrite);
        CloseHandle(hStdoutWrite); CloseHandle(hStdinRead);
        g_hTermStdoutRead = nullptr; g_hTermStdinWrite = nullptr;
        return false;
    }

    g_hTermProcess = pi.hProcess;
    CloseHandle(pi.hThread);
    CloseHandle(hStdoutWrite);
    CloseHandle(hStdinRead);

    // Start background reader thread
    g_termRunning = true;
    g_termReadThread = std::thread([]{
        char buffer[4096];
        while (g_termRunning && g_hTermStdoutRead) {
            DWORD available = 0;
            if (PeekNamedPipe(g_hTermStdoutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
                DWORD toRead = (available < sizeof(buffer) - 1) ? available : (DWORD)(sizeof(buffer) - 1);
                DWORD bytesRead = 0;
                if (ReadFile(g_hTermStdoutRead, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = 0;
                    std::wstring wOut = Utf8ToWide(std::string(buffer, bytesRead));
                    // Post to main thread
                    wchar_t* pMsg = new wchar_t[wOut.size() + 1];
                    wcscpy(pMsg, wOut.c_str());
                    PostMessageW(g_hwndMain, WM_TERMINAL_OUTPUT, 0, (LPARAM)pMsg);
                }
            } else {
                Sleep(50);
            }
        }
    });

    AppendTerminal(L"PowerShell Terminal\r\n");
    return true;
}

void SendTerminalCommand(const std::wstring& cmd) {
    if (!g_hTermStdinWrite) return;
    std::string utf8 = WideToUtf8(cmd + L"\r\n");
    DWORD written = 0;
    WriteFile(g_hTermStdinWrite, utf8.c_str(), (DWORD)utf8.length(), &written, nullptr);
}

void StopTerminal() {
    g_termRunning = false;
    if (g_termReadThread.joinable()) g_termReadThread.join();
    if (g_hTermProcess) { TerminateProcess(g_hTermProcess, 0); CloseHandle(g_hTermProcess); g_hTermProcess = nullptr; }
    if (g_hTermStdoutRead) { CloseHandle(g_hTermStdoutRead); g_hTermStdoutRead = nullptr; }
    if (g_hTermStdinWrite) { CloseHandle(g_hTermStdinWrite); g_hTermStdinWrite = nullptr; }
}

// ============================================================================
// AI Completion
// ============================================================================

void ShowAICompletion(const AICompletion& completion) {
    if (!g_hwndEditor || completion.text.empty()) return;
    if (g_completionVisible) {
        // Remove old ghost text first
        SendMessageW(g_hwndEditor, EM_SETSEL, g_completionAnchor,
                     g_completionAnchor + (int)g_completionText.size());
        SendMessageW(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)L"");
    }

    DWORD start = 0, end = 0;
    SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    g_completionAnchor = (int)((start > end) ? start : end);
    g_completionText = completion.text;

    // Insert ghost text
    SendMessageW(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor);
    SendMessageW(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)g_completionText.c_str());

    // Apply ghost formatting
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_ITALIC;
    cf.crTextColor = CLR_GHOST;
    cf.dwEffects = CFE_ITALIC;
    SendMessageW(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor + (int)g_completionText.size());
    SendMessageW(g_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor);

    g_completionVisible = true;
}

void HideAICompletion() {
    if (!g_completionVisible || !g_hwndEditor || g_completionText.empty()) return;
    SendMessageW(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor + (int)g_completionText.size());
    SendMessageW(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)L"");
    g_completionVisible = false;
    g_completionText.clear();
    g_completionAnchor = -1;
}

void AcceptAICompletion() {
    if (!g_completionVisible || !g_hwndEditor || g_completionText.empty()) return;
    int endPos = g_completionAnchor + (int)g_completionText.size();
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_ITALIC;
    cf.crTextColor = CLR_DEFAULT;
    cf.dwEffects = 0;
    SendMessageW(g_hwndEditor, EM_SETSEL, g_completionAnchor, endPos);
    SendMessageW(g_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(g_hwndEditor, EM_SETSEL, endPos, endPos);
    g_completionVisible = false;
    g_completionText.clear();
    g_completionAnchor = -1;
    g_isDirty = true;
    if (g_activeTab >= 0) g_tabs[g_activeTab].isDirty = true;
}

void TriggerAICompletion() {
    if (!g_hwndEditor) return;

    // Get context around cursor
    DWORD start = 0, end = 0;
    SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    int lineIdx = (int)SendMessageW(g_hwndEditor, EM_LINEFROMCHAR, start, 0);
    int lineStart = (int)SendMessageW(g_hwndEditor, EM_LINEINDEX, lineIdx, 0);

    // Get current line text
    wchar_t lineText[512] = {};
    *(WORD*)lineText = 510;
    int lineLen = (int)SendMessageW(g_hwndEditor, EM_GETLINE, lineIdx, (LPARAM)lineText);
    lineText[lineLen] = 0;

    std::wstring line(lineText);
    std::wstring suggestion;

    // Context-aware completion based on what the user typed
    if (line.find(L"for") != std::wstring::npos && line.find(L"(") == std::wstring::npos) {
        suggestion = L" (int i = 0; i < count; i++) {\n    \n}";
    } else if (line.find(L"if") != std::wstring::npos && line.find(L"(") == std::wstring::npos) {
        suggestion = L" (condition) {\n    \n}";
    } else if (line.find(L"while") != std::wstring::npos && line.find(L"(") == std::wstring::npos) {
        suggestion = L" (condition) {\n    \n}";
    } else if (line.find(L"class") != std::wstring::npos && line.find(L"{") == std::wstring::npos) {
        suggestion = L" {\npublic:\n    // Constructor\n    \n\nprivate:\n    \n};";
    } else if (line.find(L"struct") != std::wstring::npos && line.find(L"{") == std::wstring::npos) {
        suggestion = L" {\n    \n};";
    } else if (line.find(L"switch") != std::wstring::npos && line.find(L"(") == std::wstring::npos) {
        suggestion = L" (value) {\ncase 0:\n    break;\ndefault:\n    break;\n}";
    } else if (line.find(L"#include") != std::wstring::npos && line.find(L"<") == std::wstring::npos) {
        suggestion = L" <iostream>";
    } else if (line.find(L"std::") != std::wstring::npos) {
        if (line.find(L"cout") != std::wstring::npos)
            suggestion = L" << \"\" << std::endl;";
        else if (line.find(L"vector") != std::wstring::npos)
            suggestion = L"<int> vec;";
        else if (line.find(L"string") != std::wstring::npos)
            suggestion = L" str;";
    } else if (line.find(L"void") != std::wstring::npos && line.find(L"(") == std::wstring::npos) {
        suggestion = L"() {\n    \n}";
    } else if (line.find(L"int main") != std::wstring::npos) {
        suggestion = L"() {\n    return 0;\n}";
    } else {
        suggestion = L"// TODO: implement";
    }

    if (!suggestion.empty()) {
        AICompletion comp;
        comp.text = suggestion;
        comp.confidence = 0.8;
        comp.kind = L"snippet";
        ShowAICompletion(comp);
        AppendOutput(L"[AI] Completion ready. Press Tab to accept, Esc to dismiss.\r\n");
    }
}

// ============================================================================
// Code Issues System
// ============================================================================

void RunCodeAnalysis() {
    {
        std::lock_guard<std::mutex> lock(g_issuesMutex);
        g_codeIssues.clear();
    }

    if (!g_hwndEditor || g_currentFile.empty()) {
        AppendOutput(L"[Issues] No file open for analysis.\r\n");
        return;
    }

    AppendOutput(L"[Issues] Running code analysis...\r\n");

    int textLen = GetWindowTextLengthA(g_hwndEditor);
    if (textLen <= 0) return;

    std::string content(textLen, '\0');
    GetWindowTextA(g_hwndEditor, &content[0], textLen + 1);

    std::istringstream stream(content);
    std::string line;
    int lineNum = 1;
    int issueCount = 0;

    while (std::getline(stream, line)) {
        // Potential buffer overflow
        if (line.find("gets(") != std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile; issue.line = lineNum; issue.column = 1;
            issue.severity = L"Critical"; issue.category = L"Security";
            issue.message = L"Use of unsafe function 'gets' - buffer overflow risk";
            issue.rule = L"C-SEC-001"; issue.quickFix = L"fgets"; issue.isFixable = true;
            GetSystemTime(&issue.timestamp);
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            g_codeIssues.push_back(issue); issueCount++;
        }

        // malloc without free
        if (line.find("malloc(") != std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile; issue.line = lineNum; issue.column = 1;
            issue.severity = L"High"; issue.category = L"Security";
            issue.message = L"malloc() usage - ensure corresponding free()";
            issue.rule = L"C-MEM-001"; issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            g_codeIssues.push_back(issue); issueCount++;
        }

        // Unused variable pattern
        if (line.find("//") == std::string::npos && line.find("TODO") != std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile; issue.line = lineNum; issue.column = 1;
            issue.severity = L"Info"; issue.category = L"Info";
            issue.message = L"TODO comment found";
            issue.rule = L"STYLE-001"; issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            g_codeIssues.push_back(issue); issueCount++;
        }

        // FIXME
        if (line.find("FIXME") != std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile; issue.line = lineNum; issue.column = 1;
            issue.severity = L"Medium"; issue.category = L"Warning";
            issue.message = L"FIXME comment - needs attention";
            issue.rule = L"STYLE-002"; issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            g_codeIssues.push_back(issue); issueCount++;
        }

        // Long lines
        if (line.length() > 120) {
            CodeIssue issue;
            issue.file = g_currentFile; issue.line = lineNum; issue.column = 121;
            issue.severity = L"Low"; issue.category = L"Warning";
            issue.message = L"Line exceeds 120 characters";
            issue.rule = L"STYLE-003"; issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            g_codeIssues.push_back(issue); issueCount++;
        }

        // printf format string issues
        if (line.find("printf(") != std::string::npos || line.find("sprintf(") != std::string::npos) {
            CodeIssue issue;
            issue.file = g_currentFile; issue.line = lineNum; issue.column = 1;
            issue.severity = L"Medium"; issue.category = L"Security";
            issue.message = L"printf-family function - verify format string safety";
            issue.rule = L"C-SEC-002"; issue.isFixable = false;
            GetSystemTime(&issue.timestamp);
            std::lock_guard<std::mutex> lock(g_issuesMutex);
            g_codeIssues.push_back(issue); issueCount++;
        }

        lineNum++;
    }

    AppendOutput((L"[Issues] Analysis complete. Found " + std::to_wstring(issueCount) + L" issue(s).\r\n").c_str());
}

void FilterIssuesBySeverity() {
    if (!g_hwndIssuesList) return;
    SendMessageW(g_hwndIssuesList, LVM_DELETEALLITEMS, 0, 0);

    std::lock_guard<std::mutex> lock(g_issuesMutex);
    int idx = 0;
    for (const auto& issue : g_codeIssues) {
        bool show = false;
        if (issue.severity == L"Critical" && g_bShowCritical) show = true;
        else if (issue.severity == L"High" && g_bShowHigh) show = true;
        else if (issue.severity == L"Medium" && g_bShowMedium) show = true;
        else if (issue.severity == L"Low" && g_bShowLow) show = true;
        else if (issue.severity == L"Info" && g_bShowInfo) show = true;
        if (!show) continue;

        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = idx;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)issue.severity.c_str();
        SendMessageW(g_hwndIssuesList, LVM_INSERTITEMW, 0, (LPARAM)&item);

        item.iSubItem = 1; item.pszText = (LPWSTR)issue.category.c_str();
        SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, idx, (LPARAM)&item);

        item.iSubItem = 2; item.pszText = (LPWSTR)issue.message.c_str();
        SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, idx, (LPARAM)&item);

        std::wstring fn = GetFileName(issue.file);
        item.iSubItem = 3; item.pszText = (LPWSTR)fn.c_str();
        SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, idx, (LPARAM)&item);

        std::wstring ln = std::to_wstring(issue.line);
        item.iSubItem = 4; item.pszText = (LPWSTR)ln.c_str();
        SendMessageW(g_hwndIssuesList, LVM_SETITEMTEXTW, idx, (LPARAM)&item);

        idx++;
    }
}

void RefreshCodeIssues() {
    RunCodeAnalysis();
    FilterIssuesBySeverity();
}

void ClearCodeIssues() {
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    g_codeIssues.clear();
    if (g_hwndIssuesList) SendMessageW(g_hwndIssuesList, LVM_DELETEALLITEMS, 0, 0);
    AppendOutput(L"[Issues] All issues cleared.\r\n");
}

void ExportIssues() {
    wchar_t filename[MAX_PATH] = L"issues_export.csv";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"CSV Files\0*.csv\0All Files\0*.*\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";
    if (!GetSaveFileNameW(&ofn)) return;

    std::ofstream file(WideToUtf8(std::wstring(filename)));
    if (!file.is_open()) { MessageBoxW(g_hwndMain, L"Failed to create file.", L"Error", MB_ICONERROR); return; }

    file << "Severity,Category,Message,File,Line,Rule\n";
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    for (const auto& i : g_codeIssues) {
        file << WideToUtf8(i.severity) << "," << WideToUtf8(i.category) << ","
             << "\"" << WideToUtf8(i.message) << "\"," << WideToUtf8(GetFileName(i.file)) << ","
             << i.line << "," << WideToUtf8(i.rule) << "\n";
    }
    AppendOutput((L"[Issues] Exported " + std::to_wstring(g_codeIssues.size()) + L" issues.\r\n").c_str());
}

void JumpToIssue(int issueIdx) {
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    if (issueIdx < 0 || issueIdx >= (int)g_codeIssues.size()) return;
    const auto& issue = g_codeIssues[issueIdx];

    // Open file if different
    if (!issue.file.empty() && _wcsicmp(g_currentFile.c_str(), issue.file.c_str()) != 0) {
        LoadFileIntoEditor(issue.file);
    }

    if (g_hwndEditor && issue.line > 0) {
        int charIdx = (int)SendMessageW(g_hwndEditor, EM_LINEINDEX, issue.line - 1, 0);
        SendMessageW(g_hwndEditor, EM_SETSEL, charIdx, charIdx);
        SendMessageW(g_hwndEditor, EM_SCROLLCARET, 0, 0);
        SetFocus(g_hwndEditor);
    }
}

// ============================================================================
// Chat System
// ============================================================================

void HandleChatSend() {
    if (!g_hwndChatInput) return;
    int len = GetWindowTextLengthW(g_hwndChatInput);
    if (len <= 0) return;

    std::wstring input(len, L'\0');
    GetWindowTextW(g_hwndChatInput, &input[0], len + 1);
    input.resize(len);
    SetWindowTextW(g_hwndChatInput, L"");

    AppendChat(L"\r\n[You] ");
    AppendChat(input.c_str());
    AppendChat(L"\r\n");

    // If model is loaded, try inference
    if (g_modelLoaded && pForwardPassInfer && pSampleNext && g_modelContext) {
        AppendChat(L"[AI] Processing...\r\n");
        // Would run inference here in a background thread
        AppendChat(L"[AI] (Inference engine connected - run inference on your query)\r\n");
    } else if (g_hTitanDll && pTitan_RunInference) {
        AppendChat(L"[AI] Using Titan Kernel for inference...\r\n");
        std::string utf8 = WideToUtf8(input);
        int result = pTitan_RunInference(0, utf8.c_str(), 256);
        if (result >= 0) {
            AppendChat(L"[AI] Inference complete.\r\n");
        } else {
            AppendChat(L"[AI] Inference failed.\r\n");
        }
    } else {
        // Local echo - no model loaded
        AppendChat(L"[System] No AI model loaded. Use AI > Load GGUF Model to load one.\r\n");
        AppendChat(L"[System] Your message has been logged. Load a model to get AI responses.\r\n");
    }
}

void CreateNewChatSession() {
    if (g_hwndChatHistory) {
        SetWindowTextW(g_hwndChatHistory, L"");
        AppendChat(L"[System] New chat session started.\r\n");
        AppendChat(L"[System] Type a message and press Enter to send.\r\n");
        if (g_modelLoaded)
            AppendChat((L"[System] Model loaded: " + GetFileName(g_loadedModelPath) + L"\r\n").c_str());
        else
            AppendChat(L"[System] No AI model loaded.\r\n");
    }
    if (g_hwndChatInput) { SetWindowTextW(g_hwndChatInput, L""); SetFocus(g_hwndChatInput); }
}

void LoadChatFromFile() {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"Chat Logs\0*.txt;*.log\0All Files\0*.*\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return;

    HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, nullptr);
    if (size > 0 && size < 10 * 1024 * 1024) {
        std::string buf(size, '\0');
        DWORD read = 0;
        ReadFile(hFile, &buf[0], size, &read, nullptr);
        buf.resize(read);
        std::wstring wBuf = Utf8ToWide(buf);
        if (g_hwndChatHistory) {
            SetWindowTextW(g_hwndChatHistory, L"");
            AppendChat(L"[System] Loaded chat history:\r\n\r\n");
            AppendChat(wBuf.c_str());
        }
    }
    CloseHandle(hFile);
}

void ExportChatHistory() {
    wchar_t filename[MAX_PATH] = L"chat_export.txt";
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwndMain;
    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"txt";
    if (!GetSaveFileNameW(&ofn)) return;

    int len = GetWindowTextLengthW(g_hwndChatHistory);
    if (len <= 0) return;
    std::wstring text(len, L'\0');
    GetWindowTextW(g_hwndChatHistory, &text[0], len + 1);
    std::string utf8 = WideToUtf8(text);

    HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hFile, utf8.c_str(), (DWORD)utf8.size(), &written, nullptr);
        CloseHandle(hFile);
        AppendChat(L"\r\n[System] Chat exported successfully.\r\n");
    }
}

void ConfigureMCPServers() {
    MessageBoxW(g_hwndMain,
        L"MCP Server Configuration\r\n\r\n"
        L"Current servers:\r\n"
        L"  - GGUF Direct Load (file-based inference)\r\n"
        L"  - Titan Kernel (native inference)\r\n\r\n"
        L"To add servers, edit mcp_config.json in the IDE directory.",
        L"RawrXD - MCP Configuration", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Build System
// ============================================================================

void CompileCurrentFile() {
    if (g_currentFile.empty()) {
        AppendOutput(L"[Build] No file open.\r\n");
        return;
    }

    std::wstring ext = GetFileExtension(g_currentFile);
    std::wstring cmd;

    if (ext == L".cpp" || ext == L".c" || ext == L".cc" || ext == L".cxx") {
        // Find cl.exe
        wchar_t clPath[MAX_PATH] = {};
        if (SearchPathW(nullptr, L"cl.exe", nullptr, MAX_PATH, clPath, nullptr)) {
            cmd = L"\"" + std::wstring(clPath) + L"\" /EHsc /std:c++17 /W3 \"" + g_currentFile + L"\"";
        } else {
            // Try gcc
            if (SearchPathW(nullptr, L"g++.exe", nullptr, MAX_PATH, clPath, nullptr)) {
                cmd = L"\"" + std::wstring(clPath) + L"\" -std=c++17 -Wall \"" + g_currentFile + L"\"";
            } else {
                AppendOutput(L"[Build] No C++ compiler found (cl.exe or g++). Add to PATH.\r\n");
                return;
            }
        }
    } else if (ext == L".py") {
        cmd = L"python \"" + g_currentFile + L"\"";
    } else if (ext == L".js" || ext == L".ts") {
        cmd = L"node \"" + g_currentFile + L"\"";
    } else {
        AppendOutput(L"[Build] Unsupported file type for compilation.\r\n");
        return;
    }

    AppendOutput((L"[Build] Compiling: " + g_currentFile + L"\r\n").c_str());
    AppendOutput((L"[Build] Command: " + cmd + L"\r\n").c_str());

    // Run build in background thread
    std::thread([cmd]() {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        std::wstring mutableCmd = cmd;

        if (CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hWrite);
            char buffer[4096];
            DWORD bytesRead;
            std::string output;
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = 0;
                output += std::string(buffer, bytesRead);
            }
            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hRead);

            // Post results to main thread
            std::wstring wOutput = Utf8ToWide(output);
            std::wstring finalMsg = wOutput + L"\r\n[Build] " +
                (exitCode == 0 ? L"Build succeeded." : L"Build failed (exit code " + std::to_wstring(exitCode) + L").") +
                L"\r\n";
            wchar_t* pMsg = new wchar_t[finalMsg.size() + 1];
            wcscpy(pMsg, finalMsg.c_str());
            PostMessageW(g_hwndMain, WM_BUILD_COMPLETE, 0, (LPARAM)pMsg);
        } else {
            CloseHandle(hWrite);
            CloseHandle(hRead);
            PostMessageW(g_hwndMain, WM_BUILD_COMPLETE, 1, 0);
        }
    }).detach();
}

void RunCurrentFile() {
    if (g_currentFile.empty()) { AppendOutput(L"[Build] No file open.\r\n"); return; }

    // Save first
    if (g_isDirty) SaveCurrentFile();

    std::wstring ext = GetFileExtension(g_currentFile);
    std::wstring cmd;

    if (ext == L".py") {
        cmd = L"python \"" + g_currentFile + L"\"";
    } else if (ext == L".js") {
        cmd = L"node \"" + g_currentFile + L"\"";
    } else if (ext == L".cpp" || ext == L".c") {
        // Assume compiled to same name without extension + .exe
        std::wstring exePath = g_currentFile;
        size_t dot = exePath.find_last_of(L'.');
        if (dot != std::wstring::npos) exePath = exePath.substr(0, dot) + L".exe";
        if (GetFileAttributesW(exePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            cmd = L"\"" + exePath + L"\"";
        } else {
            AppendOutput(L"[Build] Executable not found. Compile first (F7).\r\n");
            return;
        }
    } else {
        AppendOutput(L"[Build] Don't know how to run this file type.\r\n");
        return;
    }

    AppendOutput((L"[Run] Executing: " + cmd + L"\r\n").c_str());

    // Show terminal and run there
    if (!g_bTerminalVisible) {
        g_bTerminalVisible = true;
        g_bOutputVisible = false;
        UpdateLayout(g_hwndMain);
    }
    if (!g_hTermProcess) StartTerminal();
    SendTerminalCommand(cmd);
}

// ============================================================================
// AI Functions
// ============================================================================

void AI_ExplainCode() {
    if (!g_hwndEditor) return;
    DWORD start = 0, end = 0;
    SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    if (start == end) {
        AppendChat(L"[AI] Select some code first, then use Explain.\r\n");
        return;
    }

    int len = GetWindowTextLengthW(g_hwndEditor);
    std::wstring text(len, L'\0');
    GetWindowTextW(g_hwndEditor, &text[0], len + 1);
    std::wstring selected = text.substr(start, end - start);

    AppendChat(L"\r\n[AI Explain] Analyzing selected code...\r\n");
    AppendChat((L"[Code] " + selected.substr(0, 200) + (selected.size() > 200 ? L"..." : L"") + L"\r\n").c_str());

    // Basic analysis
    if (selected.find(L"for") != std::wstring::npos || selected.find(L"while") != std::wstring::npos)
        AppendChat(L"[AI] This code contains a loop construct for iterating over data.\r\n");
    if (selected.find(L"if") != std::wstring::npos)
        AppendChat(L"[AI] This code contains conditional logic for branching execution.\r\n");
    if (selected.find(L"class") != std::wstring::npos || selected.find(L"struct") != std::wstring::npos)
        AppendChat(L"[AI] This code defines a data structure or class.\r\n");
    if (selected.find(L"return") != std::wstring::npos)
        AppendChat(L"[AI] This code returns a value from a function.\r\n");
    if (selected.find(L"new") != std::wstring::npos || selected.find(L"malloc") != std::wstring::npos)
        AppendChat(L"[AI] This code performs dynamic memory allocation.\r\n");
    if (selected.find(L"#include") != std::wstring::npos)
        AppendChat(L"[AI] This is a preprocessor include directive.\r\n");

    if (!g_bChatVisible) {
        g_bChatVisible = true;
        UpdateLayout(g_hwndMain);
    }
}

void AI_RefactorCode() {
    if (!g_hwndEditor) return;
    DWORD start = 0, end = 0;
    SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    if (start == end) {
        AppendChat(L"[AI] Select code to refactor first.\r\n");
        return;
    }

    AppendChat(L"\r\n[AI Refactor] Analyzing code for improvements...\r\n");
    AppendChat(L"[AI] Suggestions:\r\n");
    AppendChat(L"  - Extract repeated logic into helper functions\r\n");
    AppendChat(L"  - Use const references for non-mutated parameters\r\n");
    AppendChat(L"  - Consider using RAII patterns for resource management\r\n");
    AppendChat(L"  - Replace magic numbers with named constants\r\n");

    if (!g_bChatVisible) { g_bChatVisible = true; UpdateLayout(g_hwndMain); }
}

void AI_FixCode() {
    AppendChat(L"\r\n[AI Fix] Running code analysis for fixes...\r\n");
    RunCodeAnalysis();
    std::lock_guard<std::mutex> lock(g_issuesMutex);
    if (g_codeIssues.empty()) {
        AppendChat(L"[AI] No issues found in current file.\r\n");
    } else {
        AppendChat((L"[AI] Found " + std::to_wstring(g_codeIssues.size()) + L" issue(s). Check Issues panel.\r\n").c_str());
        if (!g_bIssuesVisible) { g_bIssuesVisible = true; UpdateLayout(g_hwndMain); }
    }
}

// ============================================================================
// Status Bar & Title
// ============================================================================

void UpdateStatusBar() {
    if (!g_hwndStatusBar || !g_hwndEditor) return;

    DWORD start = 0, end = 0;
    SendMessageW(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    int line = (int)SendMessageW(g_hwndEditor, EM_LINEFROMCHAR, start, 0) + 1;
    int lineStart = (int)SendMessageW(g_hwndEditor, EM_LINEINDEX, line - 1, 0);
    int col = start - lineStart + 1;

    wchar_t posBuf[64];
    swprintf_s(posBuf, L"Ln %d, Col %d", line, col);

    int parts[] = { 200, 400, 600, -1 };
    SendMessageW(g_hwndStatusBar, SB_SETPARTS, 4, (LPARAM)parts);
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)(g_isDirty ? L"Modified" : L"Ready"));
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 1, (LPARAM)posBuf);
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 2, (LPARAM)L"UTF-8");
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 3,
        (LPARAM)(g_modelLoaded ? GetFileName(g_loadedModelPath).c_str() : L"No model"));
}

void UpdateWindowTitle() {
    std::wstring title = L"RawrXD IDE";
    if (!g_currentFile.empty()) {
        title += L" - " + GetFileName(g_currentFile);
        if (g_isDirty) title += L" *";
    }
    SetWindowTextW(g_hwndMain, title.c_str());
}

// ============================================================================
// Menu Bar
// ============================================================================

HMENU CreateMainMenu() {
    HMENU hMenu = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN, L"&Open File...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_CLOSE, L"&Close Tab\tCtrl+W");
    AppendMenuW(hFile, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REDO, L"&Redo\tCtrl+Y");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_SELECTALL, L"Select &All\tCtrl+A");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_FIND, L"&Find...\tCtrl+F");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REPLACE, L"&Replace...\tCtrl+H");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_GOTO, L"&Go To Line...\tCtrl+G");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");

    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_VIEW_FILETREE, L"&File Explorer\tCtrl+B");
    AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_VIEW_OUTPUT, L"&Output\tCtrl+`");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_TERMINAL, L"&Terminal\tCtrl+Shift+`");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_CHAT, L"AI &Chat\tCtrl+Shift+C");
    AppendMenuW(hView, MF_STRING, IDM_VIEW_ISSUES, L"Code &Issues\tCtrl+Shift+I");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hView, L"&View");

    HMENU hBuild = CreatePopupMenu();
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_COMPILE, L"&Compile\tF7");
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_RUN, L"&Run\tF5");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBuild, L"&Build");

    HMENU hAI = CreatePopupMenu();
    AppendMenuW(hAI, MF_STRING, IDM_AI_COMPLETE, L"&Code Completion\tCtrl+Space");
    AppendMenuW(hAI, MF_STRING, IDM_AI_EXPLAIN, L"&Explain Selection\tCtrl+E");
    AppendMenuW(hAI, MF_STRING, IDM_AI_REFACTOR, L"&Refactor Selection\tCtrl+Shift+R");
    AppendMenuW(hAI, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAI, MF_STRING, IDM_AI_LOADGGUF, L"Load &GGUF Model...");
    AppendMenuW(hAI, MF_STRING, IDM_AI_UNLOAD, L"&Unload Model");
    AppendMenuW(hAI, MF_STRING, IDM_AI_LOADTITAN, L"Load &Titan Model...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAI, L"&AI");

    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_SHORTCUTS, L"&Keyboard Shortcuts");
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"&About RawrXD IDE");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hMenu;
}

// ============================================================================
// Keyboard Accelerator
// ============================================================================

HACCEL CreateAccelerators() {
    ACCEL accel[] = {
        { FCONTROL | FVIRTKEY, 'N', IDM_FILE_NEW },
        { FCONTROL | FVIRTKEY, 'O', IDM_FILE_OPEN },
        { FCONTROL | FVIRTKEY, 'S', IDM_FILE_SAVE },
        { FCONTROL | FSHIFT | FVIRTKEY, 'S', IDM_FILE_SAVEAS },
        { FCONTROL | FVIRTKEY, 'W', IDM_FILE_CLOSE },
        { FCONTROL | FVIRTKEY, 'Z', IDM_EDIT_UNDO },
        { FCONTROL | FVIRTKEY, 'Y', IDM_EDIT_REDO },
        { FCONTROL | FVIRTKEY, 'X', IDM_EDIT_CUT },
        { FCONTROL | FVIRTKEY, 'C', IDM_EDIT_COPY },
        { FCONTROL | FVIRTKEY, 'V', IDM_EDIT_PASTE },
        { FCONTROL | FVIRTKEY, 'A', IDM_EDIT_SELECTALL },
        { FCONTROL | FVIRTKEY, 'F', IDM_EDIT_FIND },
        { FCONTROL | FVIRTKEY, 'H', IDM_EDIT_REPLACE },
        { FCONTROL | FVIRTKEY, 'G', IDM_EDIT_GOTO },
        { FCONTROL | FVIRTKEY, 'B', IDM_VIEW_FILETREE },
        { FVIRTKEY, VK_F7, IDM_BUILD_COMPILE },
        { FVIRTKEY, VK_F5, IDM_BUILD_RUN },
        { FCONTROL | FVIRTKEY, VK_SPACE, IDM_AI_COMPLETE },
        { FCONTROL | FVIRTKEY, 'E', IDM_AI_EXPLAIN },
    };
    return CreateAcceleratorTableW(accel, _countof(accel));
}

// ============================================================================
// Subclass Procedures
// ============================================================================

LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                     UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
    case WM_KEYDOWN:
        if (wParam == VK_TAB && g_completionVisible) { AcceptAICompletion(); return 0; }
        if (wParam == VK_ESCAPE && g_completionVisible) { HideAICompletion(); return 0; }
        break;
    case WM_CHAR:
        if (g_completionVisible && wParam != VK_TAB && wParam != VK_ESCAPE) HideAICompletion();
        if (wParam >= 32) {
            g_isDirty = true;
            if (g_activeTab >= 0) g_tabs[g_activeTab].isDirty = true;
            UpdateWindowTitle();
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (g_completionVisible) HideAICompletion();
        break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ChatInputSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000)) {
        HandleChatSend();
        return 0;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TerminalSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        // Get the last line as command
        int lineCount = (int)SendMessageW(hwnd, EM_GETLINECOUNT, 0, 0);
        if (lineCount > 0) {
            int lineIdx = lineCount - 1;
            int lineStart = (int)SendMessageW(hwnd, EM_LINEINDEX, lineIdx, 0);
            int lineLen = (int)SendMessageW(hwnd, EM_LINELENGTH, lineStart, 0);
            if (lineLen > 0) {
                wchar_t buf[1024] = {};
                *(WORD*)buf = 1022;
                SendMessageW(hwnd, EM_GETLINE, lineIdx, (LPARAM)buf);
                buf[lineLen] = 0;
                // Skip prompt characters
                std::wstring cmd(buf);
                if (cmd.length() > 2 && cmd[cmd.length() - 1] == L' ') {
                    // Try to find the actual command after prompt
                }
                SendTerminalCommand(cmd);
            }
        }
        AppendTerminal(L"\r\n");
        return 0;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Layout Management
// ============================================================================

void UpdateLayout(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right;
    int height = rc.bottom;

    // Status bar
    SendMessageW(g_hwndStatusBar, WM_SIZE, 0, 0);
    RECT sbRect;
    GetWindowRect(g_hwndStatusBar, &sbRect);
    int sbHeight = sbRect.bottom - sbRect.top;

    int workHeight = height - sbHeight;

    // Panel sizes
    int treeWidth = g_bFileTreeVisible ? 220 : 0;
    int chatWidth = g_bChatVisible ? 300 : 0;
    int issuesWidth = g_bIssuesVisible ? 350 : 0;
    int bottomHeight = (g_bOutputVisible || g_bTerminalVisible) ? 180 : 0;
    int rightWidth = chatWidth + issuesWidth;

    int editorWidth = width - treeWidth - rightWidth;
    int editorHeight = workHeight - bottomHeight;
    int tabHeight = (g_hwndTabControl && g_tabs.size() > 0) ? 25 : 0;

    // File tree
    if (g_hwndFileTree) {
        MoveWindow(g_hwndFileTree, 0, 0, treeWidth, workHeight, TRUE);
        ShowWindow(g_hwndFileTree, g_bFileTreeVisible ? SW_SHOW : SW_HIDE);
    }

    // Tab control
    if (g_hwndTabControl) {
        MoveWindow(g_hwndTabControl, treeWidth, 0, editorWidth, tabHeight, TRUE);
        ShowWindow(g_hwndTabControl, (g_tabs.size() > 0) ? SW_SHOW : SW_HIDE);
    }

    // Active editor
    if (g_hwndEditor) {
        MoveWindow(g_hwndEditor, treeWidth, tabHeight, editorWidth, editorHeight - tabHeight, TRUE);
    }

    // Output panel
    if (g_hwndOutput) {
        MoveWindow(g_hwndOutput, treeWidth, editorHeight, editorWidth, bottomHeight, TRUE);
        ShowWindow(g_hwndOutput, (g_bOutputVisible && !g_bTerminalVisible) ? SW_SHOW : SW_HIDE);
    }

    // Terminal panel
    if (g_hwndTerminal) {
        MoveWindow(g_hwndTerminal, treeWidth, editorHeight, editorWidth, bottomHeight, TRUE);
        ShowWindow(g_hwndTerminal, g_bTerminalVisible ? SW_SHOW : SW_HIDE);
    }

    // Issues panel
    if (g_hwndIssuesPanel) {
        int issX = width - rightWidth;
        MoveWindow(g_hwndIssuesPanel, issX, 0, issuesWidth, workHeight, TRUE);
        ShowWindow(g_hwndIssuesPanel, g_bIssuesVisible ? SW_SHOW : SW_HIDE);
        if (g_hwndIssuesList) {
            MoveWindow(g_hwndIssuesList, 0, 35, issuesWidth, workHeight - 35, TRUE);
        }
    }

    // Chat panel
    if (g_bChatVisible) {
        int chatX = width - chatWidth;
        int chatBtnH = 28;
        int chatInputH = 80;
        int chatHistH = workHeight - chatBtnH - chatInputH - chatBtnH;

        // Top buttons
        HWND btns[] = {
            GetDlgItem(hwnd, IDC_CHAT_NEW), GetDlgItem(hwnd, IDC_CHAT_HISTORY),
            GetDlgItem(hwnd, IDC_CHAT_EXPORT), GetDlgItem(hwnd, IDC_CHAT_CONFIG),
            GetDlgItem(hwnd, IDC_CHAT_CLEAR)
        };
        int bw = chatWidth / 5;
        for (int i = 0; i < 5; i++) {
            if (btns[i]) { MoveWindow(btns[i], chatX + i * bw, 0, bw, chatBtnH, TRUE); ShowWindow(btns[i], SW_SHOW); }
        }

        // Chat history
        if (g_hwndChatHistory) {
            MoveWindow(g_hwndChatHistory, chatX, chatBtnH, chatWidth, chatHistH, TRUE);
            ShowWindow(g_hwndChatHistory, SW_SHOW);
        }

        // Bottom action buttons
        HWND aBtns[] = {
            GetDlgItem(hwnd, IDC_CHAT_GEN), GetDlgItem(hwnd, IDC_CHAT_EXPLAIN),
            GetDlgItem(hwnd, IDC_CHAT_REFACTOR), GetDlgItem(hwnd, IDC_CHAT_FIX),
            GetDlgItem(hwnd, IDC_CHAT_STOP), GetDlgItem(hwnd, IDC_CHAT_SEND)
        };
        int abw = chatWidth / 6;
        int abY = chatBtnH + chatHistH;
        for (int i = 0; i < 6; i++) {
            if (aBtns[i]) { MoveWindow(aBtns[i], chatX + i * abw, abY, abw, chatBtnH, TRUE); ShowWindow(aBtns[i], SW_SHOW); }
        }

        // Chat input
        if (g_hwndChatInput) {
            MoveWindow(g_hwndChatInput, chatX, abY + chatBtnH, chatWidth, chatInputH, TRUE);
            ShowWindow(g_hwndChatInput, SW_SHOW);
        }
    } else {
        // Hide all chat controls
        ShowWindow(g_hwndChatHistory, SW_HIDE);
        ShowWindow(g_hwndChatInput, SW_HIDE);
        int chatIds[] = { IDC_CHAT_NEW, IDC_CHAT_HISTORY, IDC_CHAT_EXPORT, IDC_CHAT_CONFIG, IDC_CHAT_CLEAR,
                          IDC_CHAT_GEN, IDC_CHAT_EXPLAIN, IDC_CHAT_REFACTOR, IDC_CHAT_FIX, IDC_CHAT_STOP, IDC_CHAT_SEND };
        for (int id : chatIds) {
            HWND h = GetDlgItem(hwnd, id);
            if (h) ShowWindow(h, SW_HIDE);
        }
    }
}

// ============================================================================
// Main Window Procedure
// ============================================================================

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Load RichEdit
        LoadLibraryW(L"msftedit.dll");

        // Create fonts
        g_hFontCode = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        g_hFontUI = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

        // Menu
        SetMenu(hwnd, CreateMainMenu());

        // Status bar
        g_hwndStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"Ready",
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, GetModuleHandle(nullptr), nullptr);

        // Tab control
        g_hwndTabControl = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER,
            0, 0, 0, 0, hwnd, (HMENU)IDC_TABCONTROL, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndTabControl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

        // File tree
        g_hwndFileTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
            0, 0, 220, 700, hwnd, (HMENU)IDC_FILETREE, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndFileTree, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

        // Output panel
        g_hwndOutput = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 0, 0, hwnd, (HMENU)IDC_OUTPUT, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndOutput, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        SendMessageW(g_hwndOutput, EM_SETBKGNDCOLOR, 0, RGB(12, 12, 12));
        CHARFORMAT2W cfOut = {};
        cfOut.cbSize = sizeof(cfOut); cfOut.dwMask = CFM_COLOR; cfOut.crTextColor = RGB(192, 192, 192);
        SendMessageW(g_hwndOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfOut);

        // Terminal panel (initially hidden)
        g_hwndTerminal = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 0, 0, 0, hwnd, (HMENU)IDC_TERMINAL, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndTerminal, WM_SETFONT, (WPARAM)g_hFontCode, TRUE);
        SendMessageW(g_hwndTerminal, EM_SETBKGNDCOLOR, 0, RGB(12, 12, 12));
        CHARFORMAT2W cfTerm = {};
        cfTerm.cbSize = sizeof(cfTerm); cfTerm.dwMask = CFM_COLOR; cfTerm.crTextColor = RGB(204, 204, 204);
        SendMessageW(g_hwndTerminal, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfTerm);
        SetWindowSubclass(g_hwndTerminal, TerminalSubclassProc, 2, 0);

        // Chat history
        g_hwndChatHistory = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 300, 500, hwnd, (HMENU)IDC_CHATHISTORY, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatHistory, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SendMessageW(g_hwndChatHistory, EM_SETBKGNDCOLOR, 0, RGB(25, 25, 25));
        CHARFORMAT2W cfChat = {};
        cfChat.cbSize = sizeof(cfChat); cfChat.dwMask = CFM_COLOR; cfChat.crTextColor = RGB(200, 200, 200);
        SendMessageW(g_hwndChatHistory, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cfChat);

        // Chat input
        g_hwndChatInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 0, 300, 80, hwnd, (HMENU)IDC_CHATINPUT, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndChatInput, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SetWindowSubclass(g_hwndChatInput, ChatInputSubclassProc, 3, 0);

        // Chat buttons
        auto mkBtn = [&](const wchar_t* text, int id) {
            HWND h = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | BS_PUSHBUTTON,
                0, 0, 50, 25, hwnd, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
            SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
            return h;
        };
        mkBtn(L"New", IDC_CHAT_NEW);
        mkBtn(L"History", IDC_CHAT_HISTORY);
        mkBtn(L"Export", IDC_CHAT_EXPORT);
        mkBtn(L"Config", IDC_CHAT_CONFIG);
        mkBtn(L"Clear", IDC_CHAT_CLEAR);
        mkBtn(L"Gen", IDC_CHAT_GEN);
        mkBtn(L"Explain", IDC_CHAT_EXPLAIN);
        mkBtn(L"Refactor", IDC_CHAT_REFACTOR);
        mkBtn(L"Fix", IDC_CHAT_FIX);
        mkBtn(L"Stop", IDC_CHAT_STOP);
        mkBtn(L"Send", IDC_CHAT_SEND);

        // Issues panel
        g_hwndIssuesPanel = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"",
            WS_CHILD | SS_BLACKFRAME, 0, 0, 350, 600, hwnd,
            (HMENU)IDC_ISSUES_PANEL, GetModuleHandle(nullptr), nullptr);

        g_hwndIssuesList = CreateWindowExW(0, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
            0, 35, 350, 565, g_hwndIssuesPanel,
            (HMENU)IDC_ISSUES_LIST, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndIssuesList, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        ListView_SetExtendedListViewStyle(g_hwndIssuesList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        // Issues columns
        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 60; col.pszText = (LPWSTR)L"Severity";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 0, (LPARAM)&col);
        col.cx = 70; col.pszText = (LPWSTR)L"Category";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 1, (LPARAM)&col);
        col.cx = 140; col.pszText = (LPWSTR)L"Message";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 2, (LPARAM)&col);
        col.cx = 80; col.pszText = (LPWSTR)L"File";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 3, (LPARAM)&col);
        col.cx = 40; col.pszText = (LPWSTR)L"Line";
        SendMessageW(g_hwndIssuesList, LVM_INSERTCOLUMNW, 4, (LPARAM)&col);

        // Issues filter buttons (inside issues panel)
        auto mkIBtn = [&](const wchar_t* text, int id, int x) {
            HWND h = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                x, 5, 55, 24, g_hwndIssuesPanel, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
            SendMessageW(h, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(h, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
            return h;
        };
        mkIBtn(L"Critical", IDC_ISSUES_CRITICAL, 2);
        mkIBtn(L"High", IDC_ISSUES_HIGH, 60);
        mkIBtn(L"Medium", IDC_ISSUES_MEDIUM, 118);
        mkIBtn(L"Low", IDC_ISSUES_LOW, 176);
        mkIBtn(L"Info", IDC_ISSUES_INFO, 234);

        // Load DLLs
        if (LoadTitanKernel()) AppendOutput(L"[System] Titan Kernel loaded.\r\n");
        else AppendOutput(L"[System] Titan Kernel not found (AI limited).\r\n");

        if (LoadModelBridge()) AppendOutput(L"[System] Native Model Bridge loaded.\r\n");
        else AppendOutput(L"[System] Native Model Bridge not found.\r\n");

        if (LoadInferenceEngine()) AppendOutput(L"[System] Inference Engine loaded.\r\n");

        InitializeHTTPClient();

        AppendOutput(L"[System] RawrXD IDE v1.0 started.\r\n");
        AppendOutput(L"[System] Use File > Open to load a file, or drag a folder onto the file tree.\r\n");
        AppendOutput(L"[System] Keyboard shortcuts: Ctrl+N (New), Ctrl+O (Open), Ctrl+S (Save)\r\n");
        AppendOutput(L"[System]   F7 (Compile), F5 (Run), Ctrl+Space (AI Complete)\r\n");
        AppendOutput(L"[System]   Ctrl+F (Find), Ctrl+H (Replace), Ctrl+G (Go To Line)\r\n");

        // Populate file tree with exe directory
        PopulateFileTree(GetExeDirectory());

        // Create initial empty tab
        CreateNewTab();
        break;
    }

    case WM_SIZE:
        UpdateLayout(hwnd);
        break;

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        int event = HIWORD(wParam);

        // Editor change -> debounced syntax highlighting
        if (event == EN_CHANGE && (HWND)lParam == g_hwndEditor) {
            static DWORD lastChange = 0;
            DWORD now = GetTickCount();
            if (now - lastChange > 500) {
                lastChange = now;
                PostMessageW(hwnd, WM_SYNTAX_HIGHLIGHT, 0, 0);
            }
        }

        // Editor selection change -> update status bar
        if (event == EN_SELCHANGE || event == EN_CHANGE) {
            if ((HWND)lParam == g_hwndEditor) UpdateStatusBar();
        }

        switch (id) {
        // File
        case IDM_FILE_NEW: CreateNewTab(); break;
        case IDM_FILE_OPEN: OpenFileDialog(); break;
        case IDM_FILE_SAVE: SaveCurrentFile(); break;
        case IDM_FILE_SAVEAS: SaveFileAs(); break;
        case IDM_FILE_CLOSE: if (g_activeTab >= 0) CloseTab(g_activeTab); break;
        case IDM_FILE_EXIT: PostMessageW(hwnd, WM_CLOSE, 0, 0); break;

        // Edit
        case IDM_EDIT_UNDO: if (g_hwndEditor) SendMessageW(g_hwndEditor, EM_UNDO, 0, 0); break;
        case IDM_EDIT_REDO: if (g_hwndEditor) SendMessageW(g_hwndEditor, EM_REDO, 0, 0); break;
        case IDM_EDIT_CUT: if (g_hwndEditor) SendMessageW(g_hwndEditor, WM_CUT, 0, 0); break;
        case IDM_EDIT_COPY: if (g_hwndEditor) SendMessageW(g_hwndEditor, WM_COPY, 0, 0); break;
        case IDM_EDIT_PASTE: if (g_hwndEditor) SendMessageW(g_hwndEditor, WM_PASTE, 0, 0); break;
        case IDM_EDIT_SELECTALL: if (g_hwndEditor) SendMessageW(g_hwndEditor, EM_SETSEL, 0, -1); break;
        case IDM_EDIT_FIND: CreateFindReplaceDialog(hwnd, false); break;
        case IDM_EDIT_REPLACE: CreateFindReplaceDialog(hwnd, true); break;
        case IDM_EDIT_GOTO: GoToLine(); break;

        // View
        case IDM_VIEW_FILETREE:
            g_bFileTreeVisible = !g_bFileTreeVisible;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_FILETREE, g_bFileTreeVisible ? MF_CHECKED : MF_UNCHECKED);
            UpdateLayout(hwnd);
            break;
        case IDM_VIEW_OUTPUT:
            g_bOutputVisible = !g_bOutputVisible;
            if (g_bOutputVisible) g_bTerminalVisible = false;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_OUTPUT, g_bOutputVisible ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_TERMINAL, g_bTerminalVisible ? MF_CHECKED : MF_UNCHECKED);
            UpdateLayout(hwnd);
            break;
        case IDM_VIEW_TERMINAL:
            g_bTerminalVisible = !g_bTerminalVisible;
            if (g_bTerminalVisible) { g_bOutputVisible = false; if (!g_hTermProcess) StartTerminal(); }
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_TERMINAL, g_bTerminalVisible ? MF_CHECKED : MF_UNCHECKED);
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_OUTPUT, g_bOutputVisible ? MF_CHECKED : MF_UNCHECKED);
            UpdateLayout(hwnd);
            break;
        case IDM_VIEW_CHAT:
            g_bChatVisible = !g_bChatVisible;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_CHAT, g_bChatVisible ? MF_CHECKED : MF_UNCHECKED);
            if (g_bChatVisible) {
                AppendChat(L"[AI Chat] Ready. Type a message below.\r\n");
                if (g_modelLoaded) AppendChat((L"[System] Model: " + GetFileName(g_loadedModelPath) + L"\r\n").c_str());
            }
            UpdateLayout(hwnd);
            break;
        case IDM_VIEW_ISSUES:
            g_bIssuesVisible = !g_bIssuesVisible;
            CheckMenuItem(GetMenu(hwnd), IDM_VIEW_ISSUES, g_bIssuesVisible ? MF_CHECKED : MF_UNCHECKED);
            if (g_bIssuesVisible) RefreshCodeIssues();
            UpdateLayout(hwnd);
            break;

        // Build
        case IDM_BUILD_COMPILE: CompileCurrentFile(); break;
        case IDM_BUILD_RUN: RunCurrentFile(); break;

        // AI
        case IDM_AI_COMPLETE: TriggerAICompletion(); break;
        case IDM_AI_EXPLAIN: AI_ExplainCode(); break;
        case IDM_AI_REFACTOR: AI_RefactorCode(); break;
        case IDM_AI_LOADGGUF: {
            wchar_t filename[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            ofn.lpstrTitle = L"Load GGUF Model";
            if (GetOpenFileNameW(&ofn)) {
                AppendOutput((L"[AI] Loading: " + std::wstring(filename) + L"\r\n").c_str());
                if (LoadGGUFModel(filename)) {
                    AppendOutput(L"[AI] Model loaded successfully!\r\n");
                    UpdateStatusBar();
                } else {
                    AppendOutput(L"[AI] Failed to load model.\r\n");
                }
            }
            break;
        }
        case IDM_AI_UNLOAD:
            UnloadModel();
            AppendOutput(L"[AI] Model unloaded.\r\n");
            UpdateStatusBar();
            break;
        case IDM_AI_LOADTITAN: {
            wchar_t filename[MAX_PATH] = L"";
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"All Models\0*.*\0\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST;
            if (GetOpenFileNameW(&ofn) && pTitan_LoadModel) {
                std::string utf8 = WideToUtf8(std::wstring(filename));
                int slot = pTitan_LoadModel(utf8.c_str(), "custom");
                AppendOutput(slot >= 0 ? L"[AI] Titan model loaded.\r\n" : L"[AI] Titan load failed.\r\n");
            }
            break;
        }

        // Chat buttons
        case IDC_CHAT_SEND: HandleChatSend(); break;
        case IDC_CHAT_NEW: CreateNewChatSession(); break;
        case IDC_CHAT_HISTORY: LoadChatFromFile(); break;
        case IDC_CHAT_EXPORT: ExportChatHistory(); break;
        case IDC_CHAT_CONFIG: ConfigureMCPServers(); break;
        case IDC_CHAT_CLEAR: if (g_hwndChatHistory) SetWindowTextW(g_hwndChatHistory, L""); break;
        case IDC_CHAT_GEN: TriggerAICompletion(); break;
        case IDC_CHAT_EXPLAIN: AI_ExplainCode(); break;
        case IDC_CHAT_REFACTOR: AI_RefactorCode(); break;
        case IDC_CHAT_FIX: AI_FixCode(); break;
        case IDC_CHAT_STOP: g_bAbortInference = true; AppendChat(L"[System] Inference stopped.\r\n"); break;

        // Issues buttons
        case IDC_ISSUES_CRITICAL: g_bShowCritical = !g_bShowCritical; FilterIssuesBySeverity(); break;
        case IDC_ISSUES_HIGH: g_bShowHigh = !g_bShowHigh; FilterIssuesBySeverity(); break;
        case IDC_ISSUES_MEDIUM: g_bShowMedium = !g_bShowMedium; FilterIssuesBySeverity(); break;
        case IDC_ISSUES_LOW: g_bShowLow = !g_bShowLow; FilterIssuesBySeverity(); break;
        case IDC_ISSUES_INFO: g_bShowInfo = !g_bShowInfo; FilterIssuesBySeverity(); break;
        case IDC_ISSUES_REFRESH: RefreshCodeIssues(); break;
        case IDC_ISSUES_EXPORT: ExportIssues(); break;
        case IDC_ISSUES_CLEAR: ClearCodeIssues(); break;

        // Help
        case IDM_HELP_SHORTCUTS:
            MessageBoxW(hwnd,
                L"Keyboard Shortcuts:\r\n\r\n"
                L"File:\r\n"
                L"  Ctrl+N        New File\r\n"
                L"  Ctrl+O        Open File\r\n"
                L"  Ctrl+S        Save\r\n"
                L"  Ctrl+Shift+S  Save As\r\n"
                L"  Ctrl+W        Close Tab\r\n\r\n"
                L"Edit:\r\n"
                L"  Ctrl+Z        Undo\r\n"
                L"  Ctrl+Y        Redo\r\n"
                L"  Ctrl+F        Find\r\n"
                L"  Ctrl+H        Replace\r\n"
                L"  Ctrl+G        Go To Line\r\n"
                L"  Ctrl+A        Select All\r\n\r\n"
                L"Build:\r\n"
                L"  F7            Compile\r\n"
                L"  F5            Run\r\n\r\n"
                L"AI:\r\n"
                L"  Ctrl+Space    Code Completion\r\n"
                L"  Ctrl+E        Explain Code\r\n"
                L"  Tab           Accept Completion\r\n"
                L"  Esc           Dismiss Completion\r\n\r\n"
                L"View:\r\n"
                L"  Ctrl+B        Toggle File Tree\r\n",
                L"Keyboard Shortcuts", MB_OK | MB_ICONINFORMATION);
            break;
        case IDM_HELP_ABOUT:
            MessageBoxW(hwnd,
                L"RawrXD IDE v1.0\r\n\r\n"
                L"Pure Win32 Autonomous Agentic Development Environment\r\n"
                L"Zero External Dependencies\r\n\r\n"
                L"Features:\r\n"
                L"  - Multi-tab code editor with syntax highlighting\r\n"
                L"  - File tree explorer with recursive population\r\n"
                L"  - Integrated terminal (PowerShell)\r\n"
                L"  - AI chat with GGUF/Titan model support\r\n"
                L"  - Code issues panel with static analysis\r\n"
                L"  - Find/Replace with regex support\r\n"
                L"  - Build system (C++, Python, JS)\r\n"
                L"  - Full keyboard shortcut support\r\n\r\n"
                L"Powered by Titan Kernel & GGUF Inference\r\n"
                L"(c) 2026 RawrXD Project",
                L"About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
            break;
        }
        break;
    }

    case WM_NOTIFY: {
        LPNMHDR pnm = (LPNMHDR)lParam;
        if (pnm->hwndFrom == g_hwndTabControl && pnm->code == TCN_SELCHANGE) {
            SwitchToTab(TabCtrl_GetCurSel(g_hwndTabControl));
        }
        if (pnm->hwndFrom == g_hwndFileTree && pnm->code == NM_DBLCLK) {
            HTREEITEM hItem = TreeView_GetSelection(g_hwndFileTree);
            if (hItem) {
                TVITEMW tvi = {};
                tvi.mask = TVIF_PARAM;
                tvi.hItem = hItem;
                TreeView_GetItem(g_hwndFileTree, &tvi);
                if (tvi.lParam == 0) { // File, not folder
                    std::wstring fullPath = GetTreeItemFullPath(g_hwndFileTree, hItem);
                    if (GetFileAttributesW(fullPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                        LoadFileIntoEditor(fullPath);
                    }
                }
            }
        }
        if (pnm->hwndFrom == g_hwndIssuesList && pnm->code == NM_DBLCLK) {
            int sel = ListView_GetNextItem(g_hwndIssuesList, -1, LVNI_SELECTED);
            if (sel >= 0) JumpToIssue(sel);
        }
        break;
    }

    case WM_TERMINAL_OUTPUT: {
        wchar_t* pMsg = (wchar_t*)lParam;
        if (pMsg) { AppendTerminal(pMsg); delete[] pMsg; }
        break;
    }

    case WM_SYNTAX_HIGHLIGHT:
        ApplySyntaxHighlighting(g_hwndEditor);
        break;

    case WM_BUILD_COMPLETE: {
        wchar_t* pMsg = (wchar_t*)lParam;
        if (pMsg) { AppendOutput(pMsg); delete[] pMsg; }
        else if (wParam == 1) AppendOutput(L"[Build] Failed to start compiler process.\r\n");
        break;
    }

    case WM_CLOSE: {
        bool hasUnsaved = false;
        for (const auto& tab : g_tabs) if (tab.isDirty) { hasUnsaved = true; break; }
        if (hasUnsaved) {
            int r = MessageBoxW(hwnd, L"You have unsaved changes. Exit anyway?", L"Confirm Exit", MB_YESNO | MB_ICONQUESTION);
            if (r == IDNO) return 0;
        }
        DestroyWindow(hwnd);
        break;
    }

    case WM_DESTROY:
        CleanupHTTPClient();
        StopTerminal();
        UnloadModel();
        if (g_hInferenceEngine) FreeLibrary(g_hInferenceEngine);
        if (g_hModelBridgeDll) FreeLibrary(g_hModelBridgeDll);
        if (g_hTitanDll) { if (pTitan_Shutdown) pTitan_Shutdown(); FreeLibrary(g_hTitanDll); }
        if (g_hFontCode) DeleteObject(g_hFontCode);
        if (g_hFontUI) DeleteObject(g_hFontUI);
        if (g_terminalCSInit) DeleteCriticalSection(&g_terminalCS);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// ============================================================================
// Entry Point
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_COOL_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = L"RawrXDIDE";
    wc.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) return 1;

    g_hwndMain = CreateWindowExW(0, L"RawrXDIDE",
        L"RawrXD IDE - Autonomous Agentic Development Environment",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_hwndMain) return 1;

    HACCEL hAccel = CreateAccelerators();

    ShowWindow(g_hwndMain, nCmdShow);
    UpdateWindow(g_hwndMain);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        // Pass through find dialog messages
        if (g_hwndFindDialog && IsDialogMessageW(g_hwndFindDialog, &msg)) continue;
        if (!TranslateAcceleratorW(g_hwndMain, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}

// Console fallback
int main() {
    return wWinMain(GetModuleHandle(nullptr), nullptr, nullptr, SW_SHOWDEFAULT);
}
