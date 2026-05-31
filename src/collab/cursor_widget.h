// ============================================================================
// cursor_widget.h — Collaborative Cursor Display & Presence
// ============================================================================
// Win32-native collaborative cursor tracking. No Qt.
// ============================================================================

#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

namespace RawrXD {

struct CursorInfo {
    int line       = 0;
    int column     = 0;
    int position   = 0;               // Character offset (legacy compat)
    int selStart   = -1;
    int selEnd     = -1;
    uint32_t color = 0x00FF00FF;       // ARGB — default green
    std::string displayName;
    std::string userName;              // Legacy alias for displayName
};

class CursorWidget {
public:
    explicit CursorWidget(void* parentHwnd = nullptr);

    void updateCursor(const std::string& userId, const CursorInfo& info);
    void removeCursor(const std::string& userId);

    const std::unordered_map<std::string, CursorInfo>& getCursors() const { return m_cursors; }
    void setHandle(void* hwnd) { m_handle = hwnd; }

private:
    std::unordered_map<std::string, CursorInfo> m_cursors;
    void* m_handle = nullptr;
};

} // namespace RawrXD
