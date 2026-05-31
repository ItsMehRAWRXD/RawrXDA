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
#include <sstream>
#include <algorithm>
#include <vector>
#include "../../include/RawrXD_ColorSpace.h"

using namespace RawrXD::ColorSpace;

// Breadcrumb bar colors - Adobe RGBa VSU Effects
static const AdobeRGBa BC_BG           = VSU::Acrylic::DarkBase;
static const AdobeRGBa BC_TEXT         = AdobeRGBa(0.66f, 0.66f, 0.66f, 1.00f);
static const AdobeRGBa BC_SEPARATOR    = AdobeRGBa(0.43f, 0.43f, 0.43f, 1.00f);
static const AdobeRGBa BC_HOVER        = AdobeRGBa(0.24f, 0.24f, 0.24f, 1.00f);
static const AdobeRGBa BC_ACTIVE_TEXT  = AdobeRGBa(0.86f, 0.86f, 0.86f, 1.00f);
static const AdobeRGBa BC_ICON_FILE    = AdobeRGBa(0.80f, 0.80f, 0.80f, 1.00f);
static const AdobeRGBa BC_ICON_CLASS   = AdobeRGBa(0.81f, 0.57f, 0.47f, 1.00f);
static const AdobeRGBa BC_ICON_METHOD  = AdobeRGBa(0.34f, 0.61f, 0.84f, 1.00f);
static const AdobeRGBa BC_ICON_FUNC    = AdobeRGBa(0.86f, 0.86f, 0.67f, 1.00f);
static const AdobeRGBa BC_ICON_FOLDER  = AdobeRGBa(0.77f, 0.53f, 0.75f, 1.00f);

// Helper to convert AdobeRGBa to COLORREF for Win32 GDI
inline COLORREF AdobeRGBaToCOLORREF(const AdobeRGBa& color) {
    auto srgb = color.TosRGB();
    return RGB(static_cast<int>(srgb.r * 255), 
               static_cast<int>(srgb.g * 255), 
               static_cast<int>(srgb.b * 255));
}

namespace {
int g_breadcrumbHoverIndex = -1;

struct PathBreadcrumbItem {
    std::string label;
    std::string symbolKind;
    int line = 0;
    int column = 0;
};

void appendPathBreadcrumbs(const std::string& currentFile, std::vector<PathBreadcrumbItem>& out)
{
    if (currentFile.empty())
        return;

    // Keep path separators normalized for breadcrumb slicing.
    std::string normalized = currentFile;
    for (char& c : normalized)
    {
        if (c == '\\')
            c = '/';
    }

    // Optional root crumb (e.g., "D:")
    if (normalized.size() >= 2 && normalized[1] == ':')
    {
        PathBreadcrumbItem root;
        root.label = normalized.substr(0, 2);
        root.symbolKind = "root";
        root.line = 0;
        root.column = 0;
        out.push_back(root);
    }

    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= normalized.size())
    {
        size_t pos = normalized.find('/', start);
        std::string part = (pos == std::string::npos) ? normalized.substr(start) : normalized.substr(start, pos - start);
        if (!part.empty() && part != ".")
            parts.push_back(part);
        if (pos == std::string::npos)
            break;
        start = pos + 1;
    }

    if (parts.empty())
        return;

    size_t firstPart = 0;
    if (normalized.size() >= 2 && normalized[1] == ':' && !parts.empty() && parts[0].size() >= 2 && parts[0][1] == ':')
    {
        firstPart = 1;
    }

    for (size_t i = firstPart; i < parts.size(); ++i)
    {
        PathBreadcrumbItem item;
        item.label = parts[i];
        item.symbolKind = (i + 1 == parts.size()) ? "file" : "folder";
        item.line = 0;
        item.column = 0;
        out.push_back(item);
    }
}
} // namespace

// ============================================================================
// BREADCRUMB BAR CREATION
// ============================================================================

