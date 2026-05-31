// ============================================================================
// command_palette.cpp — RawrXD Command Palette Win32 Implementation
// ============================================================================
#include "command_palette.h"
#include "command_registry.h"
#include <algorithm>
#include <cctype>
#include <deque>
#include <string>
#include <vector>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// ---- Internal resource IDs (must not conflict with app IDs) ---------------
#define PALETTE_IDC_SEARCHBOX   3001
#define PALETTE_IDC_LISTBOX     3002
#define PALETTE_IDC_HINT_LABEL  3003
#define PALETTE_DIALOG_WIDTH    600
#define PALETTE_DIALOG_HEIGHT   420
#define PALETTE_RESULT_HEIGHT   22
#define PALETTE_RECENT_CAP      20
#define PALETTE_WNDCLASS        L"RawrXD_CommandPalette"

// ---- Implementation struct ------------------------------------------------
struct CommandPalette::Impl {
    HWND                  parentHwnd   = nullptr;
    HWND                  dialogHwnd   = nullptr;
    HINSTANCE             hInst        = nullptr;
    CommandPaletteConfig  cfg;

    std::string           selectedId;
    std::deque<std::string> recentIds;     // MRU list

    // Commands snapshot used during a palette session
    std::vector<CommandDescriptor> allCommands;
    std::vector<PaletteEntry>      filtered;

    static Impl* s_active;  // Singleton running instance for dialog proc use
};

CommandPalette::Impl* CommandPalette::Impl::s_active = nullptr;

// ============================================================================
// Fuzzy-score a query against a target string.
// Returns 0 if no match, higher = better.
// ============================================================================
static float FuzzyScore(const std::string& query, const std::string& target) {
    if (query.empty()) return 1.0f;
    std::string q = query, t = target;
    for (auto& c : q) c = (char)std::tolower((unsigned char)c);
    for (auto& c : t) c = (char)std::tolower((unsigned char)c);

    // Substring boost
    if (t.find(q) != std::string::npos) return 10.0f + (float)q.size();

    // Fuzzy: each query char must appear in order
    size_t qi = 0, ti = 0;
    int gaps = 0;
    while (qi < q.size() && ti < t.size()) {
        if (q[qi] == t[ti]) { ++qi; }
        else { ++gaps; }
        ++ti;
    }
    if (qi < q.size()) return 0.0f;   // Not all chars matched
    return 5.0f - (float)gaps * 0.1f;
}

// ============================================================================
// Build display string
// ============================================================================
static std::string BuildLabel(const CommandDescriptor& d, bool showCategory,
                               bool showKeybinding) {
    std::string label;
    if (showCategory && !d.category.empty())
        label = d.category + ": " + d.displayName;
    else
        label = d.displayName;

    if (showKeybinding && !d.keybinding.empty())
        label += "  (" + d.keybinding + ")";
    return label;
}

static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring ws(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), ws.data(), n);
    return ws;
}

// ============================================================================
// CommandPalette
// ============================================================================
CommandPalette::CommandPalette(HWND parentHwnd, const CommandPaletteConfig& cfg) {
    m_impl = new Impl();
    m_impl->parentHwnd = parentHwnd;
    m_impl->cfg        = cfg;
    m_impl->hInst      = (HINSTANCE)GetWindowLongPtr(parentHwnd, GWLP_HINSTANCE);
}

CommandPalette::~CommandPalette() {
    delete m_impl;
}

// ============================================================================
// query() — usable without showing UI (e.g., agentic/REST calls)
// ============================================================================
std::vector<PaletteEntry> CommandPalette::query(const std::string& input,
                                                  unsigned int accessFilter) const {
    auto commands = CommandRegistry::instance().enumerate(accessFilter);
    std::vector<PaletteEntry> results;

    for (const auto& d : commands) {
        std::string label = BuildLabel(d, m_impl->cfg.showCategory, false);
        float score = FuzzyScore(input, label);
        if (input.empty() || score > 0.0f) {
            results.push_back({ d.id, label, d.keybinding, score });
        }
    }

    // Boost recent commands
    if (m_impl->cfg.showRecentFirst) {
        for (size_t ri = 0; ri < m_impl->recentIds.size(); ++ri) {
            for (auto& e : results) {
                if (e.id == m_impl->recentIds[ri])
                    e.score += 20.0f - (float)ri;
            }
        }
    }

    std::stable_sort(results.begin(), results.end(),
        [](const PaletteEntry& a, const PaletteEntry& b) {
            return a.score > b.score;
        });

    if ((int)results.size() > m_impl->cfg.maxResults)
        results.resize(m_impl->cfg.maxResults);

    return results;
}

