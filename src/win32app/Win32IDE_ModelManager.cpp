// ============================================================================
// Win32IDE_ModelManager.cpp — Win32 GUI Model Manager Dialog
// ============================================================================
// Full Win32 native dialog with 3 tabs for model management.
// Uses CreateWindowEx programmatic layout (no .rc resource file).
// Thread-safe UI updates via PostMessage from download threads.
// ============================================================================

#include "Win32IDE_ModelManager.h"
#include "model_puller/model_puller.h"
#include <windowsx.h>
#include <commdlg.h>
#include <iostream>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

// Control IDs
enum {
    IDC_TAB_CTRL       = 2001,
    IDC_SEARCH_EDIT    = 2010,
    IDC_SEARCH_BTN     = 2011,
    IDC_SEARCH_LIST    = 2012,
    IDC_FILE_LIST      = 2013,
    IDC_LIST_FILES_BTN = 2014,
    IDC_PULL_BTN       = 2015,
    IDC_PULL_SRC_EDIT  = 2016,
    IDC_DL_PROGRESS    = 2020,
    IDC_DL_STATUS      = 2021,
    IDC_DL_FILE        = 2022,
    IDC_CANCEL_BTN     = 2023,
    IDC_LOCAL_LIST     = 2030,
    IDC_SET_ACTIVE_BTN = 2031,
    IDC_REMOVE_BTN     = 2032,
    IDC_REGISTER_BTN   = 2033,
    IDC_INFO_BTN       = 2034,
    IDC_OK_BTN         = 2090,
    IDC_CLOSE_BTN      = 2091,
};

static const int DLG_WIDTH  = 760;
static const int DLG_HEIGHT = 560;
static const int TAB_TOP    = 40;

// Helper: format bytes
static std::string FormatSz(uint64_t bytes) {
    char buf[64];
    if (bytes >= 1024ULL * 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f GB",
            static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB",
            static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    }
    return buf;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
ModelManagerDialog::ModelManagerDialog(HWND parentHwnd, HINSTANCE hInstance)
    : m_parentHwnd(parentHwnd), m_hInstance(hInstance) {
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_TAB_CLASSES | ICC_PROGRESS_CLASS | ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);
}

ModelManagerDialog::~ModelManagerDialog() {
    if (m_pullActive.load()) {
        RawrXD::ModelPuller::Instance().Cancel();
    }

    if (m_pullThread && m_pullThread->joinable()) {
        m_pullThread->join();
    }
}

// ============================================================================
// Show — create and run modal dialog
// ============================================================================
void ModelManagerDialog::Show() {
    // Register a dialog class
    const char* CLASS_NAME = "RawrXD_ModelManager";

    WNDCLASSEXA wcx = {};
    wcx.cbSize        = sizeof(wcx);
    wcx.lpfnWndProc   = reinterpret_cast<WNDPROC>(DialogProc);
    wcx.hInstance      = m_hInstance;
    wcx.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcx.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcx.lpszClassName  = CLASS_NAME;
    RegisterClassExA(&wcx);

    // Center on parent
    RECT parentRect = {};
    GetWindowRect(m_parentHwnd, &parentRect);
    int cx = parentRect.left + (parentRect.right - parentRect.left - DLG_WIDTH) / 2;
    int cy = parentRect.top + (parentRect.bottom - parentRect.top - DLG_HEIGHT) / 2;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;

    m_hwndDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        "RawrXD Model Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        cx, cy, DLG_WIDTH, DLG_HEIGHT,
        m_parentHwnd,
        nullptr,
        m_hInstance,
        this  // pass `this` via CREATESTRUCT
    );

    if (!m_hwndDlg) return;

    // Disable parent for modal behavior
    EnableWindow(m_parentHwnd, FALSE);

    // Message loop
    MSG msg;
    while (IsWindow(m_hwndDlg) && GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_parentHwnd, TRUE);
    SetForegroundWindow(m_parentHwnd);
}

