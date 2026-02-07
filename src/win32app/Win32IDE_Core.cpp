// ============================================================================
// Win32IDE_Core.cpp - Core Window Management Functions
// createWindow, showWindow, runMessageLoop, ~Win32IDE, onSize,
// syncEditorToGpuSurface, initializeEditorSurface, trySendToOllama,
// createReverseEngineeringMenu, handleReverseEngineering* stubs
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include "IDEConfig.h"
#include "ModelConnection.h"
#include "../cpu_inference_engine.h"
#include "../modules/native_memory.hpp"
#include "../modules/ExtensionLoader.hpp"
#include "../native_agent.hpp"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

// Menu command IDs — must match Win32IDE.cpp definitions
#ifndef IDM_FILE_NEW
#define IDM_FILE_NEW 2001
#endif
#ifndef IDM_FILE_OPEN
#define IDM_FILE_OPEN 2002
#endif
#ifndef IDM_FILE_SAVE
#define IDM_FILE_SAVE 2003
#endif
#ifndef IDM_FILE_SAVEAS
#define IDM_FILE_SAVEAS 2004
#endif
#ifndef IDM_EDIT_FIND
#define IDM_EDIT_FIND 2016
#endif
#ifndef IDM_EDIT_REPLACE
#define IDM_EDIT_REPLACE 2017
#endif

// ============================================================================
// Window Class Name
// ============================================================================
static const char* kWindowClassName = "RawrXD_IDE_MainWindow";

// ============================================================================
// Destructor
// ============================================================================
Win32IDE::~Win32IDE() {
    if (m_backgroundBrush) {
        DeleteObject(m_backgroundBrush);
        m_backgroundBrush = nullptr;
    }
    if (m_editorFont) {
        DeleteObject(m_editorFont);
        m_editorFont = nullptr;
    }
    // AgenticBridge and AutonomyManager are unique_ptrs, auto-destroyed
    // Renderer is unique_ptr, auto-destroyed
}

