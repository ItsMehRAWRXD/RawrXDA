// Win32IDE_VSCodeUI.cpp - VS Code-like UI Components Implementation
// Activity Bar, Secondary Sidebar, Panel (Terminal/Output/Problems/Debug Console), Enhanced Status Bar

#include "Win32IDE.h"
#include "../ui/tool_action_status.h"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <map>

// SCAFFOLD_270: Status bar and Copilot status


// SCAFFOLD_024: Problems list and go-to-problem


// SCAFFOLD_004: Panel container (Terminal, Output, Problems)


// SCAFFOLD_003: Secondary sidebar (Copilot chat) creation


// SCAFFOLD_002: Activity bar and primary sidebar layout


// Define GET_X_LPARAM and GET_Y_LPARAM if not available
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Define IDC_STATUS_BAR if not defined
#ifndef IDC_STATUS_BAR
#define IDC_STATUS_BAR 2000
#endif

#pragma comment(lib, "comctl32.lib")

// Activity Bar button IDs
#define IDC_ACTIVITY_BAR 1100
#define IDC_ACTBAR_EXPLORER 1101
#define IDC_ACTBAR_SEARCH 1102
#define IDC_ACTBAR_SCM 1103
#define IDC_ACTBAR_DEBUG 1104
#define IDC_ACTBAR_EXTENSIONS 1105
#define IDC_ACTBAR_SETTINGS 1106
#define IDC_ACTBAR_ACCOUNTS 1107

// Secondary Sidebar IDs
#define IDC_SECONDARY_SIDEBAR 1200
#define IDC_SECONDARY_SIDEBAR_HEADER 1201
#define IDC_COPILOT_CHAT_INPUT 1202
#define IDC_COPILOT_CHAT_OUTPUT 1203
#define IDC_COPILOT_SEND_BTN 1204
#define IDC_COPILOT_CLEAR_BTN 1205
#define IDC_MODEL_SELECTOR 1208
#define IDC_MODEL_BROWSE_BTN 1209

// Panel IDs
#define IDC_PANEL_CONTAINER 1300
#define IDC_PANEL_TABS 1301
#define IDC_PANEL_TERMINAL 1302
#define IDC_PANEL_OUTPUT 1303
#define IDC_PANEL_PROBLEMS 1304
#define IDC_PANEL_DEBUG_CONSOLE 1305
#define IDC_PANEL_TOOLBAR 1306
#define IDC_PANEL_BTN_NEW_TERMINAL 1307
#define IDC_PANEL_BTN_SPLIT_TERMINAL 1308
#define IDC_PANEL_BTN_KILL_TERMINAL 1309
#define IDC_PANEL_BTN_MAXIMIZE 1310
#define IDC_PANEL_BTN_CLOSE 1311
#define IDC_PANEL_PROBLEMS_LIST 1312

// Status Bar item IDs
#define IDC_STATUS_REMOTE 1400
#define IDC_STATUS_BRANCH 1401
#define IDC_STATUS_SYNC 1402
#define IDC_STATUS_ERRORS 1403
#define IDC_STATUS_WARNINGS 1404
#define IDC_STATUS_LINE_COL 1405
#define IDC_STATUS_SPACES 1406
#define IDC_STATUS_ENCODING 1407
#define IDC_STATUS_EOL 1408
#define IDC_STATUS_LANGUAGE 1409
#define IDC_STATUS_COPILOT 1410
#define IDC_STATUS_NOTIFICATIONS 1411

// VS Code-like colors
static const COLORREF VSCODE_ACTIVITY_BAR_BG = RGB(51, 51, 51);      // Dark gray
static const COLORREF VSCODE_ACTIVITY_BAR_ACTIVE = RGB(37, 37, 38);  // Slightly lighter
static const COLORREF VSCODE_ACTIVITY_BAR_HOVER = RGB(90, 93, 94);   // Hover highlight
static const COLORREF VSCODE_ACTIVITY_BAR_ICON = RGB(204, 204, 204); // Icon color
static const COLORREF VSCODE_ACTIVITY_BAR_INDICATOR = RGB(0, 122, 204); // Active indicator blue

