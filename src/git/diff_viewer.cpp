#include "git/diff_viewer.h"
#include <sstream>

namespace RawrXD::Git {

DiffViewer::DiffViewer() = default;
DiffViewer::~DiffViewer() {
    shutdown();
}

bool DiffViewer::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDDiffViewer";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDDiffViewer", "Diff",
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
                            0, 0, 800, 600, parent, nullptr, GetModuleHandle(nullptr), this);

    return m_hwnd != nullptr;
}

void DiffViewer::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND DiffViewer::getHandle() const {
    return m_hwnd;
}

void DiffViewer::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void DiffViewer::setDiff(const std::string& oldContent, const std::string& newContent) {
    m_blocks = computeDiff(oldContent, newContent);
    invalidate();
}

void DiffViewer::setDiff(const std::vector<DiffBlock>& blocks) {
    m_blocks = blocks;
    invalidate();
}

void DiffViewer::clear() {
    m_blocks.clear();
    m_currentDifference = 0;
    invalidate();
}

void DiffViewer::setViewMode(DiffViewMode mode) {
    m_mode = mode;
    invalidate();
}

DiffViewMode DiffViewer::getViewMode() const {
    return m_mode;
}

void DiffViewer::cycleViewMode() {
    switch (m_mode) {
        case DiffViewMode::SideBySide:
            setViewMode(DiffViewMode::Inline);
            break;
        case DiffViewMode::Inline:
            setViewMode(DiffViewMode::Unified);
            break;
        case DiffViewMode::Unified:
            setViewMode(DiffViewMode::SideBySide);
            break;
    }
}

void DiffViewer::nextDifference() {
    if (m_currentDifference < getDifferenceCount() - 1) {
        m_currentDifference++;
        invalidate();
    }
}

void DiffViewer::previousDifference() {
    if (m_currentDifference > 0) {
        m_currentDifference--;
        invalidate();
    }
}

void DiffViewer::firstDifference() {
    m_currentDifference = 0;
    invalidate();
}

void DiffViewer::lastDifference() {
    if (!m_blocks.empty()) {
        m_currentDifference = static_cast<int>(m_blocks.size()) - 1;
        invalidate();
    }
}

int DiffViewer::getCurrentDifference() const {
    return m_currentDifference;
}

int DiffViewer::getDifferenceCount() const {
    return static_cast<int>(m_blocks.size());
}

void DiffViewer::render() {
    invalidate();
}

void DiffViewer::invalidate() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void DiffViewer::setShowLineNumbers(bool show) {
    m_showLineNumbers = show;
    invalidate();
}

void DiffViewer::setShowWhitespace(bool show) {
    m_showWhitespace = show;
    invalidate();
}

void DiffViewer::setWordWrap(bool wrap) {
    m_wordWrap = wrap;
    invalidate();
}

void DiffViewer::setAddedColor(uint32_t color) {
    m_addedColor = color;
    invalidate();
}

void DiffViewer::setRemovedColor(uint32_t color) {
    m_removedColor = color;
    invalidate();
}

void DiffViewer::setModifiedColor(uint32_t color) {
    m_modifiedColor = color;
    invalidate();
}

void DiffViewer::setContextColor(uint32_t color) {
    m_contextColor = color;
    invalidate();
}

void DiffViewer::onLineClick(LineClickCallback callback) {
    m_lineClickCallback = callback;
}

void DiffViewer::onAction(ActionCallback callback) {
    m_actionCallback = callback;
}

void DiffViewer::layout() {
    // Layout diff viewer
}

void DiffViewer::draw(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    switch (m_mode) {
        case DiffViewMode::SideBySide:
            drawSideBySide(hdc);
            break;
        case DiffViewMode::Inline:
            drawInline(hdc);
            break;
        case DiffViewMode::Unified:
            drawUnified(hdc);
            break;
    }
}

