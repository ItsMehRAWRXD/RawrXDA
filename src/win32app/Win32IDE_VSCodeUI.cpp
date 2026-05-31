// Win32IDE_VSCodeUI.cpp - VS Code-like UI Components Implementation
// Activity Bar, Secondary Sidebar, Panel (Terminal/Output/Problems/Debug Console), Enhanced Status Bar

#include "../ui/tool_action_status.h"
#include "IDEConfig.h"
#include "Win32IDE.h"
#include <algorithm>
#include <commctrl.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <richedit.h>
#include <sstream>

extern "C" void RawrXD_ApplyCopilotChatEditLimits(HWND output, HWND input);

namespace
{
std::wstring utf8ToWideForCopilotInput(const std::string& utf8)
{
    if (utf8.empty())
        return L"";
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0)
        return L"";
    std::wstring wide(static_cast<size_t>(needed), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed) <= 0)
        return L"";
    return wide;
}

void wipeStringMemory(std::string& value)
{
    if (!value.empty())
    {
        SecureZeroMemory(value.data(), value.size());
        value.clear();
        value.shrink_to_fit();
    }
}

HWND g_titanDiagWindow = nullptr;

void TitanDiagnosticForwarder(const char* text)
{
    HWND hwnd = g_titanDiagWindow;
    if (!hwnd || !text || !*text)
        return;

    WPARAM type = 0;
    if (strstr(text, "error") || strstr(text, "ERROR") || strstr(text, "fail") || strstr(text, "FAIL"))
    {
        type = 2;
    }
    else if (strstr(text, "warn") || strstr(text, "WARN"))
    {
        type = 1;
    }

    char* heapCopy = _strdup(text);
    if (!heapCopy)
        return;

    if (!PostMessageA(hwnd, WM_RAWR_LOG_MESSAGE, type, reinterpret_cast<LPARAM>(heapCopy)))
    {
        free(heapCopy);
    }
}

std::string sanitizeChatText(const std::string& input)
{
    if (input.empty())
        return input;

    std::string out;
    out.reserve(input.size());

    for (unsigned char ch : input)
    {
        if (ch == '\r' || ch == '\n' || ch == '\t')
        {
            out.push_back(static_cast<char>(ch));
            continue;
        }

        if (ch >= 32 && ch <= 126)
            out.push_back(static_cast<char>(ch));
        else
            out.push_back(' ');
    }

    // Collapse repeated spaces for readability in the chat pane.
    std::string compact;
    compact.reserve(out.size());
    bool lastSpace = false;
    for (char c : out)
    {
        const bool isSpace = (c == ' ');
        if (isSpace && lastSpace)
            continue;
        compact.push_back(c);
        lastSpace = isSpace;
    }

    // Detect '?' flood: detokenization failure causes the model to emit ASCII '?'
    // (0x3F) for every unknown token.  Since '?' is printable it passes the
    // range filter above, so we catch it here before it poisons the chat pane.
    if (compact.size() > 20)
    {
        int qCount = 0;
        for (char c : compact)
            if (c == '?')
                ++qCount;
        if (qCount > static_cast<int>(compact.size()) * 2 / 5)
            return "[Model output error: detokenization failure (" + std::to_string(qCount) +
                   " replacement tokens — check model compatibility)]";
    }

    return compact;
}
}  // namespace

// VSU (Visual Studio UI) Effect Rendering with Adobe RGBa Support
// Includes Acrylic, Mica, and elevation shadow effects for modern IDE appearance
#include "../include/RawrXD_ColorSpace.h"
#include "../../RawrXD_Renderer_D2D.h"


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

// VSU Effect Colors - Adobe RGBa for professional color accuracy
// Using RawrXD::ColorSpace::VSU namespace for modern IDE appearance
using namespace RawrXD::ColorSpace;

// Activity Bar colors with proper Adobe RGBa
static const AdobeRGBa VSCODE_ACTIVITY_BAR_BG = VSU::Acrylic::DarkBase;                    // Acrylic dark base
static const AdobeRGBa VSCODE_ACTIVITY_BAR_ACTIVE = VSU::Mica::DarkTint;                   // Mica dark tint
static const AdobeRGBa VSCODE_ACTIVITY_BAR_HOVER = AdobeRGBa(0.35f, 0.36f, 0.37f, 0.95f);  // Hover with alpha
static const AdobeRGBa VSCODE_ACTIVITY_BAR_ICON = AdobeRGBa(0.80f, 0.80f, 0.80f, 1.00f);   // Icon color
static const AdobeRGBa VSCODE_ACTIVITY_BAR_INDICATOR = VSU::Accents::Blue;                // Active indicator

// Sidebar and Panel with VSU Acrylic effects
static const AdobeRGBa VSCODE_SIDEBAR_BG = VSU::Acrylic::SidebarTint;
static const AdobeRGBa VSCODE_PANEL_BG = VSU::Acrylic::PanelTint;
static const AdobeRGBa VSCODE_STATUS_BAR_BG = VSU::Accents::Blue;
static const AdobeRGBa VSCODE_STATUS_BAR_DEBUG = VSU::Accents::Warning;
static const AdobeRGBa VSCODE_STATUS_BAR_REMOTE = VSU::Accents::Success;
static const AdobeRGBa VSCODE_STATUS_BAR_TEXT = AdobeRGBa(1.00f, 1.00f, 1.00f, 1.00f);

// Elevation shadow colors for depth perception
static const AdobeRGBa SHADOW_ELEVATION_01 = VSU::Shadows::Shadow01;
static const AdobeRGBa SHADOW_ELEVATION_04 = VSU::Shadows::Shadow04;
static const AdobeRGBa SHADOW_ELEVATION_08 = VSU::Shadows::Shadow08;