// ============================================================================
// WindowProc - Static callback that routes to instance handleMessage
// ============================================================================
LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<Win32IDE*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hwndMain = hwnd;
    } else {
        pThis = reinterpret_cast<Win32IDE*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// handleMessage - Instance message handler
// ============================================================================
LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        onCreate(hwnd);
        return 0;

    case WM_SIZE: {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        if (width > 0 && height > 0) {
            onSize(width, height);
            // Visible line range changed — trigger recoloring for the new viewport
            onEditorContentChanged();
        }
        return 0;
    }

    case WM_GETMINMAXINFO:
    case WM_NCCALCSIZE:
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
    case WM_ACTIVATEAPP:
    case WM_ACTIVATE:
    case WM_NCACTIVATE:
    case WM_KILLFOCUS:
    case WM_IME_SETCONTEXT:
    case WM_IME_NOTIFY:
    case WM_SHOWWINDOW:
    case WM_NCPAINT:
        return DefWindowProcA(hwnd, uMsg, wParam, lParam);

    case WM_SETFOCUS:
        // Forward focus to the editor so the caret appears and keyboard input works
        if (m_hwndEditor && IsWindow(m_hwndEditor)) {
            SetFocus(m_hwndEditor);
        }
        return 0;

    case WM_ERASEBKGND: {
        // Paint the background dark instead of default white
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        if (!m_backgroundBrush) {
            m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
        }
        FillRect(hdc, &rc, m_backgroundBrush);
        return 1; // We handled it
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (!m_backgroundBrush) {
            m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
        }
        FillRect(hdc, &ps.rcPaint, m_backgroundBrush);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND:
        onCommand(hwnd, LOWORD(wParam), (HWND)lParam, HIWORD(wParam));
        return 0;

    case WM_NOTIFY: {
        NMHDR* pNMHDR = reinterpret_cast<NMHDR*>(lParam);
        if (pNMHDR) {
            // Handle tab bar selection change
            if (pNMHDR->code == TCN_SELCHANGE && pNMHDR->hwndFrom == m_hwndTabBar) {
                onTabChanged();
            }
            // Handle RichEdit scroll/change notifications for line number sync
            if (pNMHDR->hwndFrom == m_hwndEditor) {
                if (pNMHDR->code == EN_VSCROLL || pNMHDR->code == EN_SELCHANGE ||
                    pNMHDR->code == EN_CHANGE) {
                    updateLineNumbers();
                    // Debounce syntax coloring on content change
                    if (pNMHDR->code == EN_CHANGE && m_syntaxColoringEnabled) {
                        onEditorContentChanged();
                    }
                    // Dismiss ghost text when caret moves (anchor becomes stale)
                    if (pNMHDR->code == EN_SELCHANGE && m_ghostTextVisible) {
                        dismissGhostText();
                    }
                    // Update status bar cursor position
                    CHARRANGE sel;
                    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
                    int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
                    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
                    int col = sel.cpMin - lineStart;
                    char posBuf[64];
                    snprintf(posBuf, sizeof(posBuf), "Ln %d, Col %d", line + 1, col + 1);
                    if (m_hwndStatusBar) {
                        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)posBuf);
                    }
                }
            }
        }
        return 0;
    }

    case WM_HSCROLL:
    case WM_VSCROLL:
        // Forward scrollbar messages and update line numbers
        {
            LRESULT result = DefWindowProcA(hwnd, uMsg, wParam, lParam);
            updateLineNumbers();
            // Recolor visible lines on scroll
            if (m_syntaxColoringEnabled) {
                onEditorContentChanged();
            }
            return result;
        }

    case WM_TIMER:
        if (wParam == SYNTAX_COLOR_TIMER_ID) {
            // Handled by SyntaxColorTimerProc callback — this is a fallback
            KillTimer(hwnd, SYNTAX_COLOR_TIMER_ID);
            if (m_syntaxColoringEnabled) {
                applySyntaxColoring();
            }
            return 0;
        }
        if (wParam == 8888) { // GHOST_TEXT_TIMER_ID
            onGhostTextTimer();
            return 0;
        }
        if (wParam == MODEL_PROGRESS_TIMER_ID) {
            // Poll model progress and update the progress bar UI
            if (m_modelOperationActive.load()) {
                float pct = m_modelProgressPercent.load();
                if (m_hwndModelProgressBar) {
                    SendMessage(m_hwndModelProgressBar, PBM_SETPOS, (WPARAM)(int)(pct * 10.0f), 0);
                }
                std::string status;
                {
                    std::lock_guard<std::mutex> lock(m_modelProgressMutex);
                    status = m_modelProgressStatus;
                }
                if (m_hwndModelProgressLabel && !status.empty()) {
                    SetWindowTextA(m_hwndModelProgressLabel, status.c_str());
                }
            } else {
                hideModelProgressBar();
            }
            return 0;
        }
        if (wParam == 8888) { // GHOST_TEXT_TIMER_ID
            onGhostTextTimer();
            return 0;
        }
        if (wParam == 42) { // IDT_STATUS_FLASH
            KillTimer(hwnd, 42);
            // Restore default status bar text
            if (m_hwndStatusBar) {
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Ready");
            }
            return 0;
        }
        break;

    case WM_CLOSE:
        if (!m_fileModified || promptSaveChanges()) {
            DestroyWindow(hwnd);
        }
        return 0;

    // Model progress update from background thread
    case WM_APP + 300: { // WM_MODEL_PROGRESS_UPDATE
        float pct = (float)wParam / 10.0f;
        if (m_hwndModelProgressBar) {
            SendMessage(m_hwndModelProgressBar, PBM_SETPOS, wParam, 0);
        }
        return 0;
    }
    case WM_APP + 301: { // WM_MODEL_PROGRESS_DONE
        hideModelProgressBar();
        return 0;
    }

    // Ghost Text delivery from background completion thread
    case WM_GHOST_TEXT_READY: {
        int cursorPos = (int)wParam;
        const char* text = reinterpret_cast<const char*>(lParam);
        onGhostTextReady(cursorPos, text);
        if (text) free(const_cast<char*>(text));  // Allocated with _strdup
        return 0;
    }

    case WM_DESTROY:
        onDestroy();
        PostQuitMessage(0);
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        // Dark mode colors for controls
        HDC hdcCtrl = (HDC)wParam;
        SetTextColor(hdcCtrl, RGB(220, 220, 220));
        SetBkColor(hdcCtrl, RGB(30, 30, 30));
        if (!m_backgroundBrush) {
            m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
        }
        return (LRESULT)m_backgroundBrush;
    }

    default:
        // Handle deferred heavy initialization (posted from onCreate)
        if (uMsg == WM_APP + 100) {
            deferredHeavyInit();
            return 0;
        }
        // Handle background init completion — refresh UI
        if (uMsg == WM_APP + 101) {
            applyTheme();
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
            return 0;
        }
        // Handle "load downloaded model" signal from background download threads
        // (HuggingFace / URL downloads complete, m_loadedModelPath already set)
        if (uMsg == WM_APP + 201) {
            if (!m_loadedModelPath.empty()) {
                std::string pathToLoad = m_loadedModelPath;
                appendToOutput("Loading downloaded model: " + pathToLoad + "\n", "Output", OutputSeverity::Info);
                if (loadGGUFModel(pathToLoad)) {
                    loadModelForInference(pathToLoad);
                    appendToOutput("Downloaded model loaded successfully!\n", "Output", OutputSeverity::Info);
                } else {
                    appendToOutput("Failed to load downloaded model: " + pathToLoad + "\n", "Errors", OutputSeverity::Error);
                }
            }
            return 0;
        }
        // Handle custom agent output message
        if (uMsg == WM_AGENT_OUTPUT_SAFE) {
            const char* text = reinterpret_cast<const char*>(lParam);
            if (text) {
                onAgentOutput(text);
                free(const_cast<char*>(text));
            }
            return 0;
        }
        // Handle Ghost Text completion delivery from background thread
        if (uMsg == WM_GHOST_TEXT_READY) {
            const char* completionText = reinterpret_cast<const char*>(lParam);
            onGhostTextReady((int)wParam, completionText);
            if (completionText) free(const_cast<char*>(completionText));
            return 0;
        }
        // Handle Plan Executor messages
        if (uMsg == WM_PLAN_READY) {
            onPlanReady((int)wParam, reinterpret_cast<PlanStep*>(lParam));
            return 0;
        }
        if (uMsg == WM_PLAN_STEP_DONE) {
            onPlanStepDone((int)wParam, (int)lParam);
            return 0;
        }
        if (uMsg == WM_PLAN_COMPLETE) {
            onPlanComplete(wParam != 0);
            return 0;
        }
        // Plan step status update from background thread (live dialog update)
        if (uMsg == WM_APP + 503) {
            updatePlanStepInDialog((int)wParam, static_cast<PlanStepStatus>((int)lParam));
            return 0;
        }
        // Agent History replay step completion
        if (uMsg == WM_AGENT_HISTORY_REPLAY_DONE) {
            onReplayStepDone((int)wParam, (int)lParam);
            return 0;
        }
        break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// createWindow - Register class and create main window
