// ============================================================================
// LiveShareSession.h — Live Share Session Management
// ============================================================================
// Host/join/leave lifecycle, permission control, participant tracking,
// file state synchronization via CRDT, and real-time presence.
// Win32-native, no Qt.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <cstdint>
#include "crdt_buffer.h"
#include "cursor_widget.h"
#include <nlohmann/json.hpp>

namespace RawrXD {

class WebSocketHub;

// ---- Permission model ----
enum class CollabPermission : uint8_t {
    ReadOnly  = 0,
    Editor    = 1,
    Owner     = 2
};

// ---- Participant descriptor ----
struct Participant {
    std::string userId;
    std::string displayName;
    CollabPermission permission = CollabPermission::ReadOnly;
    uint32_t cursorColor        = 0x00FF00FF;
    std::chrono::steady_clock::time_point lastHeartbeat;
    bool connected              = false;
};

// ---- File being collaboratively edited ----
struct SharedFile {
    std::string relativePath;
    std::unique_ptr<CRDTBuffer> crdt;
    std::string languageId;
    bool dirty = false;
};

// ---- Session state machine ----
enum class SessionState : uint8_t {
    Idle,
    Hosting,
    Joining,
    Connected,
    Disconnecting,
    Error
};

// ============================================================================
// LiveShareSession
// ============================================================================
class LiveShareSession {
public:
    LiveShareSession();
    ~LiveShareSession();

    LiveShareSession(const LiveShareSession&) = delete;
    LiveShareSession& operator=(const LiveShareSession&) = delete;

    // ---- Lifecycle ----
    bool hostSession(uint16_t port, const std::string& hostName);
    bool joinSession(const std::string& address, uint16_t port, const std::string& userName);
    void leaveSession();
    bool isActive() const { return m_state.load() == SessionState::Hosting || m_state.load() == SessionState::Connected; }

    // ---- Session info ----
    std::string getSessionToken() const;
    SessionState getState() const { return m_state.load(); }
    std::string getStateString() const;

    // ---- Participant management ----
    std::vector<Participant> getParticipants() const;
    bool setParticipantPermission(const std::string& userId, CollabPermission perm);
    bool kickParticipant(const std::string& userId);
    size_t getParticipantCount() const;

    // ---- Shared file operations ----
    bool openSharedFile(const std::string& relativePath, const std::string& initialContent, const std::string& languageId = "");
    bool closeSharedFile(const std::string& relativePath);
    std::string getSharedFileContent(const std::string& relativePath) const;
    std::vector<std::string> getSharedFiles() const;
    bool editSharedFile(const std::string& relativePath, int position, const std::string& insertText, int deleteLength = 0);

    // ---- Cursor/presence ----
    void updateLocalCursor(const std::string& filePath, int line, int column, int selStart = -1, int selEnd = -1);
    std::unordered_map<std::string, CursorInfo> getRemoteCursors(const std::string& filePath) const;

    // ---- Heartbeat / connection health ----
    void tick();  // Call periodically (~1 Hz) to process heartbeats and detect stale peers
    static constexpr int kHeartbeatIntervalSec    = 5;
    static constexpr int kHeartbeatTimeoutSec     = 15;

    // ---- Callbacks ----
    std::function<void(const Participant&)>           onParticipantJoined;
    std::function<void(const Participant&)>           onParticipantLeft;
    std::function<void(const std::string& /*file*/, const std::string& /*content*/)> onFileChanged;
    std::function<void(const std::string& /*file*/, const std::string& /*userId*/, const CursorInfo&)> onCursorMoved;
    std::function<void(SessionState)>                 onStateChanged;
    std::function<void(const std::string& /*msg*/)>   onChatMessage;

    // ---- Session chat ----
    void sendChatMessage(const std::string& message);

    // ---- Diagnostics ----
    nlohmann::json getSessionDiagnostics() const;

private:
    // ---- Message dispatch ----
    void handleIncoming(const std::string& json, void* clientCtx);
    void broadcastState();
    void broadcastParticipantList();
    void sendToClient(void* clientCtx, const nlohmann::json& msg);

    // ---- File sync ----
    void syncFileToNewPeer(void* clientCtx, const SharedFile& sf);

    // ---- Session token ----
    std::string generateSessionToken() const;

    // ---- State ----
    std::atomic<SessionState> m_state{SessionState::Idle};
    std::string m_sessionToken;
    std::string m_localUserId;
    std::string m_localDisplayName;
    bool m_isHost = false;

    // ---- Transport ----
    std::unique_ptr<WebSocketHub> m_hub;

    // ---- Participants ----
    mutable std::mutex m_participantsMutex;
    std::unordered_map<std::string, Participant> m_participants;
    std::unordered_map<void*, std::string> m_clientToUser;  // clientCtx -> userId

    // ---- Files ----
    mutable std::mutex m_filesMutex;
    std::unordered_map<std::string, std::unique_ptr<SharedFile>> m_sharedFiles;

    // ---- Cursors ----
    mutable std::mutex m_cursorsMutex;
    // file -> userId -> CursorInfo
    std::unordered_map<std::string, std::unordered_map<std::string, CursorInfo>> m_remoteCursors;

    // ---- Chat ----
    mutable std::mutex m_chatMutex;
    struct ChatEntry { std::string userId; std::string message; int64_t timestamp; };
    std::vector<ChatEntry> m_chatHistory;
};

} // namespace RawrXD
