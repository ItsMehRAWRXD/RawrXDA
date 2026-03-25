#include "production_agentic_ide.h"
<<<<<<< HEAD
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <sstream>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

// Static member initialization
const wchar_t* ProductionAgenticIDE::CLASS_NAME = L"ProductionAgenticIDEWindow";
const wchar_t* ProductionAgenticIDE::WINDOW_TITLE = L"RawrXD - Production Agentic IDE";

// Constructor
ProductionAgenticIDE::ProductionAgenticIDE()
    : m_hWnd(NULL), m_hStatusBar(NULL), m_hToolBar(NULL),
      m_hMainContainer(NULL), m_hSidebar(NULL), m_hTerminal(NULL),
      m_pCurrentPanel(NULL), m_pPaintPanel(NULL), m_pCodePanel(NULL), m_pChatPanel(NULL),
      m_zoomLevel(100), m_fullscreen(false), m_sidebarVisible(true),
      m_terminalVisible(true), m_findDialogOpen(false), m_currentPanelIndex(0),
      m_hInstance(GetModuleHandle(NULL)), m_hFindDialog(NULL), m_hReplaceDialog(NULL) {
}

// Destructor
ProductionAgenticIDE::~ProductionAgenticIDE() {
    CloseAllPanels();
    if (m_hWnd && IsWindow(m_hWnd)) {
        DestroyWindow(m_hWnd);
    }
}

// Create main window
HWND ProductionAgenticIDE::Create() {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) {
        return NULL;
    }
    
    m_hWnd = CreateWindowEx(
        WS_EX_ACCEPTFILES,
        CLASS_NAME,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1400, 900,
        NULL, NULL, m_hInstance, (LPVOID)this
    );
    
    if (!m_hWnd) {
        return NULL;
    }
    
    InitCommonControls();
    
    CreateMenuBar();
    CreateToolBar();
    CreateStatusBar();
    CreatePanelContainer();
    
    ShowWindow(m_hWnd, SW_SHOW);
    UpdateWindow(m_hWnd);
    
    return m_hWnd;
}

