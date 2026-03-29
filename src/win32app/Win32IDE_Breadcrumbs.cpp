// ============================================================================
// Win32IDE_Breadcrumbs.cpp — Tier 1 Cosmetic #3: Breadcrumbs Navigation Bar
// ============================================================================
// Renders a clickable symbol path (File > Class > Method) header bar
// above the editor, using parsed symbol hierarchy from the current file.
//
// Pattern:  Owner-draw STATIC with GDI text rendering, no exceptions
// Threading: UI thread only
// ============================================================================

#include "Win32IDE.h"
#include "Win32IDE_IELabels.h"
#include <algorithm>
#include <cctype>
#include <sstream>


// SCAFFOLD_028: Breadcrumbs and navigation

// Breadcrumb bar colors (VS Code dark theme)
static const COLORREF BC_BG = RGB(37, 37, 38);
static const COLORREF BC_TEXT = RGB(169, 169, 169);
static const COLORREF BC_SEPARATOR = RGB(110, 110, 110);
static const COLORREF BC_HOVER = RGB(60, 60, 60);
static const COLORREF BC_ACTIVE_TEXT = RGB(220, 220, 220);
static const COLORREF BC_ICON_FILE = RGB(204, 204, 204);
static const COLORREF BC_ICON_CLASS = RGB(206, 145, 120);
static const COLORREF BC_ICON_METHOD = RGB(86, 156, 214);
static const COLORREF BC_ICON_FUNC = RGB(220, 220, 170);

namespace
{
struct BreadcrumbSymbolCandidate
{
    std::string label;
    std::string symbolKind;
    int line = 0;
};

static std::string trimLeadingAscii(const std::string& text)
{
    size_t pos = text.find_first_not_of(" \t");
    return (pos == std::string::npos) ? std::string() : text.substr(pos);
}

static std::vector<BreadcrumbSymbolCandidate> collectEditorSymbols(const std::string& content)
{
    std::vector<BreadcrumbSymbolCandidate> symbols;
    std::istringstream stream(content);
    std::string lineText;
    int lineNum = 0;

    while (std::getline(stream, lineText))
    {
        ++lineNum;
        const std::string trimmed = trimLeadingAscii(lineText);
        if (trimmed.empty())
        {
            continue;
        }

        if (trimmed.find("namespace ") == 0 && trimmed.find(';') == std::string::npos)
        {
            const size_t start = 10;
            size_t end = trimmed.find_first_of(" {", start);
            if (end == std::string::npos)
                end = trimmed.size();
            if (end > start)
            {
                symbols.push_back({trimmed.substr(start, end - start), "namespace", lineNum});
            }
            continue;
        }

        if ((trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) && trimmed.find(';') == std::string::npos)
        {
            const size_t start = (trimmed[0] == 'c') ? 6 : 7;
            size_t end = trimmed.find_first_of(" :{", start);
            if (end == std::string::npos)
                end = trimmed.size();
            if (end > start)
            {
                symbols.push_back({trimmed.substr(start, end - start), "class", lineNum});
            }
            continue;
        }

        if (trimmed.find("(") != std::string::npos && trimmed.find("if ") != 0 && trimmed.find("if(") != 0 &&
            trimmed.find("for ") != 0 && trimmed.find("for(") != 0 && trimmed.find("while ") != 0 &&
            trimmed.find("while(") != 0 && trimmed.find("switch ") != 0 && trimmed.find("switch(") != 0 &&
            trimmed.find("return ") != 0 && trimmed.find('#') != 0 && trimmed.find("//") != 0)
        {
            const size_t parenPos = trimmed.find('(');
            if (parenPos > 0)
            {
                size_t nameStart = parenPos;
                while (nameStart > 0 && (std::isalnum(static_cast<unsigned char>(trimmed[nameStart - 1])) ||
                                         trimmed[nameStart - 1] == '_' || trimmed[nameStart - 1] == ':'))
                {
                    --nameStart;
                }
                if (nameStart < parenPos)
                {
                    const std::string funcName = trimmed.substr(nameStart, parenPos - nameStart);
                    if (!funcName.empty() && funcName != "sizeof" && funcName != "static_cast" &&
                        funcName != "dynamic_cast" && funcName != "reinterpret_cast" && funcName != "const_cast" &&
                        funcName != "decltype")
                    {
                        symbols.push_back({funcName, "function", lineNum});
                    }
                }
            }
        }
    }

    return symbols;
}

static void navigateEditorToLine(HWND hwndEditor, int line)
{
    if (!hwndEditor || line <= 0)
    {
        return;
    }
    int lineIndex = static_cast<int>(SendMessage(hwndEditor, EM_LINEINDEX, line - 1, 0));
    SendMessage(hwndEditor, EM_SETSEL, lineIndex, lineIndex);
    SendMessage(hwndEditor, EM_SCROLLCARET, 0, 0);
    SetFocus(hwndEditor);
}
}  // namespace

