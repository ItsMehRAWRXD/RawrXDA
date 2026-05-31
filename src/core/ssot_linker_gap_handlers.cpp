#include "feature_handlers.h"

#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {
CommandResult linkGapHandler(const CommandContext& ctx, const char* name) {
    if (ctx.isGui && ctx.hwnd != nullptr) {
        PostMessageA(reinterpret_cast<HWND>(ctx.hwnd), WM_COMMAND, static_cast<WPARAM>(ctx.commandId), 0);
    }
    if (ctx.outputFn != nullptr) {
        std::string msg = std::string("[SSOT link-gap] handler executed: ") + name + "\n";
        ctx.output(msg.c_str());
    }
    return CommandResult::ok(name);
}
} // namespace

// ============================================================================
// Audit handlers
// ============================================================================
CommandResult handleAuditCheckMenus(const CommandContext& ctx) { return linkGapHandler(ctx, "audit.checkMenus"); }
CommandResult handleAuditDetectStubs(const CommandContext& ctx) { return linkGapHandler(ctx, "audit.detectStubs"); }
CommandResult handleAuditExportReport(const CommandContext& ctx) { return linkGapHandler(ctx, "audit.exportReport"); }
CommandResult handleAuditQuickStats(const CommandContext& ctx) { return linkGapHandler(ctx, "audit.quickStats"); }
CommandResult handleAuditRunFull(const CommandContext& ctx) { return linkGapHandler(ctx, "audit.runFull"); }
CommandResult handleAuditRunTests(const CommandContext& ctx) { return linkGapHandler(ctx, "audit.runTests"); }

// ============================================================================
// Autonomy handlers
// ============================================================================
CommandResult handleAutonomyMemory(const CommandContext& ctx) { return linkGapHandler(ctx, "autonomy.memory"); }
CommandResult handleAutonomyStatus(const CommandContext& ctx) { return linkGapHandler(ctx, "autonomy.status"); }

// ============================================================================
// Decompiler handlers
// ============================================================================
CommandResult handleDecompCopyAll(const CommandContext& ctx) { return linkGapHandler(ctx, "decomp.copyAll"); }
CommandResult handleDecompCopyLine(const CommandContext& ctx) { return linkGapHandler(ctx, "decomp.copyLine"); }
CommandResult handleDecompFindRefs(const CommandContext& ctx) { return linkGapHandler(ctx, "decomp.findRefs"); }
CommandResult handleDecompGotoAddr(const CommandContext& ctx) { return linkGapHandler(ctx, "decomp.gotoAddr"); }
CommandResult handleDecompGotoDef(const CommandContext& ctx) { return linkGapHandler(ctx, "decomp.gotoDef"); }
CommandResult handleDecompRenameVar(const CommandContext& ctx) { return linkGapHandler(ctx, "decomp.renameVar"); }

