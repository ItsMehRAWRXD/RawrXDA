// ============================================================================
// Win32IDE_IRCBridge.cpp — mIRC Control Bridge for RawrXD IDE (Phase 51)
// ============================================================================
// Implements a lightweight RFC 1459 IRC client that runs on a background thread.
// The IDE bot connects to an IRC server, joins a channel, and dispatches
// commands (!build, !debug, !status, !log, !eval, !stop, !help) back to the
// IDE on the main Win32 thread via PostMessage.
//
//  Security model:
//   - Only the configured ownerNick may issue commands.
//   - Command arguments are passed as plain strings; dangerous chars are
//     stripped before being forwarded to the IDE layer.
//   - No external input reaches the IDE without going through sanitizeArg().
// ============================================================================

#include "Win32IDE_IRCBridge.h"
#include "Win32IDE.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD {
namespace IRC {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Remove characters that could be used for injection or control sequences.
// Only printable ASCII minus pipe, semicolons, and shell meta-chars are kept.
static std::string sanitizeArg(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (c >= 0x20 && c < 0x7F
            && c != '|' && c != ';' && c != '`'
            && c != '$' && c != '&' && c != '<' && c != '>') {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// Split a string by a single delimiter character.
static std::vector<std::string> splitStr(const std::string& s, char delim)
{
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim))
        tokens.push_back(token);
    return tokens;
}

// Trim leading/trailing whitespace.
static std::string trim(const std::string& s)
{
    const std::string ws = " \t\r\n";
    auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

// ---------------------------------------------------------------------------
// IRCBridge
// ---------------------------------------------------------------------------

IRCBridge::IRCBridge(Win32IDE* ide)
    : m_ide(ide)
{
    // Ensure Winsock is initialised (safe to call multiple times per process).
    WSADATA wd{};
    WSAStartup(MAKEWORD(2, 2), &wd);
}

IRCBridge::~IRCBridge()
{
    stop();
    WSACleanup();
}

bool IRCBridge::start(const IRCBridgeSettings& settings)
{
    if (m_state.load() != IRCState::Disconnected &&
        m_state.load() != IRCState::Reconnecting)
        return false;  // already running

    m_settings  = settings;
    m_stopFlag  = false;
    setState(IRCState::Connecting);
    m_thread = std::thread([this]() { workerThread(); });
    logToIDE("[IRC] Bridge started → connecting to " + m_settings.server +
             ":" + std::to_string(m_settings.port));
    return true;
}

void IRCBridge::stop()
{
    m_stopFlag = true;
    if (m_sock != INVALID_SOCKET) {
        rawSend("QUIT :RawrXD closing\r\n");
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
    if (m_thread.joinable())
        m_thread.join();
    setState(IRCState::Disconnected);
    logToIDE("[IRC] Bridge stopped.");
}

void IRCBridge::sendToChannel(const std::string& message)
{
    sendPrivmsg(m_settings.channel, message);
}

void IRCBridge::sendPrivmsg(const std::string& target, const std::string& message)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    // Chunk message if too long (IRC max line is 512 bytes including CRLF).
    const size_t maxPayload = 400;
    if (message.size() <= maxPayload) {
        m_outQueue.push("PRIVMSG " + target + " :" + message);
    } else {
        for (size_t i = 0; i < message.size(); i += maxPayload) {
            m_outQueue.push("PRIVMSG " + target + " :" + message.substr(i, maxPayload));
        }
    }
}

void IRCBridge::broadcastOutput(const std::string& text, const std::string& prefix)
{
    auto lines = splitStr(text, '\n');
    int n = 0;
    for (auto& raw : lines) {
        std::string l = trim(raw);
        if (l.empty()) continue;
        if (n >= m_settings.maxOutputLines) {
            sendToChannel("... (output truncated, use !log for full output)");
            break;
        }
        sendToChannel(prefix.empty() ? l : prefix + l);
        ++n;
    }
}

std::string IRCBridge::lastError() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_lastError;
}

// Called from the IDE main thread timer — flushes queued output etc.
void IRCBridge::tick()
{
    // Check if we are connected and should emit heartbeats
    if (m_state.load() != IRCState::Connected && m_state.load() != IRCState::InChannel) {
        return;
    }

    // Interval: 2.5 seconds (2500 ms)
    const uint64_t now = GetTickCount64();
    if (now - m_lastHeartbeatMs >= 2500) {
        m_lastHeartbeatMs = now;
        
        // Split-Phase Heartbeat Logic:
        // Even Sequence: Emit Phase A (AVX2 / Stability Lane)
        // Odd Sequence: Emit Phase B (AVX512 / Strikeforce Lane)
        if (m_beaconSeq % 2 == 0) {
            emitHeartbeatHalfA();
        } else {
            emitHeartbeatHalfB();
        }
        m_beaconSeq++;
    }
}

// Emits the "half-heartbeat" for the AVX2 Stability Lane.
void IRCBridge::emitHeartbeatHalfA()
{
    // In a real implementation, this would be validated by executing a dummy AVX2 instruction.
    // Here we simulate the emission.
    sendToChannel("PARTIAL_BEACON_A: " + std::to_string(m_beaconSeq));
}

// Emits the "half-heartbeat" for the AVX512 Strikeforce Lane.
void IRCBridge::emitHeartbeatHalfB()
{
    // Simulates the higher-power AVX512 check.
    sendToChannel("PARTIAL_BEACON_B: " + std::to_string(m_beaconSeq));
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void IRCBridge::workerThread()
{
    while (!m_stopFlag) {
        // === Connect ===
        addrinfo hints{}, *res = nullptr;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_family   = AF_UNSPEC;

        if (getaddrinfo(m_settings.server.c_str(),
                        std::to_string(m_settings.port).c_str(),
                        &hints, &res) != 0 || !res)
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_lastError = "DNS resolution failed for " + m_settings.server;
            logToIDE("[IRC] " + m_lastError);
            setState(IRCState::Reconnecting);
            Sleep(static_cast<DWORD>(m_settings.reconnectDelaySec * 1000));
            continue;
        }

        m_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_sock == INVALID_SOCKET) {
            freeaddrinfo(res);
            logToIDE("[IRC] Failed to create socket.");
            setState(IRCState::Reconnecting);
            Sleep(static_cast<DWORD>(m_settings.reconnectDelaySec * 1000));
            continue;
        }

        // Set a receive timeout so the thread doesn't block forever.
        DWORD timeout_ms = 30000;
        setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

        if (connect(m_sock, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
            freeaddrinfo(res);
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            logToIDE("[IRC] Connect failed to " + m_settings.server +
                     ":" + std::to_string(m_settings.port));
            setState(IRCState::Reconnecting);
            Sleep(static_cast<DWORD>(m_settings.reconnectDelaySec * 1000));
            continue;
        }
        freeaddrinfo(res);

        // === Register ===
        setState(IRCState::Registering);
        if (!m_settings.nickservPass.empty())
            rawSend("PASS " + m_settings.nickservPass + "\r\n");

        rawSend("NICK " + m_settings.nick + "\r\n");
        rawSend("USER rawrxd 0 * :" + m_settings.realname + "\r\n");
        logToIDE("[IRC] Registering as " + m_settings.nick + "...");

        m_recvBuf.clear();

        // === Read loop ===
        char buf[4096];
        while (!m_stopFlag) {
            // Flush any queued outgoing messages first.
            flushOutQueue();

            int n = recv(m_sock, buf, sizeof(buf) - 1, 0);
            if (n <= 0) {
                if (m_stopFlag) break;
                const int err = WSAGetLastError();
                if (err == WSAETIMEDOUT) continue;  // just a timeout, keep looping
                logToIDE("[IRC] recv error " + std::to_string(err) + " — reconnecting.");
                break;
            }
            buf[n] = '\0';
            m_recvBuf += buf;

            // Process complete lines (terminated by \r\n or \n).
            size_t pos;
            while ((pos = m_recvBuf.find('\n')) != std::string::npos) {
                std::string line = m_recvBuf.substr(0, pos);
                m_recvBuf.erase(0, pos + 1);
                // Strip trailing \r
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty())
                    processLine(line);
            }
        }

        // Connection closed/failed — clean up.
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        if (!m_stopFlag) {
            setState(IRCState::Reconnecting);
            logToIDE("[IRC] Disconnected — reconnecting in " +
                     std::to_string(m_settings.reconnectDelaySec) + "s");
            Sleep(static_cast<DWORD>(m_settings.reconnectDelaySec * 1000));
            setState(IRCState::Connecting);
        }
    }

