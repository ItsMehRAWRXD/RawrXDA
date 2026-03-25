#include "../include/collab/cursor_widget.h"
#include "../include/collab/crdt_buffer.h"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <windows.h>

// ============================================================================
// CRDTBuffer + CursorWidget — production implementations
// Provides the 3 collaboration symbols required by the Win32IDE link target.
// ============================================================================

namespace {
bool extractIntAfter(const std::string& text, const std::string& marker, int& valueOut) {
    const std::size_t pos = text.find(marker);
    if (pos == std::string::npos) {
        return false;
    }
    const char* begin = text.c_str() + pos + marker.size();
    char*       end   = nullptr;
    const long  parsed = std::strtol(begin, &end, 10);
    if (end == begin) {
        return false;
    }
    valueOut = static_cast<int>(parsed);
    return true;
}

bool extractQuotedAfter(const std::string& text, const std::string& marker, std::string& valueOut) {
    const std::size_t pos = text.find(marker);
    if (pos == std::string::npos) {
        return false;
    }
    const std::size_t start = pos + marker.size();
    const std::size_t end   = text.find('"', start);
    if (end == std::string::npos || end < start) {
        return false;
    }
    valueOut.assign(text.substr(start, end - start));
    return true;
}
} // namespace

void CursorWidget::removeCursor(const std::string& id) {
    if (id.empty()) {
        return;
    }
    m_cursors.erase(id);
    if (m_handle != nullptr && IsWindow(static_cast<HWND>(m_handle))) {
        InvalidateRect(static_cast<HWND>(m_handle), nullptr, TRUE);
    }
}

void CursorWidget::updateCursor(const std::string& id, const CursorInfo& info) {
    if (id.empty()) {
        return;
    }
    m_cursors[id] = info;
    if (m_handle != nullptr && IsWindow(static_cast<HWND>(m_handle))) {
        InvalidateRect(static_cast<HWND>(m_handle), nullptr, TRUE);
    }
}

void CRDTBuffer::applyRemoteOperation(const std::string& op) {
    if (op.empty()) {
        return;
    }

    const std::string before  = m_text;
    bool              applied = false;

    if (op.rfind("INSERT:", 0) == 0) {
        const std::size_t delim = op.find(':', 7);
        if (delim != std::string::npos) {
            const int         pos         = std::atoi(op.substr(7, delim - 7).c_str());
            const std::string payload     = op.substr(delim + 1);
            const int         boundedPos  = std::clamp(pos, 0, static_cast<int>(m_text.size()));
            m_text.insert(static_cast<std::size_t>(boundedPos), payload);
            applied = true;
        }
    } else if (op.rfind("DELETE:", 0) == 0) {
        const std::size_t delim = op.find(':', 7);
        if (delim != std::string::npos) {
            const int pos = std::atoi(op.substr(7, delim - 7).c_str());
            const int len = std::atoi(op.substr(delim + 1).c_str());
            if (len > 0) {
                const int boundedPos = std::clamp(pos, 0, static_cast<int>(m_text.size()));
                const int maxDelete  = static_cast<int>(m_text.size()) - boundedPos;
                if (maxDelete > 0) {
                    m_text.erase(static_cast<std::size_t>(boundedPos),
                                 static_cast<std::size_t>(std::min(len, maxDelete)));
                    applied = true;
                }
            }
        }
    } else if (op.find("\"type\":\"INSERT\"") != std::string::npos) {
        int         pos         = 0;
        std::string textPayload;
        if (extractIntAfter(op, "\"position\":", pos) &&
            extractQuotedAfter(op, "\"text\":\"", textPayload)) {
            const int boundedPos = std::clamp(pos, 0, static_cast<int>(m_text.size()));
            m_text.insert(static_cast<std::size_t>(boundedPos), textPayload);
            applied = true;
        }
    } else if (op.find("\"type\":\"DELETE\"") != std::string::npos) {
        int pos = 0;
        int len = 0;
        if (extractIntAfter(op, "\"position\":", pos) &&
            extractIntAfter(op, "\"length\":", len) && len > 0) {
            const int boundedPos = std::clamp(pos, 0, static_cast<int>(m_text.size()));
            const int maxDelete  = static_cast<int>(m_text.size()) - boundedPos;
            if (maxDelete > 0) {
                m_text.erase(static_cast<std::size_t>(boundedPos),
                             static_cast<std::size_t>(std::min(len, maxDelete)));
                applied = true;
            }
        }
    }

    if (applied && m_text != before && m_onTextChanged) {
        m_onTextChanged(m_text);
    }
}
