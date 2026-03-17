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
// MANIFEST — Self-introspection
// ============================================================================

CommandResult handleManifestJSON(const CommandContext& ctx);
CommandResult handleManifestMarkdown(const CommandContext& ctx);
CommandResult handleManifestSelfTest(const CommandContext& ctx);

#endif // RAWRXD_FEATURE_HANDLERS_H
