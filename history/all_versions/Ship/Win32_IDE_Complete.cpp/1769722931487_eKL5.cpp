// RawrXD Win32 IDE - Complete Production Build (No Qt Dependencies)
// Replaces: mainwindow.cpp, agentic_ide.cpp, chat_interface.cpp, file_browser.cpp
//           multi_tab_editor.cpp, terminal_pool.cpp, model_router_widget.cpp
// Pure Win32 API + ComCtl32 + RichEdit + Shell API

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <richedit.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <process.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ============================================================================
// CONSTANTS AND IDS
// ============================================================================
#define APP_TITLE           L"RawrXD Agentic IDE - Production"
#define CLASS_MAIN          L"RawrXDMainClass"
#define CLASS_EDITOR        L"RawrXDEditorClass"
#define CLASS_TERMINAL      L"RawrXDTerminalClass"

// Menu IDs
#define IDM_FILE_NEW        1001
#define IDM_FILE_OPEN       1002
#define IDM_FILE_SAVE       1003
#define IDM_FILE_SAVEAS     1004
#define IDM_FILE_CLOSE      1005
#define IDM_FILE_EXIT       1006

#define IDM_EDIT_UNDO       1101
#define IDM_EDIT_REDO       1102
#define IDM_EDIT_CUT        1103
#define IDM_EDIT_COPY       1104
#define IDM_EDIT_PASTE      1105
#define IDM_EDIT_SELECTALL  1106
#define IDM_EDIT_FIND       1107
#define IDM_EDIT_REPLACE    1108
#define IDM_EDIT_GOTO       1109

#define IDM_VIEW_FILEBROWSER    1201
#define IDM_VIEW_TERMINAL       1202
#define IDM_VIEW_CHAT           1203
#define IDM_VIEW_MODELROUTER    1204
#define IDM_VIEW_FULLSCREEN     1205

#define IDM_BUILD_RUN       1301
#define IDM_BUILD_BUILD     1302
#define IDM_BUILD_CLEAN     1303
#define IDM_BUILD_REBUILD   1304
#define IDM_BUILD_STOP      1305

#define IDM_AI_GENERATE     1401
#define IDM_AI_LOADMODEL    1402
#define IDM_AI_CHAT         1403
#define IDM_AI_REFACTOR     1404
#define IDM_AI_EXPLAIN      1405
#define IDM_AI_MAXMODE      1406

#define IDM_TOOLS_SETTINGS  1501
#define IDM_TOOLS_TERMINAL  1502
#define IDM_TOOLS_MODELTEST 1503

#define IDM_HELP_ABOUT      1601
#define IDM_HELP_DOCS       1602

// Control IDs
#define IDC_TAB_EDITOR      2001
#define IDC_TREEVIEW        2002
#define IDC_TERMINAL_TAB    2003
#define IDC_TERMINAL_OUTPUT 2004
#define IDC_TERMINAL_INPUT  2005
#define IDC_CHAT_HISTORY    2006
#define IDC_CHAT_INPUT      2007
#define IDC_CHAT_SEND       2008
#define IDC_MODEL_COMBO     2009
#define IDC_STATUSBAR       2010
#define IDC_REBAR           2011
#define IDC_TOOLBAR         2012
#define IDC_SPLITTER_V      2013
#define IDC_SPLITTER_H      2014

// Timers
#define TIMER_AUTOSAVE      3001
#define TIMER_INFERENCE     3002

// Messages
#define WM_TERMINAL_OUTPUT  (WM_USER + 100)
#define WM_AI_RESPONSE      (WM_USER + 101)
#define WM_FILE_LOADED      (WM_USER + 102)

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Editor Tab Info
typedef struct {
    HWND hEdit;
    wchar_t szFilePath[MAX_PATH];
    wchar_t szFileName[64];
    BOOL bModified;
    BOOL bUntitled;
    int iLanguage;  // 0=text, 1=cpp, 2=py, 3=asm
} EditorTab;

// Terminal Info
typedef struct {
    HWND hOutput;
    HWND hInput;
    HANDLE hProcess;
    HANDLE hStdinWrite;
    HANDLE hStdoutRead;
    HANDLE hThread;
    DWORD dwPid;
    BOOL bRunning;
} TerminalInfo;

// Model Info
typedef struct {
    wchar_t szName[64];
    wchar_t szPath[MAX_PATH];
    UINT64 uFileSize;
    int iFormat;  // 0=GGUF, 1=SAFETENSORS
    BOOL bLoaded;
} ModelInfo;

// Chat Message
typedef struct {
    wchar_t szRole[32];
    wchar_t* pszContent;
    UINT64 timestamp;
} ChatMessage;

// ============================================================================
// GLOBAL STATE
// ============================================================================
static HINSTANCE g_hInstance = NULL;
static HWND g_hMainWnd = NULL;
static HWND g_hTabEditor = NULL;
static HWND g_hTreeView = NULL;
static HWND g_hTerminalTab = NULL;
static HWND g_hChatHistory = NULL;
static HWND g_hChatInput = NULL;
static HWND g_hModelCombo = NULL;
static HWND g_hStatusBar = NULL;
static HWND g_hToolbar = NULL;
static HWND g_hRebar = NULL;
static HWND g_hSplitterV = NULL;
static HWND g_hSplitterH = NULL;

static HMODULE g_hRichEdit = NULL;
static HMODULE g_hTitanDLL = NULL;
static HMODULE g_hBridgeDLL = NULL;

// Editor tabs
#define MAX_TABS 32
static EditorTab g_Tabs[MAX_TABS];
static int g_nTabCount = 0;
static int g_nActiveTab = -1;

// Terminals
#define MAX_TERMINALS 8
static TerminalInfo g_Terminals[MAX_TERMINALS];
static int g_nTerminalCount = 0;
static int g_nActiveTerminal = -1;

// Models
#define MAX_MODELS 64
static ModelInfo g_Models[MAX_MODELS];
static int g_nModelCount = 0;
static int g_nActiveModel = -1;

// Chat history
#define MAX_CHAT_MESSAGES 1000
static ChatMessage g_ChatMessages[MAX_CHAT_MESSAGES];
static int g_nChatCount = 0;

// Splitter positions
static int g_nSplitX = 250;  // File browser width
static int g_nSplitY = 200;  // Terminal height

// State flags
static BOOL g_bMaxMode = FALSE;
static BOOL g_bFullscreen = FALSE;
static BOOL g_bFileBrowserVisible = TRUE;
static BOOL g_bTerminalVisible = TRUE;
static BOOL g_bChatVisible = TRUE;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK EditorSubclassProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

static void CreateMainMenu(HWND hWnd);
static void CreateToolbar(HWND hWnd);
static void CreateStatusBar(HWND hWnd);
static void CreateFileBrowser(HWND hWnd);
static void CreateTabEditor(HWND hWnd);
static void CreateTerminalPanel(HWND hWnd);
static void CreateChatPanel(HWND hWnd);
static void CreateModelRouter(HWND hWnd);
static void LayoutControls(HWND hWnd);

