// Win32IDE_LineStripEditor.cpp — Track B line-strip cache live editor presentation
// Gated by RAWRXD_SOFTWARE_BLIT_RASTER=1 and RAWRXD_LINE_STRIP_CACHE=1.

#include "IDELogger.h"
#include "Win32IDE.h"

#include "rawrxd/rawrxd_linestream_raster.h"
#include "rawrxd/rawrxd_linestrip_cache.h"
#include "rawrxd/rawrxd_software_raster.h"
#include "rawrxd/rawrxd_syntax_bridge.h"
#include "rawrxd/rawrxd_win32_syntax_bridge.h"
#include "rawrxd/rawrxd_workspace_matrix.h"

#include <algorithm>
#include <cstring>
#include <imm.h>
#include <richedit.h>

#ifndef IDC_LINE_STRIP_OVERLAY
#define IDC_LINE_STRIP_OVERLAY 19997
#endif

namespace
{

constexpr std::uint32_t kMaxLines = 8192u;
constexpr std::uint32_t kMaxCharsPerLine = 512u;
constexpr std::uint32_t kMaxRunsPerLine = 32u;
constexpr COLORREF kEditorBackground = RGB(30, 30, 30);
constexpr COLORREF kSelectionBackground = RGB(38, 79, 120);
constexpr COLORREF kCaretLineColor = RGB(255, 255, 255);
constexpr COLORREF kGhostTextColor = RGB(120, 124, 132);

alignas(64) std::uint8_t g_lineStripArena[16 * 1024 * 1024];
bool g_lineStripNativeCaretHidden = false;
bool g_lineStripCaretBlinkOn = true;

int queryLineStripTabWidth(HWND hwndEditor, int cellWidth)
{
    if (!hwndEditor)
    {
        return (std::max)(4, cellWidth > 0 ? cellWidth : 8);
    }

    PARAFORMAT2 pf{};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_TABSTOPS;
    if (SendMessage(hwndEditor, EM_GETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&pf)) && pf.cTabCount > 0)
    {
        const LONG tabTwips = pf.rgxTabs[0] & 0x00FFFFFF;
        if (tabTwips > 0)
        {
            HDC hdc = GetDC(hwndEditor);
            if (hdc)
            {
                const int px = MulDiv(tabTwips, GetDeviceCaps(hdc, LOGPIXELSX), 1440);
                ReleaseDC(hwndEditor, hdc);
                if (px > 0)
                {
                    return px;
                }
            }
        }
    }

    return (std::max)(cellWidth * 4, 32);
}

std::string expandTabsForLineStrip(const std::string& line, int tabWidthPx, int cellWidth)
{
    const int tabCols = (std::max)(1, tabWidthPx / (std::max)(1, cellWidth));
    std::string expanded;
    expanded.reserve(line.size() + 8);
    int column = 0;
    for (unsigned char ch : line)
    {
        if (ch == '\t')
        {
            const int spaces = tabCols - (column % tabCols);
            expanded.append(static_cast<std::size_t>(spaces), ' ');
            column += spaces;
            continue;
        }
        expanded.push_back(static_cast<char>(ch));
        ++column;
    }
    return expanded;
}

int lineStripTabColumnCount(int tabWidthPx, int cellWidth)
{
    return (std::max)(1, tabWidthPx / (std::max)(1, cellWidth));
}

int charOffsetToVisualColumn(const char* text, int textLen, int charOffset, int tabCols)
{
    if (!text || charOffset <= 0)
    {
        return 0;
    }

    const int limit = (std::min)(charOffset, textLen);
    int column = 0;
    for (int i = 0; i < limit; ++i)
    {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == '\t')
        {
            column += tabCols - (column % tabCols);
        }
        else
        {
            ++column;
        }
    }
    return column;
}

bool fetchRichEditLineText(HWND hwndEditor, int lineIndex, std::string* outLine)
{
    if (!hwndEditor || !outLine)
    {
        return false;
    }

    const int lineStartChar =
        static_cast<int>(SendMessage(hwndEditor, EM_LINEINDEX, static_cast<WPARAM>(lineIndex), 0));
    const int lineLen = static_cast<int>(SendMessage(hwndEditor, EM_LINELENGTH, static_cast<WPARAM>(lineStartChar), 0));
    if (lineLen <= 0)
    {
        outLine->clear();
        return true;
    }

    std::vector<char> buffer(static_cast<std::size_t>(lineLen) + 2u);
    *reinterpret_cast<WORD*>(buffer.data()) = static_cast<WORD>(lineLen);
    const int copied = static_cast<int>(
        SendMessage(hwndEditor, EM_GETLINE, static_cast<WPARAM>(lineIndex), reinterpret_cast<LPARAM>(buffer.data())));
    if (copied <= 0)
    {
        outLine->clear();
        return false;
    }

    outLine->assign(buffer.data(), static_cast<std::size_t>(copied));
    return true;
}

void paintDiagnosticWave(rawrxd::ui::SoftwareRenderSurface* surface, int x0, int x1, int y, std::uint32_t color)
{
    if (!surface || !surface->surfaceBits || x1 <= x0)
    {
        return;
    }

    const int baseY = y;
    for (int x = x0; x < x1; x += 3)
    {
        for (int dy = 0; dy < 2; ++dy)
        {
            const int py = baseY + ((x / 3) % 2 == 0 ? dy : (1 - dy));
            if (x < 0 || py < 0 || static_cast<std::uint32_t>(x) >= surface->width ||
                static_cast<std::uint32_t>(py) >= surface->height)
            {
                continue;
            }
            surface->surfaceBits[static_cast<std::size_t>(py) * surface->width + static_cast<std::size_t>(x)] = color;
        }
    }
}