static const COLORREF VSCODE_SIDEBAR_BG = RGB(37, 37, 38);
static const COLORREF VSCODE_PANEL_BG = RGB(30, 30, 30);
static const COLORREF VSCODE_STATUS_BAR_BG = RGB(0, 122, 204);       // Blue for normal
static const COLORREF VSCODE_STATUS_BAR_DEBUG = RGB(204, 102, 0);    // Orange for debug
static const COLORREF VSCODE_STATUS_BAR_REMOTE = RGB(22, 130, 93);   // Green for remote
static const COLORREF VSCODE_STATUS_BAR_TEXT = RGB(255, 255, 255);

// Unicode icons for Activity Bar (simple ASCII fallbacks)
static const char* ICON_EXPLORER = "[]";     // File explorer
static const char* ICON_SEARCH = "()";       // Search
static const char* ICON_SCM = "<>";          // Source control
static const char* ICON_DEBUG = ">";         // Run/Debug
static const char* ICON_EXTENSIONS = "++";   // Extensions
static const char* ICON_SETTINGS = "*";      // Settings gear
static const char* ICON_ACCOUNTS = "@";      // User account

// ============================================================================
// Activity Bar (Far Left) - VS Code style vertical icon bar
// ============================================================================

void Win32IDE::createActivityBarUI(HWND hwndParent)
{
    // Create brushes for Activity Bar colors
    m_actBarBackgroundBrush = CreateSolidBrush(VSCODE_ACTIVITY_BAR_BG);
    m_actBarHoverBrush = CreateSolidBrush(VSCODE_ACTIVITY_BAR_HOVER);
    m_actBarActiveBrush = CreateSolidBrush(VSCODE_ACTIVITY_BAR_ACTIVE);
    
    // Create the Activity Bar container
    m_hwndActivityBar = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, ACTIVITY_BAR_WIDTH, 600,
        hwndParent, (HMENU)IDC_ACTIVITY_BAR, m_hInstance, nullptr);
    
    // Set the background color
    SetClassLongPtr(m_hwndActivityBar, GCLP_HBRBACKGROUND, (LONG_PTR)m_actBarBackgroundBrush);
    
    // Create Activity Bar buttons (icons)
    const char* buttonLabels[] = { ICON_EXPLORER, ICON_SEARCH, ICON_SCM, ICON_DEBUG, ICON_EXTENSIONS, ICON_SETTINGS, ICON_ACCOUNTS };
    const char* tooltips[] = { "Explorer (Ctrl+Shift+E)", "Search (Ctrl+Shift+F)", "Source Control (Ctrl+Shift+G)", 
                               "Run and Debug (Ctrl+Shift+D)", "Extensions (Ctrl+Shift+X)", "Settings", "Accounts" };
    int buttonHeight = 48;
    
    for (int i = 0; i < 7; i++) {
        int yPos = (i < 5) ? (i * buttonHeight) : (600 - (7 - i) * buttonHeight); // Top 5 + bottom 2
        
        m_activityBarButtons[i] = CreateWindowExA(
            0, "BUTTON", buttonLabels[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            0, yPos, ACTIVITY_BAR_WIDTH, buttonHeight,
            m_hwndActivityBar, (HMENU)(IDC_ACTBAR_EXPLORER + i), m_hInstance, nullptr);
        
        // Store IDE pointer for button subclass
        SetWindowLongPtr(m_activityBarButtons[i], GWLP_USERDATA, (LONG_PTR)this);
        
        // Create tooltip
        HWND hwndTip = CreateWindowEx(0, TOOLTIPS_CLASS, nullptr,
            WS_POPUP | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
            hwndParent, nullptr, m_hInstance, nullptr);
        
        TOOLINFOA ti = { sizeof(TOOLINFOA) };
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = hwndParent;
        ti.uId = (UINT_PTR)m_activityBarButtons[i];
        ti.lpszText = const_cast<char*>(tooltips[i]);
        SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }
    
    m_activeActivityBarButton = 0; // Explorer is active by default
    m_sidebarVisible = true;
    m_sidebarWidth = 260;
}

