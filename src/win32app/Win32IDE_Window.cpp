<<<<<<< HEAD
#include "../bridge/Win32SwarmBridge.h"
#include "Win32IDE.h"
#include <cassert>
#include <commctrl.h>
#include <cstring>
#include <shellscalingapi.h>  // GetDpiForWindow
#include <windowsx.h>

// ORPHAN TRANSLATION UNIT — not listed on the root CMake Win32IDE target (see docs/IDE_MASTER_PROGRESS.md).
// Production WindowProc / onCreate / onDestroy / deferred bootstrap live in Win32IDE_Core.cpp. Do not add
// lifecycle or IPC here expecting the shipped binary to run it; merge into Core if you need this path live.
=======
#include "Win32IDE.h"
#include <windowsx.h>
#include <commctrl.h>
>>>>>>> origin/main

// Window Management Implementation for Win32IDE
// Completes the GUI IDE loop by providing the missing Window Procedure and creation logic.

<<<<<<< HEAD
bool Win32IDE::createWindow()
{
=======
bool Win32IDE::createWindow() {
>>>>>>> origin/main
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32IDE::WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "RawrXD_Win32IDE";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

<<<<<<< HEAD
    if (!RegisterClassExA(&wc))
    {
        // Class might already be registered
        // Check error
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
=======
    if (!RegisterClassExA(&wc)) {
        // Class might already be registered
        // Check error
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
>>>>>>> origin/main
            LOG_ERROR("Failed to register window class");
            return false;
        }
    }

<<<<<<< HEAD
    m_hwndMain = CreateWindowExA(0, "RawrXD_Win32IDE", "RawrXD IDE (Native/No Qt)", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                 CW_USEDEFAULT, 1280, 800, nullptr, nullptr, m_hInstance, this);

    if (!m_hwndMain)
    {
=======
    m_hwndMain = CreateWindowExA(
        0,
        "RawrXD_Win32IDE",
        "RawrXD IDE (Native/No Qt)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
        nullptr, nullptr, m_hInstance, this
    );

    if (!m_hwndMain) {
>>>>>>> origin/main
        LOG_ERROR("Failed to create main window");
        return false;
    }

    return true;
}

<<<<<<< HEAD
void Win32IDE::showWindow()
{
    if (m_hwndMain)
    {
        ShowWindow(m_hwndMain, SW_SHOW);
        UpdateWindow(m_hwndMain);

        // Post-show client-rect verification
        RECT rc = {};
        GetClientRect(m_hwndMain, &rc);
        int cw = rc.right - rc.left;
        int ch = rc.bottom - rc.top;
        if (cw < 400 || ch < 300)
        {
            // Window reported a degenerate client area — force a safe minimum size
            char warn[128];
            snprintf(warn, sizeof(warn), "[showWindow] Degenerate client rect %dx%d — forcing 1280x800\n", cw, ch);
            OutputDebugStringA(warn);
            SetWindowPos(m_hwndMain, HWND_TOP, 0, 0, 1280, 800, SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW);
        }
        else
        {
            char info[128];
            snprintf(info, sizeof(info), "[showWindow] Client rect OK: %dx%d\n", cw, ch);
            OutputDebugStringA(info);
        }
    }
}

int Win32IDE::runMessageLoop()
{
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (m_hAccel && TranslateAccelerator(m_hwndMain, m_hAccel, &msg))
        {
=======
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
>>>>>>> origin/main
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

<<<<<<< HEAD
LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE)
    {
=======
LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
>>>>>>> origin/main
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32IDE*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwndMain = hwnd;
<<<<<<< HEAD
    }
    else
    {
        pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis)
    {
=======
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
>>>>>>> origin/main
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

<<<<<<< HEAD
LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
=======
LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
>>>>>>> origin/main
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
<<<<<<< HEAD

        // Autonomy and Agent Events.
        // Kept as an explicit no-op hook so posted messages are acknowledged.
        case WM_USER + 100:
            LOG_INFO("WM_USER+100 received (reserved no-op)");
            return 0;

        // Peek overlay: deferred navigate + tear-down (posted from PeekOverlayWindow — avoids destroying
        // overlay from inside its own WndProc).
        case WM_RAWRXD_PEEK_FINISH:
            if (m_peekDeferredNavigate && !m_peekDeferredFile.empty() && m_peekDeferredLine > 0)
                navigateToFileLine(m_peekDeferredFile, m_peekDeferredLine);
            m_peekOverlayWindow.reset();
            m_peekOverlayActive = false;
            m_peekDeferredNavigate = false;
            m_peekDeferredFile.clear();
            m_peekDeferredLine = 1;
            return 0;

        // Watchdog show-window request (posted from VisibilityWatchdogThread
        // to avoid cross-thread blocking during shutdown).
        // wParam = SW_* command (SW_SHOW, SW_RESTORE, etc.)
        case WM_APP + 1:
            ShowWindow(hwnd, static_cast<int>(wParam));
            SetForegroundWindow(hwnd);
            return 0;

        // Deferred startup bootstrap: runs after the window is visible so
        // expensive runtime wiring cannot block WM_CREATE / CreateWindowEx.
        case WM_APP + 200:
        {
            LOG_INFO("Deferred startup bootstrap running");

            // Initialize Agentic Bridge for AI features
            initializeAgenticBridge();

            // Register Swarm Bridge with IAT (Closes Slot 20 Gap) and start swarm
            if (RawrXD::Bridge::RegisterSwarmBridgeWithIAT())
            {
                RawrXD::Bridge::SwarmInitConfig config{};
                config.structSize = sizeof(config);
                config.maxSubAgents = 8;
                config.taskTimeoutMs = 30000;
                config.enableGPUWorkStealing = TRUE;
                strcpy_s(config.coordinatorModel, "phi3:mini");
                if (FAILED(RawrXD::Bridge::InitializeSwarmSystem(&config)))
                {
                    OutputDebugStringA("Win32IDE: Swarm system init failed\n");
                }
            }
            else
            {
                OutputDebugStringA("Win32IDE: Swarm bridge registration failed\n");
            }

            // Kickstart Inference Engine (Native)
            if (!initializeInference())
            {
                LOG_ERROR("Failed to initialize Inference Engine on startup");
            }
            else
            {
                LOG_INFO("Inference Engine initialized successfully");
            }

            // OrchestratorBridge startup path removed: initialization now relies on
            // native orchestration flows instead of bridge-based Ollama wiring.

            // Initialize Ghost Text (FIM completions)
            initGhostText();
            LOG_INFO("Ghost text subsystem initialized");

            // Refresh explorer contents after the app is already visible.
            PostMessage(hwnd, WM_APP + 201, 0, 0);
            return 0;
        }

        case WM_APP + 201:
            if (m_hwndSidebar && IsWindow(m_hwndSidebar) && m_hwndExplorerTree)
            {
                refreshFileTree();
            }
            return 0;

        case WM_ACTIVATE:
        {
            WORD state = LOWORD(wParam);
            const char* stateStr = (state == WA_INACTIVE) ? "INACTIVE"
                                   : (state == WA_ACTIVE) ? "ACTIVE"
                                                          : "CLICKACTIVE";
            char dbg[128];
            snprintf(dbg, sizeof(dbg), "[WM_ACTIVATE] state=%s\n", stateStr);
            OutputDebugStringA(dbg);
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_MOVE:
        {
            int x = (int)(short)LOWORD(lParam);
            int y = (int)(short)HIWORD(lParam);
            RECT wa = {};
            SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
            if (x < wa.left - 1200 || y < wa.top - 100 || x > wa.right || y > wa.bottom)
            {
                char warn[128];
                snprintf(warn, sizeof(warn), "[WM_MOVE] Off-screen (%d, %d) -- recentering\n", x, y);
                OutputDebugStringA(warn);
                int sw = GetSystemMetrics(SM_CXSCREEN);
                int sh = GetSystemMetrics(SM_CYSCREEN);
                SetWindowPos(hwnd, nullptr, (sw - 1280) / 2, (sh - 800) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc)
            {
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
void Win32IDE::onCreate(HWND hwnd)
{
    LOG_INFO("Main Window Created: Initializing UI Components");

=======
            
        // Autonomy and Agent Events
        case WM_USER + 100: // Agent Output Update
            // Refresh UI if needed
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::onCreate(HWND hwnd) {
    LOG_INFO("Main Window Created: Initializing UI Components");
    
>>>>>>> origin/main
    // 1. Create Layout Components
    // Activity Bar (Leftmost strip)
    // Note: Assuming createActivityBar or similar is available or handled by createSidebar
    // If not, we rely on createSidebar to handle the left panel.
<<<<<<< HEAD

    // Primary Sidebar (Left Explorer/Search)
    createSidebar(hwnd);

=======
    
    // Primary Sidebar (Left Explorer/Search)
    createSidebar(hwnd);
    
>>>>>>> origin/main
    // Editor Area (Center - RichEdit)
    createEditor(hwnd);

    // Terminal/Panel Area (Bottom)
    createTerminal(hwnd);

    // Secondary Sidebar (Right - Copilot) - Defined in Win32IDE_VSCodeUI.cpp
    // Ensure we utilize the VS Code UI features if available
    createSecondarySidebar(hwnd);
<<<<<<< HEAD

    // Status Bar (Bottom strip)
    createStatusBar(hwnd);

    // 3. Post-Creation Setup
    // Initialize default focus
    if (m_hwndEditor)
    {
=======
    
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
>>>>>>> origin/main
        SetFocus(m_hwndEditor);
    }

    // 4. Create keyboard accelerator table for Build shortcuts
    createAcceleratorTable();
<<<<<<< HEAD

    // Defer heavy runtime initialization until after the window is shown and
    // the message loop is running, so WM_CREATE does not block the launch path.
    PostMessage(hwnd, WM_APP + 200, 0, 0);
}

void Win32IDE::onDestroy()
{
    m_shuttingDown.store(true, std::memory_order_release);

    // Shutdown Ghost Text before window teardown
    shutdownGhostText();

    RawrXD::Bridge::ShutdownSwarmSystem();

    // Parity-audit: stop watchdog before anything else so it can't race on
    // the HWND members we're about to tear down.
    stopVisibilityWatchdog();

    // Cleanup Resources
    if (m_agenticBridge)
    {
        m_agenticBridge->StopAgentLoop();
    }
    if (m_hAccel)
    {
        DestroyAcceleratorTable(m_hAccel);
        m_hAccel = nullptr;
    }
    shutdownLogging();
}

void Win32IDE::onSize(int width, int height)
{
    // ── Parity-audit: dimension guards ──────────────────────────────────────
    // Clamp to safe minimums so layout arithmetic never produces negatives.
    // The assert fires in Debug builds; the clamp protects Release builds.
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    width = (width < 400) ? 400 : width;
    height = (height < 300) ? 300 : height;
    assert(width >= 400);
    assert(height >= 300);

    // DPI-aware scaling
    UINT dpi = GetDpiForWindow(m_hwndMain ? m_hwndMain : GetDesktopWindow());
    if (dpi == 0)
        dpi = 96;  // fallback: 100% scaling
    float dpiScale = (float)dpi / 96.0f;

    // Debug trace (visible in DebugView / VS Output)
    {
        char dbg[128];
        snprintf(dbg, sizeof(dbg), "[WM_SIZE] %dx%d  DPI=%u (%.2fx)\n", width, height, dpi, dpiScale);
        OutputDebugStringA(dbg);
    }

    // Layout constants
    int statusBarHeight = (int)(25 * dpiScale);
    int terminalHeight = (int)(200 * dpiScale);
    int activityBarWidth = (int)(48 * dpiScale);
    int sidebarWidth = (int)(250 * dpiScale);
    int secondarySidebarWidth = (int)(320 * dpiScale);

=======
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
    
>>>>>>> origin/main
    // Adjust logic based on visibility
    bool showSidebar = (m_hwndSidebar != nullptr && IsWindowVisible(m_hwndSidebar));
    bool showSecondary = (m_hwndSecondarySidebar != nullptr && IsWindowVisible(m_hwndSecondarySidebar));
    bool showTerminal = (m_hwndPowerShellPanel != nullptr && IsWindowVisible(m_hwndPowerShellPanel));
<<<<<<< HEAD

    int currentX = 0;

    // 1. Activity Bar (Far Left)
    int workspaceX = 0;
    int workspaceWidth = width;

    if (showSidebar)
    {
        MoveWindow(m_hwndSidebar, activityBarWidth, 0, sidebarWidth, height - statusBarHeight, TRUE);
        workspaceX += (activityBarWidth + sidebarWidth);
        workspaceWidth -= (activityBarWidth + sidebarWidth);
    }

    // Secondary Sidebar Area
    if (showSecondary)
    {
        MoveWindow(m_hwndSecondarySidebar, width - secondarySidebarWidth, 0, secondarySidebarWidth,
                   height - statusBarHeight, TRUE);
        workspaceWidth -= secondarySidebarWidth;
    }

    // Terminal Area
    int editorHeight = height - statusBarHeight;
    if (showTerminal)
    {
        editorHeight -= terminalHeight;
        MoveWindow(m_hwndPowerShellPanel, workspaceX, editorHeight, workspaceWidth, terminalHeight, TRUE);
    }

    // Editor Area
    if (m_hwndEditor)
    {
        MoveWindow(m_hwndEditor, workspaceX, 0, workspaceWidth, editorHeight, TRUE);
    }

    // Status Bar
    if (m_hwndStatusBar)
    {
        MoveWindow(m_hwndStatusBar, 0, height - statusBarHeight, width, statusBarHeight, TRUE);
        SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0);  // specific for statusbar control
    }
}
=======
    
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
>>>>>>> origin/main
