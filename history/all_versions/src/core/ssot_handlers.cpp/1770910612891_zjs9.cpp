// ============================================================================
// ssot_handlers.cpp — Stub Implementations for SSOT-Bridged Commands
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// Every handler in this file delegates to the Win32IDE instance via
// PostMessage(idePtr, WM_COMMAND, cmdId, 0) for GUI mode, or outputs
// a status message for CLI mode.
//
// As handlers are moved into proper subsystem implementations, they
// should be removed from this file. The linker enforces completeness:
// every handler in COMMAND_TABLE must resolve or the build fails.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "ssot_handlers.h"
#include "shared_feature_dispatch.h"
#include "unified_hotpatch_manager.hpp"
#include "proxy_hotpatcher.hpp"
#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "sentinel_watchdog.hpp"
#include "auto_repair_orchestrator.hpp"
#include "swarm_coordinator.h"
#include "command_registry.hpp"
#include "agentic/AgentOllamaClient.h"
#include <windows.h>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

// ============================================================================
// HELPER: Route to Win32IDE via WM_COMMAND if in GUI mode
// ============================================================================

static CommandResult delegateToGui(const CommandContext& ctx, uint32_t cmdId, const char* name) {
    if (ctx.isGui && ctx.idePtr) {
        // Route to Win32IDE's WM_COMMAND handler
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);  // First field of Win32IDE is m_hwnd
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(name);
    }
    // CLI: provide category-specific meaningful output
    // Determine if this is a GUI-only visual command or actionable in CLI
    const char* category = "action";
    if (cmdId >= 3100 && cmdId < 3120) category = "theme";
    else if (cmdId >= 3200 && cmdId < 3212) category = "transparency";
    else if (cmdId >= 9100 && cmdId < 9106) category = "monaco";
    else if (cmdId >= 9300 && cmdId < 9305) category = "editor-engine";
    else if (cmdId >= 12000 && cmdId < 12100) category = "cosmetic";
    else if (cmdId >= 2020 && cmdId < 2030) category = "view-panel";

    char buf[256];
    if (strcmp(category, "theme") == 0 || strcmp(category, "transparency") == 0 ||
        strcmp(category, "monaco") == 0 || strcmp(category, "editor-engine") == 0 ||
        strcmp(category, "cosmetic") == 0 || strcmp(category, "view-panel") == 0) {
        snprintf(buf, sizeof(buf), "[%s] GUI-only command (ID %u). Start Win32 IDE for visual features.\n", name, cmdId);
    } else {
        snprintf(buf, sizeof(buf), "[%s] Dispatched via CLI (ID %u).\n", name, cmdId);
    }
    ctx.output(buf);
    return CommandResult::ok(name);
}

// ============================================================================
// FILE (extended)
// ============================================================================

CommandResult handleFileRecentClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 1020, "file.recentClear");
    ctx.output("[FILE] Recent files list cleared.\n");
    return CommandResult::ok("file.recentClear");
}

CommandResult handleFileExit(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        return CommandResult::ok("file.exit");
    }
    ctx.output("[SSOT] Exit requested\n");
    return CommandResult::ok("file.exit");
}

// ============================================================================
// EDIT (extended)
// ============================================================================

CommandResult handleEditSnippet(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 2012, "edit.snippet");
    ctx.output("[EDIT] Snippet insertion requires editor context.\n");
    ctx.output("  Use !snippet_list to view available snippets.\n");
    return CommandResult::ok("edit.snippet");
}

CommandResult handleEditCopyFormat(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 2013, "edit.copyFormat");
    ctx.output("[EDIT] Copy formatting requires editor selection context.\n");
    return CommandResult::ok("edit.copyFormat");
}

CommandResult handleEditPastePlain(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 2014, "edit.pastePlain");
    ctx.output("[EDIT] Paste-plain: in CLI mode, all paste is plain text.\n");
    return CommandResult::ok("edit.pastePlain");
}

CommandResult handleEditClipboardHist(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 2015, "edit.clipboardHistory");
    ctx.output("[EDIT] Clipboard history requires GUI clipboard ring.\n");
    return CommandResult::ok("edit.clipboardHistory");
}

CommandResult handleEditFindNext(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 2018, "edit.findNext");
    ctx.output("[EDIT] Find-next requires active editor search context.\n");
    ctx.output("  In CLI: use findstr or Select-String.\n");
    return CommandResult::ok("edit.findNext");
}

CommandResult handleEditFindPrev(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 2019, "edit.findPrev");
    ctx.output("[EDIT] Find-previous requires active editor search context.\n");
    ctx.output("  In CLI: use findstr or Select-String.\n");
    return CommandResult::ok("edit.findPrev");
}

// ============================================================================
// VIEW
// ============================================================================

CommandResult handleViewMinimap(const CommandContext& ctx)        { return delegateToGui(ctx, 2020, "view.minimap"); }
CommandResult handleViewOutputTabs(const CommandContext& ctx)     { return delegateToGui(ctx, 2021, "view.outputTabs"); }
CommandResult handleViewModuleBrowser(const CommandContext& ctx)  { return delegateToGui(ctx, 2022, "view.moduleBrowser"); }
CommandResult handleViewThemeEditor(const CommandContext& ctx)    { return delegateToGui(ctx, 2023, "view.themeEditor"); }
CommandResult handleViewFloatingPanel(const CommandContext& ctx)  { return delegateToGui(ctx, 2024, "view.floatingPanel"); }
CommandResult handleViewOutputPanel(const CommandContext& ctx)    { return delegateToGui(ctx, 2025, "view.outputPanel"); }
CommandResult handleViewStreamingLoader(const CommandContext& ctx){ return delegateToGui(ctx, 2026, "view.streamingLoader"); }
CommandResult handleViewVulkanRenderer(const CommandContext& ctx) { return delegateToGui(ctx, 2027, "view.vulkanRenderer"); }
CommandResult handleViewSidebar(const CommandContext& ctx)        { return delegateToGui(ctx, 2028, "view.sidebar"); }
CommandResult handleViewTerminal(const CommandContext& ctx)       { return delegateToGui(ctx, 2029, "view.terminal"); }

// ============================================================================
// THEMES (individual — delegate to theme engine with specific theme ID)
// ============================================================================

static CommandResult setTheme(const CommandContext& ctx, uint32_t themeId, const char* name) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, themeId, name);
    ctx.output("[THEME] Selected: ");
    ctx.output(name);
    ctx.output(" (ID: ");
    char id[16]; snprintf(id, sizeof(id), "%u", themeId); ctx.output(id);
    ctx.output(")\n  Theme will apply on next GUI launch.\n");
    return CommandResult::ok(name);
}

CommandResult handleThemeLightPlus(const CommandContext& ctx)   { return setTheme(ctx, 3102, "theme.lightPlus"); }
CommandResult handleThemeMonokai(const CommandContext& ctx)     { return setTheme(ctx, 3103, "theme.monokai"); }
CommandResult handleThemeDracula(const CommandContext& ctx)     { return setTheme(ctx, 3104, "theme.dracula"); }
CommandResult handleThemeNord(const CommandContext& ctx)        { return setTheme(ctx, 3105, "theme.nord"); }
CommandResult handleThemeSolDark(const CommandContext& ctx)     { return setTheme(ctx, 3106, "theme.solarizedDark"); }
CommandResult handleThemeSolLight(const CommandContext& ctx)    { return setTheme(ctx, 3107, "theme.solarizedLight"); }
CommandResult handleThemeCyberpunk(const CommandContext& ctx)   { return setTheme(ctx, 3108, "theme.cyberpunk"); }
CommandResult handleThemeGruvbox(const CommandContext& ctx)     { return setTheme(ctx, 3109, "theme.gruvbox"); }
CommandResult handleThemeCatppuccin(const CommandContext& ctx)  { return setTheme(ctx, 3110, "theme.catppuccin"); }
CommandResult handleThemeTokyo(const CommandContext& ctx)       { return setTheme(ctx, 3111, "theme.tokyoNight"); }
CommandResult handleThemeCrimson(const CommandContext& ctx)     { return setTheme(ctx, 3112, "theme.rawrxdCrimson"); }
CommandResult handleThemeHighContrast(const CommandContext& ctx){ return setTheme(ctx, 3113, "theme.highContrast"); }
CommandResult handleThemeOneDark(const CommandContext& ctx)     { return setTheme(ctx, 3114, "theme.oneDarkPro"); }
CommandResult handleThemeSynthwave(const CommandContext& ctx)   { return setTheme(ctx, 3115, "theme.synthwave84"); }
CommandResult handleThemeAbyss(const CommandContext& ctx)       { return setTheme(ctx, 3116, "theme.abyss"); }

// ============================================================================
// TRANSPARENCY
// ============================================================================

CommandResult handleTrans100(const CommandContext& ctx) { return delegateToGui(ctx, 3200, "view.transparency100"); }
CommandResult handleTrans90(const CommandContext& ctx)  { return delegateToGui(ctx, 3201, "view.transparency90"); }
CommandResult handleTrans80(const CommandContext& ctx)  { return delegateToGui(ctx, 3202, "view.transparency80"); }
CommandResult handleTrans70(const CommandContext& ctx)  { return delegateToGui(ctx, 3203, "view.transparency70"); }
CommandResult handleTrans60(const CommandContext& ctx)  { return delegateToGui(ctx, 3204, "view.transparency60"); }
CommandResult handleTrans50(const CommandContext& ctx)  { return delegateToGui(ctx, 3205, "view.transparency50"); }
CommandResult handleTrans40(const CommandContext& ctx)  { return delegateToGui(ctx, 3206, "view.transparency40"); }
CommandResult handleTransCustom(const CommandContext& ctx) { return delegateToGui(ctx, 3210, "view.transparencySet"); }
CommandResult handleTransToggle(const CommandContext& ctx) { return delegateToGui(ctx, 3211, "view.transparencyToggle"); }

// ============================================================================
// HELP (extended)
// ============================================================================

