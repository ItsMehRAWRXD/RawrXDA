// ============================================================================
// LiveShareSession.cpp — Live Share Session Implementation
// ============================================================================

#include "LiveShareSession.h"
#include "websocket_hub.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {

// ============================================================================
// Construction / Destruction
// ============================================================================

LiveShareSession::LiveShareSession()
    : m_hub(std::make_unique<WebSocketHub>())
{
}

LiveShareSession::~LiveShareSession() {
    leaveSession();
}

// ============================================================================
// Token generation (random hex, no crypto secrets)
// ============================================================================

std::string LiveShareSession::generateSessionToken() const {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t a = dist(gen), b = dist(gen);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << a << std::setw(16) << b;
    return oss.str();
}

// ============================================================================
// Lifecycle — Host
// ============================================================================

bool LiveShareSession::hostSession(uint16_t port, const std::string& hostName) {
    if (m_state.load() != SessionState::Idle) return false;

    m_isHost = true;
    m_localDisplayName = hostName;
    m_localUserId = "host_" + generateSessionToken().substr(0, 8);
    m_sessionToken = generateSessionToken();

    m_hub->m_onMessageReceived = [this](const std::string& msg, void* ctx) {
        handleIncoming(msg, ctx);
    };
    m_hub->m_onClientChanged = [this](void* ctx, bool connected) {
        if (!connected) {
            std::lock_guard<std::mutex> lk(m_participantsMutex);
            auto it = m_clientToUser.find(ctx);
            if (it != m_clientToUser.end()) {
                std::string uid = it->second;
                Participant p;
                auto pit = m_participants.find(uid);
                if (pit != m_participants.end()) {
                    p = pit->second;
                    m_participants.erase(pit);
                }
                m_clientToUser.erase(it);
                {
                    std::lock_guard<std::mutex> clk(m_cursorsMutex);
                    for (auto& [file, cursors] : m_remoteCursors)
                        cursors.erase(uid);
                }
                if (onParticipantLeft) onParticipantLeft(p);
                broadcastParticipantList();
            }
        }
    };

    if (!m_hub->startServer(port)) {
        m_state.store(SessionState::Error);
        if (onStateChanged) onStateChanged(SessionState::Error);
        return false;
    }

    // Register self as host participant
    {
        std::lock_guard<std::mutex> lk(m_participantsMutex);
        Participant host;
        host.userId = m_localUserId;
        host.displayName = hostName;
        host.permission = CollabPermission::Owner;
        host.cursorColor = 0xFF4488FF;  // Blue
        host.lastHeartbeat = std::chrono::steady_clock::now();
        host.connected = true;
        m_participants[m_localUserId] = host;
    }

    m_state.store(SessionState::Hosting);
    if (onStateChanged) onStateChanged(SessionState::Hosting);
    return true;
}

// ============================================================================
// Lifecycle — Join
// ============================================================================

bool LiveShareSession::joinSession(const std::string& address, uint16_t port, const std::string& userName) {
    if (m_state.load() != SessionState::Idle) return false;

    m_isHost = false;
    m_localDisplayName = userName;
    m_localUserId = "peer_" + generateSessionToken().substr(0, 8);
    m_state.store(SessionState::Joining);
    if (onStateChanged) onStateChanged(SessionState::Joining);

    // Client-side: connect via WebSocket client
    // For the initial implementation, the hub acts as server only.
    // Clients connect using standard WebSocket from their own hub instance or an external client.
    // Here we set up the callback infrastructure so the manager layer can wire the connection.

    m_state.store(SessionState::Connected);
    if (onStateChanged) onStateChanged(SessionState::Connected);
    return true;
}

// ============================================================================
// Lifecycle — Leave
// ============================================================================

void LiveShareSession::leaveSession() {
    SessionState expected = m_state.load();
    if (expected == SessionState::Idle) return;

    m_state.store(SessionState::Disconnecting);
    if (onStateChanged) onStateChanged(SessionState::Disconnecting);

    // Broadcast leave
    nlohmann::json leaveMsg = {
        {"type", "participant_left"},
        {"userId", m_localUserId}
    };
    m_hub->broadcastMessage(leaveMsg.dump());

    m_hub->stopServer();

    {
        std::lock_guard<std::mutex> lk(m_participantsMutex);
        m_participants.clear();
        m_clientToUser.clear();
    }
    {
        std::lock_guard<std::mutex> lk(m_filesMutex);
        m_sharedFiles.clear();
    }
    {
        std::lock_guard<std::mutex> lk(m_cursorsMutex);
        m_remoteCursors.clear();
    }
    {
        std::lock_guard<std::mutex> lk(m_chatMutex);
        m_chatHistory.clear();
    }

    m_sessionToken.clear();
    m_state.store(SessionState::Idle);
    if (onStateChanged) onStateChanged(SessionState::Idle);
}

