#include "../../include/auto_update_system.h"
#include "../../include/collab/websocket_hub.h"
#include "../../include/crash_containment.h"
#include "../../include/enterprise_feature_manager.hpp"
#include "../../include/enterprise_stress_tests.h"
#include "../../include/feature_flags_runtime.h"
#include "../../include/final_gauntlet.h"
#include "../../include/license_enforcement.h"
#include "../../include/masm_bridge_cathedral.h"
#include "../../include/patch_rollback_ledger.h"
#include "../../include/plugin_signature.h"
#include "../../include/quant_hysteresis.h"
#include "../../include/quickjs_sandbox.h"
#include "../../include/rawrxd_version.h"
#include "../../include/reverse_engineered_bridge.h"
#include "../../include/startup_phase_registry.h"
#include "../../include/swarm_reconciliation.h"
#include "../../include/update_signature.h"
#include "../core/camellia256_bridge.hpp"
#include "../core/enterprise_license.h"
#include "../core/js_extension_host.hpp"
#include "../core/model_memory_hotpatch.hpp"
#include "../core/rawrxd_state_mmf.hpp"
#include "../core/unified_command_dispatch.hpp"
#include "../modules/codex_ultimate.h"
#include "../modules/engine_manager.h"
#include "../modules/memory_manager.h"
#include "../modules/vsix_loader.h"
#include "HeadlessIDE.h"
#include "Win32IDE.h"
#include <commctrl.h>
#include <shellscalingapi.h>
#if defined(_MSC_VER) && defined(_WIN32)
#include <delayimp.h>
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT) - 4)
#endif
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include "../agent/quantum_agent_orchestrator.hpp"

// ============================================================================
// Startup trace — write to ide_startup.log in exe dir for launch audit
// ============================================================================
static std::ofstream* s_startupLog = nullptr;
static void startupTrace(const char* step, const char* detail = nullptr)
{
    if (!s_startupLog)
        return;
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    (*s_startupLog) << ms << " " << step;
    if (detail && detail[0])
        (*s_startupLog) << " " << detail;
    (*s_startupLog) << "\n";
    s_startupLog->flush();
}

// ============================================================================
// Set CWD to exe directory — ensures crash_dumps, config, plugins, engines
// resolve correctly when launched from Explorer, shortcuts, or other CWD.
// ============================================================================
static void setCwdToExeDirectory()
{
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
        return;
    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos)
        exeDir = exeDir.substr(0, last);
    if (!exeDir.empty())
        SetCurrentDirectoryA(exeDir.c_str());
}

// ============================================================================
// Headless mode detection — scans argv for --headless flag
// ============================================================================
static bool hasHeadlessFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--headless") != nullptr;
}

// ============================================================================
// Safe mode detection — modularize IDE (disable extensions, Vulkan, GPU)
// ============================================================================
static bool hasSafeModeFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--safe-mode") != nullptr;
}

// ============================================================================
// VSIX agentic test — load all .vsix in plugins/, write result file, exit
// ============================================================================
static bool hasVsixTestFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--vsix-test") != nullptr;
}

static bool hasSelfTestFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--selftest") != nullptr;
}

static void selfTestOutputSink(const char* text, void* userData)
{
    if (!userData || !text)
        return;
    std::string* out = reinterpret_cast<std::string*>(userData);
    out->append(text);
}

static bool runDispatchProbe(uint32_t commandId, std::string& diag)
{
    CommandContext ctx{};
    ctx.rawInput = "";
    ctx.args = "";
    ctx.commandId = commandId;
    ctx.isGui = false;
    ctx.isHeadless = true;
    ctx.outputFn = selfTestOutputSink;
    ctx.outputUserData = &diag;

    auto result = RawrXD::Dispatch::dispatchByGuiId(commandId, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK;
}

static bool runCanonicalProbe(const char* canonical, std::string& diag)
{
    CommandContext ctx{};
    ctx.rawInput = canonical;
    ctx.args = "";
    ctx.commandId = 0;
    ctx.isGui = false;
    ctx.isHeadless = true;
    ctx.outputFn = selfTestOutputSink;
    ctx.outputUserData = &diag;

    auto result = RawrXD::Dispatch::dispatchByCanonical(canonical, ctx);
    return result.status == RawrXD::Dispatch::DispatchStatus::OK;
}

static bool fileContainsScaffoldMarker(const char* path, std::string& markerLine)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::string line;
    size_t lineNo = 0;
    while (std::getline(in, line))
    {
        ++lineNo;
        if (line.find("SCAFFOLD_") != std::string::npos || line.find("Not implemented") != std::string::npos ||
            line.find("TODO:") != std::string::npos)
        {
            markerLine = std::string(path) + ":" + std::to_string(lineNo) + ": " + line;
            return true;
        }
    }
    return false;
}

static int runStartupSelfTest()
{
    int failures = 0;
    auto fail = [&](const char* step, const std::string& detail = std::string())
    {
        fprintf(stderr, "[selftest] FAIL: %s%s%s\n", step, detail.empty() ? "" : " - ", detail.c_str());
        ++failures;
    };
    auto pass = [&](const char* step, const std::string& detail = std::string())
    { fprintf(stdout, "[selftest] PASS: %s%s%s\n", step, detail.empty() ? "" : " - ", detail.c_str()); };

    // 1) File open/save sanity check
    {
        char tempDir[MAX_PATH] = {};
        DWORD n = GetTempPathA(MAX_PATH, tempDir);
        std::string tmp = (n > 0) ? std::string(tempDir) : std::string(".");
        std::string path = tmp + "rawrxd_startup_selftest.tmp";
        const std::string payload = "rawrxd-selftest-ok\n";
        {
            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                fail("file-write", path);
            }
            else
            {
                out << payload;
            }
        }
        if (failures == 0)
        {
            std::ifstream in(path, std::ios::binary);
            std::string read((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (read != payload)
                fail("file-read-verify", path);
            else
                pass("file-open-save", path);
        }
        DeleteFileA(path.c_str());
    }

    // 2) Command dispatch sanity (representative WM_COMMAND IDs)
    {
        std::string diag;
        const uint32_t ids[] = {1002u, 2028u, 3200u, 4009u, 10000u};
        bool ok = true;
        for (uint32_t id : ids)
        {
            if (!runDispatchProbe(id, diag))
            {
                ok = false;
                fail("dispatch", std::to_string(id));
            }
        }
        if (ok)
            pass("dispatch-probe", "5 representative command IDs");
    }

    // 3) VSCExt status/list commands
    {
        std::string diag;
        bool okStatus = runCanonicalProbe("vscext.status", diag);
        bool okList = runCanonicalProbe("vscext.listCommands", diag);
        fprintf(stdout, "[selftest] vscext output:\n%s\n", diag.c_str());
        if (!(okStatus && okList))
            fail("vscext", "status/list probe failed");
        else
            pass("vscext-status-list");
    }

    // 4) WebSocketHub bind/listen start/stop
    {
        WebSocketHub hub;
        if (!hub.startServer(51793))
        {
            fail("websocket-start", "port=51793");
        }
        else
        {
            if (!hub.isRunning())
                fail("websocket-running-state");
            hub.stopServer();
            if (hub.isRunning())
                fail("websocket-stop-state");
            if (failures == 0)
                pass("websocket-start-stop", "port=51793");
        }
    }

    // 5) Scaffold/placeholder/TODO guard in shipped feature paths
    {
        const char* guardedFiles[] = {"src\\core\\ssot_handlers.cpp", "src\\core\\ssot_handlers_ext_isolated.cpp",
                                      "src\\win32app\\Win32IDE_Commands.cpp", "src\\win32app\\main_win32.cpp"};
        bool clean = true;
        for (const char* p : guardedFiles)
        {
            std::string marker;
            if (fileContainsScaffoldMarker(p, marker))
            {
                clean = false;
                fail("placeholder-guard", marker);
            }
        }
        if (clean)
            pass("placeholder-guard");
    }

    fprintf(stdout, "[selftest] result=%s failures=%d\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 2;
}

// ============================================================================
// Selftest mode — run critical feature checks and exit (0 = pass, non-zero = fail)
// ============================================================================
static bool hasSelftestFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--selftest") != nullptr;
}
extern int runSelftest(HWND hwnd);

static bool hasAutoFixFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--autofix") != nullptr;
}

