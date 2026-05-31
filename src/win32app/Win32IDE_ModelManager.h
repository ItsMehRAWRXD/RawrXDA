#pragma once
// ============================================================================
// Win32IDE_ModelManager.h — Win32 GUI Model Manager Dialog
// ============================================================================
// Provides a Model Manager dialog for the Win32IDE:
//   - Tab: "Discover" — search HuggingFace / Ollama
//   - Tab: "Downloads" — progress bars, pause/resume
//   - Tab: "Local Models" — list, delete, set as active
// ============================================================================

#ifndef RAWRXD_WIN32IDE_MODEL_MANAGER_H
#define RAWRXD_WIN32IDE_MODEL_MANAGER_H

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <utility>

// Forward declarations
namespace RawrXD {
    class ModelPuller;
    struct ModelEntry;
    struct HFRepoInfo;
    struct HFFileInfo;
    struct PullStatus;
}

// ============================================================================
// ModelManagerDialog
// ============================================================================
class ModelManagerDialog {
public:
    ModelManagerDialog(HWND parentHwnd, HINSTANCE hInstance);
    ~ModelManagerDialog();

    // Show the dialog (modal)
    void Show();

    // Message pump integration — call from WndProc if needed
    static INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Get selected model path (after dialog closes with OK)
    std::string GetSelectedModelPath() const { return m_selectedModelPath; }

private:
    struct SearchCompletion {
        std::vector<RawrXD::HFRepoInfo> results;
        std::string error;
    };

    struct PullCompletion {
        bool success = false;
        std::string filePath;
        std::string error;
    };

    // Dialog creation helpers
    void CreateControls(HWND hwndDlg);
    void CreateDiscoverTab(HWND hwndDlg);
    void CreateDownloadsTab(HWND hwndDlg);
    void CreateLocalTab(HWND hwndDlg);

    // Tab switching
    void OnTabChanged(int tabIndex);
    void ShowTabControls(int tabIndex);

    // Discover tab handlers
    void OnSearchClicked();
    void OnSearchResultSelected();
    void OnListFilesClicked();
    void OnPullFromSearchClicked();
    void PopulateSearchResults(const std::vector<RawrXD::HFRepoInfo>& results);
    void PopulateFileList(const std::vector<RawrXD::HFFileInfo>& files);
    void HandleSearchCompleted(SearchCompletion completion);

    // Downloads tab handlers
    void OnCancelDownloadClicked();
    void UpdateDownloadProgress(const RawrXD::PullStatus& status);
    void HandlePullCompleted(PullCompletion completion);
    void FinalizePullThread();

    // Local tab handlers
    void RefreshLocalModels();
    void OnSetActiveClicked();
    void OnRemoveClicked();
    void OnRegisterClicked();
    void OnModelInfoClicked();
    void PopulateLocalModels(const std::vector<RawrXD::ModelEntry>& models);

    // Background pull
    void StartPull(const std::string& source);

    // Handles
    HWND m_parentHwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwndDlg = nullptr;

    // Tab control
    HWND m_hwndTabCtrl = nullptr;
    int  m_currentTab = 0;

    // Discover tab controls
    HWND m_hwndSearchEdit = nullptr;
    HWND m_hwndSearchBtn = nullptr;
    HWND m_hwndSearchList = nullptr;
    HWND m_hwndFileListView = nullptr;
    HWND m_hwndListFilesBtn = nullptr;
    HWND m_hwndPullBtn = nullptr;
    HWND m_hwndPullSourceEdit = nullptr;

    // Downloads tab controls
    HWND m_hwndDownloadProgress = nullptr;
    HWND m_hwndDownloadStatus = nullptr;
    HWND m_hwndDownloadFile = nullptr;
    HWND m_hwndCancelBtn = nullptr;

    // Local tab controls
    HWND m_hwndLocalListView = nullptr;
    HWND m_hwndSetActiveBtn = nullptr;
    HWND m_hwndRemoveBtn = nullptr;
    HWND m_hwndRegisterBtn = nullptr;
    HWND m_hwndInfoBtn = nullptr;

    // State
    std::string m_selectedModelPath;
    std::string m_selectedRepoId;
    std::string m_selectedFilename;
    std::vector<RawrXD::HFRepoInfo> m_searchResults;
    std::vector<RawrXD::HFFileInfo> m_fileListResults;

    // Background thread for downloads
    std::unique_ptr<std::thread> m_pullThread;
    std::atomic<bool> m_pullActive{false};
    std::mutex m_uiMutex;

    // Custom message IDs for thread-safe UI updates
    static const UINT WM_PULL_STATUS  = WM_USER + 500;
    static const UINT WM_PULL_DONE    = WM_USER + 501;
    static const UINT WM_SEARCH_DONE  = WM_USER + 502;
};

#endif // RAWRXD_WIN32IDE_MODEL_MANAGER_H
