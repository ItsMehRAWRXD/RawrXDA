#pragma once
/**
 * @file minimap.h
 * @brief Code minimap for navigation
 * Batch 4 - Item 58: Minimap
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

struct MinimapSection {
    int startLine;
    int endLine;
    uint32_t color;
    std::string type;
};

struct MinimapRenderInfo {
    int totalLines;
    int visibleStartLine;
    int visibleEndLine;
    float scale;
    int charWidth;
    int lineHeight;
};

class Minimap {
public:
    Minimap();
    ~Minimap();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void resize(int width, int height);
    void setPosition(int x, int y);

    // Content
    void setContent(const std::string& text);
    void updateContent(const std::string& text);
    void clear();

    // Viewport
    void setViewport(int startLine, int endLine);
    void setTotalLines(int lines);
    int getTotalLines() const;

    // Sections (for highlighting)
    void addSection(const MinimapSection& section);
    void clearSections();
    void setSelection(int startLine, int endLine);
    void setCursorLine(int line);

    // Rendering
    void render();
    void invalidate();

    // Configuration
    void setEnabled(bool enabled);
    bool isEnabled() const;
    void setWidth(int width);
    int getWidth() const;
    void setScale(float scale);
    float getScale() const;
    void setShowSlider(bool show);
    void setRenderCharacters(bool render);

    // Events
    using ScrollCallback = std::function<void(int line)>;
    using ClickCallback = std::function<void(int line, int column)>;
    void onScroll(ScrollCallback callback);
    void onClick(ClickCallback callback);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    HBITMAP m_bitmap{nullptr};
    HDC m_memDC{nullptr};
    std::string m_content;
    std::vector<MinimapSection> m_sections;
    int m_totalLines{0};
    int m_visibleStartLine{0};
    int m_visibleEndLine{0};
    int m_selectionStartLine{-1};
    int m_selectionEndLine{-1};
    int m_cursorLine{-1};
    int m_width{120};
    int m_height{0};
    float m_scale{0.1f};
    bool m_enabled{true};
    bool m_showSlider{true};
    bool m_renderCharacters{false};
    bool m_dragging{false};

    ScrollCallback m_scrollCallback;
    ClickCallback m_clickCallback;

    void createBitmap();
    void destroyBitmap();
    void drawContent();
    void drawViewport(HDC hdc);
    void drawSections(HDC hdc);
    void drawSlider(HDC hdc);
    int lineToY(int line);
    int yToLine(int y);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
Minimap& getMinimap();

} // namespace RawrXD::UI
