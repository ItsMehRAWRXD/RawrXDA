// ============================================================================
// Win32IDE_WebView2.h — WebView2 + Monaco Editor Integration for Win32IDE
// ============================================================================
//
// Phase 26: WebView2 Integration — Feature #206 (the 100% completion milestone)
//
// Architecture:
//   WebView2Container   — Manages the COM lifecycle of the WebView2 control
//   MonacoEditorBridge  — Two-way message bridge between C++ and Monaco JS
//   MonacoThemeExporter  — Converts Win32 IDETheme structs to Monaco JSON
//
// The WebView2 control is embedded as a child window inside the Win32IDE
// editor area. When toggled, it replaces the RichEdit control with a full
// Monaco editor running inside Microsoft Edge's Chromium engine. This gives
// the Win32 IDE VS Code-class editing capabilities: IntelliSense-grade
// autocomplete, minimap, multi-cursor, bracket colorization, semantic
// tokens, and — critically — the full RawrXD Cyberpunk Neon theme rendered
// pixel-perfect through Monaco's defineTheme API.
//
// Message Protocol (C++ ↔ Monaco):
//   C++ → JS:  PostWebMessageAsJson({"type":"...", "payload":{...}})
//   JS → C++:  window.chrome.webview.postMessage({"type":"...", ...})
//
// Message Types:
//   setContent      — Load file content into editor
//   getContent      — Request current editor content
//   setTheme        — Apply a named Monaco theme
//   defineTheme     — Register a new custom theme definition
//   setLanguage     — Change syntax language
//   setOptions      — Update editor options (fontSize, wordWrap, etc.)
//   cursorChanged   — JS→C++ notification when cursor moves
//   contentChanged  — JS→C++ notification when content changes
//   action          — Execute a Monaco editor action (format, fold, etc.)
//
// Pattern:  Structured results, no exceptions, PatchResult-style
// Threading: All COM calls on the UI thread (STA)
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <unordered_map>

// Forward declarations — avoids pulling in COM headers everywhere
struct ICoreWebView2;
struct ICoreWebView2Controller;
struct ICoreWebView2Environment;
struct IDETheme;

class Win32IDE; // The main IDE class

// ============================================================================
// WebView2 Initialization Result
// ============================================================================
struct WebView2Result {
    bool        success;
    const char* detail;
    int         errorCode;

    static WebView2Result ok(const char* msg = "Success") {
        return { true, msg, 0 };
    }
    static WebView2Result error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// WebView2 State Tracking
// ============================================================================
enum class WebView2State {
    NotInitialized,     // Before any COM calls
    LoaderFound,        // WebView2Loader.dll located
    EnvironmentCreating,// CreateCoreWebView2EnvironmentWithOptions called
    EnvironmentReady,   // Environment created, controller pending
    ControllerCreating, // CreateCoreWebView2Controller called
    ControllerReady,    // Controller + WebView ready, HTML loading
    MonacoLoading,      // Monaco editor HTML navigated-to
    MonacoReady,        // Monaco initialized + themes registered + ready
    Error,              // Terminal error state
    Destroyed           // Cleanup complete
};

// ============================================================================
// Monaco ↔ C++ Message Types
// ============================================================================
enum class MonacoMessageType {
    // C++ → JS
    SetContent,
    GetContent,
    SetTheme,
    DefineTheme,
    SetLanguage,
    SetOptions,
    ExecuteAction,
    InsertText,
    RevealLine,
    SetReadOnly,
    Focus,