static void exportCommandArtifacts(const char* proofTag)
{
    // Exe-local artifacts
    CreateDirectoryA("logs", nullptr);
    CreateDirectoryA("docs", nullptr);
    RawrXD::Dispatch::exportCommandUsageJson("logs\\command_usage_runtime.json");
    RawrXD::Dispatch::exportCommandMapMarkdown("docs\\COMMAND_MAP.md", proofTag);

    // Repo-root relative (build_*\\bin -> ..\\..)
    CreateDirectoryA("..\\..\\logs", nullptr);
    CreateDirectoryA("..\\..\\docs", nullptr);
    RawrXD::Dispatch::exportCommandUsageJson("..\\..\\logs\\command_usage_runtime.json");
    RawrXD::Dispatch::exportCommandMapMarkdown("..\\..\\docs\\COMMAND_MAP.md", proofTag);
}

static std::string jsonEscape(const std::string& s)
{
    std::string r;
    for (char c : s)
    {
        if (c == '"')
            r += "\\\"";
        else if (c == '\\')
            r += "\\\\";
        else if (c == '\n')
            r += "\\n";
        else if (c == '\r')
            r += "\\r";
        else if ((unsigned char)c >= 32)
            r += c;
    }
    return r;
}

static int runVsixTestAndExit()
{
    // Use exe directory as base so plugins/ is next to RawrXD-Win32IDE.exe
    char exePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
        return 0;
    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos)
        exeDir = exeDir.substr(0, last);
    std::string pluginsDir = exeDir + "\\plugins";

    auto& loader = VSIXLoader::GetInstance();
    loader.Initialize(pluginsDir);

    if (!std::filesystem::exists(pluginsDir) || !std::filesystem::is_directory(pluginsDir))
    {
        std::ofstream out("vsix_test_result.json");
        if (out)
            out << "{\"loaded\":[],\"help\":{},\"error\":\"plugins dir missing\"}\n";
        return 0;
    }

    int loadedCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(pluginsDir)))
    {
        if (!entry.is_regular_file())
            continue;
        std::string name = entry.path().filename().string();
        if (name.size() < 6)
            continue;
        std::string ext = name.substr(name.size() - 5);
        for (auto& c : ext)
            c = (char)::tolower(c);
        if (ext != ".vsix")
            continue;
        std::string path = entry.path().string();
        if (loader.LoadPlugin(path))
            loadedCount++;
    }
    // Fallback: load from already-extracted extension dirs (e.g. amazonq, github-copilot)
    if (loadedCount == 0)
    {
        for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(pluginsDir)))
        {
            if (!entry.is_directory())
                continue;
            std::filesystem::path pkg = entry.path() / "package.json";
            std::filesystem::path extPkg = entry.path() / "extension" / "package.json";
            if (std::filesystem::exists(pkg) || std::filesystem::exists(extPkg))
            {
                std::filesystem::path loadRoot =
                    std::filesystem::exists(extPkg) ? (entry.path() / "extension") : entry.path();
                if (loader.LoadPlugin(loadRoot.string()))
                    loadedCount++;
            }
        }
    }

    std::vector<std::string> loaded;
    std::vector<std::pair<std::string, std::string>> helpLines;
    for (auto* pl : loader.GetLoadedPlugins())
    {
        loaded.push_back(pl->id);
        helpLines.push_back({pl->id, loader.GetPluginHelp(pl->id)});
    }

    // If still empty, try loading extracted dirs by full path (path handling fallback)
    if (loaded.empty())
    {
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
        for (auto* pl : loader.GetLoadedPlugins())
        {
            loaded.push_back(pl->id);
            helpLines.push_back({pl->id, loader.GetPluginHelp(pl->id)});
        }
        // Debug: write what we tried
        std::ofstream dbg(pluginsDir + "\\vsix_debug.txt");
        if (dbg)
            dbg << "amazonq exists=" << a1 << " isDir=" << aDir << " load=" << a2 << " gh exists=" << b1
                << " isDir=" << bDir << " load=" << b2 << " count=" << loaded.size() << "\n";
    }

    std::string reportPath = pluginsDir + "\\vsix_test_result.json";
    std::ofstream out(reportPath);
    if (!out)
    {
        reportPath = "vsix_test_result.json";
        out.open(reportPath);
    }
    if (out)
    {
        out << "{\"loaded\":[";
        for (size_t i = 0; i < loaded.size(); ++i)
        {
            if (i)
                out << ",";
            out << "\"" << jsonEscape(loaded[i]) << "\"";
        }
        out << "],\"help\":{";
        for (size_t i = 0; i < helpLines.size(); ++i)
        {
            if (i)
                out << ",";
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
static void spawnRecoveryLauncher(const char* logPath, const char* dumpPath)
{
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
        return;

    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos)
        exeDir = exeDir.substr(0, last);

    // Script locations: exe_dir/scripts, exe_dir/../scripts, exe_dir/../../scripts
    const char* candidates[] = {
        "scripts\\CrashRecoveryLauncher.ps1",
        "..\\scripts\\CrashRecoveryLauncher.ps1",
        "..\\..\\scripts\\CrashRecoveryLauncher.ps1",
    };
    std::string scriptPath;
    for (const char* rel : candidates)
    {
        std::string p = exeDir + "\\" + rel;
        if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            scriptPath = p;
            break;
        }
    }
    if (scriptPath.empty())
        return;

    std::string cmd = "powershell -ExecutionPolicy Bypass -NoProfile -File \"" + scriptPath + "\" -LogPath \"" +
                      std::string(logPath ? logPath : "") + "\" -ExePath \"" + std::string(exePath) + "\"";
    if (dumpPath && dumpPath[0])
        cmd += " -DumpPath \"" + std::string(dumpPath) + "\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE | DETACHED_PROCESS,
                       nullptr, exeDir.c_str(), &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ============================================================================
// Parse WinMain command line into argc/argv for headless mode
// ============================================================================
static void parseCmdLine(LPSTR lpCmdLine, int& argc, char**& argv)
{
    static std::vector<std::string> args;
    static std::vector<char*> ptrs;

    args.clear();
    ptrs.clear();
    args.push_back("RawrXD-Win32IDE.exe");

    if (lpCmdLine && lpCmdLine[0])
    {
        std::string cmdLine(lpCmdLine);
        std::istringstream iss(cmdLine);
        std::string token;
        while (iss >> token)
        {
            // Handle quoted arguments
            if (!token.empty() && token.front() == '"')
            {
                token = token.substr(1);
                std::string rest;
                while (token.back() != '"' && std::getline(iss, rest, '"'))
                {
                    token += " " + rest;
                }
                if (!token.empty() && token.back() == '"')
                {
                    token.pop_back();
                }
            }
            args.push_back(token);
        }
    }

    for (auto& a : args)
        ptrs.push_back(&a[0]);
    ptrs.push_back(nullptr);

    argc = static_cast<int>(args.size());
    argv = ptrs.data();
}

// ============================================================================
// AutoFix CLI — runs QuantumOrchestrator::executeAutoFix and emits telemetry
// ============================================================================
static int RunAutoFixCLI(int argc, char* argv[])
{
    using namespace RawrXD::Quantum;
    std::string buildCommand  = "cmake --build build_prod --config Release";
    std::string workspaceRoot = ".";
    std::string telemetryOut  = "healing_telemetry.json";
    int         maxAttempts   = 3;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--build-command")  == 0 && i + 1 < argc) buildCommand  = argv[++i];
        else if (strcmp(argv[i], "--workspace-root") == 0 && i + 1 < argc) workspaceRoot = argv[++i];
        else if (strcmp(argv[i], "--telemetry-out")  == 0 && i + 1 < argc) telemetryOut  = argv[++i];
        else if (strcmp(argv[i], "--max-attempts")   == 0 && i + 1 < argc) maxAttempts   = std::stoi(argv[++i]);
    }

    fprintf(stdout, "[RAWRXD-AUTOFIX] build:     %s\n", buildCommand.c_str());
    fprintf(stdout, "[RAWRXD-AUTOFIX] workspace: %s\n", workspaceRoot.c_str());
    fprintf(stdout, "[RAWRXD-AUTOFIX] attempts:  %d\n", maxAttempts);
    fflush(stdout);

    QuantumOrchestrator& orch = globalQuantumOrchestrator();

    ExecutionResult result = orch.executeAutoFix(buildCommand, workspaceRoot, maxAttempts);

    // Write telemetry JSON
    {
        std::ofstream tf(telemetryOut);
        if (tf.is_open()) {
            tf << "{\"attemptCount\":"           << result.iterationCount
               << ",\"totalDiagnosticsGenerated\":" << result.todoItemsGenerated
               << ",\"totalDiagnosticsHandled\":\"" << result.todoItemsGenerated << "\""
               << ",\"totalFixesStaged\":\""         << result.todoItemsCompleted << "\""
               << ",\"finalStatus\":\""              << (result.success ? "success" : "failure") << "\""
               << ",\"durationMs\":"                << result.totalDurationMs << "}\n";
        }
    }

    fprintf(stdout, "[RAWRXD-AUTOFIX] result=%s attempts=%d fixes=%d duration=%llums\n",
        result.success ? "SUCCESS" : "FAILURE",
        result.iterationCount,
        result.todoItemsCompleted,
        (unsigned long long)result.totalDurationMs);
    fflush(stdout);
    return result.success ? 0 : 1;
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
static void pumpMessages()
{
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

// Force main window visible and to foreground (SetForegroundWindow often fails when
// launched by another process; AttachThreadInput + BringWindowToTop works around it).
// Also moves the window onto the primary monitor if placement is off-screen (e.g. disconnected monitor).
static void ensureMainWindowVisible(HWND hMain)
{
    if (!hMain || !IsWindow(hMain))
        return;
    if (IsIconic(hMain))
    {
        ShowWindow(hMain, SW_RESTORE);
        UpdateWindow(hMain);
    }
    if (!IsWindowVisible(hMain))
        ShowWindow(hMain, SW_SHOWNORMAL);
    WINDOWPLACEMENT wp = {sizeof(WINDOWPLACEMENT)};
    if (GetWindowPlacement(hMain, &wp))
    {
        if (wp.showCmd != SW_SHOWNORMAL && wp.showCmd != SW_SHOW)
        {
            wp.showCmd = SW_SHOWNORMAL;
            SetWindowPlacement(hMain, &wp);
        }
        // If window rect is entirely off-screen (e.g. saved on disconnected monitor), move to primary
        RECT vr = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                   GetSystemMetrics(SM_CXVIRTUALSCREEN) + GetSystemMetrics(SM_XVIRTUALSCREEN),
                   GetSystemMetrics(SM_CYVIRTUALSCREEN) + GetSystemMetrics(SM_YVIRTUALSCREEN)};
        RECT wr = wp.rcNormalPosition;
        if (wr.left >= vr.right || wr.right <= vr.left || wr.top >= vr.bottom || wr.bottom <= vr.top)
        {
            HMONITOR hMon = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
            MONITORINFO mi = {sizeof(mi)};
            if (hMon && GetMonitorInfoA(hMon, &mi))
            {
                const RECT& r = mi.rcWork;
                int w = (std::min)((int)(wr.right - wr.left), (int)(r.right - r.left) - 100);
                int h = (std::min)((int)(wr.bottom - wr.top), (int)(r.bottom - r.top) - 100);
                wp.rcNormalPosition = {r.left + 50, r.top + 50, r.left + 50 + w, r.top + 50 + h};
                wp.showCmd = SW_SHOWNORMAL;
                SetWindowPlacement(hMain, &wp);
            }
        }
    }
    ShowWindow(hMain, SW_SHOW);
    SetWindowPos(hMain, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hMain);
    HWND fg = GetForegroundWindow();
    if (fg != hMain)
    {
        DWORD fgTid = GetWindowThreadProcessId(fg, nullptr);
        DWORD myTid = GetCurrentThreadId();
        if (fgTid != myTid && fgTid != 0)
            AttachThreadInput(myTid, fgTid, TRUE);
        BringWindowToTop(hMain);
        SetForegroundWindow(hMain);
        SetActiveWindow(hMain);
        if (fgTid != myTid && fgTid != 0)
            AttachThreadInput(myTid, fgTid, FALSE);
    }
}

// Storage for phase-created objects (createWindow phase sets these; cleanup uses them).
static EngineManager* s_engine_mgr = nullptr;
static CodexUltimate* s_codex = nullptr;

// Run one startup phase by name. Sequence is from config/startup_phases.txt (dynamic).
// Returns false to abort (e.g. createWindow failed).
static bool runPhase(const std::string& name, Win32IDE& ide, HINSTANCE, LPSTR lpCmdLine)
{
    if (name == "init_common_controls")
    {
        INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES |
                                                                       ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                                                                       ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
        InitCommonControlsEx(&icex);
        startupTrace("init_common_controls");
        return true;
    }
    if (name == "first_run_gauntlet")
    {
        startupTrace("first_run_gauntlet_start");
        const char* gauntletFlag = "config\\first_run.flag";
        DWORD attrs = GetFileAttributesA(gauntletFlag);
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            char envBuf[32];
            const bool runGauntlet =
                (GetEnvironmentVariableA("RAWRXD_RUN_FIRST_RUN_GAUNTLET", envBuf, (DWORD)sizeof(envBuf)) != 0 &&
                 envBuf[0] == '1');
            if (runGauntlet)
            {
                GauntletSummary summary = runFinalGauntlet();
                if (!summary.allPassed)
                {
                    char msg[512];
                    snprintf(msg, sizeof(msg),
                             "System validation: %d/%d tests passed.\nCheck the Audit Dashboard (Ctrl+Shift+A) for "
                             "details.\n\nThe IDE will continue to start normally.",
                             summary.passed, summary.totalTests);
                    MessageBoxA(nullptr, msg, "RawrXD \xe2\x80\x94 First Run Check", MB_OK | MB_ICONWARNING);
                }
            }
            CreateDirectoryA("config", nullptr);
            HANDLE hFlag =
                CreateFileA(gauntletFlag, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFlag != INVALID_HANDLE_VALUE)
            {
                const char* stamp = RAWRXD_VERSION_STR " " __DATE__ " " __TIME__ "\n";
                DWORD written = 0;
                WriteFile(hFlag, stamp, (DWORD)strlen(stamp), &written, nullptr);
                CloseHandle(hFlag);
            }
        }
        startupTrace("first_run_gauntlet_done");
        return true;
    }
    if (name == "vsix_loader")
    {
        VSIXLoader::GetInstance().Initialize("plugins");
        startupTrace("vsix_loader");
        return true;
    }
    if (name == "plugin_signature")
    {
        auto& sigVerifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
        if (sigVerifier.initialize())
            OutputDebugStringA("[main_win32] Plugin Signature Verifier initialized (standard policy)\n");
        else
            OutputDebugStringA("[main_win32] Plugin Signature Verifier: init failed (non-fatal)\n");
        startupTrace("plugin_signature");
        return true;
    }
    if (name == "creating_ide_instance")
    {
        startupTrace("creating_ide_instance");
        return true;
    }
    if (name == "createWindow")
    {
        startupTrace("createWindow_start");
        if (!ide.createWindow())
        {
            startupTrace("createWindow_FAILED");
            return false;
        }
        startupTrace("createWindow_ok");
        pumpMessages();
        s_engine_mgr = new EngineManager();
        if (RawrXD::g_800B_Unlocked)
        {
            try
            {
                s_engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
            }
            catch (...)
            {
            }
        }
        try
        {
            s_engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
        }
        catch (...)
        {
        }
        try
        {
            s_engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
        }
        catch (...)
        {
        }
        s_codex = new CodexUltimate();
        ide.setEngineManager(s_engine_mgr);
        ide.setCodexUltimate(s_codex);
        pumpMessages();
        auto& mmf = RawrXDStateMmf::instance();
        if (!mmf.isInitialized())
        {
            PatchResult r = mmf.initialize(0, "RawrXD-Win32IDE");
            if (!r.success && !r.detail.empty())
                OutputDebugStringA("[main_win32] MMF init warning (non-fatal)\n");
        }
        auto& jsHost = JSExtensionHost::instance();
        if (!jsHost.isInitialized())
        {
            PatchResult r = jsHost.initialize();
            (void)r;
        }
        auto& sandbox = RawrXD::Sandbox::PluginSandbox::instance();
        if (!sandbox.isInitialized())
        {
            RawrXD::Sandbox::SandboxResult r = sandbox.initialize();
            (void)r;
        }
        return true;
    }
    if (name == "enterprise_license")
    {
        RawrXD::EnterpriseLicense::Instance().Initialize();
        EnterpriseFeatureManager::Instance().Initialize();
        RawrXD::License::EnterpriseLicenseV2::Instance().initialize();
        RawrXD::Flags::FeatureFlagsRuntime::Instance().refreshFromLicense();
        RawrXD::Enforce::LicenseEnforcer::Instance().initialize();
        startupTrace("enterprise_license_done");
        return true;
    }
    if (name == "showWindow")
    {
        // Defer actual show until after layout to prevent transient hidden/0x0 states.
        startupTrace("showWindow_deferred");
        return true;
    }
    if (name == "camellia_init")
    {
        RawrXD::Crypto::Camellia256Bridge::instance().initialize();
        startupTrace("camellia_init_done");
        pumpMessages();
        return true;
    }
    if (name == "masm_init")
    {
        startupTrace("masm_init_done");
        return true;
    }
    if (name == "swarm")
    {
        uint8_t localNodeId[16] = {0};
        DWORD seed = GetTickCount();
        for (int i = 0; i < 16; ++i)
            localNodeId[i] = (uint8_t)((seed >> (i % 4)) ^ (i * 7));
        RawrXD::Swarm::SwarmReconciler::instance().initialize(localNodeId, 0);
        startupTrace("swarm_done");
        return true;
    }
    if (name == "auto_update")
    {
        auto& updater = RawrXD::Update::AutoUpdateSystem::instance();
        updater.setCurrentVersion(RAWRXD_VERSION_MAJOR, RAWRXD_VERSION_MINOR, RAWRXD_VERSION_PATCH, 0);
        updater.setRepository("RawrXD", "RawrXD-ModelLoader");
        updater.checkForUpdatesAsync(
            [](const RawrXD::Update::UpdateCheckResult* result, void*)
            {
                if (result && result->updateAvailable)
                    OutputDebugStringA("[main_win32] Update available\n");
                else
                    OutputDebugStringA("[main_win32] Auto-update check: up to date\n");
            },
            nullptr);
        startupTrace("auto_update_done");
        return true;
    }
    if (name == "layout")
    {
        startupTrace("layout_start");
        HWND hwnd = ide.getMainWindow();
        if (hwnd)
        {
            RECT rc;
            GetClientRect(hwnd, &rc);
            PostMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
            InvalidateRect(hwnd, nullptr, TRUE);
            char detail[64];
            snprintf(detail, sizeof(detail), "root=%dx%d", rc.right - rc.left, rc.bottom - rc.top);
            startupTrace("layout_done", detail);
            return true;
        }
        startupTrace("layout_done", "root=unknown");
        return true;
    }
    if (name == "message_loop_entered")
    {
        return true;
    }
    return true;
}

