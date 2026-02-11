// ============================================================================
// ssot_handlers.h — Handler Declarations for SSOT-Bridged Commands
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// These handlers are new entries added by the SSOT command_registry.hpp
// that did not exist in the original feature_handlers.h.
// They bridge the 198 previously-unregistered GUI commands into the
// unified dispatch system.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_SSOT_HANDLERS_H
#define RAWRXD_SSOT_HANDLERS_H

#include "shared_feature_dispatch.h"

// ═══════════ FILE (extended) ═══════════
CommandResult handleFileRecentClear(const CommandContext& ctx);
CommandResult handleFileExit(const CommandContext& ctx);

// ═══════════ EDIT (extended) ═══════════
CommandResult handleEditSnippet(const CommandContext& ctx);
CommandResult handleEditCopyFormat(const CommandContext& ctx);
CommandResult handleEditPastePlain(const CommandContext& ctx);
CommandResult handleEditClipboardHist(const CommandContext& ctx);
CommandResult handleEditFindNext(const CommandContext& ctx);
CommandResult handleEditFindPrev(const CommandContext& ctx);

// ═══════════ VIEW ═══════════
CommandResult handleViewMinimap(const CommandContext& ctx);
CommandResult handleViewOutputTabs(const CommandContext& ctx);
CommandResult handleViewModuleBrowser(const CommandContext& ctx);
CommandResult handleViewThemeEditor(const CommandContext& ctx);
CommandResult handleViewFloatingPanel(const CommandContext& ctx);
CommandResult handleViewOutputPanel(const CommandContext& ctx);
CommandResult handleViewStreamingLoader(const CommandContext& ctx);
CommandResult handleViewVulkanRenderer(const CommandContext& ctx);
CommandResult handleViewSidebar(const CommandContext& ctx);
CommandResult handleViewTerminal(const CommandContext& ctx);

// ═══════════ THEMES (individual) ═══════════
CommandResult handleThemeLightPlus(const CommandContext& ctx);
CommandResult handleThemeMonokai(const CommandContext& ctx);
CommandResult handleThemeDracula(const CommandContext& ctx);
CommandResult handleThemeNord(const CommandContext& ctx);
CommandResult handleThemeSolDark(const CommandContext& ctx);
CommandResult handleThemeSolLight(const CommandContext& ctx);
CommandResult handleThemeCyberpunk(const CommandContext& ctx);
CommandResult handleThemeGruvbox(const CommandContext& ctx);
CommandResult handleThemeCatppuccin(const CommandContext& ctx);
CommandResult handleThemeTokyo(const CommandContext& ctx);
CommandResult handleThemeCrimson(const CommandContext& ctx);
CommandResult handleThemeHighContrast(const CommandContext& ctx);
CommandResult handleThemeOneDark(const CommandContext& ctx);
CommandResult handleThemeSynthwave(const CommandContext& ctx);
CommandResult handleThemeAbyss(const CommandContext& ctx);

// ═══════════ TRANSPARENCY ═══════════
CommandResult handleTrans100(const CommandContext& ctx);
CommandResult handleTrans90(const CommandContext& ctx);
CommandResult handleTrans80(const CommandContext& ctx);
CommandResult handleTrans70(const CommandContext& ctx);
CommandResult handleTrans60(const CommandContext& ctx);
CommandResult handleTrans50(const CommandContext& ctx);
CommandResult handleTrans40(const CommandContext& ctx);
CommandResult handleTransCustom(const CommandContext& ctx);
CommandResult handleTransToggle(const CommandContext& ctx);

// ═══════════ HELP (extended) ═══════════
CommandResult handleHelpCmdRef(const CommandContext& ctx);
CommandResult handleHelpPsDocs(const CommandContext& ctx);
CommandResult handleHelpSearch(const CommandContext& ctx);

// ═══════════ TERMINAL (extended) ═══════════
CommandResult handleTerminalSplitCode(const CommandContext& ctx);

// ═══════════ AUTONOMY (extended) ═══════════
CommandResult handleAutonomyStatus(const CommandContext& ctx);
CommandResult handleAutonomyMemory(const CommandContext& ctx);

