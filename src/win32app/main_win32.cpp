#include "Win32IDE.h"
#include "HeadlessIDE.h"
#include "../../include/rawrxd_version.h"
#include "../../include/final_gauntlet.h"
#include "../../include/crash_containment.h"
#include "../../include/patch_rollback_ledger.h"
#include "../../include/masm_bridge_cathedral.h"
#include "../../include/reverse_engineered_bridge.h"
#include "../../include/quant_hysteresis.h"
#include "../../include/auto_update_system.h"
#include "../../include/update_signature.h"
#include "../../include/swarm_reconciliation.h"
#include "../../include/quickjs_sandbox.h"
#include "../../include/plugin_signature.h"
#include "../../include/enterprise_stress_tests.h"
#include "../core/enterprise_license.h"
#include "../modules/vsix_loader.h"
#include "../modules/memory_manager.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include "../core/rawrxd_state_mmf.hpp"
#include "../core/js_extension_host.hpp"
#include "../core/model_memory_hotpatch.hpp"
#include "../core/camellia256_bridge.hpp"
#include <commctrl.h>
#include <shellscalingapi.h>
#if defined(_MSC_VER) && defined(_WIN32)
#include <delayimp.h>
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <filesystem>

// ============================================================================
// Startup trace — write to ide_startup.log in exe dir for launch audit
// ============================================================================
static std::ofstream* s_startupLog = nullptr;
static void startupTrace(const char* step, const char* detail = nullptr) {
    if (!s_startupLog) return;
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    (*s_startupLog) << ms << " " << step;
    if (detail && detail[0]) (*s_startupLog) << " " << detail;
    (*s_startupLog) << "\n";
    s_startupLog->flush();
}

// ============================================================================
// Set CWD to exe directory — ensures crash_dumps, config, plugins, engines
// resolve correctly when launched from Explorer, shortcuts, or other CWD.
// ============================================================================
static void setCwdToExeDirectory() {
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) return;
    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos) exeDir = exeDir.substr(0, last);
    if (!exeDir.empty()) SetCurrentDirectoryA(exeDir.c_str());
}

// ============================================================================
// Headless mode detection — scans argv for --headless flag
// ============================================================================
static bool hasHeadlessFlag(LPSTR lpCmdLine) {
    if (!lpCmdLine) return false;
    return strstr(lpCmdLine, "--headless") != nullptr;
}

// ============================================================================
// Safe mode detection — modularize IDE (disable extensions, Vulkan, GPU)
// ============================================================================
static bool hasSafeModeFlag(LPSTR lpCmdLine) {
    if (!lpCmdLine) return false;
    return strstr(lpCmdLine, "--safe-mode") != nullptr;
}

// ============================================================================
// VSIX agentic test — load all .vsix in plugins/, write result file, exit
// ============================================================================
static bool hasVsixTestFlag(LPSTR lpCmdLine) {
    if (!lpCmdLine) return false;
    return strstr(lpCmdLine, "--vsix-test") != nullptr;
}

static std::string jsonEscape(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if ((unsigned char)c >= 32) r += c;
    }
    return r;
}

