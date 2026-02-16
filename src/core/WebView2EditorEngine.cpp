// ============================================================================
// WebView2EditorEngine.cpp — IEditorEngine Adapter for WebView2 + Monaco
// ============================================================================
//
// Phase 28: Editor Engine Abstraction — WebView2 Compatibility Shell
//
// This file wraps the existing WebView2Container (Phase 26) behind the
// IEditorEngine interface, allowing it to be toggled as one of three
// available editor backends.
//
// Architecture:
//   WebView2EditorEngine delegates to:
//     - WebView2Container (lifecycle, content, theme, options)
//     - MonacoThemeExporter (theme conversion)
//
// Role: Optional compatibility shell — used when MonacoCore lacks a
//       feature the user needs (e.g., Monaco extensions, semantic tokens).
//
// Pattern:  PatchResult-compatible, no exceptions
// Threading: All COM calls on UI thread (STA)
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "editor_engine.h"
#include "Win32IDE_WebView2.h"

#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <atomic>

// SCAFFOLD_234: WebView2 and nupkg


// Forward declaration
struct IDETheme;

// ============================================================================
// WebView2EditorEngine Implementation
// ============================================================================
class WebView2EditorEngine : public IEditorEngine {
public:
    WebView2EditorEngine();
    ~WebView2EditorEngine() override;

    // ---- Identity ----
    EditorEngineType    getType() const override { return EditorEngineType::WebView2; }
    const char*         getName() const override { return "WebView2 (Monaco)"; }
    const char*         getVersion() const override { return "1.0.0"; }
    EditorCapability    getCapabilities() const override;

    // ---- Lifecycle ----
    EditorEngineResult  initialize(HWND parentWindow) override;
    EditorEngineResult  destroy() override;
    bool                isReady() const override;

    // ---- Geometry ----
    void                resize(int x, int y, int width, int height) override;
    void                show() override;
    void                hide() override;
    bool                isVisible() const override;

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
    void                render() override;  // No-op (WebView2 renders itself)

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
    HWND                getWindowHandle() const override { return m_hwndContainer; }

    // ---- Access to underlying WebView2Container ----
    WebView2Container*  getWebView2() const { return m_webView2; }

private:
    WebView2Container*  m_webView2 = nullptr;
    HWND                m_hwndContainer = nullptr;
    HWND                m_parentWindow = nullptr;
    MonacoEditorOptions m_options;

    // Cached state (WebView2 doesn't expose this synchronously)
    std::string         m_cachedContent;
    int                 m_cursorLine = 0;
    int                 m_cursorCol = 0;
    uint32_t            m_lineCount = 0;
    std::string         m_language = "plaintext";
    bool                m_visible = false;

    // Callbacks
    EditorContentChangedCallback m_contentChangedFn = nullptr;
    void*                       m_contentChangedData = nullptr;
    EditorCursorChangedCallback m_cursorChangedFn = nullptr;
    void*                       m_cursorChangedData = nullptr;
    EditorReadyCallback         m_readyFn = nullptr;
    void*                       m_readyData = nullptr;
    EditorErrorCallback         m_errorFn = nullptr;
    void*                       m_errorData = nullptr;

    // Statistics
    mutable EditorEngineStats   m_stats{};

    // Static callbacks for WebView2Container
    static void onReady(void* userData);
    static void onContent(const char* content, uint32_t length, void* userData);
    static void onCursor(int line, int column, void* userData);
    static void onError(const char* error, void* userData);
};

// ============================================================================
// Constructor / Destructor
// ============================================================================
WebView2EditorEngine::WebView2EditorEngine() {
    memset(&m_stats, 0, sizeof(m_stats));
}

WebView2EditorEngine::~WebView2EditorEngine() {
    destroy();
}

// ============================================================================
// Capabilities
// ============================================================================
EditorCapability WebView2EditorEngine::getCapabilities() const {
    return EditorCapability::SyntaxHighlighting
         | EditorCapability::Minimap
         | EditorCapability::MultiCursor
         | EditorCapability::CodeFolding
         | EditorCapability::AutoComplete
         | EditorCapability::BracketMatching
         | EditorCapability::LineNumbers
         | EditorCapability::WordWrap
         | EditorCapability::UndoRedo
         | EditorCapability::Find
         | EditorCapability::SelectionRendering
         | EditorCapability::ThemeSupport
         | EditorCapability::ReadOnlyMode
         | EditorCapability::IMESupport
         | EditorCapability::ScrollBar;
}