// Main message loop
int ProductionAgenticIDE::Run() {
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessage(m_hFindDialog, &msg) &&
            !IsDialogMessage(m_hReplaceDialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

// Window procedure
LRESULT CALLBACK ProductionAgenticIDE::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProductionAgenticIDE* pThis = NULL;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<ProductionAgenticIDE*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hWnd = hwnd;
        return pThis->OnCreate();
    } else {
        pThis = reinterpret_cast<ProductionAgenticIDE*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (!pThis) {
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    switch (msg) {
        case WM_SIZE:
            pThis->OnSize(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return 0;
        case WM_COMMAND:
            return pThis->OnCommand(wParam, lParam);
        case WM_PAINT:
            pThis->OnPaint();
            return 0;
        case WM_DESTROY:
            return pThis->OnDestroy();
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Child window procedure
LRESULT CALLBACK ProductionAgenticIDE::ChildWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Create menu bar
void ProductionAgenticIDE::CreateMenuBar() {
    HMENU hMenuBar = CreateMenu();
    
    // File Menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenu(hFileMenu, MFT_STRING, 1001, L"&New Paint\tCtrl+Shift+P");
    AppendMenu(hFileMenu, MFT_STRING, 1002, L"&New Code\tCtrl+Shift+C");
    AppendMenu(hFileMenu, MFT_STRING, 1003, L"&New Chat\tCtrl+Shift+H");
    AppendMenu(hFileMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MFT_STRING, 1004, L"&Open...\tCtrl+O");
    AppendMenu(hFileMenu, MFT_STRING, 1005, L"&Save\tCtrl+S");
    AppendMenu(hFileMenu, MFT_STRING, 1006, L"Save &As...\tCtrl+Shift+S");
    AppendMenu(hFileMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MFT_STRING, 1007, L"&Close\tCtrl+W");
    AppendMenu(hFileMenu, MFT_STRING, 1008, L"Close &All\tCtrl+Shift+W");
    AppendMenu(hFileMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hFileMenu, MFT_STRING, 1009, L"E&xit\tAlt+F4");
    AppendMenu(hMenuBar, MFT_POPUP, (UINT_PTR)hFileMenu, L"&File");
    
    // Edit Menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenu(hEditMenu, MFT_STRING, 2001, L"&Undo\tCtrl+Z");
    AppendMenu(hEditMenu, MFT_STRING, 2002, L"&Redo\tCtrl+Y");
    AppendMenu(hEditMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MFT_STRING, 2003, L"Cu&t\tCtrl+X");
    AppendMenu(hEditMenu, MFT_STRING, 2004, L"&Copy\tCtrl+C");
    AppendMenu(hEditMenu, MFT_STRING, 2005, L"&Paste\tCtrl+V");
    AppendMenu(hEditMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MFT_STRING, 2006, L"Select &All\tCtrl+A");
    AppendMenu(hEditMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hEditMenu, MFT_STRING, 2007, L"&Find\tCtrl+F");
    AppendMenu(hEditMenu, MFT_STRING, 2008, L"&Replace\tCtrl+H");
    AppendMenu(hMenuBar, MFT_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    
    // View Menu
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenu(hViewMenu, MFT_STRING, 3001, L"Toggle &Sidebar\tCtrl+B");
    AppendMenu(hViewMenu, MFT_STRING, 3002, L"Toggle &Terminal\tCtrl+`");
    AppendMenu(hViewMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hViewMenu, MFT_STRING, 3003, L"Zoom &In\tCtrl++");
    AppendMenu(hViewMenu, MFT_STRING, 3004, L"Zoom &Out\tCtrl+-");
    AppendMenu(hViewMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hViewMenu, MFT_STRING, 3005, L"&Reset Layout\tF5");
    AppendMenu(hViewMenu, MFT_STRING, 3006, L"&Fullscreen\tF11");
    AppendMenu(hMenuBar, MFT_POPUP, (UINT_PTR)hViewMenu, L"&View");
    
    // Panel Menu
    HMENU hPanelMenu = CreatePopupMenu();
    AppendMenu(hPanelMenu, MFT_STRING, 4001, L"Next &Panel\tCtrl+Tab");
    AppendMenu(hPanelMenu, MFT_STRING, 4002, L"Previous P&anel\tCtrl+Shift+Tab");
    AppendMenu(hPanelMenu, MFT_SEPARATOR, 0, NULL);
    AppendMenu(hPanelMenu, MFT_STRING, 4003, L"Split &Vertical\tCtrl+\\");
    AppendMenu(hPanelMenu, MFT_STRING, 4004, L"Split &Horizontal\tCtrl+Shift+\\");
    AppendMenu(hMenuBar, MFT_POPUP, (UINT_PTR)hPanelMenu, L"&Panels");
    
    SetMenu(m_hWnd, hMenuBar);
}

// Create status bar
void ProductionAgenticIDE::CreateStatusBar() {
    m_hStatusBar = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        L"Ready",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        m_hWnd,
        (HMENU)505,
        m_hInstance,
        NULL
    );
    
    if (m_hStatusBar) {
        int widths[] = { 200, 400, -1 };
        SendMessage(m_hStatusBar, SB_SETPARTS, 3, (LPARAM)widths);
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
    }
}

// Create toolbar
void ProductionAgenticIDE::CreateToolBar() {
    m_hToolBar = CreateWindowEx(
        0,
        TOOLBARCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS,
        0, 0, 0, 0,
        m_hWnd,
        (HMENU)506,
        m_hInstance,
        NULL
    );
}

// Create main panel container
void ProductionAgenticIDE::CreatePanelContainer() {
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    
    int toolbarHeight = 30;
    int statusbarHeight = 20;
    int containerHeight = rc.bottom - rc.top - toolbarHeight - statusbarHeight;
    
    m_hMainContainer = CreateWindowEx(
        0,
        L"STATIC",
        NULL,
        WS_CHILD | WS_VISIBLE,
        0, toolbarHeight, rc.right - rc.left, containerHeight,
        m_hWnd,
        NULL,
        m_hInstance,
        NULL
    );
    
    // Create sidebar
    m_hSidebar = CreateWindowEx(
        0,
        L"LISTBOX",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL,
        0, 0, 200, containerHeight,
        m_hMainContainer,
        NULL,
        m_hInstance,
        NULL
    );
    
    // Create terminal
    m_hTerminal = CreateWindowEx(
        0,
        L"EDIT",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        200, containerHeight - 150, rc.right - rc.left - 200, 150,
        m_hMainContainer,
        NULL,
        m_hInstance,
        NULL
    );
}

// Update layout
void ProductionAgenticIDE::UpdateLayout() {
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    
    int toolbarHeight = 30;
    int statusbarHeight = 20;
    int sidebarWidth = m_sidebarVisible ? 200 : 0;
    int terminalHeight = m_terminalVisible ? 150 : 0;
    
    if (m_hMainContainer) {
        MoveWindow(m_hMainContainer, 0, toolbarHeight,
                   rc.right - rc.left, rc.bottom - rc.top - toolbarHeight - statusbarHeight,
                   TRUE);
    }
    
    if (m_hSidebar) {
        MoveWindow(m_hSidebar, 0, 0, sidebarWidth,
                   rc.bottom - rc.top - toolbarHeight - statusbarHeight - terminalHeight,
                   TRUE);
    }
    
    if (m_hTerminal) {
        MoveWindow(m_hTerminal, sidebarWidth, rc.bottom - rc.top - toolbarHeight - statusbarHeight - terminalHeight,
                   rc.right - rc.left - sidebarWidth, terminalHeight,
                   TRUE);
    }
    
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        MoveWindow(m_pCurrentPanel->hwnd, sidebarWidth, 0,
                   rc.right - rc.left - sidebarWidth, rc.bottom - rc.top - toolbarHeight - statusbarHeight - terminalHeight,
                   TRUE);
    }
}

// Update status bar message
void ProductionAgenticIDE::UpdateStatusBar(const std::wstring& message) {
    if (m_hStatusBar && IsWindow(m_hStatusBar)) {
        SendMessage(m_hStatusBar, SB_SETTEXT, 0, (LPARAM)message.c_str());
    }
}

// Create panel
Panel* ProductionAgenticIDE::CreatePanel(PanelType type, const std::wstring& filename) {
    RECT rc;
    GetClientRect(m_hMainContainer, &rc);
    
    HWND hPanel = CreateWindowEx(
        0,
        (type == PanelType::Paint) ? L"STATIC" : L"EDIT",
        NULL,
        (type == PanelType::Paint) ? WS_CHILD | WS_VISIBLE :
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
        200, 0, rc.right - rc.left - 200, rc.bottom - rc.top - 150,
        m_hMainContainer,
        NULL,
        m_hInstance,
        NULL
    );
    
    if (!hPanel) return NULL;
    
    Panel* pPanel = new Panel(type, hPanel);
    pPanel->filename = filename.empty() ? L"Untitled" : filename;
    
    m_panels.push_back(std::unique_ptr<Panel>(pPanel));
    ActivatePanel(pPanel);
    
    switch (type) {
        case PanelType::Paint:
            m_pPaintPanel = pPanel;
            UpdateStatusBar(L"Paint panel created");
            break;
        case PanelType::Code:
            m_pCodePanel = pPanel;
            UpdateStatusBar(L"Code panel created");
            break;
        case PanelType::Chat:
            m_pChatPanel = pPanel;
            UpdateStatusBar(L"Chat panel created");
            break;
        default:
            break;
    }
    
    return pPanel;
}

// Close panel
void ProductionAgenticIDE::ClosePanel(Panel* panel) {
    if (!panel || !IsWindow(panel->hwnd)) return;
    
    if (panel->modified) {
        int result = MessageBox(m_hWnd, L"Save changes before closing?", L"Unsaved Changes",
                               MB_YESNOCANCEL | MB_ICONQUESTION);
        if (result == IDCANCEL) return;
        if (result == IDYES) SavePanelContent(panel);
    }
    
    DestroyWindow(panel->hwnd);
    
    if (panel == m_pPaintPanel) m_pPaintPanel = NULL;
    if (panel == m_pCodePanel) m_pCodePanel = NULL;
    if (panel == m_pChatPanel) m_pChatPanel = NULL;
    if (panel == m_pCurrentPanel) m_pCurrentPanel = NULL;
    
    auto it = std::find_if(m_panels.begin(), m_panels.end(),
                          [panel](const std::unique_ptr<Panel>& p) { return p.get() == panel; });
    if (it != m_panels.end()) {
        m_panels.erase(it);
    }
    
    UpdateStatusBar(L"Panel closed");
}

// Close all panels
void ProductionAgenticIDE::CloseAllPanels() {
    std::vector<Panel*> toClose;
    for (auto& p : m_panels) {
        toClose.push_back(p.get());
    }
    
    for (Panel* p : toClose) {
        ClosePanel(p);
    }
}

// Activate panel
void ProductionAgenticIDE::ActivatePanel(Panel* panel) {
    if (!panel || !IsWindow(panel->hwnd)) return;
    
    m_pCurrentPanel = panel;
    m_currentPanelIndex = 0;
    
    for (size_t i = 0; i < m_panels.size(); i++) {
        if (m_panels[i].get() == panel) {
            m_currentPanelIndex = i;
            break;
        }
    }
    
    SetFocus(panel->hwnd);
}

// Show open file dialog
std::wstring ProductionAgenticIDE::ShowOpenFileDialog() {
    wchar_t filename[MAX_PATH] = L"";
    
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"All Files (*.*)\0*.*\0C++ Files (*.cpp)\0*.cpp\0Header Files (*.h)\0*.h\0\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
    
    if (GetOpenFileName(&ofn)) {
        return std::wstring(filename);
    }
    return L"";
}

// Show save file dialog
std::wstring ProductionAgenticIDE::ShowSaveFileDialog(const std::wstring& defaultName) {
    wchar_t filename[MAX_PATH] = {};
    if (!defaultName.empty()) {
        wcscpy_s(filename, MAX_PATH, defaultName.c_str());
    }
    
    SAVEFILENAME sfn = {};
    sfn.lStructSize = sizeof(OPENFILENAME);
    sfn.hwndOwner = m_hWnd;
    sfn.lpstrFilter = L"All Files (*.*)\0*.*\0C++ Files (*.cpp)\0*.cpp\0Header Files (*.h)\0*.h\0\0";
    sfn.lpstrFile = filename;
    sfn.nMaxFile = MAX_PATH;
    sfn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    
    if (GetSaveFileName((LPOPENFILENAME)&sfn)) {
        return std::wstring(filename);
    }
    return L"";
}

// Save panel content
bool ProductionAgenticIDE::SavePanelContent(Panel* panel) {
    if (!panel) return false;
    
    std::wstring filename = ShowSaveFileDialog(panel->filename);
    if (filename.empty()) return false;
    
    HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    // Get text from edit control
    int len = GetWindowTextLength(panel->hwnd);
    if (len > 0) {
        wchar_t* buffer = new wchar_t[len + 1];
        GetWindowText(panel->hwnd, buffer, len + 1);
        
        // Convert to UTF-8 and write
        int nSize = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, NULL, 0, NULL, NULL);
        char* utf8Buffer = new char[nSize];
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8Buffer, nSize, NULL, NULL);
        
        DWORD written;
        WriteFile(hFile, utf8Buffer, nSize - 1, &written, NULL);
        
        delete[] buffer;
        delete[] utf8Buffer;
    }
    
    CloseHandle(hFile);
    panel->filename = filename;
    panel->modified = false;
    
    UpdateStatusBar(L"File saved: " + filename);
    return true;
}

// Load file into panel
bool ProductionAgenticIDE::LoadFileIntoPanel(Panel* panel, const std::wstring& filename) {
    if (!panel) return false;
    
    HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ, 0, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize > 0) {
        char* buffer = new char[fileSize + 1];
        DWORD read;
        
        if (ReadFile(hFile, buffer, fileSize, &read, NULL)) {
            buffer[fileSize] = '\0';
            
            // Convert from UTF-8 to UTF-16
            int wsize = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, NULL, 0);
            wchar_t* wbuffer = new wchar_t[wsize];
            MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, wsize);
            
            SetWindowText(panel->hwnd, wbuffer);
            panel->filename = filename;
            panel->modified = false;
            
            UpdateStatusBar(L"File loaded: " + filename);
            
            delete[] wbuffer;
        }
        
        delete[] buffer;
    }
    
    CloseHandle(hFile);
    return true;
}

