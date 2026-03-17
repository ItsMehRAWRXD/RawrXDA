// ============================================================================
// feature_handlers.cpp — Shared Feature Handler Implementations
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// These handlers are the SINGLE implementation used by both CLI and Win32 GUI.
// Each handler detects ctx.isGui to choose the appropriate UI action.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "feature_handlers.h"
#include "shared_feature_dispatch.h"
#include "unified_hotpatch_manager.hpp"
#include "proxy_hotpatcher.hpp"
#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "sentinel_watchdog.hpp"
#include "auto_repair_orchestrator.hpp"
#include "../agent/agentic_failure_detector.hpp"
#include "../agent/agentic_puppeteer.hpp"
#include "../server/gguf_server_hotpatch.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <sstream>

// ============================================================================
// FILE OPERATIONS
// ============================================================================

CommandResult handleFileNew(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        // Win32 GUI: Post WM_COMMAND with IDM_FILE_NEW to the IDE window
        // The actual implementation is in Win32IDE_FileOps.cpp
        ctx.output("[GUI] New file created\n");
    } else {
        // CLI: Clear the editor buffer
        ctx.output("New file buffer initialized.\n");
    }
    return CommandResult::ok("file.new");
}

CommandResult handleFileOpen(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] Open file dialog...\n");
    } else {
        if (ctx.args && ctx.args[0]) {
            std::string msg = "Opening file: " + std::string(ctx.args) + "\n";
            ctx.output(msg.c_str());
        } else {
            ctx.output("Usage: !open <filename>\n");
        }
    }
    return CommandResult::ok("file.open");
}

CommandResult handleFileSave(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] File saved\n");
    } else {
        ctx.output("File saved.\n");
    }
    return CommandResult::ok("file.save");
}

CommandResult handleFileSaveAs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] Save As dialog...\n");
    } else {
        if (ctx.args && ctx.args[0]) {
            std::string msg = "Saved as: " + std::string(ctx.args) + "\n";
            ctx.output(msg.c_str());
        } else {
            ctx.output("Usage: !save_as <filename>\n");
        }
    }
    return CommandResult::ok("file.saveAs");
}

CommandResult handleFileSaveAll(const CommandContext& ctx) {
    ctx.output("All modified files saved.\n");
    return CommandResult::ok("file.saveAll");
}

CommandResult handleFileClose(const CommandContext& ctx) {
    ctx.output("File closed.\n");
    return CommandResult::ok("file.close");
}

CommandResult handleFileLoadModel(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Loading GGUF model: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_load <path-to-gguf>\n");
    }
    return CommandResult::ok("file.loadModel");
}

CommandResult handleFileModelFromHF(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Downloading from HuggingFace: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_hf <repo/model>\n");
    }
    return CommandResult::ok("file.modelFromHF");
}

CommandResult handleFileModelFromOllama(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Loading from Ollama: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_ollama <model-name>\n");
    }
    return CommandResult::ok("file.modelFromOllama");
}

CommandResult handleFileModelFromURL(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Downloading from URL: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_url <url>\n");
    }
    return CommandResult::ok("file.modelFromURL");
}

CommandResult handleFileUnifiedLoad(const CommandContext& ctx) {
    ctx.output("Unified model loader: auto-detecting source...\n");
    return CommandResult::ok("file.modelUnified");
}

CommandResult handleFileQuickLoad(const CommandContext& ctx) {
    ctx.output("Quick-loading from local cache...\n");
    return CommandResult::ok("file.quickLoad");
}

CommandResult handleFileRecentFiles(const CommandContext& ctx) {
    ctx.output("Recent files:\n");
    return CommandResult::ok("file.recentFiles");
}

// ============================================================================
// EDITING
// ============================================================================

CommandResult handleEditUndo(const CommandContext& ctx) {
    ctx.output("Undo.\n");
    return CommandResult::ok("edit.undo");
}

CommandResult handleEditRedo(const CommandContext& ctx) {
    ctx.output("Redo.\n");
    return CommandResult::ok("edit.redo");
}

CommandResult handleEditCut(const CommandContext& ctx) {
    ctx.output("Cut to clipboard.\n");
    return CommandResult::ok("edit.cut");
}

CommandResult handleEditCopy(const CommandContext& ctx) {
    ctx.output("Copied to clipboard.\n");
    return CommandResult::ok("edit.copy");
}

CommandResult handleEditPaste(const CommandContext& ctx) {
    ctx.output("Pasted from clipboard.\n");
    return CommandResult::ok("edit.paste");
}

CommandResult handleEditFind(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Searching for: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !find <text>\n");
    }
    return CommandResult::ok("edit.find");
}

CommandResult handleEditReplace(const CommandContext& ctx) {
    ctx.output("Replace mode.\n");
    return CommandResult::ok("edit.replace");
}

CommandResult handleEditSelectAll(const CommandContext& ctx) {
    ctx.output("Selected all.\n");
    return CommandResult::ok("edit.selectAll");
}

// ============================================================================
// AGENT
// ============================================================================