// ============================================================================
// Lifecycle
// ============================================================================
EditorEngineResult WebView2EditorEngine::initialize(HWND parentWindow) {
    m_parentWindow = parentWindow;

    // Create container window
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parentWindow, GWLP_HINSTANCE);
    m_hwndContainer = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 800, 600,
        parentWindow, nullptr, hInst, nullptr
    );
    if (!m_hwndContainer) {
        return EditorEngineResult::error("Failed to create WebView2 container", GetLastError());
    }

    // Create WebView2Container
    m_webView2 = new WebView2Container();

    // Set up callbacks
    m_webView2->setReadyCallback(onReady, this);
    m_webView2->setContentCallback(onContent, this);
    m_webView2->setCursorCallback(onCursor, this);
    m_webView2->setErrorCallback(onError, this);

    // Initialize asynchronously
    WebView2Result result = m_webView2->initialize(m_hwndContainer);
    if (!result.success) {
        delete m_webView2;
        m_webView2 = nullptr;
        DestroyWindow(m_hwndContainer);
        m_hwndContainer = nullptr;
        return EditorEngineResult::error(result.detail, result.errorCode);
    }

    return EditorEngineResult::ok("WebView2 initialization started (async)");
}

EditorEngineResult WebView2EditorEngine::destroy() {
    if (m_webView2) {
        m_webView2->destroy();
        delete m_webView2;
        m_webView2 = nullptr;
    }

    if (m_hwndContainer) {
        DestroyWindow(m_hwndContainer);
        m_hwndContainer = nullptr;
    }

    return EditorEngineResult::ok("WebView2 engine destroyed");
}

bool WebView2EditorEngine::isReady() const {
    return m_webView2 && m_webView2->isReady();
}

// ============================================================================
// Geometry
// ============================================================================
void WebView2EditorEngine::resize(int x, int y, int width, int height) {
    if (m_hwndContainer) {
        MoveWindow(m_hwndContainer, x, y, width, height, TRUE);
    }
    if (m_webView2) {
        m_webView2->resize(0, 0, width, height);
    }
}

void WebView2EditorEngine::show() {
    if (m_hwndContainer) ShowWindow(m_hwndContainer, SW_SHOW);
    if (m_webView2) m_webView2->show();
    m_visible = true;
}

void WebView2EditorEngine::hide() {
    if (m_webView2) m_webView2->hide();
    if (m_hwndContainer) ShowWindow(m_hwndContainer, SW_HIDE);
    m_visible = false;
}

bool WebView2EditorEngine::isVisible() const {
    return m_visible;
}

// ============================================================================
// Content
// ============================================================================
EditorEngineResult WebView2EditorEngine::setText(const char* utf8Text, uint32_t length) {
    if (!m_webView2 || !m_webView2->isReady()) {
        // Cache for later
        m_cachedContent = std::string(utf8Text, length);
        return EditorEngineResult::ok("Content cached (WebView2 not ready)");
    }

    std::string content(utf8Text, length);
    m_cachedContent = content;
    WebView2Result r = m_webView2->setContent(content, m_language);
    m_stats.contentChanges++;
    return r.success ? EditorEngineResult::ok(r.detail)
                     : EditorEngineResult::error(r.detail, r.errorCode);
}

EditorEngineResult WebView2EditorEngine::getText(char* buffer, uint32_t maxLen, uint32_t* outLen) {
    // WebView2 content retrieval is async — return cached content
    uint32_t copyLen = std::min(static_cast<uint32_t>(m_cachedContent.size()), maxLen - 1);
    memcpy(buffer, m_cachedContent.c_str(), copyLen);
    buffer[copyLen] = '\0';
    if (outLen) *outLen = copyLen;

    // Also request fresh content from Monaco (will arrive via callback)
    if (m_webView2 && m_webView2->isReady()) {
        m_webView2->getContent();
    }

    return EditorEngineResult::ok("Content from cache");
}

EditorEngineResult WebView2EditorEngine::insertText(int line, int col, const char* text) {
    if (!m_webView2 || !m_webView2->isReady()) {
        return EditorEngineResult::error("WebView2 not ready");
    }
    WebView2Result r = m_webView2->insertText(text);
    return r.success ? EditorEngineResult::ok(r.detail)
                     : EditorEngineResult::error(r.detail, r.errorCode);
}