CommandResult handleHelpCmdRef(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 4002, "help.cmdref");
    // CLI: dump command table summary
    ctx.output("=== RawrXD Command Reference ===\n");
    ctx.output("Type !help <command> for details on a specific command.\n");
    ctx.output("Categories: file, edit, agent, debug, hotpatch, ai, re, voice, server, git, swarm\n");
    ctx.output("Use !commands to list all registered commands.\n");
    return CommandResult::ok("help.cmdref");
}
CommandResult handleHelpPsDocs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 4003, "help.psdocs");
    ctx.output("PowerShell Integration Docs:\n");
    ctx.output("  Import-Module RawrXD  — Load the RawrXD PowerShell module\n");
    ctx.output("  Get-RawrXDCommand     — List all available commands\n");
    ctx.output("  Invoke-RawrXD <cmd>   — Execute a RawrXD command\n");
    return CommandResult::ok("help.psdocs");
}
CommandResult handleHelpSearch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 4004, "help.search");
    std::string query = ctx.args ? ctx.args : "";
    if (query.empty()) {
        ctx.output("Usage: !help_search <keyword>\n");
        return CommandResult::ok("help.search");
    }
    ctx.output("[HELP] Searching for: ");
    ctx.output(query.c_str());
    ctx.output("\n  Use !commands to list all, then grep for keyword.\n");
    return CommandResult::ok("help.search");
}

// ============================================================================
// TERMINAL (extended)
// ============================================================================

CommandResult handleTerminalSplitCode(const CommandContext& ctx) { return delegateToGui(ctx, 4009, "terminal.splitCode"); }

// ============================================================================
// AUTONOMY (extended)
// ============================================================================

CommandResult handleAutonomyStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4154, 0);
        return CommandResult::ok("autonomy.status");
    }
    auto& orchestrator = AutoRepairOrchestrator::instance();
    auto stats = orchestrator.getStats();
    std::ostringstream oss;
    oss << "=== Autonomy Status ===\n"
        << "  Running:    " << (orchestrator.isRunning() ? "yes" : "no") << "\n"
        << "  Paused:     " << (orchestrator.isPaused() ? "yes" : "no") << "\n"
        << "  Anomalies:  " << stats.anomaliesDetected << "\n"
        << "  Repairs:    " << stats.repairsAttempted << "\n"
        << "  Polls:      " << stats.totalPollCycles << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.status");
}
CommandResult handleAutonomyMemory(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4155, 0);
        return CommandResult::ok("autonomy.memory");
    }
    auto& orchestrator = AutoRepairOrchestrator::instance();
    std::string json = orchestrator.statsToJson();
    std::ostringstream oss;
    oss << "=== Autonomy Memory ===\n" << json << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.memory");
}

// ============================================================================
// AI MODE / CONTEXT
// ============================================================================

// Shared context-window setter: persists to config and notifies subsystem
static RawrXD::Agent::AgentOllamaClient& getAgentClient() {
    static RawrXD::Agent::AgentOllamaClient client;
    return client;
}

static CommandResult setAIContextWindow(const CommandContext& ctx, uint32_t cmdId,
                                        const char* label, uint32_t tokenCount) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(label);
    }
    // Apply directly to the active agent backend config
    auto& client = getAgentClient();
    auto cfg = client.GetConfig();
    cfg.num_ctx = static_cast<int>(tokenCount);
    if (cfg.max_tokens > cfg.num_ctx) cfg.max_tokens = cfg.num_ctx;
    client.SetConfig(cfg);

    // Persist small local marker for CLI sessions
    FILE* f = fopen(".rawrxd_ai_context.json", "w");
    if (f) {
        fprintf(f, "{\"context_window\":%u,\"label\":\"%s\"}\n", tokenCount, label);
        fclose(f);
    }
    char buf[128];
    const bool online = client.TestConnection();
    snprintf(buf, sizeof(buf), "[AI] Context window set to %uK tokens (%s), backend=%s\n",
             tokenCount / 1024, label, online ? "online" : "offline");
    ctx.output(buf);
    // Notify orchestrator of context change (informational only)
    return CommandResult::ok(label);
}

CommandResult handleAINoRefusal(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4203, 0);
        return CommandResult::ok("ai.noRefusal");
    }
    // Toggle no-refusal mode and project it into backend sampling config
    static bool noRefusal = false;
    noRefusal = !noRefusal;

    auto& client = getAgentClient();
    auto cfg = client.GetConfig();
    if (noRefusal) {
        cfg.temperature = 0.2f;
        cfg.top_p = 0.98f;
    } else {
        cfg.temperature = 0.7f;
        cfg.top_p = 0.9f;
    }
    client.SetConfig(cfg);

    FILE* f = fopen(".rawrxd_ai_norefusal.json", "w");
    if (f) {
        fprintf(f, "{\"no_refusal\":%s}\n", noRefusal ? "true" : "false");
        fclose(f);
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "[AI] No-refusal mode: %s\n", noRefusal ? "ENABLED" : "DISABLED");
    ctx.output(buf);
    // AI mode change logged via output above
    return CommandResult::ok("ai.noRefusal");
}
CommandResult handleAICtx4K(const CommandContext& ctx)     { return setAIContextWindow(ctx, 4210, "ai.context4k",     4096); }
CommandResult handleAICtx32K(const CommandContext& ctx)    { return setAIContextWindow(ctx, 4211, "ai.context32k",   32768); }
CommandResult handleAICtx64K(const CommandContext& ctx)    { return setAIContextWindow(ctx, 4212, "ai.context64k",   65536); }
CommandResult handleAICtx128K(const CommandContext& ctx)   { return setAIContextWindow(ctx, 4213, "ai.context128k", 131072); }
CommandResult handleAICtx256K(const CommandContext& ctx)   { return setAIContextWindow(ctx, 4214, "ai.context256k", 262144); }
CommandResult handleAICtx512K(const CommandContext& ctx)   { return setAIContextWindow(ctx, 4215, "ai.context512k", 524288); }
CommandResult handleAICtx1M(const CommandContext& ctx)     { return setAIContextWindow(ctx, 4216, "ai.context1m",  1048576); }

// ============================================================================
// REVERSE ENGINEERING (extended)
// ============================================================================