void DiffViewer::drawSideBySide(HDC hdc) {
    // Draw side-by-side diff
    int y = 0;
    int lineHeight = 20;

    for (size_t i = 0; i < m_blocks.size(); ++i) {
        const auto& block = m_blocks[i];

        // Highlight current difference
        if (static_cast<int>(i) == m_currentDifference) {
            RECT highlightRect = {0, y, 800, y + static_cast<int>(block.lines.size()) * lineHeight};
            FillRect(hdc, &highlightRect, CreateSolidBrush(RGB(200, 200, 255)));
        }

        for (const auto& line : block.lines) {
            // Draw line numbers if enabled
            if (m_showLineNumbers) {
                drawLineNumber(hdc, line.oldLineNumber, y, true);
                drawLineNumber(hdc, line.newLineNumber, y + 400, false);
            }

            // Draw line content with appropriate color
            COLORREF color;
            switch (line.type) {
                case LineType::Added:
                    color = RGB((m_addedColor >> 16) & 0xFF, (m_addedColor >> 8) & 0xFF, m_addedColor & 0xFF);
                    break;
                case LineType::Removed:
                    color = RGB((m_removedColor >> 16) & 0xFF, (m_removedColor >> 8) & 0xFF, m_removedColor & 0xFF);
                    break;
                case LineType::Modified:
                    color = RGB((m_modifiedColor >> 16) & 0xFF, (m_modifiedColor >> 8) & 0xFF, m_modifiedColor & 0xFF);
                    break;
                default:
                    color = RGB((m_contextColor >> 16) & 0xFF, (m_contextColor >> 8) & 0xFF, m_contextColor & 0xFF);
                    break;
            }

            SetTextColor(hdc, color);
            SetBkMode(hdc, TRANSPARENT);

            RECT textRect = {50, y, 400, y + lineHeight};
            DrawText(hdc, line.content.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            y += lineHeight;
        }
    }
}

void DiffViewer::drawInline(HDC hdc) {
    // Draw inline diff
}

void DiffViewer::drawUnified(HDC hdc) {
    // Draw unified diff
}

void DiffViewer::drawLine(HDC hdc, const DiffLine& line, int y) {
    // Draw individual line
}

void DiffViewer::drawLineNumber(HDC hdc, int number, int y, bool isOld) {
    if (!m_showLineNumbers || number <= 0) return;

    std::string text = std::to_string(number);
    SetTextColor(hdc, RGB(128, 128, 128));
    SetBkMode(hdc, TRANSPARENT);

    RECT rect = {isOld ? 0 : 400, y, isOld ? 40 : 440, y + 20};
    DrawText(hdc, text.c_str(), -1, &rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

std::vector<DiffBlock> DiffViewer::computeDiff(const std::string& oldContent,
                                                  const std::string& newContent) {
    std::vector<DiffBlock> blocks;

    // Simple line-based diff
    std::vector<std::string> oldLines;
    std::vector<std::string> newLines;

    std::istringstream oldStream(oldContent);
    std::istringstream newStream(newContent);
    std::string line;

    while (std::getline(oldStream, line)) {
        oldLines.push_back(line);
    }
    while (std::getline(newStream, line)) {
        newLines.push_back(line);
    }

    // Simple diff algorithm
    int oldLine = 1;
    int newLine = 1;

    while (oldLine <= oldLines.size() || newLine <= newLines.size()) {
        DiffBlock block;
        block.oldStart = oldLine;
        block.newStart = newLine;

        if (oldLine <= oldLines.size() && newLine <= newLines.size() &&
            oldLines[oldLine - 1] == newLines[newLine - 1]) {
            // Context line
            DiffLine diffLine;
            diffLine.type = LineType::Context;
            diffLine.oldLineNumber = oldLine;
            diffLine.newLineNumber = newLine;
            diffLine.content = oldLines[oldLine - 1];
            block.lines.push_back(diffLine);
            oldLine++;
            newLine++;
        } else if (newLine <= newLines.size()) {
            // Added line
            DiffLine diffLine;
            diffLine.type = LineType::Added;
            diffLine.newLineNumber = newLine;
            diffLine.content = newLines[newLine - 1];
            block.lines.push_back(diffLine);
            newLine++;
        } else if (oldLine <= oldLines.size()) {
            // Removed line
            DiffLine diffLine;
            diffLine.type = LineType::Removed;
            diffLine.oldLineNumber = oldLine;
            diffLine.content = oldLines[oldLine - 1];
            block.lines.push_back(diffLine);
            oldLine++;
        }

        if (!block.lines.empty()) {
            blocks.push_back(block);
        }
    }

    return blocks;
}

LRESULT DiffViewer::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            draw(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int y = HIWORD(lParam);
            // Handle line click
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK DiffViewer::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DiffViewer* viewer = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        viewer = reinterpret_cast<DiffViewer*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(viewer));
    } else {
        viewer = reinterpret_cast<DiffViewer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (viewer) {
        return viewer->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
DiffViewer& getDiffViewer() {
    static DiffViewer viewer;
    return viewer;
}

} // namespace RawrXD::Git