std::uint32_t blendArgb(std::uint32_t dst, std::uint32_t src, std::uint8_t alpha)
{
    const std::uint8_t inv = static_cast<std::uint8_t>(255 - alpha);
    const std::uint8_t dr = static_cast<std::uint8_t>((dst >> 16) & 0xFFu);
    const std::uint8_t dg = static_cast<std::uint8_t>((dst >> 8) & 0xFFu);
    const std::uint8_t db = static_cast<std::uint8_t>(dst & 0xFFu);
    const std::uint8_t sr = static_cast<std::uint8_t>((src >> 16) & 0xFFu);
    const std::uint8_t sg = static_cast<std::uint8_t>((src >> 8) & 0xFFu);
    const std::uint8_t sb = static_cast<std::uint8_t>(src & 0xFFu);
    const std::uint8_t r = static_cast<std::uint8_t>((sr * alpha + dr * inv) / 255);
    const std::uint8_t g = static_cast<std::uint8_t>((sg * alpha + dg * inv) / 255);
    const std::uint8_t b = static_cast<std::uint8_t>((sb * alpha + db * inv) / 255);
    return (0xFF000000u) | (static_cast<std::uint32_t>(r) << 16) | (static_cast<std::uint32_t>(g) << 8) |
           static_cast<std::uint32_t>(b);
}

void fillRectOnSurface(rawrxd::ui::SoftwareRenderSurface* surface, RECT rc, std::uint32_t packedColor)
{
    if (!surface || !surface->surfaceBits || surface->width == 0 || surface->height == 0)
    {
        return;
    }

    if (rc.left < 0)
    {
        rc.left = 0;
    }
    if (rc.top < 0)
    {
        rc.top = 0;
    }
    if (rc.right > static_cast<LONG>(surface->width))
    {
        rc.right = static_cast<LONG>(surface->width);
    }
    if (rc.bottom > static_cast<LONG>(surface->height))
    {
        rc.bottom = static_cast<LONG>(surface->height);
    }
    if (rc.right <= rc.left || rc.bottom <= rc.top)
    {
        return;
    }

    for (LONG y = rc.top; y < rc.bottom; ++y)
    {
        std::uint32_t* row =
            surface->surfaceBits + static_cast<std::size_t>(y) * surface->width + static_cast<std::size_t>(rc.left);
        const std::uint32_t span = static_cast<std::uint32_t>(rc.right - rc.left);
        for (std::uint32_t i = 0; i < span; ++i)
        {
            row[i] = packedColor;
        }
    }
}

void paintSelectionParityLayer(HWND hwndEditor, rawrxd::ui::SoftwareRenderSurface* surface, int firstVisibleLine,
                               int lineHeight, int cellWidth, int horzScroll, int viewportStartY, int dirtyTopLine,
                               int dirtyBottomLine)
{
    if (!hwndEditor || !surface)
    {
        return;
    }

    CHARRANGE sel{};
    SendMessage(hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin == sel.cpMax)
    {
        return;
    }

    const LONG selMin = std::min(sel.cpMin, sel.cpMax);
    const LONG selMax = std::max(sel.cpMin, sel.cpMax);
    const std::uint32_t packed = rawrxd::ui::packColorRef(kSelectionBackground);
    const int tabWidthPx = queryLineStripTabWidth(hwndEditor, cellWidth);
    const int tabCols = lineStripTabColumnCount(tabWidthPx, cellWidth);

    for (int line = dirtyTopLine; line <= dirtyBottomLine; ++line)
    {
        const int lineStartChar = static_cast<int>(SendMessage(hwndEditor, EM_LINEINDEX, static_cast<WPARAM>(line), 0));
        const int lineLen =
            static_cast<int>(SendMessage(hwndEditor, EM_LINELENGTH, static_cast<WPARAM>(lineStartChar), 0));
        const int lineEndChar = lineStartChar + lineLen;

        const int overlapStart = std::max(selMin, static_cast<LONG>(lineStartChar));
        const int overlapEnd = std::min(selMax, static_cast<LONG>(lineEndChar));
        if (overlapStart >= overlapEnd)
        {
            continue;
        }

        const int colStart = overlapStart - lineStartChar;
        const int colEnd = overlapEnd - lineStartChar;

        std::string lineText;
        if (!fetchRichEditLineText(hwndEditor, line, &lineText))
        {
            continue;
        }

        const int visualColStart =
            charOffsetToVisualColumn(lineText.data(), static_cast<int>(lineText.size()), colStart, tabCols);
        const int visualColEnd =
            charOffsetToVisualColumn(lineText.data(), static_cast<int>(lineText.size()), colEnd, tabCols);

        RECT rowRect{};
        rowRect.left = visualColStart * cellWidth - horzScroll;
        rowRect.right = visualColEnd * cellWidth - horzScroll;
        rowRect.top = (line - firstVisibleLine) * lineHeight + viewportStartY;
        rowRect.bottom = rowRect.top + lineHeight;
        for (LONG y = rowRect.top; y < rowRect.bottom; ++y)
        {
            if (y < 0 || static_cast<std::uint32_t>(y) >= surface->height)
            {
                continue;
            }
            std::uint32_t* row = surface->surfaceBits + static_cast<std::size_t>(y) * surface->width +
                                 static_cast<std::size_t>(rowRect.left);
            const std::uint32_t span = static_cast<std::uint32_t>(rowRect.right - rowRect.left);
            for (std::uint32_t i = 0; i < span; ++i)
            {
                row[i] = blendArgb(row[i], packed, 96);
            }
        }
    }
}

