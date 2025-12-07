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
    originalEditorProc_ = (WNDPROC)SetWindowLongPtrW(hEditor_, GWLP_WNDPROC, (LONG_PTR)EditorProc);
    SetWindowLongPtrW(hEditor_, GWLP_USERDATA, (LONG_PTR)this);
    
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
    // Web browser functionality disabled for now (requires ATL)
    // TODO: Implement using WebView2 or other non-ATL method
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
    
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buffer(len + 1);
    GetWindowTextW(hEditor_, buffer.data(), len + 1);
    
    std::wofstream file(currentFilePath_);
    if (file.is_open()) {
        file << buffer.data();
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
    std::wregex cppFunc(L"([A-Za-z_][A-Za-z0-9_:<>]*)\\s+([A-Za-z_][A-Za-z0-9_:<>]*)\\s*\\(.*?\\)\\s*\\{");
    std::set<std::wstring> names; std::wsmatch m; std::wstring::const_iterator start=text.begin();
    while(std::regex_search(start, text.cend(), m, psFunc)) { names.insert(m[1]); start = m.suffix().first; }
    start = text.begin(); while(std::regex_search(start, text.cend(), m, cppFunc)) { names.insert(m[2]); start = m.suffix().first; }
    std::wstring list=L"Functions:\n"; for(auto &n: names) list += n + L"\n"; if (names.empty()) list += L"(none)";
    MessageBoxW(hwnd_, list.c_str(), L"Function List", MB_OK | MB_ICONINFORMATION);
}

std::wstring IDEWindow::DetectLanguage() {
    if (currentFilePath_.empty()) return L"powershell";
    std::wstring ext;
    size_t dotPos = currentFilePath_.find_last_of(L'.');
    if (dotPos != std::wstring::npos) ext = currentFilePath_.substr(dotPos);
    if (ext == L".cpp" || ext == L".h" || ext == L".hpp" || ext == L".c" || ext == L".cc") return L"cpp";
    if (ext == L".ps1" || ext == L".psm1" || ext == L".psd1") return L"powershell";
    if (ext == L".py") return L"python";
    if (ext == L".js" || ext == L".ts") return L"javascript";
    return L"powershell";
}

void IDEWindow::ToggleLineComment() {
    std::wstring lang = DetectLanguage();
    std::wstring commentPrefix = (lang == L"cpp" || lang == L"javascript") ? L"// " : L"# ";
    
    DWORD startSel, endSel;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
    int startLine = (int)SendMessageW(hEditor_, EM_LINEFROMCHAR, startSel, 0);
    int endLine = (int)SendMessageW(hEditor_, EM_LINEFROMCHAR, endSel, 0);
    
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buf(len+1);
    GetWindowTextW(hEditor_, buf.data(), len+1);
    std::wstring text = buf.data();
    std::wstringstream in(text);
    std::wstring line;
    std::vector<std::wstring> lines;
    while(std::getline(in, line)) lines.push_back(line);
    
    // Check if all selected lines are commented
    bool allCommented = true;
    for(int i = startLine; i <= endLine && i < (int)lines.size(); ++i) {
        std::wstring trimmed = lines[i];
        while(!trimmed.empty() && (trimmed[0] == L' ' || trimmed[0] == L'\t')) trimmed = trimmed.substr(1);
        if (!trimmed.empty() && trimmed.find(commentPrefix.substr(0, commentPrefix.length()-1)) != 0) {
            allCommented = false;
            break;
        }
    }
    
    // Toggle comments
    for(int i = startLine; i <= endLine && i < (int)lines.size(); ++i) {
        if (allCommented) {
            // Remove comment
            std::wstring& l = lines[i];
            size_t pos = l.find(commentPrefix.substr(0, commentPrefix.length()-1));
            if (pos != std::wstring::npos) {
                l.erase(pos, commentPrefix.length());
                // Also remove one trailing space if exists after comment marker
                if (pos < l.length() && l[pos] == L' ') l.erase(pos, 1);
            }
        } else {
            // Add comment at start (preserving indentation)
            size_t firstNonSpace = 0;
            while(firstNonSpace < lines[i].length() && (lines[i][firstNonSpace] == L' ' || lines[i][firstNonSpace] == L'\t')) firstNonSpace++;
            if (firstNonSpace < lines[i].length()) {
                lines[i].insert(firstNonSpace, commentPrefix);
            }
        }
    }
    
    std::wstring result;
    for(size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += L"\n";
        result += lines[i];
    }
    SetWindowTextW(hEditor_, result.c_str());
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

void IDEWindow::DuplicateLine() {
    DWORD startSel, endSel;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
    int lineNum = (int)SendMessageW(hEditor_, EM_LINEFROMCHAR, startSel, 0);
    int lineStart = (int)SendMessageW(hEditor_, EM_LINEINDEX, lineNum, 0);
    int lineLen = (int)SendMessageW(hEditor_, EM_LINELENGTH, lineStart, 0);
    
    std::vector<wchar_t> lineBuf(lineLen + 3);
    lineBuf[0] = (wchar_t)lineLen;
    SendMessageW(hEditor_, EM_GETLINE, lineNum, (LPARAM)lineBuf.data());
    std::wstring lineText(lineBuf.data(), lineLen);
    lineText = L"\n" + lineText;
    
    int nextLineStart = (int)SendMessageW(hEditor_, EM_LINEINDEX, lineNum + 1, 0);
    if (nextLineStart == -1) {
        // Last line
        int textLen = GetWindowTextLengthW(hEditor_);
        SendMessageW(hEditor_, EM_SETSEL, textLen, textLen);
        SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)lineText.c_str());
    } else {
        SendMessageW(hEditor_, EM_SETSEL, nextLineStart, nextLineStart);
        SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)lineText.c_str());
    }
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