// ============================================================================
// Session info
// ============================================================================

std::string LiveShareSession::getSessionToken() const {
    return m_sessionToken;
}

std::string LiveShareSession::getStateString() const {
    switch (m_state.load()) {
        case SessionState::Idle:           return "idle";
        case SessionState::Hosting:        return "hosting";
        case SessionState::Joining:        return "joining";
        case SessionState::Connected:      return "connected";
        case SessionState::Disconnecting:  return "disconnecting";
        case SessionState::Error:          return "error";
    }
    return "unknown";
}

// ============================================================================
// Participant management
// ============================================================================

std::vector<Participant> LiveShareSession::getParticipants() const {
    std::lock_guard<std::mutex> lk(m_participantsMutex);
    std::vector<Participant> out;
    out.reserve(m_participants.size());
    for (auto& [_, p] : m_participants)
        out.push_back(p);
    return out;
}

size_t LiveShareSession::getParticipantCount() const {
    std::lock_guard<std::mutex> lk(m_participantsMutex);
    return m_participants.size();
}

bool LiveShareSession::setParticipantPermission(const std::string& userId, CollabPermission perm) {
    if (!m_isHost) return false;
    std::lock_guard<std::mutex> lk(m_participantsMutex);
    auto it = m_participants.find(userId);
    if (it == m_participants.end()) return false;
    it->second.permission = perm;
    broadcastParticipantList();
    return true;
}

bool LiveShareSession::kickParticipant(const std::string& userId) {
    if (!m_isHost) return false;
    if (userId == m_localUserId) return false;

    void* clientCtx = nullptr;
    Participant removed;
    {
        std::lock_guard<std::mutex> lk(m_participantsMutex);
        auto it = m_participants.find(userId);
        if (it == m_participants.end()) return false;
        removed = it->second;
        m_participants.erase(it);

        for (auto& [ctx, uid] : m_clientToUser) {
            if (uid == userId) { clientCtx = ctx; break; }
        }
        if (clientCtx) m_clientToUser.erase(clientCtx);
    }

    // Send kick message then close
    if (clientCtx) {
        nlohmann::json kickMsg = {{"type", "kicked"}, {"reason", "Removed by host"}};
        m_hub->sendTextToClient(clientCtx, kickMsg.dump());
    }

    {
        std::lock_guard<std::mutex> clk(m_cursorsMutex);
        for (auto& [file, cursors] : m_remoteCursors)
            cursors.erase(userId);
    }

    if (onParticipantLeft) onParticipantLeft(removed);
    broadcastParticipantList();
    return true;
}

// ============================================================================
// Shared file operations
// ============================================================================

bool LiveShareSession::openSharedFile(const std::string& relativePath, const std::string& initialContent, const std::string& languageId) {
    std::lock_guard<std::mutex> lk(m_filesMutex);
    if (m_sharedFiles.count(relativePath)) return false;

    auto sf = std::make_unique<SharedFile>();
    sf->relativePath = relativePath;
    sf->languageId = languageId;
    sf->crdt = std::make_unique<CRDTBuffer>();

    // Wire CRDT callbacks
    sf->crdt->textChanged = [this, path = relativePath](const std::string& content) {
        if (onFileChanged) onFileChanged(path, content);
    };
    sf->crdt->operationGenerated = [this, path = relativePath](const std::string& opJson) {
        nlohmann::json msg = {
            {"type", "crdt_op"},
            {"file", path},
            {"operation", opJson}
        };
        m_hub->broadcastMessage(msg.dump());
    };

    // Set initial content via CRDT insert
    if (!initialContent.empty())
        sf->crdt->insertText(0, initialContent);

    m_sharedFiles[relativePath] = std::move(sf);

    // Broadcast file open
    nlohmann::json msg = {
        {"type", "file_opened"},
        {"file", relativePath},
        {"languageId", languageId}
    };
    m_hub->broadcastMessage(msg.dump());
    return true;
}

bool LiveShareSession::closeSharedFile(const std::string& relativePath) {
    std::lock_guard<std::mutex> lk(m_filesMutex);
    auto it = m_sharedFiles.find(relativePath);
    if (it == m_sharedFiles.end()) return false;
    m_sharedFiles.erase(it);

    nlohmann::json msg = {{"type", "file_closed"}, {"file", relativePath}};
    m_hub->broadcastMessage(msg.dump());
    return true;
}

std::string LiveShareSession::getSharedFileContent(const std::string& relativePath) const {
    std::lock_guard<std::mutex> lk(m_filesMutex);
    auto it = m_sharedFiles.find(relativePath);
    if (it == m_sharedFiles.end()) return {};
    return it->second->crdt->getText();
}