    // JS → C++
    ContentChanged,
    CursorChanged,
    Ready,
    ThemeApplied,
    Error,
    ActionResult,
    ContentRequested    // Response to GetContent
};

// ============================================================================
// Monaco Editor Options (subset of IStandaloneEditorConstructionOptions)
// ============================================================================
struct MonacoEditorOptions {
    int         fontSize        = 14;
    std::string fontFamily      = "Consolas, 'Courier New', monospace";
    bool        wordWrap        = false;
    bool        minimap         = true;
    bool        lineNumbers     = true;
    bool        renderWhitespace = false;
    int         tabSize         = 4;
    bool        insertSpaces    = true;
    bool        bracketPairColorization = true;
    bool        smoothScrolling = true;
    bool        cursorBlinking  = true; // "smooth"
    bool        cursorSmoothCaretAnimation = true;
    bool        stickyScroll    = false;
    bool        readOnly        = false;
    std::string cursorStyle     = "line"; // "line" | "block" | "underline"
};

// ============================================================================
// Monaco Theme Definition (mirrors monaco.editor.IStandaloneThemeData)
// ============================================================================
struct MonacoThemeDef {
    std::string name;           // Internal theme ID (e.g. "rawrxd-cyberpunk-neon")
    std::string base;           // "vs", "vs-dark", or "hc-black"

    // ITokenThemeRule[]
    struct TokenRule {
        std::string token;      // e.g. "comment", "keyword", "string"
        std::string foreground; // Hex color WITHOUT '#' (e.g. "FF00E6")
        std::string fontStyle;  // "italic", "bold", "underline", "" or combos
    };
    std::vector<TokenRule> rules;

    // IColors — editor chrome colors
    std::unordered_map<std::string, std::string> colors; // key → "#RRGGBB"
};

// ============================================================================
// WebView2 Statistics
// ============================================================================
struct WebView2Stats {
    std::atomic<uint64_t> messagesPosted{0};
    std::atomic<uint64_t> messagesReceived{0};
    std::atomic<uint64_t> themeChanges{0};
    std::atomic<uint64_t> contentSets{0};
    std::atomic<uint64_t> contentGets{0};
    std::atomic<uint64_t> navigations{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> monacoActions{0};
};

// ============================================================================
// Callback Types (function pointers, no std::function in hot path)
// ============================================================================
typedef void (*WebView2ReadyCallback)(void* userData);
typedef void (*MonacoContentCallback)(const char* content, uint32_t length, void* userData);
typedef void (*MonacoCursorCallback)(int line, int column, void* userData);
typedef void (*MonacoErrorCallback)(const char* error, void* userData);

// ============================================================================
// WebView2Container — Manages the WebView2 COM lifecycle
// ============================================================================
class WebView2Container {
public:
    WebView2Container();
    ~WebView2Container();

    // ---- Lifecycle ----
    WebView2Result initialize(HWND parentWindow, const std::string& userDataFolder = "");
    WebView2Result destroy();
    bool isReady() const { return m_state.load() == WebView2State::MonacoReady; }
    WebView2State getState() const { return m_state.load(); }
    const char* getStateString() const;

    // ---- Geometry ----
    void resize(int x, int y, int width, int height);
    void show();
    void hide();
    bool isVisible() const { return m_visible.load(); }

    // ---- Monaco API ----
    WebView2Result setContent(const std::string& content, const std::string& language = "plaintext");
    WebView2Result getContent();  // Async — result delivered via callback
    WebView2Result setTheme(const std::string& themeName);
    WebView2Result defineTheme(const MonacoThemeDef& theme);
    WebView2Result setLanguage(const std::string& language);
    WebView2Result setOptions(const MonacoEditorOptions& opts);
    WebView2Result executeAction(const std::string& actionId);
    WebView2Result executeScript(const std::string& javascript);
    WebView2Result insertText(const std::string& text);
    WebView2Result revealLine(int lineNumber);
    WebView2Result setReadOnly(bool readOnly);
    WebView2Result focus();

    // ---- Callbacks ----
    void setReadyCallback(WebView2ReadyCallback fn, void* userData);
    void setContentCallback(MonacoContentCallback fn, void* userData);
    void setCursorCallback(MonacoCursorCallback fn, void* userData);
    void setErrorCallback(MonacoErrorCallback fn, void* userData);

