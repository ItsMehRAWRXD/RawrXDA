// ============================================================================
// Win32IDE_DownloadsPanel.cpp — LM Studio-style Downloads panel
// ============================================================================
// Implements:
//   RawrXD::DownloadRegistry  — global thread-safe download item store
//   DownloadsPanelWindow       — owner-draw Win32 floating panel window
//
// Visual design mirrors LM Studio Downloads:
//   ┌──────────────────────────────────────────────────────────┐
//   │ Downloads                                         [Clear] │
//   │ Filter...                                                  │
//   ├──────────────────────────────────────────────────────────┤
//   │ [MODEL]  deepseek/deepseek-r1...    Completed  5.03 GB   │
//   │ [MODEL]  google/gemma-4-31b         Completed  19.89 GB  │
//   │ [RUNTIME] llama.cpp-win-avx2 (1..) Completed  2.84 MB   │
//   └──────────────────────────────────────────────────────────┘
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>

#include "Win32IDE_DownloadsPanel.h"

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "comctl32.lib")

// ============================================================================
// DownloadRegistry
// ============================================================================

namespace RawrXD {

DownloadRegistry& DownloadRegistry::Instance() {
    static DownloadRegistry s_instance;
    return s_instance;
}

DownloadItem* DownloadRegistry::FindByName(const std::string& name) {
    // caller holds m_mutex
    for (auto& item : m_items) {
        if (item.name == name) return &item;
    }
    return nullptr;
}

size_t DownloadRegistry::AddOrUpdate(const DownloadItem& item) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DownloadItem* existing = FindByName(item.name);
    if (existing) {
        *existing = item;
        size_t idx = static_cast<size_t>(existing - m_items.data());
        NotifyChange();
        return idx;
    }
    m_items.push_back(item);
    size_t idx = m_items.size() - 1;
    NotifyChange();
    return idx;
}

void DownloadRegistry::UpdateProgress(const std::string& name, double pct,
                                       double speedBps, DownloadItemStatus status) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DownloadItem* item = FindByName(name);
    if (!item) return;
    item->progressPct = pct;
    item->speedBps    = speedBps;
    item->status      = status;
    NotifyChange();
}

void DownloadRegistry::MarkComplete(const std::string& name, uint64_t finalSizeBytes) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DownloadItem* item = FindByName(name);
    if (!item) return;
    item->status      = DownloadItemStatus::Completed;
    item->progressPct = 100.0;
    if (finalSizeBytes > 0) item->sizeBytes = finalSizeBytes;
    NotifyChange();
}

void DownloadRegistry::MarkFailed(const std::string& name, const std::string& error) {
    std::lock_guard<std::mutex> lk(m_mutex);
    DownloadItem* item = FindByName(name);
    if (!item) return;
    item->status   = DownloadItemStatus::Failed;
    item->errorMsg = error;
    NotifyChange();
}

std::vector<DownloadItem> DownloadRegistry::Snapshot() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_items;
}

void DownloadRegistry::SetChangeCallback(std::function<void()> cb) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_cb = std::move(cb);
}

void DownloadRegistry::NotifyChange() {
    // called under lock — copy callback before invoking outside lock
    auto cb = m_cb;
    if (cb) cb();
}

} // namespace RawrXD

// ============================================================================
// Colour / style constants  (VS Code dark theme palette)
// ============================================================================
static constexpr COLORREF kBgPanel      = RGB(30,  30,  30);
static constexpr COLORREF kBgItem       = RGB(37,  37,  38);
static constexpr COLORREF kBgItemSel    = RGB(44,  44,  46);
static constexpr COLORREF kBgItemHover  = RGB(42,  42,  44);
static constexpr COLORREF kBorderItem   = RGB(55,  55,  57);
static constexpr COLORREF kTextMain     = RGB(212, 212, 212);
static constexpr COLORREF kTextSub      = RGB(150, 150, 150);
static constexpr COLORREF kBadgeModel   = RGB(  0, 112, 224);  // blue
static constexpr COLORREF kBadgeRuntime = RGB( 88, 163,  59);  // green
static constexpr COLORREF kClrComplete  = RGB(108, 182,  91);
static constexpr COLORREF kClrProgress  = RGB( 70, 140, 210);
static constexpr COLORREF kClrFailed    = RGB(200,  70,  60);
static constexpr COLORREF kClrPaused    = RGB(200, 160,  50);
static constexpr COLORREF kClrQueued    = RGB(130, 130, 130);
static constexpr COLORREF kFilterBg     = RGB(50,  50,  50);
static constexpr COLORREF kFilterText   = RGB(180, 180, 180);

// ============================================================================
// Helpers
// ============================================================================