void Win32IDE::updateActivityBarState()
{
    // Repaint all activity bar buttons to reflect current state
    for (int i = 0; i < 7; i++) {
        if (m_activityBarButtons[i]) {
            InvalidateRect(m_activityBarButtons[i], nullptr, TRUE);
        }
    }
}

LRESULT CALLBACK Win32IDE::ActivityBarButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (uMsg) {
    case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            int buttonIndex = dis->CtlID - IDC_ACTBAR_EXPLORER;
            
            // Draw background
            COLORREF bgColor = (buttonIndex == pThis->m_activeActivityBarButton) 
                ? VSCODE_ACTIVITY_BAR_ACTIVE : VSCODE_ACTIVITY_BAR_BG;
            
            if (dis->itemState & ODS_SELECTED) {
                bgColor = VSCODE_ACTIVITY_BAR_HOVER;
            }
            
            HBRUSH hBrush = CreateSolidBrush(bgColor);
            FillRect(dis->hDC, &dis->rcItem, hBrush);
            DeleteObject(hBrush);
            
            // Draw active indicator (left border)
            if (buttonIndex == pThis->m_activeActivityBarButton) {
                RECT indicatorRect = dis->rcItem;
                indicatorRect.right = 3;
                HBRUSH hIndicator = CreateSolidBrush(VSCODE_ACTIVITY_BAR_INDICATOR);
                FillRect(dis->hDC, &indicatorRect, hIndicator);
                DeleteObject(hIndicator);
            }
            
            // Draw icon text centered
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, VSCODE_ACTIVITY_BAR_ICON);
            
            char buttonText[16];
            GetWindowTextA(hwnd, buttonText, 16);
            DrawTextA(dis->hDC, buttonText, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            
            return TRUE;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Secondary Sidebar (Right) - AI Chat / Copilot area
// ============================================================================

void Win32IDE::createSecondarySidebar(HWND hwndParent)
{
    // NOTE: The authoritative chat panel is created by createChatPanel() (Win32IDE.cpp),
    // called from Win32IDE_Core.cpp::onCreate().  This function is retained for
    // source compatibility only and is NOT called in the active build path.
    // All chat control layout, SidebarProcImpl installation, and model selector
    // population live in createChatPanel() — edit that function, not this one.
    (void)hwndParent;
    return;

#if 0  // Dead path — kept for reference only
    m_secondarySidebarVisible = true;
    m_secondarySidebarWidth = 320;
    
    // Create the secondary sidebar container
    m_hwndSecondarySidebar = CreateWindowExA(
        WS_EX_CLIENTEDGE, "STATIC", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, m_secondarySidebarWidth, 600,
        hwndParent, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);

    if (m_hwndSecondarySidebar)
    {
        SetWindowLongPtrA(m_hwndSecondarySidebar, GWLP_USERDATA, (LONG_PTR)this);
        m_oldSidebarProc = (WNDPROC)SetWindowLongPtrA(m_hwndSecondarySidebar, GWLP_WNDPROC, (LONG_PTR)SidebarProc);
    }
    
    // Header label
    m_hwndSecondarySidebarHeader = CreateWindowExA(
        0, "STATIC", " GitHub Copilot Chat",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, m_secondarySidebarWidth, 28,
        m_hwndSecondarySidebar, (HMENU)IDC_SECONDARY_SIDEBAR_HEADER, m_hInstance, nullptr);

    HWND hModelLabel = CreateWindowExA(
        0, "STATIC", "Model:",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        5, 32, 50, 20,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndModelSelector = CreateWindowExA(
        0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        60, 32, m_secondarySidebarWidth - 145, 280,
        m_hwndSecondarySidebar, (HMENU)IDC_MODEL_SELECTOR, m_hInstance, nullptr);

    HWND hBrowseBtn = CreateWindowExA(
        0, "BUTTON", "Browse...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        m_secondarySidebarWidth - 80, 32, 75, 22,
        m_hwndSecondarySidebar, (HMENU)IDC_MODEL_BROWSE_BTN, m_hInstance, nullptr);

    if (m_hFontUI)
    {
        if (hModelLabel)
            SendMessage(hModelLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        if (m_hwndModelSelector)
            SendMessage(m_hwndModelSelector, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        if (hBrowseBtn)
            SendMessage(hBrowseBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    }

    if (m_hwndModelSelector)
    {
        populateModelSelector();
    }
    
    // Chat output area (read-only rich edit for formatted messages)
    m_hwndCopilotChatOutput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        5, 60, m_secondarySidebarWidth - 10, 422,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_OUTPUT, m_hInstance, nullptr);
    
    // Chat input area
    m_hwndCopilotChatInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        5, 490, m_secondarySidebarWidth - 10, 60,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_INPUT, m_hInstance, nullptr);
    
    // Send button
    m_hwndCopilotSendBtn = CreateWindowExA(
        0, "BUTTON", "Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 555, 80, 28,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);
    
    // Clear button
    m_hwndCopilotClearBtn = CreateWindowExA(
        0, "BUTTON", "Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        90, 555, 80, 28,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);
    
    // Set initial message
    SetWindowTextA(m_hwndCopilotChatOutput, 
        "GitHub Copilot Chat\r\n"
        "==================\r\n\r\n"
        "Ask me anything about your code!\r\n\r\n"
        "Examples:\r\n"
        "- Explain this code\r\n"
        "- How do I fix this error?\r\n"
        "- Generate unit tests\r\n"
        "- Refactor this function\r\n");

    // If the full chat panel added mode toggles elsewhere, this path has none — safe no-op when null.
    syncAgentModeUiFromBridge();
#endif  // Dead path end
}

// Implemented in src/win32app/Win32IDE_Sidebar.cpp (avoid duplicate definition / LNK2005).

void Win32IDE::updateSecondarySidebarContent()
{
    // If history is empty the welcome banner (set by clearCopilotChat) is already
    // in the output control — leave it untouched so it doesn't get overwritten
    // by an empty render pass.
    if (m_chatHistory.empty()) return;

    // Update chat display with history + tool action status + working bubbles
    std::string chatText;
    for (size_t i = 0; i < m_chatHistory.size(); ++i) {
        const auto& msg = m_chatHistory[i];
        if (msg.first == "user") {
            chatText += "You: " + msg.second + "\r\n\r\n";
        } else {
            // Render tool action status block before assistant message
            auto it = m_chatToolActions.find(i);
            if (it != m_chatToolActions.end() && !it->second.empty()) {
                chatText += RawrXD::UI::ToolActionStatusFormatter::formatPlainTextBlock(
                    it->second, static_cast<int>(it->second.size()));
                chatText += "\r\n";
            }

            // Render working bubbles (plain text) if accumulator has any
            if (m_currentToolActions.workingBubbles().size() > 0) {
                chatText += m_currentToolActions.renderBubblesPlainText();
                chatText += "\r\n";
            }

            chatText += "Copilot: " + msg.second + "\r\n\r\n";
        }
    }
    SetWindowTextA(m_hwndCopilotChatOutput, chatText.c_str());
    
    // Scroll to bottom
    int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
    SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
    SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::clearCopilotChat()
{
    m_chatHistory.clear();
    SetWindowTextA(m_hwndCopilotChatOutput, 
        "GitHub Copilot Chat\r\n"
        "==================\r\n\r\n"
        "Chat cleared. Ask me anything about your code!\r\n");
}

void Win32IDE::appendCopilotResponse(const std::string& response)
{
    m_chatHistory.push_back({"assistant", response});
    updateSecondarySidebarContent();
}

// WM_APP+550 — posted from background inference thread once a chat response is ready.
// lParam = heap-allocated std::string* (owned by message; receiver must delete).
#define WM_CHAT_RESPONSE (WM_APP + 550)

void Win32IDE::sendCopilotMessage(const std::string& message)
{
    if (message.empty()) return;

    RAWRXD_LOG_INFO("Win32IDE_VSCodeUI") << "=== SEND MESSAGE ===";

    // Show user turn and a "Thinking…" placeholder immediately so the UI reacts at click time.
    m_chatHistory.push_back({"user", message});
    m_chatHistory.push_back({"assistant", "\u23F3 Thinking\u2026"});
    updateSecondarySidebarContent();

    // Clear the input field immediately so the user can start composing a follow-up.
    SetWindowTextA(m_hwndCopilotChatInput, "");

    // Run inference on a background thread to keep the UI responsive.
    // When done, post WM_CHAT_RESPONSE to the sidebar so SidebarProcImpl
    // replaces the placeholder on the UI thread.
    HWND hSidebar = m_hwndSecondarySidebar;
    std::thread([this, hSidebar, message]() {
        AgentResponse ar = sendMessageToModel(message);
        std::string response = ar.content;
        if (response.empty())
            response = "\u26A0\uFE0F No response generated by the current backend.";

        // Prepend executor label so the UI shows which backend handled the request
        if (!ar.executorLabel.empty())
            response = "[Executor: " + ar.executorLabel + "]\n" + response;

        RAWRXD_LOG_INFO("Win32IDE_VSCodeUI") << "[sendCopilotMessage] response ready, length=" << response.size();
        // Ownership of the heap string transfers to the message recipient.
        PostMessage(hSidebar, WM_CHAT_RESPONSE, 0, (LPARAM)(new std::string(response)));
    }).detach();
}

void Win32IDE::togglePanel()
{
    m_panelVisible = !m_panelVisible;
    ShowWindow(m_hwndPanelContainer, m_panelVisible ? SW_SHOW : SW_HIDE);
    
    // Trigger resize to update layout
    RECT rect;
    GetClientRect(m_hwndMain, &rect);
    onSize(rect.right, rect.bottom);
}

void Win32IDE::maximizePanel()
{
    m_panelMaximized = !m_panelMaximized;
    
    if (m_panelMaximized) {
        // Store original height and maximize
        RECT rect;
        GetClientRect(m_hwndMain, &rect);
        m_panelHeight = rect.bottom - 100; // Leave some space for toolbar/status
        SetWindowTextA(m_hwndPanelMaximizeBtn, "v");
    } else {
        // Restore to default height
        m_panelHeight = 250;
        SetWindowTextA(m_hwndPanelMaximizeBtn, "^");
    }
    
    // Trigger resize
    RECT rect;
    GetClientRect(m_hwndMain, &rect);
    onSize(rect.right, rect.bottom);
}

void Win32IDE::restorePanel()
{
    if (m_panelMaximized) {
        maximizePanel(); // Toggle back to normal
    }
}

void Win32IDE::switchPanelTab(PanelTab tab)
{
    m_activePanelTab = tab;
    
    // Show/hide appropriate views
    bool showTerminal = (tab == PanelTab::Terminal);
    bool showOutput = (tab == PanelTab::Output);
    bool showProblems = (tab == PanelTab::Problems);
    bool showDebugConsole = (tab == PanelTab::DebugConsole);
    
    // Show/hide terminal panes
    for (auto& pane : m_terminalPanes) {
        ShowWindow(pane.hwnd, showTerminal ? SW_SHOW : SW_HIDE);
    }
    
    // Show/hide output windows
    for (auto& kv : m_outputWindows) {
        bool show = showOutput && (kv.first == m_activeOutputTab);
        ShowWindow(kv.second, show ? SW_SHOW : SW_HIDE);
    }
    
    // Show/hide problems list
    ShowWindow(m_hwndProblemsListView, showProblems ? SW_SHOW : SW_HIDE);
    
    // Show/hide debug console
    if (m_hwndDebugConsole) {
        ShowWindow(m_hwndDebugConsole, showDebugConsole ? SW_SHOW : SW_HIDE);
    }
    
    // Update tab selection
    TabCtrl_SetCurSel(m_hwndPanelTabs, static_cast<int>(tab));
    
    // Update toolbar buttons based on current tab
    bool isTerminalTab = (tab == PanelTab::Terminal);
    EnableWindow(m_hwndPanelNewTerminalBtn, isTerminalTab);
    EnableWindow(m_hwndPanelSplitTerminalBtn, isTerminalTab);
    EnableWindow(m_hwndPanelKillTerminalBtn, isTerminalTab);
}

void Win32IDE::updatePanelContent()
{
    // Update problems count in tab
    std::string problemsTabText = "PROBLEMS";
    if (m_errorCount > 0 || m_warningCount > 0) {
        std::ostringstream oss;
        oss << "PROBLEMS (" << m_errorCount << " errors, " << m_warningCount << " warnings)";
        problemsTabText = oss.str();
    }
    
    TCITEMA tie = { TCIF_TEXT };
    tie.pszText = const_cast<char*>(problemsTabText.c_str());
    TabCtrl_SetItem(m_hwndPanelTabs, 2, &tie);
}

void Win32IDE::addProblem(const std::string& file, int line, int col, const std::string& msg, int severity)
{
    ProblemItem problem;
    problem.file = file;
    problem.line = line;
    problem.column = col;
    problem.message = msg;
    problem.severity = severity;
    m_problems.push_back(problem);
    
    // Update counts
    if (severity == 0) m_errorCount++;
    else if (severity == 1) m_warningCount++;
    
    // Add to list view
    LVITEMA lvi = { 0 };
    lvi.mask = LVIF_TEXT;
    lvi.iItem = static_cast<int>(m_problems.size() - 1);
    
    const char* severityStr = (severity == 0) ? "Error" : (severity == 1) ? "Warning" : "Info";
    lvi.pszText = const_cast<char*>(severityStr);
    ListView_InsertItem(m_hwndProblemsListView, &lvi);
    
    // Set item text using direct SendMessage calls with ANSI structures
    LVITEMA lviSet = { 0 };
    lviSet.iSubItem = 1;
    lviSet.pszText = const_cast<char*>(msg.c_str());
    SendMessage(m_hwndProblemsListView, LVM_SETITEMTEXTA, lvi.iItem, (LPARAM)&lviSet);
    
    lviSet.iSubItem = 2;
    lviSet.pszText = const_cast<char*>(file.c_str());
    SendMessage(m_hwndProblemsListView, LVM_SETITEMTEXTA, lvi.iItem, (LPARAM)&lviSet);
    
    char lineStrBuf[32];
    _snprintf_s(lineStrBuf, sizeof(lineStrBuf), _TRUNCATE, "%d", line);
    lviSet.iSubItem = 3;
    lviSet.pszText = lineStrBuf;
    SendMessage(m_hwndProblemsListView, LVM_SETITEMTEXTA, lvi.iItem, (LPARAM)&lviSet);
    
    // Update panel content
    updatePanelContent();
    updateEnhancedStatusBar();
}

void Win32IDE::clearProblems()
{
    m_problems.clear();
    m_errorCount = 0;
    m_warningCount = 0;
    ListView_DeleteAllItems(m_hwndProblemsListView);
    updatePanelContent();
    updateEnhancedStatusBar();
}

void Win32IDE::goToProblem(int index)
{
    if (index < 0 || index >= static_cast<int>(m_problems.size())) return;
    
    const ProblemItem& problem = m_problems[index];
    
    // Open file if different from current
    if (problem.file != m_currentFile) {
        // Load the file
        std::ifstream file(problem.file);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            SetWindowTextA(m_hwndEditor, content.c_str());
            m_currentFile = problem.file;
            m_fileModified = false;
        }
    }
    
    // Go to line
    int lineIndex = SendMessage(m_hwndEditor, EM_LINEINDEX, problem.line - 1, 0);
    SendMessage(m_hwndEditor, EM_SETSEL, lineIndex + problem.column - 1, lineIndex + problem.column - 1);
    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
    SetFocus(m_hwndEditor);
}

void Win32IDE::updateProblemsPanel()
{
    updatePanelContent();
}

// ============================================================================
// Enhanced Status Bar - VS Code style with all status items
// ============================================================================

void Win32IDE::createEnhancedStatusBar(HWND hwndParent)
{
    // Initialize status bar info
    m_statusBarInfo.remoteName = "";
    m_statusBarInfo.branchName = "main";
    m_statusBarInfo.syncAhead = 0;
    m_statusBarInfo.syncBehind = 0;
    m_statusBarInfo.errors = 0;
    m_statusBarInfo.warnings = 0;
    m_statusBarInfo.line = 1;
    m_statusBarInfo.column = 1;
    m_statusBarInfo.spacesOrTabWidth = 4;
    m_statusBarInfo.useSpaces = true;
    m_statusBarInfo.encoding = "UTF-8";
    m_statusBarInfo.eolSequence = "CRLF";
    m_statusBarInfo.languageMode = "Plain Text";
    m_statusBarInfo.copilotActive = true;
    m_statusBarInfo.copilotSuggestions = 0;
    
    // Create status bar with multiple parts
    m_hwndStatusBar = CreateWindowExA(
        0, STATUSCLASSNAMEA, "",
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwndParent, (HMENU)IDC_STATUS_BAR, m_hInstance, nullptr);
    
    // Set up parts - 12 parts for all status items
    // [Remote][Branch][Sync][Errors][Warnings] ... [Line:Col][Spaces][Encoding][EOL][Language][Copilot]
    int parts[] = { 80, 150, 200, 250, 300, -1, 380, 440, 510, 560, 650, 700 };
    SendMessage(m_hwndStatusBar, SB_SETPARTS, 12, (LPARAM)parts);
    
    // Set initial text
    updateEnhancedStatusBar();
}

void Win32IDE::updateEnhancedStatusBar()
{
    if (!m_hwndStatusBar) return;
    
    // Part 0: Remote indicator (if connected)
    if (!m_statusBarInfo.remoteName.empty()) {
        std::string remoteText = ">< " + m_statusBarInfo.remoteName;
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)remoteText.c_str());
    } else {
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)"");
    }
    
    // Part 1: Branch indicator
    std::string branchText = "<> " + m_statusBarInfo.branchName;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 1, (LPARAM)branchText.c_str());
    
    // Part 2: Sync status (ahead/behind)
    std::ostringstream syncOss;
    if (m_statusBarInfo.syncAhead > 0 || m_statusBarInfo.syncBehind > 0) {
        syncOss << m_statusBarInfo.syncAhead << "↑ " << m_statusBarInfo.syncBehind << "↓";
    }
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 2, (LPARAM)syncOss.str().c_str());
    
    // Part 3: Errors count
    std::ostringstream errOss;
    errOss << "X " << m_statusBarInfo.errors;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 3, (LPARAM)errOss.str().c_str());
    
    // Part 4: Warnings count
    std::ostringstream warnOss;
    warnOss << "! " << m_statusBarInfo.warnings;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 4, (LPARAM)warnOss.str().c_str());
    
    // Part 5: Spacer / file info
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 5, (LPARAM)"");
    
    // Part 6: Line and Column
    std::ostringstream lineColOss;
    lineColOss << "Ln " << m_statusBarInfo.line << ", Col " << m_statusBarInfo.column;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 6, (LPARAM)lineColOss.str().c_str());
    
    // Part 7: Spaces/Tabs
    std::ostringstream spacesOss;
    spacesOss << (m_statusBarInfo.useSpaces ? "Spaces: " : "Tab Size: ") << m_statusBarInfo.spacesOrTabWidth;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 7, (LPARAM)spacesOss.str().c_str());
    
    // Part 8: Encoding
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 8, (LPARAM)m_statusBarInfo.encoding.c_str());
    
    // Part 9: End of Line
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 9, (LPARAM)m_statusBarInfo.eolSequence.c_str());
    
    // Part 10: Language Mode
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 10, (LPARAM)m_statusBarInfo.languageMode.c_str());
    
    // Part 11: Copilot status
    // Part 11: Backend / AI status + readiness beacon
    {
        std::string backendText = getActiveBackendName();
        if (!m_statusBarInfo.copilotActive)
            backendText += " (off)";
        bool modelReady = isModelLoaded();
        bool agentReady = static_cast<bool>(m_agenticBridge);
        backendText += std::string(" | Beacon:") + ((modelReady && agentReady) ? "READY" : "DEGRADED");
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 11, (LPARAM)backendText.c_str());
    }
}