// Helper to convert AdobeRGBa to COLORREF for legacy Win32 controls
inline COLORREF AdobeRGBaToCOLORREF(const AdobeRGBa& color) {
    auto srgb = color.TosRGB();
    return RGB(static_cast<int>(srgb.r * 255), 
               static_cast<int>(srgb.g * 255), 
               static_cast<int>(srgb.b * 255));
}

// Unicode icons for Activity Bar (simple ASCII fallbacks)
static const char* ICON_EXPLORER = "[]";    // File explorer
static const char* ICON_SEARCH = "()";      // Search
static const char* ICON_SCM = "<>";         // Source control
static const char* ICON_DEBUG = ">";        // Run/Debug
static const char* ICON_EXTENSIONS = "++";  // Extensions
static const char* ICON_SETTINGS = "*";     // Settings gear
static const char* ICON_ACCOUNTS = "@";     // User account

// ============================================================================
// Activity Bar (Far Left) - VS Code style vertical icon bar
// ============================================================================

void Win32IDE::createActivityBarUI(HWND hwndParent)
{
    // Create brushes for Activity Bar colors using Adobe RGBa
    m_actBarBackgroundBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSCODE_ACTIVITY_BAR_BG));
    m_actBarHoverBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSCODE_ACTIVITY_BAR_HOVER));
    m_actBarActiveBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSCODE_ACTIVITY_BAR_ACTIVE));

    // Create the Activity Bar container
    m_hwndActivityBar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW, 0, 0, ACTIVITY_BAR_WIDTH,
                                        600, hwndParent, (HMENU)IDC_ACTIVITY_BAR, m_hInstance, nullptr);

    // Set the background color using Adobe RGBa
    SetClassLongPtr(m_hwndActivityBar, GCLP_HBRBACKGROUND, (LONG_PTR)m_actBarBackgroundBrush);

    // Create Activity Bar buttons (icons)
    const char* buttonLabels[] = {ICON_EXPLORER,   ICON_SEARCH,   ICON_SCM,     ICON_DEBUG,
                                  ICON_EXTENSIONS, ICON_SETTINGS, ICON_ACCOUNTS};
    const char* tooltips[] = {"Explorer (Ctrl+Shift+E)",
                              "Search (Ctrl+Shift+F)",
                              "Source Control (Ctrl+Shift+G)",
                              "Run and Debug (Ctrl+Shift+D)",
                              "Extensions (Ctrl+Shift+X)",
                              "Settings",
                              "Accounts"};
    int buttonHeight = 48;

    for (int i = 0; i < 7; i++)
    {
        int yPos = (i < 5) ? (i * buttonHeight) : (600 - (7 - i) * buttonHeight);  // Top 5 + bottom 2

        m_activityBarButtons[i] = CreateWindowExA(0, "BUTTON", buttonLabels[i], WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0,
                                                  yPos, ACTIVITY_BAR_WIDTH, buttonHeight, m_hwndActivityBar,
                                                  (HMENU)(IDC_ACTBAR_EXPLORER + i), m_hInstance, nullptr);

        // Store IDE pointer for button subclass
        SetWindowLongPtr(m_activityBarButtons[i], GWLP_USERDATA, (LONG_PTR)this);

        // Create tooltip
        HWND hwndTip =
            CreateWindowEx(0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, hwndParent, nullptr, m_hInstance, nullptr);

        TOOLINFOA ti = {sizeof(TOOLINFOA)};
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = hwndParent;
        ti.uId = (UINT_PTR)m_activityBarButtons[i];
        ti.lpszText = const_cast<char*>(tooltips[i]);
        SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
    }

    m_activeActivityBarButton = 0;  // Explorer is active by default
    m_sidebarVisible = true;
    m_sidebarWidth = 260;
}

void Win32IDE::updateActivityBarState()
{
    // Repaint all activity bar buttons to reflect current state
    for (int i = 0; i < 7; i++)
    {
        if (m_activityBarButtons[i])
        {
            InvalidateRect(m_activityBarButtons[i], nullptr, TRUE);
        }
    }
}

