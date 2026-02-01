#include "ide_window.h"
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
#include <memory> 

#pragma comment(lib, "winhttp.lib")

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// Window class name
static const wchar_t* IDE_WINDOW_CLASS = L"RawrXDIDE";
static const int IDM_FILE_NEW = 1001;
static const int IDM_FILE_OPEN = 1002;
static const int IDM_FILE_OPEN_FOLDER = 1003;
static const int IDM_FILE_SAVE = 1004;
static const int IDM_FILE_EXIT = 1005;
static const int IDM_EDIT_CUT = 2001;
static const int IDM_EDIT_COPY = 2002;
static const int IDM_EDIT_PASTE = 2003;
static const int IDM_RUN_SCRIPT = 3001;
static const int IDM_VIEW_BROWSER = 3002;
static const int ID_EDITOR = 4001;
static const int ID_FILETREE = 4002;
static const int ID_TERMINAL = 4003;
static const int ID_OUTPUT = 4004;
static const int ID_TABCONTROL = 4005;
static const int ID_WEBBROWSER = 4006;
static const int ID_AUTOCOMPLETE_LIST = 4007;
static const int ID_PARAMETER_HINT = 4008;
#define IDM_TOOLS_ANALYZE 5001
#define IDM_TOOLS_TEST    5002

#include "../gui/RawrXD_EditorWindow.h" // Use Direct2D Editor

