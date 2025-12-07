// Win32IDE_VSCodeUI.cpp - VS Code-like UI Components Implementation
// Activity Bar, Secondary Sidebar, Panel (Terminal/Output/Problems/Debug Console), Enhanced Status Bar

#include "Win32IDE.h"
#include <commctrl.h>
#include <richedit.h>
#include <sstream>
#include <iomanip>

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
    m_secondarySidebarVisible = true;
    m_secondarySidebarWidth = 320;
    
    // Create the secondary sidebar container
    m_hwndSecondarySidebar = CreateWindowExA(
        WS_EX_CLIENTEDGE, "STATIC", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, m_secondarySidebarWidth, 600,
        hwndParent, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);
    
    // Header label
    m_hwndSecondarySidebarHeader = CreateWindowExA(
        0, "STATIC", " GitHub Copilot Chat",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        0, 0, m_secondarySidebarWidth, 28,
        m_hwndSecondarySidebar, (HMENU)IDC_SECONDARY_SIDEBAR_HEADER, m_hInstance, nullptr);
    
    // Chat output area (read-only rich edit for formatted messages)
    m_hwndCopilotChatOutput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        5, 32, m_secondarySidebarWidth - 10, 450,
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
}

void Win32IDE::toggleSecondarySidebar()
{
    m_secondarySidebarVisible = !m_secondarySidebarVisible;
    ShowWindow(m_hwndSecondarySidebar, m_secondarySidebarVisible ? SW_SHOW : SW_HIDE);
    
    // Trigger resize to update layout
    RECT rect;
    GetClientRect(m_hwndMain, &rect);
    onSize(rect.right, rect.bottom);
}

