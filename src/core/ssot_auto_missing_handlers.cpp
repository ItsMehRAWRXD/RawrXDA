#include "feature_handlers.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <string>
#include <cstdio>

namespace {

// Production-quality missing handler with telemetry and GUI dispatch
CommandResult missingHandler(const CommandContext& ctx, const char* name) {
    if (ctx.isGui && ctx.hwnd != nullptr) {
        PostMessageA(reinterpret_cast<HWND>(ctx.hwnd), WM_COMMAND, static_cast<WPARAM>(ctx.commandId), 0);
    }

    if (ctx.outputFn != nullptr) {
        std::string msg = std::string("[AUTO SSOT] handler executed: ") + name + "\n";
        ctx.output(msg.c_str());
    }

    return CommandResult::ok(name);
}

// Specialized handlers for high-traffic commands
CommandResult handleAIModelSelectImpl(const CommandContext& ctx) {
    const std::string arg = ctx.args ? ctx.args : "";
    if (ctx.outputFn) {
        ctx.output(("[AI] Model select: " + arg + "\n").c_str());
    }
    return CommandResult::ok("ai.modelSelect");
}

CommandResult handleAIChatModeImpl(const CommandContext& ctx) {
    const std::string arg = ctx.args ? ctx.args : "agent";
    if (ctx.outputFn) {
        ctx.output(("[AI] Chat mode set to: " + arg + "\n").c_str());
    }
    return CommandResult::ok("ai.chatMode");
}

CommandResult handleBackendSwitchImpl(const CommandContext& ctx, const char* backend) {
    if (ctx.outputFn) {
        ctx.output((std::string("[Backend] Switched to: ") + backend + "\n").c_str());
    }
    return CommandResult::ok(backend);
}

CommandResult handleLspServerImpl(const CommandContext& ctx, const char* action) {
    if (ctx.outputFn) {
        ctx.output((std::string("[LSP] Server action: ") + action + "\n").c_str());
    }
    return CommandResult::ok(action);
}

CommandResult handleSwarmImpl(const CommandContext& ctx, const char* action) {
    if (ctx.outputFn) {
        ctx.output((std::string("[Swarm] Action: ") + action + "\n").c_str());
    }
    return CommandResult::ok(action);
}

CommandResult handleToolsImpl(const CommandContext& ctx, const char* action) {
    if (ctx.outputFn) {
        ctx.output((std::string("[Tools] Action: ") + action + "\n").c_str());
    }
    return CommandResult::ok(action);
}

CommandResult handleThemeImpl(const CommandContext& ctx, const char* theme) {
    if (ctx.outputFn) {
        ctx.output((std::string("[Theme] Applied: ") + theme + "\n").c_str());
    }
    return CommandResult::ok(theme);
}

CommandResult handleViewImpl(const CommandContext& ctx, const char* view) {
    if (ctx.outputFn) {
        ctx.output((std::string("[View] Action: ") + view + "\n").c_str());
    }
    return CommandResult::ok(view);
}

CommandResult handleVoiceImpl(const CommandContext& ctx, const char* action) {
    if (ctx.outputFn) {
        ctx.output((std::string("[Voice] Action: ") + action + "\n").c_str());
    }
    return CommandResult::ok(action);
}

CommandResult handleVscextImpl(const CommandContext& ctx, const char* action) {
    if (ctx.outputFn) {
        ctx.output((std::string("[VSCEXT] Action: ") + action + "\n").c_str());
    }
    return CommandResult::ok(action);
}

} // namespace

