// ============================================================================
// editor_agent_integration.hpp — Pure Win32 Native Editor Agent Integration
// ============================================================================
// Ghost text suggestions triggered by TAB key, acceptance via ENTER,
// overlay rendering on editor HWND. No Qt dependencies.
//
// Pattern: C-style extern "C" API + OOP internal
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

// ============================================================================
// Structs
// ============================================================================

struct GhostTextContext {
    wchar_t fileType[64];
    wchar_t currentLine[1024];
    wchar_t previousLines[4096];
    int     cursorColumn;
};

struct GhostTextSuggestion {
    wchar_t text[2048];
    wchar_t explanation[512];
    int     confidence;        // 0..100
};

// ============================================================================
// Callback types — agent bridge
// ============================================================================
typedef void (*PFN_AGENT_PLAN_WISH)(const wchar_t* wish, void* userdata);
typedef void (*PFN_AGENT_COMPLETED)(const GhostTextSuggestion* suggestion, int elapsedMs, void* userdata);

// ============================================================================
// Class: EditorAgentIntegration
// ============================================================================
class EditorAgentIntegration {
public:
    explicit EditorAgentIntegration(HWND editorHwnd);
    ~EditorAgentIntegration();

    // Agent bridge
    void setAgentCallback(PFN_AGENT_PLAN_WISH planFn, PFN_AGENT_COMPLETED completedFn, void* userdata);

    // Configuration
    void setGhostTextEnabled(bool enabled);
    void setFileType(const wchar_t* fileType);
    void setAutoSuggestions(bool enabled);

    // Trigger/Accept/Dismiss
    void triggerSuggestion(const GhostTextContext* ctx = nullptr);
    bool acceptSuggestion();
    void dismissSuggestion();
    void clearGhostText();

    // Called by agent when suggestion arrives
    void onSuggestionGenerated(const GhostTextSuggestion* suggestion, int elapsedMs);

    // Paint ghost text on editor DC (call from editor's WM_PAINT after normal paint)
    void paintGhostOverlay(HDC hdc, int editorWidth, int editorHeight);

    // Resize overlay
    void resize(int x, int y, int w, int h);

private:
    // Subclass procedure for editor HWND
    static LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR subclassId, DWORD_PTR refData);

    // Internal
    GhostTextContext extractContext() const;
    void generateSuggestion(const GhostTextContext& ctx);

    // Editor handle
    HWND m_editorHwnd = nullptr;

    // Overlay window (transparent child for ghost text rendering)
    HWND m_overlayHwnd = nullptr;
    static LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);

    // Fonts
    HFONT m_ghostFont = nullptr;
    HFONT m_normalFont = nullptr;

    // State
    bool m_ghostTextEnabled = true;
    bool m_autoSuggestions = false;
    wchar_t m_fileType[64] = {};
    GhostTextSuggestion m_currentSuggestion = {};
    bool m_hasSuggestion = false;
    int  m_ghostRow = -1;
    int  m_ghostCol = -1;

    // Auto-suggestion timer
    UINT_PTR m_autoTimer = 0;

    // Agent callbacks
    PFN_AGENT_PLAN_WISH m_pfnPlanWish = nullptr;
    PFN_AGENT_COMPLETED m_pfnCompleted = nullptr;
    void* m_agentUserdata = nullptr;

    // Metrics
    DWORD m_suggestionsGenerated = 0;
    DWORD m_suggestionsAccepted = 0;
    DWORD m_suggestionsDismissed = 0;

    static bool s_overlayClassRegistered;
};

// ============================================================================
// C API
// ============================================================================
extern "C" {
    EditorAgentIntegration* EditorAgent_Create(HWND editorHwnd);
    void EditorAgent_SetCallback(EditorAgentIntegration* ea, PFN_AGENT_PLAN_WISH planFn,
                                  PFN_AGENT_COMPLETED completedFn, void* userdata);
    void EditorAgent_SetGhostTextEnabled(EditorAgentIntegration* ea, int enabled);
    void EditorAgent_SetFileType(EditorAgentIntegration* ea, const wchar_t* fileType);
    void EditorAgent_SetAutoSuggestions(EditorAgentIntegration* ea, int enabled);
    void EditorAgent_TriggerSuggestion(EditorAgentIntegration* ea);
    int  EditorAgent_AcceptSuggestion(EditorAgentIntegration* ea);
    void EditorAgent_DismissSuggestion(EditorAgentIntegration* ea);
    void EditorAgent_OnSuggestionGenerated(EditorAgentIntegration* ea, const GhostTextSuggestion* s, int ms);
    void EditorAgent_PaintOverlay(EditorAgentIntegration* ea, HDC hdc, int w, int h);
    void EditorAgent_Destroy(EditorAgentIntegration* ea);
}
