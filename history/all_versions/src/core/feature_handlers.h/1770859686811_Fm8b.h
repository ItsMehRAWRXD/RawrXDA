// ============================================================================
// feature_handlers.h — Shared Feature Handler Declarations
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Each handler works with both CLI and Win32 GUI via CommandContext.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_FEATURE_HANDLERS_H
#define RAWRXD_FEATURE_HANDLERS_H

#include "shared_feature_dispatch.h"

// ============================================================================
// FILE OPERATIONS — CLI: !new/!open/!save, GUI: IDM_FILE_*
// ============================================================================

CommandResult handleFileNew(const CommandContext& ctx);
CommandResult handleFileOpen(const CommandContext& ctx);
CommandResult handleFileSave(const CommandContext& ctx);
CommandResult handleFileSaveAs(const CommandContext& ctx);
CommandResult handleFileSaveAll(const CommandContext& ctx);
CommandResult handleFileClose(const CommandContext& ctx);
CommandResult handleFileLoadModel(const CommandContext& ctx);
CommandResult handleFileModelFromHF(const CommandContext& ctx);
CommandResult handleFileModelFromOllama(const CommandContext& ctx);
CommandResult handleFileModelFromURL(const CommandContext& ctx);
CommandResult handleFileUnifiedLoad(const CommandContext& ctx);
CommandResult handleFileQuickLoad(const CommandContext& ctx);
CommandResult handleFileRecentFiles(const CommandContext& ctx);

// ============================================================================
// EDITING — CLI: !cut/!copy/!paste, GUI: IDM_EDIT_*
// ============================================================================

CommandResult handleEditUndo(const CommandContext& ctx);
CommandResult handleEditRedo(const CommandContext& ctx);
CommandResult handleEditCut(const CommandContext& ctx);
CommandResult handleEditCopy(const CommandContext& ctx);
CommandResult handleEditPaste(const CommandContext& ctx);
CommandResult handleEditFind(const CommandContext& ctx);
CommandResult handleEditReplace(const CommandContext& ctx);
CommandResult handleEditSelectAll(const CommandContext& ctx);

// ============================================================================
// AGENT — CLI: !agent_execute/!agent_loop, GUI: IDM_AGENT_*
// ============================================================================

CommandResult handleAgentExecute(const CommandContext& ctx);
CommandResult handleAgentLoop(const CommandContext& ctx);
CommandResult handleAgentBoundedLoop(const CommandContext& ctx);
CommandResult handleAgentStop(const CommandContext& ctx);
CommandResult handleAgentGoal(const CommandContext& ctx);
CommandResult handleAgentMemory(const CommandContext& ctx);
CommandResult handleAgentMemoryView(const CommandContext& ctx);
CommandResult handleAgentMemoryClear(const CommandContext& ctx);
CommandResult handleAgentMemoryExport(const CommandContext& ctx);
CommandResult handleAgentConfigure(const CommandContext& ctx);
CommandResult handleAgentViewTools(const CommandContext& ctx);
CommandResult handleAgentViewStatus(const CommandContext& ctx);

// ============================================================================
// AUTONOMY — CLI: !autonomy_start/!autonomy_stop, GUI: IDM_AUTONOMY_*
// ============================================================================

CommandResult handleAutonomyStart(const CommandContext& ctx);
CommandResult handleAutonomyStop(const CommandContext& ctx);
CommandResult handleAutonomyGoal(const CommandContext& ctx);
CommandResult handleAutonomyRate(const CommandContext& ctx);
CommandResult handleAutonomyRun(const CommandContext& ctx);
CommandResult handleAutonomyToggle(const CommandContext& ctx);

// ============================================================================
// SUB-AGENT — CLI: !subagent/!chain/!swarm, GUI: IDM_SUBAGENT_*
// ============================================================================

CommandResult handleSubAgentChain(const CommandContext& ctx);
CommandResult handleSubAgentSwarm(const CommandContext& ctx);
CommandResult handleSubAgentTodoList(const CommandContext& ctx);
CommandResult handleSubAgentTodoClear(const CommandContext& ctx);
CommandResult handleSubAgentStatus(const CommandContext& ctx);

