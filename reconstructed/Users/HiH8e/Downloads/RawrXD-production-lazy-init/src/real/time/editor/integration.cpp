#include "real_time_editor_integration.hpp"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>

// EditorSession Implementation

EditorSession::EditorSession(
    int sessionId,
    const QString& filePath,
    QObject* parent)
    : QObject(parent),
      m_sessionId(sessionId),
      m_filePath(filePath),
      m_cursorPosition(0),
      m_createdTime(QDateTime::currentDateTime()) {
    
    m_fileType = detectFileType(filePath);
    qDebug() << "[EditorSession" << sessionId << "] Created for file:" << filePath;
}

EditorSession::~EditorSession() {
    qDebug() << "[EditorSession" << m_sessionId << "] Destroyed";
}

QString EditorSession::getFilePath() const {
    QMutexLocker lock(&m_mutex);
    return m_filePath;
}

QString EditorSession::getContent() const {
    QMutexLocker lock(&m_mutex);
    return m_content;
}

bool EditorSession::loadFile() {
    QMutexLocker lock(&m_mutex);
    
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[EditorSession" << m_sessionId << "] Failed to open file:" << m_filePath;
        return false;
    }
    
    m_content = QString::fromUtf8(file.readAll());
    file.close();
    
    m_lastModifiedTime = QDateTime::currentDateTime();
    emit fileLoaded();
    
    qDebug() << "[EditorSession" << m_sessionId << "] File loaded:" << m_content.length() << "chars";
    return true;
}

bool EditorSession::saveFile() {
    QMutexLocker lock(&m_mutex);
    
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[EditorSession" << m_sessionId << "] Failed to save file:" << m_filePath;
        return false;
    }
    
    file.write(m_content.toUtf8());
    file.close();
    
    m_lastModifiedTime = QDateTime::currentDateTime();
    emit fileSaved();
    
    qDebug() << "[EditorSession" << m_sessionId << "] File saved:" << m_filePath;
    return true;
}

void EditorSession::insertText(const QString& text, int position) {
    QMutexLocker lock(&m_mutex);
    
    if (position < 0 || position > m_content.length()) {
        position = m_content.length();
    }
    
    m_content.insert(position, text);
    m_totalEdits++;
    m_lastModifiedTime = QDateTime::currentDateTime();
    
    emit contentChanged(m_content);
    qDebug() << "[EditorSession" << m_sessionId << "] Inserted" << text.length() << "chars at" << position;
}

void EditorSession::deleteText(int position, int length) {
    QMutexLocker lock(&m_mutex);
    
    if (position < 0 || position + length > m_content.length()) {
        return;
    }
    
    m_content.remove(position, length);
    m_totalEdits++;
    m_lastModifiedTime = QDateTime::currentDateTime();
    
    emit contentChanged(m_content);
    qDebug() << "[EditorSession" << m_sessionId << "] Deleted" << length << "chars at" << position;
}

void EditorSession::replaceText(int position, int length, const QString& replacement) {
    QMutexLocker lock(&m_mutex);
    
    if (position < 0 || position + length > m_content.length()) {
        return;
    }
    
    m_content.replace(position, length, replacement);
    m_totalEdits++;
    m_lastModifiedTime = QDateTime::currentDateTime();
    
    emit contentChanged(m_content);
    qDebug() << "[EditorSession" << m_sessionId << "] Replaced" << length << "chars with" << replacement.length() << "at" << position;
}

void EditorSession::setCursorPosition(int position) {
    QMutexLocker lock(&m_mutex);
    
    if (position < 0) position = 0;
    if (position > m_content.length()) position = m_content.length();
    
    if (m_cursorPosition != position) {
        m_cursorPosition = position;
        emit cursorMoved(position);
    }
}

int EditorSession::getCursorPosition() const {
    QMutexLocker lock(&m_mutex);
    return m_cursorPosition;
}

void EditorSession::setSelection(int start, int end) {
    QMutexLocker lock(&m_mutex);
    
    if (start < 0) start = 0;
    if (end > m_content.length()) end = m_content.length();
    if (start > end) std::swap(start, end);
    
    m_selectionStart = start;
    m_selectionEnd = end;
    
    emit selectionChanged(start, end);
}

