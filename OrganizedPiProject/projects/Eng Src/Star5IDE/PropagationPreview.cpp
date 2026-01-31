#include "PropagationPreview.h"
#include "Resource.h"
#include "Config.h"
#include <commctrl.h>
#include <shlobj.h>
#include <fstream>
#include <iomanip>

std::vector<PropagationEntry> gPropPreview;

// Build dummy list of projects (for now we hardcode two projects)
static std::vector<std::wstring> gProjects = { L"CoreLib", L"App" };

static std::wstring themeToStr(UITheme t) {
    return (t == THEME_LIGHT ? L"Light" : t == THEME_DARK ? L"Dark" : L"System");
}

static void BuildPreview() {
    gPropPreview.clear();
    for (auto& name : gProjects) {
        ProjectConfig cfg;
        LoadProjectConfig(name, cfg);

        PropagationEntry e;
        e.projName = name;
        e.selected = true;
        e.targetCfg = new ProjectConfig(cfg);

        if (gSolutionConfig.overrideCompiler) {
            std::wstring old = cfg.overrideCompiler ? (cfg.compiler == COMPILER_MSVC ? L"MSVC" : L"G++")
                : (gConfig.defaultCompiler == COMPILER_MSVC ? L"MSVC" : L"G++");
            std::wstring nw = (gSolutionConfig.compiler == COMPILER_MSVC ? L"MSVC" : L"G++");
            if (old != nw) e.compilerChange = old + L" → " + nw;
        }

        if (gSolutionConfig.overrideTheme) {
            std::wstring old = cfg.overrideTheme ? themeToStr(cfg.theme) : themeToStr(gConfig.theme);
            std::wstring nw = themeToStr(gSolutionConfig.theme);
            if (old != nw) e.themeChange = old + L" → " + nw;
        }

        if (gSolutionConfig.overrideGraphZoom) {
            float old = cfg.overrideGraphZoom ? cfg.graphZoom : gConfig.defaultGraphZoom;
            float nw = gSolutionConfig.graphZoom;
            if (fabs(old - nw) > 0.001f) {
                wchar_t buf[64];
                swprintf(buf, 64, L"%.2f → %.2f", old, nw);
                e.zoomChange = buf;
            }
        }

        if (!e.compilerChange.empty() || !e.themeChange.empty() || !e.zoomChange.empty())
            gPropPreview.push_back(e);
    }
}

// Auto-save report
static void AutoSaveReport() {
    if (!gConfig.autoSaveReports) return;
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t folder[MAX_PATH];
    swprintf(folder, MAX_PATH, L"Reports");
    CreateDirectoryW(folder, nullptr);

    wchar_t fname[MAX_PATH];
    swprintf(fname, MAX_PATH, L"Reports\\PropagationReport_%04d-%02d-%02d_%02d-%02d-%02d.csv",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::wofstream out(fname);
    out << L"Project,Compiler,Theme,Zoom\n";
    for (auto& e : gPropPreview) {
        if (!e.selected) continue;
        out << L"\"" << e.projName << L"\",";
        out << (e.compilerChange.empty() ? L"-" : e.compilerChange) << L",";
        out << (e.themeChange.empty() ? L"-" : e.themeChange) << L",";
        out << (e.zoomChange.empty() ? L"-" : e.zoomChange) << L"\n";
    }
}

INT_PTR CALLBACK PropPreviewDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        HWND hList = GetDlgItem(hwnd, 1001);
        ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES);

        // Columns
        LVCOLUMN lvc = { 0 };
        lvc.mask = LVCF_TEXT | LVCF_WIDTH;
        lvc.pszText = L"Project"; lvc.cx = 150; ListView_InsertColumn(hList, 0, &lvc);
        lvc.pszText = L"Compiler"; lvc.cx = 150; ListView_InsertColumn(hList, 1, &lvc);
        lvc.pszText = L"Theme"; lvc.cx = 150; ListView_InsertColumn(hList, 2, &lvc);
        lvc.pszText = L"Zoom"; lvc.cx = 100; ListView_InsertColumn(hList, 3, &lvc);

        BuildPreview();
        int idx = 0;
        for (auto& e : gPropPreview) {
            LVITEM lvi = { 0 };
            lvi.mask = LVIF_TEXT;
            lvi.iItem = idx;
            lvi.pszText = (LPWSTR)e.projName.c_str();
            ListView_InsertItem(hList, &lvi);
            if (!e.compilerChange.empty()) ListView_SetItemText(hList, idx, 1, (LPWSTR)e.compilerChange.c_str());
            if (!e.themeChange.empty()) ListView_SetItemText(hList, idx, 2, (LPWSTR)e.themeChange.c_str());
            if (!e.zoomChange.empty()) ListView_SetItemText(hList, idx, 3, (LPWSTR)e.zoomChange.c_str());
            ListView_SetCheckState(hList, idx, TRUE);
            idx++;
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 2001: { // Select All
            HWND hList = GetDlgItem(hwnd, 1001);
            int c = ListView_GetItemCount(hList);
            for (int i = 0; i < c; i++) ListView_SetCheckState(hList, i, TRUE);
        } break;
        case 2002: { // Clear All
            HWND hList = GetDlgItem(hwnd, 1001);
            int c = ListView_GetItemCount(hList);
            for (int i = 0; i < c; i++) ListView_SetCheckState(hList, i, FALSE);
        } break;
        case 2003: { // Save Report manually
            wchar_t fname[MAX_PATH] = L"PropagationReport.csv";
            OPENFILENAMEW ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFilter = L"CSV Files\0*.csv\0All Files\0*.*\0";
            ofn.lpstrFile = fname;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"csv";
            if (GetSaveFileNameW(&ofn)) {
                std::wofstream out(fname);
                out << L"Project,Compiler,Theme,Zoom\n";
                for (auto& e : gPropPreview) {
                    out << L"\"" << e.projName << L"\",";
                    out << (e.compilerChange.empty() ? L"-" : e.compilerChange) << L",";
                    out << (e.themeChange.empty() ? L"-" : e.themeChange) << L",";
                    out << (e.zoomChange.empty() ? L"-" : e.zoomChange) << L"\n";
                }
            }
        } break;
        case IDOK: { // Apply propagation
            HWND hList = GetDlgItem(hwnd, 1001);
            int count = ListView_GetItemCount(hList);
            for (int i = 0; i < count; i++) {
                BOOL checked = ListView_GetCheckState(hList, i);
                gPropPreview[i].selected = (checked != FALSE);
                if (checked) {
                    ProjectConfig newCfg;
                    LoadProjectConfig(gPropPreview[i].projName, newCfg);
                    if (gSolutionConfig.overrideCompiler) {
                        newCfg.overrideCompiler = true;
                        newCfg.compiler = gSolutionConfig.compiler;
                    }
                    if (gSolutionConfig.overrideTheme) {
                        newCfg.overrideTheme = true;
                        newCfg.theme = gSolutionConfig.theme;
                    }
                    if (gSolutionConfig.overrideGraphZoom) {
                        newCfg.overrideGraphZoom = true;
                        newCfg.graphZoom = gSolutionConfig.graphZoom;
                    }
                    SaveProjectConfig(gPropPreview[i].projName, newCfg);
                }
            }
            AutoSaveReport();
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowPropagationPreview(HWND parent) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PROP_PREVIEW),
        parent, PropPreviewDlgProc);
}