// ============================================================================
// TERMINAL — CLI: !terminal_new/split/kill, GUI: IDM_TERMINAL_*
// ============================================================================

CommandResult handleTerminalNew(const CommandContext& ctx);
CommandResult handleTerminalSplitH(const CommandContext& ctx);
CommandResult handleTerminalSplitV(const CommandContext& ctx);
CommandResult handleTerminalKill(const CommandContext& ctx);
CommandResult handleTerminalList(const CommandContext& ctx);

// ============================================================================
// DEBUG — CLI: !breakpoint_add/!debug_start, GUI: IDM_DEBUG_*
// ============================================================================

CommandResult handleDebugStart(const CommandContext& ctx);
CommandResult handleDebugStop(const CommandContext& ctx);
CommandResult handleDebugStep(const CommandContext& ctx);
CommandResult handleDebugContinue(const CommandContext& ctx);
CommandResult handleBreakpointAdd(const CommandContext& ctx);
CommandResult handleBreakpointList(const CommandContext& ctx);
CommandResult handleBreakpointRemove(const CommandContext& ctx);

// ============================================================================
// HOTPATCH (3-Layer) — CLI: !hotpatch_apply/create, GUI: IDM_HOTPATCH_*
// ============================================================================

CommandResult handleHotpatchApply(const CommandContext& ctx);
CommandResult handleHotpatchCreate(const CommandContext& ctx);
CommandResult handleHotpatchStatus(const CommandContext& ctx);
CommandResult handleHotpatchMemory(const CommandContext& ctx);
CommandResult handleHotpatchByte(const CommandContext& ctx);
CommandResult handleHotpatchServer(const CommandContext& ctx);

// ============================================================================
// AI MODE — CLI: !mode/!engine/!deep, GUI: IDM_AI_*
// ============================================================================

CommandResult handleAIModeSet(const CommandContext& ctx);
CommandResult handleAIEngineSelect(const CommandContext& ctx);
CommandResult handleAIDeepThinking(const CommandContext& ctx);
CommandResult handleAIDeepResearch(const CommandContext& ctx);
CommandResult handleAIMaxMode(const CommandContext& ctx);

// ============================================================================
// REVERSE ENGINEERING — CLI: !ssa_lift/!auto_patch, GUI: IDM_RE_*
// ============================================================================

CommandResult handleREDecisionTree(const CommandContext& ctx);
CommandResult handleRESSALift(const CommandContext& ctx);
CommandResult handleREAutoPatch(const CommandContext& ctx);
CommandResult handleREDisassemble(const CommandContext& ctx);
CommandResult handleREDumpbin(const CommandContext& ctx);
CommandResult handleRECFGAnalysis(const CommandContext& ctx);

// ============================================================================
// VOICE — CLI: !voice, GUI: IDM_VOICE_*
// ============================================================================

CommandResult handleVoiceInit(const CommandContext& ctx);
CommandResult handleVoiceRecord(const CommandContext& ctx);
CommandResult handleVoiceTranscribe(const CommandContext& ctx);
CommandResult handleVoiceSpeak(const CommandContext& ctx);
CommandResult handleVoiceDevices(const CommandContext& ctx);
CommandResult handleVoiceMode(const CommandContext& ctx);
CommandResult handleVoiceStatus(const CommandContext& ctx);
CommandResult handleVoiceMetrics(const CommandContext& ctx);

// ============================================================================
// HEADLESS SYSTEMS (Ph20) — CLI: !safety/!confidence/!, GUI: panels
// ============================================================================

CommandResult handleSafety(const CommandContext& ctx);
CommandResult handleConfidence(const CommandContext& ctx);
CommandResult handleReplay(const CommandContext& ctx);
CommandResult handleGovernor(const CommandContext& ctx);
CommandResult handleMultiResponse(const CommandContext& ctx);
CommandResult handleHistory(const CommandContext& ctx);
CommandResult handleExplain(const CommandContext& ctx);
CommandResult handlePolicy(const CommandContext& ctx);
CommandResult handleTools(const CommandContext& ctx);

