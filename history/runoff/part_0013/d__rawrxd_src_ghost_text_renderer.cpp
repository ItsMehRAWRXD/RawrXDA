/**
 * \file ghost_text_renderer.cpp
 * \brief Cursor-style inline ghost text implementation
 * \author RawrXD Team
 * \date 2025-12-07
 *
 * Pure Win32 + Direct2D — zero Qt dependencies.
 */

#include "ghost_text_renderer.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {

// ─── Construction / destruction ──────────────────────────────────────

GhostTextRenderer::GhostTextRenderer() = default;
GhostTextRenderer::~GhostTextRenderer() = default;

void GhostTextRenderer::initialize(Renderer2D* renderer, float charWidth, float lineHeight) {
    m_renderer   = renderer;
    m_charWidth  = charWidth;
    m_lineHeight = lineHeight;
}

// ─── Ghost text ──────────────────────────────────────────────────────

void GhostTextRenderer::showGhostText(const std::wstring& text,
                                       int cursorLine,
                                       int cursorCol,
                                       const std::wstring& type) {
    if (text.empty()) {
        clearGhostText();
        return;
    }

    m_currentGhostText = text;
    m_ghostDecoration.line   = cursorLine;
    m_ghostDecoration.column = cursorCol;
    m_ghostDecoration.text   = text;
    m_ghostDecoration.type   = type;
    m_ghostDecoration.multiline = false;
    m_ghostDecoration.lines.clear();

    // Color based on type
    if (type == L"completion") {
        m_ghostDecoration.color = m_ghostColor;
    } else if (type == L"suggestion") {
        m_ghostDecoration.color = Color(100, 150, 255);
    } else {
        m_ghostDecoration.color = m_ghostColor;
    }

    m_opacity     = 1.0f;
    m_fading      = false;
    m_targetAlpha = 1.0f;
}

void GhostTextRenderer::showMultilineGhost(const std::vector<std::wstring>& lines,
                                            int cursorLine,
                                            int cursorCol) {
    if (lines.empty()) {
        clearGhostText();
        return;
    }

    // Join for the flat string representation
    m_currentGhostText.clear();
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) m_currentGhostText += L'\n';
        m_currentGhostText += lines[i];
    }

    m_ghostDecoration.line   = cursorLine;
    m_ghostDecoration.column = cursorCol;
    m_ghostDecoration.text   = m_currentGhostText;
    m_ghostDecoration.type   = L"completion";
    m_ghostDecoration.multiline = true;
    m_ghostDecoration.lines  = lines;
    m_ghostDecoration.color  = m_ghostColor;

    m_opacity     = 1.0f;
    m_fading      = false;
    m_targetAlpha = 1.0f;
}

void GhostTextRenderer::updateGhostText(const std::wstring& additionalText) {
    if (!hasGhostText()) return;

    m_currentGhostText += additionalText;
    m_ghostDecoration.text = m_currentGhostText;

    // Re-check multiline
    if (m_currentGhostText.find(L'\n') != std::wstring::npos) {
        m_ghostDecoration.multiline = true;
        m_ghostDecoration.lines.clear();
        std::wistringstream ss(m_currentGhostText);
        std::wstring line;
        while (std::getline(ss, line)) {
            m_ghostDecoration.lines.push_back(line);
        }
    }
}

void GhostTextRenderer::clearGhostText() {
    m_currentGhostText.clear();
    m_ghostDecoration = GhostTextDecoration{};
    m_fading = false;
}

void GhostTextRenderer::acceptGhostText() {
    if (!hasGhostText()) return;
    std::wstring text = m_currentGhostText;
    clearGhostText();
    ghostTextAccepted.emit(text);
}

// ─── Diff preview ────────────────────────────────────────────────────

void GhostTextRenderer::showDiffPreview(int startLine, int endLine,
                                         const std::wstring& oldText,
                                         const std::wstring& newText) {
    DiffDecoration diff;
    diff.startLine = startLine;
    diff.endLine   = endLine;
    diff.oldText   = oldText;
    diff.newText   = newText;

    if (oldText.empty())       diff.type = L"add";
    else if (newText.empty())  diff.type = L"remove";
    else                       diff.type = L"modify";

    m_diffDecorations.push_back(diff);
}

void GhostTextRenderer::clearDiffPreview() {
    m_diffDecorations.clear();
}

// ─── Rendering ───────────────────────────────────────────────────────

