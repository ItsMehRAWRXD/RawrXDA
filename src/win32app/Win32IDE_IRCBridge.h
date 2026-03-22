#pragma once
// ============================================================================
// Win32IDE_IRCBridge.h — mIRC Control Bridge for RawrXD IDE
// ============================================================================
// Connects the IDE to an IRC server so the user can:
//   - Authenticate with a username that maps to an IRC nick
//   - Have the IDE bot join and idle in a configured channel
//   - Issue commands from IRC to control the IDE remotely:
//       !build [target]        — trigger a CMake build
//       !debug [args]          — start the debugger
//       !status                — report IDE status
//       !eval <expr>           — evaluate a quick expression
//       !log [n]               — stream last n lines of build/run log
//       !stop                  — stop the running build/debug
//       !help                  — list available commands
//   - Receive streamed build/debug output back as PRIVMSG chunks
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <vector>

// Forward declaration
class Win32IDE;

namespace RawrXD {
namespace IRC {

// Settings for the IRC Bridge
struct IRCBridgeSettings {
    std::string server   = "irc.libera.chat";
    int         port     = 6667;
    std::string nick     = "RawrXD-IDE";
    std::string realname = "RawrXD IDE Bot";
    std::string channel  = "#rawrxd-ide";
    std::string nickservPass;        // Optional: NickServ password
    std::string ownerNick;           // Only this nick can issue commands
    bool        useTLS   = false;    // TLS support placeholder
    int         reconnectDelaySec = 15;
    int         maxOutputLines    = 10;  // Max PRIVMSG lines per command output
};

// Status of the IRC bridge connection
enum class IRCState {
    Disconnected,
    Connecting,
    Registering,   // Sent NICK/USER, waiting for 001
    Connected,     // Received 001 (RPL_WELCOME)
    InChannel,     // Successfully joined the channel
    Reconnecting
};

// IRC Bridge class — manages the TCP connection, login, channel join,
// command parsing, and output streaming back to IRC.
class IRCBridge {
public:
    explicit IRCBridge(Win32IDE* ide);
    ~IRCBridge();

    // Connect to the IRC server and join channel.
    // Non-blocking: starts a background thread.
    bool start(const IRCBridgeSettings& settings);

    // Gracefully disconnect and stop the background thread.
    void stop();

    // Send a message to the configured channel.
    void sendToChannel(const std::string& message);

    // Send a PRIVMSG to a specific target (nick or channel).
    void sendPrivmsg(const std::string& target, const std::string& message);

    // Broadcast multi-line output to channel, chunked by maxOutputLines.
    void broadcastOutput(const std::string& text, const std::string& prefix = "");

    IRCState getState() const { return m_state.load(); }
    const IRCBridgeSettings& settings() const { return m_settings; }

    // Returns the last error description (empty if none).
    std::string lastError() const;

    // Callback invoked on IDE thread when a command arrives from IRC.
    using CommandCallback = std::function<void(const std::string& nick,
                                               const std::string& command,
                                               const std::string& args,
                                               const std::string& replyTarget,
                                               bool isDirectMessage)>;
    void setCommandCallback(CommandCallback cb) { m_commandCb = std::move(cb); }

    // Timer tick — call periodically from the IDE's message loop to flush
    // queued output and handle auto-reconnect.
    void tick();

    // Check if the system has been authorized for Full Metal AVX-512 mode.
    bool isFullMetalMode() const { return m_fullMetalMode; }

private:
    Win32IDE*           m_ide = nullptr;
    IRCBridgeSettings   m_settings;
    std::atomic<IRCState> m_state{IRCState::Disconnected};
    SOCKET              m_sock = INVALID_SOCKET;

    std::thread         m_thread;
    std::atomic<bool>   m_stopFlag{false};

    mutable std::mutex  m_mutex;
    std::string         m_lastError;
    std::queue<std::string> m_outQueue;   // outgoing messages (thread-safe via m_mutex)

    CommandCallback     m_commandCb;

    // Beacon / Heartbeat State
    bool     m_fullMetalMode = false;
    uint64_t m_lastHeartbeatMs = 0;
    uint32_t m_beaconSeq = 0;

    void emitHeartbeatHalfA(); // Stability Lane (AVX2)
    void emitHeartbeatHalfB(); // Strikeforce Lane (AVX512)

    // Partial receive buffer
    std::string         m_recvBuf;

    // Background worker
    void workerThread();

    // Low-level send (called from worker thread only)
    bool rawSend(const std::string& line);

    // Process a single complete IRC line (without \r\n)
    void processLine(const std::string& line);

    // Parse and dispatch a PRIVMSG command
    void handlePrivmsg(const std::string& prefix, const std::string& target, const std::string& text);

    // Build the nick!user@host prefix extractor
    static std::string extractNick(const std::string& prefix);

    // Flush queued outgoing messages (called from worker)
    void flushOutQueue();

    void setState(IRCState s);
    void logToIDE(const std::string& msg);
};

} // namespace IRC
} // namespace RawrXD