void paintCaretParityLayer(HWND hwndEditor, rawrxd::ui::SoftwareRenderSurface* surface, int firstVisibleLine,
                           int lineHeight, int cellWidth, int horzScroll, int viewportStartY, bool caretVisible)
{
    if (!hwndEditor || !surface || !caretVisible)
    {
        return;
    }

    CHARRANGE sel{};
    SendMessage(hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != sel.cpMax)
    {
        return;
    }

    const int caretPos = static_cast<int>(sel.cpMin);
    const int line = static_cast<int>(SendMessage(hwndEditor, EM_LINEFROMCHAR, static_cast<WPARAM>(caretPos), 0));
    const int lineStartChar = static_cast<int>(SendMessage(hwndEditor, EM_LINEINDEX, static_cast<WPARAM>(line), 0));
    const int column = caretPos - lineStartChar;

    int caretX = column * cellWidth - horzScroll;
    std::string lineText;
    if (fetchRichEditLineText(hwndEditor, line, &lineText))
    {
        const int tabWidthPx = queryLineStripTabWidth(hwndEditor, cellWidth);
        const int tabCols = lineStripTabColumnCount(tabWidthPx, cellWidth);
        const int visualColumn =
            charOffsetToVisualColumn(lineText.data(), static_cast<int>(lineText.size()), column, tabCols);
        caretX = visualColumn * cellWidth - horzScroll;
    }

    const int caretY = (line - firstVisibleLine) * lineHeight + viewportStartY;

    RECT caretRect{};
    caretRect.left = caretX;
    caretRect.right = caretX + 2;
    caretRect.top = caretY;
    caretRect.bottom = caretY + lineHeight;
    fillRectOnSurface(surface, caretRect, rawrxd::ui::packColorRef(kCaretLineColor));
}

void splitUtf8DocumentLines(const std::string& text, std::vector<std::string>* outLines)
{
    if (!outLines)
    {
        return;
    }
    outLines->clear();
    if (text.empty())
    {
        outLines->emplace_back();
        return;
    }

    std::size_t lineStart = 0;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\n')
        {
            outLines->emplace_back(text.data() + lineStart, i - lineStart);
            lineStart = i + 1;
        }
    }
    if (lineStart <= text.size())
    {
        outLines->emplace_back(text.data() + lineStart, text.size() - lineStart);
    }
    if (outLines->empty())
    {
        outLines->emplace_back();
    }
}

}  // namespace

bool Win32IDE::lineStripEditorEnabled() const
{
    return rawrxd::ui::softwareBlitRasterEnabled() && rawrxd::ui::lineStripCacheEnabled();
}

int Win32IDE::getEditorLineHeightPx() const
{
    if (!m_hwndEditor)
    {
        return 16;
    }

    HDC hdc = GetDC(m_hwndEditor);
    if (!hdc)
    {
        return 16;
    }

    HFONT hFont = m_editorFont ? m_editorFont : static_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT));
    HFONT hOld = static_cast<HFONT>(SelectObject(hdc, hFont));
    TEXTMETRICA tm{};
    GetTextMetricsA(hdc, &tm);
    SelectObject(hdc, hOld);
    ReleaseDC(m_hwndEditor, hdc);

    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    return lineHeight > 0 ? lineHeight : 16;
}

void Win32IDE::createLineStripOverlay(HWND hwndParent)
{
    if (!hwndParent || !lineStripEditorEnabled())
    {
        return;
    }
    if (m_hwndLineStripOverlay && IsWindow(m_hwndLineStripOverlay))
    {
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXA wc{};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = LineStripOverlayProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
        wc.lpszClassName = "RawrXDLineStripOverlay";
        if (RegisterClassExA(&wc))
        {
            classRegistered = true;
        }
    }

    m_hwndLineStripOverlay = CreateWindowExA(
        WS_EX_LAYERED, "RawrXDLineStripOverlay", "", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1, 1, hwndParent,
        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(IDC_LINE_STRIP_OVERLAY)), m_hInstance, nullptr);

    if (m_hwndLineStripOverlay)
    {
        SetPropA(m_hwndLineStripOverlay, "IDE_PTR", reinterpret_cast<HANDLE>(this));
        SetLayeredWindowAttributes(m_hwndLineStripOverlay, 0, 255, LWA_ALPHA);
        LOG_INFO("Line-strip syntax overlay created");
    }
}