LRESULT CALLBACK Win32IDE::ActivityBarButtonProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            int buttonIndex = dis->CtlID - IDC_ACTBAR_EXPLORER;

            // Draw background with VSU Acrylic effect
            AdobeRGBa bgColor = (buttonIndex == pThis->m_activeActivityBarButton) 
                ? VSCODE_ACTIVITY_BAR_ACTIVE 
                : VSCODE_ACTIVITY_BAR_BG;

            if (dis->itemState & ODS_SELECTED)
            {
                bgColor = VSCODE_ACTIVITY_BAR_HOVER;
            }

            // Apply elevation shadow for depth perception
            RECT shadowRect = dis->rcItem;
            shadowRect.left += 2;
            shadowRect.top += 2;
            HBRUSH hShadowBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(SHADOW_ELEVATION_01));
            FillRect(dis->hDC, &shadowRect, hShadowBrush);
            DeleteObject(hShadowBrush);

            // Draw main background
            HBRUSH hBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(bgColor));
            FillRect(dis->hDC, &dis->rcItem, hBrush);
            DeleteObject(hBrush);

            // Draw active indicator (left border) with accent color
            if (buttonIndex == pThis->m_activeActivityBarButton)
            {
                RECT indicatorRect = dis->rcItem;
                indicatorRect.right = 3;
                HBRUSH hIndicator = CreateSolidBrush(AdobeRGBaToCOLORREF(VSCODE_ACTIVITY_BAR_INDICATOR));
                FillRect(dis->hDC, &indicatorRect, hIndicator);
                DeleteObject(hIndicator);
            }

            // Draw icon text centered with proper Adobe RGBa color
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, AdobeRGBaToCOLORREF(VSCODE_ACTIVITY_BAR_ICON));

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
    m_hwndSecondarySidebar =
        CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, m_secondarySidebarWidth, 600,
                        hwndParent, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);

    if (m_hwndSecondarySidebar)
    {
        SetWindowLongPtrA(m_hwndSecondarySidebar, GWLP_USERDATA, (LONG_PTR)this);
        m_oldSidebarProc = (WNDPROC)SetWindowLongPtrA(m_hwndSecondarySidebar, GWLP_WNDPROC, (LONG_PTR)SidebarProcImpl);
    }

    // Header label
    m_hwndSecondarySidebarHeader = CreateWindowExA(
        0, "STATIC", " GitHub Copilot Chat", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE, 0, 0,
        m_secondarySidebarWidth, 28, m_hwndSecondarySidebar, (HMENU)IDC_SECONDARY_SIDEBAR_HEADER, m_hInstance, nullptr);

    // Chat output area (read-only rich edit for formatted messages)
    m_hwndCopilotChatOutput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        5, 32, m_secondarySidebarWidth - 10, 450, m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_OUTPUT, m_hInstance,
        nullptr);

    // Chat input area
    m_hwndCopilotChatInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL, 5, 490,
        m_secondarySidebarWidth - 10, 60, m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_INPUT, m_hInstance, nullptr);

    if (m_hwndCopilotChatOutput)
    {
        m_oldCopilotOutputProc =
            (WNDPROC)SetWindowLongPtrA(m_hwndCopilotChatOutput, GWLP_WNDPROC, (LONG_PTR)CopilotChatOutputProc);
        SetWindowLongPtrA(m_hwndCopilotChatOutput, GWLP_USERDATA, (LONG_PTR)this);
    }

    if (m_hwndCopilotChatInput)
    {
        m_oldCopilotInputProc =
            (WNDPROC)SetWindowLongPtrA(m_hwndCopilotChatInput, GWLP_WNDPROC, (LONG_PTR)CopilotChatInputProc);
        SetWindowLongPtrA(m_hwndCopilotChatInput, GWLP_USERDATA, (LONG_PTR)this);
    }

    // Send button
    m_hwndCopilotSendBtn = CreateWindowExA(0, "BUTTON", "Send", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 555, 80, 28,
                                           m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);

    if (m_hwndCopilotSendBtn)
    {
        m_oldCopilotSendBtnProc =
            (WNDPROC)SetWindowLongPtrA(m_hwndCopilotSendBtn, GWLP_WNDPROC, (LONG_PTR)CopilotButtonProc);
        SetWindowLongPtrA(m_hwndCopilotSendBtn, GWLP_USERDATA, (LONG_PTR)this);
    }

    // Clear button
    m_hwndCopilotClearBtn =
        CreateWindowExA(0, "BUTTON", "Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 90, 555, 80, 28,
                        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);

    if (m_hwndCopilotClearBtn)
    {
        m_oldCopilotClearBtnProc =
            (WNDPROC)SetWindowLongPtrA(m_hwndCopilotClearBtn, GWLP_WNDPROC, (LONG_PTR)CopilotButtonProc);
        SetWindowLongPtrA(m_hwndCopilotClearBtn, GWLP_USERDATA, (LONG_PTR)this);
    }

    // Initialize Ollama bridge for streaming completions
    if (!m_ollamaBridge)
    {
        m_ollamaBridge = std::make_unique<RawrXD::OllamaBridge>();
        if (m_ollamaBridge->Initialize())
        {
            m_ollamaBridge->SetContextWindow(4096);  // Optimize for IDE chat
            m_ollamaBackendEnabled = true;
            RAWRXD_LOG_INFO("Win32IDE_VSCodeUI") << "✅ Ollama bridge initialized at 127.0.0.1:11435";
        }
        else
        {
            m_ollamaBackendEnabled = false;
            RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI") << "⚠️ Ollama bridge init failed";
        }
    }

    RawrXD_ApplyCopilotChatEditLimits(m_hwndCopilotChatOutput, m_hwndCopilotChatInput);
    reloadPersistedChatHistoryIntoUi();
    tryConfigureNativeStreamBridge();

    // If the full chat panel added mode toggles elsewhere, this path has none — safe no-op when null.
    syncAgentModeUiFromBridge();
}

// Implemented in src/win32app/Win32IDE_Sidebar.cpp (avoid duplicate definition / LNK2005).

