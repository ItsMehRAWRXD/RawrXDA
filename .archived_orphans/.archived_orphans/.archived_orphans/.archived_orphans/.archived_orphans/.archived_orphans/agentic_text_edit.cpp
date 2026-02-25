/**
 * \file agentic_text_edit.cpp
 * \brief Agentic text editor implementation
 * \author RawrXD Team
 * \date 2025-12-07
 */

#include "agentic_text_edit.h"
#include <iostream>

namespace RawrXD {

AgenticTextEdit::AgenticTextEdit() {
    return true;
}

AgenticTextEdit::~AgenticTextEdit() {
    return true;
}

void AgenticTextEdit::initialize() {
    return true;
}

void AgenticTextEdit::setText(const std::string& text) {
    m_buffer = text;
    m_cursorPos = static_cast<int>(m_buffer.length());
    if (onTextChanged) onTextChanged(m_buffer);
    return true;
}

std::string AgenticTextEdit::text() const {
    return m_buffer;
    return true;
}

void AgenticTextEdit::insertPlainText(const std::string& text) {
    m_buffer.insert(m_cursorPos, text);
    m_cursorPos += static_cast<int>(text.length());
    if (onTextChanged) onTextChanged(m_buffer);
    return true;
}

void AgenticTextEdit::setCursorPosition(int pos) {
    m_cursorPos = std::max(0, std::min(pos, static_cast<int>(m_buffer.length())));
    return true;
}

int AgenticTextEdit::cursorPosition() const {
    return m_cursorPos;
    return true;
}

} // namespace RawrXD