// ============================================================================
// SERVER — CLI: !server, GUI: IDM_SERVER_*
// ============================================================================

CommandResult handleServerStart(const CommandContext& ctx);
CommandResult handleServerStop(const CommandContext& ctx);
CommandResult handleServerStatus(const CommandContext& ctx);

// ============================================================================
// GIT — GUI: IDM_GIT_*
// ============================================================================

CommandResult handleGitStatus(const CommandContext& ctx);
CommandResult handleGitCommit(const CommandContext& ctx);
CommandResult handleGitPush(const CommandContext& ctx);
CommandResult handleGitPull(const CommandContext& ctx);
CommandResult handleGitDiff(const CommandContext& ctx);

// ============================================================================
// THEMES — GUI: IDM_THEME_*
// ============================================================================

CommandResult handleThemeSet(const CommandContext& ctx);
CommandResult handleThemeList(const CommandContext& ctx);

// ============================================================================
// LLM ROUTER — CLI: !backend, GUI: IDM_BACKEND_*
// ============================================================================

CommandResult handleBackendList(const CommandContext& ctx);
CommandResult handleBackendSelect(const CommandContext& ctx);
CommandResult handleBackendStatus(const CommandContext& ctx);

// ============================================================================
// SWARM — CLI: !swarm_*, GUI: IDM_SWARM_*
// ============================================================================

CommandResult handleSwarmJoin(const CommandContext& ctx);
CommandResult handleSwarmStatus(const CommandContext& ctx);
CommandResult handleSwarmDistribute(const CommandContext& ctx);
CommandResult handleSwarmRebalance(const CommandContext& ctx);
CommandResult handleSwarmNodes(const CommandContext& ctx);
CommandResult handleSwarmLeave(const CommandContext& ctx);

// ============================================================================
// SETTINGS — GUI: IDM_SETTINGS_*
// ============================================================================

CommandResult handleSettingsOpen(const CommandContext& ctx);
CommandResult handleSettingsExport(const CommandContext& ctx);
CommandResult handleSettingsImport(const CommandContext& ctx);

// ============================================================================
// HELP — GUI: IDM_HELP_*
// ============================================================================

CommandResult handleHelpAbout(const CommandContext& ctx);
CommandResult handleHelpDocs(const CommandContext& ctx);
CommandResult handleHelpShortcuts(const CommandContext& ctx);

// ============================================================================
// LSP CLIENT — CLI: !lsp_*, GUI: IDM_LSP_*
// ============================================================================

CommandResult handleLspStartAll(const CommandContext& ctx);
CommandResult handleLspStopAll(const CommandContext& ctx);
CommandResult handleLspStatus(const CommandContext& ctx);
CommandResult handleLspGotoDef(const CommandContext& ctx);
CommandResult handleLspFindRefs(const CommandContext& ctx);
CommandResult handleLspRename(const CommandContext& ctx);
CommandResult handleLspHover(const CommandContext& ctx);
CommandResult handleLspDiagnostics(const CommandContext& ctx);
CommandResult handleLspRestart(const CommandContext& ctx);
CommandResult handleLspClearDiag(const CommandContext& ctx);
CommandResult handleLspSymbolInfo(const CommandContext& ctx);
CommandResult handleLspConfigure(const CommandContext& ctx);
CommandResult handleLspSaveConfig(const CommandContext& ctx);

// ============================================================================
// ASM SEMANTIC — CLI: !asm_*, GUI: IDM_ASM_*
// ============================================================================

CommandResult handleAsmParse(const CommandContext& ctx);
CommandResult handleAsmGoto(const CommandContext& ctx);
CommandResult handleAsmFindRefs(const CommandContext& ctx);
CommandResult handleAsmSymbolTable(const CommandContext& ctx);
CommandResult handleAsmInstructionInfo(const CommandContext& ctx);
CommandResult handleAsmRegisterInfo(const CommandContext& ctx);
CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx);
CommandResult handleAsmCallGraph(const CommandContext& ctx);
CommandResult handleAsmDataFlow(const CommandContext& ctx);
CommandResult handleAsmDetectConvention(const CommandContext& ctx);
CommandResult handleAsmSections(const CommandContext& ctx);
CommandResult handleAsmClearSymbols(const CommandContext& ctx);