void Win32IDE::updateSecondarySidebarContent()
{
    // Update chat display with history + tool action status + working bubbles
    std::string chatText;
    if (m_chatHistory.empty())
    {
        chatText =
            "GitHub Copilot Chat\r\n"
            "==================\r\n\r\n"
            "No messages yet for this workspace. History is saved per project folder "
            "(%APPDATA%\\RawrXD\\workspaces\\...) and reloads when you reopen the IDE.\r\n\r\n"
            "Open a folder (File > Open Folder, Ctrl+Shift+O) so agentic tools use the same tree as File Explorer. "
            "Load a GGUF model — Agent mode is on by default (uncheck \"Agentic Mode\" in the sidebar "
            "or use /agent only if you need an explicit prefix).\r\n\r\n"
            "Ask anything about your code!\r\n";
        if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
        {
            std::lock_guard<std::mutex> outLock(m_outputMutex);
            SetWindowTextA(m_hwndCopilotChatOutput, chatText.c_str());
        }
        return;
    }

    for (size_t i = 0; i < m_chatHistory.size(); ++i)
    {
        const auto& msg = m_chatHistory[i];
        if (msg.first == "user")
        {
            chatText += "You: " + msg.second + "\r\n\r\n";
        }
        else if (msg.first == "system")
        {
            chatText += "System: " + sanitizeChatText(msg.second) + "\r\n\r\n";
        }
        else if (msg.first == "tool")
        {
            std::string shown = msg.second;
            if (msg.second.size() > 7 && msg.second.rfind("[tool:", 0) == 0)
            {
                const size_t close = msg.second.find(']');
                if (close != std::string::npos && close > 6)
                {
                    const std::string tname = msg.second.substr(6, close - 6);
                    std::string body;
                    if (close + 1 < msg.second.size() && msg.second[close + 1] == '\n')
                        body = msg.second.substr(close + 2);
                    else if (close + 1 < msg.second.size())
                        body = msg.second.substr(close + 1);
                    shown = std::string("[") + tname + "]\r\n" + body;
                }
            }
            chatText += "Tool: " + sanitizeChatText(shown) + "\r\n\r\n";
        }
        else
        {
            // Render tool action status block before assistant message
            auto it = m_chatToolActions.find(i);
            if (it != m_chatToolActions.end() && !it->second.empty())
            {
                chatText += RawrXD::UI::ToolActionStatusFormatter::formatPlainTextBlock(
                    it->second, static_cast<int>(it->second.size()));
                chatText += "\r\n";
            }

            // Render working bubbles (plain text) if accumulator has any
            if (m_currentToolActions.workingBubbles().size() > 0)
            {
                chatText += m_currentToolActions.renderBubblesPlainText();
                chatText += "\r\n";
            }

            chatText += "Copilot: " + sanitizeChatText(msg.second) + "\r\n\r\n";
        }
    }
    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
    {
        std::lock_guard<std::mutex> outLock(m_outputMutex);
        SetWindowTextA(m_hwndCopilotChatOutput, chatText.c_str());

        // Scroll to bottom
        int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
    }
}

void Win32IDE::sendCopilotMessage(const std::string& message)
{
    if (message.empty() || !m_hwndCopilotChatInput || !IsWindow(m_hwndCopilotChatInput))
        return;

    // Single path with HandleCopilotSend: project-scoped disk history, streaming, agentic routing, and
    // ConversationSession stay aligned (Cursor/Copilot-style parity). Avoid duplicating Ollama/GGUF logic here.
    RAWRXD_LOG_INFO("Win32IDE_VSCodeUI") << "[sendCopilotMessage] delegating to deferred HandleCopilotSend";
    SetWindowTextW(m_hwndCopilotChatInput, utf8ToWideForCopilotInput(message).c_str());
    postDeferredCopilotSend();
}

void Win32IDE::clearCopilotChat()
{
    m_chatHistory.clear();
    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
    {
        std::lock_guard<std::mutex> outLock(m_outputMutex);
        SetWindowTextA(m_hwndCopilotChatOutput, "GitHub Copilot Chat\r\n"
                                                "==================\r\n\r\n"
                                                "Chat cleared. Load a model, enable Agent if you need tools, "
                                                "then ask about your code!\r\n");
    }
}

namespace
{
std::string formatMinimalAgenticLineForUi(const rawrxd::MinimalAgenticResponse& response)
{
    if (response.tool_calls_made <= 0)
        return response.final_message;
    return "[Agent executed " + std::to_string(response.tool_calls_made) + " tools]\n\n" + response.final_message;
}
}  // namespace

void Win32IDE::finishCopilotAgenticSendState()
{
    m_chatSendInFlight.store(false);
    setCopilotInteractionBusyOnUiThread(false);
    m_streamingTokenAccumulator.clear();
}

void Win32IDE::applyMinimalAgenticCompletion(rawrxd::MinimalAgenticResponse r)
{
    if (!r.transcript_delta.empty())
    {
        size_t start = 0;
        if (!r.transcript_delta.empty() && r.transcript_delta[0].role == "user")
        {
            start = 1;
        }
        for (size_t i = start; i < r.transcript_delta.size(); ++i)
        {
            const rawrxd::ChatMessage& m = r.transcript_delta[i];
            if (m.role == "assistant")
            {
                const std::string safe = sanitizeChatText(m.content);
                conversationAddAssistant(safe);
                m_chatHistory.push_back({"assistant", safe});
                persistChatTurnToDisk("assistant", safe);
            }
            else if (m.role == "tool")
            {
                std::string tname;
                std::string body;
                const size_t sep = m.content.find(": ");
                if (sep != std::string::npos)
                {
                    tname = m.content.substr(0, sep);
                    body = m.content.substr(sep + 2);
                }
                else
                {
                    tname = "tool";
                    body = m.content.empty() ? std::string("(no output)") : m.content;
                }
                recordToolTurnInChatHistory(tname, body, "result");
            }
            else if (m.role == "user")
            {
                conversationAddUser(m.content);
                m_chatHistory.push_back({"user", m.content});
                persistChatTurnToDisk("user", m.content);
            }
            else if (m.role == "system")
            {
                const std::string safe = sanitizeChatText(m.content);
                m_chatHistory.push_back({"system", safe});
                persistChatTurnToDisk("system", safe);
            }
        }
        rehydrateConversationSessionFromChatHistory();
        updateSecondarySidebarContent();
        updateEnhancedStatusBar();
        return;
    }

    for (const auto& step : r.tool_steps)
    {
        if (!step.arguments_json.empty())
        {
            recordToolTurnInChatHistory(step.tool_name, step.arguments_json, "call");
        }
        recordToolTurnInChatHistory(step.tool_name,
                                    step.result_text.empty() ? std::string("(no output)") : step.result_text, "result");
    }
    if (r.tool_steps.empty() && r.tool_calls_made > 0)
    {
        recordToolTurnInChatHistory("minimal_agentic",
                                    std::string("[aggregated] tool_calls=") + std::to_string(r.tool_calls_made));
    }

    std::string text = formatMinimalAgenticLineForUi(r);
    if (!r.success && !r.error.empty())
    {
        text = std::string("[Agentic] ") + r.error + (text.empty() ? std::string() : std::string("\n") + text);
    }
    if (!text.empty())
    {
        appendCopilotResponse(text);
    }
    else
    {
        // Tool-only turns already updated m_chatHistory; no assistant line to append.
        updateSecondarySidebarContent();
    }
    updateEnhancedStatusBar();
}