void GhostTextRenderer::paint(const Font& font,
                               float gutterWidth,
                               int scrollX,
                               int scrollY) {
    if (!m_renderer) return;

    // ── Ghost text ──
    if (hasGhostText()) {
        // Compose a color with current fade opacity
        Color gc = m_ghostDecoration.color;
        gc.a = static_cast<uint8_t>(m_opacity * 180.0f);  // semi-transparent ghost

        float baseX = gutterWidth + 2.0f - scrollX + m_ghostDecoration.column * m_charWidth;
        float baseY = m_ghostDecoration.line * m_lineHeight - scrollY;

        if (m_ghostDecoration.multiline) {
            for (size_t i = 0; i < m_ghostDecoration.lines.size(); ++i) {
                float y = baseY + static_cast<float>(i) * m_lineHeight;
                // First line renders at cursor column; subsequent lines at column 0 indent
                float x = (i == 0) ? baseX : (gutterWidth + 2.0f - scrollX);
                m_renderer->drawText(Point(static_cast<int>(x), static_cast<int>(y)),
                                     String(m_ghostDecoration.lines[i]),
                                     font, gc);
            }
        } else {
            m_renderer->drawText(Point(static_cast<int>(baseX), static_cast<int>(baseY)),
                                 String(m_ghostDecoration.text),
                                 font, gc);
        }
    }

    // ── Diff decorations ──
    for (const auto& diff : m_diffDecorations) {
        Color bgColor;
        if (diff.type == L"add")         bgColor = Color(m_addColor.r, m_addColor.g, m_addColor.b, 50);
        else if (diff.type == L"remove") bgColor = Color(m_removeColor.r, m_removeColor.g, m_removeColor.b, 50);
        else                             bgColor = Color(m_modifyColor.r, m_modifyColor.g, m_modifyColor.b, 50);

        int numLines = diff.endLine - diff.startLine + 1;
        float y = diff.startLine * m_lineHeight - scrollY;

        // Background highlight
        m_renderer->fillRect(
            Rect(static_cast<int>(gutterWidth), static_cast<int>(y),
                 4096,  // full width
                 static_cast<int>(numLines * m_lineHeight)),
            bgColor);

        // Removed-text overlay (strikethrough style)
        if (diff.type == L"remove" || diff.type == L"modify") {
            m_renderer->drawText(
                Point(static_cast<int>(gutterWidth + 2.0f - scrollX), static_cast<int>(y)),
                String(L"- " + diff.oldText), font, m_removeColor);
        }

        // Added-text overlay
        if (diff.type == L"add" || diff.type == L"modify") {
            float offset = (diff.type == L"modify") ? m_lineHeight : 0.0f;
            m_renderer->drawText(
                Point(static_cast<int>(gutterWidth + 2.0f - scrollX), static_cast<int>(y + offset)),
                String(L"+ " + diff.newText), font, m_addColor);
        }
    }
}

// ─── Fade animation ──────────────────────────────────────────────────

void GhostTextRenderer::fadeIn() {
    m_opacity     = 0.0f;
    m_targetAlpha = 1.0f;
    m_fading      = true;
}

void GhostTextRenderer::fadeOut() {
    m_targetAlpha = 0.0f;
    m_fading      = true;
}

bool GhostTextRenderer::tick() {
    if (!m_fading) return false;

    constexpr float kStep = 0.08f;

    if (m_targetAlpha > m_opacity) {
        m_opacity = std::min(m_opacity + kStep, 1.0f);
    } else if (m_targetAlpha < m_opacity) {
        m_opacity = std::max(m_opacity - kStep, 0.0f);
    }

    // Reached target?
    if (std::abs(m_opacity - m_targetAlpha) < 0.01f) {
        m_opacity = m_targetAlpha;
        m_fading  = false;
        if (m_opacity <= 0.0f) {
            clearGhostText();
        }
    }
    return true;  // needs repaint
}

// ─── Key handling ────────────────────────────────────────────────────

bool GhostTextRenderer::handleKeyDown(UINT vk, bool /*shift*/, bool /*ctrl*/) {
    if (!hasGhostText()) return false;

    // Tab → accept
    if (vk == VK_TAB) {
        acceptGhostText();
        return true;
    }

    // Esc → dismiss
    if (vk == VK_ESCAPE) {
        ghostTextDismissed.emit();
        fadeOut();
        return true;
    }

    // Any printable key → start fading
    if (vk >= 0x20 && vk <= 0x7E) {
        fadeOut();
    }

    return false;
}

} // namespace RawrXD


