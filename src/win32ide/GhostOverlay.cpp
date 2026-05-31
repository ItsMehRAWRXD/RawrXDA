// ============================================================================
// GhostOverlay.cpp - Inline Diff Preview Implementation
// ============================================================================

#include "GhostOverlay.h"
#include <string>

namespace RawrXD {
namespace UI {

GhostOverlay::GhostOverlay() = default;
GhostOverlay::~GhostOverlay() { Detach(); }

bool GhostOverlay::Attach(HWND hEditor) {
    if (m_attached || !hEditor) return false;
    
    m_editor = hEditor;
    m_origProc = (WNDPROC)SetWindowLongPtr(hEditor, GWLP_WNDPROC, (LONG_PTR)SubclassProc);
    
    // Store this pointer in window data for callback access
    SetWindowLongPtr(hEditor, GWLP_USERDATA, (LONG_PTR)this);
    
    m_attached = true;
    return true;
}

void GhostOverlay::Detach() {
    if (!m_attached || !m_editor) return;
    
    SetWindowLongPtr(m_editor, GWLP_WNDPROC, (LONG_PTR)m_origProc);
    m_editor = nullptr;
    m_origProc = nullptr;
    m_attached = false;
}

void GhostOverlay::SetSuggestion(const GhostSuggestion& suggestion) {
    m_suggestion = suggestion;
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

void GhostOverlay::Accept() {
    ApplySuggestion();
}

void GhostOverlay::Reject() {
    RejectSuggestion();
}

std::wstring GhostOverlay::GetStatusText() const {
    if (!m_suggestion.active) return L"";
    
    switch (m_suggestion.type) {
    case GhostType::Insert:
        return L"[Ghost] Insert: Tab=Accept, Esc=Reject";
    case GhostType::Replace:
        return L"[Ghost] Replace: Tab=Accept, Esc=Reject";
    case GhostType::Delete:
        return L"[Ghost] Delete: Tab=Accept, Esc=Reject";
    }
    return L"";
}

LRESULT CALLBACK GhostOverlay::SubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GhostOverlay* self = (GhostOverlay*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!self) {
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    
    switch (msg) {
    case WM_PAINT: {
        // Let editor paint first
        LRESULT res = CallWindowProc(self->m_origProc, hWnd, msg, wParam, lParam);
        
        // Then draw ghost overlay
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
                return 0; // Swallow Tab
            }
            if (wParam == VK_ESCAPE) {
                self->RejectSuggestion();
                return 0; // Swallow Esc
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
    
    if (m_suggestion.isMultiFile) {
        SetTextColor(hdc, kColorMultiFile);
        std::wstring text = L"[Patch: " + m_suggestion.filePath + L"] " + m_suggestion.text;
        TextOutW(hdc, pt.x, pt.y, text.c_str(), (int)text.length());
        return;
    }
    
    switch (m_suggestion.type) {
    case GhostType::Insert:
        SetTextColor(hdc, kColorInsert);
        TextOutW(hdc, pt.x, pt.y, m_suggestion.text.c_str(), (int)m_suggestion.text.length());
        break;
        
    case GhostType::Replace: {
        SetTextColor(hdc, kColorReplace);
        // Draw strikethrough on original
        if (!m_suggestion.original.empty()) {
            SIZE origSize;
            GetTextExtentPoint32W(hdc, m_suggestion.original.c_str(), (int)m_suggestion.original.length(), &origSize);
            
            // Draw original with strikethrough
            SetTextColor(hdc, kColorDelete);
            TextOutW(hdc, pt.x, pt.y, m_suggestion.original.c_str(), (int)m_suggestion.original.length());
            
            // Strikethrough line
            HPEN pen = CreatePen(PS_SOLID, 1, kColorDelete);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            MoveToEx(hdc, pt.x, pt.y + origSize.cy / 2, nullptr);
            LineTo(hdc, pt.x + origSize.cx, pt.y + origSize.cy / 2);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
            
            // Draw replacement after
            SetTextColor(hdc, kColorReplace);
            TextOutW(hdc, pt.x + origSize.cx + 10, pt.y, m_suggestion.text.c_str(), (int)m_suggestion.text.length());
        } else {
            TextOutW(hdc, pt.x, pt.y, m_suggestion.text.c_str(), (int)m_suggestion.text.length());
        }
        break;
    }
    
    case GhostType::Delete:
        SetTextColor(hdc, kColorDelete);
        TextOutW(hdc, pt.x, pt.y, m_suggestion.text.c_str(), (int)m_suggestion.text.length());
        break;
    }
}

void GhostOverlay::ApplySuggestion() {
    if (!m_suggestion.active || !m_editor) return;
    
    // Replace selection with suggestion text
    SendMessageW(m_editor, EM_REPLACESEL, TRUE, (LPARAM)m_suggestion.text.c_str());
    
    m_suggestion.active = false;
    InvalidateRect(m_editor, nullptr, TRUE);
}

void GhostOverlay::RejectSuggestion() {
    m_suggestion.active = false;
    if (m_editor) {
        InvalidateRect(m_editor, nullptr, TRUE);
    }
}

POINT GhostOverlay::GetCaretPixelPos() {
    POINT pt = { 0, 0 };
    if (!m_editor) return pt;
    
    // Get caret position
    DWORD sel = SendMessage(m_editor, EM_GETSEL, 0, 0);
    int caretIndex = LOWORD(sel);
    
    int line = SendMessage(m_editor, EM_LINEFROMCHAR, caretIndex, 0);
    int lineIndex = SendMessage(m_editor, EM_LINEINDEX, line, 0);
    int col = caretIndex - lineIndex;
    
    // Get font metrics
    HDC hdc = GetDC(m_editor);
    HFONT font = (HFONT)SendMessage(m_editor, WM_GETFONT, 0, 0);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    
    // Measure text up to column
    std::wstring text(col, L'X');
    SIZE sz;
    GetTextExtentPoint32W(hdc, text.c_str(), col, &sz);
    
    int lineHeight = GetLineHeight(hdc);
    
    SelectObject(hdc, oldFont);
    ReleaseDC(m_editor, hdc);
    
    // Get editor scroll position
    int firstVisible = SendMessage(m_editor, EM_GETFIRSTVISIBLELINE, 0, 0);
    
    pt.x = sz.cx + 4;
    pt.y = (line - firstVisible) * lineHeight + 2;
    
    return pt;
}

int GhostOverlay::GetLineHeight(HDC hdc) {
    TEXTMETRIC tm{};
    GetTextMetrics(hdc, &tm);
    return tm.tmHeight;
}

} // namespace UI
} // namespace RawrXD