void Win32IDE::layoutLineStripOverlay()
{
    if (!m_hwndLineStripOverlay || !IsWindow(m_hwndLineStripOverlay) || !m_hwndEditor)
    {
        return;
    }

    HWND parent = GetParent(m_hwndEditor);
    if (!parent)
    {
        parent = m_hwndMain;
    }

    RECT editorRect{};
    GetWindowRect(m_hwndEditor, &editorRect);
    POINT topLeft{editorRect.left, editorRect.top};
    ScreenToClient(parent, &topLeft);

    const int width = editorRect.right - editorRect.left;
    const int height = editorRect.bottom - editorRect.top;
    SetWindowPos(m_hwndLineStripOverlay, HWND_TOP, topLeft.x, topLeft.y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    const std::uint32_t newW = static_cast<std::uint32_t>(std::max(1, width));
    const std::uint32_t newH = static_cast<std::uint32_t>(std::max(1, height));
    if (newW != m_lineStripSurfaceW || newH != m_lineStripSurfaceH)
    {
        m_lineStripSurfaceW = newW;
        m_lineStripSurfaceH = newH;
        m_lineStripEditorReady = false;
    }

    HDC overlayDc = GetDC(m_hwndLineStripOverlay);
    if (overlayDc)
    {
        ensureLineStripBackbuffer(overlayDc, width, height);
        ReleaseDC(m_hwndLineStripOverlay, overlayDc);
    }
}

bool Win32IDE::ensureLineStripBackbuffer(HDC refDc, int width, int height)
{
    if (!refDc || width <= 0 || height <= 0)
    {
        return false;
    }
    if (m_lineStripBakeDc && m_lineStripBakeBitmap && m_lineStripBakeW == width && m_lineStripBakeH == height)
    {
        return true;
    }

    if (!m_lineStripBakeDc)
    {
        m_lineStripBakeDc = CreateCompatibleDC(refDc);
        if (!m_lineStripBakeDc)
        {
            return false;
        }
    }

    HBITMAP newBitmap = CreateCompatibleBitmap(refDc, width, height);
    if (!newBitmap)
    {
        return false;
    }

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(m_lineStripBakeDc, newBitmap));
    if (oldBitmap && oldBitmap != m_lineStripBakeBitmap)
    {
        DeleteObject(oldBitmap);
    }
    if (m_lineStripBakeBitmap && m_lineStripBakeBitmap != oldBitmap)
    {
        DeleteObject(m_lineStripBakeBitmap);
    }

    m_lineStripBakeBitmap = newBitmap;
    m_lineStripBakeW = width;
    m_lineStripBakeH = height;
    return true;
}

void Win32IDE::shutdownLineStripEditor()
{
    if (m_lineStripBakeDc)
    {
        if (m_lineStripBakeBitmap)
        {
            SelectObject(m_lineStripBakeDc, m_lineStripBakeBitmap);
        }
        DeleteDC(m_lineStripBakeDc);
        m_lineStripBakeDc = nullptr;
    }
    if (m_lineStripBakeBitmap)
    {
        DeleteObject(m_lineStripBakeBitmap);
        m_lineStripBakeBitmap = nullptr;
    }
    m_lineStripBakeW = 0;
    m_lineStripBakeH = 0;

    m_lineStripController.shutdown();
    rawrxd::ui::shutdownSoftwareRaster(&m_lineStripRaster, &m_lineStripSurface);

    m_lineStripOwnedLines.clear();
    m_lineStripDocLines.clear();
    m_lineStripLineVersions.clear();
    m_lineStripRuns.clear();
    m_lineStripRunCounts.clear();
    m_lineStripRunTable.clear();
    m_lineStripEditorReady = false;
    m_lineStripDocumentDirty = true;

    if (m_hwndMain && IsWindow(m_hwndMain))
    {
        KillTimer(m_hwndMain, LINE_STRIP_SYNC_TIMER_ID);
        KillTimer(m_hwndMain, LINE_STRIP_CARET_BLINK_TIMER_ID);
    }

    if (m_hwndLineStripOverlay && IsWindow(m_hwndLineStripOverlay))
    {
        DestroyWindow(m_hwndLineStripOverlay);
        m_hwndLineStripOverlay = nullptr;
    }
}

bool Win32IDE::ensureLineStripEditorInitialized()
{
    if (!lineStripEditorEnabled() || !m_hwndEditor)
    {
        return false;
    }
    if (m_lineStripEditorReady)
    {
        return true;
    }

    RECT rc{};
    GetClientRect(m_hwndEditor, &rc);
    const std::uint32_t width = static_cast<std::uint32_t>(std::max(1L, rc.right - rc.left));
    const std::uint32_t height = static_cast<std::uint32_t>(std::max(1L, rc.bottom - rc.top));
    m_lineStripSurfaceW = width;
    m_lineStripSurfaceH = height;

    HDC screenDc = GetDC(m_hwndEditor);
    if (!screenDc)
    {
        return false;
    }

    HFONT font = m_editorFont ? m_editorFont : static_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT));
    if (!rawrxd::ui::initializeSoftwareRaster(screenDc, font, &m_lineStripRaster, &m_lineStripSurface, width, height) ||
        !rawrxd::ui::buildLineStreamWorkspaceFromSoftwareAtlas(&m_lineStripRaster, &m_lineStripStream))
    {
        ReleaseDC(m_hwndEditor, screenDc);
        return false;
    }

    ReleaseDC(m_hwndEditor, screenDc);

    HDC overlayDc = GetDC(m_hwndLineStripOverlay);
    if (overlayDc)
    {
        ensureLineStripBackbuffer(overlayDc, static_cast<int>(width), static_cast<int>(height));
        ReleaseDC(m_hwndLineStripOverlay, overlayDc);
    }

    const std::size_t arenaRequired = rawrxd::ui::SovereignWorkspaceController::requiredArenaBytes(
        kMaxLines, m_lineStripStream.cellWidth, m_lineStripStream.cellHeight, kMaxCharsPerLine);
    if (arenaRequired == 0 || arenaRequired > sizeof(g_lineStripArena) ||
        !m_lineStripController.initialize(g_lineStripArena, sizeof(g_lineStripArena), kMaxLines,
                                          m_lineStripStream.cellWidth, m_lineStripStream.cellHeight, kMaxCharsPerLine))
    {
        return false;
    }

    m_lineStripRuns.resize(static_cast<std::size_t>(kMaxLines) * kMaxRunsPerLine);
    m_lineStripRunCounts.assign(kMaxLines, 0u);
    m_lineStripRunTable.assign(kMaxLines, nullptr);
    m_lineStripLineVersions.assign(kMaxLines, 1u);
    m_lineStripDocLines.resize(kMaxLines);

    m_lineStripEditorReady = true;
    if (m_hwndEditor)
    {
        HideCaret(m_hwndEditor);
        g_lineStripNativeCaretHidden = true;
    }
    if (m_hwndMain)
    {
        SetTimer(m_hwndMain, LINE_STRIP_CARET_BLINK_TIMER_ID, LINE_STRIP_CARET_BLINK_MS, nullptr);
    }
    return true;
}

