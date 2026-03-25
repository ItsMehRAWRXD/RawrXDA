#include "crdt_buffer.h"
<<<<<<< HEAD
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

CRDTBuffer::CRDTBuffer()
    : m_siteId(generateSiteId()), m_logicalClock(0)
{
}

std::string CRDTBuffer::generateSiteId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    return "site_" + std::to_string(timestamp);
}

CRDTOperation CRDTBuffer::createInsertOperation(int position, const std::string& text) {
    CRDTOperation op;
    op.type = OperationType::INSERT;
    op.position = position;
    op.text = text;
    op.siteId = m_siteId;
    op.logicalTimestamp = ++m_logicalClock;
    op.operationId = m_siteId + "_" + std::to_string(op.logicalTimestamp);
    return op;
}

CRDTOperation CRDTBuffer::createDeleteOperation(int position, int length) {
    CRDTOperation op;
    op.type = OperationType::DELETE;
    op.position = position;
    op.length = length;
    op.siteId = m_siteId;
    op.logicalTimestamp = ++m_logicalClock;
    op.operationId = m_siteId + "_" + std::to_string(op.logicalTimestamp);
    return op;
}

void CRDTBuffer::applyRemoteOperation(const std::string &operationJson)
{
    // Parse JSON operation
    // Format: {"type":"INSERT","position":10,"text":"hello","siteId":"site_123","timestamp":5,"operationId":"site_123_5"}
    try {
        // Simple JSON parsing (in production, use a proper JSON library)
        std::istringstream iss(operationJson);
        std::string token;

        CRDTOperation op;
        std::getline(iss, token, ':'); // Skip "type"
        std::getline(iss, token, ',');
        if (token.find("INSERT") != std::string::npos) {
            op.type = OperationType::INSERT;
        } else if (token.find("DELETE") != std::string::npos) {
            op.type = OperationType::DELETE;
        }

        // Parse position
        std::getline(iss, token, ':'); // Skip "position"
        std::getline(iss, token, ',');
        op.position = std::stoi(token);

        if (op.type == OperationType::INSERT) {
            // Parse text
            std::getline(iss, token, ':'); // Skip "text"
            std::getline(iss, token, ',');
            // Remove quotes
            if (token.size() >= 2 && token[0] == '"' && token.back() == '"') {
                op.text = token.substr(1, token.size() - 2);
            }
        } else {
            // Parse length
            std::getline(iss, token, ':'); // Skip "length"
            std::getline(iss, token, ',');
            op.length = std::stoi(token);
        }

        // Apply the operation
        applyOperation(op);
        textChanged(m_text);

    } catch (const std::exception&) {
        // Invalid operation format
    }
}

void CRDTBuffer::applyOperation(const CRDTOperation& op) {
    if (op.type == OperationType::INSERT) {
        if (op.position >= 0 && op.position <= (int)m_text.length()) {
            m_text.insert(op.position, op.text);
        }
    } else if (op.type == OperationType::DELETE) {
        if (op.position >= 0 && op.position < (int)m_text.length() && op.length > 0) {
            int deleteLength = std::min(op.length, (int)m_text.length() - op.position);
            m_text.erase(op.position, deleteLength);
        }
    }
=======

CRDTBuffer::CRDTBuffer()
    
{
}

void CRDTBuffer::applyRemoteOperation(const std::string &operation)
{
    // In a real implementation, this would parse the operation and apply it to the text
    // For now, we'll just print a message
    // textChanged signal if the text was actually changed
    // textChanged(m_text);
>>>>>>> origin/main
}

std::string CRDTBuffer::getText() const
{
    return m_text;
}

void CRDTBuffer::insertText(int position, const std::string &text)
{
<<<<<<< HEAD
    if (position < 0 || position > (int)m_text.length() || text.empty()) {
        return;
    }

    // Apply locally
    m_text.insert(position, text);
    textChanged(m_text);

    // Generate CRDT operation
    CRDTOperation operation = createInsertOperation(position, text);

    // Serialize operation to JSON
    std::ostringstream oss;
    oss << "{\"type\":\"INSERT\",\"position\":" << operation.position
        << ",\"text\":\"" << operation.text << "\",\"siteId\":\"" << operation.siteId
        << "\",\"timestamp\":" << operation.logicalTimestamp
        << ",\"operationId\":\"" << operation.operationId << "\"}";

    // Emit operation for replication
    operationGenerated(oss.str());
=======
    if (position < 0 || position > m_text.length()) {
        return;
    }
    m_text.insert(position, text);
    textChanged(m_text);
    // In a real implementation, this would generate an operation and operationGenerated
    // operationGenerated(operation);
>>>>>>> origin/main
}

void CRDTBuffer::deleteText(int position, int length)
{
<<<<<<< HEAD
    if (position < 0 || position >= (int)m_text.length() || length <= 0) {
        return;
    }

    int actualLength = std::min(length, (int)m_text.length() - position);

    // Apply locally
    m_text.erase(position, actualLength);
    textChanged(m_text);

    // Generate CRDT operation
    CRDTOperation operation = createDeleteOperation(position, actualLength);

    // Serialize operation to JSON
    std::ostringstream oss;
    oss << "{\"type\":\"DELETE\",\"position\":" << operation.position
        << ",\"length\":" << operation.length << ",\"siteId\":\"" << operation.siteId
        << "\",\"timestamp\":" << operation.logicalTimestamp
        << ",\"operationId\":\"" << operation.operationId << "\"}";

    // Emit operation for replication
    operationGenerated(oss.str());
=======
    if (position < 0 || position >= m_text.length() || length <= 0) {
        return;
    }
    m_text.remove(position, length);
    textChanged(m_text);
    // In a real implementation, this would generate an operation and operationGenerated
    // operationGenerated(operation);
>>>>>>> origin/main
}

