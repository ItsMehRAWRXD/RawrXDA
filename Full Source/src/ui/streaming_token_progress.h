// ============================================================================
// streaming_token_progress.h — Pure Win32 Native Streaming Token Progress Bar
// ============================================================================
// Real-time inference progress visualization with tok/s metrics, ETA,
// elapsed time. No Qt dependencies.
//
// Pattern: C-style extern "C" API
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ============================================================================
// Callback type
// ============================================================================
typedef void (*PFN_GENERATION_STARTED)(void* userdata);
typedef void (*PFN_TOKEN_RECEIVED)(const wchar_t* token, int totalTokens, void* userdata);
typedef void (*PFN_GENERATION_COMPLETED)(int totalTokens, double tokensPerSecond, void* userdata);

// ============================================================================
// Class: StreamingTokenProgressBar
// ============================================================================
class StreamingTokenProgressBar {
public:
    explicit StreamingTokenProgressBar(HWND parent);
    ~StreamingTokenProgressBar();

    // Lifecycle
    void startGeneration(int estimatedTokens = 0);
    void onTokenGenerated(const wchar_t* token);
    void completeGeneration();
    void reset();

    // Configuration
    void setShowTokenRate(bool show) { m_showTokenRate = show; }
    void setShowElapsedTime(bool show) { m_showElapsedTime = show; }

    // Callbacks
    void setCallbacks(PFN_GENERATION_STARTED startedFn, PFN_TOKEN_RECEIVED tokenFn,
                      PFN_GENERATION_COMPLETED completedFn, void* userdata);

    // Window ops
    void show();
    void hide();
    void resize(int x, int y, int w, int h);
    HWND hwnd() const { return m_hwnd; }

private:
    void createWindow(HWND parent);
    void paint(HDC hdc, const RECT& rc);
    void paintProgressBar(HDC hdc, int x, int y, int w, int h);
    void updateMetrics();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND    m_hwnd       = nullptr;
    HFONT   m_fontStatus = nullptr;
    HFONT   m_fontMetrics = nullptr;
    HDC     m_backDC     = nullptr;
    HBITMAP m_backBuf    = nullptr;
    int     m_bufW = 0, m_bufH = 0;
    UINT_PTR m_timerId   = 0;

    // State
    bool    m_isGenerating    = false;
    int     m_totalTokens     = 0;
    int     m_estimatedTokens = 0;
    DWORD   m_startTick       = 0;
    double  m_tokensPerSecond = 0.0;
    wchar_t m_statusText[256] = L"Ready";
    wchar_t m_metricsText[256] = {};

    // Config
    bool m_showTokenRate   = true;
    bool m_showElapsedTime = true;

    // Callbacks
    PFN_GENERATION_STARTED   m_pfnStarted   = nullptr;
    PFN_TOKEN_RECEIVED       m_pfnToken     = nullptr;
    PFN_GENERATION_COMPLETED m_pfnCompleted = nullptr;
    void* m_cbUserdata = nullptr;

    static bool s_classRegistered;
};

// ============================================================================
// C API
// ============================================================================
extern "C" {
    StreamingTokenProgressBar* StreamingProgress_Create(HWND parent);
    void StreamingProgress_Start(StreamingTokenProgressBar* p, int estimatedTokens);
    void StreamingProgress_OnToken(StreamingTokenProgressBar* p, const wchar_t* token);
    void StreamingProgress_Complete(StreamingTokenProgressBar* p);
    void StreamingProgress_Reset(StreamingTokenProgressBar* p);
    void StreamingProgress_SetCallbacks(StreamingTokenProgressBar* p,
        PFN_GENERATION_STARTED startedFn, PFN_TOKEN_RECEIVED tokenFn,
        PFN_GENERATION_COMPLETED completedFn, void* userdata);
    void StreamingProgress_Show(StreamingTokenProgressBar* p);
    void StreamingProgress_Hide(StreamingTokenProgressBar* p);
    void StreamingProgress_Resize(StreamingTokenProgressBar* p, int x, int y, int w, int h);
    void StreamingProgress_Destroy(StreamingTokenProgressBar* p);
}
