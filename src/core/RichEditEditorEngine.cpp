// ============================================================================
// RichEditEditorEngine.cpp — IEditorEngine Adapter for Win32 RichEdit
// VSU Effects: Uses Adobe RGBa color space for professional color accuracy
// ============================================================================
//
// Phase 28: Editor Engine Abstraction — RichEdit Emergency Fallback
//
// This file wraps the Win32 RichEdit control behind the IEditorEngine
// interface. It is the simplest and most reliable engine, requiring only
// riched20.dll (ships with every Windows version since 2000).
//
// Role: Emergency fallback when both MonacoCore and WebView2 fail.
//
// RichEdit capabilities:
//   ✅ Basic text editing, undo/redo, clipboard, IME
//   ✅ Selection, find/replace
//   ⚠️ Limited syntax highlighting (via EM_SETCHARFORMAT per-token)
//   ❌ No minimap, no code folding, no multi-cursor
//   ❌ No ghost text, no deterministic replay
//
// Pattern:  PatchResult-compatible, no exceptions
// Threading: All calls on UI thread
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "editor_engine.h"
#include "../include/RawrXD_ColorSpace.h"

#include <windows.h>
#include <commctrl.h> // For Subclass
#include <richedit.h>
#include <string>
#include <cstring>
#include <vector>

#pragma comment(lib, "comctl32.lib")

using namespace RawrXD::ColorSpace;

// Helper to convert AdobeRGBa to COLORREF for Win32 GDI
inline COLORREF AdobeRGBaToCOLORREF(const AdobeRGBa& color) {
    auto srgb = color.TosRGB();
    return RGB(static_cast<int>(srgb.r * 255),
               static_cast<int>(srgb.g * 255),
               static_cast<int>(srgb.b * 255));
}

// Convert COLORREF to AdobeRGBa
inline AdobeRGBa COLORREFToAdobeRGBa(COLORREF color) {
    return AdobeRGBa(
        GetRValue(color) / 255.0f,
        GetGValue(color) / 255.0f,
        GetBValue(color) / 255.0f,
        1.0f
    );
}

// Forward declaration
struct IDETheme;

// Syntax highlighting colors for common languages
struct SyntaxColors {
    COLORREF keyword;
    COLORREF comment;
    COLORREF string;
    COLORREF number;
    COLORREF identifier;
};

static std::map<std::string, SyntaxColors> g_syntaxThemes = {
    {"cpp", {
        RGB(86, 156, 214),    // Blue keywords
        RGB(87, 166, 74),     // Green comments
        RGB(206, 145, 120),   // Tan strings
        RGB(181, 206, 168),   // Light green numbers
        RGB(220, 220, 220)    // White identifiers
    }},
    {"python", {
        RGB(86, 156, 214),    // Blue keywords
        RGB(87, 166, 74),     // Green comments
        RGB(206, 145, 120),   // Tan strings
        RGB(181, 206, 168),   // Light green numbers
        RGB(220, 220, 220)    // White identifiers
    }},
    {"javascript", {
        RGB(86, 156, 214),    // Blue keywords
        RGB(87, 166, 74),     // Green comments
        RGB(206, 145, 120),   // Tan strings
        RGB(181, 206, 168),   // Light green numbers
        RGB(220, 220, 220)    // White identifiers
    }},
    {"plaintext", {
        RGB(220, 220, 220),   // White text
        RGB(220, 220, 220),   // White text
        RGB(220, 220, 220),   // White text
        RGB(220, 220, 220),   // White text
        RGB(220, 220, 220)    // White text
    }}
};

// ============================================================================
// RichEditEditorEngine Implementation
// ============================================================================
class RichEditEditorEngine : public IEditorEngine {
public:
    RichEditEditorEngine();
    ~RichEditEditorEngine() override;

    // ---- Identity ----
    EditorEngineType    getType() const override { return EditorEngineType::RichEdit; }
    const char*         getName() const override { return "RichEdit (Fallback)"; }
    const char*         getVersion() const override { return "1.0.0"; }
    EditorCapability    getCapabilities() const override;

