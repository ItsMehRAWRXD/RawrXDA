// ============================================================================
// ssot_handlers_ext.cpp — Stub Implementations for Extended COMMAND_TABLE
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// Stubs for commands added from ide_constants.h, DecompilerView,
// vscode_extension_api.h, and VoiceAutomation. Each delegates to
// Win32IDE via WM_COMMAND (GUI) or outputs a CLI status message.
//
// As subsystems implement real logic, move handlers out of this file.
// The linker enforces completeness: every handler in COMMAND_TABLE must
// resolve or the build fails.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "shared_feature_dispatch.h"
#include <windows.h>
#include <cstdio>

// ============================================================================
// HELPER: Route to Win32IDE via WM_COMMAND if in GUI mode
// (Same pattern as ssot_handlers.cpp — duplicated to avoid header coupling)
// ============================================================================

static CommandResult delegateToGui(const CommandContext& ctx, uint32_t cmdId, const char* name) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(name);
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "[SSOT] %s invoked via CLI\n", name);
    ctx.output(buf);
    return CommandResult::ok(name);
}

// ============================================================================
// FILE — IDE Core (ide_constants.h 105-110)
// ============================================================================

CommandResult handleFileAutoSave(const CommandContext& ctx)    { return delegateToGui(ctx, 105,  "file.autoSave"); }
CommandResult handleFileCloseFolder(const CommandContext& ctx) { return delegateToGui(ctx, 106,  "file.closeFolder"); }
CommandResult handleFileOpenFolder(const CommandContext& ctx)  { return delegateToGui(ctx, 108,  "file.openFolder"); }
CommandResult handleFileNewWindow(const CommandContext& ctx)   { return delegateToGui(ctx, 109,  "file.newWindow"); }
CommandResult handleFileCloseTab(const CommandContext& ctx)    { return delegateToGui(ctx, 110,  "file.closeTab"); }

// ============================================================================
// EDIT — IDE Core (ide_constants.h 208-211)
// ============================================================================

CommandResult handleEditMulticursorAdd(const CommandContext& ctx)    { return delegateToGui(ctx, 209, "edit.multicursorAdd"); }
CommandResult handleEditMulticursorRemove(const CommandContext& ctx) { return delegateToGui(ctx, 210, "edit.multicursorRemove"); }
CommandResult handleEditGotoLine(const CommandContext& ctx)          { return delegateToGui(ctx, 211, "edit.gotoLine"); }

// ============================================================================
// VIEW — IDE Core (ide_constants.h 301-307)
// ============================================================================

CommandResult handleViewToggleSidebar(const CommandContext& ctx)   { return delegateToGui(ctx, 301, "view.toggleSidebar"); }
CommandResult handleViewToggleTerminal(const CommandContext& ctx)  { return delegateToGui(ctx, 302, "view.toggleTerminal"); }
CommandResult handleViewToggleOutput(const CommandContext& ctx)    { return delegateToGui(ctx, 303, "view.toggleOutput"); }
CommandResult handleViewToggleFullscreen(const CommandContext& ctx){ return delegateToGui(ctx, 304, "view.toggleFullscreen"); }
CommandResult handleViewZoomIn(const CommandContext& ctx)          { return delegateToGui(ctx, 305, "view.zoomIn"); }
CommandResult handleViewZoomOut(const CommandContext& ctx)         { return delegateToGui(ctx, 306, "view.zoomOut"); }
CommandResult handleViewZoomReset(const CommandContext& ctx)       { return delegateToGui(ctx, 307, "view.zoomReset"); }

// ============================================================================
// AI FEATURES (ide_constants.h 401-409)
// ============================================================================

CommandResult handleAIInlineComplete(const CommandContext& ctx) { return delegateToGui(ctx, 401, "ai.inlineComplete"); }
CommandResult handleAIChatMode(const CommandContext& ctx)       { return delegateToGui(ctx, 402, "ai.chatMode"); }
CommandResult handleAIExplainCode(const CommandContext& ctx)    { return delegateToGui(ctx, 403, "ai.explainCode"); }
CommandResult handleAIRefactor(const CommandContext& ctx)       { return delegateToGui(ctx, 404, "ai.refactor"); }
CommandResult handleAIGenerateTests(const CommandContext& ctx)  { return delegateToGui(ctx, 405, "ai.generateTests"); }
CommandResult handleAIGenerateDocs(const CommandContext& ctx)   { return delegateToGui(ctx, 406, "ai.generateDocs"); }
CommandResult handleAIFixErrors(const CommandContext& ctx)      { return delegateToGui(ctx, 407, "ai.fixErrors"); }
CommandResult handleAIOptimizeCode(const CommandContext& ctx)   { return delegateToGui(ctx, 408, "ai.optimizeCode"); }
CommandResult handleAIModelSelect(const CommandContext& ctx)    { return delegateToGui(ctx, 409, "ai.modelSelect"); }

