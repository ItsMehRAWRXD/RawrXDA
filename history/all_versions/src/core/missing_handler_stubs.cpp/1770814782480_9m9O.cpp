// ============================================================================
// missing_handler_stubs.cpp — Stub implementations for unresolved handlers
// ============================================================================
// These are auto-generated stubs for command handlers referenced in the
// COMMAND_TABLE but not yet implemented. Each returns CommandResult::ok("stub").
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "shared_feature_dispatch.h"
#include <cstdio>

static CommandResult stubHandler(const char* name, const CommandContext& ctx) {
    (void)ctx;
    char buf[256];
    snprintf(buf, sizeof(buf), "[Stub] %s: not yet implemented", name);
    return CommandResult::ok(buf);
}

// ===== BACKEND HANDLERS =====
CommandResult handleBackendSwitchLocal(const CommandContext& ctx) { return stubHandler("handleBackendSwitchLocal", ctx); }
CommandResult handleBackendSwitchOllama(const CommandContext& ctx) { return stubHandler("handleBackendSwitchOllama", ctx); }
CommandResult handleBackendSwitchOpenAI(const CommandContext& ctx) { return stubHandler("handleBackendSwitchOpenAI", ctx); }
CommandResult handleBackendSwitchClaude(const CommandContext& ctx) { return stubHandler("handleBackendSwitchClaude", ctx); }
CommandResult handleBackendSwitchGemini(const CommandContext& ctx) { return stubHandler("handleBackendSwitchGemini", ctx); }
CommandResult handleBackendShowStatus(const CommandContext& ctx) { return stubHandler("handleBackendShowStatus", ctx); }
CommandResult handleBackendShowSwitcher(const CommandContext& ctx) { return stubHandler("handleBackendShowSwitcher", ctx); }
CommandResult handleBackendConfigure(const CommandContext& ctx) { return stubHandler("handleBackendConfigure", ctx); }
CommandResult handleBackendHealthCheck(const CommandContext& ctx) { return stubHandler("handleBackendHealthCheck", ctx); }
CommandResult handleBackendSetApiKey(const CommandContext& ctx) { return stubHandler("handleBackendSetApiKey", ctx); }
CommandResult handleBackendSaveConfigs(const CommandContext& ctx) { return stubHandler("handleBackendSaveConfigs", ctx); }

// ===== ROUTER HANDLERS =====
CommandResult handleRouterEnable(const CommandContext& ctx) { return stubHandler("handleRouterEnable", ctx); }
CommandResult handleRouterDisable(const CommandContext& ctx) { return stubHandler("handleRouterDisable", ctx); }
CommandResult handleRouterStatus(const CommandContext& ctx) { return stubHandler("handleRouterStatus", ctx); }
CommandResult handleRouterDecision(const CommandContext& ctx) { return stubHandler("handleRouterDecision", ctx); }
CommandResult handleRouterSetPolicy(const CommandContext& ctx) { return stubHandler("handleRouterSetPolicy", ctx); }
CommandResult handleRouterCapabilities(const CommandContext& ctx) { return stubHandler("handleRouterCapabilities", ctx); }
CommandResult handleRouterFallbacks(const CommandContext& ctx) { return stubHandler("handleRouterFallbacks", ctx); }
CommandResult handleRouterSaveConfig(const CommandContext& ctx) { return stubHandler("handleRouterSaveConfig", ctx); }
CommandResult handleRouterRoutePrompt(const CommandContext& ctx) { return stubHandler("handleRouterRoutePrompt", ctx); }
CommandResult handleRouterResetStats(const CommandContext& ctx) { return stubHandler("handleRouterResetStats", ctx); }
CommandResult handleRouterWhyBackend(const CommandContext& ctx) { return stubHandler("handleRouterWhyBackend", ctx); }
CommandResult handleRouterPinTask(const CommandContext& ctx) { return stubHandler("handleRouterPinTask", ctx); }
CommandResult handleRouterUnpinTask(const CommandContext& ctx) { return stubHandler("handleRouterUnpinTask", ctx); }
CommandResult handleRouterShowPins(const CommandContext& ctx) { return stubHandler("handleRouterShowPins", ctx); }
CommandResult handleRouterShowHeatmap(const CommandContext& ctx) { return stubHandler("handleRouterShowHeatmap", ctx); }
CommandResult handleRouterEnsembleEnable(const CommandContext& ctx) { return stubHandler("handleRouterEnsembleEnable", ctx); }
CommandResult handleRouterEnsembleDisable(const CommandContext& ctx) { return stubHandler("handleRouterEnsembleDisable", ctx); }
CommandResult handleRouterEnsembleStatus(const CommandContext& ctx) { return stubHandler("handleRouterEnsembleStatus", ctx); }
CommandResult handleRouterSimulate(const CommandContext& ctx) { return stubHandler("handleRouterSimulate", ctx); }
CommandResult handleRouterSimulateLast(const CommandContext& ctx) { return stubHandler("handleRouterSimulateLast", ctx); }
CommandResult handleRouterShowCostStats(const CommandContext& ctx) { return stubHandler("handleRouterShowCostStats", ctx); }

