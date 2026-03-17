#include "Win32IDE.h"
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

#ifndef IDM_SOURCEFILE_OPEN_PICKER
#define IDM_SOURCEFILE_OPEN_PICKER 1040
#endif

namespace fs = std::filesystem;

static const wchar_t* SOURCE_EXTS[] = {
    L".cpp", L".h", L".hpp", L".c", L".asm", L".py", L".ps1", L".psm1", nullptr
};

static bool isSourceExt(const std::wstring& path) {
    std::wstring ext = fs::path(path).extension().wstring();
    for (int i = 0; SOURCE_EXTS[i]; ++i) {
        if (_wcsicmp(ext.c_str(), SOURCE_EXTS[i]) == 0) return true;
    }
    return false;
}

static void collectSourceFiles(const fs::path& root, std::vector<std::wstring>& out, int maxDepth, int depth) {
    if (depth > maxDepth) return;
    try {
        for (auto& e : fs::directory_iterator(root)) {
            if (e.path().filename().wstring()[0] == L'.') continue;
            std::wstring fn = e.path().filename().wstring();
            if (fn == L"3rdparty" || fn == L"node_modules" || fn == L".git") continue;
            if (e.is_directory()) {
                collectSourceFiles(e.path(), out, maxDepth, depth + 1);
            } else if (e.is_regular_file() && isSourceExt(e.path().wstring())) {
                out.push_back(e.path().wstring());
            }
        }
    } catch (...) {}
}

static std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

#define IDC_SF_FILTER 10401
#define IDC_SF_LIST   10402
#define IDC_SF_OK     10403
#define IDC_SF_CANCEL 10404

struct PickerState {
    Win32IDE* ide = nullptr;
    std::vector<std::wstring> paths;
    std::vector<std::wstring> filtered;
};

static void filterList(HWND hFilter, HWND hList, PickerState* ps) {
    if (!ps) return;
    wchar_t buf[256] = {0};
    GetWindowTextW(hFilter, buf, 255);
    std::wstring q(buf);
    std::transform(q.begin(), q.end(), q.begin(), ::towlower);
    ps->filtered.clear();
    for (const auto& p : ps->paths) {
        std::wstring lower = p;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
        if (q.empty() || lower.find(q) != std::wstring::npos) {
            ps->filtered.push_back(p);
        }
    }
    SendMessageW(hList, LB_RESETCONTENT, 0, 0);
    for (const auto& p : ps->filtered) {
        SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
    }
}

static LRESULT CALLBACK SourceFilePickerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PickerState* ps = (PickerState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        ps = new PickerState();
        ps->ide = (Win32IDE*)((CREATESTRUCTW*)lParam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ps);

        fs::path root = fs::current_path();
        if (ps->ide && !ps->ide->getCurrentFilePathPublic().empty()) {
            root = fs::path(utf8ToWide(ps->ide->getCurrentFilePathPublic())).parent_path();
        }
        collectSourceFiles(root, ps->paths, 8, 0);
        ps->filtered = ps->paths;

        RECT rc;
        GetClientRect(hwnd, &rc);
        int cw = (rc.right - rc.left) > 0 ? (rc.right - rc.left) : 620;
        int ch = (rc.bottom - rc.top) > 0 ? (rc.bottom - rc.top) : 440;
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 10, 10, cw - 20, 24, hwnd, (HMENU)(UINT_PTR)IDC_SF_FILTER, nullptr, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_HASSTRINGS | WS_VSCROLL, 10, 40, cw - 20, ch - 100, hwnd, (HMENU)(UINT_PTR)IDC_SF_LIST, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, cw - 180, ch - 50, 80, 28, hwnd, (HMENU)(UINT_PTR)IDC_SF_OK, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, cw - 90, ch - 50, 80, 28, hwnd, (HMENU)(UINT_PTR)IDC_SF_CANCEL, nullptr, nullptr);

        HWND hList = GetDlgItem(hwnd, IDC_SF_LIST);
        for (const auto& p : ps->filtered) {
            SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)p.c_str());
        }
        break;
    }
    case WM_SIZE: {
        int cw = LOWORD(lParam), ch = HIWORD(lParam);
        if (GetDlgItem(hwnd, IDC_SF_FILTER)) {
            SetWindowPos(GetDlgItem(hwnd, IDC_SF_FILTER), nullptr, 10, 10, cw - 20, 24, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd, IDC_SF_LIST), nullptr, 10, 40, cw - 20, ch - 100, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd, IDC_SF_OK), nullptr, cw - 180, ch - 50, 80, 28, SWP_NOZORDER);
            SetWindowPos(GetDlgItem(hwnd, IDC_SF_CANCEL), nullptr, cw - 90, ch - 50, 80, 28, SWP_NOZORDER);
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SF_FILTER && HIWORD(wParam) == EN_CHANGE && ps) {
            filterList(GetDlgItem(hwnd, IDC_SF_FILTER), GetDlgItem(hwnd, IDC_SF_LIST), ps);
        } else if (LOWORD(wParam) == IDC_SF_LIST && HIWORD(wParam) == LBN_DBLCLK && ps) {
            int sel = (int)SendMessageW(GetDlgItem(hwnd, IDC_SF_LIST), LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)ps->filtered.size() && ps->ide) {
                ps->ide->openFilePublic(wideToUtf8(ps->filtered[sel]));
            }
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == IDC_SF_OK && ps) {
            int sel = (int)SendMessageW(GetDlgItem(hwnd, IDC_SF_LIST), LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)ps->filtered.size() && ps->ide) {
                ps->ide->openFilePublic(wideToUtf8(ps->filtered[sel]));
            }
            DestroyWindow(hwnd);
        } else if (LOWORD(wParam) == IDC_SF_CANCEL) {
            DestroyWindow(hwnd);
        }
        break;
    case WM_DESTROY:
        if (ps) { delete ps; SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); }
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void Win32IDE::showSourceFilePicker() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SourceFilePickerWndProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"RawrXD_SourceFilePicker";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassExW(&wc);
        registered = true;
    }

    RECT r;
    GetWindowRect(m_hwndMain, &r);
    int w = 640, h = 480;
    int x = r.left + (r.right - r.left - w) / 2;
    int y = r.top + (r.bottom - r.top - h) / 2;

    HWND picker = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RawrXD_SourceFilePicker", L"#Sourcefile — Win32 GUI",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, m_hwndMain, nullptr, m_hInstance, this);
    if (picker) {
        SetFocus(GetDlgItem(picker, IDC_SF_FILTER));
    }
}
