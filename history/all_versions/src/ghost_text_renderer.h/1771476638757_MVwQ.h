/**
 * \file ghost_text_renderer.h
 * \brief Cursor-style inline ghost text with diff preview
 * \author RawrXD Team
 * \date 2025-12-07
 *
 * Pure Win32 + Direct2D — zero Qt dependencies.
 * Uses Renderer2D / Signal<> from the RawrXD framework.
 */

#pragma once

#include "RawrXD_Win32_Foundation.h"
#include "RawrXD_Renderer_D2D.h"
#include "RawrXD_SignalSlot.h"
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {

// ───────────────────────────────────────────────────────────────────────
// Data types
// ───────────────────────────────────────────────────────────────────────

struct GhostTextDecoration {
    int line   = 0;               // 0-indexed line in the editor
    int column = 0;               // Column offset
    std::wstring text;            // Ghost text to display
    std::wstring type;            // L"completion", L"diff", L"suggestion"
    Color color{128, 128, 128};   // Ghost text color (default: gray)
    float opacity = 0.7f;         // Initial alpha
    bool multiline = false;
    std::vector<std::wstring> lines; // For multi-line ghost text
};

struct DiffDecoration {
    int startLine  = 0;
    int endLine    = 0;
    std::wstring oldText;
    std::wstring newText;
    std::wstring type;            // L"add", L"remove", L"modify"
};

// ───────────────────────────────────────────────────────────────────────
// GhostTextRenderer
// ───────────────────────────────────────────────────────────────────────

class GhostTextRenderer {
public:
    GhostTextRenderer();
    ~GhostTextRenderer();

    // Non-copyable
    GhostTextRenderer(const GhostTextRenderer&) = delete;
    GhostTextRenderer& operator=(const GhostTextRenderer&) = delete;

    /// Attach to an editor surface. \a renderer must remain valid for the
    /// lifetime of this object. \a charWidth / \a lineHeight are in DIPs.
    void initialize(Renderer2D* renderer, float charWidth, float lineHeight);

    // ── Ghost text ──────────────────────────────────────────────────

    void showGhostText(const std::wstring& text,
                       int cursorLine,
                       int cursorCol,
                       const std::wstring& type = L"completion");

    void showMultilineGhost(const std::vector<std::wstring>& lines,
                            int cursorLine,
                            int cursorCol);

    /// Streaming append — extend the current ghost text.
    void updateGhostText(const std::wstring& additionalText);

    void clearGhostText();

    void acceptGhostText();

    bool hasGhostText() const { return !m_currentGhostText.empty(); }
    const std::wstring& getCurrentGhostText() const { return m_currentGhostText; }

    // ── Diff preview ────────────────────────────────────────────────

    void showDiffPreview(int startLine, int endLine,
                         const std::wstring& oldText,
                         const std::wstring& newText);

    void clearDiffPreview();

    // ── Rendering (called from EditorCore::paint) ───────────────────

    /// Paint all ghost text and diff decorations.
    /// \a gutterWidth, \a scrollX, \a scrollY are editor layout params.
    void paint(const Font& font,
               float gutterWidth,
               int scrollX,
               int scrollY);

    // ── Fade animation ──────────────────────────────────────────────

    void fadeIn();
    void fadeOut();

    /// Must be called from the editor's timer/frame tick so the fade
    /// animation progresses. Returns true if a repaint is needed.
    bool tick();

    // ── Key handling (call from WM_KEYDOWN / WM_CHAR) ───────────────

    /// Returns true if the key was consumed (Tab = accept, Esc = dismiss).
    bool handleKeyDown(UINT vk, bool shift, bool ctrl);

    // ── Signals ─────────────────────────────────────────────────────

    Signal<const std::wstring&> ghostTextAccepted;   // emitted on accept
    Signal<>                    ghostTextDismissed;   // emitted on dismiss

private:
    Renderer2D* m_renderer = nullptr;

    // Layout metrics (set via initialize, updated when font changes)
    float m_charWidth  = 9.0f;
    float m_lineHeight = 20.0f;

    // Ghost text state
    std::wstring         m_currentGhostText;
    GhostTextDecoration  m_ghostDecoration;
    std::vector<DiffDecoration> m_diffDecorations;

    // Fade animation
    float m_opacity     = 1.0f;
    float m_targetAlpha = 1.0f;  // 0 for fade-out, 1 for fade-in
    bool  m_fading      = false;

    // Colors
    Color m_ghostColor  {128, 128, 128};   // Gray
    Color m_addColor    {  0, 180,   0};   // Green
    Color m_removeColor {200,  60,  60};   // Red
    Color m_modifyColor {200, 200,  60};   // Yellow
};

} // namespace RawrXD


