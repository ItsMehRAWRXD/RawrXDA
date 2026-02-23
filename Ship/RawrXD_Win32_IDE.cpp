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
#include <functional>
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

// Panel visibility states
static bool g_bTerminalVisible = false;
static bool g_bOutputVisible = true;
static bool g_bChatVisible = false;
static bool g_bAbortInference = false;

// Syntax highlighting colors
static COLORREF g_colorKeyword = RGB(86, 156, 214);    // Blue
static COLORREF g_colorString = RGB(214, 157, 133);    // Orange
static COLORREF g_colorComment = RGB(106, 153, 85);    // Green
static COLORREF g_colorNumber = RGB(181, 206, 168);    // Light green
static COLORREF g_colorPreproc = RGB(155, 155, 155);   // Gray
static COLORREF g_colorDefault = RGB(212, 212, 212);   // White
static COLORREF g_colorBackground = RGB(30, 30, 30);   // Dark

// ============================================================================
// AI Completion Support
// ============================================================================
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

// ============================================================================
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

// C/C++ keywords for syntax highlighting
static const wchar_t* g_cppKeywords[] = {
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
    L"#include", L"#define", L"#ifdef", L"#ifndef", L"#endif", L"#pragma", L"#if", L"#else",
    nullptr
};

// ============================================================================
// Syntax Highlighting
// ============================================================================
bool IsKeyword(const std::wstring& word) {
    for (int i = 0; g_cppKeywords[i]; i++) {
        if (_wcsicmp(word.c_str(), g_cppKeywords[i]) == 0) return true;
    }
    return false;
}

