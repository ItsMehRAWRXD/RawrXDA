// smoke_test_cli_parity.cpp — Comprehensive CLI/GUI parity smoke test
// Validates all handler families: view, decomp, voice, AI, inference, VscExt,
// plus the InteractiveShell command routing for new command families.
//
// Build (MSVC):
//   cl /EHsc /std:c++20 /I..\ /DRAWRXD_SMOKE_CLI_PARITY smoke_test_cli_parity.cpp
// Build (Ninja, project-integrated):
//   Built as part of RawrXD-Win32IDE target

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <functional>
#include <sstream>
#include <cstring>
#include <mutex>

// =========================================================================
// MINIMAL STUBS — just enough to compile handler calls standalone
// =========================================================================

// Forward-declare CommandResult and CommandContext from the project
#include "../core/shared_feature_dispatch.h"
#include "../interactive_shell.h"

// =========================================================================
// TEST HARNESS
// =========================================================================
static int g_pass = 0, g_fail = 0, g_total = 0;

#define CHECK(cond, msg) do { \
    g_total++; \
    if (cond) { g_pass++; std::cout << "  [PASS] " << msg << "\n"; } \
    else      { g_fail++; std::cout << "  [FAIL] " << msg << "\n"; } \
} while (0)

// =========================================================================
// Helper: build a CLI-mode CommandContext
// =========================================================================
static std::string g_captured_output;

static void captureOutput(const char* text, void* /*ud*/) {
    g_captured_output += text;
}

static CommandContext makeCLIContext(const char* args = "") {
    CommandContext ctx{};
    ctx.rawInput   = args;
    ctx.args       = args;
    ctx.idePtr     = nullptr;
    ctx.cliStatePtr= nullptr;
    ctx.commandId  = 0;
    ctx.isGui      = false;
    ctx.isHeadless = true;
    ctx.hwnd       = nullptr;
    ctx.emitEvent  = nullptr;
    ctx.outputFn   = captureOutput;
    ctx.outputUserData = nullptr;
    g_captured_output.clear();
    return ctx;
}

// =========================================================================
// Extern declarations for handlers we test
// =========================================================================

// --- View handlers (ssot_handlers_ext_dedicated.cpp) ---
extern CommandResult handleViewToggleSidebar(const CommandContext& ctx);
extern CommandResult handleViewToggleTerminal(const CommandContext& ctx);
extern CommandResult handleViewToggleOutput(const CommandContext& ctx);
extern CommandResult handleViewToggleFullscreen(const CommandContext& ctx);
extern CommandResult handleViewZoomIn(const CommandContext& ctx);
extern CommandResult handleViewZoomOut(const CommandContext& ctx);
extern CommandResult handleViewZoomReset(const CommandContext& ctx);

// --- Decomp handlers (ssot_handlers_ext_isolated.cpp) ---
extern CommandResult handleDecompRenameVar(const CommandContext& ctx);
extern CommandResult handleDecompGotoDef(const CommandContext& ctx);
extern CommandResult handleDecompFindRefs(const CommandContext& ctx);
extern CommandResult handleDecompCopyLine(const CommandContext& ctx);
extern CommandResult handleDecompCopyAll(const CommandContext& ctx);
extern CommandResult handleDecompGotoAddr(const CommandContext& ctx);

// --- Voice handlers (ssot_handlers_ext_isolated.cpp) ---
extern CommandResult handleVoiceAutoToggle(const CommandContext& ctx);
extern CommandResult handleVoiceAutoSettings(const CommandContext& ctx);
extern CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx);
extern CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx);
extern CommandResult handleVoiceAutoRateUp(const CommandContext& ctx);
extern CommandResult handleVoiceAutoRateDown(const CommandContext& ctx);
extern CommandResult handleVoiceAutoStop(const CommandContext& ctx);

// --- AI handlers (ssot_handlers_ext_dedicated.cpp) ---
extern CommandResult handleAIExplainCode(const CommandContext& ctx);
extern CommandResult handleAIRefactor(const CommandContext& ctx);
extern CommandResult handleAIGenerateTests(const CommandContext& ctx);

// --- Inference handlers (ssot_handlers_ext_isolated.cpp) ---
extern CommandResult handleInferenceStatus(const CommandContext& ctx);
extern CommandResult handleInferenceConfig(const CommandContext& ctx);
extern CommandResult handleInferenceStop(const CommandContext& ctx);

// --- VscExt handlers (ssot_handlers_ext_isolated.cpp) ---
extern CommandResult handleVscExtStatus(const CommandContext& ctx);
extern CommandResult handleVscExtListCommands(const CommandContext& ctx);
extern CommandResult handleVscExtDiagnostics(const CommandContext& ctx);
extern CommandResult handleVscExtStats(const CommandContext& ctx);

// --- Backend handlers (ssot_handlers.cpp) ---
extern CommandResult handleBackendShowStatus(const CommandContext& ctx);
extern CommandResult handleBackendSwitchLocal(const CommandContext& ctx);
extern CommandResult handleBackendSwitchOllama(const CommandContext& ctx);
extern CommandResult handleBackendConfigure(const CommandContext& ctx);
extern CommandResult handleBackendHealthCheck(const CommandContext& ctx);
extern CommandResult handleBackendSaveConfigs(const CommandContext& ctx);

// --- Dbg handlers (ssot_handlers.cpp) ---
extern CommandResult handleDbgStatus(const CommandContext& ctx);
extern CommandResult handleDbgAddBp(const CommandContext& ctx);
extern CommandResult handleDbgRemoveBp(const CommandContext& ctx);
extern CommandResult handleDbgListBps(const CommandContext& ctx);
extern CommandResult handleDbgClearBps(const CommandContext& ctx);
extern CommandResult handleDbgStack(const CommandContext& ctx);
extern CommandResult handleDbgRegisters(const CommandContext& ctx);
extern CommandResult handleDbgModules(const CommandContext& ctx);
extern CommandResult handleDbgThreads(const CommandContext& ctx);
extern CommandResult handleDbgDisasm(const CommandContext& ctx);
extern CommandResult handleDbgGo(const CommandContext& ctx);
extern CommandResult handleDbgStepInto(const CommandContext& ctx);
extern CommandResult handleDbgStepOver(const CommandContext& ctx);
extern CommandResult handleDbgStepOut(const CommandContext& ctx);

