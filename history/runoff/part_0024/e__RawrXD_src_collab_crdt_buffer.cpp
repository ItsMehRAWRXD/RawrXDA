#include "crdt_buffer.h"

CRDTBuffer::CRDTBuffer(QObject *parent)
    : QObject(parent)
{
}

void CRDTBuffer::applyRemoteOperation(const QString &operation)
{
    // In a real implementation, this would parse the operation and apply it to the text
    // For now, we'll just print a message
    qDebug() << "Applying remote operation:" << operation;
    // Emit textChanged signal if the text was actually changed
    // emit textChanged(m_text);
}

QString CRDTBuffer::getText() const
{
    return m_text;
}

void CRDTBuffer::insertText(int position, const QString &text)
{
    if (position < 0 || position > m_text.length()) {
        qWarning() << "Invalid position for insertText:" << position;
        return;
    }
    m_text.insert(position, text);
    emit textChanged(m_text);
    // In a real implementation, this would generate an operation and emit operationGenerated
    // emit operationGenerated(operation);
}

void CRDTBuffer::deleteText(int position, int length)
{
    if (position < 0 || position >= m_text.length() || length <= 0) {
        qWarning() << "Invalid parameters for deleteText:" << position << length;
        return;
    }
    m_text.remove(position, length);
    emit textChanged(m_text);
    // In a real implementation, this would generate an operation and emit operationGenerated
    // emit operationGenerated(operation);
}