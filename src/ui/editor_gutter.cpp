#include "ui/editor_gutter.h"
#include <algorithm>

namespace RawrXD::UI {

EditorGutter::EditorGutter() = default;
EditorGutter::~EditorGutter() {
    shutdown();
}

bool EditorGutter::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDEditorGutter";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDEditorGutter", "Gutter",
                            WS_CHILD | WS_VISIBLE,
                            0, 0, m_width, 600,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    return m_hwnd != nullptr;
}

void EditorGutter::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND EditorGutter::getHandle() const {
    return m_hwnd;
}

void EditorGutter::resize(int width, int height) {
    m_height = height;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, m_width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void EditorGutter::setPosition(int x, int y) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void EditorGutter::setLineCount(int count) {
    m_lineCount = count;
    invalidate();
}

void EditorGutter::setCurrentLine(int line) {
    int oldLine = m_currentLine;
    m_currentLine = line;

    // Invalidate old and new current line
    if (oldLine >= 0) invalidateLine(oldLine);
    if (line >= 0) invalidateLine(line);
}

int EditorGutter::getCurrentLine() const {
    return m_currentLine;
}

void EditorGutter::setLineInfo(int line, const GutterLine& info) {
    m_lineInfo[line] = info;
    invalidateLine(line);
}

void EditorGutter::clearLineInfo(int line) {
    m_lineInfo.erase(line);
    invalidateLine(line);
}

void EditorGutter::clearAllLineInfo() {
    m_lineInfo.clear();
    invalidate();
}

void EditorGutter::setFoldRegions(const std::vector<FoldRegion>& regions) {
    m_foldRegions = regions;
    invalidate();
}

void EditorGutter::setFoldState(int line, FoldState state) {
    for (auto& region : m_foldRegions) {
        if (region.startLine == line) {
            region.state = state;
            invalidateLine(line);
            break;
        }
    }
}

void EditorGutter::toggleFold(int line) {
    for (auto& region : m_foldRegions) {
        if (region.startLine == line) {
            if (region.state == FoldState::Collapsed) {
                region.state = FoldState::Expanded;
            } else if (region.state == FoldState::Expanded) {
                region.state = FoldState::Collapsed;
            }
            invalidateLine(line);

            if (m_foldToggleCallback) {
                m_foldToggleCallback(line);
            }
            break;
        }
    }
}

void EditorGutter::expandAll() {
    for (auto& region : m_foldRegions) {
        region.state = FoldState::Expanded;
    }
    invalidate();
}

void EditorGutter::collapseAll() {
    for (auto& region : m_foldRegions) {
        region.state = FoldState::Collapsed;
    }
    invalidate();
}

void EditorGutter::expandLevel(int level) {
    for (auto& region : m_foldRegions) {
        if (region.level == level) {
            region.state = FoldState::Expanded;
        }
    }
    invalidate();
}

void EditorGutter::setBreakpoint(int line, bool set) {
    auto it = m_lineInfo.find(line);
    if (it != m_lineInfo.end()) {
        it->second.hasBreakpoint = set;
    } else {
        GutterLine info;
        info.lineNumber = line;
        info.hasBreakpoint = set;
        m_lineInfo[line] = info;
    }
    invalidateLine(line);
}

void EditorGutter::toggleBreakpoint(int line) {
    auto it = m_lineInfo.find(line);
    if (it != m_lineInfo.end()) {
        it->second.hasBreakpoint = !it->second.hasBreakpoint;
    } else {
        GutterLine info;
        info.lineNumber = line;
        info.hasBreakpoint = true;
        m_lineInfo[line] = info;
    }
    invalidateLine(line);
}

bool EditorGutter::hasBreakpoint(int line) const {
    auto it = m_lineInfo.find(line);
    return it != m_lineInfo.end() && it->second.hasBreakpoint;
}

void EditorGutter::clearAllBreakpoints() {
    for (auto& [line, info] : m_lineInfo) {
        info.hasBreakpoint = false;
    }
    invalidate();
}

std::vector<int> EditorGutter::getBreakpoints() const {
    std::vector<int> breakpoints;
    for (const auto& [line, info] : m_lineInfo) {
        if (info.hasBreakpoint) {
            breakpoints.push_back(line);
        }
    }
    return breakpoints;
}

void EditorGutter::setBookmark(int line, bool set) {
    auto it = m_lineInfo.find(line);
    if (it != m_lineInfo.end()) {
        it->second.hasBookmark = set;
    } else {
        GutterLine info;
        info.lineNumber = line;
        info.hasBookmark = set;
        m_lineInfo[line] = info;
    }
    invalidateLine(line);
}

void EditorGutter::toggleBookmark(int line) {
    auto it = m_lineInfo.find(line);
    if (it != m_lineInfo.end()) {
        it->second.hasBookmark = !it->second.hasBookmark;
    } else {
        GutterLine info;
        info.lineNumber = line;
        info.hasBookmark = true;
        m_lineInfo[line] = info;
    }
    invalidateLine(line);
}

bool EditorGutter::hasBookmark(int line) const {
    auto it = m_lineInfo.find(line);
    return it != m_lineInfo.end() && it->second.hasBookmark;
}

void EditorGutter::clearAllBookmarks() {
    for (auto& [line, info] : m_lineInfo) {
        info.hasBookmark = false;
    }
    invalidate();
}

std::vector<int> EditorGutter::getBookmarks() const {
    std::vector<int> bookmarks;
    for (const auto& [line, info] : m_lineInfo) {
        if (info.hasBookmark) {
            bookmarks.push_back(line);
        }
    }
    return bookmarks;
}

void EditorGutter::nextBookmark() {
    // Find next bookmark after current line
}

void EditorGutter::previousBookmark() {
    // Find previous bookmark before current line
}

void EditorGutter::render() {
    invalidate();
}

void EditorGutter::invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void EditorGutter::invalidateLine(int line) {
    if (m_hwnd) {
        RECT rect = {0, line * m_lineHeight, m_width, (line + 1) * m_lineHeight};
        InvalidateRect(m_hwnd, &rect, TRUE);
    }
}

void EditorGutter::setLineNumbersVisible(bool visible) {
    m_showLineNumbers = visible;
    invalidate();
}

bool EditorGutter::areLineNumbersVisible() const {
    return m_showLineNumbers;
}

void EditorGutter::setFoldingVisible(bool visible) {
    m_showFolding = visible;
    invalidate();
}

bool EditorGutter::isFoldingVisible() const {
    return m_showFolding;
}

void EditorGutter::setWidth(int width) {
    m_width = width;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, m_height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

int EditorGutter::getWidth() const {
    return m_width;
}

void EditorGutter::setFont(HFONT font) {
    m_font = font;
    invalidate();
}

void EditorGutter::onLineClick(LineClickCallback callback) {
    m_lineClickCallback = callback;
}

void EditorGutter::onFoldToggle(FoldToggleCallback callback) {
    m_foldToggleCallback = callback;
}

void EditorGutter::layout() {
    // Layout gutter
}

void EditorGutter::draw(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // Draw background
    FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

    // Draw line numbers and fold indicators
    for (int line = 0; line < m_lineCount; ++line) {
        RECT lineRect = {0, line * m_lineHeight, m_width, (line + 1) * m_lineHeight};

        // Highlight current line
        if (line == m_currentLine) {
            FillRect(hdc, &lineRect, (HBRUSH)(COLOR_HIGHLIGHT + 1));
        }

        // Draw line number
        if (m_showLineNumbers) {
            drawLineNumber(hdc, line, lineRect);
        }

        // Draw fold indicator
        if (m_showFolding) {
            RECT foldRect = lineRect;
            foldRect.left = getFoldIndicatorWidth();
            drawFoldIndicator(hdc, line, foldRect);
        }

        // Draw breakpoint/bookmark
        auto it = m_lineInfo.find(line);
        if (it != m_lineInfo.end()) {
            if (it->second.hasBreakpoint) {
                // Draw breakpoint indicator
                Ellipse(hdc, 5, line * m_lineHeight + 5, 15, (line + 1) * m_lineHeight - 5);
            }
            if (it->second.hasBookmark) {
                // Draw bookmark indicator
            }
        }
    }
}

void EditorGutter::drawLineNumber(HDC hdc, int line, RECT& rect) {
    if (!m_showLineNumbers) return;

    SetTextColor(hdc, line == m_currentLine ? RGB(255, 255, 255) : RGB(128, 128, 128));
    SetBkMode(hdc, TRANSPARENT);

    std::string text = std::to_string(line + 1);
    RECT textRect = rect;
    textRect.right = getFoldIndicatorWidth() - 5;
    DrawText(hdc, text.c_str(), -1, &textRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void EditorGutter::drawFoldIndicator(HDC hdc, int line, RECT& rect) {
    if (!m_showFolding) return;

    // Find fold region for this line
    for (const auto& region : m_foldRegions) {
        if (region.startLine == line) {
            // Draw fold indicator
            int x = rect.left + 5;
            int y = rect.top + m_lineHeight / 2;

            if (region.state == FoldState::Collapsed) {
                // Draw plus sign
                MoveToEx(hdc, x, y - 4, nullptr);
                LineTo(hdc, x, y + 4);
                MoveToEx(hdc, x - 4, y, nullptr);
                LineTo(hdc, x + 4, y);
            } else if (region.state == FoldState::Expanded) {
                // Draw minus sign
                MoveToEx(hdc, x - 4, y, nullptr);
                LineTo(hdc, x + 4, y);
            }
            break;
        }
    }
}

void EditorGutter::drawGlyph(HDC hdc, const std::string& glyph, RECT& rect) {
    // Draw custom glyph
}

int EditorGutter::getLineAtY(int y) const {
    return y / m_lineHeight;
}

int EditorGutter::getFoldIndicatorWidth() const {
    return m_showFolding ? 20 : 0;
}

LRESULT EditorGutter::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            draw(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            int line = getLineAtY(y);

            if (m_showFolding && x < getFoldIndicatorWidth()) {
                toggleFold(line);
            } else if (m_lineClickCallback) {
                m_lineClickCallback(line, x);
            }
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK EditorGutter::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    EditorGutter* gutter = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        gutter = reinterpret_cast<EditorGutter*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(gutter));
    } else {
        gutter = reinterpret_cast<EditorGutter*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (gutter) {
        return gutter->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
EditorGutter& getEditorGutter() {
    static EditorGutter gutter;
    return gutter;
}

} // namespace RawrXD::UI
