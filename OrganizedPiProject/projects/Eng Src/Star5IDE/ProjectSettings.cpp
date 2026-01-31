#include "ProjectSettings.h"
#include "Config.h"
#include "Resource.h"
#include <commctrl.h>

static ProjectConfig* gProjCfg = nullptr;
static std::wstring gProjName;

// Dialog procedure
INT_PTR CALLBACK ProjectSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Load pointer
        gProjCfg = (ProjectConfig*)lParam;

        // Populate controls: compiler combo, theme combo, etc.
        SendMessageW(GetDlgItem(hwnd,1002), CB_ADDSTRING, 0, (LPARAM)L"MSVC");
        SendMessageW(GetDlgItem(hwnd,1002), CB_ADDSTRING, 0, (LPARAM)L"G++");
        SendMessageW(GetDlgItem(hwnd,1001), BM_SETCHECK, gProjCfg->overrideCompiler ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd,1002), CB_SETCURSEL, gProjCfg->compiler==COMPILER_MSVC?0:1, 0);

        SendMessageW(GetDlgItem(hwnd,1004), CB_ADDSTRING, 0, (LPARAM)L"Light");
        SendMessageW(GetDlgItem(hwnd,1004), CB_ADDSTRING, 0, (LPARAM)L"Dark");
        SendMessageW(GetDlgItem(hwnd,1004), CB_ADDSTRING, 0, (LPARAM)L"System");
        SendMessageW(GetDlgItem(hwnd,1003), BM_SETCHECK, gProjCfg->overrideTheme ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd,1004), CB_SETCURSEL, gProjCfg->theme, 0);

        SendMessageW(GetDlgItem(hwnd,1005), BM_SETCHECK, gProjCfg->overrideGraphZoom ? BST_CHECKED : BST_UNCHECKED, 0);
        wchar_t buf[32];
        swprintf(buf,32,L"%.2f", gProjCfg->graphZoom);
        SetWindowTextW(GetDlgItem(hwnd,1006), buf);

        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            // Retrieve settings and save
            gProjCfg->overrideCompiler = (SendMessageW(GetDlgItem(hwnd,1001), BM_GETCHECK,0,0)==BST_CHECKED);
            int sel = (int)SendMessageW(GetDlgItem(hwnd,1002), CB_GETCURSEL, 0, 0);
            gProjCfg->compiler = (sel==0)?COMPILER_MSVC:COMPILER_GPP;

            gProjCfg->overrideTheme = (SendMessageW(GetDlgItem(hwnd,1003), BM_GETCHECK,0,0)==BST_CHECKED);
            sel = (int)SendMessageW(GetDlgItem(hwnd,1004), CB_GETCURSEL, 0, 0);
            gProjCfg->theme = (sel==0)?THEME_LIGHT:(sel==1)?THEME_DARK:THEME_SYSTEM;

            gProjCfg->overrideGraphZoom = (SendMessageW(GetDlgItem(hwnd,1005), BM_GETCHECK,0,0)==BST_CHECKED);
            wchar_t buf[32]; 
            GetWindowTextW(GetDlgItem(hwnd,1006), buf, 32);
            gProjCfg->graphZoom = (float)_wtof(buf);

            SaveProjectConfig(gProjName,*gProjCfg);
            EndDialog(hwnd,IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hwnd,IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// Project settings dialog
void ShowProjectSettings(HWND parent, const std::wstring& projectName, ProjectConfig& cfg) {
    gProjCfg = &cfg;
    gProjName = projectName;
    INT_PTR res = DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PROJECT_SETTINGS),
                                 parent, ProjectSettingsDlgProc, (LPARAM)&cfg);
}