void Win32IDE::createBreadcrumbBar(HWND hwndParent)
{
    if (!hwndParent) return;

    m_breadcrumbHeight = 22;

    // ESP:IDC_BREADCRUMB_BAR — Symbol path bar (File > Class > Method), subclassed for custom paint
    m_hwndBreadcrumbs = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY,
        0, 0, 800, m_breadcrumbHeight,
        hwndParent, (HMENU)(UINT_PTR)IDC_BREADCRUMB_BAR, m_hInstance, nullptr);

    if (!m_hwndBreadcrumbs) {
        RAWRXD_LOG_INFO("Failed to create breadcrumb bar");
        return;
    }

    SetWindowTextA(m_hwndBreadcrumbs, RAWRXD_IDE_LABEL_BREADCRUMB_BAR);
    SetWindowLongPtr(m_hwndBreadcrumbs, GWLP_USERDATA, (LONG_PTR)this);
    // Subclass for custom paint (paintBreadcrumbs) and click handling
    SetWindowLongPtr(m_hwndBreadcrumbs, GWLP_WNDPROC, (LONG_PTR)BreadcrumbProc);

    // Create breadcrumb font (smaller than editor font)
    m_breadcrumbFont = CreateFontA(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

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

    // Render full file path (root > folders > file) for aperture-like navigation context.
    std::vector<PathBreadcrumbItem> pathItems;
    appendPathBreadcrumbs(m_currentFile, pathItems);
    for (const auto& item : pathItems)
    {
        BreadcrumbItem crumb;
        crumb.label = item.label;
        crumb.symbolKind = item.symbolKind;
        crumb.line = item.line;
        crumb.column = item.column;
        m_breadcrumbPath.push_back(std::move(crumb));
    }

    if (m_breadcrumbPath.empty()) {
        BreadcrumbItem untitled;
        untitled.label = "Untitled";
        untitled.symbolKind = "file";
        untitled.line = 0;
        untitled.column = 0;
        m_breadcrumbPath.push_back(untitled);
    }

    if (m_hwndBreadcrumbs) {
        InvalidateRect(m_hwndBreadcrumbs, nullptr, TRUE);
    }
}

// ============================================================================
// BREADCRUMB UPDATE FOR CURSOR — Determine symbol hierarchy at cursor position
// ============================================================================