// ============================================================================
bool Win32IDE::createWindow() {
    // ====================================================================
    // Enterprise: Load external configuration before window creation
    // ====================================================================
    {
        auto& config = IDEConfig::getInstance();
        // Try workspace config, then user config, then defaults
        std::string configPath = "rawrxd.config.json";
        if (!config.loadFromFile(configPath)) {
            // Try in exe directory
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string exeDir(exePath);
            size_t lastSlash = exeDir.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                exeDir = exeDir.substr(0, lastSlash);
                config.loadFromFile(exeDir + "\\rawrxd.config.json");
            }
        }
        // Apply environment variable overrides (RAWRXD_* prefix)
        config.applyEnvironmentOverrides();
        // Initialize feature toggles from config
        config.applyFeatureToggles();

        // Apply config to IDE state
        m_ollamaBaseUrl = config.getString("ollama.baseUrl", "http://localhost:11434");
        m_ollamaModelOverride = config.getString("ollama.modelOverride", "");
        m_autoSaveEnabled = config.getBool("editor.autoSave", false);
        m_gpuTextEnabled = config.getBool("performance.gpuTextRendering", true);
        m_useStreamingLoader = config.getBool("performance.streamingGGUFLoad", true);
        m_useVulkanRenderer = config.getBool("performance.vulkanRenderer", false);

        LOG_INFO("Configuration loaded — " + std::to_string(config.getAllKeys().size()) + " keys");
        METRICS.increment("config.loads_total");
    }

    // Load RichEdit libraries — need both for RICHEDIT_CLASSA and MSFTEDIT_CLASS
    LoadLibraryA("riched20.dll");
    LoadLibraryA("msftedit.dll");

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32IDE::WindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = kWindowClassName;
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        // Class might already be registered
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            LOG_ERROR("Failed to register window class");
            return false;
        }
    }

    // Create the main window
    m_hwndMain = CreateWindowExA(
        WS_EX_APPWINDOW,
        kWindowClassName,
        "RawrXD IDE - Native Win32 AI Development Environment",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1600, 1000,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwndMain) {
        LOG_ERROR("Failed to create main window");
        return false;
    }

    LOG_INFO("Main window created successfully");
    return true;
}

// ============================================================================
// showWindow - Show and update the main window
// ============================================================================
void Win32IDE::showWindow() {
    if (m_hwndMain) {
        ShowWindow(m_hwndMain, SW_SHOW);
        UpdateWindow(m_hwndMain);
    }
}