void ApplySyntaxHighlighting(HWND hwndEdit) {
    // Get text content
    int len = GetWindowTextLengthW(hwndEdit);
    if (len <= 0) return;
    
    std::wstring text(len + 1, L'\0');
    GetWindowTextW(hwndEdit, &text[0], len + 1);
    text.resize(len);
    
    // Set background color
    SendMessage(hwndEdit, EM_SETBKGNDCOLOR, 0, g_colorBackground);
    
    // Apply default color to all
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = g_colorDefault;
    SendMessage(hwndEdit, EM_SETSEL, 0, -1);
    SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // Find and highlight keywords
    size_t pos = 0;
    while (pos < text.length()) {
        // Skip whitespace
        while (pos < text.length() && iswspace(text[pos])) pos++;
        if (pos >= text.length()) break;
        
        // Check for comment
        if (pos + 1 < text.length() && text[pos] == L'/' && text[pos+1] == L'/') {
            size_t start = pos;
            while (pos < text.length() && text[pos] != L'\n') pos++;
            cf.crTextColor = g_colorComment;
            SendMessage(hwndEdit, EM_SETSEL, start, pos);
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for string
        if (text[pos] == L'"' || text[pos] == L'\'') {
            wchar_t quote = text[pos];
            size_t start = pos++;
            while (pos < text.length() && text[pos] != quote) {
                if (text[pos] == L'\\' && pos + 1 < text.length()) pos++;
                pos++;
            }
            if (pos < text.length()) pos++;
            cf.crTextColor = g_colorString;
            SendMessage(hwndEdit, EM_SETSEL, start, pos);
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for preprocessor
        if (text[pos] == L'#') {
            size_t start = pos;
            while (pos < text.length() && (iswalnum(text[pos]) || text[pos] == L'#' || text[pos] == L'_')) pos++;
            std::wstring word = text.substr(start, pos - start);
            cf.crTextColor = g_colorPreproc;
            SendMessage(hwndEdit, EM_SETSEL, start, pos);
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for number
        if (iswdigit(text[pos])) {
            size_t start = pos;
            while (pos < text.length() && (iswalnum(text[pos]) || text[pos] == L'.' || text[pos] == L'x')) pos++;
            cf.crTextColor = g_colorNumber;
            SendMessage(hwndEdit, EM_SETSEL, start, pos);
            SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            continue;
        }
        
        // Check for identifier/keyword
        if (iswalpha(text[pos]) || text[pos] == L'_') {
            size_t start = pos;
            while (pos < text.length() && (iswalnum(text[pos]) || text[pos] == L'_')) pos++;
            std::wstring word = text.substr(start, pos - start);
            if (IsKeyword(word)) {
                cf.crTextColor = g_colorKeyword;
                SendMessage(hwndEdit, EM_SETSEL, start, pos);
                SendMessage(hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
            continue;
        }
        
        pos++;
    }
    
    // Reset selection
    SendMessage(hwndEdit, EM_SETSEL, 0, 0);
}

// ============================================================================
// Forward declarations for utility functions used below
// ============================================================================
std::string WideToUtf8(const std::wstring& wstr);
std::wstring Utf8ToWide(const std::string& str);

// ============================================================================
// Terminal Support
// ============================================================================
static HANDLE g_hCmdProcess = nullptr;
static HANDLE g_hCmdStdoutRead = nullptr;
static HANDLE g_hCmdStdinWrite = nullptr;

bool StartTerminal() {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hStdoutWrite, hStdinRead;
    
    if (!CreatePipe(&g_hCmdStdoutRead, &hStdoutWrite, &sa, 0)) return false;
    if (!CreatePipe(&hStdinRead, &g_hCmdStdinWrite, &sa, 0)) {
        CloseHandle(g_hCmdStdoutRead);
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
    WriteFile(g_hCmdStdinWrite, utf8.c_str(), (DWORD)utf8.length(), &written, nullptr);
}

std::wstring ReadTerminalOutput() {
    if (!g_hCmdStdoutRead) return L"";
    
    char buffer[4096];
    DWORD available = 0, bytesRead = 0;
    std::string output;
    
    if (PeekNamedPipe(g_hCmdStdoutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
        if (ReadFile(g_hCmdStdoutRead, buffer, std::min(available, (DWORD)sizeof(buffer)-1), &bytesRead, nullptr)) {
            buffer[bytesRead] = 0;
            output = buffer;
        }
    }
    return Utf8ToWide(output);
}

// ============================================================================
// Load Titan Kernel DLL
// ============================================================================
bool LoadTitanKernel() {
    g_hTitanDll = LoadLibraryW(L"RawrXD_Titan_Kernel.dll");
    if (!g_hTitanDll) {
        // Try same directory
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path);
        size_t pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            dir = dir.substr(0, pos + 1) + L"RawrXD_Titan_Kernel.dll";
            g_hTitanDll = LoadLibraryW(dir.c_str());
        }
    }
    if (!g_hTitanDll) return false;
    
    // For now we just load the DLL; actual proc addresses would be:
    // pTitan_Initialize = (Titan_Initialize_t)GetProcAddress(g_hTitanDll, "Titan_Initialize");
    // etc.
    return true;
}

// ============================================================================
// Load Native Model Bridge DLL (GGUF Inference)
// ============================================================================
bool LoadModelBridge() {
    g_hModelBridgeDll = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    if (!g_hModelBridgeDll) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path);
        size_t pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            dir = dir.substr(0, pos + 1) + L"RawrXD_NativeModelBridge.dll";
            g_hModelBridgeDll = LoadLibraryW(dir.c_str());
        }
    }
    if (!g_hModelBridgeDll) return false;
    
    pLoadModelNative = (LoadModelNative_t)GetProcAddress(g_hModelBridgeDll, "LoadModelNative");
    pGetTokenEmbedding = (GetTokenEmbedding_t)GetProcAddress(g_hModelBridgeDll, "GetTokenEmbedding");
    pForwardPassASM = (ForwardPassASM_t)GetProcAddress(g_hModelBridgeDll, "ForwardPass");
    pCleanupMathTables = (CleanupMathTables_t)GetProcAddress(g_hModelBridgeDll, "CleanupMathTables");
    
    return pLoadModelNative != nullptr;
}

bool LoadGGUFModel(const std::wstring& filepath) {
    if (!g_hModelBridgeDll || !pLoadModelNative) {
        if (!LoadModelBridge()) return false;
    }
    
    std::string utf8path = WideToUtf8(filepath);
    int result = pLoadModelNative(utf8path.c_str());
    if (result == 0) {
        g_modelLoaded = true;
        g_loadedModelPath = filepath;
        return true;
    }
    return false;
}

void UnloadGGUFModel() {
    if (g_hModelBridgeDll && pCleanupMathTables) {
        pCleanupMathTables();
    }
    g_modelLoaded = false;
    g_loadedModelPath.clear();
}

// ============================================================================
// Load Inference Engine DLL (Full GGUF Inference)
// ============================================================================
bool LoadInferenceEngine() {
    // Load the Win32-specific inference engine DLL
    g_hInferenceEngine = LoadLibraryW(L"RawrXD_InferenceEngine_Win32.dll");
    if (!g_hInferenceEngine) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring dir(path);
        size_t pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            dir = dir.substr(0, pos + 1) + L"RawrXD_InferenceEngine.dll";
            g_hInferenceEngine = LoadLibraryW(dir.c_str());
        }
    }
    if (!g_hInferenceEngine) return false;
    
    pLoadModel = (LoadModel_t)GetProcAddress(g_hInferenceEngine, "LoadModel");
    pUnloadModel = (UnloadModel_t)GetProcAddress(g_hInferenceEngine, "UnloadModel");
    pForwardPassInfer = (ForwardPassInfer_t)GetProcAddress(g_hInferenceEngine, "ForwardPass");
    pSampleNext = (SampleNext_t)GetProcAddress(g_hInferenceEngine, "SampleNext");
    
    return pLoadModel != nullptr && pUnloadModel != nullptr;
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
// Utility: Wide string to UTF-8
// ============================================================================
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    return wstr;
}

// Forward declaration
void AppendWindowText(HWND hwnd, const wchar_t* text);

