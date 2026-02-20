// ============================================================================
// diff_dock.h — Pure Win32 Native Diff Preview Dock
// ============================================================================
// Side-by-side diff viewer with Accept/Reject buttons.
// No Qt dependencies.
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
// Callbacks
// ============================================================================
typedef void (*PFN_DIFF_ACCEPTED)(const wchar_t* suggestedText, void* userdata);
typedef void (*PFN_DIFF_REJECTED)(void* userdata);

// ============================================================================
// Class: DiffDock
// ============================================================================
class DiffDock {
public:
    explicit DiffDock(HWND parent);
    ~DiffDock();

    void setDiff(const wchar_t* original, const wchar_t* suggested);
    void setCallbacks(PFN_DIFF_ACCEPTED acceptFn, PFN_DIFF_REJECTED rejectFn, void* userdata);

    void show();
    void hide();
    void resize(int x, int y, int w, int h);
    HWND hwnd() const { return m_hwnd; }

private:
    void createWindow(HWND parent);
    void paint(HDC hdc, const RECT& rc);
    void paintTextPane(HDC hdc, int x, int y, int w, int h,
                       const wchar_t* text, COLORREF bgClr, COLORREF textClr, const wchar_t* label);
    void onAccept();
    void onReject();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND    m_hwnd       = nullptr;
    HWND    m_btnAccept  = nullptr;
    HWND    m_btnReject  = nullptr;
    HFONT   m_fontTitle  = nullptr;
    HFONT   m_fontMono   = nullptr;
    HFONT   m_fontBtn    = nullptr;
    HDC     m_backDC     = nullptr;
    HBITMAP m_backBuf    = nullptr;
    int     m_bufW = 0, m_bufH = 0;

    wchar_t* m_originalText  = nullptr;
    wchar_t* m_suggestedText = nullptr;

    PFN_DIFF_ACCEPTED m_pfnAccepted = nullptr;
    PFN_DIFF_REJECTED m_pfnRejected = nullptr;
    void* m_cbUserdata = nullptr;

    static bool s_classRegistered;
};

// ============================================================================
// C API
// ============================================================================
extern "C" {
    DiffDock*  DiffDock_Create(HWND parent);
    void       DiffDock_SetDiff(DiffDock* d, const wchar_t* original, const wchar_t* suggested);
    void       DiffDock_SetCallbacks(DiffDock* d, PFN_DIFF_ACCEPTED acceptFn, PFN_DIFF_REJECTED rejectFn, void* ud);
    void       DiffDock_Show(DiffDock* d);
    void       DiffDock_Hide(DiffDock* d);
    void       DiffDock_Resize(DiffDock* d, int x, int y, int w, int h);
    void       DiffDock_Destroy(DiffDock* d);
}