// ============================================================================
// Edit handlers
// ============================================================================
CommandResult handleEditCopyFormat(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.copyFormat"); }
CommandResult handleEditFindNext(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.findNext"); }
CommandResult handleEditFindPrev(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.findPrev"); }
CommandResult handleEditGotoLine(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.gotoLine"); }
CommandResult handleEditMulticursorAdd(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.multicursorAdd"); }
CommandResult handleEditMulticursorRemove(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.multicursorRemove"); }
CommandResult handleEditPastePlain(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.pastePlain"); }
CommandResult handleEditSnippet(const CommandContext& ctx) { return linkGapHandler(ctx, "edit.snippet"); }

// ============================================================================
// File handlers
// ============================================================================
CommandResult handleFileCloseFolder(const CommandContext& ctx) { return linkGapHandler(ctx, "file.closeFolder"); }
CommandResult handleFileCloseTab(const CommandContext& ctx) { return linkGapHandler(ctx, "file.closeTab"); }
CommandResult handleFileExit(const CommandContext& ctx) { return linkGapHandler(ctx, "file.exit"); }
CommandResult handleFileNewWindow(const CommandContext& ctx) { return linkGapHandler(ctx, "file.newWindow"); }
CommandResult handleFileOpenFolder(const CommandContext& ctx) { return linkGapHandler(ctx, "file.openFolder"); }
CommandResult handleFileRecentClear(const CommandContext& ctx) { return linkGapHandler(ctx, "file.recentClear"); }

// ============================================================================
// Gauntlet handlers
// ============================================================================
CommandResult handleGauntletExport(const CommandContext& ctx) { return linkGapHandler(ctx, "gauntlet.export"); }
CommandResult handleGauntletRun(const CommandContext& ctx) { return linkGapHandler(ctx, "gauntlet.run"); }

// ============================================================================
// Help handlers
// ============================================================================
CommandResult handleHelpSearch(const CommandContext& ctx) { return linkGapHandler(ctx, "help.search"); }

// ============================================================================
// Hotpatch handlers
// ============================================================================
CommandResult handleHotpatchByteSearch(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.byteSearch"); }
CommandResult handleHotpatchPresetLoad(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.presetLoad"); }
CommandResult handleHotpatchPresetSave(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.presetSave"); }
CommandResult handleHotpatchProxyBias(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.proxyBias"); }
CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.proxyRewrite"); }
CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.proxyTerminate"); }
CommandResult handleHotpatchProxyValidate(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.proxyValidate"); }
CommandResult handleHotpatchResetStats(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.resetStats"); }
CommandResult handleHotpatchServerRemove(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.serverRemove"); }
CommandResult handleHotpatchToggleAll(const CommandContext& ctx) { return linkGapHandler(ctx, "hotpatch.toggleAll"); }

// ============================================================================
// PDB handlers
// ============================================================================
CommandResult handlePdbCacheClear(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.cacheClear"); }
CommandResult handlePdbEnable(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.enable"); }
CommandResult handlePdbExports(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.exports"); }
CommandResult handlePdbFetch(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.fetch"); }
CommandResult handlePdbIatStatus(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.iatStatus"); }
CommandResult handlePdbImports(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.imports"); }
CommandResult handlePdbLoad(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.load"); }
CommandResult handlePdbResolve(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.resolve"); }
CommandResult handlePdbStatus(const CommandContext& ctx) { return linkGapHandler(ctx, "pdb.status"); }

// ============================================================================
// QW (Quality/Watchdog) handlers
// ============================================================================
CommandResult handleQwAlertResourceStatus(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.alertResourceStatus"); }
CommandResult handleQwBackupAutoToggle(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.backupAutoToggle"); }
CommandResult handleQwBackupCreate(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.backupCreate"); }
CommandResult handleQwBackupList(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.backupList"); }
CommandResult handleQwBackupPrune(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.backupPrune"); }
CommandResult handleQwBackupRestore(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.backupRestore"); }
CommandResult handleQwShortcutEditor(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.shortcutEditor"); }
CommandResult handleQwShortcutReset(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.shortcutReset"); }
CommandResult handleQwSloDashboard(const CommandContext& ctx) { return linkGapHandler(ctx, "qw.sloDashboard"); }

// ============================================================================
// Swarm handlers
// ============================================================================
CommandResult handleSwarmBuildCmake(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.buildCmake"); }
CommandResult handleSwarmBuildSources(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.buildSources"); }
CommandResult handleSwarmCacheClear(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.cacheClear"); }
CommandResult handleSwarmCacheStatus(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.cacheStatus"); }
CommandResult handleSwarmCancelBuild(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.cancelBuild"); }
CommandResult handleSwarmRemoveNode(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.removeNode"); }
CommandResult handleSwarmResetStats(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.resetStats"); }
CommandResult handleSwarmStartBuild(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.startBuild"); }
CommandResult handleSwarmStartHybrid(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.startHybrid"); }
CommandResult handleSwarmStartLeader(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.startLeader"); }
CommandResult handleSwarmStartWorker(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.startWorker"); }
CommandResult handleSwarmWorkerConnect(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.workerConnect"); }
CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.workerDisconnect"); }
CommandResult handleSwarmWorkerStatus(const CommandContext& ctx) { return linkGapHandler(ctx, "swarm.workerStatus"); }

// ============================================================================
// Telemetry handlers
// ============================================================================
CommandResult handleTelemetryClear(const CommandContext& ctx) { return linkGapHandler(ctx, "telemetry.clear"); }
CommandResult handleTelemetryExportCsv(const CommandContext& ctx) { return linkGapHandler(ctx, "telemetry.exportCsv"); }
CommandResult handleTelemetryExportJson(const CommandContext& ctx) { return linkGapHandler(ctx, "telemetry.exportJson"); }
CommandResult handleTelemetrySnapshot(const CommandContext& ctx) { return linkGapHandler(ctx, "telemetry.snapshot"); }
CommandResult handleTelemetryToggle(const CommandContext& ctx) { return linkGapHandler(ctx, "telemetry.toggle"); }

// ============================================================================
// Terminal handlers
// ============================================================================
CommandResult handleTerminalSplitCode(const CommandContext& ctx) { return linkGapHandler(ctx, "terminal.splitCode"); }

// ============================================================================
// Theme handlers
// ============================================================================
CommandResult handleThemeAbyss(const CommandContext& ctx) { return linkGapHandler(ctx, "theme.abyss"); }
CommandResult handleThemeDracula(const CommandContext& ctx) { return linkGapHandler(ctx, "theme.dracula"); }
CommandResult handleThemeHighContrast(const CommandContext& ctx) { return linkGapHandler(ctx, "theme.highContrast"); }
CommandResult handleThemeLightPlus(const CommandContext& ctx) { return linkGapHandler(ctx, "theme.lightPlus"); }
CommandResult handleThemeMonokai(const CommandContext& ctx) { return linkGapHandler(ctx, "theme.monokai"); }
CommandResult handleThemeNord(const CommandContext& ctx) { return linkGapHandler(ctx, "theme.nord"); }

// ============================================================================
// Tools handlers
// ============================================================================
CommandResult handleToolsBuild(const CommandContext& ctx) { return linkGapHandler(ctx, "tools.build"); }
CommandResult handleToolsCommandPalette(const CommandContext& ctx) { return linkGapHandler(ctx, "tools.commandPalette"); }
CommandResult handleToolsDebug(const CommandContext& ctx) { return linkGapHandler(ctx, "tools.debug"); }
CommandResult handleToolsExtensions(const CommandContext& ctx) { return linkGapHandler(ctx, "tools.extensions"); }
CommandResult handleToolsSettings(const CommandContext& ctx) { return linkGapHandler(ctx, "tools.settings"); }
CommandResult handleToolsTerminal(const CommandContext& ctx) { return linkGapHandler(ctx, "tools.terminal"); }

// ============================================================================
// View handlers
// ============================================================================
CommandResult handleViewFloatingPanel(const CommandContext& ctx) { return linkGapHandler(ctx, "view.floatingPanel"); }
CommandResult handleViewMinimap(const CommandContext& ctx) { return linkGapHandler(ctx, "view.minimap"); }
CommandResult handleViewModuleBrowser(const CommandContext& ctx) { return linkGapHandler(ctx, "view.moduleBrowser"); }
CommandResult handleViewOutputPanel(const CommandContext& ctx) { return linkGapHandler(ctx, "view.outputPanel"); }
CommandResult handleViewOutputTabs(const CommandContext& ctx) { return linkGapHandler(ctx, "view.outputTabs"); }
CommandResult handleViewSidebar(const CommandContext& ctx) { return linkGapHandler(ctx, "view.sidebar"); }
CommandResult handleViewTerminal(const CommandContext& ctx) { return linkGapHandler(ctx, "view.terminal"); }
CommandResult handleViewThemeEditor(const CommandContext& ctx) { return linkGapHandler(ctx, "view.themeEditor"); }
CommandResult handleViewToggleFullscreen(const CommandContext& ctx) { return linkGapHandler(ctx, "view.toggleFullscreen"); }
CommandResult handleViewToggleOutput(const CommandContext& ctx) { return linkGapHandler(ctx, "view.toggleOutput"); }
CommandResult handleViewToggleSidebar(const CommandContext& ctx) { return linkGapHandler(ctx, "view.toggleSidebar"); }
CommandResult handleViewToggleTerminal(const CommandContext& ctx) { return linkGapHandler(ctx, "view.toggleTerminal"); }
CommandResult handleViewZoomIn(const CommandContext& ctx) { return linkGapHandler(ctx, "view.zoomIn"); }
CommandResult handleViewZoomOut(const CommandContext& ctx) { return linkGapHandler(ctx, "view.zoomOut"); }
CommandResult handleViewZoomReset(const CommandContext& ctx) { return linkGapHandler(ctx, "view.zoomReset"); }

// ============================================================================
// Voice handlers
// ============================================================================
CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoNextVoice"); }
CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoPrevVoice"); }
CommandResult handleVoiceAutoRateDown(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoRateDown"); }
CommandResult handleVoiceAutoRateUp(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoRateUp"); }
CommandResult handleVoiceAutoSettings(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoSettings"); }
CommandResult handleVoiceAutoStop(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoStop"); }
CommandResult handleVoiceAutoToggle(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.autoToggle"); }
CommandResult handleVoiceJoinRoom(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.joinRoom"); }
CommandResult handleVoiceModeContinuous(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.modeContinuous"); }
CommandResult handleVoiceModeDisabled(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.modeDisabled"); }
CommandResult handleVoiceModePtt(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.modePtt"); }
CommandResult handleVoicePtt(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.ptt"); }
CommandResult handleVoiceShowDevices(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.showDevices"); }
CommandResult handleVoiceTogglePanel(const CommandContext& ctx) { return linkGapHandler(ctx, "voice.togglePanel"); }