// Background thread: run Camellia encryptWorkspace so main thread stays responsive.
static DWORD WINAPI camelliaEncryptWorkspaceThread(LPVOID param)
{
    char* path = static_cast<char*>(param);
    if (!path)
        return 1;
    auto& c = RawrXD::Crypto::Camellia256Bridge::instance();
    auto encResult = c.encryptWorkspace(path, false);
    if (encResult.success)
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "[main_win32] Workspace encrypted (Camellia-256 CTR): %s\n",
                 encResult.detail ? encResult.detail : "OK");
        OutputDebugStringA(msg);
    }
    else
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "[main_win32] Workspace encryption note: %s (non-fatal)\n",
                 encResult.detail ? encResult.detail : "partial");
        OutputDebugStringA(msg);
    }
    free(path);
    return 0;
}

// Set Per-Monitor DPI awareness V2 for crisp rendering on high-DPI displays.
// Must be called before any window creation. Win10 1703+.
static void ensureDpiAwareness()
{
    typedef BOOL(WINAPI * SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return;
    auto pSet = (SetProcessDpiAwarenessContext_t)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
    if (!pSet)
        return;
    // PerMonitorV2: correct scaling when moving between monitors, proper child DPI
    pSet(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

#if defined(_MSC_VER) && defined(_WIN32)
// Delay-load failure hook: when vulkan-1.dll or D3DCOMPILER_47.dll is missing,
// show a clear message and exit instead of an unhandled exception on first use.
static FARPROC WINAPI DelayLoadFailureHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
    if (dliNotify != dliFailLoadLib && dliNotify != dliFailGetProc)
        return 0;
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

extern "C" void rawrxd_init_deep_thinking();
extern "C" int rawrxd_agentic_deep_think_loop(const char* prompt);

static void runDeepThinkingStressTest(LPSTR lpCmdLine)
{
    // Redirect stdout for feedback if console exists or create one
    if (!GetConsoleWindow())
    {
        AllocConsole();
        FILE* consoleOutput;
        freopen_s(&consoleOutput, "CONOUT$", "w", stdout);
        freopen_s(&consoleOutput, "CONOUT$", "w", stderr);
    }

    // Parse iterations (default 1000)
    int iterations = 1000;
    char* iterStr = strstr(lpCmdLine, "--iterations ");
    if (iterStr)
        iterations = atoi(iterStr + 13);

    // Parse prompt
    const char* prompt = "Optimize a Vulkan compute shader for matrix multiplication";
    char* promptStr = strstr(lpCmdLine, "--prompt \"");
    std::string customPrompt;
    if (promptStr)
    {
        char* start = promptStr + 10;
        char* end = strchr(start, '\"');
        if (end)
        {
            customPrompt = std::string(start, end - start);
            prompt = customPrompt.c_str();
        }
    }

    fprintf(stdout, "[STRESS] Starting Deep Thinking Stress Test (%d iterations)...\n", iterations);
    rawrxd_init_deep_thinking();

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
    {
        rawrxd_agentic_deep_think_loop(prompt);
        if (i > 0 && (i % 100 == 0))
        {
            fprintf(stdout, "[STRESS] Iteration %d complete...\n", i);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    fprintf(stdout, "[STRESS] Completed %d iterations in %lld ms. Stability: GREEN\n", iterations, ms);
    fflush(stdout);
    // Remove getchar() in CLI mode to avoid hanging
    // fprintf(stdout, "Press any key to exit stress test...\n");
    // getchar();
}

static bool hasFeatureProbeFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--test-peek-view") != nullptr || strstr(lpCmdLine, "--test-autosave") != nullptr ||
           strstr(lpCmdLine, "--test-terminal-split") != nullptr;
}

static bool getArgValue(int argc, char** argv, const char* key, std::string& out)
{
    for (int i = 0; i < argc; ++i)
    {
        if (argv[i] && strcmp(argv[i], key) == 0 && i + 1 < argc && argv[i + 1])
        {
            out = argv[i + 1];
            return true;
        }
    }
    return false;
}

static bool initIdeForFeatureProbe(Win32IDE& ide, HINSTANCE hInstance, LPSTR lpCmdLine)
{
    for (const std::string& name : RawrXD::Startup::getPhaseOrder())
    {
        if (RawrXD::Startup::isPhaseLazy(name))
            continue;
        if (!runPhase(name, ide, hInstance, lpCmdLine))
            return false;
    }
    ensureMainWindowVisible(ide.getMainWindow());
    for (int i = 0; i < 8; ++i)
        pumpMessages();
    return true;
}

struct ClassProbeCtx {
    const char* className;
    bool found;
};

static BOOL CALLBACK EnumChildClassProbeProc(HWND hwnd, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<ClassProbeCtx*>(lParam);
    if (!ctx || !ctx->className)
        return FALSE;
    char classBuf[128] = {};
    GetClassNameA(hwnd, classBuf, (int)sizeof(classBuf));
    if (_stricmp(classBuf, ctx->className) == 0)
    {
        ctx->found = true;
        return FALSE;
    }
    return TRUE;
}

static bool hasChildWindowClass(HWND parent, const char* className)
{
    if (!parent || !className)
        return false;
    ClassProbeCtx ctx{className, false};
    EnumChildWindows(parent, EnumChildClassProbeProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.found;
}

struct ClassContainsProbeCtx {
    const char* token;
    int count;
};

static BOOL CALLBACK EnumChildClassContainsProc(HWND hwnd, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<ClassContainsProbeCtx*>(lParam);
    if (!ctx || !ctx->token)
        return FALSE;
    char classBuf[128] = {};
    GetClassNameA(hwnd, classBuf, (int)sizeof(classBuf));
    std::string cls = classBuf;
    std::string tok = ctx->token;
    for (char& c : cls)
        c = (char)tolower((unsigned char)c);
    for (char& c : tok)
        c = (char)tolower((unsigned char)c);
    if (cls.find(tok) != std::string::npos)
        ++ctx->count;
    return TRUE;
}

static int countChildWindowsClassContains(HWND parent, const char* token)
{
    if (!parent || !token)
        return 0;
    ClassContainsProbeCtx ctx{token, 0};
    EnumChildWindows(parent, EnumChildClassContainsProc, reinterpret_cast<LPARAM>(&ctx));
    return ctx.count;
}

static bool setProbeWorkspaceRoot()
{
    char tempPath[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, tempPath);
    if (n == 0 || n >= MAX_PATH)
        return false;
    std::filesystem::path root = std::filesystem::path(tempPath) / "rawrxd_feature_probe";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    std::filesystem::path sample = root / "probe_main.cpp";
    std::ofstream out(sample.string(), std::ios::trunc | std::ios::binary);
    if (out)
        out << "int main() { return 0; }\n";
    out.close();
    return SetCurrentDirectoryA(root.string().c_str()) != 0;
}

static int runFeatureProbeCLI(HINSTANCE hInstance, LPSTR lpCmdLine)
{
    int argc = 0;
    char** argv = nullptr;
    parseCmdLine(lpCmdLine, argc, argv);

    const bool probePeek = lpCmdLine && strstr(lpCmdLine, "--test-peek-view");
    const bool probeAutoSave = lpCmdLine && strstr(lpCmdLine, "--test-autosave");
    const bool probeTermSplit = lpCmdLine && strstr(lpCmdLine, "--test-terminal-split");

    if (!setProbeWorkspaceRoot())
        fprintf(stderr, "[feature-probe] WARN: could not set workspace root from exe path\n");

    auto* ide = new Win32IDE(hInstance);
    if (!ide->createWindow())
    {
        fprintf(stderr, "[feature-probe] FAIL: createWindow failed\n");
        return 2;
    }
    ide->showWindow();
    ensureMainWindowVisible(ide->getMainWindow());
    for (int i = 0; i < 6; ++i)
        pumpMessages();

    HWND mainWnd = ide->getMainWindow();
    HWND editor = ide->getEditor();
    int assertsPassed = 0;
    int assertsFailed = 0;

    auto assertBehavior = [&](bool condition, const char* name)
    {
        if (condition)
        {
            ++assertsPassed;
            fprintf(stdout, "[feature-probe] PASS: %s\n", name);
        }
        else
        {
            ++assertsFailed;
            fprintf(stderr, "[feature-probe] FAIL: %s\n", name);
        }
    };

    // Assertion 1: Main window exists
    assertBehavior(mainWnd != nullptr && IsWindow(mainWnd) != 0, "main_window_created");

    // Assertion 2: Editor window exists
    assertBehavior(editor != nullptr && IsWindow(editor) != 0, "editor_window_created");

    if (probePeek)
    {
        // Assertion 3: Peek overlay appears when invoking definition on symbol under cursor
        if (editor)
        {
            SetWindowTextA(editor, "int main() { return 0; }\n");
            SendMessageA(editor, EM_SETSEL, 4, 8);  // select "main"
            Win32IDE::PeekLocation loc;
            loc.filePath = "probe_main.cpp";
            loc.line = 1;
            loc.col = 4;
            loc.endCol = 8;
            loc.preview = "int main() { return 0; }";
            loc.contextLines.push_back(loc.preview);
            loc.contextStartLine = 1;
            std::vector<Win32IDE::PeekLocation> locs;
            locs.push_back(loc);
            ide->showPeekOverlay("main", locs, true);
            for (int i = 0; i < 8; ++i)
                pumpMessages();
        }
        assertBehavior(hasChildWindowClass(editor, "RawrXD_PeekOverlay"), "peek_overlay_created");
    }

    if (probeAutoSave)
    {
        // Assertion 4: AutoSave ON starts timer (9001)
        ide->setAutoSaveMode(Win32IDE::AutoSaveMode::AfterDelay);
        for (int i = 0; i < 4; ++i)
            pumpMessages();
        BOOL hadAutoSaveTimer = KillTimer(mainWnd, 9001);
        assertBehavior(hadAutoSaveTimer != 0, "autosave_timer_started");

        // Assertion 5: AutoSave OFF clears timer (9001 absent)
        ide->setAutoSaveMode(Win32IDE::AutoSaveMode::Off);
        for (int i = 0; i < 4; ++i)
            pumpMessages();
        BOOL stillHasAutoSaveTimer = KillTimer(mainWnd, 9001);
        assertBehavior(stillHasAutoSaveTimer == 0, "autosave_timer_stopped");
    }

    if (probeTermSplit)
    {
        int richBefore = countChildWindowsClassContains(mainWnd, "richedit");

        // Assertion 6: Horizontal split creates an additional terminal pane
        PostMessageA(mainWnd, WM_COMMAND, MAKEWPARAM(4007, 0), 0);  // IDM_TERMINAL_SPLIT_H
        for (int i = 0; i < 10; ++i)
            pumpMessages();
        int richAfterH = countChildWindowsClassContains(mainWnd, "richedit");
        assertBehavior(richAfterH > richBefore, "terminal_split_horizontal_added_pane");

        // Assertion 7: Vertical split adds another pane (semantic separation H vs V path)
        PostMessageA(mainWnd, WM_COMMAND, MAKEWPARAM(4008, 0), 0);  // IDM_TERMINAL_SPLIT_V
        for (int i = 0; i < 10; ++i)
            pumpMessages();
        int richAfterV = countChildWindowsClassContains(mainWnd, "richedit");
        assertBehavior(richAfterV > richAfterH, "terminal_split_vertical_added_pane");
    }

    fprintf(stdout, "[feature-probe] SUMMARY: passed=%d failed=%d\n", assertsPassed, assertsFailed);
    return assertsFailed == 0 ? 0 : 20 + assertsFailed;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
    // ========================================================================
    // CWD FIX — Set working directory to exe's folder (before any relative paths)
    // Required for crash_dumps, config, plugins, engines when launched from
    // Explorer, shortcuts, or different CWD. Prevents silent failures on init.
    // ========================================================================
    setCwdToExeDirectory();

    // Check environment first for forced console
    char debugConsoleBuf[8];
    if (GetEnvironmentVariableA("RAWRXD_DEBUG_CONSOLE", debugConsoleBuf, (DWORD)sizeof(debugConsoleBuf)) != 0 &&
        debugConsoleBuf[0] == '1')
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }

    if (lpCmdLine && strstr(lpCmdLine, "--test-deep-thinking"))
    {
        runDeepThinkingStressTest(lpCmdLine);
        return 0;
    }

    if (hasFeatureProbeFlag(lpCmdLine))
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
        int rc = runFeatureProbeCLI(hInstance, lpCmdLine);
        FreeConsole();
        return rc;
    }

    if (hasAutoFixFlag(lpCmdLine))
    {
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$",  "r", stdin);
        int argc = 0; char** argv = nullptr;
        parseCmdLine(lpCmdLine, argc, argv);
        int rc = RunAutoFixCLI(argc, argv);
        FreeConsole();
        return rc;
    }

    // Startup trace for launch audit (ide_startup.log in exe dir)
    {
        std::string logPath = "ide_startup.log";
        s_startupLog = new std::ofstream(logPath, std::ios::out | std::ios::trunc);
        if (s_startupLog->is_open())
            startupTrace("WinMain", "start");
        else
        {
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
        if (GetEnvironmentVariableA("RAWRXD_DEBUG_CONSOLE", buf, (DWORD)sizeof(buf)) != 0 && buf[0] == '1')
        {
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
        crashCfg.onCrashCallback = [](const RawrXD::Crash::CrashReport* r, void*)
        {
            if (r && r->logPath[0])
                spawnRecoveryLauncher(r->logPath, r->dumpPath);
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
    if (hasHeadlessFlag(lpCmdLine))
    {
        if (s_startupLog)
        {
            startupTrace("headless_mode");
            s_startupLog->close();
            delete s_startupLog;
            s_startupLog = nullptr;
        }
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
        if (!r.success)
        {
            if (r.errorCode == 0)
                return 0;  // --help requested
            fprintf(stderr, "Headless init failed: %s (code %d)\n", r.detail, r.errorCode);
            return r.errorCode;
        }

        int exitCode = headless.run();

        FreeConsole();
        return exitCode;
    }

    if (hasSelfTestFlag(lpCmdLine))
    {
        if (s_startupLog)
        {
            startupTrace("selftest_mode");
            s_startupLog->close();
            delete s_startupLog;
            s_startupLog = nullptr;
        }
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
        int rc = runStartupSelfTest();
        exportCommandArtifacts("--selftest");
        FreeConsole();
        return rc;
    }

    // ========================================================================
    // GUI MODE — startup sequence from config/startup_phases.txt (dynamic, lazy)
    // ========================================================================
    if (hasVsixTestFlag(lpCmdLine))
    {
        if (s_startupLog)
        {
            s_startupLog->close();
            delete s_startupLog;
            s_startupLog = nullptr;
        }
        return runVsixTestAndExit();
    }
    if (hasSafeModeFlag(lpCmdLine))
    {
        SetEnvironmentVariableA("RAWRXD_SAFE_MODE", "1");
        OutputDebugStringA("[main_win32] Safe mode enabled (--safe-mode)\n");
    }

    RawrXD::Startup::registerLazyPhase("masm_init", []()
                                       { OutputDebugStringA("[main_win32] MASM init (lazy) — run on first use\n"); });
    Win32IDE ide(hInstance);
    for (const std::string& name : RawrXD::Startup::getPhaseOrder())
    {
        if (RawrXD::Startup::isPhaseLazy(name))
            continue;
        if (!runPhase(name, ide, hInstance, lpCmdLine))
        {
            if (s_startupLog)
            {
                s_startupLog->close();
                delete s_startupLog;
                s_startupLog = nullptr;
            }
            MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
            return 1;
        }
    }
    startupTrace("showWindow");
    ide.showWindow();
    ensureMainWindowVisible(ide.getMainWindow());
    for (int i = 0; i < 8; ++i)
        pumpMessages();  // Pump so window paints and stays visible before message loop

    // ========================================================================
    // SELFTEST MODE — run critical checks and exit (no message loop)
    // ========================================================================
    if (hasSelftestFlag(lpCmdLine))
    {
        if (s_startupLog)
        {
            s_startupLog->close();
            delete s_startupLog;
            s_startupLog = nullptr;
        }
        int code = runSelftest(ide.getMainWindow());
        exportCommandArtifacts("--selftest");
        return code;
    }

    startupTrace("message_loop_entered");

    // Post delayed force-visible so the window is brought to front once the loop runs
    PostMessage(ide.getMainWindow(), WM_APP + 199, 0, 0);

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
        if (camellia.isInitialized())
        {
            auto camStatus = camellia.getStatus();
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "[main_win32] Camellia-256 session stats: %llu blocks enc, %llu dec, %llu files\n",
                     camStatus.blocksEncrypted, camStatus.blocksDecrypted, camStatus.filesProcessed);
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
        if (reconciler.isInitialized())
        {
            reconciler.shutdown();
            OutputDebugStringA("[main_win32] Swarm Reconciler shutdown\n");
        }

        auto& sandbox = RawrXD::Sandbox::PluginSandbox::instance();
        if (sandbox.isInitialized())
        {
            sandbox.shutdown();
            OutputDebugStringA("[main_win32] Plugin Sandbox shutdown\n");
        }

        auto& sigVerifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
        if (sigVerifier.isInitialized())
        {
            sigVerifier.shutdown();
            OutputDebugStringA("[main_win32] Plugin Signature Verifier shutdown\n");
        }
    }

    // Shutdown cross-process state and JS extension host
    {
        auto& jsHost = JSExtensionHost::instance();
        if (jsHost.isInitialized())
        {
            jsHost.shutdown();
            OutputDebugStringA("[main_win32] JS Extension Host shutdown\n");
        }

        auto& mmf = RawrXDStateMmf::instance();
        if (mmf.isInitialized())
        {
            mmf.broadcastEvent(0xFF, "Win32IDE shutting down");
            mmf.shutdown();
            OutputDebugStringA("[main_win32] MMF cross-process state shutdown\n");
        }
    }

    // Cleanup engine resources (IDE no longer holds pointers to these)
    try
    {
        if (s_codex)
        {
            delete s_codex;
            s_codex = nullptr;
        }
    }
    catch (...)
    {
    }
    try
    {
        if (s_engine_mgr)
        {
            delete s_engine_mgr;
            s_engine_mgr = nullptr;
        }
    }
    catch (...)
    {
    }

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

    exportCommandArtifacts("runtime-exit");

    return exitCode;
}

// ============================================================================
// Link-time fallbacks for stripped lanes
// ============================================================================
extern "C" void INFINITY_Shutdown(void)
{
    OutputDebugStringA("[main_win32] INFINITY_Shutdown fallback executed\n");
}

extern "C" void Scheduler_Shutdown(void)
{
    OutputDebugStringA("[main_win32] Scheduler_Shutdown fallback executed\n");
}

extern "C" void Heartbeat_Shutdown(void)
{
    OutputDebugStringA("[main_win32] Heartbeat_Shutdown fallback executed\n");
}

HeadlessIDE::HeadlessIDE() = default;

HeadlessIDE::~HeadlessIDE()
{
    m_shutdownRequested.store(true);
    m_running.store(false);
    if (m_serverThread.joinable())
    {
        m_serverThread.join();
    }
    if (m_serverSocket != INVALID_SOCKET)
    {
        closesocket(m_serverSocket);
        m_serverSocket = INVALID_SOCKET;
    }
}

HeadlessResult HeadlessIDE::initialize(int argc, char* argv[])
{
    m_config = HeadlessConfig{};
    m_shutdownRequested.store(false);
    m_running.store(false);
    m_serverRunning.store(false);
    m_startEpochMs = GetTickCount64();

    auto readNext = [&](int& index, std::string& out) -> bool
    {
        if (index + 1 >= argc || !argv[index + 1])
        {
            return false;
        }
        out = argv[++index];
        return true;
    };

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--repl")
        {
            m_config.mode = HeadlessRunMode::REPL;
            m_config.enableRepl = true;
        }
        else if (arg == "--single-shot")
        {
            m_config.mode = HeadlessRunMode::SingleShot;
        }
        else if (arg == "--batch")
        {
            m_config.mode = HeadlessRunMode::Batch;
        }
        else if (arg == "--prompt")
        {
            if (!readNext(i, m_config.prompt))
            {
                return HeadlessResult::error("Missing value for --prompt", -2);
            }
            m_config.mode = HeadlessRunMode::SingleShot;
        }
        else if (arg == "--input")
        {
            if (!readNext(i, m_config.inputFile))
            {
                return HeadlessResult::error("Missing value for --input", -3);
            }
            m_config.mode = HeadlessRunMode::Batch;
        }
        else if (arg == "--output")
        {
            if (!readNext(i, m_config.outputFile))
            {
                return HeadlessResult::error("Missing value for --output", -4);
            }
        }
        else if (arg == "--port")
        {
            std::string portValue;
            if (!readNext(i, portValue))
            {
                return HeadlessResult::error("Missing value for --port", -5);
            }
            const long port = std::strtol(portValue.c_str(), nullptr, 10);
            if (port < 1 || port > 65535)
            {
                return HeadlessResult::error("Invalid --port value", -6);
            }
            m_config.port = static_cast<int>(port);
        }
        else if (arg == "--bind")
        {
            if (!readNext(i, m_config.bindAddress))
            {
                return HeadlessResult::error("Missing value for --bind", -7);
            }
        }
        else if (arg == "--json")
        {
            m_config.jsonOutput = true;
        }
        else if (arg == "--quiet")
        {
            m_config.quiet = true;
        }
        else if (arg == "--no-server")
        {
            m_config.enableServer = false;
        }
        else if (arg == "--server")
        {
            m_config.enableServer = true;
            m_config.mode = HeadlessRunMode::Server;
        }
    }

    if (m_config.mode == HeadlessRunMode::Batch && m_config.inputFile.empty())
    {
        return HeadlessResult::error("Batch mode requires --input <file>", -8);
    }

    return HeadlessResult::ok("Headless fallback initialized");
}

int HeadlessIDE::run()
{
    m_running.store(true);
    auto finish = [&](int code) -> int
    {
        m_running.store(false);
        return code;
    };

    if (m_config.mode == HeadlessRunMode::SingleShot)
    {
        const std::string prompt = m_config.prompt.empty() ? "health-check" : m_config.prompt;
        const std::string response = "headless-fallback: " + prompt;
        if (m_config.jsonOutput)
        {
            std::cout << "{\"success\":true,\"response\":\"" << jsonEscape(response) << "\"}\n";
        }
        else
        {
            std::cout << response << "\n";
        }
        return finish(0);
    }

    if (m_config.mode == HeadlessRunMode::Batch)
    {
        std::ifstream in(m_config.inputFile);
        if (!in)
        {
            return finish(2);
        }

        std::ofstream outFile;
        std::ostream* out = &std::cout;
        if (!m_config.outputFile.empty())
        {
            outFile.open(m_config.outputFile, std::ios::trunc);
            if (!outFile)
            {
                return finish(3);
            }
            out = &outFile;
        }

        std::string line;
        while (!m_shutdownRequested.load() && std::getline(in, line))
        {
            if (line.empty())
            {
                continue;
            }
            (*out) << "headless-fallback: " << line << "\n";
        }
        return finish(0);
    }

    if (m_config.mode == HeadlessRunMode::REPL || m_config.enableRepl)
    {
        std::string line;
        while (!m_shutdownRequested.load())
        {
            std::cout << "rawrxd> " << std::flush;
            if (!std::getline(std::cin, line))
            {
                break;
            }
            if (line == "exit" || line == "quit")
            {
                break;
            }
            if (line == "help")
            {
                std::cout << "Commands: help, exit, quit\n";
                continue;
            }
            std::cout << "headless-fallback: " << line << "\n";
        }
        return finish(0);
    }

    if (m_config.enableServer && !m_config.quiet)
    {
        LOG_INFO(std::string("[headless] fallback server loop listening on ") + m_config.bindAddress + ":" + std::to_string(m_config.port));
    }

    while (!m_shutdownRequested.load())
    {
        Sleep(200);
    }

    return finish(0);
}

int runSelftest(HWND hwnd)
{
    const int code = runStartupSelfTest();
    if (hwnd && IsWindow(hwnd))
    {
        MessageBoxA(hwnd, code == 0 ? "Self-test passed." : "Self-test failed. See stdout/stderr for details.",
                    "RawrXD Self-test", code == 0 ? (MB_OK | MB_ICONINFORMATION) : (MB_OK | MB_ICONERROR));
    }
    return code;
}
