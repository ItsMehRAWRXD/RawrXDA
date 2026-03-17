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
#include <windows.h>
#include <cstdio>

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
    // CLI: report invocation
    char buf[128];
    snprintf(buf, sizeof(buf), "[SSOT] %s invoked via CLI\n", name);
    ctx.output(buf);
    return CommandResult::ok(name);
}

// ============================================================================
// FILE (extended)
// ============================================================================

CommandResult handleFileRecentClear(const CommandContext& ctx) {
    return delegateToGui(ctx, 1020, "file.recentClear");
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
    return delegateToGui(ctx, 2012, "edit.snippet");
}

CommandResult handleEditCopyFormat(const CommandContext& ctx) {
    return delegateToGui(ctx, 2013, "edit.copyFormat");
}

CommandResult handleEditPastePlain(const CommandContext& ctx) {
    return delegateToGui(ctx, 2014, "edit.pastePlain");
}

CommandResult handleEditClipboardHist(const CommandContext& ctx) {
    return delegateToGui(ctx, 2015, "edit.clipboardHistory");
}

CommandResult handleEditFindNext(const CommandContext& ctx) {
    return delegateToGui(ctx, 2018, "edit.findNext");
}

CommandResult handleEditFindPrev(const CommandContext& ctx) {
    return delegateToGui(ctx, 2019, "edit.findPrev");
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
    return delegateToGui(ctx, themeId, name);
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

CommandResult handleHelpCmdRef(const CommandContext& ctx)  { return delegateToGui(ctx, 4002, "help.cmdref"); }
CommandResult handleHelpPsDocs(const CommandContext& ctx)  { return delegateToGui(ctx, 4003, "help.psdocs"); }
CommandResult handleHelpSearch(const CommandContext& ctx)  { return delegateToGui(ctx, 4004, "help.search"); }

// ============================================================================
// TERMINAL (extended)
// ============================================================================

CommandResult handleTerminalSplitCode(const CommandContext& ctx) { return delegateToGui(ctx, 4009, "terminal.splitCode"); }

// ============================================================================
// AUTONOMY (extended)
// ============================================================================

CommandResult handleAutonomyStatus(const CommandContext& ctx) { return delegateToGui(ctx, 4154, "autonomy.status"); }
CommandResult handleAutonomyMemory(const CommandContext& ctx) { return delegateToGui(ctx, 4155, "autonomy.memory"); }

// ============================================================================
// AI MODE / CONTEXT
// ============================================================================

CommandResult handleAINoRefusal(const CommandContext& ctx)  { return delegateToGui(ctx, 4203, "ai.noRefusal"); }
CommandResult handleAICtx4K(const CommandContext& ctx)     { return delegateToGui(ctx, 4210, "ai.context4k"); }
CommandResult handleAICtx32K(const CommandContext& ctx)    { return delegateToGui(ctx, 4211, "ai.context32k"); }
CommandResult handleAICtx64K(const CommandContext& ctx)    { return delegateToGui(ctx, 4212, "ai.context64k"); }
CommandResult handleAICtx128K(const CommandContext& ctx)   { return delegateToGui(ctx, 4213, "ai.context128k"); }
CommandResult handleAICtx256K(const CommandContext& ctx)   { return delegateToGui(ctx, 4214, "ai.context256k"); }
CommandResult handleAICtx512K(const CommandContext& ctx)   { return delegateToGui(ctx, 4215, "ai.context512k"); }
CommandResult handleAICtx1M(const CommandContext& ctx)     { return delegateToGui(ctx, 4216, "ai.context1m"); }

// ============================================================================
// REVERSE ENGINEERING (extended)
// ============================================================================

CommandResult handleRECompile(const CommandContext& ctx)        { return delegateToGui(ctx, 4303, "re.compile"); }
CommandResult handleRECompare(const CommandContext& ctx)        { return delegateToGui(ctx, 4304, "re.compare"); }
CommandResult handleREDetectVulns(const CommandContext& ctx)    { return delegateToGui(ctx, 4305, "re.detectVulns"); }
CommandResult handleREExportIDA(const CommandContext& ctx)      { return delegateToGui(ctx, 4306, "re.exportIDA"); }
CommandResult handleREExportGhidra(const CommandContext& ctx)   { return delegateToGui(ctx, 4307, "re.exportGhidra"); }
CommandResult handleREFunctions(const CommandContext& ctx)      { return delegateToGui(ctx, 4309, "re.functions"); }
CommandResult handleREDemangle(const CommandContext& ctx)       { return delegateToGui(ctx, 4310, "re.demangle"); }
CommandResult handleRERecursiveDisasm(const CommandContext& ctx){ return delegateToGui(ctx, 4312, "re.recursiveDisasm"); }
CommandResult handleRETypeRecovery(const CommandContext& ctx)   { return delegateToGui(ctx, 4313, "re.typeRecovery"); }
CommandResult handleREDataFlow(const CommandContext& ctx)       { return delegateToGui(ctx, 4314, "re.dataFlow"); }
CommandResult handleRELicenseInfo(const CommandContext& ctx)    { return delegateToGui(ctx, 4315, "re.licenseInfo"); }
CommandResult handleREDecompilerView(const CommandContext& ctx) { return delegateToGui(ctx, 4316, "re.decompilerView"); }
CommandResult handleREDecompRename(const CommandContext& ctx)   { return delegateToGui(ctx, 4317, "re.decompRename"); }
CommandResult handleREDecompSync(const CommandContext& ctx)     { return delegateToGui(ctx, 4318, "re.decompSync"); }
CommandResult handleREDecompClose(const CommandContext& ctx)    { return delegateToGui(ctx, 4319, "re.decompClose"); }

// ============================================================================
// SWARM (extended)
// ============================================================================

CommandResult handleSwarmStartLeader(const CommandContext& ctx)      { return delegateToGui(ctx, 5133, "swarm.startLeader"); }
CommandResult handleSwarmStartWorker(const CommandContext& ctx)      { return delegateToGui(ctx, 5134, "swarm.startWorker"); }
CommandResult handleSwarmStartHybrid(const CommandContext& ctx)      { return delegateToGui(ctx, 5135, "swarm.startHybrid"); }
CommandResult handleSwarmRemoveNode(const CommandContext& ctx)       { return delegateToGui(ctx, 5139, "swarm.removeNode"); }
CommandResult handleSwarmBlacklist(const CommandContext& ctx)        { return delegateToGui(ctx, 5140, "swarm.blacklistNode"); }
CommandResult handleSwarmBuildSources(const CommandContext& ctx)     { return delegateToGui(ctx, 5141, "swarm.buildSources"); }
CommandResult handleSwarmBuildCmake(const CommandContext& ctx)       { return delegateToGui(ctx, 5142, "swarm.buildCmake"); }
CommandResult handleSwarmStartBuild(const CommandContext& ctx)       { return delegateToGui(ctx, 5143, "swarm.startBuild"); }
CommandResult handleSwarmCancelBuild(const CommandContext& ctx)      { return delegateToGui(ctx, 5144, "swarm.cancelBuild"); }
CommandResult handleSwarmCacheStatus(const CommandContext& ctx)      { return delegateToGui(ctx, 5145, "swarm.cacheStatus"); }
CommandResult handleSwarmCacheClear(const CommandContext& ctx)       { return delegateToGui(ctx, 5146, "swarm.cacheClear"); }
CommandResult handleSwarmConfig(const CommandContext& ctx)           { return delegateToGui(ctx, 5147, "swarm.config"); }
CommandResult handleSwarmDiscovery(const CommandContext& ctx)        { return delegateToGui(ctx, 5148, "swarm.discovery"); }
CommandResult handleSwarmTaskGraph(const CommandContext& ctx)        { return delegateToGui(ctx, 5149, "swarm.taskGraph"); }
CommandResult handleSwarmEvents(const CommandContext& ctx)           { return delegateToGui(ctx, 5150, "swarm.events"); }
CommandResult handleSwarmStats(const CommandContext& ctx)            { return delegateToGui(ctx, 5151, "swarm.stats"); }
CommandResult handleSwarmResetStats(const CommandContext& ctx)       { return delegateToGui(ctx, 5152, "swarm.resetStats"); }
CommandResult handleSwarmWorkerStatus(const CommandContext& ctx)     { return delegateToGui(ctx, 5153, "swarm.workerStatus"); }
CommandResult handleSwarmWorkerConnect(const CommandContext& ctx)    { return delegateToGui(ctx, 5154, "swarm.workerConnect"); }
CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx) { return delegateToGui(ctx, 5155, "swarm.workerDisconnect"); }
CommandResult handleSwarmFitness(const CommandContext& ctx)          { return delegateToGui(ctx, 5156, "swarm.fitnessTest"); }