// Clipboard operations
void ProductionAgenticIDE::CopyToClipboard(const std::wstring& text) {
    if (!OpenClipboard(m_hWnd)) return;
    
    EmptyClipboard();
    
    int size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) {
        CloseClipboard();
        return;
    }
    
    wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
    wcscpy_s(pMem, text.length() + 1, text.c_str());
    GlobalUnlock(hMem);
    
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
}

std::wstring ProductionAgenticIDE::GetFromClipboard() {
    std::wstring result = L"";
    
    if (!OpenClipboard(m_hWnd)) return result;
    
    HANDLE hMem = GetClipboardData(CF_UNICODETEXT);
    if (hMem) {
        wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
        result = std::wstring(pMem);
        GlobalUnlock(hMem);
    }
    
    CloseClipboard();
    return result;
}

// Undo/Redo
void ProductionAgenticIDE::PushUndoState() {
    if (!m_pCurrentPanel || !IsWindow(m_pCurrentPanel->hwnd)) return;
    
    int len = GetWindowTextLength(m_pCurrentPanel->hwnd);
    if (len > 0) {
        wchar_t* buffer = new wchar_t[len + 1];
        GetWindowText(m_pCurrentPanel->hwnd, buffer, len + 1);
        m_undoStack.push_back(std::wstring(buffer));
        delete[] buffer;
        
        if (m_undoStack.size() > MAX_UNDO_DEPTH) {
            m_undoStack.erase(m_undoStack.begin());
        }
    }
}