// ============================================================================
// Win32 Dialog Procedure
// ============================================================================
INT_PTR CALLBACK CommandPalette::dialogProc(HWND hDlg, UINT msg,
                                             WPARAM wParam, LPARAM lParam)
{
    Impl* impl = Impl::s_active;
    if (!impl) return FALSE;

    switch (msg) {
    case WM_INITDIALOG: {
        // Centre dialog over parent
        RECT rc, rp;
        GetWindowRect(hDlg, &rc);
        GetWindowRect(impl->parentHwnd, &rp);
        int x = rp.left + (rp.right - rp.left - PALETTE_DIALOG_WIDTH) / 2;
        int y = rp.top  + 60;  // Near top for Cmd+P feel
        SetWindowPos(hDlg, HWND_TOP, x, y,
                     PALETTE_DIALOG_WIDTH, PALETTE_DIALOG_HEIGHT, SWP_NOZORDER);

        // Focus search box
        SetFocus(GetDlgItem(hDlg, PALETTE_IDC_SEARCHBOX));

        // Load initial full list
        impl->filtered = impl->allCommands.empty()
            ? CommandRegistry::instance().enumerate(CMD_ACCESS_PALETTE)
            : std::vector<CommandDescriptor>{};

        // If filtered is empty, populate from allCommands
        for (auto& d : CommandRegistry::instance().enumerate(CMD_ACCESS_PALETTE)) {
            impl->filtered.push_back({ d.id,
                BuildLabel(d, impl->cfg.showCategory, impl->cfg.showKeybinding),
                d.keybinding, 1.0f });
        }

        HWND hList = GetDlgItem(hDlg, PALETTE_IDC_LISTBOX);
        SendMessage(hList, LB_RESETCONTENT, 0, 0);
        for (const auto& e : impl->filtered) {
            std::wstring ws = ToWide(e.display);
            SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)ws.c_str());
        }
        if (!impl->filtered.empty())
            SendMessage(hList, LB_SETCURSEL, 0, 0);
        return TRUE;
    }

    case WM_COMMAND: {
        HWND hSearch = GetDlgItem(hDlg, PALETTE_IDC_SEARCHBOX);
        HWND hList   = GetDlgItem(hDlg, PALETTE_IDC_LISTBOX);

        if (LOWORD(wParam) == PALETTE_IDC_SEARCHBOX && HIWORD(wParam) == EN_CHANGE) {
            // Re-filter
            wchar_t buf[512] = {};
            GetWindowTextW(hSearch, buf, 512);
            int n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
            std::string inputStr(n - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, buf, -1, inputStr.data(), n, nullptr, nullptr);

            auto entries = impl->cfg.fuzzyMatch
                ? CommandRegistry::instance().enumerate(CMD_ACCESS_PALETTE)
                : CommandRegistry::instance().enumerate(CMD_ACCESS_PALETTE);

            impl->filtered.clear();
            for (auto& d : entries) {
                std::string label = BuildLabel(d, impl->cfg.showCategory,
                                               impl->cfg.showKeybinding);
                float score = FuzzyScore(inputStr, label);
                if (inputStr.empty() || score > 0.0f)
                    impl->filtered.push_back({ d.id, label, d.keybinding, score });
            }
            if (!inputStr.empty()) {
                std::stable_sort(impl->filtered.begin(), impl->filtered.end(),
                    [](const PaletteEntry& a, const PaletteEntry& b) {
                        return a.score > b.score;
                    });
            }
            if ((int)impl->filtered.size() > impl->cfg.maxResults)
                impl->filtered.resize(impl->cfg.maxResults);

            SendMessage(hList, LB_RESETCONTENT, 0, 0);
            for (const auto& e : impl->filtered) {
                std::wstring ws = ToWide(e.display);
                SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)ws.c_str());
            }
            if (!impl->filtered.empty())
                SendMessage(hList, LB_SETCURSEL, 0, 0);
            return TRUE;
        }

        if (LOWORD(wParam) == PALETTE_IDC_LISTBOX && HIWORD(wParam) == LBN_DBLCLK) {
            // Double-click = select
            int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)impl->filtered.size()) {
                impl->selectedId = impl->filtered[sel].id;
                EndDialog(hDlg, IDOK);
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    case WM_KEYDOWN: {
        // Arrow down/up in search box redirects focus to list
        if (GetFocus() == GetDlgItem(hDlg, PALETTE_IDC_SEARCHBOX)) {
            if (wParam == VK_DOWN || wParam == VK_UP) {
                SetFocus(GetDlgItem(hDlg, PALETTE_IDC_LISTBOX));
                return TRUE;
            }
            if (wParam == VK_RETURN) {
                HWND hList = GetDlgItem(hDlg, PALETTE_IDC_LISTBOX);
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)impl->filtered.size()) {
                    impl->selectedId = impl->filtered[sel].id;
                    EndDialog(hDlg, IDOK);
                }
                return TRUE;
            }
        }
        if (GetFocus() == GetDlgItem(hDlg, PALETTE_IDC_LISTBOX)) {
            if (wParam == VK_RETURN) {
                HWND hList = GetDlgItem(hDlg, PALETTE_IDC_LISTBOX);
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)impl->filtered.size()) {
                    impl->selectedId = impl->filtered[sel].id;
                    EndDialog(hDlg, IDOK);
                }
                return TRUE;
            }
            if (wParam == VK_ESCAPE) {
                EndDialog(hDlg, IDCANCEL);
                return TRUE;
            }
        }
        break;
    }
    }
    return FALSE;
}