// ============================================================================
// HOTPATCH (extended)
// ============================================================================

CommandResult handleHotpatchMemRevert(const CommandContext& ctx)      { return delegateToGui(ctx, 9003, "hotpatch.memRevert"); }
CommandResult handleHotpatchByteSearch(const CommandContext& ctx)     { return delegateToGui(ctx, 9005, "hotpatch.byteSearch"); }
CommandResult handleHotpatchServerRemove(const CommandContext& ctx)   { return delegateToGui(ctx, 9007, "hotpatch.serverRemove"); }
CommandResult handleHotpatchProxyBias(const CommandContext& ctx)      { return delegateToGui(ctx, 9008, "hotpatch.proxyBias"); }
CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx)   { return delegateToGui(ctx, 9009, "hotpatch.proxyRewrite"); }
CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx) { return delegateToGui(ctx, 9010, "hotpatch.proxyTerminate"); }
CommandResult handleHotpatchProxyValidate(const CommandContext& ctx)  { return delegateToGui(ctx, 9011, "hotpatch.proxyValidate"); }
CommandResult handleHotpatchPresetSave(const CommandContext& ctx)     { return delegateToGui(ctx, 9012, "hotpatch.presetSave"); }
CommandResult handleHotpatchPresetLoad(const CommandContext& ctx)     { return delegateToGui(ctx, 9013, "hotpatch.presetLoad"); }
CommandResult handleHotpatchEventLog(const CommandContext& ctx)       { return delegateToGui(ctx, 9014, "hotpatch.eventLog"); }
CommandResult handleHotpatchResetStats(const CommandContext& ctx)     { return delegateToGui(ctx, 9015, "hotpatch.resetStats"); }
CommandResult handleHotpatchToggleAll(const CommandContext& ctx)      { return delegateToGui(ctx, 9016, "hotpatch.toggleAll"); }
CommandResult handleHotpatchProxyStats(const CommandContext& ctx)     { return delegateToGui(ctx, 9017, "hotpatch.proxyStats"); }

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

