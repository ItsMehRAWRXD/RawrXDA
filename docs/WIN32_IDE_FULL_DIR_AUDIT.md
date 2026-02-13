# Win32 IDE — Full Directory Audit (Missing Logic & Source)

**Date:** 2026-02-12  
**Scope:** `D:\rawrxd\src\win32app` and `Win32IDE.cpp` — build wiring, missing implementations, and source/header consistency.

---

## 1. Executive Summary

| Area | Status | Notes |
|------|--------|--------|
| **Build (CMake)** | ✅ Complete | All production `Win32IDE_*.cpp` and support sources are in `WIN32IDE_SOURCES` (MSVC and MinGW branches). |
| **Core logic** | ✅ Wired | `Win32IDE.cpp`, `Win32IDE_Core.cpp`, `Win32IDE_Commands.cpp`, `Win32IDE_AgentCommands.cpp`, `Win32IDE_FileOps.cpp` implement main flows. |
| **Command dispatch** | ✅ SSOT | `onCommand` → `routeCommandUnified(id, this)`; no separate `handleReverseEngineeringCommand` — RE commands handled inside `handleAgentCommand` → `Win32IDE_ReverseEngineering.cpp`. |
| **Struct helpers** | ✅ Implemented | `AgentEvent::typeString/toJSONL/fromJSONL` in `Win32IDE_AgentHistory.cpp`; `RetryStrategy::typeString`, `FailureIntelligenceRecord::toMetadataJSON` in `Win32IDE_FailureIntelligence.cpp`. |
| **ContextManager** | ✅ Header-only | `ContextManager.h` is self-contained (no `ContextManager.cpp`); correctly not in CMake as a source. |
| **Stub / test-only** | ⚠️ Not in IDE target | `digestion_engine_stub.cpp`, `digestion_test_harness.cpp`, `simple_test.cpp`, `test_runner.cpp` are not part of `RawrXD-Win32IDE`. |

---

## 2. Build Configuration (CMake)

### 2.1 WIN32IDE_SOURCES — MSVC

All of the following are included in the main `RawrXD-Win32IDE` target (MSVC path):