std::vector<std::string> LiveShareSession::getSharedFiles() const {
    std::lock_guard<std::mutex> lk(m_filesMutex);
    std::vector<std::string> out;
    out.reserve(m_sharedFiles.size());
    for (auto& [path, _] : m_sharedFiles)
        out.push_back(path);
    return out;
}

bool LiveShareSession::editSharedFile(const std::string& relativePath, int position, const std::string& insertText, int deleteLength) {
    std::lock_guard<std::mutex> lk(m_filesMutex);
    auto it = m_sharedFiles.find(relativePath);
    if (it == m_sharedFiles.end()) return false;

    if (deleteLength > 0)
        it->second->crdt->deleteText(position, deleteLength);
    if (!insertText.empty())
        it->second->crdt->insertText(position, insertText);

    it->second->dirty = true;
    return true;
}

// ============================================================================
// Cursor/presence
// ============================================================================

void LiveShareSession::updateLocalCursor(const std::string& filePath, int line, int column, int selStart, int selEnd) {
    nlohmann::json msg = {
        {"type", "cursor_update"},
        {"userId", m_localUserId},
        {"file", filePath},
        {"line", line},
        {"column", column},
        {"selStart", selStart},
        {"selEnd", selEnd}
    };
    m_hub->broadcastMessage(msg.dump());
}

std::unordered_map<std::string, CursorInfo> LiveShareSession::getRemoteCursors(const std::string& filePath) const {
    std::lock_guard<std::mutex> lk(m_cursorsMutex);
    auto it = m_remoteCursors.find(filePath);
    if (it == m_remoteCursors.end()) return {};
    return it->second;
}

// ============================================================================
// Chat
// ============================================================================

void LiveShareSession::sendChatMessage(const std::string& message) {
    if (message.empty()) return;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    {
        std::lock_guard<std::mutex> lk(m_chatMutex);
        m_chatHistory.push_back({m_localUserId, message, now});
    }

    nlohmann::json msg = {
        {"type", "chat_message"},
        {"userId", m_localUserId},
        {"displayName", m_localDisplayName},
        {"message", message},
        {"timestamp", now}
    };
    m_hub->broadcastMessage(msg.dump());
}

// ============================================================================
// Heartbeat / tick
// ============================================================================

void LiveShareSession::tick() {
    if (!isActive()) return;

    auto now = std::chrono::steady_clock::now();

    // Send heartbeat
    nlohmann::json hb = {
        {"type", "heartbeat"},
        {"userId", m_localUserId},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()}
    };
    m_hub->broadcastMessage(hb.dump());

    // Check for stale peers
    if (m_isHost) {
        std::vector<std::string> staleUsers;
        {
            std::lock_guard<std::mutex> lk(m_participantsMutex);
            for (auto& [uid, p] : m_participants) {
                if (uid == m_localUserId) continue;
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - p.lastHeartbeat).count();
                if (elapsed > kHeartbeatTimeoutSec)
                    staleUsers.push_back(uid);
            }
        }
        for (auto& uid : staleUsers)
            kickParticipant(uid);
    }
}

// ============================================================================
// Incoming message dispatch
// ============================================================================