void Win32IDE::updateCursorPosition()
{
    if (!m_hwndEditor) return;
    
    // Get current selection/cursor position
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    
    // Calculate line and column
    int charIndex = range.cpMin;
    int lineIndex = SendMessage(m_hwndEditor, EM_LINEFROMCHAR, charIndex, 0);
    int lineStart = SendMessage(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = charIndex - lineStart;
    
    m_statusBarInfo.line = lineIndex + 1;
    m_statusBarInfo.column = column + 1;
    
    updateEnhancedStatusBar();
}

void Win32IDE::updateLanguageMode()
{
    detectLanguageFromFile(m_currentFile);
    updateEnhancedStatusBar();
}

void Win32IDE::detectLanguageFromFile(const std::string& filePath)
{
    if (filePath.empty()) {
        m_statusBarInfo.languageMode = "Plain Text";
        return;
    }
    
    // Get file extension
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) {
        m_statusBarInfo.languageMode = "Plain Text";
        return;
    }
    
    std::string ext = filePath.substr(dotPos + 1);
    
    // Convert to lowercase
    for (char& c : ext) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    
    // Map extension to language mode
    static const std::map<std::string, std::string> extToLang = {
        {"cpp", "C++"},
        {"c", "C"},
        {"h", "C/C++ Header"},
        {"hpp", "C++ Header"},
        {"py", "Python"},
        {"js", "JavaScript"},
        {"ts", "TypeScript"},
        {"jsx", "JavaScript React"},
        {"tsx", "TypeScript React"},
        {"json", "JSON"},
        {"xml", "XML"},
        {"html", "HTML"},
        {"htm", "HTML"},
        {"css", "CSS"},
        {"scss", "SCSS"},
        {"less", "Less"},
        {"md", "Markdown"},
        {"txt", "Plain Text"},
        {"ps1", "PowerShell"},
        {"psm1", "PowerShell"},
        {"psd1", "PowerShell"},
        {"bat", "Batch"},
        {"cmd", "Batch"},
        {"sh", "Shell Script"},
        {"bash", "Shell Script"},
        {"zsh", "Shell Script"},
        {"java", "Java"},
        {"cs", "C#"},
        {"fs", "F#"},
        {"vb", "Visual Basic"},
        {"go", "Go"},
        {"rs", "Rust"},
        {"rb", "Ruby"},
        {"php", "PHP"},
        {"swift", "Swift"},
        {"kt", "Kotlin"},
        {"scala", "Scala"},
        {"lua", "Lua"},
        {"r", "R"},
        {"sql", "SQL"},
        {"yaml", "YAML"},
        {"yml", "YAML"},
        {"toml", "TOML"},
        {"ini", "INI"},
        {"cfg", "Config"},
        {"asm", "Assembly"},
        {"s", "Assembly"}
    };
    
    auto it = extToLang.find(ext);
    if (it != extToLang.end()) {
        m_statusBarInfo.languageMode = it->second;
    } else {
        m_statusBarInfo.languageMode = "Plain Text";
    }
}
