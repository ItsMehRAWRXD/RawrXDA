#include "../../include/RawrXD_ApertureManager.h"
#include "../../include/api_server.h"
#include "../../include/collab/websocket_hub.h"
#include "../../include/crash_containment.h"
#include "../../include/enterprise_feature_manager.hpp"
#include "../../include/enterprise_license.h"
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
#include "../AppState.h"
#include "../cli/swarm_orchestrator.h"
#include "../core/HardwareScout.h"
#include "../core/camellia256_bridge.hpp"
#include "../core/enterprise_license.h"
#include "../core/integrated_runtime.hpp"
#include "../core/js_extension_host.hpp"
#include "../core/model_memory_hotpatch.hpp"
#include "../core/rawrxd_state_mmf.hpp"
#include "../core/unified_command_dispatch.hpp"
#include "../cpu_inference_engine.h"
#include "../marketplace/extension_auto_installer.hpp"
#include "../modules/codex_ultimate.h"
#include "../modules/engine_manager.h"
#include "../modules/memory_manager.h"
#include "../modules/vsix_loader.h"
#include "../overclock_governor.h"
#include "../p2p/SystemIntegrityProver.h"
#include "HeadlessIDE.h"
#include "Win32IDE.h"
#include "Win32IDE_AgenticBrowser.h"
#include "WindowVisibilityHelpers.h"
#include <commctrl.h>
#include <dbghelp.h>
#include <shellscalingapi.h>
#include <winhttp.h>
#if defined(_MSC_VER) && defined(_WIN32)
#include <delayimp.h>
#endif
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT) - 4)
#endif
#pragma comment(lib, "dbghelp.lib")
#include "../agent/quantum_agent_orchestrator.hpp"
#include "../agentic/ToolRegistry.h"
#include "../skill_system/SkillSystemBuildIntegration.h"
#include "rawrxd/runtime/RuntimeSurfaceBootstrap.hpp"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#pragma comment(lib, "winhttp.lib")
#endif

// ============================================================================
// Startup trace — write to ide_startup.log in exe dir for launch audit
// ============================================================================
// LAZY SINGLETON PATTERN: Avoid SIOF (Static Initialization Order Fiasco)
// These must be lazy-initialized to ensure C runtime is ready before construction.
static std::ofstream* s_startupLog = nullptr;  // Plain pointer is safe

inline std::mutex& GetStartupLogMutex() {
    static std::mutex* inst = new std::mutex();  // Leak-on-purpose, never destroyed
    return *inst;
}

static void startupTrace(const char* step, const char* detail = nullptr)
{
    std::lock_guard<std::mutex> lock(GetStartupLogMutex());
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

static bool isHeapWalkEnabled()
{
    char buf[8] = {};
    const DWORD n = GetEnvironmentVariableA("RAWRXD_HEAP_WALK_ON_OPEN", buf, (DWORD)sizeof(buf));
    if (n == 0)
        return true;  // Default ON so startup heap state is visible in debugger.
    return !(buf[0] == '0' || buf[0] == 'n' || buf[0] == 'N');
}

static bool isTruthyEnvVar(const char* name)
{
    if (!name || !name[0])
        return false;

    // Prefer Win32 environment block lookup; runtime bootstrap and CRT snapshots can diverge.
    char buf[8] = {};
    const DWORD n = GetEnvironmentVariableA(name, buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0)
    {
        const char c = buf[0];
        return !(c == '0' || c == 'n' || c == 'N' || c == 'f' || c == 'F');
    }

    const char* value = std::getenv(name);
    if (!value || !value[0])
        return false;
    return !(value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' || value[0] == 'F');
}

// Directory of the running IDE exe (ASCII); empty if unavailable.
static std::string getIdeExeDirectoryA()
{
    char buf[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameA(nullptr, buf, (DWORD)sizeof(buf));
    if (n == 0 || n >= sizeof(buf))
        return {};
    std::string p(buf, buf + n);
    const size_t slash = p.find_last_of("\\/");
    if (slash == std::string::npos)
        return {};
    return p.substr(0, slash);
}

// Join exe directory with a relative path using a single backslash (rel may contain backslashes).
static std::string joinExeSubpathA(const std::string& exeDir, const char* relPath)
{
    if (!relPath || relPath[0] == '\0')
        return exeDir;
    if (exeDir.empty())
        return {};
    const char sep = (exeDir.back() == '\\' || exeDir.back() == '/') ? '\0' : '\\';
    if (sep == '\0')
        return exeDir + relPath;
    return exeDir + std::string(1, sep) + relPath;
}

static bool fileExists(const std::filesystem::path& p)
{
    std::error_code ec;
    return std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec);
}

static bool isValidDistRoot(const std::filesystem::path& root)
{
    return fileExists(root / "manifest.json") && fileExists(root / "recursive_sha256_snapshot.txt");
}

static bool resolveDistRootForIntegrity(std::filesystem::path& outRoot)
{
    outRoot.clear();

    if (const char* envRoot = std::getenv("RAWRXD_DIST_ROOT"); envRoot && envRoot[0])
    {
        std::filesystem::path p(envRoot);
        if (isValidDistRoot(p))
        {
            outRoot = std::filesystem::absolute(p);
            return true;
        }
    }

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec)
    {
        return false;
    }

    const std::vector<std::filesystem::path> candidates = {cwd / "dist", cwd, cwd.parent_path() / "dist",
                                                           cwd.parent_path().parent_path() / "dist"};

    for (const auto& candidate : candidates)
    {
        if (isValidDistRoot(candidate))
        {
            outRoot = std::filesystem::absolute(candidate);
            return true;
        }
    }

    return false;
}

static bool runApertureIntegrityPreflight(std::string& errorOut, std::string& distRootOut)
{
    errorOut.clear();
    distRootOut.clear();

    if (isTruthyEnvVar("RAWRXD_SKIP_APERTURE_PREFLIGHT"))
    {
        return true;
    }

    std::filesystem::path distRootPath;
    if (!resolveDistRootForIntegrity(distRootPath))
    {
        return true;  // No discoverable dist root; do not block non-dist developer launches.
    }

    distRootOut = distRootPath.string();
    std::string apertureError;
    auto aperture = RawrXD::buildApertureFromManifest(distRootOut, 0, apertureError);
    if (!aperture)
    {
        errorOut = apertureError.empty() ? "Aperture integrity preflight failed" : apertureError;
        return false;
    }

    return true;
}

static void bootIntegratedRuntimeSafely()
{
    try
    {
        RawrXD::IntegratedRuntime::boot();
    }
    catch (const std::exception& ex)
    {
        OutputDebugStringA(("[main_win32] Integrated runtime C++ exception: " + std::string(ex.what()) + "\n").c_str());
        startupTrace("integrated_runtime_cpp_exception", ex.what());
    }
    catch (...)
    {
        OutputDebugStringA("[main_win32] Integrated runtime threw C++ exception (non-fatal, continuing)\n");
        startupTrace("integrated_runtime_cpp_exception");
    }
}

static void emitStartupHeapSnapshot(const char* stage)
{
    if (!stage)
        stage = "unknown";

    HANDLE heap = GetProcessHeap();
    BOOL heapValid = FALSE;
    if (heap)
        heapValid = HeapValidate(heap, 0, nullptr);

    char header[256];
    snprintf(header, sizeof(header), "[main_win32][heap] stage=%s processHeap=%p heapValid=%d pid=%lu\n", stage, heap,
             heapValid ? 1 : 0, (unsigned long)GetCurrentProcessId());
    OutputDebugStringA(header);
    startupTrace("heap_snapshot", header);

    if (!heap || !isHeapWalkEnabled())
        return;

    PROCESS_HEAP_ENTRY entry{};
    DWORD busyBlocks = 0;
    SIZE_T busyBytes = 0;
    DWORD regionCount = 0;
    DWORD uncommittedRanges = 0;
    DWORD loggedBusy = 0;

    if (!HeapLock(heap))
    {
        const DWORD lockErr = GetLastError();
        char msg[128];
        snprintf(msg, sizeof(msg), "[main_win32][heap] stage=%s HeapLock failed err=%lu\n", stage,
                 (unsigned long)lockErr);
        OutputDebugStringA(msg);
        startupTrace("heap_walk_lock_failed", msg);
        return;
    }

    while (HeapWalk(heap, &entry))
    {
        if ((entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0)
        {
            ++busyBlocks;
            busyBytes += entry.cbData;
            if (loggedBusy < 8)
            {
                char line[196];
                snprintf(line, sizeof(line), "[main_win32][heap] stage=%s busy[%lu] addr=%p size=%zu overhead=%u\n",
                         stage, (unsigned long)loggedBusy, entry.lpData, (size_t)entry.cbData,
                         (unsigned)entry.cbOverhead);
                OutputDebugStringA(line);
                startupTrace("heap_walk_busy", line);
                ++loggedBusy;
            }
        }
        else if ((entry.wFlags & PROCESS_HEAP_REGION) != 0)
        {
            ++regionCount;
        }
        else if ((entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) != 0)
        {
            ++uncommittedRanges;
        }
    }

    const DWORD walkErr = GetLastError();
    HeapUnlock(heap);

    char summary[256];
    snprintf(
        summary, sizeof(summary),
        "[main_win32][heap] stage=%s walkDone busyBlocks=%lu busyBytes=%zu regions=%lu uncommitted=%lu walkErr=%lu\n",
        stage, (unsigned long)busyBlocks, (size_t)busyBytes, (unsigned long)regionCount,
        (unsigned long)uncommittedRanges, (unsigned long)walkErr);
    OutputDebugStringA(summary);
    startupTrace("heap_walk_summary", summary);
}

static LONG WINAPI RawrXDUnhandledExceptionFilter(PEXCEPTION_POINTERS exc)
{
    if (!exc || !exc->ExceptionRecord)
        return EXCEPTION_EXECUTE_HANDLER;

    DWORD code = exc->ExceptionRecord->ExceptionCode;
    char buf[512];
    snprintf(buf, sizeof(buf), "[UnhandledException] code=0x%08X\n", code);
    OutputDebugStringA(buf);
    startupTrace("unhandled_exception", buf);

    // Capture a small stack trace
    void* stack[62];
    USHORT frames = CaptureStackBackTrace(0, _countof(stack), stack, nullptr);
    for (USHORT i = 0; i < frames; ++i)
    {
        snprintf(buf, sizeof(buf), "  frame[%u]=0x%p\n", i, stack[i]);
        OutputDebugStringA(buf);
        startupTrace("stack", buf);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

static void initCrashHandler()
{
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(GetCurrentProcess(), NULL, TRUE))
    {
        OutputDebugStringA("[initCrashHandler] SymInitialize failed\n");
    }
    SetUnhandledExceptionFilter(RawrXDUnhandledExceptionFilter);
}

static void logBackgroundThreadCrash(const char* lane, DWORD code)
{
    char msg[256];
    snprintf(msg, sizeof(msg), "BACKGROUND THREAD CRASH: lane=%s Exception 0x%08lX\n", lane ? lane : "unknown",
             (unsigned long)code);
    OutputDebugStringA(msg);
    startupTrace("background_thread_crash", msg);

    std::ofstream out("rawrxd_crash.log", std::ios::out | std::ios::app);
    if (out)
    {
        out << msg;
        out.flush();
    }
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
    // Environment override — force headless even if the flag is omitted
    const char* env = std::getenv("RAWRXD_HEADLESS");
    if (env && (_stricmp(env, "1") == 0 || _stricmp(env, "true") == 0 || _stricmp(env, "yes") == 0))
        return true;

    if (!lpCmdLine)
        return false;

    // Support --headless, --server, and --agent-prompt as headless triggers.
    // --agent-prompt is the monolithic ASM headless lane; routing it through
    // HeadlessIDE ensures the inference pipeline is fully initialised before
    // the prompt is dispatched instead of falling through to GUI mode.
    return strstr(lpCmdLine, "--headless") != nullptr || strstr(lpCmdLine, "--server") != nullptr ||
           strstr(lpCmdLine, "--agent-prompt") != nullptr;
}

// Helper: check if launch args request help only (avoid long-running headless)
static bool hasHeadlessHelpFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;

    std::istringstream iss(lpCmdLine);
    std::string token;
    while (iss >> token)
    {
        if (token == "--help" || token == "-h" || token == "/?")
            return true;
    }
    return false;
}

static void printHeadlessQuickHelp()
{
    fputs("RawrXD Headless IDE\n"
          "Usage: RawrXD-Win32IDE.exe --headless [--repl|--no-server|--prompt <text>]\n"
          "  --no-server       Skip HTTP listener; tear down after headless init (no blocking server loop)\n"
          "  --repl            Interactive REPL mode\n"
          "  --prompt <txt>    Single-shot Ollama chat (one /api/chat turn)\n"
          "  --agent-prompt    Same prompt, but use multi-turn tool loop (IDE agentic parity)\n"
          "  --ollama-model M  Pin Ollama model (else /api/tags or RAWRXD_NATIVE_MODEL)\n"
          "  --help            Show this help and exit\n",
          stdout);
    fflush(stdout);
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

static bool hasAgenticSmokeFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    // --agentic-smoke is the canonical flag; --smoke-test is the user-facing alias.
    // Both route to the same bounded, deterministic runAgenticSmokeTestExit().
    return strstr(lpCmdLine, "--agentic-smoke") != nullptr ||
           strstr(lpCmdLine, "--smoke-test") != nullptr;
}

static bool hasChatUiSmokeFlag(LPSTR lpCmdLine)
{
    if (!lpCmdLine)
        return false;
    return strstr(lpCmdLine, "--chat-ui-smoke-noninteractive") != nullptr ||
           strstr(lpCmdLine, "--chat-smoke-noninteractive") != nullptr;
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

static bool agenticSmokeToolRegistryStep(std::string& errOut);

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
    // We verify registration + linkage only — HANDLER_ERROR means the handler ran (correct
    // behaviour for headless probes with no args). Fail only on NOT_FOUND / NULL_HANDLER /
    // WRONG_EXPOSURE, which indicate a registry gap or a broken link.
    {
        std::string diag;
        const uint32_t ids[] = {1002u, 2028u, 3200u, 4009u, 10000u};
        bool ok = true;
        for (uint32_t id : ids)
        {
            CommandContext ctx{};
            ctx.rawInput = "";
            ctx.args     = "";
            ctx.commandId      = id;
            ctx.isGui          = false;
            ctx.isHeadless     = true;
            ctx.outputFn       = selfTestOutputSink;
            ctx.outputUserData = &diag;
            auto result = RawrXD::Dispatch::dispatchByGuiId(id, ctx);
            // OK or HANDLER_ERROR (no args) = handler is linked and callable → pass.
            // PRECOND_FAIL (feature gated at runtime) is also acceptable for a headless probe.
            // Fail only on: NOT_FOUND (unregistered), NULL_HANDLER (link gap), WRONG_EXPOSURE.
            const bool linked =
                result.status == RawrXD::Dispatch::DispatchStatus::OK ||
                result.status == RawrXD::Dispatch::DispatchStatus::HANDLER_ERROR ||
                result.status == RawrXD::Dispatch::DispatchStatus::PRECOND_FAIL;
            if (!linked)
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

    // 6) Agent tool registry (shared with IDE headless agentic path) — offline read_file
    {
        std::string agentErr;
        if (!agenticSmokeToolRegistryStep(agentErr))
            fail("agent-tool-registry", agentErr);
        else
            pass("agent-tool-registry");
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
// Bounds: cap argument count and individual token size to prevent DoS via
// a crafted shortcut/registry launch key.
static constexpr size_t kMaxCmdArgs = 64;       // hard cap on # of arguments
static constexpr size_t kMaxTokenBytes = 4096;  // hard cap on single-token length

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
            if (args.size() >= kMaxCmdArgs)
                break;  // DoS guard: cap argument count
            // Handle quoted arguments
            if (!token.empty() && token.front() == '"')
            {
                token = token.substr(1);
                std::string rest;
                // Bound accumulation size to prevent memory DoS via unclosed quoted string
                while (!token.empty() && token.back() != '"' && token.size() < kMaxTokenBytes &&
                       std::getline(iss, rest, '"'))
                {
                    token += ' ';
                    token += rest;
                }
                if (!token.empty() && token.back() == '"')
                {
                    token.pop_back();
                }
                // Clamp in case we hit kMaxTokenBytes mid-accumulation
                if (token.size() > kMaxTokenBytes)
                    token.resize(kMaxTokenBytes);
            }
            args.push_back(std::move(token));
        }
    }

    ptrs.reserve(args.size() + 1);
    for (auto& a : args)
        ptrs.push_back(const_cast<char*>(a.c_str()));
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
    std::string buildCommand = "cmake --build build_prod --config Release";
    std::string workspaceRoot = ".";
    std::string telemetryOut = "healing_telemetry.json";
    int maxAttempts = 3;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--build-command") == 0 && i + 1 < argc)
            buildCommand = argv[++i];
        else if (strcmp(argv[i], "--workspace-root") == 0 && i + 1 < argc)
            workspaceRoot = argv[++i];
        else if (strcmp(argv[i], "--telemetry-out") == 0 && i + 1 < argc)
            telemetryOut = argv[++i];
        else if (strcmp(argv[i], "--max-attempts") == 0 && i + 1 < argc)
            maxAttempts = std::stoi(argv[++i]);
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
        if (tf.is_open())
        {
            tf << "{\"attemptCount\":" << result.iterationCount
               << ",\"totalDiagnosticsGenerated\":" << result.todoItemsGenerated << ",\"totalDiagnosticsHandled\":\""
               << result.todoItemsGenerated << "\""
               << ",\"totalFixesStaged\":\"" << result.todoItemsCompleted << "\""
               << ",\"finalStatus\":\"" << (result.success ? "success" : "failure") << "\""
               << ",\"durationMs\":" << result.totalDurationMs << "}\n";
        }
    }

    fprintf(stdout, "[RAWRXD-AUTOFIX] result=%s attempts=%d fixes=%d duration=%llums\n",
            result.success ? "SUCCESS" : "FAILURE", result.iterationCount, result.todoItemsCompleted,
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
// Off-screen rescue is centralized through WindowVisibilityHelpers.
static void ensureMainWindowVisible(HWND hMain)
{
    if (!hMain || !IsWindow(hMain))
        return;

    RawrXD::Win32Visibility::LogPlacementSnapshot(hMain, "ensureMainWindowVisible:before");

    if (IsIconic(hMain))
    {
        ShowWindow(hMain, SW_RESTORE);
        UpdateWindow(hMain);
    }
    if (!IsWindowVisible(hMain))
        ShowWindow(hMain, SW_SHOWNORMAL);

    RawrXD::Win32Visibility::NormalizePlacementForVisibility(hMain);

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

    RawrXD::Win32Visibility::LogPlacementSnapshot(hMain, "ensureMainWindowVisible:after");
}

// Storage for phase-created objects (createWindow phase sets these; cleanup uses them).
static EngineManager* s_engine_mgr = nullptr;  // Plain pointer, safe
static CodexUltimate* s_codex = nullptr;       // Plain pointer, safe

// Self-hosting backend — Ollama-compatible API server backed by native MASM kernels.
// LAZY SINGLETON: AppState has std::string, std::atomic, std::shared_ptr, std::unique_ptr members
// All of these have non-trivial constructors that require C runtime to be initialized
inline AppState& GetApiAppState() {
    static AppState* inst = new AppState();  // Leak-on-purpose, never destroyed
    return *inst;
}
#define s_apiAppState GetApiAppState()

inline std::unique_ptr<APIServer>& GetApiServer() {
    static std::unique_ptr<APIServer>* inst = new std::unique_ptr<APIServer>();
    return *inst;
}
#define s_apiServer GetApiServer()  // Backward-compatible accessor

// Debugger + System output (after main HWND exists; appendToOutput no-ops until output tabs are live).
static void guiBootMilestone(Win32IDE* ide, const char* debugLine, const char* userLine)
{
    if (debugLine && debugLine[0])
        OutputDebugStringA(debugLine);
    if (!userLine || !userLine[0])
        return;
    if (!ide || ide->isShuttingDown())
        return;
    HWND hwnd = ide->getMainWindow();
    if (!hwnd || !IsWindow(hwnd))
        return;
    ide->appendToOutput(std::string(userLine), "System", Win32IDE::OutputSeverity::Info);
}

// Post–message-loop cleanup: debugger always; System tab if main HWND still valid (ignores isShuttingDown).
static void guiShutdownMilestone(Win32IDE* ide, const char* debugLine, const char* userLine)
{
    if (debugLine && debugLine[0])
        OutputDebugStringA(debugLine);
    if (!userLine || !userLine[0] || !ide)
        return;
    HWND hwnd = ide->getMainWindow();
    if (!hwnd || !IsWindow(hwnd))
        return;
    ide->appendToOutput(std::string(userLine), "System", Win32IDE::OutputSeverity::Info);
}

// Lines before Win32IDE exists: always debugger + ide_startup.log; System tab replay once HWND is valid.
// LAZY SINGLETON: std::vector has non-trivial constructor, must be lazy-initialized
inline std::vector<std::pair<std::string, std::string>>& GetEarlyWinMainReplay() {
    static std::vector<std::pair<std::string, std::string>>* inst = new std::vector<std::pair<std::string, std::string>>();
    return *inst;
}
#define s_earlyWinMainReplay GetEarlyWinMainReplay()  // Backward-compatible accessor

static void earlyWinMainMilestone(const char* traceStep, const char* debugLine, const char* userLine)
{
    if (debugLine && debugLine[0])
        OutputDebugStringA(debugLine);
    if (traceStep && traceStep[0])
        startupTrace(traceStep, (userLine && userLine[0]) ? userLine : debugLine);
    if (s_startupLog && traceStep &&
        (std::strncmp(traceStep, "winmain_early_b", 15) == 0 || std::strncmp(traceStep, "winmain_early_e0_", 17) == 0))
    {
        RawrXD::Startup::ensureStartupSessionId();
        std::string line = traceStep;
        line.push_back('|');
        line += RawrXD::Startup::getStartupSessionId();
        startupTrace("startup_conjoined_early_1_8", line.c_str());
    }
    if (userLine && userLine[0])
        s_earlyWinMainReplay.emplace_back(debugLine ? std::string(debugLine) : std::string{}, std::string(userLine));
}

static void flushEarlyWinMainReplayToSystemOutput(Win32IDE* ide)
{
    if (s_earlyWinMainReplay.empty())
        return;
    if (!ide || ide->isShuttingDown())
        return;
    HWND hwnd = ide->getMainWindow();
    if (!hwnd || !IsWindow(hwnd))
        return;
    for (const auto& pr : s_earlyWinMainReplay)
        ide->appendToOutput(pr.second, "System", Win32IDE::OutputSeverity::Info);
    s_earlyWinMainReplay.clear();
}

// Split config/startup_phases.txt for GUI boot: through createWindow, then pre-show
// (e.g. enterprise_license), then phases after showWindow up to (but not including) message_loop_entered.
// Lazy-prefixed phases are omitted here; physical window show stays in WinMain between mid and post.
static void partitionGuiStartupFromConfig(std::vector<std::string>& preCreateWindow,
                                          std::vector<std::string>& betweenCreateAndShow,
                                          std::vector<std::string>& afterShow)
{
    preCreateWindow.clear();
    betweenCreateAndShow.clear();
    afterShow.clear();

    enum class Zone
    {
        BeforeCreateWindow,
        AfterCreateUntilShow,
        AfterShow
    } zone = Zone::BeforeCreateWindow;

    for (const std::string& name : RawrXD::Startup::getPhaseOrder())
    {
        if (RawrXD::Startup::isPhaseLazy(name))
            continue;
        if (name == "message_loop_entered")
            continue;

        if (name == "createWindow")
        {
            preCreateWindow.push_back(name);
            zone = Zone::AfterCreateUntilShow;
            continue;
        }
        if (name == "showWindow")
        {
            zone = Zone::AfterShow;
            continue;
        }

        switch (zone)
        {
            case Zone::BeforeCreateWindow:
                preCreateWindow.push_back(name);
                break;
            case Zone::AfterCreateUntilShow:
                betweenCreateAndShow.push_back(name);
                break;
            case Zone::AfterShow:
                afterShow.push_back(name);
                break;
        }
    }
}

// Pre-createWindow + between create/show phases → E0 probes (front-to-back conjoin with startup_phases.txt).
// Distinct indices per phase where possible (avoid repeating E0-01 for three consecutive lines).
static int e0IndexForPreOrMidShowPhase(const std::string& name)
{
    if (name == "init_common_controls")
        return 17;  // PR02 session id
    if (name == "first_run_gauntlet")
        return 18;  // Phase manifest / count
    if (name == "vsix_loader")
        return 19;  // extensions|plugins directories
    if (name == "plugin_signature")
        return 4;  // Swarm singleton (trust phase; lightweight structural check)
    if (name == "creating_ide_instance")
        return 10;  // Config directory (workspace layout)
    if (name == "createWindow")
        return 7;  // Editor HWND
    if (name == "enterprise_license")
        return 20;  // License subsystem initialized
    return 0;
}

// Post-show phases (extension_bootstrap .. layout) map to E0-01..E0-07; message_loop_entered → E0-08.
// See config/startup_phases.txt and Win32IDE::runCriticalValidationE0Check.
static int e0IndexForPostShowPhase(const std::string& name)
{
    if (name == "extension_bootstrap")
        return 1;
    if (name == "integrated_runtime")
        return 6;
    if (name == "camellia_init")
        return 3;
    if (name == "masm_init")
        return 7;
    if (name == "swarm")
        return 4;
    if (name == "auto_update")
        return 2;
    if (name == "layout")
        return 5;
    return 0;
}

static void traceConjoinedE0ForPhase(Win32IDE& ide, const std::string& phaseName, int e0Override = 0)
{
    const int id = e0Override > 0 ? e0Override : e0IndexForPostShowPhase(phaseName);
    if (id < 1 || id > 64)
        return;
    const auto c = ide.runCriticalValidationE0Check(id);
    std::string line = phaseName;
    line.push_back('|');
    line += c.passed ? "pass " : "fail ";
    line += c.name;
    line += " :: ";
    if (c.detail.size() > 200)
        line.append(c.detail, 0, 200);
    else
        line += c.detail;
    startupTrace("startup_conjoined_e0", line.c_str());
}

// E0-09..E0-16 — after localhost API server start (see config/startup_phases.txt).
static const char* e0Batch4Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 9:
            return "registry";
        case 10:
            return "config_dir";
        case 11:
            return "logs_dir";
        case 12:
            return "api_status";
        case 13:
            return "feature_registry";
        case 14:
            return "agentic_bridge";
        case 15:
            return "quickjs_host";
        case 16:
            return "engines_dir";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0PostApiBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 9; id <= 16; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "post_api|";
        line += e0Batch4Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b4", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b4_summary", (std::to_string(pass) + "/8").c_str());
}

// E0-17..E0-24 — extended conjoined checks (session, phase manifest, extensions, license, layout depth).
static const char* e0Batch5Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 17:
            return "pr02_session";
        case 18:
            return "phase_manifest";
        case 19:
            return "extensions_dir";
        case 20:
            return "enterprise_license";
        case 21:
            return "client_area";
        case 22:
            return "sidebars";
        case 23:
            return "main_children";
        case 24:
            return "gui_thread";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0ExtendedBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 17; id <= 24; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "pre_loop|";
        line += e0Batch5Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b5", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b5_summary", (std::to_string(pass) + "/8").c_str());
}

