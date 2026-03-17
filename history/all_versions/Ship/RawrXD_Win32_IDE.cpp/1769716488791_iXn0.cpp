/*
 * RawrXD_Win32_IDE.cpp - Pure Win32 IDE Shell (Zero Qt Dependencies)
 * Production-ready autonomous agentic IDE
 * 
 * Build: cl /O2 /DNDEBUG RawrXD_Win32_IDE.cpp /link user32.lib gdi32.lib shell32.lib comctl32.lib
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
#include <fstream>
#include <sstream>
#include <functional>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

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
// Globals
// ============================================================================
static HWND g_hwndMain = nullptr;
static HWND g_hwndEditor = nullptr;
static HWND g_hwndOutput = nullptr;
static HWND g_hwndFileTree = nullptr;
static HWND g_hwndToolbar = nullptr;
static HWND g_hwndStatusBar = nullptr;
static HWND g_hwndSplitter = nullptr;

static HFONT g_hFontCode = nullptr;
static HFONT g_hFontUI = nullptr;

static std::wstring g_currentFile;
static bool g_isDirty = false;
static std::wstring g_workspaceRoot;

// ============================================================================
// Control IDs
// ============================================================================
#define IDC_EDITOR      1001
#define IDC_OUTPUT      1002
#define IDC_FILETREE    1003
#define IDC_TOOLBAR     1004
#define IDC_STATUSBAR   1005

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

#define IDM_HELP_ABOUT  2401

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
    
    // AI
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_GENERATE, L"&Generate Code\tCtrl+G");
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_EXPLAIN, L"&Explain Code\tCtrl+E");
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_REFACTOR, L"&Refactor Code\tCtrl+R");
    AppendMenuW(hAIMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAIMenu, MF_STRING, IDM_AI_LOADMODEL, L"&Load Model...");
    
    // Help
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");
    
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
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
        
        // Layout: file tree on left, editor top right, output bottom right
        int treeWidth = 200;
        int outputHeight = 150;
        int editorHeight = height - sbHeight - outputHeight;
        
        MoveWindow(g_hwndFileTree, 0, 0, treeWidth, height - sbHeight, TRUE);
        MoveWindow(g_hwndEditor, treeWidth, 0, width - treeWidth, editorHeight, TRUE);
        MoveWindow(g_hwndOutput, treeWidth, editorHeight, width - treeWidth, outputHeight, TRUE);
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