// ============================================================================
// BREADCRUMB BAR CREATION
// ============================================================================

void Win32IDE::createBreadcrumbBar(HWND hwndParent)
{
    if (!hwndParent)
        return;

    m_breadcrumbHeight = 22;

    // ESP:IDC_BREADCRUMB_BAR — Symbol path bar (File > Class > Method), subclassed for custom paint
    m_hwndBreadcrumbs =
        CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY, 0, 0, 800, m_breadcrumbHeight,
                        hwndParent, (HMENU)(UINT_PTR)IDC_BREADCRUMB_BAR, m_hInstance, nullptr);

    if (!m_hwndBreadcrumbs)
    {
        RAWRXD_LOG_INFO("Failed to create breadcrumb bar");
        return;
    }

    SetWindowTextA(m_hwndBreadcrumbs, RAWRXD_IDE_LABEL_BREADCRUMB_BAR);
    SetWindowLongPtr(m_hwndBreadcrumbs, GWLP_USERDATA, (LONG_PTR)this);
    // Subclass for custom paint (paintBreadcrumbs) and click handling
    SetWindowLongPtr(m_hwndBreadcrumbs, GWLP_WNDPROC, (LONG_PTR)BreadcrumbProc);

    // Create breadcrumb font (smaller than editor font)
    m_breadcrumbFont = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    // Initialize with file-only breadcrumb
    updateBreadcrumbs();

    // Show/hide based on settings
    ShowWindow(m_hwndBreadcrumbs, m_settings.breadcrumbsEnabled ? SW_SHOW : SW_HIDE);

    RAWRXD_LOG_INFO("Win32IDE_Breadcrumbs") << "Breadcrumb bar created";
}

// ============================================================================
// BREADCRUMB UPDATE — Parse current file for symbol hierarchy
// ============================================================================

void Win32IDE::updateBreadcrumbs()
{
    m_breadcrumbPath.clear();

    // Always add file as first breadcrumb
    if (!m_currentFile.empty())
    {
        BreadcrumbItem fileItem;
        // Extract filename from path
        size_t lastSlash = m_currentFile.find_last_of("\\/");
        fileItem.label = (lastSlash != std::string::npos) ? m_currentFile.substr(lastSlash + 1) : m_currentFile;
        fileItem.symbolKind = "file";
        fileItem.line = 0;
        fileItem.column = 0;
        m_breadcrumbPath.push_back(fileItem);
    }
    else
    {
        BreadcrumbItem untitled;
        untitled.label = "Untitled";
        untitled.symbolKind = "file";
        untitled.line = 0;
        untitled.column = 0;
        m_breadcrumbPath.push_back(untitled);
    }

    if (m_hwndBreadcrumbs)
    {
        InvalidateRect(m_hwndBreadcrumbs, nullptr, TRUE);
    }
}

// ============================================================================
// BREADCRUMB UPDATE FOR CURSOR — Determine symbol hierarchy at cursor position
// ============================================================================

