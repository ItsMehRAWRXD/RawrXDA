#include "Win32IDE.h"
#include "../modules/vsix_loader.h"
#include "../modules/memory_manager.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include <commctrl.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);
    
    // Initialize managers
    VSIXLoader::GetInstance().Initialize("plugins");
    
    // Create IDE window
    Win32IDE ide(hInstance);
    
    if (!ide.createWindow()) {
        MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Initialize engine manager (safe — LoadEngine may fail for missing DLLs)
    auto* engine_mgr = new EngineManager();
    try { engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive"); } catch (...) {}
    try { engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate"); } catch (...) {}
    try { engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler"); } catch (...) {}
    
    // Initialize Codex Ultimate
    auto* codex = new CodexUltimate();
    
    ide.setEngineManager(engine_mgr);
    ide.setCodexUltimate(codex);

    // Show window and force layout
    ide.showWindow();

    // Force layout — SendMessage WM_SIZE with current client rect
    {
        HWND hwnd = ide.getMainWindow();
        if (hwnd) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
        }
    }

    // Diagnostic: dump child window info
    {
        std::ofstream diag("C:\\Users\\HiH8e\\Desktop\\WINDOW_DIAG.txt");
        HWND hwnd = ide.getMainWindow();
        RECT mainRc;
        GetClientRect(hwnd, &mainRc);
        diag << "Main window client: " << mainRc.right << "x" << mainRc.bottom << std::endl;
        diag << "m_hwndMain: " << (void*)hwnd << std::endl;

        auto dumpCtrl = [&](const char* name, HWND h) {
            if (!h) { diag << name << ": NULL" << std::endl; return; }
            RECT r; GetWindowRect(h, &r);
            int w = r.right - r.left, ht = r.bottom - r.top;
            BOOL vis = IsWindowVisible(h);
            diag << name << ": " << (void*)h << " pos=(" << r.left << "," << r.top << ") size=" << w << "x" << ht << " visible=" << vis << std::endl;
        };

        dumpCtrl("Toolbar", ide.getToolbar());
        dumpCtrl("Sidebar", ide.getSidebar());
        dumpCtrl("Editor", ide.getEditor());
        dumpCtrl("StatusBar", ide.getStatusBar());
        dumpCtrl("ActivityBar", ide.getActivityBar());
        diag << "Done." << std::endl;
    }
    
    // Run message loop
    return ide.runMessageLoop();
}