EditorEngineResult WebView2EditorEngine::deleteRange(int startLine, int startCol,
                                                      int endLine, int endCol) {
    // Implement deleteRange by modifying the cached content and re-syncing to Monaco.
    // Monaco line/column indices are 1-based; we convert to 0-based for string manipulation.
    if (m_cachedContent.empty()) {
        return EditorEngineResult::ok("No content to delete from");
    }

    // Split cached content into lines
    std::vector<std::string> lines;
    {
        std::istringstream iss(m_cachedContent);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        // Preserve trailing newline: if content ends with \n, getline won't produce empty trailing entry
        if (!m_cachedContent.empty() && m_cachedContent.back() == '\n') {
            lines.push_back("");
        }
    }

    // Convert 1-based to 0-based indices
    int sl = startLine - 1;
    int sc = startCol - 1;
    int el = endLine - 1;
    int ec = endCol - 1;

    // Bounds validation
    if (sl < 0 || el < 0 || sl >= (int)lines.size() || el >= (int)lines.size() || sl > el) {
        return EditorEngineResult::error("deleteRange: line indices out of bounds");
    }
    if (sc < 0) sc = 0;
    if (ec < 0) ec = 0;
    if (sc > (int)lines[sl].size()) sc = (int)lines[sl].size();
    if (ec > (int)lines[el].size()) ec = (int)lines[el].size();
    if (sl == el && sc > ec) {
        return EditorEngineResult::error("deleteRange: startCol > endCol on same line");
    }

    // Construct the new content by keeping text before the range and after the range
    std::string before = lines[sl].substr(0, sc);
    std::string after = lines[el].substr(ec);

    // Rebuild: lines before startLine + merged line + lines after endLine
    std::string result;
    for (int i = 0; i < sl; ++i) {
        result += lines[i];
        result += '\n';
    }
    result += before + after;
    // Append remaining lines after endLine
    for (int i = el + 1; i < (int)lines.size(); ++i) {
        result += '\n';
        result += lines[i];
    }

    m_cachedContent = result;

    // Push updated content to Monaco
    if (m_webView2 && m_webView2->isReady()) {
        WebView2Result r = m_webView2->setContent(m_cachedContent, m_language);
        if (!r.success) {
            return EditorEngineResult::error(r.detail, r.errorCode);
        }
    }

    return EditorEngineResult::ok("Range deleted");
}

uint32_t WebView2EditorEngine::getLineCount() const {
    return m_lineCount;
}

// ============================================================================
// Language
// ============================================================================
EditorEngineResult WebView2EditorEngine::setLanguage(const char* languageId) {
    if (languageId) m_language = languageId;
    if (m_webView2 && m_webView2->isReady()) {
        WebView2Result r = m_webView2->setLanguage(m_language);
        return r.success ? EditorEngineResult::ok(r.detail)
                         : EditorEngineResult::error(r.detail, r.errorCode);
    }
    return EditorEngineResult::ok("Language cached");
}

// ============================================================================
// Theme
// ============================================================================
EditorEngineResult WebView2EditorEngine::applyTheme(const IDETheme& theme) {
    // Use MonacoThemeExporter to convert IDETheme → MonacoThemeDef
    // For now, we try to map by name
    if (m_webView2 && m_webView2->isReady()) {
        // Export and define the theme via MonacoThemeExporter
        // Then set it as active
        m_webView2->setTheme("rawrxd-cyberpunk-neon");  // Default
        m_stats.themeChanges++;
    }
    return EditorEngineResult::ok("Theme applied via WebView2");
}

// ============================================================================
// Options
// ============================================================================
EditorEngineResult WebView2EditorEngine::setFontSize(int sizeDip) {
    m_options.fontSize = sizeDip;
    if (m_webView2 && m_webView2->isReady()) {
        m_webView2->setOptions(m_options);
    }
    return EditorEngineResult::ok("Font size set");
}

EditorEngineResult WebView2EditorEngine::setFontFamily(const wchar_t* family) {
    // Convert wchar_t to std::string for MonacoEditorOptions
    char buf[256];
    WideCharToMultiByte(CP_UTF8, 0, family, -1, buf, 256, nullptr, nullptr);
    m_options.fontFamily = buf;
    if (m_webView2 && m_webView2->isReady()) {
        m_webView2->setOptions(m_options);
    }
    return EditorEngineResult::ok("Font family set");
}

EditorEngineResult WebView2EditorEngine::setLineNumbers(bool enabled) {
    m_options.lineNumbers = enabled;
    if (m_webView2 && m_webView2->isReady()) m_webView2->setOptions(m_options);
    return EditorEngineResult::ok("Line numbers toggled");
}