// --- Asm handlers (ssot_handlers.cpp) ---
extern CommandResult handleAsmSections(const CommandContext& ctx);
extern CommandResult handleAsmSymbolTable(const CommandContext& ctx);
extern CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx);
extern CommandResult handleAsmCallGraph(const CommandContext& ctx);
extern CommandResult handleAsmInstructionInfo(const CommandContext& ctx);
extern CommandResult handleAsmRegisterInfo(const CommandContext& ctx);
extern CommandResult handleAsmDetectConvention(const CommandContext& ctx);

// --- Confidence handlers (ssot_handlers.cpp) ---
extern CommandResult handleConfidenceStatus(const CommandContext& ctx);
extern CommandResult handleConfidenceSetPolicy(const CommandContext& ctx);

// --- Disk handlers (ssot_handlers.cpp) ---
extern CommandResult handleDiskListDrives(const CommandContext& ctx);
extern CommandResult handleDiskScanPartitions(const CommandContext& ctx);

// --- Governor/Gov handlers (ssot_handlers.cpp) ---
extern CommandResult handleGovernorStatus(const CommandContext& ctx);
extern CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx);
extern CommandResult handleGovStatus(const CommandContext& ctx);
extern CommandResult handleGovTaskList(const CommandContext& ctx);
extern CommandResult handleGovSubmitCommand(const CommandContext& ctx);
extern CommandResult handleGovKillAll(const CommandContext& ctx);

// --- Hybrid handlers (ssot_handlers.cpp) ---
extern CommandResult handleHybridStatus(const CommandContext& ctx);
extern CommandResult handleHybridComplete(const CommandContext& ctx);
extern CommandResult handleHybridDiagnostics(const CommandContext& ctx);
extern CommandResult handleHybridAnalyzeFile(const CommandContext& ctx);
extern CommandResult handleHybridSmartRename(const CommandContext& ctx);

// --- Lsp handlers (ssot_handlers.cpp) ---
extern CommandResult handleLspStatus(const CommandContext& ctx);
extern CommandResult handleLspDiagnostics(const CommandContext& ctx);
extern CommandResult handleLspRestart(const CommandContext& ctx);
extern CommandResult handleLspSaveConfig(const CommandContext& ctx);
extern CommandResult handleLspConfigure(const CommandContext& ctx);

// --- Marketplace handlers (ssot_handlers.cpp) ---
extern CommandResult handleMarketplaceList(const CommandContext& ctx);
extern CommandResult handleMarketplaceInstall(const CommandContext& ctx);

// --- Model handlers (ssot_handlers.cpp) ---
extern CommandResult handleModelList(const CommandContext& ctx);
extern CommandResult handleModelLoad(const CommandContext& ctx);
extern CommandResult handleModelUnload(const CommandContext& ctx);
extern CommandResult handleModelQuantize(const CommandContext& ctx);

// --- MultiResp handlers (ssot_handlers.cpp) ---
extern CommandResult handleMultiRespShowStatus(const CommandContext& ctx);
extern CommandResult handleMultiRespGenerate(const CommandContext& ctx);
extern CommandResult handleMultiRespCompare(const CommandContext& ctx);
extern CommandResult handleMultiRespShowStats(const CommandContext& ctx);
extern CommandResult handleMultiRespClearHistory(const CommandContext& ctx);

// --- Plugin handlers (ssot_handlers.cpp) ---
extern CommandResult handlePluginShowStatus(const CommandContext& ctx);
extern CommandResult handlePluginLoad(const CommandContext& ctx);
extern CommandResult handlePluginUnloadAll(const CommandContext& ctx);
extern CommandResult handlePluginScanDir(const CommandContext& ctx);
extern CommandResult handlePluginToggleHotload(const CommandContext& ctx);

// --- Replay handlers (ssot_handlers.cpp) ---
extern CommandResult handleReplayStatus(const CommandContext& ctx);
extern CommandResult handleReplayCheckpoint(const CommandContext& ctx);
extern CommandResult handleReplayShowLast(const CommandContext& ctx);

// --- Safety handlers (ssot_handlers.cpp) ---
extern CommandResult handleSafetyStatus(const CommandContext& ctx);
extern CommandResult handleSafetyShowViolations(const CommandContext& ctx);
extern CommandResult handleSafetyResetBudget(const CommandContext& ctx);
extern CommandResult handleSafetyRollbackLast(const CommandContext& ctx);

// --- Tier1 handlers (ssot_handlers.cpp) ---
extern CommandResult handleTier1WelcomePage(const CommandContext& ctx);
extern CommandResult handleTier1SmoothScrollToggle(const CommandContext& ctx);
extern CommandResult handleTier1MinimapEnhanced(const CommandContext& ctx);
extern CommandResult handleTier1BreadcrumbsToggle(const CommandContext& ctx);
extern CommandResult handleTier1FuzzyPalette(const CommandContext& ctx);
extern CommandResult handleTier1AutoUpdateCheck(const CommandContext& ctx);

// --- Theme handlers (ssot_handlers.cpp) ---
extern CommandResult handleThemeCatppuccin(const CommandContext& ctx);
extern CommandResult handleThemeDracula(const CommandContext& ctx);
extern CommandResult handleThemeCyberpunk(const CommandContext& ctx);
extern CommandResult handleThemeMonokai(const CommandContext& ctx);
extern CommandResult handleThemeCrimson(const CommandContext& ctx);

// --- Transparency handlers (ssot_handlers.cpp) ---
extern CommandResult handleTrans100(const CommandContext& ctx);
extern CommandResult handleTrans90(const CommandContext& ctx);
extern CommandResult handleTrans80(const CommandContext& ctx);
extern CommandResult handleTransCustom(const CommandContext& ctx);
extern CommandResult handleTransToggle(const CommandContext& ctx);

// --- Swarm handlers (ssot_handlers.cpp) ---
extern CommandResult handleSwarmStats(const CommandContext& ctx);
extern CommandResult handleSwarmConfig(const CommandContext& ctx);
extern CommandResult handleSwarmDiscovery(const CommandContext& ctx);
extern CommandResult handleSwarmCacheStatus(const CommandContext& ctx);

