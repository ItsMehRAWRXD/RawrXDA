#include "crdt_buffer.h"

CRDTBuffer::CRDTBuffer()
    
{
}

void CRDTBuffer::applyRemoteOperation(const std::string &operation)
{
    // In a real implementation, this would parse the operation and apply it to the text
    // For now, we'll just print a message
    // textChanged signal if the text was actually changed
    // textChanged(m_text);
}

std::string CRDTBuffer::getText() const
{
    return m_text;
}

void CRDTBuffer::insertText(int position, const std::string &text)
{
    if (position < 0 || position > m_text.length()) {
        return;
    }
    m_text.insert(position, text);
    textChanged(m_text);
    // In a real implementation, this would generate an operation and operationGenerated
    // operationGenerated(operation);
}

void CRDTBuffer::deleteText(int position, int length)
{
    if (position < 0 || position >= m_text.length() || length <= 0) {
        return;
    }
    m_text.remove(position, length);
    textChanged(m_text);
    // In a real implementation, this would generate an operation and operationGenerated
    // operationGenerated(operation);
}