void LiveShareSession::handleIncoming(const std::string& json, void* clientCtx) {
    nlohmann::json msg;
    try { msg = nlohmann::json::parse(json); }
    catch (...) { return; }

    std::string type = msg.value("type", "");

    if (type == "join_request") {
        std::string userId = msg.value("userId", "");
        std::string displayName = msg.value("displayName", "");
        if (userId.empty()) return;

        Participant p;
        p.userId = userId;
        p.displayName = displayName;
        p.permission = CollabPermission::Editor;  // Default: editor
        p.cursorColor = 0xFF00FF00 + (uint32_t)(m_participants.size() * 0x332211);
        p.lastHeartbeat = std::chrono::steady_clock::now();
        p.connected = true;

        {
            std::lock_guard<std::mutex> lk(m_participantsMutex);
            m_participants[userId] = p;
            m_clientToUser[clientCtx] = userId;
        }

        // Send session state to new peer
        nlohmann::json welcome = {
            {"type", "welcome"},
            {"sessionToken", m_sessionToken},
            {"yourPermission", (int)p.permission}
        };
        sendToClient(clientCtx, welcome);

        // Sync all open files
        {
            std::lock_guard<std::mutex> lk(m_filesMutex);
            for (auto& [path, sf] : m_sharedFiles)
                syncFileToNewPeer(clientCtx, *sf);
        }

        if (onParticipantJoined) onParticipantJoined(p);
        broadcastParticipantList();
    }
    else if (type == "crdt_op") {
        std::string file = msg.value("file", "");
        std::string operation = msg.value("operation", "");
        if (file.empty() || operation.empty()) return;

        // Check permission
        std::string userId;
        {
            std::lock_guard<std::mutex> lk(m_participantsMutex);
            auto it = m_clientToUser.find(clientCtx);
            if (it != m_clientToUser.end()) userId = it->second;
            auto pit = m_participants.find(userId);
            if (pit != m_participants.end() && pit->second.permission == CollabPermission::ReadOnly)
                return;  // No write access
        }

        {
            std::lock_guard<std::mutex> lk(m_filesMutex);
            auto it = m_sharedFiles.find(file);
            if (it != m_sharedFiles.end()) {
                it->second->crdt->applyRemoteOperation(operation);
                it->second->dirty = true;
            }
        }

        // Re-broadcast to other clients
        m_hub->broadcastMessage(json);
    }
    else if (type == "cursor_update") {
        std::string userId = msg.value("userId", "");
        std::string file = msg.value("file", "");
        CursorInfo ci;
        ci.line = msg.value("line", 0);
        ci.column = msg.value("column", 0);
        ci.selStart = msg.value("selStart", -1);
        ci.selEnd = msg.value("selEnd", -1);

        {
            std::lock_guard<std::mutex> lk(m_participantsMutex);
            auto pit = m_participants.find(userId);
            if (pit != m_participants.end()) {
                ci.color = pit->second.cursorColor;
                ci.displayName = pit->second.displayName;
            }
        }

        {
            std::lock_guard<std::mutex> lk(m_cursorsMutex);
            m_remoteCursors[file][userId] = ci;
        }

        if (onCursorMoved) onCursorMoved(file, userId, ci);
        m_hub->broadcastMessage(json);
    }
    else if (type == "heartbeat") {
        std::string userId = msg.value("userId", "");
        std::lock_guard<std::mutex> lk(m_participantsMutex);
        auto it = m_participants.find(userId);
        if (it != m_participants.end())
            it->second.lastHeartbeat = std::chrono::steady_clock::now();
    }
    else if (type == "chat_message") {
        std::string userId = msg.value("userId", "");
        std::string message = msg.value("message", "");
        int64_t ts = msg.value("timestamp", (int64_t)0);

        {
            std::lock_guard<std::mutex> lk(m_chatMutex);
            m_chatHistory.push_back({userId, message, ts});
        }

        if (onChatMessage) onChatMessage(message);
        m_hub->broadcastMessage(json);
    }
}

// ============================================================================
// Internal helpers
// ============================================================================

void LiveShareSession::sendToClient(void* clientCtx, const nlohmann::json& msg) {
    m_hub->sendTextToClient(clientCtx, msg.dump());
}

void LiveShareSession::syncFileToNewPeer(void* clientCtx, const SharedFile& sf) {
    nlohmann::json msg = {
        {"type", "file_sync"},
        {"file", sf.relativePath},
        {"content", sf.crdt->getText()},
        {"languageId", sf.languageId}
    };
    sendToClient(clientCtx, msg);
}

void LiveShareSession::broadcastParticipantList() {
    nlohmann::json arr = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lk(m_participantsMutex);
        for (auto& [_, p] : m_participants) {
            arr.push_back({
                {"userId", p.userId},
                {"displayName", p.displayName},
                {"permission", (int)p.permission},
                {"connected", p.connected}
            });
        }
    }
    nlohmann::json msg = {{"type", "participant_list"}, {"participants", arr}};
    m_hub->broadcastMessage(msg.dump());
}

void LiveShareSession::broadcastState() {
    nlohmann::json msg = {
        {"type", "session_state"},
        {"state", getStateString()},
        {"token", m_sessionToken}
    };
    m_hub->broadcastMessage(msg.dump());
}

// ============================================================================
// Diagnostics
// ============================================================================

nlohmann::json LiveShareSession::getSessionDiagnostics() const {
    nlohmann::json diag;
    diag["state"] = getStateString();
    diag["sessionToken"] = m_sessionToken;
    diag["isHost"] = m_isHost;
    diag["localUserId"] = m_localUserId;
    diag["participantCount"] = getParticipantCount();
    {
        std::lock_guard<std::mutex> lk(m_filesMutex);
        diag["sharedFileCount"] = m_sharedFiles.size();
    }
    {
        std::lock_guard<std::mutex> lk(m_chatMutex);
        diag["chatMessageCount"] = m_chatHistory.size();
    }
    diag["wsPort"] = m_hub ? m_hub->getPort() : 0;
    diag["wsClients"] = m_hub ? m_hub->getClientCount() : 0;
    return diag;
}

} // namespace RawrXD
