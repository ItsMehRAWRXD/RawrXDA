#include "../include/collab/cursor_widget.h"
#include "../include/collab/crdt_buffer.h"

// Link-only fallbacks for collaboration runtime when real handlers are absent.
void CursorWidget::removeCursor(const std::string& id) {
    m_cursors.erase(id);
}
void CursorWidget::updateCursor(const std::string& id, const CursorInfo& info) {
    if (id.empty()) {
        return;
    }
    m_cursors[id] = info;
}
void CRDTBuffer::applyRemoteOperation(const std::string& op) {
    if (op.rfind("replace:", 0) == 0) {
        m_text = op.substr(8);
    } else if (!op.empty()) {
        if (!m_text.empty()) {
            m_text.push_back('\n');
        }
        m_text += op;
    }

    if (m_onTextChanged) {
        m_onTextChanged(m_text);
    }
}