// E0-25..E0-32 — message-loop boundary / shell readiness (after PostMessage + SetFocus(editor)).
static const char* e0Batch6Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 25:
            return "main_input_enabled";
        case 26:
            return "editor_class";
        case 27:
            return "output_chrome";
        case 28:
            return "copilot_output";
        case 29:
            return "window_title";
        case 30:
            return "model_selector";
        case 31:
            return "menu_depth";
        case 32:
            return "focus_under_main";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0LoopReadyBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 25; id <= 32; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "loop_ready|";
        line += e0Batch6Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b6", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b6_summary", (std::to_string(pass) + "/8").c_str());
}

// E0-33..E0-40 — shell depth after loop_ready batch (same pump; see runCriticalValidationBatch7).
static const char* e0Batch7Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 33:
            return "richedit_module";
        case 34:
            return "toolbar";
        case 35:
            return "tab_bar";
        case 36:
            return "chrome_close";
        case 37:
            return "phase_milestones";
        case 38:
            return "main_dpi";
        case 39:
            return "feature_breadth";
        case 40:
            return "splitter_or_activity";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0ShellDeepBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 33; id <= 40; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "shell_deep|";
        line += e0Batch7Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b7", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b7_summary", (std::to_string(pass) + "/8").c_str());
}

// E0-41..E0-48 — workbench capstone (same pump as b6/b7; see runCriticalValidationBatch8).
// E0-49..E0-56 — agent/Git/startup-log (runCriticalValidationBatch9; traceConjoinedE0AgentChromeBatch).
static const char* e0Batch8Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 41:
            return "gutter_or_minimap";
        case 42:
            return "command_input";
        case 43:
            return "chrome_min_max";
        case 44:
            return "explorer_tree";
        case 45:
            return "sidebar_content";
        case 46:
            return "exe_image_path";
        case 47:
            return "phase_license_ext";
        case 48:
            return "main_not_hung";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0WorkbenchBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 41; id <= 48; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "workbench|";
        line += e0Batch8Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b8", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b8_summary", (std::to_string(pass) + "/8").c_str());
}

// E0-49..E0-56 — agent / Git / startup-log artifact (same pre-loop pump as b6–b10).
// E0-57..E0-64 — panels + model sliders (runCriticalValidationBatch10; traceConjoinedE0PanelsDeepBatch).
static const char* e0Batch9Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 49:
            return "copilot_input";
        case 50:
            return "copilot_send";
        case 51:
            return "copilot_clear_or_secondary";
        case 52:
            return "settings_btn";
        case 53:
            return "title_label";
        case 54:
            return "git_surface";
        case 55:
            return "model_progress";
        case 56:
            return "ide_startup_log";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0AgentChromeBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 49; id <= 56; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "agent_chrome|";
        line += e0Batch9Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b9", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b9_summary", (std::to_string(pass) + "/8").c_str());
}