    // ---- Lifecycle ----
    EditorEngineResult  initialize(HWND parentWindow) override;
    EditorEngineResult  destroy() override;
    bool                isReady() const override { return m_hwndEdit != nullptr; }

    // ---- Geometry ----
    void                resize(int x, int y, int width, int height) override;
    void                show() override;
    void                hide() override;
    bool                isVisible() const override { return m_visible; }

    // ---- Content ----
    EditorEngineResult  setText(const char* utf8Text, uint32_t length) override;
    EditorEngineResult  getText(char* buffer, uint32_t maxLen, uint32_t* outLen) override;
    EditorEngineResult  insertText(int line, int col, const char* text) override;
    EditorEngineResult  deleteRange(int startLine, int startCol,
                                    int endLine, int endCol) override;
    uint32_t            getLineCount() const override;

    // ---- Language ----
    EditorEngineResult  setLanguage(const char* languageId) override;

    // ---- Theme ----
    EditorEngineResult  applyTheme(const IDETheme& theme) override;

    // ---- Options ----
    EditorEngineResult  setFontSize(int sizeDip) override;
    EditorEngineResult  setFontFamily(const wchar_t* family) override;
    EditorEngineResult  setLineNumbers(bool enabled) override;
    EditorEngineResult  setWordWrap(bool enabled) override;
    EditorEngineResult  setMinimap(bool enabled) override;
    EditorEngineResult  setReadOnly(bool readOnly) override;

    // ---- Cursor & Selection ----
    EditorCursorPos     getCursorPosition() const override;
    EditorEngineResult  setCursorPosition(int line, int col) override;
    EditorSelectionRange getSelection() const override;
    EditorEngineResult  setSelection(int anchorLine, int anchorCol,
                                      int activeLine, int activeCol) override;

    // ---- Scrolling ----
    EditorEngineResult  revealLine(int lineNumber) override;
    int                 getFirstVisibleLine() const override;

    // ---- Focus ----
    EditorEngineResult  focus() override;
    bool                hasFocus() const override;

    // ---- Rendering ----
    void                render() override;  // No-op (RichEdit renders itself)

    // ---- Ghost Text ----
    EditorEngineResult  setGhostText(int line, int col, const char* text) override;
    EditorEngineResult  clearGhostText() override;

    // ---- Input ----
    bool                onKeyDown(WPARAM wParam, LPARAM lParam) override;
    bool                onChar(WCHAR ch) override;
    bool                onMouseWheel(int delta, int x, int y) override;
    bool                onLButtonDown(int x, int y, WPARAM modifiers) override;
    bool                onLButtonUp(int x, int y) override;
    bool                onMouseMove(int x, int y, WPARAM modifiers) override;
    bool                onIMEComposition(HWND hwnd, WPARAM wParam, LPARAM lParam) override;

    // ---- Callbacks ----
    void setContentChangedCallback(EditorContentChangedCallback fn, void* userData) override;
    void setCursorChangedCallback(EditorCursorChangedCallback fn, void* userData) override;
    void setReadyCallback(EditorReadyCallback fn, void* userData) override;
    void setErrorCallback(EditorErrorCallback fn, void* userData) override;

    // ---- Statistics ----
    EditorEngineStats   getStats() const override;

    // ---- HWND ----
    HWND                getWindowHandle() const override { return m_hwndEdit; }

private:
    // Convert line/col to character index
    LONG lineColToCharIndex(int line, int col) const;

    HWND        m_hwndEdit = nullptr;
    HWND        m_parentWindow = nullptr;
    HMODULE     m_hRichEdit = nullptr;
    bool        m_visible = false;
    int         m_fontSize = 14;
    std::wstring m_fontFamily = L"Consolas";
    std::string m_language = "plaintext";
    HFONT       m_hFont = nullptr;