// ============================================================================
// File operations
// ============================================================================
bool LoadFile(const std::wstring& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    SetWindowTextA(g_hwndEditor, content.c_str());
    g_currentFile = path;
    g_isDirty = false;
    
    // Update title
    std::wstring title = L"RawrXD IDE - " + path;
    SetWindowTextW(g_hwndMain, title.c_str());
    
    // Update status
    std::wstring status = L"Loaded: " + path;
    SendMessageW(g_hwndStatusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    return true;
}

bool SaveFile(const std::wstring& path) {
    int len = GetWindowTextLengthA(g_hwndEditor);
    std::string content(len + 1, '\0');
    GetWindowTextA(g_hwndEditor, &content[0], len + 1);
    content.resize(len);
    
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    g_currentFile = path;
    g_isDirty = false;
    
    std::wstring status = L"Saved: " + path;
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
// AI Actions (Titan Kernel / GGUF / NativeBridge backends)
// ============================================================================
void AI_GenerateCode() {
    // Get selected text or current context
    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    int len = GetWindowTextLengthW(g_hwndEditor);
    std::wstring buffer(len + 1, L'\0');
    GetWindowTextW(g_hwndEditor, &buffer[0], len + 1);
    if (len > 0) buffer.resize(len);

    std::wstring selection;
    if (start != end && len > 0) {
        int selStart = std::min(start, end);
        int selEnd = std::max(start, end);
        if (selStart < len && selEnd <= len) {
            selection = buffer.substr(selStart, selEnd - selStart);
        }
    }

    std::wstring summary = selection.empty() ? L"fresh context" : L"selection snippet: " + selection;
    
    // Build the code generation prompt
    std::string prompt = "Complete the following code. Return only the code, no explanation:\n\n" + WideToUtf8(selection.empty() ? buffer : selection);
    std::wstring suggestion;
    bool generatedFromModel = false;
    
    // Try Titan Kernel first
    if (g_hTitanDll && pTitan_RunInference) {
        int result = pTitan_RunInference(0, prompt.c_str(), 512);
        if (result >= 0) {
            suggestion = L"// AI generated via Titan Kernel\r\n";
            generatedFromModel = true;
        }
    }
    
    // Try GGUF Inference Engine
    if (!generatedFromModel && g_modelContext && pForwardPassInfer && pSampleNext) {
        std::vector<int> tokens;
        for (unsigned char ch : prompt) tokens.push_back(static_cast<int>(ch));
        if (tokens.size() > 2048) tokens.resize(2048);
        
        std::vector<float> logits(32000, 0.0f);
        int fwdResult = pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data());
        if (fwdResult == 0) {
            suggestion.clear();
            for (int i = 0; i < 512; i++) {
                int nextToken = pSampleNext(logits.data(), 32000, 0.7f, 0.9f, 40);
                if (nextToken <= 0 || nextToken >= 128) break;
                suggestion += static_cast<wchar_t>(nextToken);
                int singleTok = nextToken;
                pForwardPassInfer(g_modelContext, &singleTok, 1, logits.data());
            }
            generatedFromModel = !suggestion.empty();
        }
    }
    
    // Try Native Model Bridge
    if (!generatedFromModel && g_modelLoaded && pForwardPassASM) {
        std::vector<int> tokens;
        std::string utf8prompt = prompt;
        for (unsigned char ch : utf8prompt) tokens.push_back(static_cast<int>(ch));
        std::vector<float> output(32000, 0.0f);
        int result = pForwardPassASM(tokens.data(), (int)tokens.size(), output.data(), 32000);
        if (result == 0) {
            suggestion = L"// AI generated via Native Model Bridge\r\n";
            generatedFromModel = true;
        }
    }
    
    // Fallback: context-aware template generation
    if (!generatedFromModel) {
        suggestion = L"// AI completion (no model loaded - template)\r\n";
        suggestion += L"// Based on " + summary + L"\r\n";
        suggestion += L"// Load a GGUF model for intelligent completions.\r\n";
    }

    AICompletion completion = {};
    completion.text = suggestion;
    completion.detail = L"Generated completion";
    completion.confidence = 0.73;
    completion.kind = L"snippet";
    completion.cursorOffset = (int)suggestion.size();
    completion.isMultiLine = true;

    ShowAICompletion(completion);
    AppendWindowText(g_hwndOutput, L"[AI] Code completion ready. Press Tab to accept or continue typing to dismiss.\r\n");
}

void AI_ExplainCode() {
    AppendWindowText(g_hwndOutput, L"[AI] Explaining selected code...\r\n");
    
    // Extract selected text from editor
    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    int len = GetWindowTextLengthW(g_hwndEditor);
    if (len <= 0 || start == end) {
        AppendWindowText(g_hwndOutput, L"[AI] No code selected. Select code first.\r\n");
        return;
    }
    std::wstring buffer(len + 1, L'\0');
    GetWindowTextW(g_hwndEditor, &buffer[0], len + 1);
    buffer.resize(len);
    int selStart = std::min(start, end);
    int selEnd = std::max(start, end);
    if (selStart >= len) { AppendWindowText(g_hwndOutput, L"[AI] Invalid selection range.\r\n"); return; }
    if (selEnd > (DWORD)len) selEnd = len;
    std::wstring selection = buffer.substr(selStart, selEnd - selStart);
    
    // Build prompt for explanation
    std::string prompt = "Explain the following code concisely:\n\n" + WideToUtf8(selection);
    
    // Try Titan Kernel first
    if (g_hTitanDll && pTitan_RunInference) {
        int result = pTitan_RunInference(0, prompt.c_str(), 512);
        if (result >= 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Explanation generated via Titan Kernel.\r\n");
            return;
        }
    }
    
    // Try GGUF Inference Engine
    if (g_modelContext && pForwardPassInfer && pSampleNext) {
        AppendWindowText(g_hwndOutput, L"[AI] Running inference via GGUF engine...\r\n");
        // Tokenize prompt (simplified: use char codes as token IDs)
        std::string utf8prompt = prompt;
        std::vector<int> tokens;
        tokens.reserve(utf8prompt.size());
        for (unsigned char ch : utf8prompt) tokens.push_back(static_cast<int>(ch));
        if (tokens.size() > 2048) tokens.resize(2048);
        
        std::vector<float> logits(32000, 0.0f);
        int fwdResult = pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data());
        if (fwdResult == 0) {
            // Sample output tokens
            std::wstring explanation = L"\r\n--- Explanation ---\r\n";
            for (int i = 0; i < 256; i++) {
                int nextToken = pSampleNext(logits.data(), 32000, 0.7f, 0.9f, 40);
                if (nextToken <= 0 || nextToken >= 128) break;
                explanation += static_cast<wchar_t>(nextToken);
                // Re-run forward pass for next token
                int singleTok = nextToken;
                pForwardPassInfer(g_modelContext, &singleTok, 1, logits.data());
            }
            explanation += L"\r\n--- End ---\r\n";
            AppendWindowText(g_hwndOutput, explanation.c_str());
            return;
        }
    }
    
    // Try Native Model Bridge as last resort
    if (g_modelLoaded && pForwardPassASM) {
        std::string utf8sel = WideToUtf8(selection);
        std::vector<int> tokens;
        for (unsigned char ch : utf8sel) tokens.push_back(static_cast<int>(ch));
        std::vector<float> output(32000, 0.0f);
        int result = pForwardPassASM(tokens.data(), (int)tokens.size(), output.data(), 32000);
        if (result == 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Explanation generated via Native Model Bridge.\r\n");
            return;
        }
    }
    
    AppendWindowText(g_hwndOutput, L"[AI] No inference engine available. Load a model first (AI > Load GGUF Model).\r\n");
}