// ============================================================================
// show() — creates palette UI inline using CreateDialogIndirect
// ============================================================================
std::string CommandPalette::show(const std::string& prefill) {
    m_impl->selectedId.clear();
    Impl::s_active = m_impl;

    // Build DLGTEMPLATE dynamically (no .rc file needed)
    // Layout: EditBox top, ListBox below, size 600×420 DLUs
    struct {
        DLGTEMPLATE hdr;
        WORD        menu, cls, title;
        // Controls added programmatically via CreateWindow after WM_INITDIALOG
    } dlgBuf = {};
    dlgBuf.hdr.style          = DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlgBuf.hdr.cx             = PALETTE_DIALOG_WIDTH;
    dlgBuf.hdr.cy             = PALETTE_DIALOG_HEIGHT;

    // Use CreateWindow-based approach instead—simpler and no .rc dependency
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"#32770",  // Dialog class
        L"RawrXD — Command Palette",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_CENTER,
        CW_USEDEFAULT, CW_USEDEFAULT,
        PALETTE_DIALOG_WIDTH, PALETTE_DIALOG_HEIGHT,
        m_impl->parentHwnd, nullptr,
        m_impl->hInst, nullptr);

    if (!hDlg) {
        Impl::s_active = nullptr;
        return {};
    }

    // Create search edit box
    HWND hSearch = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        10, 10, PALETTE_DIALOG_WIDTH - 20, 28,
        hDlg, (HMENU)(ULONG_PTR)PALETTE_IDC_SEARCHBOX,
        m_impl->hInst, nullptr);

    // Create list box
    HWND hList = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
        LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        10, 48, PALETTE_DIALOG_WIDTH - 20, PALETTE_DIALOG_HEIGHT - 60,
        hDlg, (HMENU)(ULONG_PTR)PALETTE_IDC_LISTBOX,
        m_impl->hInst, nullptr);

    // Monospaced font for readability
    HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessage(hSearch, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(hList,   WM_SETFONT, (WPARAM)hFont, TRUE);

    // Populate list
    auto commands = CommandRegistry::instance().enumerate(CMD_ACCESS_PALETTE);
    m_impl->filtered.clear();
    for (auto& d : commands) {
        std::string label = BuildLabel(d, m_impl->cfg.showCategory,
                                       m_impl->cfg.showKeybinding);
        m_impl->filtered.push_back({ d.id, label, d.keybinding, 1.0f });
        SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)ToWide(label).c_str());
    }
    if (!m_impl->filtered.empty())
        SendMessage(hList, LB_SETCURSEL, 0, 0);

    if (!prefill.empty()) {
        std::wstring ws = ToWide(prefill);
        SetWindowTextW(hSearch, ws.c_str());
    }

    SetFocus(hSearch);
    ShowWindow(hDlg, SW_SHOW);
    EnableWindow(m_impl->parentHwnd, FALSE);

    // Modal message loop
    MSG msg;
    while (IsWindow(hDlg) && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                m_impl->selectedId.clear();
                break;
            }
            if (msg.wParam == VK_RETURN && GetFocus() == hSearch) {
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)m_impl->filtered.size())
                    m_impl->selectedId = m_impl->filtered[sel].id;
                break;
            }
            if (msg.wParam == VK_RETURN && GetFocus() == hList) {
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)m_impl->filtered.size())
                    m_impl->selectedId = m_impl->filtered[sel].id;
                break;
            }
            if ((msg.wParam == VK_DOWN || msg.wParam == VK_UP) && GetFocus() == hSearch) {
                SetFocus(hList);
                continue;
            }
            // Handle typing in search while list has focus → redirect to edit
            if (GetFocus() == hList && msg.wParam >= 0x20 && msg.wParam <= 0x7E) {
                SetFocus(hSearch);
            }
        }
        if (msg.message == WM_COMMAND) {
            if (LOWORD(msg.wParam) == PALETTE_IDC_LISTBOX &&
                HIWORD(msg.wParam) == LBN_DBLCLK) {
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)m_impl->filtered.size())
                    m_impl->selectedId = m_impl->filtered[sel].id;
                break;
            }
            if (LOWORD(msg.wParam) == PALETTE_IDC_SEARCHBOX &&
                HIWORD(msg.wParam) == EN_CHANGE) {
                // Re-filter
                wchar_t buf[512] = {};
                GetWindowTextW(hSearch, buf, 512);
                int nc = WideCharToMultiByte(CP_UTF8,0,buf,-1,nullptr,0,nullptr,nullptr);
                std::string inputStr(nc > 1 ? nc-1 : 0, 0);
                if (nc > 1)
                    WideCharToMultiByte(CP_UTF8,0,buf,-1,inputStr.data(),nc,nullptr,nullptr);

                auto entries = CommandRegistry::instance().enumerate(CMD_ACCESS_PALETTE);
                m_impl->filtered.clear();
                for (auto& d : entries) {
                    std::string label = BuildLabel(d, m_impl->cfg.showCategory,
                                                   m_impl->cfg.showKeybinding);
                    float sc = FuzzyScore(inputStr, label);
                    if (inputStr.empty() || sc > 0.0f)
                        m_impl->filtered.push_back({ d.id, label, d.keybinding, sc });
                }
                if (!inputStr.empty()) {
                    std::stable_sort(m_impl->filtered.begin(), m_impl->filtered.end(),
                        [](const PaletteEntry& a, const PaletteEntry& b){
                            return a.score > b.score;
                        });
                }
                if ((int)m_impl->filtered.size() > m_impl->cfg.maxResults)
                    m_impl->filtered.resize(m_impl->cfg.maxResults);

                SendMessage(hList, LB_RESETCONTENT, 0, 0);
                for (auto& e : m_impl->filtered) {
                    SendMessage(hList, LB_ADDSTRING, 0,
                                (LPARAM)ToWide(e.display).c_str());
                }
                if (!m_impl->filtered.empty())
                    SendMessage(hList, LB_SETCURSEL, 0, 0);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Track recent
    if (!m_impl->selectedId.empty()) {
        m_impl->recentIds.erase(std::remove(m_impl->recentIds.begin(),
                                             m_impl->recentIds.end(),
                                             m_impl->selectedId),
                                 m_impl->recentIds.end());
        m_impl->recentIds.push_front(m_impl->selectedId);
        if (m_impl->recentIds.size() > PALETTE_RECENT_CAP)
            m_impl->recentIds.pop_back();
    }

    EnableWindow(m_impl->parentHwnd, TRUE);
    DestroyWindow(hDlg);
    DeleteObject(hFont);
    Impl::s_active = nullptr;
    return m_impl->selectedId;
}

void CommandPalette::showAndExecute(const std::string& prefill) {
    std::string id = show(prefill);
    if (!id.empty()) {
        CommandRegistry::instance().execute(id);
    }
}

bool InstallCommandPaletteHotkeys(HWND /*mainHwnd*/) {
    // Hotkeys are handled via WM_KEYDOWN / accelerator table in the host;
    // IDM_TOOLS_COMMAND_PALETTE is already mapped to "workbench.action.showCommands"
    // in CommandRegistry::registerBuiltins(). Return true always.
    return true;
}

bool HandleMenuCommandViaRegistry(HWND /*hwnd*/, UINT msg, WPARAM wParam, LPARAM /*lParam*/) {
    if (msg != WM_COMMAND) return false;
    int menuId = LOWORD(wParam);
    if (HIWORD(wParam) != 0) return false;  // Not a menu command
    std::string cmdId = CommandRegistry::instance().commandIdForMenuId(menuId);
    if (cmdId.empty()) return false;
    CommandRegistry::instance().execute(cmdId);
    return true;
}
