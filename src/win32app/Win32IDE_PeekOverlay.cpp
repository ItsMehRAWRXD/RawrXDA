// ============================================================================
// Win32IDE_PeekOverlay.cpp — Peek Definition/References Overlay
// ============================================================================
// Provides VS Code-style peek overlays:
//   - Alt+F12: Peek definition
//   - Shift+F12: Peek references
//   - Hover tooltips with peek preview
//   - Click to navigate to definition/reference
//   - Multi-definition support with tabs
//   - Keyboard navigation (Up/Down arrows)
//   - Dismiss with Esc or click outside
//
// Architecture:
//   - PeekOverlayWindow: Custom window class for overlay display
//   - PeekItem: Represents a single definition/reference
//   - Navigation: Keyboard/mouse event handling
//   - Rendering: Syntax-highlighted code display
// ============================================================================

#include "Win32IDE.h"
#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <vector>


// ============================================================================
// PEEK OVERLAY WINDOW
// ============================================================================

class PeekOverlayWindow
{
  public:
    PeekOverlayWindow(HWND hwndParent, Win32IDE* ide, const std::vector<PeekItem>& items, int triggerLine,
                      int triggerCol);
    ~PeekOverlayWindow();

    void show();
    void hide();
    bool isVisible() const { return m_hwnd != nullptr && IsWindowVisible(m_hwnd); }

    // Event handling
    void onKeyDown(WPARAM vk);
    void onMouseClick(int x, int y);

  private:
    HWND m_hwnd;
    HWND m_hwndParent;
    Win32IDE* m_ide;
    std::vector<PeekItem> m_items;
    int m_currentIndex;
    int m_triggerLine;
    int m_triggerCol;

    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Rendering
    void renderItem(HDC hdc, const PeekItem& item, int x, int y, int width, int height);
    void drawBorder(HDC hdc, int x, int y, int width, int height);
};

// Deleter for forward-declared unique_ptr in Win32IDE.h
void PeekOverlayWindowDeleter::operator()(PeekOverlayWindow* p) noexcept
{
    delete p;
}

PeekOverlayWindow::PeekOverlayWindow(HWND hwndParent, Win32IDE* ide, const std::vector<PeekItem>& items,
                                     int triggerLine, int triggerCol)
    : m_hwnd(nullptr), m_hwndParent(hwndParent), m_ide(ide), m_items(items), m_currentIndex(0),
      m_triggerLine(triggerLine), m_triggerCol(triggerCol)
{

    if (items.empty())
        return;

    // Register window class
    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 35));
        wc.lpszClassName = L"RawrXD_PeekOverlay";
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    // Calculate position near trigger
    RECT parentRect;
    GetWindowRect(m_hwndParent, &parentRect);

    int x = parentRect.left + 100;  // Offset from left
    int y = parentRect.top + 100;   // Offset from top
    int width = 600;
    int height = 400;

    // Create window
    m_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"RawrXD_PeekOverlay", L"Peek",
                             WS_POPUP | WS_BORDER | WS_VISIBLE, x, y, width, height, m_hwndParent, nullptr,
                             GetModuleHandle(nullptr), this);

    if (m_hwnd)
    {
        SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
    }
}

PeekOverlayWindow::~PeekOverlayWindow()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void PeekOverlayWindow::show()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_SHOW);
        SetFocus(m_hwnd);
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void PeekOverlayWindow::hide()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void PeekOverlayWindow::onKeyDown(WPARAM vk)
{
    switch (vk)
    {
        case VK_ESCAPE: {
            Win32IDE* ide = m_ide;
            if (!ide)
                ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(m_hwndParent, GWLP_USERDATA));
            if (ide)
                ide->postPeekFinishFromOverlay(false, "", 0);
            else
                hide();
            break;
        }
        case VK_UP:
            if (m_currentIndex > 0)
            {
                m_currentIndex--;
                InvalidateRect(m_hwnd, nullptr, TRUE);
            }
            break;
        case VK_DOWN:
            if (m_currentIndex < (int)m_items.size() - 1)
            {
                m_currentIndex++;
                InvalidateRect(m_hwnd, nullptr, TRUE);
            }
            break;
        case VK_RETURN:
            if (m_currentIndex >= 0 && m_currentIndex < (int)m_items.size())
            {
                const PeekItem& item = m_items[m_currentIndex];
                Win32IDE* ide = m_ide;
                if (!ide)
                    ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(m_hwndParent, GWLP_USERDATA));
                if (ide) {
                    const bool go = !item.file.empty() && item.line > 0;
                    ide->postPeekFinishFromOverlay(go, item.file,
                                                   static_cast<uint32_t>(std::max(1, item.line)));
                } else {
                    hide();
                }
            }
            break;
    }
}