static int runVsixTestAndExit() {
    // Use exe directory as base so plugins/ is next to RawrXD-Win32IDE.exe
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) return 0;
    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos) exeDir = exeDir.substr(0, last);
    std::string pluginsDir = exeDir + "\\plugins";

    auto& loader = VSIXLoader::GetInstance();
    loader.Initialize(pluginsDir);

    if (!std::filesystem::exists(pluginsDir) || !std::filesystem::is_directory(pluginsDir)) {
        std::ofstream out("vsix_test_result.json");
        if (out) out << "{\"loaded\":[],\"help\":{},\"error\":\"plugins dir missing\"}\n";
        return 0;
    }

    int loadedCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(pluginsDir))) {
        if (!entry.is_regular_file()) continue;
        std::string name = entry.path().filename().string();
        if (name.size() < 6) continue;
        std::string ext = name.substr(name.size() - 5);
        for (auto& c : ext) c = (char)::tolower(c);
        if (ext != ".vsix") continue;
        std::string path = entry.path().string();
        if (loader.LoadPlugin(path)) loadedCount++;
    }
    // Fallback: load from already-extracted extension dirs (e.g. amazonq, github-copilot)
    if (loadedCount == 0) {
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(pluginsDir))) {
            if (!entry.is_directory()) continue;
            std::filesystem::path pkg = entry.path() / "package.json";
            std::filesystem::path extPkg = entry.path() / "extension" / "package.json";
            if (std::filesystem::exists(pkg) || std::filesystem::exists(extPkg)) {
                std::filesystem::path loadRoot = std::filesystem::exists(extPkg) ? (entry.path() / "extension") : entry.path();
                if (loader.LoadPlugin(loadRoot.string())) loadedCount++;
            }
        }
    }

    std::vector<std::string> loaded;
    std::vector<std::pair<std::string, std::string>> helpLines;
    for (auto* pl : loader.GetLoadedPlugins()) {
        loaded.push_back(pl->id);
        helpLines.push_back({ pl->id, loader.GetPluginHelp(pl->id) });
    }

    // If still empty, try loading extracted dirs by full path (path handling fallback)
    if (loaded.empty()) {
        std::filesystem::path pluginsPath(pluginsDir);
        std::string amazonqPath = (pluginsPath / "amazonq").string();
        std::string ghPath = (pluginsPath / "github-copilot" / "extension").string();
        bool a1 = std::filesystem::exists(pluginsPath / "amazonq" / "package.json");
        bool aDir = std::filesystem::is_directory(pluginsPath / "amazonq");
        bool a2 = loader.LoadPlugin(amazonqPath);
        bool b1 = std::filesystem::exists(pluginsPath / "github-copilot" / "extension" / "package.json");
        bool bDir = std::filesystem::is_directory(pluginsPath / "github-copilot" / "extension");
        bool b2 = loader.LoadPlugin(ghPath);
        loaded.clear();
        helpLines.clear();
        for (auto* pl : loader.GetLoadedPlugins()) {
            loaded.push_back(pl->id);
            helpLines.push_back({ pl->id, loader.GetPluginHelp(pl->id) });
        }
        // Debug: write what we tried
        std::ofstream dbg(pluginsDir + "\\vsix_debug.txt");
        if (dbg) dbg << "amazonq exists=" << a1 << " isDir=" << aDir << " load=" << a2
            << " gh exists=" << b1 << " isDir=" << bDir << " load=" << b2 << " count=" << loaded.size() << "\n";
    }

    std::string reportPath = pluginsDir + "\\vsix_test_result.json";
    std::ofstream out(reportPath);
    if (!out) { reportPath = "vsix_test_result.json"; out.open(reportPath); }
    if (out) {
        out << "{\"loaded\":[";
        for (size_t i = 0; i < loaded.size(); ++i) {
            if (i) out << ",";
            out << "\"" << jsonEscape(loaded[i]) << "\"";
        }
        out << "],\"help\":{";
        for (size_t i = 0; i < helpLines.size(); ++i) {
            if (i) out << ",";
            out << "\"" << jsonEscape(helpLines[i].first) << "\":\"" << jsonEscape(helpLines[i].second) << "\"";
        }
        out << "}}\n";
        out.close();
    }
    return 0;
}