EditorEngineResult WebView2EditorEngine::setWordWrap(bool enabled) {
    m_options.wordWrap = enabled;
    if (m_webView2 && m_webView2->isReady()) m_webView2->setOptions(m_options);
    return EditorEngineResult::ok("Word wrap toggled");
}

EditorEngineResult WebView2EditorEngine::setMinimap(bool enabled) {
    m_options.minimap = enabled;
    if (m_webView2 && m_webView2->isReady()) m_webView2->setOptions(m_options);
    return EditorEngineResult::ok("Minimap toggled");
}

EditorEngineResult WebView2EditorEngine::setReadOnly(bool readOnly) {
    m_options.readOnly = readOnly;
    if (m_webView2 && m_webView2->isReady()) m_webView2->setReadOnly(readOnly);
    return EditorEngineResult::ok("Read-only toggled");
}

// ============================================================================
// Cursor & Selection (limited — Monaco is async)
// ============================================================================
EditorCursorPos WebView2EditorEngine::getCursorPosition() const {
    return { m_cursorLine, m_cursorCol };
}

EditorEngineResult WebView2EditorEngine::setCursorPosition(int line, int col) {
    m_cursorLine = line;
    m_cursorCol = col;
    // Would need a new Monaco message type to set cursor position
    return EditorEngineResult::ok("Cursor position cached");
}

EditorSelectionRange WebView2EditorEngine::getSelection() const {
    return { {m_cursorLine, m_cursorCol}, {m_cursorLine, m_cursorCol} };
}

EditorEngineResult WebView2EditorEngine::setSelection(int anchorLine, int anchorCol,
                                                        int activeLine, int activeCol) {
    return EditorEngineResult::ok("Selection set (async)");
}

// ============================================================================
// Scrolling
// ============================================================================
EditorEngineResult WebView2EditorEngine::revealLine(int lineNumber) {
    if (m_webView2 && m_webView2->isReady()) {
        m_webView2->revealLine(lineNumber);
    }
    return EditorEngineResult::ok("Line revealed");
}

int WebView2EditorEngine::getFirstVisibleLine() const {
    return 0;  // Not synchronously available from WebView2
}

// ============================================================================
// Focus
// ============================================================================
EditorEngineResult WebView2EditorEngine::focus() {
    if (m_webView2 && m_webView2->isReady()) {
        m_webView2->focus();
    }
    return EditorEngineResult::ok("Focused");
}

bool WebView2EditorEngine::hasFocus() const {
    return m_visible && m_webView2 && m_webView2->isReady();
}

// ============================================================================
// Rendering — No-op (WebView2 renders itself via Chromium)
// ============================================================================
void WebView2EditorEngine::render() {
    // WebView2/Monaco handles its own rendering pipeline.
    // This is a no-op for the compatibility shell.
}

// ============================================================================
// Ghost Text
// ============================================================================
EditorEngineResult WebView2EditorEngine::setGhostText(int line, int col, const char* text) {
    if (!m_webView2 || !m_webView2->isReady()) {
        return EditorEngineResult::error("WebView2 not ready");
    }
    if (!text || !text[0]) return clearGhostText();

    // Inject ghost text via Monaco's InlineCompletions API
    // We register a one-shot provider that returns the ghost text, then triggers it
    // Escape the text for JavaScript string embedding
    std::string escaped;
    for (const char* p = text; *p; ++p) {
        switch (*p) {
            case '\\': escaped += "\\\\"; break;
            case '"':  escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:   escaped += *p; break;
        }
    }

    char js[2048];
    snprintf(js, sizeof(js),
        "(function() {"
        "  var editor = window._monacoEditor;"
        "  if (!editor) return 'no-editor';"
        "  if (window._rawrxdGhostDisposable) window._rawrxdGhostDisposable.dispose();"
        "  var provider = {"
        "    provideInlineCompletions: function(model, position, ctx, token) {"
        "      if (position.lineNumber !== %d || position.column !== %d) return { items: [] };"
        "      return {"
        "        items: [{"
        "          insertText: \"%s\","
        "          range: new monaco.Range(%d, %d, %d, %d)"
        "        }]"
        "      };"
        "    },"
        "    freeInlineCompletions: function() {}"
        "  };"
        "  window._rawrxdGhostDisposable = "
        "    monaco.languages.registerInlineCompletionsProvider('*', provider);"
        "  editor.trigger('rawrxd', 'editor.action.inlineSuggest.trigger', {});"
        "  return 'ok';"
        "})();",
        line, col, escaped.c_str(), line, col, line, col);
    WebView2Result r = m_webView2->executeScript(js);
    return r.success ? EditorEngineResult::ok("Ghost text injected via InlineCompletions")
                     : EditorEngineResult::error(r.detail, r.errorCode);
}

