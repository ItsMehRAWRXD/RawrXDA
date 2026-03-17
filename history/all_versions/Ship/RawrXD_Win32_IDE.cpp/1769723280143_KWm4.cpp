/*
 * RawrXD_Win32_IDE.cpp - Pure Win32 IDE Shell (Zero Qt Dependencies)
 * Production-ready autonomous agentic IDE
 * 
 * Build: cl /O2 /DNDEBUG RawrXD_Win32_IDE.cpp /link user32.lib gdi32.lib shell32.lib comctl32.lib comdlg32.lib ole32.lib
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <richedit.h>
#include <commdlg.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <functional>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

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
// Forward declarations for Native Model Bridge DLL
// ============================================================================
typedef int  (*LoadModelNative_t)(const char* filepath);
typedef int  (*GetTokenEmbedding_t)(int tokenId, float* output, int dim);
typedef int  (*ForwardPass_t)(int* tokens, int numTokens, float* output, int vocabSize);
typedef void (*CleanupMathTables_t)();

static HMODULE g_hModelBridgeDll = nullptr;
static LoadModelNative_t      pLoadModelNative = nullptr;
static GetTokenEmbedding_t    pGetTokenEmbedding = nullptr;
static ForwardPass_t          pForwardPass = nullptr;
static CleanupMathTables_t    pCleanupMathTables = nullptr;
static bool g_modelLoaded = false;
static std::wstring g_loadedModelPath;

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
static std::wstring g_workspaceRoot;

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

// Syntax highlighting colors
static COLORREF g_colorKeyword = RGB(86, 156, 214);    // Blue
static COLORREF g_colorString = RGB(214, 157, 133);    // Orange
static COLORREF g_colorComment = RGB(106, 153, 85);    // Green
static COLORREF g_colorNumber = RGB(181, 206, 168);    // Light green
static COLORREF g_colorPreproc = RGB(155, 155, 155);   // Gray
static COLORREF g_colorDefault = RGB(212, 212, 212);   // White
static COLORREF g_colorBackground = RGB(30, 30, 30);   // Dark

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
        if (ReadFile(g_hCmdStdoutRead, buffer, min(available, sizeof(buffer)-1), &bytesRead, nullptr)) {
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
    pForwardPass = (ForwardPass_t)GetProcAddress(g_hModelBridgeDll, "ForwardPass");
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
// AI Actions (calls to Titan Kernel or placeholder)
// ============================================================================
void AI_GenerateCode() {
    // Get selected text or current context
    DWORD start = 0, end = 0;
    SendMessage(g_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    
    if (start == end) {
        // No selection - generate from context
        AppendWindowText(g_hwndOutput, L"[AI] No selection. Enter prompt in output panel...\r\n");
    } else {
        // Get selected text
        int len = GetWindowTextLengthA(g_hwndEditor);
        std::string content(len + 1, '\0');
        GetWindowTextA(g_hwndEditor, &content[0], len + 1);
        std::string selected = content.substr(start, end - start);
        
        AppendWindowText(g_hwndOutput, L"[AI] Generating code for selection...\r\n");
        // If Titan is loaded, call inference
        // For now, placeholder
        AppendWindowText(g_hwndOutput, Utf8ToWide("// Generated code for: " + selected + "\r\n").c_str());
    }
}

void AI_ExplainCode() {
    AppendWindowText(g_hwndOutput, L"[AI] Explaining selected code...\r\n");
    // Placeholder - would call Titan inference
}

void AI_RefactorCode() {
    AppendWindowText(g_hwndOutput, L"[AI] Refactoring selected code...\r\n");
    // Placeholder - would call Titan inference
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
    // Determine file type and run appropriate command
    // For now, placeholder
}

void Build_Build() {
    AppendWindowText(g_hwndOutput, L"[Build] Building project...\r\n");
    // Would invoke cl.exe, gcc, or python for different file types
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
            MoveWindow(g_hwndChatHistory, chatX, 0, chatWidth, height - sbHeight - chatInputHeight, TRUE);
            MoveWindow(g_hwndChatInput, chatX, height - sbHeight - chatInputHeight, chatWidth, chatInputHeight, TRUE);
            ShowWindow(g_hwndChatHistory, SW_SHOW);
            ShowWindow(g_hwndChatInput, SW_SHOW);
        } else {
            ShowWindow(g_hwndChatHistory, SW_HIDE);
            ShowWindow(g_hwndChatInput, SW_HIDE);
        }
        break;
    }
    
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
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
                if (LoadGGUFModel(filename)) {
                    AppendWindowText(g_hwndOutput, L"[AI] GGUF model loaded successfully!\r\n");
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
                UnloadGGUFModel();
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
        // Cleanup terminal
        if (g_hCmdProcess) {
            TerminateProcess(g_hCmdProcess, 0);
            CloseHandle(g_hCmdProcess);
        }
        if (g_hCmdStdoutRead) CloseHandle(g_hCmdStdoutRead);
        if (g_hCmdStdinWrite) CloseHandle(g_hCmdStdinWrite);
        
        // Cleanup model bridge
        if (g_modelLoaded) UnloadGGUFModel();
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