// ═══════════ AI MODE / CONTEXT ═══════════
CommandResult handleAINoRefusal(const CommandContext& ctx);
CommandResult handleAICtx4K(const CommandContext& ctx);
CommandResult handleAICtx32K(const CommandContext& ctx);
CommandResult handleAICtx64K(const CommandContext& ctx);
CommandResult handleAICtx128K(const CommandContext& ctx);
CommandResult handleAICtx256K(const CommandContext& ctx);
CommandResult handleAICtx512K(const CommandContext& ctx);
CommandResult handleAICtx1M(const CommandContext& ctx);

// ═══════════ REVERSE ENGINEERING (extended) ═══════════
CommandResult handleRECompile(const CommandContext& ctx);
CommandResult handleRECompare(const CommandContext& ctx);
CommandResult handleREDetectVulns(const CommandContext& ctx);
CommandResult handleREExportIDA(const CommandContext& ctx);
CommandResult handleREExportGhidra(const CommandContext& ctx);
CommandResult handleREFunctions(const CommandContext& ctx);
CommandResult handleREDemangle(const CommandContext& ctx);
CommandResult handleRERecursiveDisasm(const CommandContext& ctx);
CommandResult handleRETypeRecovery(const CommandContext& ctx);
CommandResult handleREDataFlow(const CommandContext& ctx);
CommandResult handleRELicenseInfo(const CommandContext& ctx);
CommandResult handleREDecompilerView(const CommandContext& ctx);
CommandResult handleREDecompRename(const CommandContext& ctx);
CommandResult handleREDecompSync(const CommandContext& ctx);
CommandResult handleREDecompClose(const CommandContext& ctx);

// ═══════════ SWARM (extended) ═══════════
CommandResult handleSwarmStartLeader(const CommandContext& ctx);
CommandResult handleSwarmStartWorker(const CommandContext& ctx);
CommandResult handleSwarmStartHybrid(const CommandContext& ctx);
CommandResult handleSwarmRemoveNode(const CommandContext& ctx);
CommandResult handleSwarmBlacklist(const CommandContext& ctx);
CommandResult handleSwarmBuildSources(const CommandContext& ctx);
CommandResult handleSwarmBuildCmake(const CommandContext& ctx);
CommandResult handleSwarmStartBuild(const CommandContext& ctx);
CommandResult handleSwarmCancelBuild(const CommandContext& ctx);
CommandResult handleSwarmCacheStatus(const CommandContext& ctx);
CommandResult handleSwarmCacheClear(const CommandContext& ctx);
CommandResult handleSwarmConfig(const CommandContext& ctx);
CommandResult handleSwarmDiscovery(const CommandContext& ctx);
CommandResult handleSwarmTaskGraph(const CommandContext& ctx);
CommandResult handleSwarmEvents(const CommandContext& ctx);
CommandResult handleSwarmStats(const CommandContext& ctx);
CommandResult handleSwarmResetStats(const CommandContext& ctx);
CommandResult handleSwarmWorkerStatus(const CommandContext& ctx);
CommandResult handleSwarmWorkerConnect(const CommandContext& ctx);
CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx);
CommandResult handleSwarmFitness(const CommandContext& ctx);

// ═══════════ HOTPATCH (extended) ═══════════
CommandResult handleHotpatchMemRevert(const CommandContext& ctx);
CommandResult handleHotpatchByteSearch(const CommandContext& ctx);
CommandResult handleHotpatchServerRemove(const CommandContext& ctx);
CommandResult handleHotpatchProxyBias(const CommandContext& ctx);
CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx);
CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx);
CommandResult handleHotpatchProxyValidate(const CommandContext& ctx);
CommandResult handleHotpatchPresetSave(const CommandContext& ctx);
CommandResult handleHotpatchPresetLoad(const CommandContext& ctx);
CommandResult handleHotpatchEventLog(const CommandContext& ctx);
CommandResult handleHotpatchResetStats(const CommandContext& ctx);
CommandResult handleHotpatchToggleAll(const CommandContext& ctx);
CommandResult handleHotpatchProxyStats(const CommandContext& ctx);

// ═══════════ MONACO ═══════════
CommandResult handleMonacoToggle(const CommandContext& ctx);
CommandResult handleMonacoDevtools(const CommandContext& ctx);
CommandResult handleMonacoReload(const CommandContext& ctx);
CommandResult handleMonacoZoomIn(const CommandContext& ctx);
CommandResult handleMonacoZoomOut(const CommandContext& ctx);
CommandResult handleMonacoSyncTheme(const CommandContext& ctx);

