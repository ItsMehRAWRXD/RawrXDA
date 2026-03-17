#include "streaming_token_manager.h"
#include <QVBoxLayout>
#include <QFont>
#include <QTextCursor>
#include <QPalette>

StreamingTokenManager::StreamingTokenManager(QObject* parent)
    : QObject(parent)
{
    m_streamBuffer.reserve(STREAM_BUFFER_SIZE);
    m_currentCallBuffer.reserve(CALL_BUFFER_SIZE);
    m_thinkingBuffer.reserve(4096);
}

StreamingTokenManager::~StreamingTokenManager()
{
    if (m_thinkingBox) {
        m_thinkingBox->deleteLater();
    }
}

void StreamingTokenManager::initialize(QWidget* chatPanel, QTextEdit* richEdit)
{
    m_chatPanel = chatPanel;
    m_richEdit = richEdit;

    if (m_richEdit) {
        applyMessageStyle(m_richEdit);
    }
}

void StreamingTokenManager::startCall(const QString& modelName)
{
    // Clear call buffer and activate session (from MASM ChatStreamManager_StartCall)
    m_currentCallPos = 0;
    m_currentCallBuffer.clear();
    m_currentCallActive = true;

    // Show thinking UI if enabled
    if (m_thinkingEnabled) {
        showThinking();
    }
}

void StreamingTokenManager::finishCall(bool success)
{
    // From MASM ChatStreamManager_FinishCall
    if (success && m_currentCallActive) {
        // Append collected call buffer to main chat area
        if (m_richEdit && m_currentCallPos > 0) {
            QTextCursor cursor = m_richEdit->textCursor();
            cursor.movePosition(QTextCursor::End);
            m_richEdit->setTextCursor(cursor);
            m_richEdit->insertPlainText(QString::fromUtf8(m_currentCallBuffer.constData(), m_currentCallPos));
        }
    }

    // Clear call buffer and deactivate session
    m_currentCallPos = 0;
    m_currentCallBuffer.clear();
    m_currentCallActive = false;

    // Hide thinking UI
    hideThinking();

    emit streamFinished();
}

void StreamingTokenManager::onToken(const QString& token)
{
    if (token.isEmpty()) return;

    // Check buffer overflow (from MASM ChatStreamManager_OnToken)
    if (m_streamPos + token.toUtf8().size() >= STREAM_BUFFER_SIZE) {
        flushBuffer();
    }

    // If call session is active, append to call buffer (preferred path)
    if (m_currentCallActive) {
        appendToCallBuffer(token);

        // Update thinking box if visible
        if (m_thinkingVisible && m_thinkingBox) {
            m_thinkingBox->setPlainText(QString::fromUtf8(m_currentCallBuffer.constData(), m_currentCallPos));
        }

        emit streamToken(token);
        return;
    }

    // If thinking is enabled and visible, write to thinking buffer first
    if (m_thinkingEnabled && m_thinkingVisible) {
        appendToThinkingBuffer(token);

        if (m_thinkingBox) {
            m_thinkingBox->setPlainText(QString::fromUtf8(m_thinkingBuffer.constData(), m_thinkingPos));
        }

        emit streamToken(token);
        return;
    }

    // Default: append to stream buffer
    appendToStreamBuffer(token);
    m_isStreaming = true;

    emit streamToken(token);
}

void StreamingTokenManager::onToken(const char* token, int tokenLen)
{
    if (!token || tokenLen <= 0) return;
    onToken(QString::fromUtf8(token, tokenLen));
}

void StreamingTokenManager::showThinking(const QString& text)
{
    // From MASM ChatStreamManager_ShowThinking
    if (!m_thinkingVisible) {
        createThinkingBox();
    }

    if (m_thinkingBox) {
        m_thinkingBox->setPlainText(text);
    }

    m_thinkingPos = 0;
    m_thinkingBuffer.clear();
}

void StreamingTokenManager::hideThinking()
{
    // From MASM ChatStreamManager_HideThinking
    if (m_thinkingVisible) {
        destroyThinkingBox();
    }
}