void Win32IDE::appendCopilotResponse(const std::string& response)
{
    const std::string safe = sanitizeChatText(response);
    conversationAddAssistant(safe);
    m_chatHistory.push_back({"assistant", safe});
    persistChatTurnToDisk("assistant", safe);
    updateSecondarySidebarContent();
}

void Win32IDE::appendModelLoadReadyCopilotTurns(const std::string& filepath, bool assistantWelcome)
{
    if (!filepath.empty())
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        fs::path p(filepath);
        std::string canon = filepath;
        if (fs::exists(p, ec))
            canon = fs::weakly_canonical(p, ec).lexically_normal().string();
        else
            canon = fs::absolute(p, ec).lexically_normal().string();
        const std::string leaf = p.filename().string();
        const std::string sysLine = leaf.empty() ? (std::string("Model ready: ") + canon)
                                                 : (std::string("Model ready: ") + leaf + " | " + canon);
        m_chatHistory.push_back({"system", sysLine});
        persistChatTurnToDisk("system", sysLine);
        rehydrateConversationSessionFromChatHistory();
    }

    if (assistantWelcome)
    {
        const std::string msg =
            std::string("✅ Model loaded and ready for inference!\r\n\r\n"
                        "You can now ask questions in the chat panel. Agent mode is on by default "
                        "(tools use the same workspace as File Explorer — toggle off in the AI sidebar if you want "
                        "plain chat only).\r\n"
                        "Try: 'hello', 'list files in src', or a small coding task.");
        appendCopilotResponse(msg);
    }
    else
    {
        updateSecondarySidebarContent();
    }
}

void Win32IDE::tryConfigureNativeStreamBridge()
{
    if (m_streamBridgeConfigured)
    {
        return;
    }
    if (!m_hwndMain || !IsWindow(m_hwndMain))
    {
        return;
    }

    if (!m_streamBridgeModule)
    {
        m_streamBridgeModule = GetModuleHandleA("RawrXD_Titan.dll");
        if (!m_streamBridgeModule)
        {
            m_streamBridgeModule = GetModuleHandleA("RawrXD.dll");
        }
        if (!m_streamBridgeModule)
        {
            static const char* kStreamBridgeCandidates[] = {"D:\\rawrxd\\build_smoke_verify2\\bin\\RawrXD_Titan.dll",
                                                            "D:\\rawrxd\\bin\\RawrXD_Titan.dll", "RawrXD_Titan.dll",
                                                            "bin\\RawrXD_Titan.dll"};
            for (const char* candidate : kStreamBridgeCandidates)
            {
                m_streamBridgeModule = LoadLibraryA(candidate);
                if (m_streamBridgeModule)
                {
                    RAWRXD_LOG_INFO("Win32IDE_VSCodeUI") << "Loaded stream bridge module from: " << candidate;
                    break;
                }
            }
        }
    }

    if (!m_streamBridgeModule)
    {
        RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI") << "Native stream bridge module not available";
        return;
    }

    g_titanDiagWindow = m_hwndMain;

    using StreamConfigureWindowFn = int32_t(__stdcall*)(uint64_t, uint32_t, uint32_t);
    using StreamResetFn = int32_t(__stdcall*)();
    using SetDiagnosticCallbackFn = int32_t(__stdcall*)(void (*)(const char*));
    constexpr int32_t kRawrSuccess = 0;

    auto* setDiagnosticCallback =
        reinterpret_cast<SetDiagnosticCallbackFn>(GetProcAddress(m_streamBridgeModule, "RawrXD_SetDiagnosticCallback"));
    if (setDiagnosticCallback)
    {
        int32_t cbStatus = setDiagnosticCallback(TitanDiagnosticForwarder);
        if (cbStatus != kRawrSuccess)
        {
            RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI") << "RawrXD_SetDiagnosticCallback failed, status=" << cbStatus;
        }
    }

    auto* configure =
        reinterpret_cast<StreamConfigureWindowFn>(GetProcAddress(m_streamBridgeModule, "RawrXD_StreamConfigureWindow"));
    if (!configure)
    {
        RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI") << "RawrXD_StreamConfigureWindow export missing";
        return;
    }

    auto* reset = reinterpret_cast<StreamResetFn>(GetProcAddress(m_streamBridgeModule, "RawrXD_StreamReset"));
    if (reset)
    {
        (void)reset();
    }

    int32_t status =
        configure(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m_hwndMain)),
                  static_cast<uint32_t>(WM_RAWR_STREAM_DATA), static_cast<uint32_t>(RAWR_STREAM_WPARAM_TAG));
    if (status == kRawrSuccess)
    {
        m_streamBridgeConfigured = true;
        RAWRXD_LOG_INFO("Win32IDE_VSCodeUI") << "Native stream bridge configured (WM_APP+1337)";
        return;
    }

    RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI") << "Native stream bridge configuration failed, status=" << status;
}