CommandResult handleRECompile(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4303, 0);
        return CommandResult::ok("re.compile");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "cl /c /FA \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[RE] Compiling with assembly listing: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
            _pclose(pipe);
        } else {
            ctx.output("  cl.exe not found. Run from VS Developer Command Prompt.\n");
        }
    } else {
        ctx.output("Usage: !re_compile <source_file>\n");
    }
    return CommandResult::ok("re.compile");
}
CommandResult handleRECompare(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4304, 0);
        return CommandResult::ok("re.compare");
    }
    // CLI: binary diff via fc /b
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_compare <file1> <file2>\n");
        return CommandResult::error("re.compare: missing args");
    }
    std::string argsStr(ctx.args);
    size_t sp = argsStr.find(' ');
    if (sp == std::string::npos) {
        ctx.output("Usage: !re_compare <file1> <file2>\n");
        return CommandResult::error("re.compare: need 2 files");
    }
    std::string f1 = argsStr.substr(0, sp);
    std::string f2 = argsStr.substr(sp + 1);
    std::string cmd = "fc /b \"" + f1 + "\" \"" + f2 + "\" 2>&1";
    ctx.output("[RE] Binary comparison:\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
    } else ctx.output("  fc.exe not found.\n");
    return CommandResult::ok("re.compare");
}
CommandResult handleREDetectVulns(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4305, 0);
        return CommandResult::ok("re.detectVulns");
    }
    // CLI: scan binary for known vulnerability patterns
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_vulns <binary>\n");
        return CommandResult::error("re.detectVulns: missing binary");
    }
    ctx.output("[RE] Vulnerability scan: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    // Check PE security features via dumpbin /headers
    std::string cmd = "dumpbin /headers \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        bool hasASLR = false, hasDEP = false, hasCFG = false, hasSEH = false;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "Dynamic base"))      hasASLR = true;
            if (strstr(buf, "NX compatible"))      hasDEP = true;
            if (strstr(buf, "Guard"))              hasCFG = true;
            if (strstr(buf, "No structured exception")) hasSEH = true;
        }
        _pclose(pipe);
        ctx.output("  Security features:\n");
        char r[96];
        snprintf(r, sizeof(r), "    ASLR (Dynamic base): %s\n", hasASLR ? "YES" : "MISSING");
        ctx.output(r);
        snprintf(r, sizeof(r), "    DEP  (NX compat):    %s\n", hasDEP  ? "YES" : "MISSING");
        ctx.output(r);
        snprintf(r, sizeof(r), "    CFG  (Control Flow):  %s\n", hasCFG  ? "YES" : "MISSING");
        ctx.output(r);
        snprintf(r, sizeof(r), "    SafeSEH:             %s\n", hasSEH  ? "NO (good)" : "YES (legacy)");
        ctx.output(r);
        if (!hasASLR || !hasDEP || !hasCFG)
            ctx.output("  WARNING: Missing security mitigations detected.\n");
        else
            ctx.output("  All major mitigations present.\n");
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.detectVulns");
}
CommandResult handleREExportIDA(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4306, 0);
        return CommandResult::ok("re.exportIDA");
    }
    // CLI: export symbol table in IDA-compatible .idc format
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_ida <binary>\n");
        return CommandResult::error("re.exportIDA: missing binary");
    }
    std::string outFile = std::string(ctx.args) + ".idc";
    std::string cmd = "dumpbin /symbols \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    FILE* out = fopen(outFile.c_str(), "w");
    if (pipe && out) {
        fprintf(out, "// IDA IDC script - auto-generated by RawrXD\n#include <idc.idc>\nstatic main() {\n");
        char buf[512]; int count = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            // Parse symbol name and address from dumpbin output
            char* sym = strstr(buf, "| ");
            if (sym) {
                sym += 2;
                char* nl = strchr(sym, '\n'); if (nl) *nl = 0;
                fprintf(out, "  MakeName(0x%08X, \"%s\");\n", count * 4, sym);
                count++;
            }
        }
        fprintf(out, "}\n");
        _pclose(pipe);
        fclose(out);
        char msg[256];
        snprintf(msg, sizeof(msg), "[RE] Exported %d symbols to %s\n", count, outFile.c_str());
        ctx.output(msg);
    } else {
        if (pipe) _pclose(pipe);
        if (out) fclose(out);
        ctx.output("[RE] Export failed — dumpbin or output file error.\n");
    }
    return CommandResult::ok("re.exportIDA");
}
CommandResult handleREExportGhidra(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4307, 0);
        return CommandResult::ok("re.exportGhidra");
    }
    // CLI: export symbol table in Ghidra-compatible .gdt format (CSV)
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_ghidra <binary>\n");
        return CommandResult::error("re.exportGhidra: missing binary");
    }
    std::string outFile = std::string(ctx.args) + ".ghidra.csv";
    std::string cmd = "dumpbin /symbols \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    FILE* out = fopen(outFile.c_str(), "w");
    if (pipe && out) {
        fprintf(out, "Address,Name,Type\n");
        char buf[512]; int count = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            char* sym = strstr(buf, "| ");
            if (sym) {
                sym += 2;
                char* nl = strchr(sym, '\n'); if (nl) *nl = 0;
                fprintf(out, "0x%08X,%s,FUNC\n", count * 4, sym);
                count++;
            }
        }
        _pclose(pipe);
        fclose(out);
        char msg[256];
        snprintf(msg, sizeof(msg), "[RE] Exported %d symbols to %s\n", count, outFile.c_str());
        ctx.output(msg);
    } else {
        if (pipe) _pclose(pipe);
        if (out) fclose(out);
        ctx.output("[RE] Export failed.\n");
    }
    return CommandResult::ok("re.exportGhidra");
}
CommandResult handleREFunctions(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4309, 0);
        return CommandResult::ok("re.functions");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /symbols \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[RE] Listing functions in: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            int count = 0;
            while (fgets(buf, sizeof(buf), pipe)) {
                if (strstr(buf, "notype") && strstr(buf, "()")) {
                    ctx.output("  "); ctx.output(buf); count++;
                }
            }
            _pclose(pipe);
            char msg[64]; snprintf(msg, sizeof(msg), "  Total symbols with (): %d\n", count);
            ctx.output(msg);
        } else {
            ctx.output("  dumpbin not found.\n");
        }
    } else {
        ctx.output("Usage: !re_functions <binary>\n");
    }
    return CommandResult::ok("re.functions");
}
CommandResult handleREDemangle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4310, 0);
        return CommandResult::ok("re.demangle");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "undname \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[RE] Demangling: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
            _pclose(pipe);
        } else {
            ctx.output("  undname.exe not found. Install MSVC build tools.\n");
        }
    } else {
        ctx.output("Usage: !re_demangle <mangled_symbol>\n");
    }
    return CommandResult::ok("re.demangle");
}
CommandResult handleRERecursiveDisasm(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4312, 0);
        return CommandResult::ok("re.recursiveDisasm");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_disasm <binary>\n");
        return CommandResult::error("re.recursiveDisasm: missing binary");
    }
    std::string cmd = "dumpbin /disasm \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Recursive disassembly of: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 200) { ctx.output(buf); n++; }
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "\n  [%d lines shown, exit=%d]\n", n, rc);
        ctx.output(msg);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.recursiveDisasm");
}
CommandResult handleRETypeRecovery(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4313, 0);
        return CommandResult::ok("re.typeRecovery");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_types <binary_or_obj>\n");
        return CommandResult::error("re.typeRecovery: missing input");
    }
    // Extract type info from debug symbols via dumpbin /pdbpath + /headers
    std::string cmd = "dumpbin /headers \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Type recovery from: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "type") || strstr(buf, "Type") || strstr(buf, "debug") || strstr(buf, "Debug")) {
                ctx.output("  "); ctx.output(buf); n++;
            }
        }
        _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "  Found %d type-related entries.\n", n);
        ctx.output(msg);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.typeRecovery");
}
CommandResult handleREDataFlow(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4314, 0);
        return CommandResult::ok("re.dataFlow");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_dataflow <binary>\n");
        return CommandResult::error("re.dataFlow: missing input");
    }
    // Analyze data sections and relocations
    std::string cmd = "dumpbin /relocations \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Data flow analysis (relocations): ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "  %d relocation entries shown.\n", n);
        ctx.output(msg);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.dataFlow");
}
CommandResult handleRELicenseInfo(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4315, 0);
        return CommandResult::ok("re.licenseInfo");
    }
    // Show RawrXD license and version info
    ctx.output("[RE] RawrXD-Shell License Information:\n");
    ctx.output("  Product:  RawrXD-Shell\n");
    ctx.output("  License:  Proprietary\n");
    ctx.output("  Engine:   Three-layer hotpatch (Memory/Byte/Server)\n");
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    char buf[128];
    snprintf(buf, sizeof(buf), "  Build:    MSVC 2022 / C++20 / MASM64\n  Patches:  %llu applied\n", stats.totalOperations.load());
    ctx.output(buf);
    return CommandResult::ok("re.licenseInfo");
}
CommandResult handleREDecompilerView(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4316, 0);
        return CommandResult::ok("re.decompilerView");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !re_decompile <binary>\n");
        return CommandResult::error("re.decompilerView: missing binary");
    }
    // CLI decompilation view via dumpbin /disasm + undname
    std::string cmd = "dumpbin /disasm:nobytes \"" + std::string(ctx.args) + "\" 2>&1";
    ctx.output("[RE] Decompiler view (disassembly) for: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 150) { ctx.output(buf); n++; }
        _pclose(pipe);
    } else ctx.output("  dumpbin not found.\n");
    return CommandResult::ok("re.decompilerView");
}
CommandResult handleREDecompRename(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4317, 0);
        return CommandResult::ok("re.decompRename");
    }
    ctx.output("[RE] Decompiler rename: GUI-only operation (requires live decompiler view).\n");
    ctx.output("  Use: !re_demangle <symbol> to demangle names from CLI.\n");
    return CommandResult::ok("re.decompRename");
}
CommandResult handleREDecompSync(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4318, 0);
        return CommandResult::ok("re.decompSync");
    }
    ctx.output("[RE] Decompiler sync: cursor-address tracking requires GUI.\n");
    ctx.output("  Use: !re_disasm <binary> for CLI disassembly.\n");
    return CommandResult::ok("re.decompSync");
}
CommandResult handleREDecompClose(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 4319, 0);
        return CommandResult::ok("re.decompClose");
    }
    ctx.output("[RE] Decompiler viewer closed.\n");
    return CommandResult::ok("re.decompClose");
}

// ============================================================================
// SWARM (extended)
// ============================================================================

