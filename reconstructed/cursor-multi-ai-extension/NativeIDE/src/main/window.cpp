#include "main_window.h"
#include "ide_application.h"
#include "project_manager.h"
#include "git_integration.h"
#include "editor_core.h"
#include "plugin_manager.h"
#include "resource.h"
#include <windowsx.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

constexpr wchar_t WINDOW_CLASS_NAME[] = L"NativeIDEMainWindow";

MainWindow::MainWindow(IDEApplication* app) 
    : m_app(app), m_hWnd(nullptr), m_hEditor(nullptr), m_hProjectTree(nullptr),
      m_hOutputWindow(nullptr), m_hStatusBar(nullptr), m_hToolbar(nullptr),
      m_pD2DFactory(nullptr), m_pRenderTarget(nullptr), m_pDWriteFactory(nullptr),
      m_pTextFormat(nullptr), m_pTextBrush(nullptr), m_pBackgroundBrush(nullptr),
      m_isMaximized(false) {
    
    memset(&m_windowRect, 0, sizeof(m_windowRect));
}

MainWindow::~MainWindow() {
    CleanupDirect2D();
}

bool MainWindow::Create() {
    RegisterWindowClass();
    
    // Calculate window position (centered on screen)
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - IDE_WINDOW_DEFAULT_WIDTH) / 2;
    int y = (screenHeight - IDE_WINDOW_DEFAULT_HEIGHT) / 2;
    
    m_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        WINDOW_CLASS_NAME,
        L"Native IDE",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y,
        IDE_WINDOW_DEFAULT_WIDTH,
        IDE_WINDOW_DEFAULT_HEIGHT,
        nullptr,
        nullptr,
        m_app->GetInstance(),
        this
    );
    
    if (!m_hWnd) {
        return false;
    }
    
    return true;
}

void MainWindow::Show(int nCmdShow) {
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);
}

void MainWindow::Close() {
    if (m_hWnd) {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

void MainWindow::RegisterWindowClass() {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_app->GetInstance();
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    RegisterClassExW(&wc);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pcs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd;
    } else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            OnCreate();
            return 0;
            
        case WM_DESTROY:
            OnDestroy();
            return 0;
            
        case WM_SIZE:
            OnSize(wParam, lParam);
            return 0;
            
        case WM_PAINT:
            OnPaint();
            return 0;
            
        case WM_COMMAND:
            OnCommand(wParam);
            return 0;
            
        case WM_KEYDOWN:
            OnKeyDown(wParam, lParam);
            return 0;
            
        case WM_GETMINMAXINFO: {
            MINMAXINFO* pmmi = reinterpret_cast<MINMAXINFO*>(lParam);
            pmmi->ptMinTrackSize.x = IDE_WINDOW_MIN_WIDTH;
            pmmi->ptMinTrackSize.y = IDE_WINDOW_MIN_HEIGHT;
            return 0;
        }
        
        default:
            return DefWindowProcW(m_hWnd, uMsg, wParam, lParam);
    }
}

