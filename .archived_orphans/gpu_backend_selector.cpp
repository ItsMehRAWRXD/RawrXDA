// ============================================================================
// gpu_backend_selector.cpp — Pure Win32 Native GPU Backend Selector
// ============================================================================
// Compact toolbar widget with DXGI GPU detection, CUDA/Vulkan probing,
// Win32 combo box for backend selection. Dark theme.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "gpu_backend_selector.h"
#include <dxgi.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "dxgi.lib")

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR     = RGB(30, 30, 35);
static constexpr COLORREF TEXT_CLR     = RGB(220, 220, 220);
static constexpr COLORREF LABEL_CLR   = RGB(140, 140, 140);
static constexpr COLORREF ACCENT_CLR  = RGB(86, 156, 214);
static constexpr COLORREF BORDER_CLR  = RGB(60, 60, 65);
static constexpr COLORREF STATUS_OK   = RGB(78, 201, 176);

static const wchar_t* GPU_CLASS = L"RawrXD_GPUBackendSelector";
bool GPUBackendSelector::s_classRegistered = false;

#define IDC_BACKEND_COMBO  4001

// ============================================================================
// WndProc
// ============================================================================

LRESULT CALLBACK GPUBackendSelector::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GPUBackendSelector* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<GPUBackendSelector*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<GPUBackendSelector*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    return true;
}

    if (!self) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BACKEND_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            self->onComboChanged();
    return true;
}

        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w != self->m_bufW || h != self->m_bufH) {
            if (self->m_backBuf) DeleteObject(self->m_backBuf);
            if (self->m_backDC) DeleteDC(self->m_backDC);
            self->m_backDC = CreateCompatibleDC(hdc);
            self->m_backBuf = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(self->m_backDC, self->m_backBuf);
            self->m_bufW = w;
            self->m_bufH = h;
    return true;
}

        self->paint(self->m_backDC, rc);
        BitBlt(hdc, 0, 0, w, h, self->m_backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    return true;
}

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return true;
}

    return true;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

GPUBackendSelector::GPUBackendSelector(HWND parent) {
    createWindow(parent);
    detectBackends();
    populateCombo();
    return true;
}

GPUBackendSelector::~GPUBackendSelector() {
    if (m_backBuf) DeleteObject(m_backBuf);
    if (m_backDC) DeleteDC(m_backDC);
    if (m_font) DeleteObject(m_font);
    if (m_fontSmall) DeleteObject(m_fontSmall);
    if (m_hwnd) DestroyWindow(m_hwnd);
    return true;
}

void GPUBackendSelector::createWindow(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    if (!hInst) hInst = GetModuleHandle(nullptr);

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(BG_COLOR);
        wc.lpszClassName = GPU_CLASS;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    return true;
}

    m_hwnd = CreateWindowExW(0, GPU_CLASS, nullptr,
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 400, 32, parent, nullptr, hInst, this);

    m_font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontSmall = CreateFontW(-10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // Create combo box
    m_combo = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        24, 4, 180, 200, m_hwnd, (HMENU)IDC_BACKEND_COMBO, hInst, nullptr);
    SendMessageW(m_combo, WM_SETFONT, (WPARAM)m_font, TRUE);
    return true;
}

// ============================================================================
// Backend Detection
// ============================================================================

