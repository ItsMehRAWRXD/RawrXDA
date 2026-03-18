// ============================================================================
// Win32IDE_SmoothScroll.cpp — Tier 1 Cosmetic #1: Smooth Scroll + Caret Animation
// ============================================================================
// Implements 60fps smooth scrolling via WM_MOUSEWHEEL delta interpolation
// and animated caret (fade in/out) for modern editor feel.
//
// Pattern:  Timer-based interpolation, no exceptions
// Threading: UI thread only (WM_TIMER callbacks)
// ============================================================================

#include "Win32IDE.h"
#include <cmath>

#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << "[INFO] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif

// ============================================================================
// SMOOTH SCROLL INITIALIZATION
// ============================================================================

void Win32IDE::initSmoothScroll()
{
    m_smoothScrollTarget   = 0.0f;
    m_smoothScrollCurrent  = 0.0f;
    m_smoothScrollVelocity = 0.0f;
    m_smoothScrollActive   = false;
    m_smoothScrollAccumDelta = 0;

    m_caretOpacity   = 1.0f;
    m_caretVisible   = true;
    m_caretPos       = {0, 0};
    m_caretHeight    = m_settings.fontSize + 4;

    // Start caret blink animation timer if enabled
    if (m_settings.caretAnimationEnabled && m_hwndEditor) {
        SetTimer(m_hwndMain, CARET_ANIM_TIMER_ID, CARET_ANIM_INTERVAL_MS, nullptr);
    }

    RAWRXD_LOG_INFO("Smooth scroll initialized (frames=" << m_settings.smoothScrollFrames << ")");
}

void Win32IDE::shutdownSmoothScroll()
{
    if (m_smoothScrollActive) {
        KillTimer(m_hwndMain, SMOOTH_SCROLL_TIMER_ID);
        m_smoothScrollActive = false;
    }
    KillTimer(m_hwndMain, CARET_ANIM_TIMER_ID);
}

// ============================================================================
// SMOOTH SCROLL — WM_MOUSEWHEEL Hook Entry Point
// ============================================================================

void Win32IDE::startSmoothScroll(int delta)
{
    if (!m_settings.smoothScrollEnabled) {
        // Fallback: immediate scroll (legacy behavior)
        if (m_hwndEditor) {
            SendMessage(m_hwndEditor, WM_VSCROLL,
                        delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            SendMessage(m_hwndEditor, WM_VSCROLL,
                        delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
            SendMessage(m_hwndEditor, WM_VSCROLL,
                        delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        }
        return;
    }

    // Accumulate delta for multi-notch scrolls
    m_smoothScrollAccumDelta += delta;

    // Convert wheel delta to target scroll lines
    // WHEEL_DELTA (120) = 3 lines by default
    float linesToScroll = static_cast<float>(m_smoothScrollAccumDelta) / 120.0f * 3.0f;
    m_smoothScrollTarget += linesToScroll;
    m_smoothScrollAccumDelta = 0;

    // Cap the maximum scroll target to avoid runaway
    float maxDelta = 60.0f; // max 60 lines of accumulated scroll
    if (m_smoothScrollTarget > maxDelta) m_smoothScrollTarget = maxDelta;
    if (m_smoothScrollTarget < -maxDelta) m_smoothScrollTarget = -maxDelta;

    // Start the interpolation timer if not already running
    if (!m_smoothScrollActive) {
        m_smoothScrollActive = true;
        SetTimer(m_hwndMain, SMOOTH_SCROLL_TIMER_ID, SMOOTH_SCROLL_INTERVAL_MS, nullptr);
    }
}

// ============================================================================
// SMOOTH SCROLL TIMER — Interpolation (called every ~16ms = 60fps)
// ============================================================================

void Win32IDE::onSmoothScrollTimer()
{
    if (!m_hwndEditor) {
        m_smoothScrollActive = false;
        KillTimer(m_hwndMain, SMOOTH_SCROLL_TIMER_ID);
        return;
    }

    // Exponential easing: move 20% toward target each frame
    const float easing = 0.20f;
    const float epsilon = 0.05f; // threshold to stop

    float delta = m_smoothScrollTarget * easing;
    m_smoothScrollTarget -= delta;
    m_smoothScrollCurrent += delta;

    // Apply integer line scrolls when accumulated enough
    int linesToApply = static_cast<int>(m_smoothScrollCurrent);
    if (linesToApply != 0) {
        m_smoothScrollCurrent -= static_cast<float>(linesToApply);

        // Scroll the editor
        WPARAM scrollCmd = (linesToApply < 0) ? SB_LINEUP : SB_LINEDOWN;
        int absLines = abs(linesToApply);
        for (int i = 0; i < absLines; i++) {
            SendMessage(m_hwndEditor, WM_VSCROLL, scrollCmd, 0);
        }

        // Update line numbers and minimap
        if (m_hwndLineNumbers) {
            InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
        }
        if (m_hwndMinimap && m_minimapVisible) {
            InvalidateRect(m_hwndMinimap, nullptr, FALSE);
        }
    }

    // Check if scroll is complete
    if (fabs(m_smoothScrollTarget) < epsilon && fabs(m_smoothScrollCurrent) < epsilon) {
        m_smoothScrollTarget = 0.0f;
        m_smoothScrollCurrent = 0.0f;
        m_smoothScrollActive = false;
        KillTimer(m_hwndMain, SMOOTH_SCROLL_TIMER_ID);
    }
}

// ============================================================================
// CARET ANIMATION — Smooth blink (fade in/out)
// ============================================================================

void Win32IDE::onCaretAnimTimer()
{
    if (!m_settings.caretAnimationEnabled || !m_hwndEditor) return;

    // Toggle caret visibility with smooth transition
    m_caretVisible = !m_caretVisible;

    // Smooth opacity: fade from 1.0 → 0.3 → 1.0
    m_caretOpacity = m_caretVisible ? 1.0f : 0.3f;

    // Get current caret position from editor
    DWORD selStart = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, 0);
    POINTL caretPt{};
    if (SendMessage(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&caretPt, selStart) != -1) {
        m_caretPos.x = caretPt.x;
        m_caretPos.y = caretPt.y;
    }

    // Invalidate only the caret area for efficient repaint
    RECT caretRect;
    caretRect.left   = m_caretPos.x - 1;
    caretRect.top    = m_caretPos.y;
    caretRect.right  = m_caretPos.x + 2;
    caretRect.bottom = m_caretPos.y + m_caretHeight;
    InvalidateRect(m_hwndEditor, &caretRect, FALSE);
}

void Win32IDE::renderAnimatedCaret(HDC hdc)
{
    if (!m_settings.caretAnimationEnabled) return;

    // Draw a VS Code-style thin caret with current opacity
    int alpha = static_cast<int>(m_caretOpacity * 255.0f);
    COLORREF caretColor = RGB(
        (212 * alpha) / 255,
        (212 * alpha) / 255,
        (212 * alpha) / 255
    );

    HPEN hPen = CreatePen(PS_SOLID, 2, caretColor);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);

    MoveToEx(hdc, m_caretPos.x, m_caretPos.y, nullptr);
    LineTo(hdc, m_caretPos.x, m_caretPos.y + m_caretHeight);

    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}