IDEWindow::IDEWindow()
    : hwnd_(nullptr)
    , hEditor_(nullptr)
    , hFileTree_(nullptr)
    , hTerminal_(nullptr)
    , hOutput_(nullptr)
    , hStatusBar_(nullptr)
    , hToolBar_(nullptr)
    , pEditor(nullptr) 
    , hTabControl_(nullptr)
    , hWebBrowser_(nullptr)
    , hAutocompleteList_(nullptr)
    , hParameterHint_(nullptr)
    , hFindDialog_(nullptr)
    , hReplaceDialog_(nullptr)
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
    , hCommandPalette_(nullptr)
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
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_ANALYZE, L"&Analyze Codebase");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_TEST, L"Generate &Tests");
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
    
    // Create Direct2D editor control
    pEditor = new RawrXD::EditorWindow();
    
    // Initialize AI
    aiHub = std::make_shared<RawrXD::AIIntegrationHub>();
    pEditor->setAIHub(aiHub);
    
    modelManager = std::make_shared<AutonomousModelManager>();
    codebaseEngine = std::make_shared<IntelligentCodebaseEngine>();
    featureEngine = std::make_shared<AutonomousFeatureEngine>();

    if (pEditor->create(hwnd_, 200, toolbarHeight, rcClient.right - 400, rcClient.bottom - toolbarHeight - statusHeight - 200)) {
       pEditor->setText(L"# RawrXD PowerShell IDE - C++ Native Edition\n# AI Logic Enabled\n");
    }
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
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
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
    // Web browser placeholder (WebView2 integration pending)
    // Create a static control to inform user
    hWebBrowser_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"STATIC", 
        L"Web Browser Unavailable\r\n(Requires Edge WebView2 Runtime)",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        0, 0, 0, 0, // Resized later
        hwnd_,
        (HMENU)ID_WEBBROWSER,
        hInstance_,
        nullptr
    );
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
                        if (pThis->pEditor) pThis->pEditor->cut();
                        else SendMessageW(pThis->hEditor_, WM_CUT, 0, 0);
                        return 0;
                    case IDM_EDIT_COPY:
                        if (pThis->pEditor) pThis->pEditor->copy();
                        else SendMessageW(pThis->hEditor_, WM_COPY, 0, 0);
                        return 0;
                    case IDM_EDIT_PASTE:
                         if (pThis->pEditor) pThis->pEditor->paste();
                         else SendMessageW(pThis->hEditor_, WM_PASTE, 0, 0);
                        return 0;
                    case IDM_RUN_SCRIPT:
                        pThis->OnRunScript();
                        return 0;
                    case IDM_TOOLS_ANALYZE:
                         if (pThis->codebaseEngine) {
                             std::wstring path = pThis->currentFilePath_;
                             if (path.empty()) {
                                 wchar_t buffer[MAX_PATH];
                                 GetCurrentDirectoryW(MAX_PATH, buffer);
                                 path = buffer;
                             } else {
                                 size_t lastSlash = path.find_last_of(L"\\/");
                                 if (lastSlash != std::wstring::npos) {
                                    path = path.substr(0, lastSlash);
                                 }
                             }
                             
                             int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
                             std::string projectPath(len, 0);
                             WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &projectPath[0], len, nullptr, nullptr);
                             if (len > 0) projectPath.resize(len - 1);
                             
                             bool success = pThis->codebaseEngine->analyzeEntireCodebase(projectPath);
                             
                             if (success) {
                                 std::wstring msg = L"Codebase analysis completed for: " + path;
                                 MessageBoxW(hwnd, msg.c_str(), L"Analysis", MB_OK);
                                 
                                 // Also refresh UI if needed
                                 SendMessageW(pThis->hStatusBar_, SB_SETTEXTW, 0, (LPARAM)L"Analysis Complete");
                             } else {
                                 MessageBoxW(hwnd, L"Codebase analysis completed (or nothing to analyze).", L"Analysis", MB_ICONINFORMATION);
                             }
                         }
                         return 0;
                    case IDM_TOOLS_TEST:
                         if (pThis->aiHub && pThis->pEditor) {
                             std::string code = pThis->pEditor->getText().toStdString();
                             std::string tests = pThis->aiHub->generateTests(code);
                             // Display tests in Output
                             SetWindowTextA(pThis->hOutput_, tests.c_str());
                         }
                         return 0;
                    case 5011: // Search button in marketplace
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
                    
                    if (pThis->pEditor) {
                        MoveWindow(pThis->pEditor->handle(), 200, toolbarHeight + TAB_HEIGHT, 
                              rcClient.right - 400, 
                              rcClient.bottom - toolbarHeight - TAB_HEIGHT - statusHeight - 200, TRUE);
                        // Force resize event to D2D
                        RECT rc; 
                        GetClientRect(pThis->pEditor->handle(), &rc);
                        SendMessage(pThis->pEditor->handle(), WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom)); 
                    } else if (pThis->hEditor_) {
                        MoveWindow(pThis->hEditor_, 200, toolbarHeight + TAB_HEIGHT, 
                              rcClient.right - 400, 
                              rcClient.bottom - toolbarHeight - TAB_HEIGHT - statusHeight - 200, TRUE);
                    }

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
                        return result;
                    }
                    else if (ch == L' ' || ch == L'(') {
                        // Show parameter hint for cmdlets
                        std::wstring word = pThis->GetCurrentWord();
                        if (!word.empty() && word.find(L'-') != std::wstring::npos) {
                            pThis->ShowParameterHint(word);
                        }
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
    
    int len = 0;
    std::wstring content;
    if (pEditor) {
        content = pEditor->getText().toStdWString();
    } else {
        len = GetWindowTextLengthW(hEditor_);
        std::vector<wchar_t> buffer(len + 1);
        GetWindowTextW(hEditor_, buffer.data(), len + 1);
        content = buffer.data();
    }
    
    std::wofstream file(currentFilePath_);
    if (file.is_open()) {
        file << content;
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
    std::wifstream file(filePath);
    if (file.is_open()) {
        std::wstringstream ss;
        ss << file.rdbuf();
        SetWindowTextW(hEditor_, ss.str().c_str());
        currentFilePath_ = filePath;
        isModified_ = false;
        
        std::wstring msg = L"File opened: " + filePath;
        SendMessageW(hStatusBar_, SB_SETTEXTW, 0, (LPARAM)msg.c_str());
    }
}

void IDEWindow::ExecutePowerShellCommand(const std::wstring& command)
{
    // Create temp file with script
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring scriptPath = std::wstring(tempPath) + L"rawrxd_temp.ps1";
    
    std::wofstream tempFile(scriptPath);
    if (tempFile.is_open()) {
        tempFile << command;
        tempFile.close();
        
        // Execute PowerShell
        std::wstring cmdLine = L"powershell.exe -ExecutionPolicy Bypass -File \"" + scriptPath + L"\"";
        
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        
        HANDLE hReadPipe, hWritePipe;
        CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
        
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {};
        
        if (CreateProcessW(nullptr, const_cast<LPWSTR>(cmdLine.c_str()), nullptr, nullptr, 
                          TRUE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hWritePipe);
            
            char buffer[4096];
            DWORD bytesRead;
            std::string output;
            
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            }
            
            CloseHandle(hReadPipe);
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            // Convert output to wide string and display
            int wideLen = MultiByteToWideChar(CP_UTF8, 0, output.c_str(), -1, nullptr, 0);
            std::vector<wchar_t> wideOutput(wideLen);
            MultiByteToWideChar(CP_UTF8, 0, output.c_str(), -1, wideOutput.data(), wideLen);
            
            SetWindowTextW(hTerminal_, wideOutput.data());
            SendMessageW(hStatusBar_, SB_SETTEXTW, 0, (LPARAM)L"Script executed");
        }
        
        DeleteFileW(scriptPath.c_str());
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
    if (pEditor) {
        openTabs_[activeTabId_].content = pEditor->getText().toStdWString();
    } else {
        int len = GetWindowTextLengthW(hEditor_);
        std::vector<wchar_t> buffer(len + 1);
        GetWindowTextW(hEditor_, buffer.data(), len + 1);
        openTabs_[activeTabId_].content = buffer.data();
    }
    
    // Save to tab info
    openTabs_[activeTabId_].modified = isModified_;
}

void IDEWindow::LoadTabContent(int tabId)
{
    if (openTabs_.find(tabId) == openTabs_.end()) {
        return;
    }
    
    TabInfo& tab = openTabs_[tabId];
    
    // Set editor content
    if (pEditor) {
        pEditor->setText(RawrXD::String(tab.content.c_str()));
    } else {
        SetWindowTextW(hEditor_, tab.content.c_str());
    }
    
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
        int result = MessageBoxW(hwnd, 
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
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), len, nullptr, nullptr);
    return out;
}
static std::wstring UTF8ToWide(const std::string& s) {
    if (s.empty()) return std::wstring();
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), len);
    return out;
}
static std::string EscapeJSON(const std::string& in) {
    std::string out; out.reserve(in.size()*2);
    for(char c: in){
        switch(c){
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

void IDEWindow::SaveSession() {
    SaveCurrentTab();
    std::ofstream ofs(WideToUTF8(sessionPath_));
    if (!ofs.is_open()) return;
    ofs << "{\n";
    ofs << "\"activeTabId\":" << activeTabId_ << ",\n";
    ofs << "\"tabs\":[\n";
    bool first = true;
    for (auto &kv : openTabs_) {
        if (!first) ofs << ",\n"; first = false;
        const TabInfo &tab = kv.second;
        std::string filePath = WideToUTF8(tab.filePath);
        std::string content = WideToUTF8(tab.content);
        ofs << "  {\"id\":" << kv.first << ",\"filePath\":\"" << EscapeJSON(filePath) << "\",\"modified\":" << (tab.modified?"true":"false");
        if (tab.filePath.empty()) {
            ofs << ",\"content\":\"" << EscapeJSON(content) << "\"";
        }
        ofs << "}";
    }
    ofs << "\n]\n}";
}

void IDEWindow::LoadSession() {
    std::ifstream ifs(WideToUTF8(sessionPath_));
    if (!ifs.is_open()) {
        CreateNewTab(L"Untitled", L"");
        return;
    }
    std::stringstream ss; ss << ifs.rdbuf(); std::string text = ss.str();
    openTabs_.clear();
    int existingCount = hTabControl_ ? TabCtrl_GetItemCount(hTabControl_) : 0;
    for(int i=existingCount-1;i>=0;--i) TabCtrl_DeleteItem(hTabControl_, i);
    size_t pos = 0; activeTabId_ = -1;
    while ((pos = text.find("{\"id\":", pos)) != std::string::npos) {
        pos += 6; int id = std::stoi(text.substr(pos));
        size_t fpPos = text.find("\"filePath\":\"", pos); if (fpPos==std::string::npos) break; fpPos += 14;
        size_t fpEnd = text.find("\"", fpPos); if (fpEnd==std::string::npos) break;
        std::string filePathEsc = text.substr(fpPos, fpEnd - fpPos);
        size_t modPos = text.find("\"modified\":", fpEnd); if (modPos==std::string::npos) break; modPos += 12;
        bool modified = text.compare(modPos, 4, "true") == 0;
        size_t contPos = text.find("\"content\":\"", fpEnd);
        TabInfo info; info.filePath = UTF8ToWide(filePathEsc); info.modified = modified; info.content = L"";
        if (contPos != std::string::npos) {
            contPos += 13; size_t contEnd = text.find("\"", contPos); if (contEnd!=std::string::npos) {
                std::string contentEsc = text.substr(contPos, contEnd - contPos);
                info.content = UTF8ToWide(contentEsc);
            }
        }
        openTabs_[id] = info;
        std::wstring title;
        if (!info.filePath.empty()) {
            size_t lastSlash = info.filePath.find_last_of(L"\\/");
            title = (lastSlash==std::wstring::npos)?info.filePath:info.filePath.substr(lastSlash+1);
        } else title = L"Untitled";
        TCITEMW tci={}; tci.mask=TCIF_TEXT|TCIF_PARAM; tci.pszText=const_cast<LPWSTR>(title.c_str()); tci.lParam=id;
        TabCtrl_InsertItem(hTabControl_, TabCtrl_GetItemCount(hTabControl_), &tci);
    }
    size_t atPos = text.find("\"activeTabId\":"); if (atPos!=std::string::npos) { atPos += 14; activeTabId_ = std::stoi(text.substr(atPos)); }
    if (activeTabId_ < 0 || openTabs_.find(activeTabId_) == openTabs_.end()) {
        if (openTabs_.empty()) { CreateNewTab(L"Untitled", L""); return; }
        activeTabId_ = openTabs_.begin()->first;
    }
    int tabCount = TabCtrl_GetItemCount(hTabControl_);
    for(int i=0;i<tabCount;++i){ TCITEMW tci={}; tci.mask=TCIF_PARAM; TabCtrl_GetItem(hTabControl_, i, &tci); if ((int)tci.lParam==activeTabId_) { TabCtrl_SetCurSel(hTabControl_, i); break; } }
    LoadTabContent(activeTabId_);
}

void IDEWindow::UpdateTabTitle(int tabId, const std::wstring& newTitle) {
    int tabCount = TabCtrl_GetItemCount(hTabControl_);
    for(int i=0;i<tabCount;++i){ TCITEMW tci={}; tci.mask=TCIF_PARAM; TabCtrl_GetItem(hTabControl_, i, &tci); if ((int)tci.lParam==tabId) {
            TCITEMW upd={}; upd.mask=TCIF_TEXT|TCIF_PARAM; upd.pszText=const_cast<LPWSTR>(newTitle.c_str()); upd.lParam=tabId; TabCtrl_SetItem(hTabControl_, i, &upd); break; }
    }
}

void IDEWindow::ToggleCommandPalette() {
    if (!hCommandPalette_) {
        RECT rc; GetClientRect(hwnd_, &rc); int width=400; int height=200;
        hCommandPalette_ = CreateWindowExW(WS_EX_TOOLWINDOW, L"LISTBOX", nullptr,
            WS_CHILD | LBS_NOTIFY | WS_BORDER, rc.right/2 - width/2, rc.top + 80,
            width, height, hwnd_, (HMENU)5001, hInstance_, nullptr);
        HFONT hFont = CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH|FF_MODERN,L"Consolas");
        SendMessageW(hCommandPalette_, WM_SETFONT, (WPARAM)hFont, TRUE);
        PopulateCommandPalette();
    }
    BOOL visible = IsWindowVisible(hCommandPalette_);
    ShowWindow(hCommandPalette_, visible?SW_HIDE:SW_SHOW); if (!visible) SetFocus(hCommandPalette_);
}

void IDEWindow::PopulateCommandPalette() {
    if (!hCommandPalette_) return;
    SendMessageW(hCommandPalette_, LB_RESETCONTENT, 0, 0);
    SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Format: Trim Trailing Whitespace");
    SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Toggle Line Comment");
    SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Duplicate Line");
    SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Delete Line");
    SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"Sort Selected Lines");
    SendMessageW(hCommandPalette_, LB_ADDSTRING, 0, (LPARAM)L"List Functions");
}

void IDEWindow::ExecutePaletteSelection() {
    if (!hCommandPalette_) return;
    int sel = (int)SendMessageW(hCommandPalette_, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    switch(sel) {
        case 0: FormatTrimTrailingWhitespace(); break;
        case 1: ToggleLineComment(); break;
        case 2: DuplicateLine(); break;
        case 3: DeleteLine(); break;
        case 4: SortSelectedLines(); break;
        case 5: ListFunctions(); break;
    }
    ShowWindow(hCommandPalette_, SW_HIDE);
}

void IDEWindow::FormatTrimTrailingWhitespace() {
    int len = GetWindowTextLengthW(hEditor_); std::vector<wchar_t> buf(len+1); GetWindowTextW(hEditor_, buf.data(), len+1); std::wstring text=buf.data();
    std::wstringstream in(text); std::wstring line; std::wstring out; bool first=true;
    while(std::getline(in,line)) { while(!line.empty() && (line.back()==L' '||line.back()==L'\t')) line.pop_back(); if(!first) out += L"\n"; first=false; out += line; }
    SetWindowTextW(hEditor_, out.c_str()); isModified_ = true; SaveCurrentTab(); UpdateStatusBar();
}

void IDEWindow::ListFunctions() {
    int len = GetWindowTextLengthW(hEditor_); std::vector<wchar_t> buf(len+1); GetWindowTextW(hEditor_, buf.data(), len+1); std::wstring text=buf.data();
    std::wregex psFunc(L"function\\s+([A-Za-z0-9_:-]+)\\s*\\(");
    std::wregex cppFunc(L"([A-Za-z_][A-Za-z0-9_]*)\\s*\\(");
    std::wsmatch match;
    std::wstring functions;
    size_t pos = 0;
    while (std::regex_search(text.substr(pos), match, psFunc)) {
        functions += match[1].str() + L"\n";
        pos += match.position() + match.length();
    }
    pos = 0;
    while (std::regex_search(text.substr(pos), match, cppFunc)) {
        functions += match[1].str() + L"\n";
        pos += match.position() + match.length();
    }
    if (!functions.empty()) {
        functions.pop_back(); // Remove trailing newline
        MessageBoxW(hwnd_, functions.c_str(), L"Functions in Script", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(hwnd_, L"No functions found in the script.", L"Functions in Script", MB_OK | MB_ICONINFORMATION);
    }
}

void IDEWindow::ToggleLineComment() {
    int len = GetWindowTextLengthW(hEditor_);
    if (len == 0) return;
    std::vector<wchar_t> buf(len+1);
    GetWindowTextW(hEditor_, buf.data(), len+1);
    std::wstring text=buf.data();
    
    std::wstringstream in(text);
    std::wstring line;
    std::wstring out;
    bool firstLine = true;
    
    while (std::getline(in, line)) {
        if (line.find(L"#") == 0) {
            // Uncomment line
            line.erase(0, 1);
        } else {
            // Comment line
            line = L"# " + line;
        }
        out += line;
        if (in.peek() != WEOF) out += L"\n";
    }
    
    SetWindowTextW(hEditor_, out.c_str());
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

void IDEWindow::DuplicateLine() {
    DWORD start, end;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    if (start == end) return; // No selection, do nothing
    
    // Get the current line
    int lineIndex = SendMessageW(hEditor_, EM_LINEFROMCHAR, start, 0);
    int lineStart = SendMessageW(hEditor_, EM_LINEINDEX, lineIndex, 0);
    int lineLength = SendMessageW(hEditor_, EM_LINELENGTH, lineStart, 0);
    
    std::vector<wchar_t> lineBuffer(lineLength + 1);
    SendMessageW(hEditor_, EM_GETLINE, lineIndex, (LPARAM)lineBuffer.data());
    lineBuffer[lineLength] = L'\0';
    
    // Duplicate the line
    SendMessageW(hEditor_, EM_SETSEL, end, end);
    SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)lineBuffer.data());
    
    // Move cursor to the end of the duplicated line
    DWORD newStart = end + lineLength + 1;
    SendMessageW(hEditor_, EM_SETSEL, newStart, newStart);
    
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

void IDEWindow::DeleteLine() {
    DWORD start, end;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    if (start == end) return; // No selection, do nothing
    
    // Get the current line
    int lineIndex = SendMessageW(hEditor_, EM_LINEFROMCHAR, start, 0);
    int lineStart = SendMessageW(hEditor_, EM_LINEINDEX, lineIndex, 0);
    int lineLength = SendMessageW(hEditor_, EM_LINELENGTH, lineStart, 0);
    
    // Delete the line
    SendMessageW(hEditor_, EM_SETSEL, lineStart, lineStart + lineLength);
    SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)L"");
    
    // Move cursor to the start of the current line
    DWORD newStart = lineStart;
    SendMessageW(hEditor_, EM_SETSEL, newStart, newStart);
    
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

void IDEWindow::SortSelectedLines() {
    DWORD start, end;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    if (start == end) return; // No selection, do nothing
    
    // Get the selected text
    int len = end - start;
    std::vector<wchar_t> buffer(len + 1);
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)buffer.data(), len);
    buffer[len] = L'\0';
    
    // Sort the lines
    std::wstring text(buffer.data());
    std::wstringstream ss(text);
    std::wstring line;
    std::vector<std::wstring> lines;
    
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }
    
    std::sort(lines.begin(), lines.end());
    
    // Replace the selection with sorted lines
    SendMessageW(hEditor_, EM_SETSEL, start, end);
    for (const auto& sortedLine : lines) {
        SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)sortedLine.c_str());
        SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)L"\r\n");
    }
    
    // Move cursor to the end of the sorted text
    DWORD newEnd = start + len;
    SendMessageW(hEditor_, EM_SETSEL, newEnd, newEnd);
    
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