void ProductionAgenticIDE::PopUndoState() {
    if (m_undoStack.empty()) return;
    
    std::wstring state = m_undoStack.back();
    m_undoStack.pop_back();
    
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SetWindowText(m_pCurrentPanel->hwnd, state.c_str());
        UpdateStatusBar(L"Undo");
    }
}

void ProductionAgenticIDE::PushRedoState() {
    if (!m_pCurrentPanel || !IsWindow(m_pCurrentPanel->hwnd)) return;
    
    int len = GetWindowTextLength(m_pCurrentPanel->hwnd);
    if (len > 0) {
        wchar_t* buffer = new wchar_t[len + 1];
        GetWindowText(m_pCurrentPanel->hwnd, buffer, len + 1);
        m_redoStack.push_back(std::wstring(buffer));
        delete[] buffer;
    }
}

// Message handlers
LRESULT ProductionAgenticIDE::OnCreate() {
    return 0;
}

LRESULT ProductionAgenticIDE::OnSize(int width, int height) {
    if (m_hStatusBar && IsWindow(m_hStatusBar)) {
        SendMessage(m_hStatusBar, WM_SIZE, 0, 0);
    }
    UpdateLayout();
    return 0;
}

void ProductionAgenticIDE::OnPaint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hWnd, &ps);
    FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
    EndPaint(m_hWnd, &ps);
}