// --- Hotpatch handlers (ssot_handlers.cpp) ---
extern CommandResult handleHotpatchEventLog(const CommandContext& ctx);
extern CommandResult handleHotpatchProxyStats(const CommandContext& ctx);
extern CommandResult handleHotpatchMemRevert(const CommandContext& ctx);
extern CommandResult handleHotpatchResetStats(const CommandContext& ctx);

// --- Telemetry handlers (ssot_handlers.cpp) ---
extern CommandResult handleTelemetryToggle(const CommandContext& ctx);
extern CommandResult handleTelemetryDashboard(const CommandContext& ctx);
extern CommandResult handleTelemetryClear(const CommandContext& ctx);
extern CommandResult handleTelemetryExportJson(const CommandContext& ctx);

// --- Pdb handlers (ssot_handlers.cpp) ---
extern CommandResult handlePdbStatus(const CommandContext& ctx);
extern CommandResult handlePdbLoad(const CommandContext& ctx);
extern CommandResult handlePdbCacheClear(const CommandContext& ctx);
extern CommandResult handlePdbEnable(const CommandContext& ctx);

// --- Audit handlers (ssot_handlers.cpp) ---
extern CommandResult handleAuditDashboard(const CommandContext& ctx);
extern CommandResult handleAuditRunFull(const CommandContext& ctx);
extern CommandResult handleAuditDetectStubs(const CommandContext& ctx);
extern CommandResult handleAuditQuickStats(const CommandContext& ctx);

// --- RE handlers (ssot_handlers.cpp) ---
extern CommandResult handleRECompile(const CommandContext& ctx);
extern CommandResult handleREFunctions(const CommandContext& ctx);
extern CommandResult handleREDetectVulns(const CommandContext& ctx);
extern CommandResult handleREDataFlow(const CommandContext& ctx);
extern CommandResult handleREDecompilerView(const CommandContext& ctx);

// --- QW handlers (ssot_handlers.cpp) ---
extern CommandResult handleQwAlertMonitor(const CommandContext& ctx);
extern CommandResult handleQwAlertHistory(const CommandContext& ctx);
extern CommandResult handleQwAlertDismiss(const CommandContext& ctx);
extern CommandResult handleQwBackupCreate(const CommandContext& ctx);
extern CommandResult handleQwBackupList(const CommandContext& ctx);
extern CommandResult handleQwShortcutEditor(const CommandContext& ctx);

// --- Editor engine handlers (ssot_handlers.cpp) ---
extern CommandResult handleEditorStatus(const CommandContext& ctx);
extern CommandResult handleEditorCycle(const CommandContext& ctx);
extern CommandResult handleEditorRichEdit(const CommandContext& ctx);

// --- Unity/Unreal handlers (ssot_handlers.cpp) ---
extern CommandResult handleUnityInit(const CommandContext& ctx);
extern CommandResult handleUnrealInit(const CommandContext& ctx);

// --- Vision handler (ssot_handlers.cpp) ---
extern CommandResult handleVisionAnalyzeImage(const CommandContext& ctx);