// ===== LSP HANDLERS =====
CommandResult handleLspStartAll(const CommandContext& ctx) { return stubHandler("handleLspStartAll", ctx); }
CommandResult handleLspStopAll(const CommandContext& ctx) { return stubHandler("handleLspStopAll", ctx); }
CommandResult handleLspStatus(const CommandContext& ctx) { return stubHandler("handleLspStatus", ctx); }
CommandResult handleLspGotoDef(const CommandContext& ctx) { return stubHandler("handleLspGotoDef", ctx); }
CommandResult handleLspFindRefs(const CommandContext& ctx) { return stubHandler("handleLspFindRefs", ctx); }
CommandResult handleLspRename(const CommandContext& ctx) { return stubHandler("handleLspRename", ctx); }
CommandResult handleLspHover(const CommandContext& ctx) { return stubHandler("handleLspHover", ctx); }
CommandResult handleLspDiagnostics(const CommandContext& ctx) { return stubHandler("handleLspDiagnostics", ctx); }
CommandResult handleLspRestart(const CommandContext& ctx) { return stubHandler("handleLspRestart", ctx); }
CommandResult handleLspClearDiag(const CommandContext& ctx) { return stubHandler("handleLspClearDiag", ctx); }
CommandResult handleLspSymbolInfo(const CommandContext& ctx) { return stubHandler("handleLspSymbolInfo", ctx); }
CommandResult handleLspConfigure(const CommandContext& ctx) { return stubHandler("handleLspConfigure", ctx); }
CommandResult handleLspSaveConfig(const CommandContext& ctx) { return stubHandler("handleLspSaveConfig", ctx); }

// ===== ASM HANDLERS =====
CommandResult handleAsmParse(const CommandContext& ctx) { return stubHandler("handleAsmParse", ctx); }
CommandResult handleAsmGoto(const CommandContext& ctx) { return stubHandler("handleAsmGoto", ctx); }
CommandResult handleAsmFindRefs(const CommandContext& ctx) { return stubHandler("handleAsmFindRefs", ctx); }
CommandResult handleAsmSymbolTable(const CommandContext& ctx) { return stubHandler("handleAsmSymbolTable", ctx); }
CommandResult handleAsmInstructionInfo(const CommandContext& ctx) { return stubHandler("handleAsmInstructionInfo", ctx); }
CommandResult handleAsmRegisterInfo(const CommandContext& ctx) { return stubHandler("handleAsmRegisterInfo", ctx); }
CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx) { return stubHandler("handleAsmAnalyzeBlock", ctx); }
CommandResult handleAsmCallGraph(const CommandContext& ctx) { return stubHandler("handleAsmCallGraph", ctx); }
CommandResult handleAsmDataFlow(const CommandContext& ctx) { return stubHandler("handleAsmDataFlow", ctx); }
CommandResult handleAsmDetectConvention(const CommandContext& ctx) { return stubHandler("handleAsmDetectConvention", ctx); }
CommandResult handleAsmSections(const CommandContext& ctx) { return stubHandler("handleAsmSections", ctx); }
CommandResult handleAsmClearSymbols(const CommandContext& ctx) { return stubHandler("handleAsmClearSymbols", ctx); }

// ===== HYBRID HANDLERS =====
CommandResult handleHybridComplete(const CommandContext& ctx) { return stubHandler("handleHybridComplete", ctx); }
CommandResult handleHybridDiagnostics(const CommandContext& ctx) { return stubHandler("handleHybridDiagnostics", ctx); }
CommandResult handleHybridSmartRename(const CommandContext& ctx) { return stubHandler("handleHybridSmartRename", ctx); }
CommandResult handleHybridAnalyzeFile(const CommandContext& ctx) { return stubHandler("handleHybridAnalyzeFile", ctx); }
CommandResult handleHybridAutoProfile(const CommandContext& ctx) { return stubHandler("handleHybridAutoProfile", ctx); }
CommandResult handleHybridStatus(const CommandContext& ctx) { return stubHandler("handleHybridStatus", ctx); }
CommandResult handleHybridSymbolUsage(const CommandContext& ctx) { return stubHandler("handleHybridSymbolUsage", ctx); }
CommandResult handleHybridExplainSymbol(const CommandContext& ctx) { return stubHandler("handleHybridExplainSymbol", ctx); }
CommandResult handleHybridAnnotateDiag(const CommandContext& ctx) { return stubHandler("handleHybridAnnotateDiag", ctx); }
CommandResult handleHybridStreamAnalyze(const CommandContext& ctx) { return stubHandler("handleHybridStreamAnalyze", ctx); }
CommandResult handleHybridSemanticPrefetch(const CommandContext& ctx) { return stubHandler("handleHybridSemanticPrefetch", ctx); }
CommandResult handleHybridCorrectionLoop(const CommandContext& ctx) { return stubHandler("handleHybridCorrectionLoop", ctx); }