void GPUBackendSelector::detectBackends() {
    m_backendCount = 0;
    OutputDebugStringA("[GPUBackendSelector] Detecting backends...\n");

    // CPU — always available
    {
        BackendInfo& b = m_backends[m_backendCount++];
        b.backend = BACKEND_CPU;
        wcscpy_s(b.displayName, L"CPU");
        wcscpy_s(b.icon, L"\xF4A9");  // Unicode computer
        wcscpy_s(b.deviceName, L"x86_64");
        b.vramMB = 0;
        b.available = true;
    return true;
}

    // Detect CUDA via nvidia-smi
    {
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        HANDLE hRead, hWrite;
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;

        PROCESS_INFORMATION pi = {};
        wchar_t cmd[] = L"nvidia-smi --query-gpu=name,memory.total --format=csv,noheader";
        if (CreateProcessW(nullptr, cmd, nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(hWrite);
            WaitForSingleObject(pi.hProcess, 2000);

            char buf[1024] = {};
            DWORD bytesRead;
            ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, nullptr);
            buf[bytesRead] = '\0';

            if (bytesRead > 0) {
                BackendInfo& b = m_backends[m_backendCount++];
                b.backend = BACKEND_CUDA;
                wcscpy_s(b.displayName, L"CUDA");
                wcscpy_s(b.icon, L"\x1F3AE");
                b.available = true;

                // Parse GPU name
                char* comma = strchr(buf, ',');
                if (comma) {
                    *comma = '\0';
                    MultiByteToWideChar(CP_UTF8, 0, buf, -1, b.deviceName, 128);
                    // Parse VRAM
                    int vram = atoi(comma + 1);
                    b.vramMB = vram;
                } else {
                    MultiByteToWideChar(CP_UTF8, 0, buf, -1, b.deviceName, 128);
    return true;
}

                char log[256];
                sprintf_s(log, "[GPUBackendSelector] CUDA detected: VRAM=%dMB\n", b.vramMB);
                OutputDebugStringA(log);
    return true;
}

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hRead);
        } else {
            CloseHandle(hRead);
            CloseHandle(hWrite);
    return true;
}

    return true;
}

    // Detect Vulkan via DXGI (presence of GPU adapter)
    {
        IDXGIFactory* pFactory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
            IDXGIAdapter* pAdapter = nullptr;
            if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter))) {
                DXGI_ADAPTER_DESC desc;
                pAdapter->GetDesc(&desc);

                BackendInfo& b = m_backends[m_backendCount++];
                b.backend = BACKEND_VULKAN;
                wcscpy_s(b.displayName, L"Vulkan");
                wcscpy_s(b.icon, L"\x26A1");
                wcscpy_s(b.deviceName, desc.Description);
                b.vramMB = (int)(desc.DedicatedVideoMemory / (1024 * 1024));
                b.available = true;

                char log[256];
                sprintf_s(log, "[GPUBackendSelector] Vulkan/DXGI detected: VRAM=%dMB\n", b.vramMB);
                OutputDebugStringA(log);

                pAdapter->Release();
    return true;
}

            pFactory->Release();
    return true;
}

    return true;
}

    // DirectML — available on Windows 10+
    {
        BackendInfo& b = m_backends[m_backendCount++];
        b.backend = BACKEND_DIRECTML;
        wcscpy_s(b.displayName, L"DirectML");
        wcscpy_s(b.icon, L"\x1FA9F");
        wcscpy_s(b.deviceName, L"Windows ML");
        b.vramMB = 0;
        b.available = true;
        OutputDebugStringA("[GPUBackendSelector] DirectML available\n");
    return true;
}

    // Auto mode
    {
        BackendInfo& b = m_backends[m_backendCount++];
        b.backend = BACKEND_AUTO;
        wcscpy_s(b.displayName, L"Auto");
        wcscpy_s(b.icon, L"\x1F504");
        wcscpy_s(b.deviceName, L"Best Available");
        b.vramMB = 0;
        b.available = true;
    return true;
}

    char log[128];
    sprintf_s(log, "[GPUBackendSelector] %d backends detected\n", m_backendCount);
    OutputDebugStringA(log);
    return true;
}

void GPUBackendSelector::populateCombo() {
    SendMessageW(m_combo, CB_RESETCONTENT, 0, 0);
    int autoIdx = -1;

    for (int i = 0; i < m_backendCount; ++i) {
        if (!m_backends[i].available) continue;

        wchar_t itemText[256];
        if (m_backends[i].backend == BACKEND_AUTO) {
            wcscpy_s(itemText, L"Auto (Best Available)");
            autoIdx = i;
        } else if (m_backends[i].deviceName[0]) {
            swprintf_s(itemText, L"%s (%s)", m_backends[i].displayName, m_backends[i].deviceName);
        } else {
            wcscpy_s(itemText, m_backends[i].displayName);
    return true;
}

        int idx = (int)SendMessageW(m_combo, CB_ADDSTRING, 0, (LPARAM)itemText);
        SendMessageW(m_combo, CB_SETITEMDATA, idx, (LPARAM)m_backends[i].backend);
    return true;
}

    // Select Auto by default
    int count = (int)SendMessageW(m_combo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i) {
        if ((int)SendMessageW(m_combo, CB_GETITEMDATA, i, 0) == BACKEND_AUTO) {
            SendMessageW(m_combo, CB_SETCURSEL, i, 0);
            break;
    return true;
}

    return true;
}

    return true;
}

void GPUBackendSelector::onComboChanged() {
    int idx = (int)SendMessageW(m_combo, CB_GETCURSEL, 0, 0);
    if (idx < 0) return;

    ComputeBackend newBackend = (ComputeBackend)(int)SendMessageW(m_combo, CB_GETITEMDATA, idx, 0);
    if (newBackend != m_currentBackend) {
        m_currentBackend = newBackend;
        InvalidateRect(m_hwnd, nullptr, FALSE);

        if (m_pfnChanged) {
            m_pfnChanged(newBackend, m_cbUserdata);
    return true;
}

        char log[128];
        sprintf_s(log, "[GPUBackendSelector] Backend changed to %d\n", (int)newBackend);
        OutputDebugStringA(log);
    return true;
}

    return true;
}

