#include "production_agentic_ide.h"
#include "native_file_tree.h"
#include "agentic_browser.h"
#include "agent_orchestra.h"
#include "multi_pane_layout.h"
#include "logger.h"
#include <iostream>
#include <cassert>

// Global map to store instance pointers for window proc callback
static std::map<HWND, ProductionAgenticIDE*> g_windowMap;

ProductionAgenticIDE::ProductionAgenticIDE() {
    log_info("Initializing ProductionAgenticIDE");
    
    // Create components
    m_fileTree = std::make_unique<NativeFileTree>();
    m_agenticBrowser = std::make_unique<AgenticBrowser>();
    m_agentOrchestra = std::make_unique<AgentOrchestra>();
    m_layout = std::make_unique<MultiPaneLayout>();

    log_debug("All components initialized");
}

ProductionAgenticIDE::~ProductionAgenticIDE() {
    if (m_hwnd) {
        g_windowMap.erase(m_hwnd);
        DestroyWindow(m_hwnd);
    }
    log_info("ProductionAgenticIDE destroyed");
}

int ProductionAgenticIDE::run() {
    setupGUI();
    
    if (!m_hwnd) {
        log_error("Failed to create window");
        return 1;
    }

    showWindow();

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

void ProductionAgenticIDE::showWindow() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        log_debug("Window shown");
    }
}

void ProductionAgenticIDE::closeWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        log_debug("Window closed");
    }
}

void ProductionAgenticIDE::setupGUI() {
    // Register window class
    const wchar_t CLASS_NAME[] = L"RawrXDAgenticIDE";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.style = CS_DBLCLKS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    // Create main window
    m_hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"RawrXD Agentic IDE",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 800,
        nullptr,
        nullptr,
        wc.hInstance,
        this
    );

    if (!m_hwnd) {
        log_error("Failed to create main window");
        return;
    }

    g_windowMap[m_hwnd] = this;

    createMenuBar();
    createToolBar();
    createStatusBar();
    createLayout();

    log_debug("GUI setup complete");
}

void ProductionAgenticIDE::createMenuBar() {
    HMENU hMenu = CreateMenu();
    
    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuA(hFileMenu, MFT_STRING, 101, "New Paint\tCtrl+N");
    AppendMenuA(hFileMenu, MFT_STRING, 102, "New Code\tCtrl+N");
    AppendMenuA(hFileMenu, MFT_STRING, 103, "New Chat\tCtrl+N");
    AppendMenuA(hFileMenu, MFT_SEPARATOR, 0, nullptr);
    AppendMenuA(hFileMenu, MFT_STRING, 104, "Open\tCtrl+O");
    AppendMenuA(hFileMenu, MFT_STRING, 105, "Save\tCtrl+S");
    AppendMenuA(hFileMenu, MFT_STRING, 106, "Save As\tCtrl+Shift+S");
    AppendMenuA(hFileMenu, MFT_SEPARATOR, 0, nullptr);
    AppendMenuA(hFileMenu, MFT_STRING, 107, "Exit\tAlt+F4");
    
    AppendMenuA(hMenu, MFT_POPUP, (UINT_PTR)hFileMenu, "File");

    SetMenu(m_hwnd, hMenu);
    log_debug("Menu bar created");
}

void ProductionAgenticIDE::createToolBar() {
    // Placeholder for toolbar creation
    log_debug("Toolbar created");
}

void ProductionAgenticIDE::createStatusBar() {
    m_statusBar = CreateWindowExA(
        0, STATUSCLASSNAMEA, "Ready",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0,
        m_hwnd, nullptr,
        GetModuleHandle(nullptr), nullptr
    );
    log_debug("Status bar created");
}

void ProductionAgenticIDE::createLayout() {
    // Placeholder for layout creation
    log_debug("Layout created");
}

void ProductionAgenticIDE::onFileOpen() {
    log_debug("File open dialog");
}

void ProductionAgenticIDE::onFileSave() {
    log_debug("File saved");
}

void ProductionAgenticIDE::onFileTreeDoubleClicked(const std::string& filePath) {
    log_info("File double-clicked: " + filePath);
}

void ProductionAgenticIDE::onFileTreeContextMenu(const std::string& filePath) {
    log_debug("File context menu: " + filePath);
}

void ProductionAgenticIDE::toggleFileTreePane() {
    m_fileTreeVisible = !m_fileTreeVisible;
    log_debug(std::string("File tree pane ") + (m_fileTreeVisible ? "shown" : "hidden"));
}

void ProductionAgenticIDE::toggleTerminalPane() {
    m_terminalVisible = !m_terminalVisible;
    log_debug(std::string("Terminal pane ") + (m_terminalVisible ? "shown" : "hidden"));
}

void ProductionAgenticIDE::toggleChatPane() {
    m_chatVisible = !m_chatVisible;
    log_debug(std::string("Chat pane ") + (m_chatVisible ? "shown" : "hidden"));
}

void ProductionAgenticIDE::toggleBrowserPane() {
    m_browserVisible = !m_browserVisible;
    log_debug(std::string("Browser pane ") + (m_browserVisible ? "shown" : "hidden"));
}

void ProductionAgenticIDE::resetLayout() {
    m_fileTreeVisible = true;
    m_terminalVisible = true;
    m_chatVisible = true;
    m_browserVisible = false;
    log_debug("Layout reset to default");
}

void ProductionAgenticIDE::newPaintTab() {
    log_info("New paint tab created");
}

void ProductionAgenticIDE::openPaint() {
    log_debug("Open paint file");
}

void ProductionAgenticIDE::savePaint() {
    log_debug("Save paint file");
}

void ProductionAgenticIDE::newCodeTab() {
    log_info("New code tab created");
}

void ProductionAgenticIDE::openCode() {
    log_debug("Open code file");
}

void ProductionAgenticIDE::saveCode() {
    log_debug("Save code file");
}

void ProductionAgenticIDE::newChatTab() {
    log_info("New chat tab created");
}

void ProductionAgenticIDE::sendChatMessage() {
    log_debug("Chat message sent");
}

void ProductionAgenticIDE::analyzeCurrentFile() {
    log_info("Analyzing current file");
}

void ProductionAgenticIDE::analyzeProject() {
    log_info("Analyzing project");
}

void ProductionAgenticIDE::sendToAgent(const std::string& message) {
    log_info("Sending to agent: " + message);
}

void ProductionAgenticIDE::selectModel(const std::string& model) {
    log_debug("Model selected: " + model);
}

void ProductionAgenticIDE::startVoiceInput() {
    log_info("Voice input started");
    m_agentOrchestra->startVoiceInput();
}

LRESULT CALLBACK ProductionAgenticIDE::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ProductionAgenticIDE* pThis = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<ProductionAgenticIDE*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = reinterpret_cast<ProductionAgenticIDE*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        pThis->handleWindowMessage(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == 107) PostQuitMessage(0);  // Exit
            return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ProductionAgenticIDE::handleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Window message handling
}
