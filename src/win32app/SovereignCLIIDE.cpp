// ============================================================================
// SovereignCLIIDE.cpp — CLI IDE implementation
// ============================================================================

#include "SovereignCLIIDE.h"
#include "Win32TerminalManager.h"
#include "IDELogger.h"
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sstream>
#include <iomanip>
#include <chrono>

// Global instance for standalone mode
SovereignCLIIDE* g_sovereignCLIIDE = nullptr;

// ============================================================================
// Construction/Destruction
// ============================================================================

SovereignCLIIDE::SovereignCLIIDE(RunMode mode)
    : m_mode(mode)
    , m_running(false)
    , m_hwndOutput(nullptr)
    , m_hwndInput(nullptr)
    , m_hwndTabBar(nullptr)
    , m_hwndParent(nullptr)
{
    if (m_mode == RunMode::Standalone) {
        g_sovereignCLIIDE = this;
    }
}

SovereignCLIIDE::~SovereignCLIIDE()
{
    shutdown();
    if (m_mode == RunMode::Standalone) {
        g_sovereignCLIIDE = nullptr;
    }
}

// ============================================================================
// Initialization/Shutdown
// ============================================================================

bool SovereignCLIIDE::initialize()
{
    if (m_running) {
        return true;
    }

    if (!initializeTerminal()) {
        LOG_ERROR("[SovereignCLIIDE] Failed to initialize terminal");
        return false;
    }

    m_running = true;
    
    // Start output reading threads
    m_outputThread = std::thread(&SovereignCLIIDE::readOutputThread, this);
    m_errorThread = std::thread(&SovereignCLIIDE::readErrorThread, this);
    
    LOG_INFO("[SovereignCLIIDE] Initialized successfully");
    return true;
}

void SovereignCLIIDE::shutdown()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    
    // Stop threads
    if (m_outputThread.joinable()) {
        m_outputThread.join();
    }
    if (m_errorThread.joinable()) {
        m_errorThread.join();
    }
    
    // Cleanup terminal
    cleanupTerminal();
    
    // Cleanup UI (if in integrated mode)
    destroyTabContent();
    
    LOG_INFO("[SovereignCLIIDE] Shutdown complete");
}

bool SovereignCLIIDE::initializeTerminal()
{
    try {
        m_terminal = std::make_unique<Win32TerminalManager>();
        
        // Set up callbacks
        m_terminal->onOutput = [this](const std::string& output) {
            processOutput(output);
        };
        
        m_terminal->onError = [this](const std::string& error) {
            processError(error);
        };
        
        m_terminal->onStarted = []() {
            LOG_INFO("[SovereignCLIIDE] Terminal started");
        };
        
        m_terminal->onFinished = [](int exitCode) {
            LOG_INFO("[SovereignCLIIDE] Terminal finished with exit code: %d", exitCode);
        };
        
        // Start PowerShell by default
        if (!m_terminal->start(Win32TerminalManager::PowerShell, 
                               m_workingDirectory.empty() ? nullptr : m_workingDirectory.c_str())) {
            LOG_ERROR("[SovereignCLIIDE] Failed to start terminal");
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("[SovereignCLIIDE] Terminal initialization failed: %s", e.what());
        return false;
    }
}

void SovereignCLIIDE::cleanupTerminal()
{
    if (m_terminal) {
        m_terminal->stop();
        m_terminal.reset();
    }
}

// ============================================================================
// Command Execution
// ============================================================================

SovereignCLIIDE::CommandResult SovereignCLIIDE::executeCommand(const std::string& command)
{
    CommandResult result;
    
    if (!m_running || !m_terminal) {
        result.success = false;
        result.error = "CLI IDE not initialized";
        return result;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // Add command to history
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_commandHistory.push_back(command);
            if (m_commandHistory.size() > 1000) {
                m_commandHistory.pop_front();
            }
        }
        
        // Execute command
        m_terminal->writeInput(command + "\r\n");
        
        // For simplicity, we'll just return immediate result
        // In real implementation, we'd capture output and wait for completion
        result.success = true;
        result.exitCode = 0;
        result.output = "Command executed: " + command;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.executionTime = std::chrono::duration<double>(endTime - startTime).count();
        
    }
    catch (const std::exception& e) {
        result.success = false;
        result.error = std::string("Execution failed: ") + e.what();
        result.exitCode = -1;
    }
    
    return result;
}

void SovereignCLIIDE::executeCommandAsync(const std::string& command, CompletionCallback callback)
{
    if (!m_running || !m_terminal) {
        if (callback) {
            CommandResult result;
            result.success = false;
            result.error = "CLI IDE not initialized";
            callback(result);
        }
        return;
    }
    
    // For async execution, we'd need to implement proper output capture
    // and completion detection. This is a simplified version.
    std::thread([this, command, callback]() {
        CommandResult result = executeCommand(command);
        if (callback) {
            callback(result);
        }
    }).detach();
}

// ============================================================================
// Output Processing
// ============================================================================