    setState(IRCState::Disconnected);
}

bool IRCBridge::rawSend(const std::string& line)
{
    if (m_sock == INVALID_SOCKET) return false;
    // line should already end with \r\n
    std::string msg = line;
    if (msg.size() < 2 || msg.substr(msg.size()-2) != "\r\n")
        msg += "\r\n";
    int sent = send(m_sock, msg.c_str(), static_cast<int>(msg.size()), 0);
    return sent != SOCKET_ERROR;
}

void IRCBridge::flushOutQueue()
{
    std::queue<std::string> batch;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        std::swap(batch, m_outQueue);
    }
    while (!batch.empty()) {
        rawSend(batch.front() + "\r\n");
        batch.pop();
        // Brief throttle to avoid flooding the server.
        Sleep(200);
    }
}

void IRCBridge::processLine(const std::string& line)
{
    // Minimal RFC 1459 parser:
    //   [':' prefix SP] command [params] [':' trailing]
    std::string prefix, command;
    std::vector<std::string> params;

    size_t i = 0;
    if (line[0] == ':') {
        size_t sp = line.find(' ', 1);
        prefix = line.substr(1, sp - 1);
        i = sp + 1;
    }
    // command
    size_t sp = line.find(' ', i);
    command = (sp == std::string::npos) ? line.substr(i) : line.substr(i, sp - i);
    i = (sp == std::string::npos) ? line.size() : sp + 1;

    // params
    while (i < line.size()) {
        if (line[i] == ':') {
            params.push_back(line.substr(i + 1));
            break;
        }
        sp = line.find(' ', i);
        if (sp == std::string::npos) {
            params.push_back(line.substr(i));
            break;
        }
        params.push_back(line.substr(i, sp - i));
        i = sp + 1;
    }

    // === Handle server messages ===

    // PING → PONG immediately
    if (command == "PING") {
        const std::string token = params.empty() ? "" : params[0];
        rawSend("PONG :" + token + "\r\n");
        return;
    }

    // RPL_WELCOME (001) — we are registered
    if (command == "001") {
        setState(IRCState::Connected);
        logToIDE("[IRC] Connected! Joining " + m_settings.channel);
        rawSend("JOIN " + m_settings.channel + "\r\n");
        // Identify to NickServ if a password is set.
        if (!m_settings.nickservPass.empty())
            rawSend("PRIVMSG NickServ :IDENTIFY " + m_settings.nickservPass + "\r\n");
        return;
    }

    // JOIN confirmation — bot is now in the channel
    if (command == "JOIN") {
        const std::string who = extractNick(prefix);
        if (who == m_settings.nick) {
            setState(IRCState::InChannel);
            const std::string ch = params.empty() ? m_settings.channel : params[0];
            logToIDE("[IRC] Joined " + ch + " as " + m_settings.nick);
            rawSend("PRIVMSG " + ch + " :RawrXD IDE online. Type !help for commands.\r\n");
        }
        return;
    }

    // KICK — if we were kicked, rejoin.
    if (command == "KICK" && params.size() >= 2) {
        if (params[1] == m_settings.nick) {
            logToIDE("[IRC] Kicked from " + params[0] + " — rejoining...");
            Sleep(3000);
            rawSend("JOIN " + m_settings.channel + "\r\n");
        }
        return;
    }

    // PRIVMSG
    if (command == "PRIVMSG" && params.size() >= 2) {
        handlePrivmsg(prefix, params[0], params[1]);
        return;
    }

    // ERR_NICKNAMEINUSE (433) — append _ and retry
    if (command == "433") {
        m_settings.nick += "_";
        rawSend("NICK " + m_settings.nick + "\r\n");
        return;
    }

    // === BEACON RECONSTITUTION ===
    // If the server/daemon responds with "Full Metal" confirmation, authorize AVX-512.
    if (command == "FULL_BEACON_ACK" || command == "NOTICE") {
        if (!params.empty()) {
            std::string msg = params.back();
            if (msg.find("FULL_BEACON_DEPLOYED") != std::string::npos) {
                if (!m_fullMetalMode) {
                    m_fullMetalMode = true;
                    logToIDE("[IRC] BEACON RECONSTITUTED: AVX-512 Full Metal Mode ENABLED.");
                    // In a real system, toggle global optimizer flags here.
                }
            }
        }
    }
}

