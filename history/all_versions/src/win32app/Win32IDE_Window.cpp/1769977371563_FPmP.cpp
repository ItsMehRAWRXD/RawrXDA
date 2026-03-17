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
    // Basic Layout Init
    // In a real implementation, child windows are created here (Sidebar, Editor, Terminal)
    // However, Win32IDE.cpp likely has init logic in its Constructor or similar, 
    // but Child Windows need a HWND parent.
    // NOTE: Win32IDE::Win32IDE() initializes member variables but cannot create windows without m_hwndMain.
    // We should call a method to create child controls here.
    
    LOG_INFO("Main Window Created");
    
    // We call the layout initialization logic if it exists, or replicate it.
    // Given the previous file content, `Win32IDE.cpp` did NOT seemingly have a "CreateControls" method.
    // It had a HUGE constructor, but creating windows in constructor fails if parent is null.
    // Wait, the constructor took hInstance but didn't take hwnd.
    // So the child window creation MUST happen here.
    
    // Explicit Missing Logic: Create the layout components
    
    // 1. Sidebar
    // (Simplified creation)
    
    // 2. Editor
    m_hwndEditor = CreateWindowExA(0, "EDIT", "", 
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
        250, 0, 800, 600, hwnd, (HMENU)1001, m_hInstance, nullptr);
    SendMessage(m_hwndEditor, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // 3. Terminal
    m_hwndPowerShellPanel = CreateWindowExA(0, "STATIC", "Terminal Area", 
        WS_CHILD | WS_VISIBLE,
        250, 600, 800, 200, hwnd, (HMENU)1002, m_hInstance, nullptr);

    // 4. Copilot / Secondary Sidebar
    m_hwndSecondarySidebar = CreateWindowExA(0, "STATIC", "", 
            WS_CHILD | WS_VISIBLE | SS_BLACKFRAME, 
            1050, 0, 230, 800, hwnd, (HMENU)1200, m_hInstance, nullptr);
            
    // Create Copilot controls (Explicitly calling what logic was hinted at in Win32IDE.cpp but maybe nowhere called)
    // We found `IDC_COPILOT_SEND_BTN` usage in `Win32IDE.cpp`?
    // Actually, `Win32IDE.cpp` had `CreateWindow` calls in `initSecondarySidebar`? 
    // I only saw define macros and a function *using* the handles.
    // I will explicitly create them here to ensure it works.
    
    m_hwndCopilotChatOutput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "RawrXD AI Ready.", 
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        1055, 10, 220, 600, hwnd, (HMENU)1203, m_hInstance, nullptr);
        
    m_hwndCopilotChatInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", 
        WS_CHILD | WS_VISIBLE | ES_MULTILINE,
        1055, 620, 220, 60, hwnd, (HMENU)1202, m_hInstance, nullptr);
        
    m_hwndCopilotSendBtn = CreateWindowExA(0, "BUTTON", "Send", 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        1055, 690, 100, 30, hwnd, (HMENU)1204, m_hInstance, nullptr);

    m_hwndCopilotClearBtn = CreateWindowExA(0, "BUTTON", "Clear", 
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        1165, 690, 100, 30, hwnd, (HMENU)1205, m_hInstance, nullptr);

    HFONT hFont = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                             DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    SendMessage(m_hwndCopilotChatOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hwndCopilotChatInput, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hwndCopilotSendBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Initialize Agents
    initializeAgenticBridge();
    initializeInference(); // Ensure Native Engine is up
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
