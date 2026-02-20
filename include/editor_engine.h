// ============================================================================
// editor_engine.h — IEditorEngine Abstraction Layer
// ============================================================================
//
// Phase 28: MonacoCore Native Engine — Toggleable Editor Engine Architecture
//
// Purpose:
//   Defines a polymorphic editor engine interface so the Win32IDE can switch
//   between rendering backends at runtime:
//
//     1. MonacoCore  — Pure native x64 engine (Direct2D/DirectWrite, ASM core)
//     2. WebView2    — Chromium-hosted Monaco editor (existing Phase 26)
//     3. RichEdit    — Win32 RichEdit control fallback (original)
//
// Design Principles:
//   - PatchResult-style structured results (no exceptions)
//   - Function pointer callbacks (no std::function in hot path)
//   - All COM/D2D calls on UI thread (STA)
//   - Deterministic rendering for replay compatibility
//   - No circular includes
//
// Pattern:  IEditorEngine is the authoritative editor interface.
//           EditorEngineFactory creates and manages engine instances.
//           Win32IDE owns the factory and dispatches through it.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>

// Full IDETheme definition (extracted from Win32IDE.h)
#include "IDETheme.h"

// ============================================================================
// Editor Engine Result (PatchResult-compatible)
// ============================================================================
struct EditorEngineResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static EditorEngineResult ok(const char* msg = "Success") {
        return { true, msg, 0 };
    }
    static EditorEngineResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Editor Engine Type Enumeration
// ============================================================================
enum class EditorEngineType : int {
    RichEdit    = 0,    // Win32 RichEdit control (emergency fallback)
    WebView2    = 1,    // Chromium + Monaco JS (compatibility shell)
    MonacoCore  = 2,    // Native x64 engine (authoritative, Direct2D)

    Count       = 3
};

// ============================================================================
// Editor Engine Capabilities (bitfield)
// ============================================================================
enum class EditorCapability : uint32_t {
    None                = 0x00000000,
    SyntaxHighlighting  = 0x00000001,
    Minimap             = 0x00000002,
    MultiCursor         = 0x00000004,
    CodeFolding         = 0x00000008,
    AutoComplete        = 0x00000010,
    BracketMatching     = 0x00000020,
    GhostText           = 0x00000040,   // AI agent overlay
    DeterministicReplay = 0x00000080,   // Phase 5 replay support
    Hotpatchable        = 0x00000100,   // Live byte-level patching
    LineNumbers         = 0x00000200,
    WordWrap            = 0x00000400,
    UndoRedo            = 0x00000800,
    Find                = 0x00001000,
    SelectionRendering  = 0x00002000,
    ThemeSupport        = 0x00004000,
    ReadOnlyMode        = 0x00008000,
    IMESupport          = 0x00010000,
    ScrollBar           = 0x00020000,
    DirectGPURendering  = 0x00040000,   // Direct2D / D3D path

    // Composite masks
    BasicEditing        = 0x00002E01,   // Syntax + LineNumbers + UndoRedo + Selection + Find
    FullIDE             = 0x0007FFFF,   // Everything
};