    // ---- Stats ----
    const WebView2Stats& getStats() const { return m_stats; }

    // ---- Internal (called by COM callbacks, do not call directly) ----
    void onEnvironmentCreated(HRESULT hr, ICoreWebView2Environment* env);
    void onControllerCreated(HRESULT hr, ICoreWebView2Controller* ctrl);
    void onWebMessageReceived(const std::wstring& messageJson);
    void onNavigationCompleted(bool success);

private:
    // COM pointers (raw — Release'd in destroy())
    ICoreWebView2Environment*   m_environment   = nullptr;
    ICoreWebView2Controller*    m_controller    = nullptr;
    ICoreWebView2*              m_webview       = nullptr;

    // Dynamic loader
    HMODULE                     m_hLoaderDll    = nullptr;

    // State
    std::atomic<WebView2State>  m_state{WebView2State::NotInitialized};
    std::atomic<bool>           m_visible{false};
    HWND                        m_parentWindow  = nullptr;
    RECT                        m_bounds{};

    // Callbacks
    WebView2ReadyCallback       m_readyFn       = nullptr;
    void*                       m_readyData     = nullptr;
    MonacoContentCallback       m_contentFn     = nullptr;
    void*                       m_contentData   = nullptr;
    MonacoCursorCallback        m_cursorFn      = nullptr;
    void*                       m_cursorData    = nullptr;
    MonacoErrorCallback         m_errorFn       = nullptr;
    void*                       m_errorData     = nullptr;

    // Stats
    WebView2Stats               m_stats;
    mutable std::mutex          m_mutex;

    // Registered theme names (for theme cycling)
    std::vector<std::string>    m_registeredThemes;

    // Internal helpers
    WebView2Result postMessage(const std::wstring& json);
    void navigateToMonacoHtml();
    std::string generateMonacoHtml();
    std::wstring utf8ToWide(const std::string& str);
    std::string wideToUtf8(const std::wstring& str);
};

// ============================================================================
// MonacoThemeExporter — Converts Win32 IDETheme → MonacoThemeDef
// ============================================================================
// This is the bridge that makes ALL 16 Win32 themes available in Monaco.
// Each Win32 IDETheme struct (with ~60 RGB fields) is mapped to Monaco's
// IStandaloneThemeData format (token rules + editor.* color keys).

class MonacoThemeExporter {
public:
    // Convert a single Win32 theme to Monaco format
    static MonacoThemeDef exportTheme(int themeId);

    // Export ALL 16 themes at once (for bulk registration on Monaco init)
    static std::vector<MonacoThemeDef> exportAllThemes();

    // Generate the JavaScript code to define a theme via monaco.editor.defineTheme()
    static std::string toDefineThemeJs(const MonacoThemeDef& def);

    // Generate JavaScript for ALL themes (concatenated)
    static std::string toAllDefineThemeJs();

    // Theme name for a given IDM_THEME_* constant
    static const char* monacoThemeName(int themeId);
};

// ============================================================================
// Win32IDE Integration — Declared as friend methods / member additions
// ============================================================================

// These are added to the Win32IDE class via the WebView2 integration:
//   HWND          m_hwndMonacoContainer     — Container window for WebView2
//   WebView2Container* m_webView2           — Owned WebView2 instance
//   bool          m_monacoEditorActive      — True when Monaco is the active editor
//   MonacoEditorOptions m_monacoOptions     — Current Monaco editor settings
//
// Commands:
//   IDM_VIEW_TOGGLE_MONACO    — Switch between RichEdit ↔ Monaco
//   IDM_VIEW_MONACO_DEVTOOLS  — Open Edge DevTools for the WebView2
//   IDM_VIEW_MONACO_RELOAD    — Reload the Monaco editor HTML
//   IDM_VIEW_MONACO_ZOOM_IN   — Increase Monaco editor zoom
//   IDM_VIEW_MONACO_ZOOM_OUT  — Decrease Monaco editor zoom