LRESULT ProductionAgenticIDE::OnDestroy() {
    PostQuitMessage(0);
    return 0;
}

LRESULT ProductionAgenticIDE::OnCommand(WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);
    
    switch (id) {
        // File Operations
        case 1001: onNewPaint(); break;
        case 1002: onNewCode(); break;
        case 1003: onNewChat(); break;
        case 1004: onOpen(); break;
        case 1005: onSave(); break;
        case 1006: onSaveAs(); break;
        case 1007: onClose(); break;
        case 1008: onCloseAll(); break;
        case 1009: PostMessage(m_hWnd, WM_CLOSE, 0, 0); break;
        
        // Edit Operations
        case 2001: onUndo(); break;
        case 2002: onRedo(); break;
        case 2003: onCut(); break;
        case 2004: onCopy(); break;
        case 2005: onPaste(); break;
        case 2006: onSelectAll(); break;
        case 2007: onFind(); break;
        case 2008: onReplace(); break;
        
        // View Operations
        case 3001: onToggleSidebar(); break;
        case 3002: onToggleTerminal(); break;
        case 3003: onZoomIn(); break;
        case 3004: onZoomOut(); break;
        case 3005: onResetLayout(); break;
        case 3006: onFullscreen(); break;
        
        // Panel Operations
        case 4001: onNextPanel(); break;
        case 4002: onPrevPanel(); break;
        case 4003: onSplitVertical(); break;
        case 4004: onSplitHorizontal(); break;
    }
    
    return 0;
}

// ============================================================================
// FILE OPERATIONS IMPLEMENTATIONS
// ============================================================================