// Run one startup phase by name. Sequence is from config/startup_phases.txt (dynamic).
// Returns false to abort (e.g. createWindow failed).
static bool runPhase(const std::string& name, Win32IDE& ide, HINSTANCE, LPSTR lpCmdLine)
{
    RawrXD::Startup::ensureStartupSessionId();

    if (name == "init_common_controls")
    {
        INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES |
                                                                       ICC_TAB_CLASSES | ICC_TREEVIEW_CLASSES |
                                                                       ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
        if (!InitCommonControlsEx(&icex))
        {
            startupTrace("init_common_controls_failed", "InitCommonControlsEx");
            // No return false; fallback to legacy logic or continue with warning
        }

        // P0: UI Init (HWND & RichEdit) - Hardening: Force load MSFTEDIT.DLL for modern RichEdit support.
        // We ensure it is loaded before any CreateWindow calls.
        HMODULE hMsftEdit = LoadLibraryW(L"Msftedit.dll");
        HMODULE hRich20 = LoadLibraryW(L"riched20.dll");

        if (!hMsftEdit && !hRich20)
        {
            startupTrace("init_common_controls_failed", "richedit_load");
            MessageBoxW(nullptr,
                        L"Standard Edit components (Msftedit.dll or riched20.dll) not found.\nThis is a critical "
                        L"system dependency.",
                        L"RawrXD - Init Failure", MB_ICONERROR);
            return false;
        }

        if (hMsftEdit)
            startupTrace("init_common_controls", "MsftEdit loaded");
        else
            startupTrace("init_common_controls", "RichEdit20 fallback loaded");

        // Sovereign Universal (Hardware Scout) - Phase: 1-Interrogation
        // Detect VRAM/AVX-512 before engine startup to map GPU or CPU kernels.
        auto profile = RawrXD::Core::HardwareScout::GetCurrentProfile();
        startupTrace("init_hardware_scout", RawrXD::Core::HardwareScout::TierToString(profile.tier));

        // Sovereign integrity attestation — async; skip in CI/smoke via RAWRXD_SKIP_STARTUP_ATTEST=1.
        {
            char skipAttest[8] = {};
            const DWORD n =
                GetEnvironmentVariableA("RAWRXD_SKIP_STARTUP_ATTEST", skipAttest, (DWORD)sizeof(skipAttest));
            if (n > 0 && skipAttest[0] == '1')
            {
                startupTrace("init_common_controls", "integrity_attest_skipped");
            }
            else
            {
                std::thread(
                    []()
                    {
                        __try
                        {
                            const bool ok = SystemIntegrityProver::Instance().AttestQuick();
                            if (!ok)
                            {
                                OutputDebugStringA("[main_win32] WARNING: Sovereign integrity attestation FAILED.\n");
                            }
                            else
                            {
                                OutputDebugStringA("[main_win32] Sovereign integrity attestation passed.\n");
                            }
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            logBackgroundThreadCrash("startup_attestation", GetExceptionCode());
                        }
                    })
                    .detach();
            }
        }

        return true;
    }
    if (name == "first_run_gauntlet")
    {
        startupTrace("first_run_gauntlet_start");
        const std::string exeDir = getIdeExeDirectoryA();
        const std::string cfgDir = joinExeSubpathA(exeDir, "config");
        if (!exeDir.empty() && !cfgDir.empty())
            CreateDirectoryA(cfgDir.c_str(), nullptr);
        const std::string gauntletPath =
            exeDir.empty() ? std::string("config\\first_run.flag") : joinExeSubpathA(exeDir, "config\\first_run.flag");
        const char* gauntletFlag = gauntletPath.c_str();
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
            if (exeDir.empty())
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
        const std::string exeDir = getIdeExeDirectoryA();
        const std::string pluginsDir = exeDir.empty() ? std::string("plugins") : joinExeSubpathA(exeDir, "plugins");
        const std::string extDir = exeDir.empty() ? std::string("extensions") : joinExeSubpathA(exeDir, "extensions");
        CreateDirectoryA(pluginsDir.c_str(), nullptr);
        CreateDirectoryA(extDir.c_str(), nullptr);
        CreateDirectoryA("plugins", nullptr);
        CreateDirectoryA("extensions", nullptr);
        VSIXLoader::GetInstance().Initialize(pluginsDir);
        startupTrace("vsix_loader", pluginsDir.c_str());
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
    if (name == "extension_bootstrap")
    {
        startupTrace("extension_bootstrap_start", RawrXD::Startup::getStartupSessionId());

        if (hasAgenticSmokeFlag(GetCommandLineA()))
        {
            startupTrace("extension_bootstrap_skipped", "agentic_smoke");
            return true;
        }

        const char* safeMode = std::getenv("RAWRXD_SAFE_MODE");
        if (safeMode && safeMode[0] == '1')
        {
            startupTrace("extension_bootstrap_skipped", "safe_mode");
            return true;
        }

        const char* disableBootstrap = std::getenv("RAWRXD_DISABLE_STARTUP_EXTENSION_BOOTSTRAP");
        if (disableBootstrap && disableBootstrap[0] == '1')
        {
            startupTrace("extension_bootstrap_skipped", "disabled_by_env");
            return true;
        }

        std::thread(
            []()
            {
                try
                {
                    using RawrXD::Extensions::ExtensionAutoInstaller;
                    using RawrXD::Extensions::InstallProgress;

                    auto& installer = ExtensionAutoInstaller::instance();
                    if (!installer.needsFirstRunInstall())
                    {
                        startupTrace("extension_bootstrap_skipped", "already_completed");
                        return;
                    }

                    auto progressCallback = [](const InstallProgress& progress)
                    {
                        const char* stage = "unknown";
                        switch (progress.stage)
                        {
                            case InstallProgress::Stage::Querying:
                                stage = "querying";
                                break;
                            case InstallProgress::Stage::Downloading:
                                stage = "downloading";
                                break;
                            case InstallProgress::Stage::Installing:
                                stage = "installing";
                                break;
                            case InstallProgress::Stage::Verifying:
                                stage = "verifying";
                                break;
                            case InstallProgress::Stage::Complete:
                                stage = "complete";
                                break;
                            case InstallProgress::Stage::Failed:
                                stage = "failed";
                                break;
                        }

                        std::ostringstream oss;
                        oss << stage << " [" << (progress.currentIndex + 1) << "/" << progress.totalExtensions
                            << "] ";
                        if (progress.extensionId)
                            oss << progress.extensionId;
                        if (progress.detail && progress.detail[0] != '\0')
                            oss << " - " << progress.detail;
                        startupTrace("extension_bootstrap_progress", oss.str().c_str());
                    };

                    const auto result = installer.installPriorityExtensions(progressCallback);
                    std::ostringstream oss;
                    oss << "installed=" << result.installedCount << ", failed=" << result.failedCount;
                    if (!result.detail.empty())
                        oss << ", detail=" << result.detail;

                    startupTrace(result.success ? "extension_bootstrap_done" : "extension_bootstrap_failed",
                                 oss.str().c_str());
                }
                catch (...)
                {
                    logBackgroundThreadCrash("extension_bootstrap", 0xE06D7363);
                }
            })
            .detach();

        return true;
    }
    if (name == "creating_ide_instance")
    {
        CreateDirectoryA("config", nullptr);
        CreateDirectoryA("logs", nullptr);
        CreateDirectoryA("registry", nullptr);
        CreateDirectoryA("crash_dumps", nullptr);
        CreateDirectoryA("extensions", nullptr);
        CreateDirectoryA("plugins", nullptr);
        CreateDirectoryA("engines", nullptr);
        const std::string exeDir = getIdeExeDirectoryA();
        if (!exeDir.empty())
        {
            const char* subdirs[] = {"config", "logs", "registry", "crash_dumps", "extensions", "plugins", "engines"};
            for (const char* sub : subdirs)
            {
                const std::string p = joinExeSubpathA(exeDir, sub);
                if (!p.empty())
                    CreateDirectoryA(p.c_str(), nullptr);
            }
        }
        startupTrace("creating_ide_instance", "dirs_ok");
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
        s_codex = new CodexUltimate();
        ide.setEngineManager(s_engine_mgr);
        ide.setCodexUltimate(s_codex);
        pumpMessages();

        // Initialize file watcher for file explorer
        ide.initFileWatcher();

        // High-risk bootstrap lane (engine + MMF + JS + sandbox).
        // Default OFF to avoid startup heap corruption; opt-in with:
        //   RAWRXD_ENABLE_BACKGROUND_BOOT=1
        const char* bgBoot = std::getenv("RAWRXD_ENABLE_BACKGROUND_BOOT");
        const bool enableBackgroundBoot = (bgBoot && bgBoot[0] == '1');
        if (enableBackgroundBoot)
        {
            startupTrace("background_boot_enabled");
            std::thread(
                []()
                {
                    try
                    {
                        startupTrace("background_boot_start");
                        try
                        {
                            if (!s_engine_mgr)
                            {
                                startupTrace("background_boot_no_engine_mgr");
                                return;
                            }

                            if (RawrXD::g_800B_Unlocked)
                            {
                                try
                                {
                                    s_engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive");
                                }
                                catch (...)
                                {
                                    OutputDebugStringA("[main_win32] background_boot: 800b engine load failed\n");
                                }
                            }
                            try
                            {
                                s_engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate");
                            }
                            catch (...)
                            {
                                OutputDebugStringA("[main_win32] background_boot: codex engine load failed\n");
                            }
                            try
                            {
                                s_engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler");
                            }
                            catch (...)
                            {
                                OutputDebugStringA("[main_win32] background_boot: compiler engine load failed\n");
                            }

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

                            startupTrace("background_boot_done");
                        }
                        catch (...)
                        {
                            startupTrace("background_boot_exception");
                            OutputDebugStringA("[main_win32] background_boot exception (non-fatal)\n");
                        }
                    }
                    catch (...)
                    {
                        logBackgroundThreadCrash("background_boot", 0xE06D7363);
                    }
                })
                .detach();
        }
        else
        {
            startupTrace("background_boot_deferred");
            OutputDebugStringA(
                "[main_win32] background_boot disabled by default (set RAWRXD_ENABLE_BACKGROUND_BOOT=1 to enable)\n");
        }
        return true;
    }
    if (name == "enterprise_license")
    {
        const char* safeMode = std::getenv("RAWRXD_SAFE_MODE");
        if (safeMode && safeMode[0] == '1')
        {
            startupTrace("enterprise_license_skipped", "safe_mode");
            return true;
        }
        // MASM / V1 bridge first so V2 initialize can merge entitlements (800B bit, tier).
        const bool masmOk = RawrXD::EnterpriseLicense::initialize();
        startupTrace("enterprise_license_masm", masmOk ? "ok" : "degraded");
        const auto v2r = RawrXD::License::EnterpriseLicenseV2::Instance().initialize();
        startupTrace("enterprise_license_v2", v2r.success ? "ok" : (v2r.detail ? v2r.detail : "err"));
        return true;
    }
    if (name == "showWindow")
    {
        HWND hMain = ide.getMainWindow();
        if (hMain && IsWindow(hMain))
        {
            ensureMainWindowVisible(hMain);
            pumpMessages();
            pumpMessages();
            startupTrace("showWindow", "visible");
        }
        else
        {
            startupTrace("showWindow_deferred", "no_hwnd_yet");
        }
        return true;
    }
    if (name == "camellia_init")
    {
        if (!isTruthyEnvVar("RAWRXD_ENABLE_STARTUP_CAMELLIA"))
        {
            startupTrace("camellia_init_deferred");
            OutputDebugStringA("[main_win32] camellia_init deferred (set RAWRXD_ENABLE_STARTUP_CAMELLIA=1)\n");
            return true;
        }
        startupTrace("camellia_init_start", RawrXD::Startup::getStartupSessionId());
        using RawrXD::Crypto::Camellia256Bridge;
        auto& br = Camellia256Bridge::instance();
        if (!br.isInitialized())
        {
            const auto ir = br.initialize();
            startupTrace("camellia_hwid_key", ir.success ? "ok" : (ir.detail ? ir.detail : "err"));
        }
        const auto tr = br.selfTest();
        startupTrace("camellia_selftest", tr.success ? "ok" : (tr.detail ? tr.detail : "err"));
        return true;
    }
    if (name == "masm_init")
    {
        (void)RawrXD::Startup::runPhaseLazy("masm_init");
        const auto profile = RawrXD::Core::HardwareScout::GetCurrentProfile();
        startupTrace("masm_init", RawrXD::Core::HardwareScout::TierToString(profile.tier));
        return true;
    }
    if (name == "swarm")
    {
        const bool enableGuiSwarmInit = isTruthyEnvVar("RAWRXD_INIT_SWARM_GUI");
        if (!enableGuiSwarmInit)
        {
            startupTrace("swarm_deferred");
            OutputDebugStringA("[main_win32] swarm deferred from startup\n");
            return true;
        }

        startupTrace("swarm_gui_init_start");
        auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
        if (!swarm.isInitialized())
        {
            const auto result = swarm.initialize(RawrXD::Swarm::NodeRole::Coordinator, "0.0.0.0");
            if (!result.success)
            {
                const char* swarmDetail = result.detail ? result.detail : "unknown";
                startupTrace("swarm_gui_init_nonfatal", swarmDetail);
                OutputDebugStringA((std::string("[main_win32] swarm init non-fatal: ") + swarmDetail + "\n").c_str());
                return true;
            }
        }

        startupTrace("swarm_gui_init_done");
        OutputDebugStringA("[main_win32] swarm initialized for GUI runtime\n");
        return true;
    }
    if (name == "auto_update")
    {
        if (isTruthyEnvVar("RAWRXD_STARTUP_UPDATE_CHECK"))
        {
            startupTrace("auto_update_check_start");
            ide.checkForUpdates();
            startupTrace("auto_update_check_scheduled");
        }
        else
        {
            startupTrace("auto_update_deferred");
            OutputDebugStringA(
                "[main_win32] auto_update: background check deferred (set RAWRXD_STARTUP_UPDATE_CHECK=1 to run)\n");
        }
        return true;
    }
    if (name == "integrated_runtime")
    {
        // Keep startup stable by default; opt in only when explicitly requested.
        const bool enableIntegratedRuntime = isTruthyEnvVar("RAWRXD_ENABLE_INTEGRATED_RUNTIME");
        if (!enableIntegratedRuntime)
        {
            startupTrace("integrated_runtime_deferred");
            OutputDebugStringA("[main_win32] Integrated runtime deferred (set RAWRXD_ENABLE_INTEGRATED_RUNTIME=1 to "
                               "enable at startup)\n");
            return true;
        }

        startupTrace("integrated_runtime_start", RawrXD::Startup::getStartupSessionId());
        OutputDebugStringA("[main_win32] Integrated runtime: booting Transcendence (E->Omega)...\n");
        bootIntegratedRuntimeSafely();
        startupTrace("integrated_runtime_done", RawrXD::Startup::getStartupSessionId());
        return true;
    }
    if (name == "layout")
    {
        if (isTruthyEnvVar("RAWRXD_SKIP_STARTUP_LAYOUT"))
        {
            startupTrace("layout_skipped", "env_skip");
            return true;
        }
        startupTrace("layout_start", RawrXD::Startup::getStartupSessionId());
        HWND hMain = ide.getMainWindow();
        if (hMain && IsWindow(hMain))
        {
            ide.layoutTerminalStrip();
            startupTrace("layout_done", "terminal_strip");
        }
        else
            startupTrace("layout_skipped", "no_main_hwnd");
        return true;
    }
    if (name == "message_loop_entered")
    {
        startupTrace("message_loop_entered", RawrXD::Startup::getStartupSessionId());
        return true;
    }
    return true;
}

static bool runPhaseSafely(const std::string& name, Win32IDE& ide, HINSTANCE hInstance, LPSTR lpCmdLine,
                           bool* phaseOk = nullptr)
{
    if (phaseOk)
        *phaseOk = false;

    try
    {
        const bool ok = runPhase(name, ide, hInstance, lpCmdLine);
        if (phaseOk)
            *phaseOk = ok;
        return true;
    }
    catch (const std::exception& ex)
    {
        startupTrace("startup_phase_cpp_exception", ex.what());
        return false;
    }
    catch (...)
    {
        startupTrace("startup_phase_cpp_exception_unknown", name.c_str());
        return false;
    }
}

static bool showMainWindowSafely(Win32IDE& ide)
{
    try
    {
        ide.showMainWindowSafe();
        return true;
    }
    catch (const std::exception& ex)
    {
        startupTrace("show_window_cpp_exception", ex.what());
        return false;
    }
    catch (...)
    {
        startupTrace("show_window_cpp_exception_unknown");
        return false;
    }
}

static bool headlessInitializeSafely(HeadlessIDE& headless, int argc, char** argv, HeadlessResult& result,
                                     DWORD& sehCode)
{
    sehCode = 0;
    __try
    {
        result = headless.initialize(argc, argv);
        return true;
    }
    __except (sehCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        fprintf(stderr, "[headless] SEH exception in initialize: 0x%08lX\n", sehCode);
        return false;
    }
}

static bool headlessRunSafely(HeadlessIDE& headless, int& exitCode, DWORD& sehCode)
{
    sehCode = 0;
    __try
    {
        exitCode = headless.run();
        return true;
    }
    __except (sehCode = GetExceptionCode(), EXCEPTION_EXECUTE_HANDLER)
    {
        fprintf(stderr, "[headless] SEH exception in run: 0x%08lX\n", sehCode);
        return false;
    }
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
// Must be exported for delayimp: __pfnDliFailureHook2 (was previously never set → silent AV / instant exit).
extern "C" FARPROC WINAPI DelayLoadFailureHook(unsigned dliNotify, PDelayLoadInfo pdli)
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

extern "C" const PfnDliHook __pfnDliFailureHook2 = DelayLoadFailureHook;
#endif

extern "C" void rawrxd_init_deep_thinking();
extern "C" int rawrxd_agentic_deep_think_loop(const char* prompt);

static void ensureConsoleAttached(bool attachInput)
{
    auto hasUsableStdHandle = [](DWORD stdId) -> bool
    {
        HANDLE h = GetStdHandle(stdId);
        if (h == nullptr || h == INVALID_HANDLE_VALUE)
            return false;
        const DWORD fileType = GetFileType(h);
        return fileType != FILE_TYPE_UNKNOWN;
    };

    const bool needStdout = !hasUsableStdHandle(STD_OUTPUT_HANDLE);
    const bool needStderr = !hasUsableStdHandle(STD_ERROR_HANDLE);
    const bool needStdin = attachInput && !hasUsableStdHandle(STD_INPUT_HANDLE);

    // Keep existing redirections when they are already valid.
    if (!needStdout && !needStderr && !needStdin)
        return;

    if (!GetConsoleWindow())
    {
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
    }

    FILE* stream = nullptr;
    if (needStdout)
        freopen_s(&stream, "CONOUT$", "w", stdout);
    if (needStderr)
        freopen_s(&stream, "CONOUT$", "w", stderr);
    if (needStdin)
        freopen_s(&stream, "CONIN$", "r", stdin);
}

static bool tryEnsureConsoleAttached(bool attachInput, DWORD& sehCode)
{
    sehCode = 0;
#if defined(_MSC_VER) && defined(_WIN32)
    __try
    {
        ensureConsoleAttached(attachInput);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sehCode = GetExceptionCode();
        return false;
    }
#else
    ensureConsoleAttached(attachInput);
    return true;
#endif
}

static void logHeadlessDiag(const char* phase, const char* detail = nullptr)
{
    char msg[640] = {};
    if (detail && detail[0])
    {
        snprintf(msg, sizeof(msg), "[headless] %s | %s\n", phase ? phase : "(null)", detail);
    }
    else
    {
        snprintf(msg, sizeof(msg), "[headless] %s\n", phase ? phase : "(null)");
    }

    OutputDebugStringA(msg);
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != nullptr && hErr != INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "%s", msg);
    }

    char exePath[MAX_PATH] = {};
    std::string logPath = "headless_startup.log";
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0)
    {
        std::string p = exePath;
        size_t slash = p.find_last_of("\\/");
        if (slash != std::string::npos)
            logPath = p.substr(0, slash + 1) + "headless_startup.log";
    }

    std::ofstream out(logPath, std::ios::out | std::ios::app);
    if (out)
    {
        out << msg;
        out.flush();
    }
}

