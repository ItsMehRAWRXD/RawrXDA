#include "Win32IDE_InlineCompletion.h"
#include <windows.h>
#include <richedit.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "gdi32.lib")

namespace RawrXD::UX {

InlineCompletionEngine& InlineCompletionEngine::instance() {
    static InlineCompletionEngine inst;
    return inst;
}

bool InlineCompletionEngine::initialize(HWND hwndEditor) {
    if (!hwndEditor || !IsWindow(hwndEditor)) return false;
    
    m_hwndEditor = hwndEditor;
    
    // Subclass editor for Tab/ESC handling and custom rendering
    SetWindowLongPtr(hwndEditor, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    m_originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(
        hwndEditor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editorSubclassProc)
    ));
    
    return true;
}

void InlineCompletionEngine::shutdown() {
    if (m_hwndEditor && m_originalWndProc) {
        SetWindowLongPtr(m_hwndEditor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalWndProc));
        m_hwndEditor = nullptr;
    }
}

void InlineCompletionEngine::requestSuggestion(const std::string& currentLine, size_t cursorPos) {
    if (!m_enabled || m_pendingRequest) return;
    
    m_pendingRequest = true;
    m_current.triggerLine = currentLine;
    m_current.cursorPos = cursorPos;
    
    // Debounce: trigger timer in editor window
    SetTimer(m_hwndEditor, 0xC001, m_delayMs, nullptr);
}

void InlineCompletionEngine::onModelResponse(const std::string& completion, double latencyMs) {
    m_pendingRequest = false;
    
    if (completion.empty()) return;
    
    m_current.text = completion;
    m_current.latencyMs = latencyMs;
    m_current.generatedAt = std::chrono::steady_clock::now();
    
    m_metrics.totalRequests++;
    
    // Refresh editor to show ghost text
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

void InlineCompletionEngine::renderGhostText(HDC hdc, const RECT& clientRect) {
    if (m_current.text.empty()) return;
    
    POINT pt;
    // EM_POSFROMCHAR for RichEdit: returns window coords
    SendMessage(m_hwndEditor, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&pt), m_current.cursorPos);
    
    // Color: Grey (#6B7280 Style)
    SetTextColor(hdc, RGB(128, 128, 128));
    SetBkMode(hdc, TRANSPARENT);
    
    HFONT hFont = reinterpret_cast<HFONT>(SendMessage(m_hwndEditor, WM_GETFONT, 0, 0));
    HFONT hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    
    RECT textRect = { pt.x, pt.y, clientRect.right, pt.y + 20 };
    std::wstring wtext(m_current.text.begin(), m_current.text.end());
    
    DrawTextW(hdc, wtext.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    
    SelectObject(hdc, hOldFont);
}

void InlineCompletionEngine::acceptCurrentSuggestion() {
    if (m_current.text.empty()) return;
    
    // Insert text at current selection/cursor
    SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(m_current.text.c_str()));
    
    m_metrics.accepted++;
    dismissSuggestion();
}

void InlineCompletionEngine::dismissSuggestion() {
    m_current.text.clear();
    InvalidateRect(m_hwndEditor, nullptr, FALSE);
}

LRESULT CALLBACK InlineCompletionEngine::editorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* self = reinterpret_cast<InlineCompletionEngine*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProc(hwnd, msg, wParam, lParam);
    
    switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_TAB && self->hasActiveSuggestion()) {
                self->acceptCurrentSuggestion();
                return 0; // Prevent indent
            }
            if (wParam == VK_ESCAPE && self->hasActiveSuggestion()) {
                self->dismissSuggestion();
                return 0;
            }
            if (self->hasActiveSuggestion()) {
                self->dismissSuggestion();
            }
            break;
            
        case WM_PAINT: {
            // Paint existing content
            LRESULT ret = CallWindowProc(self->m_originalWndProc, hwnd, msg, wParam, lParam);
            
            // Paint ghost text on top
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            self->renderGhostText(hdc, rc);
            EndPaint(hwnd, &ps);
            return ret;
        }
    }
    
    return CallWindowProc(self->m_originalWndProc, hwnd, msg, wParam, lParam);
}

} // namespace RawrXD::UX
