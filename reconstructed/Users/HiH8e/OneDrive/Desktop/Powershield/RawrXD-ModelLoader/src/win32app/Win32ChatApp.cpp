// ============================================================================
// RawrXD Chat Application - Desktop Taskbar Chat Interface
// ============================================================================
// Minimal, focused chat UI with file support and 256k context window

#include "Win32ChatApp.h"
#include <time.h>
#include <shlobj.h>
#include <shellapi.h>
#include <winhttp.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

// Constants
#define WM_TRAYICON (WM_USER + 1)
#define IDI_TRAYICON 5001
#define IDC_SEND_BTN 2001
#define IDC_CLEAR_BTN 2002
#define IDC_UPLOAD_BTN 2003
#define IDC_CONTEXT_STATS 2004
#define TOKENS_PER_WORD 1.3
#define MAX_CONTEXT_TOKENS 256000

// ============================================================================
// CONSTRUCTION & WINDOW MANAGEMENT
// ============================================================================

Win32ChatApp::Win32ChatApp(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hwndMain(nullptr), m_hwndAgentPanel(nullptr),
      m_hwndUserPanel(nullptr), m_hwndFilePanel(nullptr),
      m_trayIconId(IDI_TRAYICON), m_isMinimized(false), m_isVisible(true),
      m_panelSplitY(0), m_panelSplitX(0), m_filePanelVisible(true),
      m_draggingSplitter(false), m_agentPanelBrush(nullptr),
      m_userPanelBrush(nullptr), m_backgroundBrush(nullptr), m_chatFont(nullptr),
      m_agentTextColor(RGB(200, 200, 200)), m_userTextColor(RGB(50, 50, 50)),
      m_agentBgColor(RGB(30, 30, 30)), m_userBgColor(RGB(240, 240, 240)),
      m_darkMode(true), m_fontSize(11), m_windowWidth(800), m_windowHeight(600),
      m_modelEndpoint("http://localhost:11434"), m_currentModel(""),
      m_isConnected(false), m_isWaitingForResponse(false),
      m_maxContextTokens(MAX_CONTEXT_TOKENS)
{
    // Initialize context window
    m_contextWindow.maxTokens = MAX_CONTEXT_TOKENS;
    m_contextWindow.currentTokens = 0;
    m_contextWindow.maxMessages = 1000;
    m_contextWindow.oldestMessageIndex = 0;

    // Create session
    m_currentSession.sessionId = "default";
    m_currentSession.sessionName = "Default Chat";
    m_currentSession.contextWindow = m_contextWindow;

    // Create resource brushes
    m_agentPanelBrush = CreateSolidBrush(m_agentBgColor);
    m_userPanelBrush = CreateSolidBrush(m_userBgColor);
    m_backgroundBrush = CreateSolidBrush(RGB(45, 45, 45));

    // Load settings
    loadSettings();
}

Win32ChatApp::~Win32ChatApp()
{
    removeTrayIcon();
    saveSettings();
    saveChatHistory();

    if (m_agentPanelBrush) DeleteObject(m_agentPanelBrush);
    if (m_userPanelBrush) DeleteObject(m_userPanelBrush);
    if (m_backgroundBrush) DeleteObject(m_backgroundBrush);
    if (m_chatFont) DeleteObject(m_chatFont);
}

bool Win32ChatApp::createWindow()
{
    // Register window class
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = "RawrXD_ChatApp_Class";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_backgroundBrush;

    if (!RegisterClassA(&wc)) {
        return false;
    }

    // Create main window
    m_hwndMain = CreateWindowA("RawrXD_ChatApp_Class", "RawrXD Chat",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, m_windowWidth, m_windowHeight,
        nullptr, nullptr, m_hInstance, this);

    if (!m_hwndMain) {
        return false;
    }

    // Register rich edit control
    LoadLibraryA("riched20.dll");

    // Create UI elements
    createChatUI();
    createTrayIcon();

    return true;
}

void Win32ChatApp::showWindow()
{
    if (!m_hwndMain) return;
    ShowWindow(m_hwndMain, SW_SHOW);
    SetForegroundWindow(m_hwndMain);
    m_isVisible = true;
    m_isMinimized = false;
}

void Win32ChatApp::hideWindow()
{
    if (!m_hwndMain) return;
    ShowWindow(m_hwndMain, SW_HIDE);
    m_isVisible = false;
    m_isMinimized = true;
}

void Win32ChatApp::toggleVisibility()
{
    if (m_isVisible) {
        hideWindow();
    } else {
        showWindow();
    }
}

int Win32ChatApp::runMessageLoop()
{
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// ============================================================================
// TRAY ICON INTEGRATION
// ============================================================================

void Win32ChatApp::createTrayIcon()
{
    if (!m_hwndMain) return;

    ZeroMemory(&m_nid, sizeof(NOTIFYICONDATA));
    m_nid.cbSize = sizeof(NOTIFYICONDATA);
    m_nid.hWnd = m_hwndMain;
    m_nid.uID = m_trayIconId;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON;
    m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    strcpy_s(m_nid.szTip, sizeof(m_nid.szTip), "RawrXD Chat");

    Shell_NotifyIconA(NIM_ADD, &m_nid);
}

void Win32ChatApp::removeTrayIcon()
{
    if (m_hwndMain) {
        Shell_NotifyIconA(NIM_DELETE, &m_nid);
    }
}

void Win32ChatApp::showContextMenu(int x, int y)
{
    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, 1, "Show");
    AppendMenuA(hMenu, MF_STRING, 2, "New Session");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, 3, "Settings");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING, 4, "Exit");

    int choice = TrackPopupMenu(hMenu, TPM_RETURNCMD, x, y, 0, m_hwndMain, nullptr);

    switch (choice) {
        case 1:
            showWindow();
            break;
        case 2:
            createNewSession();
            break;
        case 3:
            MessageBoxA(m_hwndMain, "Settings coming soon", "RawrXD Chat", MB_OK);
            break;
        case 4:
            PostQuitMessage(0);
            break;
    }

    DestroyMenu(hMenu);
}