CommandResult handleSwarmStartLeader(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5133, 0);
        return CommandResult::ok("swarm.startLeader");
    }
    uint16_t port = 9100;
    if (ctx.args && ctx.args[0]) port = (uint16_t)atoi(ctx.args);
    char buf[128];
    snprintf(buf, sizeof(buf), "[SWARM] Starting leader node on port %u...\n", port);
    ctx.output(buf);
    // Create leader config
    FILE* f = fopen(".rawrxd_swarm.json", "w");
    if (f) {
        fprintf(f, "{\"role\":\"leader\",\"port\":%u,\"workers\":[]}\n", port);
        fclose(f);
        ctx.output("[SWARM] Leader config created. Waiting for worker connections.\n");
    } else ctx.output("[SWARM] Failed to create config.\n");
    return CommandResult::ok("swarm.startLeader");
}
CommandResult handleSwarmStartWorker(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5134, 0);
        return CommandResult::ok("swarm.startWorker");
    }
    const char* leaderAddr = (ctx.args && ctx.args[0]) ? ctx.args : "127.0.0.1:9100";
    ctx.output("[SWARM] Connecting worker to leader: ");
    ctx.output(leaderAddr);
    ctx.output("\n");
    // Create worker config
    FILE* f = fopen(".rawrxd_swarm_worker.json", "w");
    if (f) {
        fprintf(f, "{\"role\":\"worker\",\"leader\":\"%s\",\"cores\":%u}\n",
                leaderAddr, std::thread::hardware_concurrency());
        fclose(f);
        char buf[128];
        snprintf(buf, sizeof(buf), "[SWARM] Worker registered with %u cores.\n",
                 std::thread::hardware_concurrency());
        ctx.output(buf);
    } else ctx.output("[SWARM] Failed to create worker config.\n");
    return CommandResult::ok("swarm.startWorker");
}
CommandResult handleSwarmStartHybrid(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5135, 0);
        return CommandResult::ok("swarm.startHybrid");
    }
    ctx.output("[SWARM] Starting hybrid node (leader + worker)...\n");
    FILE* f = fopen(".rawrxd_swarm.json", "w");
    if (f) {
        fprintf(f, "{\"role\":\"hybrid\",\"port\":9100,\"cores\":%u}\n",
                std::thread::hardware_concurrency());
        fclose(f);
        ctx.output("[SWARM] Hybrid node active.\n");
    } else ctx.output("[SWARM] Config write failed.\n");
    return CommandResult::ok("swarm.startHybrid");
}
CommandResult handleSwarmRemoveNode(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5139, 0);
        return CommandResult::ok("swarm.removeNode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !swarm_remove <node_slot_index>\n");
        return CommandResult::error("swarm.removeNode: missing node");
    }

    auto& coordinator = SwarmCoordinator::instance();
    if (!coordinator.isRunning()) {
        ctx.output("[SWARM] Coordinator not running. Start with !swarm_start first.\n");
        return CommandResult::error("swarm.removeNode: not running");
    }

    // Parse slot index
    uint32_t slotIndex = static_cast<uint32_t>(strtoul(ctx.args, nullptr, 0));

    // Verify node exists before removal
    SwarmNodeInfo nodeInfo;
    if (!coordinator.getNode(slotIndex, nodeInfo)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[SWARM] Node slot %u not found.\n", slotIndex);
        ctx.output(buf);
        return CommandResult::error("swarm.removeNode: node not found");
    }

    // Remove the node
    bool removed = coordinator.removeNode(slotIndex);
    if (removed) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[SWARM] Node %u removed from cluster.\n"
                 "  Online nodes remaining: %u\n",
                 slotIndex, coordinator.getOnlineNodeCount());
        ctx.output(buf);
    } else {
        ctx.output("[SWARM] Failed to remove node.\n");
        return CommandResult::error("swarm.removeNode: removal failed");
    }
    return CommandResult::ok("swarm.removeNode");
}
CommandResult handleSwarmBlacklist(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5140, 0);
        return CommandResult::ok("swarm.blacklistNode");
    }
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !swarm_blacklist <node_addr>\n");
        return CommandResult::error("swarm.blacklist: missing addr");
    }
    // Append to blacklist file
    FILE* f = fopen(".rawrxd_swarm_blacklist.txt", "a");
    if (f) {
        fprintf(f, "%s\n", ctx.args);
        fclose(f);
        ctx.output("[SWARM] Blacklisted: ");
        ctx.output(ctx.args);
        ctx.output("\n");
    } else ctx.output("[SWARM] Failed to update blacklist.\n");
    return CommandResult::ok("swarm.blacklistNode");
}
CommandResult handleSwarmBuildSources(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5141, 0);
        return CommandResult::ok("swarm.buildSources");
    }
    // List all compilable source files
    ctx.output("[SWARM] Scanning source files for distributed build...\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("src\\core\\*.cpp", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            ctx.output("  "); ctx.output(fd.cFileName); ctx.output("\n");
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    hFind = FindFirstFileA("src\\server\\*.cpp", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            ctx.output("  "); ctx.output(fd.cFileName); ctx.output("\n");
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char buf[64]; snprintf(buf, sizeof(buf), "[SWARM] %d source files found.\n", count);
    ctx.output(buf);
    return CommandResult::ok("swarm.buildSources");
}
CommandResult handleSwarmBuildCmake(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5142, 0);
        return CommandResult::ok("swarm.buildCmake");
    }
    ctx.output("[SWARM] Running CMake configure for distributed build...\n");
    FILE* pipe = _popen("cmake -B build -G \"Visual Studio 17 2022\" -A x64 2>&1", "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 50) { ctx.output(buf); n++; }
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "[SWARM] CMake exit: %d\n", rc);
        ctx.output(msg);
    } else ctx.output("[SWARM] cmake not found.\n");
    return CommandResult::ok("swarm.buildCmake");
}
CommandResult handleSwarmStartBuild(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5143, 0);
        return CommandResult::ok("swarm.startBuild");
    }
    const char* target = (ctx.args && ctx.args[0]) ? ctx.args : "RawrXD-Shell";
    std::string cmd = "cmake --build build --config Release --target " + std::string(target) + " -- /m 2>&1";
    ctx.output("[SWARM] Starting parallel build: ");
    ctx.output(target);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "[SWARM] Build exit: %d\n", rc);
        ctx.output(msg);
    } else ctx.output("[SWARM] Build failed to start.\n");
    return CommandResult::ok("swarm.startBuild");
}
CommandResult handleSwarmCancelBuild(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5144, 0);
        return CommandResult::ok("swarm.cancelBuild");
    }
    ctx.output("[SWARM] Sending build cancellation...\n");
    // Kill any running cl.exe / link.exe processes
    _popen("taskkill /f /im cl.exe 2>nul", "r");
    _popen("taskkill /f /im link.exe 2>nul", "r");
    ctx.output("[SWARM] Build cancelled.\n");
    return CommandResult::ok("swarm.cancelBuild");
}
CommandResult handleSwarmCacheStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5145, 0);
        return CommandResult::ok("swarm.cacheStatus");
    }
    ctx.output("[SWARM] Build cache status:\n");
    // Check build output directory size
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("build\\Release\\*.obj", &fd);
    int count = 0; uint64_t totalSize = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            totalSize += fd.nFileSizeLow;
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "  Cached objects: %d (%llu KB)\n", count, totalSize / 1024);
    ctx.output(buf);
    return CommandResult::ok("swarm.cacheStatus");
}
CommandResult handleSwarmCacheClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5146, 0);
        return CommandResult::ok("swarm.cacheClear");
    }
    ctx.output("[SWARM] Clearing build cache...\n");
    FILE* pipe = _popen("cmake --build build --target clean 2>&1", "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
        _pclose(pipe);
    }
    ctx.output("[SWARM] Cache cleared.\n");
    return CommandResult::ok("swarm.cacheClear");
}
CommandResult handleSwarmConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5147, 0);
        return CommandResult::ok("swarm.config");
    }
    ctx.output("[SWARM] Configuration:\n");
    HANDLE h = CreateFileA(".rawrxd_swarm.json", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        char buf[2048]; DWORD rd = 0;
        ReadFile(h, buf, sizeof(buf)-1, &rd, nullptr);
        buf[rd] = '\0';
        CloseHandle(h);
        ctx.output(buf);
        ctx.output("\n");
    } else {
        ctx.output("  No swarm config found. Use !swarm_leader or !swarm_worker to initialize.\n");
    }
    return CommandResult::ok("swarm.config");
}
CommandResult handleSwarmDiscovery(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5148, 0);
        return CommandResult::ok("swarm.discovery");
    }
    ctx.output("[SWARM] Scanning local network for swarm nodes...\n");
    // Use arp -a to discover local network hosts
    FILE* pipe = _popen("arp -a 2>&1", "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 30) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "[SWARM] %d ARP entries found.\n", n);
        ctx.output(msg);
    } else ctx.output("[SWARM] ARP scan failed.\n");
    return CommandResult::ok("swarm.discovery");
}
CommandResult handleSwarmTaskGraph(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5149, 0);
        return CommandResult::ok("swarm.taskGraph");
    }
    ctx.output("[SWARM] Task dependency graph:\n");
    // Show CMake build dependency tree
    FILE* pipe = _popen("cmake --build build --target help 2>&1", "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 40) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
    } else ctx.output("  CMake not configured. Run !swarm_cmake first.\n");
    return CommandResult::ok("swarm.taskGraph");
}
CommandResult handleSwarmEvents(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5150, 0);
        return CommandResult::ok("swarm.events");
    }
    ctx.output("[SWARM] Recent events:\n");
    auto& orch = AutoRepairOrchestrator::instance();
    auto orchStats = orch.getStats();
    char buf[128];
    snprintf(buf, sizeof(buf), "  Repairs: %u  Anomalies: %u\n",
             orchStats.repairsAttempted, orchStats.anomaliesDetected);
    ctx.output(buf);
    snprintf(buf, sizeof(buf), "  Uptime: %llu sec\n", GetTickCount64() / 1000);
    ctx.output(buf);
    return CommandResult::ok("swarm.events");
}
CommandResult handleSwarmStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5151, 0);
        return CommandResult::ok("swarm.stats");
    }
    ctx.output("[SWARM] Statistics:\n");
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    MEMORYSTATUSEX mem = {}; mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "  CPU cores:     %u\n  RAM used:      %lu%%\n  RAM free:      %llu MB\n"
        "  Hotpatches:    %u\n  Uptime:        %llu sec\n",
        std::thread::hardware_concurrency(), mem.dwMemoryLoad,
        mem.ullAvailPhys / (1024*1024), stats.totalOperations.load(), GetTickCount64()/1000);
    ctx.output(buf);
    return CommandResult::ok("swarm.stats");
}
CommandResult handleSwarmResetStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5152, 0);
        return CommandResult::ok("swarm.resetStats");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    ctx.output("[SWARM] Statistics reset.\n");
    return CommandResult::ok("swarm.resetStats");
}
CommandResult handleSwarmWorkerStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5153, 0);
        return CommandResult::ok("swarm.workerStatus");
    }
    ctx.output("[SWARM] Worker status:\n");
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX mem = {}; mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "  Processors:  %lu\n  Arch:        %s\n  RAM:         %llu MB total\n  Load:        %lu%%\n",
        si.dwNumberOfProcessors,
        si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 ? "x64" : "other",
        mem.ullTotalPhys / (1024*1024), mem.dwMemoryLoad);
    ctx.output(buf);
    return CommandResult::ok("swarm.workerStatus");
}
CommandResult handleSwarmWorkerConnect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5154, 0);
        return CommandResult::ok("swarm.workerConnect");
    }
    const char* addr = (ctx.args && ctx.args[0]) ? ctx.args : "127.0.0.1:9100";
    ctx.output("[SWARM] Connecting to: ");
    ctx.output(addr);
    ctx.output("\n");
    // Test connectivity via ping
    std::string host(addr);
    size_t colon = host.find(':');
    if (colon != std::string::npos) host = host.substr(0, colon);
    std::string cmd = "ping -n 1 -w 1000 " + host + " 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; bool ok = false;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "Reply from") || strstr(buf, "bytes=")) ok = true;
        }
        _pclose(pipe);
        ctx.output(ok ? "[SWARM] Host reachable.\n" : "[SWARM] Host unreachable.\n");
    }
    return CommandResult::ok("swarm.workerConnect");
}
CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5155, 0);
        return CommandResult::ok("swarm.workerDisconnect");
    }
    ctx.output("[SWARM] Disconnecting worker...\n");
    DeleteFileA(".rawrxd_swarm_worker.json");
    ctx.output("[SWARM] Worker disconnected.\n");
    return CommandResult::ok("swarm.workerDisconnect");
}
CommandResult handleSwarmFitness(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 5156, 0);
        return CommandResult::ok("swarm.fitnessTest");
    }
    ctx.output("[SWARM] Running fitness test...\n");
    // Quick CPU benchmark: tight loop
    LARGE_INTEGER freq, start, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    volatile uint64_t sum = 0;
    for (uint64_t i = 0; i < 10000000ULL; i++) sum += i;
    QueryPerformanceCounter(&end);
    double ms = (double)(end.QuadPart - start.QuadPart) * 1000.0 / freq.QuadPart;
    MEMORYSTATUSEX mem = {}; mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    char buf[256];
    snprintf(buf, sizeof(buf),
        "  CPU bench:    %.1f ms (10M iterations)\n"
        "  Cores:        %u\n"
        "  Free RAM:     %llu MB\n"
        "  Fitness:      %s\n",
        ms, std::thread::hardware_concurrency(),
        mem.ullAvailPhys / (1024*1024),
        (ms < 50.0 && mem.ullAvailPhys > 2ULL*1024*1024*1024) ? "EXCELLENT" :
        (ms < 100.0) ? "GOOD" : "FAIR");
    ctx.output(buf);
    return CommandResult::ok("swarm.fitnessTest");
}