// ===== MULTI-RESPONSE HANDLERS =====
CommandResult handleMultiRespGenerate(const CommandContext& ctx) { return stubHandler("handleMultiRespGenerate", ctx); }
CommandResult handleMultiRespSetMax(const CommandContext& ctx) { return stubHandler("handleMultiRespSetMax", ctx); }
CommandResult handleMultiRespSelectPreferred(const CommandContext& ctx) { return stubHandler("handleMultiRespSelectPreferred", ctx); }
CommandResult handleMultiRespCompare(const CommandContext& ctx) { return stubHandler("handleMultiRespCompare", ctx); }
CommandResult handleMultiRespShowStats(const CommandContext& ctx) { return stubHandler("handleMultiRespShowStats", ctx); }
CommandResult handleMultiRespShowTemplates(const CommandContext& ctx) { return stubHandler("handleMultiRespShowTemplates", ctx); }
CommandResult handleMultiRespToggleTemplate(const CommandContext& ctx) { return stubHandler("handleMultiRespToggleTemplate", ctx); }
CommandResult handleMultiRespShowPrefs(const CommandContext& ctx) { return stubHandler("handleMultiRespShowPrefs", ctx); }
CommandResult handleMultiRespShowLatest(const CommandContext& ctx) { return stubHandler("handleMultiRespShowLatest", ctx); }
CommandResult handleMultiRespShowStatus(const CommandContext& ctx) { return stubHandler("handleMultiRespShowStatus", ctx); }
CommandResult handleMultiRespClearHistory(const CommandContext& ctx) { return stubHandler("handleMultiRespClearHistory", ctx); }
CommandResult handleMultiRespApplyPreferred(const CommandContext& ctx) { return stubHandler("handleMultiRespApplyPreferred", ctx); }

// ===== GOV HANDLERS =====
CommandResult handleGovStatus(const CommandContext& ctx) { return stubHandler("handleGovStatus", ctx); }
CommandResult handleGovSubmitCommand(const CommandContext& ctx) { return stubHandler("handleGovSubmitCommand", ctx); }
CommandResult handleGovKillAll(const CommandContext& ctx) { return stubHandler("handleGovKillAll", ctx); }
CommandResult handleGovTaskList(const CommandContext& ctx) { return stubHandler("handleGovTaskList", ctx); }

// ===== SAFETY HANDLERS =====
CommandResult handleSafetyStatus(const CommandContext& ctx) { return stubHandler("handleSafetyStatus", ctx); }
CommandResult handleSafetyResetBudget(const CommandContext& ctx) { return stubHandler("handleSafetyResetBudget", ctx); }
CommandResult handleSafetyRollbackLast(const CommandContext& ctx) { return stubHandler("handleSafetyRollbackLast", ctx); }
CommandResult handleSafetyShowViolations(const CommandContext& ctx) { return stubHandler("handleSafetyShowViolations", ctx); }

// ===== REPLAY HANDLERS =====
CommandResult handleReplayStatus(const CommandContext& ctx) { return stubHandler("handleReplayStatus", ctx); }
CommandResult handleReplayShowLast(const CommandContext& ctx) { return stubHandler("handleReplayShowLast", ctx); }
CommandResult handleReplayExportSession(const CommandContext& ctx) { return stubHandler("handleReplayExportSession", ctx); }
CommandResult handleReplayCheckpoint(const CommandContext& ctx) { return stubHandler("handleReplayCheckpoint", ctx); }

// ===== CONFIDENCE HANDLERS =====
CommandResult handleConfidenceStatus(const CommandContext& ctx) { return stubHandler("handleConfidenceStatus", ctx); }
CommandResult handleConfidenceSetPolicy(const CommandContext& ctx) { return stubHandler("handleConfidenceSetPolicy", ctx); }

