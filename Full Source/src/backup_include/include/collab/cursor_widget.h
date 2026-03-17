#ifndef CURSOR_WIDGET_H
#define CURSOR_WIDGET_H

// C++20 / Win32. Presence cursor widget; no Qt. RGB color as uint32_t.

#include <string>
#include <map>
#include <cstdint>

struct CursorInfo {
    int position = 0;
    std::string userName;
    uint32_t color = 0;  // 0xRRGGBB
};

class CursorWidget
{
public:
    CursorWidget() = default;
    explicit CursorWidget(void* parent);

    void updateCursor(const std::string& userId, const CursorInfo& info);
    void removeCursor(const std::string& userId);

    void* getWidgetHandle() const { return m_handle; }
    void setWidgetHandle(void* h) { m_handle = h; }

private:
    void* m_handle = nullptr;
    std::map<std::string, CursorInfo> m_cursors;
};

#endif // CURSOR_WIDGET_H
