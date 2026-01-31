#include "streaming_inference.hpp"


StreamingInference::StreamingInference(QPlainTextEdit* target, void* parent)
    : void(parent), m_out(target)
{
}

void StreamingInference::startStream(qint64 reqId, const std::string& prompt)
{
    m_reqId = reqId;
    m_buffer.clear();
    
    QMetaObject::invokeMethod(m_out, [this, prompt, reqId]() {
        m_out->appendPlainText(std::string("[%1] ➜ %2"));
        
        // Start output line for streaming tokens
        QTextCursor cursor = m_out->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(std::string("[%1] "));
        m_out->setTextCursor(cursor);
    }, //QueuedConnection);
}

void StreamingInference::pushToken(const std::string& token)
{
    m_buffer += token;
    std::string currentToken = token;  // Capture for lambda
    
    QMetaObject::invokeMethod(m_out, [this, currentToken]() {
        QTextCursor cursor = m_out->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(currentToken);
        m_out->setTextCursor(cursor);
        
        // Auto-scroll to bottom
        QScrollBar* scrollBar = m_out->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }, //QueuedConnection);
}

void StreamingInference::finishStream()
{
    QMetaObject::invokeMethod(m_out, [this]() {
        m_out->appendPlainText("");   // Newline after stream
    }, //QueuedConnection);
}

