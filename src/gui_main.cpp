#include "gui_main.h"
#include "native_ai_integration.hpp"
#include <commctrl.h>
#include <richedit.h>
#include <fstream>
#include <sstream>
#include <format> // Added for std::format

namespace RawrXD {

// AI Integration instance for GUI
static rawrxd::GUIIntegration g_guiAI;

GUIMain::GUIMain() {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
}

GUIMain::~GUIMain() {
    shutdown();
}

std::expected<void, std::string> GUIMain::initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    
    // Register window class
    auto classResult = registerWindowClass();
    if (!classResult) {
        return classResult;
    }
    
    // Create main window
    auto windowResult = createMainWindow();
    if (!windowResult) {
        return windowResult;
    }
    
    // Create editor window
    auto editorResult = createEditorWindow();
    if (!editorResult) {
        return editorResult;
    }
    
    // Create menus
    createMenus();
    
    // Create toolbar
    createToolbar();
    
    // Create status bar
    createStatusBar();
    
    // Create docking panels
    createDockingPanels();
    
    // Setup layout
    auto layoutResult = setupLayout();
    if (!layoutResult) {
        return layoutResult;
    }
    
    // Show main window
    ShowWindow(m_mainWindow, SW_SHOW);
    UpdateWindow(m_mainWindow);
    
    // Initialize IDE orchestrator
    IDEConfig config;
    config.modelsPath = "./models";
    config.toolsPath = "./tools";
    config.maxWorkers = 8;
    config.enableNetwork = true;
    config.enableSwarm = true;
    config.enableChainOfThought = true;
    config.enableTokenization = true;
    config.enableMonaco = true;
    config.logLevel = "info";
    
    m_ide = std::make_unique<IDEOrchestrator>(config);
    auto ideResult = m_ide->initialize();
    if (!ideResult) {
        return std::unexpected(std::string("IDE initialization failed"));
    }
    
    // Initialize Monaco editor
    MonacoConfig editorConfig;
    editorConfig.variant = MonacoVariant::Enterprise;
    editorConfig.themePreset = MonacoThemePreset::Default;
    editorConfig.enableIntelliSense = true;
    editorConfig.enableDebugging = true;
    editorConfig.workspaceRoot = "./workspace";
    
    m_editor = MonacoFactory::createEditor(MonacoVariant::Enterprise);
    auto editorResult = m_editor->initialize(m_mainWindow);
    if (!editorResult) {
        return editorResult;
    }
    
    // Wire IDE and editor
    // m_ide->getEditor() = m_editor.get(); // Note: IDE manages its own editor
    
    // Initialize AI Integration for GUI
    rawrxd::AIConfig aiConfig;
    aiConfig.llmEndpoint = "127.0.0.1";
    aiConfig.llmPort = 11434;
    aiConfig.serverPort = 3001;
    aiConfig.enableServer = true;
    aiConfig.enableTools = true;
    aiConfig.enableSearch = true;
    aiConfig.enableCompletion = true;
    aiConfig.defaultModel = "codellama";
    aiConfig.workspacePath = ".";
    
    if (!g_guiAI.initialize(aiConfig, m_mainWindow)) {
        // Non-fatal: AI features will be disabled
        updateStatusBar("Warning: AI integration initialization failed");
    } else {
        // Start AI server for external clients
        rawrxd::NativeAIIntegration::Instance().startServer();
        updateStatusBar("RawrXD IDE - AI Ready");
    }
    
    return {};
}

std::expected<void, std::string> GUIMain::registerWindowClass() {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = m_hInstance;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"RawrXDMainWindow";
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        return std::unexpected(std::string("Failed to register window class"));
    }
    
    return {};
}

