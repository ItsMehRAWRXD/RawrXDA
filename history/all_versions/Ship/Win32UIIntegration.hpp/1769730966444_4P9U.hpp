// Win32UIIntegration.hpp - Native Win32 Chat UI, Diff Viewer, Status Updates
// Pure C++20 / Win32 - Zero Qt Dependencies
#pragma once

#include "agent_kernel_main.hpp"
#include "QtReplacements.hpp"
#include "AgentOrchestrator.hpp"
#include <commctrl.h>
#include <richedit.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace RawrXD {
namespace UI {

// Color scheme
struct ColorScheme {
    COLORREF background = RGB(30, 30, 30);
    COLORREF foreground = RGB(212, 212, 212);
    COLORREF userMessage = RGB(86, 156, 214);
    COLORREF assistantMessage = RGB(78, 201, 176);
    COLORREF toolCall = RGB(220, 220, 170);
    COLORREF error = RGB(244, 71, 71);
    COLORREF success = RGB(78, 201, 176);
    COLORREF selection = RGB(51, 153, 255);
    COLORREF border = RGB(60, 60, 60);
    COLORREF inputBg = RGB(45, 45, 45);
};

// Window IDs
constexpr int ID_CHAT_DISPLAY = 1001;
constexpr int ID_CHAT_INPUT = 1002;
constexpr int ID_SEND_BUTTON = 1003;
constexpr int ID_STOP_BUTTON = 1004;
constexpr int ID_CLEAR_BUTTON = 1005;
constexpr int ID_STATUS_BAR = 1006;
constexpr int ID_MODEL_COMBO = 1007;

// Chat Panel
class ChatPanel {
public:
    ChatPanel() = default;

    bool create(HWND parent, int x, int y, int width, int height) {
        m_parent = parent;

        // Load RichEdit
        LoadLibraryW(L"Msftedit.dll");

        // Create chat display (RichEdit)
        m_chatDisplay = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            x, y, width, height - 80,
            parent,
            reinterpret_cast<HMENU>(ID_CHAT_DISPLAY),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_chatDisplay) return false;

        // Set background color
        SendMessageW(m_chatDisplay, EM_SETBKGNDCOLOR, 0, m_colors.background);

        // Set default text format
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = m_colors.foreground;
        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 200; // 10pt
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        // Create input box
        m_chatInput = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            x, y + height - 75, width - 180, 70,
            parent,
            reinterpret_cast<HMENU>(ID_CHAT_INPUT),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_chatInput) return false;

        SendMessageW(m_chatInput, EM_SETBKGNDCOLOR, 0, m_colors.inputBg);
        SendMessageW(m_chatInput, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        // Create buttons
        m_sendButton = CreateWindowW(
            L"BUTTON", L"Send",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + width - 175, y + height - 75, 80, 32,
            parent,
            reinterpret_cast<HMENU>(ID_SEND_BUTTON),
            GetModuleHandleW(nullptr),
            nullptr
        );

        m_stopButton = CreateWindowW(
            L"BUTTON", L"Stop",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + width - 90, y + height - 75, 80, 32,
            parent,
            reinterpret_cast<HMENU>(ID_STOP_BUTTON),
            GetModuleHandleW(nullptr),
            nullptr
        );

        m_clearButton = CreateWindowW(
            L"BUTTON", L"Clear",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + width - 175, y + height - 38, 165, 32,
            parent,
            reinterpret_cast<HMENU>(ID_CLEAR_BUTTON),
            GetModuleHandleW(nullptr),
            nullptr
        );

        return true;
    }

    void appendMessage(const QString& sender, const QString& message, COLORREF color) {
        // Move to end
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        // Set color for sender
        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_BOLD;
        cf.crTextColor = color;
        cf.dwEffects = CFE_BOLD;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        // Add sender
        QString senderLine = sender + QString(": ");
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(senderLine.c_str()));