    // Callbacks (stored but RichEdit doesn't natively fire these — 
    // would need EN_CHANGE / EN_SELCHANGE subclassing)
    EditorContentChangedCallback m_contentChangedFn = nullptr;
    void*                       m_contentChangedData = nullptr;
    EditorCursorChangedCallback m_cursorChangedFn = nullptr;
    void*                       m_cursorChangedData = nullptr;
    EditorReadyCallback         m_readyFn = nullptr;
    void*                       m_readyData = nullptr;
    EditorErrorCallback         m_errorFn = nullptr;
    void*                       m_errorData = nullptr;

    mutable EditorEngineStats   m_stats{};

    // Ghost Text state
    std::wstring m_ghostText;
    int          m_ghostLine = -1;
    int          m_ghostCol = -1;
    bool         m_isPaintingGhost = false;

    // Line numbers
    bool         m_showLineNumbers = false;
    HFONT        m_hLineNumberFont = nullptr;
    int          m_lineNumberWidth = 0;

    // Colors
    COLORREF     m_textColor = RGB(220, 220, 220);
    COLORREF     m_backgroundColor = RGB(30, 30, 30);
    COLORREF     m_ghostTextColor = RGB(128, 128, 128);

    // Static subclass for ghost text rendering
    static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

    // Internal helpers
    void updateFont();
    void createLineNumberMargin();
    void applySyntaxHighlighting();
    void paintLineNumbers();
    void paintGhostText();
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
RichEditEditorEngine::RichEditEditorEngine() {
    memset(&m_stats, 0, sizeof(m_stats));
}

RichEditEditorEngine::~RichEditEditorEngine() {
    destroy();
}

// ============================================================================
// Capabilities
// ============================================================================
EditorCapability RichEditEditorEngine::getCapabilities() const {
    return EditorCapability::UndoRedo
         | EditorCapability::Find
         | EditorCapability::SelectionRendering
         | EditorCapability::ReadOnlyMode
         | EditorCapability::IMESupport
         | EditorCapability::ScrollBar
         | EditorCapability::GhostText
         | EditorCapability::WordWrap
         | EditorCapability::SyntaxHighlighting
         | EditorCapability::LineNumbers;
}

// ============================================================================
// Lifecycle
// ============================================================================
EditorEngineResult RichEditEditorEngine::initialize(HWND parentWindow) {
    m_parentWindow = parentWindow;

    // Load RichEdit library
    m_hRichEdit = LoadLibraryW(L"Msftedit.dll");
    if (!m_hRichEdit) {
        m_hRichEdit = LoadLibraryW(L"Riched20.dll");
    }
    if (!m_hRichEdit) {
        return EditorEngineResult::error("Failed to load RichEdit DLL", GetLastError());
    }

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parentWindow, GWLP_HINSTANCE);

    m_hwndEdit = CreateWindowExW(
        0,
        MSFTEDIT_CLASS,
        L"",
        WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL
            | ES_AUTOHSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
        0, 0, 800, 600,
        parentWindow,
        nullptr,
        hInst,
        nullptr
    );

    if (!m_hwndEdit) {
        return EditorEngineResult::error("Failed to create RichEdit control", GetLastError());
    }

    // Set event mask for change notifications
    SendMessage(m_hwndEdit, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);

    // Set default font
    updateFont();

    // Set subclass for ghost text support
    SetWindowSubclass(m_hwndEdit, SubclassProc, 0, (DWORD_PTR)this);

    // Set tab stops (4 characters)
    PARAFORMAT2 pf = {};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_TABSTOPS;
    pf.cTabCount = 1;
    pf.rgxTabs[0] = 400;  // ~4 chars in twips
    SendMessage(m_hwndEdit, EM_SETPARAFORMAT, 0, (LPARAM)&pf);

    // Set background color
    SendMessage(m_hwndEdit, EM_SETBKGNDCOLOR, 0, m_backgroundColor);

