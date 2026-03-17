#include "streaming_inference.hpp"
#include <QScrollBar>
#include <QString>

StreamingInference::StreamingInference(QPlainTextEdit* target, QObject* parent)
    : QObject(parent), m_out(target)
{
}

void StreamingInference::startStream(int64_t reqId, const std::string& prompt)
{
    m_reqId = reqId;
    m_buffer.clear();
    
    // Convert to QString for capture
    QString qPrompt = QString::fromStdString(prompt);
    
    QMetaObject::invokeMethod(m_out, [this, qPrompt, reqId]() {
        QString header = QString("[%1] ➜ %2").arg(reqId).arg(qPrompt);
        m_out->appendPlainText(header);
        
        // Start output line for streaming tokens
        QTextCursor cursor = m_out->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QString("[%1] ").arg(reqId));
        m_out->setTextCursor(cursor);
    }, Qt::QueuedConnection);
}

void StreamingInference::pushToken(const std::string& token)
{
    m_buffer += token;
    QString currentToken = QString::fromStdString(token);
    
    QMetaObject::invokeMethod(m_out, [this, currentToken]() {
        QTextCursor cursor = m_out->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(currentToken);
        m_out->setTextCursor(cursor);
        
        // Auto-scroll to bottom
        QScrollBar* scrollBar = m_out->verticalScrollBar();
        if (scrollBar) {
            scrollBar->setValue(scrollBar->maximum());
        }
    }, Qt::QueuedConnection);
}

void StreamingInference::finishStream()
{
    QMetaObject::invokeMethod(m_out, [this]() {
        m_out->appendPlainText("");   // Newline after stream
    }, Qt::QueuedConnection);
}


