#pragma once
/**
 * @file live_share.h
 * @brief Real-time collaborative editing
 * Batch 5 - Item 68: Live share
 */

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace RawrXD::Collaboration {

enum class ParticipantRole {
    Owner,
    Editor,
    Viewer
};

enum class OperationType {
    Insert,
    Delete,
    Replace,
    CursorMove,
    SelectionChange
};

struct Participant {
    std::string id;
    std::string name;
    std::string color;
    ParticipantRole role;
    int cursorLine;
    int cursorColumn;
    int selectionStartLine;
    int selectionStartColumn;
    int selectionEndLine;
    int selectionEndColumn;
    bool isActive;
    std::chrono::system_clock::time_point lastActivity;
};

struct TextOperation {
    std::string id;
    std::string participantId;
    OperationType type;
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    std::string text;
    uint64_t timestamp;
    uint64_t version;
};

struct SessionInfo {
    std::string id;
    std::string name;
    std::string hostId;
    std::vector<Participant> participants;
    bool isReadOnly;
    int maxParticipants;
};

struct TextEdit {
    size_t   start;   // character offset in the file
    size_t   length;  // number of characters to replace
    std::string replacement;
};

struct ConflictInfo {
    std::string   filePath;        // absolute path to the conflicted file
    std::string   baseContent;     // common ancestor version
    std::string   localContent;    // your copy
    std::string   remoteContent;  // the other collaborator's copy
    // optional: a vector of the raw edits that produced localContent and remoteContent
};

class LiveShare {
public:
    LiveShare();
    ~LiveShare();

    // Initialization
    bool initialize();
    void shutdown();

    // Session management
    bool createSession(const std::string& name, bool readOnly = false);
    bool joinSession(const std::string& sessionId);
    void leaveSession();
    bool isInSession() const;
    SessionInfo getSessionInfo() const;

    // Sharing
    std::string getShareLink() const;
    void setSessionReadOnly(bool readOnly);
    void setMaxParticipants(int max);

    // Participants
    std::vector<Participant> getParticipants() const;
    void inviteParticipant(const std::string& email);
    void removeParticipant(const std::string& participantId);
    void setParticipantRole(const std::string& participantId, ParticipantRole role);
    void followParticipant(const std::string& participantId);
    void unfollowParticipant();

    // Operations
    void applyLocalOperation(const TextOperation& operation);
    void sendCursorPosition(int line, int column);
    void sendSelection(int startLine, int startColumn, int endLine, int endColumn);

    // Synchronization
    void requestSync();
    bool isSynced() const;

    // Events
    using ParticipantJoinCallback = std::function<void(const Participant&)>;
    using ParticipantLeaveCallback = std::function<void(const std::string& participantId)>;
    using OperationCallback = std::function<void(const TextOperation&)>;
    using CursorCallback = std::function<void(const std::string& participantId, int line, int column)>;
    void onParticipantJoined(ParticipantJoinCallback callback);
    void onParticipantLeft(ParticipantLeaveCallback callback);
    void onRemoteOperation(OperationCallback callback);
    void onCursorMoved(CursorCallback callback);

private:
    std::string m_sessionId;
    std::string m_participantId;
    SessionInfo m_sessionInfo;
    std::map<std::string, Participant> m_participants;
    bool m_connected{false};
    uint64_t m_version{0};

    ParticipantJoinCallback m_joinCallback;
    ParticipantLeaveCallback m_leaveCallback;
    OperationCallback m_operationCallback;
    CursorCallback m_cursorCallback;

    void connectToSession();
    void disconnectFromSession();
    void handleRemoteOperation(const TextOperation& operation);
    void broadcastOperation(const TextOperation& operation);
};

// Global instance
LiveShare& getLiveShare();

} // namespace RawrXD::Collaboration

// Helper function for computing text edits (needed by AsmMergeBridge)
std::vector<RawrXD::Collaboration::TextEdit> computeEdits(const std::string& from,
                                                      const std::string& to);
