#include "Win32IDE.h"
#include "HeadlessIDE.h"
#include "../../include/rawrxd_version.h"
#include "../../include/final_gauntlet.h"
#include "../../include/crash_containment.h"
#include "../../include/patch_rollback_ledger.h"
#include "../../include/masm_bridge_cathedral.h"
#include "../../include/quant_hysteresis.h"
#include "../modules/vsix_loader.h"
#include "../modules/memory_manager.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include "../core/rawrxd_state_mmf.hpp"
#include "../core/js_extension_host.hpp"
#include "../core/model_memory_hotpatch.hpp"
#include <commctrl.h>
#include <shellscalingapi.h>
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Headless mode detection — scans argv for --headless flag
// ============================================================================
static bool hasHeadlessFlag(LPSTR lpCmdLine) {
    if (!lpCmdLine) return false;
    return strstr(lpCmdLine, "--headless") != nullptr;
}

// ============================================================================
// Parse WinMain command line into argc/argv for headless mode
// ============================================================================
static void parseCmdLine(LPSTR lpCmdLine, int& argc, char**& argv) {
    static std::vector<std::string> args;
    static std::vector<char*> ptrs;

    args.clear();
    ptrs.clear();
    args.push_back("RawrXD-Win32IDE.exe");

    if (lpCmdLine && lpCmdLine[0]) {
        std::string cmdLine(lpCmdLine);
        std::istringstream iss(cmdLine);
        std::string token;
        while (iss >> token) {
            // Handle quoted arguments
            if (!token.empty() && token.front() == '"') {
                token = token.substr(1);
                std::string rest;
                while (token.back() != '"' && std::getline(iss, rest, '"')) {
                    token += " " + rest;
                }
                if (!token.empty() && token.back() == '"') {
                    token.pop_back();
                }
            }
            args.push_back(token);
        }
    }

    for (auto& a : args) ptrs.push_back(&a[0]);
    ptrs.push_back(nullptr);

    argc = static_cast<int>(args.size());
    argv = ptrs.data();
}

// ============================================================================
// Top-level crash handler — Cathedral Crash Containment Boundary
// ============================================================================
// The old basic handler is replaced by the enterprise CrashContainment system:
//   - MiniDump (dbghelp.dll dynamic load)
//   - SelfPatch emergency rollback via PatchRollbackLedger
//   - Full register capture (all 16 GP registers)
//   - Patch quarantine for faulting patches
//   - Structured crash report logging
// Installed via RawrXD::Crash::Install() below in WinMain.
// ============================================================================

