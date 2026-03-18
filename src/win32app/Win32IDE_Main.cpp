#include "Win32IDE.h"
#include "../modules/vsix_loader.h"
#include "../modules/memory_manager.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include "../diagnostics/self_diagnose.hpp"
#include "../diagnostics/pattern_scan.hpp"
#include <commctrl.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    RawrXD::Diagnostics::SelfDiagnoser::Install("D:\\rawrxd\\crash_diag.txt");
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("WinMain.Entry");

    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("Post.InitCommonControls");
    
    // Initialize managers
    VSIXLoader::GetInstance().Initialize("plugins");
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("Post.VSIXLoader");
    
    // Create IDE window
    Win32IDE ide(hInstance);
    if (!ide.createWindow()) {
        MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("Post.CreateWindow");
    
    // Initialize agentic system
    ide.initializeAgenticBridge();
    ide.initializeAutonomy();
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("Post.AgenticInit");
    
    // Initialize engine manager
    auto* engine_mgr = new EngineManager();
    engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
    engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
    engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("Post.EngineLoad");
    
    // Initialize Codex Ultimate
    auto* codex = new CodexUltimate();
    (void)codex;

    const std::size_t suspicious = RawrXD::Diagnostics::CorruptionScanner::ScanCurrentModule();
    if (suspicious > 0) {
        RawrXD::Diagnostics::SelfDiagnoser::SelfLog("module_scan_suspicious_count=%zu", suspicious);
    }
    
    // Show window
    ide.showWindow();
    RawrXD::Diagnostics::SelfDiagnoser::CheckHeapOrDie("Post.ShowWindow");
    
    // Run message loop
    return ide.runMessageLoop();
}