void PeekOverlayWindow::onMouseClick(int x, int y)
{
    // Check if click is outside window bounds
    RECT rect;
    GetWindowRect(m_hwnd, &rect);

    POINT pt = {x, y};
    ClientToScreen(m_hwnd, &pt);

    if (!PtInRect(&rect, pt))
    {
        Win32IDE* ide = m_ide;
        if (!ide)
            ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(m_hwndParent, GWLP_USERDATA));
        if (ide)
            ide->postPeekFinishFromOverlay(false, "", 0);
        else
            hide();
    }
}

LRESULT CALLBACK PeekOverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PeekOverlayWindow* self = (PeekOverlayWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            if (self && !self->m_items.empty())
            {
                const PeekItem& item = self->m_items[self->m_currentIndex];
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                self->renderItem(hdc, item, 0, 0, clientRect.right, clientRect.bottom);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_KEYDOWN:
            if (self)
            {
                self->onKeyDown(wParam);
            }
            return 0;

        case WM_LBUTTONDOWN:
        {
            if (self)
            {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                self->onMouseClick(x, y);
            }
            return 0;
        }

        case WM_DESTROY:
            if (self)
            {
                self->m_hwnd = nullptr;
            }
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void PeekOverlayWindow::renderItem(HDC hdc, const PeekItem& item, int x, int y, int width, int height)
{
    // ---- Dark theme background ----
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 35));
    RECT bgRect = {x, y, x + width, y + height};
    FillRect(hdc, &bgRect, bgBrush);
    DeleteObject(bgBrush);

    SetBkMode(hdc, TRANSPARENT);

    // ---- Header bar (file path + tabs for multi-item) ----
    const int headerH = 28;
    HBRUSH headerBrush = CreateSolidBrush(RGB(40, 40, 48));
    RECT headerRect = {x, y, x + width, y + headerH};
    FillRect(hdc, &headerRect, headerBrush);
    DeleteObject(headerBrush);

    // Title text (file:line in header)
    HFONT headerFont = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, headerFont);

    std::wstring headerText(item.title.begin(), item.title.end());
    if (!item.file.empty()) {
        // Show filename:line
        std::string fname = item.file;
        auto slash = fname.find_last_of("\\/");
        if (slash != std::string::npos) fname = fname.substr(slash + 1);
        std::string loc = fname + ":" + std::to_string(item.line);
        headerText = std::wstring(loc.begin(), loc.end());
    }

    RECT titleRect = {x + 10, y + 4, x + width - 10, y + headerH};
    SetTextColor(hdc, RGB(200, 200, 210));
    DrawTextW(hdc, headerText.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Tab indicator (current/total)
    if (m_items.size() > 1) {
        std::wstring tabText = std::to_wstring(m_currentIndex + 1) + L"/" + std::to_wstring(m_items.size());
        RECT tabRect = {x + width - 80, y + 4, x + width - 10, y + headerH};
        SetTextColor(hdc, RGB(140, 140, 160));
        DrawTextW(hdc, tabText.c_str(), -1, &tabRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(headerFont);

    // ---- Separator line ----
    HPEN sepPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 70));
    HPEN oldPen = (HPEN)SelectObject(hdc, sepPen);
    MoveToEx(hdc, x, y + headerH, nullptr);
    LineTo(hdc, x + width, y + headerH);
    SelectObject(hdc, oldPen);
    DeleteObject(sepPen);

    // ---- Code content with line numbers ----
    HFONT codeFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                 FIXED_PITCH | FF_MODERN, L"Consolas");
    SelectObject(hdc, codeFont);

    const int gutterWidth = 48;
    const int lineHeight = 17;
    const int contentTop = y + headerH + 6;
    const int contentLeft = x + gutterWidth + 8;
    const int maxVisibleLines = (height - headerH - 12) / lineHeight;

    // Gutter background
    HBRUSH gutterBrush = CreateSolidBrush(RGB(35, 35, 42));
    RECT gutterRect = {x, y + headerH, x + gutterWidth, y + height};
    FillRect(hdc, &gutterRect, gutterBrush);
    DeleteObject(gutterBrush);

    // Parse content into lines
    std::vector<std::string> lines;
    {
        std::string remaining = item.content;
        size_t pos = 0;
        while (pos < remaining.size()) {
            size_t nl = remaining.find('\n', pos);
            if (nl == std::string::npos) {
                lines.push_back(remaining.substr(pos));
                break;
            }
            lines.push_back(remaining.substr(pos, nl - pos));
            pos = nl + 1;
        }
    }

    // Determine starting line number
    int startLineNum = (item.line > 3) ? item.line - 3 : 1;

    for (int i = 0; i < static_cast<int>(lines.size()) && i < maxVisibleLines; ++i) {
        int drawY = contentTop + i * lineHeight;
        int lineNum = startLineNum + i;
        bool isTargetLine = (lineNum == item.line);

        // Highlight target line background
        if (isTargetLine || (!lines[i].empty() && lines[i].substr(0, 4) == ">>> ")) {
            HBRUSH hlBrush = CreateSolidBrush(RGB(45, 50, 65));
            RECT hlRect = {x + gutterWidth, drawY, x + width, drawY + lineHeight};
            FillRect(hdc, &hlRect, hlBrush);
            DeleteObject(hlBrush);
        }

        // Line number in gutter
        std::wstring numStr = std::to_wstring(lineNum);
        RECT numRect = {x + 4, drawY, x + gutterWidth - 4, drawY + lineHeight};
        SetTextColor(hdc, isTargetLine ? RGB(180, 180, 200) : RGB(90, 90, 110));
        DrawTextW(hdc, numStr.c_str(), -1, &numRect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // Code text — lightweight syntax coloring
        std::string lineText = lines[i];
        // Strip the ">>> " / "    " prefix from buildPeekItemsFromLspLocations
        if (lineText.size() >= 4 && (lineText.substr(0, 4) == ">>> " || lineText.substr(0, 4) == "    ")) {
            lineText = lineText.substr(4);
        }

        // Simple token classification for coloring
        std::wstring wline(lineText.begin(), lineText.end());
        RECT codeRect = {contentLeft, drawY, x + width - 8, drawY + lineHeight};

        // Check for comment lines
        std::string trimmed = lineText;
        size_t ftrim = trimmed.find_first_not_of(" \t");
        if (ftrim != std::string::npos) trimmed = trimmed.substr(ftrim);

        if (trimmed.size() >= 2 && trimmed[0] == '/' && trimmed[1] == '/') {
            SetTextColor(hdc, RGB(106, 153, 85));  // Green comments
        } else if (trimmed.size() >= 1 && trimmed[0] == '#') {
            SetTextColor(hdc, RGB(155, 120, 180)); // Purple preprocessor
        } else if (trimmed.find("\"") != std::string::npos) {
            SetTextColor(hdc, RGB(206, 145, 120)); // Orange strings
        } else {
            SetTextColor(hdc, RGB(212, 212, 212)); // Default light gray
        }

        DrawTextW(hdc, wline.c_str(), -1, &codeRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    SelectObject(hdc, oldFont);
    DeleteObject(codeFont);

    // ---- Border ----
    drawBorder(hdc, x, y, width, height);
}

void PeekOverlayWindow::drawBorder(HDC hdc, int x, int y, int width, int height)
{
    // Accent blue border matching VS Code peek
    HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 122, 204));
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH brush = (HBRUSH)GetStockObject(NULL_BRUSH);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

    Rectangle(hdc, x, y, x + width, y + height);

    // Top accent line (brighter)
    HPEN topPen = CreatePen(PS_SOLID, 2, RGB(0, 150, 240));
    SelectObject(hdc, topPen);
    MoveToEx(hdc, x, y, nullptr);
    LineTo(hdc, x + width, y);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(topPen);
}