// ============================================================================
// CHAT UI CREATION
// ============================================================================

void Win32ChatApp::createChatUI()
{
    if (!m_hwndMain) return;

    // Create agent panel (top) - shows model responses
    m_hwndAgentPanel = CreateWindowExA(
        WS_EX_CLIENTEDGE, "RICHEDIT50W", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 800, 200, m_hwndMain, (HMENU)1001, m_hInstance, nullptr
    );

    // Create user panel (bottom) - for user input
    m_hwndUserPanel = CreateWindowExA(
        WS_EX_CLIENTEDGE, "RICHEDIT50W", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 220, 800, 250, m_hwndMain, (HMENU)1002, m_hInstance, nullptr
    );

    // Create file panel (right side)
    m_hwndFilePanel = CreateWindowExA(
        WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | WS_VSCROLL,
        0, 480, 800, 70, m_hwndMain, (HMENU)1003, m_hInstance, nullptr
    );

    // Create buttons
    m_hwndUploadButton = CreateWindowA(
        "BUTTON", "📎 Upload File",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 560, 100, 25, m_hwndMain, (HMENU)IDC_UPLOAD_BTN, m_hInstance, nullptr
    );

    m_hwndSendButton = CreateWindowA(
        "BUTTON", "📤 Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        120, 560, 80, 25, m_hwndMain, (HMENU)IDC_SEND_BTN, m_hInstance, nullptr
    );

    m_hwndClearButton = CreateWindowA(
        "BUTTON", "🗑 Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        210, 560, 80, 25, m_hwndMain, (HMENU)IDC_CLEAR_BTN, m_hInstance, nullptr
    );

    // Context stats display
    m_hwndContextStats = CreateWindowA(
        "STATIC", "Tokens: 0 / 256000",
        WS_CHILD | WS_VISIBLE,
        300, 565, 250, 20, m_hwndMain, (HMENU)IDC_CONTEXT_STATS, m_hInstance, nullptr
    );

    // Set font for panels
    HFONT hFont = CreateFontA(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Courier New");
    
    SendMessage(m_hwndAgentPanel, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_hwndUserPanel, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Set colors
    SendMessage(m_hwndAgentPanel, EM_SETBKGNDCOLOR, 0, m_agentBgColor);
    SendMessage(m_hwndUserPanel, EM_SETBKGNDCOLOR, 0, m_userBgColor);
}

void Win32ChatApp::layoutChatPanels()
{
    if (!m_hwndMain) return;

    RECT rect;
    GetClientRect(m_hwndMain, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // Agent panel: top 30%
    if (m_hwndAgentPanel) {
        MoveWindow(m_hwndAgentPanel, 0, 0, width, height / 3, TRUE);
    }

    // User panel: middle 40%
    if (m_hwndUserPanel) {
        MoveWindow(m_hwndUserPanel, 0, height / 3 + 5, width, height * 2 / 5, TRUE);
    }

    // File panel: bottom 20%
    if (m_hwndFilePanel) {
        MoveWindow(m_hwndFilePanel, 0, height / 3 + height * 2 / 5 + 10, width, height / 5, TRUE);
    }

    // Buttons at bottom
    int buttonY = height - 40;
    if (m_hwndUploadButton) MoveWindow(m_hwndUploadButton, 10, buttonY, 100, 25, TRUE);
    if (m_hwndSendButton) MoveWindow(m_hwndSendButton, 120, buttonY, 80, 25, TRUE);
    if (m_hwndClearButton) MoveWindow(m_hwndClearButton, 210, buttonY, 80, 25, TRUE);
    if (m_hwndContextStats) MoveWindow(m_hwndContextStats, 300, buttonY + 3, 250, 20, TRUE);
}

// ============================================================================
// CHAT MESSAGE HANDLING
// ============================================================================

void Win32ChatApp::appendAgentMessage(const std::string& message)
{
    if (!m_hwndAgentPanel) return;

    std::string formatted = "[Agent] " + formatTimestamp() + "\n" + message + "\n\n";
    
    // Get current text length
    int textLen = GetWindowTextLengthA(m_hwndAgentPanel);
    
    // Append text
    SendMessage(m_hwndAgentPanel, EM_SETSEL, textLen, textLen);
    SendMessageA(m_hwndAgentPanel, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());

    // Scroll to bottom
    SendMessage(m_hwndAgentPanel, WM_VSCROLL, SB_BOTTOM, 0);

    // Track for context
    ChatMessage msg;
    msg.sender = "Agent";
    msg.content = message;
    msg.timestamp = formatTimestamp();
    msg.tokenCount = estimateTokenCount(message);

    m_currentSession.messageHistory.push_back(msg);
    m_contextWindow.messages.push_back(msg);
    m_contextWindow.currentTokens += msg.tokenCount;

    updateContextWindow();
}

void Win32ChatApp::appendUserMessage(const std::string& message)
{
    if (!m_hwndUserPanel) return;

    std::string formatted = "[You] " + formatTimestamp() + "\n" + message + "\n\n";
    
    int textLen = GetWindowTextLengthA(m_hwndUserPanel);
    SendMessage(m_hwndUserPanel, EM_SETSEL, textLen, textLen);
    SendMessageA(m_hwndUserPanel, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());

    SendMessage(m_hwndUserPanel, WM_VSCROLL, SB_BOTTOM, 0);

    // Track for context
    ChatMessage msg;
    msg.sender = "User";
    msg.content = message;
    msg.timestamp = formatTimestamp();
    msg.tokenCount = estimateTokenCount(message);

    m_currentSession.messageHistory.push_back(msg);
    m_contextWindow.messages.push_back(msg);
    m_contextWindow.currentTokens += msg.tokenCount;

    updateContextWindow();
}

void Win32ChatApp::clearChat()
{
    if (m_hwndAgentPanel) SetWindowTextA(m_hwndAgentPanel, "");
    if (m_hwndUserPanel) SetWindowTextA(m_hwndUserPanel, "");
    if (m_hwndFilePanel) SendMessage(m_hwndFilePanel, LB_RESETCONTENT, 0, 0);
    
    m_currentSession.messageHistory.clear();
    m_contextWindow.messages.clear();
    m_contextWindow.currentTokens = 0;
}

// ============================================================================
// FILE HANDLING
// ============================================================================

void Win32ChatApp::uploadFile(const std::string& filePath)
{
    // Get file info
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;
    if (!GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fileInfo)) {
        MessageBoxA(m_hwndMain, "File not found", "Error", MB_OK | MB_ICONERROR);
        return;
    }

    FileUpload upload;
    upload.filePath = filePath;
    
    // Extract filename
    size_t pos = filePath.find_last_of("\\/");
    upload.fileName = (pos == std::string::npos) ? filePath : filePath.substr(pos + 1);
    
    // Get file size
    LARGE_INTEGER size;
    size.LowPart = fileInfo.nFileSizeLow;
    size.HighPart = fileInfo.nFileSizeHigh;
    upload.fileSize = size.QuadPart;

    // Get file type
    pos = upload.fileName.find_last_of(".");
    upload.fileType = (pos == std::string::npos) ? "unknown" : upload.fileName.substr(pos);

    m_uploadedFiles.push_back(upload);

    // Add to listbox
    std::string displayText = upload.fileName + " (" + formatFileSize(upload.fileSize) + ")";
    if (m_hwndFilePanel) {
        SendMessageA(m_hwndFilePanel, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
    }
}

void Win32ChatApp::onFileDropped(const std::vector<std::string>& files)
{
    for (const auto& file : files) {
        uploadFile(file);
    }
}

std::string Win32ChatApp::formatFileSize(size_t bytes)
{
    const char* sizes[] = {"B", "KB", "MB", "GB"};
    double len = static_cast<double>(bytes);
    int order = 0;
    
    while (len >= 1024 && order < 3) {
        order++;
        len /= 1024.0;
    }

    char buf[32];
    sprintf_s(buf, "%.1f %s", len, sizes[order]);
    return std::string(buf);
}

// ============================================================================
// CONTEXT WINDOW MANAGEMENT
// ============================================================================

void Win32ChatApp::updateContextWindow()
{
    // Prune old messages if exceeding max tokens
    while (m_contextWindow.currentTokens > MAX_CONTEXT_TOKENS && !m_contextWindow.messages.empty()) {
        auto& oldMsg = m_contextWindow.messages.front();
        m_contextWindow.currentTokens -= oldMsg.tokenCount;
        m_contextWindow.messages.pop_front();
        m_contextWindow.oldestMessageIndex++;
    }

    // Update display
    if (m_hwndContextStats) {
        char buf[128];
        sprintf_s(buf, "Tokens: %zu / %zu | Messages: %zu", 
            m_contextWindow.currentTokens, MAX_CONTEXT_TOKENS, m_contextWindow.messages.size());
        SetWindowTextA(m_hwndContextStats, buf);
    }
}

void Win32ChatApp::pruneOldMessages()
{
    updateContextWindow();
}

size_t Win32ChatApp::estimateTokenCount(const std::string& text)
{
    // Rough estimate: 1 token ≈ 1.3 words
    // 1 word ≈ 4-5 characters
    size_t words = text.length() / 5;
    return static_cast<size_t>(words * TOKENS_PER_WORD) + 1;
}

void Win32ChatApp::displayContextStats()
{
    std::string stats = getContextStats();
    MessageBoxA(m_hwndMain, stats.c_str(), "Context Statistics", MB_OK);
}

std::string Win32ChatApp::getContextStats() const
{
    std::ostringstream oss;
    oss << "Context Window Statistics\n";
    oss << "==========================\n\n";
    oss << "Max Tokens: " << m_maxContextTokens << "\n";
    oss << "Current Tokens: " << m_contextWindow.currentTokens << "\n";
    oss << "Tokens Used: " << (100.0 * m_contextWindow.currentTokens / m_maxContextTokens) << "%\n";
    oss << "Messages: " << m_contextWindow.messages.size() << "\n";
    oss << "Oldest Index: " << m_contextWindow.oldestMessageIndex << "\n";
    return oss.str();
}

// ============================================================================
// MODEL CONNECTION
// ============================================================================

void Win32ChatApp::sendPromptToModel(const std::string& prompt)
{
    if (m_isWaitingForResponse) {
        MessageBoxA(m_hwndMain, "Already waiting for model response", "Info", MB_OK);
        return;
    }

    appendUserMessage(prompt);
    m_isWaitingForResponse = true;

    // TODO: Connect to model endpoint via HTTP
    // For now, simulate response
    appendAgentMessage("(Model response would appear here)");
    m_isWaitingForResponse = false;
}

void Win32ChatApp::receiveModelResponse(const std::string& response)
{
    appendAgentMessage(response);
    m_isWaitingForResponse = false;
}

// ============================================================================
// SETTINGS & PERSISTENCE
// ============================================================================

void Win32ChatApp::loadSettings()
{
    // Get AppData path
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appDataPath))) {
        m_appDataPath = std::string(appDataPath) + "\\RawrXD";
        CreateDirectoryA(m_appDataPath.c_str(), nullptr);
        
        m_settingsPath = m_appDataPath + "\\chat_settings.ini";
        m_historyPath = m_appDataPath + "\\chat_history.json";
    }

    // Load window size
    std::ifstream settings(m_settingsPath);
    if (settings.is_open()) {
        settings >> m_windowWidth >> m_windowHeight >> m_darkMode;
        settings.close();
    }
}