void Win32IDE::handleNativeStreamTick(WPARAM wParam, LPARAM)
{
    if (static_cast<uint32_t>(wParam) != RAWR_STREAM_WPARAM_TAG)
    {
        return;
    }
    if (!m_hwndCopilotChatOutput || !IsWindow(m_hwndCopilotChatOutput))
    {
        return;
    }

    if (!m_streamBridgeConfigured)
    {
        tryConfigureNativeStreamBridge();
        if (!m_streamBridgeConfigured)
        {
            return;
        }
    }

    using StreamPopFn = int32_t(__stdcall*)(char*, size_t, uint32_t*, uint64_t*);
    using StreamGetStatsFn = int32_t(__stdcall*)(uint32_t*, uint32_t*);
    constexpr int32_t kRawrSuccess = 0;

    auto* popChunk = reinterpret_cast<StreamPopFn>(GetProcAddress(m_streamBridgeModule, "RawrXD_StreamPop"));
    if (!popChunk)
    {
        return;
    }

    auto* getStats = reinterpret_cast<StreamGetStatsFn>(GetProcAddress(m_streamBridgeModule, "RawrXD_StreamGetStats"));

    constexpr size_t kChunkBufferSize = 4096;
    char chunk[kChunkBufferSize] = {};
    bool appended = false;

    for (int i = 0; i < 256; ++i)
    {
        uint32_t outLen = 0;
        uint64_t outSeq = 0;
        int32_t status = popChunk(chunk, sizeof(chunk), &outLen, &outSeq);
        if (status != kRawrSuccess || outLen == 0)
        {
            break;
        }

        if (outLen >= sizeof(chunk))
        {
            outLen = static_cast<uint32_t>(sizeof(chunk) - 1);
        }
        chunk[outLen] = '\0';

        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, -1, -1);
        SendMessage(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)chunk);
        appended = true;
    }

    if (appended)
    {
        SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
    }

    if (getStats)
    {
        uint32_t queued = 0;
        uint32_t dropped = 0;
        if (getStats(&queued, &dropped) == kRawrSuccess && dropped > 0)
        {
            RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI")
                << "Native stream dropped chunks=" << dropped << " queued=" << queued;
        }
    }
}

void Win32IDE::performEmergencyWipeAndShutdown()
{
    RAWRXD_LOG_WARNING("Win32IDE_VSCodeUI") << "Emergency wipe hotkey triggered";

    for (auto& entry : m_chatHistory)
    {
        wipeStringMemory(entry.first);
        wipeStringMemory(entry.second);
    }
    m_chatHistory.clear();
    m_chatToolActions.clear();
    m_currentToolActions.clear();

    {
        std::lock_guard<std::mutex> lock(m_streamingOutputMutex);
        wipeStringMemory(m_streamingOutput);
    }
    wipeStringMemory(m_ghostTextContent);
    m_ghostTextVisible = false;
    m_ghostTextPending = false;
    m_ghostTextAccepted = false;
    m_ghostTextLine = -1;
    m_ghostTextColumn = -1;

    {
        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
        for (auto& kv : m_ghostTextCache)
        {
            wipeStringMemory(kv.second.completion);
        }
        m_ghostTextCache.clear();
    }

    if (m_streamBridgeModule)
    {
        using StreamResetFn = int32_t(__stdcall*)();
        auto* reset = reinterpret_cast<StreamResetFn>(GetProcAddress(m_streamBridgeModule, "RawrXD_StreamReset"));
        if (reset)
        {
            (void)reset();
        }
    }
    m_streamBridgeConfigured = false;

    if (m_hwndCopilotChatInput && IsWindow(m_hwndCopilotChatInput))
    {
        SetWindowTextA(m_hwndCopilotChatInput, "");
    }
    if (m_hwndCopilotChatOutput && IsWindow(m_hwndCopilotChatOutput))
    {
        SetWindowTextA(m_hwndCopilotChatOutput, "");
    }
    if (m_hwndEditor && IsWindow(m_hwndEditor))
    {
        SetWindowTextA(m_hwndEditor, "");
    }

    FlushProcessWriteBuffers();

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        DestroyWindow(m_hwndMain);
    }
    else
    {
        PostQuitMessage(0);
    }
}