- **Entry & core:** `main_win32.cpp`, `Win32IDE.cpp`, `Win32IDE.h`, `HeadlessIDE.cpp/h`, `IOutputSink.h`, `Win32IDE_Core.cpp`, `Win32IDE_Sidebar.cpp`, `Win32IDE_VSCodeUI.cpp`, `Win32IDE_NativePipeline.cpp`, `Win32IDE_PowerShellPanel.cpp`, `Win32IDE_PowerShell.cpp`, `Win32IDE_Logger.cpp`, `Win32IDE_FileOps.cpp`, `Win32IDE_Commands.cpp`, `Win32IDE_Debugger.cpp`, `Win32IDE_AgenticBridge.cpp/h`, `Win32IDE_AgentCommands.cpp`, `Win32IDE_Autonomy.cpp/h`, `Win32IDE_SyntaxHighlight.cpp`, `Win32IDE_Themes.cpp`, `Win32IDE_Annotations.cpp`, `Win32IDE_Session.cpp`, `Win32IDE_StreamingUX.cpp`, `Win32IDE_ReverseEngineering.cpp`, `Win32IDE_DecompilerView.cpp`, `Win32IDE_FeatureManifest.cpp`, `Win32IDE_GhostText.cpp`, `Win32IDE_OutlinePanel.cpp`, `Win32IDE_RenamePreview.cpp`, `Win32IDE_Tier2Cosmetics.cpp`, `Win32IDE_Tier1Cosmetics.cpp`, `Win32IDE_Breadcrumbs.cpp`, `Win32IDE_PlanExecutor.cpp`, `Win32IDE_FailureDetector.cpp`, `Win32IDE_FailureIntelligence.cpp`, `Win32IDE_Settings.cpp`, `Win32IDE_LocalServer.cpp`, `Win32IDE_BackendSwitcher.cpp`, `Win32IDE_LLMRouter.cpp`, `Win32IDE_LSPClient.cpp`, `Win32IDE_AsmSemantic.cpp`, `Win32IDE_LSP_AI_Bridge.cpp`, `Win32IDE_MultiResponse.cpp`, `Win32IDE_ExecutionGovernor.cpp`, `Win32IDE_SubAgent.cpp/h`, `Win32IDE_AgentHistory.cpp`, `Win32IDE_AgentPanel.cpp`, `Win32IDE_DiskRecovery.cpp`, `Win32TerminalManager.cpp/h`, `TransparentRenderer.cpp/h`, `ModelConnection.h`, `ContextManager.h`, `Win32IDE_Plugins.cpp`, plus config/engine/agentic/core sources.
- **Panels & extras:** `Win32IDE_SwarmPanel.cpp`, `Win32IDE_DualAgentPanel.cpp`, `Win32IDE_NativeDebugPanel.cpp`, `Win32IDE_HotpatchPanel.cpp`, `Win32IDE_VSCodeExtAPI.cpp`, `Win32IDE_WebView2.cpp`, `Win32IDE_MonacoThemes.cpp`, `Win32IDE_LSPServer.cpp`, `Win32IDE_EditorEngine.cpp`, `Win32IDE_PDBSymbols.cpp`, `Win32IDE_AuditDashboard.cpp`, `Win32IDE_Gauntlet.cpp`, `Win32IDE_VoiceChat.cpp`, `Win32IDE_VoiceAutomation.cpp`, `Win32IDE_QuickWins.cpp`, `Win32IDE_Telemetry.cpp`, `Win32IDE_MCP.cpp`, `Win32IDE_FlightRecorder.cpp`, `Win32IDE_Instructions.cpp`, `Win32IDE_GameEnginePanel.cpp`, `Win32IDE_CruciblePanel.cpp`, `Win32IDE_CopilotGapPanel.cpp`, `Win32IDE_PipelinePanel.cpp`, `Win32IDE_HotpatchCtrlPanel.cpp`, `Win32IDE_StaticAnalysisPanel.cpp`, `Win32IDE_SemanticPanel.cpp`, `Win32IDE_TelemetryPanel.cpp`, `Win32IDE_Tier3Polish.cpp`, `Win32IDE_Tier3Cosmetics.cpp`, `Win32IDE_Tier5Cosmetics.cpp`, `Win32IDE_CrashReporter.cpp`, `Win32IDE_ColorPicker.cpp`, `Win32IDE_EmojiSupport.cpp`, `Win32IDE_ShortcutEditor.cpp`, `Win32IDE_TelemetryDashboard.cpp`, `Win32IDE_MarketplacePanel.cpp`, `Win32IDE_TestExplorerTree.cpp`, `Win32IDE_NetworkPanel.cpp`, `Win32IDE_TranscendencePanel.cpp`, `VulkanRenderer.cpp`, `OSExplorerInterceptor.cpp`, `Win32IDE_MCPHooks.cpp`, `TodoManager.cpp`, `IocpFileWatcher.cpp`, `IDEDiagnosticAutoHealer.cpp`, `IDEDiagnosticAutoHealer_Impl.cpp`, `IDEAutoHealerLauncher.cpp`, `ConsentPrompt.cpp`, `AutonomousAgent.cpp`.
- **Optional/parity:** `Win32IDE_CursorParity.cpp`, `Win32IDE_GUILayoutHotpatch.cpp`, `Win32IDE_ProvableAgent.cpp`, `Win32IDE_AIReverseEngineering.cpp`, `Win32IDE_AirgappedEnterprise.cpp`, `Win32IDE_FlagshipFeatures.cpp`.

### 2.2 Include Directories (RawrXD-Win32IDE)

- `src`, `src/engine`, `src/core`, `src/server`, `src/agent`, `src/agentic`, `src/cli`, `src/win32app`, `src/modules`, `src/utils`, `src/config`, `src/asm`, `src/lsp`, `src/plugin_system`, `src/ide`, `src/context`, `src/multimodal_engine`, `src/telemetry`, `src/ui`, `src/git`, `include`.

So:

- `multi_response_engine.h` (in `src/core/`) is found via `#include "multi_response_engine.h"` because `src/core` is in the include path.
- `lsp/RawrXD_LSPServer.h` is in `include/lsp/`; `include` is in the path — OK.
- `IDEConfig.h` is in `src/config/`; `src/config` is in the path — OK.
- `multimodal/vision_encoder.h` and `logging/logger.h` (used in CursorParity / GUILayoutHotpatch) are under `include/` — OK.

### 2.3 Files in `win32app` Not Built as Part of RawrXD-Win32IDE