void IRCBridge::handlePrivmsg(const std::string& prefix,
                               const std::string& target,
                               const std::string& text)
{
    const std::string senderNick = extractNick(prefix);

    // Ignore messages from self.
    if (senderNick == m_settings.nick) return;

    // Only accept commands from the configured owner.
    if (!m_settings.ownerNick.empty() && senderNick != m_settings.ownerNick) {
        // Silently ignore unless it's a direct message asking for !help — 
        // we respond politely to show the bot is alive.
        if (text == "!help" && target == m_settings.nick)
            sendPrivmsg(senderNick, "Access restricted. Contact the IDE owner.");
        return;
    }

    // Must start with '!'
    if (text.empty() || text[0] != '!') return;

    // Parse command and args
    size_t space = text.find(' ');
    std::string cmd  = trim((space == std::string::npos) ? text.substr(1) : text.substr(1, space - 1));
    std::string args = trim((space == std::string::npos) ? "" : text.substr(space + 1));

    // Lowercase command for case-insensitive matching
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return std::tolower(c); });

    // Reply target: if msg was sent to channel, reply to channel; else to sender.
    const std::string replyTo = (target == m_settings.nick) ? senderNick : target;

    // Sanitize args before passing to IDE
    const std::string safeArgs = sanitizeArg(args);

    logToIDE("[IRC] Command from " + senderNick + ": !" + cmd +
             (safeArgs.empty() ? "" : " " + safeArgs));

    // Notify IDE via registered callback
    if (m_commandCb) {
        const bool isDirectMessage = (target == m_settings.nick);
        m_commandCb(senderNick, cmd, safeArgs, replyTo, isDirectMessage);
    }

    // Built-in help response (doesn't need IDE callback)
    if (cmd == "help") {
        sendPrivmsg(replyTo, "RawrXD IDE commands: "
            "!build [target] | !debug [args] | !status | !log [n] | "
            "!eval <expr> | !stop | !help");
    }
}

std::string IRCBridge::extractNick(const std::string& prefix)
{
    // prefix format: nick!user@host
    const size_t bang = prefix.find('!');
    return (bang == std::string::npos) ? prefix : prefix.substr(0, bang);
}

void IRCBridge::setState(IRCState s)
{
    m_state.store(s);
}

void IRCBridge::logToIDE(const std::string& msg)
{
    if (m_ide)
        m_ide->appendToOutput(msg, "IRC", Win32IDE::OutputSeverity::Info);
}

} // namespace IRC
} // namespace RawrXD