void MainWindow::OnCreate() {
    // Initialize Direct2D
    if (!InitializeDirect2D()) {
        MessageBoxW(m_hWnd, L"Failed to initialize Direct2D", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Initialize core IDE components
    if (!InitializeComponents()) {
        MessageBoxW(m_hWnd, L"Failed to initialize IDE components", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Create menu bar
    CreateMenuBar();
    
    // Create toolbar
    CreateToolbar();
    
    // Create status bar
    CreateStatusBar();
    
    // Create child windows
    CreateChildWindows();
    
    // Layout child windows
    LayoutChildWindows();
    
    // Set initial window title
    UpdateTitle();
}

void MainWindow::OnDestroy() {
    CleanupDirect2D();
    PostQuitMessage(0);
}

void MainWindow::OnSize(WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(wParam);
    
    if (m_pRenderTarget) {
        D2D1_SIZE_U size = D2D1::SizeU(LOWORD(lParam), HIWORD(lParam));
        m_pRenderTarget->Resize(size);
    }
    
    // Update status bar
    if (m_hStatusBar) {
        SendMessage(m_hStatusBar, WM_SIZE, 0, 0);
    }
    
    // Layout child windows
    LayoutChildWindows();
}

void MainWindow::OnPaint() {
    PAINTSTRUCT ps;
    BeginPaint(m_hWnd, &ps);
    
    if (m_pRenderTarget) {
        m_pRenderTarget->BeginDraw();
        m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
        
        // TODO: Custom drawing code here
        
        HRESULT hr = m_pRenderTarget->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) {
            CleanupDirect2D();
            InitializeDirect2D();
        }
    }
    
    EndPaint(m_hWnd, &ps);
}

void MainWindow::OnCommand(WPARAM wParam) {
    int menuId = LOWORD(wParam);
    OnMenuCommand(menuId);
}

void MainWindow::OnMenuCommand(int menuId) {
    switch (menuId) {
        case IDM_FILE_NEW:
            // TODO: New file
            break;
            
        case IDM_FILE_OPEN:
            // TODO: Open file dialog
            break;
            
        case IDM_FILE_SAVE:
            m_app->SaveFile();
            break;
            
        case IDM_FILE_SAVE_AS:
            // TODO: Save as dialog
            break;
            
        case IDM_FILE_EXIT:
            Close();
            break;
            
        case IDM_BUILD_COMPILE:
            {
                auto result = m_app->CompileCurrentFile();
                AppendOutput(result.output + result.errors);
            }
            break;
            
        case IDM_BUILD_BUILD:
            {
                auto result = m_app->BuildProject();
                AppendOutput(result.output + result.errors);
            }
            break;
            
        case IDM_BUILD_RUN:
            // TODO: Run executable
            break;
            
        case IDM_HELP_ABOUT:
            MessageBoxW(m_hWnd, L"Native IDE v1.0\\nPortable C/C++ Development Environment", 
                       L"About Native IDE", MB_OK | MB_ICONINFORMATION);
            break;
    }
}

void MainWindow::OnKeyDown(WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    
    // Handle keyboard shortcuts
    bool ctrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    
    if (ctrlPressed) {
        switch (wParam) {
            case 'N':
                OnMenuCommand(IDM_FILE_NEW);
                break;
            case 'O':
                OnMenuCommand(IDM_FILE_OPEN);
                break;
            case 'S':
                OnMenuCommand(IDM_FILE_SAVE);
                break;
            case VK_F5:
                OnMenuCommand(IDM_BUILD_RUN);
                break;
        }
    } else {
        switch (wParam) {
            case VK_F5:
                OnMenuCommand(IDM_BUILD_BUILD);
                break;
            case VK_F7:
                OnMenuCommand(IDM_BUILD_COMPILE);
                break;
        }
    }
}

bool MainWindow::InitializeDirect2D() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (FAILED(hr)) return false;
    
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(&m_pDWriteFactory));
    if (FAILED(hr)) return false;
    
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
    
    hr = m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hWnd, size),
        &m_pRenderTarget);
    if (FAILED(hr)) return false;
    
    hr = m_pDWriteFactory->CreateTextFormat(
        L"Consolas",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"",
        &m_pTextFormat);
    if (FAILED(hr)) return false;
    
    hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &m_pTextBrush);
    if (FAILED(hr)) return false;
    
    hr = m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_pBackgroundBrush);
    if (FAILED(hr)) return false;
    
    return true;
}

void MainWindow::CleanupDirect2D() {
    if (m_pBackgroundBrush) { m_pBackgroundBrush->Release(); m_pBackgroundBrush = nullptr; }
    if (m_pTextBrush) { m_pTextBrush->Release(); m_pTextBrush = nullptr; }
    if (m_pTextFormat) { m_pTextFormat->Release(); m_pTextFormat = nullptr; }
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
    if (m_pDWriteFactory) { m_pDWriteFactory->Release(); m_pDWriteFactory = nullptr; }
    if (m_pD2DFactory) { m_pD2DFactory->Release(); m_pD2DFactory = nullptr; }
}

bool MainWindow::CreateChildWindows() {
    // Create editor window (main text editor)
    m_hEditor = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_WANTRETURN,
        0, 0, 0, 0,
        m_hWnd,
        reinterpret_cast<HMENU>(IDC_EDITOR),
        m_app->GetInstance(),
        nullptr
    );
    
    // Create project tree view
    m_hProjectTree = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEW,
        L"",
        WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        0, 0, 0, 0,
        m_hWnd,
        reinterpret_cast<HMENU>(IDC_PROJECT_TREE),
        m_app->GetInstance(),
        nullptr
    );
    
    // Create output window
    m_hOutputWindow = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        0, 0, 0, 0,
        m_hWnd,
        reinterpret_cast<HMENU>(IDC_OUTPUT_WINDOW),
        m_app->GetInstance(),
        nullptr
    );
    
    return m_hEditor && m_hProjectTree && m_hOutputWindow;
}