LRESULT CALLBACK Win32IDE::SecondarySidebarProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        {
            HDC hdc = (HDC)wParam;
            // Use Adobe RGBa colors with proper conversion to COLORREF
            SetBkColor(hdc, AdobeRGBaToCOLORREF(VSCODE_SIDEBAR_BG));
            SetTextColor(hdc, AdobeRGBaToCOLORREF(AdobeRGBa(0.80f, 0.80f, 0.80f, 1.00f)));
            static HBRUSH hBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSCODE_SIDEBAR_BG));
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
    m_hwndPanelContainer = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 0, 0, 800, m_panelHeight, hwndParent,
                                           (HMENU)IDC_PANEL_CONTAINER, m_hInstance, nullptr);

    // Create tab control for panel views
    m_hwndPanelTabs = CreateWindowExA(0, WC_TABCONTROLA, "", WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_FOCUSNEVER, 0, 0,
                                      400, 24, m_hwndPanelContainer, (HMENU)IDC_PANEL_TABS, m_hInstance, nullptr);

    // Add tabs: Terminal, Output, Problems, Debug Console
    TCITEMA tie = {TCIF_TEXT};
    tie.pszText = const_cast<char*>("Terminal");
    TabCtrl_InsertItem(m_hwndPanelTabs, 0, &tie);
    tie.pszText = const_cast<char*>("Output");
    TabCtrl_InsertItem(m_hwndPanelTabs, 1, &tie);
    tie.pszText = const_cast<char*>("Problems");
    TabCtrl_InsertItem(m_hwndPanelTabs, 2, &tie);
    tie.pszText = const_cast<char*>("Debug Console");
    TabCtrl_InsertItem(m_hwndPanelTabs, 3, &tie);

    // Create panel toolbar (right side)
    m_hwndPanelToolbar = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 400, 0, 200, 24, m_hwndPanelContainer,
                                         (HMENU)IDC_PANEL_TOOLBAR, m_hInstance, nullptr);

    // Toolbar buttons
    m_hwndPanelNewTerminalBtn =
        CreateWindowExA(0, "BUTTON", "+", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 24, 22, m_hwndPanelToolbar,
                        (HMENU)IDC_PANEL_BTN_NEW_TERMINAL, m_hInstance, nullptr);

    m_hwndPanelSplitTerminalBtn =
        CreateWindowExA(0, "BUTTON", "||", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 26, 0, 24, 22, m_hwndPanelToolbar,
                        (HMENU)IDC_PANEL_BTN_SPLIT_TERMINAL, m_hInstance, nullptr);

    m_hwndPanelKillTerminalBtn =
        CreateWindowExA(0, "BUTTON", "X", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 52, 0, 24, 22, m_hwndPanelToolbar,
                        (HMENU)IDC_PANEL_BTN_KILL_TERMINAL, m_hInstance, nullptr);

    m_hwndPanelMaximizeBtn = CreateWindowExA(0, "BUTTON", "^", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130, 0, 24, 22,
                                             m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_MAXIMIZE, m_hInstance, nullptr);

    m_hwndPanelCloseBtn = CreateWindowExA(0, "BUTTON", "x", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 156, 0, 24, 22,
                                          m_hwndPanelToolbar, (HMENU)IDC_PANEL_BTN_CLOSE, m_hInstance, nullptr);

    // VS Code–style toolbar tooltips (parity with Terminal panel actions)
    auto addPanelToolbarTooltip = [this, hwndParent](HWND btn, const char* tipText)
    {
        if (!btn || !hwndParent || !tipText)
            return;
        HWND hwndTip =
            CreateWindowEx(0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT, hwndParent, nullptr, m_hInstance, nullptr);
        TOOLINFOA ti = {sizeof(TOOLINFOA)};
        ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
        ti.hwnd = hwndParent;
        ti.uId = (UINT_PTR)btn;
        ti.lpszText = const_cast<char*>(tipText);
        SendMessageA(hwndTip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
    };
    addPanelToolbarTooltip(m_hwndPanelNewTerminalBtn, "New Terminal (Ctrl+Shift+`)");
    addPanelToolbarTooltip(m_hwndPanelSplitTerminalBtn, "Split Terminal");
    addPanelToolbarTooltip(m_hwndPanelKillTerminalBtn, "Kill the Active Terminal Instance");
    addPanelToolbarTooltip(m_hwndPanelMaximizeBtn, "Maximize Panel Size");
    addPanelToolbarTooltip(m_hwndPanelCloseBtn, "Hide Panel");

    // Create Problems list view
    m_hwndProblemsListView =
        CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 26, 800,
                        m_panelHeight - 26, m_hwndPanelContainer, (HMENU)IDC_PANEL_PROBLEMS_LIST, m_hInstance, nullptr);

    // Add columns to Problems list
    LVCOLUMNA lvc = {0};
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

    if (m_panelMaximized)
    {
        // Store original height and maximize
        RECT rect;
        GetClientRect(m_hwndMain, &rect);
        m_panelHeight = rect.bottom - 100;  // Leave some space for toolbar/status
        SetWindowTextA(m_hwndPanelMaximizeBtn, "v");
    }
    else
    {
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
    if (m_panelMaximized)
    {
        maximizePanel();  // Toggle back to normal
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
    for (auto& pane : m_terminalPanes)
    {
        ShowWindow(pane.hwnd, showTerminal ? SW_SHOW : SW_HIDE);
    }

    // Show/hide output windows
    for (auto& kv : m_outputWindows)
    {
        bool show = showOutput && (kv.first == m_activeOutputTab);
        ShowWindow(kv.second, show ? SW_SHOW : SW_HIDE);
    }

    // Show/hide problems list
    ShowWindow(m_hwndProblemsListView, showProblems ? SW_SHOW : SW_HIDE);

    // Show/hide debug console
    if (m_hwndDebugConsole)
    {
        ShowWindow(m_hwndDebugConsole, showDebugConsole ? SW_SHOW : SW_HIDE);
    }

    // Update tab selection
    if (m_hwndPanelTabs && IsWindow(m_hwndPanelTabs))
        TabCtrl_SetCurSel(m_hwndPanelTabs, static_cast<int>(tab));

    // Update toolbar buttons based on current tab
    bool isTerminalTab = (tab == PanelTab::Terminal);
    EnableWindow(m_hwndPanelNewTerminalBtn, isTerminalTab);
    EnableWindow(m_hwndPanelSplitTerminalBtn, isTerminalTab);
    EnableWindow(m_hwndPanelKillTerminalBtn, isTerminalTab);

    if (isTerminalTab)
        refreshIntegratedTerminalContextHint();
    else if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"");
}