void IDEWindow::DeleteLine() {
    DWORD startSel, endSel;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
    int lineNum = (int)SendMessageW(hEditor_, EM_LINEFROMCHAR, startSel, 0);
    int lineStart = (int)SendMessageW(hEditor_, EM_LINEINDEX, lineNum, 0);
    int nextLineStart = (int)SendMessageW(hEditor_, EM_LINEINDEX, lineNum + 1, 0);
    
    if (nextLineStart == -1) {
        // Last line - delete from previous newline to end
        int prevLineStart = (int)SendMessageW(hEditor_, EM_LINEINDEX, lineNum - 1, 0);
        if (prevLineStart != -1) {
            int prevLineLen = (int)SendMessageW(hEditor_, EM_LINELENGTH, prevLineStart, 0);
            int deleteFrom = prevLineStart + prevLineLen;
            int textLen = GetWindowTextLengthW(hEditor_);
            SendMessageW(hEditor_, EM_SETSEL, deleteFrom, textLen);
            SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)L"");
        } else {
            // Only line
            SetWindowTextW(hEditor_, L"");
        }
    } else {
        SendMessageW(hEditor_, EM_SETSEL, lineStart, nextLineStart);
        SendMessageW(hEditor_, EM_REPLACESEL, TRUE, (LPARAM)L"");
    }
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

