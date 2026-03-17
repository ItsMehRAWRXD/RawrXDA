// ============================================================================
// Win32IDE_Minimap.cpp — Tier 1 Cosmetic #2: Code Overview Minimap
// ============================================================================
// Right-side scaled-down code thumbnail for navigation.
// Renders the full document as a minimap with syntax-colored  pixel blocks.
// Click/drag on minimap scrolls the editor to that position.
//
// Pattern:  Owner-draw STATIC with GDI rendering, no exceptions
// Threading: UI thread only
// ============================================================================

#include "Win32IDE.h"
#include <sstream>
#include <algorithm>
#include <cstring>

#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << "[INFO] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif

// VS Code minimap colors
static const COLORREF MINIMAP_BG         = RGB(30, 30, 30);
static const COLORREF MINIMAP_TEXT_DEFAULT = RGB(120, 120, 120);
static const COLORREF MINIMAP_KEYWORD    = RGB(86, 156, 214);    // blue
static const COLORREF MINIMAP_STRING     = RGB(206, 145, 120);   // orange
static const COLORREF MINIMAP_COMMENT    = RGB(106, 153, 85);    // green
static const COLORREF MINIMAP_TYPE       = RGB(78, 201, 176);    // teal
static const COLORREF MINIMAP_NUMBER     = RGB(181, 206, 168);   // light green
static const COLORREF MINIMAP_VIEWPORT_BG = RGB(60, 60, 60);     // current viewport highlight
static const COLORREF MINIMAP_VIEWPORT_BORDER = RGB(0, 122, 204); // blue border

// ============================================================================
// MINIMAP CREATION
// ============================================================================

void Win32IDE::createMinimap()
{
    if (m_hwndMinimap) return; // already created

    m_minimapWidth   = MINIMAP_DEFAULT_WIDTH;
    m_minimapVisible = m_settings.minimapEnabled;
    m_minimapScrollY = 0;
    m_minimapTotalLines = 0;
    m_minimapDragging = false;

    // Create minimap window as owner-draw static
    m_hwndMinimap = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | (m_minimapVisible ? WS_VISIBLE : 0) | SS_OWNERDRAW | SS_NOTIFY,
        0, 0, m_minimapWidth, 400,
        m_hwndMain, (HMENU)9800, m_hInstance, nullptr);

    if (!m_hwndMinimap) {
        RAWRXD_LOG_INFO("Failed to create minimap window");
        return;
    }

    // Store IDE pointer for WndProc
    SetWindowLongPtr(m_hwndMinimap, GWLP_USERDATA, (LONG_PTR)this);

    // Subclass for custom painting + mouse handling
    m_minimapFont = CreateFontA(
        -1, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    RAWRXD_LOG_INFO("Minimap created (width=" << m_minimapWidth << ")");
}

// ============================================================================
// MINIMAP UPDATE — Rebuild line cache from editor content
// ============================================================================

void Win32IDE::updateMinimap()
{
    if (!m_hwndMinimap || !m_hwndEditor) return;

    m_minimapLines.clear();
    m_minimapLineStarts.clear();

    // Get editor content
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) {
        m_minimapTotalLines = 0;
        InvalidateRect(m_hwndMinimap, nullptr, TRUE);
        return;
    }

    std::vector<char> buf(textLen + 1);
    GetWindowTextA(m_hwndEditor, buf.data(), textLen + 1);
    std::string content(buf.data());

    // Split into lines
    std::istringstream stream(content);
    std::string line;
    int offset = 0;
    while (std::getline(stream, line)) {
        m_minimapLines.push_back(line);
        m_minimapLineStarts.push_back(offset);
        offset += static_cast<int>(line.size()) + 1;
    }

    m_minimapTotalLines = static_cast<int>(m_minimapLines.size());

    // Get current scroll position
    m_minimapScrollY = static_cast<int>(SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));

    InvalidateRect(m_hwndMinimap, nullptr, FALSE);
}

// ============================================================================
// MINIMAP SCROLLING — Click/drag to scroll editor
// ============================================================================

