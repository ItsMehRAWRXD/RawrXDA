// ============================================================================
// Win32IDE_OllamaPane.cpp - Embedded Ollama Service Terminal Pane
// ============================================================================
// Provides a dedicated terminal pane for managing the embedded Ollama service
// with real-time logging, service controls, and health monitoring.

#include "Win32IDE.h"
#include "OllamaServiceManager.h"
#include "IDELogger.h"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
#include <iomanip>

// Control IDs
#define IDC_OLLAMA_PANE_CONTAINER    3200
#define IDC_OLLAMA_TOOLBAR          3201
#define IDC_OLLAMA_LOG_OUTPUT       3202
#define IDC_OLLAMA_STATUS_BAR       3203
#define IDC_OLLAMA_BTN_START        3204
#define IDC_OLLAMA_BTN_STOP         3205
#define IDC_OLLAMA_BTN_RESTART      3206
#define IDC_OLLAMA_BTN_CLEAR_LOG    3207
#define IDC_OLLAMA_BTN_HEALTH       3208
#define IDC_OLLAMA_BTN_MODELS       3209
#define IDC_OLLAMA_STATUS_TEXT      3210
#define IDC_OLLAMA_HEALTH_LED       3211

// Colors for different log levels and states
#define OLLAMA_COLOR_INFO     RGB(200, 200, 200)
#define OLLAMA_COLOR_WARNING  RGB(255, 200, 100)
#define OLLAMA_COLOR_ERROR    RGB(255, 100, 100)
#define OLLAMA_COLOR_DEBUG    RGB(150, 150, 255)
#define OLLAMA_COLOR_SUCCESS  RGB(100, 255, 100)
#define OLLAMA_COLOR_BACKGROUND RGB(20, 20, 30)

// ============================================================================
// OLLAMA PANE CREATION AND LAYOUT
// ============================================================================

void Win32IDE::createOllamaServicePane() {
    if (!m_hwndMain || m_hwndOllamaPane) return;

    // Get the panel area for placing the Ollama pane
    RECT panelRect;
    if (m_hwndPanelContainer) {
        GetClientRect(m_hwndPanelContainer, &panelRect);
    } else {
        GetClientRect(m_hwndMain, &panelRect);
        panelRect.top = panelRect.bottom - 300; // Bottom 300 pixels
    }

    // Create main container
    m_hwndOllamaPane = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"STATIC",
        L"Ollama Service",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        panelRect.left, panelRect.top,
        panelRect.right - panelRect.left, panelRect.bottom - panelRect.top,
        m_hwndPanelContainer ? m_hwndPanelContainer : m_hwndMain,
        (HMENU)IDC_OLLAMA_PANE_CONTAINER,
        GetModuleHandle(nullptr),
        nullptr);

    if (!m_hwndOllamaPane) {
        LOG_ERROR("Failed to create Ollama service pane");
        return;
    }

    createOllamaToolbar();
    createOllamaLogOutput();
    createOllamaStatusBar();
    layoutOllamaPane();

    // Initialize Ollama service manager if not already done
    if (!m_ollamaServiceManager) {
        m_ollamaServiceManager = std::make_unique<OllamaServiceManager>(this);
        m_ollamaServiceManager->initialize();
        
        // Set up callbacks for UI updates
        m_ollamaServiceManager->setLogCallback([this](const OllamaServiceManager::LogEntry& entry) {
            // Post message to UI thread for safe updates
            PostMessage(m_hwndOllamaPane, WM_USER + 100, 0, reinterpret_cast<LPARAM>(&entry));
        });

        m_ollamaServiceManager->setStateCallback([this](OllamaServiceManager::ServiceState state, const std::string& message) {
            // Update status bar and toolbar buttons
            updateOllamaStatusDisplay(state, message);
        });

        // Attach the log output window to the service manager
        m_ollamaServiceManager->attachTerminalPane(m_hwndOllamaLogOutput);
    }

    // Update initial state
    updateOllamaToolbarState();
    addOllamaLogEntry("Ollama service pane initialized", OllamaServiceManager::LogLevel::Info);
}