CommandResult handleLspSrvStart(const CommandContext& ctx)         { return delegateToGui(ctx, 9200, "lspServer.start"); }
CommandResult handleLspSrvStop(const CommandContext& ctx)          { return delegateToGui(ctx, 9201, "lspServer.stop"); }
CommandResult handleLspSrvStatus(const CommandContext& ctx)        { return delegateToGui(ctx, 9202, "lspServer.status"); }
CommandResult handleLspSrvReindex(const CommandContext& ctx)       { return delegateToGui(ctx, 9203, "lspServer.reindex"); }
CommandResult handleLspSrvStats(const CommandContext& ctx)         { return delegateToGui(ctx, 9204, "lspServer.stats"); }
CommandResult handleLspSrvPublishDiag(const CommandContext& ctx)   { return delegateToGui(ctx, 9205, "lspServer.publishDiag"); }
CommandResult handleLspSrvConfig(const CommandContext& ctx)        { return delegateToGui(ctx, 9206, "lspServer.config"); }
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx) { return delegateToGui(ctx, 9207, "lspServer.exportSymbols"); }
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx)   { return delegateToGui(ctx, 9208, "lspServer.launchStdio"); }

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

CommandResult handlePdbLoad(const CommandContext& ctx)       { return delegateToGui(ctx, 9400, "pdb.load"); }
CommandResult handlePdbFetch(const CommandContext& ctx)      { return delegateToGui(ctx, 9401, "pdb.fetch"); }
CommandResult handlePdbStatus(const CommandContext& ctx)     { return delegateToGui(ctx, 9402, "pdb.status"); }
CommandResult handlePdbCacheClear(const CommandContext& ctx) { return delegateToGui(ctx, 9403, "pdb.cacheClear"); }
CommandResult handlePdbEnable(const CommandContext& ctx)     { return delegateToGui(ctx, 9404, "pdb.enable"); }
CommandResult handlePdbResolve(const CommandContext& ctx)    { return delegateToGui(ctx, 9405, "pdb.resolve"); }
CommandResult handlePdbImports(const CommandContext& ctx)    { return delegateToGui(ctx, 9410, "pdb.imports"); }
CommandResult handlePdbExports(const CommandContext& ctx)    { return delegateToGui(ctx, 9411, "pdb.exports"); }
CommandResult handlePdbIatStatus(const CommandContext& ctx)  { return delegateToGui(ctx, 9412, "pdb.iatStatus"); }

// ============================================================================
// AUDIT
// ============================================================================