// ============================================================================
// DialogProc
// ============================================================================
INT_PTR CALLBACK ModelManagerDialog::DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ModelManagerDialog* self = nullptr;

    if (uMsg == WM_CREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
        self = reinterpret_cast<ModelManagerDialog*>(cs->lpCreateParams);
        SetWindowLongPtrA(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwndDlg = hwndDlg;
        self->CreateControls(hwndDlg);
        return 0;
    }

    self = reinterpret_cast<ModelManagerDialog*>(GetWindowLongPtrA(hwndDlg, GWLP_USERDATA));
    if (!self) return DefWindowProcA(hwndDlg, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        switch (id) {
        case IDC_SEARCH_BTN:     self->OnSearchClicked(); break;
        case IDC_LIST_FILES_BTN: self->OnListFilesClicked(); break;
        case IDC_PULL_BTN:       self->OnPullFromSearchClicked(); break;
        case IDC_CANCEL_BTN:     self->OnCancelDownloadClicked(); break;
        case IDC_SET_ACTIVE_BTN: self->OnSetActiveClicked(); break;
        case IDC_REMOVE_BTN:     self->OnRemoveClicked(); break;
        case IDC_REGISTER_BTN:   self->OnRegisterClicked(); break;
        case IDC_INFO_BTN:       self->OnModelInfoClicked(); break;
        case IDC_CLOSE_BTN:
        case IDCANCEL:
            DestroyWindow(hwndDlg);
            break;
        }
        return 0;
    }

    case WM_NOTIFY: {
        auto nmhdr = reinterpret_cast<NMHDR*>(lParam);
        if (nmhdr->idFrom == IDC_TAB_CTRL && nmhdr->code == TCN_SELCHANGE) {
            int tab = TabCtrl_GetCurSel(self->m_hwndTabCtrl);
            self->OnTabChanged(tab);
        } else if (nmhdr->idFrom == IDC_SEARCH_LIST && nmhdr->code == NM_DBLCLK) {
            self->OnSearchResultSelected();
        }
        return 0;
    }

    case WM_SEARCH_DONE:
        if (auto* completion = reinterpret_cast<SearchCompletion*>(lParam)) {
            self->HandleSearchCompleted(std::move(*completion));
            delete completion;
        }
        return 0;

    case WM_USER + 500: { // WM_PULL_STATUS
        // lParam = PullStatus* (heap allocated, we own it)
        auto* ps = reinterpret_cast<RawrXD::PullStatus*>(lParam);
        if (ps) {
            self->UpdateDownloadProgress(*ps);
            delete ps;
        }
        return 0;
    }

    case WM_USER + 501: { // WM_PULL_DONE
        if (auto* completion = reinterpret_cast<PullCompletion*>(lParam)) {
            self->HandlePullCompleted(std::move(*completion));
            delete completion;
        }
        return 0;
    }

    case WM_DESTROY:
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        return 0;
    }

    return DefWindowProcA(hwndDlg, uMsg, wParam, lParam);
}

