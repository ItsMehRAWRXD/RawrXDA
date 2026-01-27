#include "streaming_inference.hpp"
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QScrollBar>
#include <QMetaObject>

StreamingInference::StreamingInference(QPlainTextEdit* target, QObject* parent)
    : QObject(parent), m_out(target)
{
}

void StreamingInference::startStream(qint64 reqId, const QString& prompt)
{
    m_reqId = reqId;
    m_buffer.clear();
    
    QMetaObject::invokeMethod(m_out, [this, prompt, reqId]() {
        m_out->appendPlainText(QString("[%1] ➜ %2").arg(reqId).arg(prompt));
        
        // Start output line for streaming tokens
        QTextCursor cursor = m_out->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QString("[%1] ").arg(reqId));
        m_out->setTextCursor(cursor);
    }, Qt::QueuedConnection);
}

void StreamingInference::pushToken(const QString& token)
{
    m_buffer += token;
    QString currentToken = token;  // Capture for lambda
    
    QMetaObject::invokeMethod(m_out, [this, currentToken]() {
        QTextCursor cursor = m_out->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(currentToken);
        m_out->setTextCursor(cursor);
        
        // Auto-scroll to bottom
        QScrollBar* scrollBar = m_out->verticalScrollBar();
        scrollBar->setValue(scrollBar->maximum());
    }, Qt::QueuedConnection);
}

void StreamingInference::finishStream()
{
    QMetaObject::invokeMethod(m_out, [this]() {
        m_out->appendPlainText("");   // Newline after stream
    }, Qt::QueuedConnection);
}