// ============================================================================
// runMessageLoop - Standard Win32 message loop with accelerators
// ============================================================================
int Win32IDE::runMessageLoop() {
    LOG_INFO("Message loop starting");
    METRICS.increment("app.message_loop_starts");
    auto loopStart = std::chrono::high_resolution_clock::now();

    MSG msg = {};
    try {
    while (GetMessage(&msg, nullptr, 0, 0)) {
        METRICS.increment("app.messages_processed");

        // Handle accelerator keys
        if (msg.message == WM_KEYDOWN) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            if (ctrl && shift && msg.wParam == 'P') {
                if (m_commandPaletteVisible) {
                    hideCommandPalette();
                } else {
                    showCommandPalette();
                }
                continue;
            }
            if (ctrl && msg.wParam == 'N') { routeCommand(IDM_FILE_NEW); continue; }
            if (ctrl && msg.wParam == 'O') { routeCommand(IDM_FILE_OPEN); continue; }
            if (ctrl && msg.wParam == 'S') {
                if (shift) routeCommand(IDM_FILE_SAVEAS);
                else routeCommand(IDM_FILE_SAVE);
                continue;
            }
            if (ctrl && msg.wParam == 'F') { routeCommand(IDM_EDIT_FIND); continue; }
            if (ctrl && msg.wParam == 'H') { routeCommand(IDM_EDIT_REPLACE); continue; }
            if (ctrl && msg.wParam == 'B') { toggleSidebar(); continue; }
            // Ctrl+, → Settings dialog
            if (ctrl && msg.wParam == VK_OEM_COMMA) { showSettingsDialog(); continue; }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    } catch (const std::exception& e) {
        // Centralized exception handler — prevents unhandled crash
        LOG_CRITICAL(std::string("Unhandled exception in message loop: ") + e.what());
        METRICS.increment("app.unhandled_exceptions");
        MessageBoxA(nullptr,
                    (std::string("RawrXD IDE encountered an error:\n") + e.what()).c_str(),
                    "RawrXD IDE - Critical Error", MB_ICONERROR | MB_OK);
    } catch (...) {
        LOG_CRITICAL("Unknown unhandled exception in message loop");
        METRICS.increment("app.unhandled_exceptions");
        MessageBoxA(nullptr, "RawrXD IDE encountered an unknown error.",
                    "RawrXD IDE - Critical Error", MB_ICONERROR | MB_OK);
    }

    // Log session metrics on exit
    auto loopEnd = std::chrono::high_resolution_clock::now();
    double sessionMs = std::chrono::duration<double, std::milli>(loopEnd - loopStart).count();
    METRICS.recordDuration("app.session_duration_ms", sessionMs);
    LOG_INFO("Message loop ended — session duration: " + std::to_string(sessionMs / 1000.0) + "s");
    LOG_INFO("Messages processed: " + std::to_string(METRICS.getCounter("app.messages_processed")));

    // Save configuration on exit
    IDEConfig::getInstance().saveToFile("rawrxd.config.json");

    return static_cast<int>(msg.wParam);
}

// ============================================================================
// onSize - Layout all child windows when the main window is resized
// ============================================================================
void Win32IDE::onSize(int width, int height) {
    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "onSize: %dx%d (Explorer: %p, Terminal: %p)", width, height, m_hwndFileExplorer, m_hwndPowerShellPanel);
    LOG_INFO(std::string(logBuf));

    if (width <= 0 || height <= 0) return;

    const int TOOLBAR_HEIGHT = 32;
    const int STATUSBAR_HEIGHT = 24;
    const int ACTIVITY_BAR_WIDTH = 48;
    const int TAB_BAR_HEIGHT = 28;

    int sidebarWidth = m_sidebarVisible ? m_sidebarWidth : 0;
    int secondarySidebarWidth = m_secondarySidebarVisible ? 320 : 0;
    // Only reserve space for panels that actually have HWNDs created
    int panelHeight = (m_outputPanelVisible && m_hwndOutputTabs) ? m_outputTabHeight : 0;
    int powerShellHeight = (m_powerShellPanelVisible && m_hwndPowerShellPanel) ? m_powerShellPanelHeight : 0;

    int contentTop = TOOLBAR_HEIGHT;
    int contentBottom = height - STATUSBAR_HEIGHT;
    int contentHeight = contentBottom - contentTop;

    // Status bar
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0);
    }

    // Toolbar
    if (m_hwndToolbar) {
        MoveWindow(m_hwndToolbar, 0, 0, width, TOOLBAR_HEIGHT, TRUE);
    }

    // Activity bar (far left)
    if (m_hwndActivityBar) {
        MoveWindow(m_hwndActivityBar, 0, contentTop, ACTIVITY_BAR_WIDTH, contentHeight, TRUE);
    }

    // Primary sidebar
    if (m_hwndSidebar && m_sidebarVisible) {
        MoveWindow(m_hwndSidebar, ACTIVITY_BAR_WIDTH, contentTop, sidebarWidth, contentHeight, TRUE);
    }

    // Calculate editor area
    int editorLeft = ACTIVITY_BAR_WIDTH + sidebarWidth;
    int editorRight = width - secondarySidebarWidth;
    int editorWidth = editorRight - editorLeft;
    int editorAreaHeight = contentHeight - panelHeight - powerShellHeight;

    // Tab bar (above editor)
    int tabBarBottom = contentTop;
    if (m_hwndTabBar) {
        MoveWindow(m_hwndTabBar, editorLeft, contentTop, editorWidth, TAB_BAR_HEIGHT, TRUE);
        tabBarBottom = contentTop + TAB_BAR_HEIGHT;
    }

    int editorContentHeight = editorAreaHeight - (tabBarBottom - contentTop);

    // Line number gutter (left of editor)
    int gutterWidth = m_hwndLineNumbers ? m_lineNumberWidth : 0;
    if (m_hwndLineNumbers) {
        MoveWindow(m_hwndLineNumbers, editorLeft, tabBarBottom, gutterWidth, editorContentHeight, TRUE);
    }

    // Editor (right of gutter)
    if (m_hwndEditor) {
        int minimapW = (m_minimapVisible && m_hwndMinimap) ? m_minimapWidth : 0;
        int editorX = editorLeft + gutterWidth;
        int editorW = editorWidth - gutterWidth - minimapW;
        MoveWindow(m_hwndEditor, editorX, tabBarBottom, editorW, editorContentHeight, TRUE);

        // Minimap
        if (m_hwndMinimap && m_minimapVisible) {
            MoveWindow(m_hwndMinimap, editorRight - minimapW, tabBarBottom, minimapW, editorContentHeight, TRUE);
        }
    }

    // Output / Terminal panel area
    int panelTop = contentTop + editorAreaHeight;
    if (panelHeight > 0) {
        // Output tabs
        if (m_hwndOutputTabs) {
            MoveWindow(m_hwndOutputTabs, editorLeft, panelTop, editorWidth, panelHeight, TRUE);
        }
        panelTop += panelHeight;
    }

    // Terminal panes — layout within the output panel area so they're visible
    if (!m_terminalPanes.empty() && panelHeight > 0) {
        int termTop = contentTop + editorAreaHeight;
        int termHeight = panelHeight;
        // If output tabs are visible, terminal panes share the same region
        // Place them below the tab control bar (24px)
        int tabBarH = 24;
        if (m_hwndOutputTabs) {
            // Output tab windows (General, Errors, Debug, Find Results)
            for (auto& kv : m_outputWindows) {
                if (kv.second) {
                    MoveWindow(kv.second, editorLeft, termTop + tabBarH,
                               editorWidth, termHeight - tabBarH, TRUE);
                }
            }
        }
    }

    // PowerShell panel
    if (m_hwndPowerShellPanel && m_powerShellPanelVisible) {
        MoveWindow(m_hwndPowerShellPanel, editorLeft, panelTop, editorWidth, powerShellHeight, TRUE);
        // Also layout internal PowerShell controls
        updatePowerShellPanelLayout(editorWidth, powerShellHeight);
    }

    // Secondary sidebar (Copilot Chat / AI Panel)
    if (m_hwndSecondarySidebar && m_secondarySidebarVisible) {
        MoveWindow(m_hwndSecondarySidebar, editorRight, contentTop, secondarySidebarWidth, contentHeight, TRUE);
    }

    // Update line numbers after layout
    updateLineNumbers();

    // Store editor rect for GPU surface sync
    m_editorRect = {editorLeft, contentTop, editorLeft + editorWidth, contentTop + editorAreaHeight};
}