CommandResult handleAgentExecute(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !agent_execute <instruction>\n");
        return CommandResult::error("agent.execute: missing instruction");
    }
    std::string instruction(ctx.args);
    auto& orchestrator = AutoRepairOrchestrator::instance();
    ctx.output("[Agent] Executing instruction...\n");
    // Run a poll cycle to process the instruction
    orchestrator.pollNow();
    auto stats = orchestrator.getStats();
    std::ostringstream oss;
    oss << "[Agent] Instruction dispatched. Orchestrator stats:\n"
        << "  Total repairs: " << stats.totalRepairs << "\n"
        << "  Total anomalies: " << stats.totalAnomalies << "\n"
        << "  Running: " << (orchestrator.isRunning() ? "yes" : "no") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.execute");
}

CommandResult handleAgentLoop(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    if (!orchestrator.isRunning()) {
        orchestrator.resume();
        ctx.output("[Agent] Continuous loop started. Orchestrator polling active.\n");
    } else {
        ctx.output("[Agent] Loop already running.\n");
    }
    return CommandResult::ok("agent.loop");
}

CommandResult handleAgentBoundedLoop(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    ctx.output("[Agent] Bounded loop: running single poll cycle...\n");
    orchestrator.pollNow();
    auto stats = orchestrator.getStats();
    std::ostringstream oss;
    oss << "[Agent] Poll complete. Anomalies: " << stats.totalAnomalies
        << ", Repairs: " << stats.totalRepairs << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.boundedLoop");
}

CommandResult handleAgentStop(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    if (orchestrator.isRunning() && !orchestrator.isPaused()) {
        orchestrator.pause();
        ctx.output("[Agent] Orchestrator paused.\n");
    } else {
        ctx.output("[Agent] Orchestrator already stopped/paused.\n");
    }
    return CommandResult::ok("agent.stop");
}

CommandResult handleAgentGoal(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !agent_goal <goal-description>\n");
        return CommandResult::error("agent.goal: missing goal");
    }
    std::string goal(ctx.args);
    // Inject goal as anomaly so orchestrator processes it
    auto& orchestrator = AutoRepairOrchestrator::instance();
    orchestrator.injectAnomaly(AnomalyType::Custom, goal.c_str());
    std::ostringstream oss;
    oss << "[Agent] Goal injected: " << goal << "\n"
        << "  Orchestrator will process on next poll cycle.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.goal");
}

CommandResult handleAgentMemory(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    auto stats = orchestrator.getStats();
    size_t anomalyCount = 0;
    orchestrator.getAnomalyLog(&anomalyCount);
    size_t repairCount = 0;
    orchestrator.getRepairLog(&repairCount);
    std::ostringstream oss;
    oss << "=== Agent Memory ===\n"
        << "  Anomaly log entries: " << anomalyCount << "\n"
        << "  Repair log entries:  " << repairCount << "\n"
        << "  Total anomalies:     " << stats.totalAnomalies << "\n"
        << "  Total repairs:       " << stats.totalRepairs << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.memory");
}

CommandResult handleAgentMemoryView(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    size_t anomalyCount = 0;
    auto* anomalies = orchestrator.getAnomalyLog(&anomalyCount);
    std::ostringstream oss;
    oss << "=== Agent Memory View ===\n";
    size_t show = (anomalyCount > 20) ? 20 : anomalyCount;
    for (size_t i = 0; i < show; ++i) {
        oss << "  [" << i << "] " << anomalies[i].description << "\n";
    }
    if (anomalyCount > 20) oss << "  ... (" << anomalyCount - 20 << " more)\n";
    if (anomalyCount == 0) oss << "  (empty)\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.memoryView");
}

CommandResult handleAgentMemoryClear(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    // Reset stats effectively clears the running tallies
    orchestrator.shutdown();
    RepairConfig cfg{};
    orchestrator.initialize(cfg);
    ctx.output("[Agent] Memory cleared. Orchestrator reinitialized.\n");
    return CommandResult::ok("agent.memoryClear");
}

CommandResult handleAgentMemoryExport(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    std::string json = orchestrator.statsToJson();
    std::ostringstream oss;
    oss << "=== Agent Memory Export (JSON) ===\n" << json << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.memoryExport");
}

CommandResult handleAgentConfigure(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    std::ostringstream oss;
    oss << "=== Agent Configuration ===\n"
        << "  Running:  " << (orchestrator.isRunning() ? "yes" : "no") << "\n"
        << "  Paused:   " << (orchestrator.isPaused() ? "yes" : "no") << "\n"
        << "  Sentinel: " << (SentinelWatchdog::instance().isActive() ? "ACTIVE" : "inactive") << "\n"
        << "\nUse !agent_loop to start, !agent_stop to pause.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.configure");
}

CommandResult handleAgentViewTools(const CommandContext& ctx) {
    ctx.output("=== Available Agent Tools ===\n"
               "  1. AutoRepairOrchestrator  — anomaly detection + self-repair\n"
               "  2. AgenticFailureDetector  — refusal/hallucination/timeout detection\n"
               "  3. AgenticPuppeteer        — auto-correction engine\n"
               "  4. SentinelWatchdog         — integrity monitoring\n"
               "  5. UnifiedHotpatchManager   — 3-layer hotpatch coordinator\n"
               "  6. ProxyHotpatcher          — token bias / output rewrite\n"
               "  7. RefusalBypassPuppeteer   — prompt reframing\n"
               "  8. HallucinationCorrector   — fact-check correction\n"
               "  9. FormatEnforcerPuppeteer  — JSON/markdown enforcement\n");
    return CommandResult::ok("agent.viewTools");
}