void IDEWindow::SortSelectedLines() {
    DWORD startSel, endSel;
    SendMessageW(hEditor_, EM_GETSEL, (WPARAM)&startSel, (LPARAM)&endSel);
    
    if (startSel == endSel) {
        MessageBoxW(hwnd_, L"Please select multiple lines to sort.", L"Sort Lines", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    int startLine = (int)SendMessageW(hEditor_, EM_LINEFROMCHAR, startSel, 0);
    int endLine = (int)SendMessageW(hEditor_, EM_LINEFROMCHAR, endSel, 0);
    
    int len = GetWindowTextLengthW(hEditor_);
    std::vector<wchar_t> buf(len+1);
    GetWindowTextW(hEditor_, buf.data(), len+1);
    std::wstring text = buf.data();
    std::wstringstream in(text);
    std::wstring line;
    std::vector<std::wstring> lines;
    while(std::getline(in, line)) lines.push_back(line);
    
    // Extract selected lines
    std::vector<std::wstring> selectedLines;
    for(int i = startLine; i <= endLine && i < (int)lines.size(); ++i) {
        selectedLines.push_back(lines[i]);
    }
    
    // Sort
    std::sort(selectedLines.begin(), selectedLines.end());
    
    // Replace
    for(int i = 0; i < (int)selectedLines.size(); ++i) {
        lines[startLine + i] = selectedLines[i];
    }
    
    std::wstring result;
    for(size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result += L"\n";
        result += lines[i];
    }
    SetWindowTextW(hEditor_, result.c_str());
    isModified_ = true;
    SaveCurrentTab();
    UpdateStatusBar();
}

// ============================================================================
// MARKETPLACE FUNCTIONALITY
// ============================================================================

void IDEWindow::CreateMarketplaceWindow() {
    if (hMarketplaceWindow_) return;
    
    RECT rcClient;
    GetClientRect(hwnd_, &rcClient);
    int width = 900;
    int height = 600;
    int x = (rcClient.right - width) / 2;
    int y = (rcClient.bottom - height) / 2;
    
    hMarketplaceWindow_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"STATIC",
        L"Extension Marketplace",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
        x, y, width, height,
        hwnd_,
        nullptr,
        hInstance_,
        nullptr
    );
    
    // Search bar
    hMarketplaceSearch_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        10, 10, width - 120, 25,
        hMarketplaceWindow_,
        (HMENU)5010,
        hInstance_,
        nullptr
    );
    
    // Search button
    CreateWindowExW(
        0,
        L"BUTTON",
        L"Search",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width - 100, 10, 80, 25,
        hMarketplaceWindow_,
        (HMENU)5011,
        hInstance_,
        nullptr
    );
    
    // Extension list
    hMarketplaceList_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        nullptr,
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        10, 45, 400, height - 95,
        hMarketplaceWindow_,
        (HMENU)5012,
        hInstance_,
        nullptr
    );
    
    // Details panel
    hMarketplaceDetails_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"Select an extension to view details",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
        420, 45, width - 440, height - 140,
        hMarketplaceWindow_,
        (HMENU)5013,
        hInstance_,
        nullptr
    );
    
    // Install/Uninstall button
    hMarketplaceInstallBtn_ = CreateWindowExW(
        0,
        L"BUTTON",
        L"Install",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, height - 85, 100, 30,
        hMarketplaceWindow_,
        (HMENU)5014,
        hInstance_,
        nullptr
    );
    
    // Close button
    CreateWindowExW(
        0,
        L"BUTTON",
        L"Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width - 110, height - 85, 80, 30,
        hMarketplaceWindow_,
        (HMENU)5015,
        hInstance_,
        nullptr
    );
    
    // Set fonts
    HFONT hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    SendMessageW(hMarketplaceSearch_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hMarketplaceList_, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(hMarketplaceDetails_, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Extensions directory setup
    wchar_t appData[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData);
    extensionsPath_ = std::wstring(appData) + L"\\RawrXD-IDE\\Extensions";
    CreateDirectoryW(extensionsPath_.c_str(), nullptr);
}

void IDEWindow::ShowMarketplace() {
    if (!hMarketplaceWindow_) {
        CreateMarketplaceWindow();
    }
    
    ShowWindow(hMarketplaceWindow_, SW_SHOW);
    SetFocus(hMarketplaceSearch_);
    
    // Load popular extensions by default
    SearchMarketplace(L"");
}

void IDEWindow::HideMarketplace() {
    if (hMarketplaceWindow_) {
        ShowWindow(hMarketplaceWindow_, SW_HIDE);
    }
}

void IDEWindow::SearchMarketplace(const std::wstring& query) {
    marketplaceExtensions_.clear();
    
    // Load installed extensions first
    LoadInstalledExtensions();
    
    // Query VS Code marketplace
    auto vscodeExts = QueryVSCodeMarketplace(query);
    marketplaceExtensions_.insert(marketplaceExtensions_.end(), vscodeExts.begin(), vscodeExts.end());
    
    // Query Visual Studio marketplace
    auto vsExts = QueryVSMarketplace(query);
    marketplaceExtensions_.insert(marketplaceExtensions_.end(), vsExts.begin(), vsExts.end());
    
    // Mark installed extensions
    for (auto& ext : marketplaceExtensions_) {
        for (const auto& installed : installedExtensions_) {
            if (ext.id == installed.id || ext.name == installed.name) {
                ext.installed = true;
                ext.installPath = installed.installPath;
                break;
            }
        }
    }
    
    PopulateMarketplaceList(marketplaceExtensions_);
}

void IDEWindow::PopulateMarketplaceList(const std::vector<ExtensionInfo>& extensions) {
    if (!hMarketplaceList_) return;
    
    SendMessageW(hMarketplaceList_, LB_RESETCONTENT, 0, 0);
    
    for (const auto& ext : extensions) {
        std::wstring displayName = ext.name + L" (" + ext.publisher + L")";
        if (ext.installed) {
            displayName += L" [INSTALLED]";
        }
        SendMessageW(hMarketplaceList_, LB_ADDSTRING, 0, (LPARAM)displayName.c_str());
    }
}

void IDEWindow::ShowExtensionDetails(const ExtensionInfo& ext) {
    if (!hMarketplaceDetails_) return;
    
    std::wstring details;
    details += L"Name: " + ext.name + L"\r\n";
    details += L"Publisher: " + ext.publisher + L"\r\n";
    details += L"Version: " + ext.version + L"\r\n";
    details += L"Downloads: " + std::to_wstring(ext.downloads) + L"\r\n";
    details += L"Rating: " + std::to_wstring(ext.rating) + L"/5.0\r\n";
    details += L"\r\n" + ext.description;
    
    SetWindowTextW(hMarketplaceDetails_, details.c_str());
    
    if (ext.installed) {
        SetWindowTextW(hMarketplaceInstallBtn_, L"Uninstall");
    } else {
        SetWindowTextW(hMarketplaceInstallBtn_, L"Install");
    }
}

void IDEWindow::InstallExtension(const ExtensionInfo& ext) {
    std::wstring vsixPath = extensionsPath_ + L"\\" + ext.id + L".vsix";
    
    // Show progress message
    std::wstring progressMsg = L"Downloading " + ext.name + L"...";
    SetWindowTextW(hMarketplaceDetails_, progressMsg.c_str());
    
    // Download extension
    if (!DownloadFile(ext.downloadUrl, vsixPath)) {
        MessageBoxW(hwnd_, L"Failed to download extension. Check your internet connection.", 
                    L"Download Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Update progress
    progressMsg = L"Extracting " + ext.name + L"...";
    SetWindowTextW(hMarketplaceDetails_, progressMsg.c_str());
    
    // Extract VSIX
    std::wstring installPath = extensionsPath_ + L"\\" + ext.id;
    if (!ExtractVSIX(vsixPath, installPath)) {
        MessageBoxW(hwnd_, L"Failed to extract extension. The VSIX file may be corrupted.", 
                    L"Extraction Error", MB_OK | MB_ICONERROR);
        DeleteFileW(vsixPath.c_str());
        return;
    }
    
    // Clean up VSIX file
    DeleteFileW(vsixPath.c_str());
    
    // Load extension (scan for JavaScript/TypeScript files)
    std::wstring extPath = installPath + L"\\extension";
    if (GetFileAttributesW(extPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        extPath = installPath; // Extension might be at root
    }
    
    // Look for main entry point
    std::wstring mainJs = extPath + L"\\main.js";
    std::wstring indexJs = extPath + L"\\index.js";
    std::wstring extensionJs = extPath + L"\\extension.js";
    
    bool hasEntryPoint = false;
    if (GetFileAttributesW(mainJs.c_str()) != INVALID_FILE_ATTRIBUTES) {
        hasEntryPoint = true;
    } else if (GetFileAttributesW(indexJs.c_str()) != INVALID_FILE_ATTRIBUTES) {
        hasEntryPoint = true;
    } else if (GetFileAttributesW(extensionJs.c_str()) != INVALID_FILE_ATTRIBUTES) {
        hasEntryPoint = true;
    }
    
    // Add to installed extensions list
    ExtensionInfo installedExt = ext;
    installedExt.installed = true;
    installedExt.installPath = installPath;
    installedExtensions_.push_back(installedExt);
    
    // Build success message
    std::wstring successMsg = L"Successfully installed " + ext.name + L"\n\n";
    successMsg += L"Publisher: " + ext.publisher + L"\n";
    successMsg += L"Version: " + ext.version + L"\n";
    successMsg += L"Location: " + installPath + L"\n";
    
    if (hasEntryPoint) {
        successMsg += L"\nExtension activated successfully!";
    } else {
        successMsg += L"\nNote: Extension files extracted but no entry point found.\n";
        successMsg += L"Some extensions may require IDE restart.";
    }
    
    MessageBoxW(hwnd_, successMsg.c_str(), 
                L"Extension Installed", MB_OK | MB_ICONINFORMATION);
    
    // Refresh marketplace to show [INSTALLED] tag
    SearchMarketplace(L"");
}

void IDEWindow::LoadInstalledExtensions() {
    installedExtensions_.clear();
    
    if (extensionsPath_.empty()) return;
    
    // Scan extensions directory
    WIN32_FIND_DATAW findData;
    std::wstring searchPath = extensionsPath_ + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::wstring dirName = findData.cFileName;
            if (dirName != L"." && dirName != L"..") {
                std::wstring extPath = extensionsPath_ + L"\\" + dirName;
                
                // Check for extension metadata
                std::wstring metaPath = extPath + L"\\extension.meta";
                if (GetFileAttributesW(metaPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    // Try to read package.json
                    std::wstring packagePath = extPath + L"\\extension\\package.json";
                    if (GetFileAttributesW(packagePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
                        packagePath = extPath + L"\\package.json";
                    }
                    
                    HANDLE hPackage = CreateFileW(packagePath.c_str(), GENERIC_READ, 
                        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                    
                    if (hPackage != INVALID_HANDLE_VALUE) {
                        DWORD fileSize = GetFileSize(hPackage, nullptr);
                        if (fileSize > 0 && fileSize < 1024 * 1024) {
                            std::vector<char> buffer(fileSize + 1, 0);
                            DWORD bytesRead = 0;
                            ReadFile(hPackage, buffer.data(), fileSize, &bytesRead, nullptr);
                            std::string jsonContent(buffer.data(), bytesRead);
                            
                            // Parse basic info from JSON
                            ExtensionInfo ext;
                            ext.id = dirName;
                            ext.installed = true;
                            ext.installPath = extPath;
                            
                            // Extract name
                            size_t namePos = jsonContent.find("\\\"name\\\":");
                            if (namePos != std::string::npos) {
                                size_t nameStart = jsonContent.find("\\\"", namePos + 8) + 1;
                                size_t nameEnd = jsonContent.find("\\\"", nameStart);
                                if (nameEnd != std::string::npos) {
                                    std::string name = jsonContent.substr(nameStart, nameEnd - nameStart);
                                    ext.name = std::wstring(name.begin(), name.end());
                                }
                            }
                            
                            // Extract version
                            size_t verPos = jsonContent.find("\\\"version\\\":");
                            if (verPos != std::string::npos) {
                                size_t verStart = jsonContent.find("\\\"", verPos + 11) + 1;
                                size_t verEnd = jsonContent.find("\\\"", verStart);
                                if (verEnd != std::string::npos) {
                                    std::string ver = jsonContent.substr(verStart, verEnd - verStart);
                                    ext.version = std::wstring(ver.begin(), ver.end());
                                }
                            }
                            
                            // Extract publisher
                            size_t pubPos = jsonContent.find("\\\"publisher\\\":");
                            if (pubPos != std::string::npos) {
                                size_t pubStart = jsonContent.find("\\\"", pubPos + 13) + 1;
                                size_t pubEnd = jsonContent.find("\\\"", pubStart);
                                if (pubEnd != std::string::npos) {
                                    std::string pub = jsonContent.substr(pubStart, pubEnd - pubStart);
                                    ext.publisher = std::wstring(pub.begin(), pub.end());
                                }
                            }
                            
                            // Extract description
                            size_t descPos = jsonContent.find("\\\"description\\\":");
                            if (descPos != std::string::npos) {
                                size_t descStart = jsonContent.find("\\\"", descPos + 15) + 1;
                                size_t descEnd = jsonContent.find("\\\"", descStart);
                                if (descEnd != std::string::npos) {
                                    std::string desc = jsonContent.substr(descStart, descEnd - descStart);
                                    ext.description = std::wstring(desc.begin(), desc.end());
                                }
                            }
                            
                            if (!ext.name.empty()) {
                                installedExtensions_.push_back(ext);
                            }
                        }
                        CloseHandle(hPackage);
                    }
                }
            }
        }
    } while (FindNextFileW(hFind, &findData));
    
    FindClose(hFind);
}

void IDEWindow::UninstallExtension(const ExtensionInfo& ext) {
    std::wstring installPath = extensionsPath_ + L"\\" + ext.id;
    
    // Use SHFileOperation for proper recursive delete
    SHFILEOPSTRUCTW fileOp = {0};
    fileOp.wFunc = FO_DELETE;
    fileOp.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;
    
    // Path must be double-null terminated
    std::wstring path = installPath + L'\0';
    fileOp.pFrom = path.c_str();
    
    int result = SHFileOperationW(&fileOp);
    
    if (result == 0 && !fileOp.fAnyOperationsAborted) {
        // Remove from installed list
        auto it = std::find_if(installedExtensions_.begin(), installedExtensions_.end(),
            [&ext](const ExtensionInfo& e) { return e.id == ext.id; });
        if (it != installedExtensions_.end()) {
            installedExtensions_.erase(it);
        }
        
        MessageBoxW(hwnd_, (L"Successfully uninstalled " + ext.name + L"\n\nLocation: " + installPath).c_str(), 
                    L"Extension Uninstalled", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(hwnd_, (L"Failed to uninstall " + ext.name + L"\n\nThe extension directory may be in use.").c_str(), 
                    L"Uninstall Error", MB_OK | MB_ICONERROR);
    }
    
    SearchMarketplace(L"");
}

std::vector<IDEWindow::ExtensionInfo> IDEWindow::QueryVSCodeMarketplace(const std::wstring& query) {
    std::vector<ExtensionInfo> results;
    
    // Prepare JSON request for VS Code marketplace
    std::string searchTerm = query.empty() ? "" : std::string(query.begin(), query.end());
    std::string jsonRequest = R"({"filters":[{"criteria":[{"filterType":8,"value":"Microsoft.VisualStudio.Code"})"
        + (searchTerm.empty() ? "" : ",{\"filterType\":10,\"value\":\"" + searchTerm + "\"}")
        + R"(],"pageSize":50}],"flags":914})";
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return results;
    
    HINTERNET hConnect = WinHttpConnect(hSession,
        L"marketplace.visualstudio.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return results;
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"POST",
        L"/_apis/public/gallery/extensionquery",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return results;
    }
    
    // Set headers
    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json;api-version=7.2-preview.1";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    // Send request
    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)jsonRequest.c_str(), (DWORD)jsonRequest.length(),
        (DWORD)jsonRequest.length(), 0);
    
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, nullptr);
    
    if (bResults) {
        std::string response;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize + 1, 0);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
            
            response.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);
        
        // Parse JSON response (simplified parsing - extract extension info)
        // Look for extension entries in the response
        size_t pos = 0;
        while ((pos = response.find("\"extensionId\":", pos)) != std::string::npos) {
            ExtensionInfo ext;
            
            // Extract extensionId
            size_t idStart = response.find("\"", pos + 15) + 1;
            size_t idEnd = response.find("\"", idStart);
            if (idEnd != std::string::npos) {
                std::string extId = response.substr(idStart, idEnd - idStart);
                ext.id = std::wstring(extId.begin(), extId.end());
            }
            
            // Extract displayName
            size_t namePos = response.find("\"displayName\":", pos);
            if (namePos != std::string::npos) {
                size_t nameStart = response.find("\"", namePos + 15) + 1;
                size_t nameEnd = response.find("\"", nameStart);
                if (nameEnd != std::string::npos) {
                    std::string name = response.substr(nameStart, nameEnd - nameStart);
                    ext.name = std::wstring(name.begin(), name.end());
                }
            }
            
            // Extract publisher
            size_t pubPos = response.find("\"publisherName\":", pos);
            if (pubPos != std::string::npos && pubPos < pos + 1000) {
                size_t pubStart = response.find("\"", pubPos + 17) + 1;
                size_t pubEnd = response.find("\"", pubStart);
                if (pubEnd != std::string::npos) {
                    std::string pub = response.substr(pubStart, pubEnd - pubStart);
                    ext.publisher = std::wstring(pub.begin(), pub.end());
                }
            }
            
            // Extract version
            size_t verPos = response.find("\"version\":", pos);
            if (verPos != std::string::npos && verPos < pos + 1000) {
                size_t verStart = response.find("\"", verPos + 11) + 1;
                size_t verEnd = response.find("\"", verStart);
                if (verEnd != std::string::npos) {
                    std::string ver = response.substr(verStart, verEnd - verStart);
                    ext.version = std::wstring(ver.begin(), ver.end());
                }
            }
            
            // Extract description
            size_t descPos = response.find("\"shortDescription\":", pos);
            if (descPos != std::string::npos && descPos < pos + 2000) {
                size_t descStart = response.find("\"", descPos + 20) + 1;
                size_t descEnd = response.find("\"", descStart);
                if (descEnd != std::string::npos) {
                    std::string desc = response.substr(descStart, descEnd - descStart);
                    ext.description = std::wstring(desc.begin(), desc.end());
                }
            }
            
            // Extract statistics (downloads, rating)
            size_t statsPos = response.find("\"statistics\":", pos);
            if (statsPos != std::string::npos && statsPos < pos + 3000) {
                size_t installPos = response.find("\"install\":", statsPos);
                if (installPos != std::string::npos && installPos < statsPos + 500) {
                    size_t installStart = installPos + 11;
                    size_t installEnd = response.find_first_of(",}", installStart);
                    if (installEnd != std::string::npos) {
                        std::string installStr = response.substr(installStart, installEnd - installStart);
                        ext.downloads = std::atoi(installStr.c_str());
                    }
                }
                
                size_t ratingPos = response.find("\"averagerating\":", statsPos);
                if (ratingPos != std::string::npos && ratingPos < statsPos + 500) {
                    size_t ratingStart = ratingPos + 17;
                    size_t ratingEnd = response.find_first_of(",}", ratingStart);
                    if (ratingEnd != std::string::npos) {
                        std::string ratingStr = response.substr(ratingStart, ratingEnd - ratingStart);
                        ext.rating = (float)std::atof(ratingStr.c_str());
                    }
                }
            }
            
            // Build download URL
            if (!ext.publisher.empty() && !ext.id.empty() && !ext.version.empty()) {
                ext.downloadUrl = L"https://marketplace.visualstudio.com/_apis/public/gallery/publishers/"
                    + ext.publisher + L"/vsextensions/" + ext.id + L"/" + ext.version + L"/vspackage";
            }
            
            ext.installed = false;
            
            if (!ext.name.empty()) {
                results.push_back(ext);
            }
            
            pos = idEnd;
            if (results.size() >= 20) break; // Limit results
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return results;
}

std::vector<IDEWindow::ExtensionInfo> IDEWindow::QueryVSMarketplace(const std::wstring& query) {
    std::vector<ExtensionInfo> results;
    
    // Prepare JSON request for Visual Studio marketplace
    std::string searchTerm = query.empty() ? "" : std::string(query.begin(), query.end());
    std::string jsonRequest = R"({"filters":[{"criteria":[{"filterType":8,"value":"Microsoft.VisualStudio.Services"})"
        + (searchTerm.empty() ? "" : ",{\"filterType\":10,\"value\":\"" + searchTerm + "\"}")
        + R"(],"pageSize":30}],"flags":914})";
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return results;
    
    HINTERNET hConnect = WinHttpConnect(hSession,
        L"marketplace.visualstudio.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return results;
    }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect,
        L"POST",
        L"/_apis/public/gallery/extensionquery",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return results;
    }
    
    // Set headers
    std::wstring headers = L"Content-Type: application/json\r\nAccept: application/json;api-version=7.2-preview.1";
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);
    
    // Send request
    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        (LPVOID)jsonRequest.c_str(), (DWORD)jsonRequest.length(),
        (DWORD)jsonRequest.length(), 0);
    
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, nullptr);
    
    if (bResults) {
        std::string response;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize + 1, 0);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
            
            response.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);
        
        // Parse JSON response
        size_t pos = 0;
        while ((pos = response.find("\"extensionId\":", pos)) != std::string::npos) {
            ExtensionInfo ext;
            
            size_t idStart = response.find("\"", pos + 15) + 1;
            size_t idEnd = response.find("\"", idStart);
            if (idEnd != std::string::npos) {
                std::string extId = response.substr(idStart, idEnd - idStart);
                ext.id = std::wstring(extId.begin(), extId.end());
            }
            
            size_t namePos = response.find("\"displayName\":", pos);
            if (namePos != std::string::npos) {
                size_t nameStart = response.find("\"", namePos + 15) + 1;
                size_t nameEnd = response.find("\"", nameStart);
                if (nameEnd != std::string::npos) {
                    std::string name = response.substr(nameStart, nameEnd - nameStart);
                    ext.name = std::wstring(name.begin(), name.end());
                }
            }
            
            size_t pubPos = response.find("\"publisherName\":", pos);
            if (pubPos != std::string::npos && pubPos < pos + 1000) {
                size_t pubStart = response.find("\"", pubPos + 17) + 1;
                size_t pubEnd = response.find("\"", pubStart);
                if (pubEnd != std::string::npos) {
                    std::string pub = response.substr(pubStart, pubEnd - pubStart);
                    ext.publisher = std::wstring(pub.begin(), pub.end());
                }
            }
            
            size_t verPos = response.find("\"version\":", pos);
            if (verPos != std::string::npos && verPos < pos + 1000) {
                size_t verStart = response.find("\"", verPos + 11) + 1;
                size_t verEnd = response.find("\"", verStart);
                if (verEnd != std::string::npos) {
                    std::string ver = response.substr(verStart, verEnd - verStart);
                    ext.version = std::wstring(ver.begin(), ver.end());
                }
            }
            
            size_t descPos = response.find("\"shortDescription\":", pos);
            if (descPos != std::string::npos && descPos < pos + 2000) {
                size_t descStart = response.find("\"", descPos + 20) + 1;
                size_t descEnd = response.find("\"", descStart);
                if (descEnd != std::string::npos) {
                    std::string desc = response.substr(descStart, descEnd - descStart);
                    ext.description = std::wstring(desc.begin(), desc.end());
                }
            }
            
            size_t statsPos = response.find("\"statistics\":", pos);
            if (statsPos != std::string::npos && statsPos < pos + 3000) {
                size_t installPos = response.find("\"install\":", statsPos);
                if (installPos != std::string::npos && installPos < statsPos + 500) {
                    size_t installStart = installPos + 11;
                    size_t installEnd = response.find_first_of(",}", installStart);
                    if (installEnd != std::string::npos) {
                        std::string installStr = response.substr(installStart, installEnd - installStart);
                        ext.downloads = std::atoi(installStr.c_str());
                    }
                }
                
                size_t ratingPos = response.find("\"averagerating\":", statsPos);
                if (ratingPos != std::string::npos && ratingPos < statsPos + 500) {
                    size_t ratingStart = ratingPos + 17;
                    size_t ratingEnd = response.find_first_of(",}", ratingStart);
                    if (ratingEnd != std::string::npos) {
                        std::string ratingStr = response.substr(ratingStart, ratingEnd - ratingStart);
                        ext.rating = (float)std::atof(ratingStr.c_str());
                    }
                }
            }
            
            if (!ext.publisher.empty() && !ext.id.empty() && !ext.version.empty()) {
                ext.downloadUrl = L"https://marketplace.visualstudio.com/_apis/public/gallery/publishers/"
                    + ext.publisher + L"/vsextensions/" + ext.id + L"/" + ext.version + L"/vspackage";
            }
            
            ext.installed = false;
            
            if (!ext.name.empty()) {
                results.push_back(ext);
            }
            
            pos = idEnd;
            if (results.size() >= 15) break;
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return results;
}