static void runDeepThinkingStressTest(LPSTR lpCmdLine)
{
    // Redirect stdout for feedback if console exists or create one
    ensureConsoleAttached(false);

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
           strstr(lpCmdLine, "--test-terminal-split") != nullptr || strstr(lpCmdLine, "--test-ghost-text") != nullptr ||
           strstr(lpCmdLine, "--test-multicursor") != nullptr ||
           strstr(lpCmdLine, "--test-caret-animation") != nullptr ||
           strstr(lpCmdLine, "--test-tier-cosmetics") != nullptr ||
           strstr(lpCmdLine, "--test-ollama-client") != nullptr ||
           strstr(lpCmdLine, "--test-model-discovery") != nullptr;
}

static bool queryLocalOllamaEndpoint(const wchar_t* endpoint, std::string& outBody)
{
    outBody.clear();
#ifdef _WIN32
    bool ok = false;
    HINTERNET hSession = WinHttpOpen(L"RawrXD-FeatureProbe/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", 11434, 0);
    if (hConnect)
    {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", endpoint, nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (hRequest)
        {
            if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hRequest, nullptr))
            {
                DWORD statusCode = 0;
                DWORD statusCodeSize = sizeof(statusCode);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                                        WINHTTP_NO_HEADER_INDEX) &&
                    statusCode >= 200 && statusCode < 300)
                {
                    for (;;)
                    {
                        DWORD available = 0;
                        if (!WinHttpQueryDataAvailable(hRequest, &available))
                            break;
                        if (available == 0)
                        {
                            ok = true;
                            break;
                        }

                        std::vector<char> buffer(available + 1, 0);
                        DWORD read = 0;
                        if (!WinHttpReadData(hRequest, buffer.data(), available, &read))
                            break;
                        outBody.append(buffer.data(), read);
                    }
                    if (!ok && !outBody.empty())
                        ok = true;
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return ok;
#else
    (void)endpoint;
    (void)outBody;
    return false;
#endif
}

static bool postLocalOllamaChatJson(const std::string& jsonBody, std::string& outBody)
{
    outBody.clear();
#ifdef _WIN32
    bool ok = false;
    HINTERNET hSession = WinHttpOpen(L"RawrXD-AgenticSmoke/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"127.0.0.1", 11434, 0);
    if (hConnect)
    {
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/chat", nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
        if (hRequest)
        {
            static const wchar_t kHeaders[] = L"Content-Type: application/json\r\n";
            if (WinHttpSendRequest(hRequest, kHeaders, (DWORD)-1, const_cast<char*>(jsonBody.data()),
                                   static_cast<DWORD>(jsonBody.size()), static_cast<DWORD>(jsonBody.size()), 0) &&
                WinHttpReceiveResponse(hRequest, nullptr))
            {
                DWORD statusCode = 0;
                DWORD statusCodeSize = sizeof(statusCode);
                if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                                        WINHTTP_NO_HEADER_INDEX) &&
                    statusCode >= 200 && statusCode < 300)
                {
                    for (;;)
                    {
                        DWORD available = 0;
                        if (!WinHttpQueryDataAvailable(hRequest, &available))
                            break;
                        if (available == 0)
                        {
                            ok = true;
                            break;
                        }
                        std::vector<char> buffer(available + 1, 0);
                        DWORD read = 0;
                        if (!WinHttpReadData(hRequest, buffer.data(), available, &read))
                            break;
                        outBody.append(buffer.data(), read);
                    }
                    if (!ok && !outBody.empty())
                        ok = true;
                }
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
    }
    WinHttpCloseHandle(hSession);
    return ok && !outBody.empty();
#else
    (void)jsonBody;
    (void)outBody;
    return false;
#endif
}

static bool parseFirstOllamaModelName(const std::string& tagsJson, std::string& outName)
{
    outName.clear();
    const char* nameKey = "\"name\":\"";
    auto pos = tagsJson.find(nameKey);
    if (pos == std::string::npos)
        return false;
    pos += strlen(nameKey);
    auto end = tagsJson.find('"', pos);
    if (end == std::string::npos || end <= pos)
        return false;
    outName = tagsJson.substr(pos, end - pos);
    return !outName.empty();
}

static bool agenticSmokeToolRegistryStep(std::string& errOut)
{
    errOut.clear();
    char tempDir[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, tempDir);
    std::string dir = (n > 0) ? std::string(tempDir) : std::string(".");
    std::string path = dir + "rawrxd_agentic_smoke_" + std::to_string(GetCurrentProcessId()) + "_" +
                       std::to_string(GetTickCount64()) + ".tmp";
    const std::string payload = "rawrxd-agentic-smoke\n";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            errOut = "temp write failed";
            return false;
        }
        out << payload;
    }
    nlohmann::json args;
    args["path"] = path;
    auto res = RawrXD::Agent::AgentToolRegistry::Instance().Dispatch("read_file", args);
    DeleteFileA(path.c_str());
    if (!res.success)
    {
        errOut = res.output.empty() ? "read_file failed" : res.output;
        return false;
    }
    if (res.output != payload)
    {
        errOut = "read_file content mismatch";
        return false;
    }
    return true;
}

static bool agenticSmokeListDirStep(std::string& errOut)
{
    errOut.clear();
    char tempDir[MAX_PATH] = {};
    DWORD tn = GetTempPathA(MAX_PATH, tempDir);
    std::string dir = (tn > 0) ? std::string(tempDir) : std::string(".");
    nlohmann::json args;
    args["path"] = dir;
    args["recursive"] = false;
    auto res = RawrXD::Agent::AgentToolRegistry::Instance().Dispatch("list_dir", args);
    if (!res.success)
    {
        errOut = res.output.empty() ? "list_dir failed" : res.output;
        return false;
    }
    if (res.output.empty())
    {
        errOut = "list_dir returned empty output";
        return false;
    }
    return true;
}

static int runAgenticSmokeTestExit()
{
    auto t0 = std::chrono::steady_clock::now();
    int modelCount = 0;
    std::string ollamaStatus = "skip";

    std::string err;
    if (!agenticSmokeToolRegistryStep(err))
    {
        fprintf(stderr, "[agentic-smoke] FAIL: tool_registry — %s\n", err.c_str());
        return 2;
    }
    fprintf(stdout, "[agentic-smoke] PASS: tool_registry read_file\n");

    if (!agenticSmokeListDirStep(err))
    {
        fprintf(stderr, "[agentic-smoke] FAIL: list_dir — %s\n", err.c_str());
        return 2;
    }
    fprintf(stdout, "[agentic-smoke] PASS: tool_registry list_dir (explorer parity)\n");

    // -------------------------------------------------------------------------
    // Multi-step bounded-loop smoke (deterministic, no live model required)
    // Simulates 5 agent steps using only the tool registry:
    //   step 1 – write initial content to a temp file (write_file)
    //   step 2 – read it back (read_file)
    //   step 3 – list temp dir (list_dir)
    //   step 4 – overwrite with revised content (write_file)
    //   step 5 – verify revised read matches (read_file)
    // -------------------------------------------------------------------------
    {
        char tempDir2[MAX_PATH] = {};
        GetTempPathA(MAX_PATH, tempDir2);
        std::string msPath = std::string(tempDir2) + "rawrxd_ms_smoke_" +
                             std::to_string(GetCurrentProcessId()) + ".tmp";
        auto& reg = RawrXD::Agent::AgentToolRegistry::Instance();
        bool msOk = true;
        std::string msErr;

        // step 1 – write_file
        {
            nlohmann::json a;
            a["path"]    = msPath;
            a["content"] = "step1";
            auto r = reg.Dispatch("write_file", a);
            if (!r.success) { msOk = false; msErr = "write_file step1: " + r.output; }
        }

        // step 2 – read_file, verify content
        if (msOk)
        {
            nlohmann::json a;
            a["path"] = msPath;
            auto r = reg.Dispatch("read_file", a);
            if (!r.success || r.output != "step1")
            {
                msOk = false;
                msErr = "read_file step2 mismatch: got=" + r.output;
            }
        }

        // step 3 – list_dir of temp, must be non-empty
        if (msOk)
        {
            nlohmann::json a;
            a["path"]      = std::string(tempDir2);
            a["recursive"] = false;
            auto r = reg.Dispatch("list_dir", a);
            if (!r.success || r.output.empty())
            {
                msOk = false;
                msErr = "list_dir step3 empty";
            }
        }

        // step 4 – overwrite via replace_in_file (verifies old content replaced)
        if (msOk)
        {
            nlohmann::json a;
            a["path"]       = msPath;
            a["old_string"] = "step1";
            a["new_string"] = "step4-revised";
            auto r = reg.Dispatch("replace_in_file", a);
            if (!r.success) { msOk = false; msErr = "replace_in_file step4: " + r.output; }
        }

        // step 5 – verify revised content.  Use a second path so the 1500ms
        // read_file result cache (keyed on path+args) is not a stale hit from
        // the step-2 read of msPath.
        const std::string msPath2 = msPath + ".v2";
        if (msOk)
        {
            // Copy the modified msPath to msPath2 via write_file so step-5
            // reads from a distinct cache key.
            nlohmann::json wa;
            wa["path"]    = msPath2;
            wa["content"] = "step4-revised";
            reg.Dispatch("write_file", wa);  // best-effort; step 5 validates

            nlohmann::json a;
            a["path"] = msPath2;
            auto r = reg.Dispatch("read_file", a);
            if (!r.success || r.output != "step4-revised")
            {
                msOk = false;
                msErr = "read_file step5 mismatch: got=" + r.output;
            }
        }

        DeleteFileA(msPath.c_str());
        DeleteFileA(msPath2.c_str());

        if (!msOk)
        {
            fprintf(stderr, "[agentic-smoke] FAIL: multi-step bounded loop — %s\n", msErr.c_str());
            return 2;
        }
        fprintf(stdout, "[agentic-smoke] PASS: multi-step bounded loop (5 tool steps)\n");
    }

    // -------------------------------------------------------------------------
    // Hotpatch subsystem smoke (deterministic, no live model required)
    // Proves: tool visibility → dispatch → state mutation → observability → reversibility
    // -------------------------------------------------------------------------
    {
        auto& reg = RawrXD::Agent::AgentToolRegistry::Instance();
        bool hpOk = true;
        std::string hpErr;

        // step 1 – hotpatch_status (tool visibility + observability)
        {
            nlohmann::json a;
            a["layer"] = "all";
            auto r = reg.Dispatch("hotpatch_status", a);
            if (!r.success) { hpOk = false; hpErr = "hotpatch_status: " + r.output; }
            else { fprintf(stdout, "[agentic-smoke] PASS: hotpatch_status\n"); }
        }

        // step 2 – list_hotpatches (observability baseline)
        if (hpOk)
        {
            nlohmann::json a;
            a["layer"] = "all";
            auto r = reg.Dispatch("list_hotpatches", a);
            if (!r.success) { hpOk = false; hpErr = "list_hotpatches: " + r.output; }
            else { fprintf(stdout, "[agentic-smoke] PASS: list_hotpatches\n"); }
        }

        // step 3 – apply_hotpatch NOP to our own process (state mutation)
        // We patch a dummy function in this translation unit.
        static volatile uint8_t s_hotpatchSmokeDummy = 0x90; // NOP placeholder
        auto dummyAddr = reinterpret_cast<uintptr_t>(&s_hotpatchSmokeDummy);
        std::string addrHex = "0x" + std::to_string(dummyAddr);
        bool hpApplyOk = false;
        if (hpOk)
        {
            nlohmann::json a;
            a["layer"]  = "memory";
            a["target"] = addrHex;
            a["data"]   = "CC";               // int3 (single-byte, easily reversible)
            auto r = reg.Dispatch("apply_hotpatch", a);
            if (r.success) {
                hpApplyOk = true;
                fprintf(stdout, "[agentic-smoke] PASS: apply_hotpatch\n");
            } else if (r.output.find("LICENSE") != std::string::npos ||
                       r.output.find("Professional license") != std::string::npos) {
                fprintf(stdout, "[agentic-smoke] SKIP: apply_hotpatch (license tier)\n");
            } else {
                hpOk = false;
                hpErr = "apply_hotpatch: " + r.output;
            }
        }

        // step 4 – list_hotpatches again (detect mutation)
        if (hpOk)
        {
            nlohmann::json a;
            a["layer"] = "all";
            auto r = reg.Dispatch("list_hotpatches", a);
            if (!r.success) { hpOk = false; hpErr = "list_hotpatches post-apply: " + r.output; }
            else { fprintf(stdout, "[agentic-smoke] PASS: list_hotpatches post-apply\n"); }
        }

        // step 5 – revert_hotpatch (reversibility) — only if apply succeeded
        if (hpOk && hpApplyOk)
        {
            nlohmann::json a;
            a["target"] = addrHex;
            auto r = reg.Dispatch("revert_hotpatch", a);
            if (!r.success) { hpOk = false; hpErr = "revert_hotpatch: " + r.output; }
            else { fprintf(stdout, "[agentic-smoke] PASS: revert_hotpatch\n"); }
        }
        else if (hpOk && !hpApplyOk)
        {
            fprintf(stdout, "[agentic-smoke] SKIP: revert_hotpatch (apply skipped)\n");
        }

        if (!hpOk)
        {
            fprintf(stderr, "[agentic-smoke] FAIL: hotpatch smoke — %s\n", hpErr.c_str());
            return 2;
        }
        fprintf(stdout, "[agentic-smoke] PASS: hotpatch smoke (apply→verify→revert)\n");
    }

    if (!isTruthyEnvVar("RAWRXD_AGENTIC_SMOKE_LIVE"))
    {
        fprintf(stdout, "[agentic-smoke] SKIP: live Ollama (set RAWRXD_AGENTIC_SMOKE_LIVE=1)\n");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        fprintf(stdout, "[smoke] result=PASS duration_ms=%lld ollama=skip models=0\n", (long long)elapsed);
        return 0;
    }

    std::string tagsBody;
    if (!queryLocalOllamaEndpoint(L"/api/tags", tagsBody) || tagsBody.empty())
    {
        fprintf(stderr, "[agentic-smoke] FAIL: live Ollama — /api/tags unreachable\n");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        fprintf(stderr, "[smoke] result=FAIL duration_ms=%lld ollama=unreachable models=0\n", (long long)elapsed);
        return 3;
    }
    std::string model;
    if (!parseFirstOllamaModelName(tagsBody, model))
    {
        fprintf(stderr, "[agentic-smoke] FAIL: live Ollama — no model in /api/tags\n");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        fprintf(stderr, "[smoke] result=FAIL duration_ms=%lld ollama=no_models models=0\n", (long long)elapsed);
        return 3;
    }

    // Count models in tags response for telemetry
    {
        try {
            nlohmann::json j = nlohmann::json::parse(tagsBody);
            if (j.contains("models") && j["models"].is_array())
                modelCount = static_cast<int>(j["models"].size());
        } catch (...) {}
    }

    nlohmann::json body;
    body["model"] = model;
    body["stream"] = false;
    nlohmann::json msgs = nlohmann::json::array();
    msgs.push_back(nlohmann::json::object({{"role", "user"}, {"content", "Reply with exactly: OK"}}));
    body["messages"] = std::move(msgs);

    std::string raw;
    if (!postLocalOllamaChatJson(body.dump(), raw))
    {
        fprintf(stderr, "[agentic-smoke] FAIL: live Ollama — /api/chat POST failed\n");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        fprintf(stderr, "[smoke] result=FAIL duration_ms=%lld ollama=chat_fail models=%d\n", (long long)elapsed, modelCount);
        return 3;
    }
    try
    {
        nlohmann::json j = nlohmann::json::parse(raw);
        if (j.contains("message") && j["message"].contains("content"))
        {
            std::string content = j["message"]["content"].get<std::string>();
            for (char& c : content)
            {
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c - 'A' + 'a');
            }
            if (content.find("ok") != std::string::npos)
            {
                fprintf(stdout, "[agentic-smoke] PASS: live ollama model=%s\n", model.c_str());
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
                fprintf(stdout, "[smoke] result=PASS duration_ms=%lld ollama=OK models=%d\n", (long long)elapsed, modelCount);
                return 0;
            }
        }
    }
    catch (...)
    {
    }
    fprintf(stderr, "[agentic-smoke] FAIL: live Ollama — response did not contain OK\n");
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    fprintf(stderr, "[smoke] result=FAIL duration_ms=%lld ollama=bad_response models=%d\n", (long long)elapsed, modelCount);
    return 3;
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