// ============================================================================
// WIN32IDE INTEGRATION
// ============================================================================

void Win32IDE::initPeekOverlay()
{
    m_peekOverlayActive = false;
    LOG_INFO("Peek overlay system initialized");
}

void Win32IDE::shutdownPeekOverlay()
{
    if (m_peekOverlayWindow)
    {
        m_peekOverlayWindow->hide();
        m_peekOverlayWindow.reset();
    }
}

void Win32IDE::showPeekOverlayWithItems(const std::vector<PeekItem>& items, int triggerLine, int triggerCol)
{
    if (items.empty())
        return;
    m_peekOverlayWindow.reset(new PeekOverlayWindow(m_hwndMain, this, items, triggerLine, triggerCol));
    m_peekOverlayWindow->show();
    m_peekOverlayActive = true;
}

void Win32IDE::postPeekFinishFromOverlay(bool navigate, const std::string& filePath, uint32_t line1Based)
{
    m_peekDeferredNavigate = navigate;
    m_peekDeferredFile = filePath;
    m_peekDeferredLine = line1Based;
    if (m_peekOverlayWindow)
        m_peekOverlayWindow->hide();
    if (m_hwndMain)
        PostMessageW(m_hwndMain, WM_RAWRXD_PEEK_FINISH, 0, 0);
    else
    {
        if (m_peekDeferredNavigate && !m_peekDeferredFile.empty() && m_peekDeferredLine > 0)
            navigateToFileLine(m_peekDeferredFile, m_peekDeferredLine);
        m_peekOverlayWindow.reset();
        m_peekOverlayActive = false;
        m_peekDeferredNavigate = false;
        m_peekDeferredFile.clear();
    }
}

