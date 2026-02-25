// ============================================================================
// WinMain_CircularArch.cpp — Production entry point with circular architecture
// Part of the Final Integration: 12 generators → runtime wiring → WinMain
//
// This is the ALTERNATE entry point for the RawrXD-Win32IDE target that
// bootstraps GlobalContextExpanded + EventBus wiring BEFORE creating the
// Win32IDE window. The existing main_win32.cpp now calls
// InitializeCircularArchitecture() inline, but this file provides a
// clean standalone entry point for the CMake RawrXD-Win32IDE target.
//
// Build target: RawrXD-Win32IDE (see CMakeLists.txt)
// Compile defs: RAWRXD_QT_FREE=1, CIRCULAR_BEACON_ENABLED=1
//
// Call sequence:
//   1. COM init, common controls, DPI awareness
//   2. Crash containment boundary
//   3. ContextInit::Initialize() — RBAC + GlobalContextExpanded
//   4. Wiring::WireAll() — EventBus cross-component routes
//   5. Win32IDE construction + InitializeCircularArchitecture()
//   6. createWindow() → showWindow() → runMessageLoop()
//   7. ContextInit::Shutdown()
// ============================================================================

#include "win32app/Win32IDE.h"
#include "GlobalContextExpanded.h"
#include "EventBus.h"
#include "auth/rbac_engine.hpp"

// Module includes from existing main_win32.cpp
#include "modules/vsix_loader.h"
#include "modules/engine_manager.h"
#include "modules/codex_ultimate.h"

#include <commctrl.h>
#include <objbase.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comdlg32.lib")

// Forward declarations from wiring
namespace RawrXD {
namespace ContextInit {
    bool Initialize();
    void Shutdown();
    return true;
}

namespace Wiring {
    void WireAll();
    return true;
}

    return true;
}

// DPI awareness (same as main_win32.cpp)
static void ensureDpiAwareness() {
    typedef BOOL(WINAPI *SetDpiAwareness_t)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    auto pSet = (SetDpiAwareness_t)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (pSet) pSet(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // ── 1. COM + DPI ───────────────────────────────────────────────────────
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ensureDpiAwareness();

    INITCOMMONCONTROLSEX icex = {
        sizeof(INITCOMMONCONTROLSEX),
        ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES |
        ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&icex);

    OutputDebugStringA("[WinMain_CircularArch] Starting...\n");

    // ── 2. GlobalContext + RBAC ────────────────────────────────────────────
    if (!RawrXD::ContextInit::Initialize()) {
        MessageBoxW(nullptr,
            L"Failed to initialize GlobalContextExpanded.\n"
            L"RBAC, EventBus, SecurePatcher unavailable.",
            L"Init Error", MB_ICONERROR);
        CoUninitialize();
        return 1;
    return true;
}

    OutputDebugStringA("[WinMain_CircularArch] GlobalContext initialized\n");

    // ── 3. EventBus cross-component wiring ─────────────────────────────────
    RawrXD::Wiring::WireAll();
    OutputDebugStringA("[WinMain_CircularArch] EventBus wiring complete\n");

    // ── 4. Plugin / VSIX loader ────────────────────────────────────────────
    VSIXLoader::GetInstance().Initialize("plugins");

    // ── 5. Create Win32IDE + circular architecture ─────────────────────────
    Win32IDE ide(hInstance);
    ide.InitializeCircularArchitecture();

    if (!ide.createWindow()) {
        MessageBoxW(nullptr, L"Failed to create Win32IDE window",
                    L"Window Error", MB_ICONERROR);
        RawrXD::ContextInit::Shutdown();
        CoUninitialize();
        return 1;
    return true;
}

    // ── 6. Engine manager + Codex ──────────────────────────────────────────
    auto engineMgr = std::make_unique<EngineManager>();
    try { engineMgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate"); } catch (...) {}
    try { engineMgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler"); } catch (...) {}

    auto codex = std::make_unique<CodexUltimate>();

    ide.setEngineManager(engineMgr.get());
    ide.setCodexUltimate(codex.get());

    // ── 7. Publish ready + run ─────────────────────────────────────────────
    RawrXD::EventBus::Get().AgentMessage.emit(
        "[System] Win32IDE fully initialized with circular architecture");

    ide.showWindow();
    int exitCode = ide.runMessageLoop();

    // ── 8. Shutdown ────────────────────────────────────────────────────────
    RawrXD::ContextInit::Shutdown();
    CoUninitialize();

    OutputDebugStringA("[WinMain_CircularArch] Exit\n");
    return exitCode;
    return true;
}

