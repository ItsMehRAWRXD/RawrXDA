#include "ide_window.h"
#include "ide_constants.h"
#include "universal_generator_service.h"
#include "engine/react_ide_generator.h"
#include <richedit.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shldisp.h>
#include <sstream>
#include <fstream>
#include <regex>
#include <algorithm>
#include <set>
#include <cstdio>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")

// Window class name
static const wchar_t* IDE_WINDOW_CLASS = L"RawrXDIDE";

// Additional menu IDs specific to this file (not in ide_constants.h)
static const int IDM_RUN_SCRIPT = 3001;
static const int IDM_VIEW_BROWSER = 3002;
static const int IDM_TOOLS_GENERATE_PROJECT = 3100;
static const int IDM_TOOLS_GENERATE_GUIDE = 3101;
static const int IDM_TOOLS_AGENT_MODE = 3102;
static const int IDM_TOOLS_ENGINE_MANAGER = 3103;
static const int IDM_TOOLS_MEMORY_VIEWER = 3104;
static const int IDM_TOOLS_RE_TOOLS = 3105;
static const int IDM_TOOLS_HOTPATCH = 3106;
static const int IDM_TOOLS_CODE_AUDIT = 3107;
static const int IDM_TOOLS_SECURITY_CHECK = 3108;
static const int IDM_TOOLS_PERFORMANCE_ANALYZE = 3109;
static const int IDM_TOOLS_BUG_DETECTOR = 3110;
static const int IDM_TOOLS_AI_CODE_REVIEW = 3111;
static const int IDM_TOOLS_PLAN_TASK = 3112;
static const int IDM_TOOLS_SUMMARIZE_CODE = 3113;
static const int IDM_TOOLS_REAL_TIME_ANALYSIS = 3112;
static const int IDM_TOOLS_SMART_AUTOCOMPLETE = 3113;
static const int IDM_TOOLS_AUTO_REFACTOR = 3114;
static const int IDM_TOOLS_GENERATE_TESTS = 3115;
static const int IDM_TOOLS_PERFORMANCE_PROFILER = 3116;
static const int IDM_TOOLS_IDE_HEALTH = 3117;

// Control IDs specific to this file (not in ide_constants.h)
static const int ID_EDITOR = 4001;
static const int ID_FILETREE = 4002;
static const int ID_TERMINAL = 4003;
static const int ID_OUTPUT = 4004;
static const int ID_TABCONTROL = 4005;
static const int ID_WEBBROWSER = 4006;

static std::string WideToUTF8(const std::wstring& w);
static std::wstring UTF8ToWide(const std::string& s);

static std::wstring TrimWhitespace(const std::wstring& text);
static void AppendOutputText(HWND hOutput, const std::wstring& text);
static std::wstring PromptForText(HWND owner, const std::wstring& title, const std::wstring& label, const std::wstring& defaultValue);

static WNDPROC g_terminalProc = nullptr;
static LRESULT CALLBACK TerminalProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

IDEWindow::IDEWindow()
    : hwnd_(nullptr)
    , hEditor_(nullptr)
    , hFileTree_(nullptr)
    , hTerminal_(nullptr)
    , hOutput_(nullptr)
    , hStatusBar_(nullptr)
    , hToolBar_(nullptr)
    , hTabControl_(nullptr)
    , hWebBrowser_(nullptr)
    , hAutocompleteList_(nullptr)
    , hParameterHint_(nullptr)
    , hFindDialog_(nullptr)
    , hReplaceDialog_(nullptr)
    , hCommandPalette_(nullptr)
    , hMarketplaceSearch_(nullptr)
    , hMarketplaceList_(nullptr)
    , pWebBrowser_(nullptr)
    , hInstance_(nullptr)
    , originalEditorProc_(nullptr)
    , isModified_(false)
    , nextTabId_(1)
    , activeTabId_(-1)
    , selectedAutocompleteIndex_(0)
    , autocompleteVisible_(false)
    , lastSearchPos_(-1)
    , lastSearchCaseSensitive_(false)
    , lastSearchRegex_(false)
    , keywordColor_(RGB(86, 156, 214))      // VS Code blue
    , cmdletColor_(RGB(78, 201, 176))       // Teal
    , stringColor_(RGB(206, 145, 120))      // Orange
    , commentColor_(RGB(106, 153, 85))      // Green
    , variableColor_(RGB(156, 220, 254))    // Light blue
    , backgroundColor_(RGB(30, 30, 30))     // Dark gray
    , textColor_(RGB(212, 212, 212))        // Light gray
    , sessionPath_(L"RawrXDSettings.json")
{
    CoInitialize(nullptr);
    PopulatePowerShellCmdlets();
}

IDEWindow::~IDEWindow()
{
    if (pWebBrowser_) {
        pWebBrowser_->Release();
    }
    CoUninitialize();
    Shutdown();
}

bool IDEWindow::Initialize(HINSTANCE hInstance)
{
    hInstance_ = hInstance;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Load RichEdit library
    LoadLibraryW(L"Msftedit.dll");
    
    CreateMainWindow(hInstance);
    
    if (!hwnd_) {
        return false;
    }
    
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    
    return true;
}

void IDEWindow::CreateMainWindow(HINSTANCE hInstance)
{
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = IDE_WINDOW_CLASS;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    RegisterClassExW(&wc);
    
    // Create main window
    hwnd_ = CreateWindowExW(
        0,
        IDE_WINDOW_CLASS,
        L"RawrXD PowerShell IDE - C++ Edition",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1400, 900,
        nullptr,
        nullptr,
        hInstance,
        this
    );
    
    if (hwnd_) {
        CreateMenuBar();
        CreateToolBar();
        CreateStatusBar();
        CreateEditorControl();
        CreateFileExplorer();
        CreateTerminalPanel();
        CreateOutputPanel();
        CreateTabControl();
        LoadSession();
    }
}

void IDEWindow::CreateMenuBar()
{
    HMENU hMenuBar = CreateMenu();
    
    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open File...\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN_FOLDER, L"Open &Folder...");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");
    
    // Edit menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CUT, L"Cu&t\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY, L"&Copy\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE, L"&Paste\tCtrl+V");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    
    // Run menu
    HMENU hRunMenu = CreatePopupMenu();
    AppendMenuW(hRunMenu, MF_STRING, IDM_RUN_SCRIPT, L"&Run Script\tF5");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hRunMenu, L"&Run");

    // Tools menu
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_GENERATE_PROJECT, L"&Generate Project...\tCtrl+G");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_GENERATE_GUIDE, L"Generate &Guide...\tCtrl+Shift+G");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    
    // AI-Powered Code Analysis
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_CODE_AUDIT, L"&Code Audit...\tCtrl+Alt+A");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_SECURITY_CHECK, L"&Security Check...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PERFORMANCE_ANALYZE, L"&Performance Analysis...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_BUG_DETECTOR, L"&Bug Detector...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_AI_CODE_REVIEW, L"AI Code &Review...");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    
    // Real-time Intelligence
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_REAL_TIME_ANALYSIS, L"&Real-time Analysis");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_SMART_AUTOCOMPLETE, L"S&mart Autocomplete");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_AUTO_REFACTOR, L"Auto &Refactor...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_GENERATE_TESTS, L"Generate &Tests...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PLAN_TASK, L"&Plan Task...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_SUMMARIZE_CODE, L"&Summarize Code...");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    
    // System Tools
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_AGENT_MODE, L"&Agent Mode");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_ENGINE_MANAGER, L"&Engine Manager");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_MEMORY_VIEWER, L"&Memory Viewer");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PERFORMANCE_PROFILER, L"Performance &Profiler");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_RE_TOOLS, L"Reverse Engineering &Tools");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_IDE_HEALTH, L"IDE &Health Report");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_HOTPATCH, L"Apply &Hotpatch...\tCtrl+H");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hToolsMenu, L"&Tools");
    
    SetMenu(hwnd_, hMenuBar);
}