void ProductionAgenticIDE::onNewPaint() {
    Panel* panel = CreatePanel(PanelType::Paint, L"untitled_paint.pnt");
    if (panel) {
        MessageBox(m_hWnd, L"New paint panel created. Start drawing here.", L"Paint Panel", MB_OK | MB_ICONINFORMATION);
=======
ProductionAgenticIDE::ProductionAgenticIDE(void* parent)
    : void(parent) {
    setWindowTitle("RawrXD - Production IDE");
    setGeometry(100, 100, 1200, 800);

    void* centralWidget = new // Widget(this);
    setCentralWidget(centralWidget);
    void* layout = new void(centralWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    void* fileMenu = menuBar()->addMenu(tr("&File"));
    void* newPaint = fileMenu->addAction(tr("New &Paint"));
    void* newCode = fileMenu->addAction(tr("New &Code"));
    void* newChat = fileMenu->addAction(tr("New &Chat"));
    fileMenu->addSeparator();
    void* openAction = fileMenu->addAction(tr("&Open..."));
    void* saveAction = fileMenu->addAction(tr("&Save"));
    void* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    fileMenu->addSeparator();
    void* exportAction = fileMenu->addAction(tr("&Export Image..."));
    fileMenu->addSeparator();
    void* exitAction = fileMenu->addAction(tr("E&xit"));

    void* editMenu = menuBar()->addMenu(tr("&Edit"));
    void* undoAction = editMenu->addAction(tr("&Undo"));
    void* redoAction = editMenu->addAction(tr("&Redo"));
    editMenu->addSeparator();
    void* cutAction = editMenu->addAction(tr("Cu&t"));
    void* copyAction = editMenu->addAction(tr("&Copy"));
    void* pasteAction = editMenu->addAction(tr("&Paste"));

    void* viewMenu = menuBar()->addMenu(tr("&View"));
    void* togglePaint = viewMenu->addAction(tr("Toggle &Paint Panel"));
    void* toggleCode = viewMenu->addAction(tr("Toggle &Code Panel"));
    void* toggleChat = viewMenu->addAction(tr("Toggle C&hat Panel"));
    void* toggleFeatures = viewMenu->addAction(tr("Toggle &Features"));
    viewMenu->addSeparator();
    void* resetLayout = viewMenu->addAction(tr("&Reset Layout"));  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

ProductionAgenticIDE::~ProductionAgenticIDE() = default;

void ProductionAgenticIDE::onNewPaint() {
    std::string filename = // Dialog::getSaveFileName(this, tr("New Paint Document"), std::string(), tr("PNG Files (*.png);;All Files (*)"));
    if (!filename.empty()) {
        statusBar()->showMessage(tr("New paint document created: %1"), 3000);
>>>>>>> origin/main
    }
}

void ProductionAgenticIDE::onNewCode() {
<<<<<<< HEAD
    Panel* panel = CreatePanel(PanelType::Code, L"untitled.cpp");
    if (panel) {
        SetWindowText(panel->hwnd, L"// New C++ code file\n#include <iostream>\n\nint main() {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}");
        UpdateStatusBar(L"New code editor created");
=======
    std::string filename = // Dialog::getSaveFileName(this, tr("New Code File"), std::string(), tr("C++ Files (*.cpp);;Header Files (*.h);;All Files (*)"));
    if (!filename.empty()) {
        statusBar()->showMessage(tr("New code file created: %1"), 3000);
>>>>>>> origin/main
    }
}

void ProductionAgenticIDE::onNewChat() {
<<<<<<< HEAD
    Panel* panel = CreatePanel(PanelType::Chat, L"untitled_chat.txt");
    if (panel) {
        SetWindowText(panel->hwnd, L"[Chat Session Started]\n> ");
        UpdateStatusBar(L"New chat panel created");
    }
}

void ProductionAgenticIDE::onSave() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SavePanelContent(m_pCurrentPanel);
    } else {
        MessageBox(m_hWnd, L"No active panel to save.", L"Save", MB_OK | MB_ICONWARNING);
    }
}

void ProductionAgenticIDE::onSaveAs() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SavePanelContent(m_pCurrentPanel);
    } else {
        MessageBox(m_hWnd, L"No active panel to save.", L"Save As", MB_OK | MB_ICONWARNING);
=======
    bool ok;
    std::string sessionName = void::getText(this, tr("New Chat Session"), tr("Session name:"), voidEdit::Normal, std::string(), &ok);
    if (ok && !sessionName.empty()) {
        statusBar()->showMessage(tr("Chat session created: %1"), 3000);
>>>>>>> origin/main
    }
}

void ProductionAgenticIDE::onOpen() {
<<<<<<< HEAD
    std::wstring filename = ShowOpenFileDialog();
    if (!filename.empty()) {
        Panel* panel = CreatePanel(PanelType::Code, filename);
        if (panel && !LoadFileIntoPanel(panel, filename)) {
            MessageBox(m_hWnd, L"Failed to load file.", L"Error", MB_OK | MB_ICONERROR);
=======
    std::stringList files = // Dialog::getOpenFileNames(this, tr("Open Files"), std::string(), tr("All Files (*)"));
    if (!files.empty()) {
        for (const std::string& file : files) {
            statusBar()->showMessage(tr("Opened: %1"), 2000);
>>>>>>> origin/main
        }
    }
}

<<<<<<< HEAD
void ProductionAgenticIDE::onClose() {
    if (m_pCurrentPanel) {
        ClosePanel(m_pCurrentPanel);
    } else {
        MessageBox(m_hWnd, L"No active panel to close.", L"Close", MB_OK | MB_ICONWARNING);
    }
}

void ProductionAgenticIDE::onCloseAll() {
    if (m_panels.empty()) {
        MessageBox(m_hWnd, L"No panels to close.", L"Close All", MB_OK | MB_ICONINFO);
        return;
    }
    
    int result = MessageBox(m_hWnd, L"Close all panels?", L"Close All", MB_YESNO | MB_ICONQUESTION);
    if (result == IDYES) {
        CloseAllPanels();
    }
}

// ============================================================================
// EDIT OPERATIONS IMPLEMENTATIONS
// ============================================================================

void ProductionAgenticIDE::onUndo() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SendMessage(m_pCurrentPanel->hwnd, EM_UNDO, 0, 0);
        UpdateStatusBar(L"Undo");
    }
}

void ProductionAgenticIDE::onRedo() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        // Most basic controls don't have redo, so we use our redo stack
        if (!m_redoStack.empty()) {
            std::wstring state = m_redoStack.back();
            m_redoStack.pop_back();
            SetWindowText(m_pCurrentPanel->hwnd, state.c_str());
        }
        UpdateStatusBar(L"Redo");
    }
}

void ProductionAgenticIDE::onCut() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SendMessage(m_pCurrentPanel->hwnd, WM_CUT, 0, 0);
        m_pCurrentPanel->modified = true;
        UpdateStatusBar(L"Cut");
    }
}

void ProductionAgenticIDE::onCopy() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SendMessage(m_pCurrentPanel->hwnd, WM_COPY, 0, 0);
        UpdateStatusBar(L"Copy");
    }
}

void ProductionAgenticIDE::onPaste() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SendMessage(m_pCurrentPanel->hwnd, WM_PASTE, 0, 0);
        m_pCurrentPanel->modified = true;
        UpdateStatusBar(L"Paste");
    }
}

