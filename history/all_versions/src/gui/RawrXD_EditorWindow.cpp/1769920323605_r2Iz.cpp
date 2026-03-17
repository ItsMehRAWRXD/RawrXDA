// RawrXD_EditorWindow.cpp
// Direct2D text editor implementation

#include "RawrXD_EditorWindow.h"
#include "../ai_integration_hub.h" 
#include "../ai_model_caller.h"
#include "../RawrXD_Application.h"
#include <windowsx.h>
#include <dwrite.h>
#include <commdlg.h>
#include <algorithm>
#include <thread>
#include <vector>

#pragma comment(lib, "d2d1.lib")

#define WM_AGENT_COMPLETE (WM_USER + 1)

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
            case WM_LBUTTONUP:
                self->onLButtonUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
                return 0;
            case WM_MOUSEMOVE:
                self->onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
        
        // Draw selection background
        if (cursorPos.y != anchorPos.y || cursorPos.x != anchorPos.x) {
             Point p1 = anchorPos;
             Point p2 = cursorPos;
             if (p1.y > p2.y || (p1.y == p2.y && p1.x > p2.x)) std::swap(p1, p2);
             
             if (i >= p1.y && i <= p2.y) {
                 float startX = 40.0f;
                 float endX = 40.0f + (line.length() + 0.5f) * charWidth; // Cover newline
                 
                 if (i == p1.y) startX += p1.x * charWidth;
                 if (i == p2.y) endX = 40.0f + p2.x * charWidth;
                 
                 D2D1_RECT_F selRect = D2D1::RectF(startX - scrollX, y, endX - scrollX, y + lineHeight);
                 pRenderTarget->FillRectangle(selRect, pBrushSelection);
             }
        }
        
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
    if (ch < 32 && ch != '\t' && ch != '\n' && ch != '\r' && ch != '\b') return;
    
    // Clear ghost text on typing
    ghostText = L"";

    // Handle Backspace
    if (ch == '\b') {
        if (cursorPos.x > 0) {
             String deleted = lines[cursorPos.y].mid(cursorPos.x - 1, 1);
             EditorCommand cmd;
             cmd.type = EditorCommand::Delete;
             cmd.pos = {cursorPos.x - 1, cursorPos.y};
             cmd.text = deleted;
             pushCommand(cmd);

             lines[cursorPos.y].remove(cursorPos.x - 1, 1);
             cursorPos.x--;
        } else if (cursorPos.y > 0) {
             EditorCommand cmd;
             cmd.type = EditorCommand::Delete;
             cmd.pos = { lines[cursorPos.y-1].length(), cursorPos.y - 1 };
             cmd.text = L"\n";
             pushCommand(cmd);

             cursorPos.x = lines[cursorPos.y - 1].length();
             cursorPos.y--;
             lines[cursorPos.y] += lines[cursorPos.y+1];
             lines.removeAt(cursorPos.y+1);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    String& line = lines[cursorPos.y];
    if (ch == '\r') {
        EditorCommand cmd;
        cmd.type = EditorCommand::Insert;
        cmd.pos = cursorPos;
        cmd.text = L"\n";
        pushCommand(cmd);

        String rest = line.mid(cursorPos.x);
        line = line.left(cursorPos.x);
        lines.insert(cursorPos.y + 1, rest);
        cursorPos.y++;
        cursorPos.x = 0;
    } else {
        // String(ch) might need explicit cast or loop if proper constructor not available
        wchar_t buf[2] = {ch, 0};
        String s(buf);

        EditorCommand cmd;
        cmd.type = EditorCommand::Insert;
        cmd.pos = cursorPos;
        cmd.text = s;
        pushCommand(cmd);

        line.insert(cursorPos.x, s);
        cursorPos.x++;
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    
    // Trigger auto-complete timer
    SetTimer(hwnd, 1, 600, nullptr);
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
    SetCapture(hwnd);
    isSelecting = true;
    
    Point hit = hitTest(x, y);
    cursorPos = hit;
    if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
        anchorPos = cursorPos;
    }
    
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::onLButtonUp(int x, int y) {
    if (isSelecting) {
        ReleaseCapture();
        isSelecting = false;
    }
}

void EditorWindow::onMouseMove(int x, int y) {
    if (isSelecting) {
        Point hit = hitTest(x, y);
        
        // Auto-scroll if dragging outside
        if (y < 0) onScroll(0, 1);
        if (y > pRenderTarget->GetSize().height) onScroll(0, -1);

        if (hit.x != cursorPos.x || hit.y != cursorPos.y) {
            cursorPos = hit;
            ensureCursorVisible();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    }
}

Point EditorWindow::hitTest(int x, int y) {
    int line = (scrollY + y) / (int)lineHeight;
    if (line < 0) line = 0;
    if (line >= lines.count()) line = lines.count() - 1;
    
    int col = (int)((x + scrollX - 40.0f) / charWidth + 0.5f); 
    if (col < 0) col = 0;
    if (col > lines[line].length()) col = lines[line].length();
    
    return Point(col, line);
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
    
    // Horizontal scrolling
    float cursorX = 40.0f + cursorPos.x * charWidth;
    float viewportWidth = pRenderTarget->GetSize().width;
    
    if (cursorX < scrollX + 40.0f) {
        scrollX = (int)(cursorX - 40.0f);
    } else if (cursorX > scrollX + viewportWidth) {
        scrollX = (int)(cursorX - viewportWidth);
    }
    if (scrollX < 0) scrollX = 0;
    
    // Update scrollbar
    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_POS;
    si.nPos = scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void EditorWindow::setText(const String& text) {
    lines.clear();
    
    // Improved splitting respecting \r\n, \n, and \r
    std::wstring wtext = text.toStdWString();
    size_t start = 0;
    size_t pos = 0;
    
    while (pos < wtext.length()) {
        if (wtext[pos] == L'\n') {
            lines.append(String(wtext.substr(start, pos - start).c_str()));
            start = pos + 1;
        } else if (wtext[pos] == L'\r') {
             // Handle \r\n or just \r
             size_t end = pos;
             if (pos + 1 < wtext.length() && wtext[pos+1] == L'\n') {
                 pos++;
             }
             lines.append(String(wtext.substr(start, end - start).c_str()));
             start = pos + 1;
        }
        pos++;
    }
    
    if (start < wtext.length()) {
        lines.append(String(wtext.substr(start).c_str()));
    } else if (start == wtext.length() && start > 0 && (wtext[start-1] == L'\n' || wtext[start-1] == L'\r')) {
        // Trailing newline creates empty line
        lines.append(L"");
    }
    
    if (lines.isEmpty()) lines.append(L"");
    
    cursorPos = Point(0,0);
    anchorPos = Point(0,0);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::appendText(const String& text) {
    if (lines.isEmpty()) lines.append(L"");
    lines.last().append(text);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::copy() {
    String text;
    // Check selection
    if (cursorPos.x != anchorPos.x || cursorPos.y != anchorPos.y) {
         Point start = (cursorPos.y < anchorPos.y || (cursorPos.y == anchorPos.y && cursorPos.x < anchorPos.x)) ? cursorPos : anchorPos;
         Point end = (start.x == cursorPos.x && start.y == cursorPos.y) ? anchorPos : cursorPos;

         if (start.y == end.y) {
             text = lines[start.y].mid(start.x, end.x - start.x);
         } else {
             text = lines[start.y].mid(start.x) + L"\r\n";
             for (int i = start.y + 1; i < end.y; ++i) {
                 text += lines[i] + L"\r\n";
             }
             text += lines[end.y].left(end.x);
         }
    } else {
         // Copy current line if no selection
         if (cursorPos.y >= 0 && cursorPos.y < lines.count()) {
            text = lines[cursorPos.y];
         }
    }

    if (!text.isEmpty()) {
        Application::getInstance()->clipboardSetText(text);
    }
}

void EditorWindow::paste() {
    String text = Application::getInstance()->clipboardText();
    if (text.isEmpty()) return;
    
    EditorCommand cmd;
    cmd.type = EditorCommand::Insert;
    cmd.pos = cursorPos;
    cmd.text = text;
    pushCommand(cmd);
    
    // Robust multi-line paste
    std::wstring wtext = text.toStdWString();
    
    // normalize line endings to \n
    std::wstring ntext;
    for(size_t i=0; i<wtext.length(); ++i) {
        if(wtext[i] == L'\r') continue; 
        ntext += wtext[i];
    }
    
    std::vector<String> parts;
    size_t start = 0;
    for(size_t i=0; i<ntext.length(); ++i) {
        if(ntext[i] == L'\n') {
             parts.push_back(String(ntext.substr(start, i - start).c_str()));
             start = i + 1;
        }
    }
    parts.push_back(String(ntext.substr(start).c_str()));

    if (cursorPos.y < lines.count()) {
        String suffix = lines[cursorPos.y].mid(cursorPos.x);
        lines[cursorPos.y] = lines[cursorPos.y].left(cursorPos.x) + parts[0];
        
        if (parts.size() > 1) {
            for(size_t i=1; i<parts.size(); ++i) {
                lines.insert(cursorPos.y + i, parts[i]);
            }
            lines[cursorPos.y + parts.size() - 1].append(suffix);
            
            cursorPos.y += (int)parts.size() - 1;
            cursorPos.x = parts.back().length();
        } else {
            lines[cursorPos.y].append(suffix);
            cursorPos.x += parts[0].length();
        }
    }
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::cut() {
    copy();
    
    // Explicit Logic: Full Range Delete (No Qt)
    if (cursorPos.x != anchorPos.x || cursorPos.y != anchorPos.y) {
         Point start = (cursorPos.y < anchorPos.y || (cursorPos.y == anchorPos.y && cursorPos.x < anchorPos.x)) ? cursorPos : anchorPos;
         Point end = (start.x == cursorPos.x && start.y == cursorPos.y) ? anchorPos : cursorPos;
         
         EditorCommand cmd;
         cmd.type = EditorCommand::Delete;
         cmd.pos = start;
         // Store deleted text for undo
         cmd.text = getSelectedText(); 
         
         if (start.y == end.y) {
             // Single line delete
             if (start.y < lines.size()) {
                 lines[start.y] = lines[start.y].left(start.x) + lines[start.y].mid(end.x);
             }
         } else {
             // Multi-line delete
             String suffix = "";
             if (end.y < lines.size() && end.x < lines[end.y].size()) {
                 suffix = lines[end.y].mid(end.x);
             }
             
             if (start.y < lines.size()) {
                 lines[start.y] = lines[start.y].left(start.x) + suffix;
             }
             
             // Remove intermediate lines
             if (start.y + 1 < lines.size()) {
                  int removeCount = end.y - start.y;
                  if (start.y + 1 + removeCount <= lines.size()) {
                      lines.erase(lines.begin() + start.y + 1, lines.begin() + start.y + 1 + removeCount);
                  }
             }
         }
         
         cursorPos = start;
         anchorPos = start;
         isSelecting = false; // Reset selection after cut
         pushCommand(cmd);
         ensureCursorVisible();
         InvalidateRect(hwnd, nullptr, FALSE);
         return; // Done
    }

    // Naive delete line (Original behavior preservation)
    EditorCommand cmd;
    cmd.type = EditorCommand::Delete;
    cmd.pos = {0, cursorPos.y};
     
    if (cursorPos.y < lines.size() && lines.size() > 1) {
        cmd.text = lines[cursorPos.y];
        cmd.text.append(L"\r\n");
        pushCommand(cmd);
        
        lines.erase(lines.begin() + cursorPos.y);
        InvalidateRect(hwnd, nullptr, FALSE);
    } else if (lines.size() > 0) {
        cmd.text = lines[0];
        pushCommand(cmd);
        lines[0] = L""; 
    }
}

String EditorWindow::getSelectedText() {
    if (!isSelecting) return L"";
    if (cursorPos.x == anchorPos.x && cursorPos.y == anchorPos.y) return L"";

    Point start = (cursorPos.y < anchorPos.y || (cursorPos.y == anchorPos.y && cursorPos.x < anchorPos.x)) ? cursorPos : anchorPos;
    Point end = (start.x == cursorPos.x && start.y == cursorPos.y) ? anchorPos : cursorPos;
    
    if (start.y == end.y) {
        return lines[start.y].mid(start.x, end.x - start.x);
    }
    
    String text = lines[start.y].mid(start.x);
    for (int i = start.y + 1; i < end.y; ++i) {
        text.append(L"\r\n").append(lines[i]);
    }
    text.append(L"\r\n").append(lines[end.y].left(end.x));
    return text;
}

void EditorWindow::pushCommand(const EditorCommand& cmd) {
    undoStack.push_back(cmd);
    if (undoStack.size() > 50) undoStack.pop_front();
    redoStack.clear();
}

void EditorWindow::undo() {
    if (undoStack.empty()) return;
    EditorCommand cmd = undoStack.back();
    undoStack.pop_back();
    
    // Invert the command
    if (cmd.type == EditorCommand::Insert) {
        // Inverse of Insert is Delete
        // Simplification: Assume single line for basic typing
        if (cmd.pos.y < lines.count()) {
            // Need robust boundary checks
            int len = std::min((int)cmd.text.length(), (int)lines[cmd.pos.y].length() - cmd.pos.x);
            if (len > 0) {
                 lines[cmd.pos.y].remove(cmd.pos.x, len);
            }
            cursorPos = cmd.pos; // Move cursor to start of undone insertion
        }
    } else if (cmd.type == EditorCommand::Delete) {
        // Inverse of Delete is Insert
        if (cmd.pos.y < lines.count()) {
            lines[cmd.pos.y].insert(cmd.pos.x, cmd.text);
            cursorPos = cmd.pos; // Or after?
            // Usually we want to select it or place cursor at end?
            // placing at end mimics re-typing
             cursorPos.x += cmd.text.length();
        }
    }
    
    redoStack.push_back(cmd); // Push original command to redo stack
    ensureCursorVisible();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::redo() {
    if (redoStack.empty()) return;
    EditorCommand cmd = redoStack.back();
    redoStack.pop_back();
    
    if (cmd.type == EditorCommand::Insert) {
        // Re-do Insert
        if (cmd.pos.y < lines.count()) {
            lines[cmd.pos.y].insert(cmd.pos.x, cmd.text);
            cursorPos = cmd.pos;
            cursorPos.x += cmd.text.length();
        }
    } else if (cmd.type == EditorCommand::Delete) {
         // Re-do Delete
         if (cmd.pos.y < lines.count()) {
             int len = std::min((int)cmd.text.length(), (int)lines[cmd.pos.y].length() - cmd.pos.x);
             if (len > 0) lines[cmd.pos.y].remove(cmd.pos.x, len);
             cursorPos = cmd.pos;
         }
    }
    
    undoStack.push_back(cmd);
    ensureCursorVisible();
    InvalidateRect(hwnd, nullptr, FALSE);
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
        lines[cursorPos.y].insert(cursorPos.x, ghostText); // Correctly insert at cursor
        cursorPos.x += ghostText.length();
    }
    ghostText = L""; // Clear
    InvalidateRect(hwnd, nullptr, FALSE);
}

void EditorWindow::onTimer(UINT_PTR id) {
    if (id == 1) { // Ghost text timer
        KillTimer(hwnd, 1);
        
        if (!aiHub) return;

        std::string prefix;
        if (cursorPos.y < lines.count()) {
            prefix = lines[cursorPos.y].left(cursorPos.x).toUtf8();
        }
        
        // Use the Hub which talks to the Named Pipe -> ASM Engine
        std::vector<RawrXD::Completion> completions = aiHub->getCompletions("editor", prefix, "", (int)prefix.length());
        if (!completions.empty()) {
            setGhostText(String(completions[0].text));
        }
    }
}

} // namespace RawrXD