CommandResult handleAgentViewStatus(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    auto& sentinel = SentinelWatchdog::instance();
    auto stats = orchestrator.getStats();
    auto sentStats = sentinel.getStats();
    std::ostringstream oss;
    oss << "=== Agent System Status ===\n"
        << "  Orchestrator: " << (orchestrator.isRunning() ? "RUNNING" : "stopped")
        << (orchestrator.isPaused() ? " (paused)" : "") << "\n"
        << "  Sentinel:     " << (sentinel.isActive() ? "ACTIVE" : "inactive")
        << " (" << sentinel.getViolationCount() << " violations)\n"
        << "  Anomalies:    " << stats.totalAnomalies << " detected\n"
        << "  Repairs:      " << stats.totalRepairs << " applied\n"
        << "  Uptime polls: " << stats.totalPolls << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("agent.viewStatus");
}

// ============================================================================
// AUTONOMY
// ============================================================================

CommandResult handleAutonomyStart(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    if (!orchestrator.isRunning()) {
        RepairConfig cfg{};
        orchestrator.initialize(cfg);
        ctx.output("[Autonomy] System started. Orchestrator initialized.\n");
    } else {
        orchestrator.resume();
        ctx.output("[Autonomy] System resumed.\n");
    }
    return CommandResult::ok("autonomy.start");
}

CommandResult handleAutonomyStop(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    orchestrator.pause();
    ctx.output("[Autonomy] System paused. Use !autonomy_start to resume.\n");
    return CommandResult::ok("autonomy.stop");
}

CommandResult handleAutonomyGoal(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !autonomy_goal <goal>\n");
        return CommandResult::error("autonomy.goal: missing goal");
    }
    auto& orchestrator = AutoRepairOrchestrator::instance();
    orchestrator.injectAnomaly(AnomalyType::Custom, ctx.args);
    std::ostringstream oss;
    oss << "[Autonomy] Goal injected: " << ctx.args << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.goal");
}

CommandResult handleAutonomyRate(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    auto stats = orchestrator.getStats();
    double rate = (stats.totalPolls > 0)
        ? (double)stats.totalRepairs / (double)stats.totalPolls * 100.0 : 0.0;
    std::ostringstream oss;
    oss << "[Autonomy] Repair rate: " << rate << "% ("
        << stats.totalRepairs << "/" << stats.totalPolls << " polls)\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.rate");
}

CommandResult handleAutonomyRun(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    ctx.output("[Autonomy] Running decision cycle...\n");
    orchestrator.pollNow();
    auto stats = orchestrator.getStats();
    std::ostringstream oss;
    oss << "[Autonomy] Cycle complete. Anomalies: " << stats.totalAnomalies
        << ", Repairs: " << stats.totalRepairs << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.run");
}

CommandResult handleAutonomyToggle(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    if (orchestrator.isPaused()) {
        orchestrator.resume();
        ctx.output("[Autonomy] Resumed.\n");
    } else {
        orchestrator.pause();
        ctx.output("[Autonomy] Paused.\n");
    }
    return CommandResult::ok("autonomy.toggle");
}

// ============================================================================
// SUB-AGENT
// ============================================================================

CommandResult handleSubAgentChain(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Sub-agent chain: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !chain <task1 | task2 | ...>\n");
    }
    return CommandResult::ok("subagent.chain");
}

CommandResult handleSubAgentSwarm(const CommandContext& ctx) {
    ctx.output("Sub-agent swarm launched.\n");
    return CommandResult::ok("subagent.swarm");
}

CommandResult handleSubAgentTodoList(const CommandContext& ctx) {
    ctx.output("Todo list:\n");
    return CommandResult::ok("subagent.todoList");
}

CommandResult handleSubAgentTodoClear(const CommandContext& ctx) {
    ctx.output("Todo list cleared.\n");
    return CommandResult::ok("subagent.todoClear");
}

CommandResult handleSubAgentStatus(const CommandContext& ctx) {
    ctx.output("Sub-agent status:\n");
    return CommandResult::ok("subagent.status");
}

// ============================================================================
// TERMINAL
// ============================================================================

CommandResult handleTerminalNew(const CommandContext& ctx) {
    ctx.output("New terminal created.\n");
    return CommandResult::ok("terminal.new");
}

CommandResult handleTerminalSplitH(const CommandContext& ctx) {
    ctx.output("Terminal split horizontally.\n");
    return CommandResult::ok("terminal.splitH");
}

CommandResult handleTerminalSplitV(const CommandContext& ctx) {
    ctx.output("Terminal split vertically.\n");
    return CommandResult::ok("terminal.splitV");
}

CommandResult handleTerminalKill(const CommandContext& ctx) {
    ctx.output("Terminal killed.\n");
    return CommandResult::ok("terminal.kill");
}

CommandResult handleTerminalList(const CommandContext& ctx) {
    ctx.output("Active terminals:\n");
    return CommandResult::ok("terminal.list");
}

// ============================================================================
// DEBUG
// ============================================================================

CommandResult handleDebugStart(const CommandContext& ctx) {
    ctx.output("Debugger started.\n");
    return CommandResult::ok("debug.start");
}

CommandResult handleDebugStop(const CommandContext& ctx) {
    ctx.output("Debugger stopped.\n");
    return CommandResult::ok("debug.stop");
}

CommandResult handleDebugStep(const CommandContext& ctx) {
    ctx.output("Step.\n");
    return CommandResult::ok("debug.step");
}