void MainWindow::CreateMenuBar() {
    HMENU hMenubar = CreateMenu();
    
    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, L"&New\\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open...\\tCtrl+O");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save\\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE_AS, L"Save &As...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
    AppendMenuW(hMenubar, MF_POPUP, reinterpret_cast<UINT_PTR>(hFileMenu), L"&File");
    
    // Edit menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_UNDO, L"&Undo\\tCtrl+Z");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REDO, L"&Redo\\tCtrl+Y");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CUT, L"Cu&t\\tCtrl+X");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY, L"&Copy\\tCtrl+C");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE, L"&Paste\\tCtrl+V");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND, L"&Find...\\tCtrl+F");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REPLACE, L"&Replace...\\tCtrl+H");
    AppendMenuW(hMenubar, MF_POPUP, reinterpret_cast<UINT_PTR>(hEditMenu), L"&Edit");
    
    // Build menu
    HMENU hBuildMenu = CreatePopupMenu();
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_COMPILE, L"&Compile\\tF7");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_BUILD, L"&Build\\tF5");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_RUN, L"&Run\\tCtrl+F5");
    AppendMenuW(hBuildMenu, MF_STRING, IDM_BUILD_DEBUG, L"&Debug\\tF9");
    AppendMenuW(hMenubar, MF_POPUP, reinterpret_cast<UINT_PTR>(hBuildMenu), L"&Build");
    
    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About...");
    AppendMenuW(hMenubar, MF_POPUP, reinterpret_cast<UINT_PTR>(hHelpMenu), L"&Help");
    
    SetMenu(m_hWnd, hMenubar);
}

void MainWindow::CreateToolbar() {
    m_hToolbar = CreateWindowExW(
        0,
        TOOLBARCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
        0, 0, 0, 0,
        m_hWnd,
        reinterpret_cast<HMENU>(IDC_TOOLBAR),
        m_app->GetInstance(),
        nullptr
    );
    
    if (m_hToolbar) {
        SendMessage(m_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
        
        // TODO: Add toolbar buttons
        TBBUTTON buttons[] = {
            // Add toolbar button definitions here
        };
        
        SendMessage(m_hToolbar, TB_ADDBUTTONS, 0, reinterpret_cast<LPARAM>(buttons));
        SendMessage(m_hToolbar, TB_AUTOSIZE, 0, 0);
    }
}

void MainWindow::CreateStatusBar() {
    m_hStatusBar = CreateWindowExW(
        0,
        STATUSCLASSNAMEW,
        nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hWnd,
        reinterpret_cast<HMENU>(IDC_STATUS_BAR),
        m_app->GetInstance(),
        nullptr
    );
    
    if (m_hStatusBar) {
        // Set status bar parts
        int parts[] = { 200, 400, -1 };
        SendMessage(m_hStatusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));
        
        // Set initial text
        UpdateStatusBar(L"Ready");
    }
}

void MainWindow::LayoutChildWindows() {
    RECT clientRect;
    GetClientRect(m_hWnd, &clientRect);
    
    // Get toolbar and status bar heights
    int toolbarHeight = 0;
    int statusBarHeight = 0;
    
    if (m_hToolbar) {
        RECT toolbarRect;
        GetWindowRect(m_hToolbar, &toolbarRect);
        toolbarHeight = toolbarRect.bottom - toolbarRect.top;
    }
    
    if (m_hStatusBar) {
        RECT statusRect;
        GetWindowRect(m_hStatusBar, &statusRect);
        statusBarHeight = statusRect.bottom - statusRect.top;
    }
    
    int availableHeight = clientRect.bottom - toolbarHeight - statusBarHeight;
    int treeWidth = 250;
    int outputHeight = 150;
    
    // Layout project tree (left panel)
    if (m_hProjectTree) {
        SetWindowPos(m_hProjectTree, nullptr,
                    0, toolbarHeight,
                    treeWidth, availableHeight - outputHeight,
                    SWP_NOZORDER);
    }
    
    // Layout editor (main area)
    if (m_hEditor) {
        SetWindowPos(m_hEditor, nullptr,
                    treeWidth, toolbarHeight,
                    clientRect.right - treeWidth, availableHeight - outputHeight,
                    SWP_NOZORDER);
    }
    
    // Layout output window (bottom panel)
    if (m_hOutputWindow) {
        SetWindowPos(m_hOutputWindow, nullptr,
                    0, toolbarHeight + availableHeight - outputHeight,
                    clientRect.right, outputHeight,
                    SWP_NOZORDER);
    }
}