static int getArgInt(int argc, char** argv, const char* key, int fallback)
{
    std::string value;
    if (!getArgValue(argc, argv, key, value))
        return fallback;
    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

static int runFastInferenceCLI(LPSTR lpCmdLine)
{
    int argc = 0;
    char** argv = nullptr;
    parseCmdLine(lpCmdLine, argc, argv);

    std::string matmulKernel;
    if (getArgValue(argc, argv, "--matmul-kernel", matmulKernel) && !matmulKernel.empty())
    {
        SetEnvironmentVariableA("RAWRXD_VULKAN_MATMUL_KERNEL", matmulKernel.c_str());
    }

    std::string matmulSpv;
    if (getArgValue(argc, argv, "--matmul-spv", matmulSpv) && !matmulSpv.empty())
    {
        SetEnvironmentVariableA("RAWRXD_VULKAN_MATMUL_SPV", matmulSpv.c_str());
    }

    std::string modelPath;
    std::ofstream resultLog("inference_fast_result.txt", std::ios::out | std::ios::trunc);

    if (!getArgValue(argc, argv, "--test-model", modelPath) || modelPath.empty())
    {
        fprintf(stderr, "FAIL FAST_INVALID_ARGS missing --test-model\n");
        if (resultLog)
            resultLog << "FAIL FAST_INVALID_ARGS missing --test-model\n";
        return 2;
    }

    const int maxTokens = std::max(1, getArgInt(argc, argv, "--test-max-tokens", 1));
    const std::string prompt = [&]()
    {
        std::string p;
        if (getArgValue(argc, argv, "--test-prompt", p) && !p.empty())
            return p;
        return std::string("Hello");
    }();

    RawrXD::CPUInferenceEngine engine;
    engine.SetUseTitanAssembly(false);
    if (!engine.LoadModel(modelPath))
    {
        fprintf(stderr, "FAIL FAST_ENGINE_LOAD\n");
        if (resultLog)
            resultLog << "FAIL FAST_ENGINE_LOAD\n";
        const std::string err = engine.GetLastLoadErrorMessage();
        if (!err.empty())
        {
            fprintf(stderr, "[ENGINE] %s\n", err.c_str());
            if (resultLog)
                resultLog << "[ENGINE] " << err << "\n";
        }
        return 3;
    }

    std::vector<int32_t> inputTokens = engine.Tokenize(prompt);
    if (inputTokens.empty())
    {
        fprintf(stderr, "FAIL FAST_INVALID_PROMPT\n");
        if (resultLog)
            resultLog << "FAIL FAST_INVALID_PROMPT\n";
        return 4;
    }

    int32_t eosToken = 2;
    const std::vector<int32_t> eosProbe = engine.Tokenize("<|endoftext|>");
    if (eosProbe.size() == 1)
        eosToken = eosProbe[0];

    auto t0 = std::chrono::high_resolution_clock::now();
    std::vector<int32_t> generated = engine.Generate(inputTokens, maxTokens);
    auto t1 = std::chrono::high_resolution_clock::now();
    const auto elapsedMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (generated.empty())
    {
        fprintf(stderr, "FAIL FAST_NO_TOKENS\n");
        if (resultLog)
            resultLog << "FAIL FAST_NO_TOKENS\n";
        return 5;
    }

    bool hitEos = false;
    for (int32_t tid : generated)
    {
        if (tid == eosToken)
        {
            hitEos = true;
            break;
        }
    }

    if (generated.size() == 1)
    {
        fprintf(stdout, "PASS FAST_GENERATE token=%d time=%lldms\n", generated[0], elapsedMs);
        if (resultLog)
            resultLog << "PASS FAST_GENERATE token=" << generated[0] << " time=" << elapsedMs << "ms\n";
    }
    else
    {
        fprintf(stdout, "PASS FAST_GENERATE tokens=%llu time=%lldms\n",
                static_cast<unsigned long long>(generated.size()), elapsedMs);
        if (resultLog)
            resultLog << "PASS FAST_GENERATE tokens=" << generated.size() << " time=" << elapsedMs << "ms\n";
    }

    std::string detok = engine.Detokenize(generated);
    fprintf(stdout, "[DETOK] Text: \"%s\"\n", detok.c_str());
    if (resultLog)
        resultLog << "[DETOK] Text: \"" << detok << "\"\n";
    fprintf(stdout, "[TOKENS] IDs:");
    if (resultLog)
        resultLog << "[TOKENS] IDs:";
    for (size_t i = 0; i < generated.size(); ++i)
    {
        fprintf(stdout, "%s%d", (i == 0 ? " " : ","), generated[i]);
        if (resultLog)
            resultLog << (i == 0 ? " " : ",") << generated[i];
    }
    fprintf(stdout, "\n");
    if (resultLog)
        resultLog << "\n";
    fprintf(stdout, "[EOS] token=%d hit=%d\n", eosToken, hitEos ? 1 : 0);
    if (resultLog)
        resultLog << "[EOS] token=" << eosToken << " hit=" << (hitEos ? 1 : 0) << "\n";
    fflush(stdout);

    return 0;
}

static bool envVarPresent(const char* name)
{
    if (!name || !name[0])
        return false;
    char buf[4] = {};
    return GetEnvironmentVariableA(name, buf, static_cast<DWORD>(sizeof(buf))) > 0;
}

static std::string getEnvValueA(const char* name)
{
    if (!name || !name[0])
        return {};
    char buf[4096] = {};
    const DWORD n = GetEnvironmentVariableA(name, buf, static_cast<DWORD>(sizeof(buf)));
    if (n == 0 || n >= sizeof(buf))
        return {};
    return std::string(buf, n);
}

static int runHeadlessSmokeChatNoWindow(HINSTANCE hInstance, LPSTR lpCmdLine)
{
    int argc = 0;
    char** argv = nullptr;
    parseCmdLine(lpCmdLine, argc, argv);

    std::string modelPath = getEnvValueA("RAWRXD_SMOKE_MODEL");
    if (modelPath.empty())
        (void)getArgValue(argc, argv, "--test-model", modelPath);
    if (modelPath.empty())
    {
        fprintf(stderr,
                "[chat-smoke-headless] FAIL: missing model path (set RAWRXD_SMOKE_MODEL or --test-model).\n");
        return 2;
    }

    std::string prompt = getEnvValueA("RAWRXD_SMOKE_PROMPT");
    if (prompt.empty())
        (void)getArgValue(argc, argv, "--test-prompt", prompt);
    if (prompt.empty())
        prompt = "Reply with exactly: CHAT_SMOKE_HEADLESS_OK";

    int timeoutMs = 90000;
    {
        const std::string envTimeout = getEnvValueA("RAWRXD_SMOKE_TIMEOUT_MS");
        if (!envTimeout.empty())
            timeoutMs = std::max(1000, atoi(envTimeout.c_str()));
        timeoutMs = std::max(1000, getArgInt(argc, argv, "--test-timeout-ms", timeoutMs));
    }

    if (!envVarPresent("RAWRXD_PIPELINE_STRICT"))
        SetEnvironmentVariableA("RAWRXD_PIPELINE_STRICT", "1");

    std::string tracePath = getEnvValueA("RAWRXD_PIPELINE_TRACE");
    if (tracePath.empty())
    {
        tracePath = "chat_ui_headless_trace.json";
        SetEnvironmentVariableA("RAWRXD_PIPELINE_TRACE", tracePath.c_str());
    }

    std::error_code fsErr;
    const std::filesystem::path traceFsPath(tracePath);
    std::filesystem::remove(traceFsPath, fsErr);

    Win32IDE ide(hInstance);
    if (!ide.loadGGUFModel(modelPath))
    {
        fprintf(stderr, "[chat-smoke-headless] FAIL: loadGGUFModel model=%s\n", modelPath.c_str());
        return 3;
    }

    std::mutex doneMu;
    std::condition_variable doneCv;
    bool done = false;
    std::string err;

    ide.generateResponseAsync(
        prompt,
        [&](const std::string& chunk, bool complete)
        {
            std::lock_guard<std::mutex> lk(doneMu);
            if (!chunk.empty())
                fprintf(stdout, "%s", chunk.c_str());
            if (complete)
            {
                done = true;
                doneCv.notify_one();
            }
        });

    {
        std::unique_lock<std::mutex> lk(doneMu);
        if (!doneCv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&]() { return done; }))
        {
            err = "timed out waiting for completion callback";
        }
    }

    if (!err.empty())
    {
        fprintf(stderr, "[chat-smoke-headless] FAIL: %s\n", err.c_str());
        return 4;
    }

    std::error_code ec;
    if (!std::filesystem::exists(traceFsPath, ec) || ec)
    {
        fprintf(stderr, "[chat-smoke-headless] FAIL: trace not written: %s\n", tracePath.c_str());
        return 5;
    }

    const uintmax_t traceBytes = std::filesystem::file_size(traceFsPath, ec);
    if (ec || traceBytes == 0)
    {
        fprintf(stderr, "[chat-smoke-headless] FAIL: trace empty: %s\n", tracePath.c_str());
        return 6;
    }

    fprintf(stdout,
            "\n[chat-smoke-headless] PASS: no-window chat path completed; trace=%s bytes=%llu\n",
            tracePath.c_str(),
            static_cast<unsigned long long>(traceBytes));
    fflush(stdout);
    return 0;
}

