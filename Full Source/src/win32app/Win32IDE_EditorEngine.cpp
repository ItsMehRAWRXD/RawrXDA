// ============================================================================
// Win32IDE_EditorEngine.cpp — Phase 28 Editor Engine Integration
// ============================================================================
//
// Phase 28: MonacoCore — Toggleable Editor Engine System
//
// This file integrates the EditorEngineFactory into the Win32IDE,
// providing:
//   1. Editor engine initialization during IDE startup
//   2. Command routing for IDM_EDITOR_ENGINE_* commands (9300 range)
//   3. Engine switching with content preservation
//   4. Status dialog showing active engine info
//   5. Menu population for View → Editor Engine submenu
//
// Pattern:  PatchResult-compatible, no exceptions
// Threading: All calls on UI thread (STA)
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include "../../include/editor_engine.h"

#include <cstdio>
#include <string>

// ============================================================================
// Initialization — Called during IDE startup (after window creation)
// ============================================================================
void Win32IDE::initEditorEngines() {
    if (m_editorEnginesInitialized) return;

    // Initialize the EditorEngineFactory with MonacoCore as default (authoritative)
    EditorEngineFactory& factory = EditorEngineFactory::instance();

    EditorEngineResult result = factory.initialize(m_hwndMain, EditorEngineType::MonacoCore);
    if (!result.success) {
        // Log error but continue — the fallback chain should have found something
        char msg[512];
        snprintf(msg, sizeof(msg),
            "[Phase 28] Editor engine initialization: %s (code %d)",
            result.detail, result.errorCode);
        OutputDebugStringA(msg);

        // Show warning to user
        MessageBoxA(m_hwndMain, msg, "Editor Engine Warning", MB_ICONWARNING | MB_OK);
    } else {
        char msg[256];
        snprintf(msg, sizeof(msg),
            "[Phase 28] Editor engine active: %s",
            factory.getEngineName(factory.getActiveEngineType()));
        OutputDebugStringA(msg);
    }

    // Set engine-changed callback to update status bar
    factory.setEngineChangedCallback(
        [](EditorEngineType newType, void* userData) {
            auto* ide = static_cast<Win32IDE*>(userData);
            if (!ide || !ide->m_hwndStatusBar) return;

            char statusMsg[128];
            snprintf(statusMsg, sizeof(statusMsg), "Editor: %s",
                EditorEngineFactory::instance().getEngineName(newType));
            SetWindowTextA(ide->m_hwndStatusBar, statusMsg);
        },
        this
    );

    // Resize active engine to fill the editor area
    IEditorEngine* engine = factory.getActiveEngine();
    if (engine) {
        RECT rcClient;
        GetClientRect(m_hwndMain, &rcClient);

        // Account for sidebar, tab bar, and status bar dimensions
        int sidebarWidth = 0;
        if (m_hwndSidebar && IsWindowVisible(m_hwndSidebar)) {
            RECT rcSidebar;
            GetWindowRect(m_hwndSidebar, &rcSidebar);
            sidebarWidth = rcSidebar.right - rcSidebar.left;
        }

        int tabBarHeight = 0;
        if (m_hwndTabBar && IsWindowVisible(m_hwndTabBar)) {
            RECT rcTabBar;
            GetWindowRect(m_hwndTabBar, &rcTabBar);
            tabBarHeight = rcTabBar.bottom - rcTabBar.top;
        }

        int statusBarHeight = 0;
        if (m_hwndStatusBar && IsWindowVisible(m_hwndStatusBar)) {
            RECT rcStatus;
            GetWindowRect(m_hwndStatusBar, &rcStatus);
            statusBarHeight = rcStatus.bottom - rcStatus.top;
        }

        int terminalHeight = 0;
        if (m_hwndSplitter && m_splitterY > 0) {
            terminalHeight = rcClient.bottom - m_splitterY;
        }

        // Calculate editor rectangle accounting for all panels
        int editorLeft = sidebarWidth;
        int editorTop = tabBarHeight;
        int editorWidth = (rcClient.right - rcClient.left) - sidebarWidth;
        int editorHeight = (rcClient.bottom - rcClient.top) - tabBarHeight - statusBarHeight - terminalHeight;

        if (editorWidth > 0 && editorHeight > 0) {
            engine->resize(editorLeft, editorTop, editorWidth, editorHeight);
        } else if (m_hwndEditor) {
            // Fallback: use existing editor rect
            RECT rcEditor;
            GetWindowRect(m_hwndEditor, &rcEditor);
            MapWindowPoints(HWND_DESKTOP, m_hwndMain, (LPPOINT)&rcEditor, 2);
            engine->resize(rcEditor.left, rcEditor.top,
                rcEditor.right - rcEditor.left, rcEditor.bottom - rcEditor.top);
        }
    }

    m_editorEnginesInitialized = true;
}

// ============================================================================
// Shutdown — Called during IDE shutdown
// ============================================================================
void Win32IDE::shutdownEditorEngines() {
    if (!m_editorEnginesInitialized) return;

    EditorEngineFactory::instance().destroyAll();
    m_editorEnginesInitialized = false;
}