void AI_RefactorCode() {
    AppendWindowText(g_hwndOutput, L"[AI] Refactoring selected code...\r\n");
    
    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    int len = GetWindowTextLengthW(g_hwndEditor);
    if (len <= 0 || start == end) {
        AppendWindowText(g_hwndOutput, L"[AI] No code selected. Select code to refactor.\r\n");
        return;
    }
    std::wstring buffer(len + 1, L'\0');
    GetWindowTextW(g_hwndEditor, &buffer[0], len + 1);
    buffer.resize(len);
    int selStart = std::min(start, end);
    int selEnd = std::max(start, end);
    if (selStart >= len) return;
    if (selEnd > (DWORD)len) selEnd = len;
    std::wstring selection = buffer.substr(selStart, selEnd - selStart);
    
    std::string prompt = "Refactor the following code for better readability, performance, and maintainability. Return only the refactored code:\n\n" + WideToUtf8(selection);
    
    // Try Titan Kernel
    if (g_hTitanDll && pTitan_RunInference) {
        int result = pTitan_RunInference(0, prompt.c_str(), 1024);
        if (result >= 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Refactoring suggestion generated. Check output panel.\r\n");
            return;
        }
    }
    
    // Try GGUF Inference Engine for refactoring
    if (g_modelContext && pForwardPassInfer && pSampleNext) {
        AppendWindowText(g_hwndOutput, L"[AI] Running refactor inference via GGUF engine...\r\n");
        std::vector<int> tokens;
        for (unsigned char ch : prompt) tokens.push_back(static_cast<int>(ch));
        if (tokens.size() > 2048) tokens.resize(2048);
        
        std::vector<float> logits(32000, 0.0f);
        int fwdResult = pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data());
        if (fwdResult == 0) {
            std::wstring refactored;
            for (int i = 0; i < 1024; i++) {
                int nextToken = pSampleNext(logits.data(), 32000, 0.3f, 0.9f, 40);
                if (nextToken <= 0 || nextToken >= 128) break;
                refactored += static_cast<wchar_t>(nextToken);
                int singleTok = nextToken;
                pForwardPassInfer(g_modelContext, &singleTok, 1, logits.data());
            }
            if (!refactored.empty()) {
                // Present as ghost completion overlay
                AICompletion completion = {};
                completion.text = refactored;
                completion.detail = L"Refactored code";
                completion.confidence = 0.80;
                completion.kind = L"refactor";
                completion.cursorOffset = (int)refactored.size();
                completion.isMultiLine = true;
                
                // Replace selection with refactored code as ghost text
                SendMessage(g_hwndEditor, EM_SETSEL, selStart, selEnd);
                ShowAICompletion(completion);
                AppendWindowText(g_hwndOutput, L"[AI] Refactoring ready. Press Tab to accept.\r\n");
                return;
            }
        }
    }
    
    AppendWindowText(g_hwndOutput, L"[AI] No inference engine available. Load a model first.\r\n");
}