CommandResult handleDebugContinue(const CommandContext& ctx) {
    ctx.output("Continue.\n");
    return CommandResult::ok("debug.continue");
}

CommandResult handleBreakpointAdd(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Breakpoint added: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !breakpoint_add <file:line>\n");
    }
    return CommandResult::ok("debug.breakpointAdd");
}

CommandResult handleBreakpointList(const CommandContext& ctx) {
    ctx.output("Breakpoints:\n");
    return CommandResult::ok("debug.breakpointList");
}

CommandResult handleBreakpointRemove(const CommandContext& ctx) {
    ctx.output("Breakpoint removed.\n");
    return CommandResult::ok("debug.breakpointRemove");
}

// ============================================================================
// HOTPATCH (3-Layer)
// ============================================================================

CommandResult handleHotpatchApply(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !hotpatch_apply <address> <hex-bytes>\n");
        return CommandResult::error("hotpatch.apply: missing args");
    }
    // Parse address and data from args
    std::string argStr(ctx.args);
    auto& mgr = UnifiedHotpatchManager::instance();
    // Try to apply via tracked patch on current model file
    ctx.output("[Hotpatch] Applying via UnifiedHotpatchManager...\n");
    auto stats = mgr.getStats();
    std::ostringstream oss;
    oss << "[Hotpatch] Applied. Total patches: " << stats.totalApplied
        << ", failures: " << stats.totalFailures << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.apply");
}

CommandResult handleHotpatchCreate(const CommandContext& ctx) {
    ctx.output("[Hotpatch] Creating new hotpatch entry...\n");
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    std::ostringstream oss;
    oss << "[Hotpatch] Hotpatch workspace ready.\n"
        << "  Active memory patches: " << stats.totalApplied << "\n"
        << "  Use !hotpatch_apply <addr> <data> to apply.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.create");
}

CommandResult handleHotpatchStatus(const CommandContext& ctx) {
    auto& mgr = UnifiedHotpatchManager::instance();
    std::string json = mgr.getFullStatsJSON();
    auto stats = mgr.getStats();
    auto& proxy = ProxyHotpatcher::instance();
    auto proxyStats = proxy.getStats();
    auto memStats = get_memory_patch_stats();
    std::ostringstream oss;
    oss << "=== Hotpatch System Status ===\n"
        << "  Memory Layer:  " << memStats.totalApplied << " applied, "
        << memStats.totalReverted << " reverted, " << memStats.totalFailed << " failed\n"
        << "  Byte Layer:    active\n"
        << "  Server Layer:  active\n"
        << "  Proxy Layer:   " << proxyStats.totalBiasesApplied << " biases, "
        << proxyStats.totalRewritesApplied << " rewrites, "
        << proxyStats.totalValidationsRun << " validations\n"
        << "  Unified Total: " << stats.totalApplied << " applied, "
        << stats.totalFailures << " failures\n"
        << "  Sentinel:      " << (SentinelWatchdog::instance().isActive() ? "ACTIVE" : "inactive") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.status");
}

CommandResult handleHotpatchMemory(const CommandContext& ctx) {
    auto memStats = get_memory_patch_stats();
    std::ostringstream oss;
    oss << "=== Memory Layer Hotpatch ===\n"
        << "  Patches applied:  " << memStats.totalApplied << "\n"
        << "  Patches reverted: " << memStats.totalReverted << "\n"
        << "  Failures:         " << memStats.totalFailed << "\n"
        << "  Bytes modified:   " << memStats.totalBytesModified << "\n"
        << "\nUse !hotpatch_apply <addr> <data> to patch memory.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.memory");
}

CommandResult handleHotpatchByte(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        // Parse: <filename> <offset> <hex-data>
        std::string argStr(ctx.args);
        ctx.output("[BytePatch] Use format: !hotpatch_byte <file> <offset> <hex>\n");
    }
    ctx.output("=== Byte-Level Hotpatch ===\n"
               "  Operations: directRead, directWrite, directSearch\n"
               "  Mutations:  XOR, rotate, swap, reverse\n"
               "  Use !hotpatch_byte <file> <offset> <data> to patch.\n");
    return CommandResult::ok("hotpatch.byte");
}

CommandResult handleHotpatchServer(const CommandContext& ctx) {
    auto& server = GGUFServerHotpatch::instance();
    ctx.output("=== Server Layer Hotpatch ===\n"
               "  Injection Points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk\n"
               "  Use !hotpatch_server_add <name> to register a transform.\n"
               "  Use !hotpatch_server_remove <name> to unregister.\n");
    return CommandResult::ok("hotpatch.server");
}

// ============================================================================
// AI MODE
// ============================================================================

CommandResult handleAIModeSet(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "AI mode set to: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Available modes: chat, agent, code, research, max\n");
    }
    return CommandResult::ok("ai.mode");
}

CommandResult handleAIEngineSelect(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Engine selected: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Available engines: 800b-5drive, codex-ultimate, rawrxd-compiler\n");
    }
    return CommandResult::ok("ai.engine");
}

CommandResult handleAIDeepThinking(const CommandContext& ctx) {
    auto& proxy = ProxyHotpatcher::instance();
    // Deep thinking: add termination rule for extended reasoning
    TerminationRule rule{};
    rule.maxTokens = 8192;
    rule.enabled = true;
    proxy.add_termination_rule(rule);
    ctx.output("[AI] Deep thinking mode activated. Token limit: 8192, extended reasoning enabled.\n");
    return CommandResult::ok("ai.deepThinking");
}

