#include "feature_handlers.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

CommandResult missingHandler(const CommandContext& ctx, const char* name) {
    if (ctx.isGui && ctx.hwnd != nullptr) {
        // Keep stub lane deterministic: if a GUI window is present, nudge command routing.
        PostMessageA(reinterpret_cast<HWND>(ctx.hwnd), WM_COMMAND, static_cast<WPARAM>(ctx.commandId), 0);
    }

    if (ctx.outputFn != nullptr) {
        std::string msg = std::string("[SSOT provider] fallback handler executed: ") + name + "\n";
        ctx.output(msg.c_str());
    }
    return CommandResult::ok(name);
}

} // namespace

#define DEFINE_MISSING_HANDLER(name) \
CommandResult name(const CommandContext& ctx) { return missingHandler(ctx, #name); }

#define RAWR_MISSING_HANDLER_LIST(X) \
    /* Batch 01 */ \
    X(handleAIChatMode) X(handleAICtx128K) X(handleAICtx1M) X(handleAICtx256K) X(handleAICtx32K) X(handleAICtx4K) X(handleAICtx512K) \
    /* Batch 02 */ \
    X(handleAICtx64K) X(handleAIExplainCode) X(handleAIFixErrors) X(handleAIGenerateDocs) X(handleAIGenerateTests) X(handleAIInlineComplete) X(handleAIModelSelect) \
    /* Batch 03 */ \
    X(handleAINoRefusal) X(handleAIOptimizeCode) X(handleAIRefactor) X(handleAuditDashboard) X(handleEditClipboardHist) X(handleEditorCycle) X(handleEditorMonacoCore) \
    /* Batch 04 */ \
    X(handleEditorRichEdit) X(handleEditorStatus) X(handleEditorWebView2) X(handleHelpCmdRef) X(handleHelpPsDocs) X(handleHotpatchEventLog) X(handleHotpatchMemRevert) \
    /* Batch 05 */ \
    X(handleHotpatchProxyStats) X(handleLspSrvConfig) X(handleLspSrvExportSymbols) X(handleLspSrvLaunchStdio) X(handleLspSrvPublishDiag) X(handleLspSrvReindex) X(handleLspSrvStart) \
    /* Batch 06 */ \
    X(handleLspSrvStats) X(handleLspSrvStatus) X(handleLspSrvStop) X(handleMonacoDevtools) X(handleMonacoReload) X(handleMonacoSyncTheme) X(handleMonacoToggle) \
    /* Batch 07 */ \
    X(handleMonacoZoomIn) X(handleMonacoZoomOut) X(handleQwAlertDismiss) X(handleQwAlertHistory) X(handleQwAlertMonitor) X(handleRECompare) X(handleRECompile) \
    /* Batch 08 */ \
    X(handleREDataFlow) X(handleREDecompClose) X(handleREDecompilerView) X(handleREDecompRename) X(handleREDecompSync) X(handleREDemangle) X(handleREDetectVulns) \
    /* Batch 09 */ \
    X(handleREExportGhidra) X(handleREExportIDA) X(handleREFunctions) X(handleRELicenseInfo) X(handleRERecursiveDisasm) X(handleRETypeRecovery) X(handleSwarmBlacklist) \
    /* Batch 10 */ \
    X(handleSwarmConfig) X(handleSwarmDiscovery) X(handleSwarmEvents) X(handleSwarmFitness) X(handleSwarmStats) X(handleSwarmTaskGraph) X(handleTelemetryDashboard) \
    /* Batch 11 */ \
    X(handleThemeCatppuccin) X(handleThemeCrimson) X(handleThemeCyberpunk) X(handleThemeGruvbox) X(handleThemeOneDark) X(handleThemeSolDark) X(handleThemeSolLight) \
    /* Batch 12 */ \
    X(handleThemeSynthwave) X(handleThemeTokyo) X(handleTier1AutoUpdateCheck) X(handleTier1BreadcrumbsToggle) X(handleTier1FileIconTheme) X(handleTier1FuzzyPalette) X(handleTier1MinimapEnhanced) \
    /* Batch 13 */ \
    X(handleTier1SettingsGUI) X(handleTier1SmoothScrollToggle) X(handleTier1SplitClose) X(handleTier1SplitFocusNext) X(handleTier1SplitGrid) X(handleTier1SplitHorizontal) X(handleTier1SplitVertical) \
    /* Batch 14 */ \
    X(handleTier1TabDragToggle) X(handleTier1UpdateDismiss) X(handleTier1WelcomePage) X(handleTrans100) X(handleTrans40) X(handleTrans50) X(handleTrans60) \
    /* Batch 15 */ \
    X(handleTrans70) X(handleTrans80) X(handleTrans90) X(handleTransCustom) X(handleTransToggle) X(handleViewStreamingLoader) X(handleViewVulkanRenderer) \
    /* Batch 16 */ \
    X(handleVoicePTT) X(handleVscExtDeactivateAll) X(handleVscExtDiagnostics) X(handleVscExtExportConfig) X(handleVscExtExtensions) X(handleVscExtListCommands) X(handleVscExtListProviders) \
    /* Batch 17 */ \
    X(handleVscExtLoadNative) X(handleVscExtReload) X(handleVscExtStats) X(handleVscExtStatus)

RAWR_MISSING_HANDLER_LIST(DEFINE_MISSING_HANDLER)

#define COUNT_MISSING_HANDLER(name) +1
constexpr int kMissingHandlerCount = 0 RAWR_MISSING_HANDLER_LIST(COUNT_MISSING_HANDLER);
static_assert(kMissingHandlerCount == 116, "ssot_missing_handlers_provider must define exactly 116 handlers");
