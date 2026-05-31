// ============================================================================
// CollaborationManager.cpp — Collaboration Orchestrator Implementation
// ============================================================================

#include "CollaborationManager.h"
#include <chrono>
#include <thread>

namespace RawrXD {

// ============================================================================
// Construction / Destruction
// ============================================================================

CollaborationManager::CollaborationManager()
    : m_session(std::make_unique<LiveShareSession>())
    , m_terminals(std::make_unique<SharedTerminal>())
    , m_pairAI(std::make_unique<PairProgrammingAI>())
    , m_cursorWidget(std::make_unique<CursorWidget>())
{
}

CollaborationManager::~CollaborationManager() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool CollaborationManager::initialize() {
    if (m_initialized.load()) return true;

    wireCallbacks();
    m_initialized.store(true);
    return true;
}

void CollaborationManager::shutdown() {
    stopTickThread();
    stopPairProgramming();
    leaveSession();
    m_initialized.store(false);
}

// ============================================================================
// Callback wiring
// ============================================================================

void CollaborationManager::wireCallbacks() {
    // LiveShareSession -> Manager callbacks
    m_session->onParticipantJoined = [this](const Participant& p) {
        // Sync terminals to new peer
        // (clientCtx would need to be retrieved; simplified for the callback model)
        if (onParticipantJoined) onParticipantJoined(p);
    };
    m_session->onParticipantLeft = [this](const Participant& p) {
        if (onParticipantLeft) onParticipantLeft(p);
    };
    m_session->onFileChanged = [this](const std::string& file, const std::string& content) {
        if (onFileChanged) onFileChanged(file, content);
    };
    m_session->onCursorMoved = [this](const std::string& file, const std::string& userId, const CursorInfo& ci) {
        m_cursorWidget->updateCursor(userId, ci);
        if (onCursorMoved) onCursorMoved(file, userId, ci);

        // Feed to pair programming AI context
        if (m_pairActive.load()) {
            ParticipantContext ctx;
            ctx.userId = userId;
            ctx.activeFile = file;
            ctx.cursorLine = ci.line;
            ctx.cursorColumn = ci.column;
            ctx.lastActivity = std::chrono::steady_clock::now();
            m_pairAI->updateParticipantContext(ctx);
            m_pairAI->recordNavigation(userId, file, ci.line);
        }
    };
    m_session->onStateChanged = [this](SessionState state) {
        if (state == SessionState::Idle) {
            stopTickThread();
            stopPairProgramming();
        }
        if (onStateChanged) onStateChanged(state);
    };
    m_session->onChatMessage = [this](const std::string& msg) {
        if (onChatMessage) onChatMessage(msg);
    };

    // SharedTerminal -> Manager callbacks
    m_terminals->onTerminalCreated = [this](const SharedTerminalInfo& info) {
        if (onTerminalCreated) onTerminalCreated(info);
    };
    m_terminals->onTerminalDestroyed = [this](const std::string& id) {
        if (onTerminalDestroyed) onTerminalDestroyed(id);
    };
    m_terminals->onOutputReceived = [this](const TerminalOutputChunk& chunk) {
        if (onTerminalOutput) onTerminalOutput(chunk);
    };
    m_terminals->onLocalInput = [this](const std::string& termId, const std::string& input) {
        if (onTerminalLocalInput) onTerminalLocalInput(termId, input);
    };

    // PairProgrammingAI -> Manager callbacks
    m_pairAI->onSuggestionGenerated = [this](const PairSuggestion& s) {
        if (onAISuggestion) onAISuggestion(s);
    };
    m_pairAI->onConflictDetected = [this](const EditConflict& c) {
        if (onConflictDetected) onConflictDetected(c);
    };
}

// ============================================================================
// Host / Join / Leave
// ============================================================================

bool CollaborationManager::hostSession(uint16_t port, const std::string& hostName) {
    if (!m_initialized.load()) initialize();

    if (!m_session->hostSession(port, hostName)) return false;

    m_localUserId = hostName;
    m_terminals->setTransport(nullptr);  // Will use same hub via session
    m_terminals->setSession(m_session.get());
    m_pairAI->setSession(m_session.get());

    startTickThread();
    return true;
}

bool CollaborationManager::joinSession(const std::string& address, uint16_t port, const std::string& userName) {
    if (!m_initialized.load()) initialize();

    if (!m_session->joinSession(address, port, userName)) return false;

    m_localUserId = userName;
    m_terminals->setSession(m_session.get());
    m_pairAI->setSession(m_session.get());

    startTickThread();
    return true;
}

void CollaborationManager::leaveSession() {
    stopTickThread();
    m_session->leaveSession();
}

bool CollaborationManager::isActive() const {
    return m_session->isActive();
}

// ============================================================================
// Session info
// ============================================================================

CollabStatus CollaborationManager::getStatus() const {
    CollabStatus s;
    s.active = m_session->isActive();
    s.state = m_session->getStateString();
    s.participantCount = m_session->getParticipantCount();
    s.sharedFileCount = m_session->getSharedFiles().size();
    s.sharedTerminalCount = m_terminals->listTerminals().size();
    s.pairProgrammingActive = m_pairActive.load();
    s.sessionToken = m_session->getSessionToken();
    return s;
}

std::string CollaborationManager::getSessionToken() const {
    return m_session->getSessionToken();
}

// ============================================================================
// File sharing
// ============================================================================

bool CollaborationManager::shareFile(const std::string& relativePath, const std::string& initialContent, const std::string& languageId) {
    return m_session->openSharedFile(relativePath, initialContent, languageId);
}

bool CollaborationManager::unshareFile(const std::string& relativePath) {
    return m_session->closeSharedFile(relativePath);
}

bool CollaborationManager::editFile(const std::string& relativePath, int position, const std::string& text, int deleteLen) {
    bool ok = m_session->editSharedFile(relativePath, position, text, deleteLen);
    if (ok && m_pairActive.load()) {
        // Record edit activity for pair programming AI
        m_pairAI->recordEdit(m_localUserId, relativePath, position);
    }
    return ok;
}

std::string CollaborationManager::getFileContent(const std::string& relativePath) const {
    return m_session->getSharedFileContent(relativePath);
}

std::vector<std::string> CollaborationManager::getSharedFiles() const {
    return m_session->getSharedFiles();
}

// ============================================================================
// Cursor / Presence
// ============================================================================

void CollaborationManager::updateCursor(const std::string& filePath, int line, int column, int selStart, int selEnd) {
    m_session->updateLocalCursor(filePath, line, column, selStart, selEnd);
}

// ============================================================================
// Shared terminals
// ============================================================================

std::string CollaborationManager::createSharedTerminal(const std::string& title, const std::string& shellPath, const std::string& cwd) {
    return m_terminals->createTerminal(m_localUserId, title, shellPath, cwd);
}

bool CollaborationManager::destroySharedTerminal(const std::string& terminalId) {
    return m_terminals->destroyTerminal(terminalId, m_localUserId);
}

bool CollaborationManager::writeTerminalInput(const std::string& terminalId, const std::string& input) {
    return m_terminals->writeInput(terminalId, m_localUserId, input);
}

void CollaborationManager::feedTerminalOutput(const std::string& terminalId, const std::string& data) {
    m_terminals->feedOutput(terminalId, data);
}

std::string CollaborationManager::getTerminalScrollback(const std::string& terminalId) const {
    return m_terminals->getScrollback(terminalId);
}

std::vector<SharedTerminalInfo> CollaborationManager::listSharedTerminals() const {
    return m_terminals->listTerminals();
}

// ============================================================================
// Pair programming AI
// ============================================================================

bool CollaborationManager::startPairProgramming() {
    if (!m_session->isActive()) return false;
    m_pairActive.store(true);

    // Auto-assign roles based on activity
    auto participants = m_session->getParticipants();
    if (participants.size() >= 2) {
        m_pairAI->assignRole(participants[0].userId, PairRole::Driver);
        for (size_t i = 1; i < participants.size(); ++i)
            m_pairAI->assignRole(participants[i].userId, PairRole::Navigator);
    } else if (!participants.empty()) {
        m_pairAI->assignRole(participants[0].userId, PairRole::Driver);
    }
    return true;
}

void CollaborationManager::stopPairProgramming() {
    m_pairActive.store(false);
}

bool CollaborationManager::assignPairRole(const std::string& userId, PairRole role) {
    return m_pairAI->assignRole(userId, role);
}

void CollaborationManager::swapPairRoles(const std::string& userA, const std::string& userB) {
    m_pairAI->swapRoles(userA, userB);
}

void CollaborationManager::requestAISuggestion(const std::string& forUserId) {
    m_pairAI->requestSuggestion(forUserId);
}

std::vector<PairSuggestion> CollaborationManager::getAISuggestions(const std::string& forUserId) {
    return m_pairAI->getPendingSuggestions(forUserId);
}

PairSuggestion CollaborationManager::reviewCode(const std::string& filePath, int line, const std::string& code) {
    return m_pairAI->reviewCodeAtCursor(filePath, line, code);
}

void CollaborationManager::updateParticipantContext(const ParticipantContext& ctx) {
    m_pairAI->updateParticipantContext(ctx);
}

// ============================================================================
// Chat
// ============================================================================

void CollaborationManager::sendChat(const std::string& message) {
    m_session->sendChatMessage(message);
}

// ============================================================================
// Participant management
// ============================================================================

std::vector<Participant> CollaborationManager::getParticipants() const {
    return m_session->getParticipants();
}

bool CollaborationManager::setPermission(const std::string& userId, CollabPermission perm) {
    return m_session->setParticipantPermission(userId, perm);
}

bool CollaborationManager::kick(const std::string& userId) {
    return m_session->kickParticipant(userId);
}

// ============================================================================
// LLM hook
// ============================================================================

void CollaborationManager::setLLMInferCallback(std::function<std::string(const std::string&, const std::string&)> fn) {
    m_pairAI->llmInfer = std::move(fn);
}

// ============================================================================
// Tick thread
// ============================================================================

void CollaborationManager::startTickThread() {
    if (m_tickRunning.load()) return;
    m_tickRunning.store(true);
    m_tickThread = std::thread(&CollaborationManager::tickLoop, this);
}

void CollaborationManager::stopTickThread() {
    m_tickRunning.store(false);
    if (m_tickThread.joinable())
        m_tickThread.join();
}

void CollaborationManager::tickLoop() {
    while (m_tickRunning.load()) {
        m_session->tick();
        if (m_pairActive.load())
            m_pairAI->tick();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// ============================================================================
// Diagnostics
// ============================================================================

nlohmann::json CollaborationManager::getDiagnostics() const {
    nlohmann::json diag;
    diag["initialized"] = m_initialized.load();
    diag["session"] = m_session->getSessionDiagnostics();
    diag["terminals"] = m_terminals->getDiagnostics();
    diag["pairProgramming"] = m_pairAI->getDiagnostics();
    diag["pairActive"] = m_pairActive.load();
    diag["localUserId"] = m_localUserId;
    return diag;
}

} // namespace RawrXD