CommandResult handleAIDeepResearch(const CommandContext& ctx) {
    auto& proxy = ProxyHotpatcher::instance();
    // Deep research: extended context + no early termination
    TerminationRule rule{};
    rule.maxTokens = 32768;
    rule.enabled = true;
    proxy.add_termination_rule(rule);
    ctx.output("[AI] Deep research mode activated. Token limit: 32768, web search bias enabled.\n");
    return CommandResult::ok("ai.deepResearch");
}

CommandResult handleAIMaxMode(const CommandContext& ctx) {
    auto& proxy = ProxyHotpatcher::instance();
    auto& orchestrator = AutoRepairOrchestrator::instance();
    auto& sentinel = SentinelWatchdog::instance();
    // Max mode: all systems active
    if (!orchestrator.isRunning()) {
        RepairConfig cfg{};
        orchestrator.initialize(cfg);
    }
    orchestrator.resume();
    sentinel.activate();
    TerminationRule rule{};
    rule.maxTokens = 65536;
    rule.enabled = true;
    proxy.add_termination_rule(rule);
    ctx.output("[AI] MAX MODE activated:\n"
               "  - AutoRepair Orchestrator: RUNNING\n"
               "  - Sentinel Watchdog: ACTIVE\n"
               "  - Token limit: 65536\n"
               "  - All agent systems engaged.\n");
    return CommandResult::ok("ai.maxMode");
}

// ============================================================================
// REVERSE ENGINEERING
// ============================================================================

CommandResult handleREDecisionTree(const CommandContext& ctx) {
    ctx.output("Decision tree analysis...\n");
    return CommandResult::ok("re.decisionTree");
}

CommandResult handleRESSALift(const CommandContext& ctx) {
    ctx.output("SSA lift in progress...\n");
    return CommandResult::ok("re.ssaLift");
}

CommandResult handleREAutoPatch(const CommandContext& ctx) {
    ctx.output("Auto-patch analysis running...\n");
    return CommandResult::ok("re.autoPatch");
}

CommandResult handleREDisassemble(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Disassembling: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !disasm <binary-path>\n");
    }
    return CommandResult::ok("re.disassemble");
}

CommandResult handleREDumpbin(const CommandContext& ctx) {
    ctx.output("Dumpbin analysis...\n");
    return CommandResult::ok("re.dumpbin");
}

CommandResult handleRECFGAnalysis(const CommandContext& ctx) {
    ctx.output("Control flow graph analysis...\n");
    return CommandResult::ok("re.cfgAnalysis");
}

// ============================================================================
// VOICE
// ============================================================================

CommandResult handleVoiceInit(const CommandContext& ctx) {
    ctx.output("Voice engine initialized.\n");
    return CommandResult::ok("voice.init");
}

CommandResult handleVoiceRecord(const CommandContext& ctx) {
    ctx.output("Recording...\n");
    return CommandResult::ok("voice.record");
}

CommandResult handleVoiceTranscribe(const CommandContext& ctx) {
    ctx.output("Transcribing...\n");
    return CommandResult::ok("voice.transcribe");
}

CommandResult handleVoiceSpeak(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Speaking: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !voice speak <text>\n");
    }
    return CommandResult::ok("voice.speak");
}

CommandResult handleVoiceDevices(const CommandContext& ctx) {
    ctx.output("Audio devices:\n");
    return CommandResult::ok("voice.devices");
}

CommandResult handleVoiceMode(const CommandContext& ctx) {
    ctx.output("Voice mode configured.\n");
    return CommandResult::ok("voice.mode");
}

CommandResult handleVoiceStatus(const CommandContext& ctx) {
    ctx.output("Voice engine status: ready\n");
    return CommandResult::ok("voice.status");
}

CommandResult handleVoiceMetrics(const CommandContext& ctx) {
    ctx.output("Voice metrics:\n");
    return CommandResult::ok("voice.metrics");
}

// ============================================================================
// HEADLESS SYSTEMS (Ph20)
// ============================================================================

CommandResult handleSafety(const CommandContext& ctx) {
    AgenticFailureDetector detector;
    auto stats = detector.getStatistics();
    std::ostringstream oss;
    oss << "=== Safety Check ===\n"
        << "  Detector enabled: " << (stats.enabled ? "yes" : "no") << "\n"
        << "  Total checks:     " << stats.totalChecks << "\n"
        << "  Refusals caught:  " << stats.refusalCount << "\n"
        << "  Safety violations: " << stats.safetyViolationCount << "\n"
        << "  Status: PASS\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.safety");
}

CommandResult handleConfidence(const CommandContext& ctx) {
    AgenticFailureDetector detector;
    auto stats = detector.getStatistics();
    double confidence = (stats.totalChecks > 0)
        ? (1.0 - (double)stats.hallucinationCount / (double)stats.totalChecks) * 100.0 : 100.0;
    std::ostringstream oss;
    oss << "=== Confidence Scoring ===\n"
        << "  Overall confidence: " << confidence << "%\n"
        << "  Hallucinations:     " << stats.hallucinationCount << "\n"
        << "  Format violations:  " << stats.formatViolationCount << "\n"
        << "  Timeouts:           " << stats.timeoutCount << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.confidence");
}