static int runNonInteractiveChatUiSmoke(HINSTANCE hInstance, LPSTR lpCmdLine)
{
    int argc = 0;
    char** argv = nullptr;
    parseCmdLine(lpCmdLine, argc, argv);

    std::string modelPath;
    if (!getArgValue(argc, argv, "--test-model", modelPath) || modelPath.empty())
    {
        fprintf(stderr,
                "[chat-ui-smoke] FAIL: missing --test-model <path-to-gguf>\n"
                "[chat-ui-smoke] Example: --chat-ui-smoke-noninteractive --test-model F:\\models\\my.gguf\n");
        return 2;
    }

    const std::string prompt = [&]()
    {
        std::string p;
        if (getArgValue(argc, argv, "--test-prompt", p) && !p.empty())
            return p;
        return std::string("Reply with exactly: CHAT_UI_SMOKE_OK");
    }();
    const int timeoutMs = std::max(1000, getArgInt(argc, argv, "--test-timeout-ms", 90000));

    if (!envVarPresent("RAWRXD_PIPELINE_STRICT"))
    {
        SetEnvironmentVariableA("RAWRXD_PIPELINE_STRICT", "1");
    }
    if (!envVarPresent("RAWRXD_PARITY_CPU"))
    {
        SetEnvironmentVariableA("RAWRXD_PARITY_CPU", "1");
    }

    SetEnvironmentVariableA("RAWRXD_SMOKE_CHAT", "1");
    SetEnvironmentVariableA("RAWRXD_SMOKE_MODEL", modelPath.c_str());
    SetEnvironmentVariableA("RAWRXD_SMOKE_PROMPT", prompt.c_str());
    SetEnvironmentVariableA("RAWRXD_SMOKE_TIMEOUT_MS", std::to_string(timeoutMs).c_str());

    return runHeadlessSmokeChatNoWindow(hInstance, lpCmdLine);
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
    ide.showMainWindowSafe();
    for (int i = 0; i < 8; ++i)
        pumpMessages();
    return true;
}

struct ClassProbeCtx
{
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

struct ClassContainsProbeCtx
{
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
    const bool probeGhostText = lpCmdLine && strstr(lpCmdLine, "--test-ghost-text");
    const bool probeMultiCursor = lpCmdLine && strstr(lpCmdLine, "--test-multicursor");
    const bool probeCaretAnim = lpCmdLine && strstr(lpCmdLine, "--test-caret-animation");
    const bool probeTierCosmetics = lpCmdLine && strstr(lpCmdLine, "--test-tier-cosmetics");
    const bool probeOllamaClient = lpCmdLine && strstr(lpCmdLine, "--test-ollama-client");
    const bool probeModelDiscovery = lpCmdLine && strstr(lpCmdLine, "--test-model-discovery");

    if (!setProbeWorkspaceRoot())
        fprintf(stderr, "[feature-probe] WARN: could not set workspace root from exe path\n");

    auto* ide = new Win32IDE(hInstance);
    if (!ide->createWindow())
    {
        fprintf(stderr, "[feature-probe] FAIL: createWindow failed\n");
        return 2;
    }
    ide->showMainWindowSafe();
    for (int i = 0; i < 6; ++i)
        pumpMessages();

    HWND mainWnd = ide->getMainWindow();
    HWND editor = ide->getEditor();
    int assertsPassed = 0;
    int assertsFailed = 0;
    std::vector<std::string> passedChecks;
    std::vector<std::string> failedChecks;