// ============================================================================
// TOOLS (ide_constants.h 501-506)
// ============================================================================

CommandResult handleToolsCommandPalette(const CommandContext& ctx) { return delegateToGui(ctx, 501, "tools.commandPalette"); }
CommandResult handleToolsSettings(const CommandContext& ctx)       { return delegateToGui(ctx, 502, "tools.settings"); }
CommandResult handleToolsExtensions(const CommandContext& ctx)     { return delegateToGui(ctx, 503, "tools.extensions"); }
CommandResult handleToolsTerminal(const CommandContext& ctx)       { return delegateToGui(ctx, 504, "tools.terminal"); }
CommandResult handleToolsBuild(const CommandContext& ctx)          { return delegateToGui(ctx, 505, "tools.build"); }
CommandResult handleToolsDebug(const CommandContext& ctx)          { return delegateToGui(ctx, 506, "tools.debug"); }

// NOTE: handleHelpDocs (601) and handleHelpShortcuts (603) are real implementations
// in feature_handlers.cpp — no stubs needed here.

// ============================================================================
// DECOMPILER CONTEXT MENU (8001-8006)
// ============================================================================

CommandResult handleDecompRenameVar(const CommandContext& ctx) { return delegateToGui(ctx, 8001, "decomp.renameVar"); }
CommandResult handleDecompGotoDef(const CommandContext& ctx)   { return delegateToGui(ctx, 8002, "decomp.gotoDef"); }
CommandResult handleDecompFindRefs(const CommandContext& ctx)  { return delegateToGui(ctx, 8003, "decomp.findRefs"); }
CommandResult handleDecompCopyLine(const CommandContext& ctx)  { return delegateToGui(ctx, 8004, "decomp.copyLine"); }
CommandResult handleDecompCopyAll(const CommandContext& ctx)   { return delegateToGui(ctx, 8005, "decomp.copyAll"); }
CommandResult handleDecompGotoAddr(const CommandContext& ctx)  { return delegateToGui(ctx, 8006, "decomp.gotoAddr"); }

// ============================================================================
// VSCODE EXTENSION API (10000-10009)
// ============================================================================

CommandResult handleVscExtStatus(const CommandContext& ctx)        { return delegateToGui(ctx, 10000, "vscext.status"); }
CommandResult handleVscExtReload(const CommandContext& ctx)        { return delegateToGui(ctx, 10001, "vscext.reload"); }
CommandResult handleVscExtListCommands(const CommandContext& ctx)  { return delegateToGui(ctx, 10002, "vscext.listCommands"); }
CommandResult handleVscExtListProviders(const CommandContext& ctx) { return delegateToGui(ctx, 10003, "vscext.listProviders"); }
CommandResult handleVscExtDiagnostics(const CommandContext& ctx)   { return delegateToGui(ctx, 10004, "vscext.diagnostics"); }
CommandResult handleVscExtExtensions(const CommandContext& ctx)    { return delegateToGui(ctx, 10005, "vscext.extensions"); }
CommandResult handleVscExtStats(const CommandContext& ctx)         { return delegateToGui(ctx, 10006, "vscext.stats"); }
CommandResult handleVscExtLoadNative(const CommandContext& ctx)    { return delegateToGui(ctx, 10007, "vscext.loadNative"); }
CommandResult handleVscExtDeactivateAll(const CommandContext& ctx) { return delegateToGui(ctx, 10008, "vscext.deactivateAll"); }
CommandResult handleVscExtExportConfig(const CommandContext& ctx)  { return delegateToGui(ctx, 10009, "vscext.exportConfig"); }

// ============================================================================
// VOICE AUTOMATION (10200-10206)
// ============================================================================

CommandResult handleVoiceAutoToggle(const CommandContext& ctx)    { return delegateToGui(ctx, 10200, "voice.autoToggle"); }
CommandResult handleVoiceAutoSettings(const CommandContext& ctx)  { return delegateToGui(ctx, 10201, "voice.autoSettings"); }
CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx) { return delegateToGui(ctx, 10202, "voice.autoNextVoice"); }
CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx) { return delegateToGui(ctx, 10203, "voice.autoPrevVoice"); }
CommandResult handleVoiceAutoRateUp(const CommandContext& ctx)    { return delegateToGui(ctx, 10204, "voice.autoRateUp"); }
CommandResult handleVoiceAutoRateDown(const CommandContext& ctx)  { return delegateToGui(ctx, 10205, "voice.autoRateDown"); }
CommandResult handleVoiceAutoStop(const CommandContext& ctx)      { return delegateToGui(ctx, 10206, "voice.autoStop"); }