void Win32IDE::updateBreadcrumbsForCursor(int line, int column)
{
    if (!m_hwndEditor || !m_settings.breadcrumbsEnabled)
        return;

    m_breadcrumbPath.clear();

    // File breadcrumb (always first)
    {
        BreadcrumbItem fileItem;
        size_t lastSlash = m_currentFile.find_last_of("\\/");
        fileItem.label = (!m_currentFile.empty() && lastSlash != std::string::npos)
                             ? m_currentFile.substr(lastSlash + 1)
                             : (m_currentFile.empty() ? "Untitled" : m_currentFile);
        fileItem.symbolKind = "file";
        fileItem.line = 0;
        fileItem.column = 0;
        m_breadcrumbPath.push_back(fileItem);
    }

    // Parse editor text to find enclosing symbols at cursor line
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0)
    {
        if (m_hwndBreadcrumbs)
            InvalidateRect(m_hwndBreadcrumbs, nullptr, TRUE);
        return;
    }

    std::vector<char> buf(textLen + 1);
    GetWindowTextA(m_hwndEditor, buf.data(), textLen + 1);
    std::string content(buf.data());

    // Simple symbol parser: find enclosing class/struct and function
    // This is a lightweight heuristic; LSP integration would be ideal
    std::istringstream stream(content);
    std::string lineText;
    int lineNum = 0;
    int braceDepth = 0;

    std::string currentNamespace;
    std::string currentClass;
    std::string currentFunction;
    int classLine = 0, funcLine = 0;
    int classBraceDepth = -1, funcBraceDepth = -1;

    while (std::getline(stream, lineText))
    {
        lineNum++;

        // Trim leading whitespace
        std::string trimmed = lineText;
        size_t pos = trimmed.find_first_not_of(" \t");
        if (pos != std::string::npos)
            trimmed = trimmed.substr(pos);

        // Detect namespace
        if (trimmed.find("namespace ") == 0 && trimmed.find(';') == std::string::npos)
        {
            // Extract namespace name
            size_t nameStart = 10;
            size_t nameEnd = trimmed.find_first_of(" {", nameStart);
            if (nameEnd == std::string::npos)
                nameEnd = trimmed.size();
            currentNamespace = trimmed.substr(nameStart, nameEnd - nameStart);
        }

        // Detect class/struct
        if ((trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) && trimmed.find(';') == std::string::npos)
        {
            size_t nameStart = (trimmed[0] == 'c') ? 6 : 7;
            size_t nameEnd = trimmed.find_first_of(" :{", nameStart);
            if (nameEnd == std::string::npos)
                nameEnd = trimmed.size();
            if (nameEnd > nameStart)
            {
                currentClass = trimmed.substr(nameStart, nameEnd - nameStart);
                classLine = lineNum;
                classBraceDepth = braceDepth;
            }
        }

        // Detect function (heuristic: identifier followed by "(" not in control flow)
        if (trimmed.find("(") != std::string::npos && trimmed.find("if ") != 0 && trimmed.find("if(") != 0 &&
            trimmed.find("for ") != 0 && trimmed.find("for(") != 0 && trimmed.find("while ") != 0 &&
            trimmed.find("while(") != 0 && trimmed.find("switch ") != 0 && trimmed.find("switch(") != 0 &&
            trimmed.find("return ") != 0 && trimmed.find("//") != 0 && trimmed.find('#') != 0)
        {
            // Extract function name (word before '(')
            size_t parenPos = trimmed.find('(');
            if (parenPos > 0)
            {
                size_t nameEnd = parenPos;
                size_t nameStart = nameEnd;
                while (nameStart > 0 && (isalnum(trimmed[nameStart - 1]) || trimmed[nameStart - 1] == '_' ||
                                         trimmed[nameStart - 1] == ':'))
                {
                    nameStart--;
                }
                if (nameStart < nameEnd)
                {
                    std::string funcName = trimmed.substr(nameStart, nameEnd - nameStart);
                    // Filter out common non-function patterns
                    if (funcName != "sizeof" && funcName != "static_cast" && funcName != "dynamic_cast" &&
                        funcName != "reinterpret_cast" && funcName != "const_cast" && funcName != "decltype" &&
                        !funcName.empty())
                    {
                        currentFunction = funcName;
                        funcLine = lineNum;
                        funcBraceDepth = braceDepth;
                    }
                }
            }
        }

        // Track brace depth
        for (char c : lineText)
        {
            if (c == '{')
                braceDepth++;
            if (c == '}')
            {
                braceDepth--;
                // Check if we exited the class scope
                if (classBraceDepth >= 0 && braceDepth <= classBraceDepth && lineNum <= line)
                {
                    // We've closed the class; if cursor is after, clear class
                }
                // Check if we exited the function scope
                if (funcBraceDepth >= 0 && braceDepth <= funcBraceDepth && lineNum <= line)
                {
                    if (lineNum < line)
                    {
                        currentFunction.clear();
                        funcBraceDepth = -1;
                    }
                }
            }
        }

        if (lineNum >= line)
            break;  // Stop at cursor line
    }

    // Build breadcrumb path
    if (!currentNamespace.empty())
    {
        BreadcrumbItem ns;
        ns.label = currentNamespace;
        ns.symbolKind = "namespace";
        ns.line = 1;
        ns.column = 0;
        m_breadcrumbPath.push_back(ns);
    }

    if (!currentClass.empty())
    {
        BreadcrumbItem cls;
        cls.label = currentClass;
        cls.symbolKind = "class";
        cls.line = classLine;
        cls.column = 0;
        m_breadcrumbPath.push_back(cls);
    }

    if (!currentFunction.empty())
    {
        BreadcrumbItem func;
        func.label = currentFunction;
        func.symbolKind = "function";
        func.line = funcLine;
        func.column = 0;
        m_breadcrumbPath.push_back(func);
    }

    if (m_hwndBreadcrumbs)
    {
        InvalidateRect(m_hwndBreadcrumbs, nullptr, TRUE);
    }
}