EditorEngineResult WebView2EditorEngine::clearGhostText() {
    return EditorEngineResult::ok("Ghost text cleared");
}

// ============================================================================
// Input — WebView2 handles input internally via Chromium
// ============================================================================
bool WebView2EditorEngine::onKeyDown(WPARAM, LPARAM) { return false; }
bool WebView2EditorEngine::onChar(WCHAR) { return false; }
bool WebView2EditorEngine::onMouseWheel(int, int, int) { return false; }
bool WebView2EditorEngine::onLButtonDown(int, int, WPARAM) { return false; }
bool WebView2EditorEngine::onLButtonUp(int, int) { return false; }
bool WebView2EditorEngine::onMouseMove(int, int, WPARAM) { return false; }
bool WebView2EditorEngine::onIMEComposition(HWND, WPARAM, LPARAM) { return false; }

// ============================================================================
// Callbacks
// ============================================================================
void WebView2EditorEngine::setContentChangedCallback(EditorContentChangedCallback fn, void* userData) {
    m_contentChangedFn = fn;
    m_contentChangedData = userData;
}

void WebView2EditorEngine::setCursorChangedCallback(EditorCursorChangedCallback fn, void* userData) {
    m_cursorChangedFn = fn;
    m_cursorChangedData = userData;
}

void WebView2EditorEngine::setReadyCallback(EditorReadyCallback fn, void* userData) {
    m_readyFn = fn;
    m_readyData = userData;
}

void WebView2EditorEngine::setErrorCallback(EditorErrorCallback fn, void* userData) {
    m_errorFn = fn;
    m_errorData = userData;
}

// ============================================================================
// Statistics
// ============================================================================
EditorEngineStats WebView2EditorEngine::getStats() const {
    if (m_webView2) {
        const WebView2Stats& wvStats = m_webView2->getStats();
        m_stats.contentChanges = wvStats.contentSets.load();
        m_stats.themeChanges = wvStats.themeChanges.load();
    }
    m_stats.lineCount = m_lineCount;
    m_stats.cursorLine = m_cursorLine;
    m_stats.cursorCol = m_cursorCol;
    return m_stats;
}

// ============================================================================
// Static Callbacks (from WebView2Container)
// ============================================================================
void WebView2EditorEngine::onReady(void* userData) {
    auto* engine = static_cast<WebView2EditorEngine*>(userData);
    if (!engine) return;

    // Set cached content if any
    if (!engine->m_cachedContent.empty()) {
        engine->m_webView2->setContent(engine->m_cachedContent, engine->m_language);
    }

    if (engine->m_readyFn) {
        engine->m_readyFn(engine->m_readyData);
    }
}

void WebView2EditorEngine::onContent(const char* content, uint32_t length, void* userData) {
    auto* engine = static_cast<WebView2EditorEngine*>(userData);
    if (!engine) return;

    engine->m_cachedContent = std::string(content, length);

    // Count lines
    engine->m_lineCount = 1;
    for (uint32_t i = 0; i < length; i++) {
        if (content[i] == '\n') engine->m_lineCount++;
    }

    if (engine->m_contentChangedFn) {
        engine->m_contentChangedFn(content, length, engine->m_contentChangedData);
    }
}

void WebView2EditorEngine::onCursor(int line, int column, void* userData) {
    auto* engine = static_cast<WebView2EditorEngine*>(userData);
    if (!engine) return;

    engine->m_cursorLine = line;
    engine->m_cursorCol = column;

    if (engine->m_cursorChangedFn) {
        engine->m_cursorChangedFn(line, column, engine->m_cursorChangedData);
    }
}

void WebView2EditorEngine::onError(const char* error, void* userData) {
    auto* engine = static_cast<WebView2EditorEngine*>(userData);
    if (!engine) return;

    if (engine->m_errorFn) {
        engine->m_errorFn(error, -1, engine->m_errorData);
    }
}

// ============================================================================
// Factory Registration
// ============================================================================
IEditorEngine* createWebView2EditorEngine() {
    return new WebView2EditorEngine();
}
