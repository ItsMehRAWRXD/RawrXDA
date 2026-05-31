#pragma once
// ============================================================================
// Win32IDE_DownloadsPanel.h — LM Studio-style Downloads panel
// ============================================================================
// Tracks all model and runtime downloads (in progress + completed).
// Presents a filterable list with per-item name, badge (MODEL / RUNTIME),
// status, and file size — identical UX concept to LM Studio's Downloads view.
// ============================================================================

#ifndef RAWRXD_WIN32IDE_DOWNLOADS_PANEL_H
#define RAWRXD_WIN32IDE_DOWNLOADS_PANEL_H

#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>

#pragma comment(lib, "comctl32.lib")
#include <commctrl.h>

namespace RawrXD {

enum class DownloadItemKind {
    Model,    // .gguf / Ollama model
    Runtime,  // llama.cpp binary, harmony, etc.
};

enum class DownloadItemStatus {
    Queued,
    Connecting,
    InProgress,
    Paused,
    Completed,
    Failed,
    Cancelled,
};

struct DownloadItem {
    DownloadItemKind   kind       = DownloadItemKind::Model;
    DownloadItemStatus status     = DownloadItemStatus::Queued;
    std::string        name;       // e.g. "deepseek/deepseek-r1-0528-qwen3-8b" or "llama.cpp-win-x86_64-avx2 (1.104.2)"
    std::string        version;    // optional version string shown in parens
    uint64_t           sizeBytes  = 0;
    double             progressPct = 0.0;   // 0..100
    double             speedBps    = 0.0;   // bytes/sec
    std::string        destPath;
    std::string        errorMsg;
};

// ============================================================================
// DownloadRegistry — global singleton, thread-safe
// ============================================================================
class DownloadRegistry {
public:
    static DownloadRegistry& Instance();

    // Add or update an entry (by name). Returns the item's index.
    size_t AddOrUpdate(const DownloadItem& item);

    // Update progress for an existing item by name. No-op if not found.
    void UpdateProgress(const std::string& name, double pct, double speedBps, DownloadItemStatus status);

    // Mark complete (status = Completed, pct = 100).
    void MarkComplete(const std::string& name, uint64_t finalSizeBytes);

    // Mark failed.
    void MarkFailed(const std::string& name, const std::string& error);

    // Snapshot copy of all items.
    std::vector<DownloadItem> Snapshot() const;

    // Register a notification callback fired on any change (from any thread).
    // Callback should PostMessage to the panel HWND.
    void SetChangeCallback(std::function<void()> cb);

private:
    DownloadRegistry() = default;
    DownloadRegistry(const DownloadRegistry&) = delete;
    DownloadRegistry& operator=(const DownloadRegistry&) = delete;

    mutable std::mutex           m_mutex;
    std::vector<DownloadItem>    m_items;
    std::function<void()>        m_cb;

    void NotifyChange();
    DownloadItem* FindByName(const std::string& name); // call under lock
};

} // namespace RawrXD

// ============================================================================
// DownloadsPanelWindow — standalone Win32 floating panel
// ============================================================================
// Call DownloadsPanelWindow::Create(parentHwnd) once; Show()/Hide() to toggle.
// Thread-safe: background threads call DownloadRegistry::Instance() directly;
// the panel posts WM_DOWNLOADS_REFRESH to itself to refresh.
// ============================================================================

#define WM_DOWNLOADS_REFRESH (WM_APP + 350)

class DownloadsPanelWindow {
public:
    explicit DownloadsPanelWindow(HWND parentHwnd, HINSTANCE hInst);
    ~DownloadsPanelWindow();

    // Create the panel window (call once after construction).
    bool Create();

    // Show / hide the panel.
    void Show();
    void Hide();
    bool IsVisible() const;

    // Programmatically refresh the list (e.g. after adding a download).
    void Refresh();

    HWND GetHWND() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void OnCreate(HWND hwnd);
    void OnSize(int w, int h);
    void OnDrawItem(DRAWITEMSTRUCT* dis);
    void RebuildFilteredList();
    void DrawListItem(HDC hdc, const RECT& rc, int idx, bool selected);