void Win32IDE::maskRichEditForLineStripOverlay()
{
    if (!m_hwndEditor)
    {
        return;
    }

    const int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen <= 0)
    {
        return;
    }

    CHARRANGE crAll{0, textLen};
    SendMessageA(m_hwndEditor, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&crAll));
    CHARFORMAT2A cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = kEditorBackground;
    SendMessageA(m_hwndEditor, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf));
}

void Win32IDE::syncLineStripDocumentFromEditor()
{
    if (!lineStripEditorEnabled() || !m_hwndEditor)
    {
        return;
    }
    if (!ensureLineStripEditorInitialized())
    {
        return;
    }

    const int textLen = GetWindowTextLengthA(m_hwndEditor);
    std::string text;
    if (textLen > 0)
    {
        text.resize(static_cast<std::size_t>(textLen));
        GetWindowTextA(m_hwndEditor, text.data(), textLen + 1);
    }

    splitUtf8DocumentLines(text, &m_lineStripOwnedLines);
    const std::uint32_t lineCount =
        static_cast<std::uint32_t>(std::min<std::size_t>(m_lineStripOwnedLines.size(), kMaxLines));

    m_lineStripDocLines.resize(lineCount);
    if (m_lineStripLineVersions.size() < lineCount)
    {
        m_lineStripLineVersions.resize(lineCount, 1u);
    }

    int lineStartOffset = 0;
    const int cellWidth = static_cast<int>(m_lineStripStream.cellWidth > 0 ? m_lineStripStream.cellWidth : 8);
    const int tabWidthPx = queryLineStripTabWidth(m_hwndEditor, cellWidth);

    for (std::uint32_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
    {
        std::string& owned = m_lineStripOwnedLines[lineIndex];
        if (owned.find('\t') != std::string::npos)
        {
            owned = expandTabsForLineStrip(owned, tabWidthPx, cellWidth);
        }
        if (owned.size() > kMaxCharsPerLine)
        {
            owned.resize(kMaxCharsPerLine);
        }

        rawrxd::ui::DocumentLine& doc = m_lineStripDocLines[lineIndex];
        doc.rawBufferStart = owned.data();
        doc.lengthBytes = static_cast<std::uint32_t>(owned.size());
        ++m_lineStripLineVersions[lineIndex];
        doc.lineVersion = m_lineStripLineVersions[lineIndex];
        m_lineStripController.invalidateLineStructure(lineIndex, doc.lineVersion);

        rawrxd::ui::SyntaxColorRun* runStorage =
            &m_lineStripRuns[static_cast<std::size_t>(lineIndex) * kMaxRunsPerLine];
        std::uint32_t runCount = 0;

        if (m_syntaxLanguage != SyntaxLanguage::None && m_syntaxColoringEnabled)
        {
            const std::vector<SyntaxToken> tokens = tokenizeLine(owned, lineStartOffset, m_syntaxLanguage);
            rawrxd::ui::LineRenderBatch batch{};
            if (rawrxd::ui::fillLineRenderBatchFromWin32Tokens(&batch, owned, lineStartOffset, tokens.data(),
                                                               tokens.size(), sizeof(SyntaxToken)))
            {
                rawrxd::ui::exportBatchToSyntaxColorRuns(batch, runStorage, &runCount);
            }
        }

        if (runCount == 0 && doc.lengthBytes > 0)
        {
            runStorage[0] = {0u, doc.lengthBytes, RGB(212, 212, 212)};
            runCount = 1u;
        }

        m_lineStripRunCounts[lineIndex] = std::min(runCount, kMaxRunsPerLine);
        m_lineStripRunTable[lineIndex] = runStorage;

        lineStartOffset += static_cast<int>(owned.size()) + 1;
    }

    m_lineStripDocumentDirty = false;
}

void Win32IDE::syncLineStripDirtyStrips()
{
    if (!m_lineStripEditorReady)
    {
        return;
    }

    const std::uint32_t lineCount = static_cast<std::uint32_t>(m_lineStripDocLines.size());
    if (lineCount == 0)
    {
        return;
    }

    m_lineStripController.synchronizeDirtyStrips(&m_lineStripStream, m_lineStripDocLines.data(), lineCount,
                                                 m_lineStripRunTable.data(), m_lineStripRunCounts.data(), true);
}

void Win32IDE::syncLineStripImeComposition()
{
    if (!lineStripEditorEnabled() || !m_hwndEditor)
    {
        return;
    }

    CHARRANGE sel{};
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    const int caretPos = static_cast<int>(sel.cpMin);

    POINTL pt{};
    SendMessage(m_hwndEditor, EM_POSFROMCHAR, static_cast<WPARAM>(caretPos), reinterpret_cast<LPARAM>(&pt));

    POINT clientPt{pt.x, pt.y};
    ClientToScreen(m_hwndEditor, &clientPt);

    HIMC imc = ImmGetContext(m_hwndEditor);
    if (!imc)
    {
        return;
    }

    COMPOSITIONFORM form{};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = clientPt;
    ImmSetCompositionWindow(imc, &form);
    ImmReleaseContext(m_hwndEditor, imc);
}

void Win32IDE::updateLineStripGhostCaretMargin()
{
    if (!m_hwndEditor || !lineStripEditorEnabled())
    {
        return;
    }

    if (!m_ghostTextVisible || m_ghostTextContent.empty())
    {
        SendMessage(m_hwndEditor, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
        return;
    }

    int cellWidth = static_cast<int>(m_lineStripStream.cellWidth);
    if (cellWidth <= 0)
    {
        cellWidth = 8;
    }
    const int ghostPx =
        std::min(cellWidth * static_cast<int>(m_ghostTextContent.size()), static_cast<int>(m_lineStripSurfaceW));
    SendMessage(m_hwndEditor, EM_SETMARGINS, EC_RIGHTMARGIN, MAKELPARAM(0, static_cast<LPARAM>(ghostPx)));
}

void Win32IDE::onLineStripCaretBlinkTimer()
{
    g_lineStripCaretBlinkOn = !g_lineStripCaretBlinkOn;
    if (!m_hwndEditor || !m_hwndLineStripOverlay)
    {
        return;
    }

    CHARRANGE sel{};
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));
    if (sel.cpMin != sel.cpMax)
    {
        return;
    }

    const int lineHeight = getEditorLineHeightPx();
    if (lineHeight <= 0)
    {
        return;
    }

    const int firstVisibleLine = static_cast<int>(SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int line = static_cast<int>(SendMessage(m_hwndEditor, EM_LINEFROMCHAR, static_cast<WPARAM>(sel.cpMin), 0));
    const int relativeLine = line - firstVisibleLine;

    RECT rowRect{};
    rowRect.top = relativeLine * lineHeight;
    rowRect.bottom = rowRect.top + lineHeight;
    rowRect.left = 0;
    rowRect.right = 64;
    InvalidateRect(m_hwndLineStripOverlay, &rowRect, FALSE);
}

void Win32IDE::paintLineStripGhostTextParity(rawrxd::ui::SoftwareRenderSurface* surface, int firstVisibleLine,
                                             int lineHeight, int cellWidth, int horzScroll, int viewportStartY)
{
    if (!surface || !m_ghostTextVisible || m_ghostTextContent.empty() || !m_hwndEditor)
    {
        return;
    }

    const int anchorPos = m_ghostTextRequestCursorPos >= 0 ? m_ghostTextRequestCursorPos : m_ghostTextStartPos;
    if (anchorPos < 0)
    {
        return;
    }

    const int line = static_cast<int>(SendMessage(m_hwndEditor, EM_LINEFROMCHAR, static_cast<WPARAM>(anchorPos), 0));
    const int lineStartChar = static_cast<int>(SendMessage(m_hwndEditor, EM_LINEINDEX, static_cast<WPARAM>(line), 0));
    const int column = anchorPos - lineStartChar;

    int targetX = column * cellWidth - horzScroll;
    std::string lineText;
    if (fetchRichEditLineText(m_hwndEditor, line, &lineText))
    {
        const int tabWidthPx = queryLineStripTabWidth(m_hwndEditor, cellWidth);
        const int tabCols = lineStripTabColumnCount(tabWidthPx, cellWidth);
        const int visualColumn =
            charOffsetToVisualColumn(lineText.data(), static_cast<int>(lineText.size()), column, tabCols);
        targetX = visualColumn * cellWidth - horzScroll;
    }

    const int targetY = (line - firstVisibleLine) * lineHeight + viewportStartY;

    rawrxd::ui::blitMonochromeTextSoftware(surface, &m_lineStripRaster, m_ghostTextContent.c_str(),
                                           static_cast<std::uint32_t>(m_ghostTextContent.size()), targetX, targetY,
                                           kGhostTextColor);
}

void Win32IDE::paintLineStripDiagnosticSquiggles(rawrxd::ui::SoftwareRenderSurface* surface, int firstVisibleLine,
                                                 int lineHeight, int cellWidth, int horzScroll, int viewportStartY,
                                                 int dirtyTopLine, int dirtyBottomLine)
{
    if (!surface || !m_hwndEditor)
    {
        return;
    }

    std::string activePath;
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_editorTabs.size()))
    {
        activePath = m_editorTabs[static_cast<std::size_t>(m_activeTabIndex)].filePath;
    }
    if (activePath.empty())
    {
        return;
    }

    std::vector<LSPDiagnostic> diagnostics;
    {
        std::lock_guard<std::mutex> lock(m_lspDiagnosticsMutex);
        for (const auto& entry : m_lspDiagnostics)
        {
            if (entry.first.find(activePath) != std::string::npos || activePath.find(entry.first) != std::string::npos)
            {
                diagnostics = entry.second;
                break;
            }
        }
    }

    if (diagnostics.empty())
    {
        return;
    }

    const int tabWidthPx = queryLineStripTabWidth(m_hwndEditor, cellWidth);
    const int tabCols = lineStripTabColumnCount(tabWidthPx, cellWidth);

    for (const LSPDiagnostic& diag : diagnostics)
    {
        const int startLine = diag.range.start.line;
        const int endLine = diag.range.end.line;
        if (endLine < dirtyTopLine || startLine > dirtyBottomLine)
        {
            continue;
        }

        const std::uint32_t color = diag.severity <= 1 ? rawrxd::ui::packColorRef(RGB(244, 71, 71))
                                                       : rawrxd::ui::packColorRef(RGB(220, 180, 60));

        const int line = startLine;
        const int colStart = diag.range.start.character;
        const int colEnd = (startLine == endLine) ? diag.range.end.character : colStart + 1;

        int visualColStart = colStart;
        int visualColEnd = colEnd;
        std::string lineText;
        if (fetchRichEditLineText(m_hwndEditor, line, &lineText))
        {
            visualColStart =
                charOffsetToVisualColumn(lineText.data(), static_cast<int>(lineText.size()), colStart, tabCols);
            visualColEnd =
                charOffsetToVisualColumn(lineText.data(), static_cast<int>(lineText.size()), colEnd, tabCols);
        }

        const int x0 = visualColStart * cellWidth - horzScroll;
        const int x1 = visualColEnd * cellWidth - horzScroll;
        const int y = (line - firstVisibleLine) * lineHeight + viewportStartY + lineHeight - 3;
        paintDiagnosticWave(surface, x0, x1, y, color);
    }
}