void Win32IDE::updatePanelContent()
{
    // Update problems count in tab
    std::string problemsTabText = "Problems";
    if (m_errorCount > 0 || m_warningCount > 0)
    {
        std::ostringstream oss;
        oss << "Problems (" << m_errorCount << " errors, " << m_warningCount << " warnings)";
        problemsTabText = oss.str();
    }

    TCITEMA tie = {TCIF_TEXT};
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
    if (severity == 0)
        m_errorCount++;
    else if (severity == 1)
        m_warningCount++;

    // Add to list view
    LVITEMA lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = static_cast<int>(m_problems.size() - 1);

    const char* severityStr = (severity == 0) ? "Error" : (severity == 1) ? "Warning" : "Info";
    lvi.pszText = const_cast<char*>(severityStr);
    ListView_InsertItem(m_hwndProblemsListView, &lvi);

    // Set item text using direct SendMessage calls with ANSI structures
    LVITEMA lviSet = {0};
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
    if (index < 0 || index >= static_cast<int>(m_problems.size()))
        return;

    const ProblemItem& problem = m_problems[index];

    // Open file if different from current
    if (problem.file != m_currentFile)
    {
        // Load the file
        std::ifstream file(problem.file);
        if (file)
        {
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
    m_hwndStatusBar = CreateWindowExA(0, STATUSCLASSNAMEA, "", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                                      hwndParent, (HMENU)IDC_STATUS_BAR, m_hInstance, nullptr);

    // Set up parts - 12 parts for all status items
    // [Remote][Branch][Sync][Errors][Warnings] ... [Line:Col][Spaces][Encoding][EOL][Language][Copilot]
    int parts[] = {80, 150, 200, 250, 300, -1, 380, 440, 510, 560, 650, 700};
    SendMessage(m_hwndStatusBar, SB_SETPARTS, 12, (LPARAM)parts);

    // Set initial text
    updateEnhancedStatusBar();
}

void Win32IDE::updateEnhancedStatusBar()
{
    if (!m_hwndStatusBar)
        return;

    // Part 0: Remote indicator (if connected)
    if (!m_statusBarInfo.remoteName.empty())
    {
        std::string remoteText = ">< " + m_statusBarInfo.remoteName;
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)remoteText.c_str());
    }
    else
    {
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM) "");
    }

    // Part 1: Branch indicator
    std::string branchText = "<> " + m_statusBarInfo.branchName;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 1, (LPARAM)branchText.c_str());

    // Part 2: Sync status (ahead/behind)
    std::ostringstream syncOss;
    if (m_statusBarInfo.syncAhead > 0 || m_statusBarInfo.syncBehind > 0)
    {
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
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 5, (LPARAM) "");

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

    // Part 11: Backend / AI status — includes last estimated tok/s from chat completions (Cursor-style feedback).
    {
        std::string backendText = getActiveBackendName();
        if (!m_statusBarInfo.copilotActive)
            backendText += " (off)";

        // Model info (local GGUF / resolved paths) — show last successful load size + load wall-time.
        if (m_lastLoadedModelOk)
        {
            if (!m_lastLoadedModelDisplayName.empty())
            {
                backendText += " | ";
                backendText += m_lastLoadedModelDisplayName;
            }
            if (m_lastLoadedModelBytes > 0)
            {
                const double gb = static_cast<double>(m_lastLoadedModelBytes) / (1024.0 * 1024.0 * 1024.0);
                std::ostringstream szOss;
                szOss << std::fixed << std::setprecision(gb >= 10.0 ? 0 : 1) << gb;
                backendText += " ";
                backendText += szOss.str();
                backendText += "GB";
            }
            if (m_lastLoadedModelWallMs >= 1.0)
            {
                std::ostringstream msOss;
                msOss << std::fixed << std::setprecision(m_lastLoadedModelWallMs >= 10000.0 ? 0 : 1)
                      << (m_lastLoadedModelWallMs / 1000.0);
                backendText += " (load ";
                backendText += msOss.str();
                backendText += "s)";
            }
        }

        const double tpsCopilot = METRICS.getGauge("chat.copilot.estimated_tps");
        const double tpsMini = METRICS.getGauge("chat.minimal_agentic.estimated_tps");
        const double tpsEst = (std::max)(tpsCopilot, tpsMini);
        if (tpsEst > 0.5)
        {
            std::ostringstream tpsOss;
            tpsOss << std::fixed << std::setprecision(0) << tpsEst;
            backendText += " | ~";
            backendText += tpsOss.str();
            backendText += " t/s est";
        }

        const double alpha = METRICS.getGauge("spec.telemetry.acceptance_rate");
        const double roi = METRICS.getGauge("spec.telemetry.roi");
        const double totalMs = METRICS.getGauge("spec.telemetry.total_ms");
        const double effectiveTps = METRICS.getGauge("spec.telemetry.effective_tps");
        const int expertId = static_cast<int>(std::llround(METRICS.getGauge("spec.telemetry.expert_id")));
        if (totalMs > 0.0)
        {
            std::ostringstream telem;
            telem << std::fixed << std::setprecision(2);
            telem << " | α=" << alpha;
            telem << " ROI=" << roi;
            if (effectiveTps > 0.1)
            {
                telem << " t/s=" << std::setprecision(1) << effectiveTps;
                telem << std::setprecision(2);
            }
            if (expertId >= 0)
            {
                telem << " E=" << expertId;
            }
            if (roi < 1.0)
            {
                telem << " [TAX]";
            }
            else if (roi >= 1.5)
            {
                telem << " [DIV]";
            }
            backendText += telem.str();
        }
        SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 11, (LPARAM)backendText.c_str());
    }
}

void Win32IDE::updateCursorPosition()
{
    if (!m_hwndEditor)
        return;

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
    if (filePath.empty())
    {
        m_statusBarInfo.languageMode = "Plain Text";
        return;
    }

    // Get file extension
    size_t dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos)
    {
        m_statusBarInfo.languageMode = "Plain Text";
        return;
    }

    std::string ext = filePath.substr(dotPos + 1);

    // Convert to lowercase
    for (char& c : ext)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    // Map extension to language mode
    static const std::map<std::string, std::string> extToLang = {{"cpp", "C++"},
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
                                                                 {"s", "Assembly"}};

    auto it = extToLang.find(ext);
    if (it != extToLang.end())
    {
        m_statusBarInfo.languageMode = it->second;
    }
    else
    {
        m_statusBarInfo.languageMode = "Plain Text";
    }
}
