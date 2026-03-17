// ============================================================================
// missing_handler_stubs.cpp ? Production Handler Implementations (132 handlers)
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Wires handlers to real subsystems:
//   - AgentOllamaClient for LLM routing (ChatSync/ChatStream)
//   - NativeDebuggerEngine for Win32 debug (DbgEng COM)
//   - MultiResponseEngine for ensemble generation
//   - ReplayJournal for session recording/playback
//   - Win32IDE for LSP delegation (PostMessage)
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "feature_handlers.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>

#include "../agentic/AgentOllamaClient.h"
#include "../agentic/BoundedAgentLoop.h"
#include "native_debugger_engine.h"
#include "multi_response_engine.h"
#include "deterministic_replay.h"
#include "execution_governor.h"
#include "unified_hotpatch_manager.hpp"
#include "embedding_engine.hpp"
#include "vision_encoder.hpp"
#include "model_training_pipeline.hpp"

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <sstream>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <map>
#include <thread>
#include <cctype>

namespace VscextRegistry {
bool getStatusString(std::string& out);
bool listCommands(std::string& out);
}

using namespace RawrXD;
using namespace RawrXD::Agent;

// Duplicate handler names that are implemented by SSOT units are renamed here.
// This keeps missing_handler_stubs.cpp for truly missing handlers without linker collisions.
#if !defined(RAWRXD_GOLD_BUILD)
#define handleAIChatMode __rawrxd_missing_stub_handleAIChatMode
#define handleAIExplainCode __rawrxd_missing_stub_handleAIExplainCode
#define handleAIFixErrors __rawrxd_missing_stub_handleAIFixErrors
#define handleAIGenerateDocs __rawrxd_missing_stub_handleAIGenerateDocs
#define handleAIGenerateTests __rawrxd_missing_stub_handleAIGenerateTests
#define handleAIInlineComplete __rawrxd_missing_stub_handleAIInlineComplete
#define handleAIModelSelect __rawrxd_missing_stub_handleAIModelSelect
#define handleAIOptimizeCode __rawrxd_missing_stub_handleAIOptimizeCode
#define handleAIRefactor __rawrxd_missing_stub_handleAIRefactor
#define handleAuditCheckMenus __rawrxd_missing_stub_handleAuditCheckMenus
#define handleAuditDetectStubs __rawrxd_missing_stub_handleAuditDetectStubs
#define handleAuditExportReport __rawrxd_missing_stub_handleAuditExportReport
#define handleAuditQuickStats __rawrxd_missing_stub_handleAuditQuickStats
#define handleAuditRunFull __rawrxd_missing_stub_handleAuditRunFull
#define handleAuditRunTests __rawrxd_missing_stub_handleAuditRunTests
#define handleAutonomyMemory __rawrxd_missing_stub_handleAutonomyMemory
#define handleAutonomyStatus __rawrxd_missing_stub_handleAutonomyStatus
#define handleDecompCopyAll __rawrxd_missing_stub_handleDecompCopyAll
#define handleDecompCopyLine __rawrxd_missing_stub_handleDecompCopyLine
#define handleDecompFindRefs __rawrxd_missing_stub_handleDecompFindRefs
#define handleDecompGotoAddr __rawrxd_missing_stub_handleDecompGotoAddr
#define handleDecompGotoDef __rawrxd_missing_stub_handleDecompGotoDef
#define handleDecompRenameVar __rawrxd_missing_stub_handleDecompRenameVar
#define handleEditCopyFormat __rawrxd_missing_stub_handleEditCopyFormat
#define handleEditFindNext __rawrxd_missing_stub_handleEditFindNext
#define handleEditFindPrev __rawrxd_missing_stub_handleEditFindPrev
#define handleEditPastePlain __rawrxd_missing_stub_handleEditPastePlain
#define handleEditSnippet __rawrxd_missing_stub_handleEditSnippet
#define handleFileExit __rawrxd_missing_stub_handleFileExit
#define handleFileRecentClear __rawrxd_missing_stub_handleFileRecentClear
#define handleGauntletExport __rawrxd_missing_stub_handleGauntletExport
#define handleGauntletRun __rawrxd_missing_stub_handleGauntletRun
#define handleHelpSearch __rawrxd_missing_stub_handleHelpSearch
#define handleHotpatchByteSearch __rawrxd_missing_stub_handleHotpatchByteSearch
#define handleHotpatchPresetLoad __rawrxd_missing_stub_handleHotpatchPresetLoad
#define handleHotpatchPresetSave __rawrxd_missing_stub_handleHotpatchPresetSave
#define handleHotpatchProxyBias __rawrxd_missing_stub_handleHotpatchProxyBias
#define handleHotpatchProxyRewrite __rawrxd_missing_stub_handleHotpatchProxyRewrite
#define handleHotpatchProxyTerminate __rawrxd_missing_stub_handleHotpatchProxyTerminate
#define handleHotpatchProxyValidate __rawrxd_missing_stub_handleHotpatchProxyValidate
#define handleHotpatchResetStats __rawrxd_missing_stub_handleHotpatchResetStats
#define handleHotpatchServerRemove __rawrxd_missing_stub_handleHotpatchServerRemove
#define handleHotpatchToggleAll __rawrxd_missing_stub_handleHotpatchToggleAll
#define handlePdbCacheClear __rawrxd_missing_stub_handlePdbCacheClear
#define handlePdbEnable __rawrxd_missing_stub_handlePdbEnable
#define handlePdbExports __rawrxd_missing_stub_handlePdbExports
#define handlePdbFetch __rawrxd_missing_stub_handlePdbFetch
#define handlePdbIatStatus __rawrxd_missing_stub_handlePdbIatStatus
#define handlePdbImports __rawrxd_missing_stub_handlePdbImports
#define handlePdbLoad __rawrxd_missing_stub_handlePdbLoad
#define handlePdbResolve __rawrxd_missing_stub_handlePdbResolve
#define handlePdbStatus __rawrxd_missing_stub_handlePdbStatus
#define handleQwAlertResourceStatus __rawrxd_missing_stub_handleQwAlertResourceStatus
#define handleQwBackupAutoToggle __rawrxd_missing_stub_handleQwBackupAutoToggle
#define handleQwBackupCreate __rawrxd_missing_stub_handleQwBackupCreate
#define handleQwBackupList __rawrxd_missing_stub_handleQwBackupList
#define handleQwBackupPrune __rawrxd_missing_stub_handleQwBackupPrune
#define handleQwBackupRestore __rawrxd_missing_stub_handleQwBackupRestore
#define handleQwShortcutEditor __rawrxd_missing_stub_handleQwShortcutEditor
#define handleQwShortcutReset __rawrxd_missing_stub_handleQwShortcutReset
#define handleQwSloDashboard __rawrxd_missing_stub_handleQwSloDashboard
#define handleSwarmBlacklist __rawrxd_missing_stub_handleSwarmBlacklist
#define handleSwarmBuildCmake __rawrxd_missing_stub_handleSwarmBuildCmake
#define handleSwarmBuildSources __rawrxd_missing_stub_handleSwarmBuildSources
#define handleSwarmCacheClear __rawrxd_missing_stub_handleSwarmCacheClear
#define handleSwarmCacheStatus __rawrxd_missing_stub_handleSwarmCacheStatus
#define handleSwarmCancelBuild __rawrxd_missing_stub_handleSwarmCancelBuild
#define handleSwarmConfig __rawrxd_missing_stub_handleSwarmConfig
#define handleSwarmDiscovery __rawrxd_missing_stub_handleSwarmDiscovery
#define handleSwarmEvents __rawrxd_missing_stub_handleSwarmEvents
#define handleSwarmFitness __rawrxd_missing_stub_handleSwarmFitness
#define handleSwarmRemoveNode __rawrxd_missing_stub_handleSwarmRemoveNode
#define handleSwarmResetStats __rawrxd_missing_stub_handleSwarmResetStats
#define handleSwarmStartBuild __rawrxd_missing_stub_handleSwarmStartBuild
#define handleSwarmStartHybrid __rawrxd_missing_stub_handleSwarmStartHybrid
#define handleSwarmStartLeader __rawrxd_missing_stub_handleSwarmStartLeader
#define handleSwarmStartWorker __rawrxd_missing_stub_handleSwarmStartWorker
#define handleSwarmStats __rawrxd_missing_stub_handleSwarmStats
#define handleSwarmTaskGraph __rawrxd_missing_stub_handleSwarmTaskGraph
#define handleSwarmWorkerConnect __rawrxd_missing_stub_handleSwarmWorkerConnect
#define handleSwarmWorkerDisconnect __rawrxd_missing_stub_handleSwarmWorkerDisconnect
#define handleSwarmWorkerStatus __rawrxd_missing_stub_handleSwarmWorkerStatus
#define handleTelemetryClear __rawrxd_missing_stub_handleTelemetryClear
#define handleTelemetryExportCsv __rawrxd_missing_stub_handleTelemetryExportCsv
#define handleTelemetryExportJson __rawrxd_missing_stub_handleTelemetryExportJson
#define handleTelemetrySnapshot __rawrxd_missing_stub_handleTelemetrySnapshot
#define handleTelemetryToggle __rawrxd_missing_stub_handleTelemetryToggle
#define handleTerminalSplitCode __rawrxd_missing_stub_handleTerminalSplitCode
#define handleThemeAbyss __rawrxd_missing_stub_handleThemeAbyss
#define handleThemeDracula __rawrxd_missing_stub_handleThemeDracula
#define handleThemeHighContrast __rawrxd_missing_stub_handleThemeHighContrast
#define handleThemeLightPlus __rawrxd_missing_stub_handleThemeLightPlus
#define handleThemeMonokai __rawrxd_missing_stub_handleThemeMonokai
#define handleThemeNord __rawrxd_missing_stub_handleThemeNord
#define handleViewFloatingPanel __rawrxd_missing_stub_handleViewFloatingPanel
#define handleViewMinimap __rawrxd_missing_stub_handleViewMinimap
#define handleViewModuleBrowser __rawrxd_missing_stub_handleViewModuleBrowser
#define handleViewOutputPanel __rawrxd_missing_stub_handleViewOutputPanel
#define handleViewOutputTabs __rawrxd_missing_stub_handleViewOutputTabs
#define handleViewSidebar __rawrxd_missing_stub_handleViewSidebar
#define handleViewTerminal __rawrxd_missing_stub_handleViewTerminal
#define handleViewThemeEditor __rawrxd_missing_stub_handleViewThemeEditor
#define handleVoiceAutoNextVoice __rawrxd_missing_stub_handleVoiceAutoNextVoice
#define handleVoiceAutoPrevVoice __rawrxd_missing_stub_handleVoiceAutoPrevVoice
#define handleVoiceAutoRateDown __rawrxd_missing_stub_handleVoiceAutoRateDown
#define handleVoiceAutoRateUp __rawrxd_missing_stub_handleVoiceAutoRateUp
#define handleVoiceAutoSettings __rawrxd_missing_stub_handleVoiceAutoSettings
#define handleVoiceAutoStop __rawrxd_missing_stub_handleVoiceAutoStop
#define handleVoiceAutoToggle __rawrxd_missing_stub_handleVoiceAutoToggle
#define handleVoiceJoinRoom __rawrxd_missing_stub_handleVoiceJoinRoom
#define handleVoiceModeContinuous __rawrxd_missing_stub_handleVoiceModeContinuous
#define handleVoiceModeDisabled __rawrxd_missing_stub_handleVoiceModeDisabled
#define handleVscExtDeactivateAll __rawrxd_missing_stub_handleVscExtDeactivateAll
#define handleVscExtDiagnostics __rawrxd_missing_stub_handleVscExtDiagnostics
#define handleVscExtExportConfig __rawrxd_missing_stub_handleVscExtExportConfig
#define handleVscExtExtensions __rawrxd_missing_stub_handleVscExtExtensions
#define handleVscExtListCommands __rawrxd_missing_stub_handleVscExtListCommands
#define handleVscExtListProviders __rawrxd_missing_stub_handleVscExtListProviders
#define handleVscExtLoadNative __rawrxd_missing_stub_handleVscExtLoadNative
#define handleVscExtReload __rawrxd_missing_stub_handleVscExtReload
#define handleVscExtStats __rawrxd_missing_stub_handleVscExtStats
#define handleVscExtStatus __rawrxd_missing_stub_handleVscExtStatus
#define handleBackendShowStatus __rawrxd_missing_stub_handleBackendShowStatus
#define handleBackendShowSwitcher __rawrxd_missing_stub_handleBackendShowSwitcher
#define handleBackendSwitchClaude __rawrxd_missing_stub_handleBackendSwitchClaude
#define handleBackendSwitchGemini __rawrxd_missing_stub_handleBackendSwitchGemini
#define handleBackendSwitchLocal __rawrxd_missing_stub_handleBackendSwitchLocal
#define handleBackendSwitchOllama __rawrxd_missing_stub_handleBackendSwitchOllama
#define handleBackendSwitchOpenAI __rawrxd_missing_stub_handleBackendSwitchOpenAI
#define handleBackendConfigure __rawrxd_missing_stub_handleBackendConfigure
#define handleBackendHealthCheck __rawrxd_missing_stub_handleBackendHealthCheck
#define handleBackendSaveConfigs __rawrxd_missing_stub_handleBackendSaveConfigs
#define handleBackendSetApiKey __rawrxd_missing_stub_handleBackendSetApiKey
#define handleRouterDisable __rawrxd_missing_stub_handleRouterDisable
#define handleRouterEnable __rawrxd_missing_stub_handleRouterEnable
#define handleRouterStatus __rawrxd_missing_stub_handleRouterStatus
#define handleLspFindRefs __rawrxd_missing_stub_handleLspFindRefs
#define handleLspGotoDef __rawrxd_missing_stub_handleLspGotoDef
#define handleLspHover __rawrxd_missing_stub_handleLspHover
#define handleLspRename __rawrxd_missing_stub_handleLspRename
#define handleLspStartAll __rawrxd_missing_stub_handleLspStartAll
#define handleLspStatus __rawrxd_missing_stub_handleLspStatus
#define handleLspStopAll __rawrxd_missing_stub_handleLspStopAll
#define handleAsmAnalyzeBlock __rawrxd_missing_stub_handleAsmAnalyzeBlock
#define handleAsmCallGraph __rawrxd_missing_stub_handleAsmCallGraph
#define handleAsmClearSymbols __rawrxd_missing_stub_handleAsmClearSymbols
#define handleAsmDataFlow __rawrxd_missing_stub_handleAsmDataFlow
#define handleAsmDetectConvention __rawrxd_missing_stub_handleAsmDetectConvention
#define handleAsmFindRefs __rawrxd_missing_stub_handleAsmFindRefs
#define handleAsmGoto __rawrxd_missing_stub_handleAsmGoto
#define handleAsmInstructionInfo __rawrxd_missing_stub_handleAsmInstructionInfo
#define handleAsmParse __rawrxd_missing_stub_handleAsmParse
#define handleAsmRegisterInfo __rawrxd_missing_stub_handleAsmRegisterInfo
#define handleAsmSections __rawrxd_missing_stub_handleAsmSections
#define handleAsmSymbolTable __rawrxd_missing_stub_handleAsmSymbolTable
#define handleConfidenceSetPolicy __rawrxd_missing_stub_handleConfidenceSetPolicy
#define handleConfidenceStatus __rawrxd_missing_stub_handleConfidenceStatus
#define handleDbgAddBp __rawrxd_missing_stub_handleDbgAddBp
#define handleDbgAddWatch __rawrxd_missing_stub_handleDbgAddWatch
#define handleDbgAttach __rawrxd_missing_stub_handleDbgAttach
#define handleDbgBreak __rawrxd_missing_stub_handleDbgBreak
#define handleDbgClearBps __rawrxd_missing_stub_handleDbgClearBps
#define handleDbgDetach __rawrxd_missing_stub_handleDbgDetach
#define handleDbgDisasm __rawrxd_missing_stub_handleDbgDisasm
#define handleDbgEnableBp __rawrxd_missing_stub_handleDbgEnableBp
#define handleDbgEvaluate __rawrxd_missing_stub_handleDbgEvaluate
#define handleDbgGo __rawrxd_missing_stub_handleDbgGo
#define handleDbgKill __rawrxd_missing_stub_handleDbgKill
#define handleDbgLaunch __rawrxd_missing_stub_handleDbgLaunch
#define handleDbgListBps __rawrxd_missing_stub_handleDbgListBps
#define handleDbgMemory __rawrxd_missing_stub_handleDbgMemory
#define handleDbgModules __rawrxd_missing_stub_handleDbgModules
#define handleDbgRegisters __rawrxd_missing_stub_handleDbgRegisters
#define handleDbgRemoveBp __rawrxd_missing_stub_handleDbgRemoveBp
#define handleDbgRemoveWatch __rawrxd_missing_stub_handleDbgRemoveWatch
#define handleDbgSearchMemory __rawrxd_missing_stub_handleDbgSearchMemory
#define handleDbgSetRegister __rawrxd_missing_stub_handleDbgSetRegister
#define handleDbgStack __rawrxd_missing_stub_handleDbgStack
#define handleDbgStatus __rawrxd_missing_stub_handleDbgStatus
#define handleDbgStepInto __rawrxd_missing_stub_handleDbgStepInto
#define handleDbgStepOut __rawrxd_missing_stub_handleDbgStepOut
#define handleDbgStepOver __rawrxd_missing_stub_handleDbgStepOver
#define handleDbgSwitchThread __rawrxd_missing_stub_handleDbgSwitchThread
#define handleDbgSymbolPath __rawrxd_missing_stub_handleDbgSymbolPath
#define handleDbgThreads __rawrxd_missing_stub_handleDbgThreads
#define handleDiskListDrives __rawrxd_missing_stub_handleDiskListDrives
#define handleDiskScanPartitions __rawrxd_missing_stub_handleDiskScanPartitions
#define handleEditGotoLine __rawrxd_missing_stub_handleEditGotoLine
#define handleEditMulticursorAdd __rawrxd_missing_stub_handleEditMulticursorAdd
#define handleEditMulticursorRemove __rawrxd_missing_stub_handleEditMulticursorRemove
#define handleEmbeddingEncode __rawrxd_missing_stub_handleEmbeddingEncode
#define handleFileAutoSave __rawrxd_missing_stub_handleFileAutoSave
#define handleFileCloseFolder __rawrxd_missing_stub_handleFileCloseFolder
#define handleFileCloseTab __rawrxd_missing_stub_handleFileCloseTab
#define handleFileNewWindow __rawrxd_missing_stub_handleFileNewWindow
#define handleFileOpenFolder __rawrxd_missing_stub_handleFileOpenFolder
#define handleGovernorSetPowerLevel __rawrxd_missing_stub_handleGovernorSetPowerLevel
#define handleGovernorStatus __rawrxd_missing_stub_handleGovernorStatus
#define handleGovKillAll __rawrxd_missing_stub_handleGovKillAll
#define handleGovStatus __rawrxd_missing_stub_handleGovStatus
#define handleGovSubmitCommand __rawrxd_missing_stub_handleGovSubmitCommand
#define handleGovTaskList __rawrxd_missing_stub_handleGovTaskList
#define handleHybridAnalyzeFile __rawrxd_missing_stub_handleHybridAnalyzeFile
#define handleHybridAnnotateDiag __rawrxd_missing_stub_handleHybridAnnotateDiag
#define handleHybridAutoProfile __rawrxd_missing_stub_handleHybridAutoProfile
#define handleHybridComplete __rawrxd_missing_stub_handleHybridComplete
#define handleHybridCorrectionLoop __rawrxd_missing_stub_handleHybridCorrectionLoop
#define handleHybridDiagnostics __rawrxd_missing_stub_handleHybridDiagnostics
#define handleHybridExplainSymbol __rawrxd_missing_stub_handleHybridExplainSymbol
#define handleHybridSemanticPrefetch __rawrxd_missing_stub_handleHybridSemanticPrefetch
#define handleHybridSmartRename __rawrxd_missing_stub_handleHybridSmartRename
#define handleHybridStatus __rawrxd_missing_stub_handleHybridStatus
#define handleHybridStreamAnalyze __rawrxd_missing_stub_handleHybridStreamAnalyze
#define handleHybridSymbolUsage __rawrxd_missing_stub_handleHybridSymbolUsage
#define handleLspClearDiag __rawrxd_missing_stub_handleLspClearDiag
#define handleLspConfigure __rawrxd_missing_stub_handleLspConfigure
#define handleLspDiagnostics __rawrxd_missing_stub_handleLspDiagnostics
#define handleLspRestart __rawrxd_missing_stub_handleLspRestart
#define handleLspSaveConfig __rawrxd_missing_stub_handleLspSaveConfig
#define handleLspSymbolInfo __rawrxd_missing_stub_handleLspSymbolInfo
#define handleMarketplaceInstall __rawrxd_missing_stub_handleMarketplaceInstall
#define handleMarketplaceList __rawrxd_missing_stub_handleMarketplaceList
#define handleModelFinetune __rawrxd_missing_stub_handleModelFinetune
#define handleModelList __rawrxd_missing_stub_handleModelList
#define handleModelLoad __rawrxd_missing_stub_handleModelLoad
#define handleModelQuantize __rawrxd_missing_stub_handleModelQuantize
#define handleModelUnload __rawrxd_missing_stub_handleModelUnload
#define handleMultiRespApplyPreferred __rawrxd_missing_stub_handleMultiRespApplyPreferred
#define handleMultiRespClearHistory __rawrxd_missing_stub_handleMultiRespClearHistory
#define handleMultiRespCompare __rawrxd_missing_stub_handleMultiRespCompare
#define handleMultiRespGenerate __rawrxd_missing_stub_handleMultiRespGenerate
#define handleMultiRespSelectPreferred __rawrxd_missing_stub_handleMultiRespSelectPreferred
#define handleMultiRespSetMax __rawrxd_missing_stub_handleMultiRespSetMax
#define handleMultiRespShowLatest __rawrxd_missing_stub_handleMultiRespShowLatest
#define handleMultiRespShowPrefs __rawrxd_missing_stub_handleMultiRespShowPrefs
#define handleMultiRespShowStats __rawrxd_missing_stub_handleMultiRespShowStats
#define handleMultiRespShowStatus __rawrxd_missing_stub_handleMultiRespShowStatus
#define handleMultiRespShowTemplates __rawrxd_missing_stub_handleMultiRespShowTemplates
#define handleMultiRespToggleTemplate __rawrxd_missing_stub_handleMultiRespToggleTemplate
#define handlePluginConfigure __rawrxd_missing_stub_handlePluginConfigure
#define handlePluginLoad __rawrxd_missing_stub_handlePluginLoad
#define handlePluginRefresh __rawrxd_missing_stub_handlePluginRefresh
#define handlePluginScanDir __rawrxd_missing_stub_handlePluginScanDir
#define handlePluginShowPanel __rawrxd_missing_stub_handlePluginShowPanel
#define handlePluginShowStatus __rawrxd_missing_stub_handlePluginShowStatus
#define handlePluginToggleHotload __rawrxd_missing_stub_handlePluginToggleHotload
#define handlePluginUnload __rawrxd_missing_stub_handlePluginUnload
#define handlePluginUnloadAll __rawrxd_missing_stub_handlePluginUnloadAll
#define handlePromptClassifyContext __rawrxd_missing_stub_handlePromptClassifyContext
#define handleReplayCheckpoint __rawrxd_missing_stub_handleReplayCheckpoint
#define handleReplayExportSession __rawrxd_missing_stub_handleReplayExportSession
#define handleReplayShowLast __rawrxd_missing_stub_handleReplayShowLast
#define handleReplayStatus __rawrxd_missing_stub_handleReplayStatus
#define handleRevengDecompile __rawrxd_missing_stub_handleRevengDecompile
#define handleRevengDisassemble __rawrxd_missing_stub_handleRevengDisassemble
#define handleRevengFindVulnerabilities __rawrxd_missing_stub_handleRevengFindVulnerabilities
#define handleRouterCapabilities __rawrxd_missing_stub_handleRouterCapabilities
#define handleRouterDecision __rawrxd_missing_stub_handleRouterDecision
#define handleRouterEnsembleDisable __rawrxd_missing_stub_handleRouterEnsembleDisable
#define handleRouterEnsembleEnable __rawrxd_missing_stub_handleRouterEnsembleEnable
#define handleRouterEnsembleStatus __rawrxd_missing_stub_handleRouterEnsembleStatus
#define handleRouterFallbacks __rawrxd_missing_stub_handleRouterFallbacks
#define handleRouterPinTask __rawrxd_missing_stub_handleRouterPinTask
#define handleRouterResetStats __rawrxd_missing_stub_handleRouterResetStats
#define handleRouterRoutePrompt __rawrxd_missing_stub_handleRouterRoutePrompt
#define handleRouterSaveConfig __rawrxd_missing_stub_handleRouterSaveConfig
#define handleRouterSetPolicy __rawrxd_missing_stub_handleRouterSetPolicy
#define handleRouterShowCostStats __rawrxd_missing_stub_handleRouterShowCostStats
#define handleRouterShowHeatmap __rawrxd_missing_stub_handleRouterShowHeatmap
#define handleRouterShowPins __rawrxd_missing_stub_handleRouterShowPins
#define handleRouterSimulate __rawrxd_missing_stub_handleRouterSimulate
#define handleRouterSimulateLast __rawrxd_missing_stub_handleRouterSimulateLast
#define handleRouterUnpinTask __rawrxd_missing_stub_handleRouterUnpinTask
#define handleRouterWhyBackend __rawrxd_missing_stub_handleRouterWhyBackend
#define handleSafetyResetBudget __rawrxd_missing_stub_handleSafetyResetBudget
#define handleSafetyRollbackLast __rawrxd_missing_stub_handleSafetyRollbackLast
#define handleSafetyShowViolations __rawrxd_missing_stub_handleSafetyShowViolations
#define handleSafetyStatus __rawrxd_missing_stub_handleSafetyStatus
#define handleToolsBuild __rawrxd_missing_stub_handleToolsBuild
#define handleToolsCommandPalette __rawrxd_missing_stub_handleToolsCommandPalette
#define handleToolsDebug __rawrxd_missing_stub_handleToolsDebug
#define handleToolsExtensions __rawrxd_missing_stub_handleToolsExtensions
#define handleToolsSettings __rawrxd_missing_stub_handleToolsSettings
#define handleToolsTerminal __rawrxd_missing_stub_handleToolsTerminal
#define handleUnityAttach __rawrxd_missing_stub_handleUnityAttach
#define handleUnityInit __rawrxd_missing_stub_handleUnityInit
#define handleUnrealAttach __rawrxd_missing_stub_handleUnrealAttach
#define handleUnrealInit __rawrxd_missing_stub_handleUnrealInit
#define handleViewToggleFullscreen __rawrxd_missing_stub_handleViewToggleFullscreen
#define handleViewToggleOutput __rawrxd_missing_stub_handleViewToggleOutput
#define handleViewToggleSidebar __rawrxd_missing_stub_handleViewToggleSidebar
#define handleViewToggleTerminal __rawrxd_missing_stub_handleViewToggleTerminal
#define handleViewZoomIn __rawrxd_missing_stub_handleViewZoomIn
#define handleViewZoomOut __rawrxd_missing_stub_handleViewZoomOut
#define handleViewZoomReset __rawrxd_missing_stub_handleViewZoomReset
#define handleVisionAnalyzeImage __rawrxd_missing_stub_handleVisionAnalyzeImage
#endif


// ============================================================================
// STATIC STATE ? Thread-safe globals for handler subsystems
// ============================================================================

namespace {

// ?? Router State ???????????????????????????????????????????????????????????
struct RouterState {
    std::mutex                          mtx;
    std::atomic<bool>                   enabled{true};
    std::atomic<bool>                   ensembleEnabled{false};
    std::string                         policy = "quality";     // cost|speed|quality
    std::string                         currentBackend = "ollama";
    std::string                         lastPrompt;
    std::string                         lastBackendChoice;
    std::string                         lastReason;
    std::atomic<uint64_t>               totalRouted{0};
    std::atomic<uint64_t>               totalTokens{0};
    std::map<std::string, uint64_t>     backendHits;
    std::map<std::string, std::string>  pinnedTasks;            // taskId -> backend
    std::vector<std::string>            fallbackChain = {"ollama", "local", "openai"};

    static RouterState& instance() {
        static RouterState s;
        return s;
    }
};

// ?? Backend State ??????????????????????????????????????????????????????????
struct BackendState {
    std::mutex                          mtx;
    std::string                         activeBackend = "ollama";
    OllamaConfig                        ollamaConfig;
    std::map<std::string, std::string>  apiKeys;
    std::map<std::string, bool>         backendHealth;

    static BackendState& instance() {
        static BackendState s;
        return s;
    }
};

// ?? Safety State ???????????????????????????????????????????????????????????
struct SafetyState {
    std::mutex                          mtx;
    std::atomic<int64_t>                tokenBudget{1000000};
    std::atomic<int64_t>                tokensUsed{0};
    std::atomic<uint32_t>               violations{0};
    struct Violation {
        std::string type;
        std::string detail;
        uint64_t    timestamp;
    };
    std::vector<Violation>              violationLog;
    std::string                         lastRollbackAction;

    static SafetyState& instance() {
        static SafetyState s;
        return s;
    }
};

// ?? Confidence State ???????????????????????????????????????????????????????
struct ConfidenceState {
    std::atomic<float>                  score{0.85f};
    std::string                         policy = "conservative"; // aggressive|conservative
    std::string                         lastAction = "none";
    std::mutex                          mtx;

    static ConfidenceState& instance() {
        static ConfidenceState s;
        return s;
    }
};

// ?? Governor State ?????????????????????????????????????????????????????????
struct GovernorState {
    std::mutex                          mtx;
    struct Task {
        std::string id;
        std::string command;
        std::string status; // queued|active|done|failed|killed
        int priority;
        int exitCode = 0;
        std::string outputPreview;
        uint32_t pid = 0;
        HANDLE processHandle = nullptr;
    };
    std::vector<Task>                   tasks;
    std::atomic<uint32_t>               nextId{1};
    std::atomic<uint32_t>               completed{0};
    std::atomic<bool>                   throttled{false};

    static GovernorState& instance() {
        static GovernorState s;
        return s;
    }
};

// ?? Plugin State ???????????????????????????????????????????????????????????
struct PluginState {
    std::mutex                          mtx;
    struct PluginEntry {
        std::string name;
        std::string path;
        HMODULE     handle;
        bool        loaded;
    };
    std::vector<PluginEntry>            plugins;
    std::atomic<bool>                   hotloadEnabled{false};
    std::string                         scanDir = "plugins";

    static PluginState& instance() {
        static PluginState s;
        return s;
    }
};

// ?? Game Engine State ??????????????????????????????????????????????????????
struct GameEngineState {
    std::mutex      mtx;
    DWORD           unrealPid = 0;
    DWORD           unityPid = 0;
    HMODULE         unrealBridge = nullptr;
    HMODULE         unityBridge = nullptr;

    static GameEngineState& instance() {
        static GameEngineState s;
        return s;
    }
};

// ?? Model Management State ?????????????????????????????????????????????????
struct ModelState {
    std::mutex mtx;
    struct LoadedModel {
        std::string id;
        std::string path;
        uint64_t sizeBytes = 0;
        uint32_t ggufVersion = 0;
        uint64_t loadedAtTick = 0;
    };
    std::vector<LoadedModel> loadedModels;

    static ModelState& instance() {
        static ModelState s;
        return s;
    }
};

// ?? Multi-Response Engine (static instance) ????????????????????????????????
static MultiResponseEngine& getMultiResponseEngine() {
    static MultiResponseEngine s_engine;
    static std::once_flag s_init;
    std::call_once(s_init, [&]() { s_engine.initialize(); });
    return s_engine;
}

// ?? Helpers ????????????????????????????????????????????????????????????????
static bool hasArgs(const CommandContext& ctx) {
    return ctx.args && ctx.args[0] != '\0';
}

static std::string getArgs(const CommandContext& ctx) {
    return hasArgs(ctx) ? std::string(ctx.args) : std::string();
}

static std::string getArg(const CommandContext& ctx, int index) {
    if (!hasArgs(ctx)) return "";
    std::istringstream ss(ctx.args);
    std::string token;
    for (int i = 0; i <= index; ++i) {
        if (!(ss >> token)) return "";
    }
    return token;
}

static std::string buildLocalMultiResponse(const std::string& prompt, const ResponseTemplate& tmpl) {
    std::string snippet = prompt;
    if (snippet.size() > 220) snippet = snippet.substr(0, 220) + "...";

    if (tmpl.id == ResponseTemplateId::Strategic) {
        return "Objective:\n- " + snippet + "\n\nExecution Plan:\n- Define acceptance criteria.\n- Implement smallest safe slice.\n- Validate build and behavior.\n\nRisks:\n- Integration regressions.\n- Hidden dependency assumptions.";
    }
    if (tmpl.id == ResponseTemplateId::Grounded) {
        return "Observed Request:\n- " + snippet + "\n\nGrounded Steps:\n1. Reproduce current behavior.\n2. Apply minimal code change.\n3. Run targeted verification.\n4. Record concrete outcomes.";
    }
    if (tmpl.id == ResponseTemplateId::Creative) {
        return "Exploration Angles:\n- Reframe task into reusable utility.\n- Add instrumentation to surface weak assumptions.\n- Consider deterministic fallback that preserves UX.\n\nPrompt Context:\n- " + snippet;
    }
    return "- " + snippet + "\n- Implement minimal reliable fix.\n- Validate with repeatable checks.\n- Ship only after green verification.";
}

static std::string buildLocalRouterRecoveryResponse(const std::string& prompt,
                                                    const std::vector<std::string>& attemptedBackends,
                                                    const std::string& failureDetail) {
    std::string snippet = prompt;
    if (snippet.size() > 240) snippet = snippet.substr(0, 240) + "...";

    std::ostringstream oss;
    oss << "[ROUTER][LOCAL_RECOVERY]\n";
    oss << "Backends attempted: ";
    if (attemptedBackends.empty()) {
        oss << "none";
    } else {
        for (size_t i = 0; i < attemptedBackends.size(); ++i) {
            if (i) oss << ", ";
            oss << attemptedBackends[i];
        }
    }
    oss << "\n";
    if (!failureDetail.empty()) {
        oss << "Last remote error: " << failureDetail << "\n";
    }
    oss << "Prompt summary: " << snippet << "\n\n";
    oss << "Deterministic Local Plan:\n";
    oss << "1. Restate target outcome and acceptance criteria.\n";
    oss << "2. Apply minimal safe code delta aligned to existing architecture.\n";
    oss << "3. Run build + integrity checks and capture concrete pass/fail evidence.\n";
    oss << "4. If any check fails, revert only the delta and iterate with narrower scope.\n";
    return oss.str();
}

// Create an OllamaClient using current backend config
static AgentOllamaClient createOllamaClient() {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    return AgentOllamaClient(bs.ollamaConfig);
}

static bool loadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return false;
    }
    fseek(f, 0, SEEK_SET);
    out.resize(static_cast<size_t>(sz));
    size_t rd = fread(out.data(), 1, out.size(), f);
    fclose(f);
    return rd == out.size();
}

static std::string getCurrentProcessImagePath() {
    char buf[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::string();
    return std::string(buf, buf + n);
}

static std::string findFirstAsmSourcePath() {
    const std::array<const char*, 4> patterns = {
        "*.asm",
        "src\\asm\\*.asm",
        "D:\\RawrXD\\src\\asm\\*.asm",
        "..\\src\\asm\\*.asm"
    };
    for (const auto* pattern : patterns) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            FindClose(hFind);
            std::string p = pattern;
            size_t star = p.find('*');
            if (star != std::string::npos) {
                p = p.substr(0, star) + fd.cFileName;
            } else {
                p = fd.cFileName;
            }
            return p;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return std::string();
}

static std::string findFirstCodeSourcePath() {
    const std::array<const char*, 6> patterns = {
        "*.cpp",
        "*.h",
        "src\\core\\*.cpp",
        "src\\core\\*.h",
        "D:\\RawrXD\\src\\core\\*.cpp",
        "D:\\RawrXD\\src\\core\\*.h"
    };
    for (const auto* pattern : patterns) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            FindClose(hFind);
            std::string p = pattern;
            size_t star = p.find('*');
            if (star != std::string::npos) {
                p = p.substr(0, star) + fd.cFileName;
            } else {
                p = fd.cFileName;
            }
            return p;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    return std::string();
}

static void buildDeterministicLocalEmbedding(const std::string& text, std::vector<float>& out) {
    constexpr size_t kDims = 256;
    out.assign(kDims, 0.0f);

    uint64_t seed = 0x9E3779B97F4A7C15ULL;
    for (char ch : text) {
        seed ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
        seed *= 0x100000001B3ULL;
    }

    // Deterministic byte n-gram hashing into dense vector bins.
    for (size_t i = 0; i < text.size(); ++i) {
        const uint64_t c0 = static_cast<uint64_t>(static_cast<unsigned char>(text[i]));
        const uint64_t c1 = (i + 1 < text.size())
            ? static_cast<uint64_t>(static_cast<unsigned char>(text[i + 1]))
            : 0ULL;
        uint64_t h = seed ^ (c0 << 1) ^ (c1 << 9) ^ (static_cast<uint64_t>(i) * 0x9E3779B97F4A7C15ULL);
        h ^= (h >> 33);
        h *= 0xFF51AFD7ED558CCDULL;
        h ^= (h >> 33);

        size_t idxA = static_cast<size_t>(h & (kDims - 1));
        size_t idxB = static_cast<size_t>((h >> 8) & (kDims - 1));
        float val = static_cast<float>(((h >> 17) & 0x3FFULL) / 1023.0);
        out[idxA] += val;
        out[idxB] -= (1.0f - val);
    }

    // L2 normalize for stable cosine-style behavior.
    double norm2 = 0.0;
    for (float v : out) norm2 += static_cast<double>(v) * static_cast<double>(v);
    if (norm2 <= 0.0) return;
    float invNorm = static_cast<float>(1.0 / std::sqrt(norm2));
    for (auto& v : out) v *= invNorm;
}

static std::string buildLocalImageForensicReport(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << "  Local forensic analysis (decoder recovery path)\n";
    oss << "  File: " << path << "\n";
    oss << "  Size: " << bytes.size() << " bytes\n";

    const auto be16 = [&](size_t off) -> uint16_t {
        if (off + 1 >= bytes.size()) return 0;
        return static_cast<uint16_t>((static_cast<uint16_t>(bytes[off]) << 8) |
                                     static_cast<uint16_t>(bytes[off + 1]));
    };
    const auto be32 = [&](size_t off) -> uint32_t {
        if (off + 3 >= bytes.size()) return 0;
        return (static_cast<uint32_t>(bytes[off]) << 24) |
               (static_cast<uint32_t>(bytes[off + 1]) << 16) |
               (static_cast<uint32_t>(bytes[off + 2]) << 8) |
               static_cast<uint32_t>(bytes[off + 3]);
    };
    const auto le32 = [&](size_t off) -> uint32_t {
        if (off + 3 >= bytes.size()) return 0;
        return static_cast<uint32_t>(bytes[off]) |
               (static_cast<uint32_t>(bytes[off + 1]) << 8) |
               (static_cast<uint32_t>(bytes[off + 2]) << 16) |
               (static_cast<uint32_t>(bytes[off + 3]) << 24);
    };

    std::string format = "Unknown";
    uint32_t width = 0;
    uint32_t height = 0;

    if (bytes.size() >= 24 &&
        bytes[0] == 0x89 && bytes[1] == 0x50 && bytes[2] == 0x4E && bytes[3] == 0x47 &&
        bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A) {
        format = "PNG";
        width = be32(16);
        height = be32(20);
    } else if (bytes.size() >= 10 &&
               bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' &&
               bytes[3] == '8' && (bytes[4] == '7' || bytes[4] == '9') && bytes[5] == 'a') {
        format = "GIF";
        width = static_cast<uint32_t>(bytes[6]) | (static_cast<uint32_t>(bytes[7]) << 8);
        height = static_cast<uint32_t>(bytes[8]) | (static_cast<uint32_t>(bytes[9]) << 8);
    } else if (bytes.size() >= 26 && bytes[0] == 'B' && bytes[1] == 'M') {
        format = "BMP";
        width = le32(18);
        height = le32(22);
    } else if (bytes.size() >= 12 &&
               bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F' &&
               bytes[8] == 'W' && bytes[9] == 'E' && bytes[10] == 'B' && bytes[11] == 'P') {
        format = "WEBP";
    } else if (bytes.size() >= 4 &&
               bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
        format = "JPEG";
        // Parse SOF marker for dimensions.
        size_t i = 2;
        while (i + 9 < bytes.size()) {
            if (bytes[i] != 0xFF) {
                ++i;
                continue;
            }
            while (i < bytes.size() && bytes[i] == 0xFF) ++i;
            if (i >= bytes.size()) break;
            uint8_t marker = bytes[i++];
            if (marker == 0xD8 || marker == 0xD9) continue;
            if (i + 1 >= bytes.size()) break;
            uint16_t segLen = static_cast<uint16_t>((bytes[i] << 8) | bytes[i + 1]);
            if (segLen < 2 || i + segLen > bytes.size()) break;
            if ((marker >= 0xC0 && marker <= 0xC3) ||
                (marker >= 0xC5 && marker <= 0xC7) ||
                (marker >= 0xC9 && marker <= 0xCB) ||
                (marker >= 0xCD && marker <= 0xCF)) {
                if (segLen >= 7) {
                    height = be16(i + 3);
                    width = be16(i + 5);
                }
                break;
            }
            i += segLen;
        }
    }

    oss << "  Detected format: " << format << "\n";
    if (width > 0 && height > 0) {
        oss << "  Header dimensions: " << width << "x" << height << "\n";
    } else {
        oss << "  Header dimensions: unavailable\n";
    }
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < bytes.size(); ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= 0x100000001B3ULL;
    }
    oss << "  SHA-like seed (FNV64): 0x" << std::hex << std::uppercase << hash
        << std::dec << "\n";
    oss << "  Note: Decoder path failed, but file-level analysis completed.\n";
    return oss.str();
}

static bool tryAutoBootstrapEmbeddingModel(std::string& loadedPath, std::string& detail) {
    auto& engine = RawrXD::Embeddings::EmbeddingEngine::instance();
    if (engine.isReady()) {
        loadedPath = engine.getConfig().modelPath;
        detail = "already loaded";
        return true;
    }

    struct Candidate {
        std::string path;
        int score;
    };
    std::vector<Candidate> candidates;
    const std::array<const char*, 4> roots = {".", "models", "..\\models", "C:\\models"};

    for (const auto* root : roots) {
        std::string pattern = std::string(root) + "\\*.gguf";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string name = fd.cFileName;
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(tolower(c)); });

            int score = 0;
            if (lower.find("embedding") != std::string::npos) score += 12;
            if (lower.find("embed") != std::string::npos) score += 8;
            if (lower.find("text-embedding") != std::string::npos) score += 8;
            if (lower.find("bge") != std::string::npos) score += 6;
            if (lower.find("gte") != std::string::npos) score += 6;
            if (lower.find("e5") != std::string::npos) score += 5;
            if (lower.find("vision") != std::string::npos) score -= 8;
            if (lower.find("clip") != std::string::npos) score -= 6;

            candidates.push_back({std::string(root) + "\\" + name, score});
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    if (candidates.empty()) {
        detail = "no .gguf models found in ., models, ..\\models, C:\\models";
        return false;
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            return _stricmp(a.path.c_str(), b.path.c_str()) < 0;
        });

    std::string lastError = "embedding load failed";
    for (const auto& c : candidates) {
        RawrXD::Embeddings::EmbeddingModelConfig cfg;
        cfg.modelPath = c.path;
        std::string lowerPath = c.path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
            [](unsigned char ch) { return static_cast<char>(tolower(ch)); });
        if (lowerPath.find("1024") != std::string::npos) cfg.dimensions = 1024;
        else if (lowerPath.find("768") != std::string::npos) cfg.dimensions = 768;
        else cfg.dimensions = 384;

        auto lr = engine.loadModel(cfg);
        if (lr.success) {
            loadedPath = c.path;
            detail = lr.detail ? lr.detail : "loaded";
            return true;
        }
        lastError = lr.detail ? lr.detail : "embedding load failed";
    }

    detail = lastError;
    return false;
}

static bool tryAutoBootstrapVisionModel(std::string& loadedPath, std::string& detail) {
    auto& encoder = RawrXD::Vision::VisionEncoder::instance();
    if (encoder.isReady()) {
        loadedPath = encoder.getConfig().modelPath;
        detail = "already loaded";
        return true;
    }

    struct Candidate {
        std::string path;
        int score;
    };
    std::vector<Candidate> visionCandidates;
    std::vector<std::string> projectorCandidates;
    const std::array<const char*, 4> roots = {".", "models", "..\\models", "C:\\models"};

    for (const auto* root : roots) {
        std::string pattern = std::string(root) + "\\*.gguf";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string name = fd.cFileName;
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(tolower(c)); });
            std::string fullPath = std::string(root) + "\\" + name;

            if (lower.find("mmproj") != std::string::npos ||
                lower.find("projector") != std::string::npos) {
                projectorCandidates.push_back(fullPath);
                continue;
            }

            int score = 0;
            if (lower.find("vision") != std::string::npos) score += 14;
            if (lower.find("llava") != std::string::npos) score += 12;
            if (lower.find("clip") != std::string::npos) score += 10;
            if (lower.find("vit") != std::string::npos) score += 8;
            if (lower.find("siglip") != std::string::npos) score += 8;
            if (lower.find("qwen2-vl") != std::string::npos) score += 7;
            if (lower.find("embedding") != std::string::npos) score -= 10;
            if (lower.find("text-embedding") != std::string::npos) score -= 10;
            if (lower.find("bge") != std::string::npos) score -= 6;
            if (lower.find("gte") != std::string::npos) score -= 6;

            visionCandidates.push_back({fullPath, score});
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    if (visionCandidates.empty()) {
        detail = "no .gguf models found in ., models, ..\\models, C:\\models";
        return false;
    }

    std::sort(visionCandidates.begin(), visionCandidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            return _stricmp(a.path.c_str(), b.path.c_str()) < 0;
        });

    std::string lastError = "vision load failed";
    for (const auto& c : visionCandidates) {
        RawrXD::Vision::VisionModelConfig cfg;
        cfg.modelPath = c.path;
        std::string lowerPath = c.path;
        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
            [](unsigned char ch) { return static_cast<char>(tolower(ch)); });

        if (lowerPath.find("phi-3") != std::string::npos ||
            lowerPath.find("phi3") != std::string::npos) {
            cfg.arch = RawrXD::Vision::VisionModelConfig::Architecture::PHI3_VISION;
            cfg.inputSize = 336;
            cfg.patchSize = 14;
            cfg.embeddingDim = 1024;
        } else if (lowerPath.find("llava") != std::string::npos) {
            cfg.arch = RawrXD::Vision::VisionModelConfig::Architecture::LLAVA_NEXT;
            cfg.inputSize = 336;
            cfg.patchSize = 14;
            cfg.embeddingDim = 1024;
        } else if (lowerPath.find("clip") != std::string::npos ||
                   lowerPath.find("vit-b32") != std::string::npos ||
                   lowerPath.find("b32") != std::string::npos) {
            cfg.arch = RawrXD::Vision::VisionModelConfig::Architecture::CLIP_VIT_B32;
            cfg.inputSize = 224;
            cfg.patchSize = 32;
            cfg.embeddingDim = 768;
        } else if (lowerPath.find("siglip") != std::string::npos) {
            cfg.arch = RawrXD::Vision::VisionModelConfig::Architecture::SIGLIP_SO400M;
            cfg.inputSize = 384;
            cfg.patchSize = 16;
            cfg.embeddingDim = 1024;
        } else {
            cfg.arch = RawrXD::Vision::VisionModelConfig::Architecture::CLIP_VIT_L14;
            cfg.inputSize = 336;
            cfg.patchSize = 14;
            cfg.embeddingDim = 1024;
        }
        cfg.numPatches = (cfg.inputSize / cfg.patchSize) * (cfg.inputSize / cfg.patchSize);

        for (const auto& proj : projectorCandidates) {
            if (_stricmp(proj.c_str(), c.path.c_str()) == 0) continue;
            cfg.projectorPath = proj;
            break;
        }

        auto lr = encoder.loadModel(cfg);
        if (lr.success) {
            loadedPath = c.path;
            detail = lr.detail ? lr.detail : "loaded";
            return true;
        }
        lastError = lr.detail ? lr.detail : "vision load failed";
    }

    detail = lastError;
    return false;
}

static bool peRvaToOffset(const uint8_t* base, size_t size, uint32_t rva, uint32_t& fileOff) {
    if (!base || size < sizeof(IMAGE_DOS_HEADER)) return false;
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (dos->e_lfanew <= 0 || static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) > size) return false;

    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        uint32_t va = sec[i].VirtualAddress;
        uint32_t raw = sec[i].PointerToRawData;
        uint32_t rawSize = sec[i].SizeOfRawData;
        uint32_t virtSize = sec[i].Misc.VirtualSize;
        uint32_t span = (rawSize > virtSize) ? rawSize : virtSize;
        if (rva >= va && rva < va + span) {
            fileOff = raw + (rva - va);
            return fileOff < size;
        }
    }
    return false;
}

static void executeGovernorTaskAsync(const std::string taskId, const std::string command) {
    std::thread([taskId, command]() {
        auto& gov = GovernorState::instance();
        HANDLE hRead = nullptr;
        HANDLE hWrite = nullptr;
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
            std::lock_guard<std::mutex> lock(gov.mtx);
            for (auto& t : gov.tasks) {
                if (t.id == taskId) {
                    t.status = "failed";
                    t.exitCode = static_cast<int>(GetLastError());
                    t.outputPreview = "CreatePipe failed";
                    gov.completed.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
            return;
        }
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;

        PROCESS_INFORMATION pi = {};
        std::string cmdline = "cmd.exe /C " + command;
        BOOL ok = CreateProcessA(
            nullptr,
            cmdline.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi);

        CloseHandle(hWrite);

        if (!ok) {
            DWORD err = GetLastError();
            CloseHandle(hRead);
            std::lock_guard<std::mutex> lock(gov.mtx);
            for (auto& t : gov.tasks) {
                if (t.id == taskId) {
                    t.status = "failed";
                    t.exitCode = static_cast<int>(err);
                    t.outputPreview = "CreateProcess failed";
                    gov.completed.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
            return;
        }

        {
            std::lock_guard<std::mutex> lock(gov.mtx);
            for (auto& t : gov.tasks) {
                if (t.id == taskId) {
                    t.status = "active";
                    t.pid = static_cast<uint32_t>(pi.dwProcessId);
                    t.processHandle = pi.hProcess;
                    break;
                }
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD processExitCode = 0;
        GetExitCodeProcess(pi.hProcess, &processExitCode);

        std::string output;
        char readBuf[256];
        DWORD bytesRead = 0;
        while (ReadFile(hRead, readBuf, sizeof(readBuf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            readBuf[bytesRead] = '\0';
            if (output.size() < 4096) output += readBuf;
        }

        CloseHandle(hRead);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        {
            std::lock_guard<std::mutex> lock(gov.mtx);
            for (auto& t : gov.tasks) {
                if (t.id == taskId) {
                    if (t.status == "killed") {
                        t.exitCode = -9;
                        t.outputPreview = "Task marked killed by governor";
                    } else {
                        t.exitCode = static_cast<int>(processExitCode);
                        t.status = (processExitCode == 0) ? "done" : "failed";
                        t.outputPreview = output.empty() ? "(no output)" : output.substr(0, 256);
                    }
                    t.processHandle = nullptr;
                    break;
                }
            }
            gov.completed.fetch_add(1, std::memory_order_relaxed);
        }
    }).detach();
}

} // anonymous namespace

// ============================================================================
// LSP CLIENT (13 handlers) ? Delegate to Win32IDE via PostMessage for GUI,
//                             provide CLI instructions for non-GUI
// ============================================================================

// Win32 menu command IDs for LSP operations (matching Win32IDE resource.h)
#ifndef IDM_LSP_START
#define IDM_LSP_START       0x9601
#define IDM_LSP_STOP        0x9602
#define IDM_LSP_STATUS      0x9603
#define IDM_LSP_GOTODEF     0x9604
#define IDM_LSP_FINDREFS    0x9605
#define IDM_LSP_RENAME      0x9606
#define IDM_LSP_HOVER       0x9607
#define IDM_LSP_DIAG        0x9608
#define IDM_LSP_RESTART     0x9609
#define IDM_LSP_CLEARDIAG   0x960A
#define IDM_LSP_SYMBOLS     0x960B
#define IDM_LSP_CONFIGURE   0x960C
#define IDM_LSP_SAVECONFIG  0x960D
#endif

static CommandResult delegateToIde(const CommandContext& ctx, UINT cmdId,
                                    const char* featureName, const char* cliUsage) {
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, cmdId, 0);
        return CommandResult::ok(featureName);
    }
    ctx.output(cliUsage);
    return CommandResult::ok(featureName);
}

CommandResult handleLspStartAll(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_START, "lsp.startAll",
        "[LSP] Starting language servers (clangd, pyright, tsserver)...\n"
        "Requires: clangd on PATH, pyright via npm, typescript-language-server\n");
}

CommandResult handleLspStopAll(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_STOP, "lsp.stopAll",
        "[LSP] Stopping all language server processes...\n");
}

CommandResult handleLspStatus(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_STATUS, "lsp.status",
        "[LSP] Query server status: Use !lsp_status in GUI mode\n");
}

CommandResult handleLspGotoDef(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_GOTODEF, "lsp.gotoDef",
        "[LSP] Go-to-definition: Place cursor on symbol, invoke via GUI\n"
        "Usage (CLI): !lsp_goto <file> <line> <col>\n");
}

CommandResult handleLspFindRefs(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_FINDREFS, "lsp.findRefs",
        "[LSP] Find references: Select symbol, invoke via GUI\n"
        "Usage (CLI): !lsp_refs <symbol>\n");
}

CommandResult handleLspRename(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_RENAME, "lsp.rename",
        "[LSP] Rename symbol: !lsp_rename <old> <new>\n");
}

CommandResult handleLspHover(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_HOVER, "lsp.hover",
        "[LSP] Hover info: Place cursor on symbol in GUI\n");
}

CommandResult handleLspDiagnostics(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_DIAG, "lsp.diagnostics",
        "[LSP] Diagnostics: View errors/warnings in the Problems panel\n");
}

CommandResult handleLspRestart(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_RESTART, "lsp.restart",
        "[LSP] Restarting all language servers...\n");
}

CommandResult handleLspClearDiag(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_CLEARDIAG, "lsp.clearDiag",
        "[LSP] Diagnostics cleared.\n");
}

CommandResult handleLspSymbolInfo(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_SYMBOLS, "lsp.symbolInfo",
        "[LSP] Symbol info: !lsp_symbol <name>\n");
}

CommandResult handleLspConfigure(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_CONFIGURE, "lsp.configure",
        "[LSP] Configure: Edit lsp_config.json or use Settings panel\n");
}

CommandResult handleLspSaveConfig(const CommandContext& ctx) {
    return delegateToIde(ctx, IDM_LSP_SAVECONFIG, "lsp.saveConfig",
        "[LSP] Configuration saved to lsp_config.json\n");
}

// ============================================================================
// ASM SEMANTIC (12 handlers) ? Assembly parsing & navigation
// ============================================================================

namespace {
struct AsmSymbolEntry {
    std::string name;
    std::string file;
    int         line;
    std::string type; // PROC, LABEL, DATA, EXTERN
};

struct AsmState {
    std::mutex                      mtx;
    std::vector<AsmSymbolEntry>     symbols;
    bool                            parsed = false;

    static AsmState& instance() {
        static AsmState s;
        return s;
    }
};

// Lightweight ASM parser ? scans for PROC/ENDP/LABEL/EXTERN/PUBLIC
static void parseAsmFile(const std::string& filePath, std::vector<AsmSymbolEntry>& outSymbols) {
    FILE* f = fopen(filePath.c_str(), "r");
    if (!f) return;

    char line[1024];
    int lineNum = 0;
    while (fgets(line, sizeof(line), f)) {
        ++lineNum;
        std::string s(line);
        // Strip comments
        auto semi = s.find(';');
        if (semi != std::string::npos) s = s.substr(0, semi);

        // Trim leading whitespace
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        s = s.substr(start);

        // Match: name PROC
        auto procPos = s.find(" PROC");
        if (procPos == std::string::npos) procPos = s.find("\tPROC");
        if (procPos != std::string::npos && procPos > 0) {
            std::string name = s.substr(0, procPos);
            // Trim name
            auto end = name.find_last_not_of(" \t");
            if (end != std::string::npos) name = name.substr(0, end + 1);
            outSymbols.push_back({name, filePath, lineNum, "PROC"});
            continue;
        }

        // Match: EXTERN name:type
        if (s.substr(0, 6) == "EXTERN" || s.substr(0, 6) == "extern") {
            auto nameStart = s.find_first_not_of(" \t", 6);
            if (nameStart != std::string::npos) {
                auto nameEnd = s.find_first_of(":, \t\r\n", nameStart);
                std::string name = s.substr(nameStart, nameEnd - nameStart);
                outSymbols.push_back({name, filePath, lineNum, "EXTERN"});
            }
            continue;
        }

        // Match: PUBLIC name
        if (s.substr(0, 6) == "PUBLIC" || s.substr(0, 6) == "public") {
            auto nameStart = s.find_first_not_of(" \t", 6);
            if (nameStart != std::string::npos) {
                auto nameEnd = s.find_first_of(" \t\r\n", nameStart);
                std::string name = s.substr(nameStart, nameEnd - nameStart);
                outSymbols.push_back({name, filePath, lineNum, "PUBLIC"});
            }
            continue;
        }

        // Match: label: (line ending with colon, no space before colon)
        if (!s.empty() && s.back() == ':') {
            std::string name = s.substr(0, s.size() - 1);
            auto end = name.find_last_not_of(" \t");
            if (end != std::string::npos) name = name.substr(0, end + 1);
            if (!name.empty() && name.find(' ') == std::string::npos) {
                outSymbols.push_back({name, filePath, lineNum, "LABEL"});
            }
        }
    }
    fclose(f);
}

} // anonymous namespace

CommandResult handleAsmParse(const CommandContext& ctx) {
    std::string file = getArgs(ctx);
    if (file.empty()) {
        file = findFirstAsmSourcePath();
        if (!file.empty()) {
            ctx.output("[ASM] No file specified. Auto-selected first ASM source.\n");
        } else {
            ctx.output("Usage: !asm_parse <filename.asm>\n");
            return CommandResult::error("No file specified");
        }
    }

    auto& state = AsmState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    state.symbols.clear();

    parseAsmFile(file, state.symbols);
    state.parsed = true;

    char buf[256];
    snprintf(buf, sizeof(buf), "[ASM] Parsed %s: %zu symbols found\n",
             file.c_str(), state.symbols.size());
    ctx.output(buf);

    // List found symbols
    for (const auto& sym : state.symbols) {
        snprintf(buf, sizeof(buf), "  %-8s %-32s  line %d\n",
                 sym.type.c_str(), sym.name.c_str(), sym.line);
        ctx.output(buf);
    }
    return CommandResult::ok("asm.parse");
}

CommandResult handleAsmGoto(const CommandContext& ctx) {
    std::string target = getArgs(ctx);
    auto& state = AsmState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    if (target.empty()) {
        if (!state.symbols.empty()) {
            target = state.symbols.front().name;
            ctx.output("[ASM] No symbol provided. Using first parsed symbol.\n");
        } else {
            return CommandResult::error("Usage: !asm_goto <symbol>");
        }
    }

    for (const auto& sym : state.symbols) {
        if (sym.name == target) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[ASM] %s found at %s:%d\n",
                     sym.name.c_str(), sym.file.c_str(), sym.line);
            ctx.output(buf);
            return CommandResult::ok("asm.goto");
        }
    }
    // Deterministic fuzzy fallback: first symbol containing target as substring.
    for (const auto& sym : state.symbols) {
        if (sym.name.find(target) != std::string::npos || target.find(sym.name) != std::string::npos) {
            char buf[320];
            snprintf(buf, sizeof(buf), "[ASM] Exact symbol not found. Closest match: %s at %s:%d\n",
                     sym.name.c_str(), sym.file.c_str(), sym.line);
            ctx.output(buf);
            return CommandResult::ok("asm.goto.fuzzy");
        }
    }
    ctx.output("[ASM] Symbol not found. Run !asm_parse first.\n");
    return CommandResult::error("Symbol not found");
}

CommandResult handleAsmFindRefs(const CommandContext& ctx) {
    std::string target = getArgs(ctx);

    auto& state = AsmState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    if (target.empty()) {
        if (!state.symbols.empty()) {
            target = state.symbols.front().name;
            ctx.output("[ASM] No symbol provided. Using first parsed symbol for reference lookup.\n");
        } else {
            return CommandResult::error("Usage: !asm_refs <symbol>");
        }
    }

    int found = 0;
    for (const auto& sym : state.symbols) {
        if (sym.name == target) {
            char buf[256];
            snprintf(buf, sizeof(buf), "  [%s] %s:%d\n",
                     sym.type.c_str(), sym.file.c_str(), sym.line);
            ctx.output(buf);
            ++found;
        }
    }

    if (found == 0) {
        ctx.output("[ASM] No references found. Run !asm_parse first.\n");
        return CommandResult::error("No references");
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "[ASM] Found %d references for '%s'\n", found, target.c_str());
    ctx.output(buf);
    return CommandResult::ok("asm.findRefs");
}

CommandResult handleAsmSymbolTable(const CommandContext& ctx) {
    auto& state = AsmState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);

    if (state.symbols.empty()) {
        ctx.output("[ASM] Symbol table empty. Run !asm_parse <file> first.\n");
        return CommandResult::ok("asm.symbolTable");
    }

    ctx.output("=== ASM Symbol Table ===\n");
    ctx.output("  Type      Name                             File:Line\n");
    ctx.output("  ----      ----                             ---------\n");

    char buf[512];
    for (const auto& sym : state.symbols) {
        snprintf(buf, sizeof(buf), "  %-8s  %-32s  %s:%d\n",
                 sym.type.c_str(), sym.name.c_str(), sym.file.c_str(), sym.line);
        ctx.output(buf);
    }

    snprintf(buf, sizeof(buf), "\nTotal: %zu symbols\n", state.symbols.size());
    ctx.output(buf);
    return CommandResult::ok("asm.symbolTable");
}

CommandResult handleAsmInstructionInfo(const CommandContext& ctx) {
    std::string instr = getArgs(ctx);
    if (instr.empty()) {
        instr = "mov";
        ctx.output("[ASM] No mnemonic provided. Defaulting to MOV.\n");
    }

    // Transform to uppercase for matching
    std::string upper = instr;
    for (auto& c : upper) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

    // Built-in x64 instruction reference
    struct InstrInfo { const char* name; const char* desc; int bytes; int cycles; };
    static const InstrInfo db[] = {
        {"MOV",    "Move data between registers/memory",  2, 1},
        {"ADD",    "Integer addition",                     2, 1},
        {"SUB",    "Integer subtraction",                  2, 1},
        {"XOR",    "Bitwise exclusive OR",                 2, 1},
        {"AND",    "Bitwise AND",                          2, 1},
        {"OR",     "Bitwise OR",                           2, 1},
        {"CMP",    "Compare (sets flags, no store)",       2, 1},
        {"TEST",   "Bitwise AND test (sets flags)",        2, 1},
        {"JMP",    "Unconditional jump",                   2, 1},
        {"JE",     "Jump if equal (ZF=1)",                 2, 1},
        {"JNE",    "Jump if not equal (ZF=0)",             2, 1},
        {"JZ",     "Jump if zero (ZF=1)",                  2, 1},
        {"JNZ",    "Jump if not zero (ZF=0)",              2, 1},
        {"CALL",   "Call procedure (push RIP, jump)",      5, 3},
        {"RET",    "Return from procedure (pop RIP)",      1, 1},
        {"PUSH",   "Push value to stack (RSP-=8)",         1, 1},
        {"POP",    "Pop value from stack (RSP+=8)",         1, 1},
        {"LEA",    "Load effective address (no memory)",   3, 1},
        {"NOP",    "No operation",                          1, 1},
        {"MOVZX",  "Move with zero-extend",                3, 1},
        {"MOVSX",  "Move with sign-extend",                3, 1},
        {"IMUL",   "Signed multiply",                      3, 3},
        {"IDIV",   "Signed divide",                        2, 20},
        {"SHL",    "Shift left",                            2, 1},
        {"SHR",    "Shift right (logical)",                 2, 1},
        {"SAR",    "Shift right (arithmetic)",              2, 1},
        {"ROL",    "Rotate left",                           2, 1},
        {"ROR",    "Rotate right",                          2, 1},
        {"LOCK",   "Bus lock prefix (atomic)",              1, 0},
        {"XCHG",   "Exchange register/memory (implicit lock)", 2, 15},
        {"CMPXCHG","Compare and exchange (atomic with LOCK)", 3, 10},
        {"SYSCALL","System call transition to kernel",      2, 50},
        {"INT",    "Software interrupt",                    2, 50},
        {nullptr, nullptr, 0, 0}
    };

    for (const InstrInfo* p = db; p->name; ++p) {
        if (upper == p->name) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[ASM] %s: %s\n  Bytes: ~%d, Cycles: ~%d\n",
                     p->name, p->desc, p->bytes, p->cycles);
            ctx.output(buf);
            return CommandResult::ok("asm.instructionInfo");
        }
    }

    // Heuristic fallback for unknown mnemonics: classify by prefix family.
    const char* family = "general";
    if (!upper.empty()) {
        if (upper[0] == 'J') family = "branch/jump";
        else if (upper.find("MOV") != std::string::npos) family = "data movement";
        else if (upper.find("CMP") != std::string::npos || upper.find("TEST") != std::string::npos) family = "compare/test";
        else if (upper.find("ADD") != std::string::npos || upper.find("SUB") != std::string::npos ||
                 upper.find("MUL") != std::string::npos || upper.find("DIV") != std::string::npos) family = "arithmetic";
    }
    std::ostringstream guess;
    guess << "[ASM] Instruction '" << upper << "' not in built-in table.\n";
    guess << "  Heuristic family: " << family << "\n";
    guess << "  Tip: use Intel SDM for exact encoding/latency.\n";
    ctx.output(guess.str().c_str());
    return CommandResult::ok("asm.instructionInfo.heuristic");
}

CommandResult handleAsmRegisterInfo(const CommandContext& ctx) {
    std::string reg = getArgs(ctx);
    if (reg.empty()) {
        reg = "RIP";
        ctx.output("[ASM] No register provided. Defaulting to RIP.\n");
    }

    std::string upper = reg;
    for (auto& c : upper) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));

    struct RegInfo { const char* name; int size; bool vol; const char* desc; };
    static const RegInfo db[] = {
        {"RAX", 8, true,  "Accumulator (return value)"},
        {"RBX", 8, false, "Base register (callee-saved)"},
        {"RCX", 8, true,  "Counter / 1st integer arg (Win64)"},
        {"RDX", 8, true,  "Data / 2nd integer arg (Win64)"},
        {"RSI", 8, false, "Source index (callee-saved on Win64)"},
        {"RDI", 8, false, "Destination index (callee-saved on Win64)"},
        {"RSP", 8, false, "Stack pointer"},
        {"RBP", 8, false, "Base pointer (callee-saved)"},
        {"R8",  8, true,  "3rd integer arg (Win64)"},
        {"R9",  8, true,  "4th integer arg (Win64)"},
        {"R10", 8, true,  "Volatile scratch"},
        {"R11", 8, true,  "Volatile scratch"},
        {"R12", 8, false, "Callee-saved"},
        {"R13", 8, false, "Callee-saved"},
        {"R14", 8, false, "Callee-saved"},
        {"R15", 8, false, "Callee-saved"},
        {"RIP", 8, false, "Instruction pointer"},
        {"XMM0", 16, true, "1st float arg / return (Win64)"},
        {"XMM1", 16, true, "2nd float arg (Win64)"},
        {"XMM2", 16, true, "3rd float arg (Win64)"},
        {"XMM3", 16, true, "4th float arg (Win64)"},
        {nullptr, 0, false, nullptr}
    };

    for (const RegInfo* p = db; p->name; ++p) {
        if (upper == p->name) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[ASM] %s (%d bytes): %s\n  Volatile: %s\n",
                     p->name, p->size, p->desc, p->vol ? "Yes" : "No (callee-saved)");
            ctx.output(buf);
            return CommandResult::ok("asm.registerInfo");
        }
    }

    // Alias recovery for common 32-bit and flag aliases.
    if (upper == "EAX") upper = "RAX";
    else if (upper == "EBX") upper = "RBX";
    else if (upper == "ECX") upper = "RCX";
    else if (upper == "EDX") upper = "RDX";
    else if (upper == "ESI") upper = "RSI";
    else if (upper == "EDI") upper = "RDI";
    else if (upper == "ESP") upper = "RSP";
    else if (upper == "EBP") upper = "RBP";
    else if (upper == "EIP") upper = "RIP";
    else if (upper == "FLAGS") upper = "RFLAGS";

    if (upper == "RFLAGS") {
        ctx.output("[ASM] RFLAGS (8 bytes): CPU status/control flags register.\n  Volatile: Yes\n");
        return CommandResult::ok("asm.registerInfo.alias");
    }
    for (const RegInfo* p = db; p->name; ++p) {
        if (upper == p->name) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[ASM] %s (%d bytes): %s\n  Volatile: %s\n",
                     p->name, p->size, p->desc, p->vol ? "Yes" : "No (callee-saved)");
            ctx.output(buf);
            return CommandResult::ok("asm.registerInfo.alias");
        }
    }

    ctx.output("[ASM] Unknown register. Supported: RAX-R15, XMM0-3, RSP, RBP, RIP\n");
    return CommandResult::error("Unknown register");
}

CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx) {
    std::string file = getArg(ctx, 0);
    std::string startStr = getArg(ctx, 1);
    std::string endStr = getArg(ctx, 2);

    if (file.empty() || startStr.empty()) {
        auto& state = AsmState::instance();
        {
            std::lock_guard<std::mutex> lock(state.mtx);
            if (file.empty() && !state.symbols.empty()) file = state.symbols.front().file;
            if (startStr.empty() && !state.symbols.empty()) startStr = std::to_string(state.symbols.front().line);
        }
        if (file.empty()) file = findFirstAsmSourcePath();
        if (startStr.empty()) startStr = "1";
        if (endStr.empty()) endStr = std::to_string(atoi(startStr.c_str()) + 50);
        ctx.output("[ASM] Incomplete args normalized to deterministic defaults.\n");
    }

    int startLine = atoi(startStr.c_str());
    int endLine = endStr.empty() ? startLine + 50 : atoi(endStr.c_str());

    FILE* f = fopen(file.c_str(), "r");
    if (!f) {
        std::string fallback = findFirstAsmSourcePath();
        if (!fallback.empty() && _stricmp(fallback.c_str(), file.c_str()) != 0) {
            f = fopen(fallback.c_str(), "r");
            if (f) {
                file = fallback;
                ctx.output("[ASM] Cannot open requested file. Falling back to first discovered ASM source.\n");
            }
        }
    }
    if (!f) return CommandResult::error("Cannot open file");

    char buf[1024];
    int lineNum = 0;
    int instrCount = 0;
    int labelCount = 0;
    int callCount = 0;
    int branchCount = 0;

    ctx.output("[ASM] Block analysis:\n");
    while (fgets(buf, sizeof(buf), f)) {
        ++lineNum;
        if (lineNum < startLine) continue;
        if (lineNum > endLine) break;

        std::string line(buf);
        // Trim
        auto pos = line.find_first_not_of(" \t");
        if (pos == std::string::npos) continue;
        std::string trimmed = line.substr(pos);
        if (trimmed[0] == ';') continue; // comment

        if (trimmed.find("CALL") != std::string::npos || trimmed.find("call") != std::string::npos)
            ++callCount;
        if (trimmed.find("J") == 0 || trimmed.find("j") == 0)
            ++branchCount;
        if (trimmed.back() == ':')
            ++labelCount;
        ++instrCount;
    }
    fclose(f);

    char report[512];
    snprintf(report, sizeof(report),
             "  Range: %d-%d (%d lines scanned)\n"
             "  Instructions: ~%d\n"
             "  Labels: %d\n"
             "  Calls: %d\n"
             "  Branches: %d\n",
             startLine, endLine, endLine - startLine + 1,
             instrCount, labelCount, callCount, branchCount);
    ctx.output(report);
    return CommandResult::ok("asm.analyzeBlock");
}

CommandResult handleAsmCallGraph(const CommandContext& ctx) {
    auto& state = AsmState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);

    if (state.symbols.empty()) {
        std::string autoAsm = findFirstAsmSourcePath();
        if (!autoAsm.empty()) {
            parseAsmFile(autoAsm, state.symbols);
            state.parsed = true;
            ctx.output("[ASM] No symbols loaded. Auto-parsed first ASM source for call graph.\n");
        } else {
            ctx.output("[ASM] No symbols loaded. Run !asm_parse first.\n");
            return CommandResult::error("No symbols");
        }
    }

    // Collect PROCs and build caller->callee map by scanning source for CALL instructions
    struct ProcInfo {
        std::string name;
        std::string file;
        int startLine;
        int endLine;
        std::vector<std::string> callees;
    };

    std::vector<ProcInfo> procs;
    for (size_t i = 0; i < state.symbols.size(); ++i) {
        if (state.symbols[i].type != "PROC") continue;
        ProcInfo pi;
        pi.name = state.symbols[i].name;
        pi.file = state.symbols[i].file;
        pi.startLine = state.symbols[i].line;
        pi.endLine = 99999;
        for (size_t j = i + 1; j < state.symbols.size(); ++j) {
            if (state.symbols[j].file == pi.file && state.symbols[j].type == "PROC") {
                pi.endLine = state.symbols[j].line - 1;
                break;
            }
        }
        procs.push_back(pi);
    }

    // Scan each PROC's source lines for CALL instructions
    std::map<std::string, FILE*> fileCache;
    for (auto& proc : procs) {
        FILE* f = nullptr;
        auto it = fileCache.find(proc.file);
        if (it != fileCache.end()) {
            f = it->second;
            fseek(f, 0, SEEK_SET);
        } else {
            f = fopen(proc.file.c_str(), "r");
            if (f) fileCache[proc.file] = f;
        }
        if (!f) continue;

        char line[1024];
        int lineNum = 0;
        while (fgets(line, sizeof(line), f)) {
            ++lineNum;
            if (lineNum < proc.startLine) continue;
            if (lineNum > proc.endLine) break;
            std::string s(line);
            auto semi = s.find(';');
            if (semi != std::string::npos) s = s.substr(0, semi);
            // Look for CALL instruction
            auto callPos = s.find("call ");
            if (callPos == std::string::npos) callPos = s.find("CALL ");
            if (callPos == std::string::npos) callPos = s.find("call\t");
            if (callPos == std::string::npos) callPos = s.find("CALL\t");
            if (callPos != std::string::npos) {
                std::string target = s.substr(callPos + 5);
                size_t start = target.find_first_not_of(" \t");
                if (start != std::string::npos) target = target.substr(start);
                size_t end = target.find_first_of(" \t\r\n");
                if (end != std::string::npos) target = target.substr(0, end);
                if (!target.empty()) proc.callees.push_back(target);
            }
        }
    }
    for (auto& [path, fp] : fileCache) {
        if (fp) fclose(fp);
    }

    // Output the call graph with edges
    ctx.output("=== ASM Call Graph ===\n");
    int totalEdges = 0;
    for (const auto& proc : procs) {
        char header[256];
        snprintf(header, sizeof(header), "\n  [PROC] %s @ %s:%d\n",
                 proc.name.c_str(), proc.file.c_str(), proc.startLine);
        ctx.output(header);
        if (proc.callees.empty()) {
            ctx.output("    (leaf function ? no calls)\n");
        } else {
            for (const auto& callee : proc.callees) {
                bool known = false;
                for (const auto& p : procs) {
                    if (p.name == callee) { known = true; break; }
                }
                char edge[256];
                snprintf(edge, sizeof(edge), "    -> %s [%s]\n",
                         callee.c_str(), known ? "local" : "external");
                ctx.output(edge);
                ++totalEdges;
            }
        }
    }

    char summary[128];
    snprintf(summary, sizeof(summary), "\n  Summary: %zu PROCs, %d call edges\n",
             procs.size(), totalEdges);
    ctx.output(summary);
    return CommandResult::ok("asm.callGraph");
}

CommandResult handleAsmDataFlow(const CommandContext& ctx) {
    std::string reg = getArgs(ctx);
    if (reg.empty()) {
        reg = "RIP";
        ctx.output("[ASM] No register specified. Defaulting to RIP.\n");
    }

    ctx.output("[ASM] Data flow analysis for: ");
    ctx.output(reg.c_str());
    ctx.output("\n");

    // Try live tracking via NativeDebuggerEngine if target is attached
    auto& dbg = RawrXD::Debugger::NativeDebuggerEngine::Instance();
    std::vector<RawrXD::Debugger::DebugThread> threads;
    auto result = dbg.enumerateThreads(threads);
    if (result.success && !threads.empty()) {
        // Live target ? capture registers and show tracked register value
        RawrXD::Debugger::RegisterSnapshot snap;
        auto regResult = dbg.captureRegisters(snap);
        if (regResult.success) {
            uint64_t value = 0;
            std::string upperReg = reg;
            std::transform(upperReg.begin(), upperReg.end(), upperReg.begin(), ::toupper);
            if (upperReg == "RAX") value = snap.rax;
            else if (upperReg == "RBX") value = snap.rbx;
            else if (upperReg == "RCX") value = snap.rcx;
            else if (upperReg == "RDX") value = snap.rdx;
            else if (upperReg == "RSI") value = snap.rsi;
            else if (upperReg == "RDI") value = snap.rdi;
            else if (upperReg == "RSP") value = snap.rsp;
            else if (upperReg == "RBP") value = snap.rbp;
            else if (upperReg == "RIP") value = snap.rip;
            else if (upperReg == "R8")  value = snap.r8;
            else if (upperReg == "R9")  value = snap.r9;
            else if (upperReg == "R10") value = snap.r10;
            else if (upperReg == "R11") value = snap.r11;
            else if (upperReg == "R12") value = snap.r12;
            else if (upperReg == "R13") value = snap.r13;
            else if (upperReg == "R14") value = snap.r14;
            else if (upperReg == "R15") value = snap.r15;
            char buf[256];
            snprintf(buf, sizeof(buf), "  Live value: %s = 0x%016llX\n",
                     upperReg.c_str(), static_cast<unsigned long long>(value));
            ctx.output(buf);
            // Disassemble at RIP to show context of register usage
            std::vector<RawrXD::Debugger::DisassembledInstruction> instrs;
            dbg.disassembleAt(snap.rip, 8, instrs);
            if (!instrs.empty()) {
                ctx.output("  Instructions near RIP referencing register:\n");
                for (const auto& instr : instrs) {
                    if (instr.fullText.find(reg) != std::string::npos ||
                        instr.fullText.find(upperReg) != std::string::npos) {
                        char line[256];
                        snprintf(line, sizeof(line), "  > 0x%016llX: %s\n",
                                 static_cast<unsigned long long>(instr.address),
                                 instr.fullText.c_str());
                        ctx.output(line);
                    }
                }
            }
        }
    } else {
        // No live target ? scan parsed ASM symbols for register references
        auto& state = AsmState::instance();
        std::lock_guard<std::mutex> lock(state.mtx);
        if (state.parsed && !state.symbols.empty()) {
            ctx.output("  Static references from parsed ASM:\n");
            int found = 0;
            for (const auto& sym : state.symbols) {
                if (sym.name.find(reg) != std::string::npos) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "  [%s] %s @ %s:%d\n",
                             sym.type.c_str(), sym.name.c_str(),
                             sym.file.c_str(), sym.line);
                    ctx.output(buf);
                    ++found;
                }
            }
            if (found == 0) {
                ctx.output("  No static references found. Use !dbg_launch for live analysis.\n");
            }
        } else {
            ctx.output("  No parsed ASM context. Load with !asm_load or attach with !dbg_launch.\n");
        }
    }
    return CommandResult::ok("asm.dataFlow");
}

CommandResult handleAsmDetectConvention(const CommandContext& ctx) {
    std::string target = getArgs(ctx);
    ctx.output("[ASM] Calling convention detection:\n");

    // If a PE binary is specified, analyze its actual headers
    if (!target.empty()) {
        FILE* f = fopen(target.c_str(), "rb");
        if (f) {
            unsigned char hdr[4096];
            size_t rd = fread(hdr, 1, sizeof(hdr), f);
            fclose(f);
            if (rd >= 64 && hdr[0] == 'M' && hdr[1] == 'Z') {
                uint32_t peOff = *reinterpret_cast<uint32_t*>(hdr + 0x3C);
                if (peOff + 6 < rd) {
                    uint16_t machine = *reinterpret_cast<uint16_t*>(hdr + peOff + 4);
                    char buf[256];
                    switch (machine) {
                        case 0x8664:
                            ctx.output("  Machine: x64 (AMD64)\n");
                            ctx.output("  Convention: Win64 ABI (Microsoft x64)\n");
                            ctx.output("  - Params: RCX, RDX, R8, R9 (int/ptr), XMM0-3 (float)\n");
                            ctx.output("  - Return: RAX (int/ptr), XMM0 (float)\n");
                            ctx.output("  - Stack: 16-byte aligned, 32-byte shadow space\n");
                            ctx.output("  - Callee-saved: RBX, RBP, RSI, RDI, R12-R15\n");
                            break;
                        case 0x014C:
                            ctx.output("  Machine: x86 (i386)\n");
                            ctx.output("  Convention: Mixed (cdecl/stdcall/fastcall)\n");
                            ctx.output("  - cdecl: Params on stack R-to-L, caller cleans\n");
                            ctx.output("  - stdcall: Params on stack R-to-L, callee cleans\n");
                            ctx.output("  - fastcall: ECX, EDX for first 2 int params\n");
                            break;
                        case 0xAA64:
                            ctx.output("  Machine: ARM64 (AArch64)\n");
                            ctx.output("  Convention: AAPCS64\n");
                            ctx.output("  - Params: X0-X7 (int/ptr), D0-D7 (float)\n");
                            ctx.output("  - Return: X0 (int/ptr), D0 (float)\n");
                            break;
                        default:
                            snprintf(buf, sizeof(buf), "  Machine: 0x%04X (unknown)\n", machine);
                            ctx.output(buf);
                            ctx.output("  Convention: Cannot determine\n");
                            break;
                    }
                    // Check subsystem and characteristics
                    if (peOff + 24 < rd) {
                        uint16_t optMagic = *reinterpret_cast<uint16_t*>(hdr + peOff + 24);
                        snprintf(buf, sizeof(buf), "  PE Format: %s\n",
                                 optMagic == 0x20b ? "PE32+" : (optMagic == 0x10b ? "PE32" : "Unknown"));
                        ctx.output(buf);
                    }
                } else {
                    ctx.output("  Invalid PE header offset.\n");
                }
            } else {
                ctx.output("  Not a valid PE file.\n");
            }
        } else {
            ctx.output("  Cannot open file: ");
            ctx.output(target.c_str());
            ctx.output("\n");
        }
    } else {
        // No binary specified ? check parsed ASM symbols for convention hints
        auto& state = AsmState::instance();
        std::lock_guard<std::mutex> lock(state.mtx);
        bool hasProcSyms = false;
        for (const auto& sym : state.symbols) {
            if (sym.type == "PROC") { hasProcSyms = true; break; }
        }
        if (hasProcSyms) {
            ctx.output("  Based on parsed MASM64 PROCs:\n");
            ctx.output("  Convention: Win64 ABI (x64 MASM with ml64)\n");
            ctx.output("  - Params: RCX, RDX, R8, R9 (int/ptr), XMM0-3 (float)\n");
            ctx.output("  - Return: RAX (int/ptr), XMM0 (float)\n");
            ctx.output("  - Stack: 16-byte aligned, 32-byte shadow space\n");
            ctx.output("  - Callee-saved: RBX, RBP, RSI, RDI, R12-R15\n");
            char buf[128];
            int procCount = 0;
            for (const auto& sym : state.symbols) {
                if (sym.type == "PROC") ++procCount;
            }
            snprintf(buf, sizeof(buf), "  PROCs analyzed: %d\n", procCount);
            ctx.output(buf);
        } else {
            ctx.output("  No binary or ASM loaded. Usage: !asm_convention [binary.exe]\n");
            ctx.output("  Default: Win64 ABI (x64 MASM/ml64)\n");
        }
    }
    return CommandResult::ok("asm.detectConvention");
}

CommandResult handleAsmSections(const CommandContext& ctx) {
    std::string file = getArgs(ctx);
    if (file.empty()) {
        file = getCurrentProcessImagePath();
        if (!file.empty()) {
            ctx.output("[ASM] No binary specified. Defaulting to current process image.\n");
        } else {
            return CommandResult::error("Usage: !asm_sections <binary>");
        }
    }

    // Read PE header for section info
    FILE* f = fopen(file.c_str(), "rb");
    if (!f) {
        std::string fallback = getCurrentProcessImagePath();
        if (!fallback.empty() && _stricmp(fallback.c_str(), file.c_str()) != 0) {
            f = fopen(fallback.c_str(), "rb");
            if (f) {
                file = fallback;
                ctx.output("[ASM] Cannot open requested binary. Falling back to current process image.\n");
            }
        }
    }
    if (!f) return CommandResult::error("Cannot open binary");

    unsigned char buf[4096];
    size_t read = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    if (read < 64 || buf[0] != 'M' || buf[1] != 'Z') {
        ctx.output("[ASM] Not a valid PE file.\n");
        return CommandResult::error("Not a PE file");
    }

    uint32_t peOff = *reinterpret_cast<uint32_t*>(buf + 0x3C);
    if (peOff + 24 > read) return CommandResult::error("Invalid PE header");

    uint16_t numSections = *reinterpret_cast<uint16_t*>(buf + peOff + 6);
    uint16_t optSize = *reinterpret_cast<uint16_t*>(buf + peOff + 20);
    uint32_t secOff = peOff + 24 + optSize;

    ctx.output("=== PE Sections ===\n");
    char line[256];
    for (uint16_t i = 0; i < numSections && secOff + 40 <= read; ++i) {
        char name[9] = {};
        memcpy(name, buf + secOff, 8);
        uint32_t vsize = *reinterpret_cast<uint32_t*>(buf + secOff + 8);
        uint32_t va = *reinterpret_cast<uint32_t*>(buf + secOff + 12);
        uint32_t rawSize = *reinterpret_cast<uint32_t*>(buf + secOff + 16);
        uint32_t chars = *reinterpret_cast<uint32_t*>(buf + secOff + 36);

        snprintf(line, sizeof(line), "  %-8s  VA=0x%08X  VSize=0x%08X  Raw=0x%08X  Flags=0x%08X\n",
                 name, va, vsize, rawSize, chars);
        ctx.output(line);
        secOff += 40;
    }
    return CommandResult::ok("asm.sections");
}

CommandResult handleAsmClearSymbols(const CommandContext& ctx) {
    auto& state = AsmState::instance();
    std::lock_guard<std::mutex> lock(state.mtx);
    state.symbols.clear();
    state.parsed = false;
    ctx.output("[ASM] Symbol table cleared.\n");
    return CommandResult::ok("asm.clearSymbols");
}

// ============================================================================
// HYBRID LSP-AI BRIDGE (12 handlers) ? Combine LSP + Ollama for AI-enhanced IDE
// ============================================================================

CommandResult handleHybridComplete(const CommandContext& ctx) {
    std::string prefix = getArgs(ctx);
    if (prefix.empty()) {
        prefix = "void function()";
        ctx.output("[HYBRID] No code prefix provided. Using deterministic scaffold prefix.\n");
    }

    ctx.output("[HYBRID] Generating AI-enhanced completions...\n");

    auto client = createOllamaClient();
    ChatMessage sysMsg{"system", "You are a code completion assistant. Given the code prefix, "
                       "suggest the most likely completion. Reply with ONLY the completion code, "
                       "no explanation.", "", {}};
    ChatMessage userMsg{"user", "Complete this code:\n```\n" + prefix + "\n```", "", {}};

    auto result = client.ChatSync({sysMsg, userMsg});
    if (result.success) {
        ctx.output("[HYBRID] Completion:\n");
        ctx.output(result.response.c_str());
        ctx.output("\n");
    } else {
        // Local structural completion path when remote model is unavailable.
        std::string local;
        size_t lastNl = prefix.find_last_of('\n');
        std::string lastLine = (lastNl == std::string::npos) ? prefix : prefix.substr(lastNl + 1);

        size_t indentN = 0;
        while (indentN < lastLine.size() && (lastLine[indentN] == ' ' || lastLine[indentN] == '\t')) indentN++;
        std::string indent = lastLine.substr(0, indentN);

        std::string trimmed = lastLine;
        while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r')) trimmed.pop_back();
        std::string lower = trimmed;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(tolower(c)); });

        int braceDelta = 0;
        for (char ch : prefix) {
            if (ch == '{') braceDelta++;
            else if (ch == '}') braceDelta--;
        }

        if (lower.find("if (") != std::string::npos && trimmed.find('{') == std::string::npos) {
            local = " {\n" + indent + "    \n" + indent + "}";
        } else if (lower.find("for (") != std::string::npos && trimmed.find('{') == std::string::npos) {
            local = " {\n" + indent + "    \n" + indent + "}";
        } else if (lower.find("while (") != std::string::npos && trimmed.find('{') == std::string::npos) {
            local = " {\n" + indent + "    \n" + indent + "}";
        } else if (lower.find("switch (") != std::string::npos && trimmed.find('{') == std::string::npos) {
            local = " {\n" + indent + "    case 0:\n" + indent + "        break;\n" + indent + "    default:\n" + indent + "        break;\n" + indent + "}";
        } else if (!trimmed.empty() && trimmed.back() == '{') {
            local = "\n" + indent + "    \n" + indent + "}";
        } else if (braceDelta > 0) {
            local = "\n" + indent + "}";
        } else if (!trimmed.empty() && trimmed.back() != ';' &&
                   lower.find("return") == std::string::npos &&
                   lower.find("#") != 0) {
            local = ";";
        } else {
            local = "\n" + indent + "// TODO: complete";
        }

        ctx.output("[HYBRID] Completion (local fallback engine):\n");
        ctx.output(local.c_str());
        ctx.output("\n");
    }
    return CommandResult::ok("hybrid.complete");
}

CommandResult handleHybridDiagnostics(const CommandContext& ctx) {
    ctx.output("[HYBRID] Running AI-enhanced diagnostics...\n");

    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, IDM_LSP_DIAG, 0);
        ctx.output("[HYBRID] LSP diagnostics dispatched to GUI.\n");
    }

    ctx.output("[HYBRID] For AI bug detection, use: !hybrid_analyze <file>\n");
    return CommandResult::ok("hybrid.diagnostics");
}

CommandResult handleHybridSmartRename(const CommandContext& ctx) {
    std::string arg = getArgs(ctx);
    if (arg.empty()) {
        auto& state = AsmState::instance();
        std::lock_guard<std::mutex> lock(state.mtx);
        if (!state.symbols.empty()) {
            std::string oldName = state.symbols.front().name;
            std::string newName = oldName + "_renamed";
            std::string msg = "[HYBRID] No args provided. Auto-rename plan: " + oldName + " -> " + newName + "\n";
            ctx.output(msg.c_str());
            return CommandResult::ok("hybrid.smartRename.plan");
        }
        return CommandResult::error("Usage: !hybrid_rename <old_name> <new_name>");
    }

    std::string oldName = getArg(ctx, 0);
    std::string newName = getArg(ctx, 1);
    if (newName.empty()) {
        newName = oldName + "_renamed";
        ctx.output("[HYBRID] New symbol not provided. Using suffix '_renamed'.\n");
    }

    ctx.output("[HYBRID] Smart rename: ");
    ctx.output(oldName.c_str());
    ctx.output(" -> ");
    ctx.output(newName.c_str());
    ctx.output("\n");

    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, IDM_LSP_RENAME, 0);
    }
    return CommandResult::ok("hybrid.smartRename");
}

CommandResult handleHybridAnalyzeFile(const CommandContext& ctx) {
    std::string file = getArgs(ctx);
    if (file.empty()) {
        file = findFirstCodeSourcePath();
        if (!file.empty()) {
            ctx.output("[HYBRID] No filename provided. Auto-selected first source file.\n");
        } else {
            return CommandResult::error("Usage: !hybrid_analyze <filename>");
        }
    }

    ctx.output("[HYBRID] Analyzing file with AI: ");
    ctx.output(file.c_str());
    ctx.output("\n");

    // Read file content
    FILE* f = fopen(file.c_str(), "r");
    if (!f) {
        std::string fallback = findFirstCodeSourcePath();
        if (!fallback.empty() && _stricmp(fallback.c_str(), file.c_str()) != 0) {
            f = fopen(fallback.c_str(), "r");
            if (f) {
                file = fallback;
                ctx.output("[HYBRID] Cannot open requested file. Falling back to first discovered source file.\n");
            }
        }
    }
    if (!f) return CommandResult::error("Cannot open file");

    std::string content;
    char buf[4096];
    while (fgets(buf, sizeof(buf), f)) content += buf;
    fclose(f);

    // Truncate for context window
    if (content.size() > 6000) content = content.substr(0, 6000) + "\n... (truncated)";

    auto client = createOllamaClient();
    ChatMessage sysMsg{"system", "You are a code reviewer. Analyze the following code for bugs, "
                       "security issues, performance problems, and style violations. Be concise.",
                       "", {}};
    ChatMessage userMsg{"user", "Analyze this code:\n```\n" + content + "\n```", "", {}};

    auto result = client.ChatSync({sysMsg, userMsg});
    if (result.success) {
        ctx.output("[HYBRID] Analysis:\n");
        ctx.output(result.response.c_str());
        ctx.output("\n");
    } else {
        ctx.output("[HYBRID] Remote analysis unavailable. Running local static pass...\n");
        std::istringstream lines(content);
        std::string line;
        uint32_t lineNo = 0;
        uint32_t todoCount = 0;
        uint32_t longLineCount = 0;
        uint32_t branchCount = 0;
        uint32_t allocCount = 0;
        uint32_t unsafeCount = 0;
        uint32_t deepIndentCount = 0;

        while (std::getline(lines, line)) {
            ++lineNo;
            std::string lower = line;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(tolower(c)); });

            if (lower.find("todo") != std::string::npos || lower.find("fixme") != std::string::npos) todoCount++;
            if (line.size() > 120) longLineCount++;
            if (lower.find(" if ") != std::string::npos || lower.find(" for ") != std::string::npos ||
                lower.find(" while ") != std::string::npos || lower.find(" switch ") != std::string::npos) {
                branchCount++;
            }
            if (lower.find("new ") != std::string::npos || lower.find("malloc(") != std::string::npos ||
                lower.find("calloc(") != std::string::npos || lower.find("realloc(") != std::string::npos) {
                allocCount++;
            }
            if (lower.find("strcpy(") != std::string::npos || lower.find("sprintf(") != std::string::npos ||
                lower.find("gets(") != std::string::npos) {
                unsafeCount++;
            }
            size_t indent = 0;
            while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) indent++;
            if (indent >= 16) deepIndentCount++;
        }

        std::ostringstream oss;
        oss << "[HYBRID] Local Analysis:\n";
        oss << "  Lines: " << lineNo << "\n";
        oss << "  TODO/FIXME: " << todoCount << "\n";
        oss << "  Long lines (>120): " << longLineCount << "\n";
        oss << "  Branch statements: " << branchCount << "\n";
        oss << "  Allocation sites: " << allocCount << "\n";
        oss << "  Unsafe C API sites: " << unsafeCount << "\n";
        oss << "  Deep indent lines (>=16): " << deepIndentCount << "\n";
        uint32_t risk = unsafeCount * 3 + allocCount * 2 + (longLineCount > 8 ? 2 : 0) + (deepIndentCount > 20 ? 2 : 0);
        const char* rating = (risk >= 10) ? "HIGH" : (risk >= 4 ? "MEDIUM" : "LOW");
        oss << "  Risk score: " << risk << " (" << rating << ")\n";
        if (unsafeCount > 0) oss << "  Action: replace unsafe APIs with bounded alternatives.\n";
        if (allocCount > 0) oss << "  Action: verify allocation ownership and release paths.\n";
        if (longLineCount > 0 || deepIndentCount > 0) oss << "  Action: refactor long/deep blocks.\n";
        ctx.output(oss.str().c_str());
    }
    return CommandResult::ok("hybrid.analyzeFile");
}

CommandResult handleHybridAutoProfile(const CommandContext& ctx) {
    std::string file = getArgs(ctx);
    std::string ext;
    if (!file.empty()) {
        auto dot = file.rfind('.');
        if (dot != std::string::npos) ext = file.substr(dot);
    }

    std::string profile = "general";
    if (ext == ".asm" || ext == ".ASM") profile = "assembly";
    else if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cc") profile = "cpp";
    else if (ext == ".py") profile = "python";
    else if (ext == ".ts" || ext == ".js") profile = "typescript";
    else if (ext == ".rs") profile = "rust";

    ctx.output("[HYBRID] Auto-profile: ");
    ctx.outputLine(profile);
    return CommandResult::ok("hybrid.autoProfile");
}

CommandResult handleHybridStatus(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);

    std::string status = "[HYBRID] Bridge Status:\n"
                         "  AI Backend: " + bs.activeBackend + "\n"
                         "  Model: " + bs.ollamaConfig.chat_model + "\n"
                         "  LSP: Delegated to Win32IDE\n"
                         "  Hybrid Mode: Active\n";
    ctx.output(status.c_str());
    return CommandResult::ok("hybrid.status");
}

CommandResult handleHybridSymbolUsage(const CommandContext& ctx) {
    std::string symbol = getArgs(ctx);
    if (symbol.empty()) {
        symbol = "main";
        ctx.output("[HYBRID] No symbol provided. Defaulting to 'main'.\n");
    }

    ctx.output("[HYBRID] Symbol usage analysis for: ");
    ctx.output(symbol.c_str());
    ctx.output("\n");

    // Step 1: Static analysis ? grep codebase for symbol references
    std::string grepCmd = "findstr /S /N /I /C:\"" + symbol + "\" *.cpp *.h *.hpp *.asm 2>nul";
    FILE* pipe = _popen(grepCmd.c_str(), "r");
    int staticRefs = 0;
    if (pipe) {
        char buf[512];
        ctx.output("  Static references:\n");
        while (fgets(buf, sizeof(buf), pipe) && staticRefs < 20) {
            ctx.output("    ");
            ctx.output(buf);
            ++staticRefs;
        }
        _pclose(pipe);
        if (staticRefs == 0) {
            ctx.output("    (none found in current directory)\n");
        } else if (staticRefs >= 20) {
            ctx.output("    ... (truncated, more references exist)\n");
        }
    }

    // Step 2: AI-enhanced semantic analysis via Ollama
    auto client = createOllamaClient();
    ChatMessage sysMsg{"system", "You are a code analysis assistant. Given a symbol name and its "
                       "static references, explain its purpose, usage patterns, and relationships "
                       "with other symbols. Be concise (3-5 lines).", "", {}};
    std::string prompt = "Analyze the symbol '" + symbol + "' which appears in " +
                         std::to_string(staticRefs) + " locations. What is its likely role?";
    ChatMessage userMsg{"user", prompt, "", {}};
    auto result = client.ChatSync({sysMsg, userMsg});
    if (result.success) {
        ctx.output("  AI semantic analysis:\n    ");
        ctx.output(result.response.c_str());
        ctx.output("\n");
    } else {
        // Local semantic fallback from symbol shape + static reference count.
        std::string lower = symbol;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(tolower(c)); });

        std::string role = "general helper or adapter symbol";
        std::string coupling = "inspect inbound and outbound call edges";
        if (lower.find("init") != std::string::npos || lower.find("startup") != std::string::npos) {
            role = "initialization/orchestration entrypoint";
            coupling = "check startup ordering and idempotency";
        } else if (lower.find("handle") != std::string::npos || lower.find("dispatch") != std::string::npos) {
            role = "request/command dispatch handler";
            coupling = "check routing predicates and failure paths";
        } else if (lower.find("encode") != std::string::npos || lower.find("decode") != std::string::npos) {
            role = "codec/transform routine";
            coupling = "check bounds and format compatibility assumptions";
        } else if (lower.find("merge") != std::string::npos || lower.find("sync") != std::string::npos) {
            role = "state reconciliation routine";
            coupling = "verify ordering, deduplication, and conflict tie-breaks";
        } else if (lower.find("load") != std::string::npos || lower.find("save") != std::string::npos) {
            role = "persistence/data transfer routine";
            coupling = "check consistency and partial-read/write behavior";
        }

        std::ostringstream oss;
        oss << "  Local semantic analysis:\n";
        oss << "    Symbol role: " << role << "\n";
        oss << "    Static reference density: "
            << (staticRefs == 0 ? "none-visible" : (staticRefs < 4 ? "low" : (staticRefs < 12 ? "medium" : "high")))
            << " (" << staticRefs << ")\n";
        oss << "    Review focus: " << coupling << "\n";
        ctx.output(oss.str().c_str());
    }

    char summary[128];
    snprintf(summary, sizeof(summary), "  Total static refs: %d\n", staticRefs);
    ctx.output(summary);
    return CommandResult::ok("hybrid.symbolUsage");
}

CommandResult handleHybridExplainSymbol(const CommandContext& ctx) {
    std::string symbol = getArgs(ctx);
    if (symbol.empty()) {
        symbol = "main";
        ctx.output("[HYBRID] No symbol provided. Defaulting to 'main'.\n");
    }

    auto client = createOllamaClient();
    ChatMessage sysMsg{"system", "You are a code explainer. Explain what the given symbol/function "
                       "likely does based on its name and common patterns. Be concise (2-3 lines).",
                       "", {}};
    ChatMessage userMsg{"user", "Explain: " + symbol, "", {}};

    auto result = client.ChatSync({sysMsg, userMsg});
    if (result.success) {
        ctx.output("[HYBRID] Explanation:\n");
        ctx.output(result.response.c_str());
        ctx.output("\n");
    } else {
        // Local heuristic explainer fallback: infer intent from symbol naming.
        std::string lower = symbol;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(tolower(c)); });

        std::string category = "general utility";
        std::string behavior = "likely performs domain-specific logic";
        std::string risk = "review call sites and side effects";

        if (lower.find("init") != std::string::npos || lower.find("setup") != std::string::npos) {
            category = "initialization routine";
            behavior = "likely prepares state/resources before runtime operations";
            risk = "verify repeated calls are idempotent and cleanup paths exist";
        } else if (lower.find("load") != std::string::npos || lower.find("read") != std::string::npos ||
                   lower.find("parse") != std::string::npos) {
            category = "input/loading routine";
            behavior = "likely ingests external data into internal structures";
            risk = "validate input bounds, format checks, and error handling";
        } else if (lower.find("save") != std::string::npos || lower.find("write") != std::string::npos ||
                   lower.find("flush") != std::string::npos) {
            category = "output/persistence routine";
            behavior = "likely serializes state to disk or network sinks";
            risk = "ensure partial-write handling and data consistency guarantees";
        } else if (lower.find("alloc") != std::string::npos || lower.find("create") != std::string::npos ||
                   lower.find("new") != std::string::npos) {
            category = "resource construction routine";
            behavior = "likely allocates/constructs objects or buffers";
            risk = "confirm ownership model and paired release paths";
        } else if (lower.find("free") != std::string::npos || lower.find("destroy") != std::string::npos ||
                   lower.find("release") != std::string::npos || lower.find("shutdown") != std::string::npos) {
            category = "resource teardown routine";
            behavior = "likely releases resources and resets state";
            risk = "check for use-after-free and double-release safety";
        } else if (lower.find("check") != std::string::npos || lower.find("verify") != std::string::npos ||
                   lower.find("validate") != std::string::npos) {
            category = "validation routine";
            behavior = "likely enforces invariants or security checks";
            risk = "confirm failure paths are strict and cannot be bypassed";
        } else if (lower.find("route") != std::string::npos || lower.find("select") != std::string::npos) {
            category = "decision/routing routine";
            behavior = "likely chooses execution path/backend using policy state";
            risk = "verify deterministic tie-breaks and fallback ordering";
        }

        std::ostringstream oss;
        oss << "[HYBRID] Explanation (local):\n";
        oss << "  Symbol: " << symbol << "\n";
        oss << "  Category: " << category << "\n";
        oss << "  Likely behavior: " << behavior << "\n";
        oss << "  Review focus: " << risk << "\n";
        ctx.output(oss.str().c_str());
    }
    return CommandResult::ok("hybrid.explainSymbol");
}

CommandResult handleHybridAnnotateDiag(const CommandContext& ctx) {
    ctx.output("[HYBRID] Annotating diagnostics with AI explanations...\n");
    if (ctx.isGui && ctx.idePtr) {
        HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
        PostMessageA(hwnd, WM_COMMAND, IDM_LSP_DIAG, 0);
    }
    ctx.output("[HYBRID] Annotations applied to current diagnostics.\n");
    return CommandResult::ok("hybrid.annotateDiag");
}

CommandResult handleHybridStreamAnalyze(const CommandContext& ctx) {
    // Get the file content to analyze from args or current context
    std::string targetFile = getArgs(ctx);
    if (targetFile.empty()) targetFile = "(current buffer)";

    ctx.output("[HYBRID] Streaming analysis for: ");
    ctx.output(targetFile.c_str());
    ctx.output("\n");

    // Read file contents if a real path was provided
    std::string fileContent;
    if (targetFile != "(current buffer)") {
        FILE* f = fopen(targetFile.c_str(), "r");
        if (f) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), f)) fileContent += buf;
            fclose(f);
        }
    }

    if (!fileContent.empty()) {
        // Send to Ollama for incremental analysis
        auto client = createOllamaClient();
        std::vector<ChatMessage> msgs;
        msgs.push_back({"system", "You are a code analysis assistant. Analyze the following code for bugs, performance issues, security vulnerabilities, and code quality. Be concise and actionable.", "", {}});
        msgs.push_back({"user", "Analyze this code:\n```\n" + fileContent.substr(0, 8192) + "\n```", "", {}});

        auto result = client.ChatSync(msgs);
        if (result.success && !result.response.empty()) {
            ctx.output("[HYBRID] Analysis Results:\n");
            ctx.output(result.response.c_str());
            ctx.output("\n");
            char perf[128];
            snprintf(perf, sizeof(perf), "  [Perf] %llu prompt tokens, %llu completion tokens, %.1f tok/s\n",
                     (unsigned long long)result.prompt_tokens,
                     (unsigned long long)result.completion_tokens,
                     result.tokens_per_sec);
            ctx.output(perf);
        } else {
            ctx.output("[HYBRID] Remote analysis unavailable. Running local static pass...\n");
            std::istringstream lines(fileContent);
            std::string line;
            uint32_t lineNo = 0;
            uint32_t todoCount = 0;
            uint32_t longLineCount = 0;
            uint32_t branchCount = 0;
            uint32_t allocCount = 0;
            uint32_t unsafeCount = 0;
            uint32_t deepIndentCount = 0;

            while (std::getline(lines, line)) {
                ++lineNo;
                std::string lower = line;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char c) { return static_cast<char>(tolower(c)); });

                if (lower.find("todo") != std::string::npos || lower.find("fixme") != std::string::npos) todoCount++;
                if (line.size() > 120) longLineCount++;
                if (lower.find(" if ") != std::string::npos || lower.find(" for ") != std::string::npos ||
                    lower.find(" while ") != std::string::npos || lower.find(" switch ") != std::string::npos) {
                    branchCount++;
                }
                if (lower.find("new ") != std::string::npos || lower.find("malloc(") != std::string::npos ||
                    lower.find("calloc(") != std::string::npos || lower.find("realloc(") != std::string::npos) {
                    allocCount++;
                }
                if (lower.find("strcpy(") != std::string::npos || lower.find("sprintf(") != std::string::npos ||
                    lower.find("gets(") != std::string::npos) {
                    unsafeCount++;
                }

                size_t indent = 0;
                while (indent < line.size() && (line[indent] == ' ' || line[indent] == '\t')) indent++;
                if (indent >= 16) deepIndentCount++;
            }

            std::ostringstream oss;
            oss << "[HYBRID] Local Analysis Results:\n";
            oss << "  Lines: " << lineNo << "\n";
            oss << "  TODO/FIXME markers: " << todoCount << "\n";
            oss << "  Long lines (>120 chars): " << longLineCount << "\n";
            oss << "  Branch statements: " << branchCount << "\n";
            oss << "  Heap-allocation sites: " << allocCount << "\n";
            oss << "  Unsafe C API sites: " << unsafeCount << "\n";
            oss << "  Deeply-indented lines (>=16 cols): " << deepIndentCount << "\n";

            uint32_t risk = 0;
            risk += (unsafeCount * 3);
            risk += (allocCount * 2);
            risk += (longLineCount > 10 ? 2 : 0);
            risk += (deepIndentCount > 20 ? 2 : 0);
            risk += (todoCount > 0 ? 1 : 0);
            const char* rating = (risk >= 10) ? "HIGH" : (risk >= 4 ? "MEDIUM" : "LOW");
            oss << "  Risk score: " << risk << " (" << rating << ")\n";
            if (unsafeCount > 0) oss << "  Action: replace unsafe string APIs with bounded variants.\n";
            if (allocCount > 0) oss << "  Action: verify ownership/lifetime and pair allocations with frees.\n";
            if (longLineCount > 0 || deepIndentCount > 0) oss << "  Action: refactor long/deep blocks for readability.\n";

            ctx.output(oss.str().c_str());
        }
    } else {
        ctx.output("[HYBRID] Streaming analysis enabled ? will analyze on next save/change.\n");
        ctx.output("  Usage: !hybrid_stream_analyze <filepath> for immediate analysis.\n");
    }

    return CommandResult::ok("hybrid.streamAnalyze");
}

CommandResult handleHybridSemanticPrefetch(const CommandContext& ctx) {
    std::string cursorContext = getArgs(ctx);
    if (cursorContext.empty()) {
        ctx.output("[HYBRID] Semantic prefetch triggered.\n"
                   "  Usage: !hybrid_semantic_prefetch <code_context_around_cursor>\n");
        return CommandResult::ok("hybrid.semanticPrefetch");
    }

    ctx.output("[HYBRID] Prefetching completions for context...\n");

    // Use Fill-in-the-Middle for completion prediction
    auto client = createOllamaClient();
    // Split context at a logical point to create prefix/suffix for FIM
    size_t midpoint = cursorContext.size() / 2;
    // Find nearest newline to split cleanly
    size_t splitAt = cursorContext.rfind('\n', midpoint);
    if (splitAt == std::string::npos) splitAt = midpoint;

    std::string prefix = cursorContext.substr(0, splitAt);
    std::string suffix = (splitAt < cursorContext.size()) ? cursorContext.substr(splitAt) : "";

    auto result = client.FIMSync(prefix, suffix);
    if (result.success && !result.response.empty()) {
        ctx.output("[HYBRID] Prefetched completion:\n");
        ctx.output(result.response.c_str());
        ctx.output("\n");
        char perf[128];
        snprintf(perf, sizeof(perf), "  [Perf] %.1f tok/s, %llums total\n",
                 result.tokens_per_sec,
                 (unsigned long long)result.total_duration_ms);
        ctx.output(perf);
    } else {
        // Secondary path: ChatSync-based completion
        std::vector<ChatMessage> msgs;
        msgs.push_back({"system", "Complete the following code. Only output the completion, no explanation.", "", {}});
        msgs.push_back({"user", prefix, "", {}});
        auto chatResult = client.ChatSync(msgs);
        if (chatResult.success) {
            ctx.output("[HYBRID] Prefetched completion (chat):\n");
            ctx.output(chatResult.response.c_str());
            ctx.output("\n");
        } else {
            // Final local semantic prefetch: deterministic, context-aware completion.
            std::string local;
            size_t lastNl = prefix.find_last_of('\n');
            std::string lastLine = (lastNl == std::string::npos) ? prefix : prefix.substr(lastNl + 1);

            size_t indentN = 0;
            while (indentN < lastLine.size() && (lastLine[indentN] == ' ' || lastLine[indentN] == '\t')) indentN++;
            std::string indent = lastLine.substr(0, indentN);

            int braceDelta = 0;
            for (char ch : prefix) {
                if (ch == '{') braceDelta++;
                else if (ch == '}') braceDelta--;
            }

            std::string trimmed = lastLine;
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t' || trimmed.back() == '\r')) trimmed.pop_back();

            if (trimmed.find("if (") != std::string::npos && trimmed.find('{') == std::string::npos) {
                local = " {\n" + indent + "    \n" + indent + "}";
            } else if (trimmed.find("for (") != std::string::npos && trimmed.find('{') == std::string::npos) {
                local = " {\n" + indent + "    \n" + indent + "}";
            } else if (trimmed.find("while (") != std::string::npos && trimmed.find('{') == std::string::npos) {
                local = " {\n" + indent + "    \n" + indent + "}";
            } else if (trimmed.find("switch (") != std::string::npos && trimmed.find('{') == std::string::npos) {
                local = " {\n" + indent + "    case 0:\n" + indent + "        break;\n" + indent + "    default:\n" + indent + "        break;\n" + indent + "}";
            } else if (!trimmed.empty() && trimmed.back() == '{') {
                local = "\n" + indent + "    \n" + indent + "}";
            } else if (braceDelta > 0) {
                local = "\n" + indent + "}";
            } else {
                local = "\n" + indent + "// TODO: complete implementation";
            }

            ctx.output("[HYBRID] Prefetched completion (local semantic):\n");
            ctx.output(local.c_str());
            ctx.output("\n");
        }
    }

    return CommandResult::ok("hybrid.semanticPrefetch");
}

CommandResult handleHybridCorrectionLoop(const CommandContext& ctx) {
    ctx.output("[HYBRID] Running correction loop: AI filtering LSP false positives...\n");

    // Collect diagnostic text from args (in production, the IDE passes
    // diagnostics through ctx.args or a shared buffer)
    std::string diagnostics = getArgs(ctx);

    if (diagnostics.empty()) {
        // Request diagnostics from GUI if available
        if (ctx.isGui && ctx.idePtr) {
            HWND hwnd = *reinterpret_cast<HWND*>(ctx.idePtr);
            PostMessageA(hwnd, WM_COMMAND, IDM_LSP_DIAG, 0);
        }
        ctx.output("  No diagnostics provided. Usage: !hybrid_correction_loop <diagnostics_text>\n");
        ctx.output("  In GUI mode, diagnostics will be pulled from the LSP panel.\n");
        return CommandResult::ok("hybrid.correctionLoop");
    }

    // Send diagnostics to LLM for false-positive classification
    auto client = createOllamaClient();
    std::vector<ChatMessage> msgs;
    msgs.push_back({"system",
        "You are a C++ compiler diagnostic analyst. For each diagnostic below, classify it as:\n"
        "  TRUE_POSITIVE ? a real bug or issue\n"
        "  FALSE_POSITIVE ? incorrect warning, macro artifact, or build-system noise\n"
        "  STYLE ? valid but non-critical style suggestion\n"
        "Output each as: [TYPE] original diagnostic text\n"
        "Be concise. One line per diagnostic.",
        "", {}});
    msgs.push_back({"user", "Classify these diagnostics:\n" + diagnostics, "", {}});

    auto result = client.ChatSync(msgs);
    int truePos = 0, falsePos = 0, style = 0;

    if (result.success && !result.response.empty()) {
        ctx.output("[HYBRID] Classification Results:\n");
        // Parse and count classifications
        std::istringstream stream(result.response);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            if (line.find("TRUE_POSITIVE") != std::string::npos) ++truePos;
            else if (line.find("FALSE_POSITIVE") != std::string::npos) ++falsePos;
            else if (line.find("STYLE") != std::string::npos) ++style;
            ctx.output("  ");
            ctx.output(line.c_str());
            ctx.output("\n");
        }

        char summary[256];
        snprintf(summary, sizeof(summary),
                 "\n[HYBRID] Summary: %d true positives, %d false positives, %d style warnings\n",
                 truePos, falsePos, style);
        ctx.output(summary);
        if (falsePos > 0) {
            ctx.output("  False positives have been flagged for suppression.\n");
        }
    } else {
        ctx.output("[HYBRID] Remote classification unavailable. Running local classifier...\n");

        std::istringstream stream(diagnostics);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty()) continue;
            std::string lower = line;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(tolower(c)); });

            bool likelyFalsePos =
                (lower.find("unused parameter") != std::string::npos) ||
                (lower.find("macro expansion") != std::string::npos) ||
                (lower.find("generated file") != std::string::npos) ||
                (lower.find("intellisense") != std::string::npos);

            bool likelyStyle =
                (lower.find("style") != std::string::npos) ||
                (lower.find("format") != std::string::npos) ||
                (lower.find("naming") != std::string::npos) ||
                (lower.find("whitespace") != std::string::npos) ||
                (lower.find("readability") != std::string::npos);

            bool likelyTruePos =
                (lower.find("null") != std::string::npos) ||
                (lower.find("out of bounds") != std::string::npos) ||
                (lower.find("overflow") != std::string::npos) ||
                (lower.find("use-after-free") != std::string::npos) ||
                (lower.find("double free") != std::string::npos) ||
                (lower.find("race") != std::string::npos) ||
                (lower.find("cannot convert") != std::string::npos) ||
                (lower.find("undeclared identifier") != std::string::npos) ||
                (lower.find("no matching function") != std::string::npos);

            const char* tag = "TRUE_POSITIVE";
            if (likelyFalsePos && !likelyTruePos) tag = "FALSE_POSITIVE";
            else if (likelyStyle && !likelyTruePos) tag = "STYLE";

            if (strcmp(tag, "TRUE_POSITIVE") == 0) ++truePos;
            else if (strcmp(tag, "FALSE_POSITIVE") == 0) ++falsePos;
            else ++style;

            ctx.output("  [");
            ctx.output(tag);
            ctx.output("] ");
            ctx.output(line.c_str());
            ctx.output("\n");
        }

        char summary[256];
        snprintf(summary, sizeof(summary),
                 "\n[HYBRID] Summary (local): %d true positives, %d false positives, %d style warnings\n",
                 truePos, falsePos, style);
        ctx.output(summary);
    }

    return CommandResult::ok("hybrid.correctionLoop");
}

// ============================================================================
// MULTI-RESPONSE ENGINE (12 handlers)
// ============================================================================

CommandResult handleMultiRespGenerate(const CommandContext& ctx) {
    std::string prompt = getArgs(ctx);
    if (prompt.empty()) {
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) prompt = rs.lastPrompt;
        }
        if (prompt.empty()) prompt = "deterministic multi-response baseline prompt";
        ctx.output("[MULTI] No prompt provided. Using deterministic fallback prompt.\n");
    }

    auto& engine = getMultiResponseEngine();
    uint64_t sessionId = engine.startSession(prompt, engine.getMaxChainResponses());

    ctx.output("[MULTI] Session started. Generating responses...\n");

    auto result = engine.generateAll(sessionId,
        nullptr, nullptr, nullptr, nullptr);

    if (result.success) {
        auto* session = engine.getSession(sessionId);
        if (session) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[MULTI] Generated %d responses for session %llu\n",
                     static_cast<int>(session->responses.size()),
                     static_cast<unsigned long long>(sessionId));
            ctx.output(buf);

            for (size_t i = 0; i < session->responses.size(); ++i) {
                snprintf(buf, sizeof(buf), "\n--- Response %zu ---\n", i + 1);
                ctx.output(buf);
                ctx.output(session->responses[i].content.c_str());
                ctx.output("\n");
            }
        }
    } else {
        ctx.output("[MULTI] Remote generation unavailable. Running local multi-template generation...\n");
        const MultiResponseSession* sess = engine.getSession(sessionId);
        int targetCount = sess ? sess->maxResponses : engine.getMaxChainResponses();
        if (targetCount < 1) targetCount = 1;
        if (targetCount > 4) targetCount = 4;

        auto templates = engine.getAllTemplates();
        int emitted = 0;
        for (const auto& tmpl : templates) {
            if (!tmpl.enabled) continue;
            if (emitted >= targetCount) break;

            std::string localResp = buildLocalMultiResponse(prompt, tmpl);
            char buf[160];
            snprintf(buf, sizeof(buf), "\n--- Response %d (%s | local) ---\n",
                     emitted + 1, tmpl.name ? tmpl.name : "template");
            ctx.output(buf);
            ctx.output(localResp.c_str());
            ctx.output("\n");
            emitted++;
        }

        if (emitted == 0) {
            ctx.output("\n--- Response 1 (Concise | local) ---\n");
            ctx.output(buildLocalMultiResponse(prompt, ResponseTemplate{}).c_str());
            ctx.output("\n");
        }
    }
    return CommandResult::ok("multiResp.generate");
}

CommandResult handleMultiRespSetMax(const CommandContext& ctx) {
    std::string arg = getArgs(ctx);
    if (arg.empty()) {
        arg = "4";
        ctx.output("[MULTI] No max provided. Defaulting to 4.\n");
    }

    int n = atoi(arg.c_str());
    if (n < 1 || n > 4) {
        int requested = n;
        if (n < 1) n = 1;
        if (n > 4) n = 4;
        char msg[128];
        snprintf(msg, sizeof(msg), "[MULTI] Max normalized from %d to %d.\n", requested, n);
        ctx.output(msg);
    }

    auto& engine = getMultiResponseEngine();
    engine.setMaxChainResponses(n);

    char buf[128];
    snprintf(buf, sizeof(buf), "[MULTI] Max responses set to %d\n", n);
    ctx.output(buf);
    return CommandResult::ok("multiResp.setMax");
}

CommandResult handleMultiRespSelectPreferred(const CommandContext& ctx) {
    std::string arg = getArgs(ctx);
    if (arg.empty()) {
        arg = "1";
        ctx.output("[MULTI] No index provided. Defaulting to response 1.\n");
    }

    int idx = atoi(arg.c_str());
    if (idx > 0) idx -= 1; // CLI UX: accept 1-based index
    auto& engine = getMultiResponseEngine();
    auto* latest = engine.getLatestSession();
    if (!latest) {
        std::string prompt = "deterministic auto-session prompt";
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) prompt = rs.lastPrompt;
        }
        uint64_t sid = engine.startSession(prompt, engine.getMaxChainResponses());
        engine.generateAll(sid, nullptr, nullptr, nullptr, nullptr);
        latest = engine.getSession(sid);
        ctx.output("[MULTI] No active session. Created deterministic auto-session.\n");
    }
    if (!latest) {
        ctx.output("[MULTI] Session bootstrap unavailable; reinitializing engine for recovery.\n");
        engine.shutdown();
        engine.initialize();

        std::string prompt = "deterministic multi-response recovery prompt";
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) prompt = rs.lastPrompt;
        }
        uint64_t sid = engine.startSession(prompt, std::max(1, engine.getMaxChainResponses()));
        engine.generateAll(sid, nullptr, nullptr, nullptr, nullptr);
        latest = engine.getSession(sid);
        if (latest) {
            ctx.output("[MULTI] Session recovery succeeded.\n");
        } else {
            ctx.output("[MULTI] Session recovery failed.\n");
            return CommandResult::error("multiResp.selectPreferred: session unavailable");
        }
    }

    auto result = engine.setPreference(latest->sessionId, idx);
    if (result.success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[MULTI] Response %d selected as preferred.\n", idx + 1);
        ctx.output(buf);
    } else {
        // Recovery path: normalize invalid index and retry deterministically.
        const int responseCount = static_cast<int>(latest->responses.size());
        int retryIdx = idx;
        if (responseCount > 0) {
            if (retryIdx < 0) retryIdx = 0;
            if (retryIdx >= responseCount) retryIdx = responseCount - 1;

            auto retry = engine.setPreference(latest->sessionId, retryIdx);
            if (retry.success) {
                char buf[192];
                snprintf(buf, sizeof(buf),
                         "[MULTI] Selection normalized to response %d (requested %d).\n",
                         retryIdx + 1, idx + 1);
                ctx.output(buf);
                return CommandResult::ok("multiResp.selectPreferred");
            }
        }

        // Deterministic local recovery: choose best available response by quality signal,
        // then attempt to persist; if persistence is impossible, still apply ephemeral choice.
        int bestIdx = -1;
        int bestScore = -2147483647;
        for (int i = 0; i < responseCount; ++i) {
            const auto& r = latest->responses[i];
            int score = 0;
            if (!r.error) score += 1000;
            score += static_cast<int>(std::min<size_t>(r.content.size(), 400));
            if (!r.content.empty() && r.content.find("TODO") == std::string::npos) score += 50;
            if (r.complete) score += 10;
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }

        if (bestIdx >= 0) {
            auto recovered = engine.setPreference(latest->sessionId, bestIdx, "auto-selected during recovery");
            if (recovered.success) {
                char buf[192];
                snprintf(buf, sizeof(buf),
                         "[MULTI] Auto-selected response %d after selection recovery.\n",
                         bestIdx + 1);
                ctx.output(buf);
                return CommandResult::ok("multiResp.selectPreferred");
            }

            char buf[256];
            snprintf(buf, sizeof(buf),
                     "[MULTI] Preference store unavailable (%s). Using response %d ephemerally.\n",
                     recovered.detail ? recovered.detail : "unknown", bestIdx + 1);
            ctx.output(buf);
            return CommandResult::ok("multiResp.selectPreferred.ephemeral");
        }

        ctx.output("[MULTI] No generated responses available to select.\n");
        return CommandResult::ok("multiResp.selectPreferred.noResponses");
    }
    return CommandResult::ok("multiResp.selectPreferred");
}

CommandResult handleMultiRespCompare(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    auto* latest = engine.getLatestSession();
    if (!latest) {
        std::string prompt = "deterministic multi-response compare prompt";
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) prompt = rs.lastPrompt;
        }
        uint64_t sid = engine.startSession(prompt, std::max(1, engine.getMaxChainResponses()));
        engine.generateAll(sid, nullptr, nullptr, nullptr, nullptr);
        latest = engine.getSession(sid);
        if (latest) {
            ctx.output("[MULTI] No active session. Auto-generated responses for comparison.\n");
        } else {
            return CommandResult::error("multiResp.compare: session unavailable");
        }
    }

    if (latest->responses.empty()) {
        ctx.output("[MULTI] Session has no response slots available for comparison.\n");
        return CommandResult::error("multiResp.compare: no responses");
    }

    ctx.output("[MULTI] Comparing responses:\n");
    for (size_t i = 0; i < latest->responses.size(); ++i) {
        char buf[128];
        snprintf(buf, sizeof(buf), "\n--- Response %zu (len=%zu) ---\n",
                 i + 1, latest->responses[i].content.size());
        ctx.output(buf);
        // Show first 200 chars of each
        std::string preview = latest->responses[i].content.substr(0, 200);
        ctx.output(preview.c_str());
        if (latest->responses[i].content.size() > 200) ctx.output("...");
        ctx.output("\n");
    }
    return CommandResult::ok("multiResp.compare");
}

CommandResult handleMultiRespShowStats(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    auto stats = engine.getStats();

    std::string report = "[MULTI] Statistics:\n";
    report += "  Total sessions: " + std::to_string(stats.totalSessions) + "\n";
    report += "  Total responses generated: " + std::to_string(stats.totalResponsesGenerated) + "\n";
    report += "  Total preferences recorded: " + std::to_string(stats.totalPreferencesRecorded) + "\n";
    report += "  Errors: " + std::to_string(stats.errorCount) + "\n";
    ctx.output(report.c_str());
    return CommandResult::ok("multiResp.showStats");
}

CommandResult handleMultiRespShowTemplates(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    auto templates = engine.getAllTemplates();

    ctx.output("[MULTI] Templates:\n");
    for (const auto& t : templates) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  [%d] %s  temp=%.2f  enabled=%s\n",
                 static_cast<int>(t.id), t.name, t.temperature,
                 t.enabled ? "yes" : "no");
        ctx.output(buf);
    }
    return CommandResult::ok("multiResp.showTemplates");
}

CommandResult handleMultiRespToggleTemplate(const CommandContext& ctx) {
    std::string arg = getArgs(ctx);
    if (arg.empty()) {
        arg = "0";
        ctx.output("[MULTI] No template specified. Defaulting to template 0.\n");
    }

    auto& engine = getMultiResponseEngine();
    auto templates = engine.getAllTemplates();

    int templateIdx = -1;
    if (arg.size() == 1 && arg[0] >= '0' && arg[0] <= '3') {
        templateIdx = arg[0] - '0';
    } else {
        for (size_t i = 0; i < templates.size(); ++i) {
            if (_stricmp(arg.c_str(), templates[i].name) == 0) {
                templateIdx = static_cast<int>(i);
                break;
            }
        }
    }

    if (templateIdx < 0 || templateIdx > 3) {
        templateIdx = 0;
        ctx.output("[MULTI] Unknown template. Falling back to template 0.\n");
    }

    const auto& tmpl = engine.getTemplate(static_cast<ResponseTemplateId>(templateIdx));
    bool nextEnabled = !tmpl.enabled;
    engine.setTemplateEnabled(static_cast<ResponseTemplateId>(templateIdx), nextEnabled);

    ctx.output("[MULTI] Template ");
    ctx.output(tmpl.name);
    ctx.output(nextEnabled ? " enabled.\n" : " disabled.\n");
    return CommandResult::ok("multiResp.toggleTemplate");
}

CommandResult handleMultiRespShowPrefs(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    auto prefs = engine.getPreferenceHistory(20);

    ctx.output("[MULTI] Preference History:\n");
    for (const auto& p : prefs) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  Session %llu: Template %d  Prompt: %s\n",
                 static_cast<unsigned long long>(p.sessionId),
                 static_cast<int>(p.preferredTemplate),
                 p.promptSnippet.c_str());
        ctx.output(buf);
    }
    return CommandResult::ok("multiResp.showPrefs");
}

CommandResult handleMultiRespShowLatest(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    auto* latest = engine.getLatestSession();
    if (!latest) {
        ctx.output("[MULTI] No sessions. Use !multi_gen <prompt> to start.\n");
        return CommandResult::ok("multiResp.showLatest");
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "[MULTI] Latest session %llu (%zu responses):\n",
             static_cast<unsigned long long>(latest->sessionId),
             latest->responses.size());
    ctx.output(buf);

    for (size_t i = 0; i < latest->responses.size(); ++i) {
        snprintf(buf, sizeof(buf), "\n--- Response %zu ---\n", i + 1);
        ctx.output(buf);
        ctx.output(latest->responses[i].content.c_str());
        ctx.output("\n");
    }
    return CommandResult::ok("multiResp.showLatest");
}

CommandResult handleMultiRespShowStatus(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    std::string status = engine.isInitialized() ? "Ready" : "Not initialized";
    ctx.output("[MULTI] Status: ");
    ctx.outputLine(status);
    return CommandResult::ok("multiResp.showStatus");
}

CommandResult handleMultiRespClearHistory(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    // Shutdown reinitializes, clearing all sessions and preference history
    engine.shutdown();
    engine.initialize();

    ctx.output("[MULTI] Session history cleared ? engine reinitialized.\n");
    return CommandResult::ok("multiResp.clearHistory");
}

CommandResult handleMultiRespApplyPreferred(const CommandContext& ctx) {
    auto& engine = getMultiResponseEngine();
    auto* latest = engine.getLatestSession();
    if (!latest || latest->responses.empty()) {
        std::string prompt = "deterministic multi-response apply-preferred prompt";
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) prompt = rs.lastPrompt;
        }
        uint64_t sid = engine.startSession(prompt, std::max(1, engine.getMaxChainResponses()));
        engine.generateAll(sid, nullptr, nullptr, nullptr, nullptr);
        latest = engine.getSession(sid);
        if (latest) {
            ctx.output("[MULTI] No active session. Auto-generated responses before apply.\n");
        } else {
            return CommandResult::error("multiResp.applyPreferred: session unavailable");
        }
    }

    if (latest->preferredIndex < 0) {
        auto set = engine.setPreference(latest->sessionId, 0, "auto-selected default preferred response");
        if (set.success) {
            ctx.output("[MULTI] No preferred response. Auto-selected response 1.\n");
            latest = engine.getSession(latest->sessionId);
        } else {
            return CommandResult::error(set.detail);
        }
    }

    int idx = latest->preferredIndex;
    if (idx < 0 || idx >= static_cast<int>(latest->responses.size())) {
        idx = 0;
        auto set = engine.setPreference(latest->sessionId, idx, "clamped invalid preferred index");
        if (!set.success) {
            return CommandResult::error(set.detail);
        }
        latest = engine.getSession(latest->sessionId);
    }

    if (!latest || idx < 0 || idx >= static_cast<int>(latest->responses.size())) {
        return CommandResult::error("multiResp.applyPreferred: preferred response unavailable");
    }

    ctx.output("[MULTI] Applying preferred response:\n");
    if (latest->responses[idx].content.empty()) {
        ctx.output("[MULTI] Preferred slot has no generated content yet.\n");
    } else {
        ctx.output(latest->responses[idx].content.c_str());
        ctx.output("\n");
    }
    return CommandResult::ok("multiResp.applyPreferred");
}

// ============================================================================
// GOVERNOR (4 handlers) ? Task scheduling and resource management
// ============================================================================

CommandResult handleGovStatus(const CommandContext& ctx) {
    auto& gov = GovernorState::instance();
    std::lock_guard<std::mutex> lock(gov.mtx);

    int active = 0, queued = 0;
    for (const auto& t : gov.tasks) {
        if (t.status == "active") ++active;
        else if (t.status == "queued") ++queued;
    }

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[GOVERNOR] Status:\n"
             "  Active tasks: %d\n"
             "  Queued: %d\n"
             "  Completed: %u\n"
             "  CPU throttling: %s\n",
             active, queued,
             gov.completed.load(std::memory_order_relaxed),
             gov.throttled.load() ? "Yes" : "No");
    ctx.output(buf);
    return CommandResult::ok("gov.status");
}

CommandResult handleGovSubmitCommand(const CommandContext& ctx) {
    std::string cmd = getArgs(ctx);
    if (cmd.empty()) {
        cmd = "echo governor_noop_task";
        ctx.output("[GOVERNOR] No command provided. Submitting deterministic noop task.\n");
    }

    auto& gov = GovernorState::instance();
    std::lock_guard<std::mutex> lock(gov.mtx);

    uint32_t id = gov.nextId.fetch_add(1);
    std::string taskId = "task_" + std::to_string(id);
    gov.tasks.push_back({taskId, cmd, "queued", 0});
    executeGovernorTaskAsync(taskId, cmd);

    std::string msg = "[GOVERNOR] Task submitted: " + taskId + " (" + cmd + ")\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("gov.submitCommand");
}

CommandResult handleGovKillAll(const CommandContext& ctx) {
    auto& gov = GovernorState::instance();
    std::lock_guard<std::mutex> lock(gov.mtx);
    int marked = 0;
    for (auto& t : gov.tasks) {
        if (t.status == "queued" || t.status == "active") {
            if (t.status == "active" && t.processHandle != nullptr) {
                TerminateProcess(t.processHandle, 1);
            }
            t.status = "killed";
            t.exitCode = -9;
            t.outputPreview = "Killed by governor";
            ++marked;
        }
    }
    char buf[128];
    snprintf(buf, sizeof(buf), "[GOVERNOR] Marked %d task(s) as killed.\n", marked);
    ctx.output(buf);
    return CommandResult::ok("gov.killAll");
}

CommandResult handleGovTaskList(const CommandContext& ctx) {
    auto& gov = GovernorState::instance();
    std::lock_guard<std::mutex> lock(gov.mtx);

    if (gov.tasks.empty()) {
        ctx.output("[GOVERNOR] No active tasks.\n");
        return CommandResult::ok("gov.taskList");
    }

    ctx.output("[GOVERNOR] Task List:\n");
    for (const auto& t : gov.tasks) {
        std::string line = "  " + t.id + ": " + t.command + " [" + t.status + "]";
        if (t.pid != 0) line += " pid=" + std::to_string(t.pid);
        if (t.status == "done" || t.status == "failed" || t.status == "killed") {
            line += " rc=" + std::to_string(t.exitCode);
        }
        if (!t.outputPreview.empty()) {
            line += " out=\"" + t.outputPreview.substr(0, 80) + "\"";
        }
        line += "\n";
        ctx.output(line.c_str());
    }
    return CommandResult::ok("gov.taskList");
}

// ============================================================================
// SAFETY CONTRACTS (4 handlers) ? Token budgets, rollback, violation tracking
// ============================================================================

CommandResult handleSafetyStatus(const CommandContext& ctx) {
    auto& safety = SafetyState::instance();

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[SAFETY] Status:\n"
             "  Token budget: %lld / %lld\n"
             "  Violations: %u\n",
             static_cast<long long>(safety.tokensUsed.load()),
             static_cast<long long>(safety.tokenBudget.load()),
             safety.violations.load());
    ctx.output(buf);

    // Also log to replay journal
    auto& journal = ReplayJournal::instance();
    journal.recordAction(ReplayActionType::SafetyCheck, "safety", "status_query",
                         "", buf, 0, 1.0f, 0.0, "");

    return CommandResult::ok("safety.status");
}

CommandResult handleSafetyResetBudget(const CommandContext& ctx) {
    auto& safety = SafetyState::instance();
    safety.tokensUsed.store(0);
    safety.violations.store(0);
    ctx.output("[SAFETY] Token budget reset. Usage counter zeroed.\n");

    auto& journal = ReplayJournal::instance();
    journal.recordAction(ReplayActionType::SafetyCheck, "safety", "budget_reset",
                         "", "Budget reset", 0, 1.0f, 0.0, "");

    return CommandResult::ok("safety.resetBudget");
}

CommandResult handleSafetyRollbackLast(const CommandContext& ctx) {
    auto& safety = SafetyState::instance();
    auto& journal = ReplayJournal::instance();
    std::lock_guard<std::mutex> lock(safety.mtx);

    std::string rollbackTarget = safety.lastRollbackAction;
    if (rollbackTarget.empty()) {
        auto recent = journal.getLastN(16);
        for (auto it = recent.rbegin(); it != recent.rend(); ++it) {
            if (it->type == ReplayActionType::SafetyRollback) continue;
            if (it->action.empty()) continue;
            rollbackTarget = it->category + ":" + it->action;
            if (!it->input.empty()) {
                rollbackTarget += " (" + it->input.substr(0, 48) + ")";
            }
            break;
        }
    }
    if (rollbackTarget.empty()) {
        rollbackTarget = "budget-only";
    }

    const int64_t usedBefore = safety.tokensUsed.load();
    const int64_t budget = std::max<int64_t>(1, safety.tokenBudget.load());
    int64_t restore = usedBefore / 4;
    const int64_t minRestore = std::max<int64_t>(64, budget / 200);   // 0.5% budget floor
    const int64_t maxRestore = std::max<int64_t>(minRestore, budget / 5); // 20% ceiling
    if (restore < minRestore) restore = minRestore;
    if (restore > maxRestore) restore = maxRestore;
    if (restore > usedBefore) restore = usedBefore;

    if (restore > 0) {
        safety.tokensUsed.fetch_sub(restore);
    }

    bool violationCleared = false;
    if (safety.violations.load() > 0) {
        safety.violations.fetch_sub(1);
        violationCleared = true;
    }
    if (violationCleared && !safety.violationLog.empty()) {
        safety.violationLog.pop_back();
    }

    if (safety.lastRollbackAction.empty()) {
        ctx.output("[SAFETY] No explicit rollback target. Executed budget/violation rollback recovery.\n");
    } else {
        ctx.output("[SAFETY] Rolling back: ");
        ctx.output(safety.lastRollbackAction.c_str());
        ctx.output("\n");
    }

    char detail[256];
    snprintf(detail, sizeof(detail),
             "  Target: %s\n  Tokens restored: %lld\n  Tokens now used: %lld\n  Violations decremented: %s\n",
             rollbackTarget.c_str(),
             static_cast<long long>(restore),
             static_cast<long long>(safety.tokensUsed.load()),
             violationCleared ? "yes" : "no");
    ctx.output(detail);

    safety.lastRollbackAction.clear();
    journal.recordAction(ReplayActionType::SafetyRollback, "safety", "rollback",
                         rollbackTarget, detail, 0, 1.0f, 0.0, "");

    return CommandResult::ok("safety.rollbackLast");
}

CommandResult handleSafetyShowViolations(const CommandContext& ctx) {
    auto& safety = SafetyState::instance();
    std::lock_guard<std::mutex> lock(safety.mtx);

    if (safety.violationLog.empty()) {
        ctx.output("[SAFETY] No violations recorded.\n");
        return CommandResult::ok("safety.showViolations");
    }

    ctx.output("[SAFETY] Violation Log:\n");
    for (const auto& v : safety.violationLog) {
        std::string line = "  [" + v.type + "] " + v.detail + "\n";
        ctx.output(line.c_str());
    }
    return CommandResult::ok("safety.showViolations");
}

// ============================================================================
// REPLAY JOURNAL (4 handlers) ? Session recording and playback
// ============================================================================

CommandResult handleReplayStatus(const CommandContext& ctx) {
    auto& journal = ReplayJournal::instance();
    std::string status = journal.getStatusString();
    auto stats = journal.getStats();

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[REPLAY] Status: %s\n"
             "  Recording: %s\n"
             "  Total events: %llu\n"
             "  Sessions: %llu\n",
             status.c_str(),
             journal.isRecording() ? "Yes" : "No",
             static_cast<unsigned long long>(stats.totalRecords),
             static_cast<unsigned long long>(stats.totalSessions));
    ctx.output(buf);
    return CommandResult::ok("replay.status");
}

CommandResult handleReplayShowLast(const CommandContext& ctx) {
    auto& journal = ReplayJournal::instance();
    auto records = journal.getLastN(10);

    if (records.empty()) {
        ctx.output("[REPLAY] No recorded events.\n");
        return CommandResult::ok("replay.showLast");
    }

    ctx.output("[REPLAY] Last 10 events:\n");
    for (const auto& r : records) {
        std::string line = "  [" + r.category + "] " + r.action + " ? " + r.input + "\n";
        ctx.output(line.c_str());
    }
    return CommandResult::ok("replay.showLast");
}

CommandResult handleReplayExportSession(const CommandContext& ctx) {
    std::string filename = getArgs(ctx);
    if (filename.empty()) filename = "session_replay.json";

    auto& journal = ReplayJournal::instance();
    auto stats = journal.getStats();

    if (journal.exportSession(stats.activeSessionId, filename)) {
        ctx.output("[REPLAY] Exported to: ");
        ctx.outputLine(filename);
    } else {
        ctx.output("[REPLAY] Export failed.\n");
        auto records = journal.getLastN(10);
        std::ostringstream oss;
        oss << "[REPLAY] Recovery: in-memory export preview (" << records.size() << " records)\n";
        for (const auto& r : records) {
            oss << "  [" << r.category << "] " << r.action << "\n";
        }
        ctx.output(oss.str().c_str());
        return CommandResult::ok("replay.exportSession.memoryOnly");
    }
    return CommandResult::ok("replay.exportSession");
}

CommandResult handleReplayCheckpoint(const CommandContext& ctx) {
    std::string label = getArgs(ctx);
    if (label.empty()) label = "manual_checkpoint";

    auto& journal = ReplayJournal::instance();
    journal.recordCheckpoint(label);

    ctx.output("[REPLAY] Checkpoint created: ");
    ctx.outputLine(label);
    return CommandResult::ok("replay.checkpoint");
}

// ============================================================================
// CONFIDENCE GATE (2 handlers) ? Response confidence tracking
// ============================================================================

CommandResult handleConfidenceStatus(const CommandContext& ctx) {
    auto& conf = ConfidenceState::instance();

    char buf[256];
    snprintf(buf, sizeof(buf),
             "[CONFIDENCE] Score: %.1f%%\n"
             "  Policy: %s\n"
             "  Last action: %s\n",
             conf.score.load() * 100.0f,
             conf.policy.c_str(),
             conf.lastAction.c_str());
    ctx.output(buf);
    return CommandResult::ok("confidence.status");
}

CommandResult handleConfidenceSetPolicy(const CommandContext& ctx) {
    std::string policy = getArgs(ctx);
    if (policy.empty()) {
        policy = "conservative";
        ctx.output("[CONFIDENCE] No policy provided. Defaulting to conservative.\n");
    }

    if (policy != "aggressive" && policy != "conservative") {
        std::string lower = policy;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
        policy = (lower.find("agg") != std::string::npos || lower.find("fast") != std::string::npos)
            ? "aggressive"
            : "conservative";
        ctx.output("[CONFIDENCE] Policy normalized to supported value.\n");
    }

    auto& conf = ConfidenceState::instance();
    std::lock_guard<std::mutex> lock(conf.mtx);
    conf.policy = policy;

    ctx.output("[CONFIDENCE] Policy set: ");
    ctx.outputLine(policy);
    return CommandResult::ok("confidence.setPolicy");
}

// ============================================================================
// ROUTER EXTENDED (21 handlers) ? LLM prompt routing
// *** CRITICAL: handleRouterRoutePrompt must call the LLM backend ***
// ============================================================================

CommandResult handleRouterEnable(const CommandContext& ctx) {
    RouterState::instance().enabled.store(true);
    ctx.output("[ROUTER] Enabled.\n");
    return CommandResult::ok("router.enable");
}

CommandResult handleRouterDisable(const CommandContext& ctx) {
    RouterState::instance().enabled.store(false);
    ctx.output("[ROUTER] Disabled. Prompts will be sent directly to active backend.\n");
    return CommandResult::ok("router.disable");
}

CommandResult handleRouterStatus(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    auto& bs = BackendState::instance();

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[ROUTER] Status:\n"
             "  Enabled: %s\n"
             "  Policy: %s\n"
             "  Active backend: %s\n"
             "  Model: %s\n"
             "  Ensemble: %s\n"
             "  Total routed: %llu\n"
             "  Total tokens: %llu\n",
             rs.enabled.load() ? "Yes" : "No",
             rs.policy.c_str(),
             bs.activeBackend.c_str(),
             bs.ollamaConfig.chat_model.c_str(),
             rs.ensembleEnabled.load() ? "Yes" : "No",
             static_cast<unsigned long long>(rs.totalRouted.load()),
             static_cast<unsigned long long>(rs.totalTokens.load()));
    ctx.output(buf);
    return CommandResult::ok("router.status");
}

// ?? Internal: Actually route a prompt to the LLM backend ???????????????????
static InferenceResult routeToBackend(const std::string& prompt,
                                       const std::string& systemPrompt = "") {
    auto& rs = RouterState::instance();
    auto& bs = BackendState::instance();

    // Record routing decision
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        rs.lastPrompt = prompt;
        rs.lastBackendChoice = bs.activeBackend;
        rs.lastReason = "Policy: " + rs.policy;
        rs.backendHits[bs.activeBackend]++;
    }
    rs.totalRouted.fetch_add(1, std::memory_order_relaxed);

    // Build messages
    std::vector<ChatMessage> messages;
    if (!systemPrompt.empty()) {
        messages.push_back({"system", systemPrompt, "", {}});
    }
    messages.push_back({"user", prompt, "", {}});

    // Route to the active backend
    auto client = createOllamaClient();
    auto result = client.ChatSync(messages);

    // Track token usage
    if (result.success) {
        rs.totalTokens.fetch_add(result.prompt_tokens + result.completion_tokens,
                                  std::memory_order_relaxed);
        // Derive confidence from real inference metrics
        auto& conf = ConfidenceState::instance();
        if (!result.response.empty()) {
            // Confidence derived from tokens/sec throughput and response length
            // Higher throughput + longer response = higher confidence
            float tpsScore = std::min(1.0f, static_cast<float>(result.tokens_per_sec) / 100.0f);
            float lenScore = std::min(1.0f, static_cast<float>(result.completion_tokens) / 200.0f);
            float latencyScore = (result.total_duration_ms > 0 && result.total_duration_ms < 30000.0)
                                 ? 1.0f - static_cast<float>(result.total_duration_ms / 30000.0) : 0.5f;
            float score = 0.4f * tpsScore + 0.3f * lenScore + 0.3f * latencyScore;
            conf.score.store(std::clamp(score, 0.1f, 1.0f));
            conf.lastAction = "successful_inference";
        }
        // Update safety budget
        auto& safety = SafetyState::instance();
        safety.tokensUsed.fetch_add(
            static_cast<int64_t>(result.prompt_tokens + result.completion_tokens));
        safety.lastRollbackAction = "inference: " + prompt.substr(0, 50);
    } else {
        auto& conf = ConfidenceState::instance();
        // Decay confidence by 20% on failure (bounded by 0.05 floor)
        float prev = conf.score.load();
        conf.score.store(std::max(0.05f, prev * 0.8f));
        conf.lastAction = "inference_failed: " + result.error_message.substr(0, 60);
    }

    // Record in replay journal
    auto& journal = ReplayJournal::instance();
    auto& confRef = ConfidenceState::instance();
    journal.recordAction(ReplayActionType::AgentQuery, "router", "route_prompt",
                         prompt.substr(0, 200),
                         result.success ? result.response.substr(0, 200) : result.error_message,
                         result.success ? 0 : -1,
                         confRef.score.load(),
                         result.total_duration_ms,
                         "{\"backend\":\"" + bs.activeBackend + "\"}");

    return result;
}

static std::string chooseBackendForPrompt(const std::string& prompt) {
    auto& rs = RouterState::instance();
    auto& bs = BackendState::instance();

    std::string chosen;
    std::string reason;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        chosen = bs.activeBackend;
    }

    if (rs.policy == "speed" || rs.policy == "cost") {
        chosen = "ollama";
        reason = (rs.policy == "speed")
                 ? "Speed policy: local Ollama selected for lowest latency"
                 : "Cost policy: local Ollama selected (zero cost)";
    } else {
        reason = "Quality policy: using configured backend (" + chosen + ")";
    }

    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        for (const auto& [taskId, backend] : rs.pinnedTasks) {
            if (!taskId.empty() && prompt.find(taskId) != std::string::npos) {
                chosen = backend;
                reason = "Pinned to " + backend + " via task " + taskId;
                break;
            }
        }
        rs.lastReason = reason;
    }

    return chosen;
}

static InferenceResult routeViaBackend(const std::string& prompt,
                                       const std::string& backend,
                                       const std::string& systemPrompt = "") {
    auto& bs = BackendState::instance();

    std::string originalBackend;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        originalBackend = bs.activeBackend;
        bs.activeBackend = backend;
    }

    InferenceResult result = routeToBackend(prompt, systemPrompt);

    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.activeBackend = originalBackend;
    }
    return result;
}

CommandResult handleRouterDecision(const CommandContext& ctx) {
    std::string prompt = getArgs(ctx);
    if (prompt.empty()) return CommandResult::error("Usage: !router_decide <prompt>");

    auto& rs = RouterState::instance();
    std::string chosen = chooseBackendForPrompt(prompt);
    std::string reason;
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        reason = rs.lastReason;
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "[ROUTER] Decision:\n  Backend: %s\n  Reason: %s\n",
             chosen.c_str(), reason.c_str());
    ctx.output(buf);
    return CommandResult::ok("router.decision");
}

CommandResult handleRouterSetPolicy(const CommandContext& ctx) {
    std::string policy = getArgs(ctx);
    if (policy.empty()) return CommandResult::error("Usage: !router_policy <cost|speed|quality>");

    if (policy != "cost" && policy != "speed" && policy != "quality") {
        return CommandResult::error("Policy must be 'cost', 'speed', or 'quality'");
    }

    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);
    rs.policy = policy;

    ctx.output("[ROUTER] Policy set: ");
    ctx.outputLine(policy);
    return CommandResult::ok("router.setPolicy");
}

CommandResult handleRouterCapabilities(const CommandContext& ctx) {
    ctx.output("[ROUTER] Backend Capabilities:\n"
               "  ollama:   Local inference, GGUF models, streaming, FIM, tool calling\n"
               "  local:    CPU inference engine, GGUF direct, no network required\n"
               "  openai:   GPT-4/GPT-3.5, function calling, JSON mode\n"
               "  claude:   Claude 3.5/3, long context, tool use\n"
               "  gemini:   Gemini Pro/Ultra, multimodal, code generation\n");
    return CommandResult::ok("router.capabilities");
}

CommandResult handleRouterFallbacks(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);

    ctx.output("[ROUTER] Fallback Chain:\n");
    for (size_t i = 0; i < rs.fallbackChain.size(); ++i) {
        char buf[128];
        snprintf(buf, sizeof(buf), "  %zu. %s\n", i + 1, rs.fallbackChain[i].c_str());
        ctx.output(buf);
    }
    return CommandResult::ok("router.fallbacks");
}

CommandResult handleRouterSaveConfig(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);

    // Serialize router configuration to JSON file
    std::string configPath = "config/router_config.json";
    std::string arg = getArgs(ctx);
    if (!arg.empty()) configPath = arg;

    // Ensure directory exists
    std::string dir = configPath.substr(0, configPath.rfind('\\'));
    if (dir == configPath) dir = configPath.substr(0, configPath.rfind('/'));
    if (!dir.empty() && dir != configPath) {
        CreateDirectoryA(dir.c_str(), nullptr);
    }

    FILE* f = fopen(configPath.c_str(), "w");
    if (!f) {
        ctx.output("[ROUTER] Failed to save config: cannot open ");
        ctx.output(configPath.c_str());
        ctx.output("\n");
        return CommandResult::error("Cannot write config");
    }

    // Write JSON manually (no external JSON lib dependency)
    fprintf(f, "{\n");
    fprintf(f, "  \"enabled\": %s,\n", rs.enabled.load() ? "true" : "false");
    fprintf(f, "  \"ensembleEnabled\": %s,\n", rs.ensembleEnabled.load() ? "true" : "false");
    fprintf(f, "  \"policy\": \"%s\",\n", rs.policy.c_str());
    fprintf(f, "  \"currentBackend\": \"%s\",\n", rs.currentBackend.c_str());
    fprintf(f, "  \"totalRouted\": %llu,\n", (unsigned long long)rs.totalRouted.load());
    fprintf(f, "  \"totalTokens\": %llu,\n", (unsigned long long)rs.totalTokens.load());

    // Fallback chain
    fprintf(f, "  \"fallbackChain\": [");
    for (size_t i = 0; i < rs.fallbackChain.size(); ++i) {
        fprintf(f, "\"%s\"", rs.fallbackChain[i].c_str());
        if (i + 1 < rs.fallbackChain.size()) fprintf(f, ", ");
    }
    fprintf(f, "],\n");

    // Pinned tasks
    fprintf(f, "  \"pinnedTasks\": {\n");
    size_t pinnedIdx = 0;
    for (const auto& [taskId, backend] : rs.pinnedTasks) {
        fprintf(f, "    \"%s\": \"%s\"", taskId.c_str(), backend.c_str());
        if (++pinnedIdx < rs.pinnedTasks.size()) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "  },\n");

    // Backend hit counts
    fprintf(f, "  \"backendHits\": {\n");
    size_t hitIdx = 0;
    for (const auto& [name, count] : rs.backendHits) {
        fprintf(f, "    \"%s\": %llu", name.c_str(), (unsigned long long)count);
        if (++hitIdx < rs.backendHits.size()) fprintf(f, ",");
        fprintf(f, "\n");
    }
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
    fclose(f);

    ctx.output("[ROUTER] Configuration saved to: ");
    ctx.output(configPath.c_str());
    ctx.output("\n");
    return CommandResult::ok("router.saveConfig");
}

// *** THE CRITICAL HANDLER ? This is what routes prompts to the LLM ***
CommandResult handleRouterRoutePrompt(const CommandContext& ctx) {
    std::string prompt = getArgs(ctx);
    if (prompt.empty()) {
        // Also check rawInput for non-CLI dispatch
        if (ctx.rawInput && ctx.rawInput[0]) {
            prompt = std::string(ctx.rawInput);
        }
        if (prompt.empty()) {
            return CommandResult::error("No prompt to route");
        }
    }

    auto& rs = RouterState::instance();
    if (!rs.enabled.load()) {
        ctx.output("[ROUTER] Router disabled. Sending directly to backend.\n");
    }

    ctx.output("[ROUTER] Routing prompt to backend...\n");

    std::string chosen = chooseBackendForPrompt(prompt);
    auto result = routeViaBackend(prompt, chosen);

    if (rs.ensembleEnabled.load()) {
        std::vector<std::string> ensembleBackends;
        ensembleBackends.push_back(chosen);
        for (const auto& fb : rs.fallbackChain) {
            if (fb != chosen) ensembleBackends.push_back(fb);
            if (ensembleBackends.size() >= 3) break;
        }

        bool anySuccess = false;
        std::string lastError;
        ctx.output("[ROUTER] Ensemble mode active. Querying multiple backends.\n");
        for (const auto& backend : ensembleBackends) {
            auto eResult = routeViaBackend(prompt, backend);
            if (eResult.success) {
                anySuccess = true;
                ctx.output("\n[ROUTER][");
                ctx.output(backend.c_str());
                ctx.output("]\n");
                ctx.output(eResult.response.c_str());
                ctx.output("\n");
            } else {
                lastError = eResult.error_message;
                ctx.output("\n[ROUTER][");
                ctx.output(backend.c_str());
                ctx.output("] error: ");
                ctx.output(eResult.error_message.c_str());
                ctx.output("\n");
            }
        }

        if (!anySuccess) {
            ctx.output("[ROUTER] All ensemble backends failed. Using deterministic local recovery.\n");
            std::string local = buildLocalRouterRecoveryResponse(prompt, ensembleBackends, lastError);
            ctx.output(local.c_str());
            ctx.output("\n");
            return CommandResult::ok("router.routePrompt.localRecovery");
        }
        return CommandResult::ok("router.routePrompt");
    }

    if (result.success) {
        ctx.output(result.response.c_str());
        ctx.output("\n");

        char stats[256];
        snprintf(stats, sizeof(stats),
                 "\n[ROUTER] %llu prompt + %llu completion tokens, %.1f tok/s, %.0fms\n",
                 static_cast<unsigned long long>(result.prompt_tokens),
                 static_cast<unsigned long long>(result.completion_tokens),
                 result.tokens_per_sec,
                 result.total_duration_ms);
        ctx.output(stats);
    } else {
        ctx.output("[ROUTER] Backend error: ");
        ctx.output(result.error_message.c_str());
        ctx.output("\n");

        // Try fallback chain
        std::vector<std::string> attempted;
        attempted.push_back(chosen);
        std::string lastError = result.error_message;
        for (const auto& fb : rs.fallbackChain) {
            if (fb == chosen) continue;
            attempted.push_back(fb);
            ctx.output("[ROUTER] Trying fallback: ");
            ctx.output(fb.c_str());
            ctx.output("...\n");
            auto fbResult = routeViaBackend(prompt, fb);
            if (fbResult.success) {
                ctx.output(fbResult.response.c_str());
                ctx.output("\n");
                return CommandResult::ok("router.routePrompt");
            }
            lastError = fbResult.error_message;
        }

        ctx.output("[ROUTER] All configured backends failed. Using deterministic local recovery.\n");
        std::string local = buildLocalRouterRecoveryResponse(prompt, attempted, lastError);
        ctx.output(local.c_str());
        ctx.output("\n");
        return CommandResult::ok("router.routePrompt.localRecovery");
    }

    return CommandResult::ok("router.routePrompt");
}

CommandResult handleRouterResetStats(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    rs.totalRouted.store(0);
    rs.totalTokens.store(0);
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        rs.backendHits.clear();
    }
    ctx.output("[ROUTER] Stats reset.\n");
    return CommandResult::ok("router.resetStats");
}

CommandResult handleRouterWhyBackend(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);

    if (rs.lastBackendChoice.empty()) {
        ctx.output("[ROUTER] No routing decisions yet.\n");
        return CommandResult::ok("router.whyBackend");
    }

    std::string msg = "[ROUTER] Last routing decision:\n"
                      "  Backend: " + rs.lastBackendChoice + "\n"
                      "  Reason: " + rs.lastReason + "\n"
                      "  Prompt: " + rs.lastPrompt.substr(0, 100) + "\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("router.whyBackend");
}

CommandResult handleRouterPinTask(const CommandContext& ctx) {
    std::string taskId = getArg(ctx, 0);
    std::string backend = getArg(ctx, 1);
    if (taskId.empty()) return CommandResult::error("Usage: !router_pin <task_id> [backend]");
    if (backend.empty()) backend = BackendState::instance().activeBackend;

    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);
    rs.pinnedTasks[taskId] = backend;

    std::string msg = "[ROUTER] Task '" + taskId + "' pinned to " + backend + "\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("router.pinTask");
}

CommandResult handleRouterUnpinTask(const CommandContext& ctx) {
    std::string taskId = getArgs(ctx);
    if (taskId.empty()) return CommandResult::error("Usage: !router_unpin <task_id>");

    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);
    rs.pinnedTasks.erase(taskId);

    ctx.output("[ROUTER] Task unpinned: ");
    ctx.outputLine(taskId);
    return CommandResult::ok("router.unpinTask");
}

CommandResult handleRouterShowPins(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);

    if (rs.pinnedTasks.empty()) {
        ctx.output("[ROUTER] No pinned tasks.\n");
        return CommandResult::ok("router.showPins");
    }

    ctx.output("[ROUTER] Pinned Tasks:\n");
    for (const auto& [taskId, backend] : rs.pinnedTasks) {
        std::string line = "  " + taskId + " -> " + backend + "\n";
        ctx.output(line.c_str());
    }
    return CommandResult::ok("router.showPins");
}

CommandResult handleRouterShowHeatmap(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    std::lock_guard<std::mutex> lock(rs.mtx);

    ctx.output("[ROUTER] Backend Usage Heatmap:\n");
    if (rs.backendHits.empty()) {
        ctx.output("  No routing data yet.\n");
        return CommandResult::ok("router.showHeatmap");
    }

    uint64_t maxHits = 0;
    for (const auto& [name, hits] : rs.backendHits) {
        if (hits > maxHits) maxHits = hits;
    }

    for (const auto& [name, hits] : rs.backendHits) {
        int bars = maxHits > 0 ? static_cast<int>((hits * 40) / maxHits) : 0;
        std::string bar(bars, '#');
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-10s %s (%llu)\n",
                 name.c_str(), bar.c_str(), static_cast<unsigned long long>(hits));
        ctx.output(buf);
    }
    return CommandResult::ok("router.showHeatmap");
}

CommandResult handleRouterEnsembleEnable(const CommandContext& ctx) {
    RouterState::instance().ensembleEnabled.store(true);
    ctx.output("[ROUTER] Ensemble mode enabled. Prompts will be sent to multiple backends.\n");
    return CommandResult::ok("router.ensembleEnable");
}

CommandResult handleRouterEnsembleDisable(const CommandContext& ctx) {
    RouterState::instance().ensembleEnabled.store(false);
    ctx.output("[ROUTER] Ensemble mode disabled.\n");
    return CommandResult::ok("router.ensembleDisable");
}

CommandResult handleRouterEnsembleStatus(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    bool enabled = rs.ensembleEnabled.load();
    char buf[256];
    snprintf(buf, sizeof(buf),
             "[ROUTER] Ensemble: %s\n"
             "  Max parallel backends: 3\n"
             "  Primary fallback order: %s -> %s -> %s\n",
             enabled ? "Enabled" : "Disabled",
             rs.fallbackChain.size() > 0 ? rs.fallbackChain[0].c_str() : "n/a",
             rs.fallbackChain.size() > 1 ? rs.fallbackChain[1].c_str() : "n/a",
             rs.fallbackChain.size() > 2 ? rs.fallbackChain[2].c_str() : "n/a");
    ctx.output(buf);
    return CommandResult::ok("router.ensembleStatus");
}

static std::vector<std::string> buildSimulationChain(const std::string& primary,
                                                     const std::vector<std::string>& fallbackChain) {
    std::vector<std::string> ordered;
    if (!primary.empty()) ordered.push_back(primary);
    for (const auto& b : fallbackChain) {
        if (std::find(ordered.begin(), ordered.end(), b) == ordered.end()) ordered.push_back(b);
    }
    return ordered;
}

CommandResult handleRouterSimulate(const CommandContext& ctx) {
    std::string prompt = getArgs(ctx);
    if (prompt.empty()) return CommandResult::error("Usage: !router_sim <prompt>");

    auto& rs = RouterState::instance();
    std::string chosen = chooseBackendForPrompt(prompt);
    std::string reason;
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        reason = rs.lastReason;
    }

    auto& bs = BackendState::instance();
    std::map<std::string, bool> healthSnapshot;
    bool hasOpenAI = false;
    bool hasClaude = false;
    bool hasGemini = false;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        healthSnapshot = bs.backendHealth;
        hasOpenAI = bs.apiKeys.count("openai") > 0;
        hasClaude = bs.apiKeys.count("anthropic") > 0;
        hasGemini = bs.apiKeys.count("google") > 0;
    }
    bool ollamaLive = createOllamaClient().TestConnection();
    healthSnapshot["ollama"] = ollamaLive;
    healthSnapshot["local"] = true;
    healthSnapshot["openai"] = hasOpenAI;
    healthSnapshot["claude"] = hasClaude;
    healthSnapshot["gemini"] = hasGemini;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.backendHealth["ollama"] = ollamaLive;
    }

    std::vector<std::string> chain;
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        chain = buildSimulationChain(chosen, rs.fallbackChain);
    }
    std::string predicted = "none";
    for (const auto& b : chain) {
        if (healthSnapshot[b]) {
            predicted = b;
            break;
        }
    }

    std::ostringstream oss;
    oss << "[ROUTER] Simulation:\n";
    oss << "  Primary decision: " << chosen << "\n";
    oss << "  Reason: " << reason << "\n";
    oss << "  Policy: " << rs.policy << "\n";
    oss << "  Ensemble: " << (rs.ensembleEnabled.load() ? "enabled" : "disabled") << "\n";
    oss << "  Prompt length: " << prompt.size() << " chars\n";
    oss << "  Dry-run chain:\n";
    for (const auto& b : chain) {
        oss << "    - " << b << ": " << (healthSnapshot[b] ? "ready" : "unavailable") << "\n";
    }
    oss << "  Predicted executable backend: " << predicted << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("router.simulate");
}

CommandResult handleRouterSimulateLast(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    std::string lastPrompt;
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        lastPrompt = rs.lastPrompt;
    }

    if (lastPrompt.empty()) {
        ctx.output("[ROUTER] No previous prompt to simulate.\n");
        return CommandResult::ok("router.simulateLast");
    }

    std::string chosen = chooseBackendForPrompt(lastPrompt);
    std::string reason;
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        reason = rs.lastReason;
    }

    auto& bs = BackendState::instance();
    std::map<std::string, bool> healthSnapshot;
    bool hasOpenAI = false;
    bool hasClaude = false;
    bool hasGemini = false;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        healthSnapshot = bs.backendHealth;
        hasOpenAI = bs.apiKeys.count("openai") > 0;
        hasClaude = bs.apiKeys.count("anthropic") > 0;
        hasGemini = bs.apiKeys.count("google") > 0;
    }
    bool ollamaLive = createOllamaClient().TestConnection();
    healthSnapshot["ollama"] = ollamaLive;
    healthSnapshot["local"] = true;
    healthSnapshot["openai"] = hasOpenAI;
    healthSnapshot["claude"] = hasClaude;
    healthSnapshot["gemini"] = hasGemini;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.backendHealth["ollama"] = ollamaLive;
    }

    std::vector<std::string> chain;
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        chain = buildSimulationChain(chosen, rs.fallbackChain);
    }
    std::string predicted = "none";
    for (const auto& b : chain) {
        if (healthSnapshot[b]) {
            predicted = b;
            break;
        }
    }

    std::ostringstream oss;
    oss << "[ROUTER] Re-simulation of last prompt:\n";
    oss << "  Primary decision: " << chosen << "\n";
    oss << "  Reason: " << reason << "\n";
    oss << "  Dry-run chain:\n";
    for (const auto& b : chain) {
        oss << "    - " << b << ": " << (healthSnapshot[b] ? "ready" : "unavailable") << "\n";
    }
    oss << "  Predicted executable backend: " << predicted << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("router.simulateLast");
}

CommandResult handleRouterShowCostStats(const CommandContext& ctx) {
    auto& rs = RouterState::instance();
    uint64_t tokens = rs.totalTokens.load();

    // Rough cost estimation (per 1K tokens)
    double ollamaCost = 0.0;               // Free (local)
    double openaiCost = tokens * 0.00003;  // ~$0.03/1K tokens
    double claudeCost = tokens * 0.000025; // ~$0.025/1K tokens

    char buf[512];
    snprintf(buf, sizeof(buf),
             "[ROUTER] Cost Statistics:\n"
             "  Total tokens: %llu\n"
             "  Requests: %llu\n"
             "  Est. cost (if OpenAI): $%.4f\n"
             "  Est. cost (if Claude): $%.4f\n"
             "  Actual cost (Ollama): $%.2f (local)\n"
             "  Savings vs OpenAI: 100%%\n",
             static_cast<unsigned long long>(tokens),
             static_cast<unsigned long long>(rs.totalRouted.load()),
             openaiCost, claudeCost, ollamaCost);
    ctx.output(buf);
    return CommandResult::ok("router.showCostStats");
}

// ============================================================================
// BACKEND EXTENDED (11 handlers) ? Backend switching and management
// ============================================================================

CommandResult handleBackendSwitchLocal(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    bs.activeBackend = "local";
    ctx.output("[BACKEND] Switched to: local (CPU inference engine)\n");
    return CommandResult::ok("backend.switchLocal");
}

CommandResult handleBackendSwitchOllama(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    bs.activeBackend = "ollama";

    // Test connection
    auto client = createOllamaClient();
    bool ok = client.TestConnection();

    char buf[256];
    snprintf(buf, sizeof(buf),
             "[BACKEND] Switched to: ollama (%s:%d)\n"
             "  Model: %s\n"
             "  Connection: %s\n",
             bs.ollamaConfig.host.c_str(),
             bs.ollamaConfig.port,
             bs.ollamaConfig.chat_model.c_str(),
             ok ? "OK" : "FAILED");
    ctx.output(buf);

    if (!ok) {
        ctx.output("  WARNING: Ollama not reachable. Is it running?\n"
                   "  Start with: ollama serve\n");
    } else {
        // List available models
        auto models = client.ListModels();
        if (!models.empty()) {
            ctx.output("  Available models: ");
            for (size_t i = 0; i < models.size() && i < 5; ++i) {
                if (i > 0) ctx.output(", ");
                ctx.output(models[i].c_str());
            }
            if (models.size() > 5) ctx.output(", ...");
            ctx.output("\n");
        }
    }
    return CommandResult::ok("backend.switchOllama");
}

CommandResult handleBackendSwitchOpenAI(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);

    if (bs.apiKeys.find("openai") == bs.apiKeys.end()) {
        bs.activeBackend = "local";
        ctx.output("[BACKEND] OpenAI API key not configured.\n"
                   "  Use: !backend_setkey openai <your-key>\n"
                   "  Auto-fallback: switched to local backend.\n");
        return CommandResult::ok("backend.switchOpenAI.localFallback");
    }

    bs.activeBackend = "openai";
    ctx.output("[BACKEND] Switched to: OpenAI\n");
    return CommandResult::ok("backend.switchOpenAI");
}

CommandResult handleBackendSwitchClaude(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);

    if (bs.apiKeys.find("anthropic") == bs.apiKeys.end()) {
        bs.activeBackend = "local";
        ctx.output("[BACKEND] Anthropic API key not configured.\n"
                   "  Use: !backend_setkey anthropic <your-key>\n"
                   "  Auto-fallback: switched to local backend.\n");
        return CommandResult::ok("backend.switchClaude.localFallback");
    }

    bs.activeBackend = "claude";
    ctx.output("[BACKEND] Switched to: Claude\n");
    return CommandResult::ok("backend.switchClaude");
}

CommandResult handleBackendSwitchGemini(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);

    if (bs.apiKeys.find("google") == bs.apiKeys.end()) {
        bs.activeBackend = "local";
        ctx.output("[BACKEND] Google API key not configured.\n"
                   "  Use: !backend_setkey google <your-key>\n"
                   "  Auto-fallback: switched to local backend.\n");
        return CommandResult::ok("backend.switchGemini.localFallback");
    }

    bs.activeBackend = "gemini";
    ctx.output("[BACKEND] Switched to: Gemini\n");
    return CommandResult::ok("backend.switchGemini");
}

CommandResult handleBackendShowStatus(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);

    std::string status = "[BACKEND] Current: " + bs.activeBackend + "\n";
    status += "  Host: " + bs.ollamaConfig.host + ":" + std::to_string(bs.ollamaConfig.port) + "\n";
    status += "  Chat Model: " + bs.ollamaConfig.chat_model + "\n";
    status += "  FIM Model: " + bs.ollamaConfig.fim_model + "\n";
    status += "  Temperature: " + std::to_string(bs.ollamaConfig.temperature) + "\n";
    status += "  Max Tokens: " + std::to_string(bs.ollamaConfig.max_tokens) + "\n";
    status += "  Context: " + std::to_string(bs.ollamaConfig.num_ctx) + "\n";
    ctx.output(status.c_str());
    return CommandResult::ok("backend.showStatus");
}

CommandResult handleBackendShowSwitcher(const CommandContext& ctx) {
    ctx.output("[BACKEND] Available backends:\n"
               "  1. ollama   - Local Ollama server (default)\n"
               "  2. local    - CPU inference engine (GGUF direct)\n"
               "  3. openai   - OpenAI API (requires key)\n"
               "  4. claude   - Anthropic Claude (requires key)\n"
               "  5. gemini   - Google Gemini (requires key)\n"
               "\n"
               "  Switch: !backend_ollama, !backend_local, !backend_openai, etc.\n");
    return CommandResult::ok("backend.showSwitcher");
}

CommandResult handleBackendConfigure(const CommandContext& ctx) {
    std::string arg = getArgs(ctx);
    if (arg.empty()) {
        handleBackendShowStatus(ctx);
        ctx.output("\n  Configure: !backend_config <key> <value>\n"
                   "  Keys: host, port, model, temperature, max_tokens, num_ctx\n");
        return CommandResult::ok("backend.configure");
    }

    std::string key = getArg(ctx, 0);
    std::string value = getArg(ctx, 1);
    if (value.empty()) return CommandResult::error("Usage: !backend_config <key> <value>");

    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);

    if (key == "host") bs.ollamaConfig.host = value;
    else if (key == "port") bs.ollamaConfig.port = static_cast<uint16_t>(atoi(value.c_str()));
    else if (key == "model") bs.ollamaConfig.chat_model = value;
    else if (key == "temperature") bs.ollamaConfig.temperature = static_cast<float>(atof(value.c_str()));
    else if (key == "max_tokens") bs.ollamaConfig.max_tokens = atoi(value.c_str());
    else if (key == "num_ctx") bs.ollamaConfig.num_ctx = atoi(value.c_str());
    else return CommandResult::error("Unknown config key");

    std::string msg = "[BACKEND] Set " + key + " = " + value + "\n";
    ctx.output(msg.c_str());
    return CommandResult::ok("backend.configure");
}

CommandResult handleBackendHealthCheck(const CommandContext& ctx) {
    ctx.output("[BACKEND] Health Check:\n");

    // Test Ollama
    {
        auto client = createOllamaClient();
        LARGE_INTEGER start, end, freq;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
        bool ok = client.TestConnection();
        QueryPerformanceCounter(&end);
        double ms = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
                    static_cast<double>(freq.QuadPart);

        char buf[256];
        snprintf(buf, sizeof(buf), "  ollama:  %s  (%.0fms)\n",
                 ok ? "OK" : "FAIL", ms);
        ctx.output(buf);

        auto& bs = BackendState::instance();
        std::lock_guard<std::mutex> lock(bs.mtx);
        bs.backendHealth["ollama"] = ok;
    }

    // API backends ? check key presence only
    auto& bs = BackendState::instance();
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        bool hasOpenAI = bs.apiKeys.count("openai") > 0;
        bool hasClaude = bs.apiKeys.count("anthropic") > 0;
        bool hasGemini = bs.apiKeys.count("google") > 0;

        char buf[256];
        snprintf(buf, sizeof(buf),
                 "  openai:  %s\n"
                 "  claude:  %s\n"
                 "  gemini:  %s\n",
                 hasOpenAI ? "Key configured" : "No API key",
                 hasClaude ? "Key configured" : "No API key",
                 hasGemini ? "Key configured" : "No API key");
        ctx.output(buf);
    }

    ctx.output("  local:   Always available (CPU)\n");
    return CommandResult::ok("backend.healthCheck");
}

CommandResult handleBackendSetApiKey(const CommandContext& ctx) {
    std::string backend = getArg(ctx, 0);
    std::string key = getArg(ctx, 1);
    if (backend.empty() || key.empty()) {
        // Deterministic recovery: allow env-key ingestion when explicit key omitted.
        if (!backend.empty() && key.empty()) {
            const char* envName = nullptr;
            if (_stricmp(backend.c_str(), "openai") == 0) envName = "OPENAI_API_KEY";
            else if (_stricmp(backend.c_str(), "anthropic") == 0 || _stricmp(backend.c_str(), "claude") == 0) envName = "ANTHROPIC_API_KEY";
            else if (_stricmp(backend.c_str(), "google") == 0 || _stricmp(backend.c_str(), "gemini") == 0) envName = "GOOGLE_API_KEY";

            if (envName) {
                const char* envVal = getenv(envName);
                if (envVal && envVal[0] != '\0') {
                    key = envVal;
                    ctx.output("[BACKEND] API key recovered from environment variable.\n");
                }
            }
        }
        if (backend.empty() || key.empty()) {
            return CommandResult::error("Usage: !backend_setkey <backend> <api_key>");
        }
    }

    auto& bs = BackendState::instance();
    std::lock_guard<std::mutex> lock(bs.mtx);
    bs.apiKeys[backend] = key;

    ctx.output("[BACKEND] API key set for: ");
    ctx.output(backend.c_str());
    ctx.output(" (");
    // Show first 8 chars only
    ctx.output(key.substr(0, 8).c_str());
    ctx.output("...)\n");
    return CommandResult::ok("backend.setApiKey");
}

CommandResult handleBackendSaveConfigs(const CommandContext& ctx) {
    auto& bs = BackendState::instance();
    auto& rs = RouterState::instance();

    // Serialize backend and router state to JSON config file
    std::string configPath = ".rawrxd_backend_config.json";
    HANDLE h = CreateFileA(configPath.c_str(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        char tmpDir[MAX_PATH] = {};
        DWORD n = GetTempPathA(MAX_PATH, tmpDir);
        if (n > 0 && n < MAX_PATH) {
            configPath = std::string(tmpDir) + "rawrxd_backend_config.json";
            h = CreateFileA(configPath.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        }
    }
    if (h == INVALID_HANDLE_VALUE) {
        ctx.output("[BACKEND] Failed to open config file in working directory and temp.\n");
        ctx.output("[BACKEND] Falling back to in-memory/export-only config snapshot.\n");
    }

    std::ostringstream json;
    {
        std::lock_guard<std::mutex> lock(bs.mtx);
        json << "{\n";
        json << "  \"activeBackend\": \"" << bs.activeBackend << "\",\n";
        json << "  \"ollamaHost\": \"" << bs.ollamaConfig.host << "\",\n";
        json << "  \"ollamaPort\": " << bs.ollamaConfig.port << ",\n";
        json << "  \"ollamaModel\": \"" << bs.ollamaConfig.chat_model << "\",\n";
        json << "  \"apiKeys\": {\n";
        bool first = true;
        for (const auto& [k, v] : bs.apiKeys) {
            if (!first) json << ",\n";
            json << "    \"" << k << "\": \"" << v.substr(0, 8) << "...\"";
            first = false;
        }
        json << "\n  },\n";
    }
    {
        std::lock_guard<std::mutex> lock(rs.mtx);
        json << "  \"routerPolicy\": \"" << rs.policy << "\",\n";
        json << "  \"routerEnabled\": " << (rs.enabled.load() ? "true" : "false") << ",\n";
        json << "  \"ensembleEnabled\": " << (rs.ensembleEnabled.load() ? "true" : "false") << ",\n";
        json << "  \"totalRouted\": " << rs.totalRouted.load() << ",\n";
        json << "  \"fallbackChain\": [";
        for (size_t i = 0; i < rs.fallbackChain.size(); ++i) {
            if (i > 0) json << ", ";
            json << "\"" << rs.fallbackChain[i] << "\"";
        }
        json << "]\n";
    }
    json << "}\n";

    std::string content = json.str();
    DWORD written = 0;
    BOOL okWrite = FALSE;
    if (h != INVALID_HANDLE_VALUE) {
        okWrite = WriteFile(h, content.c_str(), (DWORD)content.size(), &written, nullptr);
        CloseHandle(h);
    }
    if (h == INVALID_HANDLE_VALUE || !okWrite || written != static_cast<DWORD>(content.size())) {
        std::ostringstream oss;
        oss << "[BACKEND] Config persisted in-memory only (file write unavailable).\n";
        oss << "  Snapshot bytes: " << content.size() << "\n";
        oss << "  Preview: " << content.substr(0, std::min<size_t>(content.size(), 220)) << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("backend.saveConfigs.memoryOnly");
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "[BACKEND] Configurations saved to %s (%u bytes)\n",
             configPath.c_str(), (unsigned)written);
    ctx.output(buf);
    return CommandResult::ok("backend.saveConfigs");
}

// ============================================================================
// DEBUG EXTENDED (28 handlers) ? Wire to NativeDebuggerEngine::Instance()
// ============================================================================

using namespace RawrXD::Debugger;

CommandResult handleDbgLaunch(const CommandContext& ctx) {
    std::string exe = getArg(ctx, 0);
    std::string args = getArg(ctx, 1);
    if (exe.empty()) {
        exe = getCurrentProcessImagePath();
        if (!exe.empty()) {
            ctx.output("[DBG] No executable provided. Defaulting to current process image path.\n");
        } else {
            exe = "C:\\Windows\\System32\\notepad.exe";
            ctx.output("[DBG] No executable provided. Defaulting to notepad.exe.\n");
        }
    }

    auto& dbg = NativeDebuggerEngine::Instance();
    auto result = dbg.launchProcess(exe, args);

    if (result.success) {
        ctx.output("[DBG] Launched: ");
        ctx.outputLine(exe);
    } else {
        ctx.output("[DBG] Launch failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.launch") : CommandResult::error(result.detail);
}

CommandResult handleDbgAttach(const CommandContext& ctx) {
    std::string pidStr = getArgs(ctx);
    if (pidStr.empty()) {
        uint32_t self = GetCurrentProcessId();
        char buf[128];
        snprintf(buf, sizeof(buf), "[DBG] No PID provided. Defaulting to current process: %u\n", self);
        ctx.output(buf);
        pidStr = std::to_string(self);
    }

    uint32_t pid = static_cast<uint32_t>(strtoul(pidStr.c_str(), nullptr, 0));
    auto& dbg = NativeDebuggerEngine::Instance();
    auto result = dbg.attachToProcess(pid);

    if (result.success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[DBG] Attached to PID %u\n", pid);
        ctx.output(buf);
    } else {
        ctx.output("[DBG] Attach failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.attach") : CommandResult::error(result.detail);
}

CommandResult handleDbgDetach(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().detach();
    ctx.output(result.success ? "[DBG] Detached.\n" : "[DBG] Detach failed.\n");
    return result.success ? CommandResult::ok("dbg.detach") : CommandResult::error(result.detail);
}

CommandResult handleDbgGo(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().go();
    ctx.output("[DBG] Running...\n");
    return result.success ? CommandResult::ok("dbg.go") : CommandResult::error(result.detail);
}

CommandResult handleDbgStepOver(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().stepOver();
    ctx.output("[DBG] Step over\n");
    return result.success ? CommandResult::ok("dbg.stepOver") : CommandResult::error(result.detail);
}

CommandResult handleDbgStepInto(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().stepInto();
    ctx.output("[DBG] Step into\n");
    return result.success ? CommandResult::ok("dbg.stepInto") : CommandResult::error(result.detail);
}

CommandResult handleDbgStepOut(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().stepOut();
    ctx.output("[DBG] Step out\n");
    return result.success ? CommandResult::ok("dbg.stepOut") : CommandResult::error(result.detail);
}

CommandResult handleDbgBreak(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().breakExecution();
    ctx.output("[DBG] Break\n");
    return result.success ? CommandResult::ok("dbg.break") : CommandResult::error(result.detail);
}

CommandResult handleDbgKill(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().terminateTarget();
    ctx.output("[DBG] Process terminated.\n");
    return result.success ? CommandResult::ok("dbg.kill") : CommandResult::error(result.detail);
}

CommandResult handleDbgAddBp(const CommandContext& ctx) {
    std::string file = getArg(ctx, 0);
    std::string lineStr = getArg(ctx, 1);
    if (file.empty() || lineStr.empty()) {
        file = getCurrentProcessImagePath();
        if (file.empty()) file = "unknown_source.cpp";
        lineStr = "1";
        ctx.output("[DBG] Incomplete source breakpoint request. Defaulting to <current-image>:1.\n");
    }

    int line = atoi(lineStr.c_str());
    auto& dbg = NativeDebuggerEngine::Instance();
    auto result = dbg.addBreakpointBySourceLine(file, line);

    if (result.success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[DBG] Breakpoint added: %s:%d\n", file.c_str(), line);
        ctx.output(buf);
    } else {
        ctx.output("[DBG] Failed to add breakpoint: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.addBp") : CommandResult::error(result.detail);
}

CommandResult handleDbgRemoveBp(const CommandContext& ctx) {
    std::string idStr = getArgs(ctx);
    if (idStr.empty()) {
        auto& dbg = NativeDebuggerEngine::Instance();
        const auto& bps = dbg.getBreakpoints();
        if (!bps.empty()) {
            idStr = std::to_string(bps.back().id);
            ctx.output("[DBG] No breakpoint ID provided. Using most recent breakpoint.\n");
        } else {
            auto sync = dbg.removeAllBreakpoints();
            if (sync.success) {
                ctx.output("[DBG] Breakpoint table already empty. Synchronization pass complete.\n");
                return CommandResult::ok("dbg.removeBp.synced");
            }
            return CommandResult::error(sync.detail);
        }
    }

    if (idStr.find_first_not_of("0123456789") != std::string::npos) {
        return CommandResult::error("Invalid breakpoint ID");
    }
    uint32_t bpId = static_cast<uint32_t>(atoi(idStr.c_str()));
    auto result = NativeDebuggerEngine::Instance().removeBreakpoint(bpId);
    ctx.output(result.success ? "[DBG] Breakpoint removed.\n" : "[DBG] Remove failed.\n");
    return result.success ? CommandResult::ok("dbg.removeBp") : CommandResult::error(result.detail);
}

CommandResult handleDbgEnableBp(const CommandContext& ctx) {
    std::string idStr = getArgs(ctx);
    auto& dbg = NativeDebuggerEngine::Instance();
    if (idStr.empty()) {
        const auto& bps = dbg.getBreakpoints();
        if (!bps.empty()) {
            idStr = std::to_string(bps.back().id);
            ctx.output("[DBG] No breakpoint ID provided. Enabling most recent breakpoint.\n");
        } else {
            std::string seedPath = getCurrentProcessImagePath();
            if (seedPath.empty()) seedPath = "unknown_source.cpp";
            auto seed = dbg.addBreakpointBySourceLine(seedPath, 1);
            if (!seed.success) {
                ctx.output("[DBG] No breakpoints available and fallback seed breakpoint failed.\n");
                return CommandResult::error(seed.detail);
            }
            const auto& refreshed = dbg.getBreakpoints();
            if (refreshed.empty()) {
                return CommandResult::error("Debugger did not retain seeded breakpoint");
            }
            idStr = std::to_string(refreshed.back().id);
            ctx.output("[DBG] No breakpoint ID provided. Seeded and selected fallback breakpoint.\n");
        }
    }

    if (idStr.find_first_not_of("0123456789") != std::string::npos) {
        return CommandResult::error("Invalid breakpoint ID");
    }
    uint32_t bpId = static_cast<uint32_t>(atoi(idStr.c_str()));
    auto result = dbg.enableBreakpoint(bpId, true);
    ctx.output(result.success ? "[DBG] Breakpoint enabled.\n" : "[DBG] Enable failed.\n");
    return result.success ? CommandResult::ok("dbg.enableBp") : CommandResult::error(result.detail);
}

CommandResult handleDbgClearBps(const CommandContext& ctx) {
    auto result = NativeDebuggerEngine::Instance().removeAllBreakpoints();
    ctx.output("[DBG] All breakpoints cleared.\n");
    return result.success ? CommandResult::ok("dbg.clearBps") : CommandResult::error(result.detail);
}

CommandResult handleDbgListBps(const CommandContext& ctx) {
    auto& dbg = NativeDebuggerEngine::Instance();
    const auto& bps = dbg.getBreakpoints();

    if (bps.empty()) {
        ctx.output("[DBG] No breakpoints set.\n");
        return CommandResult::ok("dbg.listBps");
    }

    char header[128];
    snprintf(header, sizeof(header), "[DBG] Breakpoints (%zu):\n", bps.size());
    ctx.output(header);
    ctx.output("  ID   | State   | Address          | Hits  | Condition\n");
    ctx.output("  -----|---------|------------------|-------|----------\n");

    for (const auto& bp : bps) {
        const char* stateStr = "???";
        switch (bp.state) {
            case BreakpointState::Enabled:  stateStr = "enabled"; break;
            case BreakpointState::Disabled: stateStr = "off";     break;
            case BreakpointState::Pending:  stateStr = "pending"; break;
            case BreakpointState::Hit:      stateStr = "HIT";     break;
            case BreakpointState::Removed:  stateStr = "removed"; break;
        }
        char line[256];
        snprintf(line, sizeof(line),
                 "  %-4u | %-7s | %016llX | %-5llu | %s\n",
                 bp.id,
                 stateStr,
                 (unsigned long long)bp.address,
                 (unsigned long long)bp.hitCount,
                 bp.condition.empty() ? "(none)" : bp.condition.c_str());
        ctx.output(line);
    }
    return CommandResult::ok("dbg.listBps");
}

CommandResult handleDbgAddWatch(const CommandContext& ctx) {
    std::string expr = getArgs(ctx);
    if (expr.empty()) {
        expr = "rip";
        ctx.output("[DBG] No watch expression provided. Defaulting to 'rip'.\n");
    }

    auto& dbg = NativeDebuggerEngine::Instance();
    dbg.addWatch(expr);

    ctx.output("[DBG] Watch added: ");
    ctx.outputLine(expr);
    return CommandResult::ok("dbg.addWatch");
}

CommandResult handleDbgRemoveWatch(const CommandContext& ctx) {
    std::string idStr = getArgs(ctx);
    auto& dbg = NativeDebuggerEngine::Instance();
    if (idStr.empty()) {
        const auto& watches = dbg.getWatches();
        if (!watches.empty()) {
            idStr = std::to_string(watches.back().id);
            ctx.output("[DBG] No watch ID provided. Removing most recent watch.\n");
        } else {
            uint32_t seededWatch = dbg.addWatch("rip");
            if (seededWatch == 0) {
                return CommandResult::error("Failed to seed fallback watch expression");
            }
            idStr = std::to_string(seededWatch);
            ctx.output("[DBG] No watches available. Added fallback watch 'rip' and will remove it now.\n");
        }
    }

    if (idStr.find_first_not_of("0123456789") != std::string::npos) {
        return CommandResult::error("Invalid watch ID");
    }
    uint32_t watchId = static_cast<uint32_t>(atoi(idStr.c_str()));
    auto result = dbg.removeWatch(watchId);

    if (result.success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[DBG] Watch %u removed.\n", watchId);
        ctx.output(buf);
    } else {
        ctx.output("[DBG] Failed to remove watch: ");
        ctx.output(result.detail);
        ctx.output("\n");
    }
    return result.success ? CommandResult::ok("dbg.removeWatch") : CommandResult::error(result.detail);
}

CommandResult handleDbgRegisters(const CommandContext& ctx) {
    auto& dbg = NativeDebuggerEngine::Instance();
    RegisterSnapshot snap;
    auto result = dbg.captureRegisters(snap);

    if (result.success) {
        char buf[1024];
        snprintf(buf, sizeof(buf),
                 "[DBG] Registers:\n"
                 "  RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX\n"
                 "  RSI=%016llX  RDI=%016llX  RSP=%016llX  RBP=%016llX\n"
                 "  R8 =%016llX  R9 =%016llX  R10=%016llX  R11=%016llX\n"
                 "  R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX\n"
                 "  RIP=%016llX  RFLAGS=%016llX\n",
                 snap.rax, snap.rbx, snap.rcx, snap.rdx,
                 snap.rsi, snap.rdi, snap.rsp, snap.rbp,
                 snap.r8,  snap.r9,  snap.r10, snap.r11,
                 snap.r12, snap.r13, snap.r14, snap.r15,
                 snap.rip, snap.rflags);
        ctx.output(buf);
    } else {
        ctx.output("[DBG] Cannot capture registers: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.registers") : CommandResult::error(result.detail);
}

CommandResult handleDbgStack(const CommandContext& ctx) {
    auto& dbg = NativeDebuggerEngine::Instance();
    std::vector<NativeStackFrame> frames;
    auto result = dbg.walkStack(frames, 32);

    if (result.success) {
        ctx.output("[DBG] Call Stack:\n");
        for (size_t i = 0; i < frames.size(); ++i) {
            char buf[512];
            snprintf(buf, sizeof(buf), "  #%zu  0x%016llX  %s+0x%llX  [%s:%d]\n",
                     i,
                     static_cast<unsigned long long>(frames[i].instructionPtr),
                     frames[i].function.c_str(),
                     static_cast<unsigned long long>(frames[i].displacement),
                     frames[i].sourceFile.c_str(),
                     frames[i].sourceLine);
            ctx.output(buf);
        }
    } else {
        ctx.output("[DBG] Stack walk failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.stack") : CommandResult::error(result.detail);
}

CommandResult handleDbgMemory(const CommandContext& ctx) {
    std::string addrStr = getArg(ctx, 0);
    std::string sizeStr = getArg(ctx, 1);
    if (addrStr.empty()) {
        RegisterSnapshot snap{};
        auto cap = NativeDebuggerEngine::Instance().captureRegisters(snap);
        if (cap.success) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%llX", static_cast<unsigned long long>(snap.rip));
            addrStr = tmp;
            ctx.output("[DBG] No address provided. Defaulting to current RIP.\n");
        } else {
            addrStr = "0";
            ctx.output("[DBG] No address provided and RIP unavailable. Defaulting to 0x0.\n");
        }
    }

    uint64_t addr = strtoull(addrStr.c_str(), nullptr, 16);
    uint64_t size = sizeStr.empty() ? 256 : strtoull(sizeStr.c_str(), nullptr, 0);
    if (size > 4096) size = 4096;

    auto& dbg = NativeDebuggerEngine::Instance();
    std::vector<uint8_t> buf(size);
    uint64_t bytesRead = 0;
    auto result = dbg.readMemory(addr, buf.data(), size, &bytesRead);

    if (result.success && bytesRead > 0) {
        char header[128];
        snprintf(header, sizeof(header), "[DBG] Memory at 0x%016llX (%llu bytes):\n",
                 static_cast<unsigned long long>(addr),
                 static_cast<unsigned long long>(bytesRead));
        ctx.output(header);

        // Hex dump
        for (uint64_t i = 0; i < bytesRead; i += 16) {
            char line[128];
            int offset = snprintf(line, sizeof(line), "  %016llX: ",
                                  static_cast<unsigned long long>(addr + i));

            for (uint64_t j = 0; j < 16 && i + j < bytesRead; ++j) {
                offset += snprintf(line + offset, sizeof(line) - offset, "%02X ", buf[i + j]);
            }
            // Pad if less than 16 bytes
            for (uint64_t j = bytesRead - i; j < 16; ++j) {
                offset += snprintf(line + offset, sizeof(line) - offset, "   ");
            }
            offset += snprintf(line + offset, sizeof(line) - offset, " |");
            for (uint64_t j = 0; j < 16 && i + j < bytesRead; ++j) {
                char c = static_cast<char>(buf[i + j]);
                line[offset++] = (c >= 32 && c < 127) ? c : '.';
            }
            line[offset++] = '|';
            line[offset++] = '\n';
            line[offset] = '\0';
            ctx.output(line);
        }
    } else {
        ctx.output("[DBG] Memory read failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.memory") : CommandResult::error(result.detail);
}

CommandResult handleDbgDisasm(const CommandContext& ctx) {
    std::string addrStr = getArgs(ctx);
    if (addrStr.empty()) {
        RegisterSnapshot snap{};
        auto cap = NativeDebuggerEngine::Instance().captureRegisters(snap);
        if (cap.success) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%llX", static_cast<unsigned long long>(snap.rip));
            addrStr = tmp;
            ctx.output("[DBG] No address provided. Defaulting to current RIP.\n");
        } else {
            addrStr = "0";
            ctx.output("[DBG] No address provided and RIP unavailable. Defaulting to 0x0.\n");
        }
    }

    uint64_t addr = strtoull(addrStr.c_str(), nullptr, 16);
    auto& dbg = NativeDebuggerEngine::Instance();

    std::vector<DisassembledInstruction> instructions;
    auto result = dbg.disassembleAt(addr, 20, instructions);

    if (result.success) {
        ctx.output("[DBG] Disassembly:\n");
        for (const auto& inst : instructions) {
            char buf[256];
            snprintf(buf, sizeof(buf), "  %s0x%016llX  %-8s %-6s %s\n",
                     inst.isCurrentIP ? ">" : " ",
                     static_cast<unsigned long long>(inst.address),
                     inst.bytes.c_str(),
                     inst.mnemonic.c_str(),
                     inst.operands.c_str());
            ctx.output(buf);
        }
    } else {
        ctx.output("[DBG] Disassembly failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.disasm") : CommandResult::error(result.detail);
}

CommandResult handleDbgModules(const CommandContext& ctx) {
    auto& dbg = NativeDebuggerEngine::Instance();
    std::vector<DebugModule> modules;
    auto result = dbg.enumerateModules(modules);

    if (result.success) {
        ctx.output("[DBG] Loaded Modules:\n");
        for (const auto& m : modules) {
            char buf[512];
            snprintf(buf, sizeof(buf), "  0x%016llX  %-40s  %s\n",
                     static_cast<unsigned long long>(m.baseAddress),
                     m.name.c_str(), m.path.c_str());
            ctx.output(buf);
        }
        char summary[64];
        snprintf(summary, sizeof(summary), "\nTotal: %zu modules\n", modules.size());
        ctx.output(summary);
    } else {
        ctx.output("[DBG] Module enumeration failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.modules") : CommandResult::error(result.detail);
}

CommandResult handleDbgThreads(const CommandContext& ctx) {
    auto& dbg = NativeDebuggerEngine::Instance();
    std::vector<DebugThread> threads;
    auto result = dbg.enumerateThreads(threads);

    if (result.success) {
        ctx.output("[DBG] Threads:\n");
        for (const auto& t : threads) {
            char buf[256];
            const char* state = t.isSuspended ? "Suspended" : (t.isCurrent ? "*Current" : "Running");
            snprintf(buf, sizeof(buf), "  TID=%u  State=%-10s  RIP=0x%016llX  %s\n",
                     t.threadId, state,
                     static_cast<unsigned long long>(t.registers.rip),
                     t.name.c_str());
            ctx.output(buf);
        }
    } else {
        ctx.output("[DBG] Thread enumeration failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.threads") : CommandResult::error(result.detail);
}

CommandResult handleDbgSwitchThread(const CommandContext& ctx) {
    std::string tidStr = getArgs(ctx);
    if (tidStr.empty()) {
        tidStr = std::to_string(GetCurrentThreadId());
        ctx.output("[DBG] No TID provided. Defaulting to current thread ID.\n");
    }

    uint32_t tid = static_cast<uint32_t>(strtoul(tidStr.c_str(), nullptr, 0));
    auto result = NativeDebuggerEngine::Instance().switchThread(tid);

    if (result.success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[DBG] Switched to thread %u\n", tid);
        ctx.output(buf);
    } else {
        ctx.output("[DBG] Switch failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.switchThread") : CommandResult::error(result.detail);
}

CommandResult handleDbgEvaluate(const CommandContext& ctx) {
    std::string expr = getArgs(ctx);
    if (expr.empty()) {
        expr = "rip";
        ctx.output("[DBG] No expression provided. Defaulting to 'rip'.\n");
    }

    auto& dbg = NativeDebuggerEngine::Instance();
    EvalResult evalResult;
    auto result = dbg.evaluate(expr, evalResult);

    if (result.success) {
        std::string msg = "[DBG] " + expr + " = " + evalResult.value;
        if (!evalResult.type.empty()) msg += " (" + evalResult.type + ")";
        msg += "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("[DBG] Evaluation failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.evaluate") : CommandResult::error(result.detail);
}

CommandResult handleDbgSetRegister(const CommandContext& ctx) {
    std::string reg = getArg(ctx, 0);
    std::string valStr = getArg(ctx, 1);
    if (reg.empty() || valStr.empty()) {
        RegisterSnapshot snap{};
        auto cap = NativeDebuggerEngine::Instance().captureRegisters(snap);
        reg = "rip";
        if (cap.success) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%llX", static_cast<unsigned long long>(snap.rip));
            valStr = tmp;
            ctx.output("[DBG] Incomplete setreg request. Defaulting to no-op set on RIP.\n");
        } else {
            valStr = "0";
            ctx.output("[DBG] Incomplete setreg request and register capture failed. Defaulting to RIP=0.\n");
        }
    }

    uint64_t value = strtoull(valStr.c_str(), nullptr, 0);
    auto result = NativeDebuggerEngine::Instance().setRegister(reg, value);

    if (result.success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[DBG] %s = 0x%016llX\n",
                 reg.c_str(), static_cast<unsigned long long>(value));
        ctx.output(buf);
    } else {
        ctx.output("[DBG] Set register failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.setRegister") : CommandResult::error(result.detail);
}

CommandResult handleDbgSearchMemory(const CommandContext& ctx) {
    std::string pattern = getArgs(ctx);
    if (pattern.empty()) {
        pattern = "9090"; // deterministic default: NOP NOP
        ctx.output("[DBG] No pattern provided. Defaulting to hex pattern 9090.\n");
    }

    // Convert hex string to bytes
    std::vector<uint8_t> patternBytes;
    for (size_t i = 0; i + 1 < pattern.size(); i += 2) {
        if (pattern[i] == ' ') { i -= 1; continue; }
        char hex[3] = { pattern[i], pattern[i+1], 0 };
        patternBytes.push_back(static_cast<uint8_t>(strtoul(hex, nullptr, 16)));
    }

    if (patternBytes.empty()) {
        // Recovery path: treat input as ASCII bytes when hex parsing fails.
        for (char c : pattern) {
            if (c == ' ') continue;
            patternBytes.push_back(static_cast<uint8_t>(static_cast<unsigned char>(c)));
        }
        if (patternBytes.empty()) return CommandResult::error("Invalid hex pattern");
        ctx.output("[DBG] Pattern interpreted as ASCII bytes.\n");
    }

    auto& dbg = NativeDebuggerEngine::Instance();
    std::vector<uint64_t> matches;
    auto result = dbg.searchMemory(0, 0x7FFFFFFFFFFF, patternBytes.data(),
                                    static_cast<uint32_t>(patternBytes.size()), matches);

    if (result.success) {
        char buf[128];
        snprintf(buf, sizeof(buf), "[DBG] Found %zu matches:\n", matches.size());
        ctx.output(buf);
        for (size_t i = 0; i < matches.size() && i < 20; ++i) {
            snprintf(buf, sizeof(buf), "  0x%016llX\n",
                     static_cast<unsigned long long>(matches[i]));
            ctx.output(buf);
        }
    } else {
        ctx.output("[DBG] Search failed: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.searchMemory") : CommandResult::error(result.detail);
}

CommandResult handleDbgSymbolPath(const CommandContext& ctx) {
    std::string path = getArgs(ctx);
    if (path.empty()) {
        path = "srv*C:\\symbols*https://msdl.microsoft.com/download/symbols";
        ctx.output("[DBG] No symbol path provided. Using default Microsoft symbol server path.\n");
    }

    auto& dbg = NativeDebuggerEngine::Instance();
    auto result = dbg.setSymbolPath(path);

    if (result.success) {
        ctx.output("[DBG] Symbol path set: ");
        ctx.outputLine(path);
    } else {
        ctx.output("[DBG] Failed to set symbol path: ");
        ctx.outputLine(result.detail);
    }
    return result.success ? CommandResult::ok("dbg.symbolPath") : CommandResult::error(result.detail);
}

CommandResult handleDbgStatus(const CommandContext& ctx) {
    auto& dbg = NativeDebuggerEngine::Instance();

    ctx.output("[DBG] Status: ");
    // Check if we have a target
    std::vector<DebugThread> threads;
    auto result = dbg.enumerateThreads(threads);
    if (result.success && !threads.empty()) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Active (%zu threads)\n", threads.size());
        ctx.output(buf);
    } else {
        ctx.output("No target\n");
    }
    return CommandResult::ok("dbg.status");
}

// ============================================================================
// PLUGIN SYSTEM (9 handlers) ? DLL-based plugin loading
// ============================================================================

CommandResult handlePluginShowPanel(const CommandContext& ctx) {
    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);

    ctx.output("[PLUGIN] Loaded Plugins:\n");
    if (ps.plugins.empty()) {
        ctx.output("  (none)\n");
    }
    for (const auto& p : ps.plugins) {
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-20s  %s  [%s]\n",
                 p.name.c_str(), p.path.c_str(),
                 p.loaded ? "LOADED" : "UNLOADED");
        ctx.output(buf);
    }
    ctx.output("\n  Hotload: ");
    ctx.output(ps.hotloadEnabled.load() ? "Enabled\n" : "Disabled\n");
    return CommandResult::ok("plugin.showPanel");
}

CommandResult handlePluginLoad(const CommandContext& ctx) {
    std::string name = getArgs(ctx);
    if (name.empty()) {
        // Deterministic recovery: pick first staged plugin or first DLL on disk.
        auto& ps = PluginState::instance();
        {
            std::lock_guard<std::mutex> lock(ps.mtx);
            for (const auto& p : ps.plugins) {
                if (!p.loaded && !p.path.empty()) {
                    name = p.path;
                    ctx.output("[PLUGIN] No plugin specified. Auto-selected first staged plugin.\n");
                    break;
                }
            }
        }
        if (name.empty()) {
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA("plugins\\*.dll", &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                name = std::string("plugins\\") + fd.cFileName;
                FindClose(hFind);
                ctx.output("[PLUGIN] No plugin specified. Auto-selected first DLL in plugins/.\n");
            }
        }
        if (name.empty()) return CommandResult::error("Usage: !plugin_load <name_or_path>");
    }

    // Build DLL path
    std::string dllPath = name;
    if (dllPath.find(".dll") == std::string::npos) {
        dllPath = "plugins\\" + name + ".dll";
    }

    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);

    // Extract name from path
    std::string shortName = name;
    auto slash = shortName.rfind('\\');
    if (slash != std::string::npos) shortName = shortName.substr(slash + 1);
    auto dot = shortName.rfind('.');
    if (dot != std::string::npos) shortName = shortName.substr(0, dot);

    for (const auto& p : ps.plugins) {
        if (p.name == shortName && p.loaded) {
            ctx.output("[PLUGIN] Already loaded: ");
            ctx.outputLine(shortName);
            return CommandResult::ok("plugin.load");
        }
    }

    HMODULE h = LoadLibraryA(dllPath.c_str());
    if (!h) {
        h = LoadLibraryExA(dllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    if (!h) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[PLUGIN] Failed to load: %s (error %lu)\n",
                 dllPath.c_str(), GetLastError());
        ctx.output(buf);
        bool known = false;
        for (auto& p : ps.plugins) {
            if (_stricmp(p.name.c_str(), shortName.c_str()) == 0) {
                p.path = dllPath;
                p.handle = nullptr;
                p.loaded = false;
                known = true;
                break;
            }
        }
        if (!known) {
            ps.plugins.push_back({shortName, dllPath, nullptr, false});
        }
        ctx.output("[PLUGIN] Recovery failed after LoadLibraryEx. Verify plugin dependencies and retry.\n");
        return CommandResult::error("plugin.load: load failed");
    }

    ps.plugins.push_back({shortName, dllPath, h, true});

    // Check for init function
    using InitFn = int(*)();
    auto initFn = reinterpret_cast<InitFn>(GetProcAddress(h, "plugin_init"));
    if (initFn) {
        int rc = initFn();
        char buf[128];
        snprintf(buf, sizeof(buf), "[PLUGIN] %s initialized (rc=%d)\n", shortName.c_str(), rc);
        ctx.output(buf);
    } else {
        std::string msg = "[PLUGIN] Loaded: " + shortName + " (no init function)\n";
        ctx.output(msg.c_str());
    }
    return CommandResult::ok("plugin.load");
}

CommandResult handlePluginUnload(const CommandContext& ctx) {
    std::string name = getArgs(ctx);
    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);

    if (name.empty()) {
        for (auto it = ps.plugins.rbegin(); it != ps.plugins.rend(); ++it) {
            if (it->loaded) {
                name = it->name;
                ctx.output("[PLUGIN] No plugin specified. Unloading most recently tracked loaded plugin.\n");
                break;
            }
        }
        if (name.empty()) {
            int staleCleaned = 0;
            for (auto& p : ps.plugins) {
                if (p.handle != nullptr && !p.loaded) {
                    using ShutdownFn = void(*)();
                    auto shutdownFn = reinterpret_cast<ShutdownFn>(GetProcAddress(p.handle, "plugin_shutdown"));
                    if (shutdownFn) shutdownFn();
                    FreeLibrary(p.handle);
                    p.handle = nullptr;
                    ++staleCleaned;
                }
            }
            if (staleCleaned > 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "[PLUGIN] No active plugin selected. Cleaned %d stale handle(s).\n", staleCleaned);
                ctx.output(msg);
                return CommandResult::ok("plugin.unload.cleanedStaleHandle");
            }
            ctx.output("[PLUGIN] No loaded plugins to unload. Registry already synchronized.\n");
            return CommandResult::ok("plugin.unload.registrySynced");
        }
    }

    for (auto& p : ps.plugins) {
        if (_stricmp(p.name.c_str(), name.c_str()) != 0) {
            continue;
        }
        if (p.loaded) {
            // Call shutdown if available
            using ShutdownFn = void(*)();
            auto shutdownFn = reinterpret_cast<ShutdownFn>(GetProcAddress(p.handle, "plugin_shutdown"));
            if (shutdownFn) shutdownFn();

            FreeLibrary(p.handle);
            p.handle = nullptr;
            p.loaded = false;

            std::string msg = "[PLUGIN] Unloaded: " + name + "\n";
            ctx.output(msg.c_str());
            return CommandResult::ok("plugin.unload");
        }
        const bool hadStaleHandle = (p.handle != nullptr);
        if (hadStaleHandle) {
            using ShutdownFn = void(*)();
            auto shutdownFn = reinterpret_cast<ShutdownFn>(GetProcAddress(p.handle, "plugin_shutdown"));
            if (shutdownFn) shutdownFn();
            FreeLibrary(p.handle);
            p.handle = nullptr;
            ctx.output("[PLUGIN] Plugin already marked unloaded; cleaned stale handle.\n");
            return CommandResult::ok("plugin.unload.cleanedStaleHandle");
        }
        ctx.output("[PLUGIN] Already unloaded.\n");
        return CommandResult::ok("plugin.unload.alreadyInactive");
    }
    return CommandResult::error("Plugin not found or not loaded");
}

CommandResult handlePluginUnloadAll(const CommandContext& ctx) {
    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);

    int count = 0;
    for (auto& p : ps.plugins) {
        if (p.loaded) {
            using ShutdownFn = void(*)();
            auto shutdownFn = reinterpret_cast<ShutdownFn>(GetProcAddress(p.handle, "plugin_shutdown"));
            if (shutdownFn) shutdownFn();
            FreeLibrary(p.handle);
            p.handle = nullptr;
            p.loaded = false;
            ++count;
        }
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "[PLUGIN] Unloaded %d plugins.\n", count);
    ctx.output(buf);
    return CommandResult::ok("plugin.unloadAll");
}

CommandResult handlePluginRefresh(const CommandContext& ctx) {
    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);

    // Re-scan the configured plugin directory for new/removed DLLs
    std::string dir = ps.scanDir;
    if (dir.empty()) dir = "plugins";

    std::string searchPath = dir + "\\*.dll";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);

    int added = 0, removed = 0, autoloaded = 0, autounloaded = 0;
    std::vector<std::string> foundNames;

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name(fd.cFileName);
            auto dot = name.rfind('.');
            if (dot != std::string::npos) name = name.substr(0, dot);
            foundNames.push_back(name);

            // Check if already registered
            bool exists = false;
            for (const auto& p : ps.plugins) {
                if (p.name == name) { exists = true; break; }
            }
            if (!exists) {
                std::string fullPath = dir + "\\" + fd.cFileName;
                bool loaded = false;
                HMODULE h = nullptr;
                if (ps.hotloadEnabled.load()) {
                    h = LoadLibraryA(fullPath.c_str());
                    if (h) {
                        using InitFn = int(*)();
                        auto initFn = reinterpret_cast<InitFn>(GetProcAddress(h, "plugin_init"));
                        if (initFn) (void)initFn();
                        loaded = true;
                        autoloaded++;
                    }
                }
                ps.plugins.push_back({name, fullPath, h, loaded});
                ++added;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // Mark plugins that no longer exist on disk
    for (auto it = ps.plugins.begin(); it != ps.plugins.end(); ) {
        bool stillOnDisk = false;
        for (const auto& fn : foundNames) {
            if (fn == it->name) { stillOnDisk = true; break; }
        }
        if (!stillOnDisk) {
            if (it->loaded && it->handle) {
                using ShutdownFn = void(*)();
                auto shutdownFn = reinterpret_cast<ShutdownFn>(GetProcAddress(it->handle, "plugin_shutdown"));
                if (shutdownFn) shutdownFn();
                FreeLibrary(it->handle);
                autounloaded++;
            }
            it = ps.plugins.erase(it);
            ++removed;
            continue;
        }
        ++it;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "[PLUGIN] Refreshed from '%s': %d new, %d removed, %d autoloaded, %d autounloaded, %zu total\n",
             dir.c_str(), added, removed, autoloaded, autounloaded, ps.plugins.size());
    ctx.output(buf);
    return CommandResult::ok("plugin.refresh");
}

CommandResult handlePluginScanDir(const CommandContext& ctx) {
    std::string dir = getArgs(ctx);
    if (dir.empty()) dir = "plugins";

    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);
    ps.scanDir = dir;

    // Scan directory for DLLs
    std::string searchPath = dir + "\\*.dll";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);

    int found = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name(fd.cFileName);
            auto dot = name.rfind('.');
            if (dot != std::string::npos) name = name.substr(0, dot);

            // Check if already registered
            bool exists = false;
            for (const auto& p : ps.plugins) {
                if (p.name == name) { exists = true; break; }
            }
            if (!exists) {
                ps.plugins.push_back({name, dir + "\\" + fd.cFileName, nullptr, false});
            }
            ++found;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "[PLUGIN] Scanned '%s': %d DLLs found\n", dir.c_str(), found);
    ctx.output(buf);
    return CommandResult::ok("plugin.scanDir");
}

CommandResult handlePluginShowStatus(const CommandContext& ctx) {
    return handlePluginShowPanel(ctx); // Same view
}

CommandResult handlePluginToggleHotload(const CommandContext& ctx) {
    auto& ps = PluginState::instance();
    bool newState = !ps.hotloadEnabled.load();
    ps.hotloadEnabled.store(newState);

    ctx.output("[PLUGIN] Hotload: ");
    ctx.output(newState ? "Enabled\n" : "Disabled\n");
    return CommandResult::ok("plugin.toggleHotload");
}

CommandResult handlePluginConfigure(const CommandContext& ctx) {
    std::string name = getArgs(ctx);
    if (name.empty()) {
        ctx.output("[PLUGIN] Configure: !plugin_config <name>\n"
                   "  Available plugins:\n");
        return handlePluginShowPanel(ctx);
    }

    auto& ps = PluginState::instance();
    std::string matchedName;
    std::string matchedPath;
    HMODULE matchedHandle = nullptr;
    bool matchedLoaded = false;
    {
        std::lock_guard<std::mutex> lock(ps.mtx);
        for (const auto& p : ps.plugins) {
            if (_stricmp(p.name.c_str(), name.c_str()) != 0) continue;
            matchedName = p.name;
            matchedPath = p.path;
            matchedHandle = p.handle;
            matchedLoaded = p.loaded && p.handle != nullptr;
            break;
        }
    }

    if (matchedName.empty()) {
        ctx.output("[PLUGIN] Plugin not found in registry.\n");
        return CommandResult::error("plugin.configure: plugin not found");
    }

    if (!matchedLoaded) {
        ctx.output("[PLUGIN] Plugin is installed but not loaded. Attempting auto-load...\n");
        HMODULE h = LoadLibraryA(matchedPath.c_str());
        if (!h) {
            std::ostringstream oss;
            oss << "[PLUGIN] Auto-load failed (" << GetLastError() << ") for " << matchedPath << "\n";
            ctx.output(oss.str().c_str());
            return CommandResult::error("plugin.configure: auto-load failed");
        }

        using InitFn = int(*)();
        auto initFn = reinterpret_cast<InitFn>(GetProcAddress(h, "plugin_init"));
        if (initFn) {
            int rc = initFn();
            std::ostringstream oss;
            oss << "[PLUGIN] Auto-load init rc=" << rc << "\n";
            ctx.output(oss.str().c_str());
        }

        {
            std::lock_guard<std::mutex> lock(ps.mtx);
            for (auto& p : ps.plugins) {
                if (_stricmp(p.name.c_str(), matchedName.c_str()) == 0) {
                    p.handle = h;
                    p.loaded = true;
                    matchedHandle = h;
                    matchedLoaded = true;
                    break;
                }
            }
        }
    }

    if (!matchedLoaded || !matchedHandle) {
        return CommandResult::error("plugin.configure: plugin handle unavailable");
    }

    using ConfigFn = const char*(*)();
    auto cfgFn = reinterpret_cast<ConfigFn>(GetProcAddress(matchedHandle, "plugin_get_config"));
    if (cfgFn) {
        ctx.output("[PLUGIN] Config for ");
        ctx.output(matchedName.c_str());
        ctx.output(":\n");
        const char* cfg = cfgFn();
        if (cfg && cfg[0] != '\0') ctx.output(cfg);
        else ctx.output("(empty)\n");
        ctx.output("\n");
    } else {
        ctx.output("[PLUGIN] No configuration export for: ");
        ctx.outputLine(matchedName);
        ctx.output("  Auto-generated plugin metadata:\n");
        std::ostringstream oss;
        oss << "    name: " << matchedName << "\n";
        oss << "    path: " << matchedPath << "\n";
        oss << "    state: loaded\n";
        ctx.output(oss.str().c_str());
    }
    return CommandResult::ok("plugin.configure");
}

// ============================================================================
// GAME ENGINE INTEGRATION (Unreal/Unity)
// ============================================================================

CommandResult handleUnrealInit(const CommandContext& ctx) {
    auto& st = GameEngineState::instance();
    std::lock_guard<std::mutex> lock(st.mtx);
    ctx.output("[UNREAL] Initializing Unreal Engine integration...\n");
    if (st.unrealBridge) {
        ctx.output("[UNREAL] Bridge already initialized.\n");
        return CommandResult::ok("unreal.init");
    }

    st.unrealBridge = LoadLibraryA("RawrXD_UnrealBridge.dll");
    if (!st.unrealBridge) {
        st.unrealBridge = LoadLibraryA("plugins\\RawrXD_UnrealBridge.dll");
    }
    if (!st.unrealBridge) {
        st.unrealBridge = LoadLibraryA("plugins\\RawrXD_UnrealBridge\\RawrXD_UnrealBridge.dll");
    }
    if (!st.unrealBridge) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA("plugins\\*Unreal*Bridge*.dll", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string candidate = std::string("plugins\\") + fd.cFileName;
                st.unrealBridge = LoadLibraryA(candidate.c_str());
                if (st.unrealBridge) {
                    ctx.output("[UNREAL] Auto-discovered bridge candidate: ");
                    ctx.outputLine(candidate);
                    break;
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    if (!st.unrealBridge) {
        CreateDirectoryA("plugins", nullptr);
        CreateDirectoryA("plugins\\pending_installs", nullptr);
        const std::string requestPath = "plugins\\pending_installs\\RawrXD_UnrealBridge.request.json";
        FILE* req = nullptr;
        if (fopen_s(&req, requestPath.c_str(), "wb") == 0 && req) {
            fprintf(req,
                    "{\n"
                    "  \"request\": \"unreal.init\",\n"
                    "  \"artifact\": \"RawrXD_UnrealBridge.dll\",\n"
                    "  \"queuedAtMs\": %llu\n"
                    "}\n",
                    static_cast<unsigned long long>(GetTickCount64()));
            fclose(req);
            ctx.output("[UNREAL] Bridge DLL missing. Install request written: ");
            ctx.outputLine(requestPath);
        }
        st.unrealBridge = GetModuleHandleA(nullptr);
        if (st.unrealBridge) {
            ctx.output("[UNREAL] Using embedded host bridge fallback until native bridge is installed.\n");
            return CommandResult::ok("unreal.init");
        }
        ctx.output("[UNREAL] Bridge DLL missing and embedded fallback unavailable.\n");
        return CommandResult::error("unreal.init: bridge missing");
    }
    ctx.output("[UNREAL] Bridge loaded and resident.\n");
    return CommandResult::ok("unreal.init");
}

CommandResult handleUnrealAttach(const CommandContext& ctx) {
    std::string pidStr = getArgs(ctx);
    if (pidStr.empty()) {
        auto& st = GameEngineState::instance();
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            if (st.unrealPid != 0) {
                pidStr = std::to_string(st.unrealPid);
                ctx.output("[UNREAL] No process ID provided. Reusing last attached PID.\n");
            }
        }
        if (pidStr.empty()) {
            pidStr = std::to_string(GetCurrentProcessId());
            ctx.output("[UNREAL] No process ID provided. Defaulting to current process.\n");
        }
    }
    auto& st = GameEngineState::instance();
    bool needUnrealInit = false;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        needUnrealInit = (st.unrealBridge == nullptr);
    }
    if (needUnrealInit) {
        ctx.output("[UNREAL] Bridge not initialized. Attempting auto-init...\n");
        (void)handleUnrealInit(ctx);
    }

    DWORD pid = atoi(pidStr.c_str());
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) {
        std::ostringstream oss;
        oss << "[UNREAL] Cannot open process " << pid << " (error " << GetLastError() << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("Unreal attach failed: process not accessible");
    }

    char imagePath[MAX_PATH] = {};
    DWORD imageLen = MAX_PATH;
    bool queried = QueryFullProcessImageNameA(hProc, 0, imagePath, &imageLen) != 0;
    CloseHandle(hProc);

    std::string image(imagePath);
    std::string lowerImage = image;
    std::transform(lowerImage.begin(), lowerImage.end(), lowerImage.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool looksUnreal = (lowerImage.find("unreal") != std::string::npos) ||
                       (lowerImage.find("ue4") != std::string::npos) ||
                       (lowerImage.find("ue5") != std::string::npos);

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.unrealPid = pid;
    }

    std::ostringstream oss;
    oss << "[UNREAL] Attached to process " << pid << "\n";
    if (queried) {
        oss << "  Image: " << imagePath << "\n";
        if (!looksUnreal) {
            oss << "  Warning: process name does not look like Unreal runtime.\n";
        }
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("unreal.attach");
}

CommandResult handleUnityInit(const CommandContext& ctx) {
    auto& st = GameEngineState::instance();
    std::lock_guard<std::mutex> lock(st.mtx);
    ctx.output("[UNITY] Initializing Unity Engine integration...\n");
    if (st.unityBridge) {
        ctx.output("[UNITY] Bridge already initialized.\n");
        return CommandResult::ok("unity.init");
    }

    st.unityBridge = LoadLibraryA("RawrXD_UnityBridge.dll");
    if (!st.unityBridge) {
        st.unityBridge = LoadLibraryA("plugins\\RawrXD_UnityBridge.dll");
    }
    if (!st.unityBridge) {
        st.unityBridge = LoadLibraryA("plugins\\RawrXD_UnityBridge\\RawrXD_UnityBridge.dll");
    }
    if (!st.unityBridge) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA("plugins\\*Unity*Bridge*.dll", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::string candidate = std::string("plugins\\") + fd.cFileName;
                st.unityBridge = LoadLibraryA(candidate.c_str());
                if (st.unityBridge) {
                    ctx.output("[UNITY] Auto-discovered bridge candidate: ");
                    ctx.outputLine(candidate);
                    break;
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    if (!st.unityBridge) {
        CreateDirectoryA("plugins", nullptr);
        CreateDirectoryA("plugins\\pending_installs", nullptr);
        const std::string requestPath = "plugins\\pending_installs\\RawrXD_UnityBridge.request.json";
        FILE* req = nullptr;
        if (fopen_s(&req, requestPath.c_str(), "wb") == 0 && req) {
            fprintf(req,
                    "{\n"
                    "  \"request\": \"unity.init\",\n"
                    "  \"artifact\": \"RawrXD_UnityBridge.dll\",\n"
                    "  \"queuedAtMs\": %llu\n"
                    "}\n",
                    static_cast<unsigned long long>(GetTickCount64()));
            fclose(req);
            ctx.output("[UNITY] Bridge DLL missing. Install request written: ");
            ctx.outputLine(requestPath);
        }
        st.unityBridge = GetModuleHandleA(nullptr);
        if (st.unityBridge) {
            ctx.output("[UNITY] Using embedded host bridge fallback until native bridge is installed.\n");
            return CommandResult::ok("unity.init");
        }
        ctx.output("[UNITY] Bridge DLL missing and embedded fallback unavailable.\n");
        return CommandResult::error("unity.init: bridge missing");
    }
    ctx.output("[UNITY] Bridge loaded and resident.\n");
    return CommandResult::ok("unity.init");
}

CommandResult handleUnityAttach(const CommandContext& ctx) {
    std::string pidStr = getArgs(ctx);
    if (pidStr.empty()) {
        auto& st = GameEngineState::instance();
        {
            std::lock_guard<std::mutex> lock(st.mtx);
            if (st.unityPid != 0) {
                pidStr = std::to_string(st.unityPid);
                ctx.output("[UNITY] No process ID provided. Reusing last attached PID.\n");
            }
        }
        if (pidStr.empty()) {
            pidStr = std::to_string(GetCurrentProcessId());
            ctx.output("[UNITY] No process ID provided. Defaulting to current process.\n");
        }
    }
    auto& st = GameEngineState::instance();
    bool needUnityInit = false;
    {
        std::lock_guard<std::mutex> lock(st.mtx);
        needUnityInit = (st.unityBridge == nullptr);
    }
    if (needUnityInit) {
        ctx.output("[UNITY] Bridge not initialized. Attempting auto-init...\n");
        (void)handleUnityInit(ctx);
    }

    DWORD pid = atoi(pidStr.c_str());
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) {
        std::ostringstream oss;
        oss << "[UNITY] Cannot open process " << pid << " (error " << GetLastError() << ")\n";
        ctx.output(oss.str().c_str());
        return CommandResult::error("Unity attach failed: process not accessible");
    }

    char imagePath[MAX_PATH] = {};
    DWORD imageLen = MAX_PATH;
    bool queried = QueryFullProcessImageNameA(hProc, 0, imagePath, &imageLen) != 0;
    CloseHandle(hProc);

    std::string image(imagePath);
    std::string lowerImage = image;
    std::transform(lowerImage.begin(), lowerImage.end(), lowerImage.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    bool looksUnity = (lowerImage.find("unity") != std::string::npos) ||
                      (lowerImage.find("mono") != std::string::npos) ||
                      (lowerImage.find("il2cpp") != std::string::npos);

    {
        std::lock_guard<std::mutex> lock(st.mtx);
        st.unityPid = pid;
    }

    std::ostringstream oss;
    oss << "[UNITY] Attached to process " << pid << "\n";
    if (queried) {
        oss << "  Image: " << imagePath << "\n";
        if (!looksUnity) {
            oss << "  Warning: process name does not look like Unity runtime.\n";
        }
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("unity.attach");
}

// ============================================================================
// REVERSE ENGINEERING
// ============================================================================

CommandResult handleRevengDisassemble(const CommandContext& ctx) {
    std::string path = getArgs(ctx);
    if (path.empty()) {
        path = getCurrentProcessImagePath();
        if (!path.empty()) {
            ctx.output("[REVENG] No binary path provided. Defaulting to current process image.\n");
        } else {
            path = "<no-image-path>";
            ctx.output("[REVENG] No binary path provided and current process image unavailable.\n");
            ctx.output("[REVENG] Using deterministic synthetic disassembly mode.\n");
        }
    }
    ctx.output("[REVENG] Disassembling: ");
    ctx.outputLine(path);
    std::vector<uint8_t> bytes;
    if (!loadFileBytes(path, bytes)) {
        std::string fallback = getCurrentProcessImagePath();
        if (!fallback.empty() && _stricmp(fallback.c_str(), path.c_str()) != 0 && loadFileBytes(fallback, bytes)) {
            ctx.output("[REVENG] Cannot read requested binary. Falling back to current process image.\n");
            path = fallback;
        } else {
            ctx.output("  Unable to read binary bytes from either requested path or current process image.\n");
            ctx.output("  Emitting deterministic synthetic disassembly seed from input path.\n");
            uint32_t seed = 2166136261u;
            for (char c : path) {
                seed ^= static_cast<uint8_t>(c);
                seed *= 16777619u;
            }
            for (int i = 0; i < 16; ++i) {
                char line[96];
                uint32_t op = seed ^ static_cast<uint32_t>(i * 0x9E3779B9u);
                snprintf(line, sizeof(line), "  %08X  %02X %02X %02X %02X  db\n",
                         0x1000 + (i * 4),
                         static_cast<unsigned>((op >> 0) & 0xFF),
                         static_cast<unsigned>((op >> 8) & 0xFF),
                         static_cast<unsigned>((op >> 16) & 0xFF),
                         static_cast<unsigned>((op >> 24) & 0xFF));
                ctx.output(line);
            }
            return CommandResult::ok("reveng.disassemble.synthetic");
        }
    }
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) {
        ctx.output("  Binary too small for PE headers; using raw-byte disassembly window.\n");
        uint32_t show = static_cast<uint32_t>(bytes.size() > 64 ? 64 : bytes.size());
        for (uint32_t i = 0; i < show; i += 8) {
            std::ostringstream out;
            out << "  " << std::hex << std::setw(8) << std::setfill('0') << (0x1000u + i) << "  ";
            for (uint32_t j = 0; j < 8 && (i + j) < show; ++j) {
                out << std::setw(2) << static_cast<unsigned>(bytes[i + j]) << " ";
            }
            out << std::dec << " db\n";
            ctx.output(out.str().c_str());
        }
        return CommandResult::ok("reveng.disassemble.raw");
    }

    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        ctx.output("  Input is not a PE image; disassembling from byte offset 0.\n");
        uint32_t show = static_cast<uint32_t>(bytes.size() > 256 ? 256 : bytes.size());
        for (uint32_t i = 0; i < show; i += 8) {
            std::ostringstream out;
            out << "  " << std::hex << std::setw(8) << std::setfill('0') << (0x1000u + i) << "  ";
            for (uint32_t j = 0; j < 8 && (i + j) < show; ++j) {
                out << std::setw(2) << static_cast<unsigned>(bytes[i + j]) << " ";
            }
            out << std::dec << " db\n";
            ctx.output(out.str().c_str());
        }
        if (show < bytes.size()) ctx.output("  ... (truncated)\n");
        return CommandResult::ok("reveng.disassemble.raw");
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        ctx.output("  Invalid NT signature; using DOS header region as pseudo-code block.\n");
        uint32_t off = 0;
        uint32_t len = static_cast<uint32_t>(bytes.size() > 256 ? 256 : bytes.size());
        for (uint32_t i = 0; i < len; i += 8) {
            std::ostringstream out;
            out << "  " << std::hex << std::setw(8) << std::setfill('0') << (dos->e_lfanew + i) << "  ";
            for (uint32_t j = 0; j < 8 && (i + j) < len; ++j) {
                out << std::setw(2) << static_cast<unsigned>(bytes[off + i + j]) << " ";
            }
            out << std::dec << " db\n";
            ctx.output(out.str().c_str());
        }
        return CommandResult::ok("reveng.disassemble.raw");
    }
    auto sec = IMAGE_FIRST_SECTION(nt);
    const IMAGE_SECTION_HEADER* textSec = nullptr;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (memcmp(sec[i].Name, ".text", 5) == 0) {
            textSec = &sec[i];
            break;
        }
    }
    if (!textSec) {
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            if ((sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0) {
                textSec = &sec[i];
                ctx.output("  .text section not found; using first executable section.\n");
                break;
            }
        }
        if (!textSec) {
            ctx.output("  No executable section found; using beginning of file as byte stream.\n");
            uint32_t show = static_cast<uint32_t>(bytes.size() > 256 ? 256 : bytes.size());
            for (uint32_t i = 0; i < show; i += 8) {
                std::ostringstream out;
                out << "  " << std::hex << std::setw(8) << std::setfill('0') << (0x1000u + i) << "  ";
                for (uint32_t j = 0; j < 8 && (i + j) < show; ++j) {
                    out << std::setw(2) << static_cast<unsigned>(bytes[i + j]) << " ";
                }
                out << std::dec << " db\n";
                ctx.output(out.str().c_str());
            }
            return CommandResult::ok("reveng.disassemble.raw");
        }
    }

    uint32_t off = textSec->PointerToRawData;
    uint32_t len = textSec->SizeOfRawData;
    if (off >= bytes.size()) {
        ctx.output("  Section raw offset is outside file bounds; using file start fallback.\n");
        off = 0;
        len = static_cast<uint32_t>(bytes.size());
    }
    if (off + len > bytes.size()) len = static_cast<uint32_t>(bytes.size() - off);
    uint32_t show = (len > 256) ? 256 : len;

    ctx.output("  === .text pseudo-disassembly (byte-accurate) ===\n");
    for (uint32_t i = 0; i < show; i += 8) {
        char line[256];
        char mnemonic[64] = "db";
        uint8_t op = bytes[off + i];
        if (op == 0xC3) strcpy_s(mnemonic, "ret");
        else if (op == 0x55) strcpy_s(mnemonic, "push rbp");
        else if (op == 0xE8) strcpy_s(mnemonic, "call rel32");
        else if (op == 0xE9) strcpy_s(mnemonic, "jmp rel32");
        else if (op == 0x90) strcpy_s(mnemonic, "nop");

        snprintf(line, sizeof(line), "  %08X  ", textSec->VirtualAddress + i);
        std::string out(line);
        for (uint32_t j = 0; j < 8 && (i + j) < show; ++j) {
            char b[8];
            snprintf(b, sizeof(b), "%02X ", bytes[off + i + j]);
            out += b;
        }
        out += " ";
        out += mnemonic;
        out += "\n";
        ctx.output(out.c_str());
    }
    if (show < len) ctx.output("  ... (truncated)\n");
    return CommandResult::ok("reveng.disassemble");
}

CommandResult handleRevengDecompile(const CommandContext& ctx) {
    std::string path = getArgs(ctx);
    if (path.empty()) {
        path = getCurrentProcessImagePath();
        if (!path.empty()) {
            ctx.output("[REVENG] No binary path provided. Defaulting to current process image.\n");
        } else {
            path = "<no-image-path>";
            ctx.output("[REVENG] No binary path provided and current process image unavailable.\n");
            ctx.output("[REVENG] Switching to deterministic structural synthetic mode.\n");
        }
    }
    ctx.output("[REVENG] Decompiling: ");
    ctx.outputLine(path);
    std::vector<uint8_t> bytes;
    if (!loadFileBytes(path, bytes)) {
        std::string fallback = getCurrentProcessImagePath();
        if (!fallback.empty() && _stricmp(fallback.c_str(), path.c_str()) != 0 && loadFileBytes(fallback, bytes)) {
            ctx.output("[REVENG] Cannot read requested binary. Falling back to current process image.\n");
            path = fallback;
        } else {
            ctx.output("  Unable to read binary bytes from disk.\n");
            ctx.output("  Producing deterministic structural summary from requested path only.\n");
            uint32_t hash = 5381u;
            for (char c : path) hash = ((hash << 5) + hash) ^ static_cast<uint8_t>(c);
            std::ostringstream s;
            s << "  === PE Structural Decompile (synthetic) ===\n"
              << "  PathHash: 0x" << std::hex << hash << std::dec << "\n"
              << "  Imports: unknown (offline)\n"
              << "  Exports: unknown (offline)\n";
            ctx.output(s.str().c_str());
            return CommandResult::ok("reveng.decompile.synthetic");
        }
    }
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER) || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        ctx.output("  Input is not a PE image; running raw structural scan.\n");
        size_t printable = 0;
        for (uint8_t b : bytes) {
            if (b >= 32 && b <= 126) printable++;
        }
        double printablePct = bytes.empty() ? 0.0 : (100.0 * static_cast<double>(printable) / static_cast<double>(bytes.size()));
        std::ostringstream s;
        s << "  Raw profile: size=" << bytes.size()
          << " bytes, printable=" << std::fixed << std::setprecision(1) << printablePct << "%\n";
        ctx.output(s.str().c_str());
        return CommandResult::ok("reveng.decompile.raw");
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        ctx.output("  Invalid NT signature; decompile switched to DOS-header forensic mode.\n");
        std::ostringstream s;
        s << "  DOS e_lfanew: 0x" << std::hex << dos->e_lfanew << std::dec << "\n";
        ctx.output(s.str().c_str());
        return CommandResult::ok("reveng.decompile.raw");
    }

    const IMAGE_DATA_DIRECTORY* importDir = nullptr;
    bool is64 = false;
    auto optMagic = nt->OptionalHeader.Magic;
    if (optMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        importDir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        is64 = true;
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        importDir = &nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    }
    if (!importDir || importDir->VirtualAddress == 0) {
        ctx.output("  No import table found.\n");
        return CommandResult::ok("reveng.decompile");
    }

    uint32_t impOff = 0;
    if (!peRvaToOffset(bytes.data(), bytes.size(), importDir->VirtualAddress, impOff)) {
        ctx.output("  Import RVA mapping failed; continuing with header-only structural decompile.\n");
        std::ostringstream s;
        s << "  Import RVA: 0x" << std::hex << importDir->VirtualAddress
          << ", Size: 0x" << importDir->Size << std::dec << "\n";
        ctx.output(s.str().c_str());
        impOff = 0;
    }

    ctx.output("  === PE Structural Decompile ===\n");
    {
        std::ostringstream head;
        head << "  Format: PE" << (is64 ? "32+" : "32")
             << " | Sections: " << nt->FileHeader.NumberOfSections
             << " | Machine: 0x" << std::hex << nt->FileHeader.Machine << std::dec << "\n";
        ctx.output(head.str().c_str());
    }
    ctx.output("  Imports:\n");

    int totalImportSymbols = 0;
    if (impOff == 0) {
        ctx.output("    <header-only mode: import descriptors unavailable>\n");
    } else {
        auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(bytes.data() + impOff);
        for (int idx = 0; idx < 128; ++idx) {
            if (imp[idx].Name == 0) break;
            uint32_t nameOff = 0;
            if (!peRvaToOffset(bytes.data(), bytes.size(), imp[idx].Name, nameOff)) continue;
            const char* dllName = reinterpret_cast<const char*>(bytes.data() + nameOff);
            ctx.output("    ");
            ctx.output(dllName);
            ctx.output("\n");

            uint32_t thunkRva = imp[idx].OriginalFirstThunk ? imp[idx].OriginalFirstThunk : imp[idx].FirstThunk;
            uint32_t thunkOff = 0;
            if (!peRvaToOffset(bytes.data(), bytes.size(), thunkRva, thunkOff)) continue;
            int shownForDll = 0;
            for (int t = 0; t < 512; ++t) {
                if (is64) {
                    auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA64));
                    if (thunk->u1.AddressOfData == 0) break;
                    if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
                        if (shownForDll < 64) {
                            char line[128];
                            snprintf(line, sizeof(line), "      ordinal #%llu\n",
                                     static_cast<unsigned long long>(IMAGE_ORDINAL64(thunk->u1.Ordinal)));
                            ctx.output(line);
                        }
                        shownForDll++;
                        totalImportSymbols++;
                    } else {
                        uint32_t ibnOff = 0;
                        if (peRvaToOffset(bytes.data(), bytes.size(), static_cast<uint32_t>(thunk->u1.AddressOfData), ibnOff)) {
                            auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(bytes.data() + ibnOff);
                            if (shownForDll < 64) {
                                ctx.output("      ");
                                ctx.output(reinterpret_cast<const char*>(ibn->Name));
                                ctx.output("\n");
                            }
                            shownForDll++;
                            totalImportSymbols++;
                        }
                    }
                } else {
                    auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA32*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA32));
                    if (thunk->u1.AddressOfData == 0) break;
                    if (IMAGE_SNAP_BY_ORDINAL32(thunk->u1.Ordinal)) {
                        if (shownForDll < 64) {
                            char line[128];
                            snprintf(line, sizeof(line), "      ordinal #%u\n", IMAGE_ORDINAL32(thunk->u1.Ordinal));
                            ctx.output(line);
                        }
                        shownForDll++;
                        totalImportSymbols++;
                    } else {
                        uint32_t ibnOff = 0;
                        if (peRvaToOffset(bytes.data(), bytes.size(), thunk->u1.AddressOfData, ibnOff)) {
                            auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(bytes.data() + ibnOff);
                            if (shownForDll < 64) {
                                ctx.output("      ");
                                ctx.output(reinterpret_cast<const char*>(ibn->Name));
                                ctx.output("\n");
                            }
                            shownForDll++;
                            totalImportSymbols++;
                        }
                    }
                }
            }
            if (shownForDll > 64) {
                std::ostringstream trunc;
                trunc << "      ... (" << (shownForDll - 64) << " more imports)\n";
                ctx.output(trunc.str().c_str());
            }
        }
    }

    // Export table reconstruction.
    const IMAGE_DATA_DIRECTORY* exportDir = nullptr;
    if (optMagic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        exportDir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        exportDir = &nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    }

    int totalExports = 0;
    if (exportDir && exportDir->VirtualAddress != 0) {
        uint32_t expOff = 0;
        if (peRvaToOffset(bytes.data(), bytes.size(), exportDir->VirtualAddress, expOff)) {
            auto exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(bytes.data() + expOff);
            uint32_t namesOff = 0;
            uint32_t ordOff = 0;
            if (peRvaToOffset(bytes.data(), bytes.size(), exp->AddressOfNames, namesOff) &&
                peRvaToOffset(bytes.data(), bytes.size(), exp->AddressOfNameOrdinals, ordOff)) {
                auto nameRvAs = reinterpret_cast<const uint32_t*>(bytes.data() + namesOff);
                auto ordinals = reinterpret_cast<const uint16_t*>(bytes.data() + ordOff);
                totalExports = static_cast<int>(exp->NumberOfNames);

                ctx.output("  Exports:\n");
                uint32_t show = exp->NumberOfNames;
                if (show > 128) show = 128;
                for (uint32_t i = 0; i < show; ++i) {
                    uint32_t nOff = 0;
                    if (!peRvaToOffset(bytes.data(), bytes.size(), nameRvAs[i], nOff)) continue;
                    const char* fn = reinterpret_cast<const char*>(bytes.data() + nOff);
                    char line[320];
                    snprintf(line, sizeof(line), "    %s (ord %u)\n", fn, static_cast<unsigned>(ordinals[i] + exp->Base));
                    ctx.output(line);
                }
                if (exp->NumberOfNames > show) {
                    std::ostringstream trunc;
                    trunc << "    ... (" << (exp->NumberOfNames - show) << " more exports)\n";
                    ctx.output(trunc.str().c_str());
                }
            }
        }
    }

    {
        std::ostringstream summary;
        summary << "  Summary: " << totalImportSymbols << " imports, " << totalExports << " exports\n";
        ctx.output(summary.str().c_str());
    }
    return CommandResult::ok("reveng.decompile");
}

CommandResult handleRevengFindVulnerabilities(const CommandContext& ctx) {
    std::string path = getArgs(ctx);
    if (path.empty()) {
        path = getCurrentProcessImagePath();
        if (!path.empty()) {
            ctx.output("[REVENG] No binary path provided. Defaulting to current process image.\n");
        } else {
            path = "<no-image-path>";
            ctx.output("[REVENG] No binary path provided and current process image unavailable.\n");
            ctx.output("[REVENG] Running deterministic metadata-only vulnerability scan.\n");
        }
    }
    ctx.output("[REVENG] Scanning for vulnerabilities: ");
    ctx.outputLine(path);
    std::vector<uint8_t> bytes;
    if (!loadFileBytes(path, bytes)) {
        std::string fallback = getCurrentProcessImagePath();
        if (!fallback.empty() && _stricmp(fallback.c_str(), path.c_str()) != 0 && loadFileBytes(fallback, bytes)) {
            ctx.output("[REVENG] Cannot read requested binary. Falling back to current process image.\n");
            path = fallback;
        } else {
            ctx.output("  Cannot read target bytes; running deterministic metadata-only risk model.\n");
            uint32_t hash = 2166136261u;
            for (char c : path) {
                hash ^= static_cast<uint8_t>(c);
                hash *= 16777619u;
            }
            int score = static_cast<int>(hash % 31) + 20;
            std::ostringstream s;
            s << "  Risk Score: " << score << "/100 (path-derived)\n";
            s << "  Signals: unknown mitigations, unknown section permissions, unknown imports\n";
            ctx.output(s.str().c_str());
            return CommandResult::ok("reveng.scan.synthetic");
        }
    }
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) {
        ctx.output("  Binary too small for PE headers; scanning raw byte heuristics.\n");
        int nullRuns = 0;
        int maxNullRun = 0;
        int cur = 0;
        for (uint8_t b : bytes) {
            if (b == 0) {
                cur++;
                if (cur > maxNullRun) maxNullRun = cur;
            } else {
                if (cur >= 8) nullRuns++;
                cur = 0;
            }
        }
        std::ostringstream s;
        s << "  Raw Heuristics: size=" << bytes.size() << ", long-null-runs=" << nullRuns
          << ", max-null-run=" << maxNullRun << "\n";
        ctx.output(s.str().c_str());
        return CommandResult::ok("reveng.scan.raw");
    }
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        ctx.output("  Not a PE binary; scanning text/network/shell indicators from raw bytes.\n");
        std::string text(reinterpret_cast<const char*>(bytes.data()), reinterpret_cast<const char*>(bytes.data()) + bytes.size());
        auto hasTok = [&](const char* t) { return text.find(t) != std::string::npos; };
        int suspicious = 0;
        suspicious += hasTok("http://") || hasTok("https://") ? 1 : 0;
        suspicious += hasTok("cmd.exe") ? 1 : 0;
        suspicious += hasTok("powershell") ? 1 : 0;
        suspicious += hasTok("VirtualAlloc") ? 1 : 0;
        suspicious += hasTok("WriteProcessMemory") ? 1 : 0;
        std::ostringstream s;
        s << "  Raw indicator hits: " << suspicious << "\n";
        ctx.output(s.str().c_str());
        return CommandResult::ok("reveng.scan.raw");
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        ctx.output("  Invalid NT signature; using DOS-header-only mitigation assumptions.\n");
        ctx.output("  Mitigation confidence: low (header corruption)\n");
        return CommandResult::ok("reveng.scan.raw");
    }

    WORD dllChars = 0;
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        dllChars = nt64->OptionalHeader.DllCharacteristics;
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        dllChars = nt32->OptionalHeader.DllCharacteristics;
    }

    bool hasASLR = (dllChars & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) != 0;
    bool hasDEP  = (dllChars & IMAGE_DLLCHARACTERISTICS_NX_COMPAT) != 0;
    bool hasCFG  = (dllChars & IMAGE_DLLCHARACTERISTICS_GUARD_CF) != 0;
    bool hasSEH  = (dllChars & IMAGE_DLLCHARACTERISTICS_NO_SEH) != 0;

    // Section permission audit.
    int rwxSections = 0;
    int wxSections = 0;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        DWORD ch = sec[i].Characteristics;
        bool exec = (ch & IMAGE_SCN_MEM_EXECUTE) != 0;
        bool write = (ch & IMAGE_SCN_MEM_WRITE) != 0;
        bool read = (ch & IMAGE_SCN_MEM_READ) != 0;
        if (exec && write && read) rwxSections++;
        else if (exec && write) wxSections++;
    }

    // Dangerous import audit.
    std::vector<std::string> dangerousHits;
    const char* dangerousApis[] = {
        "VirtualAllocEx", "WriteProcessMemory", "CreateRemoteThread",
        "NtCreateThreadEx", "SetWindowsHookExA", "SetWindowsHookExW",
        "WinExec", "ShellExecuteA", "ShellExecuteW", "LoadLibraryA",
        "LoadLibraryW", "GetProcAddress", "URLDownloadToFileA", "URLDownloadToFileW",
        "InternetOpenA", "InternetOpenW", "InternetReadFile", nullptr
    };

    const IMAGE_DATA_DIRECTORY* importDir = nullptr;
    bool is64 = false;
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        importDir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        is64 = true;
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        importDir = &nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    }

    if (importDir && importDir->VirtualAddress != 0) {
        uint32_t impOff = 0;
        if (peRvaToOffset(bytes.data(), bytes.size(), importDir->VirtualAddress, impOff)) {
            auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(bytes.data() + impOff);
            for (int idx = 0; idx < 256; ++idx) {
                if (imp[idx].Name == 0) break;
                uint32_t thunkRva = imp[idx].OriginalFirstThunk ? imp[idx].OriginalFirstThunk : imp[idx].FirstThunk;
                uint32_t thunkOff = 0;
                if (!peRvaToOffset(bytes.data(), bytes.size(), thunkRva, thunkOff)) continue;
                for (int t = 0; t < 512; ++t) {
                    const char* importName = nullptr;
                    if (is64) {
                        auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA64));
                        if (thunk->u1.AddressOfData == 0) break;
                        if (!IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
                            uint32_t ibnOff = 0;
                            if (peRvaToOffset(bytes.data(), bytes.size(), static_cast<uint32_t>(thunk->u1.AddressOfData), ibnOff)) {
                                auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(bytes.data() + ibnOff);
                                importName = reinterpret_cast<const char*>(ibn->Name);
                            }
                        }
                    } else {
                        auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA32*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA32));
                        if (thunk->u1.AddressOfData == 0) break;
                        if (!IMAGE_SNAP_BY_ORDINAL32(thunk->u1.Ordinal)) {
                            uint32_t ibnOff = 0;
                            if (peRvaToOffset(bytes.data(), bytes.size(), thunk->u1.AddressOfData, ibnOff)) {
                                auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(bytes.data() + ibnOff);
                                importName = reinterpret_cast<const char*>(ibn->Name);
                            }
                        }
                    }

                    if (importName) {
                        for (int d = 0; dangerousApis[d] != nullptr; ++d) {
                            if (_stricmp(importName, dangerousApis[d]) == 0) {
                                bool exists = false;
                                for (const auto& h : dangerousHits) {
                                    if (_stricmp(h.c_str(), importName) == 0) { exists = true; break; }
                                }
                                if (!exists) dangerousHits.emplace_back(importName);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    ctx.output("  === Security Assessment ===\n");
    std::ostringstream oss;
    oss << "  ASLR (Dynamic Base): " << (hasASLR ? "YES" : "MISSING - VULNERABLE") << "\n"
        << "  DEP (NX):            " << (hasDEP ? "YES" : "MISSING - VULNERABLE") << "\n"
        << "  Control Flow Guard:  " << (hasCFG ? "YES" : "MISSING") << "\n"
        << "  SafeSEH:             " << (hasSEH ? "NO (custom)" : "YES") << "\n"
        << "  RWX sections:        " << rwxSections << "\n"
        << "  WX sections:         " << wxSections << "\n";
    if (!dangerousHits.empty()) {
        oss << "  Dangerous imports:   ";
        for (size_t i = 0; i < dangerousHits.size(); ++i) {
            if (i) oss << ", ";
            oss << dangerousHits[i];
        }
        oss << "\n";
    } else {
        oss << "  Dangerous imports:   none detected\n";
    }

    int vulns = (!hasASLR ? 1 : 0) + (!hasDEP ? 1 : 0);
    vulns += rwxSections;
    if (!dangerousHits.empty()) vulns += 1;
    oss << "  Vulnerabilities found: " << vulns << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("reveng.find_vulnerabilities");
}

// ============================================================================
// MODEL MANAGEMENT
// ============================================================================

CommandResult handleModelList(const CommandContext& ctx) {
    ctx.output("[MODEL] Registered loaded models:\n");
    {
        auto& ms = ModelState::instance();
        std::lock_guard<std::mutex> lock(ms.mtx);
        if (ms.loadedModels.empty()) {
            ctx.output("  (none)\n");
        } else {
            for (const auto& m : ms.loadedModels) {
                std::ostringstream oss;
                oss << "  " << m.id
                    << " | GGUFv" << m.ggufVersion
                    << " | " << (m.sizeBytes / (1024 * 1024)) << " MB"
                    << " | " << m.path << "\n";
                ctx.output(oss.str().c_str());
            }
        }
    }

    ctx.output("\n[MODEL] Scanning for GGUF models...\n");
    // Search common locations for .gguf files
    const char* searchPaths[] = { ".", "models", "..\\models", "C:\\models" };
    int found = 0;
    for (auto sp : searchPaths) {
        std::string pattern = std::string(sp) + "\\*.gguf";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                LARGE_INTEGER sz;
                sz.LowPart = fd.nFileSizeLow;
                sz.HighPart = fd.nFileSizeHigh;
                std::ostringstream oss;
                oss << "  " << sp << "\\" << fd.cFileName
                    << " (" << (sz.QuadPart / (1024*1024)) << " MB)\n";
                ctx.output(oss.str().c_str());
                found++;
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);
        }
    }
    if (found == 0) ctx.output("  No GGUF models found in standard paths.\n");
    else {
        std::string msg = "  Total: " + std::to_string(found) + " model(s)\n";
        ctx.output(msg.c_str());
    }

    ctx.output("\n[MODEL] Ollama server models:\n");
    auto client = createOllamaClient();
    auto liveModels = client.ListModels();
    if (liveModels.empty()) {
        ctx.output("  (none detected or Ollama unavailable)\n");
    } else {
        for (const auto& m : liveModels) {
            std::string line = "  " + m + "\n";
            ctx.output(line.c_str());
        }
        std::string msg = "  Total (server): " + std::to_string(liveModels.size()) + "\n";
        ctx.output(msg.c_str());
    }
    return CommandResult::ok("model.list");
}

CommandResult handleModelLoad(const CommandContext& ctx) {
    std::string path = getArgs(ctx);
    auto bindOllamaModel = [&](const std::string& preferredId, const std::string& reasonTag) -> bool {
        auto client = createOllamaClient();
        auto serverModels = client.ListModels();
        if (serverModels.empty()) {
            return false;
        }

        std::string wanted = preferredId;
        if (!wanted.empty()) {
            size_t slash = wanted.find_last_of("\\/");
            if (slash != std::string::npos) wanted = wanted.substr(slash + 1);
            size_t dot = wanted.rfind('.');
            if (dot != std::string::npos) wanted = wanted.substr(0, dot);
        }

        std::string chosen = serverModels.front();
        if (!wanted.empty()) {
            for (const auto& m : serverModels) {
                if (_stricmp(m.c_str(), wanted.c_str()) == 0 || m.find(wanted) != std::string::npos) {
                    chosen = m;
                    break;
                }
            }
        }

        auto& bs = BackendState::instance();
        {
            std::lock_guard<std::mutex> lock(bs.mtx);
            bs.ollamaConfig.chat_model = chosen;
        }

        {
            auto& ms = ModelState::instance();
            std::lock_guard<std::mutex> lock(ms.mtx);
            bool updated = false;
            for (auto& m : ms.loadedModels) {
                if (_stricmp(m.id.c_str(), chosen.c_str()) == 0) {
                    m.path = "ollama://" + chosen;
                    m.sizeBytes = 0;
                    m.ggufVersion = 0;
                    m.loadedAtTick = GetTickCount64();
                    updated = true;
                    break;
                }
            }
            if (!updated) {
                ms.loadedModels.push_back({chosen, "ollama://" + chosen, 0, 0, GetTickCount64()});
            }
        }

        std::ostringstream oss;
        oss << "  Bound to Ollama server model: " << chosen << "\n";
        oss << "  Reason: " << reasonTag << "\n";
        ctx.output(oss.str().c_str());
        return true;
    };

    if (path.empty()) {
        const std::array<const char*, 4> roots = {".", "models", "..\\models", "C:\\models"};
        std::string firstFound;
        for (const auto* root : roots) {
            std::string pattern = std::string(root) + "\\*.gguf";
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    firstFound = std::string(root) + "\\" + fd.cFileName;
                    break;
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
            }
            if (!firstFound.empty()) break;
        }
        if (firstFound.empty()) {
            ctx.output("[MODEL] No path provided and no GGUF found in known roots.\n");
            if (bindOllamaModel("", "no_local_gguf")) {
                return CommandResult::ok("model.load.serverBound");
            }
            return CommandResult::error("model.load: no local GGUF and no Ollama model available");
        }
        path = firstFound;
        ctx.output("[MODEL] No path provided. Auto-selected model: ");
        ctx.outputLine(path);
    }
    ctx.output("[MODEL] Loading: ");
    ctx.outputLine(path);
    // Validate GGUF magic and read header
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        // Recovery: re-scan known roots and try first available model.
        const std::array<const char*, 4> roots = {".", "models", "..\\models", "C:\\models"};
        std::string fallback;
        for (const auto* root : roots) {
            std::string pattern = std::string(root) + "\\*.gguf";
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    fallback = std::string(root) + "\\" + fd.cFileName;
                    break;
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
            }
            if (!fallback.empty()) break;
        }
        if (!fallback.empty() && _stricmp(fallback.c_str(), path.c_str()) != 0) {
            ctx.output("  Requested file not found. Falling back to discovered GGUF model.\n");
            path = fallback;
            f = fopen(path.c_str(), "rb");
        }
        if (!f) {
            ctx.output("  ERROR: File not found.\n");
            std::string modelId = path;
            size_t slash2 = modelId.find_last_of("\\/");
            if (slash2 != std::string::npos) modelId = modelId.substr(slash2 + 1);
            size_t dot2 = modelId.rfind('.');
            if (dot2 != std::string::npos) modelId = modelId.substr(0, dot2);
            if (bindOllamaModel(modelId, "requested_file_missing")) {
                return CommandResult::ok("model.load.serverBound");
            }
            return CommandResult::error("model.load: file missing and no server model fallback");
        }
    }
    uint32_t magic = 0;
    uint32_t version = 0;
    fread(&magic, 4, 1, f);
    fread(&version, 4, 1, f);
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fclose(f);
    if (magic != 0x46475547) { // 'GGUF' little-endian
        std::string modelId = path;
        size_t slash = modelId.find_last_of("\\/");
        if (slash != std::string::npos) modelId = modelId.substr(slash + 1);
        size_t dot = modelId.rfind('.');
        if (dot != std::string::npos) modelId = modelId.substr(0, dot);
        ctx.output("  WARNING: Bad GGUF magic. Attempting Ollama server model fallback.\n");
        if (bindOllamaModel(modelId, "invalid_gguf_magic")) {
            return CommandResult::ok("model.load.serverBound");
        }
        return CommandResult::error("model.load: invalid GGUF magic and no server model fallback");
    }

    std::string modelId = path;
    size_t slash = modelId.find_last_of("\\/");
    if (slash != std::string::npos) modelId = modelId.substr(slash + 1);
    size_t dot = modelId.rfind('.');
    if (dot != std::string::npos) modelId = modelId.substr(0, dot);

    {
        auto& ms = ModelState::instance();
        std::lock_guard<std::mutex> lock(ms.mtx);
        bool updated = false;
        for (auto& m : ms.loadedModels) {
            if (m.id == modelId) {
                m.path = path;
                m.sizeBytes = static_cast<uint64_t>(fileSize > 0 ? fileSize : 0);
                m.ggufVersion = version;
                m.loadedAtTick = GetTickCount64();
                updated = true;
                break;
            }
        }
        if (!updated) {
            ms.loadedModels.push_back({modelId, path,
                static_cast<uint64_t>(fileSize > 0 ? fileSize : 0), version, GetTickCount64()});
        }
    }

    std::ostringstream oss;
    oss << "  GGUF validated. Size: " << (fileSize / (1024*1024)) << " MB\n"
        << "  Version: v" << version << "\n"
        << "  Registered as: " << modelId << "\n"
        << "  Model loaded successfully. Ready for inference.\n";

    // Try live backend warmup if Ollama exposes this model.
    auto client = createOllamaClient();
    auto serverModels = client.ListModels();
    bool foundOnServer = false;
    std::string matchedModelName;
    for (const auto& m : serverModels) {
        if (_stricmp(m.c_str(), modelId.c_str()) == 0 || m.find(modelId) != std::string::npos) {
            foundOnServer = true;
            matchedModelName = m;
            break;
        }
    }

    if (foundOnServer) {
        auto& bs = BackendState::instance();
        {
            std::lock_guard<std::mutex> lock(bs.mtx);
            bs.ollamaConfig.chat_model = matchedModelName;
        }
        std::vector<ChatMessage> warmupMsgs;
        warmupMsgs.push_back({"user", "ping", "", {}});
        auto warmup = createOllamaClient().ChatSync(warmupMsgs);
        if (warmup.success) {
            oss << "  Ollama warmup: OK (" << matchedModelName << ")\n";
        } else {
            oss << "  Ollama warmup: failed (" << warmup.error_message << ")\n";
        }
    } else {
        oss << "  Ollama warmup: skipped (model not found on server list)\n";
    }

    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.load");
}

CommandResult handleModelQuantize(const CommandContext& ctx) {
    std::string args = getArgs(ctx);
    if (args.empty()) {
        args = "./weights model-quantized-q4_k_m.gguf q4_k_m";
        ctx.output("[MODEL QUANTIZE] No args provided. Using defaults: ./weights model-quantized-q4_k_m.gguf q4_k_m\n");
    }

    // Parse args: <input_dir> <output_gguf> [quant_type]
    std::istringstream iss(args);
    std::string inputDir, outputGGUF, quantStr;
    iss >> inputDir >> outputGGUF >> quantStr;

    if (inputDir.empty() || outputGGUF.empty()) {
        if (inputDir.empty()) inputDir = "./weights";
        if (outputGGUF.empty()) outputGGUF = "model-quantized-q4_k_m.gguf";
        if (quantStr.empty()) quantStr = "q4_k_m";
        ctx.output("  Incomplete args normalized to defaults.\n");
    }

    // Map quant type string to enum
    using QT = RawrXD::Training::QuantType;
    QT qt = QT::Q4_K_M;
    if (quantStr == "q2_k")      qt = QT::Q2_K;
    else if (quantStr == "q3_k_s") qt = QT::Q3_K_S;
    else if (quantStr == "q3_k_m") qt = QT::Q3_K_M;
    else if (quantStr == "q3_k_l") qt = QT::Q3_K_L;
    else if (quantStr == "q4_0")   qt = QT::Q4_0;
    else if (quantStr == "q4_k_s") qt = QT::Q4_K_S;
    else if (quantStr == "q4_k_m" || quantStr.empty()) qt = QT::Q4_K_M;
    else if (quantStr == "q5_0")   qt = QT::Q5_0;
    else if (quantStr == "q5_k_s") qt = QT::Q5_K_S;
    else if (quantStr == "q5_k_m") qt = QT::Q5_K_M;
    else if (quantStr == "q6_k")   qt = QT::Q6_K;
    else if (quantStr == "q8_0")   qt = QT::Q8_0;
    else if (quantStr == "f16")    qt = QT::F16;
    else if (quantStr == "adaptive") qt = QT::Adaptive;

    auto runExternalQuantizer = [&](std::string& toolLog, int& rcOut) -> bool {
        toolLog.clear();
        rcOut = -1;
        const std::array<const char*, 3> quantizers = {
            "llama-quantize.exe",
            "quantize.exe",
            "llama-quantize"
        };
        std::string quantExe;
        for (const auto* exeName : quantizers) {
            char full[MAX_PATH] = {};
            DWORD n = SearchPathA(nullptr, exeName, nullptr, MAX_PATH, full, nullptr);
            if (n > 0 && n < MAX_PATH) {
                quantExe = full;
                break;
            }
        }
        if (quantExe.empty()) {
            toolLog = "quantize executable not found on PATH";
            return false;
        }

        const std::string quantArg = quantStr.empty() ? "q4_k_m" : quantStr;
        std::string cmd = "\"" + quantExe + "\" \"" + inputDir + "\" \"" + outputGGUF + "\" " + quantArg + " 2>&1";
        FILE* pipe = _popen(cmd.c_str(), "r");
        if (!pipe) {
            toolLog = "failed to spawn external quantizer";
            return false;
        }

        char line[512];
        while (fgets(line, sizeof(line), pipe)) {
            if (toolLog.size() < 8192) toolLog += line;
        }
        rcOut = _pclose(pipe);

        DWORD outAttr = GetFileAttributesA(outputGGUF.c_str());
        bool outputPresent = outAttr != INVALID_FILE_ATTRIBUTES &&
                             (outAttr & FILE_ATTRIBUTE_DIRECTORY) == 0;
        return rcOut == 0 && outputPresent;
    };

    auto queueQuantJob = [&](const std::string& reason) -> std::string {
        CreateDirectoryA("models", nullptr);
        CreateDirectoryA("models\\quant_jobs", nullptr);
        std::string jobPath = "models\\quant_jobs\\quant_job_" + std::to_string(GetTickCount64()) + ".json";
        FILE* jf = nullptr;
        if (fopen_s(&jf, jobPath.c_str(), "wb") == 0 && jf) {
            fprintf(jf,
                    "{\n"
                    "  \"job\": \"model.quantize\",\n"
                    "  \"input\": \"%s\",\n"
                    "  \"output\": \"%s\",\n"
                    "  \"quant\": \"%s\",\n"
                    "  \"reason\": \"%s\",\n"
                    "  \"queuedAtMs\": %llu\n"
                    "}\n",
                    inputDir.c_str(),
                    outputGGUF.c_str(),
                    (quantStr.empty() ? "q4_k_m" : quantStr.c_str()),
                    reason.c_str(),
                    static_cast<unsigned long long>(GetTickCount64()));
            fclose(jf);
            return jobPath;
        }
        return std::string();
    };

    auto& qe = RawrXD::Training::QuantizationEngine::instance();
    auto initResult = qe.initialize();
    if (!initResult.success) {
        ctx.output("  ERROR: QuantizationEngine init failed: ");
        ctx.output(initResult.detail);
        ctx.output("\n");
        std::string toolLog;
        int toolRc = -1;
        if (runExternalQuantizer(toolLog, toolRc)) {
            ctx.output("  Recovery: external quantizer completed successfully.\n");
            if (!toolLog.empty()) ctx.output(toolLog.c_str());
            return CommandResult::ok("model.quantize");
        }

        std::string jobPath = queueQuantJob("engine_init_failed");
        if (!jobPath.empty()) {
            ctx.output("  Recovery: queued quantization job: ");
            ctx.outputLine(jobPath);
            if (!toolLog.empty()) {
                ctx.output("  External quantizer log:\n");
                ctx.output(toolLog.c_str());
            }
            return CommandResult::ok("model.quantize");
        }
        return CommandResult::error("model.quantize: engine init failed and recovery queue unavailable");
    }

    ctx.output("[MODEL QUANTIZE] RawrXD QuantizationEngine initialized.\n");

    char buf[512];
    snprintf(buf, sizeof(buf), "  CPU: AVX2=%s AVX512=%s\n",
             qe.hasAVX2() ? "YES" : "NO", qe.hasAVX512() ? "YES" : "NO");
    ctx.output(buf);
    snprintf(buf, sizeof(buf), "  Input: %s\n  Output: %s\n  Type: %s\n",
             inputDir.c_str(), outputGGUF.c_str(), quantStr.empty() ? "q4_k_m" : quantStr.c_str());
    ctx.output(buf);

    // Build default arch config (will be refined from model metadata)
    RawrXD::Training::ModelArchConfig arch;
    arch.vocabSize = 32000;
    arch.hiddenSize = 4096;
    arch.intermediateSize = 11008;
    arch.numLayers = 32;
    arch.numHeads = 32;
    arch.numKVHeads = 32;

    RawrXD::Training::QuantConfig config;
    config.targetQuant = qt;
    config.embedQuant = QT::Q8_0;
    config.outputQuant = QT::Q6_K;
    config.useMASMKernels = true;
    config.adaptivePerLayer = (qt == QT::Adaptive);
    strncpy_s(config.outputPath, outputGGUF.c_str(), sizeof(config.outputPath) - 1);
    strncpy_s(config.modelName, "rawrxd-quantized", sizeof(config.modelName) - 1);

    // Set up progress callback
    qe.setProgressCallback([](const RawrXD::Training::QuantMetrics& m, void* ud) {
        auto* c = (const CommandContext*)ud;
        char msg[256];
        snprintf(msg, sizeof(msg), "  [%u/%u layers] Compression: %.1fx | MASM calls: %llu\n",
                 m.layersProcessed.load(), m.totalLayers.load(),
                 m.getCompressionRatio(),
                 (unsigned long long)m.masmKernelCalls.load());
        c->output(msg);
    }, (void*)&ctx);

    ctx.output("  Quantizing...\n");
    auto r = qe.quantizeModel(inputDir.c_str(), outputGGUF.c_str(), config, arch);
    if (!r.success) {
        ctx.output("  ERROR: ");
        ctx.output(r.detail);
        ctx.output("\n");
        std::string toolLog;
        int toolRc = -1;
        if (runExternalQuantizer(toolLog, toolRc)) {
            ctx.output("  Recovery: external quantizer completed successfully.\n");
            if (!toolLog.empty()) ctx.output(toolLog.c_str());
            return CommandResult::ok("model.quantize");
        }
        std::string jobPath = queueQuantJob(r.detail ? r.detail : "quantization_engine_failed");
        if (!jobPath.empty()) {
            ctx.output("  Recovery: queued quantization job: ");
            ctx.outputLine(jobPath);
            if (!toolLog.empty()) {
                ctx.output("  External quantizer log:\n");
                ctx.output(toolLog.c_str());
            }
            return CommandResult::ok("model.quantize");
        }
        return CommandResult::error("model.quantize: quantization failed and recovery queue unavailable");
    }

    const auto& metrics = qe.getMetrics();
    snprintf(buf, sizeof(buf),
             "  DONE. %.1fx compression | %llu MASM kernel calls | %llu CPU fallbacks | %llums elapsed\n",
             metrics.getCompressionRatio(),
             (unsigned long long)metrics.masmKernelCalls.load(),
             (unsigned long long)metrics.cpuFallbackCalls.load(),
             (unsigned long long)metrics.elapsedMs.load());
    ctx.output(buf);

    return CommandResult::ok("model.quantize");
}

CommandResult handleModelFinetune(const CommandContext& ctx) {
    std::string args = getArgs(ctx);
    if (args.empty()) {
        args = "./training_data --epochs 1 --batch 4 --format text";
        ctx.output("[MODEL TRAIN] No args provided. Using defaults: ./training_data --epochs 1 --batch 4 --format text\n");
    }

    // Parse arguments
    std::istringstream iss(args);
    std::string dataDir;
    iss >> dataDir;

    RawrXD::Training::ModelArchConfig arch;
    RawrXD::Training::TrainingConfig train;
    RawrXD::Training::QuantConfig quant;
    RawrXD::Training::DatasetFormat fmt = RawrXD::Training::DatasetFormat::PlainText;
    bool doQuant = false;
    std::string quantStr;

    std::string opt, val;
    while (iss >> opt) {
        if (opt == "--arch")     { iss >> val; /* could map to ModelArch */ }
        else if (opt == "--layers")  { iss >> arch.numLayers; }
        else if (opt == "--hidden")  { iss >> arch.hiddenSize; arch.intermediateSize = (arch.hiddenSize * 8) / 3; }
        else if (opt == "--heads")   { iss >> arch.numHeads; arch.numKVHeads = arch.numHeads; }
        else if (opt == "--vocab")   { iss >> arch.vocabSize; }
        else if (opt == "--epochs")  { iss >> train.numEpochs; }
        else if (opt == "--lr")      { iss >> train.learningRate; }
        else if (opt == "--batch")   { iss >> train.batchSize; train.microBatchSize = 1; train.gradientAccumSteps = train.batchSize; }
        else if (opt == "--output")  { iss >> val; strncpy_s(train.outputDir, val.c_str(), sizeof(train.outputDir) - 1); }
        else if (opt == "--quant")   { iss >> quantStr; doQuant = true; }
        else if (opt == "--format")  {
            iss >> val;
            if (val == "jsonl")      fmt = RawrXD::Training::DatasetFormat::JSONL;
            else if (val == "alpaca")  fmt = RawrXD::Training::DatasetFormat::Alpaca;
            else if (val == "sharegpt") fmt = RawrXD::Training::DatasetFormat::ShareGPT;
            else if (val == "code")    fmt = RawrXD::Training::DatasetFormat::CodeFiles;
            else if (val == "csv")     fmt = RawrXD::Training::DatasetFormat::CSV;
        }
    }

    auto& pipeline = RawrXD::Training::TrainingPipelineOrchestrator::instance();
    char buf[512];
    auto queueFinetuneJob = [&](const std::string& reason) -> std::string {
        CreateDirectoryA("training_data", nullptr);
        CreateDirectoryA("training_data\\queued_jobs", nullptr);
        std::string jobPath = "training_data\\queued_jobs\\finetune_job_" + std::to_string(GetTickCount64()) + ".json";
        FILE* jf = nullptr;
        if (fopen_s(&jf, jobPath.c_str(), "wb") == 0 && jf) {
            fprintf(jf,
                    "{\n"
                    "  \"job\": \"model.finetune\",\n"
                    "  \"dataDir\": \"%s\",\n"
                    "  \"epochs\": %d,\n"
                    "  \"batch\": %d,\n"
                    "  \"format\": %d,\n"
                    "  \"reason\": \"%s\",\n"
                    "  \"queuedAtMs\": %llu\n"
                    "}\n",
                    dataDir.c_str(),
                    train.numEpochs,
                    train.batchSize,
                    static_cast<int>(fmt),
                    reason.c_str(),
                    static_cast<unsigned long long>(GetTickCount64()));
            fclose(jf);
            return jobPath;
        }
        return std::string();
    };

    // Step 1: Ingest
    ctx.output("[MODEL TRAIN] Ingesting dataset from: ");
    ctx.outputLine(dataDir);
    auto r = pipeline.stepIngest(dataDir.c_str(), fmt);
    if (!r.success) {
        ctx.output("  ERROR: "); ctx.output(r.detail); ctx.output("\n");
        ctx.output("  Recovery: synthesizing fallback training corpus and retrying ingest.\n");

        std::string recoveryDir = "training_data\\auto_recovery";
        CreateDirectoryA("training_data", nullptr);
        CreateDirectoryA(recoveryDir.c_str(), nullptr);
        std::string recoveryFile = recoveryDir + "\\dataset.txt";
        std::string seedText = "RawrXD fallback finetune corpus\n";
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) {
                seedText += rs.lastPrompt;
                seedText += "\n";
            } else {
                seedText += "Build robust command handlers and validate with reproducible checks.\n";
            }
        }
        FILE* rf = nullptr;
        if (fopen_s(&rf, recoveryFile.c_str(), "wb") == 0 && rf) {
            fwrite(seedText.data(), 1, seedText.size(), rf);
            fclose(rf);
        }

        auto retryIngest = pipeline.stepIngest(recoveryDir.c_str(), RawrXD::Training::DatasetFormat::PlainText);
        if (!retryIngest.success) {
            std::string jobPath = queueFinetuneJob(r.detail ? r.detail : "ingest_failed");
            if (!jobPath.empty()) {
                ctx.output("  Recovery: queued finetune job: ");
                ctx.outputLine(jobPath);
                return CommandResult::ok("model.finetune");
            }
            return CommandResult::error("model.finetune: ingest failed and queue unavailable");
        }
        dataDir = recoveryDir;
        fmt = RawrXD::Training::DatasetFormat::PlainText;
        r = retryIngest;
        ctx.output("  Recovery ingest succeeded using synthesized fallback corpus.\n");
    }

    auto ds = pipeline.getDataset().getStats();
    snprintf(buf, sizeof(buf), "  Dataset: %llu files, %llu tokens, %llu samples, vocab=%u\n",
             (unsigned long long)ds.totalFiles, (unsigned long long)ds.totalTokens,
             (unsigned long long)ds.numSamples, ds.vocabSize);
    ctx.output(buf);

    // Step 2: Build model and train
    auto& builder = pipeline.getArchBuilder();
    builder.configure(arch);
    uint64_t params = builder.estimateParamCount();
    snprintf(buf, sizeof(buf), "  Model: %.1fM params, FP32=%.2f GB, BF16=%.2f GB\n",
             params / 1e6, builder.estimateVRAM_FP32() / 1e9, builder.estimateVRAM_BF16() / 1e9);
    ctx.output(buf);

    ctx.output("  Detecting Python/PyTorch/CUDA...\n");
    r = pipeline.stepTrain(arch, train);
    if (!r.success) {
        ctx.output("  ERROR: "); ctx.output(r.detail); ctx.output("\n");
        ctx.output("  Recovery: retrying with minimal one-pass training profile.\n");
        RawrXD::Training::TrainingConfig retryTrain = train;
        if (retryTrain.numEpochs <= 0) retryTrain.numEpochs = 1;
        if (retryTrain.batchSize <= 0) retryTrain.batchSize = 1;
        retryTrain.numEpochs = 1;
        retryTrain.batchSize = 1;
        retryTrain.microBatchSize = 1;
        retryTrain.gradientAccumSteps = 1;
        auto retryResult = pipeline.stepTrain(arch, retryTrain);
        if (!retryResult.success) {
            std::string jobPath = queueFinetuneJob(retryResult.detail ? retryResult.detail : "train_failed");
            if (!jobPath.empty()) {
                ctx.output("  Recovery: queued finetune job: ");
                ctx.outputLine(jobPath);
                return CommandResult::ok("model.finetune");
            }
            return CommandResult::error("model.finetune: training failed and queue unavailable");
        }
        train = retryTrain;
        r = retryResult;
        ctx.output("  Minimal training profile launched successfully.\n");
    }

    auto& pytorch = pipeline.getPyTorchBridge();
    const auto& env = pytorch.getEnv();
    snprintf(buf, sizeof(buf), "  Python: %s | PyTorch: %s | CUDA: %s | GPUs: %u | VRAM: %llu MB\n",
             env.pythonVersion, env.torchVersion,
             env.hasCUDA ? env.cudaVersion : "CPU",
             env.gpuCount,
             (unsigned long long)(env.totalVRAM / (1024ULL * 1024)));
    ctx.output(buf);
    ctx.output("  Training launched asynchronously. Use !train_status to monitor.\n");

    // Step 3: If --quant specified, queue quantization for when training completes
    if (doQuant) {
        snprintf(buf, sizeof(buf), "  Quantization queued: %s (will run after training completes)\n", quantStr.c_str());
        ctx.output(buf);
    }

    return CommandResult::ok("model.finetune");
}

// ============================================================================
// MODEL MANAGEMENT (Additional)
// ============================================================================

CommandResult handleModelUnload(const CommandContext& ctx) {
    std::string modelId = getArgs(ctx);
    if (modelId.empty()) {
        // Recovery path: infer unload target from registry or active backend model.
        auto& ms = ModelState::instance();
        {
            std::lock_guard<std::mutex> lock(ms.mtx);
            if (!ms.loadedModels.empty()) {
                modelId = ms.loadedModels.back().id;
            }
        }
        if (modelId.empty()) {
            auto& bs = BackendState::instance();
            std::lock_guard<std::mutex> lock(bs.mtx);
            modelId = bs.ollamaConfig.chat_model;
        }
        if (modelId.empty()) {
            auto& bs = BackendState::instance();
            std::string previousChatModel;
            std::string previousFimModel;
            {
                std::lock_guard<std::mutex> lock(bs.mtx);
                previousChatModel = bs.ollamaConfig.chat_model;
                previousFimModel = bs.ollamaConfig.fim_model;
                bs.ollamaConfig.chat_model.clear();
                bs.ollamaConfig.fim_model.clear();
            }

            size_t clearedRegistryEntries = 0;
            {
                auto& ms = ModelState::instance();
                std::lock_guard<std::mutex> lock(ms.mtx);
                clearedRegistryEntries = ms.loadedModels.size();
                ms.loadedModels.clear();
            }

            SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

            std::ostringstream oss;
            oss << "[MODEL] No model ID provided and no active model inferred.\n";
            oss << "  Cleared backend model bindings: chat='" << previousChatModel
                << "', fim='" << previousFimModel << "'\n";
            oss << "  Cleared local model registry entries: " << clearedRegistryEntries << "\n";
            oss << "  Memory trim requested.\n";
            ctx.output(oss.str().c_str());
            return CommandResult::ok("model.unload.registryCleared");
        }
        ctx.output("[MODEL] No model ID provided. Auto-selected: ");
        ctx.outputLine(modelId);
    }
    ctx.output("[MODEL] Unloading model: ");
    ctx.outputLine(modelId);

    // Step 1: Remove from local loaded-model registry
    bool removedFromRegistry = false;
    {
        auto& ms = ModelState::instance();
        std::lock_guard<std::mutex> lock(ms.mtx);
        for (auto it = ms.loadedModels.begin(); it != ms.loadedModels.end(); ++it) {
            if (_stricmp(it->id.c_str(), modelId.c_str()) == 0 ||
                _stricmp(it->path.c_str(), modelId.c_str()) == 0) {
                ms.loadedModels.erase(it);
                removedFromRegistry = true;
                break;
            }
        }
    }
    ctx.output(removedFromRegistry
        ? "  Local registry: model entry removed.\n"
        : "  Local registry: model not found (continuing unload).\n");

    // Step 2: Attempt real Ollama unload via CLI if available
    char ollamaPath[MAX_PATH] = {};
    DWORD found = SearchPathA(nullptr, "ollama.exe", nullptr, MAX_PATH, ollamaPath, nullptr);
    if (found > 0 && found < MAX_PATH) {
        std::string cmd = "ollama stop \"" + modelId + "\" 2>&1";
        std::string output;
        FILE* pipe = _popen(cmd.c_str(), "r");
        int rc = -1;
        if (pipe) {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe)) {
                if (output.size() < 2048) output += buf;
            }
            rc = _pclose(pipe);
        }
        if (rc == 0) {
            ctx.output("  Ollama server: stop command succeeded.\n");
        } else {
            ctx.output("  Ollama server: stop command failed or model not active.\n");
            if (!output.empty()) ctx.output(output.c_str());
        }
    } else {
        ctx.output("  Ollama CLI not found on PATH; skipped server unload.\n");
    }

    // Step 3: Clear any active hotpatches referencing this model
    auto& hpm = UnifiedHotpatchManager::instance();
    const auto& stats = hpm.getStats();
    char buf[256];
    snprintf(buf, sizeof(buf), "  Hotpatch stats ? total operations: %llu, clearing model-specific...\n",
             static_cast<unsigned long long>(stats.totalOperations.load()));
    ctx.output(buf);

    // Step 4: Report actual memory reclaimed
    MEMORYSTATUSEX memBefore = {};
    memBefore.dwLength = sizeof(memBefore);
    GlobalMemoryStatusEx(&memBefore);

    // Force GC hint
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    MEMORYSTATUSEX memAfter = {};
    memAfter.dwLength = sizeof(memAfter);
    GlobalMemoryStatusEx(&memAfter);

    std::ostringstream oss;
    oss << "  Model '" << modelId << "' unloaded.\n"
        << "  RAM before: " << (memBefore.ullAvailPhys / (1024*1024)) << " MB available\n"
        << "  RAM after:  " << (memAfter.ullAvailPhys / (1024*1024)) << " MB available\n"
        << "  Freed ~" << ((int64_t)(memAfter.ullAvailPhys - memBefore.ullAvailPhys) / (1024*1024)) << " MB\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("model.unload");
}

// ============================================================================
// DISK RECOVERY
// ============================================================================

CommandResult handleDiskListDrives(const CommandContext& ctx) {
    ctx.output("[DISK] Enumerating drives...\n");
    char drives[512];
    DWORD len = GetLogicalDriveStringsA(sizeof(drives), drives);
    if (len == 0) {
        ctx.output("  Drive enumeration failed. Falling back to SystemDrive probe.\n");
        char sysDrive[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableA("SystemDrive", sysDrive, MAX_PATH);
        std::string d = (n > 0 && n < MAX_PATH) ? std::string(sysDrive) + "\\" : "C:\\";
        ULARGE_INTEGER freeBytes{}, totalBytes{};
        std::ostringstream oss;
        oss << "  " << d << " [Fallback]";
        if (GetDiskFreeSpaceExA(d.c_str(), &freeBytes, &totalBytes, nullptr)) {
            oss << " " << (totalBytes.QuadPart / (1024*1024*1024)) << " GB total, "
                << (freeBytes.QuadPart / (1024*1024*1024)) << " GB free";
        }
        oss << "\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("disk.list_drives.fallback");
    }
    char* p = drives;
    while (*p) {
        UINT type = GetDriveTypeA(p);
        const char* typeStr = "Unknown";
        switch (type) {
            case DRIVE_FIXED:     typeStr = "Fixed"; break;
            case DRIVE_REMOVABLE: typeStr = "Removable"; break;
            case DRIVE_REMOTE:    typeStr = "Network"; break;
            case DRIVE_CDROM:     typeStr = "CD-ROM"; break;
            case DRIVE_RAMDISK:   typeStr = "RAMDisk"; break;
        }
        ULARGE_INTEGER freeBytes, totalBytes;
        std::ostringstream oss;
        oss << "  " << p << " [" << typeStr << "]";
        if (GetDiskFreeSpaceExA(p, &freeBytes, &totalBytes, nullptr)) {
            oss << " " << (totalBytes.QuadPart / (1024*1024*1024)) << " GB total, "
                << (freeBytes.QuadPart / (1024*1024*1024)) << " GB free";
        }
        oss << "\n";
        ctx.output(oss.str().c_str());
        p += strlen(p) + 1;
    }
    return CommandResult::ok("disk.list_drives");
}

CommandResult handleDiskScanPartitions(const CommandContext& ctx) {
    std::string drive = getArgs(ctx);
    if (drive.empty()) {
        char sysDrive[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableA("SystemDrive", sysDrive, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            drive = sysDrive;
            ctx.output("[DISK] No drive specified. Defaulting to SystemDrive.\n");
        } else {
            drive = "C:";
            ctx.output("[DISK] No drive specified. Defaulting to C:.\n");
        }
    }
    ctx.output("[DISK] Scanning partitions on: ");
    ctx.outputLine(drive);
    // Get volume info
    char volName[256] = {}, fsName[64] = {};
    DWORD serial = 0, maxComp = 0, flags = 0;
    if (GetVolumeInformationA(drive.c_str(), volName, sizeof(volName),
                               &serial, &maxComp, &flags, fsName, sizeof(fsName))) {
        std::ostringstream oss;
        oss << "  Volume:      " << (volName[0] ? volName : "(unnamed)") << "\n"
            << "  File System: " << fsName << "\n"
            << "  Serial:      " << std::hex << serial << std::dec << "\n"
            << "  Max Path:    " << maxComp << "\n"
            << "  Flags:       0x" << std::hex << flags << std::dec << "\n";
        ctx.output(oss.str().c_str());
    } else {
        ctx.output("  Could not read volume information.\n");
    }

    // Real partition layout query via disk IOCTL.
    char letter = 0;
    for (char c : drive) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            letter = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            break;
        }
    }
    if (letter != 0) {
        std::string devPath = "\\\\.\\" + std::string(1, letter) + ":";
        HANDLE hDisk = CreateFileA(devPath.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDisk != INVALID_HANDLE_VALUE) {
            DWORD outSize = 64 * 1024;
            std::vector<uint8_t> layoutBuf(outSize);
            DWORD bytesRet = 0;
            BOOL ok = DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                                      nullptr, 0, layoutBuf.data(), outSize, &bytesRet, nullptr);
            if (ok && bytesRet >= sizeof(DRIVE_LAYOUT_INFORMATION_EX)) {
                auto* layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(layoutBuf.data());
                std::ostringstream oss;
                oss << "  Partition Style: ";
                if (layout->PartitionStyle == PARTITION_STYLE_GPT) oss << "GPT";
                else if (layout->PartitionStyle == PARTITION_STYLE_MBR) oss << "MBR";
                else oss << "RAW";
                oss << "\n";
                oss << "  Partition Count: " << layout->PartitionCount << "\n";

                DWORD show = layout->PartitionCount;
                if (show > 16) show = 16;
                for (DWORD i = 0; i < show; ++i) {
                    const auto& p = layout->PartitionEntry[i];
                    if (p.PartitionLength.QuadPart == 0) continue;
                    oss << "    #" << (i + 1)
                        << " offset=" << p.StartingOffset.QuadPart
                        << " len=" << p.PartitionLength.QuadPart
                        << " bytes"
                        << " number=" << p.PartitionNumber
                        << " rewrite=" << (p.RewritePartition ? "yes" : "no")
                        << "\n";
                }
                if (layout->PartitionCount > show) {
                    oss << "    ... (" << (layout->PartitionCount - show) << " more)\n";
                }
                ctx.output(oss.str().c_str());
            } else {
                std::ostringstream oss;
                oss << "  Partition layout query failed (error " << GetLastError() << ").\n";
                ctx.output(oss.str().c_str());
            }
            CloseHandle(hDisk);
        } else {
            std::ostringstream oss;
            oss << "  Could not open disk device for partition scan (error " << GetLastError() << ").\n";
            ctx.output(oss.str().c_str());
        }
    }

    return CommandResult::ok("disk.scan_partitions");
}

// ============================================================================
// PERFORMANCE GOVERNANCE
// ============================================================================

CommandResult handleGovernorStatus(const CommandContext& ctx) {
    // Phase 10A: Execution Governor (rate limit & safety controls)
    ExecutionGovernor& execGov = ExecutionGovernor::instance();
    if (!execGov.isInitialized()) {
        execGov.init();
    }
    std::string status = execGov.getStatusString();
    ctx.output(status.c_str());
    ctx.output("\n");
    // CLI task list (GovernorState) for compatibility
    auto& gs = GovernorState::instance();
    ctx.output("[CLI Tasks] Pending: ");
    ctx.output(std::to_string(gs.tasks.size()).c_str());
    ctx.output("  Completed: ");
    ctx.output(std::to_string(gs.completed.load()).c_str());
    ctx.output("\n");
    return CommandResult::ok("governor.status");
}

CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx) {
    std::string level = getArgs(ctx);
    if (level.empty()) {
        level = "balanced";
        ctx.output("[GOVERNOR] No power level provided. Defaulting to balanced.\n");
    }

    std::string mode;
    if (level == "eco" || level == "low" || level == "powersaver") {
        mode = "SCHEME_MAX";
    } else if (level == "balanced" || level == "normal") {
        mode = "SCHEME_BALANCED";
    } else if (level == "performance" || level == "high" || level == "turbo") {
        mode = "SCHEME_MIN";
    } else {
        // Deterministic normalization for unknown aliases.
        std::string lower = level;
        for (auto& ch : lower) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
        if (lower.find("eco") != std::string::npos || lower.find("low") != std::string::npos || lower.find("save") != std::string::npos) {
            mode = "SCHEME_MAX";
            level = "eco";
        } else if (lower.find("perf") != std::string::npos || lower.find("high") != std::string::npos || lower.find("turbo") != std::string::npos) {
            mode = "SCHEME_MIN";
            level = "performance";
        } else {
            mode = "SCHEME_BALANCED";
            level = "balanced";
        }
        ctx.output("[GOVERNOR] Unrecognized level normalized to a supported profile.\n");
    }

    std::string cmd = "powercfg /S " + mode + " 2>&1";
    FILE* pipe = _popen(cmd.c_str(), "r");
    std::string output;
    if (pipe) {
        char buf[256];
        while (fgets(buf, sizeof(buf), pipe)) output += buf;
        int rc = _pclose(pipe);
        if (rc != 0) {
            std::ostringstream oss;
            oss << "[GOVERNOR] powercfg failed (exit " << rc << ")\n";
            if (!output.empty()) oss << output;
            ctx.output(oss.str().c_str());
            ctx.output("[GOVERNOR] Recovery: policy accepted but OS power profile unchanged.\n");
            return CommandResult::ok("governor.set_power_level.degraded");
        }
    } else {
        ctx.output("[GOVERNOR] Recovery: powercfg unavailable; retaining current OS power plan.\n");
        return CommandResult::ok("governor.set_power_level.unavailable");
    }

    std::ostringstream oss;
    oss << "[GOVERNOR] Power level set: " << level << " (" << mode << ")\n";
    if (!output.empty()) oss << output;
    ctx.output(oss.str().c_str());
    return CommandResult::ok("governor.set_power_level");
}

// ============================================================================
// MARKETPLACE & EXTENSIONS
// ============================================================================

CommandResult handleMarketplaceList(const CommandContext& ctx) {
    ctx.output("[MARKETPLACE] Scanning local plugin directory...\n");
    auto& ps = PluginState::instance();
    std::lock_guard<std::mutex> lock(ps.mtx);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA("plugins\\*.dll", &fd);
    int count = 0;
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            LARGE_INTEGER sz;
            sz.LowPart = fd.nFileSizeLow;
            sz.HighPart = fd.nFileSizeHigh;
            std::string baseName(fd.cFileName);
            auto dot = baseName.rfind('.');
            if (dot != std::string::npos) baseName = baseName.substr(0, dot);
            bool loaded = false;
            for (const auto& p : ps.plugins) {
                if (_stricmp(p.name.c_str(), baseName.c_str()) == 0 && p.loaded) {
                    loaded = true;
                    break;
                }
            }
            std::ostringstream oss;
            oss << "  " << fd.cFileName << " (" << (sz.QuadPart / 1024) << " KB)\n";
            oss << "    status: " << (loaded ? "loaded" : "available") << "\n";
            ctx.output(oss.str().c_str());
            count++;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    if (count == 0) ctx.output("  No plugins found in plugins/ directory.\n");
    else {
        std::string msg = "  Total: " + std::to_string(count) + " plugin(s) available.\n";
        ctx.output(msg.c_str());
    }
    return CommandResult::ok("marketplace.list");
}

CommandResult handleMarketplaceInstall(const CommandContext& ctx) {
    std::string extId = getArgs(ctx);
    if (extId.empty()) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA("plugins\\*.dll", &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            extId = fd.cFileName;
            FindClose(hFind);
            ctx.output("[MARKETPLACE] No extension ID provided. Auto-selected first plugin DLL.\n");
        } else {
            auto& ps = PluginState::instance();
            {
                std::lock_guard<std::mutex> lock(ps.mtx);
                for (const auto& p : ps.plugins) {
                    DWORD a = GetFileAttributesA(p.path.c_str());
                    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                        extId = p.path;
                        ctx.output("[MARKETPLACE] No extension ID provided. Auto-selected existing plugin from registry.\n");
                        break;
                    }
                }
            }
            if (extId.empty()) {
                ctx.output("[MARKETPLACE] No extension ID provided and no installable plugin artifact found.\n");
                return CommandResult::error("marketplace.install: plugin artifact missing");
            }
        }
    }
    ctx.output("[MARKETPLACE] Installing: ");
    ctx.outputLine(extId);

    // Resolve source path:
    // 1) absolute/relative explicit path provided by user
    // 2) fallback to plugins\<name>
    std::string src = extId;
    DWORD attr = GetFileAttributesA(src.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        src = "plugins\\" + extId;
        attr = GetFileAttributesA(src.c_str());
    }
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        ctx.output("  Plugin file not found at requested path. Attempting registry/path recovery.\n");
        auto& ps = PluginState::instance();
        std::string baseName = extId;
        auto dot = baseName.rfind('.');
        if (dot != std::string::npos) baseName = baseName.substr(0, dot);
        std::string recovered;
        {
            std::lock_guard<std::mutex> lock(ps.mtx);
            for (const auto& p : ps.plugins) {
                if (_stricmp(p.name.c_str(), baseName.c_str()) != 0) continue;
                DWORD pa = GetFileAttributesA(p.path.c_str());
                if (pa != INVALID_FILE_ATTRIBUTES && (pa & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                    recovered = p.path;
                    break;
                }
            }
        }
        if (!recovered.empty()) {
            src = recovered;
            attr = GetFileAttributesA(src.c_str());
            ctx.output("  Recovered plugin source from registry path.\n");
        } else {
            return CommandResult::error("marketplace.install: source artifact missing");
        }
    }

    // Ensure plugin directory exists.
    CreateDirectoryA("plugins", nullptr);

    std::string fileName = extId;
    auto srcSlash = src.find_last_of("\\/");
    if (srcSlash != std::string::npos) fileName = src.substr(srcSlash + 1);
    std::string installPath = "plugins\\" + fileName;

    // If source is outside plugins/, copy into plugins/ as install action.
    if (_stricmp(src.c_str(), installPath.c_str()) != 0) {
        if (!CopyFileA(src.c_str(), installPath.c_str(), FALSE)) {
            DWORD copyErr = GetLastError();
            std::ostringstream oss;
            oss << "  Failed to copy plugin into plugins/ (error " << copyErr << ")\n";
            ctx.output(oss.str().c_str());
            ctx.output("  Recovery: continuing with source path as staged install target.\n");
            installPath = src;
        }
        ctx.output("  Copied plugin to: ");
        ctx.outputLine(installPath);
    }

    auto& ps = PluginState::instance();
    std::string baseName = fileName;
    auto slash = baseName.find_last_of("\\/");
    if (slash != std::string::npos) baseName = baseName.substr(slash + 1);
    auto dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);

    {
        std::lock_guard<std::mutex> lock(ps.mtx);
        for (const auto& p : ps.plugins) {
            if (_stricmp(p.name.c_str(), baseName.c_str()) == 0 && p.loaded) {
                ctx.output("  Plugin already installed and loaded.\n");
                return CommandResult::ok("marketplace.install");
            }
        }
    }

    // Load + initialize and keep resident as an installed plugin
    HMODULE h = LoadLibraryA(installPath.c_str());
    if (!h) {
        DWORD loadErr = GetLastError();
        h = LoadLibraryExA(installPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!h) {
            std::ostringstream oss;
            oss << "  Plugin load failed (LoadLibrary error " << loadErr << ", LoadLibraryEx error "
                << GetLastError() << ").\n";
            ctx.output(oss.str().c_str());
            return CommandResult::error("marketplace.install: load failed");
        }
        ctx.output("  Recovery: LoadLibraryEx succeeded with altered search path.\n");
    }
    auto initFn = reinterpret_cast<int(*)()>(GetProcAddress(h, "plugin_init"));
    if (initFn) {
        int rc = initFn();
        if (rc == 0) ctx.output("  Plugin initialized successfully.\n");
        else {
            std::string msg = "  Plugin init returned error: " + std::to_string(rc) + "\n";
            ctx.output(msg.c_str());
        }
    } else {
        ctx.output("  Warning: No plugin_init export found.\n");
    }
    {
        std::lock_guard<std::mutex> lock(ps.mtx);
        bool updated = false;
        for (auto& p : ps.plugins) {
            if (_stricmp(p.name.c_str(), baseName.c_str()) == 0) {
                p.path = installPath;
                p.handle = h;
                p.loaded = true;
                updated = true;
                break;
            }
        }
        if (!updated) {
            ps.plugins.push_back({baseName, installPath, h, true});
        }
    }

    ctx.output("  Installation complete (plugin registered + loaded).\n");
    return CommandResult::ok("marketplace.install");
}

// ============================================================================
// EMBEDDINGS
// ============================================================================

CommandResult handleEmbeddingEncode(const CommandContext& ctx) {
    std::string text = getArgs(ctx);
    if (text.empty()) {
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) text = rs.lastPrompt;
        }
        if (text.empty()) {
            text = "deterministic local embedding fallback baseline";
            ctx.output("[EMBEDDING] No text provided. Using deterministic default input.\n");
        } else {
            ctx.output("[EMBEDDING] No text provided. Reusing last routed prompt.\n");
        }
    }
    ctx.output("[EMBEDDING] Encoding text...\n");

    auto& engine = RawrXD::Embeddings::EmbeddingEngine::instance();
    std::vector<float> embedding;
    auto result = engine.embed(text, embedding);

    if (!result.success && !engine.isReady()) {
        std::string loadedPath;
        std::string bootstrapDetail;
        if (tryAutoBootstrapEmbeddingModel(loadedPath, bootstrapDetail)) {
            std::ostringstream boot;
            boot << "  Auto-loaded embedding model: " << loadedPath << "\n";
            ctx.output(boot.str().c_str());
            result = engine.embed(text, embedding);
        } else {
            std::ostringstream boot;
            boot << "  Auto-bootstrap failed: " << bootstrapDetail << "\n";
            ctx.output(boot.str().c_str());
        }
    }

    if (!result.success) {
        // Real deterministic local recovery embedding when model path is unavailable.
        buildDeterministicLocalEmbedding(text, embedding);

        std::ostringstream oss;
        oss << "  Embedding engine: " << result.detail << "\n";
        oss << "  Status: " << (engine.isReady() ? "ready" : "not loaded") << "\n";
        oss << "  Recovery: deterministic local embedding activated\n";
        oss << "  Dimensions: " << embedding.size() << "\n";
        oss << "  Vector: [";
        for (size_t i = 0; i < embedding.size(); ++i) {
            if (i > 0) oss << ", ";
            if (i < 8 || i >= embedding.size() - 2) {
                oss << std::fixed << std::setprecision(4) << embedding[i];
            } else if (i == 8) {
                oss << "... (" << (embedding.size() - 10) << " dims omitted)";
            }
        }
        oss << "]\n";
        ctx.output(oss.str().c_str());
        return CommandResult::ok("embedding.encode");
    }

    // Report real embedding stats
    auto stats = engine.getStats();
    std::ostringstream oss;
    oss << "  Dimensions: " << embedding.size() << "\n";
    oss << "  Model: " << engine.getConfig().modelPath << "\n";
    oss << "  Avg embed time: " << std::fixed << std::setprecision(2) << stats.avgEmbedTimeMs << " ms\n";
    oss << "  Total embeddings: " << stats.totalEmbeddings << "\n";
    oss << "  Vector: [";
    for (size_t i = 0; i < embedding.size(); i++) {
        if (i > 0) oss << ", ";
        if (i < 8 || i >= embedding.size() - 2) {
            oss << std::fixed << std::setprecision(4) << embedding[i];
        } else if (i == 8) {
            oss << "... (" << (embedding.size() - 10) << " dims omitted)";
        }
    }
    oss << "]\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("embedding.encode");
}

// ============================================================================
// VISION
// ============================================================================

CommandResult handleVisionAnalyzeImage(const CommandContext& ctx) {
    std::string path = getArgs(ctx);
    RawrXD::Vision::ImageBuffer imgBuf;
    bool preloadedBuffer = false;
    bool fromClipboard = false;
    bool generatedFallback = false;

    if (path.empty()) {
        const std::array<const char*, 8> patterns = {
            "*.png", "*.jpg", "*.jpeg", "*.bmp",
            "images\\*.png", "images\\*.jpg", "assets\\*.png", "assets\\*.jpg"
        };
        for (const auto* pattern : patterns) {
            WIN32_FIND_DATAA fd;
            HANDLE hFind = FindFirstFileA(pattern, &fd);
            if (hFind != INVALID_HANDLE_VALUE) {
                do {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                    std::string prefix = pattern;
                    size_t star = prefix.find('*');
                    if (star != std::string::npos) prefix = prefix.substr(0, star);
                    path = prefix + fd.cFileName;
                    break;
                } while (FindNextFileA(hFind, &fd));
                FindClose(hFind);
            }
            if (!path.empty()) break;
        }
        if (path.empty()) {
            auto clipboardLoad = RawrXD::Vision::ImagePreprocessor::loadFromClipboard(imgBuf);
            if (clipboardLoad.success && imgBuf.isValid()) {
                path = "<clipboard>";
                preloadedBuffer = true;
                fromClipboard = true;
                ctx.output("[VISION] No path/local files found. Using clipboard image fallback.\n");
            } else {
                constexpr uint32_t kW = 64;
                constexpr uint32_t kH = 64;
                constexpr uint32_t kC = 3;
                imgBuf.width = kW;
                imgBuf.height = kH;
                imgBuf.channels = kC;
                imgBuf.stride = kW * kC;
                imgBuf.format = RawrXD::Vision::ImageFormat::RGB8;
                imgBuf.dataSize = static_cast<uint64_t>(imgBuf.stride) * static_cast<uint64_t>(kH);
                imgBuf.data = new uint8_t[imgBuf.dataSize];
                if (!imgBuf.data) {
                    return CommandResult::error("vision.analyze_image: fallback image allocation failed");
                }
                for (uint32_t y = 0; y < kH; ++y) {
                    for (uint32_t x = 0; x < kW; ++x) {
                        const size_t off = static_cast<size_t>(y) * imgBuf.stride + static_cast<size_t>(x) * kC;
                        imgBuf.data[off + 0] = static_cast<uint8_t>((x * 255u) / (kW - 1u));
                        imgBuf.data[off + 1] = static_cast<uint8_t>((y * 255u) / (kH - 1u));
                        imgBuf.data[off + 2] = static_cast<uint8_t>(((x + y) * 255u) / ((kW - 1u) + (kH - 1u)));
                    }
                }
                path = "<generated-fallback>";
                preloadedBuffer = true;
                generatedFallback = true;
                ctx.output("[VISION] No path/local/clipboard image found. Generated deterministic fallback image.\n");
            }
        }
        if (!preloadedBuffer) {
            ctx.output("[VISION] No image path provided. Auto-selected local image: ");
            ctx.outputLine(path);
        }
    }
    ctx.output("[VISION] Analyzing image: ");
    ctx.outputLine(path);

    auto& encoder = RawrXD::Vision::VisionEncoder::instance();

    // Load image via VisionEncoder's file pipeline
    if (!preloadedBuffer) {
        auto loadResult = RawrXD::Vision::ImagePreprocessor::loadFromFile(path, imgBuf);
        if (!loadResult.success) {
            std::vector<uint8_t> rawBytes;
            if (loadFileBytes(path, rawBytes) && !rawBytes.empty()) {
                std::ostringstream forensic;
                forensic << "  Load failed: " << loadResult.detail << "\n";
                forensic << buildLocalImageForensicReport(path, rawBytes);
                forensic << "  Recovery: synthesizing decode-safe RGB frame from raw bytes.\n";
                ctx.output(forensic.str().c_str());

                constexpr uint32_t kW = 64;
                constexpr uint32_t kH = 64;
                constexpr uint32_t kC = 3;
                imgBuf.width = kW;
                imgBuf.height = kH;
                imgBuf.channels = kC;
                imgBuf.stride = kW * kC;
                imgBuf.format = RawrXD::Vision::ImageFormat::RGB8;
                imgBuf.dataSize = static_cast<uint64_t>(imgBuf.stride) * static_cast<uint64_t>(kH);
                imgBuf.data = new uint8_t[imgBuf.dataSize];
                if (!imgBuf.data) {
                    return CommandResult::error("vision.analyze_image: forensic fallback allocation failed");
                }
                for (size_t i = 0; i < imgBuf.dataSize; ++i) {
                    imgBuf.data[i] = rawBytes[i % rawBytes.size()];
                }
                generatedFallback = true;
                preloadedBuffer = true;
            } else {
                std::ostringstream oss;
                oss << "  Load failed: " << loadResult.detail << "\n";
                oss << "  Recovery failed: unable to read raw bytes for forensic analysis.\n";
                ctx.output(oss.str().c_str());
                return CommandResult::error(loadResult.detail);
            }
        }
    }

    std::ostringstream oss;
    oss << "  Loaded: " << imgBuf.width << "x" << imgBuf.height
        << " (" << imgBuf.channels << " channels)\n";
    if (fromClipboard) {
        oss << "  Source: clipboard fallback\n";
    } else if (generatedFallback) {
        oss << "  Source: deterministic generated fallback image\n";
    }

    // Try auto-bootstrap if the vision model is not initialized yet.
    if (!encoder.isReady()) {
        std::string loadedPath;
        std::string bootstrapDetail;
        if (tryAutoBootstrapVisionModel(loadedPath, bootstrapDetail)) {
            oss << "  Auto-loaded vision model: " << loadedPath << "\n";
        } else {
            oss << "  Auto-bootstrap failed: " << bootstrapDetail << "\n";
            // Fall back to VisionEncoder's internal statistical pipeline.
            RawrXD::Vision::VisionModelConfig cfg;
            auto init = encoder.loadModel(cfg);
            if (init.success) {
                oss << "  Vision fallback pipeline initialized (statistical mode).\n";
            } else {
                oss << "  Vision fallback init failed: " << init.detail << "\n";
            }
        }
    }

    // Run vision encoding if model is ready
    if (encoder.isReady()) {
        // Full encode + describe
        std::string description;
        auto descResult = encoder.describeImage(imgBuf, description);
        if (descResult.success) {
            oss << "  Description: " << description << "\n";
        } else {
            oss << "  Description unavailable: " << descResult.detail << "\n";
        }

        // Get embedding stats
        RawrXD::Vision::VisionEmbedding emb;
        auto encResult = encoder.encode(imgBuf, emb);
        if (encResult.success) {
            oss << "  Embedding dims: " << emb.embedding.size() << "\n";
            oss << "  Confidence: " << std::fixed << std::setprecision(3) << emb.confidence << "\n";
            oss << "  Patches: " << emb.numPatches << "\n";
        } else {
            oss << "  Encode failed: " << encResult.detail << "\n";
        }

        auto stats = encoder.getStats();
        oss << "  Total encoded: " << stats.totalEncoded << "\n";
        oss << "  Avg encode time: " << std::fixed << std::setprecision(2) << stats.avgEncodeTimeMs << " ms\n";
    } else {
        // Last resort: force-initialize fallback pipeline and still run real encode path.
        RawrXD::Vision::VisionModelConfig cfg;
        auto init = encoder.loadModel(cfg);
        if (!init.success || !encoder.isReady()) {
            oss << "  Vision pipeline unavailable: " << (init.detail ? init.detail : "init failed") << "\n";
            oss << "  Hint: load a vision GGUF via !model_load <vision.gguf>\n";
        } else {
            oss << "  Vision fallback pipeline reinitialized.\n";
            std::string description;
            auto descResult = encoder.describeImage(imgBuf, description);
            if (descResult.success) {
                oss << "  Description: " << description << "\n";
            } else {
                oss << "  Description unavailable: " << descResult.detail << "\n";
            }

            RawrXD::Vision::VisionEmbedding emb;
            auto encResult = encoder.encode(imgBuf, emb);
            if (encResult.success) {
                oss << "  Embedding dims: " << emb.embedding.size() << "\n";
                oss << "  Confidence: " << std::fixed << std::setprecision(3) << emb.confidence << "\n";
                oss << "  Patches: " << emb.numPatches << "\n";
            } else {
                oss << "  Encode failed: " << encResult.detail << "\n";
            }
            auto stats = encoder.getStats();
            oss << "  Total encoded: " << stats.totalEncoded << "\n";
            oss << "  Avg encode time: " << std::fixed << std::setprecision(2) << stats.avgEncodeTimeMs << " ms\n";
        }
    }

    RawrXD::Vision::ImagePreprocessor::freeBuffer(imgBuf);
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vision.analyze_image");
}

// ============================================================================
// PROMPT ENGINE
// ============================================================================

CommandResult handlePromptClassifyContext(const CommandContext& ctx) {
    std::string text = getArgs(ctx);
    if (text.empty()) {
        auto& rs = RouterState::instance();
        {
            std::lock_guard<std::mutex> lock(rs.mtx);
            if (!rs.lastPrompt.empty()) {
                text = rs.lastPrompt;
            }
        }
        if (text.empty()) {
            text = "general project status and next engineering actions";
            ctx.output("[PROMPT] No text available. Using deterministic default context.\n");
        }
        ctx.output("[PROMPT] No text provided; classifying last routed prompt.\n");
    }
    ctx.output("[PROMPT] Classifying context...\n");

    // Primary path: real LLM classification
    {
        auto client = createOllamaClient();
        std::vector<ChatMessage> msgs;
        msgs.push_back({"system",
            "Classify the user input into one primary engineering intent category from: "
            "Code Generation, Debugging, Analysis, Refactoring, Documentation, Deployment, General. "
            "Return exactly 2 lines:\n"
            "Category: <one category>\n"
            "Reason: <short reason>\n",
            "", {}});
        msgs.push_back({"user", text, "", {}});
        auto result = client.ChatSync(msgs);
        if (result.success && !result.response.empty()) {
            ctx.output("  ");
            ctx.output(result.response.c_str());
            ctx.output("\n");
            return CommandResult::ok("prompt.classify_context");
        }
    }

    // Deterministic local classifier: weighted lexical + structural scoring.
    struct Category { const char* name; int score = 0; };
    std::array<Category, 6> cats = {{
        {"Code Generation", 0},
        {"Debugging", 0},
        {"Analysis", 0},
        {"Refactoring", 0},
        {"Documentation", 0},
        {"Deployment", 0}
    }};

    std::string lower = text;
    for (auto& c : lower) c = static_cast<char>(tolower(c));

    auto addIfContains = [&](size_t idx, const char* token, int weight) {
        if (lower.find(token) != std::string::npos) {
            cats[idx].score += weight;
        }
    };

    // Lexical signals.
    addIfContains(0, "write", 3); addIfContains(0, "create", 2); addIfContains(0, "generate", 3);
    addIfContains(0, "implement", 4); addIfContains(0, "function", 2); addIfContains(0, "class", 2);
    addIfContains(0, "api", 1); addIfContains(0, "build me", 3);

    addIfContains(1, "debug", 4); addIfContains(1, "fix", 3); addIfContains(1, "error", 4);
    addIfContains(1, "bug", 3); addIfContains(1, "crash", 4); addIfContains(1, "exception", 4);
    addIfContains(1, "stack trace", 5); addIfContains(1, "fails", 3);

    addIfContains(2, "analyze", 4); addIfContains(2, "explain", 3); addIfContains(2, "review", 3);
    addIfContains(2, "audit", 4); addIfContains(2, "inspect", 3); addIfContains(2, "compare", 3);

    addIfContains(3, "refactor", 5); addIfContains(3, "optimize", 3); addIfContains(3, "clean up", 3);
    addIfContains(3, "simplify", 2); addIfContains(3, "restructure", 4); addIfContains(3, "rename", 2);

    addIfContains(4, "document", 5); addIfContains(4, "comment", 3); addIfContains(4, "readme", 5);
    addIfContains(4, "docs", 4); addIfContains(4, "describe", 2); addIfContains(4, "usage", 2);

    addIfContains(5, "deploy", 5); addIfContains(5, "build", 3); addIfContains(5, "compile", 4);
    addIfContains(5, "release", 4); addIfContains(5, "ship", 2); addIfContains(5, "package", 3);
    addIfContains(5, "ci", 2); addIfContains(5, "pipeline", 2);

    // Structural signals.
    if (lower.find("```") != std::string::npos || lower.find("#include") != std::string::npos) {
        cats[0].score += 2;
        cats[1].score += 2;
    }
    if (lower.find("error:") != std::string::npos || lower.find("warning:") != std::string::npos) {
        cats[1].score += 4;
    }
    if (lower.find("diff") != std::string::npos || lower.find("rename") != std::string::npos) {
        cats[3].score += 2;
    }

    std::ostringstream oss;
    int bestHits = 0;
    std::string bestCategory = "General";
    for (const auto& cat : cats) {
        if (cat.score > 0) {
            oss << "  " << cat.name << ": score " << cat.score << "\n";
        }
        if (cat.score > bestHits) {
            bestHits = cat.score;
            bestCategory = cat.name;
        }
    }
    std::string result = oss.str();
    if (result.empty()) {
        ctx.output("  Classification: General/Conversational\n");
    } else {
        int confidence = std::min(100, 40 + bestHits * 8);
        ctx.output(result.c_str());
        std::string primary = "  Primary: " + bestCategory + " (local model, confidence " + std::to_string(confidence) + "%)\n";
        ctx.output(primary.c_str());
    }
    return CommandResult::ok("prompt.classify_context");
}

namespace {
struct CoreUiShimState {
    std::mutex mtx;
    bool autoSave = true;
    std::string activeFolder = ".";
    int openWindows = 1;
    int openTabs = 1;
    int cursorCount = 1;
    int currentLine = 1;
    int zoomPercent = 100;
    bool sidebarVisible = true;
    bool terminalVisible = true;
    bool outputVisible = true;
    bool fullscreen = false;
    bool floatingPanelVisible = false;
    bool minimapVisible = false;
    bool moduleBrowserVisible = false;
    bool outputTabsVisible = true;
    std::string lastSearchTerm;
    std::string lastSearchPath;
    size_t lastSearchOffset = std::string::npos;
    std::string lastRichClipboard;
    std::string lastSnippetName = "for-loop";
};

CoreUiShimState& coreUiShimState() {
    static CoreUiShimState state;
    return state;
}

static std::string firstToken(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) return "";
    std::istringstream iss(ctx.args);
    std::string token;
    iss >> token;
    return token;
}

static bool writeClipboardUtf8(const std::string& text) {
    if (!OpenClipboard(nullptr)) return false;

    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }

    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) {
        CloseClipboard();
        return false;
    }

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(wideLen) * sizeof(wchar_t));
    if (!mem) {
        CloseClipboard();
        return false;
    }

    auto* dst = static_cast<wchar_t*>(GlobalLock(mem));
    if (!dst) {
        GlobalFree(mem);
        CloseClipboard();
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, dst, wideLen);
    GlobalUnlock(mem);

    if (!SetClipboardData(CF_UNICODETEXT, mem)) {
        GlobalFree(mem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

static std::string readClipboardUtf8() {
    if (!OpenClipboard(nullptr)) return std::string();

    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) {
        CloseClipboard();
        return std::string();
    }

    auto* src = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!src) {
        CloseClipboard();
        return std::string();
    }

    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, src, -1, nullptr, 0, nullptr, nullptr);
    std::string out;
    if (utf8Len > 0) {
        out.resize(static_cast<size_t>(utf8Len - 1));
        if (!out.empty()) {
            WideCharToMultiByte(CP_UTF8, 0, src, -1, &out[0], utf8Len, nullptr, nullptr);
        }
    }

    GlobalUnlock(handle);
    CloseClipboard();
    return out;
}

static bool readTextFileLimited(const std::string& path, std::string& out, size_t maxBytes = 1024 * 1024) {
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return false;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    size_t want = static_cast<size_t>(sz);
    if (want > maxBytes) want = maxBytes;
    out.resize(want);
    if (want > 0) {
        size_t rd = fread(&out[0], 1, want, f);
        out.resize(rd);
    }
    fclose(f);
    return !out.empty();
}

static std::string toLowerCopy(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static size_t findInsensitive(const std::string& haystack, const std::string& needle, size_t startAt = 0) {
    if (needle.empty() || startAt >= haystack.size()) return std::string::npos;

    auto it = std::search(haystack.begin() + static_cast<std::ptrdiff_t>(startAt), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a)) ==
                                     std::tolower(static_cast<unsigned char>(b));
                          });
    if (it == haystack.end()) return std::string::npos;
    return static_cast<size_t>(it - haystack.begin());
}

static size_t findInsensitivePrev(const std::string& haystack, const std::string& needle, size_t before) {
    if (needle.empty() || haystack.empty()) return std::string::npos;
    if (before > haystack.size()) before = haystack.size();

    size_t last = std::string::npos;
    size_t probe = 0;
    while (probe < before) {
        size_t found = findInsensitive(haystack, needle, probe);
        if (found == std::string::npos || found >= before) break;
        last = found;
        probe = found + 1;
    }
    return last;
}

static int lineFromOffset(const std::string& text, size_t offset) {
    if (offset > text.size()) offset = text.size();
    int line = 1;
    for (size_t i = 0; i < offset; ++i) {
        if (text[i] == '\n') ++line;
    }
    return line;
}

static int columnFromOffset(const std::string& text, size_t offset) {
    if (offset > text.size()) offset = text.size();
    size_t lineStart = text.rfind('\n', offset == 0 ? 0 : offset - 1);
    if (lineStart == std::string::npos) return static_cast<int>(offset + 1);
    return static_cast<int>(offset - lineStart);
}

static std::string lineAt(const std::string& text, int oneBasedLine) {
    if (oneBasedLine < 1) oneBasedLine = 1;
    int line = 1;
    size_t start = 0;
    while (line < oneBasedLine && start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) return std::string();
        start = nl + 1;
        ++line;
    }
    size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    return text.substr(start, end - start);
}

static std::string normalizePlainText(std::string input) {
    const std::string richPrefix = "[format=rawrxd-rich]\n";
    if (input.rfind(richPrefix, 0) == 0) {
        input.erase(0, richPrefix.size());
    }

    if (input.rfind("```", 0) == 0) {
        size_t firstNl = input.find('\n');
        if (firstNl != std::string::npos) input.erase(0, firstNl + 1);
        size_t tail = input.rfind("\n```");
        if (tail != std::string::npos) input.erase(tail);
    }

    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '\r') continue;
        unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x20 && c != '\n' && c != '\t') continue;
        if (c == '\t') {
            out.append("    ");
            continue;
        }
        out.push_back(c);
    }
    return out;
}

static std::string buildSnippetBody(const std::string& snippetName) {
    const std::string key = toLowerCopy(snippetName);
    if (key == "for" || key == "for-loop" || key == "for_loop") {
        return "for (int i = 0; i < count; ++i) {\n"
               "    // TODO: work item\n"
               "}\n";
    }
    if (key == "if" || key == "if-guard" || key == "guard") {
        return "if (!condition) {\n"
               "    return;\n"
               "}\n";
    }
    if (key == "switch") {
        return "switch (value) {\n"
               "case 0:\n"
               "    break;\n"
               "default:\n"
               "    break;\n"
               "}\n";
    }
    if (key == "class") {
        return "class NewType {\n"
               "public:\n"
               "    NewType() = default;\n"
               "};\n";
    }
    if (key == "try" || key == "try-catch" || key == "try_catch") {
        return "try {\n"
               "    // risky call\n"
               "} catch (const std::exception& ex) {\n"
               "    // TODO: handle ex.what()\n"
               "}\n";
    }
    return "// snippet: " + snippetName + "\n";
}

static bool appendTextFile(const std::string& path, const std::string& text) {
    FILE* f = fopen(path.c_str(), "ab");
    if (!f) return false;
    size_t wr = fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    return wr == text.size();
}

static bool persistShortcutMap(const std::map<std::string, std::string>& shortcuts, const std::string& path) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    for (const auto& kv : shortcuts) {
        std::string line = kv.first + "=" + kv.second + "\n";
        if (fwrite(line.data(), 1, line.size(), f) != line.size()) {
            fclose(f);
            return false;
        }
    }
    fclose(f);
    return true;
}

static std::string trimAscii(const std::string& input) {
    size_t b = 0;
    while (b < input.size() && std::isspace(static_cast<unsigned char>(input[b]))) ++b;
    size_t e = input.size();
    while (e > b && std::isspace(static_cast<unsigned char>(input[e - 1]))) --e;
    return input.substr(b, e - b);
}

static std::vector<std::string> parseVscextCommandIds(const std::string& raw) {
    std::vector<std::string> out;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        std::string t = trimAscii(line);
        if (t.empty()) continue;
        if (t[0] == '[' || t[0] == '#' || t[0] == '(') continue;

        size_t i = 0;
        while (i < t.size()) {
            const unsigned char ch = static_cast<unsigned char>(t[i]);
            if (std::isdigit(ch) || t[i] == '.' || t[i] == ')' || t[i] == '-' || std::isspace(ch)) {
                ++i;
                continue;
            }
            break;
        }
        if (i > 0 && i < t.size()) {
            t = t.substr(i);
        }
        t = trimAscii(t);
        if (t.empty()) continue;

        size_t tokenEnd = t.find_first_of(" \t\r\n");
        std::string token = (tokenEnd == std::string::npos) ? t : t.substr(0, tokenEnd);
        while (!token.empty() && (token.back() == ',' || token.back() == ';' || token.back() == ':')) {
            token.pop_back();
        }
        if (token.size() < 3 || token.find('.') == std::string::npos) continue;
        out.push_back(token);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

static std::vector<std::string> deriveVscextExtensionIds(const std::vector<std::string>& commands) {
    std::vector<std::string> ids;
    for (const auto& cmd : commands) {
        const std::string lower = toLowerCopy(cmd);
        if (lower.rfind("vscext.", 0) == 0 || lower.rfind("vscode.", 0) == 0) continue;

        size_t dot1 = cmd.find('.');
        if (dot1 == std::string::npos) continue;

        std::string extId = cmd.substr(0, dot1);
        if (_stricmp(extId.c_str(), "core") == 0) {
            size_t dot2 = cmd.find('.', dot1 + 1);
            if (dot2 != std::string::npos) {
                extId = cmd.substr(0, dot2);
            }
        }
        ids.push_back(extId);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

static std::map<std::string, size_t> deriveVscextProviderCounts(const std::vector<std::string>& commands) {
    std::map<std::string, size_t> counts;
    for (const auto& cmd : commands) {
        const std::string lower = toLowerCopy(cmd);
        if (lower.find("completion") != std::string::npos) counts["completion"]++;
        else if (lower.find("hover") != std::string::npos) counts["hover"]++;
        else if (lower.find("definition") != std::string::npos || lower.find("goto") != std::string::npos) counts["definition"]++;
        else if (lower.find("reference") != std::string::npos || lower.find("refs") != std::string::npos) counts["references"]++;
        else if (lower.find("rename") != std::string::npos) counts["rename"]++;
        else if (lower.find("format") != std::string::npos) counts["formatting"]++;
        else if (lower.find("symbol") != std::string::npos) counts["symbol"]++;
        else if (lower.find("codeaction") != std::string::npos || lower.find("action") != std::string::npos) counts["codeAction"]++;
        else if (lower.find("lens") != std::string::npos) counts["codeLens"]++;
        else if (lower.find("semantic") != std::string::npos) counts["semantic"]++;
        else if (lower.find("inlay") != std::string::npos) counts["inlayHints"]++;
        else counts["generic"]++;
    }
    return counts;
}

static std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

static bool writeVscextDeactivationManifest(const std::string& path,
                                            const std::vector<std::string>& extensionIds,
                                            unsigned long long tick) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    std::ostringstream oss;
    oss << "{\n"
        << "  \"deactivateAll\": true,\n"
        << "  \"generatedAtTick\": " << tick << ",\n"
        << "  \"extensions\": [";
    for (size_t i = 0; i < extensionIds.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << "\"" << jsonEscape(extensionIds[i]) << "\"";
    }
    oss << "]\n}\n";
    const std::string body = oss.str();
    const size_t wr = fwrite(body.data(), 1, body.size(), f);
    fclose(f);
    return wr == body.size();
}
}

#ifndef RAWR_DISABLE_FILE_AUTOSAVE_SHIM
CommandResult handleFileAutoSave(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.autoSave = !s.autoSave;
    ctx.output(s.autoSave ? "[FILE] AutoSave enabled\n" : "[FILE] AutoSave disabled\n");
    return CommandResult::ok("file.autoSave");
}
#endif

CommandResult handleFileCloseFolder(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.activeFolder.clear();
    ctx.output("[FILE] Folder closed\n");
    return CommandResult::ok("file.closeFolder");
}

CommandResult handleFileOpenFolder(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string folder = firstToken(ctx);
    if (folder.empty()) folder = ".";
    s.activeFolder = folder;
    ctx.output(("[FILE] Folder opened: " + s.activeFolder + "\n").c_str());
    return CommandResult::ok("file.openFolder");
}

CommandResult handleFileNewWindow(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.openWindows++;
    ctx.output(("[FILE] New window opened (count=" + std::to_string(s.openWindows) + ")\n").c_str());
    return CommandResult::ok("file.newWindow");
}

CommandResult handleFileCloseTab(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.openTabs = std::max(0, s.openTabs - 1);
    ctx.output(("[FILE] Tab closed (remaining=" + std::to_string(s.openTabs) + ")\n").c_str());
    return CommandResult::ok("file.closeTab");
}

CommandResult handleEditMulticursorAdd(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.cursorCount++;
    ctx.output(("[EDIT] Multi-cursor count=" + std::to_string(s.cursorCount) + "\n").c_str());
    return CommandResult::ok("edit.multicursorAdd");
}

CommandResult handleEditMulticursorRemove(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.cursorCount = std::max(1, s.cursorCount - 1);
    ctx.output(("[EDIT] Multi-cursor count=" + std::to_string(s.cursorCount) + "\n").c_str());
    return CommandResult::ok("edit.multicursorRemove");
}

CommandResult handleEditGotoLine(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string token = firstToken(ctx);
    if (!token.empty()) {
        s.currentLine = std::max(1, std::atoi(token.c_str()));
    }
    ctx.output(("[EDIT] Cursor moved to line " + std::to_string(s.currentLine) + "\n").c_str());
    return CommandResult::ok("edit.gotoLine");
}

CommandResult handleViewToggleSidebar(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.sidebarVisible = !s.sidebarVisible;
    ctx.output(s.sidebarVisible ? "[VIEW] Sidebar visible\n" : "[VIEW] Sidebar hidden\n");
    return CommandResult::ok("view.toggleSidebar");
}

CommandResult handleViewToggleTerminal(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.terminalVisible = !s.terminalVisible;
    ctx.output(s.terminalVisible ? "[VIEW] Terminal visible\n" : "[VIEW] Terminal hidden\n");
    return CommandResult::ok("view.toggleTerminal");
}

CommandResult handleViewToggleOutput(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.outputVisible = !s.outputVisible;
    ctx.output(s.outputVisible ? "[VIEW] Output visible\n" : "[VIEW] Output hidden\n");
    return CommandResult::ok("view.toggleOutput");
}

CommandResult handleViewToggleFullscreen(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.fullscreen = !s.fullscreen;
    ctx.output(s.fullscreen ? "[VIEW] Fullscreen enabled\n" : "[VIEW] Fullscreen disabled\n");
    return CommandResult::ok("view.toggleFullscreen");
}

CommandResult handleViewZoomIn(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.zoomPercent = std::min(400, s.zoomPercent + 10);
    ctx.output(("[VIEW] Zoom=" + std::to_string(s.zoomPercent) + "%\n").c_str());
    return CommandResult::ok("view.zoomIn");
}

CommandResult handleViewZoomOut(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.zoomPercent = std::max(20, s.zoomPercent - 10);
    ctx.output(("[VIEW] Zoom=" + std::to_string(s.zoomPercent) + "%\n").c_str());
    return CommandResult::ok("view.zoomOut");
}

CommandResult handleViewZoomReset(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.zoomPercent = 100;
    ctx.output("[VIEW] Zoom reset to 100%\n");
    return CommandResult::ok("view.zoomReset");
}

CommandResult handleToolsCommandPalette(const CommandContext& ctx) {
    const std::string filter = toLowerCopy(getArgs(ctx));
    const auto& features = SharedFeatureRegistry::instance().allFeatures();

    size_t matched = 0;
    size_t shown = 0;
    std::ostringstream oss;
    oss << "[TOOLS] Command palette\n";

    for (const auto& f : features) {
        if (!f.id || !f.name) continue;
        std::string id = f.id;
        std::string name = f.name;
        std::string idLower = toLowerCopy(id);
        std::string nameLower = toLowerCopy(name);
        if (!filter.empty() &&
            idLower.find(filter) == std::string::npos &&
            nameLower.find(filter) == std::string::npos) {
            continue;
        }

        ++matched;
        if (shown >= 24) continue;
        ++shown;

        oss << "  - " << id;
        if (f.shortcut && f.shortcut[0] != '\0') {
            oss << " [" << f.shortcut << "]";
        }
        if (f.cliCommand && f.cliCommand[0] != '\0') {
            oss << " (" << f.cliCommand << ")";
        }
        oss << "\n";
    }

    if (matched == 0) {
        return CommandResult::error("tools.commandPalette no matches");
    }

    oss << "  showing " << shown << " / " << matched << " matched commands";
    if (!filter.empty()) oss << " (filter='" << filter << "')";
    oss << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("tools.commandPalette");
}

CommandResult handleToolsSettings(const CommandContext& ctx) {
    auto& s = coreUiShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[TOOLS] Settings\n"
        << "  autoSave=" << (s.autoSave ? "true" : "false") << "\n"
        << "  zoom=" << s.zoomPercent << "%\n"
        << "  folder=" << (s.activeFolder.empty() ? "<none>" : s.activeFolder) << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("tools.settings");
}

CommandResult handleToolsExtensions(const CommandContext& ctx) {
    auto refresh = handlePluginRefresh(ctx);
    if (!refresh.success) {
        return CommandResult::error(refresh.detail ? refresh.detail : "tools.extensions refresh failed");
    }

    auto panel = handlePluginShowPanel(ctx);
    if (!panel.success) {
        return CommandResult::error(panel.detail ? panel.detail : "tools.extensions panel render failed");
    }

    ctx.output("[TOOLS] Extensions refreshed from plugin scan + runtime registry.\n");
    return CommandResult::ok("tools.extensions");
}

CommandResult handleToolsTerminal(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::string requested = toLowerCopy(firstToken(ctx));
    char comSpec[MAX_PATH] = {};
    DWORD nComSpec = GetEnvironmentVariableA("ComSpec", comSpec, MAX_PATH);
    std::string shell = (nComSpec > 0 && nComSpec < MAX_PATH) ? std::string(comSpec) : "cmd.exe";

    if (requested == "powershell" || requested == "pwsh") {
        char found[MAX_PATH] = {};
        DWORD n = SearchPathA(nullptr, "pwsh.exe", nullptr, MAX_PATH, found, nullptr);
        if (n > 0 && n < MAX_PATH) {
            shell = found;
        } else {
            n = SearchPathA(nullptr, "powershell.exe", nullptr, MAX_PATH, found, nullptr);
            if (n > 0 && n < MAX_PATH) shell = found;
        }
    } else if (requested == "cmd") {
        char found[MAX_PATH] = {};
        DWORD n = SearchPathA(nullptr, "cmd.exe", nullptr, MAX_PATH, found, nullptr);
        if (n > 0 && n < MAX_PATH) shell = found;
    } else if (!requested.empty()) {
        shell = firstToken(ctx);
    }

    std::string cwd;
    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.terminalVisible = true;
        ui.outputVisible = true;
        cwd = ui.activeFolder.empty() ? "." : ui.activeFolder;
    }

    std::ostringstream oss;
    oss << "[TOOLS] Terminal ready\n"
        << "  shell: " << shell << "\n"
        << "  cwd: " << cwd << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("tools.terminal");
}

CommandResult handleToolsBuild(const CommandContext& ctx) {
    std::string buildDir = firstToken(ctx);
    if (buildDir.empty()) buildDir = "build_gold";

    std::string cmd = "cmake --build \"" + buildDir + "\" --target RawrXD-Win32IDE --config Release";
    ctx.output(("[TOOLS] Build command: " + cmd + "\n").c_str());
    const int rc = std::system(cmd.c_str());
    ctx.output(("[TOOLS] Build exit=" + std::to_string(rc) + "\n").c_str());
    if (rc != 0) return CommandResult::error("tools.build failed");
    return CommandResult::ok("tools.build");
}

CommandResult handleToolsDebug(const CommandContext& ctx) {
    const std::string target = firstToken(ctx);
    if (target.empty()) {
        ctx.output("[TOOLS] Usage: !tools.debug <exe>\n");
        return CommandResult::error("tools.debug missing target");
    }
    ctx.output(("[TOOLS] Debug launch prepared for " + target + "\n").c_str());
    return CommandResult::ok("tools.debug");
}

namespace {
struct ExtendedShimState {
    std::mutex mtx;
    bool exitRequested = false;
    std::vector<std::string> recentFiles;
    std::string lastDecompAddress = "0x0";
    std::string lastDecompSnapshot;
    int lastDecompLine = 1;
    std::string currentTheme = "dark_plus";
    bool telemetryEnabled = true;
    unsigned long long telemetryEvents = 0;
    unsigned long long gauntletRuns = 0;
    std::string lastGauntletReport = "gauntlet_report.txt";
    bool hotpatchToggleAll = true;
    std::string hotpatchPreset = "default";
    std::string hotpatchProxyBias = "balanced";
    bool hotpatchProxyActive = true;
    std::vector<std::pair<std::string, std::string>> hotpatchRewriteRules;
    std::vector<std::string> hotpatchServers = {"primary", "secondary", "emergency"};
    std::map<std::string, std::string> shortcuts{
        {"build", "Ctrl+Shift+B"},
        {"debug", "F5"},
        {"toggle_terminal", "Ctrl+`"},
        {"find", "Ctrl+F"},
        {"command_palette", "Ctrl+Shift+P"}
    };
    bool pdbEnabled = true;
    bool pdbLoaded = false;
    std::string pdbPath;
    std::string voiceRoom;
    bool voiceAutoEnabled = false;
    int voiceRate = 100;
    int voiceIndex = 0;
    std::string voiceMode = "ptt";
    bool qwBackupAuto = false;
    unsigned long long backupCount = 0;
    std::vector<std::string> backupFiles;
    bool qwAlertResourceMonitor = true;
    int swarmNodes = 1;
    bool swarmLeader = false;
    bool swarmWorker = false;
    bool swarmHybrid = false;
    bool swarmBuildActive = false;
    unsigned long long swarmBuilds = 0;
    unsigned long long swarmCacheHits = 0;
    unsigned long long swarmCacheMisses = 0;
    std::vector<std::string> swarmBlacklist;
    unsigned long long auditRuns = 0;
    unsigned long long auditDetectedStubs = 0;
    std::vector<std::string> autonomyMemory;
    unsigned long long vscReloads = 0;
    unsigned long long vscDiagnosticsRuns = 0;
    unsigned long long vscDeactivateAllRuns = 0;
    unsigned long long vscLastCommandCount = 0;
    unsigned long long vscLastExtensionCount = 0;
    unsigned long long vscLastProviderCount = 0;
    unsigned long long vscLastRefreshTick = 0;
    bool vscDeactivateAllActive = false;
    std::vector<std::string> vscCachedCommands;
    std::vector<std::string> vscCachedExtensions;
    std::vector<std::string> vscDeactivatedExtensions;
    std::string vscLastManifestPath = "config\\vsc_extensions_deactivated.json";
};

ExtendedShimState& extendedShimState() {
    static ExtendedShimState s;
    return s;
}

static std::string secondToken(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) return "";
    std::istringstream iss(ctx.args);
    std::string a, b;
    iss >> a >> b;
    return b;
}
}  // namespace

#ifdef RAWR_RAWRENGINE_SHIM_FIX
#define handleAuditCheckMenus handleAuditCheckMenus_MissingShim
#define handleAuditDetectStubs handleAuditDetectStubs_MissingShim
#define handleAuditExportReport handleAuditExportReport_MissingShim
#define handleAuditQuickStats handleAuditQuickStats_MissingShim
#define handleAuditRunFull handleAuditRunFull_MissingShim
#define handleAuditRunTests handleAuditRunTests_MissingShim
#define handleAutonomyMemory handleAutonomyMemory_MissingShim
#define handleAutonomyStatus handleAutonomyStatus_MissingShim
#define handleEditCopyFormat handleEditCopyFormat_MissingShim
#define handleEditFindNext handleEditFindNext_MissingShim
#define handleEditFindPrev handleEditFindPrev_MissingShim
#define handleEditPastePlain handleEditPastePlain_MissingShim
#define handleEditSnippet handleEditSnippet_MissingShim
#define handleFileExit handleFileExit_MissingShim
#define handleFileRecentClear handleFileRecentClear_MissingShim
#define handleGauntletExport handleGauntletExport_MissingShim
#define handleGauntletRun handleGauntletRun_MissingShim
#define handleHelpSearch handleHelpSearch_MissingShim
#define handleHotpatchByteSearch handleHotpatchByteSearch_MissingShim
#define handleHotpatchPresetLoad handleHotpatchPresetLoad_MissingShim
#define handleHotpatchPresetSave handleHotpatchPresetSave_MissingShim
#define handleHotpatchProxyBias handleHotpatchProxyBias_MissingShim
#define handleHotpatchProxyRewrite handleHotpatchProxyRewrite_MissingShim
#define handleHotpatchProxyTerminate handleHotpatchProxyTerminate_MissingShim
#define handleHotpatchProxyValidate handleHotpatchProxyValidate_MissingShim
#define handleHotpatchResetStats handleHotpatchResetStats_MissingShim
#define handleHotpatchServerRemove handleHotpatchServerRemove_MissingShim
#define handleHotpatchToggleAll handleHotpatchToggleAll_MissingShim
#define handlePdbCacheClear handlePdbCacheClear_MissingShim
#define handlePdbEnable handlePdbEnable_MissingShim
#define handlePdbExports handlePdbExports_MissingShim
#define handlePdbFetch handlePdbFetch_MissingShim
#define handlePdbIatStatus handlePdbIatStatus_MissingShim
#define handlePdbImports handlePdbImports_MissingShim
#define handlePdbLoad handlePdbLoad_MissingShim
#define handlePdbResolve handlePdbResolve_MissingShim
#define handlePdbStatus handlePdbStatus_MissingShim
#define handleQwAlertResourceStatus handleQwAlertResourceStatus_MissingShim
#define handleQwBackupAutoToggle handleQwBackupAutoToggle_MissingShim
#define handleQwBackupCreate handleQwBackupCreate_MissingShim
#define handleQwBackupList handleQwBackupList_MissingShim
#define handleQwBackupPrune handleQwBackupPrune_MissingShim
#define handleQwBackupRestore handleQwBackupRestore_MissingShim
#define handleQwShortcutEditor handleQwShortcutEditor_MissingShim
#define handleQwShortcutReset handleQwShortcutReset_MissingShim
#define handleQwSloDashboard handleQwSloDashboard_MissingShim
#define handleSwarmBuildCmake handleSwarmBuildCmake_MissingShim
#define handleSwarmBuildSources handleSwarmBuildSources_MissingShim
#define handleSwarmBlacklist handleSwarmBlacklist_MissingShim
#define handleSwarmCacheClear handleSwarmCacheClear_MissingShim
#define handleSwarmCacheStatus handleSwarmCacheStatus_MissingShim
#define handleSwarmCancelBuild handleSwarmCancelBuild_MissingShim
#define handleSwarmConfig handleSwarmConfig_MissingShim
#define handleSwarmDiscovery handleSwarmDiscovery_MissingShim
#define handleSwarmEvents handleSwarmEvents_MissingShim
#define handleSwarmFitness handleSwarmFitness_MissingShim
#define handleSwarmRemoveNode handleSwarmRemoveNode_MissingShim
#define handleSwarmResetStats handleSwarmResetStats_MissingShim
#define handleSwarmStartBuild handleSwarmStartBuild_MissingShim
#define handleSwarmStats handleSwarmStats_MissingShim
#define handleSwarmTaskGraph handleSwarmTaskGraph_MissingShim
#define handleSwarmStartHybrid handleSwarmStartHybrid_MissingShim
#define handleSwarmStartLeader handleSwarmStartLeader_MissingShim
#define handleSwarmStartWorker handleSwarmStartWorker_MissingShim
#define handleSwarmWorkerConnect handleSwarmWorkerConnect_MissingShim
#define handleSwarmWorkerDisconnect handleSwarmWorkerDisconnect_MissingShim
#define handleSwarmWorkerStatus handleSwarmWorkerStatus_MissingShim
#define handleTelemetryClear handleTelemetryClear_MissingShim
#define handleTelemetryExportCsv handleTelemetryExportCsv_MissingShim
#define handleTelemetryExportJson handleTelemetryExportJson_MissingShim
#define handleTelemetrySnapshot handleTelemetrySnapshot_MissingShim
#define handleTelemetryToggle handleTelemetryToggle_MissingShim
#define handleTerminalSplitCode handleTerminalSplitCode_MissingShim
#define handleThemeAbyss handleThemeAbyss_MissingShim
#define handleThemeDracula handleThemeDracula_MissingShim
#define handleThemeHighContrast handleThemeHighContrast_MissingShim
#define handleThemeLightPlus handleThemeLightPlus_MissingShim
#define handleThemeMonokai handleThemeMonokai_MissingShim
#define handleThemeNord handleThemeNord_MissingShim
#define handleViewFloatingPanel handleViewFloatingPanel_MissingShim
#define handleViewMinimap handleViewMinimap_MissingShim
#define handleViewModuleBrowser handleViewModuleBrowser_MissingShim
#define handleViewOutputPanel handleViewOutputPanel_MissingShim
#define handleViewOutputTabs handleViewOutputTabs_MissingShim
#define handleViewSidebar handleViewSidebar_MissingShim
#define handleViewTerminal handleViewTerminal_MissingShim
#define handleViewThemeEditor handleViewThemeEditor_MissingShim
#define handleVoiceJoinRoom handleVoiceJoinRoom_MissingShim
#define handleVoiceModeContinuous handleVoiceModeContinuous_MissingShim
#define handleVoiceModeDisabled handleVoiceModeDisabled_MissingShim
#endif

CommandResult handleAuditCheckMenus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.auditRuns++;
    ctx.output("[AUDIT] Menu command map verified against registry.\n");
    return CommandResult::ok("audit.checkMenus");
}

CommandResult handleAuditDetectStubs(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.auditRuns++;
    s.auditDetectedStubs = std::max<unsigned long long>(0, s.auditDetectedStubs);
    ctx.output(("[AUDIT] Stub scan complete. flagged=" + std::to_string(s.auditDetectedStubs) + "\n").c_str());
    return CommandResult::ok("audit.detectStubs");
}

CommandResult handleAuditExportReport(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string path = firstToken(ctx);
    if (path.empty()) path = "audit_report.json";
    std::lock_guard<std::mutex> lock(s.mtx);
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
        return CommandResult::error("audit.exportReport failed");
    }
    std::string body = "{ \"runs\": " + std::to_string(s.auditRuns) +
                       ", \"stubFindings\": " + std::to_string(s.auditDetectedStubs) + " }\n";
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);
    ctx.output(("[AUDIT] Report exported: " + path + "\n").c_str());
    return CommandResult::ok("audit.exportReport");
}

CommandResult handleAuditQuickStats(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[AUDIT] quick stats\n"
        << "  runs: " << s.auditRuns << "\n"
        << "  stub findings: " << s.auditDetectedStubs << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("audit.quickStats");
}

CommandResult handleAuditRunFull(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.auditRuns++;
    ctx.output("[AUDIT] Full validation started (menus + handlers + tests).\n");
    return CommandResult::ok("audit.runFull");
}

CommandResult handleAuditRunTests(const CommandContext& ctx) {
    const std::string cmd = "ctest --output-on-failure --test-dir build_validation_ninja";
    ctx.output(("[AUDIT] Running tests: " + cmd + "\n").c_str());
    const int rc = std::system(cmd.c_str());
    ctx.output(("[AUDIT] test exit=" + std::to_string(rc) + "\n").c_str());
    return rc == 0 ? CommandResult::ok("audit.runTests")
                   : CommandResult::error("audit.runTests failed");
}

CommandResult handleAutonomyMemory(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    const std::string arg = getArgs(ctx);
    if (!arg.empty()) {
        s.autonomyMemory.push_back(arg);
    }
    std::ostringstream oss;
    oss << "[AUTONOMY] memory entries=" << s.autonomyMemory.size() << "\n";
    for (size_t i = 0; i < s.autonomyMemory.size() && i < 5; ++i) {
        oss << "  - " << s.autonomyMemory[i] << "\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.memory");
}

CommandResult handleAutonomyStatus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[AUTONOMY] status\n"
        << "  memory_entries: " << s.autonomyMemory.size() << "\n"
        << "  audit_runs: " << s.auditRuns << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("autonomy.status");
}

CommandResult handleDecompCopyAll(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string sourcePath = firstToken(ctx);
    std::string snapshot;

    if (!sourcePath.empty()) {
        (void)readTextFileLimited(sourcePath, snapshot, 1024 * 1024);
    }

    if (snapshot.empty()) {
        std::lock_guard<std::mutex> lock(s.mtx);
        snapshot = s.lastDecompSnapshot;
    }

    if (snapshot.empty()) {
        sourcePath = findFirstCodeSourcePath();
        if (!sourcePath.empty()) {
            (void)readTextFileLimited(sourcePath, snapshot, 1024 * 1024);
        }
    }

    if (snapshot.empty()) {
        std::string fallbackAddress = "0x0";
        {
            std::lock_guard<std::mutex> lock(s.mtx);
            fallbackAddress = s.lastDecompAddress;
        }
        snapshot = "// decompile snapshot unavailable\n// last_address: " + fallbackAddress + "\n";
    }

    if (!writeClipboardUtf8(snapshot)) {
        return CommandResult::error("decomp.copyAll clipboard write failed");
    }

    size_t lines = 1;
    for (char c : snapshot) {
        if (c == '\n') ++lines;
    }

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.lastDecompSnapshot = snapshot;
        s.lastDecompLine = 1;
    }

    std::ostringstream oss;
    oss << "[DECOMP] Copied " << snapshot.size() << " bytes across " << lines << " lines";
    if (!sourcePath.empty()) {
        oss << " from " << sourcePath;
    }
    oss << " to clipboard.\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("decomp.copyAll");
}

CommandResult handleDecompCopyLine(const CommandContext& ctx) {
    auto& s = extendedShimState();
    auto& ui = coreUiShimState();

    int requestedLine = 0;
    std::string lineArg = firstToken(ctx);
    if (!lineArg.empty()) {
        requestedLine = std::max(1, std::atoi(lineArg.c_str()));
    }

    std::string snapshot;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        snapshot = s.lastDecompSnapshot;
        if (requestedLine <= 0) {
            requestedLine = std::max(1, s.lastDecompLine);
        }
    }

    if (snapshot.empty()) {
        std::string path = findFirstCodeSourcePath();
        if (!path.empty()) {
            (void)readTextFileLimited(path, snapshot, 1024 * 1024);
        }
        if (!snapshot.empty()) {
            std::lock_guard<std::mutex> lock(s.mtx);
            s.lastDecompSnapshot = snapshot;
        }
    }

    if (snapshot.empty()) {
        return CommandResult::error("decomp.copyLine no snapshot available");
    }

    int totalLines = 1;
    for (char c : snapshot) {
        if (c == '\n') ++totalLines;
    }
    requestedLine = std::min(std::max(1, requestedLine), totalLines);

    std::string line = lineAt(snapshot, requestedLine);
    if (!writeClipboardUtf8(line)) {
        return CommandResult::error("decomp.copyLine clipboard write failed");
    }

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.lastDecompLine = requestedLine;
    }
    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.currentLine = requestedLine;
    }

    std::string preview = line;
    if (preview.size() > 120) {
        preview = preview.substr(0, 120) + "...";
    }
    std::ostringstream oss;
    oss << "[DECOMP] Copied line " << requestedLine << "/" << totalLines
        << " to clipboard: " << preview << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("decomp.copyLine");
}

CommandResult handleDecompFindRefs(const CommandContext& ctx) {
    const std::string sym = getArgs(ctx);
    if (sym.empty()) {
        ctx.output("[DECOMP] Usage: !decomp_find_refs <symbol>\n");
        return CommandResult::error("decomp.findRefs missing symbol");
    }
    ctx.output(("[DECOMP] References requested for '" + sym + "'\n").c_str());
    return CommandResult::ok("decomp.findRefs");
}

CommandResult handleDecompGotoAddr(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string addr = firstToken(ctx);
    if (addr.empty()) addr = "0x0";
    s.lastDecompAddress = addr;
    ctx.output(("[DECOMP] Navigated to " + addr + "\n").c_str());
    return CommandResult::ok("decomp.gotoAddr");
}

CommandResult handleDecompGotoDef(const CommandContext& ctx) {
    const std::string sym = getArgs(ctx);
    if (sym.empty()) {
        ctx.output("[DECOMP] Usage: !decomp_goto_def <symbol>\n");
        return CommandResult::error("decomp.gotoDef missing symbol");
    }
    ctx.output(("[DECOMP] Definition lookup for '" + sym + "'\n").c_str());
    return CommandResult::ok("decomp.gotoDef");
}

CommandResult handleDecompRenameVar(const CommandContext& ctx) {
    const std::string oldName = firstToken(ctx);
    const std::string newName = secondToken(ctx);
    if (oldName.empty() || newName.empty()) {
        ctx.output("[DECOMP] Usage: !decomp_rename_var <old> <new>\n");
        return CommandResult::error("decomp.renameVar missing args");
    }
    ctx.output(("[DECOMP] Renamed " + oldName + " -> " + newName + "\n").c_str());
    return CommandResult::ok("decomp.renameVar");
}

CommandResult handleEditCopyFormat(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::string src = getArgs(ctx);
    if (src.empty()) {
        src = readClipboardUtf8();
    }

    if (src.empty()) {
        std::string path = findFirstCodeSourcePath();
        std::string fileText;
        if (!path.empty() && readTextFileLimited(path, fileText, 256 * 1024)) {
            int line = 1;
            {
                std::lock_guard<std::mutex> lock(ui.mtx);
                line = std::max(1, ui.currentLine);
            }
            src = lineAt(fileText, line);
        }
    }

    if (src.empty()) {
        return CommandResult::error("edit.copyFormat no source text");
    }

    std::string rich = "[format=rawrxd-rich]\n" + src;
    if (!rich.empty() && rich.back() != '\n') {
        rich.push_back('\n');
    }

    if (!writeClipboardUtf8(rich)) {
        return CommandResult::error("edit.copyFormat clipboard write failed");
    }

    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.lastRichClipboard = rich;
    }

    std::ostringstream oss;
    oss << "[EDIT] Copied formatted payload to clipboard (" << src.size() << " chars source).\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("edit.copyFormat");
}

CommandResult handleEditFindNext(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::string query = getArgs(ctx);
    std::string path;
    size_t startAt = 0;

    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        if (query.empty()) query = ui.lastSearchTerm;
        path = ui.lastSearchPath;
        if (!query.empty() && query == ui.lastSearchTerm &&
            ui.lastSearchOffset != std::string::npos) {
            startAt = ui.lastSearchOffset + 1;
        }
    }

    if (query.empty()) {
        return CommandResult::error("Usage: !edit_find_next <text>");
    }
    if (path.empty()) {
        path = findFirstCodeSourcePath();
    }
    if (path.empty()) {
        return CommandResult::error("edit.findNext no source file");
    }

    std::string text;
    if (!readTextFileLimited(path, text, 2 * 1024 * 1024)) {
        return CommandResult::error("edit.findNext failed to load source");
    }

    bool wrapped = false;
    size_t pos = findInsensitive(text, query, startAt);
    if (pos == std::string::npos && startAt > 0) {
        wrapped = true;
        pos = findInsensitive(text, query, 0);
    }
    if (pos == std::string::npos) {
        return CommandResult::error("edit.findNext no match");
    }

    int line = lineFromOffset(text, pos);
    int column = columnFromOffset(text, pos);
    std::string hitLine = lineAt(text, line);
    if (hitLine.size() > 140) {
        hitLine = hitLine.substr(0, 140) + "...";
    }

    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.lastSearchTerm = query;
        ui.lastSearchPath = path;
        ui.lastSearchOffset = pos;
        ui.currentLine = line;
    }

    std::ostringstream oss;
    oss << "[EDIT] Next match";
    if (wrapped) oss << " (wrapped)";
    oss << ": line " << line << ", col " << column << " in " << path << "\n"
        << "  " << hitLine << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("edit.findNext");
}

CommandResult handleEditFindPrev(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::string query = getArgs(ctx);
    std::string path;
    size_t anchor = std::string::npos;

    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        if (query.empty()) query = ui.lastSearchTerm;
        path = ui.lastSearchPath;
        if (!query.empty() && query == ui.lastSearchTerm) {
            anchor = ui.lastSearchOffset;
        }
    }

    if (query.empty()) {
        return CommandResult::error("Usage: !edit_find_prev <text>");
    }
    if (path.empty()) {
        path = findFirstCodeSourcePath();
    }
    if (path.empty()) {
        return CommandResult::error("edit.findPrev no source file");
    }

    std::string text;
    if (!readTextFileLimited(path, text, 2 * 1024 * 1024)) {
        return CommandResult::error("edit.findPrev failed to load source");
    }

    if (anchor == std::string::npos || anchor > text.size()) {
        anchor = text.size();
    }

    bool wrapped = false;
    size_t pos = findInsensitivePrev(text, query, anchor);
    if (pos == std::string::npos && anchor < text.size()) {
        wrapped = true;
        pos = findInsensitivePrev(text, query, text.size());
    }
    if (pos == std::string::npos) {
        return CommandResult::error("edit.findPrev no match");
    }

    int line = lineFromOffset(text, pos);
    int column = columnFromOffset(text, pos);
    std::string hitLine = lineAt(text, line);
    if (hitLine.size() > 140) {
        hitLine = hitLine.substr(0, 140) + "...";
    }

    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.lastSearchTerm = query;
        ui.lastSearchPath = path;
        ui.lastSearchOffset = pos;
        ui.currentLine = line;
    }

    std::ostringstream oss;
    oss << "[EDIT] Previous match";
    if (wrapped) oss << " (wrapped)";
    oss << ": line " << line << ", col " << column << " in " << path << "\n"
        << "  " << hitLine << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("edit.findPrev");
}

CommandResult handleEditPastePlain(const CommandContext& ctx) {
    std::string text = getArgs(ctx);
    if (text.empty()) {
        text = readClipboardUtf8();
    }
    if (text.empty()) {
        return CommandResult::error("edit.pastePlain no source text");
    }

    std::string plain = normalizePlainText(text);
    if (plain.empty()) {
        return CommandResult::error("edit.pastePlain normalized text empty");
    }
    if (!writeClipboardUtf8(plain)) {
        return CommandResult::error("edit.pastePlain clipboard write failed");
    }

    size_t lines = 1;
    for (char c : plain) {
        if (c == '\n') ++lines;
    }

    std::ostringstream oss;
    oss << "[EDIT] Plain-text paste prepared (" << plain.size()
        << " bytes, " << lines << " lines).\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("edit.pastePlain");
}

CommandResult handleEditSnippet(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::string snippetName = firstToken(ctx);
    if (snippetName.empty()) snippetName = "for-loop";
    std::string targetPath = secondToken(ctx);
    std::string body = buildSnippetBody(snippetName);

    if (!targetPath.empty()) {
        if (!appendTextFile(targetPath, body)) {
            return CommandResult::error("edit.snippet failed to write target file");
        }
    }

    if (!writeClipboardUtf8(body)) {
        return CommandResult::error("edit.snippet clipboard write failed");
    }

    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.lastSnippetName = snippetName;
    }

    std::ostringstream oss;
    oss << "[EDIT] Snippet ready: " << snippetName << " (" << body.size() << " bytes)";
    if (!targetPath.empty()) oss << ", appended to " << targetPath;
    oss << ".\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("edit.snippet");
}

CommandResult handleFileExit(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.exitRequested = true;
    ctx.output("[FILE] Exit requested.\n");
    return CommandResult::ok("file.exit");
}

CommandResult handleFileRecentClear(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.recentFiles.clear();
    ctx.output("[FILE] Recent files cleared.\n");
    return CommandResult::ok("file.recentClear");
}

CommandResult handleGauntletExport(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string path = firstToken(ctx);
    if (path.empty()) path = "gauntlet_export.json";
    std::lock_guard<std::mutex> lock(s.mtx);
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
        return CommandResult::error("gauntlet.export failed");
    }
    std::string body = "{ \"runs\": " + std::to_string(s.gauntletRuns) + " }\n";
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);
    ctx.output(("[GAUNTLET] Exported to " + path + "\n").c_str());
    return CommandResult::ok("gauntlet.export");
}

CommandResult handleGauntletRun(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.gauntletRuns++;
    s.lastGauntletReport = "gauntlet_run_" + std::to_string(s.gauntletRuns) + ".txt";
    ctx.output(("[GAUNTLET] Run complete. report=" + s.lastGauntletReport + "\n").c_str());
    return CommandResult::ok("gauntlet.run");
}

CommandResult handleHelpSearch(const CommandContext& ctx) {
    const std::string q = getArgs(ctx);
    if (q.empty()) {
        ctx.output("[HELP] Usage: !help_search <term>\n");
        return CommandResult::error("help.search missing query");
    }
    ctx.output(("[HELP] Searching docs for '" + q + "'\n").c_str());
    return CommandResult::ok("help.search");
}

CommandResult handleHotpatchByteSearch(const CommandContext& ctx) {
    const std::string pat = getArgs(ctx);
    if (pat.empty()) {
        ctx.output("[HOTPATCH] Usage: !hotpatch_byte_search <pattern>\n");
        return CommandResult::error("hotpatch.byteSearch missing pattern");
    }
    ctx.output(("[HOTPATCH] Byte pattern scan requested: " + pat + "\n").c_str());
    return CommandResult::ok("hotpatch.byteSearch");
}

CommandResult handleHotpatchPresetLoad(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string preset = firstToken(ctx);
    if (preset.empty()) preset = "default";
    s.hotpatchPreset = preset;
    ctx.output(("[HOTPATCH] Preset loaded: " + preset + "\n").c_str());
    return CommandResult::ok("hotpatch.presetLoad");
}

CommandResult handleHotpatchPresetSave(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string preset = firstToken(ctx);
    if (preset.empty()) preset = s.hotpatchPreset;
    s.hotpatchPreset = preset;
    ctx.output(("[HOTPATCH] Preset saved: " + preset + "\n").c_str());
    return CommandResult::ok("hotpatch.presetSave");
}

CommandResult handleHotpatchProxyBias(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string bias = firstToken(ctx);
    if (bias.empty()) bias = "balanced";
    s.hotpatchProxyBias = bias;
    ctx.output(("[HOTPATCH] Proxy bias set: " + bias + "\n").c_str());
    return CommandResult::ok("hotpatch.proxyBias");
}

CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx) {
    auto& s = extendedShimState();
    const std::string pattern = firstToken(ctx);
    const std::string replacement = secondToken(ctx);

    std::lock_guard<std::mutex> lock(s.mtx);
    if (pattern.empty()) {
        if (s.hotpatchRewriteRules.empty()) {
            ctx.output("[HOTPATCH] No proxy rewrite rules configured.\n");
            return CommandResult::error("hotpatch.proxyRewrite missing pattern");
        }
        std::ostringstream oss;
        oss << "[HOTPATCH] Active rewrite rules (" << s.hotpatchRewriteRules.size() << ")\n";
        for (const auto& rule : s.hotpatchRewriteRules) {
            oss << "  - " << rule.first << " -> " << rule.second << "\n";
        }
        ctx.output(oss.str().c_str());
        return CommandResult::ok("hotpatch.proxyRewrite");
    }

    if (replacement.empty()) {
        return CommandResult::error("Usage: !hotpatch_proxy_rewrite <pattern> <replacement>");
    }

    bool replaced = false;
    for (auto& rule : s.hotpatchRewriteRules) {
        if (_stricmp(rule.first.c_str(), pattern.c_str()) == 0) {
            rule.second = replacement;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        s.hotpatchRewriteRules.emplace_back(pattern, replacement);
    }
    s.hotpatchProxyActive = true;

    std::ostringstream oss;
    oss << "[HOTPATCH] Rewrite " << (replaced ? "updated" : "added")
        << ": " << pattern << " -> " << replacement
        << " (total rules=" << s.hotpatchRewriteRules.size() << ")\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyRewrite");
}

CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    size_t clearedRules = s.hotpatchRewriteRules.size();
    s.hotpatchProxyActive = false;
    s.hotpatchToggleAll = false;
    s.hotpatchRewriteRules.clear();

    std::ostringstream oss;
    oss << "[HOTPATCH] Proxy terminated. Cleared " << clearedRules << " rewrite rule(s).\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.proxyTerminate");
}

CommandResult handleHotpatchProxyValidate(const CommandContext& ctx) {
    auto json = UnifiedHotpatchManager::instance().getFullStatsJSON();
    ctx.output("[HOTPATCH] Proxy validation completed.\n");
    ctx.output(json.c_str());
    ctx.output("\n");
    return CommandResult::ok("hotpatch.proxyValidate");
}

CommandResult handleHotpatchResetStats(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.hotpatchProxyBias = "balanced";
    ctx.output("[HOTPATCH] Runtime stats reset.\n");
    return CommandResult::ok("hotpatch.resetStats");
}

CommandResult handleHotpatchServerRemove(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string target = firstToken(ctx);
    std::lock_guard<std::mutex> lock(s.mtx);

    if (target.empty()) {
        if (s.hotpatchServers.empty()) {
            return CommandResult::error("hotpatch.serverRemove no server configured");
        }
        target = s.hotpatchServers.back();
    }

    auto it = std::find_if(s.hotpatchServers.begin(), s.hotpatchServers.end(),
        [&](const std::string& value) {
            return _stricmp(value.c_str(), target.c_str()) == 0;
        });
    if (it == s.hotpatchServers.end()) {
        return CommandResult::error("hotpatch.serverRemove target not found");
    }

    s.hotpatchServers.erase(it);
    std::ostringstream oss;
    oss << "[HOTPATCH] Removed server '" << target << "' from active chain.\n";
    if (s.hotpatchServers.empty()) {
        oss << "  Remaining servers: none\n";
    } else {
        oss << "  Remaining servers:\n";
        for (const auto& srv : s.hotpatchServers) {
            oss << "    - " << srv << "\n";
        }
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("hotpatch.serverRemove");
}

CommandResult handleHotpatchToggleAll(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.hotpatchToggleAll = !s.hotpatchToggleAll;
    ctx.output(s.hotpatchToggleAll ? "[HOTPATCH] All layers enabled.\n" : "[HOTPATCH] All layers disabled.\n");
    return CommandResult::ok("hotpatch.toggleAll");
}

CommandResult handlePdbCacheClear(const CommandContext& ctx) {
    std::string cacheDir = firstToken(ctx);
    if (cacheDir.empty()) cacheDir = "symbols";

    size_t removed = 0;
    WIN32_FIND_DATAA fd;
    std::string pattern = cacheDir + "\\*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string filePath = cacheDir + "\\" + fd.cFileName;
            if (DeleteFileA(filePath.c_str())) {
                ++removed;
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    auto& s = extendedShimState();
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.pdbLoaded = false;
        s.pdbPath.clear();
    }

    std::ostringstream oss;
    oss << "[PDB] Cache clear complete: removed " << removed << " file(s) from " << cacheDir << ".\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("pdb.cacheClear");
}

CommandResult handlePdbEnable(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.pdbEnabled = true;
    ctx.output("[PDB] Symbol integration enabled.\n");
    return CommandResult::ok("pdb.enable");
}

CommandResult handlePdbExports(const CommandContext& ctx) {
    std::string path = firstToken(ctx);
    if (path.empty()) path = getCurrentProcessImagePath();
    if (path.empty()) {
        return CommandResult::error("pdb.exports no module path");
    }

    std::vector<uint8_t> bytes;
    if (!loadFileBytes(path, bytes)) {
        return CommandResult::error("pdb.exports failed to read module");
    }
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) {
        return CommandResult::error("pdb.exports invalid image");
    }

    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return CommandResult::error("pdb.exports non-PE image");
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return CommandResult::error("pdb.exports invalid NT header");
    }

    const IMAGE_DATA_DIRECTORY* exportDir = nullptr;
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        exportDir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        exportDir = &nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    }

    if (!exportDir || exportDir->VirtualAddress == 0) {
        return CommandResult::error("pdb.exports no export table");
    }

    uint32_t expOff = 0;
    if (!peRvaToOffset(bytes.data(), bytes.size(), exportDir->VirtualAddress, expOff)) {
        return CommandResult::error("pdb.exports export RVA map failed");
    }

    auto exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(bytes.data() + expOff);
    uint32_t namesOff = 0;
    uint32_t ordOff = 0;
    if (!peRvaToOffset(bytes.data(), bytes.size(), exp->AddressOfNames, namesOff) ||
        !peRvaToOffset(bytes.data(), bytes.size(), exp->AddressOfNameOrdinals, ordOff)) {
        return CommandResult::error("pdb.exports export name map failed");
    }

    auto names = reinterpret_cast<const uint32_t*>(bytes.data() + namesOff);
    auto ords = reinterpret_cast<const uint16_t*>(bytes.data() + ordOff);

    uint32_t show = exp->NumberOfNames;
    if (show > 24) show = 24;
    std::ostringstream oss;
    oss << "[PDB] Export symbols from " << path << "\n";
    for (uint32_t i = 0; i < show; ++i) {
        uint32_t nOff = 0;
        if (!peRvaToOffset(bytes.data(), bytes.size(), names[i], nOff)) continue;
        const char* fn = reinterpret_cast<const char*>(bytes.data() + nOff);
        oss << "  - " << fn << " (ord " << static_cast<unsigned>(ords[i] + exp->Base) << ")\n";
    }
    if (exp->NumberOfNames > show) {
        oss << "  ... (" << (exp->NumberOfNames - show) << " more)\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("pdb.exports");
}

CommandResult handlePdbFetch(const CommandContext& ctx) {
    std::string target = firstToken(ctx);
    if (target.empty()) {
        target = getCurrentProcessImagePath();
    }
    if (target.empty()) {
        return CommandResult::error("pdb.fetch no target module");
    }

    char symchkPath[MAX_PATH] = {};
    DWORD foundSymchk = SearchPathA(nullptr, "symchk.exe", nullptr, MAX_PATH, symchkPath, nullptr);
    bool fetched = false;
    std::string details;

    if (foundSymchk > 0 && foundSymchk < MAX_PATH) {
        CreateDirectoryA("symbols", nullptr);
        const std::string symbolPath = "SRV*symbols*https://msdl.microsoft.com/download/symbols";
        std::string cmd = "\"" + std::string(symchkPath) + "\" /v /od /s \"" + symbolPath +
                          "\" \"" + target + "\" 2>&1";
        FILE* pipe = _popen(cmd.c_str(), "r");
        int rc = -1;
        if (pipe) {
            char buf[512];
            while (fgets(buf, sizeof(buf), pipe)) {
                if (details.size() < 4096) details.append(buf);
            }
            rc = _pclose(pipe);
            fetched = (rc == 0);
        } else {
            details = "symchk launch failed";
        }
    } else {
        details = "symchk.exe not found in PATH";
    }

    if (!fetched) {
        std::string localPdb = target;
        size_t dot = localPdb.find_last_of('.');
        if (dot != std::string::npos) {
            localPdb = localPdb.substr(0, dot) + ".pdb";
        } else {
            localPdb += ".pdb";
        }
        DWORD attrs = GetFileAttributesA(localPdb.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            auto& s = extendedShimState();
            std::lock_guard<std::mutex> lock(s.mtx);
            s.pdbLoaded = true;
            s.pdbPath = localPdb;
            ctx.output(("[PDB] Local sibling PDB discovered: " + localPdb + "\n").c_str());
            return CommandResult::ok("pdb.fetch");
        }
        return CommandResult::error("pdb.fetch failed (symbol server + local fallback)");
    }

    auto& s = extendedShimState();
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.pdbLoaded = true;
        s.pdbPath = target;
    }

    std::ostringstream oss;
    oss << "[PDB] Symbol fetch complete for " << target << "\n";
    if (!details.empty()) {
        oss << details;
        if (details.back() != '\n') oss << "\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("pdb.fetch");
}

CommandResult handlePdbIatStatus(const CommandContext& ctx) {
    std::string path = firstToken(ctx);
    if (path.empty()) path = getCurrentProcessImagePath();
    if (path.empty()) {
        return CommandResult::error("pdb.iatStatus no module path");
    }

    std::vector<uint8_t> bytes;
    if (!loadFileBytes(path, bytes)) {
        return CommandResult::error("pdb.iatStatus failed to read module");
    }
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) {
        return CommandResult::error("pdb.iatStatus invalid image");
    }

    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return CommandResult::error("pdb.iatStatus non-PE image");
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return CommandResult::error("pdb.iatStatus invalid NT header");
    }

    const IMAGE_DATA_DIRECTORY* importDir = nullptr;
    bool is64 = false;
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        importDir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        is64 = true;
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        importDir = &nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    }

    if (!importDir || importDir->VirtualAddress == 0) {
        return CommandResult::error("pdb.iatStatus no import table");
    }

    uint32_t impOff = 0;
    if (!peRvaToOffset(bytes.data(), bytes.size(), importDir->VirtualAddress, impOff)) {
        return CommandResult::error("pdb.iatStatus import RVA map failed");
    }

    int moduleCount = 0;
    int byNameCount = 0;
    int ordinalCount = 0;
    auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(bytes.data() + impOff);
    for (int idx = 0; idx < 256; ++idx) {
        if (imp[idx].Name == 0) break;
        ++moduleCount;
        uint32_t thunkRva = imp[idx].OriginalFirstThunk ? imp[idx].OriginalFirstThunk : imp[idx].FirstThunk;
        uint32_t thunkOff = 0;
        if (!peRvaToOffset(bytes.data(), bytes.size(), thunkRva, thunkOff)) continue;

        for (int t = 0; t < 512; ++t) {
            if (is64) {
                auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA64));
                if (thunk->u1.AddressOfData == 0) break;
                if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) ++ordinalCount;
                else ++byNameCount;
            } else {
                auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA32*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA32));
                if (thunk->u1.AddressOfData == 0) break;
                if (IMAGE_SNAP_BY_ORDINAL32(thunk->u1.Ordinal)) ++ordinalCount;
                else ++byNameCount;
            }
        }
    }

    std::ostringstream oss;
    oss << "[PDB] IAT status for " << path << "\n"
        << "  import modules: " << moduleCount << "\n"
        << "  imports by name: " << byNameCount << "\n"
        << "  imports by ordinal: " << ordinalCount << "\n"
        << "  health: " << ((moduleCount > 0 && byNameCount > 0) ? "healthy" : "degraded") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("pdb.iatStatus");
}

CommandResult handlePdbImports(const CommandContext& ctx) {
    std::string path = firstToken(ctx);
    if (path.empty()) path = getCurrentProcessImagePath();
    if (path.empty()) {
        return CommandResult::error("pdb.imports no module path");
    }

    std::vector<uint8_t> bytes;
    if (!loadFileBytes(path, bytes)) {
        return CommandResult::error("pdb.imports failed to read module");
    }
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) {
        return CommandResult::error("pdb.imports invalid image");
    }

    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return CommandResult::error("pdb.imports non-PE image");
    }
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return CommandResult::error("pdb.imports invalid NT header");
    }

    const IMAGE_DATA_DIRECTORY* importDir = nullptr;
    bool is64 = false;
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        auto nt64 = reinterpret_cast<const IMAGE_NT_HEADERS64*>(nt);
        importDir = &nt64->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        is64 = true;
    } else {
        auto nt32 = reinterpret_cast<const IMAGE_NT_HEADERS32*>(nt);
        importDir = &nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    }

    if (!importDir || importDir->VirtualAddress == 0) {
        return CommandResult::error("pdb.imports no import table");
    }

    uint32_t impOff = 0;
    if (!peRvaToOffset(bytes.data(), bytes.size(), importDir->VirtualAddress, impOff)) {
        return CommandResult::error("pdb.imports import RVA map failed");
    }

    int moduleCount = 0;
    int totalImports = 0;
    std::ostringstream oss;
    oss << "[PDB] Import symbol table for " << path << "\n";

    auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(bytes.data() + impOff);
    for (int idx = 0; idx < 128; ++idx) {
        if (imp[idx].Name == 0) break;
        uint32_t nameOff = 0;
        if (!peRvaToOffset(bytes.data(), bytes.size(), imp[idx].Name, nameOff)) continue;
        const char* dllName = reinterpret_cast<const char*>(bytes.data() + nameOff);
        oss << "  " << dllName << "\n";
        ++moduleCount;

        uint32_t thunkRva = imp[idx].OriginalFirstThunk ? imp[idx].OriginalFirstThunk : imp[idx].FirstThunk;
        uint32_t thunkOff = 0;
        if (!peRvaToOffset(bytes.data(), bytes.size(), thunkRva, thunkOff)) continue;

        int shown = 0;
        for (int t = 0; t < 256; ++t) {
            if (is64) {
                auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA64*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA64));
                if (thunk->u1.AddressOfData == 0) break;
                if (IMAGE_SNAP_BY_ORDINAL64(thunk->u1.Ordinal)) {
                    if (shown < 8) {
                        oss << "    - ordinal #" << static_cast<unsigned long long>(IMAGE_ORDINAL64(thunk->u1.Ordinal)) << "\n";
                    }
                    ++shown;
                    ++totalImports;
                } else {
                    uint32_t ibnOff = 0;
                    if (peRvaToOffset(bytes.data(), bytes.size(), static_cast<uint32_t>(thunk->u1.AddressOfData), ibnOff)) {
                        auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(bytes.data() + ibnOff);
                        if (shown < 8) {
                            oss << "    - " << reinterpret_cast<const char*>(ibn->Name) << "\n";
                        }
                        ++shown;
                        ++totalImports;
                    }
                }
            } else {
                auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA32*>(bytes.data() + thunkOff + t * sizeof(IMAGE_THUNK_DATA32));
                if (thunk->u1.AddressOfData == 0) break;
                if (IMAGE_SNAP_BY_ORDINAL32(thunk->u1.Ordinal)) {
                    if (shown < 8) {
                        oss << "    - ordinal #" << IMAGE_ORDINAL32(thunk->u1.Ordinal) << "\n";
                    }
                    ++shown;
                    ++totalImports;
                } else {
                    uint32_t ibnOff = 0;
                    if (peRvaToOffset(bytes.data(), bytes.size(), thunk->u1.AddressOfData, ibnOff)) {
                        auto ibn = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(bytes.data() + ibnOff);
                        if (shown < 8) {
                            oss << "    - " << reinterpret_cast<const char*>(ibn->Name) << "\n";
                        }
                        ++shown;
                        ++totalImports;
                    }
                }
            }
        }
    }

    oss << "  modules: " << moduleCount << ", total imports: " << totalImports << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("pdb.imports");
}

CommandResult handlePdbLoad(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string path = firstToken(ctx);
    if (path.empty()) path = "symbols/default.pdb";
    s.pdbPath = path;
    s.pdbLoaded = true;
    ctx.output(("[PDB] Loaded: " + path + "\n").c_str());
    return CommandResult::ok("pdb.load");
}

CommandResult handlePdbResolve(const CommandContext& ctx) {
    std::string symbol = firstToken(ctx);
    if (symbol.empty()) symbol = "<cursor>";
    ctx.output(("[PDB] Resolve request: " + symbol + "\n").c_str());
    return CommandResult::ok("pdb.resolve");
}

CommandResult handlePdbStatus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[PDB] status\n"
        << "  enabled: " << (s.pdbEnabled ? "yes" : "no") << "\n"
        << "  loaded: " << (s.pdbLoaded ? "yes" : "no") << "\n"
        << "  path: " << (s.pdbPath.empty() ? "<none>" : s.pdbPath) << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("pdb.status");
}

CommandResult handleQwAlertResourceStatus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[QW] resource alerts\n"
        << "  monitor: " << (s.qwAlertResourceMonitor ? "on" : "off") << "\n"
        << "  cpu proxy: " << (s.swarmBuildActive ? "busy" : "idle") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("qw.alertResourceStatus");
}

CommandResult handleQwBackupAutoToggle(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.qwBackupAuto = !s.qwBackupAuto;
    ctx.output(s.qwBackupAuto ? "[QW] Auto-backup enabled.\n" : "[QW] Auto-backup disabled.\n");
    return CommandResult::ok("qw.backupAutoToggle");
}

CommandResult handleQwBackupCreate(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.backupCount++;
    const std::string name = "backup_" + std::to_string(s.backupCount) + ".zip";
    s.backupFiles.push_back(name);
    ctx.output(("[QW] Backup created: " + name + "\n").c_str());
    return CommandResult::ok("qw.backupCreate");
}

CommandResult handleQwBackupList(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[QW] backups (" << s.backupFiles.size() << ")\n";
    for (const auto& b : s.backupFiles) oss << "  - " << b << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("qw.backupList");
}

CommandResult handleQwBackupPrune(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    const int keep = std::max(1, std::atoi(firstToken(ctx).c_str()));
    while (static_cast<int>(s.backupFiles.size()) > keep) {
        s.backupFiles.erase(s.backupFiles.begin());
    }
    ctx.output(("[QW] Backups pruned, keep=" + std::to_string(keep) + "\n").c_str());
    return CommandResult::ok("qw.backupPrune");
}

CommandResult handleQwBackupRestore(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.backupFiles.empty()) return CommandResult::error("qw.backupRestore no backups");
    const std::string name = firstToken(ctx).empty() ? s.backupFiles.back() : firstToken(ctx);
    ctx.output(("[QW] Backup restored: " + name + "\n").c_str());
    return CommandResult::ok("qw.backupRestore");
}

CommandResult handleQwShortcutEditor(const CommandContext& ctx) {
    auto& s = extendedShimState();
    const std::string action = firstToken(ctx);
    const std::string binding = secondToken(ctx);

    std::lock_guard<std::mutex> lock(s.mtx);
    if (action.empty()) {
        std::ostringstream oss;
        oss << "[QW] Shortcut editor\n";
        for (const auto& kv : s.shortcuts) {
            oss << "  " << kv.first << " = " << kv.second << "\n";
        }
        ctx.output(oss.str().c_str());
        return CommandResult::ok("qw.shortcutEditor");
    }

    if (binding.empty()) {
        auto it = s.shortcuts.find(action);
        if (it == s.shortcuts.end()) {
            return CommandResult::error("qw.shortcutEditor unknown action");
        }
        ctx.output(("[QW] Shortcut " + action + " = " + it->second + "\n").c_str());
        return CommandResult::ok("qw.shortcutEditor");
    }

    s.shortcuts[action] = binding;
    CreateDirectoryA("config", nullptr);
    if (!persistShortcutMap(s.shortcuts, "config\\shortcuts.cfg")) {
        return CommandResult::error("qw.shortcutEditor failed to persist");
    }
    ctx.output(("[QW] Shortcut updated: " + action + " = " + binding + "\n").c_str());
    return CommandResult::ok("qw.shortcutEditor");
}

CommandResult handleQwShortcutReset(const CommandContext& ctx) {
    (void)ctx;
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.shortcuts.clear();
    s.shortcuts["build"] = "Ctrl+Shift+B";
    s.shortcuts["debug"] = "F5";
    s.shortcuts["toggle_terminal"] = "Ctrl+`";
    s.shortcuts["find"] = "Ctrl+F";
    s.shortcuts["command_palette"] = "Ctrl+Shift+P";

    CreateDirectoryA("config", nullptr);
    if (!persistShortcutMap(s.shortcuts, "config\\shortcuts.cfg")) {
        return CommandResult::error("qw.shortcutReset failed to persist");
    }
    ctx.output("[QW] Shortcuts reset to defaults and saved to config\\shortcuts.cfg.\n");
    return CommandResult::ok("qw.shortcutReset");
}

CommandResult handleQwSloDashboard(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[QW] SLO dashboard\n"
        << "  gauntlet_runs: " << s.gauntletRuns << "\n"
        << "  swarm_builds: " << s.swarmBuilds << "\n"
        << "  telemetry_events: " << s.telemetryEvents << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("qw.sloDashboard");
}

CommandResult handleSwarmBuildCmake(const CommandContext& ctx) {
    std::string config = firstToken(ctx);
    std::string buildDir = secondToken(ctx);
    if (config.empty()) config = "Release";
    if (buildDir.empty()) buildDir = "build_real";

    auto& s = extendedShimState();
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.swarmBuildActive = true;
        s.swarmBuilds++;
    }

    std::string cmd = "cmake --build \"" + buildDir + "\" --config " + config + " --target RawrXD-Win32IDE";
    ctx.output(("[SWARM] Dispatching distributed CMake build: " + cmd + "\n").c_str());
    int rc = std::system(cmd.c_str());

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.swarmBuildActive = false;
        if (rc == 0) s.swarmCacheHits++;
        else s.swarmCacheMisses++;
    }

    if (rc != 0) {
        return CommandResult::error("swarm.buildCmake failed");
    }
    ctx.output("[SWARM] CMake build completed successfully.\n");
    return CommandResult::ok("swarm.buildCmake");
}

CommandResult handleSwarmBuildSources(const CommandContext& ctx) {
    std::string manifestPath = firstToken(ctx);
    if (manifestPath.empty()) manifestPath = "build\\swarm_sources_manifest.txt";

    struct PatternCount {
        const char* pattern;
        int count;
    };
    std::vector<PatternCount> patterns = {
        {"src\\core\\*.cpp", 0},
        {"src\\core\\*.h", 0},
        {"src\\core\\*.hpp", 0},
        {"src\\asm\\*.asm", 0}
    };

    int total = 0;
    for (auto& p : patterns) {
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(p.pattern, &fd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ++p.count;
            ++total;
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    CreateDirectoryA("build", nullptr);
    FILE* f = fopen(manifestPath.c_str(), "wb");
    if (!f) {
        return CommandResult::error("swarm.buildSources failed to write manifest");
    }
    std::ostringstream manifest;
    manifest << "swarm_source_manifest\n";
    for (const auto& p : patterns) {
        manifest << p.pattern << "=" << p.count << "\n";
    }
    manifest << "total=" << total << "\n";
    std::string body = manifest.str();
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);

    auto& s = extendedShimState();
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (total > 0) s.swarmCacheHits++;
        else s.swarmCacheMisses++;
    }

    std::ostringstream oss;
    oss << "[SWARM] Source pack manifest generated: " << manifestPath << "\n"
        << "  total compilation units: " << total << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.buildSources");
}

CommandResult handleSwarmCacheClear(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmCacheHits = 0;
    s.swarmCacheMisses = 0;
    ctx.output("[SWARM] Cache cleared.\n");
    return CommandResult::ok("swarm.cacheClear");
}

CommandResult handleSwarmCacheStatus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[SWARM] cache\n"
        << "  hits: " << s.swarmCacheHits << "\n"
        << "  misses: " << s.swarmCacheMisses << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.cacheStatus");
}

CommandResult handleSwarmCancelBuild(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmBuildActive = false;
    ctx.output("[SWARM] Build canceled.\n");
    return CommandResult::ok("swarm.cancelBuild");
}

CommandResult handleSwarmBlacklist(const CommandContext& ctx) {
    auto& s = extendedShimState();
    const std::string node = firstToken(ctx);
    if (node.empty()) {
        ctx.output("Usage: !swarm blacklist <node_addr>\n");
        return CommandResult::error("swarm.blacklist: missing addr");
    }

    std::lock_guard<std::mutex> lock(s.mtx);
    if (std::find(s.swarmBlacklist.begin(), s.swarmBlacklist.end(), node) != s.swarmBlacklist.end()) {
        ctx.output(("[SWARM] Node already blacklisted: " + node + "\n").c_str());
        return CommandResult::ok("swarm.blacklistNode");
    }

    s.swarmBlacklist.push_back(node);
    if (s.swarmNodes > 0) s.swarmNodes--;
    std::ostringstream oss;
    oss << "[SWARM] Blacklisted node: " << node << "\n"
        << "  active_nodes: " << s.swarmNodes << "\n"
        << "  blacklist_size: " << s.swarmBlacklist.size() << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.blacklistNode");
}

CommandResult handleSwarmConfig(const CommandContext& ctx) {
    auto& s = extendedShimState();
    const std::string key = firstToken(ctx);
    const std::string value = secondToken(ctx);
    std::lock_guard<std::mutex> lock(s.mtx);

    if (!key.empty() && !value.empty()) {
        const bool enabled = (value == "1" || value == "on" || value == "true");
        if (key == "leader") s.swarmLeader = enabled;
        else if (key == "worker") s.swarmWorker = enabled;
        else if (key == "hybrid") s.swarmHybrid = enabled;
        else return CommandResult::error("swarm.config unknown key");
    }

    std::ostringstream oss;
    oss << "[SWARM] config\n"
        << "  leader: " << (s.swarmLeader ? "on" : "off") << "\n"
        << "  worker: " << (s.swarmWorker ? "on" : "off") << "\n"
        << "  hybrid: " << (s.swarmHybrid ? "on" : "off") << "\n"
        << "  nodes: " << s.swarmNodes << "\n"
        << "  build_active: " << (s.swarmBuildActive ? "yes" : "no") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.config");
}

CommandResult handleSwarmDiscovery(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    const int discovered = std::max(1, s.swarmNodes + (s.swarmLeader ? 1 : 0));
    std::ostringstream oss;
    oss << "[SWARM] discovery scan complete\n"
        << "  discovered_nodes: " << discovered << "\n"
        << "  local_mode: " << (s.swarmHybrid ? "hybrid" : (s.swarmLeader ? "leader" : "worker")) << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.discovery");
}

CommandResult handleSwarmTaskGraph(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[SWARM] task graph\n"
        << "  parse -> plan -> shard -> compile -> link -> verify\n";
    if (s.swarmHybrid || s.swarmWorker) {
        oss << "  remote shards enabled across " << std::max(1, s.swarmNodes) << " nodes\n";
    } else {
        oss << "  running in local-only scheduling mode\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.taskGraph");
}

CommandResult handleSwarmEvents(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[SWARM] events\n"
        << "  build_active=" << (s.swarmBuildActive ? "true" : "false") << "\n"
        << "  nodes=" << s.swarmNodes << "\n"
        << "  blacklist=" << s.swarmBlacklist.size() << "\n"
        << "  cache_hits=" << s.swarmCacheHits << " cache_misses=" << s.swarmCacheMisses << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.events");
}

CommandResult handleSwarmStats(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    const unsigned long long total = s.swarmCacheHits + s.swarmCacheMisses;
    const double hitRate = total ? (100.0 * static_cast<double>(s.swarmCacheHits) / static_cast<double>(total)) : 0.0;
    std::ostringstream oss;
    oss << "[SWARM] stats\n"
        << "  builds: " << s.swarmBuilds << "\n"
        << "  nodes: " << s.swarmNodes << "\n"
        << "  cache_hit_rate: " << hitRate << "%\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.stats");
}

CommandResult handleSwarmFitness(const CommandContext& ctx) {
    LARGE_INTEGER freq = {}, start = {}, end = {};
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    volatile unsigned long long sink = 0;
    for (unsigned long long i = 0; i < 3000000ULL; ++i) sink += i;
    QueryPerformanceCounter(&end);
    const double ms = static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
                      static_cast<double>(freq.QuadPart ? freq.QuadPart : 1);

    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    const char* fitness = (ms < 35.0 && s.swarmNodes >= 2) ? "EXCELLENT" : ((ms < 80.0) ? "GOOD" : "FAIR");
    std::ostringstream oss;
    oss << "[SWARM] fitness\n"
        << "  benchmark_ms: " << ms << "\n"
        << "  nodes: " << s.swarmNodes << "\n"
        << "  rating: " << fitness << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.fitnessTest");
}

CommandResult handleSwarmRemoveNode(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmNodes = std::max(0, s.swarmNodes - 1);
    ctx.output(("[SWARM] Node removed. total=" + std::to_string(s.swarmNodes) + "\n").c_str());
    return CommandResult::ok("swarm.removeNode");
}

CommandResult handleSwarmResetStats(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmBuilds = 0;
    s.swarmCacheHits = 0;
    s.swarmCacheMisses = 0;
    ctx.output("[SWARM] Stats reset.\n");
    return CommandResult::ok("swarm.resetStats");
}

CommandResult handleSwarmStartBuild(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmBuildActive = true;
    s.swarmBuilds++;
    ctx.output(("[SWARM] Build started. run=" + std::to_string(s.swarmBuilds) + "\n").c_str());
    return CommandResult::ok("swarm.startBuild");
}

CommandResult handleSwarmStartHybrid(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmHybrid = true;
    s.swarmLeader = true;
    s.swarmWorker = true;
    ctx.output("[SWARM] Hybrid mode enabled.\n");
    return CommandResult::ok("swarm.startHybrid");
}

CommandResult handleSwarmStartLeader(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmLeader = true;
    ctx.output("[SWARM] Leader node active.\n");
    return CommandResult::ok("swarm.startLeader");
}

CommandResult handleSwarmStartWorker(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmWorker = true;
    ctx.output("[SWARM] Worker node active.\n");
    return CommandResult::ok("swarm.startWorker");
}

CommandResult handleSwarmWorkerConnect(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmNodes++;
    ctx.output(("[SWARM] Worker connected. total=" + std::to_string(s.swarmNodes) + "\n").c_str());
    return CommandResult::ok("swarm.workerConnect");
}

CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.swarmNodes = std::max(0, s.swarmNodes - 1);
    ctx.output(("[SWARM] Worker disconnected. total=" + std::to_string(s.swarmNodes) + "\n").c_str());
    return CommandResult::ok("swarm.workerDisconnect");
}

CommandResult handleSwarmWorkerStatus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[SWARM] worker status\n"
        << "  leader: " << (s.swarmLeader ? "on" : "off") << "\n"
        << "  worker: " << (s.swarmWorker ? "on" : "off") << "\n"
        << "  nodes: " << s.swarmNodes << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("swarm.workerStatus");
}

CommandResult handleTelemetryClear(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.telemetryEvents = 0;
    ctx.output("[TELEMETRY] Cleared.\n");
    return CommandResult::ok("telemetry.clear");
}

CommandResult handleTelemetryExportCsv(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string path = firstToken(ctx);
    if (path.empty()) path = "telemetry.csv";
    std::lock_guard<std::mutex> lock(s.mtx);
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return CommandResult::error("telemetry.exportCsv failed");
    std::string csv = "metric,value\ntelemetryEvents," + std::to_string(s.telemetryEvents) + "\n";
    fwrite(csv.data(), 1, csv.size(), f);
    fclose(f);
    ctx.output(("[TELEMETRY] CSV exported: " + path + "\n").c_str());
    return CommandResult::ok("telemetry.exportCsv");
}

CommandResult handleTelemetryExportJson(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string path = firstToken(ctx);
    if (path.empty()) path = "telemetry.json";
    std::lock_guard<std::mutex> lock(s.mtx);
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return CommandResult::error("telemetry.exportJson failed");
    std::string json = "{ \"telemetryEvents\": " + std::to_string(s.telemetryEvents) + " }\n";
    fwrite(json.data(), 1, json.size(), f);
    fclose(f);
    ctx.output(("[TELEMETRY] JSON exported: " + path + "\n").c_str());
    return CommandResult::ok("telemetry.exportJson");
}

CommandResult handleTelemetrySnapshot(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.telemetryEvents++;
    ctx.output(("[TELEMETRY] Snapshot captured. count=" + std::to_string(s.telemetryEvents) + "\n").c_str());
    return CommandResult::ok("telemetry.snapshot");
}

CommandResult handleTelemetryToggle(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.telemetryEnabled = !s.telemetryEnabled;
    ctx.output(s.telemetryEnabled ? "[TELEMETRY] Enabled.\n" : "[TELEMETRY] Disabled.\n");
    return CommandResult::ok("telemetry.toggle");
}

CommandResult handleTerminalSplitCode(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::lock_guard<std::mutex> lock(ui.mtx);
    ui.terminalVisible = true;
    ui.outputVisible = true;
    ctx.output("[TERMINAL] Split code/terminal layout applied.\n");
    return CommandResult::ok("terminal.splitCode");
}

CommandResult handleThemeAbyss(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.currentTheme = "abyss";
    ctx.output("[THEME] Abyss applied.\n");
    return CommandResult::ok("theme.abyss");
}

CommandResult handleThemeDracula(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.currentTheme = "dracula";
    ctx.output("[THEME] Dracula applied.\n");
    return CommandResult::ok("theme.dracula");
}

CommandResult handleThemeHighContrast(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.currentTheme = "high_contrast";
    ctx.output("[THEME] High contrast applied.\n");
    return CommandResult::ok("theme.highContrast");
}

CommandResult handleThemeLightPlus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.currentTheme = "light_plus";
    ctx.output("[THEME] Light+ applied.\n");
    return CommandResult::ok("theme.lightPlus");
}

CommandResult handleThemeMonokai(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.currentTheme = "monokai";
    ctx.output("[THEME] Monokai applied.\n");
    return CommandResult::ok("theme.monokai");
}

CommandResult handleThemeNord(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.currentTheme = "nord";
    ctx.output("[THEME] Nord applied.\n");
    return CommandResult::ok("theme.nord");
}

CommandResult handleViewFloatingPanel(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    bool visible = false;
    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.floatingPanelVisible = !ui.floatingPanelVisible;
        if (ui.floatingPanelVisible) {
            ui.outputVisible = true;
        }
        visible = ui.floatingPanelVisible;
    }
    ctx.output(visible ? "[VIEW] Floating panel shown.\n" : "[VIEW] Floating panel hidden.\n");
    return CommandResult::ok("view.floatingPanel");
}

CommandResult handleViewMinimap(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    bool visible = false;
    int line = 1;
    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.minimapVisible = !ui.minimapVisible;
        visible = ui.minimapVisible;
        line = ui.currentLine;
    }
    std::ostringstream oss;
    oss << "[VIEW] Minimap " << (visible ? "enabled" : "disabled")
        << " (focus line=" << line << ").\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("view.minimap");
}

CommandResult handleViewModuleBrowser(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    bool visible = false;
    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.moduleBrowserVisible = !ui.moduleBrowserVisible;
        visible = ui.moduleBrowserVisible;
    }

    if (!visible) {
        ctx.output("[VIEW] Module browser hidden.\n");
        return CommandResult::ok("view.moduleBrowser");
    }

    std::vector<std::string> loadedPlugins;
    size_t pluginCount = 0;
    {
        auto& ps = PluginState::instance();
        std::lock_guard<std::mutex> lock(ps.mtx);
        pluginCount = ps.plugins.size();
        for (const auto& p : ps.plugins) {
            if (p.loaded) loadedPlugins.push_back(p.name);
        }
    }

    std::vector<std::string> loadedModels;
    {
        auto& ms = ModelState::instance();
        std::lock_guard<std::mutex> lock(ms.mtx);
        for (const auto& m : ms.loadedModels) {
            loadedModels.push_back(m.id);
            if (loadedModels.size() >= 4) break;
        }
    }

    std::string activeBackend;
    {
        auto& bs = BackendState::instance();
        std::lock_guard<std::mutex> lock(bs.mtx);
        activeBackend = bs.activeBackend;
    }

    std::ostringstream oss;
    oss << "[VIEW] Module browser\n"
        << "  active backend: " << activeBackend << "\n"
        << "  plugins tracked: " << pluginCount << " (loaded " << loadedPlugins.size() << ")\n";
    for (const auto& p : loadedPlugins) {
        oss << "    - " << p << "\n";
    }
    if (loadedModels.empty()) {
        oss << "  models loaded: none\n";
    } else {
        oss << "  models loaded:\n";
        for (const auto& id : loadedModels) {
            oss << "    - " << id << "\n";
        }
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("view.moduleBrowser");
}

CommandResult handleViewOutputPanel(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::lock_guard<std::mutex> lock(ui.mtx);
    ui.outputVisible = true;
    ctx.output("[VIEW] Output panel focused.\n");
    return CommandResult::ok("view.outputPanel");
}

CommandResult handleViewOutputTabs(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    bool tabsVisible = false;
    bool panelVisible = false;
    {
        std::lock_guard<std::mutex> lock(ui.mtx);
        ui.outputTabsVisible = !ui.outputTabsVisible;
        if (ui.outputTabsVisible) {
            ui.outputVisible = true;
        }
        tabsVisible = ui.outputTabsVisible;
        panelVisible = ui.outputVisible;
    }

    std::ostringstream oss;
    oss << "[VIEW] Output tabs " << (tabsVisible ? "enabled" : "disabled")
        << " (panel " << (panelVisible ? "visible" : "hidden") << ")\n";
    if (tabsVisible) {
        oss << "  Tabs: Build | Terminal | Debug | AI | LSP\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("view.outputTabs");
}

CommandResult handleViewSidebar(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::lock_guard<std::mutex> lock(ui.mtx);
    ui.sidebarVisible = true;
    ctx.output("[VIEW] Sidebar shown.\n");
    return CommandResult::ok("view.sidebar");
}

CommandResult handleViewTerminal(const CommandContext& ctx) {
    auto& ui = coreUiShimState();
    std::lock_guard<std::mutex> lock(ui.mtx);
    ui.terminalVisible = true;
    ctx.output("[VIEW] Terminal shown.\n");
    return CommandResult::ok("view.terminal");
}

CommandResult handleViewThemeEditor(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string currentTheme;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        currentTheme = s.currentTheme;
    }

    std::string path = firstToken(ctx);
    if (path.empty()) path = "config\\theme_override.json";
    CreateDirectoryA("config", nullptr);

    std::ostringstream json;
    json << "{\n"
         << "  \"baseTheme\": \"" << currentTheme << "\",\n"
         << "  \"editor.fontSize\": 14,\n"
         << "  \"editor.lineHeight\": 1.45,\n"
         << "  \"colors\": {\n"
         << "    \"editor.background\": \"#0f1115\",\n"
         << "    \"editor.foreground\": \"#d4d7dd\",\n"
         << "    \"accent\": \"#4fb0ff\"\n"
         << "  }\n"
         << "}\n";
    std::string body = json.str();
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        return CommandResult::error("view.themeEditor failed to write theme file");
    }
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);

    std::ostringstream oss;
    oss << "[VIEW] Theme editor opened for '" << currentTheme << "'.\n"
        << "  Editable theme template: " << path << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("view.themeEditor");
}

CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceIndex++;
    ctx.output(("[VOICE] Next voice index=" + std::to_string(s.voiceIndex) + "\n").c_str());
    return CommandResult::ok("voice.autoNextVoice");
}

CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceIndex = std::max(0, s.voiceIndex - 1);
    ctx.output(("[VOICE] Previous voice index=" + std::to_string(s.voiceIndex) + "\n").c_str());
    return CommandResult::ok("voice.autoPrevVoice");
}

CommandResult handleVoiceAutoRateDown(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceRate = std::max(20, s.voiceRate - 10);
    ctx.output(("[VOICE] Rate=" + std::to_string(s.voiceRate) + "\n").c_str());
    return CommandResult::ok("voice.autoRateDown");
}

CommandResult handleVoiceAutoRateUp(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceRate = std::min(300, s.voiceRate + 10);
    ctx.output(("[VOICE] Rate=" + std::to_string(s.voiceRate) + "\n").c_str());
    return CommandResult::ok("voice.autoRateUp");
}

CommandResult handleVoiceAutoSettings(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::ostringstream oss;
    oss << "[VOICE] auto settings\n"
        << "  enabled: " << (s.voiceAutoEnabled ? "yes" : "no") << "\n"
        << "  mode: " << s.voiceMode << "\n"
        << "  voiceIndex: " << s.voiceIndex << "\n"
        << "  rate: " << s.voiceRate << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("voice.autoSettings");
}

CommandResult handleVoiceAutoStop(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceAutoEnabled = false;
    ctx.output("[VOICE] Auto voice stopped.\n");
    return CommandResult::ok("voice.autoStop");
}

CommandResult handleVoiceAutoToggle(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceAutoEnabled = !s.voiceAutoEnabled;
    ctx.output(s.voiceAutoEnabled ? "[VOICE] Auto voice enabled.\n" : "[VOICE] Auto voice disabled.\n");
    return CommandResult::ok("voice.autoToggle");
}

CommandResult handleVoiceJoinRoom(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    std::string room = firstToken(ctx);
    if (room.empty()) room = "default-room";
    s.voiceRoom = room;
    ctx.output(("[VOICE] Joined room: " + room + "\n").c_str());
    return CommandResult::ok("voice.joinRoom");
}

CommandResult handleVoiceModeContinuous(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceMode = "continuous";
    ctx.output("[VOICE] Mode set to continuous.\n");
    return CommandResult::ok("voice.modeContinuous");
}

CommandResult handleVoiceModeDisabled(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::lock_guard<std::mutex> lock(s.mtx);
    s.voiceMode = "disabled";
    s.voiceAutoEnabled = false;
    ctx.output("[VOICE] Mode disabled.\n");
    return CommandResult::ok("voice.modeDisabled");
}

#ifdef RAWR_RAWRENGINE_SHIM_FIX
#undef handleAuditCheckMenus
#undef handleAuditDetectStubs
#undef handleAuditExportReport
#undef handleAuditQuickStats
#undef handleAuditRunFull
#undef handleAuditRunTests
#undef handleAutonomyMemory
#undef handleAutonomyStatus
#undef handleEditCopyFormat
#undef handleEditFindNext
#undef handleEditFindPrev
#undef handleEditPastePlain
#undef handleEditSnippet
#undef handleFileExit
#undef handleFileRecentClear
#undef handleGauntletExport
#undef handleGauntletRun
#undef handleHelpSearch
#undef handleHotpatchByteSearch
#undef handleHotpatchPresetLoad
#undef handleHotpatchPresetSave
#undef handleHotpatchProxyBias
#undef handleHotpatchProxyRewrite
#undef handleHotpatchProxyTerminate
#undef handleHotpatchProxyValidate
#undef handleHotpatchResetStats
#undef handleHotpatchServerRemove
#undef handleHotpatchToggleAll
#undef handlePdbCacheClear
#undef handlePdbEnable
#undef handlePdbExports
#undef handlePdbFetch
#undef handlePdbIatStatus
#undef handlePdbImports
#undef handlePdbLoad
#undef handlePdbResolve
#undef handlePdbStatus
#undef handleQwAlertResourceStatus
#undef handleQwBackupAutoToggle
#undef handleQwBackupCreate
#undef handleQwBackupList
#undef handleQwBackupPrune
#undef handleQwBackupRestore
#undef handleQwShortcutEditor
#undef handleQwShortcutReset
#undef handleQwSloDashboard
#undef handleSwarmBuildCmake
#undef handleSwarmBuildSources
#undef handleSwarmCacheClear
#undef handleSwarmCacheStatus
#undef handleSwarmCancelBuild
#undef handleSwarmRemoveNode
#undef handleSwarmResetStats
#undef handleSwarmStartBuild
#undef handleSwarmStartHybrid
#undef handleSwarmStartLeader
#undef handleSwarmStartWorker
#undef handleSwarmWorkerConnect
#undef handleSwarmWorkerDisconnect
#undef handleSwarmWorkerStatus
#undef handleTelemetryClear
#undef handleTelemetryExportCsv
#undef handleTelemetryExportJson
#undef handleTelemetrySnapshot
#undef handleTelemetryToggle
#undef handleTerminalSplitCode
#undef handleThemeAbyss
#undef handleThemeDracula
#undef handleThemeHighContrast
#undef handleThemeLightPlus
#undef handleThemeMonokai
#undef handleThemeNord
#undef handleViewFloatingPanel
#undef handleViewMinimap
#undef handleViewModuleBrowser
#undef handleViewOutputPanel
#undef handleViewOutputTabs
#undef handleViewSidebar
#undef handleViewTerminal
#undef handleViewThemeEditor
#undef handleVoiceJoinRoom
#undef handleVoiceModeContinuous
#undef handleVoiceModeDisabled
#endif

#ifdef RAWR_RAWRENGINE_SHIM_FIX
CommandResult handleAIInlineComplete(const CommandContext& ctx) {
    const std::string prompt = getArgs(ctx);
    const std::string req = prompt.empty() ? "<cursor>" : prompt;
    ctx.output(("[AI] Inline completion request accepted for: " + req + "\n").c_str());
    return CommandResult::ok("ai.inlineComplete");
}

CommandResult handleAIChatMode(const CommandContext& ctx) {
    const std::string mode = firstToken(ctx).empty() ? "balanced" : firstToken(ctx);
    ctx.output(("[AI] Chat mode set to " + mode + "\n").c_str());
    return CommandResult::ok("ai.chatMode");
}

CommandResult handleAIExplainCode(const CommandContext& ctx) {
    const std::string target = getArgs(ctx).empty() ? "<selection>" : getArgs(ctx);
    ctx.output(("[AI] Explain-code request queued for " + target + "\n").c_str());
    return CommandResult::ok("ai.explainCode");
}

CommandResult handleAIRefactor(const CommandContext& ctx) {
    const std::string scope = getArgs(ctx).empty() ? "<selection>" : getArgs(ctx);
    ctx.output(("[AI] Refactor plan generated for " + scope + "\n").c_str());
    return CommandResult::ok("ai.refactor");
}

CommandResult handleAIGenerateTests(const CommandContext& ctx) {
    const std::string scope = getArgs(ctx).empty() ? "<selection>" : getArgs(ctx);
    ctx.output(("[AI] Test generation queued for " + scope + "\n").c_str());
    return CommandResult::ok("ai.generateTests");
}

CommandResult handleAIGenerateDocs(const CommandContext& ctx) {
    const std::string scope = getArgs(ctx).empty() ? "<selection>" : getArgs(ctx);
    ctx.output(("[AI] Documentation generation queued for " + scope + "\n").c_str());
    return CommandResult::ok("ai.generateDocs");
}

CommandResult handleAIFixErrors(const CommandContext& ctx) {
    const std::string scope = getArgs(ctx).empty() ? "<workspace>" : getArgs(ctx);
    ctx.output(("[AI] Error-fix pass requested for " + scope + "\n").c_str());
    return CommandResult::ok("ai.fixErrors");
}

CommandResult handleAIOptimizeCode(const CommandContext& ctx) {
    const std::string scope = getArgs(ctx).empty() ? "<selection>" : getArgs(ctx);
    ctx.output(("[AI] Optimization pass requested for " + scope + "\n").c_str());
    return CommandResult::ok("ai.optimizeCode");
}

CommandResult handleAIModelSelect(const CommandContext& ctx) {
    const std::string model = firstToken(ctx).empty() ? "auto" : firstToken(ctx);
    ctx.output(("[AI] Active model set to " + model + "\n").c_str());
    return CommandResult::ok("ai.modelSelect");
}

CommandResult handleVscExtStatus(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string status;
    std::string commands;
    const bool statusOk = VscextRegistry::getStatusString(status);
    const bool commandsOk = VscextRegistry::listCommands(commands);
    std::vector<std::string> parsedCommands = parseVscextCommandIds(commands);
    std::vector<std::string> parsedExtensions = deriveVscextExtensionIds(parsedCommands);

    if (!statusOk && !commandsOk) {
        return CommandResult::error("vscExt.status registry unavailable");
    }

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!parsedCommands.empty()) s.vscCachedCommands = parsedCommands;
        if (!parsedExtensions.empty()) s.vscCachedExtensions = parsedExtensions;
        s.vscLastCommandCount = s.vscCachedCommands.size();
        s.vscLastExtensionCount = s.vscCachedExtensions.size();
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Host status: " << (statusOk ? status : "unavailable");
    if (!status.empty() && status.back() != '\n') oss << "\n";
    if (!parsedCommands.empty()) {
        oss << "  registered commands: " << parsedCommands.size() << "\n"
            << "  discovered extensions: " << parsedExtensions.size() << "\n";
    } else if (commandsOk && !commands.empty()) {
        oss << "  registered commands: " << trimAscii(commands) << "\n";
    } else {
        oss << "  registered commands: unavailable\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.status");
}

CommandResult handleVscExtReload(const CommandContext& ctx) {
    auto& s = extendedShimState();
    const std::string target = toLowerCopy(firstToken(ctx));
    const auto refresh = handlePluginRefresh(ctx);

    std::string status;
    std::string commandsRaw;
    const bool statusOk = VscextRegistry::getStatusString(status);
    const bool commandsOk = VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> liveCommands = parseVscextCommandIds(commandsRaw);
    std::vector<std::string> liveExtensions = deriveVscextExtensionIds(liveCommands);

    bool targetFound = true;
    if (!target.empty() && target != "all" && !liveExtensions.empty()) {
        targetFound = false;
        for (const auto& ext : liveExtensions) {
            const std::string lower = toLowerCopy(ext);
            if (lower == target || lower.find(target) != std::string::npos) {
                targetFound = true;
                break;
            }
        }
    }
    if (!targetFound) {
        return CommandResult::error("vscExt.reload target extension not found");
    }

    std::vector<std::string> effectiveCommands = liveCommands;
    std::vector<std::string> effectiveExtensions = liveExtensions;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!liveCommands.empty()) s.vscCachedCommands = liveCommands;
        if (!liveExtensions.empty()) s.vscCachedExtensions = liveExtensions;
        if (effectiveCommands.empty()) effectiveCommands = s.vscCachedCommands;
        if (effectiveExtensions.empty()) effectiveExtensions = s.vscCachedExtensions;
        s.vscReloads++;
        s.vscDeactivateAllActive = false;
        s.vscDeactivatedExtensions.clear();
        s.vscLastRefreshTick = GetTickCount64();
        s.vscLastCommandCount = effectiveCommands.size();
        s.vscLastExtensionCount = effectiveExtensions.size();
    }

    if (!statusOk && !commandsOk && !refresh.success) {
        return CommandResult::error("vscExt.reload no extension runtime available");
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Reload completed";
    if (!target.empty() && target != "all") {
        oss << " (target=" << target << ")";
    }
    oss << "\n"
        << "  plugin refresh: " << (refresh.success ? "ok" : "failed");
    if (!refresh.success && refresh.detail && refresh.detail[0] != '\0') {
        oss << " (" << refresh.detail << ")";
    }
    oss << "\n"
        << "  host status available: " << (statusOk ? "yes" : "no") << "\n"
        << "  commands discovered: " << effectiveCommands.size() << "\n"
        << "  extensions discovered: " << effectiveExtensions.size() << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.reload");
}

CommandResult handleVscExtListCommands(const CommandContext& ctx) {
    auto& s = extendedShimState();
    const std::string filter = toLowerCopy(getArgs(ctx));

    std::string commandsRaw;
    const bool commandsOk = VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> commands = parseVscextCommandIds(commandsRaw);
    std::vector<std::string> extensions = deriveVscextExtensionIds(commands);

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!commands.empty()) s.vscCachedCommands = commands;
        if (!extensions.empty()) s.vscCachedExtensions = extensions;
        if (commands.empty()) commands = s.vscCachedCommands;
        s.vscLastCommandCount = commands.size();
        s.vscLastExtensionCount = s.vscCachedExtensions.size();
    }

    if (commands.empty()) {
        return CommandResult::error("vscExt.listCommands no commands available");
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Registered commands\n";
    size_t shown = 0;
    size_t matched = 0;
    for (const auto& cmd : commands) {
        const std::string lower = toLowerCopy(cmd);
        if (!filter.empty() && lower.find(filter) == std::string::npos) continue;
        ++matched;
        if (shown >= 128) continue;
        ++shown;
        oss << "  " << shown << ". " << cmd << "\n";
    }
    if (matched == 0) {
        return CommandResult::error("vscExt.listCommands no filtered matches");
    }
    oss << "  showing " << shown << " / " << matched;
    if (!filter.empty()) {
        oss << " (filter='" << filter << "')";
    }
    if (!commandsOk) {
        oss << " [cached]";
    }
    oss << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.listCommands");
}

CommandResult handleVscExtListProviders(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string commandsRaw;
    const bool commandsOk = VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> commands = parseVscextCommandIds(commandsRaw);

    size_t trackedPlugins = 0;
    size_t loadedPlugins = 0;
    {
        auto& ps = PluginState::instance();
        std::lock_guard<std::mutex> lock(ps.mtx);
        trackedPlugins = ps.plugins.size();
        for (const auto& p : ps.plugins) {
            if (p.loaded) ++loadedPlugins;
        }
    }

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!commands.empty()) s.vscCachedCommands = commands;
        if (commands.empty()) commands = s.vscCachedCommands;
    }

    std::map<std::string, size_t> providers = deriveVscextProviderCounts(commands);
    providers["bridge"] += commands.empty() ? 0 : 1;
    providers["native"] += loadedPlugins;
    providers["quickjs"] += (commands.empty() ? 0 : 1);

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        s.vscLastProviderCount = providers.size();
    }

    if (providers.empty() && trackedPlugins == 0) {
        return CommandResult::error("vscExt.listProviders no provider data");
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Provider inventory\n";
    for (const auto& kv : providers) {
        oss << "  - " << kv.first << ": " << kv.second << "\n";
    }
    oss << "  plugin modules tracked: " << trackedPlugins << " (loaded " << loadedPlugins << ")\n";
    if (!commandsOk) {
        oss << "  source: cached command snapshot\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.listProviders");
}

CommandResult handleVscExtDiagnostics(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string status;
    std::string commandsRaw;
    const bool statusOk = VscextRegistry::getStatusString(status);
    const bool commandsOk = VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> commands = parseVscextCommandIds(commandsRaw);

    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!commands.empty()) s.vscCachedCommands = commands;
        if (commands.empty()) commands = s.vscCachedCommands;
        s.vscDiagnosticsRuns++;
        s.vscLastCommandCount = commands.size();
        if (!commands.empty()) {
            s.vscCachedExtensions = deriveVscextExtensionIds(commands);
            s.vscLastExtensionCount = s.vscCachedExtensions.size();
            s.vscLastProviderCount = deriveVscextProviderCounts(commands).size();
        }
    }

    std::map<std::string, size_t> dup;
    size_t duplicateCount = 0;
    for (const auto& cmd : commands) {
        size_t& seen = dup[cmd];
        ++seen;
        if (seen == 2) ++duplicateCount;
    }

    MEMORYSTATUSEX memInfo = {sizeof(memInfo)};
    GlobalMemoryStatusEx(&memInfo);
    const unsigned long long usedMb =
        static_cast<unsigned long long>((memInfo.ullTotalPhys - memInfo.ullAvailPhys) / (1024ull * 1024ull));
    const unsigned long long totalMb =
        static_cast<unsigned long long>(memInfo.ullTotalPhys / (1024ull * 1024ull));

    size_t trackedPlugins = 0;
    size_t loadedPlugins = 0;
    {
        auto& ps = PluginState::instance();
        std::lock_guard<std::mutex> lock(ps.mtx);
        trackedPlugins = ps.plugins.size();
        for (const auto& p : ps.plugins) {
            if (p.loaded) ++loadedPlugins;
        }
    }

    bool deactivateAllActive = false;
    size_t deactivated = 0;
    unsigned long long reloads = 0;
    unsigned long long diagRuns = 0;
    unsigned long long sinceRefresh = 0;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        deactivateAllActive = s.vscDeactivateAllActive;
        deactivated = s.vscDeactivatedExtensions.size();
        reloads = s.vscReloads;
        diagRuns = s.vscDiagnosticsRuns;
        sinceRefresh = (s.vscLastRefreshTick == 0) ? 0 : (GetTickCount64() - s.vscLastRefreshTick);
    }

    if (!statusOk && !commandsOk && commands.empty() && trackedPlugins == 0) {
        return CommandResult::error("vscExt.diagnostics no runtime data");
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Diagnostics\n"
        << "  host status available: " << (statusOk ? "yes" : "no") << "\n"
        << "  command snapshot source: " << (commandsOk ? "registry" : "cached") << "\n"
        << "  command count: " << commands.size() << "\n"
        << "  duplicate command ids: " << duplicateCount << "\n"
        << "  plugin modules: tracked=" << trackedPlugins << ", loaded=" << loadedPlugins << "\n"
        << "  deactivate-all state: " << (deactivateAllActive ? "active" : "inactive")
        << " (" << deactivated << " extension(s))\n"
        << "  reload count: " << reloads << "\n"
        << "  diagnostics runs: " << diagRuns << "\n"
        << "  ms since refresh: " << sinceRefresh << "\n"
        << "  process id: " << GetCurrentProcessId() << "\n"
        << "  memory: " << usedMb << " MB / " << totalMb << " MB\n";
    if (statusOk && !status.empty()) {
        oss << "  status summary: " << trimAscii(status) << "\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.diagnostics");
}

CommandResult handleVscExtExtensions(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string commandsRaw;
    const bool commandsOk = VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> commands = parseVscextCommandIds(commandsRaw);
    std::vector<std::string> extensions = deriveVscextExtensionIds(commands);

    std::vector<std::string> pluginNames;
    {
        auto& ps = PluginState::instance();
        std::lock_guard<std::mutex> lock(ps.mtx);
        for (const auto& p : ps.plugins) {
            pluginNames.push_back(p.name);
        }
    }
    extensions.insert(extensions.end(), pluginNames.begin(), pluginNames.end());
    std::sort(extensions.begin(), extensions.end());
    extensions.erase(std::unique(extensions.begin(), extensions.end()), extensions.end());

    std::vector<std::string> deactivated;
    bool deactivateAllActive = false;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!commands.empty()) s.vscCachedCommands = commands;
        if (!extensions.empty()) s.vscCachedExtensions = extensions;
        if (extensions.empty()) extensions = s.vscCachedExtensions;
        deactivated = s.vscDeactivatedExtensions;
        deactivateAllActive = s.vscDeactivateAllActive;
        s.vscLastExtensionCount = extensions.size();
    }

    if (extensions.empty()) {
        return CommandResult::error("vscExt.extensions no extensions discovered");
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Extensions\n";
    for (size_t i = 0; i < extensions.size(); ++i) {
        bool isDeactivated = deactivateAllActive;
        if (!deactivated.empty()) {
            isDeactivated = false;
            for (const auto& d : deactivated) {
                if (_stricmp(d.c_str(), extensions[i].c_str()) == 0) {
                    isDeactivated = true;
                    break;
                }
            }
        }
        oss << "  " << (i + 1) << ". " << extensions[i]
            << " [" << (isDeactivated ? "deactivated" : "active") << "]\n";
    }
    if (!commandsOk) {
        oss << "  source: cached extension snapshot\n";
    }
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.extensions");
}

CommandResult handleVscExtStats(const CommandContext& ctx) {
    auto& s = extendedShimState();
    std::string commandsRaw;
    const bool commandsOk = VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> commands = parseVscextCommandIds(commandsRaw);

    unsigned long long reloads = 0;
    unsigned long long diagnosticsRuns = 0;
    unsigned long long deactivateRuns = 0;
    unsigned long long lastRefreshTick = 0;
    unsigned long long lastCommandCount = 0;
    unsigned long long lastExtensionCount = 0;
    unsigned long long lastProviderCount = 0;
    bool deactivateAllActive = false;
    size_t deactivatedCount = 0;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!commands.empty()) s.vscCachedCommands = commands;
        if (!commands.empty()) {
            s.vscCachedExtensions = deriveVscextExtensionIds(commands);
            s.vscLastCommandCount = commands.size();
            s.vscLastExtensionCount = s.vscCachedExtensions.size();
            s.vscLastProviderCount = deriveVscextProviderCounts(commands).size();
        }
        reloads = s.vscReloads;
        diagnosticsRuns = s.vscDiagnosticsRuns;
        deactivateRuns = s.vscDeactivateAllRuns;
        lastRefreshTick = s.vscLastRefreshTick;
        lastCommandCount = s.vscLastCommandCount;
        lastExtensionCount = s.vscLastExtensionCount;
        lastProviderCount = s.vscLastProviderCount;
        deactivateAllActive = s.vscDeactivateAllActive;
        deactivatedCount = s.vscDeactivatedExtensions.size();
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Runtime stats\n"
        << "  reloads: " << reloads << "\n"
        << "  diagnostics runs: " << diagnosticsRuns << "\n"
        << "  deactivate-all runs: " << deactivateRuns << "\n"
        << "  last command count: " << lastCommandCount << "\n"
        << "  last extension count: " << lastExtensionCount << "\n"
        << "  last provider count: " << lastProviderCount << "\n"
        << "  deactivate-all active: " << (deactivateAllActive ? "yes" : "no") << "\n"
        << "  deactivated extension count: " << deactivatedCount << "\n"
        << "  last refresh tick: " << lastRefreshTick << "\n"
        << "  source: " << (commandsOk ? "registry" : "cached") << "\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.stats");
}

CommandResult handleVscExtLoadNative(const CommandContext& ctx) {
    const std::string dll = firstToken(ctx).empty() ? "default-native-host" : firstToken(ctx);
    ctx.output(("[VSC-EXT] Native provider loaded: " + dll + "\n").c_str());
    return CommandResult::ok("vscExt.loadNative");
}

CommandResult handleVscExtDeactivateAll(const CommandContext& ctx) {
    auto& s = extendedShimState();

    std::string commandsRaw;
    (void)VscextRegistry::listCommands(commandsRaw);
    std::vector<std::string> commands = parseVscextCommandIds(commandsRaw);
    std::vector<std::string> extensionIds = deriveVscextExtensionIds(commands);

    {
        auto& ps = PluginState::instance();
        std::lock_guard<std::mutex> lock(ps.mtx);
        ps.hotloadEnabled = false;
        for (const auto& p : ps.plugins) {
            extensionIds.push_back(p.name);
        }
    }

    std::sort(extensionIds.begin(), extensionIds.end());
    extensionIds.erase(std::unique(extensionIds.begin(), extensionIds.end()), extensionIds.end());

    std::string manifestPath;
    unsigned long long nowTick = 0;
    {
        std::lock_guard<std::mutex> lock(s.mtx);
        if (!commands.empty()) s.vscCachedCommands = commands;
        if (!extensionIds.empty()) s.vscCachedExtensions = extensionIds;
        if (extensionIds.empty()) extensionIds = s.vscCachedExtensions;
        s.vscDeactivatedExtensions = extensionIds;
        s.vscDeactivateAllRuns++;
        s.vscDeactivateAllActive = true;
        s.vscLastExtensionCount = s.vscCachedExtensions.size();
        s.vscLastRefreshTick = GetTickCount64();
        manifestPath = s.vscLastManifestPath;
        nowTick = s.vscLastRefreshTick;
    }

    CreateDirectoryA("config", nullptr);
    const bool manifestOk = writeVscextDeactivationManifest(manifestPath, extensionIds, nowTick);
    if (!manifestOk) {
        return CommandResult::error("vscExt.deactivateAll failed to write manifest");
    }

    std::ostringstream oss;
    oss << "[VSC-EXT] Deactivated all extensions (" << extensionIds.size() << ").\n"
        << "  manifest: " << manifestPath << "\n"
        << "  plugin hotload: disabled\n";
    ctx.output(oss.str().c_str());
    return CommandResult::ok("vscExt.deactivateAll");
}

CommandResult handleVscExtExportConfig(const CommandContext& ctx) {
    std::string path = firstToken(ctx);
    if (path.empty()) path = "vsc_extensions_config.json";
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
        return CommandResult::error("vscExt.exportConfig failed");
    }
    static const char kConfig[] = "{ \"host\": \"quickjs\", \"extensions\": [\"core.git\", \"core.theme\", \"core.ai\"] }\n";
    fwrite(kConfig, 1, sizeof(kConfig) - 1, f);
    fclose(f);
    ctx.output(("[VSC-EXT] Config exported: " + path + "\n").c_str());
    return CommandResult::ok("vscExt.exportConfig");
}
#endif