inline EditorCapability operator|(EditorCapability a, EditorCapability b) {
    return static_cast<EditorCapability>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline EditorCapability operator&(EditorCapability a, EditorCapability b) {
    return static_cast<EditorCapability>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasCapability(EditorCapability set, EditorCapability flag) {
    return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

// ============================================================================
// Editor Engine Statistics
// ============================================================================
struct EditorEngineStats {
    uint64_t    framesRendered;
    uint64_t    keyEventsProcessed;
    uint64_t    contentChanges;
    uint64_t    themeChanges;
    double      lastFrameTimeMs;
    double      avgFrameTimeMs;
    uint64_t    memoryUsedBytes;
    uint32_t    lineCount;
    uint32_t    cursorLine;
    uint32_t    cursorCol;
};

// ============================================================================
// Editor Cursor Position
// ============================================================================
struct EditorCursorPos {
    int line;       // 0-based
    int column;     // 0-based, UTF-16 code units
};

// ============================================================================
// Editor Selection Range
// ============================================================================
struct EditorSelectionRange {
    EditorCursorPos anchor;     // Selection start
    EditorCursorPos active;     // Selection end (cursor position)
};

// ============================================================================
// Callback Types (function pointers, no std::function in hot path)
// ============================================================================
typedef void (*EditorContentChangedCallback)(const char* newContent, uint32_t length, void* userData);
typedef void (*EditorCursorChangedCallback)(int line, int column, void* userData);
typedef void (*EditorReadyCallback)(void* userData);
typedef void (*EditorErrorCallback)(const char* error, int code, void* userData);

// ============================================================================
// IEditorEngine — Abstract Editor Engine Interface
// ============================================================================
//
// This is the authoritative interface for all editor engines in RawrXD.
// Every engine (MonacoCore, WebView2, RichEdit) implements this interface.
//
// Threading: All methods must be called from the UI thread (STA).
// Memory:    String parameters are borrowed — engines must copy if needed.
// Lifecycle: initialize() → [use] → destroy()
//
class IEditorEngine {
public:
    virtual ~IEditorEngine() = default;

    // ---- Identity ----
    virtual EditorEngineType    getType() const = 0;
    virtual const char*         getName() const = 0;
    virtual const char*         getVersion() const = 0;
    virtual EditorCapability    getCapabilities() const = 0;

    // ---- Lifecycle ----
    virtual EditorEngineResult  initialize(HWND parentWindow) = 0;
    virtual EditorEngineResult  destroy() = 0;
    virtual bool                isReady() const = 0;

    // ---- Geometry ----
    virtual void                resize(int x, int y, int width, int height) = 0;
    virtual void                show() = 0;
    virtual void                hide() = 0;
    virtual bool                isVisible() const = 0;

    // ---- Content ----
    virtual EditorEngineResult  setText(const char* utf8Text, uint32_t length) = 0;
    virtual EditorEngineResult  getText(char* buffer, uint32_t maxLen, uint32_t* outLen) = 0;
    virtual EditorEngineResult  insertText(int line, int col, const char* text) = 0;
    virtual EditorEngineResult  deleteRange(int startLine, int startCol,
                                            int endLine, int endCol) = 0;
    virtual uint32_t            getLineCount() const = 0;

    // ---- Language / Syntax ----
    virtual EditorEngineResult  setLanguage(const char* languageId) = 0;

    // ---- Theme ----
    virtual EditorEngineResult  applyTheme(const IDETheme& theme) = 0;

    // ---- Options ----
    virtual EditorEngineResult  setFontSize(int sizeDip) = 0;
    virtual EditorEngineResult  setFontFamily(const wchar_t* family) = 0;
    virtual EditorEngineResult  setLineNumbers(bool enabled) = 0;
    virtual EditorEngineResult  setWordWrap(bool enabled) = 0;
    virtual EditorEngineResult  setMinimap(bool enabled) = 0;
    virtual EditorEngineResult  setReadOnly(bool readOnly) = 0;

    // ---- Cursor & Selection ----
    virtual EditorCursorPos     getCursorPosition() const = 0;
    virtual EditorEngineResult  setCursorPosition(int line, int col) = 0;
    virtual EditorSelectionRange getSelection() const = 0;
    virtual EditorEngineResult  setSelection(int anchorLine, int anchorCol,
                                              int activeLine, int activeCol) = 0;

    // ---- Scrolling ----
    virtual EditorEngineResult  revealLine(int lineNumber) = 0;
    virtual int                 getFirstVisibleLine() const = 0;

    // ---- Focus ----
    virtual EditorEngineResult  focus() = 0;
    virtual bool                hasFocus() const = 0;

    // ---- Rendering ----
    // Called by the host in WM_PAINT for engines that do their own rendering
    // (MonacoCore). WebView2 and RichEdit render themselves.
    virtual void                render() = 0;

    // ---- Ghost Text / AI Overlay ----
    // Insert temporary "phantom" text for AI suggestions (not committed to buffer)
    virtual EditorEngineResult  setGhostText(int line, int col, const char* text) = 0;
    virtual EditorEngineResult  clearGhostText() = 0;

    // ---- Input Dispatch ----
    // Forward Win32 messages to the engine. Returns true if consumed.
    virtual bool                onKeyDown(WPARAM wParam, LPARAM lParam) = 0;
    virtual bool                onChar(WCHAR ch) = 0;
    virtual bool                onMouseWheel(int delta, int x, int y) = 0;
    virtual bool                onLButtonDown(int x, int y, WPARAM modifiers) = 0;
    virtual bool                onLButtonUp(int x, int y) = 0;
    virtual bool                onMouseMove(int x, int y, WPARAM modifiers) = 0;
    virtual bool                onIMEComposition(HWND hwnd, WPARAM wParam, LPARAM lParam) = 0;

    // ---- Callbacks ----
    virtual void setContentChangedCallback(EditorContentChangedCallback fn, void* userData) = 0;
    virtual void setCursorChangedCallback(EditorCursorChangedCallback fn, void* userData) = 0;
    virtual void setReadyCallback(EditorReadyCallback fn, void* userData) = 0;
    virtual void setErrorCallback(EditorErrorCallback fn, void* userData) = 0;

    // ---- Statistics ----
    virtual EditorEngineStats   getStats() const = 0;

    // ---- HWND Access ----
    // Returns the native window handle managed by this engine (if any).
    // MonacoCore may return NULL (renders to parent directly).
    // WebView2 returns the container HWND.
    // RichEdit returns the HWND of the RichEdit control.
    virtual HWND                getWindowHandle() const = 0;
};

// ============================================================================
// EditorEngineInfo — Metadata for registered engines
// ============================================================================
struct EditorEngineInfo {
    EditorEngineType    type;
    const char*         name;
    const char*         description;
    EditorCapability    capabilities;
    bool                available;      // Runtime availability check
};

// ============================================================================
// EditorEngineFactory — Creates and manages engine instances
// ============================================================================
//
// Singleton factory that:
//   1. Registers available engine types at startup
//   2. Creates engine instances on demand
//   3. Manages the "active" engine for the IDE
//   4. Handles content synchronization during engine switches
//
// MonacoCore is the default (authoritative) engine.
// WebView2 is the optional compatibility shell.
// RichEdit is the emergency fallback.
//
class EditorEngineFactory {
public:
    // Singleton access
    static EditorEngineFactory& instance();

    // ---- Initialization ----
    // Initialize the factory with a parent window and default engine type.
    // Implements fallback chain: MonacoCore → WebView2 → RichEdit
    EditorEngineResult      initialize(HWND parentWindow,
                                        EditorEngineType defaultType = EditorEngineType::MonacoCore);

    // ---- Engine Switching ----
    // The core toggle logic. Switches to targetType with content preservation:
    //   1. Gets content/cursor from current engine
    //   2. Hides current, shows target (creates if needed)
    //   3. Transfers content and restores cursor/scroll
    //   4. Fires engineChangedCallback
    EditorEngineResult      switchEngine(EditorEngineType targetType);
    EditorEngineResult      cycleEngine();

    // ---- Active Engine Access ----
    IEditorEngine*          getActiveEngine() const;
    EditorEngineType        getActiveEngineType() const;
    IEditorEngine*          getEngine(EditorEngineType type) const;
    bool                    isEngineAvailable(EditorEngineType type) const;
    const char*             getEngineName(EditorEngineType type) const;

    // ---- Lifecycle ----
    void                    destroyAll();

    // ---- Callbacks ----
    typedef void (*EngineChangedCallback)(EditorEngineType newType, void* userData);
    void                    setEngineChangedCallback(EngineChangedCallback fn, void* userData);

    // ---- Statistics ----
    EditorEngineStats       getActiveStats() const;
    void                    getStatusString(char* buf, uint32_t maxLen) const;

    // ---- Input Forwarding ----
    void                    resizeActive(int x, int y, int width, int height);
    bool                    forwardKeyDown(WPARAM wParam, LPARAM lParam);
    bool                    forwardChar(WCHAR ch);
    bool                    forwardMouseWheel(int delta, int x, int y);
    bool                    forwardLButtonDown(int x, int y, WPARAM mod);
    bool                    forwardLButtonUp(int x, int y);
    bool                    forwardMouseMove(int x, int y, WPARAM mod);

    // ---- Display Names ----
    static const char*      engineTypeName(EditorEngineType type);
    static const char*      engineTypeDescription(EditorEngineType type);

private:
    EditorEngineFactory();
    ~EditorEngineFactory();
    EditorEngineFactory(const EditorEngineFactory&) = delete;
    EditorEngineFactory& operator=(const EditorEngineFactory&) = delete;

    EditorEngineResult createEngine_internal(EditorEngineType type);
    void logError(const char* msg);

    HWND                    m_parentWindow;
    IEditorEngine*          m_activeEngine;
    IEditorEngine*          m_monacoCore;
    IEditorEngine*          m_webView2;
    IEditorEngine*          m_richEdit;
    EditorEngineType        m_defaultType;
    bool                    m_initialized;
    std::mutex              m_engineMutex;
    EditorEngineResult      m_lastSwitchResult;

    EngineChangedCallback   m_engineChangedFn;
    void*                   m_engineChangedData;
};

// ============================================================================
// Command IDs for Editor Engine Switching (Phase 28)
// NOTE: 9200 range is occupied by LSP Server (Phase 27). Using 9300 range.
// ============================================================================
#define IDM_EDITOR_ENGINE_RICHEDIT      9300
#define IDM_EDITOR_ENGINE_WEBVIEW2      9301
#define IDM_EDITOR_ENGINE_MONACOCORE    9302
#define IDM_EDITOR_ENGINE_CYCLE         9303    // Cycle to next available
#define IDM_EDITOR_ENGINE_STATUS        9304    // Show engine info dialog

// End of editor_engine.h — guarded by #pragma once