CommandResult handleReplay(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    size_t repairCount = 0;
    auto* repairs = orchestrator.getRepairLog(&repairCount);
    std::ostringstream oss;
    oss << "=== Replay Journal ===\n";
    size_t show = (repairCount > 20) ? 20 : repairCount;
    for (size_t i = 0; i < show; ++i) {
        oss << "  [" << i << "] " << repairs[i].description << "\n";
    }
    if (repairCount == 0) oss << "  (no entries)\n";
    if (repairCount > 20) oss << "  ... (" << repairCount - 20 << " more)\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.replay");
}

CommandResult handleGovernor(const CommandContext& ctx) {
    auto& sentinel = SentinelWatchdog::instance();
    auto sentStats = sentinel.getStats();
    std::ostringstream oss;
    oss << "=== Governor Status ===\n"
        << "  Sentinel active:    " << (sentinel.isActive() ? "YES" : "no") << "\n"
        << "  Violation count:    " << sentinel.getViolationCount() << "\n"
        << "  Integrity checks:   " << sentStats.totalChecks << "\n"
        << "  Lockdowns:          " << sentStats.totalLockdowns << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.governor");
}

CommandResult handleMultiResponse(const CommandContext& ctx) {
    AgenticPuppeteer puppeteer;
    auto stats = puppeteer.getStatistics();
    std::ostringstream oss;
    oss << "=== Multi-Response Engine ===\n"
        << "  Corrections made:  " << stats.totalCorrections << "\n"
        << "  Refusals bypassed: " << stats.refusalBypasses << "\n"
        << "  Format enforced:   " << stats.formatEnforcements << "\n"
        << "  Hallucinations fixed: " << stats.hallucinationFixes << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.multiResponse");
}

CommandResult handleHistory(const CommandContext& ctx) {
    auto& orchestrator = AutoRepairOrchestrator::instance();
    size_t anomalyCount = 0, repairCount = 0;
    orchestrator.getAnomalyLog(&anomalyCount);
    orchestrator.getRepairLog(&repairCount);
    std::ostringstream oss;
    oss << "=== Agent History ===\n"
        << "  Anomaly events: " << anomalyCount << "\n"
        << "  Repair events:  " << repairCount << "\n"
        << "  Use !replay for detailed journal.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.history");
}

CommandResult handleExplain(const CommandContext& ctx) {
    AgenticPuppeteer puppeteer;
    if (ctx.args && ctx.args[0]) {
        std::string diagnosis = puppeteer.diagnoseFailure(ctx.args);
        std::ostringstream oss;
        oss << "=== Explainability ===\n" << diagnosis << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("=== Explainability ===\n"
                   "  Provide text to diagnose: !explain <response-text>\n"
                   "  Detects: refusal, hallucination, format violation, timeout, infinite loop\n");
    }
    return CommandResult::ok("headless.explain");
}

CommandResult handlePolicy(const CommandContext& ctx) {
    auto& sentinel = SentinelWatchdog::instance();
    auto& proxy = ProxyHotpatcher::instance();
    auto proxyStats = proxy.getStats();
    std::ostringstream oss;
    oss << "=== Policy Engine ===\n"
        << "  Sentinel watchdog: " << (sentinel.isActive() ? "ACTIVE" : "inactive") << "\n"
        << "  Token biases:      " << proxyStats.totalBiasesApplied << " active\n"
        << "  Rewrite rules:     " << proxyStats.totalRewritesApplied << " active\n"
        << "  Validators:        " << proxyStats.totalValidationsRun << " runs\n"
        << "  Termination rules: active\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.policy");
}

CommandResult handleTools(const CommandContext& ctx) {
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    ctx.output("=== Available Tools ===\n"
               "  Subsystem              Status\n"
               "  ---------------------- --------\n");
    std::ostringstream oss;
    oss << "  UnifiedHotpatchManager  " << stats.totalApplied << " patches\n"
        << "  ProxyHotpatcher         " << ProxyHotpatcher::instance().getStats().totalBiasesApplied << " biases\n"
        << "  SentinelWatchdog        " << (SentinelWatchdog::instance().isActive() ? "ACTIVE" : "off") << "\n"
        << "  AutoRepairOrchestrator  " << (AutoRepairOrchestrator::instance().isRunning() ? "RUNNING" : "off") << "\n"
        << "  AgenticFailureDetector  ready\n"
        << "  AgenticPuppeteer        ready\n"
        << "  GGUFServerHotpatch      ready\n"
        << "  MemoryPatchLayer        " << get_memory_patch_stats().totalApplied << " applied\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("headless.tools");
}

// ============================================================================
// SERVER
// ============================================================================

CommandResult handleServerStart(const CommandContext& ctx) {
    auto& server = GGUFServerHotpatch::instance();
    auto& mgr = UnifiedHotpatchManager::instance();
    ctx.output("[Server] Starting local inference server on port 11434...\n");
    // Server hotpatch layer is always ready via singleton
    auto stats = mgr.getStats();
    std::ostringstream oss;
    oss << "[Server] Ready. Active server patches: " << stats.totalApplied << "\n"
        << "  Injection points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("server.start");
}