void Win32IDE::invalidateLineStripOverlay(const RECT* optionalLineRect)
{
    if (!m_hwndLineStripOverlay || !IsWindow(m_hwndLineStripOverlay))
    {
        return;
    }

    if (optionalLineRect)
    {
        InvalidateRect(m_hwndLineStripOverlay, optionalLineRect, FALSE);
    }
    else
    {
        InvalidateRect(m_hwndLineStripOverlay, nullptr, FALSE);
    }
}

void Win32IDE::onEditorLineStripContentChanged()
{
    if (!lineStripEditorEnabled())
    {
        return;
    }

    if (!m_hwndLineStripOverlay && m_hwndMain)
    {
        createLineStripOverlay(m_hwndMain);
        layoutLineStripOverlay();
    }

    m_lineStripDocumentDirty = true;
    if (m_hwndMain)
    {
        SetTimer(m_hwndMain, LINE_STRIP_SYNC_TIMER_ID, LINE_STRIP_SYNC_DELAY_MS, nullptr);
    }

    if (!m_hwndLineStripOverlay || !IsWindow(m_hwndLineStripOverlay) || !m_hwndEditor)
    {
        return;
    }

    const int lineHeight = getEditorLineHeightPx();
    if (lineHeight <= 0)
    {
        invalidateLineStripOverlay(nullptr);
        return;
    }

    int cellWidth = 8;
    if (m_lineStripEditorReady && m_lineStripStream.cellWidth > 0)
    {
        cellWidth = static_cast<int>(m_lineStripStream.cellWidth);
    }
    else
    {
        HDC hdc = GetDC(m_hwndEditor);
        if (hdc)
        {
            HFONT font = m_editorFont ? m_editorFont : static_cast<HFONT>(GetStockObject(ANSI_FIXED_FONT));
            HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
            TEXTMETRICA tm{};
            GetTextMetricsA(hdc, &tm);
            SelectObject(hdc, old);
            ReleaseDC(m_hwndEditor, hdc);
            if (tm.tmAveCharWidth > 0)
            {
                cellWidth = tm.tmAveCharWidth;
            }
        }
    }

    const int firstVisibleLine = static_cast<int>(SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));

    CHARRANGE sel{};
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&sel));

    const int lineFrom = static_cast<int>(SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0));
    const int lineTo = static_cast<int>(SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMax, 0));
    const int lineStart = std::min(lineFrom, lineTo);
    const int lineEnd = std::max(lineFrom, lineTo);

    RECT overlayClient{};
    GetClientRect(m_hwndLineStripOverlay, &overlayClient);
    const int overlayW = overlayClient.right - overlayClient.left;

    for (int line = lineStart; line <= lineEnd; ++line)
    {
        const int relativeLine = line - firstVisibleLine;
        if (relativeLine < 0)
        {
            continue;
        }

        const int lineStartChar =
            static_cast<int>(SendMessage(m_hwndEditor, EM_LINEINDEX, static_cast<WPARAM>(line), 0));
        const int lineLen =
            static_cast<int>(SendMessage(m_hwndEditor, EM_LINELENGTH, static_cast<WPARAM>(lineStartChar), 0));

        int charStart = 0;
        int charEnd = std::max(0, lineLen);
        if (line == lineFrom)
        {
            charStart = std::max(0, static_cast<int>(sel.cpMin) - lineStartChar);
        }
        if (line == lineTo)
        {
            charEnd = std::max(charStart + 1, static_cast<int>(sel.cpMax) - lineStartChar);
        }

        charEnd = std::min(charEnd + 2, std::max(charEnd, lineLen + 2));

        RECT rowRect{};
        rowRect.top = relativeLine * lineHeight;
        rowRect.bottom = rowRect.top + lineHeight;
        rowRect.left = charStart * cellWidth;
        rowRect.right = charEnd * cellWidth;
        if (rowRect.right > overlayW)
        {
            rowRect.right = overlayW;
        }
        if (rowRect.left < 0)
        {
            rowRect.left = 0;
        }
        if (rowRect.right <= rowRect.left)
        {
            rowRect.right = rowRect.left + cellWidth * 2;
        }

        InvalidateRect(m_hwndLineStripOverlay, &rowRect, FALSE);
    }
}