void Win32IDE::scrollToMinimapPosition(int y)
{
    if (!m_hwndEditor || m_minimapTotalLines <= 0) return;

    // Get minimap client height
    RECT rc;
    GetClientRect(m_hwndMinimap, &rc);
    int minimapHeight = rc.bottom - rc.top;

    // Calculate the target line from mouse Y
    int totalRenderedHeight = m_minimapTotalLines * MINIMAP_CHAR_HEIGHT;
    float ratio = static_cast<float>(y) / static_cast<float>(minimapHeight);
    
    if (totalRenderedHeight > minimapHeight) {
        // Scrollable minimap
        int targetLine = static_cast<int>(ratio * m_minimapTotalLines);
        targetLine = (std::max)(0, (std::min)(targetLine, m_minimapTotalLines - 1));

        // Scroll editor to target line (center it in view)
        int editorVisibleLines = 30; // approximate
        RECT editorRC;
        if (GetClientRect(m_hwndEditor, &editorRC)) {
            int lineHeight = m_settings.fontSize + 4;
            if (lineHeight > 0)
                editorVisibleLines = (editorRC.bottom - editorRC.top) / lineHeight;
        }
        int scrollTo = targetLine - editorVisibleLines / 2;
        if (scrollTo < 0) scrollTo = 0;

        // Use EM_LINESCROLL for precise positioning
        int currentFirst = static_cast<int>(SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));
        int scrollDelta = scrollTo - currentFirst;
        SendMessage(m_hwndEditor, EM_LINESCROLL, 0, scrollDelta);

        // Update minimap display
        m_minimapScrollY = scrollTo;
        InvalidateRect(m_hwndMinimap, nullptr, FALSE);
    }
}

// ============================================================================
// MINIMAP TOGGLE
// ============================================================================

void Win32IDE::toggleMinimap()
{
    m_minimapVisible = !m_minimapVisible;
    m_settings.minimapEnabled = m_minimapVisible;

    if (m_hwndMinimap) {
        ShowWindow(m_hwndMinimap, m_minimapVisible ? SW_SHOW : SW_HIDE);
    }

    // Trigger relayout
    RECT rect;
    GetClientRect(m_hwndMain, &rect);
    onSize(rect.right, rect.bottom);

    RAWRXD_LOG_INFO("Minimap " << (m_minimapVisible ? "shown" : "hidden"));
}

// ============================================================================
// MINIMAP PAINTING — Render scaled-down code with basic syntax color
// ============================================================================