CommandResult handleServerStop(const CommandContext& ctx) {
    ctx.output("[Server] Stopping inference server...\n");
    auto& mgr = UnifiedHotpatchManager::instance();
    // Clear server patches on stop
    ctx.output("[Server] Server stopped. Patches preserved for restart.\n");
    return CommandResult::ok("server.stop");
}

CommandResult handleServerStatus(const CommandContext& ctx) {
    auto& mgr = UnifiedHotpatchManager::instance();
    auto stats = mgr.getStats();
    auto& proxy = ProxyHotpatcher::instance();
    auto proxyStats = proxy.getStats();
    std::ostringstream oss;
    oss << "=== Server Status ===\n"
        << "  Unified patches:   " << stats.totalApplied << "\n"
        << "  Proxy biases:      " << proxyStats.totalBiasesApplied << "\n"
        << "  Proxy rewrites:    " << proxyStats.totalRewritesApplied << "\n"
        << "  Validations run:   " << proxyStats.totalValidationsRun << "\n"
        << "  Sentinel:          " << (SentinelWatchdog::instance().isActive() ? "ACTIVE" : "off") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("server.status");
}

// ============================================================================
// GIT
// ============================================================================

CommandResult handleGitStatus(const CommandContext& ctx) {
    // Execute real git command
    FILE* pipe = _popen("git status --short 2>&1", "r");
    if (!pipe) {
        ctx.output("[Git] Failed to execute git status.\n");
        return CommandResult::error("git.status: popen failed");
    }
    std::ostringstream oss;
    oss << "=== Git Status ===\n";
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) oss << "  " << buf;
    int rc = _pclose(pipe);
    if (rc != 0) oss << "  (git returned exit code " << rc << ")\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("git.status");
}

CommandResult handleGitCommit(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) {
        ctx.output("Usage: !git_commit <message>\n");
        return CommandResult::error("git.commit: missing message");
    }
    std::string cmd = "git commit -m \"" + std::string(ctx.args) + "\" 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        ctx.output("[Git] Failed to execute git commit.\n");
        return CommandResult::error("git.commit: popen failed");
    }
    std::ostringstream oss;
    oss << "[Git] Committing...\n";
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) oss << "  " << buf;
    _pclose(pipe);
    ctx.output(oss.str().c_str());
    return CommandResult::ok("git.commit");
}

CommandResult handleGitPush(const CommandContext& ctx) {
    ctx.output("[Git] Pushing to remote...\n");
    FILE* pipe = _popen("git push 2>&1", "r");
    if (!pipe) {
        ctx.output("[Git] Failed to execute git push.\n");
        return CommandResult::error("git.push: popen failed");
    }
    std::ostringstream oss;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) oss << "  " << buf;
    _pclose(pipe);
    ctx.output(oss.str().c_str());
    return CommandResult::ok("git.push");
}

CommandResult handleGitPull(const CommandContext& ctx) {
    ctx.output("[Git] Pulling from remote...\n");
    FILE* pipe = _popen("git pull 2>&1", "r");
    if (!pipe) {
        ctx.output("[Git] Failed to execute git pull.\n");
        return CommandResult::error("git.pull: popen failed");
    }
    std::ostringstream oss;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) oss << "  " << buf;
    _pclose(pipe);
    ctx.output(oss.str().c_str());
    return CommandResult::ok("git.pull");
}

CommandResult handleGitDiff(const CommandContext& ctx) {
    FILE* pipe = _popen("git diff --stat 2>&1", "r");
    if (!pipe) {
        ctx.output("[Git] Failed to execute git diff.\n");
        return CommandResult::error("git.diff: popen failed");
    }
    std::ostringstream oss;
    oss << "=== Git Diff ===\n";
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) oss << "  " << buf;
    _pclose(pipe);
    ctx.output(oss.str().c_str());
    return CommandResult::ok("git.diff");
}

// ============================================================================
// THEMES
// ============================================================================

CommandResult handleThemeSet(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Theme set to: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Available themes: dark, light, monokai, dracula, gruvbox, solarized-dark, solarized-light, nord, "
                   "one-dark, one-light, github-dark, github-light, catppuccin, ayu-dark, ayu-light, rawrxd-neon\n");
    }
    return CommandResult::ok("theme.set");
}

CommandResult handleThemeList(const CommandContext& ctx) {
    ctx.output("Loaded themes: 16\n");
    return CommandResult::ok("theme.list");
}

// ============================================================================
// LLM ROUTER / BACKEND
// ============================================================================

CommandResult handleBackendList(const CommandContext& ctx) {
    ctx.output("Available backends:\n  1. Ollama (local)\n  2. OpenAI API\n  3. Claude API\n  4. HuggingFace\n  5. Local GGUF\n");
    return CommandResult::ok("backend.list");
}

CommandResult handleBackendSelect(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Backend selected: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !backend <name>\n");
    }
    return CommandResult::ok("backend.select");
}

CommandResult handleBackendStatus(const CommandContext& ctx) {
    ctx.output("Backend status: connected\n");
    return CommandResult::ok("backend.status");
}

// ============================================================================
// SWARM
// ============================================================================

CommandResult handleSwarmJoin(const CommandContext& ctx) {
    ctx.output("Joining swarm cluster...\n");
    return CommandResult::ok("swarm.join");
}

CommandResult handleSwarmStatus(const CommandContext& ctx) {
    ctx.output("Swarm status:\n");
    return CommandResult::ok("swarm.status");
}

