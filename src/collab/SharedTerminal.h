// ============================================================================
// SharedTerminal.h — Shared Terminal Multiplexer for Collaboration
// ============================================================================
// Multiplexes terminal I/O across Live Share participants.
// Permission-based input control, output broadcast, history sync.
// Win32-native, no Qt.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <deque>
#include <cstdint>
#include <chrono>
#include <nlohmann/json.hpp>

namespace RawrXD {

class WebSocketHub;
class LiveShareSession;

// ---- Who can type into a shared terminal ----
enum class TerminalAccessLevel : uint8_t {
    None     = 0,   // Can only observe
    Input    = 1,   // Can type commands
    Admin    = 2    // Can type + resize + kill processes
};

// ---- Single shared terminal instance ----
struct SharedTerminalInfo {
    std::string terminalId;
    std::string title;
    std::string shellPath;          // e.g. "cmd.exe", "powershell.exe"
    std::string cwd;
    int cols = 120;
    int rows = 30;
    bool alive = true;
    std::string ownerId;            // User who created it
    TerminalAccessLevel defaultAccess = TerminalAccessLevel::Input;
};

// ---- Terminal output chunk ----
struct TerminalOutputChunk {
    std::string terminalId;
    std::string data;               // Raw terminal bytes (may include ANSI)
    int64_t     timestamp = 0;
};

// ============================================================================
// SharedTerminal — Manages shared terminal sessions
// ============================================================================
class SharedTerminal {
public:
    SharedTerminal();
    ~SharedTerminal();

    SharedTerminal(const SharedTerminal&) = delete;
    SharedTerminal& operator=(const SharedTerminal&) = delete;

    // ---- Lifecycle ----
    void setTransport(WebSocketHub* hub);
    void setSession(LiveShareSession* session);

    // ---- Terminal management ----
    std::string createTerminal(const std::string& ownerId, const std::string& title = "",
                               const std::string& shellPath = "", const std::string& cwd = "");
    bool destroyTerminal(const std::string& terminalId, const std::string& requesterId);
    std::vector<SharedTerminalInfo> listTerminals() const;
    bool terminalExists(const std::string& terminalId) const;

    // ---- I/O ----
    bool writeInput(const std::string& terminalId, const std::string& userId, const std::string& input);
    void feedOutput(const std::string& terminalId, const std::string& data);
    std::string getScrollback(const std::string& terminalId, size_t maxLines = 500) const;

    // ---- Access control ----
    bool setUserAccess(const std::string& terminalId, const std::string& userId, TerminalAccessLevel level);
    TerminalAccessLevel getUserAccess(const std::string& terminalId, const std::string& userId) const;

    // ---- Resize ----
    bool resizeTerminal(const std::string& terminalId, const std::string& userId, int cols, int rows);

    // ---- Sync to new peer ----
    void syncAllTerminals(void* clientCtx);

    // ---- Process messages from the wire ----
    void handleMessage(const std::string& json, void* clientCtx);

    // ---- Callbacks ----
    std::function<void(const std::string& terminalId, const std::string& input)> onLocalInput;   // Hook for actual process stdin
    std::function<void(const SharedTerminalInfo&)> onTerminalCreated;
    std::function<void(const std::string& terminalId)> onTerminalDestroyed;
    std::function<void(const TerminalOutputChunk&)> onOutputReceived;

    // ---- Diagnostics ----
    nlohmann::json getDiagnostics() const;

    static constexpr size_t kMaxScrollbackLines  = 10000;
    static constexpr size_t kMaxInputLength       = 4096;

private:
    struct InternalTerminal {
        SharedTerminalInfo info;
        std::deque<std::string> scrollback;      // Line-based scrollback
        std::unordered_map<std::string, TerminalAccessLevel> userAccess;
    };

    std::string generateTerminalId();
    void broadcastTerminalList();

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::unique_ptr<InternalTerminal>> m_terminals;
    WebSocketHub* m_hub     = nullptr;
    LiveShareSession* m_session = nullptr;
    uint32_t m_nextId       = 1;
};

} // namespace RawrXD
