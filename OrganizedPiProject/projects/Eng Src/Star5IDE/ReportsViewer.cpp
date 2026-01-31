#include "ReportsViewer.h"
#include "Resource.h"
#include <commctrl.h>
#include <shellapi.h>
#include <fstream>
#include <vector>

static std::vector<std::wstring> gReports;

static void LoadReports(HWND hList) {
    ListView_DeleteAllItems(hList);
    gReports.clear();

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(L"Reports\\PropagationReport_*.csv", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        int idx = 0;
        do {
            std::wstring path = L"Reports\\" + std::wstring(fd.cFileName);
            gReports.push_back(path);

            LVITEMW lvi = { 0 };
            lvi.mask = LVIF_TEXT;
            lvi.iItem = idx;
            lvi.pszText = (LPWSTR)fd.cFileName;
            ListView_InsertItem(hList, &lvi);

            // extract date from filename
            std::wstring fname = fd.cFileName;
            size_t pos = fname.find(L"Report_");
            std::wstring date = (pos != std::wstring::npos) ? fname.substr(pos + 7, 19) : L"";
            ListView_SetItemText(hList, idx, 1, (LPWSTR)date.c_str());

            idx++;
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
}

static void ShowPreview(HWND hEdit, const std::wstring& path) {
    std::wifstream in(path);
    std::wstring text, line;
    int lines = 0;
    while (lines < 15 && std::getline(in, line)) {
        text += line + L"\r\n";
        lines++;
    }
    SetWindowTextW(hEdit, text.c_str());
}

INT_PTR CALLBACK ReportsViewerDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hList, hPreview;
    switch (msg) {
    case WM_INITDIALOG: {
        hList = GetDlgItem(hwnd, 1001);
        hPreview = GetDlgItem(hwnd, 1002);

        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

        // Columns
        LVCOLUMN lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = L"Report File"; lvc.cx = 350; ListView_InsertColumn(hList, 0, &lvc);
        lvc.pszText = L"Date"; lvc.cx = 200; ListView_InsertColumn(hList, 1, &lvc);

        LoadReports(hList);
        return TRUE;
    }
    case WM_NOTIFY: {
        LPNMLISTVIEW pnm = (LPNMLISTVIEW)lParam;
        if (pnm->hdr.idFrom == 1001 && pnm->hdr.code == LVN_ITEMCHANGED && (pnm->uChanged & LVIF_STATE)) {
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel >= 0 && sel < (int)gReports.size()) {
                ShowPreview(hPreview, gReports[sel]);
            }
        }
    } break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: { // Open selected
            int sel = ListView_GetNextItem(hList, -1, LVNI_SELECTED);
            if (sel >= 0 && sel < (int)gReports.size()) {
                ShellExecuteW(hwnd, L"open", gReports[sel].c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        } break;

        case 2001: { // Delete selected
            std::vector<int> toDel;
            int count = ListView_GetItemCount(hList);
            for (int i = 0; i < count; i++) {
                if (ListView_GetCheckState(hList, i)) toDel.push_back(i);
            }
            if (toDel.empty()) {
                MessageBoxW(hwnd, L"No reports selected.", L"Delete", MB_OK | MB_ICONWARNING);
                break;
            }
            if (MessageBoxW(hwnd, L"Delete selected reports?", L"Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                for (int i = (int)toDel.size() - 1; i >= 0; i--) {
                    int idx = toDel[i];
                    if (idx < (int)gReports.size()) {
                        DeleteFileW(gReports[idx].c_str());
                        gReports.erase(gReports.begin() + idx);
                        ListView_DeleteItem(hList, idx);
                    }
                }
            }
        } break;

        case 2002: { // Select All
            int c = ListView_GetItemCount(hList);
            for (int i = 0; i < c; i++) ListView_SetCheckState(hList, i, TRUE);
        } break;

        case 2003: { // Clear All
            int c = ListView_GetItemCount(hList);
            for (int i = 0; i < c; i++) ListView_SetCheckState(hList, i, FALSE);
        } break;

        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowReportsViewer(HWND parent) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_REPORTS_VIEWER),
        parent, ReportsViewerDlgProc);
}