// ============================================================================
// syncEditorToGpuSurface - Sync RichEdit content to GPU-accelerated overlay
// ============================================================================
void Win32IDE::syncEditorToGpuSurface() {
    if (!m_renderer || !m_rendererReady || !m_hwndEditor) return;

    // The transparent renderer overlays the editor for GPU-accelerated effects.
    // If the renderer isn't initialized yet, this is a safe no-op.
    try {
        RECT editorRect;
        GetClientRect(m_hwndEditor, &editorRect);
        // Renderer will pick up content on next paint cycle
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    } catch (...) {
        // Swallow errors — GPU sync is optional enhancement
    }
}

// ============================================================================
// initializeEditorSurface - Set up the GPU rendering surface for the editor
// ============================================================================
void Win32IDE::initializeEditorSurface() {
    if (!m_renderer || !m_hwndEditor) return;

    try {
        m_rendererReady = m_renderer->Initialize(m_hwndEditor);
        if (m_rendererReady) {
            LOG_INFO("Editor GPU surface initialized");
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Editor surface init failed: ") + e.what());
        m_rendererReady = false;
    } catch (...) {
        LOG_ERROR("Editor surface init failed with unknown error");
        m_rendererReady = false;
    }
}

// ============================================================================
// trySendToOllama - Attempt to send a prompt to Ollama and get a response
// ============================================================================
bool Win32IDE::trySendToOllama(const std::string& prompt, std::string& outResponse) {
    try {
        ModelConnection conn(m_ollamaBaseUrl.empty() ? "http://localhost:11434" : m_ollamaBaseUrl);

        if (!conn.checkConnection()) {
            return false;
        }

        std::string modelTag = m_ollamaModelOverride;
        if (modelTag.empty()) {
            // Use the currently loaded model's filename as tag
            if (!m_loadedModelPath.empty()) {
                std::string filename = m_loadedModelPath;
                auto pos = filename.find_last_of("/\\");
                if (pos != std::string::npos) filename = filename.substr(pos + 1);
                // Strip .gguf extension
                auto extPos = filename.rfind(".gguf");
                if (extPos != std::string::npos) filename = filename.substr(0, extPos);
                modelTag = filename;
            } else {
                modelTag = "llama3.2:latest";
            }
        }

        // Synchronous send for simplicity — uses sendPrompt internally
        bool gotResponse = false;
        std::string responseText;
        std::mutex mtx;
        std::condition_variable cv;

        conn.sendPrompt(modelTag, prompt, {},
            [&](const std::string& token) {
                std::lock_guard<std::mutex> lock(mtx);
                responseText += token;
            },
            [&](const std::string& error) {
                std::lock_guard<std::mutex> lock(mtx);
                responseText = "[Error] " + error;
                gotResponse = true;
                cv.notify_one();
            },
            [&]() {
                std::lock_guard<std::mutex> lock(mtx);
                gotResponse = true;
                cv.notify_one();
            });

        // Wait up to 60 seconds
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(60), [&]() { return gotResponse; });

        if (!responseText.empty()) {
            outResponse = responseText;
            return true;
        }

        return false;
    } catch (const std::exception& e) {
        outResponse = std::string("[Error] ") + e.what();
        return false;
    } catch (...) {
        outResponse = "[Error] Unknown exception in Ollama communication";
        return false;
    }
}

