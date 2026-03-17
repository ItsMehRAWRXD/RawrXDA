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
    
    // Initialize agentic system
    ide.initializeAgenticBridge();
    ide.initializeAutonomy();
    
    // Initialize engine manager
    auto* engine_mgr = new EngineManager();
    engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
    engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
    engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
    
    // Initialize Codex Ultimate
    auto* codex = new CodexUltimate();
    
    // Show window
    ide.showWindow();
    
    // Run message loop
    return ide.runMessageLoop();
}