QString EditorSession::getSelectedText() const {
    QMutexLocker lock(&m_mutex);
    
    if (m_selectionStart >= m_selectionEnd) {
        return QString();
    }
    
    return m_content.mid(m_selectionStart, m_selectionEnd - m_selectionStart);
}

bool EditorSession::undo() {
    QMutexLocker lock(&m_mutex);
    
    if (m_undoStack.isEmpty()) {
        return false;
    }
    
    m_redoStack.append(m_content);
    m_content = m_undoStack.takeLast();
    
    emit contentChanged(m_content);
    qDebug() << "[EditorSession" << m_sessionId << "] Undo performed";
    return true;
}

bool EditorSession::redo() {
    QMutexLocker lock(&m_mutex);
    
    if (m_redoStack.isEmpty()) {
        return false;
    }
    
    m_undoStack.append(m_content);
    m_content = m_redoStack.takeLast();
    
    emit contentChanged(m_content);
    qDebug() << "[EditorSession" << m_sessionId << "] Redo performed";
    return true;
}

void EditorSession::clearHistory() {
    QMutexLocker lock(&m_mutex);
    m_undoStack.clear();
    m_redoStack.clear();
}

EditorSession::FileType EditorSession::detectFileType(const QString& path) const {
    if (path.endsWith(".cpp") || path.endsWith(".cc") || path.endsWith(".cxx")) {
        return CPP;
    } else if (path.endsWith(".py")) {
        return Python;
    } else if (path.endsWith(".asm") || path.endsWith(".asm32")) {
        return Assembly;
    } else if (path.endsWith(".c")) {
        return C;
    } else if (path.endsWith(".js")) {
        return JavaScript;
    } else if (path.endsWith(".md") || path.endsWith(".markdown")) {
        return Markdown;
    }
    return Unknown;
}

QJsonObject EditorSession::getStatistics() const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject stats;
    stats["sessionId"] = m_sessionId;
    stats["filePath"] = m_filePath;
    stats["fileType"] = (int)m_fileType;
    stats["contentLength"] = m_content.length();
    stats["totalEdits"] = m_totalEdits;
    stats["createdTime"] = m_createdTime.toString(Qt::ISODate);
    stats["lastModifiedTime"] = m_lastModifiedTime.toString(Qt::ISODate);
    
    return stats;
}

QString EditorSession::getGhostText() const {
    QMutexLocker lock(&m_mutex);
    return m_ghostText;
}

void EditorSession::setGhostText(const QString& suggestion) {
    {
        QMutexLocker lock(&m_mutex);
        m_ghostText = suggestion;
    }
    emit ghostTextChanged(suggestion);
}

// RealTimeEditorIntegration Implementation

RealTimeEditorIntegration::RealTimeEditorIntegration(QObject* parent)
    : QObject(parent) {
    qDebug() << "[RealTimeEditorIntegration] Initialized";
}

RealTimeEditorIntegration::~RealTimeEditorIntegration() {
    {
        QMutexLocker lock(&m_sessionsMutex);
        m_sessions.clear();
    }
    qDebug() << "[RealTimeEditorIntegration] Destroyed";
}

int RealTimeEditorIntegration::openFile(const QString& filePath) {
    QMutexLocker lock(&m_sessionsMutex);
    
    if (m_sessions.size() >= (size_t)m_maxSessions) {
        qWarning() << "[RealTimeEditorIntegration] Max sessions reached";
        return -1;
    }
    
    int id = m_nextSessionId++;
    auto session = std::make_unique<EditorSession>(id, filePath, this);
    
    m_sessions[id] = std::move(session);
    
    emit fileOpened(id, filePath);
    qDebug() << "[RealTimeEditorIntegration] Opened file session:" << id;
    
    return id;
}

bool RealTimeEditorIntegration::closeFile(int sessionId) {
    QMutexLocker lock(&m_sessionsMutex);
    
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        m_sessions.erase(it);
        emit fileClosed(sessionId);
        qDebug() << "[RealTimeEditorIntegration] Closed file session:" << sessionId;
        return true;
    }
    
    return false;
}

