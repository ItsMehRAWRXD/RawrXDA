#include "Win32IDE.h"
#include "../bridge/Win32SwarmBridge.h"
#include <windowsx.h>
#include <commctrl.h>
#include <shellscalingapi.h>   // GetDpiForWindow
#include <cassert>
#include <cstring>

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

        // Post-show client-rect verification
        RECT rc = {};
        GetClientRect(m_hwndMain, &rc);
        int cw = rc.right - rc.left;
        int ch = rc.bottom - rc.top;
        if (cw < 400 || ch < 300) {
            // Window reported a degenerate client area — force a safe minimum size
            char warn[128];
            snprintf(warn, sizeof(warn),
                     "[showWindow] Degenerate client rect %dx%d — forcing 1280x800\n", cw, ch);
            OutputDebugStringA(warn);
            SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 1280, 800,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
        } else {
            char info[128];
            snprintf(info, sizeof(info),
                     "[showWindow] Client rect OK: %dx%d\n", cw, ch);
            OutputDebugStringA(info);
        }
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
        case WM_USER + 100:
            return 0;

        // Watchdog show-window request (posted from VisibilityWatchdogThread
        // to avoid cross-thread blocking during shutdown).
        // wParam = SW_* command (SW_SHOW, SW_RESTORE, etc.)
        case WM_APP + 1:
            ShowWindow(hwnd, static_cast<int>(wParam));
            SetForegroundWindow(hwnd);
            return 0;

        case WM_ACTIVATE: {
            WORD state = LOWORD(wParam);
            const char* stateStr = (state == WA_INACTIVE) ? "INACTIVE"
                                 : (state == WA_ACTIVE)   ? "ACTIVE"
                                                          : "CLICKACTIVE";
            char dbg[128];
            snprintf(dbg, sizeof(dbg), "[WM_ACTIVATE] state=%s\n", stateStr);
            OutputDebugStringA(dbg);
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_MOVE: {
            int x = (int)(short)LOWORD(lParam);
            int y = (int)(short)HIWORD(lParam);
            RECT wa = {};
            SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
            if (x < wa.left - 1200 || y < wa.top - 100 || x > wa.right || y > wa.bottom) {
                char warn[128];
                snprintf(warn, sizeof(warn),
                         "[WM_MOVE] Off-screen (%d, %d) -- recentering\n", x, y);
                OutputDebugStringA(warn);
                int sw = GetSystemMetrics(SM_CXSCREEN);
                int sh = GetSystemMetrics(SM_CYSCREEN);
                SetWindowPos(hwnd, nullptr, (sw - 1280) / 2, (sh - 800) / 2, 0, 0,
                             SWP_NOSIZE | SWP_NOZORDER);
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc) {
                RECT rcClient;
                GetClientRect(hwnd, &rcClient);
                FillRect(hdc, &rcClient, (HBRUSH)GetStockObject(BLACK_BRUSH));
                EndPaint(hwnd, &ps);
            }
            return 0;
        }
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
    
    // Register Swarm Bridge with IAT (Closes Slot 20 Gap) and start swarm
    if (RawrXD::Bridge::RegisterSwarmBridgeWithIAT()) {
        RawrXD::Bridge::SwarmInitConfig config{};
        config.structSize = sizeof(config);
        config.maxSubAgents = 8;
        config.taskTimeoutMs = 30000;
        config.enableGPUWorkStealing = TRUE;
        strcpy_s(config.coordinatorModel, "phi3:mini");
        if (FAILED(RawrXD::Bridge::InitializeSwarmSystem(&config))) {
            OutputDebugStringA("Win32IDE: Swarm system init failed\n");
        }
    } else {
        OutputDebugStringA("Win32IDE: Swarm bridge registration failed\n");
    }

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
    m_shuttingDown.store(true, std::memory_order_release);

    RawrXD::Bridge::ShutdownSwarmSystem();

    // Parity-audit: stop watchdog before anything else so it can't race on
    // the HWND members we're about to tear down.
    stopVisibilityWatchdog();

    // Cleanup Resources
    if (m_agenticBridge) {
        m_agenticBridge->StopAgentLoop();
    }
    if (m_hAccel) {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    shutdownLogging();
}

void Win32IDE::onSize(int width, int height) {
    // ── Parity-audit: dimension guards ──────────────────────────────────────
    // Clamp to safe minimums so layout arithmetic never produces negatives.
    // The assert fires in Debug builds; the clamp protects Release builds.
    if (width  < 1) width  = 1;
    if (height < 1) height = 1;
    width  = (width  < 400) ? 400  : width;
    height = (height < 300) ? 300  : height;
    assert(width  >= 400);
    assert(height >= 300);

    // DPI-aware scaling
    UINT dpi = GetDpiForWindow(m_hwndMain ? m_hwndMain : GetDesktopWindow());
    if (dpi == 0) dpi = 96;  // fallback: 100% scaling
    float dpiScale = (float)dpi / 96.0f;

    // Debug trace (visible in DebugView / VS Output)
    {
        char dbg[128];
        snprintf(dbg, sizeof(dbg),
                 "[WM_SIZE] %dx%d  DPI=%u (%.2fx)\n", width, height, dpi, dpiScale);
        OutputDebugStringA(dbg);
    }

    // Layout constants
    int statusBarHeight      = (int)(25  * dpiScale);
    int terminalHeight       = (int)(200 * dpiScale);
    int activityBarWidth     = (int)(48  * dpiScale);
    int sidebarWidth         = (int)(250 * dpiScale);
    int secondarySidebarWidth= (int)(320 * dpiScale);
    
    // Adjust logic based on visibility
    bool showSidebar = (m_hwndSidebar != nullptr && IsWindowVisible(m_hwndSidebar));
    bool showSecondary = (m_hwndSecondarySidebar != nullptr && IsWindowVisible(m_hwndSecondarySidebar));
    bool showTerminal = (m_hwndPowerShellPanel != nullptr && IsWindowVisible(m_hwndPowerShellPanel));
    
    int currentX = 0;
    
    // 1. Activity Bar (Far Left)
    int workspaceX = 0;
    int workspaceWidth = width;

    if (showSidebar) {
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

// ----------------------------------------------------------------------------
// Agentic Bridge Initialization
// ----------------------------------------------------------------------------

// Single definition in Win32IDE_AgentCommands.cpp (Full Agentic IDE). Do not redefine here.
void Win32IDE::initializeAgenticBridge() {
    // Single shared definition in src/win32app/Win32IDE_AgentCommands.cpp
    // This file (Win32IDE_Window.cpp) is for window/message loop logic only.
    extern void initializeAgenticBridge_Internal(Win32IDE*); 
    // This is just a marker; the compiler/linker finds the definition in AgentCommands.cpp
}

void Win32IDE::initializeAgenticBridge_Redundant() {
    // This was the duplicate found during audit.
    // It is preserved here commented out as a reference during migration if needed,
    // but the actual call site in onCreate() should use the canonical version.
    /*
    m_agenticBridge = std::make_unique<AgenticBridge>(this);
    m_agenticBridge->Initialize(".", "");
    m_agenticBridge->SetOutputCallback([this](const std::string& type, const std::string& msg) {
        std::string fullMsg = "[" + type + "] " + msg + "\n";
        HandleCopilotStreamUpdate(fullMsg.c_str(), fullMsg.length());
    });
    initializeSwarmSystem();
    */
}