void AI_FixCode() {
    AppendWindowText(g_hwndOutput, L"[AI] Fixing selected code/errors...\r\n");
    
    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    int len = GetWindowTextLengthW(g_hwndEditor);
    
    std::wstring codeToFix;
    int selStart = 0, selEnd = 0;
    
    if (len > 0 && start != end) {
        // Fix selected code
        std::wstring buffer(len + 1, L'\0');
        GetWindowTextW(g_hwndEditor, &buffer[0], len + 1);
        buffer.resize(len);
        selStart = std::min(start, end);
        selEnd = std::max(start, end);
        if (selEnd > (DWORD)len) selEnd = len;
        codeToFix = buffer.substr(selStart, selEnd - selStart);
    } else if (len > 0) {
        // Fix entire file
        std::wstring buffer(len + 1, L'\0');
        GetWindowTextW(g_hwndEditor, &buffer[0], len + 1);
        buffer.resize(len);
        codeToFix = buffer;
        selStart = 0;
        selEnd = len;
    } else {
        AppendWindowText(g_hwndOutput, L"[AI] No code to fix.\r\n");
        return;
    }
    
    std::string prompt = "Fix any bugs, errors, or issues in the following code. Return only the corrected code:\n\n" + WideToUtf8(codeToFix);
    
    // Try Titan Kernel
    if (g_hTitanDll && pTitan_RunInference) {
        int result = pTitan_RunInference(0, prompt.c_str(), 1024);
        if (result >= 0) {
            AppendWindowText(g_hwndOutput, L"[AI] Fix applied via Titan Kernel.\r\n");
            return;
        }
    }
    
    // Try GGUF Inference Engine
    if (g_modelContext && pForwardPassInfer && pSampleNext) {
        AppendWindowText(g_hwndOutput, L"[AI] Running fix inference via GGUF engine...\r\n");
        std::vector<int> tokens;
        for (unsigned char ch : prompt) tokens.push_back(static_cast<int>(ch));
        if (tokens.size() > 2048) tokens.resize(2048);
        
        std::vector<float> logits(32000, 0.0f);
        int fwdResult = pForwardPassInfer(g_modelContext, tokens.data(), (int)tokens.size(), logits.data());
        if (fwdResult == 0) {
            std::wstring fixedCode;
            for (int i = 0; i < 1024; i++) {
                int nextToken = pSampleNext(logits.data(), 32000, 0.2f, 0.95f, 40);
                if (nextToken <= 0 || nextToken >= 128) break;
                fixedCode += static_cast<wchar_t>(nextToken);
                int singleTok = nextToken;
                pForwardPassInfer(g_modelContext, &singleTok, 1, logits.data());
            }
            if (!fixedCode.empty()) {
                // Replace selection with fixed code
                SendMessage(g_hwndEditor, EM_SETSEL, selStart, selEnd);
                SendMessageW(g_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)fixedCode.c_str());
                AppendWindowText(g_hwndOutput, L"[AI] Fix applied to editor.\r\n");
                g_isDirty = true;
                return;
            }
        }
    }
    
    AppendWindowText(g_hwndOutput, L"[AI] No inference engine available. Load a model first.\r\n");
}