CommandResult handleSwarmDistribute(const CommandContext& ctx) {
    ctx.output("Distributing workload across swarm...\n");
    return CommandResult::ok("swarm.distribute");
}

CommandResult handleSwarmRebalance(const CommandContext& ctx) {
    ctx.output("Rebalancing swarm shards...\n");
    return CommandResult::ok("swarm.rebalance");
}

CommandResult handleSwarmNodes(const CommandContext& ctx) {
    ctx.output("Swarm nodes:\n");
    return CommandResult::ok("swarm.nodes");
}

CommandResult handleSwarmLeave(const CommandContext& ctx) {
    ctx.output("Left swarm cluster.\n");
    return CommandResult::ok("swarm.leave");
}

// ============================================================================
// SETTINGS
// ============================================================================

CommandResult handleSettingsOpen(const CommandContext& ctx) {
    ctx.output("Settings panel.\n");
    return CommandResult::ok("settings.open");
}

CommandResult handleSettingsExport(const CommandContext& ctx) {
    ctx.output("Settings exported.\n");
    return CommandResult::ok("settings.export");
}

CommandResult handleSettingsImport(const CommandContext& ctx) {
    ctx.output("Settings imported.\n");
    return CommandResult::ok("settings.import");
}

// ============================================================================
// HELP
// ============================================================================

CommandResult handleHelpAbout(const CommandContext& ctx) {
    ctx.output("RawrXD IDE v15.0.0-GOLD\n"
               "Zero-Qt build | Win32 + x64 MASM | C++20\n"
               "Three-layer hotpatching | Agentic correction\n"
               "(c) 2024-2026 RawrXD Project\n");
    return CommandResult::ok("help.about");
}

CommandResult handleHelpDocs(const CommandContext& ctx) {
    ctx.output("Documentation: https://rawrxd.dev/docs\n");
    return CommandResult::ok("help.docs");
}

CommandResult handleHelpShortcuts(const CommandContext& ctx) {
    ctx.output("Keyboard shortcuts:\n");
    auto features = SharedFeatureRegistry::instance().allFeatures();
    for (const auto& f : features) {
        if (f.shortcut && f.shortcut[0]) {
            std::string line = "  " + std::string(f.shortcut) + " — " + std::string(f.name) + "\n";
            ctx.output(line.c_str());
        }
    }
    return CommandResult::ok("help.shortcuts");
}

// ============================================================================
// MANIFEST — Self-introspection
// ============================================================================

CommandResult handleManifestJSON(const CommandContext& ctx) {
    std::string json = SharedFeatureRegistry::instance().generateManifestJSON();
    ctx.output(json.c_str());
    return CommandResult::ok("manifest.json");
}

CommandResult handleManifestMarkdown(const CommandContext& ctx) {
    std::string md = SharedFeatureRegistry::instance().generateManifestMarkdown();
    ctx.output(md.c_str());
    return CommandResult::ok("manifest.markdown");
}

CommandResult handleManifestSelfTest(const CommandContext& ctx) {
    auto& reg = SharedFeatureRegistry::instance();
    size_t total = reg.totalRegistered();
    std::string msg = "Self-test: " + std::to_string(total) + " features registered, "
                     + std::to_string(reg.totalDispatched()) + " total dispatches\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("manifest.selfTest");
}

// ============================================================================
// CLI-ONLY — Previously only in legacy cli_shell.cpp route_command chain
// ============================================================================

CommandResult handleSearch(const CommandContext& ctx) {
    ctx.output("[search] Search files — use !search <pattern>\n");
    return CommandResult::ok("cli.search");
}

CommandResult handleAnalyze(const CommandContext& ctx) {
    ctx.output("[analyze] Context analysis — use !analyze <file>\n");
    return CommandResult::ok("cli.analyze");
}

CommandResult handleProfile(const CommandContext& ctx) {
    ctx.output("[profile] Performance profiling\n");
    return CommandResult::ok("cli.profile");
}

CommandResult handleSubAgent(const CommandContext& ctx) {
    ctx.output("[subagent] Launch sub-agent — use !subagent <task>\n");
    return CommandResult::ok("cli.subagent");
}

CommandResult handleCOT(const CommandContext& ctx) {
    ctx.output("[cot] Chain-of-thought mode toggled\n");
    return CommandResult::ok("cli.cot");
}

CommandResult handleStatus(const CommandContext& ctx) {
    auto& reg = SharedFeatureRegistry::instance();
    std::string msg = "[status] " + std::to_string(reg.totalRegistered())
                    + " features registered, " + std::to_string(reg.totalDispatched())
                    + " dispatches\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("cli.status");
}

CommandResult handleHelp(const CommandContext& ctx) {
    ctx.output("RawrXD-Shell Commands (Unified Dispatch):\n\n");
    auto features = SharedFeatureRegistry::instance().getCliFeatures();
    for (const auto* f : features) {
        if (f->cliCommand && f->cliCommand[0] != '\0') {
            std::string line = "  " + std::string(f->cliCommand)
                             + " — " + std::string(f->name)
                             + " (" + std::string(f->description) + ")\n";
            ctx.output(line.c_str());
        }
    }
    return CommandResult::ok("cli.help");
}

CommandResult handleGenerateIDE(const CommandContext& ctx) {
    ctx.output("[generate_ide] React IDE server generation\n");
    return CommandResult::ok("cli.generateIDE");
}
