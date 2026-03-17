// digestion_gui_widget.h — Pure C++20/Win32 (zero Qt)
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <functional>
#include "digestion_reverse_engineering.h"

/**
 * @class DigestionGuiWidget
 * @brief Win32 dialog for code digestion / reverse-engineering pipeline.
 *
 * Provides: directory browser, option checkboxes, a progress bar,
 * a list-view results table, and start/stop buttons — all native Win32.
 */
class DigestionGuiWidget {
public:
    explicit DigestionGuiWidget(HWND hwndParent = nullptr);
    ~DigestionGuiWidget();

    DigestionGuiWidget(const DigestionGuiWidget&) = delete;
    DigestionGuiWidget& operator=(const DigestionGuiWidget&) = delete;

    /// Show the dialog (non-modal popup).
    void show();

    /// Pre-fill the root directory path.
    void setRootDirectory(const std::string& path);

private:
    static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM);
    INT_PTR handleMessage(HWND, UINT, WPARAM, LPARAM);

    void onInitDialog(HWND hDlg);
    void browseDirectory();
    void startDigestion();
    void stopDigestion();

    void onProgress(int done, int total, int stubs, int percent);
    void onFileScanned(const std::string& path, const std::string& lang, int stubs);
    void onFinished(int totalFiles, int totalStubs, int64_t elapsedMs);

    HWND m_hwndParent     = nullptr;
    HWND m_hDlg           = nullptr;
    HWND m_hwndPathEdit   = nullptr;
    HWND m_hwndBrowseBtn  = nullptr;
    HWND m_hwndApplyFixes = nullptr;
    HWND m_hwndGitMode    = nullptr;
    HWND m_hwndIncremental= nullptr;
    HWND m_hwndProgress   = nullptr;
    HWND m_hwndResultsLV  = nullptr;
    HWND m_hwndStartBtn   = nullptr;
    HWND m_hwndStopBtn    = nullptr;

    DigestionReverseEngineeringSystem* m_digester = nullptr;
    std::string m_rootDir;

    enum {
        IDC_DIG_PATH    = 2001, IDC_DIG_BROWSE  = 2002,
        IDC_DIG_APPLY   = 2003, IDC_DIG_GIT     = 2004,
        IDC_DIG_INCR    = 2005, IDC_DIG_PROG    = 2006,
        IDC_DIG_LIST    = 2007, IDC_DIG_START   = 2008,
        IDC_DIG_STOP    = 2009,
    };
};