// ============================================================================
// BREADCRUMB CLICK — Navigate to symbol
// ============================================================================

void Win32IDE::onBreadcrumbClick(int index)
{
    if (index < 0 || index >= static_cast<int>(m_breadcrumbPath.size()))
        return;

    const auto& item = m_breadcrumbPath[index];
    if (!m_hwndEditor)
    {
        return;
    }

    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0)
    {
        navigateEditorToLine(m_hwndEditor, item.line);
        return;
    }

    std::vector<char> buf(textLen + 1);
    GetWindowTextA(m_hwndEditor, buf.data(), textLen + 1);
    const std::string content(buf.data());
    const auto allSymbols = collectEditorSymbols(content);

    std::vector<BreadcrumbSymbolCandidate> menuCandidates;
    menuCandidates.reserve(allSymbols.size());
    if (item.symbolKind == "file")
    {
        for (const auto& s : allSymbols)
        {
            if (s.symbolKind == "namespace" || s.symbolKind == "class" || s.symbolKind == "function")
            {
                menuCandidates.push_back(s);
            }
        }
    }
    else if (item.symbolKind == "namespace")
    {
        for (const auto& s : allSymbols)
        {
            if (s.symbolKind == "class" || s.symbolKind == "function")
            {
                menuCandidates.push_back(s);
            }
        }
    }
    else if (item.symbolKind == "class")
    {
        for (const auto& s : allSymbols)
        {
            if (s.symbolKind == "function")
            {
                menuCandidates.push_back(s);
            }
        }
    }

    if (!menuCandidates.empty())
    {
        HMENU hMenu = CreatePopupMenu();
        if (hMenu != nullptr)
        {
            constexpr UINT kMenuBase = 50000;
            const size_t maxItems = (menuCandidates.size() > 40) ? 40 : menuCandidates.size();
            for (size_t i = 0; i < maxItems; ++i)
            {
                const auto& c = menuCandidates[i];
                std::string label = c.label + " (line " + std::to_string(c.line) + ")";
                AppendMenuA(hMenu, MF_STRING, kMenuBase + static_cast<UINT>(i), label.c_str());
            }

            RECT rc = {0, 0, 0, 0};
            if (index < static_cast<int>(m_breadcrumbRects.size()))
            {
                rc = m_breadcrumbRects[index];
            }
            POINT popupPt = {rc.left, rc.bottom};
            ClientToScreen(m_hwndBreadcrumbs ? m_hwndBreadcrumbs : m_hwndMain, &popupPt);

            UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, popupPt.x, popupPt.y, 0,
                                      m_hwndMain ? m_hwndMain : m_hwndBreadcrumbs, nullptr);
            DestroyMenu(hMenu);

            if (cmd >= kMenuBase)
            {
                const size_t selected = static_cast<size_t>(cmd - kMenuBase);
                if (selected < maxItems)
                {
                    navigateEditorToLine(m_hwndEditor, menuCandidates[selected].line);
                    return;
                }
            }
        }
    }

    if (item.line > 0)
    {
        navigateEditorToLine(m_hwndEditor, item.line);
    }
}

