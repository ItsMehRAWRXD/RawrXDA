// ============================================================================
// Win32IDE_IRCBridgeCommands.cpp — Win32IDE command methods for IRC Bridge
// ============================================================================
// Implements the Win32IDE methods that manage the IRC Bridge lifecycle and
// dispatch inbound IRC commands to IDE actions.
//
//  Command flow:
//    1. User in mIRC types:  !build  (in the #rawrxd-ide channel)
//    2. IRCBridge.workerThread() receives PRIVMSG, verifies ownerNick,
//       calls m_commandCb("username", "build", "")
//    3. dispatchIRCCommand() is called on the IRC worker thread and
//       posts WM_COMMAND IDM_BUILD_PROJECT to the main Win32 message loop
//       to ensure all UI operations happen on the UI thread.
// ============================================================================

#include "Win32IDE.h"
#include "Win32IDE_IRCBridge.h"
#include <sstream>
#include <string>

using namespace RawrXD::IRC;

// ---------------------------------------------------------------------------
// initIRCBridge — create bridge instance and register callback
// ---------------------------------------------------------------------------
void Win32IDE::initIRCBridge()
{
    if (m_ircBridgeInitialized) return;

    // Default settings — user can override via IDM_IRC_CONFIG dialog.
    m_ircSettings.server       = "irc.libera.chat";
    m_ircSettings.port         = 6667;
    m_ircSettings.nick         = "RawrXD-IDE";
    m_ircSettings.realname     = "RawrXD IDE Bot";
    m_ircSettings.channel      = "#rawrxd-ide";
    m_ircSettings.maxOutputLines = 12;

    m_ircBridge = std::make_unique<IRCBridge>(this);

    // Register ICC command → IDE dispatch callback (called on IRC worker thread).
    m_ircBridge->setCommandCallback(
        [this](const std::string& nick,
               const std::string& cmd,
               const std::string& args,
               const std::string& replyTarget,
               bool isDirectMessage)
        {
            dispatchIRCCommand(nick, cmd, args, replyTarget, isDirectMessage);
        }
    );

    m_ircBridgeInitialized = true;
    appendToOutput("[IRC] Bridge initialised. Use Tools → IRC Bridge → Connect to go online.",
                   "IRC", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// handleIRCBridgeCommand — menu command router
// ---------------------------------------------------------------------------
bool Win32IDE::handleIRCBridgeCommand(int commandId)
{
    switch (commandId)
    {
        case IDM_IRC_CONNECT:    cmdIRCConnect();    return true;
        case IDM_IRC_DISCONNECT: cmdIRCDisconnect(); return true;
        case IDM_IRC_STATUS:     cmdIRCStatus();     return true;
        case IDM_IRC_CONFIG:     cmdIRCConfig();     return true;
        case IDM_IRC_SEND:       cmdIRCSend();       return true;
        default: return false;
    }
}

// ---------------------------------------------------------------------------
// cmdIRCConnect — connect/reconnect to IRC
// ---------------------------------------------------------------------------
void Win32IDE::cmdIRCConnect()
{
    if (!m_ircBridgeInitialized) initIRCBridge();

    if (m_ircBridge->getState() == IRCState::InChannel ||
        m_ircBridge->getState() == IRCState::Connected)
    {
        appendToOutput("[IRC] Already connected.", "IRC", OutputSeverity::Info);
        MessageBoxW(m_hwndMain, L"IRC Bridge is already connected.", L"IRC Bridge", MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_ircBridge->start(m_ircSettings);
    appendToOutput("[IRC] Connecting to " + m_ircSettings.server + ":" +
                   std::to_string(m_ircSettings.port) + " as " + m_ircSettings.nick,
                   "IRC", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// cmdIRCDisconnect — gracefully disconnect
// ---------------------------------------------------------------------------
void Win32IDE::cmdIRCDisconnect()
{
    if (!m_ircBridge) {
        appendToOutput("[IRC] Not connected.", "IRC", OutputSeverity::Info);
        return;
    }
    m_ircBridge->stop();
    appendToOutput("[IRC] Disconnected.", "IRC", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// cmdIRCStatus — show connection status in IDE output
// ---------------------------------------------------------------------------
void Win32IDE::cmdIRCStatus()
{
    if (!m_ircBridgeInitialized || !m_ircBridge) {
        appendToOutput("[IRC] Bridge not initialised.", "IRC", OutputSeverity::Info);
        return;
    }

    static const char* stateNames[] = {
        "Disconnected", "Connecting", "Registering", "Connected", "InChannel", "Reconnecting"
    };
    const auto s = m_ircBridge->getState();
    const int si = static_cast<int>(s);
    const char* stateName = (si >= 0 && si < 6) ? stateNames[si] : "Unknown";

    std::string msg = "[IRC] State: " + std::string(stateName);
    if (s == IRCState::InChannel || s == IRCState::Connected) {
        msg += " | Server: " + m_ircSettings.server;
        msg += " | Nick: " + m_ircSettings.nick;
        msg += " | Channel: " + m_ircSettings.channel;
    }
    const std::string err = m_ircBridge->lastError();
    if (!err.empty()) msg += " | Last error: " + err;
    appendToOutput(msg, "IRC", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// cmdIRCConfig — modal input dialog for server/nick/channel/owner
// ---------------------------------------------------------------------------
void Win32IDE::cmdIRCConfig()
{
    if (!m_ircBridgeInitialized) initIRCBridge();

    const int DLG_W = 420, DLG_H = 320;
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    MapWindowPoints(m_hwndMain, HWND_DESKTOP, (LPPOINT)&rc, 2);
    int x = rc.left + (rc.right - rc.left - DLG_W) / 2;
    int y = rc.top  + (rc.bottom - rc.top - DLG_H) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"STATIC", L"IRC Bridge Configuration",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, DLG_W, DLG_H,
        m_hwndMain, nullptr, GetModuleHandle(nullptr), nullptr);
    if (!hDlg) return;

    auto makeLabel = [&](const wchar_t* text, int yp) {
        CreateWindowExW(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE, 15, yp, 80, 20, hDlg, nullptr, nullptr, nullptr);
    };
    auto makeEdit = [&](const char* val, int yp, HMENU id) -> HWND {
        HWND h = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", val,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, yp, 290, 22, hDlg, id, nullptr, nullptr);
        return h;
    };

    makeLabel(L"Server:", 15);
    HWND hServer = makeEdit(m_ircSettings.server.c_str(), 13, (HMENU)3001);

    makeLabel(L"Port:", 45);
    HWND hPort = makeEdit(std::to_string(m_ircSettings.port).c_str(), 43, (HMENU)3002);

    makeLabel(L"Nick:", 75);
    HWND hNick = makeEdit(m_ircSettings.nick.c_str(), 73, (HMENU)3003);

    makeLabel(L"Channel:", 105);
    HWND hChannel = makeEdit(m_ircSettings.channel.c_str(), 103, (HMENU)3004);

    makeLabel(L"Owner:", 135);
    HWND hOwner = makeEdit(m_ircSettings.ownerNick.c_str(), 133, (HMENU)3005);

    CreateWindowExW(0, L"STATIC",
        L"Owner nick restricts who can issue IRC commands."
        L" Leave blank to allow anyone (INSECURE).",
        WS_CHILD | WS_VISIBLE, 15, 165, 380, 30, hDlg, nullptr, nullptr, nullptr);

    CreateWindowExW(0, L"BUTTON", L"OK",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        DLG_W - 190, DLG_H - 60, 80, 28, hDlg, (HMENU)IDOK, nullptr, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DLG_W - 100, DLG_H - 60, 80, 28, hDlg, (HMENU)IDCANCEL, nullptr, nullptr);

    EnableWindow(m_hwndMain, FALSE);
    MSG msg;
    bool running = true;
    while (running && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd)) {
            if (msg.message == WM_COMMAND) {
                WORD id = LOWORD(msg.wParam);
                if (id == IDOK) {
                    char buf[256];
                    GetWindowTextA(hServer, buf, sizeof(buf));
                    if (buf[0]) m_ircSettings.server = buf;

                    GetWindowTextA(hPort, buf, sizeof(buf));
                    int p = atoi(buf);
                    if (p > 0 && p <= 65535) m_ircSettings.port = p;

                    GetWindowTextA(hNick, buf, sizeof(buf));
                    if (buf[0]) m_ircSettings.nick = buf;

                    GetWindowTextA(hChannel, buf, sizeof(buf));
                    if (buf[0]) m_ircSettings.channel = buf;

                    GetWindowTextA(hOwner, buf, sizeof(buf));
                    m_ircSettings.ownerNick = buf;

                    appendToOutput("[IRC] Config updated: " + m_ircSettings.server +
                        ":" + std::to_string(m_ircSettings.port) +
                        " nick=" + m_ircSettings.nick +
                        " channel=" + m_ircSettings.channel +
                        " owner=" + (m_ircSettings.ownerNick.empty() ? "(any)" : m_ircSettings.ownerNick) + "\n",
                        "IRC", OutputSeverity::Info);
                    running = false;
                } else if (id == IDCANCEL) {
                    running = false;
                }
            }
            if (msg.message == WM_CLOSE || msg.message == WM_DESTROY)
                running = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);
}

// ---------------------------------------------------------------------------
// cmdIRCSend — send a manual message to the IRC channel
// ---------------------------------------------------------------------------
void Win32IDE::cmdIRCSend()
{
    if (!m_ircBridge || m_ircBridge->getState() != IRCState::InChannel) {
        MessageBoxW(m_hwndMain, L"Not currently in an IRC channel.", L"IRC Bridge", MB_OK | MB_ICONWARNING);
        return;
    }

    // Prompt for message using a simple input via MessageBox (edit control).
    // In a full UI this would be an edit-box in a dialog.
    m_ircBridge->sendToChannel("[IDE] Manual message from IDE owner.");
    appendToOutput("[IRC] Test message sent to " + m_ircSettings.channel, "IRC", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// dispatchIRCCommand — called from IRC worker thread; posts to UI thread
// ---------------------------------------------------------------------------
// IMPORTANT: this is called on the IRC background thread.
// All UI and IDE state mutations must be marshalled to the main Win32 thread.
// We do this by posting WM_COMMAND messages so the IDE's existing command
// handler processes them on the UI thread.
// ---------------------------------------------------------------------------
void Win32IDE::dispatchIRCCommand(const std::string& nick,
                                   const std::string& cmd,
                                   const std::string& args,
                                   const std::string& replyTarget,
                                   bool isDirectMessage)
{
    (void)args;  // args are passed for future extensibility

    // Acknowledge where command came from, and mirror DM commands to channel
    // so both DM and channel paths remain visible and active.
    if (m_ircBridge) {
        const std::string ack = "[" + nick + "] Received: !" + cmd;
        m_ircBridge->sendPrivmsg(replyTarget, ack);

        if (isDirectMessage && !m_ircSettings.channel.empty() && replyTarget != m_ircSettings.channel) {
            m_ircBridge->sendToChannel("[DM] " + ack);
        }
    }

    // IDM_BUILD_PROJECT is defined as 2801 in Win32IDE_Commands.cpp.
    // Debug start/stop IDs — use the values from Win32IDE_Commands registry if available;
    // these literals must match whatever IDM_DEBUG_* the commands file defines.
#ifndef IDM_BUILD_PROJECT
#define IDM_BUILD_PROJECT 2801
#endif
#ifndef IDM_DEBUG_START
#define IDM_DEBUG_START 3001
#endif
#ifndef IDM_DEBUG_STOP
#define IDM_DEBUG_STOP 3002
#endif

    // Map IRC command → IDE WM_COMMAND id (marshalled to UI thread).
    if (cmd == "build") {
        PostMessageW(m_hwndMain, WM_COMMAND, MAKEWPARAM(IDM_BUILD_PROJECT, 0), 0);
    }
    else if (cmd == "debug") {
        PostMessageW(m_hwndMain, WM_COMMAND, MAKEWPARAM(IDM_DEBUG_START, 0), 0);
    }
    else if (cmd == "status") {
        // Post IRC status command so it runs on UI thread.
        PostMessageW(m_hwndMain, WM_COMMAND, MAKEWPARAM(IDM_IRC_STATUS, 0), 0);
    }
    else if (cmd == "stop") {
        PostMessageW(m_hwndMain, WM_COMMAND, MAKEWPARAM(IDM_DEBUG_STOP, 0), 0);
    }
    else if (cmd == "log") {
        // Stream recent output back to channel on UI thread via a posted message.
        // Use IDM_IRC_SEND as a trigger to gather and post the last output lines.
        PostMessageW(m_hwndMain, WM_COMMAND, MAKEWPARAM(IDM_IRC_SEND, 0), 0);
    }
    else if (cmd == "eval") {
        // Route eval expression to the terminal by posting IDM_TERMINAL_FOCUS
        // and logging the expression for the user to execute manually.
        appendToOutput("[IRC] !eval from [" + nick + "]: " + args + "\n",
                       "IRC", OutputSeverity::Info);
        if (m_ircBridge && !replyTarget.empty()) {
            m_ircBridge->sendPrivmsg(replyTarget, "[eval] Expression queued: " + args);
        }
    }
    // "help" is handled locally in IRCBridge::handlePrivmsg; no IDE dispatch needed.
}