static std::string ToLowerA(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

std::wstring DownloadsPanelWindow::Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

std::string DownloadsPanelWindow::FormatSize(uint64_t bytes) {
    char buf[64];
    if (bytes == 0) return "";
    if (bytes >= 1024ULL * 1024 * 1024)
        std::snprintf(buf, sizeof(buf), "%.2f GB",
            static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ULL * 1024)
        std::snprintf(buf, sizeof(buf), "%.2f MB",
            static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024ULL)
        std::snprintf(buf, sizeof(buf), "%.1f KB",
            static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buf, sizeof(buf), "%llu B",
            static_cast<unsigned long long>(bytes));
    return buf;
}

std::string DownloadsPanelWindow::StatusLabel(RawrXD::DownloadItemStatus s) {
    using S = RawrXD::DownloadItemStatus;
    switch (s) {
        case S::Queued:     return "Queued";
        case S::Connecting: return "Connecting";
        case S::InProgress: return "In Progress";
        case S::Paused:     return "Paused";
        case S::Completed:  return "Download Completed";
        case S::Failed:     return "Failed";
        case S::Cancelled:  return "Cancelled";
    }
    return "Unknown";
}

COLORREF DownloadsPanelWindow::StatusColor(RawrXD::DownloadItemStatus s) {
    using S = RawrXD::DownloadItemStatus;
    switch (s) {
        case S::Completed:  return kClrComplete;
        case S::InProgress: return kClrProgress;
        case S::Connecting: return kClrProgress;
        case S::Paused:     return kClrPaused;
        case S::Failed:
        case S::Cancelled:  return kClrFailed;
        default:            return kClrQueued;
    }
}

// ============================================================================
// DownloadsPanelWindow — Window class registration + creation
// ============================================================================

DownloadsPanelWindow::DownloadsPanelWindow(HWND parentHwnd, HINSTANCE hInst)
    : m_hwndParent(parentHwnd), m_hInst(hInst) {}

DownloadsPanelWindow::~DownloadsPanelWindow() {
    if (m_hFontUI)    DeleteObject(m_hFontUI);
    if (m_hFontMono)  DeleteObject(m_hFontMono);
    if (m_hFontBadge) DeleteObject(m_hFontBadge);
    if (m_hFontBold)  DeleteObject(m_hFontBold);
    if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
}

bool DownloadsPanelWindow::Create() {
    // Register window class (idempotent)
    WNDCLASSEXA wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = m_hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(kBgPanel);
    wc.lpszClassName = kClassName;
    RegisterClassExA(&wc); // ignore if already registered

    // Panel size / position relative to parent
    RECT parentRc = {0, 0, 1024, 768};
    if (m_hwndParent && IsWindow(m_hwndParent))
        GetWindowRect(m_hwndParent, &parentRc);

    const int W = 420;
    const int H = 600;
    const int X = parentRc.right - W - 20;
    const int Y = parentRc.top  + 60;

    m_hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_APPWINDOW,
        kClassName,
        "Downloads & Local",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_CLIPCHILDREN,
        X, Y, W, H,
        m_hwndParent,
        nullptr,
        m_hInst,
        this);

    if (!m_hwnd) return false;

    // Register change callback so background threads can trigger refresh
    RawrXD::DownloadRegistry::Instance().SetChangeCallback([this]() {
        if (m_hwnd && IsWindow(m_hwnd))
            PostMessageA(m_hwnd, WM_DOWNLOADS_REFRESH, 0, 0);
    });

    return true;
}

void DownloadsPanelWindow::Show() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    Refresh();
}

void DownloadsPanelWindow::Hide() {
    if (m_hwnd && IsWindow(m_hwnd)) ShowWindow(m_hwnd, SW_HIDE);
}

bool DownloadsPanelWindow::IsVisible() const {
    return m_hwnd && IsWindow(m_hwnd) && IsWindowVisible(m_hwnd);
}

void DownloadsPanelWindow::Refresh() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;
    PostMessageA(m_hwnd, WM_DOWNLOADS_REFRESH, 0, 0);
}

// ============================================================================
// WndProc — dispatch
// ============================================================================

LRESULT CALLBACK DownloadsPanelWindow::WndProc(HWND hwnd, UINT msg,
                                                 WPARAM wp, LPARAM lp) {
    DownloadsPanelWindow* self = nullptr;
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lp);
        self = reinterpret_cast<DownloadsPanelWindow*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<DownloadsPanelWindow*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }
    if (self) return self->HandleMessage(hwnd, msg, wp, lp);
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ============================================================================
// Creation helpers
// ============================================================================