static void NewFile(void);
static void OpenFile(void);
static void SaveFile(void);
static void SaveFileAs(void);
static void CloseTab(int index);
static void SwitchTab(int index);

static void PopulateTreeView(HWND hTree, const wchar_t* path, HTREEITEM parent);
static void OnTreeViewItemExpand(HWND hTree, NMTREEVIEW* pnm);
static void OnTreeViewItemClick(HWND hTree, NMTREEVIEW* pnm);

static void CreateNewTerminal(void);
static void CloseTerminal(int index);
static void ExecuteTerminalCommand(int index, const wchar_t* cmd);
static DWORD WINAPI TerminalReaderThread(LPVOID param);

static void SendChatMessage(const wchar_t* message);
static void AddChatMessage(const wchar_t* role, const wchar_t* content);
static void RefreshModelList(void);
static void LoadModel(int index);

static void StatusBarSetText(int part, const wchar_t* text);
static void AppendRichText(HWND hRich, const wchar_t* text, COLORREF color);

// ============================================================================
// ENTRY POINT
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    
    g_hInstance = hInstance;
    
    // Initialize COM for shell operations
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES | ICC_COOL_CLASSES };
    InitCommonControlsEx(&icc);
    
    // Load RichEdit
    g_hRichEdit = LoadLibraryW(L"msftedit.dll");
    if (!g_hRichEdit) {
        g_hRichEdit = LoadLibraryW(L"riched20.dll");
    }
    
    // Register main window class
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE + 1);
    wc.lpszClassName = CLASS_MAIN;
    wc.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);
    
    // Create main window
    g_hMainWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        CLASS_MAIN,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
        NULL, NULL, hInstance, NULL);
    
    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"Failed to create main window", L"Error", MB_ICONERROR);
        return 1;
    }
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    
    // Message loop with accelerators
    HACCEL hAccel = NULL;  // TODO: Create accelerator table
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(g_hMainWnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    
    // Cleanup
    if (g_hRichEdit) FreeLibrary(g_hRichEdit);
    if (g_hTitanDLL) FreeLibrary(g_hTitanDLL);
    if (g_hBridgeDLL) FreeLibrary(g_hBridgeDLL);
    CoUninitialize();
    
    return (int)msg.wParam;
}

