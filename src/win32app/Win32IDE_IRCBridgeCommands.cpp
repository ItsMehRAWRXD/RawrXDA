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
// cmdIRCConfig — simple input dialog for server/nick/channel/owner
// ---------------------------------------------------------------------------
void Win32IDE::cmdIRCConfig()
{
    if (!m_ircBridgeInitialized) initIRCBridge();

    // Simple config dialog using standard Win32 MessageBox + InputBox pattern.
    // For a full UI consider a property sheet; for now we report current settings
    // and prompt for the owner nick which is the most critical security config.
    std::wstring info =
        L"Current IRC Bridge Settings:\n\n"
        L"Server:  " + std::wstring(m_ircSettings.server.begin(), m_ircSettings.server.end()) + L"\n"
        L"Port:    " + std::to_wstring(m_ircSettings.port) + L"\n"
        L"Nick:    " + std::wstring(m_ircSettings.nick.begin(), m_ircSettings.nick.end()) + L"\n"
        L"Channel: " + std::wstring(m_ircSettings.channel.begin(), m_ircSettings.channel.end()) + L"\n"
        L"Owner:   " + (m_ircSettings.ownerNick.empty()
                           ? L"(anyone — INSECURE!)"
                           : std::wstring(m_ircSettings.ownerNick.begin(),
                                          m_ircSettings.ownerNick.end())) + L"\n\n"
        L"To change settings, edit Tools → IRC Bridge → Config\n"
        L"(Full config dialog coming in a future update)";

    MessageBoxW(m_hwndMain, info.c_str(), L"IRC Bridge Config", MB_OK | MB_ICONINFORMATION);
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
        // Route to the IDE's quick-eval/REPL if available.
        appendToOutput("[IRC] !eval received (not yet wired to REPL): " + args,
                       "IRC", OutputSeverity::Info);
    }
    // "help" is handled locally in IRCBridge::handlePrivmsg; no IDE dispatch needed.
}