// ═══════════ LSP SERVER ═══════════
CommandResult handleLspSrvStart(const CommandContext& ctx);
CommandResult handleLspSrvStop(const CommandContext& ctx);
CommandResult handleLspSrvStatus(const CommandContext& ctx);
CommandResult handleLspSrvReindex(const CommandContext& ctx);
CommandResult handleLspSrvStats(const CommandContext& ctx);
CommandResult handleLspSrvPublishDiag(const CommandContext& ctx);
CommandResult handleLspSrvConfig(const CommandContext& ctx);
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx);
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx);

// ═══════════ EDITOR ENGINE ═══════════
CommandResult handleEditorRichEdit(const CommandContext& ctx);
CommandResult handleEditorWebView2(const CommandContext& ctx);
CommandResult handleEditorMonacoCore(const CommandContext& ctx);
CommandResult handleEditorCycle(const CommandContext& ctx);
CommandResult handleEditorStatus(const CommandContext& ctx);

// ═══════════ PDB ═══════════
CommandResult handlePdbLoad(const CommandContext& ctx);
CommandResult handlePdbFetch(const CommandContext& ctx);
CommandResult handlePdbStatus(const CommandContext& ctx);
CommandResult handlePdbCacheClear(const CommandContext& ctx);
CommandResult handlePdbEnable(const CommandContext& ctx);
CommandResult handlePdbResolve(const CommandContext& ctx);
CommandResult handlePdbImports(const CommandContext& ctx);
CommandResult handlePdbExports(const CommandContext& ctx);
CommandResult handlePdbIatStatus(const CommandContext& ctx);

// ═══════════ AUDIT ═══════════
CommandResult handleAuditDashboard(const CommandContext& ctx);
CommandResult handleAuditRunFull(const CommandContext& ctx);
CommandResult handleAuditDetectStubs(const CommandContext& ctx);
CommandResult handleAuditCheckMenus(const CommandContext& ctx);
CommandResult handleAuditRunTests(const CommandContext& ctx);
CommandResult handleAuditExportReport(const CommandContext& ctx);
CommandResult handleAuditQuickStats(const CommandContext& ctx);

// ═══════════ GAUNTLET ═══════════
CommandResult handleGauntletRun(const CommandContext& ctx);
CommandResult handleGauntletExport(const CommandContext& ctx);

// ═══════════ VOICE (extended) ═══════════
CommandResult handleVoicePTT(const CommandContext& ctx);
CommandResult handleVoiceJoinRoom(const CommandContext& ctx);
CommandResult handleVoiceModeContinuous(const CommandContext& ctx);
CommandResult handleVoiceModeDisabled(const CommandContext& ctx);

// ═══════════ QW (Quality/Workflow) ═══════════
CommandResult handleQwShortcutEditor(const CommandContext& ctx);
CommandResult handleQwShortcutReset(const CommandContext& ctx);
CommandResult handleQwBackupCreate(const CommandContext& ctx);
CommandResult handleQwBackupRestore(const CommandContext& ctx);
CommandResult handleQwBackupAutoToggle(const CommandContext& ctx);
CommandResult handleQwBackupList(const CommandContext& ctx);
CommandResult handleQwBackupPrune(const CommandContext& ctx);
CommandResult handleQwAlertMonitor(const CommandContext& ctx);
CommandResult handleQwAlertHistory(const CommandContext& ctx);
CommandResult handleQwAlertDismiss(const CommandContext& ctx);
CommandResult handleQwAlertResourceStatus(const CommandContext& ctx);
CommandResult handleQwSloDashboard(const CommandContext& ctx);

// ═══════════ TELEMETRY ═══════════
CommandResult handleTelemetryToggle(const CommandContext& ctx);
CommandResult handleTelemetryExportJson(const CommandContext& ctx);
CommandResult handleTelemetryExportCsv(const CommandContext& ctx);
CommandResult handleTelemetryDashboard(const CommandContext& ctx);
CommandResult handleTelemetryClear(const CommandContext& ctx);
CommandResult handleTelemetrySnapshot(const CommandContext& ctx);

#endif // RAWRXD_SSOT_HANDLERS_H