CommandResult handleAuditDashboard(const CommandContext& ctx)   { return delegateToGui(ctx, 9500, "audit.dashboard"); }
CommandResult handleAuditRunFull(const CommandContext& ctx)     { return delegateToGui(ctx, 9501, "audit.runFull"); }
CommandResult handleAuditDetectStubs(const CommandContext& ctx) { return delegateToGui(ctx, 9502, "audit.detectStubs"); }
CommandResult handleAuditCheckMenus(const CommandContext& ctx)  { return delegateToGui(ctx, 9503, "audit.checkMenus"); }
CommandResult handleAuditRunTests(const CommandContext& ctx)    { return delegateToGui(ctx, 9504, "audit.runTests"); }
CommandResult handleAuditExportReport(const CommandContext& ctx){ return delegateToGui(ctx, 9505, "audit.exportReport"); }
CommandResult handleAuditQuickStats(const CommandContext& ctx)  { return delegateToGui(ctx, 9506, "audit.quickStats"); }

// ============================================================================
// GAUNTLET
// ============================================================================

CommandResult handleGauntletRun(const CommandContext& ctx)    { return delegateToGui(ctx, 9600, "gauntlet.run"); }
CommandResult handleGauntletExport(const CommandContext& ctx) { return delegateToGui(ctx, 9601, "gauntlet.export"); }

// ============================================================================
// VOICE (extended)
// ============================================================================

CommandResult handleVoicePTT(const CommandContext& ctx)            { return delegateToGui(ctx, 9701, "voice.ptt"); }
CommandResult handleVoiceJoinRoom(const CommandContext& ctx)       { return delegateToGui(ctx, 9703, "voice.joinRoom"); }
CommandResult handleVoiceModeContinuous(const CommandContext& ctx) { return delegateToGui(ctx, 9708, "voice.modeContinuous"); }
CommandResult handleVoiceModeDisabled(const CommandContext& ctx)   { return delegateToGui(ctx, 9709, "voice.modeDisabled"); }

// ============================================================================
// QW (Quality/Workflow)
// ============================================================================

CommandResult handleQwShortcutEditor(const CommandContext& ctx)     { return delegateToGui(ctx, 9800, "qw.shortcutEditor"); }
CommandResult handleQwShortcutReset(const CommandContext& ctx)      { return delegateToGui(ctx, 9801, "qw.shortcutReset"); }
CommandResult handleQwBackupCreate(const CommandContext& ctx)       { return delegateToGui(ctx, 9810, "qw.backupCreate"); }
CommandResult handleQwBackupRestore(const CommandContext& ctx)      { return delegateToGui(ctx, 9811, "qw.backupRestore"); }
CommandResult handleQwBackupAutoToggle(const CommandContext& ctx)   { return delegateToGui(ctx, 9812, "qw.backupAutoToggle"); }
CommandResult handleQwBackupList(const CommandContext& ctx)         { return delegateToGui(ctx, 9813, "qw.backupList"); }
CommandResult handleQwBackupPrune(const CommandContext& ctx)        { return delegateToGui(ctx, 9814, "qw.backupPrune"); }
CommandResult handleQwAlertMonitor(const CommandContext& ctx)       { return delegateToGui(ctx, 9820, "qw.alertToggleMonitor"); }
CommandResult handleQwAlertHistory(const CommandContext& ctx)       { return delegateToGui(ctx, 9821, "qw.alertShowHistory"); }
CommandResult handleQwAlertDismiss(const CommandContext& ctx)       { return delegateToGui(ctx, 9822, "qw.alertDismissAll"); }
CommandResult handleQwAlertResourceStatus(const CommandContext& ctx){ return delegateToGui(ctx, 9823, "qw.alertResourceStatus"); }
CommandResult handleQwSloDashboard(const CommandContext& ctx)       { return delegateToGui(ctx, 9830, "qw.sloDashboard"); }

// ============================================================================
// TELEMETRY
// ============================================================================

CommandResult handleTelemetryToggle(const CommandContext& ctx)     { return delegateToGui(ctx, 9900, "telemetry.toggle"); }
CommandResult handleTelemetryExportJson(const CommandContext& ctx) { return delegateToGui(ctx, 9901, "telemetry.exportJson"); }
CommandResult handleTelemetryExportCsv(const CommandContext& ctx)  { return delegateToGui(ctx, 9902, "telemetry.exportCsv"); }
CommandResult handleTelemetryDashboard(const CommandContext& ctx)  { return delegateToGui(ctx, 9903, "telemetry.dashboard"); }
CommandResult handleTelemetryClear(const CommandContext& ctx)      { return delegateToGui(ctx, 9904, "telemetry.clear"); }
CommandResult handleTelemetrySnapshot(const CommandContext& ctx)   { return delegateToGui(ctx, 9905, "telemetry.snapshot"); }

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
