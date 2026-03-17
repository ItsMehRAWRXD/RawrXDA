// RawrXD_EditorWindow.cpp
// Direct2D text editor implementation

#include "RawrXD_EditorWindow.h"
#include <windowsx.h>
#include <dwrite.h>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")

namespace RawrXD {

EditorWindow::EditorWindow() 
    : hwnd(nullptr), hParent(nullptr),
      pD2DFactory(nullptr), pRenderTarget(nullptr),
      pDWriteFactory(nullptr), pTextFormat(nullptr),
      pBrushText(nullptr), pBrushSelection(nullptr),
      pBrushBackground(nullptr), pBrushLineNumber(nullptr), pBrushGhost(nullptr),
      scrollX(0), scrollY(0), cursorPos(0,0), anchorPos(0,0),
      lineHeight(16.0f), charWidth(8.0f), visibleLines(0)
{
    lines.append(L""); // Empty doc
    
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory));
    
    if(pDWriteFactory) {
        pDWriteFactory->CreateTextFormat(
            L"Consolas",
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            10.0f * (96.0f/72.0f), // 10pt
            L"en-us",
            &pTextFormat
        );
    }
}

EditorWindow::~EditorWindow() {
    discardDeviceResources();
    if(pTextFormat) pTextFormat->Release();
    if(pDWriteFactory) pDWriteFactory->Release();
    if(pD2DFactory) pD2DFactory->Release();
}

bool EditorWindow::create(HWND parent, int x, int y, int w, int h) {
    hParent = parent;
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = sizeof(LONG_PTR);
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_IBEAM);
    wc.hbrBackground = nullptr; // We draw background
    wc.lpszClassName = L"RawrXD_EditorWindow";
    
    RegisterClassEx(&wc);
    
    hwnd = CreateWindowEx(
        0,
        L"RawrXD_EditorWindow",
        L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
        x, y, w, h,
        parent,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    
    return hwnd != nullptr;
}

LRESULT CALLBACK EditorWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EditorWindow* self;
    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
        self = reinterpret_cast<EditorWindow*>(lpcs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<EditorWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (self) {
        switch (msg) {
            case WM_PAINT:
                self->onPaint();
                ValidateRect(hwnd, nullptr);
                return 0;
            case WM_SIZE:
                self->onResize(LOWORD(lParam), HIWORD(lParam));
                return 0;
            case WM_MOUSEWHEEL:
                self->onScroll(0, -GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 3);
                return 0;
            case WM_CHAR:
                self->onChar(static_cast<wchar_t>(wParam));
                return 0;
            case WM_KEYDOWN:
                self->onKeyDown(static_cast<int>(wParam));
                return 0;
            case WM_LBUTTONDOWN:
                self->onLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_TIMER:
                self->onTimer(wParam);
                return 0;
            case WM_ERASEBKGND:
                return 1; // Prevent flickering
        }
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void EditorWindow::createDeviceResources() {
    if (!pRenderTarget && hwnd) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        
        pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &pRenderTarget
        );
        
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray), &pBrushText);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.2f, 0.4f, 0.8f, 0.4f), &pBrushSelection);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.12f, 0.12f), &pBrushBackground);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.3f, 0.3f, 0.3f), &pBrushLineNumber);
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.8f), &pBrushGhost);
    }
}

void EditorWindow::discardDeviceResources() {
    if (pRenderTarget) {
        pRenderTarget->Release();
        pRenderTarget = nullptr;
    }
    if (pBrushText) { pBrushText->Release(); pBrushText = nullptr; }
    if (pBrushSelection) { pBrushSelection->Release(); pBrushSelection = nullptr; }
    if (pBrushBackground) { pBrushBackground->Release(); pBrushBackground = nullptr; }
    if (pBrushLineNumber) { pBrushLineNumber->Release(); pBrushLineNumber = nullptr; }
    if (pBrushGhost) { pBrushGhost->Release(); pBrushGhost = nullptr; }
}

void EditorWindow::onPaint() {
    createDeviceResources();
    if (!pRenderTarget) return;
    
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(0.12f, 0.12f, 0.12f));
    
    D2D1_RECT_F visibleRect = D2D1::RectF(
        (float)scrollX, 
        (float)scrollY, 
        (float)scrollX + pRenderTarget->GetSize().width, 
        (float)scrollY + pRenderTarget->GetSize().height
    );
    
    // Draw lines
    int startLine = scrollY / (int)lineHeight;
    int endLine = std::min(lines.count(), startLine + (int)(pRenderTarget->GetSize().height / lineHeight) + 2);
    
    for (int i = startLine; i < endLine; ++i) {
        if (i < 0) continue;
        
        float y = i * lineHeight - scrollY;
        const String& line = lines[i];
        
        // Draw selection background if needed (simplified)
        
        // Draw text
        if (!line.isEmpty()) {
            D2D1_RECT_F layoutRect = D2D1::RectF(40.0f - scrollX, y, 2000.0f, y + lineHeight);
            pRenderTarget->DrawText(
                line.constData(),
                line.length(),
                pTextFormat,
                layoutRect,
                pBrushText
            );
            
            // Draw Ghost Text if cursor is on this line and at the end
            if (!ghostText.isEmpty() && i == cursorPos.y && cursorPos.x == line.length()) {
                float lineWidth = line.length() * charWidth; // Approximate
                D2D1_RECT_F ghostRect = D2D1::RectF(40.0f - scrollX + lineWidth, y, 2000.0f, y + lineHeight);
                pRenderTarget->DrawText(
                    ghostText.constData(),
                    ghostText.length(),
                    pTextFormat,
                    ghostRect,
                    pBrushGhost
                );
            }
        } else if (i == cursorPos.y && !ghostText.isEmpty()) {
            // Ghost text on empty line
             D2D1_RECT_F ghostRect = D2D1::RectF(40.0f - scrollX, y, 2000.0f, y + lineHeight);
             pRenderTarget->DrawText(
                 ghostText.constData(),
                 ghostText.length(),
                 pTextFormat,
                 ghostRect,
                 pBrushGhost
             );
        }
    }
    
    HRESULT hr = pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        discardDeviceResources();
    }
}

