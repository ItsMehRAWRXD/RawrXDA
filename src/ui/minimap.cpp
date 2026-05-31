#include "ui/minimap.h"

namespace RawrXD::UI {

Minimap::Minimap() = default;
Minimap::~Minimap() {
    shutdown();
}

bool Minimap::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDMinimap";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDMinimap", "Minimap",
                            WS_CHILD | WS_VISIBLE,
                            0, 0, m_width, 400,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    if (m_hwnd) {
        createBitmap();
    }

    return m_hwnd != nullptr;
}

void Minimap::shutdown() {
    destroyBitmap();
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND Minimap::getHandle() const {
    return m_hwnd;
}

void Minimap::resize(int width, int height) {
    m_height = height;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, m_width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
    createBitmap();
}

void Minimap::setPosition(int x, int y) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
}

void Minimap::setContent(const std::string& text) {
    m_content = text;
    render();
}

void Minimap::updateContent(const std::string& text) {
    m_content = text;
    render();
}

void Minimap::clear() {
    m_content.clear();
    m_sections.clear();
    render();
}

void Minimap::setViewport(int startLine, int endLine) {
    m_visibleStartLine = startLine;
    m_visibleEndLine = endLine;
    invalidate();
}

void Minimap::setTotalLines(int lines) {
    m_totalLines = lines;
    render();
}

int Minimap::getTotalLines() const {
    return m_totalLines;
}

void Minimap::addSection(const MinimapSection& section) {
    m_sections.push_back(section);
    render();
}

void Minimap::clearSections() {
    m_sections.clear();
    render();
}

void Minimap::setSelection(int startLine, int endLine) {
    m_selectionStartLine = startLine;
    m_selectionEndLine = endLine;
    invalidate();
}

void Minimap::setCursorLine(int line) {
    m_cursorLine = line;
    invalidate();
}

void Minimap::render() {
    if (!m_memDC) return;

    // Clear background
    RECT rect = {0, 0, m_width, m_height};
    FillRect(m_memDC, &rect, (HBRUSH)(COLOR_WINDOW + 1));

    // Draw content
    drawContent();

    // Draw viewport overlay
    drawViewport(m_memDC);

    // Draw sections
    drawSections(m_memDC);

    // Draw slider
    if (m_showSlider) {
        drawSlider(m_memDC);
    }

    // Copy to window
    invalidate();
}

void Minimap::invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void Minimap::setEnabled(bool enabled) {
    m_enabled = enabled;
    ShowWindow(m_hwnd, enabled ? SW_SHOW : SW_HIDE);
}

bool Minimap::isEnabled() const {
    return m_enabled;
}

void Minimap::setWidth(int width) {
    m_width = width;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, m_height, SWP_NOMOVE | SWP_NOZORDER);
    }
    createBitmap();
}

int Minimap::getWidth() const {
    return m_width;
}

void Minimap::setScale(float scale) {
    m_scale = scale;
    render();
}

float Minimap::getScale() const {
    return m_scale;
}

void Minimap::setShowSlider(bool show) {
    m_showSlider = show;
    invalidate();
}

void Minimap::setRenderCharacters(bool render) {
    m_renderCharacters = render;
    render();
}

void Minimap::onScroll(ScrollCallback callback) {
    m_scrollCallback = callback;
}

void Minimap::onClick(ClickCallback callback) {
    m_clickCallback = callback;
}

void Minimap::createBitmap() {
    destroyBitmap();

    if (!m_hwnd) return;

    HDC hdc = GetDC(m_hwnd);
    m_memDC = CreateCompatibleDC(hdc);
    m_bitmap = CreateCompatibleBitmap(hdc, m_width, m_height);
    SelectObject(m_memDC, m_bitmap);
    ReleaseDC(m_hwnd, hdc);
}

void Minimap::destroyBitmap() {
    if (m_bitmap) {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
}

void Minimap::drawContent() {
    if (m_content.empty()) return;

    // Parse content and draw simplified representation
    std::istringstream stream(m_content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line) && lineNum < m_height / 2) {
        // Draw line representation
        int y = lineToY(lineNum);
        if (y >= 0 && y < m_height) {
            // Draw simplified line
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
            SelectObject(m_memDC, pen);
            MoveToEx(m_memDC, 0, y, nullptr);
            LineTo(m_memDC, static_cast<int>(line.length() * m_scale), y);
            DeleteObject(pen);
        }
        lineNum++;
    }
}

void Minimap::drawViewport(HDC hdc) {
    if (m_visibleStartLine < 0 || m_visibleEndLine < 0) return;

    int y1 = lineToY(m_visibleStartLine);
    int y2 = lineToY(m_visibleEndLine);

    // Draw viewport highlight
    RECT viewportRect = {0, y1, m_width, y2};
    FillRect(hdc, &viewportRect, CreateSolidBrush(RGB(100, 100, 100)));
}

void Minimap::drawSections(HDC hdc) {
    for (const auto& section : m_sections) {
        int y1 = lineToY(section.startLine);
        int y2 = lineToY(section.endLine);

        RECT sectionRect = {0, y1, m_width, y2};
        HBRUSH brush = CreateSolidBrush(section.color);
        FillRect(hdc, &sectionRect, brush);
        DeleteObject(brush);
    }
}

void Minimap::drawSlider(HDC hdc) {
    // Draw scrollbar-like slider
}

int Minimap::lineToY(int line) {
    return static_cast<int>(line * 2 * m_scale);
}

int Minimap::yToLine(int y) {
    return static_cast<int>(y / (2 * m_scale));
}

LRESULT Minimap::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);

            if (m_memDC && m_bitmap) {
                BitBlt(hdc, 0, 0, m_width, m_height, m_memDC, 0, 0, SRCCOPY);
            }

            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            m_dragging = true;
            SetCapture(m_hwnd);

            int y = HIWORD(lParam);
            int line = yToLine(y);

            if (m_clickCallback) {
                m_clickCallback(line, 0);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (m_dragging) {
                int y = HIWORD(lParam);
                int line = yToLine(y);

                if (m_scrollCallback) {
                    m_scrollCallback(line);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            m_dragging = false;
            ReleaseCapture();
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK Minimap::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Minimap* minimap = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        minimap = reinterpret_cast<Minimap*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(minimap));
    } else {
        minimap = reinterpret_cast<Minimap*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (minimap) {
        return minimap->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
Minimap& getMinimap() {
    static Minimap minimap;
    return minimap;
}

} // namespace RawrXD::UI