// Set Per-Monitor DPI awareness V2 for crisp rendering on high-DPI displays.
// Must be called before any window creation. Win10 1703+.
static void ensureDpiAwareness() {
    typedef BOOL(WINAPI *SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    auto pSet = (SetProcessDpiAwarenessContext_t)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (!pSet) return;
    // PerMonitorV2: correct scaling when moving between monitors, proper child DPI
    pSet(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // ========================================================================
    // CRASH CONTAINMENT — Cathedral Boundary Guard
    // MiniDump + SelfPatch rollback + register capture + patch quarantine
    // ========================================================================
    {
        CreateDirectoryA("crash_dumps", nullptr);

        // Initialize the patch rollback ledger (WAL-journaled)
        auto& ledger = RawrXD::Patch::PatchRollbackLedger::Global();
        ledger.initialize("crash_dumps\\patch_journal.wal");

        RawrXD::Crash::CrashConfig crashCfg;
        memset(&crashCfg, 0, sizeof(crashCfg));
        crashCfg.dumpType = RawrXD::Crash::DumpType::Normal;
        strncpy(crashCfg.dumpDirectory, "crash_dumps", sizeof(crashCfg.dumpDirectory) - 1);
        crashCfg.enableMiniDump = true;
        crashCfg.enablePatchRollback = true;
        crashCfg.enablePatchQuarantine = true;
        crashCfg.showMessageBox = true;
        crashCfg.terminateAfterDump = true;
        crashCfg.onCrashCallback = nullptr;
        crashCfg.callbackUserData = nullptr;
        RawrXD::Crash::Install(crashCfg);
        OutputDebugStringA("[main_win32] Cathedral crash containment boundary installed\n");
    }

    // DPI awareness — before any GUI (Win32 GUI fix)
    ensureDpiAwareness();

    // ========================================================================
    // HEADLESS MODE — Phase 19C
    // If --headless is present, skip all GUI initialization and run the
    // HeadlessIDE surface with console I/O + HTTP server.
    // ========================================================================
    if (hasHeadlessFlag(lpCmdLine)) {
        // Allocate a console for stdout/stderr (WinMain doesn't have one)
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);

        int argc = 0;
        char** argv = nullptr;
        parseCmdLine(lpCmdLine, argc, argv);

        HeadlessIDE headless;
        HeadlessResult r = headless.initialize(argc, argv);
        if (!r.success) {
            if (r.errorCode == 0) return 0;  // --help requested
            fprintf(stderr, "Headless init failed: %s (code %d)\n", r.detail, r.errorCode);
            return r.errorCode;
        }

        int exitCode = headless.run();

        FreeConsole();
        return exitCode;
    }

    // ========================================================================
    // GUI MODE — original Win32IDE path (unchanged)
    // ========================================================================
    // Initialize common controls (toolbar, status bar, tab, tree, list, etc.)
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX),
        ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES |
        ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);
    
    // ========================================================================
    // FIRST-RUN GAUNTLET GATE (Phase 33: Gold Master)
    // On very first launch, run the full system verification gauntlet.
    // If it fails, warn the user but allow them to continue (non-blocking).
    // The flag file prevents re-running on subsequent launches.
    // ========================================================================
    {
        const char* gauntletFlag = "config\\first_run.flag";
        DWORD attrs = GetFileAttributesA(gauntletFlag);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            // First run — execute gauntlet
            GauntletSummary summary = runFinalGauntlet();
            if (!summary.allPassed) {
                char msg[512];
                snprintf(msg, sizeof(msg),
                    "System validation: %d/%d tests passed.\n"
                    "Check the Audit Dashboard (Ctrl+Shift+A) for details.\n\n"
                    "The IDE will continue to start normally.",
                    summary.passed, summary.totalTests);
                MessageBoxA(nullptr, msg,
                    "RawrXD \xe2\x80\x94 First Run Check", MB_OK | MB_ICONWARNING);
            }
            // Create the flag file so we don't re-run
            CreateDirectoryA("config", nullptr);
            HANDLE hFlag = CreateFileA(gauntletFlag, GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFlag != INVALID_HANDLE_VALUE) {
                const char* stamp = RAWRXD_VERSION_STR " " __DATE__ " " __TIME__ "\n";
                DWORD written = 0;
                WriteFile(hFlag, stamp, (DWORD)strlen(stamp), &written, nullptr);
                CloseHandle(hFlag);
            }
        }
    }

    // Command registration is automatic via static AutoRegistrar in
    // unified_command_dispatch.cpp — reads COMMAND_TABLE at startup.

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

    // ========================================================================
    // CROSS-PROCESS STATE SYNC — Phase 36: MMF Initialization
    // Register this Win32IDE process in the shared memory region so that
    // React, CLI, and HTML frontends see our patch/config/model state.
    // ========================================================================
    {
        auto& mmf = RawrXDStateMmf::instance();
        if (!mmf.isInitialized()) {
            PatchResult mmfResult = mmf.initialize(0, "RawrXD-Win32IDE"); // processType=0 (Win32)
            if (mmfResult.success) {
                OutputDebugStringA("[main_win32] MMF cross-process state sync initialized\n");
            } else {
                char err[256];
                snprintf(err, sizeof(err),
                         "[main_win32] MMF init warning: %s (non-fatal)\n",
                         mmfResult.detail ? mmfResult.detail : "unknown");
                OutputDebugStringA(err);
            }
        }
    }

    // ========================================================================
    // JS EXTENSION HOST — Phase 37: QuickJS VSIX Runtime Bootstrap
    // Initialize the embedded JavaScript engine so that .vsix extensions
    // can be loaded and activated from the VSIX panel or drag-and-drop.
    // ========================================================================
    {
        auto& jsHost = JSExtensionHost::instance();
        if (!jsHost.isInitialized()) {
            PatchResult jsResult = jsHost.initialize();
            if (jsResult.success) {
                OutputDebugStringA("[main_win32] JS Extension Host initialized (QuickJS + PolyfillEngine)\n");
            } else {
                char err[256];
                snprintf(err, sizeof(err),
                         "[main_win32] JS Extension Host init warning: %s (non-fatal)\n",
                         jsResult.detail ? jsResult.detail : "unknown");
                OutputDebugStringA(err);
            }
        }
    }

    // Show window and force layout
    ide.showWindow();

    // ========================================================================
    // MASM SUBSYSTEM INITIALIZATION — Cathedral Bridge
    // Initialize all 5 Tier-2 MASM kernel modules via unified bridge.
    // Order matters: SelfPatch → GGUF Loader → LSP Bridge → Orchestrator → QuadBuffer
    // ========================================================================
    {
        OutputDebugStringA("[main_win32] Initializing MASM subsystems...\n");

        HWND editor = ide.getEditor();

        // 1. SelfPatch Engine — must be first (other modules depend on patching)
        int r = asm_spengine_init(0);
        if (r >= 0) {
            OutputDebugStringA("[main_win32] SelfPatch Engine: OK\n");
            asm_spengine_cpu_optimize();
        } else {
            OutputDebugStringA("[main_win32] SelfPatch Engine: FAILED (non-fatal)\n");
        }

        // 2. GGUF Vulkan Loader
        r = asm_gguf_loader_init(nullptr, 0);
        if (r >= 0) {
            OutputDebugStringA("[main_win32] GGUF Vulkan Loader: OK\n");
        } else {
            OutputDebugStringA("[main_win32] GGUF Vulkan Loader: FAILED (non-fatal)\n");
        }

        // 3. LSP AI Bridge
        r = asm_lsp_bridge_init(nullptr, nullptr);
        if (r >= 0) {
            OutputDebugStringA("[main_win32] LSP AI Bridge: OK\n");
        } else {
            OutputDebugStringA("[main_win32] LSP AI Bridge: FAILED (non-fatal)\n");
        }

        // 4. Agentic Orchestrator
        r = asm_orchestrator_init(4, nullptr);
        if (r >= 0) {
            OutputDebugStringA("[main_win32] Agentic Orchestrator: OK\n");
        } else {
            OutputDebugStringA("[main_win32] Agentic Orchestrator: FAILED (non-fatal)\n");
        }

        // 5. Streaming QuadBuffer (needs editor HWND for render target)
        r = asm_quadbuf_init(editor, 4096, 4);
        if (r >= 0) {
            OutputDebugStringA("[main_win32] Streaming QuadBuffer: OK\n");
        } else {
            OutputDebugStringA("[main_win32] Streaming QuadBuffer: FAILED (non-fatal)\n");
        }

        // Initialize quant hysteresis controller with defaults
        auto& hysteresis = RawrXD::Quant::QuantHysteresisController::Global();
        QuantHysteresisConfig hcfg;
        memset(&hcfg, 0, sizeof(hcfg));
        hcfg.windowPct = 3;
        hcfg.cooldownMs = 2000;
        hcfg.thresholdLow = 65;
        hcfg.thresholdHigh = 85;
        hcfg.thresholdCritical = 88;
        hcfg.enableCooldown = true;
        hcfg.enableLogging = true;
        hysteresis.configure(hcfg);

        OutputDebugStringA("[main_win32] All MASM subsystems initialized\n");
    }

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

    // Set focus to the editor so the caret appears and keyboard input works immediately
    {
        HWND editor = ide.getEditor();
        if (editor && IsWindow(editor)) {
            SetFocus(editor);
        }
    }
    
    // Run message loop
    int exitCode = ide.runMessageLoop();

    // ========================================================================
    // CLEANUP — Null out IDE's raw pointers BEFORE deleting external objects.
    // The IDE's onDestroy() already ran (from WM_DESTROY), but the Win32IDE
    // object is still alive on the stack. Clear its dangling pointers first.
    // ========================================================================
    ide.setEngineManager(nullptr);
    ide.setCodexUltimate(nullptr);

    // ========================================================================
    // MASM SUBSYSTEM SHUTDOWN — Reverse order of initialization
    // ========================================================================
    {
        OutputDebugStringA("[main_win32] Shutting down MASM subsystems...\n");
        asm_quadbuf_shutdown(nullptr);
        asm_orchestrator_shutdown();
        asm_lsp_bridge_shutdown();
        asm_gguf_loader_close(nullptr);
        asm_spengine_shutdown();
        OutputDebugStringA("[main_win32] MASM subsystems shutdown complete\n");
    }

    // Shutdown cross-process state and JS extension host
    {
        auto& jsHost = JSExtensionHost::instance();
        if (jsHost.isInitialized()) {
            jsHost.shutdown();
            OutputDebugStringA("[main_win32] JS Extension Host shutdown\n");
        }

        auto& mmf = RawrXDStateMmf::instance();
        if (mmf.isInitialized()) {
            mmf.broadcastEvent(0xFF, "Win32IDE shutting down");
            mmf.shutdown();
            OutputDebugStringA("[main_win32] MMF cross-process state shutdown\n");
        }
    }

    // Cleanup engine resources (IDE no longer holds pointers to these)
    try {
        delete codex;
    } catch (...) {}
    try {
        delete engine_mgr;
    } catch (...) {}

    // ========================================================================
    // CRASH CONTAINMENT UNINSTALL — Cathedral teardown
    // ========================================================================
    {
        auto& ledger = RawrXD::Patch::PatchRollbackLedger::Global();
        ledger.flushJournal();
        ledger.shutdown();
        RawrXD::Crash::Uninstall();
        OutputDebugStringA("[main_win32] Cathedral crash containment uninstalled\n");
    }

    return exitCode;
}
