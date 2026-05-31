// ============================================================================
// Win32IDE_CaretAnimation.cpp — Caret Animation Implementation
// ============================================================================
// Provides smooth caret animations and blinking:
//   - Configurable blink rate
//   - Smooth position transitions
//   - Animation timing controls
//
// ============================================================================

#include "Win32IDE.h"
#include <algorithm>

// ============================================================================
// CONSTANTS
// ============================================================================
static const UINT_PTR CARET_BLINK_TIMER_ID = 9999;
static const int DEFAULT_CARET_BLINK_RATE = 500; // milliseconds

// ============================================================================
// CARET ANIMATION METHODS
// ============================================================================

void Win32IDE::initCaretAnimation() {
    m_caretAnimationEnabled = true;
    m_caretBlinking = true;
    m_caretBlinkRate = DEFAULT_CARET_BLINK_RATE;
    m_caretBlinkTimer = 0;

    // Start caret blinking
    startCaretBlink();

    LOG_INFO("Caret animation initialized");
}

void Win32IDE::shutdownCaretAnimation() {
    stopCaretBlink();
    m_caretAnimationEnabled = false;
}

void Win32IDE::startCaretBlink() {
    if (m_caretAnimationEnabled && m_hwndEditor && !m_caretBlinkTimer) {
        m_caretBlinkTimer = SetTimer(m_hwndMain, CARET_BLINK_TIMER_ID, m_caretBlinkRate, nullptr);
        m_caretBlinking = true;
    }
}

void Win32IDE::stopCaretBlink() {
    if (m_caretBlinkTimer) {
        KillTimer(m_hwndMain, m_caretBlinkTimer);
        m_caretBlinkTimer = 0;
    }
    m_caretBlinking = false;
}

void Win32IDE::setCaretBlinkRate(int milliseconds) {
    m_caretBlinkRate = std::max(100, std::min(2000, milliseconds)); // Clamp to reasonable range

    // Restart timer with new rate
    if (m_caretBlinkTimer) {
        stopCaretBlink();
        startCaretBlink();
    }
}

void Win32IDE::animateCaretToPosition(int line, int column) {
    if (!m_hwndEditor) return;

    // Convert 1-based line/column to absolute character index.
    const int line0 = std::max(0, line - 1);
    const LRESULT lineStart = SendMessage(m_hwndEditor, EM_LINEINDEX, (WPARAM)line0, 0);
    if (lineStart < 0) return;

    const int requestedCol0 = std::max(0, column - 1);
    const int lineLength = static_cast<int>(SendMessage(m_hwndEditor, EM_LINELENGTH, (WPARAM)lineStart, 0));
    const int clampedCol0 = std::min(requestedCol0, std::max(0, lineLength));
    const int charPos = static_cast<int>(lineStart) + clampedCol0;

    // Smooth scroll: if the target line is off-screen, scroll incrementally
    if (m_caretAnimationEnabled) {
        int firstVisible = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        RECT editorRect;
        GetClientRect(m_hwndEditor, &editorRect);
        // Estimate visible line count from editor height and font metrics
        HDC hdc = GetDC(m_hwndEditor);
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        ReleaseDC(m_hwndEditor, hdc);
        int visibleLines = (tm.tmHeight > 0) ? (editorRect.bottom - editorRect.top) / tm.tmHeight : 30;

        int delta = line0 - firstVisible;
        // If target is off-screen, animate the scroll in steps
        if (delta < 0 || delta >= visibleLines) {
            int scrollTarget = line0 - visibleLines / 3; // place target at ~1/3 from top
            int scrollDelta = scrollTarget - firstVisible;
            // Animate in up to 6 steps for a smooth feel
            int steps = std::min(6, std::max(1, std::abs(scrollDelta) / 4));
            for (int i = 1; i <= steps; ++i) {
                int partial = scrollDelta * i / steps;
                int stepDelta = partial - (scrollDelta * (i - 1) / steps);
                if (stepDelta != 0)
                    SendMessage(m_hwndEditor, EM_LINESCROLL, 0, stepDelta);
                // Yield briefly for visual effect (~16ms per step)
                MSG msg;
                DWORD deadline = GetTickCount() + 16;
                while (GetTickCount() < deadline) {
                    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                        TranslateMessage(&msg);
                        DispatchMessage(&msg);
                    }
                }
            }
        }
    }

    SendMessage(m_hwndEditor, EM_SETSEL, (WPARAM)charPos, (LPARAM)charPos);
}

bool Win32IDE::isCaretAnimationEnabled() const {
    return m_caretAnimationEnabled;
}

void Win32IDE::toggleCaretAnimation() {
    m_caretAnimationEnabled = !m_caretAnimationEnabled;

    if (m_caretAnimationEnabled) {
        startCaretBlink();
    } else {
        stopCaretBlink();
    }
}
