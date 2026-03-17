/**
 * \file ghost_text_renderer.h
 * \brief Cursor-style inline ghost text with diff preview
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

// C++20 Win32-only data structs and GhostTextRenderer overlay (no Qt).
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

/** RGBA color (replaces QColor) */
struct GhostTextColor {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

/**
 * \brief Ghost text decoration for inline completions
 */
struct GhostTextDecoration {
    int line = 0;
    int column = 0;
    std::string text;
    std::string type;           // "completion", "diff", "suggestion"
    GhostTextColor color;
    bool multiline = false;
    std::vector<std::string> lines;
};

/**
 * \brief Diff preview decoration
 */
struct DiffDecoration {
    int startLine = 0;
    int endLine = 0;
    std::string oldText;
    std::string newText;
    std::string type;           // "add", "remove", "modify"
};

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

/**
 * \brief Ghost text renderer overlay — Qt-free C++20/Win32
 * 
 * Features:
 * - Cursor-style inline ghost text
 * - Real-time streaming completions
 * - Multi-line ghost text with proper indentation
 * - Diff preview (green additions, red deletions)
 * - Fade-in/fade-out animations
 * - Tab to accept, Esc to dismiss
 */
class GhostTextRenderer
{
public:
    explicit GhostTextRenderer(HWND editorHwnd = nullptr)
        : m_editorHwnd(editorHwnd) {}
    ~GhostTextRenderer() = default;

    /** Set editor HWND (call before initialize when created without one). */
    void setEditorHwnd(HWND hwnd) { m_editorHwnd = hwnd; }

    /**
     * Two-phase initialization — sets up overlay for the current m_editorHwnd.
     */
    void initialize() { /* Win32 overlay setup when m_editorHwnd is set */ }

    /**
     * Show ghost text at cursor position
     */
    void showGhostText(const std::string& text, const std::string& type = "completion");

    /**
     * Show multi-line ghost text
     */
    void showMultilineGhost(const std::vector<std::string>& lines);

    /**
     * Update ghost text (for streaming)
     */
    void updateGhostText(const std::string& additionalText);

    /**
     * Show diff preview
     */
    void showDiffPreview(int startLine, int endLine, const std::string& oldText, const std::string& newText);

    /**
     * Clear all ghost text
     */
    void clearGhostText();

    /**
     * Clear diff preview
     */
    void clearDiffPreview();

    /**
     * Accept current ghost text (insert into editor)
     */
    void acceptGhostText();

    /**
     * Get current ghost text
     */
    std::string getCurrentGhostText() const { return m_currentGhostText; }

    /**
     * Check if ghost text is visible
     */
    bool hasGhostText() const { return !m_currentGhostText.empty(); }

    // --- Callbacks (replaces Qt signals) ---
    using TextCb = void(*)(void* ctx, const char* text);
    using VoidCb = void(*)(void* ctx);

    void setGhostTextAcceptedCb(TextCb cb, void* ctx) { m_acceptedCb = cb; m_acceptedCtx = ctx; }
    void setGhostTextDismissedCb(VoidCb cb, void* ctx) { m_dismissedCb = cb; m_dismissedCtx = ctx; }
    void setDiffAcceptedCb(TextCb cb, void* ctx) { m_diffAcceptedCb = cb; m_diffAcceptedCtx = ctx; }

    /** \brief Point structure (replaces QPoint) */
    struct Point { int x = 0; int y = 0; };

private:
    void renderGhostText(HDC hdc);
    void renderDiffPreview(HDC hdc);
    void updateOverlayGeometry();
    Point getCursorPosition() const;
    int getLineHeight() const;
    void fadeIn();
    void fadeOut();

    HWND m_editorHwnd{};
    
    std::string m_currentGhostText;
    GhostTextDecoration m_ghostDecoration;
    std::vector<DiffDecoration> m_diffDecorations;
    
    float m_opacity = 1.0f;
    UINT_PTR m_fadeTimerId = 0;
    bool m_fading = false;
    
    GhostTextColor m_ghostColor{128, 128, 128, 180};  // Gray with transparency
    GhostTextColor m_addColor{0, 255, 0, 100};        // Green for additions
    GhostTextColor m_removeColor{255, 0, 0, 100};     // Red for deletions

    // Callback state
    TextCb m_acceptedCb = nullptr;      void* m_acceptedCtx = nullptr;
    VoidCb m_dismissedCb = nullptr;     void* m_dismissedCtx = nullptr;
    TextCb m_diffAcceptedCb = nullptr;  void* m_diffAcceptedCtx = nullptr;
};

// Legacy Qt fallback removed — the class above is the only Win32 implementation.
// The duplicate stub (GhostTextRenderer for headless/Qt paths) was removed during
// Qt→Win32 migration. Win32IDE_GhostText.cpp drives rendering through the class above.

} // namespace RawrXD