void Win32ChatApp::saveSettings()
{
    if (m_hwndMain) {
        RECT rect;
        GetWindowRect(m_hwndMain, &rect);
        m_windowWidth = rect.right - rect.left;
        m_windowHeight = rect.bottom - rect.top;
    }

    std::ofstream settings(m_settingsPath);
    if (settings.is_open()) {
        settings << m_windowWidth << " " << m_windowHeight << " " << (m_darkMode ? 1 : 0);
        settings.close();
    }
}

void Win32ChatApp::saveChatHistory()
{
    // TODO: Save to JSON file
}

void Win32ChatApp::loadChatHistory(const std::string& sessionId)
{
    // TODO: Load from JSON file
}

void Win32ChatApp::createNewSession()
{
    m_currentSession.sessionId = "session_" + std::to_string(time(nullptr));
    m_currentSession.sessionName = "Chat " + m_currentSession.sessionId;
    m_currentSession.messageHistory.clear();
    m_contextWindow.messages.clear();
    m_contextWindow.currentTokens = 0;
    clearChat();
}

// ============================================================================
// HELPERS
// ============================================================================

std::string Win32ChatApp::formatTimestamp()
{
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
    return std::string(buf);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK Win32ChatApp::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32ChatApp* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32ChatApp*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (Win32ChatApp*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Win32ChatApp::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
        onCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;

    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONUP) {
            toggleVisibility();
        } else if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            showContextMenu(pt.x, pt.y);
        }
        return 0;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void Win32ChatApp::onCreate(HWND hwnd)
{
    // Placeholder
}

void Win32ChatApp::onDestroy()
{
    // Cleanup
}

void Win32ChatApp::onSize(int width, int height)
{
    layoutChatPanels();
}

void Win32ChatApp::onCommand(int id, int notifyCode)
{
    switch (id) {
    case IDC_SEND_BTN:
    {
        char userInput[4096];
        GetWindowTextA(m_hwndUserPanel, userInput, sizeof(userInput));
        if (strlen(userInput) > 0) {
            sendPromptToModel(userInput);
            SetWindowTextA(m_hwndUserPanel, "");
        }
        break;
    }
    case IDC_CLEAR_BTN:
        clearChat();
        break;

    case IDC_UPLOAD_BTN:
    {
        OPENFILENAMEA ofn = {};
        char szFile[260] = "";
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = m_hwndMain;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn)) {
            uploadFile(szFile);
        }
        break;
    }
    }
}
