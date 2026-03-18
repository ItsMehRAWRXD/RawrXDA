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
    // For now, just set the position immediately
    // Future enhancement: implement smooth animation
    if (m_hwndEditor) {
        // Convert 1-based line/column to absolute character index.
        const int line0 = std::max(0, line - 1);
        const LRESULT lineStart = SendMessage(m_hwndEditor, EM_LINEINDEX, (WPARAM)line0, 0);
        if (lineStart < 0) {
            return;
        }

        const int requestedCol0 = std::max(0, column - 1);
        const int lineLength = static_cast<int>(SendMessage(m_hwndEditor, EM_LINELENGTH, (WPARAM)lineStart, 0));
        const int clampedCol0 = std::min(requestedCol0, std::max(0, lineLength));
        const int charPos = static_cast<int>(lineStart) + clampedCol0;

        SendMessage(m_hwndEditor, EM_SETSEL, (WPARAM)charPos, (LPARAM)charPos);
    }
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