#include "../include/collab/cursor_widget.h"
#include "../include/collab/crdt_buffer.h"

// Link-only fallbacks for collaboration runtime when real handlers are absent.
void CursorWidget::removeCursor(const std::string& id) { (void)id; }
void CursorWidget::updateCursor(const std::string& id, const CursorInfo& info) { (void)id; (void)info; }
void CRDTBuffer::applyRemoteOperation(const std::string& op) { (void)op; }
