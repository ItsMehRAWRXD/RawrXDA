// ============================================================================
// EditorEngineFactory.cpp — Singleton Factory for Engine Lifecycle
// ============================================================================
//
// Phase 28: MonacoCore — Toggleable Editor Engine System
//
// The EditorEngineFactory is the central coordination point for:
//   1. Creating editor engine instances (MonacoCore, WebView2, RichEdit)
//   2. Switching between engines with content preservation
//   3. Managing engine lifecycle and fallback chain
//   4. Collecting engine statistics
//
// Switching Flow:
//   1. Factory receives switchEngine(targetType) call
//   2. Gets current content from active engine (getText)
//   3. Gets current cursor/selection from active engine
//   4. Hides current engine
//   5. Shows target engine (creates if needed)
//   6. Sets content on target engine (setText)
//   7. Restores cursor/selection
//   8. Updates active engine pointer
//   9. Fires engineChangedCallback
//
// Fallback Chain: MonacoCore → WebView2 → RichEdit
//   If MonacoCore fails to initialize, try WebView2.
//   If WebView2 fails, fall back to RichEdit.
//   RichEdit is guaranteed to succeed (ships with Windows).
//
// Default Engine: MonacoCore (authoritative / primary)
//
// Pattern:  PatchResult-compatible, no exceptions
// Threading: All calls on UI thread (STA)
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "editor_engine.h"

#include <mutex>
#include <cstring>
#include <cstdio>

// ---- External factory functions (defined in each engine .cpp) ----
extern IEditorEngine* createMonacoCoreEngine();
extern IEditorEngine* createWebView2EditorEngine();
extern IEditorEngine* createRichEditEditorEngine();

// ============================================================================
// EditorEngineFactory — Singleton Implementation
// ============================================================================

// Static instance storage
static EditorEngineFactory* s_factoryInstance = nullptr;
static std::mutex s_factoryMutex;

// ---- Singleton ----
EditorEngineFactory& EditorEngineFactory::instance() {
    std::lock_guard<std::mutex> lock(s_factoryMutex);
    if (!s_factoryInstance) {
        s_factoryInstance = new EditorEngineFactory();
    }
    return *s_factoryInstance;
}

// ---- Constructor ----
EditorEngineFactory::EditorEngineFactory()
    : m_parentWindow(nullptr)
    , m_activeEngine(nullptr)
    , m_monacoCore(nullptr)
    , m_webView2(nullptr)
    , m_richEdit(nullptr)
    , m_defaultType(EditorEngineType::MonacoCore)
    , m_initialized(false)
    , m_engineChangedFn(nullptr)
    , m_engineChangedData(nullptr)
{
    memset(&m_lastSwitchResult, 0, sizeof(m_lastSwitchResult));
}

// ---- Destructor ----
EditorEngineFactory::~EditorEngineFactory() {
    destroyAll();
}

// ============================================================================
// Initialization
// ============================================================================
EditorEngineResult EditorEngineFactory::initialize(HWND parentWindow, EditorEngineType defaultType) {
    std::lock_guard<std::mutex> lock(m_engineMutex);

    if (m_initialized) {
        return EditorEngineResult::ok("Already initialized");
    }

    m_parentWindow = parentWindow;
    m_defaultType = defaultType;

    // Attempt to create the default engine
    EditorEngineResult result = createEngine_internal(defaultType);
    if (!result.success) {
        // Fallback chain: MonacoCore → WebView2 → RichEdit
        if (defaultType == EditorEngineType::MonacoCore) {
            char msg[256];
            snprintf(msg, sizeof(msg), "MonacoCore init failed (%s), trying WebView2...", result.detail);
            logError(msg);

            result = createEngine_internal(EditorEngineType::WebView2);
            if (!result.success) {
                snprintf(msg, sizeof(msg), "WebView2 init failed (%s), falling back to RichEdit", result.detail);
                logError(msg);

                result = createEngine_internal(EditorEngineType::RichEdit);
            }
        } else if (defaultType == EditorEngineType::WebView2) {
            result = createEngine_internal(EditorEngineType::RichEdit);
        }
    }

    if (!result.success) {
        return EditorEngineResult::error("All editor engines failed to initialize");
    }

    m_initialized = true;
    return EditorEngineResult::ok("Editor engine factory initialized");
}