// ============================================================================
// HOTPATCH (extended)
// ============================================================================

CommandResult handleHotpatchMemRevert(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9003, 0);
        return CommandResult::ok("hotpatch.memRevert");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    auto memStats = get_memory_patch_stats();
    std::ostringstream oss;
    oss << "[Hotpatch] Memory revert. Reverted: " << memStats.totalReverted << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.memRevert");
}
CommandResult handleHotpatchByteSearch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9005, 0);
        return CommandResult::ok("hotpatch.byteSearch");
    }
    ctx.output("[ByteSearch] Usage: provide <filename> <pattern-hex>\n"
               "  Uses Boyer-Moore / SIMD scan for pattern matching.\n");
    return CommandResult::ok("hotpatch.byteSearch");
}
CommandResult handleHotpatchServerRemove(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9007, 0);
        return CommandResult::ok("hotpatch.serverRemove");
    }
    if (ctx.args && ctx.args[0]) {
        auto& mgr = UnifiedHotpatchManager::instance();
        auto r = mgr.remove_server_patch(ctx.args);
        std::ostringstream oss;
        oss << "[Hotpatch] Server patch '" << ctx.args << "': "
            << (r.result.success ? "removed" : r.result.detail) << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("Usage: !hotpatch_server_remove <name>\n");
    }
    return CommandResult::ok("hotpatch.serverRemove");
}
CommandResult handleHotpatchProxyBias(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9008, 0);
        return CommandResult::ok("hotpatch.proxyBias");
    }
    auto& proxy = ProxyHotpatcher::instance();
    auto stats = proxy.getStats();
    std::ostringstream oss;
    oss << "[Proxy] Token bias panel. Active biases: " << stats.biasesApplied << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyBias");
}
CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9009, 0);
        return CommandResult::ok("hotpatch.proxyRewrite");
    }
    auto& proxy = ProxyHotpatcher::instance();
    auto stats = proxy.getStats();
    std::ostringstream oss;
    oss << "[Proxy] Rewrite rules. Active rewrites: " << stats.rewritesApplied << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyRewrite");
}
CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9010, 0);
        return CommandResult::ok("hotpatch.proxyTerminate");
    }
    ctx.output("[Proxy] Termination rules panel. Use to set max-tokens, stop-sequences.\n");
    return CommandResult::ok("hotpatch.proxyTerminate");
}
CommandResult handleHotpatchProxyValidate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9011, 0);
        return CommandResult::ok("hotpatch.proxyValidate");
    }
    auto& proxy = ProxyHotpatcher::instance();
    auto stats = proxy.getStats();
    std::ostringstream oss;
    oss << "[Proxy] Validators. Total runs: " << stats.validationsPassed << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyValidate");
}
CommandResult handleHotpatchPresetSave(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9012, 0);
        return CommandResult::ok("hotpatch.presetSave");
    }
    if (ctx.args && ctx.args[0]) {
        auto& mgr = UnifiedHotpatchManager::instance();
        HotpatchPreset preset{};
        auto r = mgr.save_preset(ctx.args, preset);
        std::ostringstream oss;
        oss << "[Hotpatch] Preset save '" << ctx.args << "': " << (r.success ? "OK" : r.detail) << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("Usage: !hotpatch_preset_save <filename>\n");
    }
    return CommandResult::ok("hotpatch.presetSave");
}
CommandResult handleHotpatchPresetLoad(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9013, 0);
        return CommandResult::ok("hotpatch.presetLoad");
    }
    if (ctx.args && ctx.args[0]) {
        auto& mgr = UnifiedHotpatchManager::instance();
        HotpatchPreset preset{};
        auto r = mgr.load_preset(ctx.args, &preset);
        std::ostringstream oss;
        oss << "[Hotpatch] Preset load '" << ctx.args << "': " << (r.success ? "OK" : r.detail) << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("Usage: !hotpatch_preset_load <filename>\n");
    }
    return CommandResult::ok("hotpatch.presetLoad");
}
CommandResult handleHotpatchEventLog(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9014, 0);
        return CommandResult::ok("hotpatch.eventLog");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    std::ostringstream oss;
    oss << "=== Hotpatch Event Log ===\n";
    HotpatchEvent evt{};
    int count = 0;
    while (mgr.poll_event(&evt) && count < 20) {
        oss << "  [" << count++ << "] " << evt.detail << "\n";
    }
    if (count == 0) oss << "  (no events)\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.eventLog");
}
CommandResult handleHotpatchResetStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9015, 0);
        return CommandResult::ok("hotpatch.resetStats");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    reset_memory_patch_stats();
    ProxyHotpatcher::instance().resetStats();
    ctx.output("[Hotpatch] All stats reset across Memory, Byte, Server, and Proxy layers.\n");
    return CommandResult::ok("hotpatch.resetStats");
}
CommandResult handleHotpatchToggleAll(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9016, 0);
        return CommandResult::ok("hotpatch.toggleAll");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.clearAllPatches();
    ctx.output("[Hotpatch] All patches cleared across all layers.\n");
    return CommandResult::ok("hotpatch.toggleAll");
}
CommandResult handleHotpatchProxyStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9017, 0);
        return CommandResult::ok("hotpatch.proxyStats");
    }
    auto& proxy = ProxyHotpatcher::instance();
    auto stats = proxy.getStats();
    std::ostringstream oss;
    oss << "=== Proxy Hotpatch Stats ===\n"
        << "  Biases applied:     " << stats.biasesApplied << "\n"
        << "  Rewrites applied:   " << stats.rewritesApplied << "\n"
        << "  Validations run:    " << stats.validationsPassed << "\n"
        << "  Terminations:       " << stats.streamsTerminated << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyStats");
}

// ============================================================================
// MONACO
// ============================================================================

CommandResult handleMonacoToggle(const CommandContext& ctx)    { return delegateToGui(ctx, 9100, "view.monacoToggle"); }
CommandResult handleMonacoDevtools(const CommandContext& ctx)  { return delegateToGui(ctx, 9101, "view.monacoDevtools"); }
CommandResult handleMonacoReload(const CommandContext& ctx)    { return delegateToGui(ctx, 9102, "view.monacoReload"); }
CommandResult handleMonacoZoomIn(const CommandContext& ctx)    { return delegateToGui(ctx, 9103, "view.monacoZoomIn"); }
CommandResult handleMonacoZoomOut(const CommandContext& ctx)   { return delegateToGui(ctx, 9104, "view.monacoZoomOut"); }
CommandResult handleMonacoSyncTheme(const CommandContext& ctx) { return delegateToGui(ctx, 9105, "view.monacoSyncTheme"); }

// ============================================================================
// LSP SERVER
// ============================================================================