void StreamingTokenManager::setThinkingEnabled(bool enabled)
{
    m_thinkingEnabled = enabled;
}

void StreamingTokenManager::flushBuffer()
{
    // From MASM ChatStreamManager_FlushBuffer
    if (m_streamPos == 0 || !m_richEdit) return;

    QTextCursor cursor = m_richEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_richEdit->setTextCursor(cursor);
    m_richEdit->insertPlainText(QString::fromUtf8(m_streamBuffer.constData(), m_streamPos));

    m_streamPos = 0;
    m_streamBuffer.clear();
}

void StreamingTokenManager::finishStream()
{
    m_isStreaming = false;
    flushBuffer();
}

QString StreamingTokenManager::getCurrentCallBuffer() const
{
    return QString::fromUtf8(m_currentCallBuffer.constData(), m_currentCallPos);
}

// Private helper methods

void StreamingTokenManager::appendToStreamBuffer(const QString& token)
{
    QByteArray tokenBytes = token.toUtf8();
    m_streamBuffer.append(tokenBytes);
    m_streamPos += tokenBytes.size();
}

void StreamingTokenManager::appendToCallBuffer(const QString& token)
{
    QByteArray tokenBytes = token.toUtf8();

    // Ensure we don't overflow the call buffer
    if (m_currentCallPos + tokenBytes.size() < CALL_BUFFER_SIZE) {
        m_currentCallBuffer.append(tokenBytes);
        m_currentCallPos += tokenBytes.size();
    }
}

void StreamingTokenManager::appendToThinkingBuffer(const QString& token)
{
    QByteArray tokenBytes = token.toUtf8();

    if (m_thinkingPos + tokenBytes.size() < m_thinkingBuffer.capacity()) {
        m_thinkingBuffer.append(tokenBytes);
        m_thinkingPos += tokenBytes.size();
    }
}

void StreamingTokenManager::createThinkingBox()
{
    // From MASM ChatStreamManager_ShowThinking - create monospace thinking box
    if (!m_chatPanel) return;

    m_thinkingBox = new QTextEdit(m_chatPanel);
    m_thinkingBox->setReadOnly(true);
    m_thinkingBox->setObjectName("ThinkingBox");

    // Set monospace font (Consolas) for code-style look
    QFont font("Consolas", 10);
    if (!font.exactMatch()) {
        font.setFamily("Courier New");
    }
    font.setFixedPitch(true);
    m_thinkingBox->setFont(font);

    // Style: white background, black text, grey border (from MASM README)
    QPalette palette = m_thinkingBox->palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    m_thinkingBox->setPalette(palette);

    m_thinkingBox->setStyleSheet(
        "QTextEdit {"
        "   background-color: white;"
        "   color: black;"
        "   border: 1px solid #c0c0c0;"
        "   padding: 4px;"
        "}"
    );

    // Position at top of chat panel
    m_thinkingBox->setGeometry(10, 10, 580, 120);
    m_thinkingBox->show();

    m_thinkingVisible = true;
    emit thinkingVisibilityChanged(true);
}

void StreamingTokenManager::destroyThinkingBox()
{
    if (m_thinkingBox) {
        m_thinkingBox->deleteLater();
        m_thinkingBox = nullptr;
    }

    m_thinkingVisible = false;
    emit thinkingVisibilityChanged(false);
}

void StreamingTokenManager::applyMessageStyle(QTextEdit* target)
{
    // From MASM ChatStreamManager_ApplyMessageStyle
    // Set UTF-8 capable font with clean styling
    if (!target) return;

    QFont font("Segoe UI", 12);
    if (!font.exactMatch()) {
        font.setFamily("Arial");
    }
    target->setFont(font);

    QPalette palette = target->palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Text, Qt::black);
    target->setPalette(palette);

    target->setStyleSheet(
        "QTextEdit {"
        "   background-color: white;"
        "   color: black;"
        "   border: 1px solid #d0d0d0;"
        "}"
    );
}