// ============================================================================
// Reverse Engineering methods are implemented in Win32IDE_ReverseEngineering.cpp
// ============================================================================

// ============================================================================
// onCreate - Called when WM_CREATE is received
// ============================================================================
void Win32IDE::onCreate(HWND hwnd) {
    m_hwndMain = hwnd;

    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES
               | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    // ================================================================
    // Create UI components one by one with MessageBox diagnostics
    // (C++ try/catch can't catch SEH crashes on MinGW)
    // ================================================================
    createMenuBar(hwnd);
    createToolbar(hwnd);

    // Full VS Code-style sidebar with Activity Bar + Explorer/Search/SCM/Debug/Extensions
    createActivityBar(hwnd);
    createPrimarySidebar(hwnd);

    createTabBar(hwnd);
    createLineNumberGutter(hwnd);
    createEditor(hwnd);
    createTerminal(hwnd);
    createStatusBar(hwnd);

    // ================================================================
    // Create bottom panels — these were previously missing, causing
    // the output and terminal panes to be invisible (flags=true but
    // HWNDs were nullptr).
    // ================================================================
    createOutputTabs();
    createPowerShellPanel();

    LOG_INFO("onCreate complete — all panels created");

    // Initialize syntax coloring engine
    initSyntaxColorizer();

    // Initialize ghost text / inline completions engine
    initGhostText();

    // Restore previous session (tabs, cursor, panel state)
    restoreSession();

    {
        char buf[512];
        sprintf_s(buf, "HWND audit: Main=%p Editor=%p Sidebar=%p "
                       "ExplorerTree=%p OutputTabs=%p PowerShellPanel=%p",
                  m_hwndMain, m_hwndEditor, m_hwndSidebar,
                  m_hwndExplorerTree, m_hwndOutputTabs, m_hwndPowerShellPanel);
        LOG_INFO(std::string(buf));
    }

    // Apply dark theme immediately (lightweight — just sets color values + SendMessage)
    populateBuiltinThemes();  // Register all 16 built-in themes
    m_currentTheme.backgroundColor = RGB(30,30,30);
    m_currentTheme.textColor = RGB(212,212,212);
    m_currentTheme.selectionColor = RGB(38,79,120);
    m_currentTheme.lineNumberColor = RGB(128,128,128);
    if (m_backgroundBrush) DeleteObject(m_backgroundBrush);
    m_backgroundBrush = CreateSolidBrush(RGB(30, 30, 30));
    applyTheme();

    // Update status bar with initial state
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Ready");
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"Ln 1, Col 1");
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)"UTF-8");
    }

    // Defer heavy init to after window is fully created
    PostMessage(hwnd, WM_APP + 100, 0, 0);
}

