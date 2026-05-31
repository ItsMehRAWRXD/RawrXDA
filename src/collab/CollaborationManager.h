// ============================================================================
// CollaborationManager.h — Top-Level Collaboration Orchestrator
// ============================================================================
// Wires LiveShareSession, SharedTerminal, PairProgrammingAI, CRDT, and
// CursorWidget into a single manager integrated with AgenticIDE.
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <nlohmann/json.hpp>
#include "LiveShareSession.h"
#include "SharedTerminal.h"
#include "PairProgrammingAI.h"
#include "cursor_widget.h"

namespace RawrXD {

// ---- Collaboration subsystem status ----
struct CollabStatus {
    bool active            = false;
    std::string state;                   // idle/hosting/connected/...
    size_t participantCount = 0;
    size_t sharedFileCount  = 0;
    size_t sharedTerminalCount = 0;
    bool pairProgrammingActive = false;
    std::string sessionToken;
    uint16_t port          = 0;
};

// ============================================================================
// CollaborationManager
// ============================================================================
class CollaborationManager {
public:
    CollaborationManager();
    ~CollaborationManager();

    CollaborationManager(const CollaborationManager&) = delete;
    CollaborationManager& operator=(const CollaborationManager&) = delete;

    // ---- Lifecycle ----
    bool initialize();
    void shutdown();

    // ---- Host / Join / Leave ----
    bool hostSession(uint16_t port, const std::string& hostName);
    bool joinSession(const std::string& address, uint16_t port, const std::string& userName);
    void leaveSession();
    bool isActive() const;

    // ---- Session info ----
    CollabStatus getStatus() const;
    std::string getSessionToken() const;

    // ---- File sharing ----
    bool shareFile(const std::string& relativePath, const std::string& initialContent, const std::string& languageId = "");
    bool unshareFile(const std::string& relativePath);
    bool editFile(const std::string& relativePath, int position, const std::string& text, int deleteLen = 0);
    std::string getFileContent(const std::string& relativePath) const;
    std::vector<std::string> getSharedFiles() const;

    // ---- Cursor / Presence ----
    void updateCursor(const std::string& filePath, int line, int column, int selStart = -1, int selEnd = -1);

    // ---- Shared terminals ----
    std::string createSharedTerminal(const std::string& title = "", const std::string& shellPath = "", const std::string& cwd = "");
    bool destroySharedTerminal(const std::string& terminalId);
    bool writeTerminalInput(const std::string& terminalId, const std::string& input);
    void feedTerminalOutput(const std::string& terminalId, const std::string& data);
    std::string getTerminalScrollback(const std::string& terminalId) const;
    std::vector<SharedTerminalInfo> listSharedTerminals() const;

    // ---- Pair programming AI ----
    bool startPairProgramming();
    void stopPairProgramming();
    bool assignPairRole(const std::string& userId, PairRole role);
    void swapPairRoles(const std::string& userA, const std::string& userB);
    void requestAISuggestion(const std::string& forUserId);
    std::vector<PairSuggestion> getAISuggestions(const std::string& forUserId);
    PairSuggestion reviewCode(const std::string& filePath, int line, const std::string& code);
    void updateParticipantContext(const ParticipantContext& ctx);

    // ---- Chat ----
    void sendChat(const std::string& message);

    // ---- Participant management ----
    std::vector<Participant> getParticipants() const;
    bool setPermission(const std::string& userId, CollabPermission perm);
    bool kick(const std::string& userId);

    // ---- LLM inference hook (for pair programming AI) ----
    void setLLMInferCallback(std::function<std::string(const std::string&, const std::string&)> fn);

    // ---- Callbacks (for IDE integration) ----
    std::function<void(const Participant&)>         onParticipantJoined;
    std::function<void(const Participant&)>         onParticipantLeft;
    std::function<void(const std::string&, const std::string&)> onFileChanged;
    std::function<void(const std::string&, const std::string&, const CursorInfo&)> onCursorMoved;
    std::function<void(SessionState)>               onStateChanged;
    std::function<void(const std::string&)>         onChatMessage;
    std::function<void(const PairSuggestion&)>      onAISuggestion;
    std::function<void(const EditConflict&)>        onConflictDetected;
    std::function<void(const SharedTerminalInfo&)>  onTerminalCreated;
    std::function<void(const std::string&)>         onTerminalDestroyed;
    std::function<void(const TerminalOutputChunk&)> onTerminalOutput;
    std::function<void(const std::string& terminalId, const std::string& input)> onTerminalLocalInput;

    // ---- Diagnostics ----
    nlohmann::json getDiagnostics() const;

    // ---- Direct access (for advanced wiring) ----
    LiveShareSession*   getSession()   { return m_session.get(); }
    SharedTerminal*     getTerminals() { return m_terminals.get(); }
    PairProgrammingAI*  getPairAI()    { return m_pairAI.get(); }

private:
    void startTickThread();
    void stopTickThread();
    void tickLoop();
    void wireCallbacks();

    std::unique_ptr<LiveShareSession>   m_session;
    std::unique_ptr<SharedTerminal>     m_terminals;
    std::unique_ptr<PairProgrammingAI>  m_pairAI;
    std::unique_ptr<CursorWidget>       m_cursorWidget;

    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_pairActive{false};
    std::string m_localUserId;

    // Tick thread for heartbeats, conflict detection, etc.
    std::atomic<bool> m_tickRunning{false};
    std::thread m_tickThread;
};

} // namespace RawrXD