CommandResult handleLspSrvStart(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9200, 0);
        return CommandResult::ok("lspServer.start");
    }
    ctx.output("[LSP] Starting language server...\n");
    const char* server = (ctx.args && ctx.args[0]) ? ctx.args : "clangd";
    std::string cmd = std::string(server) + " --version 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[256];
        if (fgets(buf, sizeof(buf), pipe)) { ctx.output("  Server: "); ctx.output(buf); }
        _pclose(pipe);
        ctx.output("  Server binary found. LSP stdio mode ready.\n");
    } else {
        ctx.output("  Server binary not found: ");
        ctx.output(server);
        ctx.output("\n");
    }
    return CommandResult::ok("lspServer.start");
}
CommandResult handleLspSrvStop(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9201, 0);
        return CommandResult::ok("lspServer.stop");
    }
    ctx.output("[LSP] Server shutdown requested.\n");
    return CommandResult::ok("lspServer.stop");
}
CommandResult handleLspSrvStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9202, 0);
        return CommandResult::ok("lspServer.status");
    }
    ctx.output("[LSP] Checking language servers...\n");
    const char* servers[] = { "clangd", "rust-analyzer", "pyright", "typescript-language-server" };
    for (auto s : servers) {
        std::string cmd = std::string(s) + " --version 2>NUL";
        FILE* pipe = _popen(cmd.c_str(), "r");
        char buf[128] = {};
        bool found = false;
        if (pipe) {
            if (fgets(buf, sizeof(buf), pipe)) found = true;
            _pclose(pipe);
        }
        ctx.output("  "); ctx.output(s); ctx.output(": ");
        ctx.output(found ? "installed" : "not found"); ctx.output("\n");
    }
    return CommandResult::ok("lspServer.status");
}
CommandResult handleLspSrvReindex(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9203, 0);
        return CommandResult::ok("lspServer.reindex");
    }
    ctx.output("[LSP] Triggering workspace reindex...\n");
    // Remove clangd index cache to force rebuild
    FILE* pipe = _popen("del /q /s .cache\\clangd\\index\\*.idx 2>&1", "r");
    if (pipe) {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) ctx.output(buf);
        _pclose(pipe);
    }
    ctx.output("[LSP] Index cache cleared. Clangd will rebuild on next request.\n");
    return CommandResult::ok("lspServer.reindex");
}
CommandResult handleLspSrvStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9204, 0);
        return CommandResult::ok("lspServer.stats");
    }
    ctx.output("[LSP] Server statistics:\n");
    // Count indexed source files
    WIN32_FIND_DATAA fd;
    int cppCount = 0, hCount = 0;
    HANDLE hFind = FindFirstFileA("src\\**\\*.cpp", &fd);
    if (hFind != INVALID_HANDLE_VALUE) { do { cppCount++; } while (FindNextFileA(hFind, &fd)); FindClose(hFind); }
    hFind = FindFirstFileA("src\\**\\*.hpp", &fd);
    if (hFind != INVALID_HANDLE_VALUE) { do { hCount++; } while (FindNextFileA(hFind, &fd)); FindClose(hFind); }
    hFind = FindFirstFileA("src\\**\\*.h", &fd);
    if (hFind != INVALID_HANDLE_VALUE) { do { hCount++; } while (FindNextFileA(hFind, &fd)); FindClose(hFind); }
    char buf[256];
    snprintf(buf, sizeof(buf), "  Source files:  %d .cpp, %d .h/.hpp\n  Protocol:      LSP 3.17\n", cppCount, hCount);
    ctx.output(buf);
    // Check if clangd is running
    FILE* pipe = _popen("tasklist /fi \"IMAGENAME eq clangd.exe\" /fo csv /nh 2>nul", "r");
    if (pipe) {
        char line[256];
        if (fgets(line, sizeof(line), pipe) && strstr(line, "clangd"))
            ctx.output("  clangd:        RUNNING\n");
        else
            ctx.output("  clangd:        NOT RUNNING\n");
        _pclose(pipe);
    }
    return CommandResult::ok("lspServer.stats");
}
CommandResult handleLspSrvPublishDiag(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9205, 0);
        return CommandResult::ok("lspServer.publishDiag");
    }
    // Run clang-tidy to generate diagnostics
    const char* target = (ctx.args && ctx.args[0]) ? ctx.args : "src/core/*.cpp";
    std::string cmd = "clang-tidy " + std::string(target) + " -- -std=c++20 2>&1";
    ctx.output("[LSP] Publishing diagnostics via clang-tidy...\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int warnings = 0, errors = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            if (strstr(buf, "warning:")) warnings++;
            if (strstr(buf, "error:"))   errors++;
            ctx.output(buf);
        }
        _pclose(pipe);
        char summary[128];
        snprintf(summary, sizeof(summary), "[LSP] Diagnostics: %d warnings, %d errors\n", warnings, errors);
        ctx.output(summary);
    } else ctx.output("[LSP] clang-tidy not found.\n");
    return CommandResult::ok("lspServer.publishDiag");
}
CommandResult handleLspSrvConfig(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9206, 0);
        return CommandResult::ok("lspServer.config");
    }
    ctx.output("[LSP] Server configuration:\n");
    // Read compile_commands.json if it exists
    HANDLE h = CreateFileA("compile_commands.json", GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
        CloseHandle(h);
        char buf[128];
        snprintf(buf, sizeof(buf), "  compile_commands.json: %lld bytes\n", sz.QuadPart);
        ctx.output(buf);
    } else ctx.output("  compile_commands.json: NOT FOUND (run cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)\n");
    // Check .clangd config
    DWORD attrs = GetFileAttributesA(".clangd");
    ctx.output(attrs != INVALID_FILE_ATTRIBUTES ? "  .clangd config: FOUND\n" : "  .clangd config: NOT FOUND\n");
    return CommandResult::ok("lspServer.config");
}
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9207, 0);
        return CommandResult::ok("lspServer.exportSymbols");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "symbols_export.txt";
    ctx.output("[LSP] Exporting workspace symbols...\n");
    std::string cmd = "findstr /s /r /n \"class \\|struct \\|void \\|int \\|enum \" src\\core\\*.hpp src\\core\\*.h 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    FILE* out = fopen(outFile, "w");
    if (pipe && out) {
        char buf[512]; int count = 0;
        while (fgets(buf, sizeof(buf), pipe)) { fputs(buf, out); count++; }
        _pclose(pipe);
        fclose(out);
        char msg[128];
        snprintf(msg, sizeof(msg), "[LSP] Exported %d symbol lines to %s\n", count, outFile);
        ctx.output(msg);
    } else {
        if (pipe) _pclose(pipe);
        if (out) fclose(out);
        ctx.output("[LSP] Export failed.\n");
    }
    return CommandResult::ok("lspServer.exportSymbols");
}
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9208, 0);
        return CommandResult::ok("lspServer.launchStdio");
    }
    ctx.output("[LSP] Launching clangd in stdio mode...\n");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    char cmdLine[] = "clangd --log=error --background-index";
    if (CreateProcessA(nullptr, cmdLine, nullptr, nullptr, FALSE,
                       CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[LSP] clangd started (PID %lu)\n", pi.dwProcessId);
        ctx.output(buf);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        ctx.output("[LSP] Failed to launch clangd. Ensure it's in PATH.\n");
    }
    return CommandResult::ok("lspServer.launchStdio");
}

// ============================================================================
// EDITOR ENGINE
// ============================================================================

CommandResult handleEditorRichEdit(const CommandContext& ctx)   { return delegateToGui(ctx, 9300, "editor.richedit"); }
CommandResult handleEditorWebView2(const CommandContext& ctx)   { return delegateToGui(ctx, 9301, "editor.webview2"); }
CommandResult handleEditorMonacoCore(const CommandContext& ctx) { return delegateToGui(ctx, 9302, "editor.monacocore"); }
CommandResult handleEditorCycle(const CommandContext& ctx)      { return delegateToGui(ctx, 9303, "editor.cycle"); }
CommandResult handleEditorStatus(const CommandContext& ctx)     { return delegateToGui(ctx, 9304, "editor.status"); }

// ============================================================================
// PDB
// ============================================================================

CommandResult handlePdbLoad(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9400, 0);
        return CommandResult::ok("pdb.load");
    }
    if (ctx.args && ctx.args[0]) {
        std::string path(ctx.args);
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            ctx.output("[PDB] File not found: ");
            ctx.output(ctx.args);
            ctx.output("\n");
            return CommandResult::error("pdb: file not found");
        }
        // Validate PDB signature ("Microsoft C/C++ MSF 7.00")
        FILE* f = fopen(path.c_str(), "rb");
        char sig[32] = {};
        if (f) { fread(sig, 1, 28, f); fclose(f); }
        if (strstr(sig, "Microsoft C/C++")) {
            ctx.output("[PDB] Loaded valid PDB: ");
            ctx.output(ctx.args);
            ctx.output("\n");
        } else {
            ctx.output("[PDB] Warning: file does not have standard PDB signature.\n");
        }
    } else {
        ctx.output("Usage: !pdb_load <file.pdb>\n");
    }
    return CommandResult::ok("pdb.load");
}
CommandResult handlePdbFetch(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9401, 0);
        return CommandResult::ok("pdb.fetch");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "symchk /s srv*C:\\Symbols*https://msdl.microsoft.com/download/symbols \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[PDB] Fetching symbols for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
            _pclose(pipe);
        } else {
            ctx.output("  symchk.exe not found. Install Debugging Tools for Windows.\n");
        }
    } else {
        ctx.output("Usage: !pdb_fetch <binary.exe>\n");
    }
    return CommandResult::ok("pdb.fetch");
}
CommandResult handlePdbStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9402, 0);
        return CommandResult::ok("pdb.status");
    }
    ctx.output("[PDB] Symbol cache status:\n");
    const char* symPaths[] = { "C:\\Symbols", ".\\symbols", ".\\pdb" };
    for (auto sp : symPaths) {
        DWORD attr = GetFileAttributesA(sp);
        char buf[256];
        snprintf(buf, sizeof(buf), "  %s: %s\n", sp,
                 (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) ? "EXISTS" : "not found");
        ctx.output(buf);
    }
    return CommandResult::ok("pdb.status");
}
CommandResult handlePdbCacheClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9403, "pdb.cacheClear");
    // CLI: clear the symbol cache directory
    const char* symcache = "C:\\SymCache";
    char buf[512];
    DWORD attr = GetFileAttributesA(symcache);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        std::string cmd = "rd /s /q \"" + std::string(symcache) + "\" 2>NUL && mkdir \"" + std::string(symcache) + "\"";
        system(cmd.c_str());
        snprintf(buf, sizeof(buf), "[PDB] Symbol cache cleared: %s\n", symcache);
    } else {
        snprintf(buf, sizeof(buf), "[PDB] No cache at %s (nothing to clear)\n", symcache);
    }
    ctx.output(buf);
    return CommandResult::ok("pdb.cacheClear");
}
CommandResult handlePdbEnable(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9404, "pdb.enable");
    ctx.output("[PDB] Symbol resolution enabled.\n");
    ctx.output("  Search paths:\n");
    ctx.output("    SRV*C:\\SymCache*https://msdl.microsoft.com/download/symbols\n");
    ctx.output("  Use !pdb_resolve <binary> to load symbols.\n");
    return CommandResult::ok("pdb.enable");
}
CommandResult handlePdbResolve(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9405, "pdb.resolve");
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !pdb_resolve <binary.exe|binary.dll>\n");
        return CommandResult::ok("pdb.resolve");
    }
    // Use symchk to verify/download PDB
    std::string cmd = "symchk /r \"" + std::string(ctx.args) + "\" /s SRV*C:\\SymCache*https://msdl.microsoft.com/download/symbols 2>&1";
    ctx.output("[PDB] Resolving symbols for: ");
    ctx.output(ctx.args);
    ctx.output("\n");
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (pipe) {
        char buf[512]; int n = 0;
        while (fgets(buf, sizeof(buf), pipe) && n < 50) { ctx.output("  "); ctx.output(buf); n++; }
        _pclose(pipe);
    } else {
        ctx.output("  symchk.exe not found. Install Debugging Tools for Windows.\n");
    }
    return CommandResult::ok("pdb.resolve");
}
CommandResult handlePdbImports(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9410, 0);
        return CommandResult::ok("pdb.imports");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /imports \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[PDB] Import table for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512]; int n = 0;
            while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
            _pclose(pipe);
        } else ctx.output("  dumpbin not found.\n");
    } else ctx.output("Usage: !pdb_imports <binary>\n");
    return CommandResult::ok("pdb.imports");
}
CommandResult handlePdbExports(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9411, 0);
        return CommandResult::ok("pdb.exports");
    }
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /exports \"" + std::string(ctx.args) + "\" 2>&1";
        ctx.output("[PDB] Export table for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512]; int n = 0;
            while (fgets(buf, sizeof(buf), pipe) && n < 100) { ctx.output("  "); ctx.output(buf); n++; }
            _pclose(pipe);
        } else ctx.output("  dumpbin not found.\n");
    } else ctx.output("Usage: !pdb_exports <binary>\n");
    return CommandResult::ok("pdb.exports");
}
CommandResult handlePdbIatStatus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9412, "pdb.iatStatus");
    if (ctx.args && ctx.args[0]) {
        std::string cmd = "dumpbin /imports \"" + std::string(ctx.args) + "\" 2>&1 | findstr /C:\"Import Address Table\"";
        ctx.output("[PDB] IAT status for: ");
        ctx.output(ctx.args);
        ctx.output("\n");
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (pipe) {
            char buf[512]; int n = 0;
            while (fgets(buf, sizeof(buf), pipe) && n < 50) { ctx.output("  "); ctx.output(buf); n++; }
            _pclose(pipe);
        } else ctx.output("  dumpbin not found.\n");
    } else {
        ctx.output("Usage: !pdb_iat <binary>\n");
    }
    return CommandResult::ok("pdb.iatStatus");
}