// ============================================================================
// deferredHeavyInit - Heavy initialization run AFTER window is fully created
// This runs outside CreateWindowExA, so SEH crashes here won't prevent
// the window from appearing.
// ============================================================================
void Win32IDE::deferredHeavyInit() {
    // Run heavy initialization on a background thread so the UI stays responsive.
    // Any UI updates from here must use PostMessage back to the main thread.
    std::thread([this]() {
        // Initialize logger
        try {
            IDELogger::getInstance().initialize("C:\\RawrXD_IDE.log");
        } catch (...) {
            OutputDebugStringA("ERROR: Logger init failed\n");
        }

        // Initialize Native CPU Inference Engine
        try {
            m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
            auto memPlugin = std::make_shared<RawrXD::Modules::NativeMemoryModule>();
            m_nativeEngine->RegisterMemoryPlugin(memPlugin);
            m_nativeEngineLoaded = true;
        } catch (...) {
            m_nativeEngine.reset();
            m_nativeEngineLoaded = false;
            OutputDebugStringA("ERROR: CPUInferenceEngine init failed\n");
        }

        // Initialize DirectX renderer (needs to be on UI thread ideally, but creation is OK)
        try {
            m_renderer = std::make_unique<TransparentRenderer>();
        } catch (...) {
            m_renderer = nullptr;
            OutputDebugStringA("ERROR: TransparentRenderer creation failed\n");
        }

        // Initialize PowerShell state
        try {
            initializePowerShellState();
        } catch (...) {
            OutputDebugStringA("ERROR: PowerShell init failed\n");
        }

        // Theme already applied in onCreate — skip here

        // Load code snippets
        try {
            loadCodeSnippets();
        } catch (...) {
            OutputDebugStringA("ERROR: Code snippets loading failed\n");
        }

        // Initialize Agent
        try {
            if (m_nativeEngine) {
                m_agent = std::make_unique<RawrXD::NativeAgent>(m_nativeEngine.get());
                m_agent->SetOutputCallback([this](const std::string& text) {
                    postAgentOutputSafe(text);
                });
            }
        } catch (...) {
            OutputDebugStringA("ERROR: NativeAgent init failed\n");
        }

        // Initialize Extension Loader
        try {
            m_extensionLoader = std::make_unique<RawrXD::ExtensionLoader>();
            m_extensionLoader->Scan();
            m_extensionLoader->LoadNativeModules();
        } catch (...) {
            OutputDebugStringA("ERROR: ExtensionLoader init failed\n");
        }

        // Initialise the agentic bridge (needs m_hwndMain, which is set)
        try {
            initializeAgenticBridge();
        } catch (...) {
            OutputDebugStringA("ERROR: initializeAgenticBridge failed\n");
        }

        // Initialize Ghost Text renderer (Copilot-style inline completions)
        try {
            initGhostText();
        } catch (...) {
            OutputDebugStringA("ERROR: initGhostText failed\n");
        }

        // Initialize Failure Detector (agent self-correction)
        try {
            initFailureDetector();
        } catch (...) {
            OutputDebugStringA("ERROR: initFailureDetector failed\n");
        }

        // Load persistent settings from %APPDATA%\RawrXD\settings.json
        try {
            loadSettings();
            applySettings();
        } catch (...) {
            OutputDebugStringA("ERROR: loadSettings/applySettings failed\n");
        }

        // Initialize Agent History (append-only JSONL event log)
        try {
            initAgentHistory();
        } catch (...) {
            OutputDebugStringA("ERROR: initAgentHistory failed\n");
        }

        // Initialize Failure Intelligence — Phase 6 (classification + retry strategies)
        try {
            initFailureIntelligence();
        } catch (...) {
            OutputDebugStringA("ERROR: initFailureIntelligence failed\n");
        }

        // Initialize Unified Model Source Resolver (HuggingFace, Ollama blobs, HTTP, local)
        try {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
            // Set cache directory for downloaded models
            m_modelResolver->SetCacheDirectory(
                m_modelResolver->GetCacheDirectory()); // Use default: %USERPROFILE%/.cache/rawrxd/models
            OutputDebugStringA("ModelSourceResolver initialized OK\n");
        } catch (const std::exception& e) {
            m_modelResolver.reset();
            OutputDebugStringA("ERROR: ModelSourceResolver init failed: ");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
        } catch (...) {
            m_modelResolver.reset();
            OutputDebugStringA("ERROR: ModelSourceResolver init failed (unknown)\n");
        }

        OutputDebugStringA("deferredHeavyInit complete (background thread)\n");

        // Notify UI thread to refresh
        if (m_hwndMain) {
            PostMessage(m_hwndMain, WM_APP + 101, 0, 0);
        }
    }).detach();
}

