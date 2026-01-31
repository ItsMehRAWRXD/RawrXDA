#include "SolutionSettings.h"
#include "Resource.h"
#include "Config.h"
#include "PropagationPreview.h"
#include <commctrl.h>

INT_PTR CALLBACK SolutionSettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        // Compiler
        SendMessageW(GetDlgItem(hwnd,1002), CB_ADDSTRING, 0, (LPARAM)L"MSVC");
        SendMessageW(GetDlgItem(hwnd,1002), CB_ADDSTRING, 0, (LPARAM)L"G++");
        SendMessageW(GetDlgItem(hwnd,1001), BM_SETCHECK, gSolutionConfig.overrideCompiler ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd,1002), CB_SETCURSEL, gSolutionConfig.compiler==COMPILER_MSVC?0:1, 0);

        // Theme
        SendMessageW(GetDlgItem(hwnd,1004), CB_ADDSTRING, 0, (LPARAM)L"Light");
        SendMessageW(GetDlgItem(hwnd,1004), CB_ADDSTRING, 0, (LPARAM)L"Dark");
        SendMessageW(GetDlgItem(hwnd,1004), CB_ADDSTRING, 0, (LPARAM)L"System");
        SendMessageW(GetDlgItem(hwnd,1003), BM_SETCHECK, gSolutionConfig.overrideTheme ? BST_CHECKED : BST_UNCHECKED, 0);
        SendMessageW(GetDlgItem(hwnd,1004), CB_SETCURSEL, gSolutionConfig.theme, 0);

        // Zoom
        SendMessageW(GetDlgItem(hwnd,1005), BM_SETCHECK, gSolutionConfig.overrideGraphZoom ? BST_CHECKED : BST_UNCHECKED, 0);
        wchar_t buf[32];
        swprintf(buf,32,L"%.2f", gSolutionConfig.graphZoom);
        SetWindowTextW(GetDlgItem(hwnd,1006), buf);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1007: { // Propagate
            // Read current form values into gSolutionConfig
            gSolutionConfig.overrideCompiler = (SendMessageW(GetDlgItem(hwnd,1001), BM_GETCHECK,0,0)==BST_CHECKED);
            int sel = (int)SendMessageW(GetDlgItem(hwnd,1002), CB_GETCURSEL, 0, 0);
            gSolutionConfig.compiler = (sel==0)?COMPILER_MSVC:COMPILER_GPP;

            gSolutionConfig.overrideTheme = (SendMessageW(GetDlgItem(hwnd,1003), BM_GETCHECK,0,0)==BST_CHECKED);
            sel = (int)SendMessageW(GetDlgItem(hwnd,1004), CB_GETCURSEL, 0, 0);
            gSolutionConfig.theme = (sel==0)?THEME_LIGHT:(sel==1)?THEME_DARK:THEME_SYSTEM;

            gSolutionConfig.overrideGraphZoom = (SendMessageW(GetDlgItem(hwnd,1005), BM_GETCHECK,0,0)==BST_CHECKED);
            wchar_t buf[32]; 
            GetWindowTextW(GetDlgItem(hwnd,1006), buf, 32);
            gSolutionConfig.graphZoom = (float)_wtof(buf);

            // Launch propagation preview
            ShowPropagationPreview(hwnd);
        } break;

        case IDOK: {
            // Save only
            gSolutionConfig.overrideCompiler = (SendMessageW(GetDlgItem(hwnd,1001), BM_GETCHECK,0,0)==BST_CHECKED);
            int sel = (int)SendMessageW(GetDlgItem(hwnd,1002), CB_GETCURSEL, 0, 0);
            gSolutionConfig.compiler = (sel==0)?COMPILER_MSVC:COMPILER_GPP;

            gSolutionConfig.overrideTheme = (SendMessageW(GetDlgItem(hwnd,1003), BM_GETCHECK,0,0)==BST_CHECKED);
            sel = (int)SendMessageW(GetDlgItem(hwnd,1004), CB_GETCURSEL, 0, 0);
            gSolutionConfig.theme = (sel==0)?THEME_LIGHT:(sel==1)?THEME_DARK:THEME_SYSTEM;

            gSolutionConfig.overrideGraphZoom = (SendMessageW(GetDlgItem(hwnd,1005), BM_GETCHECK,0,0)==BST_CHECKED);
            wchar_t buf[32]; 
            GetWindowTextW(GetDlgItem(hwnd,1006), buf, 32);
            gSolutionConfig.graphZoom = (float)_wtof(buf);

            SaveSolutionConfig(L"Solution.solconfig.ini");
            EndDialog(hwnd,IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd,IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowSolutionSettings(HWND parent) {
    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SOLUTION_SETTINGS),
              parent, SolutionSettingsDlgProc);
}