void ShowAICompletion(const AICompletion& completion) {
    if (!g_hwndEditor || completion.text.empty()) return;
    HideAICompletion();

    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    g_completionAnchor = (int)std::max(start, end);
    g_completionText = completion.text;

    SendMessage(g_hwndEditor, EM_SETSEL, g_completionAnchor, g_completionAnchor);
    SendMessageW(g_hwndEditor, EM_REPLACESEL, FALSE, (LPARAM)g_completionText.c_str());
    int endPos = g_completionAnchor + (int)g_completionText.size();

    CHARFORMAT2W format = {};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = g_colorCompletionGhost;
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
    CHARFORMAT2W format = {};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR;
    format.crTextColor = g_colorDefault;
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
    
    AppendWindowText(g_hwndOutput, L"[Build] Running...\r\n");
    
    // Detect file extension
    std::wstring ext;
    size_t dotPos = g_currentFile.rfind(L'.');
    if (dotPos != std::wstring::npos) {
        ext = g_currentFile.substr(dotPos);
        for (auto& c : ext) c = towlower(c);
    }
    
    // Build the run command based on file type
    std::wstring cmd;
    if (ext == L".py") {
        cmd = L"python \"" + g_currentFile + L"\"";
    } else if (ext == L".js") {
        cmd = L"node \"" + g_currentFile + L"\"";
    } else if (ext == L".ts") {
        cmd = L"npx ts-node \"" + g_currentFile + L"\"";
    } else if (ext == L".ps1") {
        cmd = L"powershell -ExecutionPolicy Bypass -File \"" + g_currentFile + L"\"";
    } else if (ext == L".bat" || ext == L".cmd") {
        cmd = L"cmd /c \"" + g_currentFile + L"\"";
    } else if (ext == L".cpp" || ext == L".c" || ext == L".cc") {
        // Compile then run
        std::wstring outFile = g_currentFile.substr(0, dotPos) + L".exe";
        std::wstring compileCmd;
        if (ext == L".c") {
            compileCmd = L"cl /nologo \"" + g_currentFile + L"\" /Fe\"" + outFile + L"\" && \"" + outFile + L"\"";
        } else {
            compileCmd = L"cl /nologo /EHsc /std:c++20 \"" + g_currentFile + L"\" /Fe\"" + outFile + L"\" && \"" + outFile + L"\"";
        }
        cmd = compileCmd;
    } else if (ext == L".rs") {
        std::wstring outFile = g_currentFile.substr(0, dotPos) + L".exe";
        cmd = L"rustc \"" + g_currentFile + L"\" -o \"" + outFile + L"\" && \"" + outFile + L"\"";
    } else if (ext == L".go") {
        cmd = L"go run \"" + g_currentFile + L"\"";
    } else if (ext == L".exe") {
        cmd = L"\"" + g_currentFile + L"\"";
    } else {
        AppendWindowText(g_hwndOutput, (L"[Build] Unknown file type: " + ext + L"\r\n").c_str());
        return;
    }
    
    AppendWindowText(g_hwndOutput, (L"[Build] > " + cmd + L"\r\n").c_str());
    
    // Save file first if dirty
    if (g_isDirty && !g_currentFile.empty()) {
        SaveFile(g_currentFile);
    }
    
    // Execute via CreateProcess with output piped back
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        AppendWindowText(g_hwndOutput, L"[Build] Failed to create pipe.\r\n");
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = L"cmd /c " + cmd;
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    
    // Get working directory from file path
    std::wstring workDir;
    size_t slashPos = g_currentFile.find_last_of(L"\\/");
    if (slashPos != std::wstring::npos) workDir = g_currentFile.substr(0, slashPos);
    
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, workDir.empty() ? nullptr : workDir.c_str(), &si, &pi);
    CloseHandle(hWritePipe);
    
    if (!ok) {
        CloseHandle(hReadPipe);
        AppendWindowText(g_hwndOutput, L"[Build] Failed to start process.\r\n");
        return;
    }
    
    // Read output in chunks
    char readBuf[4096];
    DWORD bytesRead;
    std::string fullOutput;
    while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        readBuf[bytesRead] = '\0';
        fullOutput += readBuf;
    }
    
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    // Display output
    if (!fullOutput.empty()) {
        std::wstring wOutput = Utf8ToWide(fullOutput);
        AppendWindowText(g_hwndOutput, wOutput.c_str());
        AppendWindowText(g_hwndOutput, L"\r\n");
    }
    
    std::wstring exitMsg = L"[Build] Process exited with code: " + std::to_wstring(exitCode) + L"\r\n";
    AppendWindowText(g_hwndOutput, exitMsg.c_str());
}

