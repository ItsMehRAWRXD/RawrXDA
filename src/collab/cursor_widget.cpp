// CursorWidget — Win32/native. No Qt. Presence cursors for collaboration.

#include "cursor_widget.h"
#include <windows.h>

CursorWidget::CursorWidget(void* parent)
{
    (void)parent;
}

void CursorWidget::updateCursor(const std::string& userId, const CursorInfo& info)
{
    m_cursors[userId] = info;
    if (m_handle) {
        InvalidateRect((HWND)m_handle, nullptr, TRUE);
    }
}

void CursorWidget::removeCursor(const std::string& userId)
{
    m_cursors.erase(userId);
    if (m_handle) {
        InvalidateRect((HWND)m_handle, nullptr, TRUE);
    }
}