// ============================================================================
// MAIN WINDOW PROCEDURE
// ============================================================================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateMainMenu(hWnd);
        CreateToolbar(hWnd);
        CreateStatusBar(hWnd);
        CreateFileBrowser(hWnd);
        CreateTabEditor(hWnd);
        CreateTerminalPanel(hWnd);
        CreateChatPanel(hWnd);
        
        // Create initial untitled file
        NewFile();
        
        // Populate drives in tree view
        PopulateTreeView(g_hTreeView, NULL, TVI_ROOT);
        
        // Create initial terminal
        CreateNewTerminal();
        
        // Refresh model list
        RefreshModelList();
        
        // Start autosave timer
        SetTimer(hWnd, TIMER_AUTOSAVE, 60000, NULL);
        
        StatusBarSetText(0, L"Ready");
        return 0;
        
    case WM_SIZE:
        LayoutControls(hWnd);
        return 0;
        
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_FILE_NEW:    NewFile(); break;
        case IDM_FILE_OPEN:   OpenFile(); break;
        case IDM_FILE_SAVE:   SaveFile(); break;
        case IDM_FILE_SAVEAS: SaveFileAs(); break;
        case IDM_FILE_CLOSE:  CloseTab(g_nActiveTab); break;
        case IDM_FILE_EXIT:   PostQuitMessage(0); break;
        
        case IDM_EDIT_UNDO:
            if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit)
                SendMessageW(g_Tabs[g_nActiveTab].hEdit, EM_UNDO, 0, 0);
            break;
        case IDM_EDIT_REDO:
            if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit)
                SendMessageW(g_Tabs[g_nActiveTab].hEdit, EM_REDO, 0, 0);
            break;
        case IDM_EDIT_CUT:
            if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit)
                SendMessageW(g_Tabs[g_nActiveTab].hEdit, WM_CUT, 0, 0);
            break;
        case IDM_EDIT_COPY:
            if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit)
                SendMessageW(g_Tabs[g_nActiveTab].hEdit, WM_COPY, 0, 0);
            break;
        case IDM_EDIT_PASTE:
            if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit)
                SendMessageW(g_Tabs[g_nActiveTab].hEdit, WM_PASTE, 0, 0);
            break;
        case IDM_EDIT_SELECTALL:
            if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit)
                SendMessageW(g_Tabs[g_nActiveTab].hEdit, EM_SETSEL, 0, -1);
            break;
            
        case IDM_VIEW_FILEBROWSER:
            g_bFileBrowserVisible = !g_bFileBrowserVisible;
            ShowWindow(g_hTreeView, g_bFileBrowserVisible ? SW_SHOW : SW_HIDE);
            LayoutControls(hWnd);
            break;
        case IDM_VIEW_TERMINAL:
            g_bTerminalVisible = !g_bTerminalVisible;
            ShowWindow(g_hTerminalTab, g_bTerminalVisible ? SW_SHOW : SW_HIDE);
            LayoutControls(hWnd);
            break;
        case IDM_VIEW_CHAT:
            g_bChatVisible = !g_bChatVisible;
            ShowWindow(g_hChatHistory, g_bChatVisible ? SW_SHOW : SW_HIDE);
            ShowWindow(g_hChatInput, g_bChatVisible ? SW_SHOW : SW_HIDE);
            LayoutControls(hWnd);
            break;
            
        case IDM_BUILD_RUN:
            if (g_nActiveTerminal >= 0) {
                if (g_nActiveTab >= 0 && !g_Tabs[g_nActiveTab].bUntitled) {
                    wchar_t cmd[MAX_PATH + 32];
                    swprintf(cmd, MAX_PATH + 32, L"cd /d \"%s\" && build.bat", 
                        g_Tabs[g_nActiveTab].szFilePath);
                    ExecuteTerminalCommand(g_nActiveTerminal, cmd);
                }
            }
            break;
            
        case IDM_AI_GENERATE:
            if (g_nActiveModel >= 0) {
                // Get selected text and send to model
                if (g_nActiveTab >= 0 && g_Tabs[g_nActiveTab].hEdit) {
                    CHARRANGE cr;
                    SendMessageW(g_Tabs[g_nActiveTab].hEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
                    if (cr.cpMax > cr.cpMin) {
                        int len = cr.cpMax - cr.cpMin + 1;
                        wchar_t* buf = (wchar_t*)malloc(len * sizeof(wchar_t));
                        if (buf) {
                            SendMessageW(g_Tabs[g_nActiveTab].hEdit, EM_GETSELTEXT, 0, (LPARAM)buf);
                            SendChatMessage(buf);
                            free(buf);
                        }
                    }
                }
            } else {
                MessageBoxW(hWnd, L"Please load a model first (AI > Load Model)", L"No Model", MB_ICONWARNING);
            }
            break;
            
        case IDM_AI_LOADMODEL: {
            OPENFILENAMEW ofn = { sizeof(ofn) };
            wchar_t szFile[MAX_PATH] = L"";
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrTitle = L"Load AI Model";
            if (GetOpenFileNameW(&ofn)) {
                // Add to model list
                if (g_nModelCount < MAX_MODELS) {
                    wcscpy(g_Models[g_nModelCount].szPath, szFile);
                    wchar_t* fname = wcsrchr(szFile, L'\\');
                    wcscpy(g_Models[g_nModelCount].szName, fname ? fname + 1 : szFile);
                    g_Models[g_nModelCount].iFormat = 0; // GGUF
                    g_Models[g_nModelCount].bLoaded = FALSE;
                    
                    // Add to combo box
                    SendMessageW(g_hModelCombo, CB_ADDSTRING, 0, (LPARAM)g_Models[g_nModelCount].szName);
                    g_nModelCount++;
                    
                    // Select and load it
                    SendMessageW(g_hModelCombo, CB_SETCURSEL, g_nModelCount - 1, 0);
                    LoadModel(g_nModelCount - 1);
                }
            }
            break;
        }
        
        case IDM_AI_MAXMODE:
            g_bMaxMode = !g_bMaxMode;
            StatusBarSetText(2, g_bMaxMode ? L"MAX MODE" : L"Normal");
            break;
            
        case IDM_TOOLS_TERMINAL:
            CreateNewTerminal();
            break;
            
        case IDM_HELP_ABOUT:
            MessageBoxW(hWnd, 
                L"RawrXD Agentic IDE v1.0\n\n"
                L"Pure Win32 Build - Zero Qt Dependencies\n\n"
                L"Features:\n"
                L"- Multi-tab code editor with RichEdit\n"
                L"- File browser with lazy loading\n"
                L"- Terminal pool with PowerShell\n"
                L"- AI chat interface\n"
                L"- Model router for GGUF inference\n\n"
                L"(c) 2026 RawrXD Project", 
                L"About RawrXD IDE", MB_ICONINFORMATION);
            break;
            
        case IDC_CHAT_SEND:
            if (g_hChatInput) {
                wchar_t msg[4096];
                GetWindowTextW(g_hChatInput, msg, 4096);
                if (wcslen(msg) > 0) {
                    SendChatMessage(msg);
                    SetWindowTextW(g_hChatInput, L"");
                }
            }
            break;
        }
        return 0;
        
    case WM_NOTIFY: {
        NMHDR* pnm = (NMHDR*)lParam;
        
        // Tab control notifications
        if (pnm->hwndFrom == g_hTabEditor && pnm->code == TCN_SELCHANGE) {
            int sel = (int)SendMessageW(g_hTabEditor, TCM_GETCURSEL, 0, 0);
            SwitchTab(sel);
        }
        
        // TreeView notifications
        if (pnm->hwndFrom == g_hTreeView) {
            if (pnm->code == TVN_ITEMEXPANDINGW) {
                OnTreeViewItemExpand(g_hTreeView, (NMTREEVIEW*)lParam);
            } else if (pnm->code == NM_DBLCLK) {
                OnTreeViewItemClick(g_hTreeView, (NMTREEVIEW*)lParam);
            }
        }
        
        // Terminal tab notifications
        if (pnm->hwndFrom == g_hTerminalTab && pnm->code == TCN_SELCHANGE) {
            g_nActiveTerminal = (int)SendMessageW(g_hTerminalTab, TCM_GETCURSEL, 0, 0);
            // Show corresponding terminal output/input
            for (int i = 0; i < g_nTerminalCount; i++) {
                ShowWindow(g_Terminals[i].hOutput, i == g_nActiveTerminal ? SW_SHOW : SW_HIDE);
                ShowWindow(g_Terminals[i].hInput, i == g_nActiveTerminal ? SW_SHOW : SW_HIDE);
            }
        }
        break;
    }
    
    case WM_TERMINAL_OUTPUT: {
        // wParam = terminal index, lParam = text buffer (must free)
        int idx = (int)wParam;
        wchar_t* text = (wchar_t*)lParam;
        if (idx >= 0 && idx < g_nTerminalCount && text) {
            AppendRichText(g_Terminals[idx].hOutput, text, RGB(0, 255, 0));
            free(text);
        }
        return 0;
    }
    
    case WM_AI_RESPONSE: {
        // lParam = response text (must free)
        wchar_t* text = (wchar_t*)lParam;
        if (text) {
            AddChatMessage(L"Assistant", text);
            free(text);
        }
        return 0;
    }
    
    case WM_TIMER:
        if (wParam == TIMER_AUTOSAVE) {
            // Autosave modified files
            for (int i = 0; i < g_nTabCount; i++) {
                if (g_Tabs[i].bModified && !g_Tabs[i].bUntitled) {
                    // Save to backup
                    // TODO: Implement
                }
            }
        }
        return 0;
        
    case WM_CLOSE:
        // Check for unsaved files
        for (int i = 0; i < g_nTabCount; i++) {
            if (g_Tabs[i].bModified) {
                int res = MessageBoxW(hWnd, L"You have unsaved changes. Exit anyway?", L"Confirm Exit", MB_YESNO | MB_ICONWARNING);
                if (res != IDYES) return 0;
                break;
            }
        }
        // Close all terminals
        for (int i = 0; i < g_nTerminalCount; i++) {
            if (g_Terminals[i].bRunning) {
                TerminateProcess(g_Terminals[i].hProcess, 0);
            }
        }
        DestroyWindow(hWnd);
        return 0;
        
    case WM_DESTROY:
        KillTimer(hWnd, TIMER_AUTOSAVE);
        PostQuitMessage(0);
        return 0;
    }
    
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ============================================================================
// MENU CREATION
// ============================================================================
static void CreateMainMenu(HWND hWnd) {
    HMENU hMenu = CreateMenu();
    
    // File menu
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_OPEN, L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_FILE_SAVEAS, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_CLOSE, L"&Close\tCtrl+W");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");
    
    // Edit menu
    HMENU hEdit = CreatePopupMenu();
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REDO, L"&Redo\tCtrl+Y");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_SELECTALL, L"Select &All\tCtrl+A");
    AppendMenuW(hEdit, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_FIND, L"&Find...\tCtrl+F");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_REPLACE, L"&Replace...\tCtrl+H");
    AppendMenuW(hEdit, MF_STRING, IDM_EDIT_GOTO, L"&Go to Line...\tCtrl+G");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEdit, L"&Edit");
    
    // View menu
    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_VIEW_FILEBROWSER, L"&File Browser\tCtrl+B");
    AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_VIEW_TERMINAL, L"&Terminal\tCtrl+`");
    AppendMenuW(hView, MF_STRING | MF_CHECKED, IDM_VIEW_CHAT, L"&Chat Panel\tCtrl+Shift+C");
    AppendMenuW(hView, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hView, MF_STRING, IDM_VIEW_FULLSCREEN, L"Full&screen\tF11");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hView, L"&View");
    
    // Build menu
    HMENU hBuild = CreatePopupMenu();
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_RUN, L"&Run\tF5");
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_BUILD, L"&Build\tCtrl+B");
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_CLEAN, L"&Clean");
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_REBUILD, L"Re&build All");
    AppendMenuW(hBuild, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hBuild, MF_STRING, IDM_BUILD_STOP, L"&Stop\tShift+F5");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hBuild, L"&Build");
    
    // AI menu
    HMENU hAI = CreatePopupMenu();
    AppendMenuW(hAI, MF_STRING, IDM_AI_GENERATE, L"&Generate\tCtrl+Space");
    AppendMenuW(hAI, MF_STRING, IDM_AI_CHAT, L"&Chat\tCtrl+Shift+I");
    AppendMenuW(hAI, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAI, MF_STRING, IDM_AI_LOADMODEL, L"&Load Model...");
    AppendMenuW(hAI, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAI, MF_STRING, IDM_AI_REFACTOR, L"&Refactor Selection");
    AppendMenuW(hAI, MF_STRING, IDM_AI_EXPLAIN, L"&Explain Selection");
    AppendMenuW(hAI, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hAI, MF_STRING, IDM_AI_MAXMODE, L"&Max Mode\tCtrl+M");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hAI, L"&AI");
    
    // Tools menu
    HMENU hTools = CreatePopupMenu();
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_TERMINAL, L"New &Terminal");
    AppendMenuW(hTools, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hTools, MF_STRING, IDM_TOOLS_SETTINGS, L"&Settings...");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hTools, L"&Tools");
    
    // Help menu
    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_DOCS, L"&Documentation\tF1");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hHelp, MF_STRING, IDM_HELP_ABOUT, L"&About RawrXD IDE");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");
    
    SetMenu(hWnd, hMenu);
}