std::expected<void, std::string> GUIMain::createMainWindow() {
    m_mainWindow = CreateWindowExW(
        0,
        L"RawrXDMainWindow",
        L"RawrXD v3.0 - Agentic IDE",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 800,
        nullptr,
        nullptr,
        m_hInstance,
        this
    );
    
    if (!m_mainWindow) {
        return std::unexpected(std::string("Failed to create main window"));
    }
    
    SetWindowLongPtrW(m_mainWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    return {};
}

LRESULT CALLBACK GUIMain::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GUIMain* gui = reinterpret_cast<GUIMain*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    
    if (gui) {
        return gui->handleMessageInternal(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT GUIMain::handleMessageInternal(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            return 0;
            
        case WM_COMMAND:
            handleMenuCommand(LOWORD(wParam));
            return 0; // Fixed: Was missing return
            
        case WM_SIZE:
            updateDockingLayout();
            return 0;
            
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
            }
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void GUIMain::createMenus() {
    m_mainMenu = CreateMenu();
    
    // File menu
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, 1001, L"&New\tCtrl+N");
    AppendMenuW(fileMenu, MF_STRING, 1002, L"&Open...\tCtrl+O");
    AppendMenuW(fileMenu, MF_STRING, 1003, L"&Save\tCtrl+S");
    AppendMenuW(fileMenu, MF_STRING, 1004, L"Save &As...\tCtrl+Shift+S");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(fileMenu, MF_STRING, 1005, L"E&xit\tAlt+F4");
    AppendMenuW(m_mainMenu, MF_POPUP, (UINT_PTR)fileMenu, L"&File");
    
    // Edit menu
    HMENU editMenu = CreatePopupMenu();
    AppendMenuW(editMenu, MF_STRING, 2001, L"&Undo\tCtrl+Z");
    AppendMenuW(editMenu, MF_STRING, 2002, L"&Redo\tCtrl+Y");
    AppendMenuW(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(editMenu, MF_STRING, 2003, L"Cu&t\tCtrl+X");
    AppendMenuW(editMenu, MF_STRING, 2004, L"&Copy\tCtrl+C");
    AppendMenuW(editMenu, MF_STRING, 2005, L"&Paste\tCtrl+V");
    AppendMenuW(m_mainMenu, MF_POPUP, (UINT_PTR)editMenu, L"&Edit");
    
    // Build menu
    HMENU buildMenu = CreatePopupMenu();
    AppendMenuW(buildMenu, MF_STRING, 3001, L"&Build\tCtrl+B");
    AppendMenuW(buildMenu, MF_STRING, 3002, L"&Run\tF5");
    AppendMenuW(buildMenu, MF_STRING, 3003, L"&Debug\tF11");
    AppendMenuW(m_mainMenu, MF_POPUP, (UINT_PTR)buildMenu, L"&Build");
    
    // AI menu
    HMENU aiMenu = CreatePopupMenu();
    AppendMenuW(aiMenu, MF_STRING, 4001, L"&Generate\tCtrl+G");
    AppendMenuW(aiMenu, MF_STRING, 4002, L"&Debug with AI\tCtrl+D");
    AppendMenuW(aiMenu, MF_STRING, 4003, L"&Optimize\tCtrl+O");
    AppendMenuW(aiMenu, MF_STRING, 4004, L"Analyze &Codebase\tCtrl+A");
    AppendMenuW(m_mainMenu, MF_POPUP, (UINT_PTR)aiMenu, L"&AI");
    
    SetMenu(m_mainWindow, m_mainMenu);
}

void GUIMain::handleMenuCommand(int commandId) {
    switch (commandId) {
        case 1001: // File New
            onFileNewInternal();
            break;
        case 1002: // File Open
            onFileOpenInternal();
            break;
        case 1003: // File Save
            onFileSaveInternal();
            break;
        case 1004: // File Save As
            // Implementation
            break;
        case 1005: // File Exit
            PostQuitMessage(0);
            break;
            
        case 2001: // Edit Undo
            onEditUndoInternal();
            break;
        case 2002: // Edit Redo
            onEditRedoInternal();
            break;
        case 2003: // Edit Cut
            onEditCutInternal();
            break;
        case 2004: // Edit Copy
            onEditCopyInternal();
            break;
        case 2005: // Edit Paste
            onEditPasteInternal();
            break;
            
        case 3001: // Build
            onBuildInternal();
            break;
        case 3002: // Run
            onRunInternal();
            break;
        case 3003: // Debug
            onDebugInternal();
            break;
            
        case 4001: // AI Generate
            onAIGenerateInternal();
            break;
        case 4002: // AI Debug
            onAIDebugInternal();
            break;
        case 4003: // AI Optimize
            onAIOptimizeInternal();
            break;
        case 4004: // AI Analyze
            onAIAnalyzeInternal();
            break;
    }
}

void GUIMain::onAIGenerateInternal() {
    if (!g_guiAI.isReady()) {
        MessageBoxW(m_mainWindow, L"AI not initialized", L"AI Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Get current code from editor
    if (m_editor) {
        std::string code = m_editor->getText();
        std::string prompt = "Generate code based on the following context:\n\n" + code;
        
        rawrxd::LLMResponse resp = g_guiAI.sendChatMessage(prompt);
        if (!resp.error.empty()) {
            MessageBoxA(m_mainWindow, resp.error.c_str(), "AI Error", MB_OK | MB_ICONERROR);
        } else {
            // Insert generated code
            m_editor->insertText(resp.content);
            updateStatusBar("AI generation complete");
        }
    }
}

void GUIMain::onAIDebugInternal() {
    if (!g_guiAI.isReady()) {
        MessageBoxW(m_mainWindow, L"AI not initialized", L"AI Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    if (m_editor) {
        std::string code = m_editor->getText();
        std::string prompt = "Analyze and debug the following code:\n\n" + code;
        
        rawrxd::LLMResponse resp = g_guiAI.sendChatMessage(prompt);
        if (!resp.error.empty()) {
            MessageBoxA(m_mainWindow, resp.error.c_str(), "AI Error", MB_OK | MB_ICONERROR);
        } else {
            // Show debug analysis in a message box or output panel
            MessageBoxA(m_mainWindow, resp.content.c_str(), "AI Debug Analysis", MB_OK | MB_ICONINFORMATION);
            updateStatusBar("AI debug analysis complete");
        }
    }
}

void GUIMain::onAIOptimizeInternal() {
    if (!g_guiAI.isReady()) {
        MessageBoxW(m_mainWindow, L"AI not initialized", L"AI Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    if (m_editor) {
        std::string code = m_editor->getText();
        rawrxd::LLMResponse resp = g_guiAI.requestRefactor(code, "optimize for performance");
        
        if (!resp.error.empty()) {
            MessageBoxA(m_mainWindow, resp.error.c_str(), "AI Error", MB_OK | MB_ICONERROR);
        } else {
            m_editor->setText(resp.content);
            updateStatusBar("AI optimization complete");
        }
    }
}

void GUIMain::onAIAnalyzeInternal() {
    if (!g_guiAI.isReady()) {
        MessageBoxW(m_mainWindow, L"AI not initialized", L"AI Error", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Index workspace for semantic search
    rawrxd::NativeAIIntegration::Instance().indexWorkspace();
    
    if (m_editor) {
        std::string code = m_editor->getText();
        rawrxd::LLMResponse resp = g_guiAI.requestExplanation(code);
        
        if (!resp.error.empty()) {
            MessageBoxA(m_mainWindow, resp.error.c_str(), "AI Error", MB_OK | MB_ICONERROR);
        } else {
            MessageBoxA(m_mainWindow, resp.content.c_str(), "AI Code Analysis", MB_OK | MB_ICONINFORMATION);
            updateStatusBar("AI analysis complete");
        }
    }
}

void GUIMain::onFileNewInternal() {
    if (m_editor) {
        m_editor->setText("");
        updateStatusBar("New file created");
    }
}

void GUIMain::onFileOpenInternal() {
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_mainWindow;
    ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.lpstrFile = new WCHAR[MAX_PATH];
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        std::wstring ws(ofn.lpstrFile);
        std::string path(ws.begin(), ws.end());
        
        if (m_editor) {
            auto result = m_editor->loadFile(path);
            if (result) {
                updateStatusBar(std::format("Loaded: {}", path));
            } else {
                updateStatusBar(std::format("Failed to load: {}", result.error()));
            }
        }
        
        delete[] ofn.lpstrFile;
    }
}

void GUIMain::onFileSaveInternal() {
    if (!m_editor) return;
    
    std::string currentFile = m_editor->getCurrentFile();
    if (currentFile.empty()) {
        // Show save dialog
        OPENFILENAMEW ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_mainWindow;
        ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
        ofn.lpstrFile = new WCHAR[MAX_PATH];
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT;
        
        if (GetSaveFileNameW(&ofn)) {
            std::wstring ws(ofn.lpstrFile);
            currentFile = std::string(ws.begin(), ws.end());
            delete[] ofn.lpstrFile;
        } else {
            return;
        }
    }
    
    auto result = m_editor->saveFile(currentFile);
    if (result) {
        updateStatusBar(std::format("Saved: {}", currentFile));
    } else {
        updateStatusBar(std::format("Failed to save: {}", result.error()));
    }
}

void GUIMain::updateStatusBar(const std::string& message) {
    if (m_statusBar) {
        SetWindowTextA(m_statusBar, message.c_str());
    }
}

std::expected<void, std::string> GUIMain::run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return {};
}

void GUIMain::shutdown() {
    if (m_ide) {
        m_ide->stop();
    }
    
    // m_editor is a unique_ptr, will clean itself up
    
    if (m_mainWindow) {
        DestroyWindow(m_mainWindow);
        m_mainWindow = nullptr;
    }
}

// Real Win32 Implementations for GUI
std::expected<void, std::string> GUIMain::createEditorWindow() {
    // Create Monaco-like editor using RichEdit or WebView
    // Using RichEdit for simpler Win32 "RawrXD" feel
    LoadLibrary(TEXT("Msftedit.dll")); 
    
    RECT rcClient;
    GetClientRect(m_mainWindow, &rcClient);
    
    m_editorWindow = CreateWindowEx(
        0, 
        TEXT("RICHEDIT50W"), 
        TEXT(""), 
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 
        0, 28, rcClient.right, rcClient.bottom - 48, // Adjust for toolbar/status
        m_mainWindow, 
        (HMENU)1001, 
        m_hInstance, 
        NULL
    );
    
    if (!m_editorWindow) return std::unexpected("Failed to create editor window");
    
    // Set font
    HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
      FIXED_PITCH | FF_MODERN, TEXT("Consolas"));
    SendMessage(m_editorWindow, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    return {};
}

std::expected<void, std::string> GUIMain::setupLayout() {
    // Simple layout management handled in WM_SIZE usually
    return {};
}

void GUIMain::createToolbar() {
    // Basic Toolbar
    HWND hTool = CreateWindowEx(0, TOOLBARCLASSNAME, NULL, 
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, 
        m_mainWindow, (HMENU)1002, m_hInstance, NULL);
        
    SendMessage(hTool, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
    
    TBBUTTON tbb[3];
    ZeroMemory(tbb, sizeof(tbb));
    
    tbb[0].iBitmap = I_IMAGENONE; 
    tbb[0].fsState = TBSTATE_ENABLED; 
    tbb[0].fsStyle = BTNS_BUTTON; 
    tbb[0].idCommand = 2001; // CMD_RUN
    tbb[0].iString = (INT_PTR)TEXT("Run");
    
    tbb[1].iBitmap = I_IMAGENONE; 
    tbb[1].fsState = TBSTATE_ENABLED; 
    tbb[1].fsStyle = BTNS_BUTTON; 
    tbb[1].idCommand = 2002; // CMD_DEBUG
    tbb[1].iString = (INT_PTR)TEXT("Debug");
    
    tbb[2].iBitmap = I_IMAGENONE; 
    tbb[2].fsState = TBSTATE_ENABLED; 
    tbb[2].fsStyle = BTNS_BUTTON; 
    tbb[2].idCommand = 2003; // CMD_BUILD
    tbb[2].iString = (INT_PTR)TEXT("Build");
    
    SendMessage(hTool, TB_ADDBUTTONS, 3, (LPARAM)&tbb);
    SendMessage(hTool, TB_AUTOSIZE, 0, 0);
}

void GUIMain::createStatusBar() {
    HWND hStatus = CreateWindowEx(0, STATUSCLASSNAME, NULL, 
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, 
        m_mainWindow, (HMENU)1003, m_hInstance, NULL);
    
    int statwidths[] = {100, 200, -1};
    SendMessage(hStatus, SB_SETPARTS, sizeof(statwidths)/sizeof(int), (LPARAM)statwidths);
    SendMessage(hStatus, SB_SETTEXT, 0, (LPARAM)TEXT("Ready"));
}

void GUIMain::createDockingPanels() {
    // Create docking panels for file explorer, output, and properties
    if (!m_mainWindow) return;
    
    // File explorer panel (left)
    m_fileExplorerPanel = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("SysTreeView32"), TEXT(""),
        WS_CHILD | WS_VISIBLE | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
        0, 0, 200, 400, m_mainWindow, (HMENU)1001, m_hInstance, nullptr);
    
    // Output panel (bottom)
    m_outputPanel = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 0, 600, 150, m_mainWindow, (HMENU)1002, m_hInstance, nullptr);
    
    // Properties panel (right)
    m_propertiesPanel = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("LISTBOX"), TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY,
        0, 0, 200, 400, m_mainWindow, (HMENU)1003, m_hInstance, nullptr);
}

void GUIMain::updateDockingLayout() {
    // Update docking layout based on window size
    if (!m_mainWindow) return;
    
    RECT rc;
    GetClientRect(m_mainWindow, &rc);
    
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    
    // Toolbar height
    int toolbarHeight = 30;
    // Status bar height
    int statusHeight = 25;
    
    // Available area
    int availHeight = height - toolbarHeight - statusHeight;
    int availWidth = width;
    
    // Panel widths
    int leftPanelWidth = 200;
    int rightPanelWidth = 200;
    int bottomPanelHeight = 150;
    
    // Editor area
    int editorX = leftPanelWidth;
    int editorY = toolbarHeight;
    int editorWidth = availWidth - leftPanelWidth - rightPanelWidth;
    int editorHeight = availHeight - bottomPanelHeight;
    
    // Position panels
    if (m_fileExplorerPanel) {
        SetWindowPos(m_fileExplorerPanel, nullptr, 0, toolbarHeight, leftPanelWidth, availHeight - bottomPanelHeight,
                     SWP_NOZORDER);
    }
    if (m_outputPanel) {
        SetWindowPos(m_outputPanel, nullptr, leftPanelWidth, toolbarHeight + editorHeight, 
                     editorWidth, bottomPanelHeight, SWP_NOZORDER);
    }
    if (m_propertiesPanel) {
        SetWindowPos(m_propertiesPanel, nullptr, width - rightPanelWidth, toolbarHeight, 
                     rightPanelWidth, availHeight - bottomPanelHeight, SWP_NOZORDER);
    }
    if (m_editorWindow) {
        SetWindowPos(m_editorWindow, nullptr, editorX, editorY, editorWidth, editorHeight, SWP_NOZORDER);
    }
}

void GUIMain::onDebugInternal() {
    // Launch debugger or show debug output
    if (m_outputPanel) {
        SetWindowText(m_outputPanel, TEXT("Debug mode activated.\r\nWaiting for debugger attachment...\r\n"));
    }
    
    // Try to launch VS JIT debugger
    std::string cmd = "vsjitdebugger.exe -p " + std::to_string(GetCurrentProcessId());
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()), nullptr, nullptr, FALSE, 
                   CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi);
}


} // namespace RawrXD