// ============================================================================
// Engine Creation (internal — called under lock)
// ============================================================================
EditorEngineResult EditorEngineFactory::createEngine_internal(EditorEngineType type) {
    IEditorEngine** slot = nullptr;

    switch (type) {
        case EditorEngineType::MonacoCore:
            if (m_monacoCore) { m_activeEngine = m_monacoCore; return EditorEngineResult::ok("Reused"); }
            slot = &m_monacoCore;
            m_monacoCore = createMonacoCoreEngine();
            break;

        case EditorEngineType::WebView2:
            if (m_webView2) { m_activeEngine = m_webView2; return EditorEngineResult::ok("Reused"); }
            slot = &m_webView2;
            m_webView2 = createWebView2EditorEngine();
            break;

        case EditorEngineType::RichEdit:
            if (m_richEdit) { m_activeEngine = m_richEdit; return EditorEngineResult::ok("Reused"); }
            slot = &m_richEdit;
            m_richEdit = createRichEditEditorEngine();
            break;

        default:
            return EditorEngineResult::error("Unknown engine type");
    }

    IEditorEngine* engine = *slot;
    if (!engine) {
        return EditorEngineResult::error("Failed to allocate engine");
    }

    EditorEngineResult initResult = engine->initialize(m_parentWindow);
    if (!initResult.success) {
        delete engine;
        *slot = nullptr;
        return initResult;
    }

    m_activeEngine = engine;
    m_activeEngine->show();

    return EditorEngineResult::ok("Engine created and activated");
}

// ============================================================================
// Engine Switching (the core toggle logic)
// ============================================================================
EditorEngineResult EditorEngineFactory::switchEngine(EditorEngineType targetType) {
    std::lock_guard<std::mutex> lock(m_engineMutex);

    if (!m_activeEngine) {
        return EditorEngineResult::error("No active engine");
    }

    // Same engine — no-op
    if (m_activeEngine->getType() == targetType) {
        return EditorEngineResult::ok("Already active");
    }

    // ---- Step 1: Extract state from current engine ----
    char* contentBuf = nullptr;
    uint32_t contentLen = 0;
    EditorCursorPos cursor = m_activeEngine->getCursorPosition();
    EditorSelectionRange selection = m_activeEngine->getSelection();
    int firstVisible = m_activeEngine->getFirstVisibleLine();

    // Allocate buffer for content extraction
    uint32_t lineCount = m_activeEngine->getLineCount();
    uint32_t estimatedSize = lineCount * 120;  // Rough estimate
    if (estimatedSize < 65536) estimatedSize = 65536;

    contentBuf = new char[estimatedSize];
    EditorEngineResult getResult = m_activeEngine->getText(contentBuf, estimatedSize, &contentLen);
    if (!getResult.success) {
        delete[] contentBuf;
        return EditorEngineResult::error("Failed to extract content from current engine");
    }

    // ---- Step 2: Hide current engine ----
    m_activeEngine->hide();

    // ---- Step 3: Activate target engine (create if needed) ----
    EditorEngineResult createResult = createEngine_internal(targetType);
    if (!createResult.success) {
        // Fallback: re-show the previous engine
        m_activeEngine->show();
        delete[] contentBuf;

        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to switch to %s: %s",
            getEngineName(targetType), createResult.detail);
        m_lastSwitchResult = EditorEngineResult::error(msg);
        return m_lastSwitchResult;
    }

    // ---- Step 4: Transfer content ----
    EditorEngineResult setResult = m_activeEngine->setText(contentBuf, contentLen);
    delete[] contentBuf;
    contentBuf = nullptr;

    if (!setResult.success) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Engine switched but content transfer failed: %s",
            setResult.detail);
        // Continue anyway — the engine is active
        logError(msg);
    }

    // ---- Step 5: Restore cursor and scroll ----
    m_activeEngine->setCursorPosition(cursor.line, cursor.column);
    m_activeEngine->setSelection(
        selection.anchor.line, selection.anchor.column,
        selection.active.line, selection.active.column
    );
    m_activeEngine->revealLine(firstVisible);

    // ---- Step 6: Resize to current viewport ----
    RECT rc;
    if (m_parentWindow && GetClientRect(m_parentWindow, &rc)) {
        m_activeEngine->resize(0, 0, rc.right - rc.left, rc.bottom - rc.top);
    }

    // ---- Step 7: Focus ----
    m_activeEngine->focus();

    // ---- Step 8: Fire callback ----
    if (m_engineChangedFn) {
        m_engineChangedFn(targetType, m_engineChangedData);
    }

    m_lastSwitchResult = EditorEngineResult::ok("Engine switched successfully");
    return m_lastSwitchResult;
}

// ============================================================================
// Cycle through engines: MonacoCore → WebView2 → RichEdit → MonacoCore
// ============================================================================
EditorEngineResult EditorEngineFactory::cycleEngine() {
    if (!m_activeEngine) {
        return EditorEngineResult::error("No active engine");
    }

    EditorEngineType current = m_activeEngine->getType();
    EditorEngineType next;

    switch (current) {
        case EditorEngineType::MonacoCore:
            next = EditorEngineType::WebView2;
            break;
        case EditorEngineType::WebView2:
            next = EditorEngineType::RichEdit;
            break;
        case EditorEngineType::RichEdit:
            next = EditorEngineType::MonacoCore;
            break;
        default:
            next = EditorEngineType::MonacoCore;
            break;
    }

    return switchEngine(next);
}

