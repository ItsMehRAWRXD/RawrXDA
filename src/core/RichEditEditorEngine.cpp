// ============================================================================
// RichEditEditorEngine.cpp — IEditorEngine Adapter for Win32 RichEdit
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

#include <windows.h>
#include <richedit.h>
#include <string>
#include <cstring>
#include <vector>

// Forward declaration
struct IDETheme;

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
         | EditorCapability::WordWrap;
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
    m_hFont = CreateFontW(
        -MulDiv(m_fontSize, GetDeviceCaps(GetDC(m_hwndEdit), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        m_fontFamily.c_str()
    );
    if (m_hFont) {
        SendMessage(m_hwndEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }

    // Set tab stops (4 characters)
    PARAFORMAT2 pf = {};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_TABSTOPS;
    pf.cTabCount = 1;
    pf.rgxTabs[0] = 400;  // ~4 chars in twips
    SendMessage(m_hwndEdit, EM_SETPARAFORMAT, 0, (LPARAM)&pf);

    if (m_readyFn) {
        m_readyFn(m_readyData);
    }

    return EditorEngineResult::ok("RichEdit initialized");
}

EditorEngineResult RichEditEditorEngine::destroy() {
    if (m_hwndEdit) {
        DestroyWindow(m_hwndEdit);
        m_hwndEdit = nullptr;
    }
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
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

    // Set background color
    SendMessage(m_hwndEdit, EM_SETBKGNDCOLOR, 0, theme.backgroundColor);

    // Set default text color
    CHARFORMAT2 cf = {};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = theme.textColor;
    SendMessage(m_hwndEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    m_stats.themeChanges++;
    return EditorEngineResult::ok("Theme applied (basic colors only)");
}

EditorEngineResult RichEditEditorEngine::setFontSize(int sizeDip) {
    m_fontSize = sizeDip;
    if (m_hFont) DeleteObject(m_hFont);
    m_hFont = CreateFontW(
        -MulDiv(m_fontSize, GetDeviceCaps(GetDC(m_hwndEdit), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        m_fontFamily.c_str()
    );
    if (m_hFont && m_hwndEdit) {
        SendMessage(m_hwndEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }
    return EditorEngineResult::ok("Font size set");
}

EditorEngineResult RichEditEditorEngine::setFontFamily(const wchar_t* family) {
    if (family) m_fontFamily = family;
    return setFontSize(m_fontSize);  // Recreate font
}

EditorEngineResult RichEditEditorEngine::setLineNumbers(bool) {
    return EditorEngineResult::error("RichEdit does not support line numbers natively");
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
}

// ============================================================================
// Ghost Text — Not supported by RichEdit
// ============================================================================
EditorEngineResult RichEditEditorEngine::setGhostText(int, int, const char*) {
    return EditorEngineResult::error("Ghost text not supported by RichEdit fallback");
}

EditorEngineResult RichEditEditorEngine::clearGhostText() {
    return EditorEngineResult::ok("No ghost text to clear");
}

// ============================================================================
// Input — RichEdit handles input internally
// ============================================================================
bool RichEditEditorEngine::onKeyDown(WPARAM, LPARAM) { return false; }
bool RichEditEditorEngine::onChar(WCHAR) { return false; }
bool RichEditEditorEngine::onMouseWheel(int, int, int) { return false; }
bool RichEditEditorEngine::onLButtonDown(int, int, WPARAM) { return false; }
bool RichEditEditorEngine::onLButtonUp(int, int) { return false; }
bool RichEditEditorEngine::onMouseMove(int, int, WPARAM) { return false; }
bool RichEditEditorEngine::onIMEComposition(HWND, WPARAM, LPARAM) { return false; }

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