void ProductionAgenticIDE::onSelectAll() {
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        SendMessage(m_pCurrentPanel->hwnd, EM_SETSEL, 0, -1);
        UpdateStatusBar(L"All content selected");
    }
}

void ProductionAgenticIDE::onFind() {
    if (m_findDialogOpen) {
        if (m_hFindDialog && IsWindow(m_hFindDialog)) {
            SetFocus(m_hFindDialog);
        }
    } else {
        // Create a simple find toolbar
        MessageBox(m_hWnd, L"Find: Use Ctrl+F shortcut in edit control.", L"Find", MB_OK | MB_ICONINFORMATION);
        UpdateStatusBar(L"Find dialog opened");
    }
}

void ProductionAgenticIDE::onReplace() {
    if (m_hReplaceDialog && IsWindow(m_hReplaceDialog)) {
        SetFocus(m_hReplaceDialog);
    } else {
        MessageBox(m_hWnd, L"Replace: Use Ctrl+H to open replace dialog.", L"Replace", MB_OK | MB_ICONINFORMATION);
        UpdateStatusBar(L"Replace dialog opened");
    }
}

// ============================================================================
// VIEW OPERATIONS IMPLEMENTATIONS
// ============================================================================

void ProductionAgenticIDE::onToggleSidebar() {
    m_sidebarVisible = !m_sidebarVisible;
    
    if (m_hSidebar && IsWindow(m_hSidebar)) {
        ShowWindow(m_hSidebar, m_sidebarVisible ? SW_SHOW : SW_HIDE);
    }
    
    UpdateLayout();
    UpdateStatusBar(m_sidebarVisible ? L"Sidebar shown" : L"Sidebar hidden");
}

void ProductionAgenticIDE::onToggleTerminal() {
    m_terminalVisible = !m_terminalVisible;
    
    if (m_hTerminal && IsWindow(m_hTerminal)) {
        ShowWindow(m_hTerminal, m_terminalVisible ? SW_SHOW : SW_HIDE);
    }
    
    UpdateLayout();
    UpdateStatusBar(m_terminalVisible ? L"Terminal shown" : L"Terminal hidden");
}

void ProductionAgenticIDE::onResetLayout() {
    m_sidebarVisible = true;
    m_terminalVisible = true;
    m_zoomLevel = 100;
    
    if (m_hSidebar && IsWindow(m_hSidebar)) {
        ShowWindow(m_hSidebar, SW_SHOW);
    }
    if (m_hTerminal && IsWindow(m_hTerminal)) {
        ShowWindow(m_hTerminal, SW_SHOW);
    }
    
    UpdateLayout();
    UpdateStatusBar(L"Layout reset to default");
}

void ProductionAgenticIDE::onZoomIn() {
    m_zoomLevel = (m_zoomLevel < 200) ? m_zoomLevel + 10 : 200;
    
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        // Change font size
        HFONT hFont = (HFONT)SendMessage(m_pCurrentPanel->hwnd, WM_GETFONT, 0, 0);
        if (hFont) {
            LOGFONT lf;
            GetObject(hFont, sizeof(LOGFONT), &lf);
            lf.lfHeight = (lf.lfHeight * m_zoomLevel) / 100;
            HFONT hNewFont = CreateFontIndirect(&lf);
            SendMessage(m_pCurrentPanel->hwnd, WM_SETFONT, (WPARAM)hNewFont, TRUE);
        }
    }
    
    std::wstring msg = L"Zoom: " + std::to_wstring(m_zoomLevel) + L"%";
    UpdateStatusBar(msg);
}

void ProductionAgenticIDE::onZoomOut() {
    m_zoomLevel = (m_zoomLevel > 50) ? m_zoomLevel - 10 : 50;
    
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        HFONT hFont = (HFONT)SendMessage(m_pCurrentPanel->hwnd, WM_GETFONT, 0, 0);
        if (hFont) {
            LOGFONT lf;
            GetObject(hFont, sizeof(LOGFONT), &lf);
            lf.lfHeight = (lf.lfHeight * m_zoomLevel) / 100;
            HFONT hNewFont = CreateFontIndirect(&lf);
            SendMessage(m_pCurrentPanel->hwnd, WM_SETFONT, (WPARAM)hNewFont, TRUE);
        }
    }
    
    std::wstring msg = L"Zoom: " + std::to_wstring(m_zoomLevel) + L"%";
    UpdateStatusBar(msg);
}