void EditorWindow::onResize(int w, int h) {
    if (pRenderTarget) {
        pRenderTarget->Resize(D2D1::SizeU(w, h));
    }
}

void EditorWindow::onChar(wchar_t ch) {
    if (ch < 32 && ch != '\t' && ch != '\n' && ch != '\r') return;
    
    // Clear ghost text on typing
    ghostText = L"";

    String& line = lines[cursorPos.y];
    if (ch == '\r') {
        // Simple enter key handling
        String rest = line.mid(cursorPos.x);
        line = line.left(cursorPos.x);
        lines.insert(cursorPos.y + 1, rest);
        cursorPos.y++;
        cursorPos.x = 0;
    } else {
        line.insert(cursorPos.x, String(ch));
        cursorPos.x++;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    
    // Trigger auto-complete timer
    SetTimer(hwnd, 1, 1000, nullptr);
}

void EditorWindow::onKeyDown(int key) {
    // Check TAB for ghost text acceptance
    if (key == VK_TAB && !ghostText.isEmpty()) {
        acceptGhostText();
        return;
    }
    
    // Basic navigation
    if (key == VK_LEFT) cursorPos.x--;
    if (key == VK_RIGHT) cursorPos.x++;
    if (key == VK_UP) cursorPos.y--;
    if (key == VK_DOWN) cursorPos.y++;
    
    // Clamp
    if (cursorPos.y < 0) cursorPos.y = 0;
    if (cursorPos.y >= lines.count()) cursorPos.y = lines.count() - 1;
    if (cursorPos.x < 0) cursorPos.x = 0;
    if (cursorPos.x > lines[cursorPos.y].length()) cursorPos.x = lines[cursorPos.y].length();
    
    ensureCursorVisible();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::onScroll(int dx, int dy) {
    scrollY += dy * (int)lineHeight;
    if (scrollY < 0) scrollY = 0;
    InvalidateRect(hwnd, nullptr, FALSE);
    
    // Update ScrollInfo
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = lines.count() * (int)lineHeight;
    si.nPage = (int)pRenderTarget->GetSize().height;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void EditorWindow::onLButtonDown(int x, int y) {
    SetFocus(hwnd);
    
    int line = (scrollY + y) / (int)lineHeight;
    if (line < 0) line = 0;
    if (line >= lines.count()) line = lines.count() - 1;
    
    cursorPos.y = line;
    
    // Naive column calculation (assuming fixed width font)
    // 40.0f matches the layoutRect offset in onPaint
    cursorPos.x = (int)((x + scrollX - 40.0f) / charWidth); 
    if (cursorPos.x < 0) cursorPos.x = 0;
    if (cursorPos.x > lines[line].length()) cursorPos.x = lines[line].length();
    
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::ensureCursorVisible() {
    if (!pRenderTarget) return;
    
    float viewportHeight = pRenderTarget->GetSize().height;
    if (viewportHeight <= 0) return;

    float cursorY = cursorPos.y * lineHeight;
    
    if (cursorY < scrollY) {
        scrollY = (int)cursorY;
    } else if (cursorY + lineHeight > scrollY + viewportHeight) {
        scrollY = (int)(cursorY + lineHeight - viewportHeight);
    }
    
    // Update scrollbar
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void EditorWindow::setText(const String& text) {
    // Split into lines (naively)
    lines.clear();
    // TODO: Use better split
    lines.append(text); 
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::appendText(const String& text) {
    if (lines.isEmpty()) lines.append(L"");
    lines.last().append(text);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::copy() {
    // Placeholder: Copy current line
    if (cursorPos.y >= 0 && cursorPos.y < lines.count()) {
        Application::getInstance()->clipboardSetText(lines[cursorPos.y]);
    }
}

void EditorWindow::paste() {
    String text = Application::getInstance()->clipboardText();
    if (text.isEmpty()) return;
    
    // Naive paste at cursor
    if (cursorPos.y < lines.count()) {
        lines[cursorPos.y].insert(cursorPos.x, text);
        cursorPos.x += text.length();
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::cut() {
    copy();
    // Naive delete line
    if (cursorPos.y < lines.count() && lines.count() > 1) {
        lines.removeAt(cursorPos.y);
        InvalidateRect(hwnd, nullptr, FALSE);
    } else {
        lines[0] = ""; 
    }
}

void EditorWindow::undo() {
    // TODO: Command stack
}

void EditorWindow::redo() {
    // TODO: Command stack
}

void EditorWindow::setGhostText(const String& text) {
    ghostText = text;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::acceptGhostText() {
    if (ghostText.isEmpty()) return;
    
    // Insert ghost text at cursor
    if (lines.isEmpty()) lines.append(L"");
    if (cursorPos.y < lines.count()) {
        lines[cursorPos.y].append(ghostText);
        cursorPos.x += ghostText.length();
    }
    ghostText = L""; // Clear
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::onTimer(UINT_PTR id) {
    if (id == 1) {
        KillTimer(hwnd, 1);
        // Trigger agent completion (Simplified for this context)
        // In a full build, this would dispatch to a background thread to query ModelCaller
        // For now, we demonstrate the hook is active
        // setGhostText(L""); 
    }
}

} // namespace RawrXD