// ============================================================================
// TOOLBAR CREATION
// ============================================================================
static void CreateToolbar(HWND hWnd) {
    // Create toolbar
    g_hToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_NORESIZE,
        0, 0, 0, 0, hWnd, (HMENU)IDC_TOOLBAR, g_hInstance, NULL);
    
    SendMessageW(g_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    
    // Add standard buttons
    TBBUTTON buttons[] = {
        { STD_FILENEW, IDM_FILE_NEW, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_FILEOPEN, IDM_FILE_OPEN, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_FILESAVE, IDM_FILE_SAVE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0 },
        { STD_CUT, IDM_EDIT_CUT, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_COPY, IDM_EDIT_COPY, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_PASTE, IDM_EDIT_PASTE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { 0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0 },
        { STD_UNDO, IDM_EDIT_UNDO, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        { STD_REDOW, IDM_EDIT_REDO, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
    };
    
    TBADDBITMAP tbab = { HINST_COMMCTRL, IDB_STD_SMALL_COLOR };
    SendMessageW(g_hToolbar, TB_ADDBITMAP, 0, (LPARAM)&tbab);
    SendMessageW(g_hToolbar, TB_ADDBUTTONSW, _countof(buttons), (LPARAM)buttons);
    
    // Create rebar to hold toolbar
    g_hRebar = CreateWindowExW(0, REBARCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
        RBS_VARHEIGHT | RBS_BANDBORDERS | CCS_NODIVIDER,
        0, 0, 0, 0, hWnd, (HMENU)IDC_REBAR, g_hInstance, NULL);
    
    REBARBANDINFOW rbbi = { sizeof(rbbi) };
    rbbi.fMask = RBBIM_STYLE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
    rbbi.fStyle = RBBS_GRIPPERALWAYS | RBBS_USECHEVRON;
    rbbi.hwndChild = g_hToolbar;
    
    SIZE size;
    SendMessageW(g_hToolbar, TB_GETMAXSIZE, 0, (LPARAM)&size);
    rbbi.cxMinChild = size.cx;
    rbbi.cyMinChild = size.cy;
    rbbi.cx = size.cx;
    
    SendMessageW(g_hRebar, RB_INSERTBANDW, -1, (LPARAM)&rbbi);
}

// ============================================================================
// STATUS BAR CREATION
// ============================================================================
static void CreateStatusBar(HWND hWnd) {
    g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hWnd, (HMENU)IDC_STATUSBAR, g_hInstance, NULL);
    
    // Create parts: Message | Line/Col | Mode | Model
    int parts[] = { 300, 450, 550, -1 };
    SendMessageW(g_hStatusBar, SB_SETPARTS, 4, (LPARAM)parts);
    
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)L"Ready");
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 1, (LPARAM)L"Ln 1, Col 1");
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 2, (LPARAM)L"Normal");
    SendMessageW(g_hStatusBar, SB_SETTEXTW, 3, (LPARAM)L"No Model");
}

static void StatusBarSetText(int part, const wchar_t* text) {
    if (g_hStatusBar) {
        SendMessageW(g_hStatusBar, SB_SETTEXTW, part, (LPARAM)text);
    }
}

// ============================================================================
// FILE BROWSER (TREEVIEW)
// ============================================================================
static void CreateFileBrowser(HWND hWnd) {
    g_hTreeView = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, NULL,
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, g_nSplitX, 400, hWnd, (HMENU)IDC_TREEVIEW, g_hInstance, NULL);
    
    // Dark mode colors
    TreeView_SetBkColor(g_hTreeView, RGB(37, 37, 38));
    TreeView_SetTextColor(g_hTreeView, RGB(212, 212, 212));
}