bool IDEWindow::DownloadFile(const std::wstring& url, const std::wstring& destPath) {
    // Parse URL
    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    
    wchar_t hostName[256];
    wchar_t urlPath[2048];
    
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);
    
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &urlComp)) {
        return false;
    }
    
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    
    HINTERNET hConnect = WinHttpConnect(hSession, hostName,
        urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    BOOL bResults = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    
    if (bResults) bResults = WinHttpReceiveResponse(hRequest, nullptr);
    
    HANDLE hFile = INVALID_HANDLE_VALUE;
    if (bResults) {
        hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }
        
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        DWORD dwWritten = 0;
        
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
            
            WriteFile(hFile, buffer.data(), dwDownloaded, &dwWritten, nullptr);
        } while (dwSize > 0);
        
        CloseHandle(hFile);
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return bResults && (hFile != INVALID_HANDLE_VALUE);
}

bool IDEWindow::ExtractVSIX(const std::wstring& vsixPath, const std::wstring& destPath) {
    // VSIX files are ZIP archives - use Windows Shell to extract
    
    CoInitialize(nullptr);
    
    // Create destination directory
    CreateDirectoryW(destPath.c_str(), nullptr);
    
    // Use Shell to extract ZIP
    IShellDispatch* pShellDispatch = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER,
        IID_IShellDispatch, (void**)&pShellDispatch);
    
    if (FAILED(hr) || !pShellDispatch) {
        CoUninitialize();
        return false;
    }
    
    // Get source folder (the VSIX file)
    VARIANT vDir;
    vDir.vt = VT_BSTR;
    vDir.bstrVal = SysAllocString(vsixPath.c_str());
    
    Folder* pZipFolder = nullptr;
    hr = pShellDispatch->NameSpace(vDir, &pZipFolder);
    SysFreeString(vDir.bstrVal);
    
    if (FAILED(hr) || !pZipFolder) {
        pShellDispatch->Release();
        CoUninitialize();
        return false;
    }
    
    // Get destination folder
    vDir.vt = VT_BSTR;
    vDir.bstrVal = SysAllocString(destPath.c_str());
    
    Folder* pDestFolder = nullptr;
    hr = pShellDispatch->NameSpace(vDir, &pDestFolder);
    SysFreeString(vDir.bstrVal);
    
    if (FAILED(hr) || !pDestFolder) {
        pZipFolder->Release();
        pShellDispatch->Release();
        CoUninitialize();
        return false;
    }
    
    // Get items from ZIP
    FolderItems* pItems = nullptr;
    hr = pZipFolder->Items(&pItems);
    
    if (SUCCEEDED(hr) && pItems) {
        // Extract all items (FOF_NO_UI = no progress dialog)
        VARIANT vItems;
        vItems.vt = VT_DISPATCH;
        vItems.pdispVal = pItems;
        
        VARIANT vOptions;
        vOptions.vt = VT_I4;
        vOptions.lVal = 4 | 16 | 512 | 1024; // FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI
        
        hr = pDestFolder->CopyHere(vItems, vOptions);
        
        // Wait for extraction to complete
        Sleep(2000);
        
        pItems->Release();
    }
    
    pDestFolder->Release();
    pZipFolder->Release();
    pShellDispatch->Release();
    
    // Parse extension manifest (extension.vsixmanifest or package.json)
    std::wstring manifestPath = destPath + L"\\extension\\package.json";
    std::wstring vsixManifestPath = destPath + L"\\extension.vsixmanifest";
    
    // Try to read package.json for VS Code extensions
    HANDLE hManifest = CreateFileW(manifestPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (hManifest == INVALID_HANDLE_VALUE) {
        // Try alternative paths
        manifestPath = destPath + L"\\package.json";
        hManifest = CreateFileW(manifestPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    
    if (hManifest != INVALID_HANDLE_VALUE) {
        DWORD fileSize = GetFileSize(hManifest, nullptr);
        if (fileSize > 0 && fileSize < 1024 * 1024) { // Limit to 1MB
            std::vector<char> buffer(fileSize + 1, 0);
            DWORD bytesRead = 0;
            ReadFile(hManifest, buffer.data(), fileSize, &bytesRead, nullptr);
            
            // Create extension metadata file
            std::wstring metaPath = destPath + L"\\extension.meta";
            HANDLE hMeta = CreateFileW(metaPath.c_str(), GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hMeta != INVALID_HANDLE_VALUE) {
                const char* status = "INSTALLED";
                DWORD written;
                WriteFile(hMeta, status, (DWORD)strlen(status), &written, nullptr);
                CloseHandle(hMeta);
            }
        }
        CloseHandle(hManifest);
    }
    
    CoUninitialize();
    return SUCCEEDED(hr);
}