void Win32IDE::createOllamaToolbar() {
    if (!m_hwndOllamaPane) return;

    RECT containerRect;
    GetClientRect(m_hwndOllamaPane, &containerRect);

    // Create toolbar
    m_hwndOllamaToolbar = CreateWindowExW(
        0,
        TOOLBARCLASSNAME,
        nullptr,
        WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS,
        0, 0,
        containerRect.right, 30,
        m_hwndOllamaPane,
        (HMENU)IDC_OLLAMA_TOOLBAR,
        GetModuleHandle(nullptr),
        nullptr);

    if (!m_hwndOllamaToolbar) return;

    // Initialize toolbar
    SendMessage(m_hwndOllamaToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

    // Create buttons
    TBBUTTON tbButtons[] = {
        {0, IDC_OLLAMA_BTN_START,    TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Start"},
        {1, IDC_OLLAMA_BTN_STOP,     TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Stop"},
        {2, IDC_OLLAMA_BTN_RESTART,  TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Restart"},
        {0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0},
        {3, IDC_OLLAMA_BTN_HEALTH,   TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Health Check"},
        {4, IDC_OLLAMA_BTN_MODELS,   TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Models"},
        {0, 0, TBSTATE_ENABLED, BTNS_SEP, {0}, 0, 0},
        {5, IDC_OLLAMA_BTN_CLEAR_LOG, TBSTATE_ENABLED, BTNS_BUTTON | BTNS_AUTOSIZE, {0}, 0, (INT_PTR)L"Clear Log"},
    };

    SendMessage(m_hwndOllamaToolbar, TB_ADDBUTTONS, ARRAYSIZE(tbButtons), (LPARAM)tbButtons);
    SendMessage(m_hwndOllamaToolbar, TB_AUTOSIZE, 0, 0);
}

void Win32IDE::createOllamaLogOutput() {
    if (!m_hwndOllamaPane) return;

    RECT containerRect;
    GetClientRect(m_hwndOllamaPane, &containerRect);

    // Create rich edit control for log output
    m_hwndOllamaLogOutput = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        RICHEDIT_CLASS,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        5, 35,  // Leave space for toolbar
        containerRect.right - 10, containerRect.bottom - 70, // Leave space for status bar
        m_hwndOllamaPane,
        (HMENU)IDC_OLLAMA_LOG_OUTPUT,
        GetModuleHandle(nullptr),
        nullptr);

    if (m_hwndOllamaLogOutput) {
        // Set dark theme colors
        SendMessage(m_hwndOllamaLogOutput, EM_SETBKGNDCOLOR, 0, OLLAMA_COLOR_BACKGROUND);
        
        // Set font to Consolas
        HFONT hFont = CreateFontW(
            -12,                        // Height
            0,                          // Width
            0,                          // Escapement
            0,                          // Orientation
            FW_NORMAL,                  // Weight
            FALSE,                      // Italic
            FALSE,                      // Underline
            FALSE,                      // StrikeOut
            DEFAULT_CHARSET,            // CharSet
            OUT_DEFAULT_PRECIS,         // OutPrecision
            CLIP_DEFAULT_PRECIS,        // ClipPrecision
            CLEARTYPE_QUALITY,          // Quality
            FF_DONTCARE,               // PitchAndFamily
            L"Consolas"                 // Face
        );
        
        if (hFont) {
            SendMessage(m_hwndOllamaLogOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
        }

        // Enable Unicode
        SendMessage(m_hwndOllamaLogOutput, EM_SETTEXTMODE, TM_PLAINTEXT | TM_MULTILEVELUNDO, 0);
    }
}

void Win32IDE::createOllamaStatusBar() {
    if (!m_hwndOllamaPane) return;

    RECT containerRect;
    GetClientRect(m_hwndOllamaPane, &containerRect);

    // Create status bar container
    HWND hwndStatusContainer = CreateWindowExW(
        0,
        L"STATIC",
        nullptr,
        WS_CHILD | WS_VISIBLE,
        0, containerRect.bottom - 25,
        containerRect.right, 25,
        m_hwndOllamaPane,
        (HMENU)IDC_OLLAMA_STATUS_BAR,
        GetModuleHandle(nullptr),
        nullptr);

    if (!hwndStatusContainer) return;

    // Status text
    m_hwndOllamaStatusText = CreateWindowExW(
        0,
        L"STATIC",
        L"Ollama Service: Stopped",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        30, 5,
        containerRect.right - 200, 15,
        hwndStatusContainer,
        (HMENU)IDC_OLLAMA_STATUS_TEXT,
        GetModuleHandle(nullptr),
        nullptr);

    // Health indicator LED
    m_hwndOllamaHealthLED = CreateWindowExW(
        0,
        L"STATIC",
        L"●",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        5, 5,
        20, 15,
        hwndStatusContainer,
        (HMENU)IDC_OLLAMA_HEALTH_LED,
        GetModuleHandle(nullptr),
        nullptr);

    // Set initial LED color to red (stopped)
    updateOllamaHealthLED(false);
}

void Win32IDE::layoutOllamaPane() {
    if (!m_hwndOllamaPane) return;

    RECT containerRect;
    GetClientRect(m_hwndOllamaPane, &containerRect);

    const int toolbarHeight = 30;
    const int statusHeight = 25;
    const int margin = 5;

    // Layout toolbar
    if (m_hwndOllamaToolbar) {
        SetWindowPos(m_hwndOllamaToolbar, nullptr,
                     0, 0,
                     containerRect.right, toolbarHeight,
                     SWP_NOZORDER);
    }

    // Layout log output
    if (m_hwndOllamaLogOutput) {
        SetWindowPos(m_hwndOllamaLogOutput, nullptr,
                     margin, toolbarHeight + margin,
                     containerRect.right - (2 * margin), 
                     containerRect.bottom - toolbarHeight - statusHeight - (2 * margin),
                     SWP_NOZORDER);
    }

    // Layout status bar
    HWND hwndStatusContainer = GetDlgItem(m_hwndOllamaPane, IDC_OLLAMA_STATUS_BAR);
    if (hwndStatusContainer) {
        SetWindowPos(hwndStatusContainer, nullptr,
                     0, containerRect.bottom - statusHeight,
                     containerRect.right, statusHeight,
                     SWP_NOZORDER);
    }
}

// ============================================================================
// OLLAMA PANE EVENT HANDLING
// ============================================================================

LRESULT Win32IDE::handleOllamaPaneCommand(WPARAM wParam, LPARAM lParam) {
    int commandId = LOWORD(wParam);
    
    if (!m_ollamaServiceManager) {
        addOllamaLogEntry("Ollama service manager not available", OllamaServiceManager::LogLevel::Error);
        return 0;
    }

    switch (commandId) {
        case IDC_OLLAMA_BTN_START:
            handleOllamaStartService();
            break;
            
        case IDC_OLLAMA_BTN_STOP:
            handleOllamaStopService();
            break;
            
        case IDC_OLLAMA_BTN_RESTART:
            handleOllamaRestartService();
            break;
            
        case IDC_OLLAMA_BTN_HEALTH:
            handleOllamaHealthCheck();
            break;
            
        case IDC_OLLAMA_BTN_MODELS:
            handleOllamaShowModels();
            break;
            
        case IDC_OLLAMA_BTN_CLEAR_LOG:
            handleOllamaClearLog();
            break;
            
        default:
            return DefWindowProcW(m_hwndOllamaPane, WM_COMMAND, wParam, lParam);
    }
    
    return 0;
}

void Win32IDE::handleOllamaStartService() {
    addOllamaLogEntry("Starting Ollama service...", OllamaServiceManager::LogLevel::Info);
    
    // Start service in background thread to avoid blocking UI
    std::thread([this]() {
        bool success = m_ollamaServiceManager->startService();
        
        // Post result back to UI thread
        PostMessage(m_hwndOllamaPane, WM_USER + 101, success ? 1 : 0, 0);
    }).detach();
    
    updateOllamaToolbarState();
}

void Win32IDE::handleOllamaStopService() {
    addOllamaLogEntry("Stopping Ollama service...", OllamaServiceManager::LogLevel::Info);
    
    std::thread([this]() {
        bool success = m_ollamaServiceManager->stopService();
        PostMessage(m_hwndOllamaPane, WM_USER + 102, success ? 1 : 0, 0);
    }).detach();
    
    updateOllamaToolbarState();
}

void Win32IDE::handleOllamaRestartService() {
    addOllamaLogEntry("Restarting Ollama service...", OllamaServiceManager::LogLevel::Info);
    
    std::thread([this]() {
        bool success = m_ollamaServiceManager->restartService();
        PostMessage(m_hwndOllamaPane, WM_USER + 103, success ? 1 : 0, 0);
    }).detach();
    
    updateOllamaToolbarState();
}

void Win32IDE::handleOllamaHealthCheck() {
    addOllamaLogEntry("Performing health check...", OllamaServiceManager::LogLevel::Info);
    
    std::thread([this]() {
        bool healthy = m_ollamaServiceManager->performHealthCheck();
        std::string status = m_ollamaServiceManager->getHealthStatus();
        
        // Post results to UI thread
        std::string* statusPtr = new std::string(status);
        PostMessage(m_hwndOllamaPane, WM_USER + 104, healthy ? 1 : 0, (LPARAM)statusPtr);
    }).detach();
}

void Win32IDE::handleOllamaShowModels() {
    addOllamaLogEntry("Retrieving loaded models...", OllamaServiceManager::LogLevel::Info);
    
    std::thread([this]() {
        auto models = m_ollamaServiceManager->getLoadedModels();
        
        std::ostringstream oss;
        oss << "Loaded models (" << models.size() << "):";
        if (models.empty()) {
            oss << " None";
        } else {
            for (const auto& model : models) {
                oss << "\n  - " << model;
            }
        }
        
        std::string* resultPtr = new std::string(oss.str());
        PostMessage(m_hwndOllamaPane, WM_USER + 105, 0, (LPARAM)resultPtr);
    }).detach();
}

void Win32IDE::handleOllamaClearLog() {
    if (m_hwndOllamaLogOutput) {
        SetWindowTextW(m_hwndOllamaLogOutput, L"");
        if (m_ollamaServiceManager) {
            m_ollamaServiceManager->clearLogs();
        }
        addOllamaLogEntry("Log cleared", OllamaServiceManager::LogLevel::Info);
    }
}

// ============================================================================
// OLLAMA PANE UI UPDATES
// ============================================================================

void Win32IDE::updateOllamaToolbarState() {
    if (!m_hwndOllamaToolbar || !m_ollamaServiceManager) return;

    auto state = m_ollamaServiceManager->getServiceState();
    bool isRunning = (state == OllamaServiceManager::ServiceState::Running);
    bool isTransitioning = (state == OllamaServiceManager::ServiceState::Starting || 
                           state == OllamaServiceManager::ServiceState::Stopping);

    // Update button states
    SendMessage(m_hwndOllamaToolbar, TB_ENABLEBUTTON, IDC_OLLAMA_BTN_START, 
                MAKELONG(!isRunning && !isTransitioning, 0));
    SendMessage(m_hwndOllamaToolbar, TB_ENABLEBUTTON, IDC_OLLAMA_BTN_STOP, 
                MAKELONG(isRunning && !isTransitioning, 0));
    SendMessage(m_hwndOllamaToolbar, TB_ENABLEBUTTON, IDC_OLLAMA_BTN_RESTART, 
                MAKELONG(isRunning && !isTransitioning, 0));
    SendMessage(m_hwndOllamaToolbar, TB_ENABLEBUTTON, IDC_OLLAMA_BTN_HEALTH, 
                MAKELONG(isRunning, 0));
    SendMessage(m_hwndOllamaToolbar, TB_ENABLEBUTTON, IDC_OLLAMA_BTN_MODELS, 
                MAKELONG(isRunning, 0));
}

void Win32IDE::updateOllamaStatusDisplay(OllamaServiceManager::ServiceState state, const std::string& message) {
    if (!m_hwndOllamaStatusText) return;

    std::wstring statusText = L"Ollama Service: ";
    switch (state) {
        case OllamaServiceManager::ServiceState::Stopped:
            statusText += L"Stopped";
            break;
        case OllamaServiceManager::ServiceState::Starting:
            statusText += L"Starting...";
            break;
        case OllamaServiceManager::ServiceState::Running:
            statusText += L"Running";
            if (m_ollamaServiceManager) {
                std::string endpoint = m_ollamaServiceManager->getServiceEndpoint();
                statusText += L" (" + std::wstring(endpoint.begin(), endpoint.end()) + L")";
            }
            break;
        case OllamaServiceManager::ServiceState::Stopping:
            statusText += L"Stopping...";
            break;
        case OllamaServiceManager::ServiceState::Error:
            statusText += L"Error";
            break;
        case OllamaServiceManager::ServiceState::Downloading:
            statusText += L"Downloading...";
            break;
        case OllamaServiceManager::ServiceState::Installing:
            statusText += L"Installing...";
            break;
    }

    if (!message.empty()) {
        statusText += L" - " + std::wstring(message.begin(), message.end());
    }

    SetWindowTextW(m_hwndOllamaStatusText, statusText.c_str());
    
    // Update health LED
    bool isHealthy = (state == OllamaServiceManager::ServiceState::Running && 
                     m_ollamaServiceManager && m_ollamaServiceManager->isServiceHealthy());
    updateOllamaHealthLED(isHealthy);
    
    // Update toolbar
    updateOllamaToolbarState();
}

void Win32IDE::updateOllamaHealthLED(bool isHealthy) {
    if (!m_hwndOllamaHealthLED) return;

    // Set LED color based on health status
    HBRUSH hBrush;
    if (isHealthy) {
        hBrush = CreateSolidBrush(RGB(0, 255, 0));  // Green
    } else {
        hBrush = CreateSolidBrush(RGB(255, 0, 0));  // Red
    }

    HDC hdc = GetDC(m_hwndOllamaHealthLED);
    if (hdc) {
        RECT rect;
        GetClientRect(m_hwndOllamaHealthLED, &rect);
        FillRect(hdc, &rect, hBrush);
        ReleaseDC(m_hwndOllamaHealthLED, hdc);
    }
    DeleteObject(hBrush);
}

void Win32IDE::addOllamaLogEntry(const std::string& message, OllamaServiceManager::LogLevel level) {
    if (!m_hwndOllamaLogOutput) return;

    // Get timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
    
    // Add level prefix
    switch (level) {
        case OllamaServiceManager::LogLevel::Error:   oss << "[ERROR] "; break;
        case OllamaServiceManager::LogLevel::Warning: oss << "[WARN]  "; break;
        case OllamaServiceManager::LogLevel::Info:    oss << "[INFO]  "; break;
        case OllamaServiceManager::LogLevel::Debug:   oss << "[DEBUG] "; break;
    }
    
    oss << message << "\r\n";
    std::string logEntry = oss.str();

    // Append to log output with color formatting
    CHARFORMAT2W cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
    cf.dwEffects = 0;
    cf.yHeight = 180; // 9pt
    wcscpy_s(cf.szFaceName, L"Consolas");
    
    // Set color based on log level
    switch (level) {
        case OllamaServiceManager::LogLevel::Error:
            cf.crTextColor = OLLAMA_COLOR_ERROR;
            break;
        case OllamaServiceManager::LogLevel::Warning:
            cf.crTextColor = OLLAMA_COLOR_WARNING;
            break;
        case OllamaServiceManager::LogLevel::Info:
            cf.crTextColor = OLLAMA_COLOR_INFO;
            break;
        case OllamaServiceManager::LogLevel::Debug:
            cf.crTextColor = OLLAMA_COLOR_DEBUG;
            break;
    }

    // Move to end of text
    SendMessage(m_hwndOllamaLogOutput, EM_SETSEL, -1, -1);
    
    // Set character format
    SendMessage(m_hwndOllamaLogOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // Add the text
    std::wstring wLogEntry(logEntry.begin(), logEntry.end());
    SendMessage(m_hwndOllamaLogOutput, EM_REPLACESEL, FALSE, (LPARAM)wLogEntry.c_str());
    
    // Scroll to end
    SendMessage(m_hwndOllamaLogOutput, WM_VSCROLL, SB_BOTTOM, 0);
    
    // Limit log size (keep last 1000 lines)
    int lineCount = (int)SendMessage(m_hwndOllamaLogOutput, EM_GETLINECOUNT, 0, 0);
    if (lineCount > 1000) {
        int excessLines = lineCount - 800; // Remove oldest 200 lines
        int charIndex = (int)SendMessage(m_hwndOllamaLogOutput, EM_LINEINDEX, excessLines, 0);
        SendMessage(m_hwndOllamaLogOutput, EM_SETSEL, 0, charIndex);
        SendMessage(m_hwndOllamaLogOutput, EM_REPLACESEL, FALSE, (LPARAM)L"");
    }
}

// ============================================================================
// OLLAMA PANE MESSAGE HANDLING
// ============================================================================

LRESULT Win32IDE::handleOllamaPaneMessages(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            layoutOllamaPane();
            return 0;
            
        case WM_COMMAND:
            return handleOllamaPaneCommand(wParam, lParam);
            
        // Custom messages from background threads
        case WM_USER + 100: // Log entry callback
            {
                const auto* entry = reinterpret_cast<const OllamaServiceManager::LogEntry*>(lParam);
                if (entry) {
                    addOllamaLogEntry(entry->message, entry->level);
                }
            }
            return 0;
            
        case WM_USER + 101: // Start service result
            addOllamaLogEntry(wParam ? "Service started successfully" : "Failed to start service", 
                            wParam ? OllamaServiceManager::LogLevel::Info : OllamaServiceManager::LogLevel::Error);
            updateOllamaToolbarState();
            return 0;
            
        case WM_USER + 102: // Stop service result
            addOllamaLogEntry(wParam ? "Service stopped successfully" : "Failed to stop service", 
                            wParam ? OllamaServiceManager::LogLevel::Info : OllamaServiceManager::LogLevel::Error);
            updateOllamaToolbarState();
            return 0;
            
        case WM_USER + 103: // Restart service result
            addOllamaLogEntry(wParam ? "Service restarted successfully" : "Failed to restart service", 
                            wParam ? OllamaServiceManager::LogLevel::Info : OllamaServiceManager::LogLevel::Error);
            updateOllamaToolbarState();
            return 0;
            
        case WM_USER + 104: // Health check result
            {
                std::string* statusPtr = reinterpret_cast<std::string*>(lParam);
                if (statusPtr) {
                    addOllamaLogEntry("Health check: " + *statusPtr, 
                                    wParam ? OllamaServiceManager::LogLevel::Info : OllamaServiceManager::LogLevel::Warning);
                    delete statusPtr;
                }
                updateOllamaHealthLED(wParam != 0);
            }
            return 0;
            
        case WM_USER + 105: // Models list result
            {
                std::string* resultPtr = reinterpret_cast<std::string*>(lParam);
                if (resultPtr) {
                    addOllamaLogEntry(*resultPtr, OllamaServiceManager::LogLevel::Info);
                    delete resultPtr;
                }
            }
            return 0;
            
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}