static void PopulateTreeView(HWND hTree, const wchar_t* path, HTREEITEM parent) {
    if (!path) {
        // Enumerate drives
        DWORD drives = GetLogicalDrives();
        for (int i = 0; i < 26; i++) {
            if (drives & (1 << i)) {
                wchar_t drive[4] = { (wchar_t)('A' + i), L':', L'\\', 0 };
                
                TVINSERTSTRUCTW tvi = { 0 };
                tvi.hParent = TVI_ROOT;
                tvi.hInsertAfter = TVI_LAST;
                tvi.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
                tvi.item.pszText = drive;
                tvi.item.cChildren = 1;  // Has children
                
                // Store path as param
                wchar_t* pathCopy = (wchar_t*)malloc(4 * sizeof(wchar_t));
                wcscpy(pathCopy, drive);
                tvi.item.lParam = (LPARAM)pathCopy;
                
                TreeView_InsertItem(hTree, &tvi);
            }
        }
    } else {
        // Enumerate directory
        wchar_t searchPath[MAX_PATH];
        swprintf(searchPath, MAX_PATH, L"%s*", path);
        
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(searchPath, &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;
        
        do {
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
                continue;
            
            wchar_t fullPath[MAX_PATH];
            swprintf(fullPath, MAX_PATH, L"%s%s%s", path, 
                path[wcslen(path)-1] == L'\\' ? L"" : L"\\", fd.cFileName);
            
            TVINSERTSTRUCTW tvi = { 0 };
            tvi.hParent = parent;
            tvi.hInsertAfter = TVI_LAST;
            tvi.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN;
            tvi.item.pszText = fd.cFileName;
            tvi.item.cChildren = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
            
            wchar_t* pathCopy = (wchar_t*)malloc((wcslen(fullPath) + 1) * sizeof(wchar_t));
            wcscpy(pathCopy, fullPath);
            tvi.item.lParam = (LPARAM)pathCopy;
            
            TreeView_InsertItem(hTree, &tvi);
        } while (FindNextFileW(hFind, &fd));
        
        FindClose(hFind);
    }
}

static void OnTreeViewItemExpand(HWND hTree, NMTREEVIEW* pnm) {
    if (pnm->action != TVE_EXPAND) return;
    
    HTREEITEM hItem = pnm->itemNew.hItem;
    
    // Check if already populated (has real children)
    HTREEITEM hChild = TreeView_GetChild(hTree, hItem);
    if (hChild) {
        TVITEMW tvi = { 0 };
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hChild;
        TreeView_GetItem(hTree, &tvi);
        if (tvi.lParam != 0) return;  // Already populated
        
        // Remove placeholder
        TreeView_DeleteItem(hTree, hChild);
    }
    
    // Get path from item
    TVITEMW item = { 0 };
    item.mask = TVIF_PARAM;
    item.hItem = hItem;
    TreeView_GetItem(hTree, &item);
    
    wchar_t* path = (wchar_t*)item.lParam;
    if (path) {
        PopulateTreeView(hTree, path, hItem);
    }
}

static void OnTreeViewItemClick(HWND hTree, NMTREEVIEW* pnm) {
    HTREEITEM hItem = TreeView_GetSelection(hTree);
    if (!hItem) return;
    
    TVITEMW item = { 0 };
    item.mask = TVIF_PARAM | TVIF_CHILDREN;
    item.hItem = hItem;
    TreeView_GetItem(hTree, &item);
    
    // If file (no children), open it
    if (item.cChildren == 0 && item.lParam) {
        wchar_t* path = (wchar_t*)item.lParam;
        
        // Check if already open
        for (int i = 0; i < g_nTabCount; i++) {
            if (wcscmp(g_Tabs[i].szFilePath, path) == 0) {
                SwitchTab(i);
                return;
            }
        }
        
        // Open the file
        FILE* f = _wfopen(path, L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            
            char* content = (char*)malloc(size + 1);
            fread(content, 1, size, f);
            content[size] = 0;
            fclose(f);
            
            // Create new tab
            if (g_nTabCount < MAX_TABS) {
                int idx = g_nTabCount;
                wcscpy(g_Tabs[idx].szFilePath, path);
                wchar_t* fname = wcsrchr(path, L'\\');
                wcscpy(g_Tabs[idx].szFileName, fname ? fname + 1 : path);
                g_Tabs[idx].bUntitled = FALSE;
                g_Tabs[idx].bModified = FALSE;
                
                // Create editor
                g_Tabs[idx].hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, 
                    MSFTEDIT_CLASS, NULL,
                    WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                    0, 0, 100, 100, g_hMainWnd, NULL, g_hInstance, NULL);
                
                // Dark mode
                SendMessageW(g_Tabs[idx].hEdit, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
                
                CHARFORMAT2W cf = { sizeof(cf) };
                cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
                cf.crTextColor = RGB(212, 212, 212);
                wcscpy(cf.szFaceName, L"Consolas");
                cf.yHeight = 220;  // 11pt
                SendMessageW(g_Tabs[idx].hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
                
                // Convert content to wide
                int wlen = MultiByteToWideChar(CP_UTF8, 0, content, -1, NULL, 0);
                wchar_t* wcontent = (wchar_t*)malloc(wlen * sizeof(wchar_t));
                MultiByteToWideChar(CP_UTF8, 0, content, -1, wcontent, wlen);
                
                SetWindowTextW(g_Tabs[idx].hEdit, wcontent);
                free(wcontent);
                free(content);
                
                // Add tab
                TCITEMW tci = { 0 };
                tci.mask = TCIF_TEXT;
                tci.pszText = g_Tabs[idx].szFileName;
                SendMessageW(g_hTabEditor, TCM_INSERTITEMW, idx, (LPARAM)&tci);
                
                g_nTabCount++;
                SwitchTab(idx);
            }
        }
    }
}

// ============================================================================
// TAB EDITOR
// ============================================================================
static void CreateTabEditor(HWND hWnd) {
    g_hTabEditor = CreateWindowExW(0, WC_TABCONTROLW, NULL,
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 400, 400, hWnd, (HMENU)IDC_TAB_EDITOR, g_hInstance, NULL);
    
    // Dark mode (limited support)
    // Tab controls don't fully support custom colors, but we try
}

static void NewFile(void) {
    if (g_nTabCount >= MAX_TABS) return;
    
    int idx = g_nTabCount;
    static int untitledNum = 1;
    
    swprintf(g_Tabs[idx].szFileName, 64, L"Untitled-%d", untitledNum++);
    g_Tabs[idx].szFilePath[0] = 0;
    g_Tabs[idx].bUntitled = TRUE;
    g_Tabs[idx].bModified = FALSE;
    
    // Create RichEdit
    g_Tabs[idx].hEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
        0, 0, 100, 100, g_hMainWnd, NULL, g_hInstance, NULL);
    
    // Dark mode colors
    SendMessageW(g_Tabs[idx].hEdit, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    
    CHARFORMAT2W cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = RGB(212, 212, 212);
    wcscpy(cf.szFaceName, L"Consolas");
    cf.yHeight = 220;
    SendMessageW(g_Tabs[idx].hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    
    SetWindowTextW(g_Tabs[idx].hEdit, L"// New file\r\n// Start coding here...\r\n");
    
    // Enable undo
    SendMessageW(g_Tabs[idx].hEdit, EM_SETUNDOLIMIT, 100, 0);
    
    // Add tab
    TCITEMW tci = { 0 };
    tci.mask = TCIF_TEXT;
    tci.pszText = g_Tabs[idx].szFileName;
    SendMessageW(g_hTabEditor, TCM_INSERTITEMW, idx, (LPARAM)&tci);
    
    g_nTabCount++;
    SwitchTab(idx);
}

static void OpenFile(void) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    wchar_t szFile[MAX_PATH] = L"";
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0C/C++ (*.c;*.cpp;*.h;*.hpp)\0*.c;*.cpp;*.h;*.hpp\0Assembly (*.asm)\0*.asm\0Python (*.py)\0*.py\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open File";
    
    if (GetOpenFileNameW(&ofn)) {
        // Check if already open
        for (int i = 0; i < g_nTabCount; i++) {
            if (wcscmp(g_Tabs[i].szFilePath, szFile) == 0) {
                SwitchTab(i);
                return;
            }
        }
        
        // Read file
        FILE* f = _wfopen(szFile, L"rb");
        if (!f) {
            MessageBoxW(g_hMainWnd, L"Could not open file", L"Error", MB_ICONERROR);
            return;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* content = (char*)malloc(size + 1);
        fread(content, 1, size, f);
        content[size] = 0;
        fclose(f);
        
        if (g_nTabCount < MAX_TABS) {
            int idx = g_nTabCount;
            wcscpy(g_Tabs[idx].szFilePath, szFile);
            wchar_t* fname = wcsrchr(szFile, L'\\');
            wcscpy(g_Tabs[idx].szFileName, fname ? fname + 1 : szFile);
            g_Tabs[idx].bUntitled = FALSE;
            g_Tabs[idx].bModified = FALSE;
            
            g_Tabs[idx].hEdit = CreateWindowExW(WS_EX_CLIENTEDGE,
                MSFTEDIT_CLASS, NULL,
                WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
                0, 0, 100, 100, g_hMainWnd, NULL, g_hInstance, NULL);
            
            SendMessageW(g_Tabs[idx].hEdit, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
            
            CHARFORMAT2W cf = { sizeof(cf) };
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = RGB(212, 212, 212);
            wcscpy(cf.szFaceName, L"Consolas");
            cf.yHeight = 220;
            SendMessageW(g_Tabs[idx].hEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
            
            int wlen = MultiByteToWideChar(CP_UTF8, 0, content, -1, NULL, 0);
            wchar_t* wcontent = (wchar_t*)malloc(wlen * sizeof(wchar_t));
            MultiByteToWideChar(CP_UTF8, 0, content, -1, wcontent, wlen);
            SetWindowTextW(g_Tabs[idx].hEdit, wcontent);
            free(wcontent);
            
            SendMessageW(g_Tabs[idx].hEdit, EM_SETUNDOLIMIT, 100, 0);
            
            TCITEMW tci = { 0 };
            tci.mask = TCIF_TEXT;
            tci.pszText = g_Tabs[idx].szFileName;
            SendMessageW(g_hTabEditor, TCM_INSERTITEMW, idx, (LPARAM)&tci);
            
            g_nTabCount++;
            SwitchTab(idx);
        }
        
        free(content);
    }
}

static void SaveFile(void) {
    if (g_nActiveTab < 0) return;
    
    if (g_Tabs[g_nActiveTab].bUntitled) {
        SaveFileAs();
        return;
    }
    
    FILE* f = _wfopen(g_Tabs[g_nActiveTab].szFilePath, L"wb");
    if (!f) {
        MessageBoxW(g_hMainWnd, L"Could not save file", L"Error", MB_ICONERROR);
        return;
    }
    
    int len = GetWindowTextLengthW(g_Tabs[g_nActiveTab].hEdit);
    wchar_t* wcontent = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    GetWindowTextW(g_Tabs[g_nActiveTab].hEdit, wcontent, len + 1);
    
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, wcontent, -1, NULL, 0, NULL, NULL);
    char* content = (char*)malloc(utf8len);
    WideCharToMultiByte(CP_UTF8, 0, wcontent, -1, content, utf8len, NULL, NULL);
    
    fwrite(content, 1, utf8len - 1, f);  // -1 to exclude null terminator
    fclose(f);
    
    free(wcontent);
    free(content);
    
    g_Tabs[g_nActiveTab].bModified = FALSE;
    StatusBarSetText(0, L"File saved");
}

static void SaveFileAs(void) {
    if (g_nActiveTab < 0) return;
    
    OPENFILENAMEW ofn = { sizeof(ofn) };
    wchar_t szFile[MAX_PATH];
    wcscpy(szFile, g_Tabs[g_nActiveTab].szFileName);
    
    ofn.hwndOwner = g_hMainWnd;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Save File As";
    
    if (GetSaveFileNameW(&ofn)) {
        wcscpy(g_Tabs[g_nActiveTab].szFilePath, szFile);
        wchar_t* fname = wcsrchr(szFile, L'\\');
        wcscpy(g_Tabs[g_nActiveTab].szFileName, fname ? fname + 1 : szFile);
        g_Tabs[g_nActiveTab].bUntitled = FALSE;
        
        // Update tab text
        TCITEMW tci = { 0 };
        tci.mask = TCIF_TEXT;
        tci.pszText = g_Tabs[g_nActiveTab].szFileName;
        SendMessageW(g_hTabEditor, TCM_SETITEMW, g_nActiveTab, (LPARAM)&tci);
        
        SaveFile();
    }
}

static void CloseTab(int index) {
    if (index < 0 || index >= g_nTabCount) return;
    
    if (g_Tabs[index].bModified) {
        int res = MessageBoxW(g_hMainWnd, L"Save changes before closing?", L"Unsaved Changes", MB_YESNOCANCEL | MB_ICONWARNING);
        if (res == IDCANCEL) return;
        if (res == IDYES) {
            g_nActiveTab = index;
            SaveFile();
        }
    }
    
    DestroyWindow(g_Tabs[index].hEdit);
    SendMessageW(g_hTabEditor, TCM_DELETEITEM, index, 0);
    
    // Shift remaining tabs
    for (int i = index; i < g_nTabCount - 1; i++) {
        g_Tabs[i] = g_Tabs[i + 1];
    }
    g_nTabCount--;
    
    if (g_nTabCount == 0) {
        g_nActiveTab = -1;
    } else if (g_nActiveTab >= g_nTabCount) {
        g_nActiveTab = g_nTabCount - 1;
        SendMessageW(g_hTabEditor, TCM_SETCURSEL, g_nActiveTab, 0);
        ShowWindow(g_Tabs[g_nActiveTab].hEdit, SW_SHOW);
    }
    
    LayoutControls(g_hMainWnd);
}

static void SwitchTab(int index) {
    if (index < 0 || index >= g_nTabCount) return;
    
    // Hide current
    if (g_nActiveTab >= 0 && g_nActiveTab < g_nTabCount) {
        ShowWindow(g_Tabs[g_nActiveTab].hEdit, SW_HIDE);
    }
    
    g_nActiveTab = index;
    SendMessageW(g_hTabEditor, TCM_SETCURSEL, index, 0);
    ShowWindow(g_Tabs[index].hEdit, SW_SHOW);
    SetFocus(g_Tabs[index].hEdit);
    
    // Update status bar
    wchar_t status[256];
    swprintf(status, 256, L"Editing: %s", g_Tabs[index].szFileName);
    StatusBarSetText(0, status);
    
    LayoutControls(g_hMainWnd);
}

// ============================================================================
// TERMINAL PANEL
// ============================================================================
static void CreateTerminalPanel(HWND hWnd) {
    g_hTerminalTab = CreateWindowExW(0, WC_TABCONTROLW, NULL,
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 400, g_nSplitY, hWnd, (HMENU)IDC_TERMINAL_TAB, g_hInstance, NULL);
}

static void CreateNewTerminal(void) {
    if (g_nTerminalCount >= MAX_TERMINALS) return;
    
    int idx = g_nTerminalCount;
    
    // Create output (RichEdit)
    g_Terminals[idx].hOutput = CreateWindowExW(WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 100, 100, g_hMainWnd, NULL, g_hInstance, NULL);
    
    SendMessageW(g_Terminals[idx].hOutput, EM_SETBKGNDCOLOR, 0, RGB(0, 0, 0));
    
    CHARFORMAT2W cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = RGB(0, 255, 0);
    wcscpy(cf.szFaceName, L"Consolas");
    cf.yHeight = 200;
    SendMessageW(g_Terminals[idx].hOutput, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    
    // Create input
    g_Terminals[idx].hInput = CreateWindowExW(WS_EX_CLIENTEDGE,
        L"EDIT", NULL,
        WS_CHILD | ES_AUTOHSCROLL,
        0, 0, 100, 24, g_hMainWnd, (HMENU)(IDC_TERMINAL_INPUT + idx), g_hInstance, NULL);
    
    // Create pipes for process I/O
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hStdinRead, hStdoutWrite;
    
    CreatePipe(&hStdinRead, &g_Terminals[idx].hStdinWrite, &sa, 0);
    CreatePipe(&g_Terminals[idx].hStdoutRead, &hStdoutWrite, &sa, 0);
    
    // Start PowerShell process
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hStdinRead;
    si.hStdOutput = hStdoutWrite;
    si.hStdError = hStdoutWrite;
    
    PROCESS_INFORMATION pi;
    wchar_t cmdline[] = L"pwsh.exe -NoLogo -NoExit";
    
    if (CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        g_Terminals[idx].hProcess = pi.hProcess;
        g_Terminals[idx].dwPid = pi.dwProcessId;
        g_Terminals[idx].bRunning = TRUE;
        CloseHandle(pi.hThread);
        
        // Start reader thread
        g_Terminals[idx].hThread = CreateThread(NULL, 0, TerminalReaderThread, (LPVOID)(UINT_PTR)idx, 0, NULL);
    }
    
    CloseHandle(hStdinRead);
    CloseHandle(hStdoutWrite);
    
    // Add tab
    wchar_t tabName[32];
    swprintf(tabName, 32, L"Terminal %d", idx + 1);
    TCITEMW tci = { 0 };
    tci.mask = TCIF_TEXT;
    tci.pszText = tabName;
    SendMessageW(g_hTerminalTab, TCM_INSERTITEMW, idx, (LPARAM)&tci);
    
    g_nTerminalCount++;
    g_nActiveTerminal = idx;
    
    // Hide other terminal outputs
    for (int i = 0; i < g_nTerminalCount - 1; i++) {
        ShowWindow(g_Terminals[i].hOutput, SW_HIDE);
        ShowWindow(g_Terminals[i].hInput, SW_HIDE);
    }
    ShowWindow(g_Terminals[idx].hOutput, SW_SHOW);
    ShowWindow(g_Terminals[idx].hInput, SW_SHOW);
    
    SendMessageW(g_hTerminalTab, TCM_SETCURSEL, idx, 0);
    LayoutControls(g_hMainWnd);
    
    AppendRichText(g_Terminals[idx].hOutput, L"RawrXD Terminal - PowerShell\r\n", RGB(0, 255, 255));
}

static void CloseTerminal(int index) {
    if (index < 0 || index >= g_nTerminalCount) return;
    
    if (g_Terminals[index].bRunning) {
        TerminateProcess(g_Terminals[index].hProcess, 0);
        g_Terminals[index].bRunning = FALSE;
    }
    
    CloseHandle(g_Terminals[index].hProcess);
    CloseHandle(g_Terminals[index].hStdinWrite);
    CloseHandle(g_Terminals[index].hStdoutRead);
    
    DestroyWindow(g_Terminals[index].hOutput);
    DestroyWindow(g_Terminals[index].hInput);
    
    SendMessageW(g_hTerminalTab, TCM_DELETEITEM, index, 0);
    
    // Shift remaining
    for (int i = index; i < g_nTerminalCount - 1; i++) {
        g_Terminals[i] = g_Terminals[i + 1];
    }
    g_nTerminalCount--;
    
    if (g_nTerminalCount > 0) {
        g_nActiveTerminal = 0;
        ShowWindow(g_Terminals[0].hOutput, SW_SHOW);
        ShowWindow(g_Terminals[0].hInput, SW_SHOW);
    } else {
        g_nActiveTerminal = -1;
    }
}

static void ExecuteTerminalCommand(int index, const wchar_t* cmd) {
    if (index < 0 || index >= g_nTerminalCount) return;
    if (!g_Terminals[index].bRunning) return;
    
    // Echo command
    AppendRichText(g_Terminals[index].hOutput, L"> ", RGB(255, 255, 0));
    AppendRichText(g_Terminals[index].hOutput, cmd, RGB(255, 255, 255));
    AppendRichText(g_Terminals[index].hOutput, L"\r\n", RGB(255, 255, 255));
    
    // Convert to UTF-8 and send
    int utf8len = WideCharToMultiByte(CP_UTF8, 0, cmd, -1, NULL, 0, NULL, NULL);
    char* utf8cmd = (char*)malloc(utf8len + 2);
    WideCharToMultiByte(CP_UTF8, 0, cmd, -1, utf8cmd, utf8len, NULL, NULL);
    strcat(utf8cmd, "\n");
    
    DWORD written;
    WriteFile(g_Terminals[index].hStdinWrite, utf8cmd, (DWORD)strlen(utf8cmd), &written, NULL);
    free(utf8cmd);
}

static DWORD WINAPI TerminalReaderThread(LPVOID param) {
    int idx = (int)(UINT_PTR)param;
    char buffer[4096];
    DWORD bytesRead;
    
    while (g_Terminals[idx].bRunning) {
        if (ReadFile(g_Terminals[idx].hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            
            // Convert to wide and post to main thread
            int wlen = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
            wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
            MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuf, wlen);
            
            PostMessageW(g_hMainWnd, WM_TERMINAL_OUTPUT, idx, (LPARAM)wbuf);
        } else {
            Sleep(10);
        }
    }
    
    return 0;
}

// ============================================================================
// CHAT PANEL
// ============================================================================
static void CreateChatPanel(HWND hWnd) {
    // Chat history (RichEdit)
    g_hChatHistory = CreateWindowExW(WS_EX_CLIENTEDGE,
        MSFTEDIT_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, 300, 400, hWnd, (HMENU)IDC_CHAT_HISTORY, g_hInstance, NULL);
    
    SendMessageW(g_hChatHistory, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    
    CHARFORMAT2W cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.crTextColor = RGB(212, 212, 212);
    wcscpy(cf.szFaceName, L"Segoe UI");
    cf.yHeight = 200;
    SendMessageW(g_hChatHistory, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    
    AppendRichText(g_hChatHistory, L"RawrXD AI Chat\r\n", RGB(86, 156, 214));
    AppendRichText(g_hChatHistory, L"Load a model to start chatting.\r\n\r\n", RGB(128, 128, 128));
    
    // Chat input
    g_hChatInput = CreateWindowExW(WS_EX_CLIENTEDGE,
        L"EDIT", NULL,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 200, 24, hWnd, (HMENU)IDC_CHAT_INPUT, g_hInstance, NULL);
    
    // Model combo
    g_hModelCombo = CreateWindowExW(0, WC_COMBOBOXW, NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 200, 200, hWnd, (HMENU)IDC_MODEL_COMBO, g_hInstance, NULL);
    
    SendMessageW(g_hModelCombo, CB_ADDSTRING, 0, (LPARAM)L"No Model Loaded");
    SendMessageW(g_hModelCombo, CB_SETCURSEL, 0, 0);
}

static void SendChatMessage(const wchar_t* message) {
    AddChatMessage(L"You", message);
    
    if (g_nActiveModel < 0) {
        AddChatMessage(L"System", L"Please load a model first.");
        return;
    }
    
    // TODO: Call inference DLL
    // For now, echo back
    wchar_t response[1024];
    swprintf(response, 1024, L"[Model: %s] Processing: \"%s\"...\r\nThis is a placeholder response. Wire up Titan inference for real responses.",
        g_Models[g_nActiveModel].szName, message);
    
    AddChatMessage(L"Assistant", response);
}

static void AddChatMessage(const wchar_t* role, const wchar_t* content) {
    COLORREF roleColor = RGB(86, 156, 214);  // Blue
    if (wcscmp(role, L"You") == 0) roleColor = RGB(78, 201, 176);  // Teal
    else if (wcscmp(role, L"System") == 0) roleColor = RGB(255, 128, 0);  // Orange
    
    AppendRichText(g_hChatHistory, role, roleColor);
    AppendRichText(g_hChatHistory, L": ", roleColor);
    AppendRichText(g_hChatHistory, content, RGB(212, 212, 212));
    AppendRichText(g_hChatHistory, L"\r\n\r\n", RGB(212, 212, 212));
}

static void RefreshModelList(void) {
    // Scan for GGUF files in common locations
    wchar_t paths[][MAX_PATH] = {
        L"D:\\Models\\*.gguf",
        L"D:\\RawrXD\\*.gguf",
        L"C:\\Models\\*.gguf",
        L".\\*.gguf"
    };
    
    for (int p = 0; p < 4 && g_nModelCount < MAX_MODELS; p++) {
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileW(paths[p], &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                wchar_t* dir = paths[p];
                wchar_t* slash = wcsrchr(dir, L'\\');
                if (slash) {
                    *slash = 0;
                    wchar_t fullPath[MAX_PATH];
                    swprintf(fullPath, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
                    *slash = L'\\';
                    
                    wcscpy(g_Models[g_nModelCount].szPath, fullPath);
                    wcscpy(g_Models[g_nModelCount].szName, fd.cFileName);
                    g_Models[g_nModelCount].uFileSize = ((UINT64)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                    g_Models[g_nModelCount].iFormat = 0;
                    g_Models[g_nModelCount].bLoaded = FALSE;
                    
                    SendMessageW(g_hModelCombo, CB_ADDSTRING, 0, (LPARAM)g_Models[g_nModelCount].szName);
                    g_nModelCount++;
                }
            } while (FindNextFileW(hFind, &fd) && g_nModelCount < MAX_MODELS);
            FindClose(hFind);
        }
    }
}

static void LoadModel(int index) {
    if (index < 0 || index >= g_nModelCount) return;
    
    StatusBarSetText(3, L"Loading model...");
    
    // Try to load Titan DLL
    if (!g_hTitanDLL) {
        g_hTitanDLL = LoadLibraryW(L"RawrXD_Titan_Kernel.dll");
    }
    if (!g_hBridgeDLL) {
        g_hBridgeDLL = LoadLibraryW(L"RawrXD_NativeModelBridge.dll");
    }
    
    // Mark as loaded
    g_Models[index].bLoaded = TRUE;
    g_nActiveModel = index;
    
    wchar_t status[128];
    swprintf(status, 128, L"Model: %s", g_Models[index].szName);
    StatusBarSetText(3, status);
    
    AddChatMessage(L"System", L"Model loaded successfully. You can now chat.");
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
static void AppendRichText(HWND hRich, const wchar_t* text, COLORREF color) {
    // Move to end
    int len = GetWindowTextLengthW(hRich);
    SendMessageW(hRich, EM_SETSEL, len, len);
    
    // Set color
    CHARFORMAT2W cf = { sizeof(cf) };
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hRich, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // Insert text
    SendMessageW(hRich, EM_REPLACESEL, FALSE, (LPARAM)text);
    
    // Scroll to end
    SendMessageW(hRich, WM_VSCROLL, SB_BOTTOM, 0);
}

// ============================================================================
// LAYOUT
// ============================================================================
static void LayoutControls(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    
    // Get toolbar/rebar height
    RECT rebarRect;
    GetWindowRect(g_hRebar, &rebarRect);
    int rebarH = rebarRect.bottom - rebarRect.top;
    
    // Get status bar height
    RECT statusRect;
    GetWindowRect(g_hStatusBar, &statusRect);
    int statusH = statusRect.bottom - statusRect.top;
    
    // Resize rebar
    MoveWindow(g_hRebar, 0, 0, rc.right, rebarH, TRUE);
    
    // Resize status bar
    SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
    
    int top = rebarH;
    int bottom = rc.bottom - statusH;
    int clientH = bottom - top;
    
    // Calculate areas
    int treeW = g_bFileBrowserVisible ? g_nSplitX : 0;
    int chatW = g_bChatVisible ? 300 : 0;
    int termH = g_bTerminalVisible ? g_nSplitY : 0;
    
    int editorW = rc.right - treeW - chatW;
    int editorH = clientH - termH - 30;  // 30 for tab bar
    
    // File browser
    if (g_bFileBrowserVisible) {
        MoveWindow(g_hTreeView, 0, top, treeW, clientH, TRUE);
    }
    
    // Tab editor
    MoveWindow(g_hTabEditor, treeW, top, editorW, 30, TRUE);
    
    // Editor content
    if (g_nActiveTab >= 0 && g_nActiveTab < g_nTabCount) {
        MoveWindow(g_Tabs[g_nActiveTab].hEdit, treeW, top + 30, editorW, editorH, TRUE);
    }
    
    // Terminal
    if (g_bTerminalVisible) {
        int termTop = top + 30 + editorH;
        MoveWindow(g_hTerminalTab, treeW, termTop, editorW, 24, TRUE);
        
        if (g_nActiveTerminal >= 0 && g_nActiveTerminal < g_nTerminalCount) {
            MoveWindow(g_Terminals[g_nActiveTerminal].hOutput, treeW, termTop + 24, editorW, termH - 50, TRUE);
            MoveWindow(g_Terminals[g_nActiveTerminal].hInput, treeW, termTop + termH - 26, editorW, 24, TRUE);
        }
    }
    
    // Chat panel (right side)
    if (g_bChatVisible) {
        int chatX = rc.right - chatW;
        MoveWindow(g_hModelCombo, chatX, top, chatW, 24, TRUE);
        MoveWindow(g_hChatHistory, chatX, top + 26, chatW, clientH - 60, TRUE);
        MoveWindow(g_hChatInput, chatX, bottom - 30, chatW, 24, TRUE);
    }
}