    // Set default text color
    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = m_textColor;
    SendMessage(m_hwndEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Create line number margin
    createLineNumberMargin();

    // Subclass and tab stops moved to updateFont section above

    if (m_readyFn) {
        m_readyFn(m_readyData);
    }

    return EditorEngineResult::ok("RichEdit initialized");
}

EditorEngineResult RichEditEditorEngine::destroy() {
    if (m_hwndEdit) {
        RemoveWindowSubclass(m_hwndEdit, SubclassProc, 0);
        DestroyWindow(m_hwndEdit);
        m_hwndEdit = nullptr;
    }
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    if (m_hLineNumberFont) {
        DeleteObject(m_hLineNumberFont);
        m_hLineNumberFont = nullptr;
    }
    if (m_hRichEdit) {
        FreeLibrary(m_hRichEdit);
        m_hRichEdit = nullptr;
    }
    return EditorEngineResult::ok("RichEdit destroyed");
}

// ============================================================================
// Geometry
// ============================================================================
void RichEditEditorEngine::resize(int x, int y, int width, int height) {
    if (m_hwndEdit) {
        MoveWindow(m_hwndEdit, x, y, width, height, TRUE);
    }
}

void RichEditEditorEngine::show() {
    if (m_hwndEdit) ShowWindow(m_hwndEdit, SW_SHOW);
    m_visible = true;
}

void RichEditEditorEngine::hide() {
    if (m_hwndEdit) ShowWindow(m_hwndEdit, SW_HIDE);
    m_visible = false;
}

// ============================================================================
// Content
// ============================================================================
EditorEngineResult RichEditEditorEngine::setText(const char* utf8Text, uint32_t length) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");

    // Convert UTF-8 to wide
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8Text, length, nullptr, 0);
    std::vector<wchar_t> wide(wideLen + 1);
    MultiByteToWideChar(CP_UTF8, 0, utf8Text, length, wide.data(), wideLen);
    wide[wideLen] = L'\0';

    SetWindowTextW(m_hwndEdit, wide.data());
    m_stats.contentChanges++;

    // Apply syntax highlighting if needed
    if (m_language != "plaintext") {
        applySyntaxHighlighting();
    }

    return EditorEngineResult::ok("Content set");
}

EditorEngineResult RichEditEditorEngine::getText(char* buffer, uint32_t maxLen, uint32_t* outLen) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");

    int wideLen = GetWindowTextLengthW(m_hwndEdit);
    std::vector<wchar_t> wide(wideLen + 1);
    GetWindowTextW(m_hwndEdit, wide.data(), wideLen + 1);

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wideLen,
                                       buffer, maxLen - 1, nullptr, nullptr);
    buffer[utf8Len] = '\0';
    if (outLen) *outLen = utf8Len;

    return EditorEngineResult::ok("Content retrieved");
}

EditorEngineResult RichEditEditorEngine::insertText(int line, int col, const char* text) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");

    LONG charIdx = lineColToCharIndex(line, col);
    SendMessage(m_hwndEdit, EM_SETSEL, charIdx, charIdx);

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    std::vector<wchar_t> wide(wideLen);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide.data(), wideLen);

    SendMessageW(m_hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)wide.data());
    m_stats.contentChanges++;
    return EditorEngineResult::ok("Text inserted");
}

EditorEngineResult RichEditEditorEngine::deleteRange(int startLine, int startCol,
                                                      int endLine, int endCol) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");

    LONG start = lineColToCharIndex(startLine, startCol);
    LONG end = lineColToCharIndex(endLine, endCol);
    SendMessage(m_hwndEdit, EM_SETSEL, start, end);
    SendMessageW(m_hwndEdit, EM_REPLACESEL, TRUE, (LPARAM)L"");
    m_stats.contentChanges++;
    return EditorEngineResult::ok("Range deleted");
}

uint32_t RichEditEditorEngine::getLineCount() const {
    if (!m_hwndEdit) return 0;
    return (uint32_t)SendMessage(m_hwndEdit, EM_GETLINECOUNT, 0, 0);
}

// ============================================================================
// Language / Theme / Options
// ============================================================================
EditorEngineResult RichEditEditorEngine::setLanguage(const char* languageId) {
    if (languageId) m_language = languageId;
    return EditorEngineResult::ok("Language set (no highlighting in RichEdit fallback)");
}