void Win32IDE::updateSecondarySidebarContent()
{
    // Update chat display with history
    std::string chatText;
    for (const auto& msg : m_chatHistory) {
        if (msg.first == "user") {
            chatText += "You: " + msg.second + "\r\n\r\n";
        } else {
            chatText += "Copilot: " + msg.second + "\r\n\r\n";
        }
    }
    SetWindowTextA(m_hwndCopilotChatOutput, chatText.c_str());
    
    // Scroll to bottom
    int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
    SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
    SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::sendCopilotMessage(const std::string& message)
{
    if (message.empty()) return;
    
    // Add user message to history
    m_chatHistory.push_back({"user", message});
    
    // Generate response using the AI inference system
    std::string response;
    
    if (isModelLoaded()) {
        // Use the loaded GGUF model for inference
        response = generateResponse(message);
    } else {
        // No model loaded - prompt user to load one
        response = "⚠️ No AI model loaded.\r\n\r\n"
                   "To use AI assistance, please load a GGUF model:\r\n"
                   "1. Open the File Explorer (Activity Bar → Explorer icon)\r\n"
                   "2. Navigate to a folder containing .gguf files\r\n"
                   "3. Double-click a model file to load it\r\n\r\n"
                   "Supported models: LLaMA, Mistral, Phi, Qwen, and other GGUF-compatible models.\r\n\r\n"
                   "Once loaded, I can help with:\r\n"
                   "• Code explanation and analysis\r\n"
                   "• Bug fixing suggestions\r\n"
                   "• Code generation\r\n"
                   "• Programming questions";
    }
    
    m_chatHistory.push_back({"assistant", response});
    
    // Update display
    updateSecondarySidebarContent();
    
    // Clear input
    SetWindowTextA(m_hwndCopilotChatInput, "");
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

LRESULT CALLBACK Win32IDE::SecondarySidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (uMsg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, VSCODE_SIDEBAR_BG);
            SetTextColor(hdc, RGB(204, 204, 204));
            static HBRUSH hBrush = CreateSolidBrush(VSCODE_SIDEBAR_BG);
            return (LRESULT)hBrush;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// Panel (Bottom) - Terminal, Output, Problems, Debug Console
// ============================================================================

void Win32IDE::createPanel(HWND hwndParent)
{
    m_panelVisible = true;
    m_panelMaximized = false;
    m_panelHeight = 250;
    m_activePanelTab = PanelTab::Terminal;
    m_errorCount = 0;
    m_warningCount = 0;
    
    // Create panel container
    m_hwndPanelContainer = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 800, m_panelHeight,
        hwndParent, (HMENU)IDC_PANEL_CONTAINER, m_hInstance, nullptr);
    
    // Create tab control for panel views
    m_hwndPanelTabs = CreateWindowExA(
        0, WC_TABCONTROLA, "",
        WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER,
        0, 0, 400, 24,
        m_hwndPanelContainer, (HMENU)IDC_PANEL_TABS, m_hInstance, nullptr);
    
    // Add tabs: Terminal, Output, Problems, Debug Console
    TCITEMA tie = { TCIF_TEXT };
    tie.pszText = const_cast<char*>("TERMINAL");
    TabCtrl_InsertItem(m_hwndPanelTabs, 0, &tie);
    tie.pszText = const_cast<char*>("OUTPUT");
    TabCtrl_InsertItem(m_hwndPanelTabs, 1, &tie);
    tie.pszText = const_cast<char*>("PROBLEMS");
    TabCtrl_InsertItem(m_hwndPanelTabs, 2, &tie);
    tie.pszText = const_cast<char*>("DEBUG CONSOLE");
    TabCtrl_InsertItem(m_hwndPanelTabs, 3, &tie);
    
    // Create panel toolbar (right side)
    m_hwndPanelToolbar = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE,
        400, 0, 200, 24,
        m_hwndPanelContainer, (HMENU)IDC_PANEL_TOOLBAR, m_hInstance, nullptr);
    
    // Toolbar buttons
    m_hwndPanelNewTerminalBtn = CreateWindowExA(
        0, "BUTTON", "+",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 24, 22,
        m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_NEW_TERMINAL, m_hInstance, nullptr);
    
    m_hwndPanelSplitTerminalBtn = CreateWindowExA(
        0, "BUTTON", "||",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        26, 0, 24, 22,
        m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_SPLIT_TERMINAL, m_hInstance, nullptr);
    
    m_hwndPanelKillTerminalBtn = CreateWindowExA(
        0, "BUTTON", "X",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        52, 0, 24, 22,
        m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_KILL_TERMINAL, m_hInstance, nullptr);
    
    m_hwndPanelMaximizeBtn = CreateWindowExA(
        0, "BUTTON", "^",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        130, 0, 24, 22,
        m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_MAXIMIZE, m_hInstance, nullptr);
    
    m_hwndPanelCloseBtn = CreateWindowExA(
        0, "BUTTON", "x",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        156, 0, 24, 22,
        m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_CLOSE, m_hInstance, nullptr);
    
    // Create Problems list view
    m_hwndProblemsListView = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 26, 800, m_panelHeight - 26,
        m_hwndPanelContainer, (HMENU)IDC_PANEL_PROBLEMS_LIST, m_hInstance, nullptr);
    
    // Add columns to Problems list
    LVCOLUMNA lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    
    lvc.pszText = const_cast<char*>("Severity");
    lvc.cx = 70;
    lvc.iSubItem = 0;
    ListView_InsertColumn(m_hwndProblemsListView, 0, &lvc);
    
    lvc.pszText = const_cast<char*>("Message");
    lvc.cx = 400;
    lvc.iSubItem = 1;
    ListView_InsertColumn(m_hwndProblemsListView, 1, &lvc);
    
    lvc.pszText = const_cast<char*>("File");
    lvc.cx = 200;
    lvc.iSubItem = 2;
    ListView_InsertColumn(m_hwndProblemsListView, 2, &lvc);
    
    lvc.pszText = const_cast<char*>("Line");
    lvc.cx = 60;
    lvc.iSubItem = 3;
    ListView_InsertColumn(m_hwndProblemsListView, 3, &lvc);
    
    // Initially show terminal, hide problems list
    ShowWindow(m_hwndProblemsListView, SW_HIDE);
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
    std::string copilotText = m_statusBarInfo.copilotActive ? "Copilot" : "Copilot (off)";
    if (m_statusBarInfo.copilotSuggestions > 0) {
        copilotText += " ▼";  // Indicates suggestions available
    }
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 11, (LPARAM)copilotText.c_str());
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