void Win32IDE::paintMinimap(HDC hdc, RECT& rc)
{
    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(MINIMAP_BG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    if (m_minimapTotalLines <= 0) return;

    int minimapHeight = rc.bottom - rc.top;
    int minimapWidth = rc.right - rc.left;

    // Calculate visible viewport in editor
    int firstVisible = static_cast<int>(SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));
    int editorVisibleLines = 30;
    RECT editorRC;
    if (m_hwndEditor && GetClientRect(m_hwndEditor, &editorRC)) {
        int lineHeight = m_settings.fontSize + 4;
        if (lineHeight > 0)
            editorVisibleLines = (editorRC.bottom - editorRC.top) / lineHeight;
    }

    // Render each line as compact colored blocks
    float yScale = 1.0f;
    int totalRendered = m_minimapTotalLines * MINIMAP_CHAR_HEIGHT;
    if (totalRendered > minimapHeight) {
        yScale = static_cast<float>(minimapHeight) / static_cast<float>(totalRendered);
    }

    for (int i = 0; i < m_minimapTotalLines; i++) {
        int yPos = static_cast<int>(i * MINIMAP_CHAR_HEIGHT * yScale);
        if (yPos >= minimapHeight) break;

        const std::string& line = m_minimapLines[i];

        // Determine basic syntax color
        COLORREF lineColor = MINIMAP_TEXT_DEFAULT;
        std::string trimmed = line;
        size_t firstNonSpace = trimmed.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            trimmed = trimmed.substr(firstNonSpace);
        }

        if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') {
            lineColor = MINIMAP_COMMENT;
        } else if (trimmed.size() >= 1 && trimmed[0] == '#') {
            lineColor = MINIMAP_KEYWORD;
        } else if (trimmed.find("void ") == 0 || trimmed.find("int ") == 0 ||
                   trimmed.find("bool ") == 0 || trimmed.find("auto ") == 0 ||
                   trimmed.find("class ") == 0 || trimmed.find("struct ") == 0 ||
                   trimmed.find("return ") == 0 || trimmed.find("if ") == 0 ||
                   trimmed.find("for ") == 0 || trimmed.find("while ") == 0) {
            lineColor = MINIMAP_KEYWORD;
        } else if (trimmed.find("\"") != std::string::npos) {
            lineColor = MINIMAP_STRING;
        }

        // Render line as colored pixels (each char = MINIMAP_CHAR_WIDTH wide)
        int lineLen = static_cast<int>(line.size());
        int maxChars = minimapWidth / MINIMAP_CHAR_WIDTH;
        if (lineLen > maxChars) lineLen = maxChars;

        for (int c = 0; c < lineLen; c++) {
            if (line[c] == ' ' || line[c] == '\t') continue;

            int xPos = static_cast<int>(c * MINIMAP_CHAR_WIDTH);
            int blockH = (std::max)(1, static_cast<int>(MINIMAP_CHAR_HEIGHT * yScale));

            RECT charRect;
            charRect.left   = xPos;
            charRect.top    = yPos;
            charRect.right  = xPos + MINIMAP_CHAR_WIDTH;
            charRect.bottom = yPos + blockH;

            // Dim the color slightly (minimap is muted)
            COLORREF dimColor = RGB(
                GetRValue(lineColor) * 7 / 10,
                GetGValue(lineColor) * 7 / 10,
                GetBValue(lineColor) * 7 / 10
            );

            HBRUSH charBrush = CreateSolidBrush(dimColor);
            FillRect(hdc, &charRect, charBrush);
            DeleteObject(charBrush);
        }
    }

    // Draw viewport indicator (semi-transparent rectangle)
    int vpTop = static_cast<int>(firstVisible * MINIMAP_CHAR_HEIGHT * yScale);
    int vpBottom = static_cast<int>((firstVisible + editorVisibleLines) * MINIMAP_CHAR_HEIGHT * yScale);
    vpBottom = (std::min)(vpBottom, minimapHeight);

    m_minimapViewRect = { rc.left, vpTop, rc.right, vpBottom };

    // Draw viewport highlight
    HBRUSH vpBrush = CreateSolidBrush(MINIMAP_VIEWPORT_BG);
    RECT vpRect = { rc.left, vpTop, rc.right, vpBottom };
    FillRect(hdc, &vpRect, vpBrush);
    DeleteObject(vpBrush);

    // Draw viewport border (left and right edges)
    HPEN vpPen = CreatePen(PS_SOLID, 1, MINIMAP_VIEWPORT_BORDER);
    HPEN oldPen = (HPEN)SelectObject(hdc, vpPen);
    MoveToEx(hdc, rc.left, vpTop, nullptr);
    LineTo(hdc, rc.left, vpBottom);
    MoveToEx(hdc, rc.right - 1, vpTop, nullptr);
    LineTo(hdc, rc.right - 1, vpBottom);
    // Top and bottom borders
    MoveToEx(hdc, rc.left, vpTop, nullptr);
    LineTo(hdc, rc.right, vpTop);
    MoveToEx(hdc, rc.left, vpBottom - 1, nullptr);
    LineTo(hdc, rc.right, vpBottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(vpPen);
}

// ============================================================================
// MINIMAP HIT TEST
// ============================================================================

void Win32IDE::minimapHitTest(int mouseY, int& outLine)
{
    RECT rc;
    GetClientRect(m_hwndMinimap, &rc);
    int minimapHeight = rc.bottom - rc.top;
    int totalRendered = m_minimapTotalLines * MINIMAP_CHAR_HEIGHT;

    float yScale = 1.0f;
    if (totalRendered > minimapHeight && totalRendered > 0) {
        yScale = static_cast<float>(minimapHeight) / static_cast<float>(totalRendered);
    }

    if (yScale > 0.0f && MINIMAP_CHAR_HEIGHT > 0) {
        outLine = static_cast<int>(mouseY / (MINIMAP_CHAR_HEIGHT * yScale));
    } else {
        outLine = 0;
    }
    outLine = (std::max)(0, (std::min)(outLine, m_minimapTotalLines - 1));
}

// ============================================================================
// MINIMAP WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK Win32IDE::MinimapProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Double buffer
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        if (pThis) {
            pThis->paintMinimap(memDC, rc);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        if (pThis) {
            int mouseY = HIWORD(lParam);
            pThis->m_minimapDragging = true;
            SetCapture(hwnd);
            pThis->scrollToMinimapPosition(mouseY);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (pThis && pThis->m_minimapDragging) {
            int mouseY = HIWORD(lParam);
            pThis->scrollToMinimapPosition(mouseY);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        if (pThis) {
            pThis->m_minimapDragging = false;
            ReleaseCapture();
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // prevent flicker
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