void ProductionAgenticIDE::onFullscreen() {
    m_fullscreen = !m_fullscreen;
    
    if (m_fullscreen) {
        // Enter fullscreen
        MONITORINFO mi = { sizeof(mi) };
        if (GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(m_hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(m_hWnd, HWND_TOP,
                        mi.rcMonitor.left, mi.rcMonitor.top,
                        mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top,
                        SWP_NOOWNERZORDER);
        }
        UpdateStatusBar(L"Fullscreen mode");
    } else {
        // Exit fullscreen
        SetWindowLong(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(m_hWnd, HWND_NOTOPMOST, 100, 100, 1400, 900, SWP_SHOWWINDOW);
        UpdateStatusBar(L"Window mode");
    }
}

// ============================================================================
// PANEL MANAGEMENT IMPLEMENTATIONS
// ============================================================================

void ProductionAgenticIDE::onNextPanel() {
    if (m_panels.empty()) return;
    
    m_currentPanelIndex = (m_currentPanelIndex + 1) % m_panels.size();
    ActivatePanel(m_panels[m_currentPanelIndex].get());
    UpdateStatusBar(L"Switched to next panel");
}

void ProductionAgenticIDE::onPrevPanel() {
    if (m_panels.empty()) return;
    
    m_currentPanelIndex = (m_currentPanelIndex == 0) ? m_panels.size() - 1 : m_currentPanelIndex - 1;
    ActivatePanel(m_panels[m_currentPanelIndex].get());
    UpdateStatusBar(L"Switched to previous panel");
}

void ProductionAgenticIDE::onSplitVertical() {
    // Create a vertical splitter layout
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        m_splitLayout.isVertical = true;
        m_splitLayout.leftPanel = m_pCurrentPanel->hwnd;
        
        // Create new panel on the right
        Panel* newPanel = CreatePanel(PanelType::Code, L"untitled.cpp");
        if (newPanel) {
            m_splitLayout.rightPanel = newPanel->hwnd;
            UpdateLayout();
            UpdateStatusBar(L"Vertical split created");
        }
    }
}

void ProductionAgenticIDE::onSplitHorizontal() {
    // Create a horizontal splitter layout
    if (m_pCurrentPanel && IsWindow(m_pCurrentPanel->hwnd)) {
        m_splitLayout.isVertical = false;
        m_splitLayout.topPanel = m_pCurrentPanel->hwnd;
        
        // Create new panel below
        Panel* newPanel = CreatePanel(PanelType::Code, L"untitled.cpp");
        if (newPanel) {
            m_splitLayout.bottomPanel = newPanel->hwnd;
            UpdateLayout();
            UpdateStatusBar(L"Horizontal split created");
        }
    }
=======
void ProductionAgenticIDE::onSave() {
    statusBar()->showMessage(tr("Document saved"), 2000);
}

void ProductionAgenticIDE::onSaveAs() {
    std::string filename = // Dialog::getSaveFileName(this, tr("Save File As"), std::string(), tr("All Files (*)"));
    if (!filename.empty()) {
        statusBar()->showMessage(tr("Document saved as: %1"), 3000);
    }
}

void ProductionAgenticIDE::onExportImage() {
    std::string filename = // Dialog::getSaveFileName(this, tr("Export Image"), std::string(), tr("PNG Files (*.png);;JPEG Files (*.jpg);;All Files (*)"));
    if (!filename.empty()) {
        statusBar()->showMessage(tr("Image exported: %1"), 3000);
    }
}

void ProductionAgenticIDE::onExit() {
    close();
}

void ProductionAgenticIDE::onUndo() {
    statusBar()->showMessage(tr("Undo"), 1000);
}

void ProductionAgenticIDE::onRedo() {
    statusBar()->showMessage(tr("Redo"), 1000);
}

void ProductionAgenticIDE::onCut() {
    statusBar()->showMessage(tr("Cut"), 1000);
}

void ProductionAgenticIDE::onCopy() {
    statusBar()->showMessage(tr("Copy"), 1000);
}

void ProductionAgenticIDE::onPaste() {
    statusBar()->showMessage(tr("Paste"), 1000);
}

void ProductionAgenticIDE::onTogglePaintPanel() {
    statusBar()->showMessage(tr("Paint panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleCodePanel() {
    statusBar()->showMessage(tr("Code panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleChatPanel() {
    statusBar()->showMessage(tr("Chat panel toggled"), 1000);
}

void ProductionAgenticIDE::onToggleFeaturesPanel() {
    statusBar()->showMessage(tr("Features panel toggled"), 1000);
}

void ProductionAgenticIDE::onResetLayout() {
    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

void ProductionAgenticIDE::onFeatureToggled(const std::string& featureId, bool enabled) {
    statusBar()->showMessage(tr("Feature %1: %2"), 2000);
}

void ProductionAgenticIDE::onFeatureClicked(const std::string& featureId) {
    statusBar()->showMessage(tr("Feature activated: %1"), 2000);
>>>>>>> origin/main
}