| File | Reason |
|------|--------|
| `digestion_engine_stub.cpp` | Stub / test harness only. |
| `digestion_test_harness.cpp` | Test harness only. |
| `simple_test.cpp` | Test only. |
| `test_runner.cpp` | Test only. |
| `Win32IDE_AIBackend.cpp.superseded_by_BackendSwitcher` | Superseded; not a build unit. |
| `Win32IDE_FailureDetector.cpp.phase4a_backup` | Backup; not a build unit. |

No production Win32 IDE source file under `win32app` is missing from CMake.

---

## 3. Missing or Stub Logic

### 3.1 Command Routing (No Missing Handlers)

- **Reverse Engineering:** There is no function named `handleReverseEngineeringCommand`. RE menu commands (`IDM_REVENG_*`) are registered in `Win32IDE_Commands.cpp` and dispatched from `handleAgentCommand()` in `Win32IDE_AgentCommands.cpp` (cases 515–591), which delegates to implementations in `Win32IDE_ReverseEngineering.cpp` and `Win32IDE_DecompilerView.cpp`. Logic is present.
- **Plan Executor:** Plan execution is driven by `Win32IDE_PlanExecutor.cpp` and `WM_PLAN_READY`, `WM_PLAN_STEP_DONE`, `WM_PLAN_COMPLETE` in `Win32IDE_Core.cpp` (handleMessage). No separate `handlePlanExecutorCommand` is required; plan commands are routed through the unified command table / `routeCommandUnified`.

### 3.2 Declared vs Implemented (Win32IDE)

- **File ops:** `openFileDialog`, `openRecentFile`, `saveAll`, `closeFile`, `promptSaveChanges` are implemented in `Win32IDE_FileOps.cpp`.
- **Monaco/WebView2:** `createMonacoEditor` (and related) are implemented in `Win32IDE_Commands.cpp`.
- **Transcendence:** `initTranscendence`, `shutdownTranscendence`, `handleTranscendenceCommand`, `handleTranscendenceEndpoint` are in `Win32IDE_TranscendencePanel.cpp`.
- **SubAgent / split viewer:** `splitCodeViewerHorizontal`, `splitCodeViewerVertical`, `isCodeViewerSplit`, `closeSplitCodeViewer` are in `Win32IDE_SubAgent.cpp`.
- **Agent/Memory/RE:** `onAgentStartLoop`, `onAgentStop`, `onAIModeMax`, `loadMemoryPlugin`, `onAgentMemoryStore`, `onSubAgentChain`, etc. are in `Win32IDE_AgentCommands.cpp` or `Win32IDE_SubAgent.cpp`.
- **License/feature:** `checkFeatureLicense`, `syncLicenseWithManifest` are in `Win32IDE_SubAgent.cpp`.
- **RVA parity:** `testFindPatternRVAParity` is in `Win32IDE_SubAgent.cpp`.

No Win32IDE member declared in `Win32IDE.h` was found to be completely missing from the `win32app` tree; implementations are either present or intentionally stubbed (e.g. some PowerShell APIs).

### 3.3 Struct Helper Implementations

- **AgentEvent:** `typeString()`, `toJSONL()`, `fromJSONL()` — in `Win32IDE_AgentHistory.cpp`.
- **RetryStrategy:** `typeString()` — in `Win32IDE_FailureIntelligence.cpp`.
- **FailureIntelligenceRecord:** `toMetadataJSON()` — in `Win32IDE_FailureIntelligence.cpp`.

---

## 4. Source vs Header Consistency

### 4.1 Headers Referenced by Win32IDE / Win32IDE.h

- `Win32IDE.h` pulls in: `Win32TerminalManager.h`, `TransparentRenderer.h`, `gguf_loader.h`, `streaming_gguf_loader.h`, `model_source_resolver.h`, `Win32IDE_AgenticBridge.h`, `Win32IDE_Autonomy.h`, `Win32IDE_SubAgent.h`, `Win32IDE_WebView2.h`, `engine_manager.h`, `codex_ultimate.h`, `game_engine_manager.h`, `crucible_engine.h`, `copilot_gap_closer.h`, `editor_engine.h`, `win32_plugin_loader.h`, `ExtensionLoader.hpp`, `vscode_extension_api.h`, `mcp_integration.h`, `instructions_provider.hpp`, `native_inference_pipeline.hpp`, `tool_action_status.h`, `IocpFileWatcher.h`, `OllamaProvider.h`, `agentic_composer_ux.h`. All exist under `include/` or `src/` with the configured include paths.
- **Cursor parity / GUILayoutHotpatch:** `Win32IDE_CursorParity.cpp` uses `telemetry/telemetry_export.h`, `agentic/agentic_composer_ux.h`, `context/context_mention_parser.h`, `multimodal/vision_encoder.h`, `ide/refactoring_plugin.h`, `ide/language_plugin.h`, `context/semantic_index.h`, `ide/resource_generator.h`, `cursor_github_parity_bridge.h`. Corresponding headers/sources exist under `include/` and `src/` (e.g. `include/cursor_github_parity_bridge.h`, `src/core/cursor_github_parity_bridge.cpp`; `include/telemetry/telemetry_export.h`; `include/agentic/agentic_composer_ux.h`; etc.).