// ============================================================================
// CreateControls
// ============================================================================
void ModelManagerDialog::CreateControls(HWND hwndDlg) {
    HFONT hFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");

    // Tab control
    m_hwndTabCtrl = CreateWindowExA(0, WC_TABCONTROLA, "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        8, 8, DLG_WIDTH - 28, DLG_HEIGHT - 60,
        hwndDlg, (HMENU)IDC_TAB_CTRL, m_hInstance, nullptr);
    SendMessage(m_hwndTabCtrl, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Add tabs
    TCITEMA tie = {};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<char*>("Discover");
    TabCtrl_InsertItem(m_hwndTabCtrl, 0, &tie);
    tie.pszText = const_cast<char*>("Downloads");
    TabCtrl_InsertItem(m_hwndTabCtrl, 1, &tie);
    tie.pszText = const_cast<char*>("Local Models");
    TabCtrl_InsertItem(m_hwndTabCtrl, 2, &tie);

    // Close button
    CreateWindowExA(0, "BUTTON", "Close",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        DLG_WIDTH - 100, DLG_HEIGHT - 45, 80, 28,
        hwndDlg, (HMENU)IDC_CLOSE_BTN, m_hInstance, nullptr);

    CreateDiscoverTab(hwndDlg);
    CreateDownloadsTab(hwndDlg);
    CreateLocalTab(hwndDlg);

    // Show first tab
    OnTabChanged(0);
}

// ============================================================================
// CreateDiscoverTab
// ============================================================================
void ModelManagerDialog::CreateDiscoverTab(HWND hwndDlg) {
    int y = TAB_TOP + 10;
    int leftMargin = 20;

    // "Search or pull" label
    CreateWindowExA(0, "STATIC", "Pull source (HuggingFace repo, Ollama model, or URL):",
        WS_CHILD, leftMargin, y, 500, 18, hwndDlg, nullptr, m_hInstance, nullptr);
    y += 22;

    // Pull source edit
    m_hwndPullSourceEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | ES_AUTOHSCROLL,
        leftMargin, y, 530, 24, hwndDlg, (HMENU)IDC_PULL_SRC_EDIT, m_hInstance, nullptr);
    SetWindowTextA(m_hwndPullSourceEdit, "bartowski/Qwen2.5-Coder-32B-Instruct-GGUF:Q4_K_M");

    m_hwndPullBtn = CreateWindowExA(0, "BUTTON", "Pull",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin + 540, y, 60, 24, hwndDlg, (HMENU)IDC_PULL_BTN, m_hInstance, nullptr);
    y += 36;

    // Separator
    CreateWindowExA(0, "STATIC", "",
        WS_CHILD | SS_ETCHEDHORZ,
        leftMargin, y, DLG_WIDTH - 50, 2, hwndDlg, nullptr, m_hInstance, nullptr);
    y += 10;

    // Search label
    CreateWindowExA(0, "STATIC", "Search HuggingFace GGUF models:",
        WS_CHILD, leftMargin, y, 300, 18, hwndDlg, nullptr, m_hInstance, nullptr);
    y += 22;

    // Search box + button
    m_hwndSearchEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | ES_AUTOHSCROLL,
        leftMargin, y, 420, 24, hwndDlg, (HMENU)IDC_SEARCH_EDIT, m_hInstance, nullptr);

    m_hwndSearchBtn = CreateWindowExA(0, "BUTTON", "Search",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin + 430, y, 70, 24, hwndDlg, (HMENU)IDC_SEARCH_BTN, m_hInstance, nullptr);

    m_hwndListFilesBtn = CreateWindowExA(0, "BUTTON", "List Files",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin + 510, y, 90, 24, hwndDlg, (HMENU)IDC_LIST_FILES_BTN, m_hInstance, nullptr);
    y += 32;

    // Search results list
    m_hwndSearchList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        leftMargin, y, DLG_WIDTH - 50, 150,
        hwndDlg, (HMENU)IDC_SEARCH_LIST, m_hInstance, nullptr);
    ListView_SetExtendedListViewStyle(m_hwndSearchList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    // Columns for search results
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = const_cast<char*>("Repository");
    col.cx = 400;
    ListView_InsertColumn(m_hwndSearchList, 0, &col);
    col.pszText = const_cast<char*>("Downloads");
    col.cx = 100;
    ListView_InsertColumn(m_hwndSearchList, 1, &col);
    col.pszText = const_cast<char*>("Likes");
    col.cx = 80;
    ListView_InsertColumn(m_hwndSearchList, 2, &col);
    y += 158;

    // File list (shown when List Files is clicked)
    m_hwndFileListView = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        leftMargin, y, DLG_WIDTH - 50, 140,
        hwndDlg, (HMENU)IDC_FILE_LIST, m_hInstance, nullptr);
    ListView_SetExtendedListViewStyle(m_hwndFileListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    col.pszText = const_cast<char*>("Filename");
    col.cx = 380;
    ListView_InsertColumn(m_hwndFileListView, 0, &col);
    col.pszText = const_cast<char*>("Size");
    col.cx = 100;
    ListView_InsertColumn(m_hwndFileListView, 1, &col);
    col.pszText = const_cast<char*>("Quant");
    col.cx = 100;
    ListView_InsertColumn(m_hwndFileListView, 2, &col);
}

