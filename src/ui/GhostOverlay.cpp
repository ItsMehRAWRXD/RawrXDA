#include "GhostOverlay.h"
#include <sstream>

namespace RawrXD::UI {

GhostOverlay* GhostOverlay::g_instance = nullptr;

GhostOverlay::GhostOverlay() {
    if (!g_instance) {
        g_instance = this;
    }
}

GhostOverlay::~GhostOverlay() {
    Detach();
    if (g_instance == this) {
        g_instance = nullptr;
    }
}

GhostOverlay* GhostOverlay::GetInstance() {
    return g_instance;
}

bool GhostOverlay::Attach(HWND hEditor) {
    if (m_editor) {
        Detach();
    }

    m_editor = hEditor;
    m_origProc = (WNDPROC)SetWindowLongPtr(hEditor, GWLP_WNDPROC, (LONG_PTR)SubclassProc);

    return m_origProc != nullptr;
}

void GhostOverlay::Detach() {
    if (m_editor && m_origProc) {
        SetWindowLongPtr(m_editor, GWLP_WNDPROC, (LONG_PTR)m_origProc);
        m_origProc = nullptr;
        m_editor = nullptr;
    }
}

void GhostOverlay::SetSuggestion(const GhostSuggestion& s) {
    m_suggestion = s;
    m_suggestion.active = true;
    if (m_editor) {
        InvalidateRect(m_editor, nullptr, TRUE);
    }
}

void GhostOverlay::ClearSuggestion() {
    m_suggestion.active = false;
    if (m_editor) {
        InvalidateRect(m_editor, nullptr, TRUE);
    }
}

bool GhostOverlay::HasSuggestion() const {
    return m_suggestion.active;
}

bool GhostOverlay::ApplySuggestion() {
    if (!m_suggestion.active || !m_editor) {
        return false;
    }

    // Replace selected text or insert at cursor
    SendMessage(m_editor, EM_REPLACESEL, TRUE, (LPARAM)m_suggestion.text.c_str());

    m_suggestion.active = false;
    InvalidateRect(m_editor, nullptr, TRUE);
    return true;
}

void GhostOverlay::RejectSuggestion() {
    m_suggestion.active = false;
    if (m_editor) {
        InvalidateRect(m_editor, nullptr, TRUE);
    }
}

LRESULT CALLBACK GhostOverlay::SubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GhostOverlay* self = g_instance;
    if (!self || self->m_editor != hWnd) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_PAINT: {
        // Let original control paint first
        LRESULT res = CallWindowProc(self->m_origProc, hWnd, msg, wParam, lParam);

        // Then draw our ghost overlay
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        self->DrawGhost(hdc);
        EndPaint(hWnd, &ps);

        return res;
    }

    case WM_KEYDOWN: {
        if (self->m_suggestion.active) {
            if (wParam == VK_TAB) {
                self->ApplySuggestion();
                return 0; // Consume Tab
            }
            if (wParam == VK_ESCAPE) {
                self->RejectSuggestion();
                return 0; // Consume Esc
            }
        }
        break;
    }

    case WM_CHAR: {
        // Typing cancels suggestion
        if (self->m_suggestion.active) {
            self->m_suggestion.active = false;
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        break;
    }
    }

    return CallWindowProc(self->m_origProc, hWnd, msg, wParam, lParam);
}

void GhostOverlay::DrawGhost(HDC hdc) {
    if (!m_suggestion.active) return;

    POINT pt = GetCaretPixelPos();

    SetBkMode(hdc, TRANSPARENT);

    if (m_suggestion.type == GhostType::Insert) {
        // Green ghost text for insertions
        SetTextColor(hdc, RGB(120, 200, 120));
        TextOutW(hdc, pt.x, pt.y, m_suggestion.text.c_str(),
                (int)m_suggestion.text.length());
    }
    else if (m_suggestion.type == GhostType::Replace) {
        // Orange ghost text for replacements
        SetTextColor(hdc, RGB(255, 180, 80));
        TextOutW(hdc, pt.x, pt.y, m_suggestion.text.c_str(),
                (int)m_suggestion.text.length());
    }
}

POINT GhostOverlay::GetCaretPixelPos() {
    POINT pt = {0, 0};
    if (!m_editor) return pt;

    // Get current selection (caret position)
    DWORD sel = SendMessage(m_editor, EM_GETSEL, 0, 0);
    int caretIndex = LOWORD(sel);

    // Get line info
    int line = SendMessage(m_editor, EM_LINEFROMCHAR, caretIndex, 0);
    int lineIndex = SendMessage(m_editor, EM_LINEINDEX, line, 0);
    int col = caretIndex - lineIndex;

    // Calculate pixel position
    HDC hdc = GetDC(m_editor);
    if (hdc) {
        // Get font metrics
        int lineHeight = GetLineHeight(hdc);

        // Approximate width using average character width
        TEXTMETRIC tm;
        GetTextMetrics(hdc, &tm);
        int charWidth = tm.tmAveCharWidth;

        pt.x = col * charWidth + 4;
        pt.y = line * lineHeight + 2;

        ReleaseDC(m_editor, hdc);
    }

    return pt;
}

int GhostOverlay::GetLineHeight(HDC hdc) {
    TEXTMETRIC tm{};
    GetTextMetrics(hdc, &tm);
    return tm.tmHeight;
}

} // namespace RawrXD::UI
