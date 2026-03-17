#include "Win32IDE.h"
#include <windowsx.h>
#include <commctrl.h>

// Window Management Implementation for Win32IDE
// Completes the GUI IDE loop by providing the missing Window Procedure and creation logic.

bool Win32IDE::createWindow() {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32IDE::WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "RawrXD_Win32IDE";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        // Class might already be registered
        // Check error
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            LOG_ERROR("Failed to register window class");
            return false;
        }
    }

    m_hwndMain = CreateWindowExA(
        0,
        "RawrXD_Win32IDE",
        "RawrXD IDE (Native/No Qt)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hwndMain) {
        LOG_ERROR("Failed to create main window");
        return false;
    }

    return true;
}

void Win32IDE::showWindow() {
    if (m_hwndMain) {
        ShowWindow(m_hwndMain, SW_SHOW);
        UpdateWindow(m_hwndMain);
    }
}

int Win32IDE::runMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32IDE*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwndMain = hwnd;
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            onCreate(hwnd);
            return 0;

        case WM_DESTROY:
            onDestroy();
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            onSize(LOWORD(lParam), HIWORD(lParam));
            return 0;

        case WM_COMMAND:
            onCommand(hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
            
        // Autonomy and Agent Events
        case WM_USER + 100: // Agent Output Update
            // Refresh UI if needed
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::onCreate(HWND hwnd) {
    LOG_INFO("Main Window Created: Initializing UI Components");
    
    // 1. Create Layout Components
    // Activity Bar (Leftmost strip)
    // Note: Assuming createActivityBar or similar is available or handled by createSidebar
    // If not, we rely on createSidebar to handle the left panel.
    
    // Primary Sidebar (Left Explorer/Search)
    createSidebar(hwnd);
    
    // Editor Area (Center - RichEdit)
    createEditor(hwnd);

    // Terminal/Panel Area (Bottom)
    createTerminal(hwnd);

    // Secondary Sidebar (Right - Copilot) - Defined in Win32IDE_VSCodeUI.cpp
    // Ensure we utilize the VS Code UI features if available
    createSecondarySidebar(hwnd);
    
    // Status Bar (Bottom strip)
    createStatusBar(hwnd);
    
    // 2. Initialize Logic Components
    // Initialize Agentic Bridge for AI features
    initializeAgenticBridge();
    
    // Kickstart Inference Engine (Native)
    if (!initializeInference()) {
        LOG_ERROR("Failed to initialize Inference Engine on startup");
    } else {
        LOG_INFO("Inference Engine initialized successfully");
    }
    
    // 3. Post-Creation Setup
    // Initialize default focus
    if (m_hwndEditor) {
        SetFocus(m_hwndEditor);
    }
}

void Win32IDE::onDestroy() {
    // Cleanup
}

void Win32IDE::onSize(int width, int height) {
    // Basic resizing logic
    if (m_hwndEditor) MoveWindow(m_hwndEditor, 250, 0, width - 250 - 230, height - 200, TRUE);
    if (m_hwndPowerShellPanel) MoveWindow(m_hwndPowerShellPanel, 250, height - 200, width - 250, 200, TRUE);
    if (m_hwndSecondarySidebar) {
        MoveWindow(m_hwndSecondarySidebar, width - 230, 0, 230, height, TRUE);
        // Resize Copilot controls
        if (m_hwndCopilotChatOutput) MoveWindow(m_hwndCopilotChatOutput, width - 225, 10, 220, height - 150, TRUE);
        if (m_hwndCopilotChatInput) MoveWindow(m_hwndCopilotChatInput, width - 225, height - 130, 220, 60, TRUE);
        if (m_hwndCopilotSendBtn) MoveWindow(m_hwndCopilotSendBtn, width - 225, height - 60, 100, 30, TRUE);
        if (m_hwndCopilotClearBtn) MoveWindow(m_hwndCopilotClearBtn, width - 110, height - 60, 100, 30, TRUE);
    }
}

void Win32IDE::onCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
    switch (id) {
        case 1204: // IDC_COPILOT_SEND_BTN
            HandleCopilotSend();
            break;
        case 1205: // IDC_COPILOT_CLEAR_BTN
            HandleCopilotClear();
            break;
        default:
            routeCommand(id);
            break;
    }
}

void Win32IDE::initializeAgenticBridge() {
    m_agenticBridge = std::make_unique<AgenticBridge>(this);
    // Explicitly assume local path for tools
    m_agenticBridge->Initialize(".", "titan-micro");
    
    // Connect output to Copilot Chat
    m_agenticBridge->SetOutputCallback([this](const std::string& type, const std::string& msg) {
        std::string fullMsg = "[" + type + "] " + msg + "\n";
        HandleCopilotStreamUpdate(fullMsg.c_str(), fullMsg.length());
    });
}