// ============================================================================
// CreateDownloadsTab
// ============================================================================
void ModelManagerDialog::CreateDownloadsTab(HWND hwndDlg) {
    int y = TAB_TOP + 30;
    int leftMargin = 20;

    // Current download file name
    m_hwndDownloadFile = CreateWindowExA(0, "STATIC", "No active download",
        WS_CHILD, leftMargin, y, 600, 20, hwndDlg, (HMENU)IDC_DL_FILE, m_hInstance, nullptr);
    y += 26;

    // Progress bar
    m_hwndDownloadProgress = CreateWindowExA(0, PROGRESS_CLASSA, "",
        WS_CHILD | PBS_SMOOTH,
        leftMargin, y, DLG_WIDTH - 70, 24, hwndDlg, (HMENU)IDC_DL_PROGRESS, m_hInstance, nullptr);
    SendMessage(m_hwndDownloadProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    y += 32;

    // Status text
    m_hwndDownloadStatus = CreateWindowExA(0, "STATIC", "",
        WS_CHILD, leftMargin, y, 600, 20, hwndDlg, (HMENU)IDC_DL_STATUS, m_hInstance, nullptr);
    y += 30;

    // Cancel button
    m_hwndCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel Download",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin, y, 140, 28, hwndDlg, (HMENU)IDC_CANCEL_BTN, m_hInstance, nullptr);
}

// ============================================================================
// CreateLocalTab
// ============================================================================
void ModelManagerDialog::CreateLocalTab(HWND hwndDlg) {
    int y = TAB_TOP + 10;
    int leftMargin = 20;

    // Local models list
    m_hwndLocalListView = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        leftMargin, y, DLG_WIDTH - 50, 360,
        hwndDlg, (HMENU)IDC_LOCAL_LIST, m_hInstance, nullptr);
    ListView_SetExtendedListViewStyle(m_hwndLocalListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = const_cast<char*>("");
    col.cx = 20;
    ListView_InsertColumn(m_hwndLocalListView, 0, &col);
    col.pszText = const_cast<char*>("ID");
    col.cx = 160;
    ListView_InsertColumn(m_hwndLocalListView, 1, &col);
    col.pszText = const_cast<char*>("Name");
    col.cx = 200;
    ListView_InsertColumn(m_hwndLocalListView, 2, &col);
    col.pszText = const_cast<char*>("Quant");
    col.cx = 80;
    ListView_InsertColumn(m_hwndLocalListView, 3, &col);
    col.pszText = const_cast<char*>("Size");
    col.cx = 80;
    ListView_InsertColumn(m_hwndLocalListView, 4, &col);
    col.pszText = const_cast<char*>("Source");
    col.cx = 140;
    ListView_InsertColumn(m_hwndLocalListView, 5, &col);

    y += 370;

    // Buttons row
    m_hwndSetActiveBtn = CreateWindowExA(0, "BUTTON", "Set Active",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin, y, 100, 28, hwndDlg, (HMENU)IDC_SET_ACTIVE_BTN, m_hInstance, nullptr);
    m_hwndInfoBtn = CreateWindowExA(0, "BUTTON", "Info",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin + 110, y, 60, 28, hwndDlg, (HMENU)IDC_INFO_BTN, m_hInstance, nullptr);
    m_hwndRemoveBtn = CreateWindowExA(0, "BUTTON", "Remove",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin + 180, y, 80, 28, hwndDlg, (HMENU)IDC_REMOVE_BTN, m_hInstance, nullptr);
    m_hwndRegisterBtn = CreateWindowExA(0, "BUTTON", "Register File...",
        WS_CHILD | BS_PUSHBUTTON,
        leftMargin + 270, y, 120, 28, hwndDlg, (HMENU)IDC_REGISTER_BTN, m_hInstance, nullptr);
}

// ============================================================================
// Tab switching
// ============================================================================
void ModelManagerDialog::OnTabChanged(int tabIndex) {
    m_currentTab = tabIndex;
    ShowTabControls(tabIndex);

    if (tabIndex == 2) {
        RefreshLocalModels();
    }
}

void ModelManagerDialog::ShowTabControls(int tabIndex) {
    // Discover tab controls
    int discoverShow = (tabIndex == 0) ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hwndSearchEdit, discoverShow);
    ShowWindow(m_hwndSearchBtn, discoverShow);
    ShowWindow(m_hwndSearchList, discoverShow);
    ShowWindow(m_hwndFileListView, discoverShow);
    ShowWindow(m_hwndListFilesBtn, discoverShow);
    ShowWindow(m_hwndPullBtn, discoverShow);
    ShowWindow(m_hwndPullSourceEdit, discoverShow);

    // Downloads tab controls
    int dlShow = (tabIndex == 1) ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hwndDownloadProgress, dlShow);
    ShowWindow(m_hwndDownloadStatus, dlShow);
    ShowWindow(m_hwndDownloadFile, dlShow);
    ShowWindow(m_hwndCancelBtn, dlShow);

    // Local tab controls
    int localShow = (tabIndex == 2) ? SW_SHOW : SW_HIDE;
    ShowWindow(m_hwndLocalListView, localShow);
    ShowWindow(m_hwndSetActiveBtn, localShow);
    ShowWindow(m_hwndRemoveBtn, localShow);
    ShowWindow(m_hwndRegisterBtn, localShow);
    ShowWindow(m_hwndInfoBtn, localShow);

    // Show static labels for each tab
    // The labels are children of the dialog; we created them in each tab function
    // They need to be toggled too. For simplicity, all "STATIC" labels in the
    // discover region are always visible (they sit behind tabs when not active)
}