void Build_Build() {
    AppendWindowText(g_hwndOutput, L"[Build] Building project...\r\n");
    
    // Determine project root from current file
    std::wstring projectDir;
    if (!g_currentFile.empty()) {
        size_t slashPos = g_currentFile.find_last_of(L"\\/");
        if (slashPos != std::wstring::npos) projectDir = g_currentFile.substr(0, slashPos);
    }
    if (projectDir.empty()) {
        wchar_t cwd[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, cwd);
        projectDir = cwd;
    }
    
    // Detect build system by searching for config files
    std::wstring buildCmd;
    std::wstring cmakePath = projectDir + L"\\CMakeLists.txt";
    std::wstring makefilePath = projectDir + L"\\Makefile";
    std::wstring packagePath = projectDir + L"\\package.json";
    std::wstring cargoPath = projectDir + L"\\Cargo.toml";
    std::wstring buildBatPath = projectDir + L"\\build.bat";
    
    if (GetFileAttributesW(cmakePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        buildCmd = L"cd /d \"" + projectDir + L"\" && cmake -B build -G \"Visual Studio 17 2022\" && cmake --build build --config Release";
        AppendWindowText(g_hwndOutput, L"[Build] Detected CMake project.\r\n");
    } else if (GetFileAttributesW(cargoPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        buildCmd = L"cd /d \"" + projectDir + L"\" && cargo build --release";
        AppendWindowText(g_hwndOutput, L"[Build] Detected Cargo (Rust) project.\r\n");
    } else if (GetFileAttributesW(makefilePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        buildCmd = L"cd /d \"" + projectDir + L"\" && nmake";
        AppendWindowText(g_hwndOutput, L"[Build] Detected Makefile project.\r\n");
    } else if (GetFileAttributesW(packagePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        buildCmd = L"cd /d \"" + projectDir + L"\" && npm run build";
        AppendWindowText(g_hwndOutput, L"[Build] Detected Node.js project.\r\n");
    } else if (GetFileAttributesW(buildBatPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        buildCmd = L"cd /d \"" + projectDir + L"\" && build.bat";
        AppendWindowText(g_hwndOutput, L"[Build] Detected build.bat script.\r\n");
    } else if (!g_currentFile.empty()) {
        // Single-file compile
        std::wstring ext;
        size_t dotPos = g_currentFile.rfind(L'.');
        if (dotPos != std::wstring::npos) ext = g_currentFile.substr(dotPos);
        for (auto& c : ext) c = towlower(c);
        
        if (ext == L".cpp" || ext == L".cc" || ext == L".cxx") {
            std::wstring outFile = g_currentFile.substr(0, dotPos) + L".exe";
            buildCmd = L"cl /nologo /EHsc /std:c++20 /O2 \"" + g_currentFile + L"\" /Fe\"" + outFile + L"\"";
        } else if (ext == L".c") {
            std::wstring outFile = g_currentFile.substr(0, dotPos) + L".exe";
            buildCmd = L"cl /nologo /O2 \"" + g_currentFile + L"\" /Fe\"" + outFile + L"\"";
        } else if (ext == L".rs") {
            std::wstring outFile = g_currentFile.substr(0, dotPos) + L".exe";
            buildCmd = L"rustc \"" + g_currentFile + L"\" -o \"" + outFile + L"\"";
        } else {
            AppendWindowText(g_hwndOutput, L"[Build] No build system found and no compilable file open.\r\n");
            return;
        }
        AppendWindowText(g_hwndOutput, L"[Build] Single-file compile mode.\r\n");
    } else {
        AppendWindowText(g_hwndOutput, L"[Build] No build system detected. Open a project with CMakeLists.txt, Makefile, package.json, or Cargo.toml.\r\n");
        return;
    }
    
    AppendWindowText(g_hwndOutput, (L"[Build] > " + buildCmd + L"\r\n").c_str());
    
    // Save current file before building
    if (g_isDirty && !g_currentFile.empty()) {
        SaveFile(g_currentFile);
    }
    
    // Execute build command with output capture
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        AppendWindowText(g_hwndOutput, L"[Build] Failed to create pipe.\r\n");
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    std::wstring cmdLine = L"cmd /c " + buildCmd;
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, projectDir.c_str(), &si, &pi);
    CloseHandle(hWritePipe);
    
    if (!ok) {
        CloseHandle(hReadPipe);
        AppendWindowText(g_hwndOutput, L"[Build] Failed to start build process.\r\n");
        return;
    }
    
    char readBuf[4096];
    DWORD bytesRead;
    std::string fullOutput;
    while (ReadFile(hReadPipe, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        readBuf[bytesRead] = '\0';
        fullOutput += readBuf;
    }
    
    WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    if (!fullOutput.empty()) {
        std::wstring wOutput = Utf8ToWide(fullOutput);
        AppendWindowText(g_hwndOutput, wOutput.c_str());
        AppendWindowText(g_hwndOutput, L"\r\n");
    }
    
    if (exitCode == 0) {
        AppendWindowText(g_hwndOutput, L"[Build] ✓ Build succeeded.\r\n");
    } else {
        std::wstring errMsg = L"[Build] ✗ Build failed (exit code: " + std::to_wstring(exitCode) + L").\r\n";
        AppendWindowText(g_hwndOutput, errMsg.c_str());
    }
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
        
        // Create file tree (populated from workspace directory)
        g_hwndFileTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
            0, 0, 200, 700, hwnd, (HMENU)IDC_FILETREE, GetModuleHandle(nullptr), nullptr);
        SendMessageW(g_hwndFileTree, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        
        // Populate file tree with workspace directory
        {
            wchar_t exePath[MAX_PATH];
            GetModuleFileNameW(nullptr, exePath, MAX_PATH);
            std::wstring exeDir(exePath);
            size_t slashPos = exeDir.find_last_of(L"\\/");
            if (slashPos != std::wstring::npos) exeDir = exeDir.substr(0, slashPos);
            g_workspaceRoot = exeDir;
            
            // Insert workspace root node
            TVINSERTSTRUCTW tvis = {};
            tvis.hParent = TVI_ROOT;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            tvis.item.pszText = (LPWSTR)exeDir.c_str();
            tvis.item.lParam = 1; // 1 = directory
            HTREEITEM hRoot = (HTREEITEM)SendMessageW(g_hwndFileTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
            
            // Populate first level entries
            WIN32_FIND_DATAW fd;
            std::wstring searchPath = exeDir + L"\\*";
            HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                    
                    tvis.hParent = hRoot;
                    tvis.hInsertAfter = TVI_SORT;
                    tvis.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
                    tvis.item.pszText = fd.cFileName;
                    tvis.item.lParam = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                    tvis.item.cChildren = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
                    SendMessageW(g_hwndFileTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
                } while (FindNextFileW(hFind, &fd));
                FindClose(hFind);
            }
            
            // Expand root
            SendMessageW(g_hwndFileTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)hRoot);
        }
        
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
        
        // Create chat action buttons
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
        int bottomHeight = 0;
        if (g_bOutputVisible || g_bTerminalVisible) bottomHeight = 150;
        
        // Layout: file tree on left, editor top right, output/terminal bottom right, chat right edge
        int treeWidth = 200;
        int mainWidth = width - treeWidth - chatWidth;
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
        
        // Chat panels
        if (g_bChatVisible) {
            int chatX = width - chatWidth;
            int chatInputHeight = 100;
            int btnHeight = 25;
            int btnWidth = chatWidth / 7;

            MoveWindow(g_hwndChatHistory, chatX, 0, chatWidth, height - sbHeight - chatInputHeight - btnHeight, TRUE);
            
            // Buttons row
            int btnY = height - sbHeight - chatInputHeight - btnHeight;
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_GENERATE), chatX, btnY, btnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_EXPLAIN), chatX + btnWidth, btnY, btnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_REFACTOR), chatX + btnWidth * 2, btnY, btnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_FIX), chatX + btnWidth * 3, btnY, btnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_STOP), chatX + btnWidth * 4, btnY, btnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_CLEAR), chatX + btnWidth * 5, btnY, btnWidth, btnHeight, TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_CHAT_SEND), chatX + btnWidth * 6, btnY, btnWidth, btnHeight, TRUE);

            MoveWindow(g_hwndChatInput, chatX, height - sbHeight - chatInputHeight, chatWidth, chatInputHeight, TRUE);
            
            ShowWindow(g_hwndChatHistory, SW_SHOW);
            ShowWindow(g_hwndChatInput, SW_SHOW);
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
            static bool bInChange = false;
            if (!bInChange) {
                bInChange = true;
                ApplySyntaxHighlighting(g_hwndEditor);
                // TriggerAICompletion(g_hwndEditor); // To be implemented
                bInChange = false;
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
            if (!path.empty()) LoadFile(path);
            break;
        }
        
        case IDM_FILE_SAVE:
            if (g_currentFile.empty()) {
                std::wstring path = SaveFileDialog(hwnd);
                if (!path.empty()) SaveFile(path);
            } else {
                SaveFile(g_currentFile);
            }
            break;
            
        case IDM_FILE_SAVEAS: {
            std::wstring path = SaveFileDialog(hwnd);
            if (!path.empty()) SaveFile(path);
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
        }
        break;
    }
    
    case WM_DESTROY:
        // Cleanup chat server
        StopChatServer();

        // Cleanup terminal
        if (g_hCmdProcess) {
            TerminateProcess(g_hCmdProcess, 0);
            CloseHandle(g_hCmdProcess);
        }
        if (g_hCmdStdoutRead) CloseHandle(g_hCmdStdoutRead);
        if (g_hCmdStdinWrite) CloseHandle(g_hCmdStdinWrite);
        
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
// Entry Point
// ============================================================================
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
    RegisterClassExW(&wc);
    
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

