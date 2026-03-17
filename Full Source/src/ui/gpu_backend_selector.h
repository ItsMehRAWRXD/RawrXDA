// ============================================================================
// gpu_backend_selector.h — Pure Win32 Native GPU Backend Selector
// ============================================================================
// Compact widget for toolbar/status bar. Detects CUDA, Vulkan, DirectML
// via DXGI and external tool probing. No Qt dependencies.
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
// Enums
// ============================================================================

enum ComputeBackend {
    BACKEND_CPU      = 0,
    BACKEND_CUDA     = 1,
    BACKEND_VULKAN   = 2,
    BACKEND_METAL    = 3,
    BACKEND_DIRECTML = 4,
    BACKEND_AUTO     = 5
};

// ============================================================================
// Backend info
// ============================================================================

struct BackendInfo {
    ComputeBackend backend;
    wchar_t displayName[32];
    wchar_t icon[8];
    wchar_t deviceName[128];
    int     vramMB;
    bool    available;
};

// ============================================================================
// Callback type
// ============================================================================
typedef void (*PFN_BACKEND_CHANGED)(ComputeBackend newBackend, void* userdata);

// ============================================================================
// Class: GPUBackendSelector
// ============================================================================
class GPUBackendSelector {
public:
    explicit GPUBackendSelector(HWND parent);
    ~GPUBackendSelector();

    // Get/Set
    ComputeBackend selectedBackend() const { return m_currentBackend; }
    void setBackend(ComputeBackend backend);
    bool isBackendAvailable(ComputeBackend backend) const;
    void refreshBackends();

    // Callback
    void setOnBackendChanged(PFN_BACKEND_CHANGED fn, void* userdata);

    // Window ops
    void show();
    void hide();
    void resize(int x, int y, int w, int h);
    HWND hwnd() const { return m_hwnd; }

private:
    void createWindow(HWND parent);
    void detectBackends();
    void populateCombo();
    void onComboChanged();
    void paint(HDC hdc, const RECT& rc);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    HWND     m_hwnd       = nullptr;
    HWND     m_combo      = nullptr;
    HFONT    m_font       = nullptr;
    HFONT    m_fontSmall  = nullptr;
    HDC      m_backDC     = nullptr;
    HBITMAP  m_backBuf    = nullptr;
    int      m_bufW = 0, m_bufH = 0;

    BackendInfo  m_backends[8] = {};
    int          m_backendCount = 0;
    ComputeBackend m_currentBackend = BACKEND_AUTO;

    PFN_BACKEND_CHANGED m_pfnChanged = nullptr;
    void* m_cbUserdata = nullptr;

    static bool s_classRegistered;
};

// ============================================================================
// C API
// ============================================================================
extern "C" {
    GPUBackendSelector* GPUBackendSelector_Create(HWND parent);
    int   GPUBackendSelector_GetBackend(GPUBackendSelector* s);
    void  GPUBackendSelector_SetBackend(GPUBackendSelector* s, int backend);
    int   GPUBackendSelector_IsAvailable(GPUBackendSelector* s, int backend);
    void  GPUBackendSelector_Refresh(GPUBackendSelector* s);
    void  GPUBackendSelector_SetCallback(GPUBackendSelector* s, PFN_BACKEND_CHANGED fn, void* ud);
    void  GPUBackendSelector_Show(GPUBackendSelector* s);
    void  GPUBackendSelector_Hide(GPUBackendSelector* s);
    void  GPUBackendSelector_Resize(GPUBackendSelector* s, int x, int y, int w, int h);
    void  GPUBackendSelector_Destroy(GPUBackendSelector* s);
}