// ============================================================================
// HYBRID (LSP-AI Bridge) — CLI: !hybrid_*, GUI: IDM_HYBRID_*
// ============================================================================

CommandResult handleHybridComplete(const CommandContext& ctx);
CommandResult handleHybridDiagnostics(const CommandContext& ctx);
CommandResult handleHybridSmartRename(const CommandContext& ctx);
CommandResult handleHybridAnalyzeFile(const CommandContext& ctx);
CommandResult handleHybridAutoProfile(const CommandContext& ctx);
CommandResult handleHybridStatus(const CommandContext& ctx);
CommandResult handleHybridSymbolUsage(const CommandContext& ctx);
CommandResult handleHybridExplainSymbol(const CommandContext& ctx);
CommandResult handleHybridAnnotateDiag(const CommandContext& ctx);
CommandResult handleHybridStreamAnalyze(const CommandContext& ctx);
CommandResult handleHybridSemanticPrefetch(const CommandContext& ctx);
CommandResult handleHybridCorrectionLoop(const CommandContext& ctx);

// ============================================================================
// MULTI-RESPONSE — CLI: !multi_*, GUI: IDM_MULTI_RESP_*
// ============================================================================

CommandResult handleMultiRespGenerate(const CommandContext& ctx);
CommandResult handleMultiRespSetMax(const CommandContext& ctx);
CommandResult handleMultiRespSelectPreferred(const CommandContext& ctx);
CommandResult handleMultiRespCompare(const CommandContext& ctx);
CommandResult handleMultiRespShowStats(const CommandContext& ctx);
CommandResult handleMultiRespShowTemplates(const CommandContext& ctx);
CommandResult handleMultiRespToggleTemplate(const CommandContext& ctx);
CommandResult handleMultiRespShowPrefs(const CommandContext& ctx);
CommandResult handleMultiRespShowLatest(const CommandContext& ctx);
CommandResult handleMultiRespShowStatus(const CommandContext& ctx);
CommandResult handleMultiRespClearHistory(const CommandContext& ctx);
CommandResult handleMultiRespApplyPreferred(const CommandContext& ctx);

// ============================================================================
// GOVERNOR — CLI: !gov_*, GUI: IDM_GOV_*
// ============================================================================

CommandResult handleGovStatus(const CommandContext& ctx);
CommandResult handleGovSubmitCommand(const CommandContext& ctx);
CommandResult handleGovKillAll(const CommandContext& ctx);
CommandResult handleGovTaskList(const CommandContext& ctx);

// ============================================================================
// SAFETY CONTRACTS — CLI: !safety_*, GUI: IDM_SAFETY_*
// ============================================================================

CommandResult handleSafetyStatus(const CommandContext& ctx);
CommandResult handleSafetyResetBudget(const CommandContext& ctx);
CommandResult handleSafetyRollbackLast(const CommandContext& ctx);
CommandResult handleSafetyShowViolations(const CommandContext& ctx);

// ============================================================================
// REPLAY JOURNAL — CLI: !replay_*, GUI: IDM_REPLAY_*
// ============================================================================

CommandResult handleReplayStatus(const CommandContext& ctx);
CommandResult handleReplayShowLast(const CommandContext& ctx);
CommandResult handleReplayExportSession(const CommandContext& ctx);
CommandResult handleReplayCheckpoint(const CommandContext& ctx);

// ============================================================================
// CONFIDENCE GATE — CLI: !confidence_*, GUI: IDM_CONFIDENCE_*
// ============================================================================

CommandResult handleConfidenceStatus(const CommandContext& ctx);
CommandResult handleConfidenceSetPolicy(const CommandContext& ctx);

// ============================================================================
// ROUTER EXTENDED — CLI: !router_*, GUI: IDM_ROUTER_*
// ============================================================================