        // Set color for message (not bold)
        cf.dwEffects = 0;
        cf.crTextColor = m_colors.foreground;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        // Add message
        QString msgLine = message + QString("\n\n");
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(msgLine.c_str()));

        // Scroll to bottom
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void appendUserMessage(const QString& message) {
        appendMessage(QString("You"), message, m_colors.userMessage);
    }

    void appendAssistantMessage(const QString& message) {
        appendMessage(QString("Assistant"), message, m_colors.assistantMessage);
    }

    void appendToolCall(const QString& tool, const QString& args) {
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_ITALIC;
        cf.crTextColor = m_colors.toolCall;
        cf.dwEffects = CFE_ITALIC;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        QString text = QString("[Tool: ") + tool + QString("]\n") + args + QString("\n\n");
        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void appendError(const QString& error) {
        appendMessage(QString("Error"), error, m_colors.error);
    }

    void appendStreamChunk(const QString& chunk) {
        int length = GetWindowTextLengthW(m_chatDisplay);
        SendMessageW(m_chatDisplay, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = m_colors.foreground;
        SendMessageW(m_chatDisplay, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        SendMessageW(m_chatDisplay, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(chunk.c_str()));
        SendMessageW(m_chatDisplay, WM_VSCROLL, SB_BOTTOM, 0);
    }

    void clear() {
        SetWindowTextW(m_chatDisplay, L"");
    }

    QString getInputText() const {
        int length = GetWindowTextLengthW(m_chatInput);
        if (length == 0) return QString();

        std::wstring buffer(length + 1, L'\0');
        GetWindowTextW(m_chatInput, buffer.data(), length + 1);
        buffer.resize(length);
        return QString(buffer);
    }

    void clearInput() {
        SetWindowTextW(m_chatInput, L"");
    }

    void setInputFocus() {
        SetFocus(m_chatInput);
    }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_chatDisplay, x, y, width, height - 80, TRUE);
        MoveWindow(m_chatInput, x, y + height - 75, width - 180, 70, TRUE);
        MoveWindow(m_sendButton, x + width - 175, y + height - 75, 80, 32, TRUE);
        MoveWindow(m_stopButton, x + width - 90, y + height - 75, 80, 32, TRUE);
        MoveWindow(m_clearButton, x + width - 175, y + height - 38, 165, 32, TRUE);
    }

    HWND chatDisplay() const { return m_chatDisplay; }
    HWND chatInput() const { return m_chatInput; }
    HWND sendButton() const { return m_sendButton; }
    HWND stopButton() const { return m_stopButton; }
    HWND clearButton() const { return m_clearButton; }

private:
    HWND m_parent = nullptr;
    HWND m_chatDisplay = nullptr;
    HWND m_chatInput = nullptr;
    HWND m_sendButton = nullptr;
    HWND m_stopButton = nullptr;
    HWND m_clearButton = nullptr;
    ColorScheme m_colors;
};

// Status Bar
class StatusBar {
public:
    bool create(HWND parent) {
        m_statusBar = CreateWindowExW(
            0,
            STATUSCLASSNAMEW,
            nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0,
            parent,
            reinterpret_cast<HMENU>(ID_STATUS_BAR),
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_statusBar) return false;

        // Set parts
        int parts[] = { 200, 400, -1 };
        SendMessageW(m_statusBar, SB_SETPARTS, 3, reinterpret_cast<LPARAM>(parts));

        return true;
    }

    void setState(const QString& state) {
        SendMessageW(m_statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(state.c_str()));
    }

    void setModel(const QString& model) {
        QString text = QString("Model: ") + model;
        SendMessageW(m_statusBar, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(text.c_str()));
    }

    void setMessage(const QString& message) {
        SendMessageW(m_statusBar, SB_SETTEXTW, 2, reinterpret_cast<LPARAM>(message.c_str()));
    }

    void resize() {
        SendMessageW(m_statusBar, WM_SIZE, 0, 0);
    }

    int height() const {
        RECT rc;
        GetWindowRect(m_statusBar, &rc);
        return rc.bottom - rc.top;
    }

    HWND handle() const { return m_statusBar; }

private:
    HWND m_statusBar = nullptr;
};

// Diff Viewer (for showing file changes)
class DiffViewer {
public:
    bool create(HWND parent, int x, int y, int width, int height) {
        LoadLibraryW(L"Msftedit.dll");

        m_window = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            MSFTEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY,
            x, y, width, height,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            nullptr
        );

        if (!m_window) return false;

        SendMessageW(m_window, EM_SETBKGNDCOLOR, 0, m_colors.background);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = m_colors.foreground;
        wcscpy_s(cf.szFaceName, L"Consolas");
        cf.yHeight = 180;
        SendMessageW(m_window, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

        return true;
    }

    void showDiff(const QString& original, const QString& modified, const QString& filename) {
        SetWindowTextW(m_window, L"");

        appendLine(QString("=== ") + filename + QString(" ==="), m_colors.foreground);
        appendLine(QString(""), m_colors.foreground);

        auto origLines = original.split(QString("\n"));
        auto modLines = modified.split(QString("\n"));

        // Simple line-by-line diff
        size_t maxLines = (std::max)(origLines.size(), modLines.size());

        for (size_t i = 0; i < maxLines; ++i) {
            QString origLine = i < origLines.size() ? origLines[i] : QString();
            QString modLine = i < modLines.size() ? modLines[i] : QString();

            if (origLine == modLine) {
                appendLine(QString("  ") + origLine, m_colors.foreground);
            } else {
                if (!origLine.isEmpty()) {
                    appendLine(QString("- ") + origLine, RGB(244, 71, 71));
                }
                if (!modLine.isEmpty()) {
                    appendLine(QString("+ ") + modLine, RGB(78, 201, 176));
                }
            }
        }
    }