// ============================================================================
// Command Router — Routes IDM_EDITOR_ENGINE_* commands
// ============================================================================
bool Win32IDE::handleEditorEngineCommand(int commandId) {
    switch (commandId) {
        case IDM_EDITOR_ENGINE_RICHEDIT_CMD:
            cmdEditorEngineSetRichEdit();
            return true;
        case IDM_EDITOR_ENGINE_WEBVIEW2_CMD:
            cmdEditorEngineSetWebView2();
            return true;
        case IDM_EDITOR_ENGINE_MONACOCORE_CMD:
            cmdEditorEngineSetMonacoCore();
            return true;
        case IDM_EDITOR_ENGINE_CYCLE_CMD:
            cmdEditorEngineCycle();
            return true;
        case IDM_EDITOR_ENGINE_STATUS_CMD:
            cmdEditorEngineStatus();
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdEditorEngineSetRichEdit() {
    if (!m_editorEnginesInitialized) initEditorEngines();

    EditorEngineResult result = EditorEngineFactory::instance().switchEngine(EditorEngineType::RichEdit);
    if (!result.success) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to switch to RichEdit: %s", result.detail);
        MessageBoxA(m_hwndMain, msg, "Engine Switch Error", MB_ICONERROR | MB_OK);
    }
}

void Win32IDE::cmdEditorEngineSetWebView2() {
    if (!m_editorEnginesInitialized) initEditorEngines();

    EditorEngineResult result = EditorEngineFactory::instance().switchEngine(EditorEngineType::WebView2);
    if (!result.success) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to switch to WebView2: %s", result.detail);
        MessageBoxA(m_hwndMain, msg, "Engine Switch Error", MB_ICONERROR | MB_OK);
    }
}

void Win32IDE::cmdEditorEngineSetMonacoCore() {
    if (!m_editorEnginesInitialized) initEditorEngines();

    EditorEngineResult result = EditorEngineFactory::instance().switchEngine(EditorEngineType::MonacoCore);
    if (!result.success) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to switch to MonacoCore: %s", result.detail);
        MessageBoxA(m_hwndMain, msg, "Engine Switch Error", MB_ICONERROR | MB_OK);
    }
}

void Win32IDE::cmdEditorEngineCycle() {
    if (!m_editorEnginesInitialized) initEditorEngines();

    EditorEngineResult result = EditorEngineFactory::instance().cycleEngine();
    if (!result.success) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to cycle engine: %s", result.detail);
        MessageBoxA(m_hwndMain, msg, "Engine Switch Error", MB_ICONERROR | MB_OK);
    }
}

void Win32IDE::cmdEditorEngineStatus() {
    EditorEngineFactory& factory = EditorEngineFactory::instance();

    if (!m_editorEnginesInitialized) {
        MessageBoxA(m_hwndMain,
            "Editor engines not initialized.\n\n"
            "Use View → Editor Engine → MonacoCore/WebView2/RichEdit to activate.",
            "Editor Engine Status", MB_ICONINFORMATION | MB_OK);
        return;
    }

    IEditorEngine* engine = factory.getActiveEngine();
    if (!engine) {
        MessageBoxA(m_hwndMain, "No active editor engine.", "Editor Engine Status",
            MB_ICONWARNING | MB_OK);
        return;
    }

    EditorEngineStats stats = engine->getStats();
    EditorCapability caps = engine->getCapabilities();

    char statusBuf[2048];
    snprintf(statusBuf, sizeof(statusBuf),
        "═══════════════════════════════════════\n"
        "     RawrXD Editor Engine Status\n"
        "═══════════════════════════════════════\n"
        "\n"
        "Active Engine:    %s\n"
        "Engine Type:      %s\n"
        "Version:          %s\n"
        "Ready:            %s\n"
        "Visible:          %s\n"
        "\n"
        "───────── Statistics ─────────\n"
        "Lines:            %u\n"
        "Cursor:           Ln %u, Col %u\n"
        "Frames Rendered:  %llu\n"
        "Key Events:       %llu\n"
        "Content Changes:  %llu\n"
        "Theme Changes:    %llu\n"
        "Memory Used:      %llu bytes\n"
        "\n"
        "───────── Capabilities ─────────\n"
        "Syntax Highlight: %s\n"
        "Minimap:          %s\n"
        "Multi-Cursor:     %s\n"
        "Code Folding:     %s\n"
        "Ghost Text (AI):  %s\n"
        "Deterministic:    %s\n"
        "Hotpatchable:     %s\n"
        "Direct GPU:       %s\n"
        "IME Support:      %s\n"
        "\n"
        "───────── Availability ─────────\n"
        "MonacoCore:       %s\n"
        "WebView2:         %s\n"
        "RichEdit:         %s\n",

        engine->getName(),
        factory.getEngineName(engine->getType()),
        engine->getVersion(),
        engine->isReady() ? "Yes" : "No",
        engine->isVisible() ? "Yes" : "No",

        stats.lineCount,
        stats.cursorLine + 1, stats.cursorCol + 1,
        (unsigned long long)stats.framesRendered,
        (unsigned long long)stats.keyEventsProcessed,
        (unsigned long long)stats.contentChanges,
        (unsigned long long)stats.themeChanges,
        (unsigned long long)stats.memoryUsedBytes,

        hasCapability(caps, EditorCapability::SyntaxHighlighting) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::Minimap) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::MultiCursor) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::CodeFolding) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::GhostText) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::DeterministicReplay) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::Hotpatchable) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::DirectGPURendering) ? "✓" : "✗",
        hasCapability(caps, EditorCapability::IMESupport) ? "✓" : "✗",

        factory.isEngineAvailable(EditorEngineType::MonacoCore) ? "Available" : "Not available",
        factory.isEngineAvailable(EditorEngineType::WebView2) ? "Available" : "Not available",
        factory.isEngineAvailable(EditorEngineType::RichEdit) ? "Available" : "Not available"
    );

    MessageBoxA(m_hwndMain, statusBuf, "Editor Engine Status — Phase 28",
        MB_ICONINFORMATION | MB_OK);
}