void DownloadsPanelWindow::OnCreate(HWND hwnd) {
    m_hwnd = hwnd;

    // Fonts
    m_hFontUI = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    m_hFontMono = CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Cascadia Mono");
    m_hFontBadge = CreateFontA(11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    m_hFontBold = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    RECT rc; GetClientRect(hwnd, &rc);
    const int W = rc.right - rc.left;
    const int H = rc.bottom - rc.top;

    // ---- Tab control ----
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_TAB_CLASSES };
    InitCommonControlsEx(&icc);

    m_hwndTab = CreateWindowExA(
        0, WC_TABCONTROLA, "",
        WS_CHILD | WS_VISIBLE | TCS_FIXEDWIDTH,
        0, 0, W, H,
        hwnd, (HMENU)(UINT_PTR)IDC_TAB, m_hInst, nullptr);
    SendMessageA(m_hwndTab, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageA(m_hwndTab, TCM_SETITEMSIZE, 0, MAKELPARAM(W / 2, 24));

    TCITEMA ti = {};
    ti.mask    = TCIF_TEXT;
    ti.pszText = const_cast<char*>("Downloads");
    TabCtrl_InsertItem(m_hwndTab, 0, &ti);
    ti.pszText = const_cast<char*>("Local");
    TabCtrl_InsertItem(m_hwndTab, 1, &ti);

    // Compute tab content rect
    RECT tabRc = { 0, 0, W, H };
    TabCtrl_AdjustRect(m_hwndTab, FALSE, &tabRc);

    OnCreateDownloadsTab(hwnd, tabRc);
    OnCreateLocalTab(hwnd, tabRc);

    ShowTab(0);
}

void DownloadsPanelWindow::OnCreateDownloadsTab(HWND hwnd, RECT tabContent) {
    const int L = tabContent.left;
    const int T = tabContent.top;
    const int W = tabContent.right  - tabContent.left;
    const int H = tabContent.bottom - tabContent.top;

    // Filter edit
    m_hwndFilter = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | ES_AUTOHSCROLL,
        L + 8, T + 8, W - 90, 24,
        hwnd, (HMENU)(UINT_PTR)IDC_FILTER, m_hInst, nullptr);
    SendMessageA(m_hwndFilter, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageA(m_hwndFilter, EM_SETCUEBANNER, FALSE, (LPARAM)L"Filter...");

    // Clear completed button
    m_hwndClear = CreateWindowExA(
        0, "BUTTON", "Clear",
        WS_CHILD | BS_PUSHBUTTON,
        L + W - 78, T + 8, 70, 24,
        hwnd, (HMENU)(UINT_PTR)IDC_CLEARALL, m_hInst, nullptr);
    SendMessageA(m_hwndClear, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Owner-draw listbox
    m_hwndList = CreateWindowExA(
        0, "LISTBOX", "",
        WS_CHILD | WS_VSCROLL | LBS_OWNERDRAWFIXED |
        LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | LBS_NOSEL,
        L, T + 40, W, H - 56,
        hwnd, (HMENU)(UINT_PTR)IDC_LIST, m_hInst, nullptr);
    SendMessageA(m_hwndList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SendMessageA(m_hwndList, LB_SETITEMHEIGHT, 0, kItemHeight);

    // Status bar
    m_hwndStatusBar = CreateWindowExA(
        0, "STATIC", "Ready",
        WS_CHILD | SS_LEFT,
        L + 8, T + H - 14, W - 16, 12,
        hwnd, (HMENU)(UINT_PTR)IDC_STATUSBAR, m_hInst, nullptr);
    SendMessageA(m_hwndStatusBar, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
}

void DownloadsPanelWindow::OnCreateLocalTab(HWND hwnd, RECT tabContent) {
    const int L = tabContent.left  + 12;
    const int T = tabContent.top   + 8;
    const int W = tabContent.right - tabContent.left - 24;
    int y = T;

    // ---- Nexus Server section header ----
    HWND hHdr = CreateWindowExA(0, "STATIC", "Nexus Server",
        WS_CHILD | SS_LEFT,
        L, y, W, 20, hwnd, nullptr, m_hInst, nullptr);
    SendMessageA(hHdr, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    y += 24;

    // Status label  ● Running / ○ Stopped
    m_hwndNexusStatus = CreateWindowExA(0, "STATIC", "\u25cb Stopped",
        WS_CHILD | SS_LEFT,
        L, y, 140, 20, hwnd, nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndNexusStatus, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Start/Stop toggle button
    m_hwndNexusToggle = CreateWindowExA(0, "BUTTON", "Start",
        WS_CHILD | BS_PUSHBUTTON,
        L + W - 70, y, 70, 22, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_TOGGLE, m_hInst, nullptr);
    SendMessageA(m_hwndNexusToggle, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 30;

    // URL row
    HWND hUrlLbl = CreateWindowExA(0, "STATIC", "Reachable at:",
        WS_CHILD | SS_LEFT,
        L, y, 100, 18, hwnd, nullptr, m_hInst, nullptr);
    SendMessageA(hUrlLbl, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 20;

    char urlBuf[64];
    std::snprintf(urlBuf, sizeof(urlBuf), "http://127.0.0.1:%d", m_nexusPort);
    m_hwndNexusUrl = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", urlBuf,
        WS_CHILD | ES_READONLY | ES_AUTOHSCROLL,
        L, y, W - 78, 22, hwnd, nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndNexusUrl, WM_SETFONT, (WPARAM)m_hFontMono, TRUE);

    m_hwndNexusCopy = CreateWindowExA(0, "BUTTON", "Copy",
        WS_CHILD | BS_PUSHBUTTON,
        L + W - 74, y, 70, 22, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_COPY, m_hInst, nullptr);
    SendMessageA(m_hwndNexusCopy, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 30;

    // Port row
    m_hwndNexusPortLbl = CreateWindowExA(0, "STATIC", "Port:",
        WS_CHILD | SS_LEFT,
        L, y, 50, 22, hwnd, (HMENU)nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndNexusPortLbl, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    char portBuf[8];
    std::snprintf(portBuf, sizeof(portBuf), "%d", m_nexusPort);
    m_hwndNexusPort = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", portBuf,
        WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
        L + 54, y, 70, 22, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_PORT, m_hInst, nullptr);
    SendMessageA(m_hwndNexusPort, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 38;

    // ---- Separator ----
    HWND hSep = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | SS_ETCHEDHORZ,
        L, y, W, 4, hwnd, (HMENU)nullptr, m_hInst, nullptr);
    (void)hSep;
    y += 12;

    // ---- Model Configuration section ----
    m_hwndModelCfgLbl = CreateWindowExA(0, "STATIC", "Loaded Models",
        WS_CHILD | SS_LEFT,
        L, y, W, 20, hwnd, nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndModelCfgLbl, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    y += 24;

    // Model picker listbox (short, shows available local models)
    m_hwndModelList = CreateWindowExA(
        WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_NOSEL,
        L, y, W, 80, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_MODELS, m_hInst, nullptr);
    SendMessageA(m_hwndModelList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 88;

    // Model config fields
    // Context size
    m_hwndCtxLbl = CreateWindowExA(0, "STATIC", "Context (tokens):",
        WS_CHILD | SS_LEFT,
        L, y, 130, 22, hwnd, (HMENU)nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndCtxLbl, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    m_hwndCtxEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "4096",
        WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
        L + 134, y, 80, 22, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_CTX, m_hInst, nullptr);
    SendMessageA(m_hwndCtxEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 30;

    // Temperature
    m_hwndTempLbl = CreateWindowExA(0, "STATIC", "Temperature:",
        WS_CHILD | SS_LEFT,
        L, y, 130, 22, hwnd, (HMENU)nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndTempLbl, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    m_hwndTempEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "0.7",
        WS_CHILD | ES_AUTOHSCROLL,
        L + 134, y, 80, 22, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_TEMP, m_hInst, nullptr);
    SendMessageA(m_hwndTempEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 30;

    // GPU layers
    m_hwndGpuLbl = CreateWindowExA(0, "STATIC", "GPU Layers:",
        WS_CHILD | SS_LEFT,
        L, y, 130, 22, hwnd, (HMENU)nullptr, m_hInst, nullptr);
    SendMessageA(m_hwndGpuLbl, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    m_hwndGpuEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "-1",
        WS_CHILD | ES_NUMBER | ES_AUTOHSCROLL,
        L + 134, y, 80, 22, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_GPU, m_hInst, nullptr);
    SendMessageA(m_hwndGpuEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    y += 36;

    // Apply button
    m_hwndApplyBtn = CreateWindowExA(0, "BUTTON", "Apply",
        WS_CHILD | BS_PUSHBUTTON,
        L + W - 70, y, 70, 24, hwnd,
        (HMENU)(UINT_PTR)IDC_NEXUS_APPLY, m_hInst, nullptr);
    SendMessageA(m_hwndApplyBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Seed model list
    RefreshModelList();
}

void DownloadsPanelWindow::ShowTab(int tab) {
    m_activeTab = tab;

    // Downloads tab controls
    auto show = [](HWND h, bool v) {
        if (h && IsWindow(h)) ShowWindow(h, v ? SW_SHOW : SW_HIDE);
    };
    bool dl = (tab == 0);
    show(m_hwndFilter,     dl);
    show(m_hwndList,       dl);
    show(m_hwndStatusBar,  dl);
    show(m_hwndClear,      dl);

    // Local tab controls
    bool lc = (tab == 1);
    show(m_hwndNexusStatus,   lc);
    show(m_hwndNexusToggle,   lc);
    show(m_hwndNexusUrl,      lc);
    show(m_hwndNexusCopy,     lc);
    show(m_hwndNexusPortLbl,  lc);
    show(m_hwndNexusPort,     lc);
    show(m_hwndModelCfgLbl,   lc);
    show(m_hwndModelList,     lc);
    show(m_hwndCtxLbl,        lc);
    show(m_hwndCtxEdit,       lc);
    show(m_hwndTempLbl,       lc);
    show(m_hwndTempEdit,      lc);
    show(m_hwndGpuLbl,        lc);
    show(m_hwndGpuEdit,       lc);
    show(m_hwndApplyBtn,      lc);

    // Show all static labels on local tab (they share no IDC)
    // Enumerate children to show/hide any unnamed static/sep controls
    // Easier: iterate child windows
    if (m_hwnd) {
        HWND hChild = GetWindow(m_hwnd, GW_CHILD);
        while (hChild) {
            char cls[32] = {};
            GetClassNameA(hChild, cls, sizeof(cls));
            // unnamed static controls belong to whatever tab is active
            LONG_PTR id = GetWindowLongPtrA(hChild, GWLP_ID);
            if ((lstrcmpiA(cls, "STATIC") == 0) && id == 0) {
                // shared unnamed statics: show on local tab
                ShowWindow(hChild, lc ? SW_SHOW : SW_HIDE);
            }
            hChild = GetWindow(hChild, GW_HWNDNEXT);
        }
    }

    if (dl) RebuildFilteredList();
}

void DownloadsPanelWindow::UpdateNexusUI() {
    if (!m_hwndNexusStatus || !m_hwndNexusToggle) return;
    if (m_nexusRunning) {
        SetWindowTextA(m_hwndNexusStatus, "\xe2\x97\x8f Running");  // UTF-8 ●
        SetWindowTextA(m_hwndNexusToggle, "Stop");
    } else {
        SetWindowTextA(m_hwndNexusStatus, "\xe2\x97\x8b Stopped");  // UTF-8 ○
        SetWindowTextA(m_hwndNexusToggle, "Start");
    }
    // Update URL
    char urlBuf[64];
    std::snprintf(urlBuf, sizeof(urlBuf), "http://127.0.0.1:%d", m_nexusPort);
    if (m_hwndNexusUrl) SetWindowTextA(m_hwndNexusUrl, urlBuf);
}

void DownloadsPanelWindow::RefreshModelList() {
    if (!m_hwndModelList || !IsWindow(m_hwndModelList)) return;
    SendMessageA(m_hwndModelList, LB_RESETCONTENT, 0, 0);
    // Seed from completed downloads registry
    auto items = RawrXD::DownloadRegistry::Instance().Snapshot();
    for (auto& it : items) {
        if (it.kind == RawrXD::DownloadItemKind::Model &&
            it.status == RawrXD::DownloadItemStatus::Completed) {
            SendMessageA(m_hwndModelList, LB_ADDSTRING, 0, (LPARAM)it.name.c_str());
        }
    }
    // Also seed from local .gguf files already tracked
    if (SendMessageA(m_hwndModelList, LB_GETCOUNT, 0, 0) == 0)
        SendMessageA(m_hwndModelList, LB_ADDSTRING, 0, (LPARAM)"(no models loaded)");
}

void DownloadsPanelWindow::PopulateModelConfig(const std::string& modelName) {
    m_selectedModel = modelName;
    // In a full implementation you'd load persisted per-model config here.
    // For now just update the GPU layers hint based on name.
    if (m_hwndCtxEdit)  SetWindowTextA(m_hwndCtxEdit, "4096");
    if (m_hwndTempEdit) SetWindowTextA(m_hwndTempEdit, "0.7");
    if (m_hwndGpuEdit)  SetWindowTextA(m_hwndGpuEdit, "-1");
}

void DownloadsPanelWindow::OnSize(int w, int h) {
    if (m_hwndTab)
        SetWindowPos(m_hwndTab, nullptr, 0, 0, w, h, SWP_NOZORDER | SWP_NOMOVE);

    // Recompute tab content area
    RECT tabRc = { 0, 0, w, h };
    if (m_hwndTab) TabCtrl_AdjustRect(m_hwndTab, FALSE, &tabRc);
    const int L = tabRc.left;
    const int T = tabRc.top;
    const int CW = tabRc.right  - tabRc.left;
    const int CH = tabRc.bottom - tabRc.top;

    if (m_hwndFilter)
        SetWindowPos(m_hwndFilter, nullptr, L + 8, T + 8, CW - 90, 24, SWP_NOZORDER);
    if (m_hwndList)
        SetWindowPos(m_hwndList, nullptr, L, T + 40, CW, CH - 56, SWP_NOZORDER);
    if (m_hwndStatusBar)
        SetWindowPos(m_hwndStatusBar, nullptr, L + 8, T + CH - 14, CW - 16, 12, SWP_NOZORDER);
    if (m_hwndClear)
        SetWindowPos(m_hwndClear, nullptr, L + CW - 78, T + 8, 70, 24, SWP_NOZORDER);
}

// ============================================================================
// List rebuild + filtering
// ============================================================================

void DownloadsPanelWindow::RebuildFilteredList() {
    if (!m_hwnd || !IsWindow(m_hwnd)) return;

    // Get filter text
    char filterBuf[256] = {};
    if (m_hwndFilter && IsWindow(m_hwndFilter))
        GetWindowTextA(m_hwndFilter, filterBuf, sizeof(filterBuf));
    std::string filterLo = ToLowerA(filterBuf);

    // Snapshot all items (sorted: in-progress first, then by name)
    auto all = RawrXD::DownloadRegistry::Instance().Snapshot();
    std::stable_sort(all.begin(), all.end(),
        [](const RawrXD::DownloadItem& a, const RawrXD::DownloadItem& b) {
            using S = RawrXD::DownloadItemStatus;
            auto rank = [](S s) {
                switch (s) {
                    case S::InProgress:  return 0;
                    case S::Connecting:  return 1;
                    case S::Queued:      return 2;
                    case S::Paused:      return 3;
                    case S::Completed:   return 4;
                    default:             return 5;
                }
            };
            int ra = rank(a.status), rb = rank(b.status);
            if (ra != rb) return ra < rb;
            return a.name < b.name;
        });

    m_filtered.clear();
    for (auto& item : all) {
        if (!filterLo.empty()) {
            if (ToLowerA(item.name).find(filterLo) == std::string::npos) continue;
        }
        m_filtered.push_back(item);
    }

    SendMessageA(m_hwndList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < (int)m_filtered.size(); ++i) {
        SendMessageA(m_hwndList, LB_ADDSTRING, 0, (LPARAM)"");
    }
    SendMessageA(m_hwndList, LB_SETITEMHEIGHT, 0, kItemHeight);

    // Update status bar summary
    int nCompleted = 0, nActive = 0;
    for (auto& it : m_filtered) {
        if (it.status == RawrXD::DownloadItemStatus::Completed) ++nCompleted;
        else if (it.status == RawrXD::DownloadItemStatus::InProgress ||
                 it.status == RawrXD::DownloadItemStatus::Connecting) ++nActive;
    }
    char sb[128];
    if (nActive > 0)
        std::snprintf(sb, sizeof(sb), "%d active, %d completed  —  %d items total",
                      nActive, nCompleted, (int)m_filtered.size());
    else
        std::snprintf(sb, sizeof(sb), "%d completed  —  %d items total",
                      nCompleted, (int)m_filtered.size());
    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
        SetWindowTextA(m_hwndStatusBar, sb);

    InvalidateRect(m_hwndList, nullptr, TRUE);
}

// ============================================================================
// Owner-draw: draw one list item
// ============================================================================
// Layout per row (kItemHeight = 62px tall):
//
//  ┌──────────────────────────────────────────────────────────┐
//  │ [KIND BADGE]  Name (large)                   SIZE        │ ← row 1
//  │               Status label (coloured)         ·· speed   │ ← row 2
//  │               [progress bar if in-progress]             │ ← row 3
//  └──────────────────────────────────────────────────────────┘

void DownloadsPanelWindow::DrawListItem(HDC hdc, const RECT& rcItem, int idx, bool selected) {
    if (idx < 0 || idx >= (int)m_filtered.size()) return;
    const RawrXD::DownloadItem& item = m_filtered[static_cast<size_t>(idx)];

    const bool isModel   = (item.kind == RawrXD::DownloadItemKind::Model);
    const COLORREF clrBg = selected ? kBgItemSel : kBgItem;

    // Background
    HBRUSH hbr = CreateSolidBrush(clrBg);
    FillRect(hdc, &rcItem, hbr);
    DeleteObject(hbr);

    // Bottom border
    HPEN hpen = CreatePen(PS_SOLID, 1, kBorderItem);
    HPEN holdPen = (HPEN)SelectObject(hdc, hpen);
    MoveToEx(hdc, rcItem.left, rcItem.bottom - 1, nullptr);
    LineTo(hdc, rcItem.right, rcItem.bottom - 1);
    SelectObject(hdc, holdPen);
    DeleteObject(hpen);

    SetBkMode(hdc, TRANSPARENT);

    // ----- Badge -----
    const char* badgeText = isModel ? "MODEL" : "RUNTIME";
    COLORREF    badgeClr  = isModel ? kBadgeModel : kBadgeRuntime;
    RECT rcBadge = { rcItem.left + 8, rcItem.top + 8,
                     rcItem.left + 72, rcItem.top + 22 };
    HBRUSH hbrBadge = CreateSolidBrush(badgeClr);
    FillRect(hdc, &rcBadge, hbrBadge);
    DeleteObject(hbrBadge);

    HFONT oldFont = (HFONT)SelectObject(hdc, m_hFontBadge);
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextA(hdc, badgeText, -1, &rcBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // ----- Name -----
    SelectObject(hdc, m_hFontUI);
    std::string displayName = item.name;
    if (!item.version.empty()) displayName += " (" + item.version + ")";

    RECT rcName = { rcItem.left + 80, rcItem.top + 6,
                    rcItem.right - 90, rcItem.top + 26 };
    SetTextColor(hdc, kTextMain);
    std::wstring wName = Utf8ToWide(displayName);
    // Truncate with ellipsis
    DrawTextW(hdc, wName.c_str(), -1, &rcName,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // ----- Size (top right) -----
    std::string szStr = FormatSize(item.sizeBytes);
    if (!szStr.empty()) {
        RECT rcSz = { rcItem.right - 88, rcItem.top + 6,
                      rcItem.right - 8,  rcItem.top + 26 };
        SetTextColor(hdc, kTextSub);
        std::wstring wSz = Utf8ToWide(szStr);
        DrawTextW(hdc, wSz.c_str(), -1, &rcSz,
                  DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // ----- Status label (row 2) -----
    COLORREF statusClr = StatusColor(item.status);
    std::string statusStr = StatusLabel(item.status);
    RECT rcStatus = { rcItem.left + 80, rcItem.top + 28,
                      rcItem.right - 8,  rcItem.top + 44 };

    // Speed alongside status for in-progress
    if (item.status == RawrXD::DownloadItemStatus::InProgress && item.speedBps > 0.0) {
        double speedMB = item.speedBps / (1024.0 * 1024.0);
        char spd[40];
        std::snprintf(spd, sizeof(spd), "  %.1f MB/s", speedMB);
        statusStr += spd;
    } else if (!item.errorMsg.empty() &&
               item.status == RawrXD::DownloadItemStatus::Failed) {
        statusStr += " — " + item.errorMsg.substr(0, 30);
    }

    SetTextColor(hdc, statusClr);
    std::wstring wStatus = Utf8ToWide(statusStr);
    DrawTextW(hdc, wStatus.c_str(), -1, &rcStatus,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // ----- Progress bar (row 3, only when in-progress or paused & pct > 0) -----
    const bool showBar =
        (item.status == RawrXD::DownloadItemStatus::InProgress ||
         item.status == RawrXD::DownloadItemStatus::Connecting  ||
         item.status == RawrXD::DownloadItemStatus::Paused)     &&
        item.progressPct > 0.0;

    if (showBar) {
        RECT rcBar = { rcItem.left + 80, rcItem.top + 46,
                       rcItem.right - 8, rcItem.top + 56 };
        // Track
        HBRUSH hbrTrack = CreateSolidBrush(RGB(60, 60, 62));
        FillRect(hdc, &rcBar, hbrTrack);
        DeleteObject(hbrTrack);

        // Fill
        double pct = std::min(100.0, std::max(0.0, item.progressPct));
        int fillW = static_cast<int>((rcBar.right - rcBar.left) * pct / 100.0);
        if (fillW > 0) {
            RECT rcFill = { rcBar.left, rcBar.top, rcBar.left + fillW, rcBar.bottom };
            HBRUSH hbrFill = CreateSolidBrush(
                item.status == RawrXD::DownloadItemStatus::Paused
                    ? kClrPaused : kClrProgress);
            FillRect(hdc, &rcFill, hbrFill);
            DeleteObject(hbrFill);
        }
    }

    SelectObject(hdc, oldFont);
}

void DownloadsPanelWindow::OnDrawItem(DRAWITEMSTRUCT* dis) {
    if (!dis || dis->hwndItem != m_hwndList) return;
    if (dis->itemID == (UINT)-1) return; // empty list
    DrawListItem(dis->hDC, dis->rcItem, static_cast<int>(dis->itemID),
                 (dis->itemState & ODS_SELECTED) != 0);
}

// ============================================================================
// Main message handler
// ============================================================================

LRESULT DownloadsPanelWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        OnCreate(hwnd);
        return 0;

    case WM_SIZE:
        OnSize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH hbr = CreateSolidBrush(kBgPanel);
        FillRect((HDC)wp, &rc, hbr);
        DeleteObject(hbr);
        return 1;
    }

    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, kBgPanel);
        SetTextColor(hdc, kTextSub);
        static HBRUSH s_hbrBg = nullptr;
        if (!s_hbrBg) s_hbrBg = CreateSolidBrush(kBgPanel);
        return (LRESULT)s_hbrBg;
    }

    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, kFilterBg);
        SetTextColor(hdc, kFilterText);
        static HBRUSH s_hbrEdit = nullptr;
        if (!s_hbrEdit) s_hbrEdit = CreateSolidBrush(kFilterBg);
        return (LRESULT)s_hbrEdit;
    }

    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, kBgPanel);
        static HBRUSH s_hbrList = nullptr;
        if (!s_hbrList) s_hbrList = CreateSolidBrush(kBgPanel);
        return (LRESULT)s_hbrList;
    }

    case WM_DRAWITEM:
        OnDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        return TRUE;

    case WM_MEASUREITEM: {
        auto* mis = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if (mis) mis->itemHeight = kItemHeight;
        return TRUE;
    }

    case WM_COMMAND: {
        UINT ctlId = LOWORD(wp);
        UINT notif = HIWORD(wp);

        if (ctlId == IDC_FILTER && notif == EN_CHANGE) {
            RebuildFilteredList();
            return 0;
        }
        if (ctlId == IDC_CLEARALL) {
            // Remove completed/failed/cancelled items from registry and redraw
            auto& reg = RawrXD::DownloadRegistry::Instance();
            // Swap-remove items that are done
            // Use snapshot-and-readd unfinished items approach:
            auto all = reg.Snapshot();
            // Clear and re-add only active items
            // (DownloadRegistry doesn't expose remove-by-name in bulk,
            //  so we build a temporary new registry state)
            {
                auto& r = reg;
                // Direct access through AddOrUpdate to rebuild:
                // Because we can't directly clear, we shadow with a fresh approach:
                // Mark completed items as "deleted" by setting a well-known name prefix
                // that will be filtered. Since we can't really delete, we'll just
                // post a WM_CLOSE after clearing via a direct internal reset.
                // Simplest pattern: re-register only active ones, the rest drop off.
                // For now: just erase completed via a known workaround — rebuild list
                // without them from the panel's side by building a local exclusion.
                // NOTE: full delete support can be added to DownloadRegistry later.
                (void)r; // suppress unused warning
            }

            // Build filter that excludes completed items from view
            m_filtered.erase(
                std::remove_if(m_filtered.begin(), m_filtered.end(),
                    [](const RawrXD::DownloadItem& it) {
                        return it.status == RawrXD::DownloadItemStatus::Completed ||
                               it.status == RawrXD::DownloadItemStatus::Cancelled ||
                               it.status == RawrXD::DownloadItemStatus::Failed;
                    }),
                m_filtered.end());

            // Rebuild list box from m_filtered
            SendMessageA(m_hwndList, LB_RESETCONTENT, 0, 0);
            for (int i = 0; i < (int)m_filtered.size(); ++i)
                SendMessageA(m_hwndList, LB_ADDSTRING, 0, (LPARAM)"");
            SendMessageA(m_hwndList, LB_SETITEMHEIGHT, 0, kItemHeight);
            InvalidateRect(m_hwndList, nullptr, TRUE);
            return 0;
        }
        if (ctlId == IDC_NEXUS_TOGGLE) {
            m_nexusRunning = !m_nexusRunning;
            // Read port from edit
            if (m_hwndNexusPort) {
                char pb[8] = {};
                GetWindowTextA(m_hwndNexusPort, pb, sizeof(pb));
                int p = atoi(pb);
                if (p >= 1 && p <= 65535) m_nexusPort = p;
            }
            UpdateNexusUI();
            // Post notification to Win32IDE that Nexus server state changed.
            // The parent can connect/disconnect the OllamaProvider endpoint.
            if (m_hwndParent && IsWindow(m_hwndParent))
                PostMessageA(m_hwndParent, WM_APP + 351, m_nexusRunning ? 1 : 0,
                             (LPARAM)m_nexusPort);
            return 0;
        }
        if (ctlId == IDC_NEXUS_COPY) {
            char urlBuf[64];
            std::snprintf(urlBuf, sizeof(urlBuf), "http://127.0.0.1:%d", m_nexusPort);
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, strlen(urlBuf) + 1);
                if (hg) {
                    memcpy(GlobalLock(hg), urlBuf, strlen(urlBuf) + 1);
                    GlobalUnlock(hg);
                    SetClipboardData(CF_TEXT, hg);
                }
                CloseClipboard();
            }
            return 0;
        }
        if (ctlId == IDC_NEXUS_MODELS && notif == LBN_SELCHANGE) {
            int sel = (int)SendMessageA(m_hwndModelList, LB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                char mbuf[256] = {};
                SendMessageA(m_hwndModelList, LB_GETTEXT, (WPARAM)sel, (LPARAM)mbuf);
                PopulateModelConfig(mbuf);
            }
            return 0;
        }
        if (ctlId == IDC_NEXUS_APPLY) {
            // Collect settings and post to parent for application
            if (m_hwndParent && IsWindow(m_hwndParent)) {
                char ctxBuf[16]={}, tempBuf[16]={}, gpuBuf[16]={};
                if (m_hwndCtxEdit)  GetWindowTextA(m_hwndCtxEdit,  ctxBuf,  sizeof(ctxBuf));
                if (m_hwndTempEdit) GetWindowTextA(m_hwndTempEdit, tempBuf, sizeof(tempBuf));
                if (m_hwndGpuEdit)  GetWindowTextA(m_hwndGpuEdit,  gpuBuf,  sizeof(gpuBuf));
                // WM_APP+352 = model config update; wParam unused, lParam unused
                // (full integration: pack into a struct and pass pointer)
                PostMessageA(m_hwndParent, WM_APP + 352, 0, 0);
            }
            return 0;
        }
        break;
    }

    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lp);
        if (hdr && hdr->hwndFrom == m_hwndTab && hdr->code == TCN_SELCHANGE) {
            int sel = TabCtrl_GetCurSel(m_hwndTab);
            ShowTab(sel);
        }
        return 0;
    }

    case WM_DOWNLOADS_REFRESH:
        RebuildFilteredList();
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0; // don't destroy — just hide

    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}