// ============================================================================
// Discover tab handlers
// ============================================================================
void ModelManagerDialog::OnSearchClicked() {
    char buf[512] = {};
    GetWindowTextA(m_hwndSearchEdit, buf, sizeof(buf));
    std::string query(buf);
    if (query.empty()) return;

    SetWindowTextA(m_hwndSearchBtn, "...");
    EnableWindow(m_hwndSearchBtn, FALSE);

    // Background search
    HWND dlgHwnd = m_hwndDlg;
    std::thread([query, dlgHwnd]() {
        auto* completion = new SearchCompletion();
        completion->results = RawrXD::ModelPuller::Instance().Search(query);
        if (completion->results.empty()) {
            completion->error = "No matching GGUF repositories found, or the Hugging Face search request failed.";
        }

        if (!PostMessage(dlgHwnd, WM_SEARCH_DONE, 0, reinterpret_cast<LPARAM>(completion))) {
            delete completion;
        }
    }).detach();
}

void ModelManagerDialog::PopulateSearchResults(const std::vector<RawrXD::HFRepoInfo>& results) {
    ListView_DeleteAllItems(m_hwndSearchList);

    for (int i = 0; i < static_cast<int>(results.size()); ++i) {
        auto& r = results[static_cast<size_t>(i)];
        LVITEMA lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = const_cast<char*>(r.repoId.c_str());
        ListView_InsertItem(m_hwndSearchList, &lvi);

        std::string dl = std::to_string(r.downloads);
        ListView_SetItemText(m_hwndSearchList, i, 1, const_cast<char*>(dl.c_str()));

        std::string lk = std::to_string(r.likes);
        ListView_SetItemText(m_hwndSearchList, i, 2, const_cast<char*>(lk.c_str()));
    }

    SetWindowTextA(m_hwndSearchBtn, "Search");
    EnableWindow(m_hwndSearchBtn, TRUE);
}

void ModelManagerDialog::HandleSearchCompleted(SearchCompletion completion) {
    m_searchResults = std::move(completion.results);
    PopulateSearchResults(m_searchResults);

    if (!completion.error.empty()) {
        MessageBoxA(m_hwndDlg, completion.error.c_str(), "Model Search", MB_OK | MB_ICONINFORMATION);
    }
}

void ModelManagerDialog::OnSearchResultSelected() {
    int sel = ListView_GetNextItem(m_hwndSearchList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_searchResults.size())) {
        return;
    }

    const auto& repo = m_searchResults[static_cast<size_t>(sel)].repoId;
    SetWindowTextA(m_hwndPullSourceEdit, repo.c_str());
    m_selectedRepoId = repo;
    OnListFilesClicked();
}

void ModelManagerDialog::OnListFilesClicked() {
    // Get selected repo from search results list
    int sel = ListView_GetNextItem(m_hwndSearchList, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_searchResults.size())) {
        // Try the pull source edit
        char buf[512] = {};
        GetWindowTextA(m_hwndPullSourceEdit, buf, sizeof(buf));
        m_selectedRepoId = buf;
    } else {
        m_selectedRepoId = m_searchResults[static_cast<size_t>(sel)].repoId;
    }

    if (m_selectedRepoId.empty()) return;

    auto files = RawrXD::ModelPuller::Instance().ListQuantizations(m_selectedRepoId);
    m_fileListResults = files;
    PopulateFileList(files);

    if (files.empty()) {
        MessageBoxA(m_hwndDlg,
            "No GGUF files were found for that repository, or the file listing request failed.",
            "Model Files",
            MB_OK | MB_ICONINFORMATION);
    }
}

void ModelManagerDialog::PopulateFileList(const std::vector<RawrXD::HFFileInfo>& files) {
    ListView_DeleteAllItems(m_hwndFileListView);

    for (int i = 0; i < static_cast<int>(files.size()); ++i) {
        auto& f = files[static_cast<size_t>(i)];
        LVITEMA lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        lvi.pszText = const_cast<char*>(f.filename.c_str());
        ListView_InsertItem(m_hwndFileListView, &lvi);

        std::string sz = FormatSz(f.sizeBytes);
        ListView_SetItemText(m_hwndFileListView, i, 1, const_cast<char*>(sz.c_str()));

        ListView_SetItemText(m_hwndFileListView, i, 2, const_cast<char*>(f.quantization.c_str()));
    }
}

