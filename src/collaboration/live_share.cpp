/**
 * @file live_share.cpp
 * @brief Real-time collaborative editing implementation
 * Batch 5 - Item 68: Live share
 */

#include "collaboration/live_share.h"
#include <algorithm>
#include <sstream>
#include <random>

namespace RawrXD::Collaboration {

LiveShare::LiveShare()
    : m_inSession(false)
    , m_isHost(false)
    , m_version(0)
    , m_followParticipantId("") {
}

LiveShare::~LiveShare() {
    shutdown();
}

bool LiveShare::initialize() {
    // Generate unique participant ID
    m_participantId = generateParticipantId();
    return true;
}

void LiveShare::shutdown() {
    if (m_inSession) {
        leaveSession();
    }
}

bool LiveShare::createSession(const std::string& name, bool readOnly) {
    if (m_inSession) {
        return false;
    }
    
    m_sessionInfo.id = generateSessionId();
    m_sessionInfo.name = name;
    m_sessionInfo.hostId = m_participantId;
    m_sessionInfo.isReadOnly = readOnly;
    m_sessionInfo.maxParticipants = 30;
    
    // Add self as host
    Participant host;
    host.id = m_participantId;
    host.name = "Host";
    host.color = generateColor();
    host.role = ParticipantRole::Owner;
    host.cursorLine = 0;
    host.cursorColumn = 0;
    host.isActive = true;
    host.lastActivity = std::chrono::system_clock::now();
    
    m_participants[m_participantId] = host;
    m_sessionInfo.participants.push_back(host);
    
    m_inSession = true;
    m_isHost = true;
    
    if (m_sessionCallback) {
        m_sessionCallback(m_sessionInfo);
    }
    
    return true;
}

bool LiveShare::joinSession(const std::string& sessionId) {
    if (m_inSession) {
        return false;
    }
    
    // Would connect to server and join session
    m_sessionInfo.id = sessionId;
    m_sessionInfo.name = "Joined Session";
    m_sessionInfo.isReadOnly = false;
    
    // Add self as participant
    Participant self;
    self.id = m_participantId;
    self.name = "Guest";
    self.color = generateColor();
    self.role = ParticipantRole::Editor;
    self.cursorLine = 0;
    self.cursorColumn = 0;
    self.isActive = true;
    self.lastActivity = std::chrono::system_clock::now();
    
    m_participants[m_participantId] = self;
    m_sessionInfo.participants.push_back(self);
    
    m_inSession = true;
    m_isHost = false;
    
    if (m_sessionCallback) {
        m_sessionCallback(m_sessionInfo);
    }
    
    return true;
}

void LiveShare::leaveSession() {
    if (!m_inSession) {
        return;
    }
    
    // Notify server
    if (m_isHost) {
        // End session for all
    }
    
    m_inSession = false;
    m_isHost = false;
    m_participants.clear();
    m_pendingOperations.clear();
    m_sessionInfo = SessionInfo{};
    
    if (m_sessionCallback) {
        m_sessionCallback(m_sessionInfo);
    }
}

bool LiveShare::isInSession() const {
    return m_inSession;
}

SessionInfo LiveShare::getSessionInfo() const {
    return m_sessionInfo;
}

std::string LiveShare::getShareLink() const {
    if (!m_inSession) {
        return "";
    }
    
    return "rawrxd://live-share/" + m_sessionInfo.id;
}

void LiveShare::setSessionReadOnly(bool readOnly) {
    if (m_isHost) {
        m_sessionInfo.isReadOnly = readOnly;
    }
}

void LiveShare::setMaxParticipants(int max) {
    if (m_isHost) {
        m_sessionInfo.maxParticipants = max;
    }
}

std::vector<Participant> LiveShare::getParticipants() const {
    std::vector<Participant> result;
    for (const auto& [id, participant] : m_participants) {
        result.push_back(participant);
    }
    return result;
}

void LiveShare::inviteParticipant(const std::string& email) {
    // Would send invitation email
}

void LiveShare::removeParticipant(const std::string& participantId) {
    if (!m_isHost) {
        return;
    }
    
    m_participants.erase(participantId);
    
    // Update session info
    auto it = std::remove_if(m_sessionInfo.participants.begin(), m_sessionInfo.participants.end(),
        [&participantId](const Participant& p) { return p.id == participantId; });
    m_sessionInfo.participants.erase(it, m_sessionInfo.participants.end());
}

void LiveShare::setParticipantRole(const std::string& participantId, ParticipantRole role) {
    if (!m_isHost) {
        return;
    }
    
    auto it = m_participants.find(participantId);
    if (it != m_participants.end()) {
        it->second.role = role;
    }
}

void LiveShare::followParticipant(const std::string& participantId) {
    m_followParticipantId = participantId;
}

void LiveShare::unfollowParticipant() {
    m_followParticipantId.clear();
}

void LiveShare::applyLocalOperation(const TextOperation& operation) {
    if (!m_inSession || m_sessionInfo.isReadOnly) {
        return;
    }
    
    // Apply locally
    applyOperation(operation);
    
    // Send to server
    sendOperation(operation);
}

void LiveShare::sendCursorPosition(int line, int column) {
    if (!m_inSession) {
        return;
    }
    
    auto it = m_participants.find(m_participantId);
    if (it != m_participants.end()) {
        it->second.cursorLine = line;
        it->second.cursorColumn = column;
        it->second.lastActivity = std::chrono::system_clock::now();
    }
    
    // Send to server
    TextOperation op;
    op.id = generateOperationId();
    op.participantId = m_participantId;
    op.type = OperationType::CursorMove;
    op.startLine = line;
    op.startColumn = column;
    op.timestamp = getTimestamp();
    op.version = ++m_version;
    
    sendOperation(op);
}

void LiveShare::sendSelection(int startLine, int startColumn, int endLine, int endColumn) {
    if (!m_inSession) {
        return;
    }
    
    auto it = m_participants.find(m_participantId);
    if (it != m_participants.end()) {
        it->second.selectionStartLine = startLine;
        it->second.selectionStartColumn = startColumn;
        it->second.selectionEndLine = endLine;
        it->second.selectionEndColumn = endColumn;
    }
    
    TextOperation op;
    op.id = generateOperationId();
    op.participantId = m_participantId;
    op.type = OperationType::SelectionChange;
    op.startLine = startLine;
    op.startColumn = startColumn;
    op.endLine = endLine;
    op.endColumn = endColumn;
    op.timestamp = getTimestamp();
    op.version = ++m_version;
    
    sendOperation(op);
}

void LiveShare::onSessionChange(SessionCallback callback) {
    m_sessionCallback = callback;
}

void LiveShare::onParticipantsChange(ParticipantsCallback callback) {
    m_participantsCallback = callback;
}

void LiveShare::onOperationReceived(OperationCallback callback) {
    m_operationCallback = callback;
}

void LiveShare::onCursorChange(CursorCallback callback) {
    m_cursorCallback = callback;
}

void LiveShare::onSelectionChange(SelectionCallback callback) {
    m_selectionCallback = callback;
}

void LiveShare::onError(ErrorCallback callback) {
    m_errorCallback = callback;
}

std::string LiveShare::generateParticipantId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "p_";
    for (int i = 0; i < 16; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string LiveShare::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    for (int i = 0; i < 8; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string LiveShare::generateOperationId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "op_";
    for (int i = 0; i < 12; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string LiveShare::generateColor() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    
    int r = dis(gen);
    int g = dis(gen);
    int b = dis(gen);
    
    std::stringstream ss;
    ss << "#" << std::hex << r << g << b;
    return ss.str();
}

uint64_t LiveShare::getTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void LiveShare::applyOperation(const TextOperation& operation) {
    // Apply operation to document
    // This is where operational transform would be applied
    
    if (m_operationCallback) {
        m_operationCallback(operation);
    }
}

void LiveShare::sendOperation(const TextOperation& operation) {
    // Would send to server via WebSocket
    // For now, just store locally
    m_pendingOperations.push_back(operation);
}

void LiveShare::receiveOperation(const TextOperation& operation) {
    // Transform and apply
    TextOperation transformed = transformOperation(operation);
    applyOperation(transformed);
}

TextOperation LiveShare::transformOperation(const TextOperation& operation) {
    // Simplified operational transform
    // In a real implementation, this would handle concurrent edits
    return operation;
}

} // namespace RawrXD::Collaboration