bool RealTimeEditorIntegration::loadFileContent(int sessionId) {
    auto session = getSession(sessionId);
    if (session) {
        return session->loadFile();
    }
    return false;
}

bool RealTimeEditorIntegration::saveFile(int sessionId) {
    auto session = getSession(sessionId);
    if (session) {
        return session->saveFile();
    }
    return false;
}

bool RealTimeEditorIntegration::insertCodeAt(int sessionId, const QString& code, int position) {
    auto session = getSession(sessionId);
    if (session) {
        session->insertText(code, position);
        return true;
    }
    return false;
}

bool RealTimeEditorIntegration::deleteTextAt(int sessionId, int position, int length) {
    auto session = getSession(sessionId);
    if (session) {
        session->deleteText(position, length);
        return true;
    }
    return false;
}

bool RealTimeEditorIntegration::replaceTextAt(int sessionId, int position, int length, const QString& replacement) {
    auto session = getSession(sessionId);
    if (session) {
        session->replaceText(position, length, replacement);
        return true;
    }
    return false;
}

QString RealTimeEditorIntegration::getFileContent(int sessionId) const {
    auto session = getSession(sessionId);
    if (session) {
        return session->getContent();
    }
    return QString();
}

QString RealTimeEditorIntegration::getSelectedText(int sessionId) const {
    auto session = getSession(sessionId);
    if (session) {
        return session->getSelectedText();
    }
    return QString();
}

bool RealTimeEditorIntegration::findAndReplace(const QString& pattern, const QString& replacement, bool regex) {
    QMutexLocker lock(&m_sessionsMutex);
    
    int count = 0;
    for (auto& pair : m_sessions) {
        QString content = pair.second->getContent();
        int oldLength = content.length();
        
        if (regex) {
            // Qt6: use QRegularExpression for regex replacements
            content.replace(QRegularExpression(pattern), replacement);
        } else {
            content.replace(pattern, replacement);
        }
        
        if (content.length() != oldLength) {
            count++;
        }
    }
    
    qDebug() << "[RealTimeEditorIntegration] Find and replace found" << count << "matches";
    return count > 0;
}

void RealTimeEditorIntegration::requestCodeCompletion(int sessionId, const QString& context) {
    if (validateSessionId(sessionId)) {
        emit codeCompletionRequested(sessionId);
        qDebug() << "[RealTimeEditorIntegration] Code completion requested for session:" << sessionId;
        
        // Simulate completion
        emit codeCompletionReady(sessionId, "/* suggestions */");
    }
}

int RealTimeEditorIntegration::getOpenFileCount() const {
    QMutexLocker lock(&m_sessionsMutex);
    return m_sessions.size();
}

QJsonObject RealTimeEditorIntegration::getEditorStatistics() const {
    QMutexLocker lock(&m_sessionsMutex);
    
    QJsonObject stats;
    stats["openFiles"] = (int)m_sessions.size();
    stats["maxSessions"] = m_maxSessions;
    
    return stats;
}

void RealTimeEditorIntegration::setAgenticBridge(AgenticCopilotBridge* bridge) {
    QMutexLocker lock(&m_sessionsMutex);
    m_agenticBridge = bridge;
    qDebug() << "[RealTimeEditorIntegration] Agentic bridge set";
}

bool RealTimeEditorIntegration::undo(int sessionId) {
    auto session = getSession(sessionId);
    if (session) {
        return session->undo();
    }
    return false;
}

bool RealTimeEditorIntegration::redo(int sessionId) {
    auto session = getSession(sessionId);
    if (session) {
        return session->redo();
    }
    return false;
}

bool RealTimeEditorIntegration::validateSessionId(int sessionId) const {
    QMutexLocker lock(&m_sessionsMutex);
    return m_sessions.find(sessionId) != m_sessions.end();
}

EditorSession* RealTimeEditorIntegration::getSession(int sessionId) const {
    QMutexLocker lock(&m_sessionsMutex);
    
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) {
        return it->second.get();
    }
    
    return nullptr;
}