void ModelManagerDialog::OnPullFromSearchClicked() {
    char buf[512] = {};
    GetWindowTextA(m_hwndPullSourceEdit, buf, sizeof(buf));
    std::string source(buf);
    if (source.empty()) return;

    // Switch to Downloads tab
    TabCtrl_SetCurSel(m_hwndTabCtrl, 1);
    OnTabChanged(1);

    StartPull(source);
}

// ============================================================================
// Downloads tab handlers
// ============================================================================
void ModelManagerDialog::StartPull(const std::string& source) {
    if (m_pullActive.load()) {
        MessageBoxA(m_hwndDlg, "A download is already in progress.", "RawrXD", MB_OK);
        return;
    }

    FinalizePullThread();

    m_pullActive = true;
    SetWindowTextA(m_hwndDownloadFile, source.c_str());
    SetWindowTextA(m_hwndDownloadStatus, "Starting...");
    SendMessage(m_hwndDownloadProgress, PBM_SETPOS, 0, 0);

    HWND dlgHwnd = m_hwndDlg;

    m_pullThread = std::make_unique<std::thread>([source, dlgHwnd]() {
        auto result = RawrXD::ModelPuller::Instance().Pull(source,
            [dlgHwnd](const RawrXD::PullStatus& status) {
                // Post status update to UI thread
                auto* ps = new RawrXD::PullStatus(status);
                PostMessage(dlgHwnd, WM_USER + 500, 0, reinterpret_cast<LPARAM>(ps));
            });

        auto* completion = new PullCompletion();
        completion->success = result.success;
        completion->filePath = std::move(result.filePath);
        completion->error = std::move(result.error);

        if (!PostMessage(dlgHwnd, WM_USER + 501, 0, reinterpret_cast<LPARAM>(completion))) {
            delete completion;
        }
    });
}

void ModelManagerDialog::UpdateDownloadProgress(const RawrXD::PullStatus& status) {
    int pct = static_cast<int>(status.downloadProgress.progressPercent);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    SendMessage(m_hwndDownloadProgress, PBM_SETPOS, pct, 0);

    char statusBuf[256];
    auto& dp = status.downloadProgress;
    std::snprintf(statusBuf, sizeof(statusBuf),
        "[%d/%d] %s | %s / %s | %s",
        status.stepNumber, status.totalSteps,
        status.stepDescription.c_str(),
        FormatSz(dp.bytesDownloaded).c_str(),
        dp.totalBytes > 0 ? FormatSz(dp.totalBytes).c_str() : "??",
        dp.speedBytesPerSec > 0 ? (FormatSz(static_cast<uint64_t>(dp.speedBytesPerSec)) + "/s").c_str() : "");

    SetWindowTextA(m_hwndDownloadStatus, statusBuf);
}

void ModelManagerDialog::OnCancelDownloadClicked() {
    RawrXD::ModelPuller::Instance().Cancel();
    SetWindowTextA(m_hwndDownloadStatus, "Cancelling...");
}

void ModelManagerDialog::HandlePullCompleted(PullCompletion completion) {
    m_pullActive = false;
    FinalizePullThread();
    RefreshLocalModels();

    if (completion.success) {
        SetWindowTextA(m_hwndDownloadStatus, "Download complete!");
        SendMessage(m_hwndDownloadProgress, PBM_SETPOS, 100, 0);
        std::string successMessage = "Model ready:\n\n" + completion.filePath;
        MessageBoxA(m_hwndDlg, successMessage.c_str(), "Model Ready", MB_OK | MB_ICONINFORMATION);
        return;
    }

    SetWindowTextA(m_hwndDownloadStatus,
        completion.error.empty() ? "Download failed." : completion.error.c_str());
    MessageBoxA(
        m_hwndDlg,
        completion.error.empty() ? "The model pull did not complete successfully." : completion.error.c_str(),
        "Model Pull Failed",
        MB_OK | MB_ICONERROR);
}

void ModelManagerDialog::FinalizePullThread() {
    if (m_pullThread && m_pullThread->joinable()) {
        m_pullThread->join();
    }
    m_pullThread.reset();
}