// ===== DEBUG HANDLERS =====
CommandResult handleDbgLaunch(const CommandContext& ctx) { return stubHandler("handleDbgLaunch", ctx); }
CommandResult handleDbgAttach(const CommandContext& ctx) { return stubHandler("handleDbgAttach", ctx); }
CommandResult handleDbgDetach(const CommandContext& ctx) { return stubHandler("handleDbgDetach", ctx); }
CommandResult handleDbgGo(const CommandContext& ctx) { return stubHandler("handleDbgGo", ctx); }
CommandResult handleDbgStepOver(const CommandContext& ctx) { return stubHandler("handleDbgStepOver", ctx); }
CommandResult handleDbgStepInto(const CommandContext& ctx) { return stubHandler("handleDbgStepInto", ctx); }
CommandResult handleDbgStepOut(const CommandContext& ctx) { return stubHandler("handleDbgStepOut", ctx); }
CommandResult handleDbgBreak(const CommandContext& ctx) { return stubHandler("handleDbgBreak", ctx); }
CommandResult handleDbgKill(const CommandContext& ctx) { return stubHandler("handleDbgKill", ctx); }
CommandResult handleDbgAddBp(const CommandContext& ctx) { return stubHandler("handleDbgAddBp", ctx); }
CommandResult handleDbgRemoveBp(const CommandContext& ctx) { return stubHandler("handleDbgRemoveBp", ctx); }
CommandResult handleDbgEnableBp(const CommandContext& ctx) { return stubHandler("handleDbgEnableBp", ctx); }
CommandResult handleDbgClearBps(const CommandContext& ctx) { return stubHandler("handleDbgClearBps", ctx); }
CommandResult handleDbgListBps(const CommandContext& ctx) { return stubHandler("handleDbgListBps", ctx); }
CommandResult handleDbgAddWatch(const CommandContext& ctx) { return stubHandler("handleDbgAddWatch", ctx); }
CommandResult handleDbgRemoveWatch(const CommandContext& ctx) { return stubHandler("handleDbgRemoveWatch", ctx); }
CommandResult handleDbgRegisters(const CommandContext& ctx) { return stubHandler("handleDbgRegisters", ctx); }
CommandResult handleDbgStack(const CommandContext& ctx) { return stubHandler("handleDbgStack", ctx); }
CommandResult handleDbgMemory(const CommandContext& ctx) { return stubHandler("handleDbgMemory", ctx); }
CommandResult handleDbgDisasm(const CommandContext& ctx) { return stubHandler("handleDbgDisasm", ctx); }
CommandResult handleDbgModules(const CommandContext& ctx) { return stubHandler("handleDbgModules", ctx); }
CommandResult handleDbgThreads(const CommandContext& ctx) { return stubHandler("handleDbgThreads", ctx); }
CommandResult handleDbgSwitchThread(const CommandContext& ctx) { return stubHandler("handleDbgSwitchThread", ctx); }
CommandResult handleDbgEvaluate(const CommandContext& ctx) { return stubHandler("handleDbgEvaluate", ctx); }
CommandResult handleDbgSetRegister(const CommandContext& ctx) { return stubHandler("handleDbgSetRegister", ctx); }
CommandResult handleDbgSearchMemory(const CommandContext& ctx) { return stubHandler("handleDbgSearchMemory", ctx); }
CommandResult handleDbgSymbolPath(const CommandContext& ctx) { return stubHandler("handleDbgSymbolPath", ctx); }
CommandResult handleDbgStatus(const CommandContext& ctx) { return stubHandler("handleDbgStatus", ctx); }

// ===== PLUGIN HANDLERS =====
CommandResult handlePluginShowPanel(const CommandContext& ctx) { return stubHandler("handlePluginShowPanel", ctx); }
CommandResult handlePluginLoad(const CommandContext& ctx) { return stubHandler("handlePluginLoad", ctx); }
CommandResult handlePluginUnload(const CommandContext& ctx) { return stubHandler("handlePluginUnload", ctx); }
CommandResult handlePluginUnloadAll(const CommandContext& ctx) { return stubHandler("handlePluginUnloadAll", ctx); }
CommandResult handlePluginRefresh(const CommandContext& ctx) { return stubHandler("handlePluginRefresh", ctx); }
CommandResult handlePluginScanDir(const CommandContext& ctx) { return stubHandler("handlePluginScanDir", ctx); }
CommandResult handlePluginShowStatus(const CommandContext& ctx) { return stubHandler("handlePluginShowStatus", ctx); }
CommandResult handlePluginToggleHotload(const CommandContext& ctx) { return stubHandler("handlePluginToggleHotload", ctx); }
CommandResult handlePluginConfigure(const CommandContext& ctx) { return stubHandler("handlePluginConfigure", ctx); }