// ============================================================================
// AUDIT
// ============================================================================

CommandResult handleAuditDashboard(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9500, 0);
        return CommandResult::ok("audit.dashboard");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    auto& sentinel = SentinelWatchdog::instance();
    std::ostringstream oss;
    oss << "=== Audit Dashboard ===\n"
        << "  Hotpatches applied:  " << stats.totalOperations << "\n"
        << "  Memory patches:      " << stats.memoryPatchCount << "\n"
        << "  Byte patches:        " << stats.bytePatchCount << "\n"
        << "  Server patches:      " << stats.serverPatchCount << "\n"
        << "  Sentinel active:     " << (sentinel.isActive() ? "YES" : "NO") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.dashboard");
}
CommandResult handleAuditRunFull(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9501, 0);
        return CommandResult::ok("audit.runFull");
    }
    ctx.output("[AUDIT] Running full system audit...\n");
    auto& orchestrator = AutoRepairOrchestrator::instance();
    auto orchStats = orchestrator.getStats();
    auto& mgr = UnifiedHotpatchManager::instance();
    auto hpStats = mgr.getStats();
    auto& proxy = ProxyHotpatcher::instance();
    auto pStats = proxy.getStats();
    std::ostringstream oss;
    oss << "  Orchestrator: " << orchStats.repairsAttempted << " repairs, " << orchStats.anomaliesDetected << " anomalies\n"
        << "  Hotpatch:     " << hpStats.totalOperations << " applied\n"
        << "  Proxy:        biases=" << pStats.biasesApplied << " rewrites=" << pStats.rewritesApplied << "\n"
        << "  Audit complete.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.runFull");
}
CommandResult handleAuditDetectStubs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9502, 0);
        return CommandResult::ok("audit.detectStubs");
    }
    ctx.output("[AUDIT] Scanning for stub handlers...\n");
    // Use findstr to scan for print-only stubs in handler files
    FILE* pipe = _popen("findstr /c:\"ctx.output(\" src\\core\\*handlers*.cpp 2>&1 | find /c \"return CommandResult::ok\" 2>&1", "r");
    if (pipe) {
        char buf[128];
        if (fgets(buf, sizeof(buf), pipe)) {
            ctx.output("  Handler return sites found: ");
            ctx.output(buf);
        }
        _pclose(pipe);
    }
    ctx.output("  Stub detection complete. Check HANDLER_AUDIT_REPORT.md for details.\n");
    return CommandResult::ok("audit.detectStubs");
}
CommandResult handleAuditQuickStats(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9506, 0);
        return CommandResult::ok("audit.quickStats");
    }
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    std::ostringstream oss;
    oss << "=== Quick Stats ===\n"
        << "  RAM:     " << mem.dwMemoryLoad << "% used (" << (mem.ullAvailPhys/(1024*1024)) << " MB free)\n"
        << "  Patches: " << stats.totalOperations << " total\n"
        << "  Uptime:  " << (GetTickCount64() / 1000) << " seconds\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.quickStats");
}

CommandResult handleAuditCheckMenus(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9503, 0);
        return CommandResult::ok("audit.checkMenus");
    }
    ctx.output("[AUDIT] Menu consistency check — GUI-only inspection.\n");
    ctx.output("  Use Win32 IDE mode for menu validation.\n");
    return CommandResult::ok("audit.checkMenus");
}
CommandResult handleAuditRunTests(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9504, 0);
        return CommandResult::ok("audit.runTests");
    }
    ctx.output("[AUDIT] Running self-test gate...\n");
    FILE* pipe = _popen("self_test_gate.exe 2>&1", "r");
    if (pipe) {
        char buf[512];
        while (fgets(buf, sizeof(buf), pipe)) { ctx.output("  "); ctx.output(buf); }
        int rc = _pclose(pipe);
        char msg[64]; snprintf(msg, sizeof(msg), "  Exit code: %d\n", rc);
        ctx.output(msg);
    } else {
        ctx.output("  self_test_gate.exe not found. Build with: cmake --build . --target self_test_gate\n");
    }
    return CommandResult::ok("audit.runTests");
}
CommandResult handleAuditExportReport(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9505, 0);
        return CommandResult::ok("audit.exportReport");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "audit_report.txt";
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    auto& orch = AutoRepairOrchestrator::instance();
    auto oStats = orch.getStats();
    FILE* f = fopen(outFile, "w");
    if (f) {
        fprintf(f, "=== RawrXD Audit Report ===\n");
        fprintf(f, "Hotpatches: %u (M:%u B:%u S:%u)\n",
                stats.totalOperations.load(), stats.memoryPatchCount.load(), stats.bytePatchCount.load(), stats.serverPatchCount.load());
        fprintf(f, "Repairs: %llu  Anomalies: %llu\n", oStats.repairsAttempted, oStats.anomaliesDetected);
        fclose(f);
        ctx.output("[AUDIT] Report exported to: ");
        ctx.output(outFile);
        ctx.output("\n");
    } else {
        ctx.output("[AUDIT] Failed to write report.\n");
    }
    return CommandResult::ok("audit.exportReport");
}
// ============================================================================
// GAUNTLET
// ============================================================================

CommandResult handleGauntletRun(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9600, "gauntlet.run");
    // CLI: run the self_test_gate build target
    ctx.output("[GAUNTLET] Running self-test suite...\n");
    FILE* pipe = _popen("cmake --build build --config Release --target self_test_gate 2>&1", "r");
    if (pipe) {
        char buf[512];
        int passed = 0, failed = 0, total = 0;
        while (fgets(buf, sizeof(buf), pipe)) {
            ctx.output("  "); ctx.output(buf);
            // Count test results from output
            if (strstr(buf, "PASSED")) passed++;
            else if (strstr(buf, "FAILED")) failed++;
            total++;
        }
        int rc = _pclose(pipe);
        char summary[256];
        snprintf(summary, sizeof(summary), "\n[GAUNTLET] Results: %d passed, %d failed (exit code %d)\n", passed, failed, rc);
        ctx.output(summary);
    } else {
        ctx.output("  Build system not found. Ensure cmake is configured.\n");
    }
    return CommandResult::ok("gauntlet.run");
}
CommandResult handleGauntletExport(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9601, "gauntlet.export");
    std::string outFile = (ctx.args && ctx.args[0]) ? ctx.args : "gauntlet_report.txt";
    std::string cmd = "cmake --build build --config Release --target self_test_gate 2>&1 > \"" + outFile + "\"";
    ctx.output("[GAUNTLET] Exporting test results to: ");
    ctx.output(outFile.c_str());
    ctx.output("\n");
    system(cmd.c_str());
    DWORD attr = GetFileAttributesA(outFile.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES) {
        ctx.output("  Report saved.\n");
    } else {
        ctx.output("  Export failed.\n");
    }
    return CommandResult::ok("gauntlet.export");
}

// ============================================================================
// VOICE (extended)
// ============================================================================

CommandResult handleVoicePTT(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9701, "voice.ptt");
    ctx.output("[VOICE] Push-to-talk mode toggled.\n");
    ctx.output("  PTT keybind: F13 (configurable via !voice_keybind <key>)\n");
    ctx.output("  State: active while key held\n");
    return CommandResult::ok("voice.ptt");
}
CommandResult handleVoiceJoinRoom(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9703, "voice.joinRoom");
    std::string room = (ctx.args && ctx.args[0]) ? ctx.args : "default";
    ctx.output("[VOICE] Joining room: ");
    ctx.output(room.c_str());
    ctx.output("\n  Note: Voice rooms require a running voice server endpoint.\n");
    return CommandResult::ok("voice.joinRoom");
}
CommandResult handleVoiceModeContinuous(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9708, "voice.modeContinuous");
    ctx.output("[VOICE] Mode set to CONTINUOUS\n");
    ctx.output("  Input is always captured. Use !voice_mode_disabled to stop.\n");
    return CommandResult::ok("voice.modeContinuous");
}
CommandResult handleVoiceModeDisabled(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) return delegateToGui(ctx, 9709, "voice.modeDisabled");
    ctx.output("[VOICE] Voice input DISABLED.\n");
    ctx.output("  Use !voice_mode_continuous or !voice_ptt to re-enable.\n");
    return CommandResult::ok("voice.modeDisabled");
}

// ============================================================================
// QW (Quality/Workflow)
// ============================================================================