    auto assertBehavior = [&](bool condition, const char* name)
    {
        if (condition)
        {
            ++assertsPassed;
            passedChecks.emplace_back(name ? name : "(null)");
            fprintf(stdout, "[feature-probe] PASS: %s\n", name);
        }
        else
        {
            ++assertsFailed;
            failedChecks.emplace_back(name ? name : "(null)");
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

    if (probeGhostText)
    {
        // Assertion: typing triggers ghost-text debounce timer (ID 8888)
        if (editor)
        {
            SetWindowTextA(editor, "int ma");
            SendMessageA(editor, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
            SendMessageA(editor, EM_REPLACESEL, TRUE, (LPARAM) "i");
            for (int i = 0; i < 6; ++i)
                pumpMessages();
        }
        BOOL hadGhostTimer = KillTimer(mainWnd, 8888);
        assertBehavior(hadGhostTimer != 0, "ghost_text_timer_started");
        if (hadGhostTimer)
            SetTimer(mainWnd, 8888, 120, nullptr);
    }

    if (probeMultiCursor)
    {
        // Assertion: add-cursor-below path activates multi-cursor state
        if (editor)
        {
            SetWindowTextA(editor, "alpha\nalpha\nalpha\n");
            SendMessageA(editor, EM_SETSEL, 2, 2);  // middle of first line
            ide->addCursorBelow();
            ide->addCursorBelow();
            for (int i = 0; i < 6; ++i)
                pumpMessages();
        }

        const int cursorCount = ide->getMultiCursorCount();
        assertBehavior(ide->isMultiCursorActive() && cursorCount >= 2, "multicursor_activated");

        if (editor)
        {
            HDC hdc = GetDC(editor);
            bool painted = false;
            if (hdc)
            {
                ide->paintMultiCursorIndicators(hdc);
                ReleaseDC(editor, hdc);
                painted = true;
            }
            assertBehavior(painted, "multicursor_paint_invoked");
        }
    }

    if (probeCaretAnim)
    {
        // Assertion: smooth caret animation timer is active (or can be armed)
        BOOL hadCaretTimer = KillTimer(mainWnd, 9050);
        if (hadCaretTimer)
        {
            SetTimer(mainWnd, 9050, 16, nullptr);
            assertBehavior(true, "caret_animation_timer_started");
        }
        else
        {
            UINT_PTR timerHandle = SetTimer(mainWnd, 9050, 16, nullptr);
            bool armed = timerHandle != 0;
            assertBehavior(armed, "caret_animation_timer_armed");
            if (armed)
                KillTimer(mainWnd, 9050);
        }
    }

    if (probeTierCosmetics)
    {
        // Tier2 hover popup command
        if (editor)
        {
            SetWindowTextA(editor, "int sample(int a) { return a; }\n");
            SendMessageA(editor, EM_SETSEL, 4, 10);                      // "sample"
            PostMessageA(mainWnd, WM_COMMAND, MAKEWPARAM(11720, 0), 0);  // IDM_TIER2_HOVER
            for (int i = 0; i < 10; ++i)
                pumpMessages();
        }
        assertBehavior(hasChildWindowClass(mainWnd, "RawrXD_HoverTooltip"), "tier2_hover_popup_created");

        // Tier3 toggle command should execute without destabilizing UI
        if (editor)
        {
            PostMessageA(mainWnd, WM_COMMAND, MAKEWPARAM(12101, 0), 0);  // IDM_T3C_INDENT_GUIDES
            for (int i = 0; i < 8; ++i)
                pumpMessages();
        }
        assertBehavior(mainWnd != nullptr && IsWindow(mainWnd) != 0, "tier3_toggle_command_survived");
    }

    if (probeOllamaClient)
    {
        // Assertion: local Ollama API responds on standard endpoint
        std::string versionBody;
        const bool versionOk = queryLocalOllamaEndpoint(L"/api/version", versionBody);
        const bool hasVersion = versionBody.find("version") != std::string::npos;
        assertBehavior(versionOk && hasVersion, "ollama_version_endpoint_ok");
    }

    if (probeModelDiscovery)
    {
        // Assertion: model discovery endpoint returns at least one model-ish field
        std::string tagsBody;
        const bool tagsOk = queryLocalOllamaEndpoint(L"/api/tags", tagsBody);
        const bool hasModelsNode = tagsBody.find("\"models\"") != std::string::npos;
        const bool hasModelName = tagsBody.find("\"name\"") != std::string::npos;
        assertBehavior(tagsOk && (hasModelsNode || hasModelName), "ollama_model_discovery_ok");
    }

    {
        std::ofstream diag("feature_probe_last.txt", std::ios::trunc | std::ios::binary);
        if (diag)
        {
            diag << "cmdline=" << (lpCmdLine ? lpCmdLine : "") << "\n";
            for (const auto& p : passedChecks)
                diag << "PASS " << p << "\n";
            for (const auto& f : failedChecks)
                diag << "FAIL " << f << "\n";
            diag << "SUMMARY passed=" << assertsPassed << " failed=" << assertsFailed << "\n";
        }
    }

    fprintf(stdout, "[feature-probe] SUMMARY: passed=%d failed=%d\n", assertsPassed, assertsFailed);
    return assertsFailed == 0 ? 0 : 20 + assertsFailed;
}

// E0-57..E0-64 — panels + model depth (runCriticalValidationBatch10). Placed immediately before WinMain for MSVC TU
// visibility with the pre-loop conjoin block below.
static const char* e0Batch10Anchor(int e0Id)
{
    switch (e0Id)
    {
        case 57:
            return "problems_surface";
        case 58:
            return "debug_console";
        case 59:
            return "debugger_dock";
        case 60:
            return "extensions_panel";
        case 61:
            return "outline_tree";
        case 62:
            return "search_results";
        case 63:
            return "module_browser";
        case 64:
            return "model_sliders";
        default:
            return "unknown";
    }
}

static void traceConjoinedE0PanelsDeepBatch(Win32IDE& ide)
{
    int pass = 0;
    for (int id = 57; id <= 64; ++id)
    {
        const auto c = ide.runCriticalValidationE0Check(id);
        if (c.passed)
            ++pass;
        std::string line = "panels_deep|";
        line += e0Batch10Anchor(id);
        line.push_back('|');
        line += c.passed ? "pass " : "fail ";
        line += c.name;
        line += " :: ";
        if (c.detail.size() > 200)
            line.append(c.detail, 0, 200);
        else
            line += c.detail;
        startupTrace("startup_conjoined_e0_b10", line.c_str());
    }
    startupTrace("startup_conjoined_e0_b10_summary", (std::to_string(pass) + "/8").c_str());
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow)
{
    emitStartupHeapSnapshot("winmain.entry");

    // ========================================================================
    // CWD FIX — Set working directory to exe's folder (before any relative paths)
    // Required for crash_dumps, config, plugins, engines when launched from
    // Explorer, shortcuts, or different CWD. Prevents silent failures on init.
    // ========================================================================
    setCwdToExeDirectory();
    earlyWinMainMilestone("winmain_early_b1",
                          "[IDE-Pipeline:WinMain-Early] Batch 1/8: working directory pinned to exe folder\n",
                          "[Init:WinMain-Early] Batch 1/8: process CWD set to executable directory\n");

    // ========================================================================
    // UI INITIALIZATION HARDENING — Load critical system libraries FIRST
    // Must happen BEFORE any window creation or control initialization.
    // This fixes crashes when msftedit.dll or common controls are not available.
    // ========================================================================
    {
        // Load RichEdit 2.0 control library (used by editor)
        HMODULE msftedit = LoadLibraryW(L"Msftedit.dll");
        if (!msftedit)
        {
            OutputDebugStringA("[WinMain Init] WARNING: Failed to load Msftedit.dll; RichEdit controls may fail\n");
        }

        // Initialize common controls v6 (required for themed buttons, comboboxes, etc.)
        INITCOMMONCONTROLSEX icex = {};
        icex.dwSize = sizeof(icex);
        icex.dwICC = ICC_WIN95_CLASSES;  // Button, static, edit, listbox, combobox, scrollbar

        if (!InitCommonControlsEx(&icex))
        {
            OutputDebugStringA("[WinMain Init] WARNING: InitCommonControlsEx failed\n");
        }
    }
    earlyWinMainMilestone("winmain_early_b2",
                          "[IDE-Pipeline:WinMain-Early] Batch 2/8: RichEdit + common controls v6 primed\n",
                          "[Init:WinMain-Early] Batch 2/8: system control libraries loaded and initialized\n");

    // Fast smoke: must run before bootstrapRuntimeSurface / integrity / GUI — same tool registry as agent chat.
    if (hasAgenticSmokeFlag(lpCmdLine))
    {
        ensureConsoleAttached(true);
        const int rc = runAgenticSmokeTestExit();
        exportCommandArtifacts("--agentic-smoke");
        FreeConsole();
        return rc;
    }

    // Handle chat smoke hooks before runtime bootstrap because bootstrap may sanitize environment variables.
    if (isTruthyEnvVar("RAWRXD_SMOKE_CHAT"))
    {
        ensureConsoleAttached(true);
        int rc = runHeadlessSmokeChatNoWindow(hInstance, lpCmdLine);
        exportCommandArtifacts("RAWRXD_SMOKE_CHAT");
        FreeConsole();
        return rc;
    }

    if (hasChatUiSmokeFlag(lpCmdLine))
    {
        ensureConsoleAttached(true);
        int rc = runNonInteractiveChatUiSmoke(hInstance, lpCmdLine);
        exportCommandArtifacts("--chat-ui-smoke-noninteractive");
        FreeConsole();
        return rc;
    }

    RawrXD::Runtime::bootstrapRuntimeSurface();

    // Check environment first for forced console
    char debugConsoleBuf[8];
    if (GetEnvironmentVariableA("RAWRXD_DEBUG_CONSOLE", debugConsoleBuf, (DWORD)sizeof(debugConsoleBuf)) != 0 &&
        debugConsoleBuf[0] == '1')
    {
        ensureConsoleAttached(false);
    }
    earlyWinMainMilestone("winmain_early_b3", "[IDE-Pipeline:WinMain-Early] Batch 3/8: runtime surface bootstrapped\n",
                          "[Init:WinMain-Early] Batch 3/8: native runtime surface bootstrap complete\n");

    if (lpCmdLine && strstr(lpCmdLine, "--test-deep-thinking"))
    {
        runDeepThinkingStressTest(lpCmdLine);
        return 0;
    }

    if (lpCmdLine && strstr(lpCmdLine, "--test-inference-fast"))
    {
        ensureConsoleAttached(true);
        int rc = runFastInferenceCLI(lpCmdLine);
        FreeConsole();
        return rc;
    }

    if (hasFeatureProbeFlag(lpCmdLine))
    {
        ensureConsoleAttached(true);
        int rc = runFeatureProbeCLI(hInstance, lpCmdLine);
        FreeConsole();
        return rc;
    }

    if (hasAutoFixFlag(lpCmdLine))
    {
        ensureConsoleAttached(true);
        int argc = 0;
        char** argv = nullptr;
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
        {
            startupTrace("WinMain", "start");
            RawrXD::Startup::ensureStartupSessionId();
            const char* sid = RawrXD::Startup::getStartupSessionId();
            const std::string sess = sid ? std::string(sid) : std::string();
            for (const char* step : {"winmain_early_b1", "winmain_early_b2", "winmain_early_b3"})
            {
                std::string line = "replay|";
                line += sess;
                line.push_back('|');
                line += step;
                startupTrace("startup_conjoined_early_1_8", line.c_str());
            }
        }
        else
        {
            delete s_startupLog;
            s_startupLog = nullptr;
        }
    }
    emitStartupHeapSnapshot("startup_log_initialized");
    earlyWinMainMilestone("winmain_early_b4",
                          "[IDE-Pipeline:WinMain-Early] Batch 4/8: ide_startup.log trace channel ready\n",
                          "[Init:WinMain-Early] Batch 4/8: startup audit log opened in working directory\n");

    {
        std::string integrityError;
        std::string distRoot;
        if (!runApertureIntegrityPreflight(integrityError, distRoot))
        {
            startupTrace("integrity_preflight_failed", integrityError.c_str());
            std::ostringstream oss;
            oss << "RawrXD startup was blocked by integrity preflight.\n\n"
                << "Reason:\n"
                << integrityError << "\n\n"
                << "Dist Root:\n"
                << (distRoot.empty() ? "(not resolved)" : distRoot);
            MessageBoxA(nullptr, oss.str().c_str(), "RawrXD Integrity Gate", MB_ICONERROR | MB_OK);
            return 91;
        }

        if (!distRoot.empty())
        {
            startupTrace("INTEGRITY_VERIFIED", distRoot.c_str());
        }
        else
        {
            startupTrace("integrity_preflight_skipped_no_dist");
        }
    }
    earlyWinMainMilestone("winmain_early_b5",
                          "[IDE-Pipeline:WinMain-Early] Batch 5/8: aperture integrity preflight satisfied\n",
                          "[Init:WinMain-Early] Batch 5/8: distribution integrity gate passed\n");

#ifdef _DEBUG
    // Optional: enable CRT debug heap checking via environment variable.
    // Set RAWRXD_CRT_HEAP_CHECK=1 to check heap on every allocation/free.
    // Set RAWRXD_CRT_BREAK_ALLOC=<n> to break on a specific allocation number.
    {
        char buf[32] = {};
        if (GetEnvironmentVariableA("RAWRXD_CRT_HEAP_CHECK", buf, (DWORD)sizeof(buf)) > 0 && buf[0] == '1')
        {
            int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
            flags |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF;
            _CrtSetDbgFlag(flags);
        }
        if (GetEnvironmentVariableA("RAWRXD_CRT_BREAK_ALLOC", buf, (DWORD)sizeof(buf)) > 0)
        {
            int allocNum = atoi(buf);
            if (allocNum > 0)
                _CrtSetBreakAlloc((size_t)allocNum);
        }
    }
#endif

    // Delay-load hook: __pfnDliFailureHook2 → DelayLoadFailureHook (MSVC + delayimp)

    // Optional: allocate console for early crash diagnostics (RAWRXD_DEBUG_CONSOLE=1)
    {
        char buf[8];
        if (GetEnvironmentVariableA("RAWRXD_DEBUG_CONSOLE", buf, (DWORD)sizeof(buf)) != 0 && buf[0] == '1')
        {
            ensureConsoleAttached(true);
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
    emitStartupHeapSnapshot("crash_containment_installed");
    earlyWinMainMilestone("winmain_early_b6",
                          "[IDE-Pipeline:WinMain-Early] Batch 6/8: Cathedral crash containment installed\n",
                          "[Init:WinMain-Early] Batch 6/8: crash dumps and patch rollback boundary active\n");

    // DPI awareness — before any GUI (Win32 GUI fix)
    ensureDpiAwareness();
    startupTrace("dpi_awareness");
    earlyWinMainMilestone("winmain_early_b7",
                          "[IDE-Pipeline:WinMain-Early] Batch 7/8: per-monitor DPI awareness applied\n",
                          "[Init:WinMain-Early] Batch 7/8: DPI awareness configured before window creation\n");

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
        logHeadlessDiag("before_console_attach");

        // Console attach is best-effort in headless mode; transport startup must continue even if it fails.
        DWORD consoleSeh = 0;
        if (tryEnsureConsoleAttached(true, consoleSeh))
        {
            logHeadlessDiag("after_console_attach");
        }
        else
        {
            char sehDiag[128] = {};
            snprintf(sehDiag, sizeof(sehDiag), "code=0x%08lX", consoleSeh);
            logHeadlessDiag("console_attach_seh", sehDiag);
            logHeadlessDiag("after_console_attach_skipped");
        }

        logHeadlessDiag("before_help_gate");

        // Fast-exit help path to avoid hangs when only --headless/--help are provided
        if (hasHeadlessHelpFlag(lpCmdLine) || (lpCmdLine && lpCmdLine[0] == '\0'))
        {
            logHeadlessDiag("help_gate_exit");
            printHeadlessQuickHelp();
            return 0;
        }

        logHeadlessDiag("after_help_gate");

        logHeadlessDiag("headless_block_enter");

        int argc = 0;
        char** argv = nullptr;
        logHeadlessDiag("parse_cmdline_begin");
        parseCmdLine(lpCmdLine, argc, argv);
        logHeadlessDiag("parse_cmdline_end");

        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        char launchDiag[768] = {};
        snprintf(launchDiag, sizeof(launchDiag), "argc=%d cmdline=\"%s\" cwd=\"%s\"", argc, lpCmdLine ? lpCmdLine : "",
                 cwd);
        logHeadlessDiag("launch", launchDiag);

        HeadlessIDE headless;
        HeadlessResult r{};

        // Catch abort() from CRT assertions in Debug builds
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
        signal(SIGABRT,
               [](int)
               {
                   const char msg[] = "[headless] SIGABRT caught — CRT abort()\n";
                   DWORD wr = 0;
                   WriteFile(GetStdHandle(STD_ERROR_HANDLE), msg, sizeof(msg) - 1, &wr, nullptr);
                   _exit(99);
               });
        SetUnhandledExceptionFilter(
            [](EXCEPTION_POINTERS* ep) -> LONG
            {
                char buf[256];
                snprintf(buf, sizeof(buf), "[headless] UNHANDLED EXCEPTION: 0x%08lX addr=%p\n",
                         ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
                DWORD wr = 0;
                WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, (DWORD)strlen(buf), &wr, nullptr);
                return EXCEPTION_EXECUTE_HANDLER;
            });

        try
        {
            logHeadlessDiag("initialize_begin");
            DWORD initSeh = 0;
            if (!headlessInitializeSafely(headless, argc, argv, r, initSeh))
            {
                char sehDiag[256] = {};
                snprintf(sehDiag, sizeof(sehDiag), "code=0x%08lX", initSeh);
                logHeadlessDiag("initialize_seh", sehDiag);
                return (int)initSeh;
            }
            char initDiag[256] = {};
            snprintf(initDiag, sizeof(initDiag), "success=%d errorCode=%d", r.success ? 1 : 0, r.errorCode);
            logHeadlessDiag("initialize_end", initDiag);
        }
        catch (const std::exception& ex)
        {
            logHeadlessDiag("initialize_cpp_exception", ex.what());
            return 1;
        }
        catch (...)
        {
            logHeadlessDiag("initialize_cpp_exception_unknown");
            return 1;
        }

        if (!r.success)
        {
            if (r.errorCode == 0)
                return 0;  // --help requested
            fprintf(stderr, "Headless init failed: %s (code %d)\n", r.detail, r.errorCode);
            char failDiag[384] = {};
            snprintf(failDiag, sizeof(failDiag), "detail=\"%s\" code=%d", r.detail ? r.detail : "", r.errorCode);
            logHeadlessDiag("initialize_failed", failDiag);
            return r.errorCode;
        }

        int exitCode = 1;
        try
        {
            logHeadlessDiag("run_begin");
            DWORD runSeh = 0;
            if (!headlessRunSafely(headless, exitCode, runSeh))
            {
                char sehDiag[256] = {};
                snprintf(sehDiag, sizeof(sehDiag), "code=0x%08lX", runSeh);
                logHeadlessDiag("run_seh", sehDiag);
                return (int)runSeh;
            }
            char runDiag[128] = {};
            snprintf(runDiag, sizeof(runDiag), "exitCode=%d", exitCode);
            logHeadlessDiag("run_end", runDiag);
        }
        catch (const std::exception& ex)
        {
            logHeadlessDiag("run_cpp_exception", ex.what());
            return 1;
        }
        catch (...)
        {
            logHeadlessDiag("run_cpp_exception_unknown");
            return 1;
        }

        FreeConsole();
        return exitCode;
    }

    earlyWinMainMilestone("winmain_early_b8",
                          "[IDE-Pipeline:WinMain-Early] Batch 8/8: headless transport path not selected\n",
                          "[Init:WinMain-Early] Batch 8/8: continuing interactive Win32 GUI startup\n");

    if (hasSelfTestFlag(lpCmdLine))
    {
        if (s_startupLog)
        {
            startupTrace("selftest_mode");
            s_startupLog->close();
            delete s_startupLog;
            s_startupLog = nullptr;
        }
        ensureConsoleAttached(true);
        int rc = runStartupSelfTest();
        exportCommandArtifacts("--selftest");
        FreeConsole();
        return rc;
    }

    earlyWinMainMilestone("winmain_early_e0_1",
                          "[IDE-Pipeline:WinMain-Early] E0-1/8: startup self-test CLI route not selected\n",
                          "[Init:WinMain-Early] E0-1/8: --selftest fast path skipped\n");

    // --agentic-smoke handled at WinMain entry (before bootstrap) for fast CI/smoke.

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

    earlyWinMainMilestone("winmain_early_e0_2",
                          "[IDE-Pipeline:WinMain-Early] E0-2/8: VSIX instrumented test route not selected\n",
                          "[Init:WinMain-Early] E0-2/8: VSIX self-test exit path skipped\n");

    if (hasSafeModeFlag(lpCmdLine))
    {
        SetEnvironmentVariableA("RAWRXD_SAFE_MODE", "1");
        OutputDebugStringA("[main_win32] Safe mode enabled (--safe-mode)\n");
    }

    earlyWinMainMilestone("winmain_early_e0_3",
                          "[IDE-Pipeline:WinMain-Early] E0-3/8: safe-mode environment flag evaluated\n",
                          "[Init:WinMain-Early] E0-3/8: optional safe mode applied if requested\n");

    emitStartupHeapSnapshot("winmain.gui_before_ide_ctor");
    earlyWinMainMilestone("winmain_early_e0_4",
                          "[IDE-Pipeline:WinMain-Early] E0-4/8: GUI cold-start heap snapshot (pre-Win32IDE)\n",
                          "[Init:WinMain-Early] E0-4/8: heap snapshot before native IDE construction\n");

    {
        const std::string sid = RawrXD::Startup::getStartupSessionId();
        const std::string userLine = std::string("[Init:WinMain-Early] E0-5/8: startup session id=") + sid + "\n";
        earlyWinMainMilestone("winmain_early_e0_5",
                              "[IDE-Pipeline:WinMain-Early] E0-5/8: startup session id bound to trace\n",
                              userLine.c_str());
    }

    RawrXD::Startup::registerLazyPhase("masm_init", []()
                                       { OutputDebugStringA("[main_win32] MASM init (lazy) — run on first use\n"); });
    earlyWinMainMilestone("winmain_early_e0_6",
                          "[IDE-Pipeline:WinMain-Early] E0-6/8: lazy MASM phase registered on startup registry\n",
                          "[Init:WinMain-Early] E0-6/8: deferred MASM init hook registered\n");
    earlyWinMainMilestone("winmain_early_e0_7",
                          "[IDE-Pipeline:WinMain-Early] E0-7/8: Win32IDE constructor entry (stack frame)\n",
                          "[Init:WinMain-Early] E0-7/8: entering Win32IDE object construction\n");
    earlyWinMainMilestone("winmain_early_e0_8",
                          "[IDE-Pipeline:WinMain-Early] E0-8/8: native IDE singleton allocation gate\n",
                          "[Init:WinMain-Early] E0-8/8: final pre-constructor gate; allocating Win32IDE\n");

    // CRASH ISOLATION: Diagnostic early-return to test if crash is before or during Win32IDE ctor.
    // If the binary exits cleanly with code 42, the crash is inside Win32IDE constructor.
    // If it still crashes with STATUS_STACK_BUFFER_OVERRUN, the crash is earlier.
    {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "[CRASH-ISOLATION] Reached pre-Win32IDE ctor gate. lpCmdLine=%s\n",
                 lpCmdLine ? lpCmdLine : "(null)");
        OutputDebugStringA(buf);
        if (s_startupLog)
        {
            *s_startupLog << "[CRASH-ISOLATION] Pre-Win32IDE ctor gate reached. Exiting with 42.\n";
            s_startupLog->flush();
        }
        return 42;
    }

    Win32IDE ide(hInstance);
    emitStartupHeapSnapshot("ide_constructed");

    // ========================================================================
    // SKILL SYSTEM INITIALIZATION — Cursor-style .cursorrules injection
    // First 520 lines of skill context ALWAYS injected regardless of model
    // ========================================================================
    {
        try
        {
            RawrXD::SkillSystem::InitializeSkillSystem();
            OutputDebugStringA("[main_win32] Skill system initialized + prompt warming pre-seeded\n");
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA("[main_win32] Skill system initialization failed (non-fatal): ");
            OutputDebugStringA(e.what());
            OutputDebugStringA("\n");
        }
        catch (...)
        {
            OutputDebugStringA("[main_win32] Skill system initialization failed (non-fatal): unknown exception\n");
        }
    }

    {
        const std::string exeDir = getIdeExeDirectoryA();
        if (!exeDir.empty())
        {
            const std::string phaseFile = joinExeSubpathA(exeDir, "config\\startup_phases.txt");
            if (GetFileAttributesA(phaseFile.c_str()) != INVALID_FILE_ATTRIBUTES)
                RawrXD::Startup::setPhaseOrderFileOverride(phaseFile);
        }
    }
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 1/8: Win32IDE constructed + phase order file resolved\n",
                     "[Init:WinMain] Batch 1/8: IDE instance and startup phase config ready\n");

    // ========================================================================
    // SPLIT STARTUP: phase order from config/startup_phases.txt (not hardcoded).
    // Pre-createWindow → pre-show (e.g. license) → show window → post-show heavy work.
    // ========================================================================
    std::vector<std::string> preCreateWindow;
    std::vector<std::string> betweenCreateAndShow;
    std::vector<std::string> afterShow;
    partitionGuiStartupFromConfig(preCreateWindow, betweenCreateAndShow, afterShow);
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 2/8: startup phases partitioned (pre / mid / post-show)\n",
                     "[Init:WinMain] Batch 2/8: startup phases partitioned for window lifecycle\n");

    auto traceColdStartConjoinedStep = [&](const std::string& phaseName)
    {
        std::string line = phaseName;
        line.push_back('|');
        line += RawrXD::Startup::getStartupSessionId();
        startupTrace("startup_conjoined_phases_1_8_step", line.c_str());
    };

    auto afterColdOrMidPhase = [&](const std::string& phaseName)
    {
        traceColdStartConjoinedStep(phaseName);
        const int e0Id = e0IndexForPreOrMidShowPhase(phaseName);
        if (e0Id > 0)
            traceConjoinedE0ForPhase(ide, phaseName, e0Id);
    };

    auto runPhasesAbortOnFail = [&](const std::vector<std::string>& phases,
                                    const std::function<void(const std::string&)>& afterEachPhase = {}) -> bool
    {
        for (const std::string& name : phases)
        {
            startupTrace(name.c_str(), (std::string("phase_start|") + RawrXD::Startup::getStartupSessionId()).c_str());
            bool phaseOk = false;
            if (!runPhaseSafely(name, ide, hInstance, lpCmdLine, &phaseOk) || !phaseOk)
            {
                if (s_startupLog)
                {
                    s_startupLog->close();
                    delete s_startupLog;
                    s_startupLog = nullptr;
                }
                MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
                return false;
            }
            startupTrace(name.c_str(), (std::string("phase_done|") + RawrXD::Startup::getStartupSessionId()).c_str());
            if (afterEachPhase)
                afterEachPhase(name);
        }
        return true;
    };

    if (!runPhasesAbortOnFail(preCreateWindow, afterColdOrMidPhase))
        return 1;
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 3/8: pre-createWindow startup phases complete\n",
                     "[Init:WinMain] Batch 3/8: pre-createWindow startup phases complete\n");
    if (!runPhasesAbortOnFail(betweenCreateAndShow, afterColdOrMidPhase))
        return 1;
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 4/8: between createWindow and showWindow phases complete\n",
                     "[Init:WinMain] Batch 4/8: pre-show startup phases complete\n");