void IDEWindow::CreateToolBar()
{
    hToolBar_ = CreateWindowExW(
        0,
        TOOLBARCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
        0, 0, 0, 0,
        hwnd_,
        nullptr,
        hInstance_,
        nullptr
    );
    
    SendMessageW(hToolBar_, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    
    TBBUTTON buttons[] = {
        {0, IDM_FILE_NEW, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"New"},
        {1, IDM_FILE_OPEN, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Open"},
        {2, IDM_FILE_SAVE, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Save"},
        {0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0},
        {3, IDM_RUN_SCRIPT, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)L"Run"},
    };
    
    SendMessageW(hToolBar_, TB_ADDBUTTONSW, ARRAYSIZE(buttons), (LPARAM)buttons);
    SendMessageW(hToolBar_, TB_AUTOSIZE, 0, 0);
}

void IDEWindow::CreateStatusBar()
{
    hStatusBar_ = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd_,
        nullptr,
        hInstance_,
        nullptr
    );
    
    int parts[] = {200, 400, -1};
    SendMessageW(hStatusBar_, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessageW(hStatusBar_, SB_SETTEXTW, 0, (LPARAM)L"Ready");
    SendMessageW(hStatusBar_, SB_SETTEXTW, 1, (LPARAM)L"Line 1, Col 1");
    SendMessageW(hStatusBar_, SB_SETTEXTW, 2, (LPARAM)L"PowerShell");
}

void IDEWindow::CreateEditorControl()
{
    RECT rcClient;
    GetClientRect(hwnd_, &rcClient);
    
    // Calculate positions accounting for toolbar and statusbar
    RECT rcToolBar;
    GetWindowRect(hToolBar_, &rcToolBar);
    int toolbarHeight = rcToolBar.bottom - rcToolBar.top;
    
    RECT rcStatus;
    GetWindowRect(hStatusBar_, &rcStatus);
    int statusHeight = rcStatus.bottom - rcStatus.top;
    
    // Create rich edit control
    hEditor_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"RICHEDIT50W",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
        200, toolbarHeight,
        rcClient.right - 400,
        rcClient.bottom - toolbarHeight - statusHeight - 200,
        hwnd_,
        (HMENU)ID_EDITOR,
        hInstance_,
        nullptr
    );
    
    // Set editor font
    HFONT hFont = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
    SendMessageW(hEditor_, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Set background and text colors
    SendMessageW(hEditor_, EM_SETBKGNDCOLOR, 0, backgroundColor_);
    
    // Subclass editor for custom handling
    // Store 'this' pointer in window user data for access in static callback
    SetWindowLongPtrW(hEditor_, GWLP_USERDATA, (LONG_PTR)this);
    originalEditorProc_ = (WNDPROC)SetWindowLongPtrW(hEditor_, GWLP_WNDPROC, (LONG_PTR)&IDEWindow::EditorProc);
    
    // Set initial text
    std::wstring initialText =
        L"# RawrXD PowerShell IDE - C++ Native Edition\n"
        L"# Migration from RawrXD.ps1 to high-performance C++\n\n"
        L"Write-Host \"Welcome to RawrXD IDE!\"\n"
        L"$version = \"1.0\"\n"
        L"Get-Process | Where-Object {$_.CPU -gt 10}\n";
    
    SetWindowTextW(hEditor_, initialText.c_str());
}

void IDEWindow::CreateFileExplorer()
{
    RECT rcClient;
    GetClientRect(hwnd_, &rcClient);
    
    RECT rcToolBar;
    GetWindowRect(hToolBar_, &rcToolBar);
    int toolbarHeight = rcToolBar.bottom - rcToolBar.top;
    
    RECT rcStatus;
    GetWindowRect(hStatusBar_, &rcStatus);
    int statusHeight = rcStatus.bottom - rcStatus.top;
    
    hFileTree_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWW,
        nullptr,
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT,
        0, toolbarHeight,
        200,
        rcClient.bottom - toolbarHeight - statusHeight,
        hwnd_,
        (HMENU)ID_FILETREE,
        hInstance_,
        nullptr
    );
    
    // Add root items
    TVINSERTSTRUCTW tvins = {};
    tvins.hParent = TVI_ROOT;
    tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT;
    tvins.item.pszText = (LPWSTR)L"Workspace";
    TreeView_InsertItem(hFileTree_, &tvins);
}

void IDEWindow::CreateTerminalPanel()
{
    RECT rcClient;
    GetClientRect(hwnd_, &rcClient);
    
    RECT rcStatus;
    GetWindowRect(hStatusBar_, &rcStatus);
    int statusHeight = rcStatus.bottom - rcStatus.top;
    
    hTerminal_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        200, rcClient.bottom - statusHeight - 200,
        rcClient.right - 400,
        200,
        hwnd_,
        (HMENU)ID_TERMINAL,
        hInstance_,
        nullptr
    );
    
    HFONT hFont = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
    SendMessageW(hTerminal_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SetWindowLongPtrW(hTerminal_, GWLP_USERDATA, (LONG_PTR)this);
    g_terminalProc = (WNDPROC)SetWindowLongPtrW(hTerminal_, GWLP_WNDPROC, (LONG_PTR)TerminalProc);
}

void IDEWindow::CreateOutputPanel()
{
    RECT rcClient;
    GetClientRect(hwnd_, &rcClient);
    
    RECT rcToolBar;
    GetWindowRect(hToolBar_, &rcToolBar);
    int toolbarHeight = rcToolBar.bottom - rcToolBar.top;
    
    RECT rcStatus;
    GetWindowRect(hStatusBar_, &rcStatus);
    int statusHeight = rcStatus.bottom - rcStatus.top;
    
    hOutput_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"Output Panel\r\n",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        rcClient.right - 200, toolbarHeight,
        200,
        rcClient.bottom - toolbarHeight - statusHeight,
        hwnd_,
        (HMENU)ID_OUTPUT,
        hInstance_,
        nullptr
    );
    
    HFONT hFont = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
    SendMessageW(hOutput_, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void IDEWindow::CreateTabControl()
{
    RECT rcClient;
    GetClientRect(hwnd_, &rcClient);
    
    RECT rcToolBar;
    GetWindowRect(hToolBar_, &rcToolBar);
    int toolbarHeight = rcToolBar.bottom - rcToolBar.top;
    
    hTabControl_ = CreateWindowExW(
        0,
        WC_TABCONTROLW,
        nullptr,
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_TOOLTIPS | TCS_FOCUSNEVER,
        200, toolbarHeight,
        rcClient.right - 400, 28,
        hwnd_,
        (HMENU)ID_TABCONTROL,
        hInstance_,
        nullptr
    );
    
    // Set tab control font
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(hTabControl_, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Tabs will be created by LoadSession()
}

void IDEWindow::CreateWebBrowser()
{
    // Web browser functionality removed as per pure C++ requirement.
    // Generated UI components are now viewed as source code or handled via external browser if needed.
    hWebBrowser_ = nullptr;
}

void IDEWindow::PopulateFileTree(const std::wstring& rootPath)
{
    if (rootPath.empty()) return;
    
    currentFolderPath_ = rootPath;
    TreeView_DeleteAllItems(hFileTree_);
    
    // Add root folder
    TVINSERTSTRUCTW tvins = {};
    tvins.hParent = TVI_ROOT;
    tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvins.item.pszText = const_cast<LPWSTR>(rootPath.c_str());
    tvins.item.lParam = 0; // 0 for folder
    HTREEITEM hRoot = TreeView_InsertItem(hFileTree_, &tvins);
    
    // Enumerate files and folders
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = rootPath + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            
            tvins.hParent = hRoot;
            tvins.item.pszText = findData.cFileName;
            tvins.item.lParam = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1; // 0 for folder, 1 for file
            TreeView_InsertItem(hFileTree_, &tvins);
            
        } while (FindNextFileW(hFind, &findData));
        FindClose(hFind);
    }
    
    TreeView_Expand(hFileTree_, hRoot, TVE_EXPAND);
}

LRESULT CALLBACK IDEWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    IDEWindow* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (IDEWindow*)pCreate->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (IDEWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    
    if (pThis) {
        switch (uMsg) {
            case WM_TIMER:
                if (wParam == 1) {
                    pThis->HideParameterHint();
                    KillTimer(hwnd, 1);
                }
                return 0;
                
            case WM_NOTIFY:
                {
                    NMHDR* pNmhdr = (NMHDR*)lParam;
                    if (pNmhdr->idFrom == ID_TABCONTROL && pNmhdr->code == TCN_SELCHANGE) {
                        int tabIndex = TabCtrl_GetCurSel(pThis->hTabControl_);
                        pThis->OnSwitchTab(tabIndex);
                    } else if (pNmhdr->idFrom == ID_FILETREE && pNmhdr->code == NM_DBLCLK) {
                        // File tree double-click - open file
                        TVITEMW item = {};
                        item.mask = TVIF_PARAM | TVIF_TEXT;
                        wchar_t buffer[MAX_PATH];
                        item.pszText = buffer;
                        item.cchTextMax = MAX_PATH;
                        item.hItem = TreeView_GetSelection(pThis->hFileTree_);
                        
                        if (TreeView_GetItem(pThis->hFileTree_, &item)) {
                            if (item.lParam == 1) { // 1 = file (not folder)
                                // Build full path
                                std::wstring fileName = item.pszText;
                                std::wstring fullPath = pThis->currentFolderPath_ + L"\\" + fileName;
                                
                                // Load file
                                pThis->LoadFileIntoEditor(fullPath);
                                pThis->CreateNewTab(fileName, fullPath);
                            }
                        }
                        return 0;
                    }
                }
                break;
                
            case WM_KEYDOWN:
                {
                    bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                    bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                    
                    if (ctrlPressed) {
                        switch (wParam) {
                            case VK_TAB:
                                {
                                    // Ctrl+Tab: Switch to next tab
                                    int currentIndex = TabCtrl_GetCurSel(pThis->hTabControl_);
                                    int tabCount = TabCtrl_GetItemCount(pThis->hTabControl_);
                                    int nextIndex = (currentIndex + 1) % tabCount;
                                    TabCtrl_SetCurSel(pThis->hTabControl_, nextIndex);
                                    pThis->OnSwitchTab(nextIndex);
                                    return 0;
                                }
                            case 'W':
                                {
                                    // Ctrl+W: Close current tab
                                    int currentIndex = TabCtrl_GetCurSel(pThis->hTabControl_);
                                    pThis->OnCloseTab(currentIndex);
                                    return 0;
                                }
                            case 'S':
                                {
                                    // Ctrl+S Save
                                    pThis->OnSaveFile();
                                    return 0;
                                }
                        }
                        if (shiftPressed && wParam == 'P') {
                            pThis->ToggleCommandPalette();
                            return 0;
                        }
                    }
                }
                break;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case IDM_FILE_NEW:
                        pThis->OnNewFile();
                        return 0;
                    case IDM_FILE_OPEN:
                        pThis->OnOpenFile();
                        return 0;
                    case IDM_FILE_SAVE:
                        pThis->OnSaveFile();
                        return 0;
                    case IDM_FILE_EXIT:
                        PostQuitMessage(0);
                        return 0;
                    case IDM_EDIT_CUT:
                        SendMessageW(pThis->hEditor_, WM_CUT, 0, 0);
                        return 0;
                    case IDM_EDIT_COPY:
                        SendMessageW(pThis->hEditor_, WM_COPY, 0, 0);
                        return 0;
                    case IDM_EDIT_PASTE:
                        SendMessageW(pThis->hEditor_, WM_PASTE, 0, 0);
                        return 0;
                    case IDM_RUN_SCRIPT:
                        pThis->OnRunScript();
                        return 0;
                    case IDM_TOOLS_GENERATE_PROJECT:
                        {
                            std::wstring params = PromptForText(pThis->hwnd_, L"Generate Project", L"Enter generator params (JSON or key=value):", L"{\"name\":\"MyApp\",\"type\":\"web\",\"path\":\".\"}");
                            if (!params.empty()) {
                                std::string result = GenerateAnything("generate_project", WideToUTF8(params));
                                AppendOutputText(pThis->hOutput_, L"[Generator] " + UTF8ToWide(result) + L"\r\n");
                            }
                        }
                        return 0;
                    case IDM_TOOLS_GENERATE_GUIDE:
                        {
                            std::wstring topic = PromptForText(pThis->hwnd_, L"Generate Guide", L"Enter guide topic:", L"chocolate chip cookies");
                            if (!topic.empty()) {
                                std::string result = GenerateAnything("generate_guide", WideToUTF8(topic));
                                AppendOutputText(pThis->hOutput_, L"[Guide] " + UTF8ToWide(result) + L"\r\n");
                            }
                        }
                        return 0;
                    case IDM_TOOLS_AGENT_MODE:
                        {
                            // Generate Agent Mode UI and load as code
                            std::string htmlPath = GenerateAnything("generate_component", "agent_mode");
                            if (!htmlPath.empty() && htmlPath.find("Error") == std::string::npos) {
                                pThis->CreateNewTab(L"AgentMode.js", UTF8ToWide(htmlPath));
                            } else {
                                MessageBoxW(pThis->hwnd_, L"Failed to initialize Agent Mode.", L"Error", MB_ICONERROR);
                            }
                            
                            // Native override
                            std::wstring query = PromptForText(pThis->hwnd_, L"Agent Mode", L"Enter task:", L"Optimize memory");
                            if (!query.empty()) {
                                std::string json = "{\"prompt\":\"" + WideToUTF8(query) + "\"}";
                                std::string result = GenerateAnything("agent_query", json);
                                AppendOutputText(pThis->hOutput_, L"[Agent] " + UTF8ToWide(result) + L"\r\n");
                            }
                        }
                        return 0;
                    case IDM_TOOLS_ENGINE_MANAGER:
                        {
                             std::string htmlPath = GenerateAnything("generate_component", "engine_manager");
                             if (!htmlPath.empty() && htmlPath.find("Error") == std::string::npos) {
                                pThis->CreateNewTab(L"EngineManager.js", UTF8ToWide(htmlPath));
                             } else {
                                MessageBoxW(pThis->hwnd_, L"Failed to generate Engine Manager.", L"Error", MB_ICONERROR);
                             }
                        }
                        return 0;
                    case IDM_TOOLS_MEMORY_VIEWER:
                        {
                            std::string stats = GenerateAnything("get_memory_stats", "");
                            std::wstring wStats = UTF8ToWide(stats);
                            
                            // Create temp file for stats
                            wchar_t tempPath[MAX_PATH];
                            GetTempPathW(MAX_PATH, tempPath);
                            std::wstring statFile = std::wstring(tempPath) + L"memory_stats.txt";
                            std::string statFileUTF8 = WideToUTF8(statFile);
                            std::ofstream file(statFileUTF8, std::ios::binary);
                            if (file.is_open()) {
                                file.write(stats.data(), stats.size());
                                file.close();
                                pThis->CreateNewTab(L"Memory.stats", statFile);
                            } else {
                                // Fallback if file creation fails
                                MessageBoxW(pThis->hwnd_, wStats.c_str(), L"Memory Core Stats", MB_OK | MB_ICONINFORMATION);
                            }
                        }
                        return 0;
                    case IDM_TOOLS_RE_TOOLS:
                        {
                            std::string htmlPath = GenerateAnything("generate_component", "re_tools");
                             if (!htmlPath.empty() && htmlPath.find("Error") == std::string::npos) {
                                pThis->CreateNewTab(L"RETools.js", UTF8ToWide(htmlPath));
                                pThis->LoadFileIntoEditor(UTF8ToWide(htmlPath));
                             }
                        }
                        return 0;
                    case IDM_TOOLS_HOTPATCH:
                        {
                            std::wstring target = PromptForText(pThis->hwnd_, L"Hotpatch", L"Enter target (e.g., 0x1234):", L"0x00000000");
                            std::wstring bytes = PromptForText(pThis->hwnd_, L"Hotpatch", L"Enter bytes (hex):", L"90 90");
                            if (!target.empty() && !bytes.empty()) {
                                std::string json = "{\"target\":\"" + WideToUTF8(target) + "\", \"bytes\":\"" + WideToUTF8(bytes) + "\"}";
                                std::string result = GenerateAnything("apply_hotpatch", json);
                                AppendOutputText(pThis->hOutput_, L"[Hotpatch] " + UTF8ToWide(result) + L"\r\n");
                            }
                        }
                        return 0;
                    case IDM_TOOLS_CODE_AUDIT:
                        {
                            // Get current editor content
                            int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                            std::wstring wCode(textLen, L'\0');
                            GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                            wCode.pop_back(); // Remove null terminator
                            
                            std::string code = WideToUTF8(wCode);
                            std::string audit = GenerateAnything("code_audit", code);
                            
                            AppendOutputText(pThis->hOutput_, L"[Code Audit]\r\n" + UTF8ToWide(audit) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_SECURITY_CHECK:
                        {
                            int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                            std::wstring wCode(textLen, L'\0');
                            GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                            wCode.pop_back();
                            
                            std::string code = WideToUTF8(wCode);
                            std::string security = GenerateAnything("security_check", code);
                            
                            AppendOutputText(pThis->hOutput_, L"[Security Check]\r\n" + UTF8ToWide(security) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_BUG_DETECTOR:
                        {
                            int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                            std::wstring wCode(textLen, L'\0');
                            GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                            wCode.pop_back();
                            std::string code = WideToUTF8(wCode);
                            std::string params = "{\"code\":\"" + code + "\"}";
                            std::string bugs = GenerateAnything("bug_detector", params);
                            AppendOutputText(pThis->hOutput_, L"[Bug Detector]\r\n" + UTF8ToWide(bugs) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_PERFORMANCE_ANALYZE:
                        {
                            int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                            std::wstring wCode(textLen, L'\0');
                            GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                            wCode.pop_back();
                            
                            std::string code = WideToUTF8(wCode);
                            std::string perf = GenerateAnything("performance_check", code);
                            
                            AppendOutputText(pThis->hOutput_, L"[Performance Analysis]\r\n" + UTF8ToWide(perf) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_AI_CODE_REVIEW:
                        {
                            int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                            std::wstring wCode(textLen, L'\0');
                            GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                            wCode.pop_back();
                            
                            std::string code = WideToUTF8(wCode);
                            std::string review = GenerateAnything("ai_code_review", code);
                            
                            AppendOutputText(pThis->hOutput_, L"[AI Code Review]\r\n" + UTF8ToWide(review) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_REAL_TIME_ANALYSIS:
                        {
                            // Toggle real-time analysis mode
                            std::string result = GenerateAnything("real_time_analysis", "toggle");
                            AppendOutputText(pThis->hOutput_, L"[Real-time Analysis] " + UTF8ToWide(result) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_SMART_AUTOCOMPLETE:
                        {
                            // Toggle smart autocomplete
                            std::string result = GenerateAnything("smart_autocomplete", "toggle");
                            AppendOutputText(pThis->hOutput_, L"[Smart Autocomplete] " + UTF8ToWide(result) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_AUTO_REFACTOR:
                        {
                            std::wstring refactorType = PromptForText(pThis->hwnd_, L"Auto Refactor", L"Enter refactor type (extract_method, rename_variable, etc.):", L"extract_method");
                            if (!refactorType.empty()) {
                                int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                                std::wstring wCode(textLen, L'\0');
                                GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                                wCode.pop_back();
                                
                                std::string code = WideToUTF8(wCode);
                                std::string json = "{\"type\":\"" + WideToUTF8(refactorType) + "\", \"code\":\"" + code + "\"}";
                                std::string result = GenerateAnything("auto_refactor", json);
                                
                                AppendOutputText(pThis->hOutput_, L"[Auto Refactor]\r\n" + UTF8ToWide(result) + L"\r\n");
                            }
                        }
                        return 0;
                    case IDM_TOOLS_GENERATE_TESTS:
                        {
                            std::wstring testType = PromptForText(pThis->hwnd_, L"Generate Tests", L"Enter test type (unit, integration, e2e):", L"unit");
                            if (!testType.empty()) {
                                int textLen = GetWindowTextLengthW(pThis->hEditor_) + 1;
                                std::wstring wCode(textLen, L'\0');
                                GetWindowTextW(pThis->hEditor_, &wCode[0], textLen);
                                wCode.pop_back();
                                
                                std::string code = WideToUTF8(wCode);
                                std::string json = "{\"type\":\"" + WideToUTF8(testType) + "\", \"code\":\"" + code + "\"}";
                                std::string result = GenerateAnything("generate_tests", json);
                                
                                AppendOutputText(pThis->hOutput_, L"[Test Generation]\r\n" + UTF8ToWide(result) + L"\r\n");
                            }
                        }
                        return 0;
                    case IDM_TOOLS_PERFORMANCE_PROFILER:
                        {
                            // Launch performance profiler
                            std::string result = GenerateAnything("performance_profiler", "start");
                            AppendOutputText(pThis->hOutput_, L"[Performance Profiler] " + UTF8ToWide(result) + L"\r\n");
                        }
                        return 0;
                    case IDM_TOOLS_IDE_HEALTH:
                        {
                            std::string health = GenerateAnything("ide_health", "");
                            AppendOutputText(pThis->hOutput_, L"[IDE Health]\r\n" + UTF8ToWide(health) + L"\r\n");
                        }
                        return 0;
                        if (pThis->hMarketplaceSearch_) {
                            wchar_t searchText[256] = {0};
                            GetWindowTextW(pThis->hMarketplaceSearch_, searchText, 256);
                            pThis->SearchMarketplace(searchText);
                        }
                        return 0;
                    case 5012: // Extension list selection
                        if (HIWORD(wParam) == LBN_SELCHANGE && pThis->hMarketplaceList_) {
                            int index = SendMessageW(pThis->hMarketplaceList_, LB_GETCURSEL, 0, 0);
                            if (index >= 0 && index < (int)pThis->marketplaceExtensions_.size()) {
                                pThis->ShowExtensionDetails(pThis->marketplaceExtensions_[index]);
                            }
                        }
                        return 0;
                    case 5014: // Install/Uninstall button
                        if (pThis->hMarketplaceList_) {
                            int index = SendMessageW(pThis->hMarketplaceList_, LB_GETCURSEL, 0, 0);
                            if (index >= 0 && index < (int)pThis->marketplaceExtensions_.size()) {
                                const auto& ext = pThis->marketplaceExtensions_[index];
                                if (ext.installed) {
                                    pThis->UninstallExtension(ext);
                                } else {
                                    pThis->InstallExtension(ext);
                                }
                            }
                        }
                        return 0;
                    case 5015: // Close marketplace button
                        pThis->HideMarketplace();
                        return 0;
                    default:
                        if (pThis->hCommandPalette_ && (HWND)lParam == pThis->hCommandPalette_) {
                            WORD notif = HIWORD(wParam);
                            if (notif == LBN_DBLCLK || notif == LBN_SELCHANGE) {
                                pThis->ExecutePaletteSelection();
                            }
                        }
                        break;
                }
                break;
                
            case WM_SIZE:
                {
                    RECT rcClient;
                    GetClientRect(hwnd, &rcClient);
                    
                    SendMessageW(pThis->hToolBar_, WM_SIZE, 0, 0);
                    SendMessageW(pThis->hStatusBar_, WM_SIZE, 0, 0);
                    
                    RECT rcToolBar;
                    GetWindowRect(pThis->hToolBar_, &rcToolBar);
                    int toolbarHeight = rcToolBar.bottom - rcToolBar.top;
                    
                    RECT rcStatus;
                    GetWindowRect(pThis->hStatusBar_, &rcStatus);
                    int statusHeight = rcStatus.bottom - rcStatus.top;
                    
                    const int TAB_HEIGHT = 28;
                    
                    // Resize controls with tab control
                    MoveWindow(pThis->hFileTree_, 0, toolbarHeight, 200, 
                              rcClient.bottom - toolbarHeight - statusHeight, TRUE);
                    MoveWindow(pThis->hTabControl_, 200, toolbarHeight,
                              rcClient.right - 400, TAB_HEIGHT, TRUE);
                    MoveWindow(pThis->hEditor_, 200, toolbarHeight + TAB_HEIGHT, 
                              rcClient.right - 400, 
                              rcClient.bottom - toolbarHeight - TAB_HEIGHT - statusHeight - 200, TRUE);
                    MoveWindow(pThis->hTerminal_, 200, 
                              rcClient.bottom - statusHeight - 200, 
                              rcClient.right - 400, 200, TRUE);
                    MoveWindow(pThis->hOutput_, rcClient.right - 200, toolbarHeight, 
                              200, rcClient.bottom - toolbarHeight - statusHeight, TRUE);
                }
                return 0;
                
            case WM_DESTROY:
                if (pThis) pThis->SaveSession();
                PostQuitMessage(0);
                return 0;
        }
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK IDEWindow::EditorProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    IDEWindow* pThis = (IDEWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    
    if (pThis) {
        switch (uMsg) {
            case WM_CHAR:
                {
                    pThis->isModified_ = true;
                    pThis->UpdateStatusBar();
                    
                    // Trigger autocomplete on certain characters
                    wchar_t ch = (wchar_t)wParam;
                    if (iswalpha(ch) || ch == L'-' || ch == L'$') {
                        // Allow the character to be inserted first
                        LRESULT result = CallWindowProcW(pThis->originalEditorProc_, hwnd, uMsg, wParam, lParam);
                        
                        // Then show autocomplete
                        std::wstring word = pThis->GetCurrentWord();
                        if (word.length() >= 2) {  // Show after 2 characters
                            pThis->ShowAutocompleteList(word);
                        }
                        
                        // Apply syntax highlighting on newline or space
                        if (ch == L'\r' || ch == L'\n' || ch == L' ') {
                            pThis->ApplySyntaxHighlighting();
                        }
                        
                        return result;
                    }
                    else if (ch == L' ' || ch == L'(') {
                        // Show parameter hint for cmdlets
                        std::wstring word = pThis->GetCurrentWord();
                        if (!word.empty() && word.find(L'-') != std::wstring::npos) {
                            pThis->ShowParameterHint(word);
                        }
                        
                        // Apply syntax highlighting
                        LRESULT result = CallWindowProcW(pThis->originalEditorProc_, hwnd, uMsg, wParam, lParam);
                        pThis->ApplySyntaxHighlighting();
                        return result;
                    }
                }
                break;
                
            case WM_KEYDOWN:
                {
                    if (pThis->autocompleteVisible_) {
                        switch (wParam) {
                            case VK_DOWN:
                                pThis->SelectAutocompleteItem(pThis->selectedAutocompleteIndex_ + 1);
                                return 0;
                            case VK_UP:
                                pThis->SelectAutocompleteItem(pThis->selectedAutocompleteIndex_ - 1);
                                return 0;
                            case VK_RETURN:
                            case VK_TAB:
                                pThis->InsertAutocompleteSelection();
                                return 0;
                            case VK_ESCAPE:
                                pThis->HideAutocompleteList();
                                return 0;
                        }
                    }
                    
                    // Trigger syntax highlighting on paste (Ctrl+V)
                    if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'V') {
                        LRESULT result = CallWindowProcW(pThis->originalEditorProc_, hwnd, uMsg, wParam, lParam);
                        pThis->ApplySyntaxHighlighting();
                        return result;
                    }
                }
                break;
        }
        return CallWindowProcW(pThis->originalEditorProc_, hwnd, uMsg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void IDEWindow::OnNewFile()
{
    CreateNewTab(L"Untitled", L"");
    SendMessageW(hStatusBar_, SB_SETTEXTW, 0, (LPARAM)L"New file created");
}

void IDEWindow::OnOpenFile()
{
    OPENFILENAMEW ofn = {};
    wchar_t szFile[260] = {0};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0PowerShell Scripts (*.ps1)\0*.ps1\0C++ Files (*.cpp;*.h)\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        // Extract filename for tab title
        std::wstring fullPath = szFile;
        std::wstring fileName = fullPath;
        size_t lastSlash = fullPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            fileName = fullPath.substr(lastSlash + 1);
        }
        
        CreateNewTab(fileName, fullPath);
    }
}

void IDEWindow::OnSaveFile()
{
    if (currentFilePath_.empty()) {
        OPENFILENAMEW ofn = {};
        wchar_t szFile[260] = {0};
        
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
        ofn.lpstrFilter = L"PowerShell Scripts (*.ps1)\0*.ps1\0All Files (*.*)\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        ofn.lpstrDefExt = L"ps1";
        
        if (!GetSaveFileNameW(&ofn)) {
            return;
        }
        currentFilePath_ = szFile;
    }
    
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hEditor_, buffer.data(), len + 1);
    
    std::string filePath = WideToUTF8(currentFilePath_);
    std::ofstream file(filePath, std::ios::binary);
    if (file.is_open()) {
        std::string content = WideToUTF8(buffer.data());
        file.write(content.data(), content.size());
        file.close();
        isModified_ = false;
        
        std::wstring msg = L"File saved: " + currentFilePath_;
        SendMessageW(hStatusBar_, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
        if (activeTabId_ >= 0 && openTabs_.find(activeTabId_) != openTabs_.end()) {
            openTabs_[activeTabId_].filePath = currentFilePath_;
            openTabs_[activeTabId_].content = buffer.data();
            openTabs_[activeTabId_].modified = false;
            std::wstring fileName = currentFilePath_;
            size_t lastSlash = fileName.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) fileName = fileName.substr(lastSlash+1);
            UpdateTabTitle(activeTabId_, fileName);
        }
    }
}

void IDEWindow::OnRunScript()
{
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hEditor_, buffer.data(), len + 1);
    
    ExecutePowerShellCommand(buffer.data());
}

void IDEWindow::LoadFileIntoEditor(const std::wstring& filePath)
{
    std::string pathUTF8 = WideToUTF8(filePath);
    std::ifstream file(pathUTF8, std::ios::binary);
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        std::wstring content = UTF8ToWide(ss.str());
        SetWindowTextW(hEditor_, content.c_str());
        currentFilePath_ = filePath;
        isModified_ = false;
        
        std::wstring msg = L"File opened: " + filePath;
        SendMessageW(hStatusBar_, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
    }
}

void IDEWindow::ExecutePowerShellCommand(const std::wstring& command)
{
    if (command.empty()) return;
    
    AppendOutputText(hOutput_, L"PS> " + command + L"\r\n");
    
    // Create process to execute PowerShell command
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;
    
    HANDLE hStdOutRead, hStdOutWrite;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        AppendOutputText(hOutput_, L"[Error] Failed to create pipe\r\n");
        return;
    }
    
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdOutWrite;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    
    std::wstring cmdLine = L"powershell.exe -NoProfile -Command \"" + command + L"\"";
    
    if (!CreateProcessW(nullptr, &cmdLine[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        AppendOutputText(hOutput_, L"[Error] Failed to execute command\r\n");
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return;
    }
    
    CloseHandle(hStdOutWrite);
    
    // Read output using helper method
    ReadTerminalOutput(hStdOutRead, pi.hProcess);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hStdOutRead);
}

void IDEWindow::ReadTerminalOutput(HANDLE hPipeRead, HANDLE hProcess) {
    char buffer[4096];
    DWORD bytesRead;
    DWORD bytesAvailable;
    std::string output;
    
    // Non-blocking read with timeout
    DWORD startTime = GetTickCount();
    const DWORD timeout = 5000; // 5 seconds
    
    while (GetTickCount() - startTime < timeout) {
        // Check if process is still running
        DWORD exitCode;
        if (GetExitCodeProcess(hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            // Process finished, read remaining data
            while (PeekNamedPipe(hPipeRead, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0) {
                if (ReadFile(hPipeRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output.append(buffer, bytesRead);
                }
            }
            break;
        }
        
        // Check for available data
        if (PeekNamedPipe(hPipeRead, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0) {
            DWORD toRead = std::min(bytesAvailable, (DWORD)(sizeof(buffer) - 1));
            if (ReadFile(hPipeRead, buffer, toRead, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output.append(buffer, bytesRead);
                
                // Update output panel immediately for responsiveness
                AppendOutputText(hOutput_, UTF8ToWide(std::string(buffer, bytesRead)));
                output.clear(); // Clear after displaying
            }
        }
        
        Sleep(50); // Brief sleep to avoid busy-waiting
    }
    
    // Append any remaining output
    if (!output.empty()) {
        AppendOutputText(hOutput_, UTF8ToWide(output) + L"\r\n");
    }
}

void IDEWindow::UpdateStatusBar()
{
    LRESULT pos = SendMessageW(hEditor_, EM_GETSEL, 0, 0);
    int startPos = LOWORD(pos);
    
    int lineIndex = SendMessageW(hEditor_, EM_LINEFROMCHAR, startPos, 0);
    int lineStart = SendMessageW(hEditor_, EM_LINEINDEX, lineIndex, 0);
    int col = startPos - lineStart + 1;
    
    wchar_t buffer[64];
    swprintf_s(buffer, L"Line %d, Col %d", lineIndex + 1, col);
    SendMessageW(hStatusBar_, SB_SETTEXTW, 1, (LPARAM)buffer);
}

void IDEWindow::Run()
{
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void IDEWindow::Shutdown()
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

// ============================================
// CODE INTELLIGENCE & INTELLISENSE
// ============================================

void IDEWindow::PopulatePowerShellCmdlets()
{
    // Populate keyword list
    keywordList_ = {
        L"if", L"else", L"elseif", L"switch", L"foreach", L"for", L"while", L"do",
        L"function", L"filter", L"param", L"begin", L"process", L"end", L"try", L"catch",
        L"finally", L"throw", L"return", L"break", L"continue", L"exit", L"class",
        L"enum", L"using", L"namespace", L"module", L"workflow", L"parallel", L"sequence"
    };
    
    // Populate common cmdlets
    cmdletList_ = {
        // File System
        L"Get-ChildItem", L"Get-Content", L"Set-Content", L"Copy-Item", L"Move-Item",
        L"Remove-Item", L"New-Item", L"Get-Item", L"Set-Item", L"Clear-Content",
        L"Get-Location", L"Set-Location", L"Push-Location", L"Pop-Location",
        L"Test-Path", L"Resolve-Path", L"Split-Path", L"Join-Path",
        
        // Process Management
        L"Get-Process", L"Start-Process", L"Stop-Process", L"Wait-Process",
        L"Get-Service", L"Start-Service", L"Stop-Service", L"Restart-Service",
        
        // Variables and Environment
        L"Get-Variable", L"Set-Variable", L"New-Variable", L"Remove-Variable",
        L"Clear-Variable", L"Get-ChildItem Env:", L"Get-PSDrive",
        
        // Output and Formatting
        L"Write-Host", L"Write-Output", L"Write-Verbose", L"Write-Warning",
        L"Write-Error", L"Write-Debug", L"Format-Table", L"Format-List",
        L"Out-File", L"Out-String", L"Out-GridView", L"Out-Null",
        
        // Object Manipulation
        L"Select-Object", L"Where-Object", L"ForEach-Object", L"Sort-Object",
        L"Group-Object", L"Measure-Object", L"Compare-Object", L"Tee-Object",
        
        // String and Text
        L"Select-String", L"Get-Unique", L"ConvertTo-Json", L"ConvertFrom-Json",
        L"ConvertTo-Xml", L"ConvertFrom-Csv", L"Export-Csv", L"Import-Csv",
        
        // Module Management
        L"Get-Module", L"Import-Module", L"Remove-Module", L"Get-Command",
        L"Get-Help", L"Update-Help", L"Get-Member",
        
        // Network
        L"Test-Connection", L"Invoke-WebRequest", L"Invoke-RestMethod",
        
        // Registry
        L"Get-ItemProperty", L"Set-ItemProperty", L"New-ItemProperty",
        
        // WMI/CIM
        L"Get-WmiObject", L"Get-CimInstance", L"Invoke-CimMethod"
    };
}

std::wstring IDEWindow::GetCurrentWord()
{
    // Get current cursor position
    DWORD startPos, endPos;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
    
    // Get the entire text
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hEditor_, buffer.data(), len + 1);
    std::wstring text(buffer.data());
    
    // Find word boundaries
    int wordStart = startPos;
    while (wordStart > 0 && (iswalnum(text[wordStart - 1]) || 
           text[wordStart - 1] == L'-' || text[wordStart - 1] == L'$')) {
        wordStart--;
    }
    
    if (wordStart < 0 || startPos <= 0) return L"";
    
    return text.substr(wordStart, startPos - wordStart);
}

std::wstring IDEWindow::GetCurrentLine()
{
    DWORD startPos, endPos;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
    
    int lineIndex = SendMessageW(hEditor_, EM_LINEFROMCHAR, startPos, 0);
    int lineStart = SendMessageW(hEditor_, EM_LINEINDEX, lineIndex, 0);
    int lineLength = SendMessageW(hEditor_, EM_LINELENGTH, startPos, 0);
    
    std::vector<wchar_t> lineBuffer(lineLength + 1);
    lineBuffer[0] = lineLength;
    SendMessageW(hEditor_, EM_GETLINE, lineIndex, (LPARAM)lineBuffer.data());
    lineBuffer[lineLength] = L'\0';
    
    return std::wstring(lineBuffer.data());
}

void IDEWindow::ShowAutocompleteList(const std::wstring& partialText)
{
    if (partialText.empty()) {
        HideAutocompleteList();
        return;
    }
    
    // Create autocomplete listbox if it doesn't exist
    if (!hAutocompleteList_) {
        hAutocompleteList_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"LISTBOX",
            nullptr,
            WS_POPUP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
            0, 0, 300, 200,
            hwnd_,
            (HMENU)ID_AUTOCOMPLETE_LIST,
            hInstance_,
            nullptr
        );
        
        // Set font
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        SendMessageW(hAutocompleteList_, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Clear existing items
    SendMessageW(hAutocompleteList_, LB_RESETCONTENT, 0, 0);
    
    // Convert to lowercase for case-insensitive matching
    std::wstring lowerPartial = partialText;
    std::transform(lowerPartial.begin(), lowerPartial.end(), lowerPartial.begin(), ::towlower);
    
    // Add matching cmdlets
    for (const auto& cmdlet : cmdletList_) {
        std::wstring lowerCmdlet = cmdlet;
        std::transform(lowerCmdlet.begin(), lowerCmdlet.end(), lowerCmdlet.begin(), ::towlower);
        
        if (lowerCmdlet.find(lowerPartial) == 0) {  // Starts with
            SendMessageW(hAutocompleteList_, LB_ADDSTRING, 0, (LPARAM)cmdlet.c_str());
        }
    }
    
    // Add matching keywords
    for (const auto& keyword : keywordList_) {
        std::wstring lowerKeyword = keyword;
        std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::towlower);
        
        if (lowerKeyword.find(lowerPartial) == 0) {
            SendMessageW(hAutocompleteList_, LB_ADDSTRING, 0, (LPARAM)keyword.c_str());
        }
    }
    
    // Add matching variables
    ParsePowerShellVariables();
    for (const auto& var : variableList_) {
        std::wstring lowerVar = var;
        std::transform(lowerVar.begin(), lowerVar.end(), lowerVar.begin(), ::towlower);
        
        if (lowerVar.find(lowerPartial) == 0) {
            SendMessageW(hAutocompleteList_, LB_ADDSTRING, 0, (LPARAM)var.c_str());
        }
    }
    
    int itemCount = SendMessageW(hAutocompleteList_, LB_GETCOUNT, 0, 0);
    if (itemCount > 0) {
        selectedAutocompleteIndex_ = 0;
        SendMessageW(hAutocompleteList_, LB_SETCURSEL, 0, 0);
        UpdateAutocompletePosition();
        ShowWindow(hAutocompleteList_, SW_SHOW);
        autocompleteVisible_ = true;
    } else {
        HideAutocompleteList();
    }
}

void IDEWindow::HideAutocompleteList()
{
    if (hAutocompleteList_) {
        ShowWindow(hAutocompleteList_, SW_HIDE);
        autocompleteVisible_ = false;
    }
}

void IDEWindow::UpdateAutocompletePosition()
{
    if (!hAutocompleteList_) return;
    
    // Get cursor position in editor
    DWORD startPos, endPos;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
    
    // Get position of cursor in editor coordinates
    POINT pt;
    SendMessageW(hEditor_, EM_POSFROMCHAR, (WPARAM)&pt, startPos);
    
    // Convert to screen coordinates
    RECT editorRect;
    GetWindowRect(hEditor_, &editorRect);
    
    int x = editorRect.left + pt.x;
    int y = editorRect.top + pt.y + 20;  // Offset below cursor
    
    // Position the autocomplete list
    SetWindowPos(hAutocompleteList_, HWND_TOPMOST, x, y, 300, 200, SWP_NOACTIVATE);
}

void IDEWindow::SelectAutocompleteItem(int index)
{
    if (!hAutocompleteList_) return;
    
    int itemCount = SendMessageW(hAutocompleteList_, LB_GETCOUNT, 0, 0);
    if (itemCount == 0) return;
    
    // Wrap around
    if (index < 0) index = itemCount - 1;
    if (index >= itemCount) index = 0;
    
    selectedAutocompleteIndex_ = index;
    SendMessageW(hAutocompleteList_, LB_SETCURSEL, index, 0);
}

void IDEWindow::InsertAutocompleteSelection()
{
    if (!hAutocompleteList_ || !autocompleteVisible_) return;
    
    int index = SendMessageW(hAutocompleteList_, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR) return;
    
    // Get selected text
    int len = SendMessageW(hAutocompleteList_, LB_GETTEXTLEN, index, 0);
    std::vector<wchar_t> buffer(len + 1);
    SendMessageW(hAutocompleteList_, LB_GETTEXT, index, (LPARAM)buffer.data());
    std::wstring selectedText(buffer.data());
    
    // Get current word to replace
    std::wstring currentWord = GetCurrentWord();
    
    // Get current selection
    DWORD startPos, endPos;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
    
    // Calculate word start position
    int wordStart = startPos - currentWord.length();
    
    // Select the current word
    SendMessageW(hEditor_, EM_SETSEL, wordStart, startPos);
    
    // Replace with selected text
    SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)selectedText.c_str());
    
    HideAutocompleteList();
}

void IDEWindow::ShowParameterHint(const std::wstring& cmdlet)
{
    // Create parameter hint window if it doesn't exist
    if (!hParameterHint_) {
        hParameterHint_ = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"STATIC",
            nullptr,
            WS_POPUP | WS_BORDER | SS_LEFT,
            0, 0, 400, 100,
            hwnd_,
            (HMENU)ID_PARAMETER_HINT,
            hInstance_,
            nullptr
        );
        
        // Set font and background
        HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
        SendMessageW(hParameterHint_, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Build parameter hint text (simplified - in production, query Get-Command)
    std::wstring hintText = cmdlet + L" ";
    
    // Add common parameter patterns based on cmdlet
    if (cmdlet.find(L"Get-") == 0) {
        hintText += L"[-Name <String>] [-Path <String>]";
    } else if (cmdlet.find(L"Set-") == 0) {
        hintText += L"[-Name <String>] [-Value <Object>]";
    } else if (cmdlet.find(L"New-") == 0) {
        hintText += L"[-Name <String>] [-Path <String>]";
    } else {
        hintText += L"[Parameters...]";
    }
    
    SetWindowTextW(hParameterHint_, hintText.c_str());
    
    // Position hint above cursor
    DWORD startPos, endPos;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startPos, (LPARAM)&endPos);
    
    POINT pt;
    SendMessageW(hEditor_, EM_POSFROMCHAR, (WPARAM)&pt, startPos);
    
    RECT editorRect;
    GetWindowRect(hEditor_, &editorRect);
    
    int x = editorRect.left + pt.x;
    int y = editorRect.top + pt.y - 105;  // Above cursor
    
    SetWindowPos(hParameterHint_, HWND_TOPMOST, x, y, 400, 100, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    // Auto-hide after 5 seconds
    SetTimer(hwnd_, 1, 5000, nullptr);
}

void IDEWindow::HideParameterHint()
{
    if (hParameterHint_) {
        ShowWindow(hParameterHint_, SW_HIDE);
    }
}

void IDEWindow::ParsePowerShellVariables()
{
    variableList_.clear();
    
    // Get editor text
    int len = GetWindowTextLengthW(hEditor_);
    if (len == 0) return;
    
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hEditor_, buffer.data(), len + 1);
    std::wstring text(buffer.data());
    
    // Simple regex to find variables ($variable)
    std::wregex varPattern(L"\\$[a-zA-Z_][a-zA-Z0-9_]*");
    std::wsregex_iterator it(text.begin(), text.end(), varPattern);
    std::wsregex_iterator end;
    
    std::set<std::wstring> uniqueVars;
    while (it != end) {
        uniqueVars.insert(it->str());
        ++it;
    }
    
    variableList_.assign(uniqueVars.begin(), uniqueVars.end());
}

void IDEWindow::ApplySyntaxHighlighting() {
    // Get editor text
    int textLen = GetWindowTextLengthW(hEditor_);
    if (textLen == 0) return;
    
    std::wstring text(textLen + 1, L'\0');
    GetWindowTextW(hEditor_, &text[0], textLen + 1);
    text.pop_back();
    
    // Disable redraw during highlighting
    SendMessageW(hEditor_, WM_SETREDRAW, FALSE, 0);
    
    // Save current selection
    DWORD startSel, endSel;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
    
    // Reset all text to default color first
    CHARFORMAT2W cfDefault = {};
    cfDefault.cbSize = sizeof(CHARFORMAT2W);
    cfDefault.dwMask = CFM_COLOR;
    cfDefault.crTextColor = textColor_;
    SendMessageW(hEditor_, EM_SETSEL, 0, textLen);
    SendMessageW(hEditor_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cfDefault);
    
    // Apply syntax highlighting patterns
    std::wregex commentRegex(L"#[^\\r\\n]*");
    std::wregex stringRegex(LR"(("[^"]*"|'[^']*'))");
    std::wregex variableRegex(L"\\$[a-zA-Z_][a-zA-Z0-9_]*");
    std::wregex cmdletRegex(L"\\b(Get|Set|New|Remove|Add|Clear|Export|Import|Select|Where|ForEach|Write|Read|Test|Invoke|Start|Stop|Restart|Out|Format|Compare|Copy|Move|Rename)-[A-Za-z]+\\b");
    std::wregex keywordRegex(L"\\b(if|else|elseif|switch|while|for|foreach|function|param|begin|process|end|try|catch|finally|throw|return|break|continue)\\b");
    
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(CHARFORMAT2W);
    cf.dwMask = CFM_COLOR;
    
    // Highlight comments (green)
    cf.crTextColor = commentColor_;
    for (std::wsregex_iterator it(text.begin(), text.end(), commentRegex), end; it != end; ++it) {
        SendMessageW(hEditor_, EM_SETSEL, it->position(), it->position() + it->length());
        SendMessageW(hEditor_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    // Highlight strings (orange)
    cf.crTextColor = stringColor_;
    for (std::wsregex_iterator it(text.begin(), text.end(), stringRegex), end; it != end; ++it) {
        SendMessageW(hEditor_, EM_SETSEL, it->position(), it->position() + it->length());
        SendMessageW(hEditor_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    // Highlight variables (light blue)
    cf.crTextColor = variableColor_;
    for (std::wsregex_iterator it(text.begin(), text.end(), variableRegex), end; it != end; ++it) {
        SendMessageW(hEditor_, EM_SETSEL, it->position(), it->position() + it->length());
        SendMessageW(hEditor_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    // Highlight cmdlets (teal)
    cf.crTextColor = cmdletColor_;
    for (std::wsregex_iterator it(text.begin(), text.end(), cmdletRegex), end; it != end; ++it) {
        SendMessageW(hEditor_, EM_SETSEL, it->position(), it->position() + it->length());
        SendMessageW(hEditor_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    // Highlight keywords (blue)
    cf.crTextColor = keywordColor_;
    for (std::wsregex_iterator it(text.begin(), text.end(), keywordRegex), end; it != end; ++it) {
        SendMessageW(hEditor_, EM_SETSEL, it->position(), it->position() + it->length());
        SendMessageW(hEditor_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    
    // Restore selection
    SendMessageW(hEditor_, EM_SETSEL, startSel, endSel);
    
    // Re-enable redraw
    SendMessageW(hEditor_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hEditor_, nullptr, TRUE);
}

// ============================================
// MULTI-TAB EDITOR SYSTEM
// ============================================

void IDEWindow::CreateNewTab(const std::wstring& title, const std::wstring& filePath)
{
    // Create tab info
    TabInfo tabInfo;
    tabInfo.filePath = filePath;
    tabInfo.content = L"";
    tabInfo.modified = false;
    
    int tabId = nextTabId_++;
    openTabs_[tabId] = tabInfo;
    
    // Add tab to control
    TCITEMW tci = {};
    tci.mask = TCIF_TEXT | TCIF_PARAM;
    tci.pszText = const_cast<LPWSTR>(title.c_str());
    tci.lParam = tabId;
    
    int tabIndex = TabCtrl_InsertItem(hTabControl_, TabCtrl_GetItemCount(hTabControl_), &tci);
    TabCtrl_SetCurSel(hTabControl_, tabIndex);
    activeTabId_ = tabId;
    
    // Load content if file exists
    if (!filePath.empty() && filePath != L"") {
        LoadFileIntoEditor(filePath);
    } else {
        SetWindowTextW(hEditor_, L"");
    }
    
    UpdateStatusBar();
}

void IDEWindow::OnSwitchTab(int tabIndex)
{
    if (tabIndex < 0) return;
    
    // Save current tab content before switching
    if (activeTabId_ >= 0) {
        SaveCurrentTab();
    }
    
    // Get the tab ID from lParam
    TCITEMW tci = {};
    tci.mask = TCIF_PARAM;
    TabCtrl_GetItem(hTabControl_, tabIndex, &tci);
    int tabId = (int)tci.lParam;
    
    // Load the new tab content
    activeTabId_ = tabId;
    LoadTabContent(tabId);
}

void IDEWindow::SaveCurrentTab()
{
    if (activeTabId_ < 0 || openTabs_.find(activeTabId_) == openTabs_.end()) {
        return;
    }
    
    // Get current editor content
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hEditor_, buffer.data(), len + 1);
    
    // Save to tab info
    openTabs_[activeTabId_].content = buffer.data();
    openTabs_[activeTabId_].modified = isModified_;
}

void IDEWindow::LoadTabContent(int tabId)
{
    if (openTabs_.find(tabId) == openTabs_.end()) {
        return;
    }
    
    TabInfo& tab = openTabs_[tabId];
    
    // Set editor content
    SetWindowTextW(hEditor_, tab.content.c_str());
    currentFilePath_ = tab.filePath;
    isModified_ = tab.modified;
    
    // Update window title
    std::wstring title = L"RawrXD PowerShell IDE - ";
    if (!tab.filePath.empty()) {
        // Extract filename from path
        size_t lastSlash = tab.filePath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            title += tab.filePath.substr(lastSlash + 1);
        } else {
            title += tab.filePath;
        }
    } else {
        title += L"Untitled";
    }
    
    if (tab.modified) {
        title += L" *";
    }
    
    SetWindowTextW(hwnd_, title.c_str());
    UpdateStatusBar();
}

int IDEWindow::GetCurrentTabId()
{
    int tabIndex = TabCtrl_GetCurSel(hTabControl_);
    if (tabIndex < 0) return -1;
    
    TCITEMW tci = {};
    tci.mask = TCIF_PARAM;
    TabCtrl_GetItem(hTabControl_, tabIndex, &tci);
    return (int)tci.lParam;
}

void IDEWindow::OnCloseTab(int tabIndex)
{
    if (tabIndex < 0) return;
    
    // Get tab ID
    TCITEMW tci = {};
    tci.mask = TCIF_PARAM;
    TabCtrl_GetItem(hTabControl_, tabIndex, &tci);
    int tabId = (int)tci.lParam;
    
    // Check if modified and ask to save
    if (openTabs_[tabId].modified) {
        int result = MessageBoxW(hwnd_, 
            L"Do you want to save changes?", 
            L"Unsaved Changes", 
            MB_YESNOCANCEL | MB_ICONQUESTION);
            
        if (result == IDCANCEL) {
            return;
        } else if (result == IDYES) {
            SaveCurrentTab();
            OnSaveFile();
        }
    }
    
    // Remove tab
    TabCtrl_DeleteItem(hTabControl_, tabIndex);
    openTabs_.erase(tabId);
    
    // If no tabs left, create a new one
    int tabCount = TabCtrl_GetItemCount(hTabControl_);
    if (tabCount == 0) {
        CreateNewTab(L"Untitled", L"");
    } else {
        // Switch to adjacent tab
        int newIndex = (tabIndex > 0) ? tabIndex - 1 : 0;
        TabCtrl_SetCurSel(hTabControl_, newIndex);
        OnSwitchTab(newIndex);
    }
}

// -------- Session Persistence & Command Palette Implementation ---------
static std::string WideToUTF8(const std::wstring& w) {
    if (w.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &w[0], (int)w.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &w[0], (int)w.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}
static std::wstring UTF8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
    std::wstring strTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &strTo[0], size_needed);
    return strTo;
}
static std::wstring TrimWhitespace(const std::wstring& text) {
    size_t first = text.find_first_not_of(L" \t\n\r");
    if (std::wstring::npos == first) return L"";
    size_t last = text.find_last_not_of(L" \t\n\r");
    return text.substr(first, (last - first + 1));
}
static void AppendOutputText(HWND hOutput, const std::wstring& text) {
    if (!hOutput) return;
    int len = GetWindowTextLengthW(hOutput);
    SendMessageW(hOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hOutput, EM_REPLACESEL, 0, (LPARAM)text.c_str());
    SendMessageW(hOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

// Simple prompt implementation for Win32 (no resource file needed)
static std::wstring PromptForText(HWND owner, const std::wstring& title, const std::wstring& label, const std::wstring& defaultValue) {
    static std::wstring result;
    result = defaultValue;
    
    struct DialogData {
        std::wstring title;
        std::wstring label;
        std::wstring* result;
    } data = { title, label, &result };

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        if (msg == WM_CREATE) {
            DialogData* pData = (DialogData*)((CREATESTRUCT*)lParam)->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pData);
            
            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            
            HWND hStatic = CreateWindowW(L"STATIC", pData->label.c_str(), WS_VISIBLE | WS_CHILD, 10, 10, 360, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hStatic, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hEdit = CreateWindowW(L"EDIT", pData->result->c_str(), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 10, 35, 360, 25, hwnd, (HMENU)101, NULL, NULL);
            SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnOK = CreateWindowW(L"BUTTON", L"OK", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 200, 70, 80, 25, hwnd, (HMENU)IDOK, NULL, NULL);
            SendMessage(hBtnOK, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_VISIBLE | WS_CHILD, 290, 70, 80, 25, hwnd, (HMENU)IDCANCEL, NULL, NULL);
            SendMessage(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
            
            SendMessage(hEdit, EM_SETSEL, 0, -1);
            SetFocus(hEdit);
            return 0;
        }
        if (msg == WM_COMMAND) {
            int id = LOWORD(wParam);
            if (id == IDOK) {
                DialogData* pData = (DialogData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                if (pData) {
                    wchar_t buffer[1024];
                    GetWindowTextW(GetDlgItem(hwnd, 101), buffer, 1024);
                    pData->result->operator=(buffer);
                }
                DestroyWindow(hwnd);
            } else if (id == IDCANCEL) {
                DialogData* pData = (DialogData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                if (pData) *(pData->result) = L"";
                DestroyWindow(hwnd);
            }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    };
    
    // Unique class name
    std::wstring className = L"RawrXDPromptClass_" + std::to_wstring(GetTickCount64());
    wc.lpszClassName = className.c_str();
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    RegisterClassW(&wc);
    
    // Calculate center of owner
    RECT rcOwner;
    GetWindowRect(owner, &rcOwner);
    int x = rcOwner.left + (rcOwner.right - rcOwner.left - 400) / 2;
    int y = rcOwner.top + (rcOwner.bottom - rcOwner.top - 150) / 2;

    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, className.c_str(), title.c_str(), WS_VISIBLE | WS_POPUP | WS_CAPTION | WS_SYSMENU, 
        x, y, 400, 150, owner, NULL, GetModuleHandle(NULL), &data);
         
    MSG msg;
    EnableWindow(owner, FALSE);
    while (IsWindow(hDlg) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    if(IsWindow(owner)) SetForegroundWindow(owner);
    UnregisterClassW(className.c_str(), GetModuleHandle(NULL));
    
    return result;
}

static LRESULT CALLBACK TerminalProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        // Get IDEWindow instance to call terminal handler
        HWND parent = GetParent(hwnd);
        if (parent) {
            IDEWindow* pThis = (IDEWindow*)GetWindowLongPtrW(parent, GWLP_USERDATA);
            if (pThis) {
                // Get terminal text
                int len = GetWindowTextLengthW(hwnd);
                if (len > 0) {
                    std::wstring text(len + 1, L'\0');
                    GetWindowTextW(hwnd, &text[0], len + 1);
                    text.pop_back();
                    
                    // Execute command
                    pThis->ExecutePowerShellCommand(text);
                    
                    // Clear terminal input
                    SetWindowTextW(hwnd, L"");
                }
            }
        }
        return 0;
    }
    return CallWindowProc(g_terminalProc, hwnd, msg, wParam, lParam);
}

void IDEWindow::SaveSession() {
    // Basic session saving logic (placeholder)
}

void IDEWindow::LoadSession() {
    CreateNewTab(L"Untitled-1", L"");
}

void IDEWindow::ToggleCommandPalette() {
    if (hCommandPalette_ && IsWindowVisible(hCommandPalette_)) {
        ShowWindow(hCommandPalette_, SW_HIDE);
    } else {
        if (!hCommandPalette_) {
            hCommandPalette_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"LISTBOX", L"", 
                WS_POPUP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS, 
                0, 0, 400, 300, hwnd_, (HMENU)8001, hInstance_, NULL);
            
            HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessageW(hCommandPalette_, WM_SETFONT, (WPARAM)hFont, TRUE);

            SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Generate Project");
            SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Generate Guide");
            SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Hotpatch Memory");
            SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Open Settings");
        }
        
        RECT rc;
        GetWindowRect(hwnd_, &rc);
        int center_x = rc.left + (rc.right - rc.left) / 2 - 200;
        int center_y = rc.top + 100;
        
        SetWindowPos(hCommandPalette_, HWND_TOPMOST, center_x, center_y, 400, 300, SWP_SHOWWINDOW);
        SetFocus(hCommandPalette_);
    }
}

void IDEWindow::ExecutePaletteSelection() {
    if (!hCommandPalette_) return;
    int cur = SendMessageW(hCommandPalette_, LB_GETCURSEL, 0, 0);
    if (cur != LB_ERR) {
        switch(cur) {
            case 0: PostMessageW(hwnd_, WM_COMMAND, IDM_TOOLS_GENERATE_PROJECT, 0); break;
            case 1: PostMessageW(hwnd_, WM_COMMAND, IDM_TOOLS_GENERATE_GUIDE, 0); break;
            case 2: PostMessageW(hwnd_, WM_COMMAND,IDM_TOOLS_HOTPATCH, 0); break;
        }
        ShowWindow(hCommandPalette_, SW_HIDE);
    }
}

void IDEWindow::HideMarketplace() {
    if (hMarketplaceList_) ShowWindow(hMarketplaceList_, SW_HIDE);
}

void IDEWindow::SearchMarketplace(const std::wstring& searchText) {
    AppendOutputText(hOutput_, L"[Marketplace] Searching for: " + searchText + L"\r\n");
}

void IDEWindow::ShowExtensionDetails(const ExtensionInfo& ext) {
    std::wstring details = L"Name: " + ext.name + L"\r\nAuthor: " + ext.author + 
                           L"\r\nVersion: " + ext.version + L"\r\n" + ext.description;
    AppendOutputText(hOutput_, details + L"\r\n");
}

void IDEWindow::InstallExtension(const ExtensionInfo& ext) {
    AppendOutputText(hOutput_, L"[Extension] Installing " + ext.name + L"...\r\n");
}

void IDEWindow::UninstallExtension(const ExtensionInfo& ext) {
    AppendOutputText(hOutput_, L"[Extension] Uninstalling " + ext.name + L"...\r\n");
}

void IDEWindow::UpdateTabTitle(int tabId, const std::wstring& title) {
    // Update tab title in tab control
    if (tabId < 0 || !hTabControl_) return;
    
    TCITEMW tie;
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(title.c_str());
    
    TabCtrl_SetItem(hTabControl_, tabId, &tie);
    
    // Update the internal tab structure if it exists
    auto it = openTabs_.find(tabId);
    if (it != openTabs_.end()) {
        // Extract filename from full path if needed
        std::wstring fileName = title;
        size_t lastSlash = fileName.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            fileName = fileName.substr(lastSlash + 1);
        }
        // Don't modify the filePath, just note that title was updated
    }
}