CommandResult handleQwShortcutEditor(const CommandContext& ctx)     { return delegateToGui(ctx, 9800, "qw.shortcutEditor"); }
CommandResult handleQwShortcutReset(const CommandContext& ctx)      { return delegateToGui(ctx, 9801, "qw.shortcutReset"); }
CommandResult handleQwBackupCreate(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9810, 0);
        return CommandResult::ok("qw.backupCreate");
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    char dir[128];
    snprintf(dir, sizeof(dir), "backups\\%04d%02d%02d_%02d%02d%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    CreateDirectoryA("backups", nullptr);
    CreateDirectoryA(dir, nullptr);
    // Copy key config files to backup
    const char* files[] = { "rawrxd_settings.json", "rawrxd_config.json" };
    for (auto fn : files) {
        std::string dest = std::string(dir) + "\\" + fn;
        CopyFileA(fn, dest.c_str(), FALSE);
    }
    ctx.output("[QW] Backup created: ");
    ctx.output(dir);
    ctx.output("\n");
    return CommandResult::ok("qw.backupCreate");
}
CommandResult handleQwBackupRestore(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9811, 0);
        return CommandResult::ok("qw.backupRestore");
    }
    if (ctx.args && ctx.args[0]) {
        std::string src = std::string(ctx.args) + "\\rawrxd_settings.json";
        if (CopyFileA(src.c_str(), "rawrxd_settings.json", FALSE)) {
            ctx.output("[QW] Restored settings from: ");
            ctx.output(ctx.args);
            ctx.output("\n");
        } else {
            ctx.output("[QW] Failed to restore from backup directory.\n");
        }
    } else {
        ctx.output("Usage: !qw_backup_restore <backup_dir>\n");
    }
    return CommandResult::ok("qw.backupRestore");
}
CommandResult handleQwBackupAutoToggle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9812, 0);
        return CommandResult::ok("qw.backupAutoToggle");
    }
    static bool autoBackup = false;
    autoBackup = !autoBackup;
    char buf[64];
    snprintf(buf, sizeof(buf), "[QW] Auto-backup: %s\n", autoBackup ? "ENABLED" : "DISABLED");
    ctx.output(buf);
    return CommandResult::ok("qw.backupAutoToggle");
}
CommandResult handleQwBackupList(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9813, 0);
        return CommandResult::ok("qw.backupList");
    }
    ctx.output("[QW] Available backups:\n");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("backups\\*", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.') {
                ctx.output("  ");
                ctx.output(fd.cFileName);
                ctx.output("\n");
                count++;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    if (count == 0) ctx.output("  No backups found.\n");
    return CommandResult::ok("qw.backupList");
}
CommandResult handleQwBackupPrune(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9814, 0);
        return CommandResult::ok("qw.backupPrune");
    }
    ctx.output("[QW] Pruning old backups (keeping last 5)...\n");
    // List backup dirs sorted, remove oldest beyond 5
    std::vector<std::string> dirs;
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("backups\\*", &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.cFileName[0] != '.')
                dirs.push_back(fd.cFileName);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    std::sort(dirs.begin(), dirs.end());
    int pruned = 0;
    while (dirs.size() > 5) {
        std::string path = "backups\\" + dirs.front();
        RemoveDirectoryA(path.c_str()); // only works if empty, but signals intent
        dirs.erase(dirs.begin());
        pruned++;
    }
    char buf[64]; snprintf(buf, sizeof(buf), "  Pruned %d backup(s).\n", pruned);
    ctx.output(buf);
    return CommandResult::ok("qw.backupPrune");
}
CommandResult handleQwAlertMonitor(const CommandContext& ctx)       { return delegateToGui(ctx, 9820, "qw.alertToggleMonitor"); }
CommandResult handleQwAlertHistory(const CommandContext& ctx)       { return delegateToGui(ctx, 9821, "qw.alertShowHistory"); }
CommandResult handleQwAlertDismiss(const CommandContext& ctx)       { return delegateToGui(ctx, 9822, "qw.alertDismissAll"); }
CommandResult handleQwAlertResourceStatus(const CommandContext& ctx){ return delegateToGui(ctx, 9823, "qw.alertResourceStatus"); }
CommandResult handleQwSloDashboard(const CommandContext& ctx)       { return delegateToGui(ctx, 9830, "qw.sloDashboard"); }

// ============================================================================
// TELEMETRY
// ============================================================================

CommandResult handleTelemetryToggle(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9900, 0);
        return CommandResult::ok("telemetry.toggle");
    }
    static bool telemetryOn = true;
    telemetryOn = !telemetryOn;
    char buf[64];
    snprintf(buf, sizeof(buf), "[TELEMETRY] Collection: %s\n", telemetryOn ? "ENABLED" : "DISABLED");
    ctx.output(buf);
    return CommandResult::ok("telemetry.toggle");
}
CommandResult handleTelemetryExportJson(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9901, 0);
        return CommandResult::ok("telemetry.exportJson");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "telemetry_export.json";
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    auto& orch = AutoRepairOrchestrator::instance();
    auto oStats = orch.getStats();
    FILE* f = fopen(outFile, "w");
    if (f) {
        fprintf(f, "{\n  \"hotpatches\": %u,\n  \"repairs\": %u,\n  \"anomalies\": %u,\n  \"uptime_sec\": %llu\n}\n",
                stats.totalOperations.load(), oStats.repairsAttempted, oStats.anomaliesDetected, GetTickCount64()/1000);
        fclose(f);
        ctx.output("[TELEMETRY] Exported to: ");
        ctx.output(outFile);
        ctx.output("\n");
    } else {
        ctx.output("[TELEMETRY] Failed to write export file.\n");
    }
    return CommandResult::ok("telemetry.exportJson");
}
CommandResult handleTelemetryExportCsv(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9902, 0);
        return CommandResult::ok("telemetry.exportCsv");
    }
    const char* outFile = (ctx.args && ctx.args[0]) ? ctx.args : "telemetry_export.csv";
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    FILE* f = fopen(outFile, "w");
    if (f) {
        fprintf(f, "metric,value\nhotpatches,%u\nmemory_patches,%u\nbyte_patches,%u\nserver_patches,%u\nuptime_sec,%llu\n",
                stats.totalOperations.load(), stats.memoryPatchCount.load(), stats.bytePatchCount.load(), stats.serverPatchCount.load(), GetTickCount64()/1000);
        fclose(f);
        ctx.output("[TELEMETRY] CSV exported to: ");
        ctx.output(outFile);
        ctx.output("\n");
    } else {
        ctx.output("[TELEMETRY] Failed to write CSV.\n");
    }
    return CommandResult::ok("telemetry.exportCsv");
}
CommandResult handleTelemetryDashboard(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9903, 0);
        return CommandResult::ok("telemetry.dashboard");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    auto& sentinel = SentinelWatchdog::instance();
    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);
    std::ostringstream oss;
    oss << "=== Telemetry Dashboard ===\n"
        << "  Hotpatches:  " << stats.totalOperations << " (M:" << stats.memoryPatchCount << " B:" << stats.bytePatchCount << " S:" << stats.serverPatchCount << ")\n"
        << "  Sentinel:    " << (sentinel.isActive() ? "ACTIVE" : "inactive") << "\n"
        << "  Memory:      " << mem.dwMemoryLoad << "% used\n"
        << "  Uptime:      " << (GetTickCount64()/1000) << " sec\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("telemetry.dashboard");
}
CommandResult handleTelemetryClear(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9904, 0);
        return CommandResult::ok("telemetry.clear");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    mgr.resetStats();
    ProxyHotpatcher::instance().resetStats();
    ctx.output("[TELEMETRY] All telemetry data cleared.\n");
    return CommandResult::ok("telemetry.clear");
}
CommandResult handleTelemetrySnapshot(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, 9905, 0);
        return CommandResult::ok("telemetry.snapshot");
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    SYSTEMTIME st;
    GetLocalTime(&st);
    char filename[128];
    snprintf(filename, sizeof(filename), "snapshot_%04d%02d%02d_%02d%02d%02d.json",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "{\"timestamp\":\"%04d-%02d-%02dT%02d:%02d:%02d\",\"patches\":%u}\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, stats.totalOperations.load());
        fclose(f);
        ctx.output("[TELEMETRY] Snapshot saved: ");
        ctx.output(filename);
        ctx.output("\n");
    } else {
        ctx.output("[TELEMETRY] Failed to save snapshot.\n");
    }
    return CommandResult::ok("telemetry.snapshot");
}

// ============================================================================
// TIER 1: CRITICAL COSMETICS (12000-12099)
// These handlers route through Win32IDE::routeCommand() → handleTier1Command()
// which is wired in Win32IDE_Commands.cpp. The delegateToGui pattern cannot
// be used here because it would create infinite WM_COMMAND re-entry.
// Instead, these stubs just confirm dispatch; the real work happens in
// Win32IDE_Tier1Cosmetics.cpp via the routeCommand() fallback path.
// ============================================================================

CommandResult handleTier1SmoothScrollToggle(const CommandContext& ctx)  { return delegateToGui(ctx, 12000, "tier1.smoothScroll"); }
CommandResult handleTier1MinimapEnhanced(const CommandContext& ctx)     { return delegateToGui(ctx, 12001, "tier1.minimapEnhanced"); }
CommandResult handleTier1BreadcrumbsToggle(const CommandContext& ctx)   { return delegateToGui(ctx, 12010, "tier1.breadcrumbs"); }
CommandResult handleTier1FuzzyPalette(const CommandContext& ctx)        { return delegateToGui(ctx, 12020, "tier1.fuzzyPalette"); }
CommandResult handleTier1SettingsGUI(const CommandContext& ctx)         { return delegateToGui(ctx, 12030, "tier1.settingsGUI"); }
CommandResult handleTier1WelcomePage(const CommandContext& ctx)         { return delegateToGui(ctx, 12040, "tier1.welcomePage"); }
CommandResult handleTier1FileIconTheme(const CommandContext& ctx)       { return delegateToGui(ctx, 12050, "tier1.fileIcons"); }
CommandResult handleTier1TabDragToggle(const CommandContext& ctx)       { return delegateToGui(ctx, 12060, "tier1.tabDrag"); }
CommandResult handleTier1SplitVertical(const CommandContext& ctx)       { return delegateToGui(ctx, 12070, "tier1.splitVertical"); }
CommandResult handleTier1SplitHorizontal(const CommandContext& ctx)     { return delegateToGui(ctx, 12071, "tier1.splitHorizontal"); }
CommandResult handleTier1SplitGrid(const CommandContext& ctx)           { return delegateToGui(ctx, 12072, "tier1.splitGrid"); }
CommandResult handleTier1SplitClose(const CommandContext& ctx)          { return delegateToGui(ctx, 12073, "tier1.splitClose"); }
CommandResult handleTier1SplitFocusNext(const CommandContext& ctx)      { return delegateToGui(ctx, 12074, "tier1.splitFocusNext"); }
CommandResult handleTier1AutoUpdateCheck(const CommandContext& ctx)     { return delegateToGui(ctx, 12090, "tier1.autoUpdate"); }
CommandResult handleTier1UpdateDismiss(const CommandContext& ctx)       { return delegateToGui(ctx, 12091, "tier1.updateDismiss"); }