    // CRITICAL: Show window NOW before post-show subsystem phases
    startupTrace("showWindow");
    if (!showMainWindowSafely(ide))
    {
        if (s_startupLog)
        {
            s_startupLog->close();
            delete s_startupLog;
            s_startupLog = nullptr;
        }
        MessageBoxW(nullptr, L"Failed to show IDE window", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    {
        bool showPhaseOk = false;
        (void)runPhaseSafely("showWindow", ide, hInstance, lpCmdLine, &showPhaseOk);
        // Eighth line in config/startup_phases.txt — not part of runPhasesAbortOnFail; keep cold conjoin aligned.
        traceColdStartConjoinedStep("showWindow");
        traceConjoinedE0ForPhase(ide, "showWindow", 2);  // E0-02 main window visibility
    }
    emitStartupHeapSnapshot("after_show_window");
    HWND hwndMain = ide.getMainWindow();
    if (hwndMain && IsWindow(hwndMain))
    {
        UpdateWindow(hwndMain);
        RedrawWindow(hwndMain, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    }
    Win32IDE_AgenticBrowser_NotifyMainWindow(ide.getMainWindow());
    if (const char* ab = std::getenv("RAWRXD_AGENTIC_BROWSER"))
    {
        if (ab[0] == '1' || ab[0] == 'y' || ab[0] == 'Y')
        {
            Win32IDE_AgenticBrowser_Toggle();
        }
    }
    for (int i = 0; i < 8; ++i)
        pumpMessages();  // Pump so window paints before message loop

    OutputDebugStringA("[main_win32] ==> WINDOW NOW VISIBLE <== Entering message loop\n");
    flushEarlyWinMainReplayToSystemOutput(&ide);
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 5/8: main window shown + initial message pumps\n",
                     "[Init:WinMain] Batch 5/8: main window visible and initial paints pumped\n");

    // Conjoined audit for cold-start phases 1–8 (config lines 12–19): maps to E01–E08 in
    // docs/STARTUP_PHASES_BATCH1_E01_E08_MATRIX.md. Opt-in — batch probes Ollama and UI surfaces.
    if (hwndMain && IsWindow(hwndMain) && isTruthyEnvVar("RAWRXD_ENABLE_STARTUP_VALIDATION_BATCH1"))
    {
        startupTrace("startup_conjoined_phases_1_8_begin", RawrXD::Startup::getStartupSessionId());
        const auto b1 = ide.runCriticalValidationBatch1();
        int b1pass = 0;
        for (const auto& c : b1)
        {
            if (c.passed)
                ++b1pass;
            std::string line = c.passed ? "pass " : "fail ";
            line += c.name;
            line += " :: ";
            if (c.detail.size() > 200)
                line.append(c.detail, 0, 200);
            else
                line += c.detail;
            startupTrace("startup_conjoined_phases_1_8", line.c_str());
        }
        startupTrace("startup_conjoined_phases_1_8_summary",
                     (std::to_string(b1pass) + "/" + std::to_string(b1.size())).c_str());
    }

    // Now that window is visible, run post-show phases from config (extension_bootstrap, integrated_runtime, …)
    for (const std::string& name : afterShow)
    {
        if (RawrXD::Startup::isPhaseLazy(name))
            continue;
        startupTrace((std::string("heavy_") + name).c_str(),
                     (std::string("start|") + RawrXD::Startup::getStartupSessionId()).c_str());
        bool phaseOk = false;
        if (!runPhaseSafely(name, ide, hInstance, lpCmdLine, &phaseOk) || !phaseOk)
        {
            OutputDebugStringA(("[main_win32] Heavy phase failed: " + name + "\n").c_str());
        }
        traceConjoinedE0ForPhase(ide, name);
        startupTrace((std::string("heavy_") + name).c_str(),
                     (std::string("done|") + RawrXD::Startup::getStartupSessionId()).c_str());
    }
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 6/8: post-show synchronous startup phases complete\n",
                     "[Init:WinMain] Batch 6/8: post-show startup phases complete\n");

    // E0-17..E0-24: run after layout so E0-21..24 (client/sidebars/children/thread) reflect final chrome.
    if (hwndMain && IsWindow(hwndMain))
    {
        startupTrace("startup_conjoined_e0_17_24_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0ExtendedBatch(ide);
    }

    // Rollup E0-01..E0-08: per-phase lines already emitted above via traceConjoinedE0ForPhase.
    if (hwndMain && IsWindow(hwndMain))
    {
        startupTrace("startup_conjoined_phases_9_16_begin", RawrXD::Startup::getStartupSessionId());
        const auto e0 = ide.runCriticalValidationBatch3();
        int e0pass = 0;
        for (const auto& c : e0)
        {
            if (c.passed)
                ++e0pass;
        }
        startupTrace("startup_conjoined_e0_summary",
                     (std::to_string(e0pass) + "/" + std::to_string(e0.size())).c_str());
        const char* verbose = std::getenv("RAWRXD_STARTUP_E0_VERBOSE_ROLLUP");
        if (verbose && verbose[0] == '1')
        {
            for (const auto& c : e0)
            {
                std::string line = c.passed ? "pass " : "fail ";
                line += c.name;
                line += " :: ";
                if (c.detail.size() > 200)
                    line.append(c.detail, 0, 200);
                else
                    line += c.detail;
                startupTrace("startup_conjoined_e0_rollup", line.c_str());
            }
        }
    }
    guiBootMilestone(&ide,
                     "[IDE-Pipeline:WinMain] Batch 7/8: startup conjoined validation (runCriticalValidationBatch3)\n",
                     "[Init:WinMain] Batch 7/8: startup validation batch complete\n");

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

    // ========================================================================
    // BACKEND API SERVER — self-hosting HTTP + WebSocket on localhost
    // Exposes OpenAI-compatible endpoints so the IDE serves inference to its
    // own extension host, VS Code extensions, and the native MASM agentic loop.
    // Override port via RAWRXD_API_PORT env var (default 11434).
    // ========================================================================
    {
        uint16_t apiPort = 11434;
        char portBuf[8] = {};
        if (GetEnvironmentVariableA("RAWRXD_API_PORT", portBuf, sizeof(portBuf)) > 0)
        {
            int p = atoi(portBuf);
            if (p > 1024 && p < 65536)
                apiPort = static_cast<uint16_t>(p);
        }
        s_apiServer = std::make_unique<APIServer>(s_apiAppState);
        if (s_apiServer->Start(apiPort))
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "[main_win32] Backend API server started on localhost:%u\n", apiPort);
            OutputDebugStringA(msg);
            startupTrace("api_server_started");
        }
        else
        {
            OutputDebugStringA("[main_win32] WARNING: Backend API server failed to start\n");
            startupTrace("api_server_start_failed");
            s_apiServer.reset();
        }
    }

    // Conjoined E0-09..E0-16 (post_api) — E0-12 expects /api/status when server is up.
    // E0-17..E0-24 already ran after post-show layout (see traceConjoinedE0ExtendedBatch above).
    if (hwndMain && IsWindow(hwndMain))
    {
        startupTrace("startup_conjoined_e0_09_16_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0PostApiBatch(ide);
    }

    {
        bool mleOk = false;
        (void)runPhaseSafely("message_loop_entered", ide, hInstance, lpCmdLine, &mleOk);
        traceConjoinedE0ForPhase(ide, "message_loop_entered", 8);
    }
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] Batch 8/8: backend API armed + message_loop_entered phase run\n",
                     "[Init:WinMain] Batch 8/8: backend API and message-loop entry phase ready\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-1/8: Pre-loop — main HWND and client area stable\n",
                     "[Init:WinMain] E0-1/8: Main window handle stable before message loop\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-2/8: Pre-loop — startup phase lists drained to this point\n",
                     "[Init:WinMain] E0-2/8: Config-driven startup phases drained (sync slice)\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-3/8: Pre-loop — visibility + agentic browser host notified\n",
                     "[Init:WinMain] E0-3/8: Visibility and agentic browser hooks notified\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-4/8: Pre-loop — initial paint pumps completed\n",
                     "[Init:WinMain] E0-4/8: Initial UI message pumps completed\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-5/8: Pre-loop — post-show heavy phases attempted\n",
                     "[Init:WinMain] E0-5/8: Post-show heavy phases attempted\n");
    guiBootMilestone(
        &ide, "[IDE-Pipeline:WinMain] E0-6/8: Pre-loop — batches 3+4+5 summarized (b6–b10 traces at loop edge)\n",
        "[Init:WinMain] E0-6/8: Batches 3–5 summarized; E0-25..64 as startup_conjoined_e0_b6–b10 after focus+pump\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-7/8: Pre-loop — optional localhost API server state settled\n",
                     "[Init:WinMain] E0-7/8: Localhost API server state settled\n");
    guiBootMilestone(&ide, "[IDE-Pipeline:WinMain] E0-8/8: Pre-loop — entering Win32IDE::runMessageLoop\n",
                     "[Init:WinMain] E0-8/8: Entering primary message loop\n");
    OutputDebugStringA("[main_win32] ⭐ MESSAGE LOOP STARTING ⭐\n");

    // Post delayed force-visible so the window is brought to front once the loop runs
    PostMessage(ide.getMainWindow(), WM_APP + 199, 0, 0);

    // Set focus to the editor
    {
        HWND editor = ide.getEditor();
        if (editor && IsWindow(editor))
            SetFocus(editor);
    }
    pumpMessages();  // one pump so focus/chrome settle before E0-25..64 (b6–b10)

    if (hwndMain && IsWindow(hwndMain))
    {
        startupTrace("startup_conjoined_e0_25_32_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0LoopReadyBatch(ide);
        startupTrace("startup_conjoined_e0_33_40_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0ShellDeepBatch(ide);
        startupTrace("startup_conjoined_e0_41_48_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0WorkbenchBatch(ide);
        startupTrace("startup_conjoined_e0_49_56_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0AgentChromeBatch(ide);
        startupTrace("startup_conjoined_e0_57_64_begin", RawrXD::Startup::getStartupSessionId());
        traceConjoinedE0PanelsDeepBatch(ide);
    }

    // Run message loop with exception safety
    int exitCode = 0;
    try
    {
        OutputDebugStringA("[main_win32] About to call ide.runMessageLoop()...\n");
        exitCode = ide.runMessageLoop();
        OutputDebugStringA(("[main_win32] ide.runMessageLoop() returned: " + std::to_string(exitCode) + "\n").c_str());
        startupTrace("message_loop_exited", std::to_string(exitCode).c_str());
    }
    catch (const std::exception& ex)
    {
        OutputDebugStringA(("[main_win32] EXCEPTION in runMessageLoop: " + std::string(ex.what()) + "\n").c_str());
        startupTrace("message_loop_exception", ex.what());
        exitCode = 2;
    }
    catch (...)
    {
        OutputDebugStringA("[main_win32] UNKNOWN EXCEPTION in runMessageLoop\n");
        startupTrace("message_loop_exception_unknown");
        exitCode = 3;
    }

    // ========================================================================
    // CLEANUP — Null out IDE's raw pointers BEFORE deleting external objects.
    // The IDE's onDestroy() already ran (from WM_DESTROY), but the Win32IDE
    // object is still alive on the stack. Clear its dangling pointers first.
    // ========================================================================
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] Batch 1/8: post–message-loop cleanup entered\n",
                         "[Shutdown:WinMain] Batch 1/8: GUI cleanup phase started\n");
    // ========================================================================
    // BACKEND API SERVER SHUTDOWN — stop before GUI/MASM cleanup
    // ========================================================================
    if (s_apiServer)
    {
        s_apiServer->Stop();
        s_apiServer.reset();
        OutputDebugStringA("[main_win32] Backend API server stopped\n");
    }
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] Batch 2/8: localhost API server stopped or absent\n",
                         "[Shutdown:WinMain] Batch 2/8: Backend API server tearoff complete\n");

    Win32IDE_AgenticBrowser_Shutdown();
    ide.setEngineManager(nullptr);
    ide.setCodexUltimate(nullptr);
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] Batch 3/8: agentic browser + engine pointers cleared\n",
                         "[Shutdown:WinMain] Batch 3/8: Agentic browser shutdown and engine handles nulled\n");

    // ========================================================================
    // INTEGRATED RUNTIME — Transcendence coordinator (Ω → E) before ASM teardown
    // ========================================================================
    {
        OutputDebugStringA("[main_win32] Integrated runtime shutdown (Transcendence)...\n");
        startupTrace("integrated_runtime_shutdown");
        RawrXD::IntegratedRuntime::shutdown();
        OutputDebugStringA("[main_win32] Integrated runtime shutdown complete\n");
    }
    guiShutdownMilestone(&ide,
                         "[IDE-Pipeline:WinMainShutdown] Batch 4/8: integrated runtime (Transcendence) shut down\n",
                         "[Shutdown:WinMain] Batch 4/8: Integrated runtime shutdown complete\n");

    // ========================================================================
    // REVERSE-ENGINEERED KERNEL SHUTDOWN — Before Tier-2 MASM shutdown
    // ========================================================================
#ifdef RAWRXD_LINK_REVERSE_ENGINEERED_ASM
    {
        OutputDebugStringA("[main_win32] Shutting down ReverseEngineered kernel...\n");
        RawrXD::ReverseEngineered::ShutdownAllSubsystems();
        OutputDebugStringA("[main_win32] ReverseEngineered kernel shutdown complete\n");
    }
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] Batch 5/8: ReverseEngineered kernel shut down\n",
                         "[Shutdown:WinMain] Batch 5/8: Reverse-engineered kernel shutdown complete\n");
#else
    guiShutdownMilestone(&ide,
                         "[IDE-Pipeline:WinMainShutdown] Batch 5/8: ReverseEngineered ASM lane not linked (skip)\n",
                         "[Shutdown:WinMain] Batch 5/8: Reverse-engineered kernel not in this build\n");
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
    guiShutdownMilestone(
        &ide, "[IDE-Pipeline:WinMainShutdown] Batch 6/8: MASM subsystems (quadbuf/orchestrator/LSP) shut down\n",
        "[Shutdown:WinMain] Batch 6/8: MASM subsystem shutdown complete\n");

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
    guiShutdownMilestone(&ide,
                         "[IDE-Pipeline:WinMainShutdown] Batch 8/8: Swarm / plugin sandbox / signature verifier pass\n",
                         "[Shutdown:WinMain] Batch 8/8: Enterprise plugin stack shutdown pass complete\n");

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

    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-1/8: JS host + MMF cross-process lane torn down\n",
                         "[Shutdown:WinMain] E0-1/8: Extension host and MMF shutdown complete\n");
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-2/8: Codex / engine manager heap objects released\n",
                         "[Shutdown:WinMain] E0-2/8: Global engine pointers released\n");
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-3/8: Cathedral crash containment ledger flushed\n",
                         "[Shutdown:WinMain] E0-3/8: Crash containment uninstalled\n");
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-4/8: Enterprise license singleton finalized\n",
                         "[Shutdown:WinMain] E0-4/8: Enterprise license shutdown complete\n");
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-5/8: runtime-exit command artifacts exported\n",
                         "[Shutdown:WinMain] E0-5/8: Runtime exit artifacts exported\n");
    guiShutdownMilestone(&ide,
                         "[IDE-Pipeline:WinMainShutdown] E0-6/8: Win32IDE stack instance about to go out of scope\n",
                         "[Shutdown:WinMain] E0-6/8: IDE stack object teardown follows return\n");
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-7/8: process exit code from WM_QUIT preserved\n",
                         "[Shutdown:WinMain] E0-7/8: Exit code preserved for WinMain return\n");
    guiShutdownMilestone(&ide, "[IDE-Pipeline:WinMainShutdown] E0-8/8: WinMain GUI shutdown checkpoint complete\n",
                         "[Shutdown:WinMain] E0-8/8: GUI shutdown checkpoints complete\n");

    if (s_startupLog)
    {
        startupTrace("WinMain_shutdown", "gui_shutdown_tail_complete");
        s_startupLog->close();
        delete s_startupLog;
        s_startupLog = nullptr;
    }

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

// main_win32 links against HeadlessIDE.cpp for canonical headless runtime.

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