CommandResult handleRouterEnable(const CommandContext& ctx);
CommandResult handleRouterDisable(const CommandContext& ctx);
CommandResult handleRouterStatus(const CommandContext& ctx);
CommandResult handleRouterDecision(const CommandContext& ctx);
CommandResult handleRouterSetPolicy(const CommandContext& ctx);
CommandResult handleRouterCapabilities(const CommandContext& ctx);
CommandResult handleRouterFallbacks(const CommandContext& ctx);
CommandResult handleRouterSaveConfig(const CommandContext& ctx);
CommandResult handleRouterRoutePrompt(const CommandContext& ctx);
CommandResult handleRouterResetStats(const CommandContext& ctx);
CommandResult handleRouterWhyBackend(const CommandContext& ctx);
CommandResult handleRouterPinTask(const CommandContext& ctx);
CommandResult handleRouterUnpinTask(const CommandContext& ctx);
CommandResult handleRouterShowPins(const CommandContext& ctx);
CommandResult handleRouterShowHeatmap(const CommandContext& ctx);
CommandResult handleRouterEnsembleEnable(const CommandContext& ctx);
CommandResult handleRouterEnsembleDisable(const CommandContext& ctx);
CommandResult handleRouterEnsembleStatus(const CommandContext& ctx);
CommandResult handleRouterSimulate(const CommandContext& ctx);
CommandResult handleRouterSimulateLast(const CommandContext& ctx);
CommandResult handleRouterShowCostStats(const CommandContext& ctx);

// ============================================================================
// BACKEND EXTENDED — CLI: !backend_*, GUI: IDM_BACKEND_*
// ============================================================================

CommandResult handleBackendSwitchLocal(const CommandContext& ctx);
CommandResult handleBackendSwitchOllama(const CommandContext& ctx);
CommandResult handleBackendSwitchOpenAI(const CommandContext& ctx);
CommandResult handleBackendSwitchClaude(const CommandContext& ctx);
CommandResult handleBackendSwitchGemini(const CommandContext& ctx);
CommandResult handleBackendShowStatus(const CommandContext& ctx);
CommandResult handleBackendShowSwitcher(const CommandContext& ctx);
CommandResult handleBackendConfigure(const CommandContext& ctx);
CommandResult handleBackendHealthCheck(const CommandContext& ctx);
CommandResult handleBackendSetApiKey(const CommandContext& ctx);
CommandResult handleBackendSaveConfigs(const CommandContext& ctx);

// ============================================================================
// DEBUG EXTENDED — CLI: !dbg_*, GUI: IDM_DBG_*
// ============================================================================

CommandResult handleDbgLaunch(const CommandContext& ctx);
CommandResult handleDbgAttach(const CommandContext& ctx);
CommandResult handleDbgDetach(const CommandContext& ctx);
CommandResult handleDbgGo(const CommandContext& ctx);
CommandResult handleDbgStepOver(const CommandContext& ctx);
CommandResult handleDbgStepInto(const CommandContext& ctx);
CommandResult handleDbgStepOut(const CommandContext& ctx);
CommandResult handleDbgBreak(const CommandContext& ctx);
CommandResult handleDbgKill(const CommandContext& ctx);
CommandResult handleDbgAddBp(const CommandContext& ctx);
CommandResult handleDbgRemoveBp(const CommandContext& ctx);
CommandResult handleDbgEnableBp(const CommandContext& ctx);
CommandResult handleDbgClearBps(const CommandContext& ctx);
CommandResult handleDbgListBps(const CommandContext& ctx);
CommandResult handleDbgAddWatch(const CommandContext& ctx);
CommandResult handleDbgRemoveWatch(const CommandContext& ctx);
CommandResult handleDbgRegisters(const CommandContext& ctx);
CommandResult handleDbgStack(const CommandContext& ctx);
CommandResult handleDbgMemory(const CommandContext& ctx);
CommandResult handleDbgDisasm(const CommandContext& ctx);
CommandResult handleDbgModules(const CommandContext& ctx);
CommandResult handleDbgThreads(const CommandContext& ctx);
CommandResult handleDbgSwitchThread(const CommandContext& ctx);
CommandResult handleDbgEvaluate(const CommandContext& ctx);
CommandResult handleDbgSetRegister(const CommandContext& ctx);
CommandResult handleDbgSearchMemory(const CommandContext& ctx);
CommandResult handleDbgSymbolPath(const CommandContext& ctx);
CommandResult handleDbgStatus(const CommandContext& ctx);