// ============================================================================
// Recovery launcher — spawns agent/model to analyze crash and suggest fixes
// ============================================================================
static void spawnRecoveryLauncher(const char* logPath, const char* dumpPath) {
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) return;

    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos) exeDir = exeDir.substr(0, last);

    // Script locations: exe_dir/scripts, exe_dir/../scripts, exe_dir/../../scripts
    const char* candidates[] = {
        "scripts\\CrashRecoveryLauncher.ps1",
        "..\\scripts\\CrashRecoveryLauncher.ps1",
        "..\\..\\scripts\\CrashRecoveryLauncher.ps1",
    };
    std::string scriptPath;
    for (const char* rel : candidates) {
        std::string p = exeDir + "\\" + rel;
        if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
            scriptPath = p;
            break;
        }
    }
    if (scriptPath.empty()) return;

    std::string cmd = "powershell -ExecutionPolicy Bypass -NoProfile -File \"" + scriptPath + "\" -LogPath \"" +
        std::string(logPath ? logPath : "") + "\" -ExePath \"" + std::string(exePath) + "\"";
    if (dumpPath && dumpPath[0])
        cmd += " -DumpPath \"" + std::string(dumpPath) + "\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
                       CREATE_NEW_CONSOLE | DETACHED_PROCESS, nullptr, exeDir.c_str(), &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
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

// Pump pending messages so the window can paint and not appear locked during startup.
static void pumpMessages() {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// Background thread: run Camellia encryptWorkspace so main thread stays responsive.
static DWORD WINAPI camelliaEncryptWorkspaceThread(LPVOID param) {
    char* path = static_cast<char*>(param);
    if (!path) return 1;
    auto& c = RawrXD::Crypto::Camellia256Bridge::instance();
    auto encResult = c.encryptWorkspace(path, false);
    if (encResult.success) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "[main_win32] Workspace encrypted (Camellia-256 CTR): %s\n",
                 encResult.detail ? encResult.detail : "OK");
        OutputDebugStringA(msg);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "[main_win32] Workspace encryption note: %s (non-fatal)\n",
                 encResult.detail ? encResult.detail : "partial");
        OutputDebugStringA(msg);
    }
    free(path);
    return 0;
}

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