void MainWindow::UpdateTitle(const std::wstring& title) {
    std::wstring windowTitle = L"Native IDE";
    if (!title.empty()) {
        windowTitle += L" - " + title;
    }
    SetWindowTextW(m_hWnd, windowTitle.c_str());
}

void MainWindow::UpdateStatusBar(const std::wstring& text) {
    if (m_hStatusBar) {
        SendMessageW(m_hStatusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(text.c_str()));
    }
}

void MainWindow::RefreshProjectTree() {
    if (m_hProjectTree) {
        // TODO: Refresh project tree view
        TreeView_DeleteAllItems(m_hProjectTree);
        
        // Add project items
        // This would be implemented with actual project data
    }
}

void MainWindow::AppendOutput(const std::string& text) {
    if (m_hOutputWindow) {
        std::wstring wtext = IDEUtils::StringToWString(text);
        
        // Get current text length
        int length = GetWindowTextLength(m_hOutputWindow);
        
        // Move to end and append new text
        SendMessage(m_hOutputWindow, EM_SETSEL, length, length);
        SendMessageW(m_hOutputWindow, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wtext.c_str()));
        
        // Scroll to bottom
        SendMessage(m_hOutputWindow, EM_SCROLLCARET, 0, 0);
    }
}

bool MainWindow::InitializeComponents() {
    // Initialize editor core
    m_editorCore = std::make_unique<NativeIDE::EditorCore>();
    if (!m_editorCore->Initialize()) {
        return false;
    }
    
    // Initialize project manager
    m_projectManager = std::make_unique<NativeIDE::ProjectManager>();
    m_projectManager->SetOnFileChanged([this](const std::wstring& eventType, const std::wstring& filePath) {
        HandleFileEvent(eventType, filePath);
    });
    
    // Initialize Git integration
    m_gitIntegration = std::make_unique<NativeIDE::GitIntegration>();
    m_gitIntegration->SetOnStatusChanged([this](const NativeIDE::GitStatus& status) {
        HandleGitStatusChanged(status);
    });
    
    // Initialize plugin manager
    m_pluginManager = std::make_unique<NativeIDE::PluginManager>();
    
    // Load plugins from plugins directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring pluginsDir = std::filesystem::path(exePath).parent_path() / L"plugins";
    m_pluginManager->LoadPluginsFromDirectory(pluginsDir);
    
    return true;
}

void MainWindow::HandleFileEvent(const std::wstring& eventType, const std::wstring& filePath) {
    // Update project tree view
    RefreshProjectTree();
    
    // Notify plugins
    if (m_pluginManager) {
        if (eventType == L"added") {
            m_pluginManager->NotifyFileCreated(filePath);
        } else if (eventType == L"removed") {
            m_pluginManager->NotifyFileDeleted(filePath);
        } else if (eventType == L"changed") {
            m_pluginManager->NotifyFileModified(filePath);
        }
    }
    
    // Update status bar
    UpdateStatusBar(L"File " + eventType + L": " + std::filesystem::path(filePath).filename().wstring());
}

void MainWindow::HandleGitStatusChanged(const NativeIDE::GitStatus& status) {
    // Update window title with Git branch info
    std::wstring title = L"Native IDE";
    if (!status.branch.empty()) {
        title += L" [" + status.branch + L"]";
        if (!status.isClean) {
            title += L" *";
        }
    }
    UpdateTitle(title);
    
    // Update status bar with Git info
    std::wstring statusText;
    if (!status.branch.empty()) {
        statusText = L"Branch: " + status.branch;
        if (status.aheadBy > 0 || status.behindBy > 0) {
            statusText += L" (";
            if (status.aheadBy > 0) statusText += L"+" + std::to_wstring(status.aheadBy);
            if (status.behindBy > 0) statusText += L"-" + std::to_wstring(status.behindBy);
            statusText += L")";
        }
    }
    UpdateStatusBar(statusText);
    
    // Notify plugins
    if (m_pluginManager) {
        m_pluginManager->NotifyProjectChanged(m_projectManager->GetProjectPath());
    }
}

void MainWindow::ClearOutput() {
    if (m_hOutputWindow) {
        SetWindowTextW(m_hOutputWindow, L"");
    }
}

void MainWindow::CreateScintilla() {
    m_scintilla = CreateWindowEx(
        0, L"Scintilla", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
        0, 0, 0, 0, m_hWnd, (HMENU)IDC_SCINTILLA, GetModuleHandle(nullptr), nullptr
    );
}