### 4.2 ContextManager

- **ContextManager** is header-only (`ContextManager.h`). No `ContextManager.cpp`; CMake correctly does not add one. No missing source here.

---

## 5. Recommendations

1. **Keep WIN32IDE_SOURCES in sync:** When adding new `Win32IDE_*.cpp` or new panels under `win32app`, add them to both the MSVC and MinGW branches in `CMakeLists.txt` so both builds stay equivalent.
2. **Stub/test files:** If `digestion_engine_stub.cpp`, `digestion_test_harness.cpp`, `simple_test.cpp`, or `test_runner.cpp` are ever needed in the main IDE binary, add them explicitly to a dedicated target or to `WIN32IDE_SOURCES`; otherwise leave them out and document as test/stub-only.
3. **Backup/superseded:** Keep `Win32IDE_AIBackend.cpp.superseded_by_BackendSwitcher` and `Win32IDE_FailureDetector.cpp.phase4a_backup` out of the build; consider moving to a `backup/` or `archive/` folder if you want to avoid clutter in `win32app`.
4. **Optional modules:** Cursor parity, GUILayoutHotpatch, ProvableAgent, AIReverseEngineering, AirgappedEnterprise, FlagshipFeatures are already optional in the build; no change required for “missing” logic — they are intentionally feature modules.

---

## 6. File-Level Checklist (Win32 IDE Core)

| Component | In CMake | Implemented | Notes |
|-----------|----------|-------------|--------|
| Win32IDE.cpp | ✅ | ✅ | Main UI, menu, editor, terminal, model load, chat. |
| Win32IDE_Core.cpp | ✅ | ✅ | createWindow, onCreate, onDestroy, onCommand, onSize, deferredHeavyInit, WM_PLAN_* handling. |
| Win32IDE_Commands.cpp | ✅ | ✅ | handleViewCommand, handleToolsCommand, buildThemeMenu, createMonacoEditor, saveAll, closeFile, etc. |
| Win32IDE_AgentCommands.cpp | ✅ | ✅ | handleAgentCommand, onAgentStartLoop, onAgentStop, RE cases, onAIModeMax, loadMemoryPlugin. |
| Win32IDE_FileOps.cpp | ✅ | ✅ | openFileDialog, openRecentFile, saveAll, closeFile, promptSaveChanges. |
| Win32IDE_ReverseEngineering.cpp | ✅ | ✅ | All IDM_REVENG_* handlers. |
| Win32IDE_PlanExecutor.cpp | ✅ | ✅ | Plan generation/approval/execution, WM_PLAN_* posts. |
| Win32IDE_TranscendencePanel.cpp | ✅ | ✅ | initTranscendence, shutdownTranscendence, handleTranscendenceCommand. |
| Win32IDE_SubAgent.cpp | ✅ | ✅ | SubAgent commands, split code viewer, testFindPatternRVAParity, checkFeatureLicense. |
| Win32IDE_AgentHistory.cpp | ✅ | ✅ | AgentEvent::typeString/toJSONL/fromJSONL, history persistence. |
| Win32IDE_FailureIntelligence.cpp | ✅ | ✅ | RetryStrategy::typeString, FailureIntelligenceRecord::toMetadataJSON. |
| ContextManager.h | Header only | N/A | No .cpp. |
| ModelConnection.h | Header only | N/A | No .cpp in WIN32IDE_SOURCES. |

---

**Audit complete.** No missing production source files for the Win32 IDE; no missing command or struct helper implementations. Optional/stub/test-only files are correctly excluded from the main target.