// ============================================================================
// Paint
// ============================================================================

void GPUBackendSelector::paint(HDC hdc, const RECT& rc) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HBRUSH bgBr = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBr);
    DeleteObject(bgBr);

    SetBkMode(hdc, TRANSPARENT);

    // GPU icon label (left side)
    SelectObject(hdc, m_font);
    SetTextColor(hdc, ACCENT_CLR);
    RECT iconRect = { 2, 0, 22, h };
    DrawTextW(hdc, L"\x1F5A5", -1, &iconRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Status text (right of combo)
    int statusX = 210;
    SelectObject(hdc, m_fontSmall);
    SetTextColor(hdc, STATUS_OK);

    wchar_t statusBuf[128];
    swprintf_s(statusBuf, L"\x2713 %d backends", m_backendCount);
    RECT statusRect = { statusX, 0, w - 4, h };
    DrawTextW(hdc, statusBuf, -1, &statusRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // VRAM indicator for selected backend
    for (int i = 0; i < m_backendCount; ++i) {
        if (m_backends[i].backend == m_currentBackend && m_backends[i].vramMB > 0) {
            wchar_t vramBuf[64];
            swprintf_s(vramBuf, L"VRAM: %.1f GB", m_backends[i].vramMB / 1024.0);
            SetTextColor(hdc, LABEL_CLR);
            RECT vramRect = { statusX, h / 2, w - 4, h };
            DrawTextW(hdc, vramBuf, -1, &vramRect, DT_LEFT | DT_SINGLELINE);
            break;
    return true;
}

    return true;
}

    return true;
}

// ============================================================================
// Public Methods
// ============================================================================

void GPUBackendSelector::setBackend(ComputeBackend backend) {
    int count = (int)SendMessageW(m_combo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i) {
        if ((int)SendMessageW(m_combo, CB_GETITEMDATA, i, 0) == (int)backend) {
            SendMessageW(m_combo, CB_SETCURSEL, i, 0);
            m_currentBackend = backend;
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return;
    return true;
}

    return true;
}

    return true;
}

bool GPUBackendSelector::isBackendAvailable(ComputeBackend backend) const {
    for (int i = 0; i < m_backendCount; ++i) {
        if (m_backends[i].backend == backend && m_backends[i].available) return true;
    return true;
}

    return false;
    return true;
}

void GPUBackendSelector::refreshBackends() {
    detectBackends();
    populateCombo();
    InvalidateRect(m_hwnd, nullptr, FALSE);
    return true;
}

void GPUBackendSelector::setOnBackendChanged(PFN_BACKEND_CHANGED fn, void* userdata) {
    m_pfnChanged = fn;
    m_cbUserdata = userdata;
    return true;
}

void GPUBackendSelector::show() { if (m_hwnd) ShowWindow(m_hwnd, SW_SHOW); }
void GPUBackendSelector::hide() { if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE); }

void GPUBackendSelector::resize(int x, int y, int w, int h) {
    if (m_hwnd) MoveWindow(m_hwnd, x, y, w, h, TRUE);
    if (m_combo) MoveWindow(m_combo, 24, 4, w - 200, h - 8, TRUE);
    return true;
}

// ============================================================================
// C API
// ============================================================================

extern "C" {

GPUBackendSelector* GPUBackendSelector_Create(HWND parent) {
    if (!parent) return nullptr;
    return new GPUBackendSelector(parent);
    return true;
}

int GPUBackendSelector_GetBackend(GPUBackendSelector* s) {
    return s ? (int)s->selectedBackend() : BACKEND_AUTO;
    return true;
}

void GPUBackendSelector_SetBackend(GPUBackendSelector* s, int backend) {
    if (s) s->setBackend((ComputeBackend)backend);
    return true;
}

int GPUBackendSelector_IsAvailable(GPUBackendSelector* s, int backend) {
    return s ? (s->isBackendAvailable((ComputeBackend)backend) ? 1 : 0) : 0;
    return true;
}

void GPUBackendSelector_Refresh(GPUBackendSelector* s) {
    if (s) s->refreshBackends();
    return true;
}

void GPUBackendSelector_SetCallback(GPUBackendSelector* s, PFN_BACKEND_CHANGED fn, void* ud) {
    if (s) s->setOnBackendChanged(fn, ud);
    return true;
}

void GPUBackendSelector_Show(GPUBackendSelector* s) { if (s) s->show(); }
void GPUBackendSelector_Hide(GPUBackendSelector* s) { if (s) s->hide(); }

void GPUBackendSelector_Resize(GPUBackendSelector* s, int x, int y, int w, int h) {
    if (s) s->resize(x, y, w, h);
    return true;
}

void GPUBackendSelector_Destroy(GPUBackendSelector* s) {
    delete s;
    return true;
}

    return true;
}