void SovereignCLIIDE::readOutputThread()
{
    while (m_running) {
        // Thread would typically read from terminal output
        // and process it. Simplified for this implementation.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SovereignCLIIDE::readErrorThread()
{
    while (m_running) {
        // Thread would typically read from terminal error output
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SovereignCLIIDE::processOutput(const std::string& data)
{
    // Update UI if in integrated mode
    if (m_hwndOutput) {
        // Append to output window
        // Implementation would use EM_SETSEL + EM_REPLACESEL
    }
    
    // Call user callback
    if (m_outputCallback) {
        m_outputCallback(data);
    }
}

void SovereignCLIIDE::processError(const std::string& data)
{
    // Update UI if in integrated mode
    if (m_hwndOutput) {
        // Append error to output window (with different color)
    }
    
    // Call user callback
    if (m_errorCallback) {
        m_errorCallback(data);
    }
}

// ============================================================================
// Integrated Tab Interface
// ============================================================================

HWND SovereignCLIIDE::createTabContent(HWND parent)
{
    if (m_mode != RunMode::IntegratedTab) {
        return nullptr;
    }
    
    m_hwndParent = parent;
    
    // Create output window
    m_hwndOutput = createOutputWindow(parent);
    if (!m_hwndOutput) {
        return nullptr;
    }
    
    // Create input window
    m_hwndInput = createInputWindow(parent);
    if (!m_hwndInput) {
        DestroyWindow(m_hwndOutput);
        m_hwndOutput = nullptr;
        return nullptr;
    }
    
    // Create tab bar
    m_hwndTabBar = createTabBar(parent);
    
    return m_hwndOutput;
}

void SovereignCLIIDE::destroyTabContent()
{
    if (m_hwndOutput) {
        DestroyWindow(m_hwndOutput);
        m_hwndOutput = nullptr;
    }
    
    if (m_hwndInput) {
        DestroyWindow(m_hwndInput);
        m_hwndInput = nullptr;
    }
    
    if (m_hwndTabBar) {
        DestroyWindow(m_hwndTabBar);
        m_hwndTabBar = nullptr;
    }
    
    m_hwndParent = nullptr;
}

void SovereignCLIIDE::focusTab()
{
    if (m_hwndInput) {
        SetFocus(m_hwndInput);
    }
}

HWND SovereignCLIIDE::createOutputWindow(HWND parent)
{
    RECT rc;
    GetClientRect(parent, &rc);
    
    HWND hwnd = CreateWindowExA(0, "RICHEDIT50W", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        0, 0, rc.right, rc.bottom - 30, parent,
        (HMENU)(UINT_PTR)1001, GetModuleHandle(NULL), NULL);
    
    if (hwnd) {
        // Set font and colors
        SendMessage(hwnd, EM_SETBKGNDCOLOR, 0, RGB(12, 12, 12));
        { CHARFORMAT2W cf{}; cf.cbSize = sizeof(cf); cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE; cf.crTextColor = RGB(204, 204, 204); cf.yHeight = 200; lstrcpyW(cf.szFaceName, L"Consolas"); SendMessage(hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf); }
        SetPropA(hwnd, "SOVEREIGN_CLI_IDE", (HANDLE)this);
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)OutputWndProc);
    }
    
    return hwnd;
}

HWND SovereignCLIIDE::createInputWindow(HWND parent)
{
    RECT rc;
    GetClientRect(parent, &rc);
    
    HWND hwnd = CreateWindowExA(0, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, rc.bottom - 25, rc.right, 25, parent,
        (HMENU)(UINT_PTR)1002, GetModuleHandle(NULL), NULL);
    
    if (hwnd) {
        SetPropA(hwnd, "SOVEREIGN_CLI_IDE", (HANDLE)this);
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)InputWndProc);
    }
    
    return hwnd;
}

HWND SovereignCLIIDE::createTabBar(HWND parent)
{
    // Similar to terminal tab bar implementation
    return nullptr;
}

// ============================================================================
// Message Handlers
// ============================================================================

LRESULT CALLBACK SovereignCLIIDE::OutputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SovereignCLIIDE* pThis = (SovereignCLIIDE*)GetPropA(hwnd, "SOVEREIGN_CLI_IDE");
    
    switch (msg) {
        case WM_KEYDOWN:
            if (pThis && wParam == VK_RETURN) {
                // Handle enter key in output window
                return 0;
            }
            break;
        
        case WM_CHAR:
            // Handle character input
            break;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK SovereignCLIIDE::InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SovereignCLIIDE* pThis = (SovereignCLIIDE*)GetPropA(hwnd, "SOVEREIGN_CLI_IDE");
    
    switch (msg) {
        case WM_KEYDOWN:
            if (pThis && wParam == VK_RETURN) {
                // Get text from input window
                char buffer[1024];
                GetWindowTextA(hwnd, buffer, sizeof(buffer));
                
                // Execute command
                pThis->executeCommand(buffer);
                
                // Clear input
                SetWindowTextA(hwnd, "");
                return 0;
            }
            break;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Utility Methods
// ============================================================================

void SovereignCLIIDE::startSession()
{
    initialize();
}

void SovereignCLIIDE::endSession()
{
    shutdown();
}

void SovereignCLIIDE::clearHistory()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commandHistory.clear();
}

std::vector<std::string> SovereignCLIIDE::getCommandHistory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<std::string>(m_commandHistory.begin(), m_commandHistory.end());
}

std::vector<std::string> SovereignCLIIDE::getCommandSuggestions(const std::string& prefix) const
{
    std::vector<std::string> suggestions;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& cmd : m_commandHistory) {
        if (cmd.find(prefix) == 0) {
            suggestions.push_back(cmd);
        }
    }
    
    return suggestions;
}

void SovereignCLIIDE::setWorkingDirectory(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_workingDirectory = path;
}

void SovereignCLIIDE::setEnvironmentVariables(
    const std::vector<std::pair<std::string, std::string>>& envVars)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_environmentVars = envVars;
}

void SovereignCLIIDE::setOutputCallback(OutputCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_outputCallback = callback;
}

void SovereignCLIIDE::setErrorCallback(ErrorCallback callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_errorCallback = callback;
}