// ============================================================================
// onDestroy - Called when WM_DESTROY is received
// ============================================================================
void Win32IDE::onDestroy() {
    LOG_INFO("Win32IDE::onDestroy - shutting down");

    // Shutdown ghost text renderer (kill timers, free font)
    shutdownGhostText();

    // Stop local GGUF HTTP server
    stopLocalServer();

    // Shutdown agent history (flush event buffer to disk)
    shutdownAgentHistory();

    // Save settings to disk
    try { saveSettings(); } catch (...) {}

    // Save full session state for next launch
    saveSession();

    // Clean up resources
    if (m_renderer) {
        m_renderer.reset();
    }

    // Save any unsaved state / editor config for session restore
    try {
        nlohmann::json session;

        // Save open file path
        if (!m_currentFile.empty()) {
            session["lastOpenFile"] = m_currentFile;
        }

        // Save window position and size
        if (m_hwndMain) {
            RECT rc;
            if (GetWindowRect(m_hwndMain, &rc)) {
                session["window"]["x"] = (int)rc.left;
                session["window"]["y"] = (int)rc.top;
                session["window"]["width"] = (int)(rc.right - rc.left);
                session["window"]["height"] = (int)(rc.bottom - rc.top);
            }
            session["window"]["maximized"] = (IsZoomed(m_hwndMain) != 0);
        }

        // Save open tabs
        nlohmann::json tabs = nlohmann::json::array();
        for (const auto& tab : m_editorTabs) {
            nlohmann::json t;
            t["path"] = tab.filePath;
            t["name"] = tab.displayName;
            t["modified"] = tab.modified;
            tabs.push_back(t);
        }
        session["tabs"] = tabs;

        // Save current working directory
        if (!m_currentDirectory.empty()) {
            session["workingDirectory"] = m_currentDirectory;
        }

        // Save sidebar width / active view
        session["sidebar"]["activeView"] = static_cast<int>(m_currentSidebarView);
        session["sidebar"]["width"] = m_sidebarWidth;

        // Write session file
        std::string sessionPath = ".rawrxd/session.json";
        CreateDirectoryA(".rawrxd", nullptr);
        std::ofstream sessionFile(sessionPath);
        if (sessionFile) {
            sessionFile << session.dump(4);
            LOG_INFO("Session state saved to " + sessionPath);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to save session state: " + std::string(e.what()));
    }
}

// ============================================================================
// onCommand - WM_COMMAND dispatcher
// ============================================================================
void Win32IDE::onCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
    // Handle editor notifications (EN_CHANGE, EN_VSCROLL, EN_SELCHANGE come via WM_COMMAND)
    if (hwndCtl == m_hwndEditor) {
        if (codeNotify == EN_CHANGE || codeNotify == EN_VSCROLL || codeNotify == EN_SELCHANGE) {
            updateLineNumbers();
            // Debounce syntax coloring on content/scroll change
            if (m_syntaxColoringEnabled) {
                onEditorContentChanged();
            }
            // Update cursor position in status bar
            CHARRANGE sel;
            SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
            int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
            int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
            int col = sel.cpMin - lineStart;
            char posBuf[64];
            snprintf(posBuf, sizeof(posBuf), "Ln %d, Col %d", line + 1, col + 1);
            if (m_hwndStatusBar) {
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)posBuf);
            }
        }
        return; // Don't route editor notifications through the command system
    }

    // First try the unified command router
    if (id == 9903) { // Model progress cancel button
        cancelModelOperation();
        return;
    }
    if (routeCommand(id)) {
        return; // handled
    }

    // Fall through to default handling
    DefWindowProcA(hwnd, WM_COMMAND, MAKEWPARAM(id, codeNotify), (LPARAM)hwndCtl);
}
