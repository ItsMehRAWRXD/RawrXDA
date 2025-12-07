// Dedicated PowerShell Panel Implementation
// Always-available PowerShell console for Win32IDE

#include "Win32IDE.h"
#include <sstream>
#include <algorithm>
#include <richedit.h>

// PowerShell Panel Control IDs
#define IDC_PS_PANEL_CONTAINER 5000
#define IDC_PS_OUTPUT 5001
#define IDC_PS_INPUT 5002
#define IDC_PS_TOOLBAR 5003
#define IDC_PS_STATUSBAR 5004
#define IDC_PS_BTN_EXECUTE 5010
#define IDC_PS_BTN_CLEAR 5011
#define IDC_PS_BTN_STOP 5012
#define IDC_PS_BTN_HISTORY 5013
#define IDC_PS_BTN_RESTART 5014
#define IDC_PS_BTN_LOAD_RAWRXD 5015
#define IDC_PS_BTN_TOGGLE 5016

// ============================================================================
// POWERSHELL PANEL CREATION
// ============================================================================

void Win32IDE::createPowerShellPanel() {
    if (m_hwndPowerShellPanel) {
        return; // Already created
    }
    
    // Create main PowerShell panel container
    m_hwndPowerShellPanel = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"STATIC", L"PowerShell Console",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, 800, m_powerShellPanelHeight,
        m_hwndMain,
        (HMENU)IDC_PS_PANEL_CONTAINER,
        m_hInstance,
        NULL
    );
    
    if (!m_hwndPowerShellPanel) {
        return;
    }
    
    // Store IDE pointer for callbacks
    SetProp(m_hwndPowerShellPanel, L"IDE_PTR", this);
    
    // Create toolbar
    createPowerShellToolbar();
    
    // Create output area (rich edit for colored text)
    LoadLibrary(L"Riched20.dll");
    
    m_hwndPowerShellOutput = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"RichEdit20A", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        5, 35, 790, m_powerShellPanelHeight - 95,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_OUTPUT,
        m_hInstance,
        NULL
    );
    
    // Set output font
    HFONT hFont = CreateFont(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas"
    );
    SendMessage(m_hwndPowerShellOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Set background color
    SendMessage(m_hwndPowerShellOutput, EM_SETBKGNDCOLOR, 0, RGB(1, 36, 86)); // PowerShell blue
    
    // Create input area
    m_hwndPowerShellInput = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        5, m_powerShellPanelHeight - 55, 690, 25,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_INPUT,
        m_hInstance,
        NULL
    );
    
    SendMessage(m_hwndPowerShellInput, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Subclass input for custom handling (Enter key, history navigation)
    SetProp(m_hwndPowerShellInput, L"IDE_PTR", this);
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(m_hwndPowerShellInput, GWLP_WNDPROC, (LONG_PTR)PowerShellInputProc);
    SetProp(m_hwndPowerShellInput, L"OLDPROC", (HANDLE)oldProc);
    
    // Create Execute button
    m_hwndPSBtnExecute = CreateWindowEx(
        0, L"BUTTON", L"Execute",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        700, m_powerShellPanelHeight - 55, 90, 25,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_BTN_EXECUTE,
        m_hInstance,
        NULL
    );
    SendMessage(m_hwndPSBtnExecute, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // Create status bar
    m_hwndPowerShellStatusBar = CreateWindowEx(
        0, L"STATIC", L"PowerShell Status: Ready",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, m_powerShellPanelHeight - 25, 790, 20,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_STATUSBAR,
        m_hInstance,
        NULL
    );
    
    HFONT hSmallFont = CreateFont(
        12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    SendMessage(m_hwndPowerShellStatusBar, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
    
    // Initialize PowerShell session
    initializePowerShellPanel();
    
    // Show welcome message
    appendPowerShellOutput("═══════════════════════════════════════════════════════════════\n", RGB(0, 255, 255));
    appendPowerShellOutput("  RawrXD Integrated PowerShell Console\n", RGB(255, 255, 0));
    appendPowerShellOutput("═══════════════════════════════════════════════════════════════\n", RGB(0, 255, 255));
    appendPowerShellOutput("\n", RGB(200, 200, 200));
    
    std::string version = getPowerShellVersion();
    appendPowerShellOutput("PowerShell Version: " + version + "\n", RGB(0, 255, 0));
    appendPowerShellOutput("Edition: " + getPowerShellEdition() + "\n", RGB(0, 255, 0));
    appendPowerShellOutput("\nType commands below or click 'Load RawrXD' to access RawrXD.ps1 functions\n", RGB(200, 200, 200));
    appendPowerShellOutput("\nCommands:\n", RGB(255, 255, 0));
    appendPowerShellOutput("  - Enter: Execute command\n", RGB(150, 150, 150));
    appendPowerShellOutput("  - Up/Down: Navigate history\n", RGB(150, 150, 150));
    appendPowerShellOutput("  - Ctrl+L: Clear console\n", RGB(150, 150, 150));
    appendPowerShellOutput("  - Ctrl+`: Toggle panel\n", RGB(150, 150, 150));
    appendPowerShellOutput("\n" + getPowerShellPrompt(), RGB(0, 255, 0));
}

void Win32IDE::createPowerShellToolbar() {
    if (!m_hwndPowerShellPanel) return;
    
    // Create toolbar buttons
    int btnX = 5;
    int btnY = 5;
    int btnWidth = 90;
    int btnHeight = 25;
    int btnSpacing = 5;
    
    m_hwndPSBtnClear = CreateWindowEx(
        0, L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_BTN_CLEAR,
        m_hInstance, NULL
    );
    btnX += btnWidth + btnSpacing;
    
    m_hwndPSBtnStop = CreateWindowEx(
        0, L"BUTTON", L"Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_BTN_STOP,
        m_hInstance, NULL
    );
    btnX += btnWidth + btnSpacing;
    
    m_hwndPSBtnHistory = CreateWindowEx(
        0, L"BUTTON", L"History",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_BTN_HISTORY,
        m_hInstance, NULL
    );
    btnX += btnWidth + btnSpacing;
    
    m_hwndPSBtnRestart = CreateWindowEx(
        0, L"BUTTON", L"Restart",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, btnWidth, btnHeight,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_BTN_RESTART,
        m_hInstance, NULL
    );
    btnX += btnWidth + btnSpacing;
    
    m_hwndPSBtnLoadRawrXD = CreateWindowEx(
        0, L"BUTTON", L"Load RawrXD",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnX, btnY, 120, btnHeight,
        m_hwndPowerShellPanel,
        (HMENU)IDC_PS_BTN_LOAD_RAWRXD,
        m_hInstance, NULL
    );
}

void Win32IDE::initializePowerShellPanel() {
    // Start dedicated PowerShell terminal
    m_dedicatedPowerShellTerminal = std::make_unique<Win32TerminalManager>();
    
    // Set up callbacks
    m_dedicatedPowerShellTerminal->onOutput = [this](const std::string& output) {
        appendPowerShellOutput(output, RGB(200, 200, 200));
    };
    
    m_dedicatedPowerShellTerminal->onError = [this](const std::string& error) {
        appendPowerShellOutput("[ERROR] " + error, RGB(255, 100, 100));
    };
    
    m_dedicatedPowerShellTerminal->onStarted = [this]() {
        m_powerShellSessionActive = true;
        updatePowerShellStatus();
    };
    
    m_dedicatedPowerShellTerminal->onFinished = [this](int exitCode) {
        m_powerShellSessionActive = false;
        appendPowerShellOutput("\n[PowerShell session ended with code: " + std::to_string(exitCode) + "]\n", RGB(255, 255, 0));
        updatePowerShellStatus();
    };
    
    // Start PowerShell
    startPowerShellSession();
}

// ============================================================================
// POWERSHELL PANEL VISIBILITY & LAYOUT
// ============================================================================

void Win32IDE::showPowerShellPanel() {
    if (m_hwndPowerShellPanel) {
        ShowWindow(m_hwndPowerShellPanel, SW_SHOW);
        m_powerShellPanelVisible = true;
        
        // Force layout update
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right - rc.left, rc.bottom - rc.top);
    }
}

void Win32IDE::hidePowerShellPanel() {
    if (m_hwndPowerShellPanel) {
        ShowWindow(m_hwndPowerShellPanel, SW_HIDE);
        m_powerShellPanelVisible = false;
        
        // Force layout update
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right - rc.left, rc.bottom - rc.top);
    }
}

void Win32IDE::togglePowerShellPanel() {
    if (m_powerShellPanelVisible) {
        hidePowerShellPanel();
    } else {
        showPowerShellPanel();
    }
}

void Win32IDE::layoutPowerShellPanel() {
    if (!m_hwndPowerShellPanel || !m_powerShellPanelVisible) {
        return;
    }
    
    RECT mainRect;
    GetClientRect(m_hwndMain, &mainRect);
    
    int mainWidth = mainRect.right - mainRect.left;
    int mainHeight = mainRect.bottom - mainRect.top;
    
    // Position at bottom of IDE
    int panelTop = mainHeight - m_powerShellPanelHeight;
    
    SetWindowPos(m_hwndPowerShellPanel, NULL,
        0, panelTop,
        mainWidth, m_powerShellPanelHeight,
        SWP_NOZORDER);
    
    updatePowerShellPanelLayout(mainWidth, m_powerShellPanelHeight);
}

void Win32IDE::updatePowerShellPanelLayout(int width, int height) {
    if (!m_hwndPowerShellPanel) return;
    
    // Layout internal controls
    if (m_hwndPowerShellOutput) {
        SetWindowPos(m_hwndPowerShellOutput, NULL,
            5, 35,
            width - 10, height - 95,
            SWP_NOZORDER);
    }
    
    if (m_hwndPowerShellInput) {
        SetWindowPos(m_hwndPowerShellInput, NULL,
            5, height - 55,
            width - 110, 25,
            SWP_NOZORDER);
    }
    
    if (m_hwndPSBtnExecute) {
        SetWindowPos(m_hwndPSBtnExecute, NULL,
            width - 100, height - 55,
            90, 25,
            SWP_NOZORDER);
    }
    
    if (m_hwndPowerShellStatusBar) {
        SetWindowPos(m_hwndPowerShellStatusBar, NULL,
            5, height - 25,
            width - 10, 20,
            SWP_NOZORDER);
    }
}

void Win32IDE::resizePowerShellPanel(int width, int height) {
    m_powerShellPanelHeight = height;
    layoutPowerShellPanel();
}

// ============================================================================
// POWERSHELL EXECUTION
// ============================================================================

void Win32IDE::executePowerShellInput() {
    if (!m_hwndPowerShellInput) return;
    
    char buffer[4096];
    GetWindowTextA(m_hwndPowerShellInput, buffer, sizeof(buffer));
    
    std::string command(buffer);
    if (command.empty()) {
        return;
    }
    
    // Clear input
    SetWindowText(m_hwndPowerShellInput, L"");
    
    // Add to history
    addPowerShellHistory(command);
    
    // Echo command
    appendPowerShellOutput(getPowerShellPrompt() + command + "\n", RGB(255, 255, 255));
    
    // Execute
    executePowerShellPanelCommand(command);
}

void Win32IDE::executePowerShellPanelCommand(const std::string& command) {
    if (!m_dedicatedPowerShellTerminal || !m_powerShellSessionActive) {
        appendPowerShellOutput("[ERROR] PowerShell session not active\n", RGB(255, 0, 0));
        return;
    }
    
    m_powerShellExecuting = true;
    updatePowerShellStatus();
    
    // Send command to PowerShell
    m_dedicatedPowerShellTerminal->writeInput(command + "\r\n");
    
    m_powerShellExecuting = false;
    updatePowerShellStatus();
}

void Win32IDE::stopPowerShellExecution() {
    if (m_dedicatedPowerShellTerminal && m_powerShellSessionActive) {
        // Send Ctrl+C
        m_dedicatedPowerShellTerminal->writeInput("\x03");
        appendPowerShellOutput("\n[Execution stopped]\n", RGB(255, 255, 0));
    }
}

void Win32IDE::clearPowerShellConsole() {
    if (m_hwndPowerShellOutput) {
        SetWindowText(m_hwndPowerShellOutput, L"");
        appendPowerShellOutput(getPowerShellPrompt(), RGB(0, 255, 0));
    }
}

void Win32IDE::appendPowerShellOutput(const std::string& text, COLORREF color) {
    if (!m_hwndPowerShellOutput) return;
    
    // Get current text length
    int len = GetWindowTextLength(m_hwndPowerShellOutput);
    
    // Select end
    SendMessage(m_hwndPowerShellOutput, EM_SETSEL, len, len);
    
    // Set color
    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(CHARFORMAT2);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessage(m_hwndPowerShellOutput, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    // Append text
    SendMessage(m_hwndPowerShellOutput, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    
    // Scroll to bottom
    scrollPowerShellOutputToBottom();
}

// ============================================================================
// POWERSHELL HISTORY
// ============================================================================

void Win32IDE::addPowerShellHistory(const std::string& command) {
    if (command.empty()) return;
    
    // Don't add duplicates
    if (!m_powerShellCommandHistory.empty() && 
        m_powerShellCommandHistory.back() == command) {
        return;
    }
    
    m_powerShellCommandHistory.push_back(command);
    
    // Limit history size
    if (m_powerShellCommandHistory.size() > m_maxPowerShellHistory) {
        m_powerShellCommandHistory.erase(m_powerShellCommandHistory.begin());
    }
    
    // Reset history index
    m_powerShellHistoryIndex = m_powerShellCommandHistory.size();
}

void Win32IDE::navigatePowerShellHistoryUp() {
    if (m_powerShellCommandHistory.empty()) return;
    
    if (m_powerShellHistoryIndex > 0) {
        m_powerShellHistoryIndex--;
        SetWindowTextA(m_hwndPowerShellInput, 
            m_powerShellCommandHistory[m_powerShellHistoryIndex].c_str());
        
        // Select all text
        SendMessage(m_hwndPowerShellInput, EM_SETSEL, 0, -1);
    }
}

void Win32IDE::navigatePowerShellHistoryDown() {
    if (m_powerShellCommandHistory.empty()) return;
    
    if (m_powerShellHistoryIndex < static_cast<int>(m_powerShellCommandHistory.size()) - 1) {
        m_powerShellHistoryIndex++;
        SetWindowTextA(m_hwndPowerShellInput,
            m_powerShellCommandHistory[m_powerShellHistoryIndex].c_str());
        
        // Select all text
        SendMessage(m_hwndPowerShellInput, EM_SETSEL, 0, -1);
    } else if (m_powerShellHistoryIndex == static_cast<int>(m_powerShellCommandHistory.size()) - 1) {
        m_powerShellHistoryIndex++;
        SetWindowText(m_hwndPowerShellInput, L"");
    }
}

void Win32IDE::showPowerShellHistory() {
    if (m_powerShellCommandHistory.empty()) {
        MessageBoxA(m_hwndMain, "No command history", "PowerShell History", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    std::string history;
    for (size_t i = 0; i < m_powerShellCommandHistory.size(); i++) {
        history += std::to_string(i + 1) + ": " + m_powerShellCommandHistory[i] + "\n";
    }
    
    MessageBoxA(m_hwndMain, history.c_str(), "PowerShell Command History", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// POWERSHELL SESSION MANAGEMENT
// ============================================================================

void Win32IDE::startPowerShellSession() {
    if (m_powerShellSessionActive) {
        return;
    }
    
    if (!m_dedicatedPowerShellTerminal) {
        m_dedicatedPowerShellTerminal = std::make_unique<Win32TerminalManager>();
    }
    
    bool started = m_dedicatedPowerShellTerminal->start(Win32TerminalManager::PowerShell);
    
    if (started) {
        m_powerShellSessionActive = true;
        appendPowerShellOutput("[PowerShell session started]\n", RGB(0, 255, 0));
    } else {
        appendPowerShellOutput("[ERROR] Failed to start PowerShell session\n", RGB(255, 0, 0));
    }
    
    updatePowerShellStatus();
}

void Win32IDE::restartPowerShellSession() {
    stopPowerShellSession();
    Sleep(500); // Brief delay
    startPowerShellSession();
}

void Win32IDE::stopPowerShellSession() {
    if (m_dedicatedPowerShellTerminal) {
        m_dedicatedPowerShellTerminal->stop();
        m_powerShellSessionActive = false;
        appendPowerShellOutput("[PowerShell session stopped]\n", RGB(255, 255, 0));
    }
    
    updatePowerShellStatus();
}

bool Win32IDE::isPowerShellSessionActive() const {
    return m_powerShellSessionActive;
}

void Win32IDE::updatePowerShellStatus() {
    if (!m_hwndPowerShellStatusBar) return;
    
    std::string status = "PowerShell: ";
    
    if (m_powerShellSessionActive) {
        status += "Active";
        if (m_powerShellExecuting) {
            status += " (Executing...)";
        }
        if (m_powerShellRawrXDLoaded) {
            status += " | RawrXD Module: Loaded";
        }
    } else {
        status += "Not Active";
    }
    
    status += " | " + getPowerShellVersion();
    
    SetWindowTextA(m_hwndPowerShellStatusBar, status.c_str());
}

// ============================================================================
// RAWRXD.PS1 INTEGRATION
// ============================================================================

void Win32IDE::loadRawrXDModule() {
    if (m_powerShellRawrXDLoaded) {
        appendPowerShellOutput("[RawrXD module already loaded]\n", RGB(255, 255, 0));
        return;
    }
    
    appendPowerShellOutput("[Loading RawrXD.ps1 module...]\n", RGB(0, 255, 255));
    
    bool success = loadRawrXDPowerShellModule();
    
    if (success) {
        m_powerShellRawrXDLoaded = true;
        appendPowerShellOutput("[SUCCESS] RawrXD module loaded!\n", RGB(0, 255, 0));
        appendPowerShellOutput("Available functions:\n", RGB(255, 255, 0));
        appendPowerShellOutput("  - Open-GGUFModel\n", RGB(150, 150, 150));
        appendPowerShellOutput("  - Invoke-PoshLLMInference\n", RGB(150, 150, 150));
        appendPowerShellOutput("  - Get-PoshLLMStatus\n", RGB(150, 150, 150));
        appendPowerShellOutput("\n" + getPowerShellPrompt(), RGB(0, 255, 0));
    } else {
        appendPowerShellOutput("[ERROR] Failed to load RawrXD module\n", RGB(255, 0, 0));
        appendPowerShellOutput("Make sure RawrXD.ps1 is in the Powershield directory\n", RGB(255, 100, 100));
    }
    
    updatePowerShellStatus();
}

void Win32IDE::unloadRawrXDModule() {
    if (!m_powerShellRawrXDLoaded) {
        return;
    }
    
    // There's no easy way to unload functions in PowerShell
    // So we just mark it as unloaded
    m_powerShellRawrXDLoaded = false;
    appendPowerShellOutput("[RawrXD module marked as unloaded]\n", RGB(255, 255, 0));
    updatePowerShellStatus();
}

void Win32IDE::executeRawrXDCommand(const std::string& command) {
    if (!m_powerShellRawrXDLoaded) {
        loadRawrXDModule();
    }
    
    executePowerShellPanelCommand(command);
}

void Win32IDE::quickLoadGGUFModel() {
    // Simple dialog to load a GGUF model
    std::string modelPath = getFileDialogPath(false);
    
    if (!modelPath.empty()) {
        std::string command = "Open-GGUFModel -ModelPath '" + modelPath + "' -MaxZoneMB 512";
        executeRawrXDCommand(command);
    }
}

void Win32IDE::quickInference() {
    // Simple dialog for inference
    char prompt[1024] = "";
    
    if (MessageBoxA(m_hwndMain, "Enter your prompt in the PowerShell console using:\nInvoke-PoshLLMInference -Prompt 'your prompt' -MaxTokens 100",
        "Quick Inference", MB_OKCANCEL | MB_ICONINFORMATION) == IDOK) {
        
        // Focus the input
        SetFocus(m_hwndPowerShellInput);
        SetWindowText(m_hwndPowerShellInput, L"Invoke-PoshLLMInference -Prompt '' -MaxTokens 100");
        
        // Position cursor before second quote
        SendMessage(m_hwndPowerShellInput, EM_SETSEL, 36, 36);
    }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

std::string Win32IDE::getPowerShellPrompt() {
    return "PS> ";
}

void Win32IDE::scrollPowerShellOutputToBottom() {
    if (!m_hwndPowerShellOutput) return;
    
    // Scroll to bottom
    SendMessage(m_hwndPowerShellOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

// ============================================================================
// WINDOW PROCEDURES
// ============================================================================

LRESULT CALLBACK Win32IDE::PowerShellPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetProp(hwnd, L"IDE_PTR");
    
    switch (uMsg) {
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            
            switch (id) {
                case IDC_PS_BTN_EXECUTE:
                    if (ide) ide->executePowerShellInput();
                    break;
                case IDC_PS_BTN_CLEAR:
                    if (ide) ide->clearPowerShellConsole();
                    break;
                case IDC_PS_BTN_STOP:
                    if (ide) ide->stopPowerShellExecution();
                    break;
                case IDC_PS_BTN_HISTORY:
                    if (ide) ide->showPowerShellHistory();
                    break;
                case IDC_PS_BTN_RESTART:
                    if (ide) ide->restartPowerShellSession();
                    break;
                case IDC_PS_BTN_LOAD_RAWRXD:
                    if (ide) ide->loadRawrXDModule();
                    break;
            }
            break;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Win32IDE::PowerShellInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetProp(hwnd, L"IDE_PTR");
    WNDPROC oldProc = (WNDPROC)GetProp(hwnd, L"OLDPROC");
    
    switch (uMsg) {
        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_RETURN:
                    // Execute command on Enter
                    if (ide) ide->executePowerShellInput();
                    return 0;
                    
                case VK_UP:
                    // Navigate history up
                    if (ide) ide->navigatePowerShellHistoryUp();
                    return 0;
                    
                case VK_DOWN:
                    // Navigate history down
                    if (ide) ide->navigatePowerShellHistoryDown();
                    return 0;
                    
                case 'L':
                    // Ctrl+L - Clear console
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        if (ide) ide->clearPowerShellConsole();
                        return 0;
                    }
                    break;
            }
            break;
        }
    }
    
    return CallWindowProc(oldProc, hwnd, uMsg, wParam, lParam);
}