// =========================================================================
// TEST: View handlers return success and produce output
// =========================================================================
void test_view_handlers() {
    std::cout << "\n--- test_view_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleViewToggleSidebar(ctx);
        CHECK(r.success, "handleViewToggleSidebar returns success");
        CHECK(!g_captured_output.empty(), "handleViewToggleSidebar produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleViewToggleTerminal(ctx);
        CHECK(r.success, "handleViewToggleTerminal returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleViewToggleOutput(ctx);
        CHECK(r.success, "handleViewToggleOutput returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleViewToggleFullscreen(ctx);
        CHECK(r.success, "handleViewToggleFullscreen returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleViewZoomIn(ctx);
        CHECK(r.success, "handleViewZoomIn returns success");
        CHECK(g_captured_output.find("Zoom") != std::string::npos ||
              g_captured_output.find("zoom") != std::string::npos,
              "handleViewZoomIn mentions zoom level");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleViewZoomOut(ctx);
        CHECK(r.success, "handleViewZoomOut returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleViewZoomReset(ctx);
        CHECK(r.success, "handleViewZoomReset returns success");
        CHECK(g_captured_output.find("100") != std::string::npos,
              "handleViewZoomReset resets to 100%");
    }
}

// =========================================================================
// TEST: Decomp handlers return success
// =========================================================================
void test_decomp_handlers() {
    std::cout << "\n--- test_decomp_handlers ---\n";

    {
        auto ctx = makeCLIContext("oldVar newVar");
        auto r = handleDecompRenameVar(ctx);
        CHECK(r.success, "handleDecompRenameVar returns success");
        CHECK(g_captured_output.find("oldVar") != std::string::npos,
              "handleDecompRenameVar echoes old name");
        CHECK(g_captured_output.find("newVar") != std::string::npos,
              "handleDecompRenameVar echoes new name");
    }
    {
        auto ctx = makeCLIContext("mySymbol");
        auto r = handleDecompGotoDef(ctx);
        CHECK(r.success, "handleDecompGotoDef returns success");
    }
    {
        auto ctx = makeCLIContext("myFunc");
        auto r = handleDecompFindRefs(ctx);
        CHECK(r.success, "handleDecompFindRefs returns success");
    }
    {
        auto ctx = makeCLIContext("42");
        auto r = handleDecompCopyLine(ctx);
        CHECK(r.success, "handleDecompCopyLine returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDecompCopyAll(ctx);
        CHECK(r.success, "handleDecompCopyAll returns success");
    }
    {
        auto ctx = makeCLIContext("0x401000");
        auto r = handleDecompGotoAddr(ctx);
        CHECK(r.success, "handleDecompGotoAddr returns success");
        CHECK(g_captured_output.find("0x401000") != std::string::npos,
              "handleDecompGotoAddr echoes target address");
    }
}

// =========================================================================
// TEST: Voice handlers return success
// =========================================================================
void test_voice_handlers() {
    std::cout << "\n--- test_voice_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoToggle(ctx);
        CHECK(r.success, "handleVoiceAutoToggle returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoSettings(ctx);
        CHECK(r.success, "handleVoiceAutoSettings returns success");
        CHECK(g_captured_output.find("SAPI5") != std::string::npos,
              "handleVoiceAutoSettings mentions SAPI5");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoNextVoice(ctx);
        CHECK(r.success, "handleVoiceAutoNextVoice returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoPrevVoice(ctx);
        CHECK(r.success, "handleVoiceAutoPrevVoice returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoRateUp(ctx);
        CHECK(r.success, "handleVoiceAutoRateUp returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoRateDown(ctx);
        CHECK(r.success, "handleVoiceAutoRateDown returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVoiceAutoStop(ctx);
        CHECK(r.success, "handleVoiceAutoStop returns success");
    }
}

// =========================================================================
// TEST: AI handlers return success (uses Ollama fallback; ok even w/o server)
// =========================================================================
void test_ai_handlers() {
    std::cout << "\n--- test_ai_handlers ---\n";

    {
        auto ctx = makeCLIContext("int main() { return 0; }");
        auto r = handleAIExplainCode(ctx);
        CHECK(r.success, "handleAIExplainCode returns success");
        CHECK(!g_captured_output.empty(), "handleAIExplainCode produces output");
    }
    {
        auto ctx = makeCLIContext("void foo() { for(int i=0;i<10;i++) {} }");
        auto r = handleAIRefactor(ctx);
        CHECK(r.success, "handleAIRefactor returns success");
    }
    {
        auto ctx = makeCLIContext("int add(int a, int b) { return a+b; }");
        auto r = handleAIGenerateTests(ctx);
        CHECK(r.success, "handleAIGenerateTests returns success");
    }
}

// =========================================================================
// TEST: Inference handlers return success
// =========================================================================
void test_inference_handlers() {
    std::cout << "\n--- test_inference_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleInferenceStatus(ctx);
        CHECK(r.success, "handleInferenceStatus returns success");
        CHECK(!g_captured_output.empty(), "handleInferenceStatus produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleInferenceConfig(ctx);
        CHECK(r.success, "handleInferenceConfig returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleInferenceStop(ctx);
        CHECK(r.success, "handleInferenceStop returns success");
    }
}

// =========================================================================
// TEST: VscExt handlers return success
// =========================================================================
void test_vscext_handlers() {
    std::cout << "\n--- test_vscext_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleVscExtStatus(ctx);
        CHECK(r.success, "handleVscExtStatus returns success");
        CHECK(!g_captured_output.empty(), "handleVscExtStatus produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVscExtListCommands(ctx);
        CHECK(r.success, "handleVscExtListCommands returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVscExtDiagnostics(ctx);
        CHECK(r.success, "handleVscExtDiagnostics returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleVscExtStats(ctx);
        CHECK(r.success, "handleVscExtStats returns success");
    }
}

// =========================================================================
// TEST: InteractiveShell routing — verify new commands reach Process*Command
// =========================================================================
void test_interactive_shell_routing() {
    std::cout << "\n--- test_interactive_shell_routing ---\n";

    // We just verify that ProcessCommand does not crash when given the new
    // command prefixes. The shell needs running state, so we set up a basic one.

    ShellConfig cfg;
    cfg.cli_mode = true;
    cfg.show_welcome = false;

    InteractiveShell shell(cfg);

    std::string captured;
    auto out_cb = [&](const std::string& s) { captured += s; };

    // Start with all nullptrs — commands don't require backends for routing
    shell.Start(nullptr, nullptr, nullptr, nullptr, out_cb, nullptr);

    // Test each new command route produces output (not a crash)
    auto testRoute = [&](const std::string& cmd, const char* label) {
        captured.clear();
        shell.ProcessCommand(cmd);
        CHECK(!captured.empty(), label);
    };

    testRoute("/debugger status",    "'/debugger status' produces output");
    testRoute("/dbg status",         "'/dbg status' alias works");
    testRoute("/swarm status",       "'/swarm status' produces output");
    testRoute("/hotpatch status",    "'/hotpatch status' produces output");
    testRoute("/build status",       "'/build status' produces output");
    testRoute("/view sidebar",       "'/view sidebar' produces output");
    testRoute("/voice toggle",       "'/voice toggle' produces output");
    testRoute("/telemetry status",   "'/telemetry status' produces output");
    testRoute("/backend status",     "'/backend status' produces output");
    testRoute("/git status",         "'/git status' produces output");
    testRoute("/agent iteration status", "'/agent iteration status' produces output");
    testRoute("/agent iteration set 1 3 phaseA testing", "'/agent iteration set' produces output");
    testRoute("/agent iteration reset", "'/agent iteration reset' produces output");

        // Validate iteration state mutation semantics
        captured.clear();
        shell.ProcessCommand("/agent iteration set 1 3 phaseA testing");
        CHECK(captured.find("Iteration updated: 1/3") != std::string::npos,
            "'/agent iteration set' reports expected progress");

        captured.clear();
        shell.ProcessCommand("/agent iteration status");
        CHECK(captured.find("Busy: true") != std::string::npos,
            "'/agent iteration status' reports busy=true after set");
        CHECK(captured.find("Progress: 1/3") != std::string::npos,
            "'/agent iteration status' reports updated progress");
        CHECK(captured.find("Phase: phaseA") != std::string::npos,
            "'/agent iteration status' reports updated phase");
        CHECK(captured.find("Message: testing") != std::string::npos,
            "'/agent iteration status' reports updated message");

        captured.clear();
        shell.ProcessCommand("/agent iteration reset");
        CHECK(captured.find("Iteration status reset") != std::string::npos,
            "'/agent iteration reset' confirms reset");

        captured.clear();
        shell.ProcessCommand("/agent iteration status");
        CHECK(captured.find("Busy: false") != std::string::npos,
            "'/agent iteration status' reports busy=false after reset");
        CHECK(captured.find("Progress: 0/0") != std::string::npos,
            "'/agent iteration status' reports reset progress");
        CHECK(captured.find("Phase: idle") != std::string::npos,
            "'/agent iteration status' reports reset phase");

    // Test help subcommands (no subcommand shows help)
    testRoute("/debugger",           "'/debugger' (no arg) shows help");
    testRoute("/swarm",              "'/swarm' (no arg) shows help");
    testRoute("/hotpatch",           "'/hotpatch' (no arg) shows help");
    testRoute("/build",              "'/build' (no arg) shows help");
    testRoute("/view",               "'/view' (no arg) shows help");
    testRoute("/voice",              "'/voice' (no arg) shows help");
    testRoute("/telemetry",          "'/telemetry' (no arg) shows help");
    testRoute("/backend",            "'/backend' (no arg) shows help");
    testRoute("/git",                "'/git' (no arg) shows help");
    testRoute("/agent iteration",    "'/agent iteration' (no action) shows usage");

    shell.Stop();
}

// =========================================================================
// TEST: View state idempotency (toggle twice returns to original)
// =========================================================================
void test_view_state_idempotency() {
    std::cout << "\n--- test_view_state_idempotency ---\n";

    // Toggle sidebar twice — should be back to original state
    auto ctx1 = makeCLIContext("");
    handleViewToggleSidebar(ctx1);
    std::string first = g_captured_output;

    auto ctx2 = makeCLIContext("");
    handleViewToggleSidebar(ctx2);
    std::string second = g_captured_output;

    CHECK(first != second, "Toggle sidebar twice produces different output");

    // Zoom in then reset — should say 100%
    auto ctxZ1 = makeCLIContext("");
    handleViewZoomIn(ctxZ1);
    auto ctxZ2 = makeCLIContext("");
    handleViewZoomReset(ctxZ2);
    CHECK(g_captured_output.find("100") != std::string::npos,
          "Zoom reset after zoom-in returns to 100%");
}

// =========================================================================
// TEST: Error handling — decomp rename with no args
// =========================================================================
void test_error_paths() {
    std::cout << "\n--- test_error_paths ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleDecompRenameVar(ctx);
        CHECK(!r.success, "handleDecompRenameVar with empty args returns error");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDecompGotoAddr(ctx);
        // Empty addr should still succeed but with no-op or error
        CHECK(true, "handleDecompGotoAddr with empty args does not crash");
    }
}

// =========================================================================
// TEST: Backend handlers
// =========================================================================
void test_backend_handlers() {
    std::cout << "\n--- test_backend_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleBackendShowStatus(ctx);
        CHECK(r.success, "handleBackendShowStatus returns success");
        CHECK(!g_captured_output.empty(), "handleBackendShowStatus produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleBackendSwitchLocal(ctx);
        CHECK(r.success, "handleBackendSwitchLocal returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleBackendSwitchOllama(ctx);
        CHECK(r.success, "handleBackendSwitchOllama returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleBackendConfigure(ctx);
        CHECK(r.success, "handleBackendConfigure returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleBackendHealthCheck(ctx);
        CHECK(r.success, "handleBackendHealthCheck returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleBackendSaveConfigs(ctx);
        CHECK(r.success, "handleBackendSaveConfigs returns success");
    }
}

// =========================================================================
// TEST: Dbg handlers
// =========================================================================
void test_dbg_handlers() {
    std::cout << "\n--- test_dbg_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgStatus(ctx);
        CHECK(r.success, "handleDbgStatus returns success");
        CHECK(!g_captured_output.empty(), "handleDbgStatus produces output");
    }
    {
        auto ctx = makeCLIContext("0x401000");
        auto r = handleDbgAddBp(ctx);
        CHECK(r.success, "handleDbgAddBp returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgListBps(ctx);
        CHECK(r.success, "handleDbgListBps returns success");
    }
    {
        auto ctx = makeCLIContext("0x401000");
        auto r = handleDbgRemoveBp(ctx);
        CHECK(r.success, "handleDbgRemoveBp returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgClearBps(ctx);
        CHECK(r.success, "handleDbgClearBps returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgStack(ctx);
        CHECK(r.success, "handleDbgStack returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgRegisters(ctx);
        CHECK(r.success, "handleDbgRegisters returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgModules(ctx);
        CHECK(r.success, "handleDbgModules returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgThreads(ctx);
        CHECK(r.success, "handleDbgThreads returns success");
    }
    {
        auto ctx = makeCLIContext("0x401000");
        auto r = handleDbgDisasm(ctx);
        CHECK(r.success, "handleDbgDisasm returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgGo(ctx);
        CHECK(r.success, "handleDbgGo returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgStepInto(ctx);
        CHECK(r.success, "handleDbgStepInto returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgStepOver(ctx);
        CHECK(r.success, "handleDbgStepOver returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDbgStepOut(ctx);
        CHECK(r.success, "handleDbgStepOut returns success");
    }
}

// =========================================================================
// TEST: Asm handlers
// =========================================================================
void test_asm_handlers() {
    std::cout << "\n--- test_asm_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleAsmSections(ctx);
        CHECK(r.success, "handleAsmSections returns success");
        CHECK(!g_captured_output.empty(), "handleAsmSections produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleAsmSymbolTable(ctx);
        CHECK(r.success, "handleAsmSymbolTable returns success");
    }
    {
        auto ctx = makeCLIContext("0x401000");
        auto r = handleAsmAnalyzeBlock(ctx);
        CHECK(r.success, "handleAsmAnalyzeBlock returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleAsmCallGraph(ctx);
        CHECK(r.success, "handleAsmCallGraph returns success");
    }
    {
        auto ctx = makeCLIContext("mov");
        auto r = handleAsmInstructionInfo(ctx);
        CHECK(r.success, "handleAsmInstructionInfo returns success");
    }
    {
        auto ctx = makeCLIContext("rax");
        auto r = handleAsmRegisterInfo(ctx);
        CHECK(r.success, "handleAsmRegisterInfo returns success");
    }
    {
        auto ctx = makeCLIContext("0x401000");
        auto r = handleAsmDetectConvention(ctx);
        CHECK(r.success, "handleAsmDetectConvention returns success");
    }
}

// =========================================================================
// TEST: Confidence handlers
// =========================================================================
void test_confidence_handlers() {
    std::cout << "\n--- test_confidence_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleConfidenceStatus(ctx);
        CHECK(r.success, "handleConfidenceStatus returns success");
        CHECK(!g_captured_output.empty(), "handleConfidenceStatus produces output");
    }
    {
        auto ctx = makeCLIContext("strict");
        auto r = handleConfidenceSetPolicy(ctx);
        CHECK(r.success, "handleConfidenceSetPolicy returns success");
    }
}

// =========================================================================
// TEST: Disk handlers
// =========================================================================
void test_disk_handlers() {
    std::cout << "\n--- test_disk_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleDiskListDrives(ctx);
        CHECK(r.success, "handleDiskListDrives returns success");
        CHECK(!g_captured_output.empty(), "handleDiskListDrives produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleDiskScanPartitions(ctx);
        CHECK(r.success, "handleDiskScanPartitions returns success");
    }
}

// =========================================================================
// TEST: Governor/Gov handlers
// =========================================================================
void test_governor_handlers() {
    std::cout << "\n--- test_governor_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleGovernorStatus(ctx);
        CHECK(r.success, "handleGovernorStatus returns success");
        CHECK(!g_captured_output.empty(), "handleGovernorStatus produces output");
    }
    {
        auto ctx = makeCLIContext("5");
        auto r = handleGovernorSetPowerLevel(ctx);
        CHECK(r.success, "handleGovernorSetPowerLevel returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleGovStatus(ctx);
        CHECK(r.success, "handleGovStatus returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleGovTaskList(ctx);
        CHECK(r.success, "handleGovTaskList returns success");
    }
    {
        auto ctx = makeCLIContext("echo hello");
        auto r = handleGovSubmitCommand(ctx);
        CHECK(r.success, "handleGovSubmitCommand returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleGovKillAll(ctx);
        CHECK(r.success, "handleGovKillAll returns success");
    }
}

// =========================================================================
// TEST: Hybrid handlers
// =========================================================================
void test_hybrid_handlers() {
    std::cout << "\n--- test_hybrid_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleHybridStatus(ctx);
        CHECK(r.success, "handleHybridStatus returns success");
        CHECK(!g_captured_output.empty(), "handleHybridStatus produces output");
    }
    {
        auto ctx = makeCLIContext("int x = 0;");
        auto r = handleHybridComplete(ctx);
        CHECK(r.success, "handleHybridComplete returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleHybridDiagnostics(ctx);
        CHECK(r.success, "handleHybridDiagnostics returns success");
    }
    {
        auto ctx = makeCLIContext("test.cpp");
        auto r = handleHybridAnalyzeFile(ctx);
        CHECK(r.success, "handleHybridAnalyzeFile returns success");
    }
    {
        auto ctx = makeCLIContext("oldName newName");
        auto r = handleHybridSmartRename(ctx);
        CHECK(r.success, "handleHybridSmartRename returns success");
    }
}

// =========================================================================
// TEST: Lsp handlers
// =========================================================================
void test_lsp_handlers() {
    std::cout << "\n--- test_lsp_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleLspStatus(ctx);
        CHECK(r.success, "handleLspStatus returns success");
        CHECK(!g_captured_output.empty(), "handleLspStatus produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleLspDiagnostics(ctx);
        CHECK(r.success, "handleLspDiagnostics returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleLspRestart(ctx);
        CHECK(r.success, "handleLspRestart returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleLspConfigure(ctx);
        CHECK(r.success, "handleLspConfigure returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleLspSaveConfig(ctx);
        CHECK(r.success, "handleLspSaveConfig returns success");
    }
}

// =========================================================================
// TEST: Marketplace handlers
// =========================================================================
void test_marketplace_handlers() {
    std::cout << "\n--- test_marketplace_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleMarketplaceList(ctx);
        CHECK(r.success, "handleMarketplaceList returns success");
        CHECK(!g_captured_output.empty(), "handleMarketplaceList produces output");
    }
    {
        auto ctx = makeCLIContext("test-extension");
        auto r = handleMarketplaceInstall(ctx);
        CHECK(r.success, "handleMarketplaceInstall returns success");
    }
}

// =========================================================================
// TEST: Model handlers
// =========================================================================
void test_model_handlers() {
    std::cout << "\n--- test_model_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleModelList(ctx);
        CHECK(r.success, "handleModelList returns success");
        CHECK(!g_captured_output.empty(), "handleModelList produces output");
    }
    {
        auto ctx = makeCLIContext("nonexistent.gguf");
        auto r = handleModelLoad(ctx);
        // May fail if file doesn't exist, but should not crash
        CHECK(true, "handleModelLoad does not crash with bad path");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleModelUnload(ctx);
        CHECK(r.success, "handleModelUnload returns success");
    }
    {
        auto ctx = makeCLIContext("test.gguf q4_0");
        auto r = handleModelQuantize(ctx);
        CHECK(r.success, "handleModelQuantize returns success");
    }
}

// =========================================================================
// TEST: MultiResp handlers
// =========================================================================
void test_multiresp_handlers() {
    std::cout << "\n--- test_multiresp_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleMultiRespShowStatus(ctx);
        CHECK(r.success, "handleMultiRespShowStatus returns success");
        CHECK(!g_captured_output.empty(), "handleMultiRespShowStatus produces output");
    }
    {
        auto ctx = makeCLIContext("hello");
        auto r = handleMultiRespGenerate(ctx);
        CHECK(r.success, "handleMultiRespGenerate returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleMultiRespCompare(ctx);
        CHECK(r.success, "handleMultiRespCompare returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleMultiRespShowStats(ctx);
        CHECK(r.success, "handleMultiRespShowStats returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleMultiRespClearHistory(ctx);
        CHECK(r.success, "handleMultiRespClearHistory returns success");
    }
}

// =========================================================================
// TEST: Plugin handlers
// =========================================================================
void test_plugin_handlers() {
    std::cout << "\n--- test_plugin_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handlePluginShowStatus(ctx);
        CHECK(r.success, "handlePluginShowStatus returns success");
        CHECK(!g_captured_output.empty(), "handlePluginShowStatus produces output");
    }
    {
        auto ctx = makeCLIContext("nonexistent.dll");
        auto r = handlePluginLoad(ctx);
        // Expected to fail gracefully with bad path
        CHECK(true, "handlePluginLoad does not crash with bad path");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handlePluginUnloadAll(ctx);
        CHECK(r.success, "handlePluginUnloadAll returns success");
    }
    {
        auto ctx = makeCLIContext(".");
        auto r = handlePluginScanDir(ctx);
        CHECK(r.success, "handlePluginScanDir returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handlePluginToggleHotload(ctx);
        CHECK(r.success, "handlePluginToggleHotload returns success");
    }
}

// =========================================================================
// TEST: Replay handlers
// =========================================================================
void test_replay_handlers() {
    std::cout << "\n--- test_replay_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleReplayStatus(ctx);
        CHECK(r.success, "handleReplayStatus returns success");
        CHECK(!g_captured_output.empty(), "handleReplayStatus produces output");
    }
    {
        auto ctx = makeCLIContext("test_checkpoint");
        auto r = handleReplayCheckpoint(ctx);
        CHECK(r.success, "handleReplayCheckpoint returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleReplayShowLast(ctx);
        CHECK(r.success, "handleReplayShowLast returns success");
    }
}

// =========================================================================
// TEST: Safety handlers
// =========================================================================
void test_safety_handlers() {
    std::cout << "\n--- test_safety_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleSafetyStatus(ctx);
        CHECK(r.success, "handleSafetyStatus returns success");
        CHECK(!g_captured_output.empty(), "handleSafetyStatus produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleSafetyShowViolations(ctx);
        CHECK(r.success, "handleSafetyShowViolations returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleSafetyResetBudget(ctx);
        CHECK(r.success, "handleSafetyResetBudget returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleSafetyRollbackLast(ctx);
        CHECK(r.success, "handleSafetyRollbackLast returns success");
    }
}

// =========================================================================
// TEST: Tier1 handlers
// =========================================================================
void test_tier1_handlers() {
    std::cout << "\n--- test_tier1_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleTier1WelcomePage(ctx);
        CHECK(r.success, "handleTier1WelcomePage returns success");
        CHECK(!g_captured_output.empty(), "handleTier1WelcomePage produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTier1SmoothScrollToggle(ctx);
        CHECK(r.success, "handleTier1SmoothScrollToggle returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTier1MinimapEnhanced(ctx);
        CHECK(r.success, "handleTier1MinimapEnhanced returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTier1BreadcrumbsToggle(ctx);
        CHECK(r.success, "handleTier1BreadcrumbsToggle returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTier1FuzzyPalette(ctx);
        CHECK(r.success, "handleTier1FuzzyPalette returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTier1AutoUpdateCheck(ctx);
        CHECK(r.success, "handleTier1AutoUpdateCheck returns success");
    }
}

// =========================================================================
// TEST: Theme handlers
// =========================================================================
void test_theme_handlers() {
    std::cout << "\n--- test_theme_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleThemeCatppuccin(ctx);
        CHECK(r.success, "handleThemeCatppuccin returns success");
        CHECK(!g_captured_output.empty(), "handleThemeCatppuccin produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleThemeDracula(ctx);
        CHECK(r.success, "handleThemeDracula returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleThemeCyberpunk(ctx);
        CHECK(r.success, "handleThemeCyberpunk returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleThemeMonokai(ctx);
        CHECK(r.success, "handleThemeMonokai returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleThemeCrimson(ctx);
        CHECK(r.success, "handleThemeCrimson returns success");
    }
}

// =========================================================================
// TEST: Transparency handlers
// =========================================================================
void test_transparency_handlers() {
    std::cout << "\n--- test_transparency_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleTrans100(ctx);
        CHECK(r.success, "handleTrans100 returns success");
        CHECK(!g_captured_output.empty(), "handleTrans100 produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTrans90(ctx);
        CHECK(r.success, "handleTrans90 returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTrans80(ctx);
        CHECK(r.success, "handleTrans80 returns success");
    }
    {
        auto ctx = makeCLIContext("75");
        auto r = handleTransCustom(ctx);
        CHECK(r.success, "handleTransCustom returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTransToggle(ctx);
        CHECK(r.success, "handleTransToggle returns success");
    }
}

// =========================================================================
// TEST: Swarm handlers
// =========================================================================
void test_swarm_handlers() {
    std::cout << "\n--- test_swarm_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleSwarmStats(ctx);
        CHECK(r.success, "handleSwarmStats returns success");
        CHECK(!g_captured_output.empty(), "handleSwarmStats produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleSwarmConfig(ctx);
        CHECK(r.success, "handleSwarmConfig returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleSwarmDiscovery(ctx);
        CHECK(r.success, "handleSwarmDiscovery returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleSwarmCacheStatus(ctx);
        CHECK(r.success, "handleSwarmCacheStatus returns success");
    }
}

// =========================================================================
// TEST: Hotpatch handlers
// =========================================================================
void test_hotpatch_handlers() {
    std::cout << "\n--- test_hotpatch_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleHotpatchEventLog(ctx);
        CHECK(r.success, "handleHotpatchEventLog returns success");
        CHECK(!g_captured_output.empty(), "handleHotpatchEventLog produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleHotpatchProxyStats(ctx);
        CHECK(r.success, "handleHotpatchProxyStats returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleHotpatchMemRevert(ctx);
        CHECK(r.success, "handleHotpatchMemRevert returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleHotpatchResetStats(ctx);
        CHECK(r.success, "handleHotpatchResetStats returns success");
    }
}

// =========================================================================
// TEST: Telemetry handlers
// =========================================================================
void test_telemetry_handlers() {
    std::cout << "\n--- test_telemetry_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleTelemetryDashboard(ctx);
        CHECK(r.success, "handleTelemetryDashboard returns success");
        CHECK(!g_captured_output.empty(), "handleTelemetryDashboard produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTelemetryToggle(ctx);
        CHECK(r.success, "handleTelemetryToggle returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTelemetryClear(ctx);
        CHECK(r.success, "handleTelemetryClear returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleTelemetryExportJson(ctx);
        CHECK(r.success, "handleTelemetryExportJson returns success");
    }
}

// =========================================================================
// TEST: Pdb handlers
// =========================================================================
void test_pdb_handlers() {
    std::cout << "\n--- test_pdb_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handlePdbStatus(ctx);
        CHECK(r.success, "handlePdbStatus returns success");
        CHECK(!g_captured_output.empty(), "handlePdbStatus produces output");
    }
    {
        auto ctx = makeCLIContext("test.pdb");
        auto r = handlePdbLoad(ctx);
        CHECK(r.success, "handlePdbLoad returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handlePdbCacheClear(ctx);
        CHECK(r.success, "handlePdbCacheClear returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handlePdbEnable(ctx);
        CHECK(r.success, "handlePdbEnable returns success");
    }
}

// =========================================================================
// TEST: Audit handlers
// =========================================================================
void test_audit_handlers() {
    std::cout << "\n--- test_audit_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleAuditDashboard(ctx);
        CHECK(r.success, "handleAuditDashboard returns success");
        CHECK(!g_captured_output.empty(), "handleAuditDashboard produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleAuditQuickStats(ctx);
        CHECK(r.success, "handleAuditQuickStats returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleAuditDetectStubs(ctx);
        CHECK(r.success, "handleAuditDetectStubs returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleAuditRunFull(ctx);
        CHECK(r.success, "handleAuditRunFull returns success");
    }
}

// =========================================================================
// TEST: RE handlers
// =========================================================================
void test_re_handlers() {
    std::cout << "\n--- test_re_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleREFunctions(ctx);
        CHECK(r.success, "handleREFunctions returns success");
        CHECK(!g_captured_output.empty(), "handleREFunctions produces output");
    }
    {
        auto ctx = makeCLIContext("test.c");
        auto r = handleRECompile(ctx);
        CHECK(r.success, "handleRECompile returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleREDetectVulns(ctx);
        CHECK(r.success, "handleREDetectVulns returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleREDataFlow(ctx);
        CHECK(r.success, "handleREDataFlow returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleREDecompilerView(ctx);
        CHECK(r.success, "handleREDecompilerView returns success");
    }
}

// =========================================================================
// TEST: QW handlers
// =========================================================================
void test_qw_handlers() {
    std::cout << "\n--- test_qw_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleQwAlertMonitor(ctx);
        CHECK(r.success, "handleQwAlertMonitor returns success");
        CHECK(!g_captured_output.empty(), "handleQwAlertMonitor produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleQwAlertHistory(ctx);
        CHECK(r.success, "handleQwAlertHistory returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleQwAlertDismiss(ctx);
        CHECK(r.success, "handleQwAlertDismiss returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleQwBackupCreate(ctx);
        CHECK(r.success, "handleQwBackupCreate returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleQwBackupList(ctx);
        CHECK(r.success, "handleQwBackupList returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleQwShortcutEditor(ctx);
        CHECK(r.success, "handleQwShortcutEditor returns success");
    }
}

// =========================================================================
// TEST: Editor engine handlers
// =========================================================================
void test_editor_engine_handlers() {
    std::cout << "\n--- test_editor_engine_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleEditorStatus(ctx);
        CHECK(r.success, "handleEditorStatus returns success");
        CHECK(!g_captured_output.empty(), "handleEditorStatus produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleEditorCycle(ctx);
        CHECK(r.success, "handleEditorCycle returns success");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleEditorRichEdit(ctx);
        CHECK(r.success, "handleEditorRichEdit returns success");
    }
}

// =========================================================================
// TEST: Unity/Unreal handlers
// =========================================================================
void test_unity_unreal_handlers() {
    std::cout << "\n--- test_unity_unreal_handlers ---\n";

    {
        auto ctx = makeCLIContext("");
        auto r = handleUnityInit(ctx);
        CHECK(r.success, "handleUnityInit returns success");
        CHECK(!g_captured_output.empty(), "handleUnityInit produces output");
    }
    {
        auto ctx = makeCLIContext("");
        auto r = handleUnrealInit(ctx);
        CHECK(r.success, "handleUnrealInit returns success");
    }
}

// =========================================================================
// TEST: Vision handler
// =========================================================================
void test_vision_handler() {
    std::cout << "\n--- test_vision_handler ---\n";

    {
        auto ctx = makeCLIContext("test.png");
        auto r = handleVisionAnalyzeImage(ctx);
        CHECK(r.success, "handleVisionAnalyzeImage returns success");
        CHECK(!g_captured_output.empty(), "handleVisionAnalyzeImage produces output");
    }
}

// =========================================================================
// MAIN
// =========================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "  CLI/GUI Parity Smoke Test Suite\n";
    std::cout << "========================================\n";

    test_view_handlers();
    test_decomp_handlers();
    test_voice_handlers();
    test_ai_handlers();
    test_inference_handlers();
    test_vscext_handlers();
    test_interactive_shell_routing();
    test_view_state_idempotency();
    test_error_paths();
    test_backend_handlers();
    test_dbg_handlers();
    test_asm_handlers();
    test_confidence_handlers();
    test_disk_handlers();
    test_governor_handlers();
    test_hybrid_handlers();
    test_lsp_handlers();
    test_marketplace_handlers();
    test_model_handlers();
    test_multiresp_handlers();
    test_plugin_handlers();
    test_replay_handlers();
    test_safety_handlers();
    test_tier1_handlers();
    test_theme_handlers();
    test_transparency_handlers();
    test_swarm_handlers();
    test_hotpatch_handlers();
    test_telemetry_handlers();
    test_pdb_handlers();
    test_audit_handlers();
    test_re_handlers();
    test_qw_handlers();
    test_editor_engine_handlers();
    test_unity_unreal_handlers();
    test_vision_handler();

    std::cout << "\n========================================\n";
    std::cout << "  Results: " << g_pass << "/" << g_total << " passed, "
              << g_fail << " failed\n";
    std::cout << "========================================\n";

    if (g_fail > 0) {
        std::cout << "  *** FAILURES DETECTED ***\n";
        return 1;
    }
    std::cout << "  ALL TESTS PASSED\n";
    return 0;
}
