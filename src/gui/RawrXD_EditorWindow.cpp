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
      pBrushBackground(nullptr), pBrushLineNumber(nullptr),
      scrollX(0), scrollY(0), cursorPos(0,0), anchorPos(0,0),
      lineHeight(16.0f), charWidth(8.0f), visibleLines(0)
{
    // MiniMonaco buffer initialized with default capacity
    
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
    
    // Draw lines using MiniMonaco buffer
    int startLine = scrollY / (int)lineHeight;
    int endLine = std::min((int)buffer_.lineCount(), startLine + (int)(pRenderTarget->GetSize().height / lineHeight) + 2);
    
    for (int i = startLine; i < endLine; ++i) {
        if (i < 0) continue;
        
        float y = i * lineHeight - scrollY;
        
        // Get line content from MiniMonaco buffer
        std::wstring lineContent = buffer_.lineContent(i);
        
        // Draw selection background if needed (simplified)
        
        // Draw text
        if (!lineContent.empty()) {
            D2D1_RECT_F layoutRect = D2D1::RectF(40.0f - scrollX, y, 2000.0f, y + lineHeight);
            pRenderTarget->DrawText(
                lineContent.c_str(),
                (UINT32)lineContent.length(),
                pTextFormat,
                layoutRect,
                pBrushText
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
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Convert cursor position to buffer offset
    size_t buffer_pos = convertCursorToBufferOffset(cursorPos);
    
    if (ch == '\r') {
        // Enter key - insert newline
        buffer_.insert(buffer_pos, L"\n", 1);
        cursorPos.y++;
        cursorPos.x = 0;
    } else {
        // Insert character at cursor position
        buffer_.insert(buffer_pos, &ch, 1);
        cursorPos.x++;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    perf_metrics_.record_edit(end - start);
    
    ensureCursorVisible();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::onKeyDown(int key) {
    // Convert cursor position to buffer offset for operations
    size_t buffer_pos = convertCursorToBufferOffset(cursorPos);
    
    // Basic navigation
    if (key == VK_LEFT) {
        if (cursorPos.x > 0) {
            cursorPos.x--;
        } else if (cursorPos.y > 0) {
            cursorPos.y--;
            cursorPos.x = (int)buffer_.lineContent(cursorPos.y).length();
        }
    }
    if (key == VK_RIGHT) {
        size_t current_line_length = buffer_.lineContent(cursorPos.y).length();
        if (cursorPos.x < (int)current_line_length) {
            cursorPos.x++;
        } else if (cursorPos.y < (int)buffer_.lineCount() - 1) {
            cursorPos.y++;
            cursorPos.x = 0;
        }
    }
    if (key == VK_UP && cursorPos.y > 0) {
        cursorPos.y--;
        size_t prev_line_length = buffer_.lineContent(cursorPos.y).length();
        if (cursorPos.x > (int)prev_line_length) {
            cursorPos.x = (int)prev_line_length;
        }
    }
    if (key == VK_DOWN && cursorPos.y < (int)buffer_.lineCount() - 1) {
        cursorPos.y++;
        size_t next_line_length = buffer_.lineContent(cursorPos.y).length();
        if (cursorPos.x > (int)next_line_length) {
            cursorPos.x = (int)next_line_length;
        }
    }
    
    // Clamp
    if (cursorPos.y < 0) cursorPos.y = 0;
    if (cursorPos.y >= (int)buffer_.lineCount()) cursorPos.y = (int)buffer_.lineCount() - 1;
    if (cursorPos.x < 0) cursorPos.x = 0;
    size_t current_line_len = buffer_.lineContent(cursorPos.y).length();
    if (cursorPos.x > (int)current_line_len) cursorPos.x = (int)current_line_len;
    
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
    si.nMax = (int)buffer_.lineCount() * (int)lineHeight;
    si.nPage = (int)pRenderTarget->GetSize().height;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void EditorWindow::onLButtonDown(int x, int y) {
    SetFocus(hwnd);
    SetCapture(hwnd);

    // Convert pixel coordinates to document row/col
    // Account for vertical scroll and line height
    int row = (int)((y + scrollY) / lineHeight);
    if (row < 0) row = 0;
    if (row >= (int)buffer_.lineCount()) row = (int)buffer_.lineCount() - 1;

    // Account for gutter (40px) and horizontal scroll
    const float gutterWidth = 40.0f;
    int col = (int)((x - gutterWidth + scrollX) / charWidth);
    if (col < 0) col = 0;

    // Clamp column to line length
    if (row >= 0 && row < (int)buffer_.lineCount()) {
        int lineLen = (int)buffer_.lineContent(row).length();
        if (col > lineLen) col = lineLen;
    }

    // Set cursor and anchor (anchor = selection start; same as cursor for click)
    cursorPos.x = col;
    cursorPos.y = row;
    anchorPos.x = col;
    anchorPos.y = row;

    ensureCursorVisible();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::ensureCursorVisible() {
    if (!pRenderTarget) return;

    D2D1_SIZE_F viewSize = pRenderTarget->GetSize();
    const float gutterWidth = 40.0f;
    float viewportHeight = viewSize.height;
    float viewportWidth = viewSize.width - gutterWidth;

    // Vertical: ensure cursor row is within visible range
    float cursorTop = cursorPos.y * lineHeight;
    float cursorBottom = cursorTop + lineHeight;

    if (cursorTop < (float)scrollY) {
        // Cursor is above the visible area — scroll up
        scrollY = (int)cursorTop;
    } else if (cursorBottom > (float)scrollY + viewportHeight) {
        // Cursor is below the visible area — scroll down
        scrollY = (int)(cursorBottom - viewportHeight);
    }
    if (scrollY < 0) scrollY = 0;

    // Horizontal: ensure cursor column is within visible range
    float cursorLeft = cursorPos.x * charWidth;
    float cursorRight = cursorLeft + charWidth;

    if (cursorLeft < (float)scrollX) {
        // Cursor is to the left of visible area
        scrollX = (int)cursorLeft;
    } else if (cursorRight > (float)scrollX + viewportWidth) {
        // Cursor is to the right of visible area
        scrollX = (int)(cursorRight - viewportWidth);
    }
    if (scrollX < 0) scrollX = 0;

    // Update vertical scrollbar to reflect new scroll position
    SCROLLINFO si = {};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
    si.nMin = 0;
    si.nMax = (int)buffer_.lineCount() * (int)lineHeight;
    si.nPage = (int)viewportHeight;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    InvalidateRect(hwnd, nullptr, FALSE);
}

// MiniMonaco buffer helper methods
size_t EditorWindow::convertCursorToBufferOffset(const Point& cursor) const {
    if (cursor.y >= (int)buffer_.lineCount()) {
        return buffer_.length(); // End of document
    }
    
    size_t line_start = buffer_.lineStart(cursor.y);
    size_t line_length = buffer_.lineContent(cursor.y).length();
    
    // Handle column bounds
    size_t column = std::min(static_cast<size_t>(cursor.x), line_length);
    return line_start + column;
}

Point EditorWindow::convertBufferOffsetToCursor(size_t offset) const {
    size_t line = buffer_.lineFromPos(offset);
    size_t line_start = buffer_.lineStart(line);
    size_t column = offset - line_start;
    
    return {static_cast<int>(line), static_cast<int>(column)};
}

void EditorWindow::updateCursorPosition(size_t buffer_pos) {
    Point new_cursor = convertBufferOffsetToCursor(buffer_pos);
    cursorPos = new_cursor;
    anchorPos = new_cursor;
    ensureCursorVisible();
}

void EditorWindow::setText(const String& text) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Convert String to wstring for MiniMonaco
    std::wstring wtext(text.begin(), text.end());
    buffer_.setText(wtext);
    
    auto end = std::chrono::high_resolution_clock::now();
    perf_metrics_.record_edit(end - start);
    
    InvalidateRect(hwnd, nullptr, FALSE);
}

String EditorWindow::getText() const {
    std::wstring wtext = buffer_.text();
    return String(wtext.begin(), wtext.end());
}

void EditorWindow::appendText(const String& text) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::wstring wtext(text.begin(), text.end());
    size_t pos = buffer_.length();
    buffer_.insert(pos, wtext.data(), wtext.size());
    
    auto end = std::chrono::high_resolution_clock::now();
    perf_metrics_.record_edit(end - start);
    
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::dumpPerformanceStats() const {
    std::cout << "=== MiniMonaco Buffer Performance ===\n";
    std::cout << "Total edits: " << perf_metrics_.edit_count << "\n";
    std::cout << "Total edit time: " 
              << std::chrono::duration<double, std::milli>(perf_metrics_.total_edit_time).count() 
              << " ms\n";
    std::cout << "Average edit time: " 
              << std::chrono::duration<double, std::micro>(perf_metrics_.total_edit_time).count() / perf_metrics_.edit_count 
              << " μs\n";
    std::cout << "Max throughput: " << perf_metrics_.max_throughput << " ops/sec\n";
    
    // MiniMonaco buffer statistics
    std::cout << "Buffer size: " << buffer_.length() << " characters\n";
    std::cout << "Line count: " << buffer_.lineCount() << " lines\n";
}

} // namespace RawrXD