void Win32IDE::paintLineStripOverlay(HDC hdcScreen, const RECT& paintRect)
{
    if (!hdcScreen || !m_lineStripEditorReady || !m_hwndEditor)
    {
        return;
    }

    const bool documentDirty = m_lineStripDocumentDirty;
    if (documentDirty)
    {
        syncLineStripDocumentFromEditor();
        syncLineStripDirtyStrips();
    }

    const int firstVisibleLine = static_cast<int>(SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0));
    const int lineHeight = getEditorLineHeightPx();
    if (lineHeight <= 0)
    {
        return;
    }

    int horzScroll = 0;
    SCROLLINFO horzInfo{};
    horzInfo.cbSize = sizeof(horzInfo);
    horzInfo.fMask = SIF_POS;
    if (GetScrollInfo(m_hwndEditor, SB_HORZ, &horzInfo))
    {
        horzScroll = horzInfo.nPos;
    }

    int verticalScrollPx = firstVisibleLine * lineHeight;
    POINT scrollPos{};
    if (SendMessage(m_hwndEditor, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scrollPos)))
    {
        verticalScrollPx = scrollPos.y;
    }

    const int pixelScrollRemainder = verticalScrollPx - firstVisibleLine * lineHeight;

    int dirtyTopLine = paintRect.top / lineHeight + firstVisibleLine;
    int dirtyBottomLine = (paintRect.bottom + lineHeight - 1) / lineHeight + firstVisibleLine;

    const int lineCount = static_cast<int>(m_lineStripDocLines.size());
    if (dirtyTopLine < 0)
    {
        dirtyTopLine = 0;
    }
    if (dirtyBottomLine >= lineCount)
    {
        dirtyBottomLine = lineCount - 1;
    }
    if (dirtyTopLine > dirtyBottomLine || lineCount <= 0)
    {
        return;
    }

    std::uint32_t visibleLines[256];
    std::uint32_t visibleCount = 0;
    for (int line = dirtyTopLine; line <= dirtyBottomLine; ++line)
    {
        if (visibleCount >= 256)
        {
            break;
        }
        visibleLines[visibleCount++] = static_cast<std::uint32_t>(line);
    }

    if (visibleCount == 0)
    {
        return;
    }

    rawrxd::ui::clearSoftwareSurfaceRect(&m_lineStripSurface, kEditorBackground, paintRect);

    const rawrxd::ui::LineStripCacheWorkspace stripWorkspace = m_lineStripController.workspaceProxy();
    const int viewportStartY = (dirtyTopLine - firstVisibleLine) * lineHeight - pixelScrollRemainder;

    paintSelectionParityLayer(m_hwndEditor, &m_lineStripSurface, firstVisibleLine, lineHeight,
                              static_cast<int>(m_lineStripStream.cellWidth), horzScroll, viewportStartY, dirtyTopLine,
                              dirtyBottomLine);

    rawrxd::ui::renderViewportLineStrips(&m_lineStripSurface, &stripWorkspace, visibleLines, visibleCount, horzScroll,
                                         viewportStartY, true);

    paintLineStripGhostTextParity(&m_lineStripSurface, firstVisibleLine, lineHeight,
                                  static_cast<int>(m_lineStripStream.cellWidth), horzScroll, viewportStartY);

    paintLineStripDiagnosticSquiggles(&m_lineStripSurface, firstVisibleLine, lineHeight,
                                      static_cast<int>(m_lineStripStream.cellWidth), horzScroll, viewportStartY,
                                      dirtyTopLine, dirtyBottomLine);

    paintCaretParityLayer(m_hwndEditor, &m_lineStripSurface, firstVisibleLine, lineHeight,
                          static_cast<int>(m_lineStripStream.cellWidth), horzScroll, viewportStartY,
                          g_lineStripCaretBlinkOn);

    if (g_lineStripNativeCaretHidden && m_hwndEditor)
    {
        HideCaret(m_hwndEditor);
    }

    HDC presentDc = m_lineStripBakeDc ? m_lineStripBakeDc : hdcScreen;
    if (m_lineStripBakeDc)
    {
        HDC refDc = GetDC(m_hwndLineStripOverlay);
        if (refDc)
        {
            ensureLineStripBackbuffer(refDc, m_lineStripBakeW > 0 ? m_lineStripBakeW : static_cast<int>(m_lineStripSurfaceW),
                                      m_lineStripBakeH > 0 ? m_lineStripBakeH : static_cast<int>(m_lineStripSurfaceH));
            ReleaseDC(m_hwndLineStripOverlay, refDc);
        }
    }

    rawrxd::ui::presentSoftwareSurfaceRect(presentDc, &m_lineStripSurface, paintRect);

    if (m_lineStripBakeDc && hdcScreen != m_lineStripBakeDc)
    {
        const int blitW = paintRect.right - paintRect.left;
        const int blitH = paintRect.bottom - paintRect.top;
        if (blitW > 0 && blitH > 0)
        {
            BitBlt(hdcScreen, paintRect.left, paintRect.top, blitW, blitH, m_lineStripBakeDc, paintRect.left,
                   paintRect.top, SRCCOPY);
        }
    }
}

LRESULT CALLBACK Win32IDE::LineStripOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetPropA(hwnd, "IDE_PTR"));

    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            if (ide)
            {
                ide->paintLineStripOverlay(hdc, ps.rcPaint);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            if (ide)
            {
                ide->layoutLineStripOverlay();
            }
            return 0;

        case WM_NCHITTEST:
            return HTTRANSPARENT;

        default:
            break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
