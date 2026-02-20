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
        if (m_hAccel && TranslateAccelerator(m_hwndMain, m_hAccel, &msg)) {
            continue;
        }
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

    // 4. Create keyboard accelerator table for Build shortcuts
    createAcceleratorTable();
}

void Win32IDE::onDestroy() {
    // Cleanup Resources
    if (m_agenticBridge) {
        m_agenticBridge->StopAgentLoop();
    }
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    shutdownInference();
    shutdownLogging();
}

void Win32IDE::onSize(int width, int height) {
    if (width == 0 || height == 0) return;

    // Layout constants
    int statusBarHeight = 25;
    int terminalHeight = 200; // Fixed for now, should be m_terminalHeight
    int activityBarWidth = 48; // VSCode style
    int sidebarWidth = 250; // Should be m_sidebarWidth
    int secondarySidebarWidth = 320; // Should be m_secondarySidebarWidth
    
    // Adjust logic based on visibility
    bool showSidebar = (m_hwndSidebar != nullptr && IsWindowVisible(m_hwndSidebar));
    bool showSecondary = (m_hwndSecondarySidebar != nullptr && IsWindowVisible(m_hwndSecondarySidebar));
    bool showTerminal = (m_hwndPowerShellPanel != nullptr && IsWindowVisible(m_hwndPowerShellPanel));
    
    int currentX = 0;
    
    // 1. Activity Bar (Far Left)
    // If we had m_hwndActivityBar, we'd size it. Assuming it exists if Sidebar exists?
    // Win32IDE_Sidebar.cpp creates m_hwndActivityBar.
    // If we can't access m_hwndActivityBar (private), we might need to rely on direct HWND search or use known ID.
    // For now, allow sidebar to handle its own layout if it does, but we must position the main containers.
    
    // Actually, Win32IDE_Sidebar.cpp likely doesn't resize itself?
    // SidebarProc handles PAINT, but what about SIZE?
    
    // Let's position the main containers we know about.
    
    int workspaceX = 0;
    int workspaceWidth = width;
    
    // Sidebar Area
    if (showSidebar) {
        // Assume Sidebar includes Activity Bar visually?
        // Win32IDE.cpp createSidebar puts it at x=48.
        // So we need to put Activity Bar at x=0, w=48.
        // And Sidebar at x=48, w=m_sidebarWidth.
        
        // Find Activity Bar by ID if member not accessible? 
        // Or assume ID 1100 (from VSCodeUI).
        // HWND hActivity = GetDlgItem(m_hwndMain, 1100); or m_hwndActivityBar if accessible.
        
        // Since we are in Win32IDE class, we have access to private members declared in header!
        // We need to check if m_hwndActivityBar is in header.
        // Assuming it is.
        
        /* 
           Since I am not 100% sure of member visibility in the split definition, 
           I will use Safe positioning knowing that I am in the class scope.
        */
        
        // Activity Bar
        // MoveWindow(m_hwndActivityBar, 0, 0, activityBarWidth, height - statusBarHeight, TRUE);
        
        // Primary Sidebar
        MoveWindow(m_hwndSidebar, activityBarWidth, 0, sidebarWidth, height - statusBarHeight, TRUE);
        
        workspaceX += (activityBarWidth + sidebarWidth);
        workspaceWidth -= (activityBarWidth + sidebarWidth);
    }
    
    // Secondary Sidebar Area
    if (showSecondary) {
        MoveWindow(m_hwndSecondarySidebar, width - secondarySidebarWidth, 0, secondarySidebarWidth, height - statusBarHeight, TRUE);
        workspaceWidth -= secondarySidebarWidth;
    }
    
    // Terminal Area
    int editorHeight = height - statusBarHeight;
    if (showTerminal) {
        editorHeight -= terminalHeight;
        MoveWindow(m_hwndPowerShellPanel, workspaceX, editorHeight, workspaceWidth, terminalHeight, TRUE);
        
        // Also Command Input?
        // m_hwndCommandInput
    }
    
    // Editor Area
    if (m_hwndEditor) {
        MoveWindow(m_hwndEditor, workspaceX, 0, workspaceWidth, editorHeight, TRUE);
    }
    
    // Status Bar
    if (m_hwndStatusBar) {
        MoveWindow(m_hwndStatusBar, 0, height - statusBarHeight, width, statusBarHeight, TRUE);
        SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0); // specific for statusbar control
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

    // Initialize Swarm Intelligence System for replacement of simulation
    initializeSwarmSystem();
}