    // Helpers
    static std::wstring Utf8ToWide(const std::string& s);
    static std::string FormatSize(uint64_t bytes);
    static std::string StatusLabel(RawrXD::DownloadItemStatus s);
    static COLORREF StatusColor(RawrXD::DownloadItemStatus s);

    HWND        m_hwnd           = nullptr;
    HWND        m_hwndParent     = nullptr;
    HINSTANCE   m_hInst          = nullptr;

    // Tab control
    HWND        m_hwndTab        = nullptr;
    int         m_activeTab      = 0;  // 0 = Downloads, 1 = Local

    // Downloads tab
    HWND        m_hwndFilter     = nullptr;  // edit box
    HWND        m_hwndList       = nullptr;  // owner-draw LISTBOX
    HWND        m_hwndStatusBar  = nullptr;  // bottom label
    HWND        m_hwndClear      = nullptr;  // clear completed button

    // Local (Nexus) tab
    HWND        m_hwndLocalPanel    = nullptr;  // container
    HWND        m_hwndNexusStatus   = nullptr;  // status label "● Running" / "○ Stopped"
    HWND        m_hwndNexusToggle   = nullptr;  // Start / Stop button
    HWND        m_hwndNexusUrl      = nullptr;  // read-only edit with URL
    HWND        m_hwndNexusCopy     = nullptr;  // Copy button
    HWND        m_hwndNexusPortLbl  = nullptr;
    HWND        m_hwndNexusPort     = nullptr;  // port edit
    HWND        m_hwndModelCfgLbl   = nullptr;
    HWND        m_hwndModelList     = nullptr;  // listbox of available models
    HWND        m_hwndModelCfgPanel = nullptr;  // model config area
    HWND        m_hwndCtxLbl        = nullptr;
    HWND        m_hwndCtxEdit       = nullptr;
    HWND        m_hwndTempLbl       = nullptr;
    HWND        m_hwndTempEdit      = nullptr;
    HWND        m_hwndGpuLbl        = nullptr;
    HWND        m_hwndGpuEdit       = nullptr;
    HWND        m_hwndApplyBtn      = nullptr;

    bool        m_nexusRunning   = false;
    int         m_nexusPort      = 1234;
    std::string m_selectedModel;

    HFONT       m_hFontUI        = nullptr;
    HFONT       m_hFontMono      = nullptr;
    HFONT       m_hFontBadge     = nullptr;
    HFONT       m_hFontBold      = nullptr;

    // Filtered snapshot used for rendering (main thread only)
    std::vector<RawrXD::DownloadItem> m_filtered;

    void OnCreateDownloadsTab(HWND hwnd, RECT tabContent);
    void OnCreateLocalTab(HWND hwnd, RECT tabContent);
    void ShowTab(int tab);
    void UpdateNexusUI();
    void RefreshModelList();
    void PopulateModelConfig(const std::string& modelName);

    static constexpr int  kItemHeight      = 62;
    static constexpr UINT IDC_FILTER       = 4010;
    static constexpr UINT IDC_LIST         = 4011;
    static constexpr UINT IDC_STATUSBAR    = 4012;
    static constexpr UINT IDC_CLEARALL     = 4013;
    static constexpr UINT IDC_TAB          = 4014;
    static constexpr UINT IDC_NEXUS_TOGGLE = 4020;
    static constexpr UINT IDC_NEXUS_COPY   = 4021;
    static constexpr UINT IDC_NEXUS_PORT   = 4022;
    static constexpr UINT IDC_NEXUS_MODELS = 4023;
    static constexpr UINT IDC_NEXUS_APPLY  = 4024;
    static constexpr UINT IDC_NEXUS_CTX    = 4025;
    static constexpr UINT IDC_NEXUS_TEMP   = 4026;
    static constexpr UINT IDC_NEXUS_GPU    = 4027;
    static constexpr char kClassName[]     = "RawrXD_DownloadsPanel";
};

#endif // RAWRXD_WIN32IDE_DOWNLOADS_PANEL_H