EditorEngineResult RichEditEditorEngine::applyTheme(const IDETheme& theme) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");

    // Convert AdobeRGBa colors to COLORREF
    m_backgroundColor = AdobeRGBaToCOLORREF(theme.backgroundColor);
    m_textColor = AdobeRGBaToCOLORREF(theme.textColor);
    m_ghostTextColor = AdobeRGBaToCOLORREF(theme.ghostTextColor);

    // Set background color
    SendMessage(m_hwndEdit, EM_SETBKGNDCOLOR, 0, m_backgroundColor);

    // Set default text color
    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = m_textColor;
    SendMessage(m_hwndEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Update syntax highlighting if needed
    if (m_language != "plaintext") {
        applySyntaxHighlighting();
    }

    m_stats.themeChanges++;
    return EditorEngineResult::ok("Theme applied");
}

void RichEditEditorEngine::updateFont() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }

    HDC hdc = GetDC(m_hwndEdit);
    int fontSizePixels = -MulDiv(m_fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(m_hwndEdit, hdc);

    m_hFont = CreateFontW(
        fontSizePixels,
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        m_fontFamily.c_str()
    );

    if (m_hFont && m_hwndEdit) {
        SendMessage(m_hwndEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }

    // Update line number font
    if (m_hLineNumberFont) {
        DeleteObject(m_hLineNumberFont);
    }
    m_hLineNumberFont = CreateFontW(
        fontSizePixels,
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        L"Consolas"
    );
}

void RichEditEditorEngine::createLineNumberMargin() {
    if (!m_hwndEdit || !m_showLineNumbers) return;

    // Get text metrics to calculate margin width
    HDC hdc = GetDC(m_hwndEdit);
    HFONT hOldFont = (HFONT)SelectObject(hdc, m_hFont);

    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int charWidth = tm.tmAveCharWidth;
    int marginWidth = charWidth * 6; // Space for 5 digits + padding

    SelectObject(hdc, hOldFont);
    ReleaseDC(m_hwndEdit, hdc);

    // Set margin
    SendMessage(m_hwndEdit, EM_SETMARGINS, EC_LEFTMARGIN, marginWidth);
}

void RichEditEditorEngine::applySyntaxHighlighting() {
    if (m_language == "plaintext" || !m_hwndEdit) return;

    auto it = g_syntaxThemes.find(m_language);
    if (it == g_syntaxThemes.end()) return;

    const SyntaxColors& colors = it->second;

    // Get text length
    int textLength = GetWindowTextLengthW(m_hwndEdit);
    if (textLength <= 0) return;

    // Get text content
    std::vector<wchar_t> buffer(textLength + 1);
    GetWindowTextW(m_hwndEdit, buffer.data(), textLength + 1);
    std::wstring text = buffer.data();

    // Simple keyword-based highlighting
    std::vector<std::wstring> keywords = {
        L"if", L"else", L"for", L"while", L"return", L"function",
        L"class", L"struct", L"void", L"int", L"float", L"double",
        L"const", L"static", L"public", L"private", L"protected"
    };

    for (const auto& keyword : keywords) {
        size_t pos = 0;
        while ((pos = text.find(keyword, pos)) != std::wstring::npos) {
            // Check if it's a whole word
            if ((pos == 0 || !iswalnum(text[pos - 1])) &&
                (pos + keyword.length() >= text.length() || !iswalnum(text[pos + keyword.length()]))) {

                // Set keyword color
                CHARFORMAT2 cf = {};
                cf.cbSize = sizeof(cf);
                cf.dwMask = CFM_COLOR;
                cf.crTextColor = colors.keyword;

                SendMessage(m_hwndEdit, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + keyword.length()));
                SendMessage(m_hwndEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
            pos += keyword.length();
        }
    }
}

void RichEditEditorEngine::paintLineNumbers() {
    if (!m_hwndEdit || !m_showLineNumbers) return;

    HDC hdc = GetDC(m_hwndEdit);
    HFONT hOldFont = (HFONT)SelectObject(hdc, m_hLineNumberFont);

    // Get client rect and text metrics
    RECT clientRect;
    GetClientRect(m_hwndEdit, &clientRect);

    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    int lineHeight = tm.tmHeight;

    // Get scroll position
    int firstVisibleLine = getFirstVisibleLine();
    int visibleLines = clientRect.bottom / lineHeight + 2;

    // Set up colors
    SetTextColor(hdc, RGB(128, 128, 128)); // Gray line numbers
    SetBkColor(hdc, RGB(40, 40, 40));      // Dark background
    SetBkMode(hdc, OPAQUE);

    // Paint line numbers
    for (int i = 0; i < visibleLines; i++) {
        int lineNum = firstVisibleLine + i + 1;
        if (lineNum > (int)getLineCount()) break;

        std::wstring numText = std::to_wstring(lineNum);
        RECT numRect = {
            2,
            i * lineHeight,
            clientRect.right - 2,
            (i + 1) * lineHeight
        };

        DrawTextW(hdc, numText.c_str(), -1, &numRect,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, hOldFont);
    ReleaseDC(m_hwndEdit, hdc);
}

void RichEditEditorEngine::paintGhostText() {
    if (!m_hwndEdit || m_ghostText.empty()) return;

    HDC hdc = GetDC(m_hwndEdit);
    HFONT hOldFont = (HFONT)SelectObject(hdc, m_hFont);

    // Get cursor position
    POINT pt;
    GetCaretPos(&pt);

    // Set ghost text color
    SetTextColor(hdc, m_ghostTextColor);
    SetBkMode(hdc, TRANSPARENT);

    // Paint ghost text
    TextOutW(hdc, pt.x, pt.y, m_ghostText.c_str(), (int)m_ghostText.length());

    SelectObject(hdc, hOldFont);
    ReleaseDC(m_hwndEdit, hdc);
}

EditorEngineResult RichEditEditorEngine::setFontSize(int sizeDip) {
    m_fontSize = sizeDip;
    updateFont();
    return EditorEngineResult::ok("Font size set");
}

EditorEngineResult RichEditEditorEngine::setFontFamily(const wchar_t* family) {
    if (family) m_fontFamily = family;
    return setFontSize(m_fontSize);  // Recreate font
}

EditorEngineResult RichEditEditorEngine::setLineNumbers(bool enabled) {
    m_showLineNumbers = enabled;
    createLineNumberMargin();
    return EditorEngineResult::ok("Line numbers toggled");
}

EditorEngineResult RichEditEditorEngine::setWordWrap(bool enabled) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");
    SendMessage(m_hwndEdit, EM_SETTARGETDEVICE, 0, enabled ? 0 : 1);
    return EditorEngineResult::ok("Word wrap toggled");
}

EditorEngineResult RichEditEditorEngine::setMinimap(bool) {
    return EditorEngineResult::error("RichEdit does not support minimap");
}

EditorEngineResult RichEditEditorEngine::setReadOnly(bool readOnly) {
    if (m_hwndEdit) {
        SendMessage(m_hwndEdit, EM_SETREADONLY, readOnly ? TRUE : FALSE, 0);
    }
    return EditorEngineResult::ok("Read-only toggled");
}

// ============================================================================
// Cursor & Selection
// ============================================================================
EditorCursorPos RichEditEditorEngine::getCursorPosition() const {
    if (!m_hwndEdit) return {0, 0};

    CHARRANGE cr;
    SendMessage(m_hwndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);
    int line = (int)SendMessage(m_hwndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMax);
    int lineStart = (int)SendMessage(m_hwndEdit, EM_LINEINDEX, line, 0);
    int col = cr.cpMax - lineStart;
    return { line, col };
}

EditorEngineResult RichEditEditorEngine::setCursorPosition(int line, int col) {
    LONG idx = lineColToCharIndex(line, col);
    SendMessage(m_hwndEdit, EM_SETSEL, idx, idx);
    return EditorEngineResult::ok("Cursor set");
}

EditorSelectionRange RichEditEditorEngine::getSelection() const {
    if (!m_hwndEdit) return {{0,0},{0,0}};

    CHARRANGE cr;
    SendMessage(m_hwndEdit, EM_EXGETSEL, 0, (LPARAM)&cr);

    int startLine = (int)SendMessage(m_hwndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMin);
    int startLineStart = (int)SendMessage(m_hwndEdit, EM_LINEINDEX, startLine, 0);
    int endLine = (int)SendMessage(m_hwndEdit, EM_EXLINEFROMCHAR, 0, cr.cpMax);
    int endLineStart = (int)SendMessage(m_hwndEdit, EM_LINEINDEX, endLine, 0);

    return {
        { startLine, (int)(cr.cpMin - startLineStart) },
        { endLine,   (int)(cr.cpMax - endLineStart) }
    };
}

EditorEngineResult RichEditEditorEngine::setSelection(int anchorLine, int anchorCol,
                                                        int activeLine, int activeCol) {
    LONG start = lineColToCharIndex(anchorLine, anchorCol);
    LONG end = lineColToCharIndex(activeLine, activeCol);
    SendMessage(m_hwndEdit, EM_SETSEL, start, end);
    return EditorEngineResult::ok("Selection set");
}

// ============================================================================
// Scrolling
// ============================================================================
EditorEngineResult RichEditEditorEngine::revealLine(int lineNumber) {
    if (m_hwndEdit) {
        SendMessage(m_hwndEdit, EM_LINESCROLL, 0,
            lineNumber - (int)SendMessage(m_hwndEdit, EM_GETFIRSTVISIBLELINE, 0, 0));
    }
    return EditorEngineResult::ok("Line revealed");
}

int RichEditEditorEngine::getFirstVisibleLine() const {
    if (!m_hwndEdit) return 0;
    return (int)SendMessage(m_hwndEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
}

// ============================================================================
// Focus
// ============================================================================
EditorEngineResult RichEditEditorEngine::focus() {
    if (m_hwndEdit) SetFocus(m_hwndEdit);
    return EditorEngineResult::ok("Focused");
}

bool RichEditEditorEngine::hasFocus() const {
    return m_hwndEdit && GetFocus() == m_hwndEdit;
}

// ============================================================================
// Rendering — No-op (RichEdit renders itself)
// ============================================================================
void RichEditEditorEngine::render() {
    // RichEdit handles its own rendering via Win32 messages.
    // Force redraw to ensure content is visible
    if (m_hwndEdit) {
        InvalidateRect(m_hwndEdit, nullptr, FALSE);
        UpdateWindow(m_hwndEdit);
    }
}

// ============================================================================
// Ghost Text — Implemented via GDI Overlay Subclassing
// ============================================================================
LRESULT CALLBACK RichEditEditorEngine::SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto* self = reinterpret_cast<RichEditEditorEngine*>(dwRefData);

    switch (uMsg) {
        case WM_PAINT: {
            // First let RichEdit paint itself
            LRESULT result = DefSubclassProc(hWnd, uMsg, wParam, lParam);

            // Paint line numbers if enabled
            if (self && self->m_showLineNumbers) {
                self->paintLineNumbers();
            }

            // Paint ghost text if present
            if (self && !self->m_ghostText.empty()) {
                self->paintGhostText();
            }

            return result;
        }

        case WM_NCDESTROY:
            RemoveWindowSubclass(hWnd, SubclassProc, uIdSubclass);
            break;
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

EditorEngineResult RichEditEditorEngine::setGhostText(int line, int col, const char* text) {
    if (!m_hwndEdit) return EditorEngineResult::error("Not initialized");

    // Convert to wide for rendering
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    m_ghostText.resize(wideLen > 0 ? wideLen - 1 : 0);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, &m_ghostText[0], (int)m_ghostText.size() + 1);

    m_ghostLine = line;
    m_ghostCol = col;

    // Move cursor to ghost text position
    setCursorPosition(line, col);

    InvalidateRect(m_hwndEdit, NULL, FALSE);
    return EditorEngineResult::ok("Ghost text set");
}

EditorEngineResult RichEditEditorEngine::clearGhostText() {
    m_ghostText.clear();
    m_ghostLine = -1;
    m_ghostCol = -1;
    if (m_hwndEdit) InvalidateRect(m_hwndEdit, NULL, FALSE);
    return EditorEngineResult::ok("Ghost text cleared");
}

// ============================================================================
// Input — RichEdit handles input internally
// ============================================================================
bool RichEditEditorEngine::onKeyDown(WPARAM wParam, LPARAM lParam) {
    if (!m_hwndEdit) {
        return false;
    }
    m_stats.keyEventsProcessed++;
    SendMessageW(m_hwndEdit, WM_KEYDOWN, wParam, lParam);
    if (m_cursorChangedFn) {
        const auto pos = getCursorPosition();
        m_cursorChangedFn(pos.line, pos.column, m_cursorChangedData);
    }
    return true;
}
bool RichEditEditorEngine::onChar(WCHAR ch) {
    if (!m_hwndEdit) {
        return false;
    }
    m_stats.keyEventsProcessed++;
    SendMessageW(m_hwndEdit, WM_CHAR, static_cast<WPARAM>(ch), 1);
    return true;
}
bool RichEditEditorEngine::onMouseWheel(int delta, int x, int y) {
    if (!m_hwndEdit) {
        return false;
    }
    const WPARAM wParam = MAKEWPARAM(0, static_cast<UINT>(static_cast<SHORT>(delta)));
    const LPARAM lParam = MAKELPARAM(x, y);
    SendMessageW(m_hwndEdit, WM_MOUSEWHEEL, wParam, lParam);
    return true;
}
bool RichEditEditorEngine::onLButtonDown(int x, int y, WPARAM modifiers) {
    if (!m_hwndEdit) {
        return false;
    }
    SetFocus(m_hwndEdit);
    SendMessageW(m_hwndEdit, WM_LBUTTONDOWN, modifiers, MAKELPARAM(x, y));
    return true;
}
bool RichEditEditorEngine::onLButtonUp(int x, int y) {
    if (!m_hwndEdit) {
        return false;
    }
    SendMessageW(m_hwndEdit, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
    return true;
}
bool RichEditEditorEngine::onMouseMove(int x, int y, WPARAM modifiers) {
    if (!m_hwndEdit) {
        return false;
    }
    SendMessageW(m_hwndEdit, WM_MOUSEMOVE, modifiers, MAKELPARAM(x, y));
    return true;
}
bool RichEditEditorEngine::onIMEComposition(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    HWND target = hwnd ? hwnd : m_hwndEdit;
    if (!target) {
        return false;
    }
    SendMessageW(target, WM_IME_COMPOSITION, wParam, lParam);
    return true;
}

// ============================================================================
// Callbacks
// ============================================================================
void RichEditEditorEngine::setContentChangedCallback(EditorContentChangedCallback fn, void* userData) {
    m_contentChangedFn = fn;
    m_contentChangedData = userData;
}

void RichEditEditorEngine::setCursorChangedCallback(EditorCursorChangedCallback fn, void* userData) {
    m_cursorChangedFn = fn;
    m_cursorChangedData = userData;
}

void RichEditEditorEngine::setReadyCallback(EditorReadyCallback fn, void* userData) {
    m_readyFn = fn;
    m_readyData = userData;
}

void RichEditEditorEngine::setErrorCallback(EditorErrorCallback fn, void* userData) {
    m_errorFn = fn;
    m_errorData = userData;
}

// ============================================================================
// Statistics
// ============================================================================
EditorEngineStats RichEditEditorEngine::getStats() const {
    m_stats.lineCount = getLineCount();
    auto pos = getCursorPosition();
    m_stats.cursorLine = pos.line;
    m_stats.cursorCol = pos.column;
    return m_stats;
}

// ============================================================================
// Helpers
// ============================================================================
LONG RichEditEditorEngine::lineColToCharIndex(int line, int col) const {
    if (!m_hwndEdit) return 0;
    LONG lineStart = (LONG)SendMessage(m_hwndEdit, EM_LINEINDEX, line, 0);
    if (lineStart < 0) lineStart = 0;
    return lineStart + col;
}

// ============================================================================
// Factory Registration
// ============================================================================
IEditorEngine* createRichEditEditorEngine() {
    return new RichEditEditorEngine();
}
