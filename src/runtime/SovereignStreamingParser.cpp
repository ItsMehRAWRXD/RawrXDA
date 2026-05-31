#include "SovereignStreamingParser.h"
#include <iostream>
#include <vector>
#include <string>

namespace RawrXD::Runtime {

SovereignStreamingParser::SovereignStreamingParser(TokenCallback cb) : m_callback(cb), m_state(0) {}

void SovereignStreamingParser::consume(const char* data, size_t length) {
    m_buffer.append(data, length);
    processState();
}

void SovereignStreamingParser::processState() {
    // Simple state machine for tag detection (e.g., <thought>, <tool_call>)
    // Designed for SSE performance (one-pass)
    size_t pos = 0;
    while (pos < m_buffer.length()) {
        char c = m_buffer[pos];
        
        switch (m_state) {
            case 0: // Default Content
                if (c == '<') {
                    m_state = 3; // Start Tag Detection
                } else {
                    m_callback({TokenType::PlainContent, std::string(1, c)});
                }
                break;
            case 1: // Inside <thought>
                if (c == '<' && m_buffer.substr(pos, 9) == "</thought") {
                    m_callback({TokenType::ThoughtEnd, "", ""});
                    pos += 9;
                    m_state = 0;
                    continue;
                } else {
                    m_callback({TokenType::PlainContent, std::string(1, c), "thought"});
                }
                break;
            case 2: // Inside <tool_call>
                if (c == '<' && m_buffer.substr(pos, 11) == "</tool_call") {
                    m_callback({TokenType::ToolCallEnd, "", m_currentToolName});
                    pos += 11;
                    m_state = 0;
                    continue;
                } else {
                    m_callback({TokenType::ToolArguments, std::string(1, c), m_currentToolName});
                }
                break;
            case 3: // Detecting Special Tag
                if (m_buffer.substr(pos, 7) == "thought") {
                    m_callback({TokenType::ThoughtStart, "", ""});
                    pos += 7;
                    m_state = 1;
                    continue;
                } else if (m_buffer.substr(pos, 5) == "call:") {
                    m_state = 2; // In-Tool
                    pos += 5;
                    // Extract tool name if possible (simple for first version)
                    size_t nextSpace = m_buffer.find(' ', pos);
                    if (nextSpace != std::string::npos) {
                        m_currentToolName = m_buffer.substr(pos, nextSpace - pos);
                        pos = nextSpace;
                        m_callback({TokenType::ToolCallStart, "", m_currentToolName});
                    }
                    continue;
                }
                // If not code/thought, roll back to plain
                m_callback({TokenType::PlainContent, "<"});
                m_state = 0;
                break;
        }
        pos++;
    }
    // Optimization: Shrink buffer after processing
    m_buffer.erase(0, pos);
}

void SovereignStreamingParser::finalize() {
    if (!m_buffer.empty()) {
        m_callback({TokenType::PlainContent, m_buffer});
        m_buffer.clear();
    }
}

} // namespace RawrXD::Runtime
