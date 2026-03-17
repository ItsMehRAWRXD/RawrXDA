#include "Win32IDE.h"
#include "HeadlessIDE.h"
#include "../../include/rawrxd_version.h"
#include "../../include/final_gauntlet.h"
#include "../modules/vsix_loader.h"
#include "../modules/memory_manager.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include "../core/rawrxd_state_mmf.hpp"
#include "../core/js_extension_host.hpp"
#include "../core/model_memory_hotpatch.hpp"
#include <commctrl.h>
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
// Top-level crash handler — writes crash info to file for diagnosis
// ============================================================================
static LONG WINAPI RawrXDCrashHandler(EXCEPTION_POINTERS* ep) {
    char msg[512];
    snprintf(msg, sizeof(msg),
        "CRASH: Exception 0x%08lX at address 0x%p\n",
        ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress);
    OutputDebugStringA(msg);

    // Write crash log to file
    HANDLE hLog = CreateFileA("rawrxd_crash.log", GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hLog != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hLog, msg, (DWORD)strlen(msg), &written, nullptr);
        // Add register context
        char regBuf[512];
        snprintf(regBuf, sizeof(regBuf),
            "RIP=0x%016llX RSP=0x%016llX RBP=0x%016llX\n"
            "RAX=0x%016llX RBX=0x%016llX RCX=0x%016llX\n"
            "RDX=0x%016llX RSI=0x%016llX RDI=0x%016llX\n",
            ep->ContextRecord->Rip, ep->ContextRecord->Rsp, ep->ContextRecord->Rbp,
            ep->ContextRecord->Rax, ep->ContextRecord->Rbx, ep->ContextRecord->Rcx,
            ep->ContextRecord->Rdx, ep->ContextRecord->Rsi, ep->ContextRecord->Rdi);
        WriteFile(hLog, regBuf, (DWORD)strlen(regBuf), &written, nullptr);
        CloseHandle(hLog);
    }

    MessageBoxA(nullptr, msg, "RawrXD IDE — CRASH REPORT", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    SetUnhandledExceptionFilter(RawrXDCrashHandler);

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
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES };
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
    delete codex;
    delete engine_mgr;

    return exitCode;
}