void Win32IDE::showPeekDefinition(int line, int col)
{
    std::vector<PeekItem> items = findDefinitionsAt(line, col);
    showPeekOverlayWithItems(items, line, col);
}

void Win32IDE::showPeekReferences(int line, int col)
{
    std::vector<PeekItem> items = findReferencesAt(line, col);
    showPeekOverlayWithItems(items, line, col);
}

std::vector<PeekItem> Win32IDE::buildPeekItemsFromLspLocations(const std::vector<LSPLocation>& locations,
                                                               PeekItemType type, int contextLinesBefore,
                                                               int contextLinesAfter)
{
    std::vector<PeekItem> items;
    items.reserve(locations.size());

    for (const auto& loc : locations)
    {
        PeekItem item;
        item.file = uriToFilePath(loc.uri);
        item.line = loc.range.start.line + 1;
        item.column = loc.range.start.character + 1;
        item.type = type;

        std::ifstream file(item.file);
        if (file.is_open())
        {
            std::vector<std::string> lines;
            std::string lineContent;
            while (std::getline(file, lineContent))
                lines.push_back(lineContent);
            file.close();

            const int target0 = item.line - 1;
            const int startLine = std::max(0, target0 - contextLinesBefore);
            const int endLine = std::min((int)lines.size() - 1, target0 + contextLinesAfter);

            std::string content;
            for (int i = startLine; i <= endLine; ++i)
            {
                if (i == target0)
                    content += ">>> " + lines[i] + "\n";
                else
                    content += "    " + lines[i] + "\n";
            }
            item.content = content;

            if (target0 >= 0 && target0 < (int)lines.size())
            {
                if (type == PeekItemType::Definition)
                {
                    std::string defLine = lines[target0];
                    size_t start = defLine.find_first_not_of(" \t");
                    if (start != std::string::npos)
                    {
                        size_t end = defLine.find_first_of("({[", start);
                        if (end != std::string::npos)
                            item.title = defLine.substr(start, end - start);
                        else
                            item.title = defLine.substr(start, 30) + "...";
                    }
                }
                else
                {
                    std::string refLine = lines[target0];
                    size_t start = refLine.find_first_not_of(" \t");
                    if (start != std::string::npos)
                    {
                        size_t end = refLine.find_first_of(" \t;.,()[]{}", start);
                        if (end != std::string::npos)
                            item.title = "Reference: " + refLine.substr(start, end - start);
                        else
                            item.title = "Reference at line " + std::to_string(item.line);
                    }
                }
            }
        }
        else
        {
            item.title = (type == PeekItemType::Definition ? "Definition" : "Reference");
            item.title += " at line " + std::to_string(item.line);
            item.content = "[Could not read file: " + item.file + "]";
        }

        items.push_back(std::move(item));
    }

    return items;
}

std::vector<PeekItem> Win32IDE::findDefinitionsAt(int line, int col)
{
    if (m_currentFile.empty())
        return {};
    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspGotoDefinition(uri, line - 1, col - 1);
    // Match prior vertical context: three lines above and three below the target (0-based window).
    return buildPeekItemsFromLspLocations(locations, PeekItemType::Definition, 3, 3);
}

std::vector<PeekItem> Win32IDE::findReferencesAt(int line, int col)
{
    if (m_currentFile.empty())
        return {};
    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspFindReferences(uri, line - 1, col - 1);
    return buildPeekItemsFromLspLocations(locations, PeekItemType::Reference, 2, 2);
}

void Win32IDE::handlePeekOverlayKey(UINT vk, bool ctrl, bool alt, bool shift)
{
    if (!m_peekOverlayActive || !m_peekOverlayWindow)
        return;

    if (vk == VK_ESCAPE)
        postPeekFinishFromOverlay(false, "", 0);
}

bool Win32IDE::isPeekOverlayActive() const
{
    return m_peekOverlayActive;
}