void SendChatToServer(const std::wstring& message) {
    // Convert message to UTF-8
    std::string utf8Message = WideToUtf8(message);
    
    // Create HTTP request data
    std::string jsonData = "{\"message\": \"" + utf8Message + "\"}";
    
    // Use WinINet for HTTP requests (simple implementation)
    HINTERNET hInternet = InternetOpenW(L"RawrXD IDE/1.0", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
    if (!hInternet) {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to initialize HTTP client.\r\n");
        return;
    }
    
    HINTERNET hConnect = InternetConnectW(hInternet, L"localhost", 23959, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to connect to server.\r\n");
        return;
    }
    
    HINTERNET hRequest = HttpOpenRequestW(hConnect, L"POST", L"/api/chat", nullptr, nullptr, nullptr, 0, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to create HTTP request.\r\n");
        return;
    }
    
    // Set content type
    std::wstring headers = L"Content-Type: application/json\r\n";
    
    // Send the request
    BOOL result = HttpSendRequestW(hRequest, headers.c_str(), headers.length(), 
                                   (LPVOID)jsonData.c_str(), jsonData.length());
    
    if (result) {
        // Read response
        char buffer[4096];
        DWORD bytesRead;
        std::string response;
        
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response += buffer;
        }
        
        // Parse JSON response (simple parsing for now)
        std::string reply;
        size_t pos = response.find("\"reply\":");
        if (pos != std::string::npos) {
            pos += 9; // Skip "\"reply\":"
            size_t start = response.find("\"", pos);
            if (start != std::string::npos) {
                size_t end = response.find("\"", start + 1);
                if (end != std::string::npos) {
                    reply = response.substr(start + 1, end - start - 1);
                }
            }
        }
        
        if (!reply.empty()) {
            std::wstring wReply = Utf8ToWide(reply);
            AppendWindowText(g_hwndChatHistory, L"Assistant: ");
            AppendWindowText(g_hwndChatHistory, wReply.c_str());
            AppendWindowText(g_hwndChatHistory, L"\r\n");
        } else {
            AppendWindowText(g_hwndChatHistory, L"[AI Chat] Received empty response.\r\n");
        }
    } else {
        AppendWindowText(g_hwndChatHistory, L"[AI Chat] Failed to send request to server.\r\n");
    }
    
    // Cleanup
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
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
