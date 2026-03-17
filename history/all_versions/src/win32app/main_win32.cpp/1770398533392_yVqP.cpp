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
    
    MessageBoxA(nullptr, "Stage 1: Common controls initialized", "Debug", MB_OK);
    
    // Initialize managers
    VSIXLoader::GetInstance().Initialize("plugins");
    
    MessageBoxA(nullptr, "Stage 2: VSIXLoader initialized", "Debug", MB_OK);
    
    // Create IDE window
    Win32IDE ide(hInstance);
    MessageBoxA(nullptr, "Stage 3: Win32IDE constructed", "Debug", MB_OK);
    
    if (!ide.createWindow()) {
        MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    MessageBoxA(nullptr, "Stage 4: Window created", "Debug", MB_OK);
    
    // Initialize engine manager
    auto* engine_mgr = new EngineManager();
    engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
    engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
    engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
    
    // Initialize Codex Ultimate
    auto* codex = new CodexUltimate();
    
    ide.setEngineManager(engine_mgr);
    ide.setCodexUltimate(codex);

    // Show window
    ide.showWindow();
    
    // Run message loop
    return ide.runMessageLoop();
}