void Win32IDE::updateBreadcrumbsForCursor(int line, int column)
{
    if (!m_hwndEditor || !m_settings.breadcrumbsEnabled) return;

    m_breadcrumbPath.clear();

    // File path breadcrumbs (root > folders > file) are always first.
    std::vector<PathBreadcrumbItem> pathItems;
    appendPathBreadcrumbs(m_currentFile, pathItems);
    for (const auto& item : pathItems)
    {
        BreadcrumbItem crumb;
        crumb.label = item.label;
        crumb.symbolKind = item.symbolKind;
        crumb.line = item.line;
        crumb.column = item.column;
        m_breadcrumbPath.push_back(std::move(crumb));
    }
    if (m_breadcrumbPath.empty()) {
        BreadcrumbItem untitled;
        untitled.label = "Untitled";
        untitled.symbolKind = "file";
        untitled.line = 0;
        untitled.column = 0;
        m_breadcrumbPath.push_back(untitled);
    }

    // Parse editor text to find enclosing symbols at cursor line
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0) {
        if (m_hwndBreadcrumbs) InvalidateRect(m_hwndBreadcrumbs, nullptr, TRUE);
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

    while (std::getline(stream, lineText)) {
        lineNum++;

        // Trim leading whitespace
        std::string trimmed = lineText;
        size_t pos = trimmed.find_first_not_of(" \t");
        if (pos != std::string::npos) trimmed = trimmed.substr(pos);

        // Detect namespace
        if (trimmed.find("namespace ") == 0 && trimmed.find(';') == std::string::npos) {
            // Extract namespace name
            size_t nameStart = 10;
            size_t nameEnd = trimmed.find_first_of(" {", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            currentNamespace = trimmed.substr(nameStart, nameEnd - nameStart);
        }

        // Detect class/struct
        if ((trimmed.find("class ") == 0 || trimmed.find("struct ") == 0) &&
            trimmed.find(';') == std::string::npos) {
            size_t nameStart = (trimmed[0] == 'c') ? 6 : 7;
            size_t nameEnd = trimmed.find_first_of(" :{", nameStart);
            if (nameEnd == std::string::npos) nameEnd = trimmed.size();
            if (nameEnd > nameStart) {
                currentClass = trimmed.substr(nameStart, nameEnd - nameStart);
                classLine = lineNum;
                classBraceDepth = braceDepth;
            }
        }

        // Detect function (heuristic: identifier followed by "(" not in control flow)
        if (trimmed.find("(") != std::string::npos &&
            trimmed.find("if ") != 0 && trimmed.find("if(") != 0 &&
            trimmed.find("for ") != 0 && trimmed.find("for(") != 0 &&
            trimmed.find("while ") != 0 && trimmed.find("while(") != 0 &&
            trimmed.find("switch ") != 0 && trimmed.find("switch(") != 0 &&
            trimmed.find("return ") != 0 && trimmed.find("//") != 0 &&
            trimmed.find('#') != 0) {
            // Extract function name (word before '(')
            size_t parenPos = trimmed.find('(');
            if (parenPos > 0) {
                size_t nameEnd = parenPos;
                size_t nameStart = nameEnd;
                while (nameStart > 0 && (isalnum(trimmed[nameStart-1]) || trimmed[nameStart-1] == '_' || trimmed[nameStart-1] == ':')) {
                    nameStart--;
                }
                if (nameStart < nameEnd) {
                    std::string funcName = trimmed.substr(nameStart, nameEnd - nameStart);
                    // Filter out common non-function patterns
                    if (funcName != "sizeof" && funcName != "static_cast" &&
                        funcName != "dynamic_cast" && funcName != "reinterpret_cast" &&
                        funcName != "const_cast" && funcName != "decltype" &&
                        !funcName.empty()) {
                        currentFunction = funcName;
                        funcLine = lineNum;
                        funcBraceDepth = braceDepth;
                    }
                }
            }
        }

        // Track brace depth
        for (char c : lineText) {
            if (c == '{') braceDepth++;
            if (c == '}') {
                braceDepth--;
                // Check if we exited the class scope
                if (classBraceDepth >= 0 && braceDepth <= classBraceDepth && lineNum <= line) {
                    // We've closed the class; if cursor is after, clear class
                }
                // Check if we exited the function scope
                if (funcBraceDepth >= 0 && braceDepth <= funcBraceDepth && lineNum <= line) {
                    if (lineNum < line) {
                        currentFunction.clear();
                        funcBraceDepth = -1;
                    }
                }
            }
        }

        if (lineNum >= line) break; // Stop at cursor line
    }

    // Build breadcrumb path
    if (!currentNamespace.empty()) {
        BreadcrumbItem ns;
        ns.label = currentNamespace;
        ns.symbolKind = "namespace";
        ns.line = 1;
        ns.column = 0;
        m_breadcrumbPath.push_back(ns);
    }

    if (!currentClass.empty()) {
        BreadcrumbItem cls;
        cls.label = currentClass;
        cls.symbolKind = "class";
        cls.line = classLine;
        cls.column = 0;
        m_breadcrumbPath.push_back(cls);
    }

    if (!currentFunction.empty()) {
        BreadcrumbItem func;
        func.label = currentFunction;
        func.symbolKind = "function";
        func.line = funcLine;
        func.column = 0;
        m_breadcrumbPath.push_back(func);
    }

    if (m_hwndBreadcrumbs) {
        InvalidateRect(m_hwndBreadcrumbs, nullptr, TRUE);
    }
}

// ============================================================================
// BREADCRUMB CLICK — Navigate to symbol
// ============================================================================

void Win32IDE::onBreadcrumbClick(int index)
{
    if (index < 0 || index >= static_cast<int>(m_breadcrumbPath.size())) return;

    const auto& item = m_breadcrumbPath[index];

    if (item.line > 0 && m_hwndEditor) {
        // Navigate to the symbol's line
        int lineIndex = static_cast<int>(SendMessage(m_hwndEditor, EM_LINEINDEX, item.line - 1, 0));
        SendMessage(m_hwndEditor, EM_SETSEL, lineIndex, lineIndex);
        SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
        SetFocus(m_hwndEditor);
    }
}

// ============================================================================
// BREADCRUMB PAINTING — VS Code-style breadcrumb bar
// ============================================================================

void Win32IDE::paintBreadcrumbs(HDC hdc, RECT& rc)
{
    // Fill background with VSU Acrylic effect
    HBRUSH bgBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(BC_BG));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Draw bottom border
    HPEN borderPen = CreatePen(PS_SOLID, 1, AdobeRGBaToCOLORREF(VSU::Acrylic::DarkLuminosity));
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.right, rc.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    if (m_breadcrumbPath.empty()) return;

    m_breadcrumbRects.clear();
    m_breadcrumbRects.reserve(m_breadcrumbPath.size());

    // Set font and text properties
    HFONT oldFont = (HFONT)SelectObject(hdc, m_breadcrumbFont ? m_breadcrumbFont : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);

    int xPos = 8; // left margin

    for (size_t i = 0; i < m_breadcrumbPath.size(); i++) {
        const auto& item = m_breadcrumbPath[i];
        int itemLeft = xPos;

        // Draw separator before items (except first)
        if (i > 0) {
            SetTextColor(hdc, AdobeRGBaToCOLORREF(BC_SEPARATOR));
            const char* sep = " > ";
            RECT sepRect = { xPos, rc.top, xPos + 24, rc.bottom };
            DrawTextA(hdc, sep, -1, &sepRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            xPos += 22;
        }

        // Determine icon color based on symbol kind
        COLORREF iconColor = AdobeRGBaToCOLORREF(BC_ICON_FILE);
        const char* icon = "";
        if (item.symbolKind == "class" || item.symbolKind == "struct") {
            iconColor = AdobeRGBaToCOLORREF(BC_ICON_CLASS);
            icon = "C ";
        } else if (item.symbolKind == "method" || item.symbolKind == "function") {
            iconColor = AdobeRGBaToCOLORREF(BC_ICON_METHOD);
            icon = "f ";
        } else if (item.symbolKind == "namespace") {
            iconColor = AdobeRGBaToCOLORREF(BC_ICON_FUNC);
            icon = "N ";
        } else if (item.symbolKind == "folder" || item.symbolKind == "root") {
            iconColor = AdobeRGBaToCOLORREF(BC_ICON_FOLDER);
            icon = "D ";
        } else if (item.symbolKind == "file") {
            icon = "";
        }

        // Draw icon prefix
        if (icon[0] != '\0') {
            SetTextColor(hdc, iconColor);
            RECT iconRect = { xPos, rc.top, xPos + 16, rc.bottom };
            DrawTextA(hdc, icon, -1, &iconRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            xPos += 14;
        }

        // Draw breadcrumb text
        SetTextColor(hdc, (i == m_breadcrumbPath.size() - 1) ? AdobeRGBaToCOLORREF(BC_ACTIVE_TEXT) : AdobeRGBaToCOLORREF(BC_TEXT));
        SIZE textSize;
        GetTextExtentPoint32A(hdc, item.label.c_str(), static_cast<int>(item.label.size()), &textSize);

        RECT textRect = { xPos, rc.top, xPos + textSize.cx, rc.bottom };
        DrawTextA(hdc, item.label.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        xPos += textSize.cx + 4;

        RECT hitRect = { itemLeft, rc.top, xPos, rc.bottom };
        if (static_cast<int>(i) == g_breadcrumbHoverIndex) {
            RECT hoverRect = { hitRect.left - 2, rc.top + 2, hitRect.right + 2, rc.bottom - 2 };
            HBRUSH hHover = CreateSolidBrush(AdobeRGBaToCOLORREF(BC_HOVER));
            FillRect(hdc, &hoverRect, hHover);
            DeleteObject(hHover);

            // Re-draw text/icon above hover fill.
            if (icon[0] != '\0') {
                SetTextColor(hdc, iconColor);
                RECT iconRect = { itemLeft + ((i > 0) ? 22 : 0), rc.top, itemLeft + ((i > 0) ? 22 : 0) + 16, rc.bottom };
                DrawTextA(hdc, icon, -1, &iconRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            SetTextColor(hdc, (i == m_breadcrumbPath.size() - 1) ? AdobeRGBaToCOLORREF(BC_ACTIVE_TEXT) : AdobeRGBaToCOLORREF(BC_TEXT));
            DrawTextA(hdc, item.label.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
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

    switch (uMsg) {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        if (pThis) {
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
        if (pThis) {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            int clickedIndex = -1;
            for (size_t i = 0; i < pThis->m_breadcrumbRects.size(); i++) {
                if (PtInRect(&pThis->m_breadcrumbRects[i], pt)) {
                    clickedIndex = static_cast<int>(i);
                    break;
                }
            }
            if (clickedIndex >= 0)
                pThis->onBreadcrumbClick(clickedIndex);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (pThis) {
            TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);

            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            int hoverIndex = -1;
            for (size_t i = 0; i < pThis->m_breadcrumbRects.size(); i++) {
                if (PtInRect(&pThis->m_breadcrumbRects[i], pt)) {
                    hoverIndex = static_cast<int>(i);
                    break;
                }
            }
            if (hoverIndex != g_breadcrumbHoverIndex) {
                g_breadcrumbHoverIndex = hoverIndex;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_breadcrumbHoverIndex != -1) {
            g_breadcrumbHoverIndex = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