// ============================================================================
// PLUGIN SYSTEM — CLI: !plugin_*, GUI: IDM_PLUGIN_*
// ============================================================================

CommandResult handlePluginShowPanel(const CommandContext& ctx);
CommandResult handlePluginLoad(const CommandContext& ctx);
CommandResult handlePluginUnload(const CommandContext& ctx);
CommandResult handlePluginUnloadAll(const CommandContext& ctx);
CommandResult handlePluginRefresh(const CommandContext& ctx);
CommandResult handlePluginScanDir(const CommandContext& ctx);
CommandResult handlePluginShowStatus(const CommandContext& ctx);
CommandResult handlePluginToggleHotload(const CommandContext& ctx);
CommandResult handlePluginConfigure(const CommandContext& ctx);

// ============================================================================
// GAME ENGINE INTEGRATION — Unreal/Unity development
// ============================================================================

CommandResult handleUnrealInit(const CommandContext& ctx);
CommandResult handleUnrealAttach(const CommandContext& ctx);
CommandResult handleUnityInit(const CommandContext& ctx);
CommandResult handleUnityAttach(const CommandContext& ctx);

// ============================================================================
// REVERSE ENGINEERING — Binary analysis & patching
// ============================================================================

CommandResult handleRevengDisassemble(const CommandContext& ctx);
CommandResult handleRevengDecompile(const CommandContext& ctx);
CommandResult handleRevengFindVulnerabilities(const CommandContext& ctx);

// ============================================================================
// MODEL MANAGEMENT — Quantization, fine-tuning, LoRA
// ============================================================================

CommandResult handleModelList(const CommandContext& ctx);
CommandResult handleModelLoad(const CommandContext& ctx);
CommandResult handleModelQuantize(const CommandContext& ctx);
CommandResult handleModelFinetune(const CommandContext& ctx);
CommandResult handleModelUnload(const CommandContext& ctx);

// ============================================================================
// DISK RECOVERY — File recovery & partition analysis
// ============================================================================

CommandResult handleDiskListDrives(const CommandContext& ctx);
CommandResult handleDiskScanPartitions(const CommandContext& ctx);

// ============================================================================
// PERFORMANCE GOVERNANCE — CPU/memory/task management
// ============================================================================

CommandResult handleGovernorStatus(const CommandContext& ctx);
CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx);

// ============================================================================
// MARKETPLACE & EXTENSIONS — Extension management
// ============================================================================

CommandResult handleMarketplaceList(const CommandContext& ctx);
CommandResult handleMarketplaceInstall(const CommandContext& ctx);

// ============================================================================
// EMBEDDINGS — Vector encoding & search
// ============================================================================

CommandResult handleEmbeddingEncode(const CommandContext& ctx);

// ============================================================================
// VISION — Image analysis & understanding
// ============================================================================

CommandResult handleVisionAnalyzeImage(const CommandContext& ctx);

// ============================================================================
// PROMPT ENGINE — Context classification & generation
// ============================================================================

CommandResult handlePromptClassifyContext(const CommandContext& ctx);

// ============================================================================
// MANIFEST — Self-introspection
// ============================================================================

CommandResult handleManifestJSON(const CommandContext& ctx);
CommandResult handleManifestMarkdown(const CommandContext& ctx);
CommandResult handleManifestSelfTest(const CommandContext& ctx);

// ============================================================================
// CLI-ONLY COMMANDS — Legacy CLI features not exposed in GUI
// ============================================================================

CommandResult handleSearch(const CommandContext& ctx);
CommandResult handleAnalyze(const CommandContext& ctx);
CommandResult handleProfile(const CommandContext& ctx);
CommandResult handleSubAgent(const CommandContext& ctx);
CommandResult handleCOT(const CommandContext& ctx);
CommandResult handleStatus(const CommandContext& ctx);
CommandResult handleHelp(const CommandContext& ctx);
CommandResult handleGenerateIDE(const CommandContext& ctx);

#endif // RAWRXD_FEATURE_HANDLERS_H