#if defined(_MSC_VER) && defined(_WIN32)
// Delay-load failure hook: when vulkan-1.dll or D3DCOMPILER_47.dll is missing,
// show a clear message and exit instead of an unhandled exception on first use.
static FARPROC WINAPI DelayLoadFailureHook(unsigned dliNotify, PDelayLoadInfo pdli) {
    if (dliNotify != dliFailLoadLib && dliNotify != dliFailGetProc) return 0;
    const char* dllName = pdli ? pdli->szDll : "unknown";
    char msg[384];
    snprintf(msg, sizeof(msg),
             "RawrXD IDE could not load a required DLL:\n\n%s\n\n"
             "Install Vulkan Runtime (vulkan-1) or DirectX Redist (D3DCOMPILER_47) if needed, "
             "or run from the build directory.",
             dllName);
    MessageBoxA(nullptr, msg, "RawrXD IDE - Missing DLL", MB_OK | MB_ICONERROR);
    ExitProcess(1);
    return 0;
}
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {
    // ========================================================================
    // CWD FIX — Set working directory to exe's folder (before any relative paths)
    // Required for crash_dumps, config, plugins, engines when launched from
    // Explorer, shortcuts, or different CWD. Prevents silent failures on init.
    // ========================================================================
    setCwdToExeDirectory();

    // Startup trace for launch audit (ide_startup.log in exe dir)
    {
        std::string logPath = "ide_startup.log";
        s_startupLog = new std::ofstream(logPath, std::ios::out | std::ios::trunc);
        if (s_startupLog->is_open())
            startupTrace("WinMain", "start");
        else {
            delete s_startupLog;
            s_startupLog = nullptr;
        }
    }

#if defined(_MSC_VER) && defined(_WIN32)
    (void)DelayLoadFailureHook; /* reserved for delay-load; __pfnDliFailureHook2 is const in MSVC */
#endif

    // Optional: allocate console for early crash diagnostics (RAWRXD_DEBUG_CONSOLE=1)
    {
        char buf[8];
        if (GetEnvironmentVariableA("RAWRXD_DEBUG_CONSOLE", buf, (DWORD)sizeof(buf)) != 0 && buf[0] == '1') {
            AllocConsole();
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
            freopen("CONIN$", "r", stdin);
            fprintf(stderr, "[RawrXD] Debug console enabled\n");
        }
    }
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
        crashCfg.dumpDirectory = "crash_dumps";
        crashCfg.enableMiniDump = true;
        crashCfg.enablePatchRollback = true;
        crashCfg.enablePatchQuarantine = true;
        crashCfg.showMessageBox = true;
        crashCfg.terminateAfterDump = true;
        crashCfg.onCrashCallback = [](const RawrXD::Crash::CrashReport* r, void*) {
            if (r && r->logPath[0]) spawnRecoveryLauncher(r->logPath, r->dumpPath);
        };
        crashCfg.callbackUserData = nullptr;
        RawrXD::Crash::Install(crashCfg);
        OutputDebugStringA("[main_win32] Cathedral crash containment boundary installed\n");
    }
    startupTrace("crash_containment_installed");

    // DPI awareness — before any GUI (Win32 GUI fix)
    ensureDpiAwareness();
    startupTrace("dpi_awareness");

    // ========================================================================
    // HEADLESS MODE — Phase 19C
    // If --headless is present, skip all GUI initialization and run the
    // HeadlessIDE surface with console I/O + HTTP server.
    // ========================================================================
    if (hasHeadlessFlag(lpCmdLine)) {
        if (s_startupLog) { startupTrace("headless_mode"); s_startupLog->close(); delete s_startupLog; s_startupLog = nullptr; }
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
    startupTrace("init_common_controls");
    
    startupTrace("first_run_gauntlet_start");
    // ========================================================================
    // FIRST-RUN GAUNTLET GATE (Phase 33: Gold Master)
    // On very first launch we used to run the full gauntlet and show N/10;
    // that could lead to silent exit after OK (crash in later init). So we
    // skip the gauntlet at startup and create the flag so the IDE always starts.
    // Run "Gauntlet: Run All Tests" from the menu (Tools / Gauntlet) if needed.
    // Set RAWRXD_RUN_FIRST_RUN_GAUNTLET=1 to run gauntlet on first launch again.
    // ========================================================================
    {
        const char* gauntletFlag = "config\\first_run.flag";
        DWORD attrs = GetFileAttributesA(gauntletFlag);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            char envBuf[32];
            const bool runGauntlet = (GetEnvironmentVariableA("RAWRXD_RUN_FIRST_RUN_GAUNTLET", envBuf, (DWORD)sizeof(envBuf)) != 0 && envBuf[0] == '1');
            if (runGauntlet) {
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
            }
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
    startupTrace("first_run_gauntlet_done");

    // Command registration is automatic via static AutoRegistrar in
    // unified_command_dispatch.cpp — reads COMMAND_TABLE at startup.

    // Initialize managers
    VSIXLoader::GetInstance().Initialize("plugins");
    startupTrace("vsix_loader");

    // ========================================================================
    // PLUGIN SIGNATURE ENFORCEMENT — Phase 50: Authenticode + RawrXD Authority
    // Must init before VSIX loading so marketplace installs are gated.
    // ========================================================================
    {
        auto& sigVerifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
        if (sigVerifier.initialize()) {
            OutputDebugStringA("[main_win32] Plugin Signature Verifier initialized (standard policy)\n");
        } else {
            OutputDebugStringA("[main_win32] Plugin Signature Verifier: init failed (non-fatal)\n");
        }
    }
    
    startupTrace("plugin_signature");

    // ========================================================================
    // VSIX AGENTIC TEST — load all .vsix in plugins/, write result JSON, exit
    // ========================================================================
    if (hasVsixTestFlag(lpCmdLine)) {
        if (s_startupLog) { s_startupLog->close(); delete s_startupLog; s_startupLog = nullptr; }
        return runVsixTestAndExit();
    }

    // ========================================================================
    // SAFE MODE — modularize IDE (disable extensions, Vulkan, GPU) when --safe-mode
    // ========================================================================
    if (hasSafeModeFlag(lpCmdLine)) {
        SetEnvironmentVariableA("RAWRXD_SAFE_MODE", "1");
        OutputDebugStringA("[main_win32] Safe mode enabled (--safe-mode)\n");
    }

    startupTrace("creating_ide_instance");
    // Create IDE window FIRST so user sees a window even if later init fails
    Win32IDE ide(hInstance);

    startupTrace("createWindow_start");
    if (!ide.createWindow()) {
        startupTrace("createWindow_FAILED");
        if (s_startupLog) { s_startupLog->close(); delete s_startupLog; s_startupLog = nullptr; }
        MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    startupTrace("createWindow_ok");
    pumpMessages();

    // ========================================================================
    // ENTERPRISE LICENSE — run on background thread so IDE window and message
    // loop start immediately even if license init hangs or crashes.
    // ========================================================================
    startupTrace("enterprise_license_skipped");  // run after message loop to avoid AV in thread

    // Initialize engine manager (safe — LoadEngine may fail for missing DLLs)
    // 800B dual-engine load gated by enterprise unlock (g_800B_Unlocked)
    auto* engine_mgr = new EngineManager();
    if (RawrXD::g_800B_Unlocked) {
        try { engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive"); } catch (...) {}
    }
    try { engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate"); } catch (...) {}
    try { engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler"); } catch (...) {}
    
    // Initialize Codex Ultimate
    auto* codex = new CodexUltimate();
    
    ide.setEngineManager(engine_mgr);
    ide.setCodexUltimate(codex);
    pumpMessages();

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

    // ========================================================================
    // PLUGIN TRUST BOUNDARY — Phase 50: QuickJS Sandbox Enforcement
    // Limits memory, CPU, file-system, network, native calls per security tier.
    // Must init after JS Extension Host so sandbox policies can be applied.
    // ========================================================================
    {
        auto& sandbox = RawrXD::Sandbox::PluginSandbox::instance();
        if (!sandbox.isInitialized()) {
            RawrXD::Sandbox::SandboxResult sbResult = sandbox.initialize();
            if (sbResult.success) {
                OutputDebugStringA("[main_win32] Plugin Sandbox (QuickJS trust boundary) initialized\n");
            } else {
                char err[256];
                snprintf(err, sizeof(err),
                         "[main_win32] Plugin Sandbox init warning: %s (non-fatal)\n",
                         sbResult.detail ? sbResult.detail : "unknown");
                OutputDebugStringA(err);
            }
        }
    }

    // Show window and force layout
    startupTrace("showWindow");
    ide.showWindow();
    pumpMessages();

    // ========================================================================
    // CAMELLIA-256 — run in background so main thread reaches message loop
    // even if init or encryptWorkspace blocks.
    // ========================================================================
    startupTrace("camellia_skipped");
    pumpMessages();

    // ========================================================================
    // MASM + RE KERNEL — skipped on startup to avoid AV in Vulkan/GGUF path
    // killing process. Re-enable from menu or WM_APP+150 when needed.
    // ========================================================================
    startupTrace("masm_init_skipped");

    startupTrace("swarm_skipped");

    // ========================================================================
    // AUTO-UPDATE CHECK — Phase 50: Background manifest fetch + sig verify
    // Non-blocking async check so we don't delay window show.
    // ========================================================================
    {
        auto& updater = RawrXD::Update::AutoUpdateSystem::instance();
        updater.setCurrentVersion(RAWRXD_VERSION_MAJOR, RAWRXD_VERSION_MINOR, RAWRXD_VERSION_PATCH, 0);
        updater.setRepository("RawrXD", "RawrXD-ModelLoader");
        updater.checkForUpdatesAsync([](const RawrXD::Update::UpdateCheckResult* result, void*) {
            if (result && result->updateAvailable) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "[main_win32] Update available: v%u.%u.%u\n",
                         result->latestVersion.major,
                         result->latestVersion.minor,
                         result->latestVersion.patch);
                OutputDebugStringA(msg);
            } else {
                OutputDebugStringA("[main_win32] Auto-update check: up to date\n");
            }
        }, nullptr);
        OutputDebugStringA("[main_win32] Auto-update check initiated (async)\n");
    }
    startupTrace("auto_update_done");

    startupTrace("layout_start");
    {
        HWND hwnd = ide.getMainWindow();
        if (hwnd) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            PostMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
            InvalidateRect(hwnd, nullptr, TRUE);
        }
    }
    startupTrace("layout_done");
    startupTrace("message_loop_entered");

    // Set focus to the editor
    {
        HWND editor = ide.getEditor();
        if (editor && IsWindow(editor))
            SetFocus(editor);
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
    // REVERSE-ENGINEERED KERNEL SHUTDOWN — Before Tier-2 MASM shutdown
    // ========================================================================
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    {
        OutputDebugStringA("[main_win32] Shutting down ReverseEngineered kernel...\n");
        RawrXD::ReverseEngineered::ShutdownAllSubsystems();
        OutputDebugStringA("[main_win32] ReverseEngineered kernel shutdown complete\n");
    }
#endif

    // ========================================================================
    // MASM SUBSYSTEM SHUTDOWN — Reverse order of initialization
    // ========================================================================
    {
        OutputDebugStringA("[main_win32] Shutting down MASM subsystems...\n");
        asm_quadbuf_shutdown();
        asm_orchestrator_shutdown();
        asm_lsp_bridge_shutdown();
        asm_gguf_loader_close(nullptr);
        asm_spengine_shutdown();
        OutputDebugStringA("[main_win32] MASM subsystems shutdown complete\n");
    }

    // ========================================================================
    // CAMELLIA-256 SHUTDOWN — Secure zero all key material
    // ========================================================================
    {
        auto& camellia = RawrXD::Crypto::Camellia256Bridge::instance();
        if (camellia.isInitialized()) {
            auto camStatus = camellia.getStatus();
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "[main_win32] Camellia-256 session stats: %llu blocks enc, %llu dec, %llu files\n",
                     camStatus.blocksEncrypted, camStatus.blocksDecrypted,
                     camStatus.filesProcessed);
            OutputDebugStringA(msg);
            camellia.shutdown();
            OutputDebugStringA("[main_win32] Camellia-256 engine shutdown (keys zeroed)\n");
        }
    }

    // ========================================================================
    // ENTERPRISE SUBSYSTEM SHUTDOWN — Reverse order of boot initialization
    // Swarm → Plugin Sandbox → Plugin Signature → Enterprise License
    // ========================================================================
    {
        auto& reconciler = RawrXD::Swarm::SwarmReconciler::instance();
        if (reconciler.isInitialized()) {
            reconciler.shutdown();
            OutputDebugStringA("[main_win32] Swarm Reconciler shutdown\n");
        }

        auto& sandbox = RawrXD::Sandbox::PluginSandbox::instance();
        if (sandbox.isInitialized()) {
            sandbox.shutdown();
            OutputDebugStringA("[main_win32] Plugin Sandbox shutdown\n");
        }

        auto& sigVerifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
        if (sigVerifier.isInitialized()) {
            sigVerifier.shutdown();
            OutputDebugStringA("[main_win32] Plugin Signature Verifier shutdown\n");
        }
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

    // ========================================================================
    // ENTERPRISE LICENSE SHUTDOWN — Final teardown (after all subsystems)
    // ========================================================================
    {
        RawrXD::EnterpriseLicense::Instance().Shutdown();
        OutputDebugStringA("[main_win32] Enterprise License System shutdown\n");
    }

    return exitCode;
}