// ============================================================================
// Local tab handlers
// ============================================================================
void ModelManagerDialog::RefreshLocalModels() {
    auto models = RawrXD::ModelPuller::Instance().ListLocalModels();
    PopulateLocalModels(models);
}

void ModelManagerDialog::PopulateLocalModels(const std::vector<RawrXD::ModelEntry>& models) {
    ListView_DeleteAllItems(m_hwndLocalListView);

    for (int i = 0; i < static_cast<int>(models.size()); ++i) {
        auto& m = models[static_cast<size_t>(i)];
        LVITEMA lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = i;
        std::string active = m.active ? "*" : "";
        lvi.pszText = const_cast<char*>(active.c_str());
        ListView_InsertItem(m_hwndLocalListView, &lvi);

        ListView_SetItemText(m_hwndLocalListView, i, 1, const_cast<char*>(m.id.c_str()));
        ListView_SetItemText(m_hwndLocalListView, i, 2, const_cast<char*>(m.name.c_str()));
        ListView_SetItemText(m_hwndLocalListView, i, 3, const_cast<char*>(m.quantization.c_str()));

        std::string sz = FormatSz(m.sizeBytes);
        ListView_SetItemText(m_hwndLocalListView, i, 4, const_cast<char*>(sz.c_str()));

        std::string src = m.source.size() > 20 ? m.source.substr(0, 20) + "..." : m.source;
        ListView_SetItemText(m_hwndLocalListView, i, 5, const_cast<char*>(src.c_str()));
    }
}

void ModelManagerDialog::OnSetActiveClicked() {
    int sel = ListView_GetNextItem(m_hwndLocalListView, -1, LVNI_SELECTED);
    auto models = RawrXD::ModelPuller::Instance().ListLocalModels();
    if (sel < 0 || sel >= static_cast<int>(models.size())) return;

    RawrXD::ModelPuller::Instance().SetActiveModel(models[static_cast<size_t>(sel)].id);
    m_selectedModelPath = models[static_cast<size_t>(sel)].absolutePath;
    RefreshLocalModels();
}

void ModelManagerDialog::OnRemoveClicked() {
    int sel = ListView_GetNextItem(m_hwndLocalListView, -1, LVNI_SELECTED);
    auto models = RawrXD::ModelPuller::Instance().ListLocalModels();
    if (sel < 0 || sel >= static_cast<int>(models.size())) return;

    auto& m = models[static_cast<size_t>(sel)];
    char msg[512];
    std::snprintf(msg, sizeof(msg),
        "Remove model '%s' from registry?\n\n"
        "Click Yes to also delete the file from disk.\n"
        "Click No to only remove from registry.",
        m.id.c_str());

    int result = MessageBoxA(m_hwndDlg, msg, "Remove Model", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (result == IDYES) {
        RawrXD::ModelPuller::Instance().RemoveModel(m.id, true);
    } else if (result == IDNO) {
        RawrXD::ModelPuller::Instance().RemoveModel(m.id, false);
    }
    RefreshLocalModels();
}

void ModelManagerDialog::OnRegisterClicked() {
    // Open file dialog
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = m_hwndDlg;
    ofn.lpstrFilter  = "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile    = filename;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle   = "Register GGUF Model";

    if (GetOpenFileNameA(&ofn)) {
        RawrXD::ModelPuller::Instance().RegisterLocalModel(filename);
        RefreshLocalModels();
    }
}

void ModelManagerDialog::OnModelInfoClicked() {
    int sel = ListView_GetNextItem(m_hwndLocalListView, -1, LVNI_SELECTED);
    auto models = RawrXD::ModelPuller::Instance().ListLocalModels();
    if (sel < 0 || sel >= static_cast<int>(models.size())) return;

    auto& m = models[static_cast<size_t>(sel)];

    std::ostringstream oss;
    oss << "ID: " << m.id << "\n"
        << "Name: " << m.name << "\n"
        << "Quantization: " << m.quantization << "\n"
        << "Size: " << FormatSz(m.sizeBytes) << "\n"
        << "Path: " << m.absolutePath << "\n"
        << "Source: " << m.source << "\n"
        << "SHA256: " << m.sha256 << "\n"
        << "Downloaded: " << m.downloadedAt << "\n"
        << "Active: " << (m.active ? "Yes" : "No");

    MessageBoxA(m_hwndDlg, oss.str().c_str(), "Model Info", MB_OK | MB_ICONINFORMATION);
}