    void show() { ShowWindow(m_window, SW_SHOW); }
    void hide() { ShowWindow(m_window, SW_HIDE); }

    void resize(int x, int y, int width, int height) {
        MoveWindow(m_window, x, y, width, height, TRUE);
    }

    HWND handle() const { return m_window; }

private:
    void appendLine(const QString& line, COLORREF color) {
        int length = GetWindowTextLengthW(m_window);
        SendMessageW(m_window, EM_SETSEL, length, length);

        CHARFORMAT2W cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        SendMessageW(m_window, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));

        QString text = line + QString("\n");
        SendMessageW(m_window, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    }

    HWND m_window = nullptr;
    ColorScheme m_colors;
};

// Main Agent Window
class AgentWindow {
public:
    AgentWindow() = default;

    bool create(HINSTANCE hInstance, const QString& title, int width = 1200, int height = 800) {
        m_hInstance = hInstance;

        // Register window class
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
        wc.lpszClassName = L"RawrXDAgentWindow";
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

        if (!RegisterClassExW(&wc)) return false;

        // Create window
        m_window = CreateWindowExW(
            0,
            L"RawrXDAgentWindow",
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, hInstance, this
        );

        if (!m_window) return false;

        // Initialize common controls
        INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
        InitCommonControlsEx(&icc);

        // Create UI components
        RECT rc;
        GetClientRect(m_window, &rc);

        if (!m_statusBar.create(m_window)) return false;

        int statusHeight = m_statusBar.height();
        if (!m_chatPanel.create(m_window, 10, 10, rc.right - 20, rc.bottom - statusHeight - 20)) {
            return false;
        }

        m_statusBar.setState(QString("Ready"));
        m_statusBar.setModel(QString("Not connected"));

        return true;
    }

    void show() {
        ShowWindow(m_window, SW_SHOW);
        UpdateWindow(m_window);
    }

    void setAgent(AgentOrchestrator* agent) {
        m_agent = agent;
        if (agent) {
            agent->setEventCallback([this](const AgentEvent& event) {
                handleAgentEvent(event);
            });

            if (agent->isLLMAvailable()) {
                m_statusBar.setModel(agent->config().model);
            }
        }
    }

    int run() {
        MSG msg = {};
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return static_cast<int>(msg.wParam);
    }

    HWND handle() const { return m_window; }
    ChatPanel& chatPanel() { return m_chatPanel; }
    StatusBar& statusBar() { return m_statusBar; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        AgentWindow* self = nullptr;

        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            self = static_cast<AgentWindow*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<AgentWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self) {
            return self->handleMessage(hwnd, msg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
            case WM_SIZE: {
                RECT rc;
                GetClientRect(hwnd, &rc);
                m_statusBar.resize();
                int statusHeight = m_statusBar.height();
                m_chatPanel.resize(10, 10, rc.right - 20, rc.bottom - statusHeight - 20);
                return 0;
            }

            case WM_COMMAND: {
                int id = LOWORD(wParam);

                if (id == ID_SEND_BUTTON) {
                    sendMessage();
                } else if (id == ID_STOP_BUTTON) {
                    if (m_agent) m_agent->stop();
                } else if (id == ID_CLEAR_BUTTON) {
                    m_chatPanel.clear();
                    if (m_agent) m_agent->clearConversation();
                }
                return 0;
            }

            case WM_KEYDOWN: {
                if (wParam == VK_RETURN && GetKeyState(VK_CONTROL) < 0) {
                    sendMessage();
                    return 0;
                }
                break;
            }

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void sendMessage() {
        QString text = m_chatPanel.getInputText();
        if (text.isEmpty()) return;

        m_chatPanel.clearInput();
        m_chatPanel.appendUserMessage(text);

        if (m_agent) {
            m_agent->runAgentLoopAsync(text);
        }
    }

    void handleAgentEvent(const AgentEvent& event) {
        // Post to UI thread
        PostMessageW(m_window, WM_USER + 1, static_cast<WPARAM>(event.type),
            reinterpret_cast<LPARAM>(new AgentEvent(event)));
    }

    // Note: In a real implementation, you'd handle WM_USER+1 to process events on UI thread
    // For simplicity, we're calling UI updates directly (which requires thread sync)

    HINSTANCE m_hInstance = nullptr;
    HWND m_window = nullptr;
    ChatPanel m_chatPanel;
    StatusBar m_statusBar;
    AgentOrchestrator* m_agent = nullptr;
};

} // namespace UI
} // namespace RawrXD