// ============================================================================
// Query
// ============================================================================
IEditorEngine* EditorEngineFactory::getActiveEngine() const {
    return m_activeEngine;
}

EditorEngineType EditorEngineFactory::getActiveEngineType() const {
    return m_activeEngine ? m_activeEngine->getType() : EditorEngineType::RichEdit;
}

IEditorEngine* EditorEngineFactory::getEngine(EditorEngineType type) const {
    switch (type) {
        case EditorEngineType::MonacoCore: return m_monacoCore;
        case EditorEngineType::WebView2:   return m_webView2;
        case EditorEngineType::RichEdit:   return m_richEdit;
        default: return nullptr;
    }
}

bool EditorEngineFactory::isEngineAvailable(EditorEngineType type) const {
    IEditorEngine* engine = getEngine(type);
    return engine && engine->isReady();
}

const char* EditorEngineFactory::getEngineName(EditorEngineType type) const {
    switch (type) {
        case EditorEngineType::MonacoCore: return "MonacoCore";
        case EditorEngineType::WebView2:   return "WebView2 (Monaco)";
        case EditorEngineType::RichEdit:   return "RichEdit (Fallback)";
        default: return "Unknown";
    }
}

// ============================================================================
// Cleanup
// ============================================================================
void EditorEngineFactory::destroyAll() {
    std::lock_guard<std::mutex> lock(m_engineMutex);

    m_activeEngine = nullptr;

    if (m_monacoCore) {
        m_monacoCore->destroy();
        delete m_monacoCore;
        m_monacoCore = nullptr;
    }
    if (m_webView2) {
        m_webView2->destroy();
        delete m_webView2;
        m_webView2 = nullptr;
    }
    if (m_richEdit) {
        m_richEdit->destroy();
        delete m_richEdit;
        m_richEdit = nullptr;
    }

    m_initialized = false;
}

// ============================================================================
// Callbacks
// ============================================================================
void EditorEngineFactory::setEngineChangedCallback(EngineChangedCallback fn, void* userData) {
    m_engineChangedFn = fn;
    m_engineChangedData = userData;
}

// ============================================================================
// Statistics
// ============================================================================
EditorEngineStats EditorEngineFactory::getActiveStats() const {
    if (m_activeEngine) return m_activeEngine->getStats();
    EditorEngineStats empty{};
    return empty;
}

// ============================================================================
// Status String (for status bar display)
// ============================================================================
void EditorEngineFactory::getStatusString(char* buf, uint32_t maxLen) const {
    if (!m_activeEngine || !buf || maxLen == 0) {
        if (buf && maxLen > 0) buf[0] = '\0';
        return;
    }

    const char* name = m_activeEngine->getName();
    uint32_t lines = m_activeEngine->getLineCount();
    EditorCursorPos pos = m_activeEngine->getCursorPosition();

    snprintf(buf, maxLen, "[%s] Ln %d, Col %d | %u lines",
        name, pos.line + 1, pos.column + 1, lines);
}

// ============================================================================
// Error Logging (internal)
// ============================================================================
void EditorEngineFactory::logError(const char* msg) {
    // Route to error callback on active engine if available
    // In production, this would write to the IDE's log panel
    OutputDebugStringA("[EditorEngineFactory] ");
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
}

// ============================================================================
// Resize All (propagate to active engine)
// ============================================================================
void EditorEngineFactory::resizeActive(int x, int y, int width, int height) {
    if (m_activeEngine) {
        m_activeEngine->resize(x, y, width, height);
    }
}

// ============================================================================
// Forward Input to Active Engine
// ============================================================================
bool EditorEngineFactory::forwardKeyDown(WPARAM wParam, LPARAM lParam) {
    return m_activeEngine ? m_activeEngine->onKeyDown(wParam, lParam) : false;
}

bool EditorEngineFactory::forwardChar(WCHAR ch) {
    return m_activeEngine ? m_activeEngine->onChar(ch) : false;
}

bool EditorEngineFactory::forwardMouseWheel(int delta, int x, int y) {
    return m_activeEngine ? m_activeEngine->onMouseWheel(delta, x, y) : false;
}

bool EditorEngineFactory::forwardLButtonDown(int x, int y, WPARAM mod) {
    return m_activeEngine ? m_activeEngine->onLButtonDown(x, y, mod) : false;
}

bool EditorEngineFactory::forwardLButtonUp(int x, int y) {
    return m_activeEngine ? m_activeEngine->onLButtonUp(x, y) : false;
}

bool EditorEngineFactory::forwardMouseMove(int x, int y, WPARAM mod) {
    return m_activeEngine ? m_activeEngine->onMouseMove(x, y, mod) : false;
}