// ============================================================================
// BREADCRUMB PAINTING — VS Code-style breadcrumb bar
// ============================================================================

void Win32IDE::paintBreadcrumbs(HDC hdc, RECT& rc)
{
    // Fill background
    HBRUSH bgBrush = CreateSolidBrush(BC_BG);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw bottom border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(50, 50, 50));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.right, rc.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    if (m_breadcrumbPath.empty())
        return;

    m_breadcrumbRects.clear();
    m_breadcrumbRects.reserve(m_breadcrumbPath.size());

    // Set font and text properties
    HFONT oldFont = (HFONT)SelectObject(hdc, m_breadcrumbFont ? m_breadcrumbFont : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);

    int xPos = 8;  // left margin

    for (size_t i = 0; i < m_breadcrumbPath.size(); i++)
    {
        const auto& item = m_breadcrumbPath[i];
        int itemLeft = xPos;

        // Draw separator before items (except first)
        if (i > 0)
        {
            SetTextColor(hdc, BC_SEPARATOR);
            const char* sep = " > ";
            RECT sepRect = {xPos, rc.top, xPos + 24, rc.bottom};
            DrawTextA(hdc, sep, -1, &sepRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            xPos += 22;
        }

        // Determine icon color based on symbol kind
        COLORREF iconColor = BC_ICON_FILE;
        const char* icon = "";
        if (item.symbolKind == "class" || item.symbolKind == "struct")
        {
            iconColor = BC_ICON_CLASS;
            icon = "C ";
        }
        else if (item.symbolKind == "method" || item.symbolKind == "function")
        {
            iconColor = BC_ICON_METHOD;
            icon = "f ";
        }
        else if (item.symbolKind == "namespace")
        {
            iconColor = BC_ICON_FUNC;
            icon = "N ";
        }
        else if (item.symbolKind == "file")
        {
            icon = "";
        }

        // Draw icon prefix
        if (icon[0] != '\0')
        {
            SetTextColor(hdc, iconColor);
            RECT iconRect = {xPos, rc.top, xPos + 16, rc.bottom};
            DrawTextA(hdc, icon, -1, &iconRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            xPos += 14;
        }

        // Draw breadcrumb text
        SetTextColor(hdc, (i == m_breadcrumbPath.size() - 1) ? BC_ACTIVE_TEXT : BC_TEXT);
        SIZE textSize;
        GetTextExtentPoint32A(hdc, item.label.c_str(), static_cast<int>(item.label.size()), &textSize);

        RECT textRect = {xPos, rc.top, xPos + textSize.cx, rc.bottom};
        DrawTextA(hdc, item.label.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        xPos += textSize.cx + 4;

        RECT hitRect = {itemLeft, rc.top, xPos, rc.bottom};
        m_breadcrumbRects.push_back(hitRect);
    }

    SelectObject(hdc, oldFont);
}

// ============================================================================
// BREADCRUMB WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK Win32IDE::BreadcrumbProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            if (pThis)
            {
                pThis->paintBreadcrumbs(memDC, rc);
            }

            BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONUP:
        {
            if (pThis)
            {
                POINT pt = {LOWORD(lParam), HIWORD(lParam)};
                int clickedIndex = -1;
                for (size_t i = 0; i < pThis->m_breadcrumbRects.size(); i++)
                {
                    if (PtInRect(&pThis->m_breadcrumbRects[i], pt))
                    {
                        clickedIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (clickedIndex >= 0)
                    pThis->onBreadcrumbClick(clickedIndex);
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