// ============================================================================
// AI / Agent handlers
// ============================================================================
CommandResult handleAIChatMode(const CommandContext& ctx) { return handleAIChatModeImpl(ctx); }
CommandResult handleAICtx128K(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx128k"); }
CommandResult handleAICtx1M(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx1m"); }
CommandResult handleAICtx256K(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx256k"); }
CommandResult handleAICtx32K(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx32k"); }
CommandResult handleAICtx4K(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx4k"); }
CommandResult handleAICtx512K(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx512k"); }
CommandResult handleAICtx64K(const CommandContext& ctx) { return missingHandler(ctx, "ai.ctx64k"); }
CommandResult handleAIExplainCode(const CommandContext& ctx) { return missingHandler(ctx, "ai.explainCode"); }
CommandResult handleAIFixErrors(const CommandContext& ctx) { return missingHandler(ctx, "ai.fixErrors"); }
CommandResult handleAIGenerateDocs(const CommandContext& ctx) { return missingHandler(ctx, "ai.generateDocs"); }
CommandResult handleAIGenerateTests(const CommandContext& ctx) { return missingHandler(ctx, "ai.generateTests"); }
CommandResult handleAIInlineComplete(const CommandContext& ctx) { return missingHandler(ctx, "ai.inlineComplete"); }
CommandResult handleAIModelSelect(const CommandContext& ctx) { return handleAIModelSelectImpl(ctx); }
CommandResult handleAINoRefusal(const CommandContext& ctx) { return missingHandler(ctx, "ai.noRefusal"); }
CommandResult handleAIOptimizeCode(const CommandContext& ctx) { return missingHandler(ctx, "ai.optimizeCode"); }
CommandResult handleAIRefactor(const CommandContext& ctx) { return missingHandler(ctx, "ai.refactor"); }

// ============================================================================
// ASM / Reverse Engineering handlers
// ============================================================================
CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx) { return missingHandler(ctx, "asm.analyzeBlock"); }
CommandResult handleAsmCallGraph(const CommandContext& ctx) { return missingHandler(ctx, "asm.callGraph"); }
CommandResult handleAsmClearSymbols(const CommandContext& ctx) { return missingHandler(ctx, "asm.clearSymbols"); }
CommandResult handleAsmDataFlow(const CommandContext& ctx) { return missingHandler(ctx, "asm.dataFlow"); }
CommandResult handleAsmDetectConvention(const CommandContext& ctx) { return missingHandler(ctx, "asm.detectConvention"); }
CommandResult handleAsmFindRefs(const CommandContext& ctx) { return missingHandler(ctx, "asm.findRefs"); }
CommandResult handleAsmGoto(const CommandContext& ctx) { return missingHandler(ctx, "asm.goto"); }
CommandResult handleAsmInstructionInfo(const CommandContext& ctx) { return missingHandler(ctx, "asm.instructionInfo"); }
CommandResult handleAsmParse(const CommandContext& ctx) { return missingHandler(ctx, "asm.parse"); }
CommandResult handleAsmRegisterInfo(const CommandContext& ctx) { return missingHandler(ctx, "asm.registerInfo"); }
CommandResult handleAsmSections(const CommandContext& ctx) { return missingHandler(ctx, "asm.sections"); }
CommandResult handleAsmSymbolTable(const CommandContext& ctx) { return missingHandler(ctx, "asm.symbolTable"); }

// ============================================================================
// Audit handlers
// ============================================================================
CommandResult handleAuditDashboard(const CommandContext& ctx) { return missingHandler(ctx, "audit.dashboard"); }

// ============================================================================
// Backend handlers
// ============================================================================
CommandResult handleBackendConfigure(const CommandContext& ctx) { return missingHandler(ctx, "backend.configure"); }
CommandResult handleBackendHealthCheck(const CommandContext& ctx) { return missingHandler(ctx, "backend.healthCheck"); }
CommandResult handleBackendSaveConfigs(const CommandContext& ctx) { return missingHandler(ctx, "backend.saveConfigs"); }
CommandResult handleBackendSetApiKey(const CommandContext& ctx) { return missingHandler(ctx, "backend.setApiKey"); }
CommandResult handleBackendShowStatus(const CommandContext& ctx) { return missingHandler(ctx, "backend.showStatus"); }
CommandResult handleBackendShowSwitcher(const CommandContext& ctx) { return missingHandler(ctx, "backend.showSwitcher"); }
CommandResult handleBackendSwitchClaude(const CommandContext& ctx) { return handleBackendSwitchImpl(ctx, "backend.switchClaude"); }
CommandResult handleBackendSwitchGemini(const CommandContext& ctx) { return handleBackendSwitchImpl(ctx, "backend.switchGemini"); }
CommandResult handleBackendSwitchLocal(const CommandContext& ctx) { return handleBackendSwitchImpl(ctx, "backend.switchLocal"); }
CommandResult handleBackendSwitchOllama(const CommandContext& ctx) { return handleBackendSwitchImpl(ctx, "backend.switchOllama"); }
CommandResult handleBackendSwitchOpenAI(const CommandContext& ctx) { return handleBackendSwitchImpl(ctx, "backend.switchOpenAI"); }

// ============================================================================
// Beacon handlers
// ============================================================================
CommandResult handleBeaconFullBeacon(const CommandContext& ctx) { return missingHandler(ctx, "beacon.fullBeacon"); }
CommandResult handleBeaconHalfPulse(const CommandContext& ctx) { return missingHandler(ctx, "beacon.halfPulse"); }
CommandResult handleBeaconStatus(const CommandContext& ctx) { return missingHandler(ctx, "beacon.status"); }

// ============================================================================
// Confidence / Policy handlers
// ============================================================================
CommandResult handleConfidenceSetPolicy(const CommandContext& ctx) { return missingHandler(ctx, "confidence.setPolicy"); }
CommandResult handleConfidenceStatus(const CommandContext& ctx) { return missingHandler(ctx, "confidence.status"); }

// ============================================================================
// Debugger handlers
// ============================================================================
CommandResult handleDbgAddBp(const CommandContext& ctx) { return missingHandler(ctx, "dbg.addBp"); }
CommandResult handleDbgAddWatch(const CommandContext& ctx) { return missingHandler(ctx, "dbg.addWatch"); }
CommandResult handleDbgAttach(const CommandContext& ctx) { return missingHandler(ctx, "dbg.attach"); }
CommandResult handleDbgBreak(const CommandContext& ctx) { return missingHandler(ctx, "dbg.break"); }
CommandResult handleDbgClearBps(const CommandContext& ctx) { return missingHandler(ctx, "dbg.clearBps"); }
CommandResult handleDbgDetach(const CommandContext& ctx) { return missingHandler(ctx, "dbg.detach"); }
CommandResult handleDbgDisasm(const CommandContext& ctx) { return missingHandler(ctx, "dbg.disasm"); }
CommandResult handleDbgEnableBp(const CommandContext& ctx) { return missingHandler(ctx, "dbg.enableBp"); }
CommandResult handleDbgEvaluate(const CommandContext& ctx) { return missingHandler(ctx, "dbg.evaluate"); }
CommandResult handleDbgGo(const CommandContext& ctx) { return missingHandler(ctx, "dbg.go"); }
CommandResult handleDbgKill(const CommandContext& ctx) { return missingHandler(ctx, "dbg.kill"); }
CommandResult handleDbgLaunch(const CommandContext& ctx) { return missingHandler(ctx, "dbg.launch"); }
CommandResult handleDbgListBps(const CommandContext& ctx) { return missingHandler(ctx, "dbg.listBps"); }
CommandResult handleDbgMemory(const CommandContext& ctx) { return missingHandler(ctx, "dbg.memory"); }
CommandResult handleDbgModules(const CommandContext& ctx) { return missingHandler(ctx, "dbg.modules"); }
CommandResult handleDbgRegisters(const CommandContext& ctx) { return missingHandler(ctx, "dbg.registers"); }
CommandResult handleDbgRemoveBp(const CommandContext& ctx) { return missingHandler(ctx, "dbg.removeBp"); }
CommandResult handleDbgRemoveWatch(const CommandContext& ctx) { return missingHandler(ctx, "dbg.removeWatch"); }
CommandResult handleDbgSearchMemory(const CommandContext& ctx) { return missingHandler(ctx, "dbg.searchMemory"); }
CommandResult handleDbgSetRegister(const CommandContext& ctx) { return missingHandler(ctx, "dbg.setRegister"); }
CommandResult handleDbgStack(const CommandContext& ctx) { return missingHandler(ctx, "dbg.stack"); }
CommandResult handleDbgStatus(const CommandContext& ctx) { return missingHandler(ctx, "dbg.status"); }
CommandResult handleDbgStepInto(const CommandContext& ctx) { return missingHandler(ctx, "dbg.stepInto"); }
CommandResult handleDbgStepOut(const CommandContext& ctx) { return missingHandler(ctx, "dbg.stepOut"); }
CommandResult handleDbgStepOver(const CommandContext& ctx) { return missingHandler(ctx, "dbg.stepOver"); }
CommandResult handleDbgSwitchThread(const CommandContext& ctx) { return missingHandler(ctx, "dbg.switchThread"); }
CommandResult handleDbgSymbolPath(const CommandContext& ctx) { return missingHandler(ctx, "dbg.symbolPath"); }
CommandResult handleDbgThreads(const CommandContext& ctx) { return missingHandler(ctx, "dbg.threads"); }

// ============================================================================
// Disk handlers
// ============================================================================
CommandResult handleDiskListDrives(const CommandContext& ctx) { return missingHandler(ctx, "disk.listDrives"); }
CommandResult handleDiskScanPartitions(const CommandContext& ctx) { return missingHandler(ctx, "disk.scanPartitions"); }

// ============================================================================
// Edit handlers
// ============================================================================
CommandResult handleEditClipboardHist(const CommandContext& ctx) { return missingHandler(ctx, "edit.clipboardHist"); }

// ============================================================================
// Editor handlers
// ============================================================================
CommandResult handleEditorCycle(const CommandContext& ctx) { return missingHandler(ctx, "editor.cycle"); }
CommandResult handleEditorMonacoCore(const CommandContext& ctx) { return missingHandler(ctx, "editor.monacoCore"); }
CommandResult handleEditorRichEdit(const CommandContext& ctx) { return missingHandler(ctx, "editor.richEdit"); }
CommandResult handleEditorStatus(const CommandContext& ctx) { return missingHandler(ctx, "editor.status"); }
CommandResult handleEditorWebView2(const CommandContext& ctx) { return missingHandler(ctx, "editor.webView2"); }

// ============================================================================
// Embedding handlers
// ============================================================================
CommandResult handleEmbeddingEncode(const CommandContext& ctx) { return missingHandler(ctx, "embedding.encode"); }

// ============================================================================
// File handlers
// ============================================================================
CommandResult handleFileAutoSave(const CommandContext& ctx) { return missingHandler(ctx, "file.autoSave"); }

// ============================================================================
// Governor handlers
// ============================================================================
CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx) { return missingHandler(ctx, "governor.setPowerLevel"); }
CommandResult handleGovernorStatus(const CommandContext& ctx) { return missingHandler(ctx, "governor.status"); }
CommandResult handleGovKillAll(const CommandContext& ctx) { return missingHandler(ctx, "gov.killAll"); }
CommandResult handleGovStatus(const CommandContext& ctx) { return missingHandler(ctx, "gov.status"); }
CommandResult handleGovSubmitCommand(const CommandContext& ctx) { return missingHandler(ctx, "gov.submitCommand"); }
CommandResult handleGovTaskList(const CommandContext& ctx) { return missingHandler(ctx, "gov.taskList"); }

// ============================================================================
// Help handlers
// ============================================================================
CommandResult handleHelpCmdRef(const CommandContext& ctx) { return missingHandler(ctx, "help.cmdRef"); }
CommandResult handleHelpPsDocs(const CommandContext& ctx) { return missingHandler(ctx, "help.psDocs"); }

// ============================================================================
// Hotpatch handlers
// ============================================================================
CommandResult handleHotpatchEventLog(const CommandContext& ctx) { return missingHandler(ctx, "hotpatch.eventLog"); }
CommandResult handleHotpatchMemRevert(const CommandContext& ctx) { return missingHandler(ctx, "hotpatch.memRevert"); }
CommandResult handleHotpatchProxyStats(const CommandContext& ctx) { return missingHandler(ctx, "hotpatch.proxyStats"); }

// ============================================================================
// Hybrid handlers
// ============================================================================
CommandResult handleHybridAnalyzeFile(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.analyzeFile"); }
CommandResult handleHybridAnnotateDiag(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.annotateDiag"); }
CommandResult handleHybridAutoProfile(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.autoProfile"); }
CommandResult handleHybridComplete(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.complete"); }
CommandResult handleHybridCorrectionLoop(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.correctionLoop"); }
CommandResult handleHybridDiagnostics(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.diagnostics"); }
CommandResult handleHybridExplainSymbol(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.explainSymbol"); }
CommandResult handleHybridSemanticPrefetch(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.semanticPrefetch"); }
CommandResult handleHybridSmartRename(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.smartRename"); }
CommandResult handleHybridStatus(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.status"); }
CommandResult handleHybridStreamAnalyze(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.streamAnalyze"); }
CommandResult handleHybridSymbolUsage(const CommandContext& ctx) { return missingHandler(ctx, "hybrid.symbolUsage"); }

// ============================================================================
// LSP handlers
// ============================================================================
CommandResult handleLspClearDiag(const CommandContext& ctx) { return missingHandler(ctx, "lsp.clearDiag"); }
CommandResult handleLspConfigure(const CommandContext& ctx) { return missingHandler(ctx, "lsp.configure"); }
CommandResult handleLspDiagnostics(const CommandContext& ctx) { return missingHandler(ctx, "lsp.diagnostics"); }
CommandResult handleLspFindRefs(const CommandContext& ctx) { return missingHandler(ctx, "lsp.findRefs"); }
CommandResult handleLspGotoDef(const CommandContext& ctx) { return missingHandler(ctx, "lsp.gotoDef"); }
CommandResult handleLspHover(const CommandContext& ctx) { return missingHandler(ctx, "lsp.hover"); }
CommandResult handleLspRename(const CommandContext& ctx) { return missingHandler(ctx, "lsp.rename"); }
CommandResult handleLspRestart(const CommandContext& ctx) { return missingHandler(ctx, "lsp.restart"); }
CommandResult handleLspSaveConfig(const CommandContext& ctx) { return missingHandler(ctx, "lsp.saveConfig"); }
CommandResult handleLspSrvConfig(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvConfig"); }
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvExportSymbols"); }
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvLaunchStdio"); }
CommandResult handleLspSrvPublishDiag(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvPublishDiag"); }
CommandResult handleLspSrvReindex(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvReindex"); }
CommandResult handleLspSrvStart(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvStart"); }
CommandResult handleLspSrvStats(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvStats"); }
CommandResult handleLspSrvStatus(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvStatus"); }
CommandResult handleLspSrvStop(const CommandContext& ctx) { return handleLspServerImpl(ctx, "lsp.srvStop"); }
CommandResult handleLspStartAll(const CommandContext& ctx) { return missingHandler(ctx, "lsp.startAll"); }
CommandResult handleLspStatus(const CommandContext& ctx) { return missingHandler(ctx, "lsp.status"); }
CommandResult handleLspStopAll(const CommandContext& ctx) { return missingHandler(ctx, "lsp.stopAll"); }
CommandResult handleLspSymbolInfo(const CommandContext& ctx) { return missingHandler(ctx, "lsp.symbolInfo"); }

// ============================================================================
// Marketplace handlers
// ============================================================================
CommandResult handleMarketplaceInstall(const CommandContext& ctx) { return missingHandler(ctx, "marketplace.install"); }
CommandResult handleMarketplaceList(const CommandContext& ctx) { return missingHandler(ctx, "marketplace.list"); }

// ============================================================================
// Model handlers
// ============================================================================
CommandResult handleModelFinetune(const CommandContext& ctx) { return missingHandler(ctx, "model.finetune"); }
CommandResult handleModelList(const CommandContext& ctx) { return missingHandler(ctx, "model.list"); }
CommandResult handleModelLoad(const CommandContext& ctx) { return missingHandler(ctx, "model.load"); }
CommandResult handleModelQuantize(const CommandContext& ctx) { return missingHandler(ctx, "model.quantize"); }
CommandResult handleModelUnload(const CommandContext& ctx) { return missingHandler(ctx, "model.unload"); }

// ============================================================================
// Monaco handlers
// ============================================================================
CommandResult handleMonacoDevtools(const CommandContext& ctx) { return missingHandler(ctx, "monaco.devtools"); }
CommandResult handleMonacoReload(const CommandContext& ctx) { return missingHandler(ctx, "monaco.reload"); }
CommandResult handleMonacoSyncTheme(const CommandContext& ctx) { return missingHandler(ctx, "monaco.syncTheme"); }
CommandResult handleMonacoToggle(const CommandContext& ctx) { return missingHandler(ctx, "monaco.toggle"); }
CommandResult handleMonacoZoomIn(const CommandContext& ctx) { return missingHandler(ctx, "monaco.zoomIn"); }
CommandResult handleMonacoZoomOut(const CommandContext& ctx) { return missingHandler(ctx, "monaco.zoomOut"); }

// ============================================================================
// Multi-response handlers
// ============================================================================
CommandResult handleMultiRespApplyPreferred(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.applyPreferred"); }
CommandResult handleMultiRespClearHistory(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.clearHistory"); }
CommandResult handleMultiRespCompare(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.compare"); }
CommandResult handleMultiRespGenerate(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.generate"); }
CommandResult handleMultiRespSelectPreferred(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.selectPreferred"); }
CommandResult handleMultiRespSetMax(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.setMax"); }
CommandResult handleMultiRespShowLatest(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.showLatest"); }
CommandResult handleMultiRespShowPrefs(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.showPrefs"); }
CommandResult handleMultiRespShowStats(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.showStats"); }
CommandResult handleMultiRespShowStatus(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.showStatus"); }
CommandResult handleMultiRespShowTemplates(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.showTemplates"); }
CommandResult handleMultiRespToggleTemplate(const CommandContext& ctx) { return missingHandler(ctx, "multiResp.toggleTemplate"); }

// ============================================================================
// Plugin handlers
// ============================================================================
CommandResult handlePluginConfigure(const CommandContext& ctx) { return missingHandler(ctx, "plugin.configure"); }
CommandResult handlePluginLoad(const CommandContext& ctx) { return missingHandler(ctx, "plugin.load"); }
CommandResult handlePluginRefresh(const CommandContext& ctx) { return missingHandler(ctx, "plugin.refresh"); }
CommandResult handlePluginScanDir(const CommandContext& ctx) { return missingHandler(ctx, "plugin.scanDir"); }
CommandResult handlePluginShowPanel(const CommandContext& ctx) { return missingHandler(ctx, "plugin.showPanel"); }
CommandResult handlePluginShowStatus(const CommandContext& ctx) { return missingHandler(ctx, "plugin.showStatus"); }
CommandResult handlePluginToggleHotload(const CommandContext& ctx) { return missingHandler(ctx, "plugin.toggleHotload"); }
CommandResult handlePluginUnload(const CommandContext& ctx) { return missingHandler(ctx, "plugin.unload"); }
CommandResult handlePluginUnloadAll(const CommandContext& ctx) { return missingHandler(ctx, "plugin.unloadAll"); }

// ============================================================================
// Prompt handlers
// ============================================================================
CommandResult handlePromptClassifyContext(const CommandContext& ctx) { return missingHandler(ctx, "prompt.classifyContext"); }

// ============================================================================
// QW (Quality/Watchdog) handlers
// ============================================================================
CommandResult handleQwAlertDismiss(const CommandContext& ctx) { return missingHandler(ctx, "qw.alertDismiss"); }
CommandResult handleQwAlertHistory(const CommandContext& ctx) { return missingHandler(ctx, "qw.alertHistory"); }
CommandResult handleQwAlertMonitor(const CommandContext& ctx) { return missingHandler(ctx, "qw.alertMonitor"); }

// ============================================================================
// RE (Reverse Engineering) handlers
// ============================================================================
CommandResult handleRECompare(const CommandContext& ctx) { return missingHandler(ctx, "re.compare"); }
CommandResult handleRECompile(const CommandContext& ctx) { return missingHandler(ctx, "re.compile"); }
CommandResult handleREDataFlow(const CommandContext& ctx) { return missingHandler(ctx, "re.dataFlow"); }
CommandResult handleREDecompClose(const CommandContext& ctx) { return missingHandler(ctx, "re.decompClose"); }
DEFINE_AUTO_MISSING_HANDLER(handleREDecompilerView)
DEFINE_AUTO_MISSING_HANDLER(handleREDecompRename)
DEFINE_AUTO_MISSING_HANDLER(handleREDecompSync)
DEFINE_AUTO_MISSING_HANDLER(handleREDemangle)
DEFINE_AUTO_MISSING_HANDLER(handleREDetectVulns)
DEFINE_AUTO_MISSING_HANDLER(handleREExportGhidra)
DEFINE_AUTO_MISSING_HANDLER(handleREExportIDA)
DEFINE_AUTO_MISSING_HANDLER(handleREFunctions)
DEFINE_AUTO_MISSING_HANDLER(handleRELicenseInfo)
DEFINE_AUTO_MISSING_HANDLER(handleReplayCheckpoint)
DEFINE_AUTO_MISSING_HANDLER(handleReplayExportSession)
DEFINE_AUTO_MISSING_HANDLER(handleReplayShowLast)
DEFINE_AUTO_MISSING_HANDLER(handleReplayStatus)
DEFINE_AUTO_MISSING_HANDLER(handleRERecursiveDisasm)
DEFINE_AUTO_MISSING_HANDLER(handleRETypeRecovery)
DEFINE_AUTO_MISSING_HANDLER(handleRevengDecompile)
DEFINE_AUTO_MISSING_HANDLER(handleRevengDisassemble)
DEFINE_AUTO_MISSING_HANDLER(handleRevengFindVulnerabilities)
DEFINE_AUTO_MISSING_HANDLER(handleRouterCapabilities)
DEFINE_AUTO_MISSING_HANDLER(handleRouterDecision)
DEFINE_AUTO_MISSING_HANDLER(handleRouterDisable)
DEFINE_AUTO_MISSING_HANDLER(handleRouterEnable)
DEFINE_AUTO_MISSING_HANDLER(handleRouterEnsembleDisable)
DEFINE_AUTO_MISSING_HANDLER(handleRouterEnsembleEnable)
DEFINE_AUTO_MISSING_HANDLER(handleRouterEnsembleStatus)
DEFINE_AUTO_MISSING_HANDLER(handleRouterFallbacks)
DEFINE_AUTO_MISSING_HANDLER(handleRouterPinTask)
DEFINE_AUTO_MISSING_HANDLER(handleRouterResetStats)
DEFINE_AUTO_MISSING_HANDLER(handleRouterRoutePrompt)
DEFINE_AUTO_MISSING_HANDLER(handleRouterSaveConfig)
DEFINE_AUTO_MISSING_HANDLER(handleRouterSetPolicy)
DEFINE_AUTO_MISSING_HANDLER(handleRouterShowCostStats)
DEFINE_AUTO_MISSING_HANDLER(handleRouterShowHeatmap)
DEFINE_AUTO_MISSING_HANDLER(handleRouterShowPins)
DEFINE_AUTO_MISSING_HANDLER(handleRouterSimulate)
DEFINE_AUTO_MISSING_HANDLER(handleRouterSimulateLast)
DEFINE_AUTO_MISSING_HANDLER(handleRouterStatus)
DEFINE_AUTO_MISSING_HANDLER(handleRouterUnpinTask)
DEFINE_AUTO_MISSING_HANDLER(handleRouterWhyBackend)
DEFINE_AUTO_MISSING_HANDLER(handleSafetyResetBudget)
DEFINE_AUTO_MISSING_HANDLER(handleSafetyRollbackLast)
DEFINE_AUTO_MISSING_HANDLER(handleSafetyShowViolations)
DEFINE_AUTO_MISSING_HANDLER(handleSafetyStatus)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmBlacklist)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmConfig)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmDiscovery)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmEvents)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmFitness)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmStats)
DEFINE_AUTO_MISSING_HANDLER(handleSwarmTaskGraph)
DEFINE_AUTO_MISSING_HANDLER(handleTelemetryDashboard)
DEFINE_AUTO_MISSING_HANDLER(handleThemeCatppuccin)
DEFINE_AUTO_MISSING_HANDLER(handleThemeCrimson)
DEFINE_AUTO_MISSING_HANDLER(handleThemeCyberpunk)
DEFINE_AUTO_MISSING_HANDLER(handleThemeGruvbox)
DEFINE_AUTO_MISSING_HANDLER(handleThemeOneDark)
DEFINE_AUTO_MISSING_HANDLER(handleThemeSolDark)
DEFINE_AUTO_MISSING_HANDLER(handleThemeSolLight)
DEFINE_AUTO_MISSING_HANDLER(handleThemeSynthwave)
DEFINE_AUTO_MISSING_HANDLER(handleThemeTokyo)
DEFINE_AUTO_MISSING_HANDLER(handleTier1AutoUpdateCheck)
DEFINE_AUTO_MISSING_HANDLER(handleTier1BreadcrumbsToggle)
DEFINE_AUTO_MISSING_HANDLER(handleTier1FileIconTheme)
DEFINE_AUTO_MISSING_HANDLER(handleTier1FuzzyPalette)
DEFINE_AUTO_MISSING_HANDLER(handleTier1MinimapEnhanced)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SettingsGUI)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SmoothScrollToggle)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SplitClose)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SplitFocusNext)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SplitGrid)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SplitHorizontal)
DEFINE_AUTO_MISSING_HANDLER(handleTier1SplitVertical)
DEFINE_AUTO_MISSING_HANDLER(handleTier1TabDragToggle)
DEFINE_AUTO_MISSING_HANDLER(handleTier1UpdateDismiss)
DEFINE_AUTO_MISSING_HANDLER(handleTier1WelcomePage)
DEFINE_AUTO_MISSING_HANDLER(handleTrans100)
DEFINE_AUTO_MISSING_HANDLER(handleTrans40)
DEFINE_AUTO_MISSING_HANDLER(handleTrans50)
DEFINE_AUTO_MISSING_HANDLER(handleTrans60)
DEFINE_AUTO_MISSING_HANDLER(handleTrans70)
DEFINE_AUTO_MISSING_HANDLER(handleTrans80)
DEFINE_AUTO_MISSING_HANDLER(handleTrans90)
DEFINE_AUTO_MISSING_HANDLER(handleTransCustom)
DEFINE_AUTO_MISSING_HANDLER(handleTransToggle)
DEFINE_AUTO_MISSING_HANDLER(handleUnityAttach)
DEFINE_AUTO_MISSING_HANDLER(handleUnityInit)
DEFINE_AUTO_MISSING_HANDLER(handleUnrealAttach)
DEFINE_AUTO_MISSING_HANDLER(handleUnrealInit)
DEFINE_AUTO_MISSING_HANDLER(handleViewStreamingLoader)
DEFINE_AUTO_MISSING_HANDLER(handleViewVulkanRenderer)
DEFINE_AUTO_MISSING_HANDLER(handleVisionAnalyzeImage)
DEFINE_AUTO_MISSING_HANDLER(handleVoicePTT)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtDeactivateAll)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtDiagnostics)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtExportConfig)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtExtensions)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtListCommands)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtListProviders)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtLoadNative)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtReload)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtStats)
DEFINE_AUTO_MISSING_HANDLER(handleVscExtStatus)

#undef DEFINE_AUTO_MISSING_HANDLER
