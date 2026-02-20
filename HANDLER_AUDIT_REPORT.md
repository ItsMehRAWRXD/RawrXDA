# RawrXD Handler Audit Report
## Fake "Print-Text" Handlers Needing Real Subsystem Wiring

**Generated from:** 14 source files (6 implementations + 8 headers)  
**Total COMMAND_TABLE entries:** ~320 unique handler functions (508 registry rows, minus legacy aliases sharing handlers)

---

## SECTION A: Complete List of ALL Fake/Stub Handler Functions

Handlers are classified into **3 tiers**:

### TIER 1: Pure Print-Text Stubs (feature_handlers.cpp) — 114 handlers
These only call `ctx.output("...")` with no real subsystem interaction.
**These are the PRIMARY targets for wiring.**

| # | Handler Function | File | Line | Category | CLI Alias |
|---|-----------------|------|------|----------|-----------|
| 1 | `handleFileNew` | feature_handlers.cpp | 21 | File | `!new` |
| 2 | `handleFileOpen` | feature_handlers.cpp | 33 | File | `!open` |
| 3 | `handleFileSave` | feature_handlers.cpp | 47 | File | `!save` |
| 4 | `handleFileSaveAs` | feature_handlers.cpp | 55 | File | `!save_as` |
| 5 | `handleFileSaveAll` | feature_handlers.cpp | 67 | File | `!save_all` |
| 6 | `handleFileClose` | feature_handlers.cpp | 72 | File | `!close` |
| 7 | `handleFileLoadModel` | feature_handlers.cpp | 77 | File | `!model_load` |
| 8 | `handleFileModelFromHF` | feature_handlers.cpp | 86 | File | `!model_hf` |
| 9 | `handleFileModelFromOllama` | feature_handlers.cpp | 95 | File | `!model_ollama` |
| 10 | `handleFileModelFromURL` | feature_handlers.cpp | 104 | File | `!model_url` |
| 11 | `handleFileUnifiedLoad` | feature_handlers.cpp | 113 | File | `!model_unified` |
| 12 | `handleFileQuickLoad` | feature_handlers.cpp | 118 | File | `!quick_load` |
| 13 | `handleFileRecentFiles` | feature_handlers.cpp | 123 | File | `!recent` |
| 14 | `handleEditUndo` | feature_handlers.cpp | 132 | Edit | `!undo` |
| 15 | `handleEditRedo` | feature_handlers.cpp | 137 | Edit | `!redo` |
| 16 | `handleEditCut` | feature_handlers.cpp | 142 | Edit | `!cut` |
| 17 | `handleEditCopy` | feature_handlers.cpp | 147 | Edit | `!copy` |
| 18 | `handleEditPaste` | feature_handlers.cpp | 152 | Edit | `!paste` |
| 19 | `handleEditFind` | feature_handlers.cpp | 157 | Edit | `!find` |
| 20 | `handleEditReplace` | feature_handlers.cpp | 165 | Edit | `!replace` |
| 21 | `handleEditSelectAll` | feature_handlers.cpp | 170 | Edit | `!select_all` |
| 22 | `handleAgentExecute` | feature_handlers.cpp | 179 | Agent | `!agent_execute` |
| 23 | `handleAgentLoop` | feature_handlers.cpp | 190 | Agent | `!agent_loop` |
| 24 | `handleAgentBoundedLoop` | feature_handlers.cpp | 195 | Agent | `!agent_bounded` |
| 25 | `handleAgentStop` | feature_handlers.cpp | 200 | Agent | `!agent_stop` |
| 26 | `handleAgentGoal` | feature_handlers.cpp | 205 | Agent | — |
| 27 | `handleAgentMemory` | feature_handlers.cpp | 214 | Agent | `!agent_memory` |
| 28 | `handleAgentMemoryView` | feature_handlers.cpp | 219 | Agent | `!agent_memory_view` |
| 29 | `handleAgentMemoryClear` | feature_handlers.cpp | 224 | Agent | `!agent_memory_clear` |
| 30 | `handleAgentMemoryExport` | feature_handlers.cpp | 229 | Agent | `!agent_memory_export` |
| 31 | `handleAgentConfigure` | feature_handlers.cpp | 234 | Agent | `!agent_config` |
| 32 | `handleAgentViewTools` | feature_handlers.cpp | 239 | Agent | `!tools` |
| 33 | `handleAgentViewStatus` | feature_handlers.cpp | 251 | Agent | `!agent_status` |
| 34 | `handleAutonomyStart` | feature_handlers.cpp | 260 | Autonomy | `!autonomy_start` |
| 35 | `handleAutonomyStop` | feature_handlers.cpp | 265 | Autonomy | `!autonomy_stop` |
| 36 | `handleAutonomyGoal` | feature_handlers.cpp | 270 | Autonomy | `!autonomy_goal` |
| 37 | `handleAutonomyRate` | feature_handlers.cpp | 280 | Autonomy | `!autonomy_rate` |
| 38 | `handleAutonomyRun` | feature_handlers.cpp | 285 | Autonomy | `!autonomy_run` |
| 39 | `handleAutonomyToggle` | feature_handlers.cpp | 290 | Autonomy | `!autonomy_toggle` |
| 40 | `handleSubAgentChain` | feature_handlers.cpp | 299 | SubAgent | `!chain` |
| 41 | `handleSubAgentSwarm` | feature_handlers.cpp | 309 | SubAgent | `!swarm` |
| 42 | `handleSubAgentTodoList` | feature_handlers.cpp | 314 | SubAgent | `!todo` |
| 43 | `handleSubAgentTodoClear` | feature_handlers.cpp | 319 | SubAgent | `!todo_clear` |
| 44 | `handleSubAgentStatus` | feature_handlers.cpp | 324 | SubAgent | `!agents` |
| 45 | `handleTerminalNew` | feature_handlers.cpp | 333 | Terminal | — |
| 46 | `handleTerminalSplitH` | feature_handlers.cpp | 338 | Terminal | `!terminal_split` |
| 47 | `handleTerminalSplitV` | feature_handlers.cpp | 343 | Terminal | `!terminal_split_v` |
| 48 | `handleTerminalKill` | feature_handlers.cpp | 348 | Terminal | `!terminal_kill` |
| 49 | `handleTerminalList` | feature_handlers.cpp | 353 | Terminal | `!terminal_list` |
| 50 | `handleDebugStart` | feature_handlers.cpp | 362 | Debug | — |
| 51 | `handleDebugStop` | feature_handlers.cpp | 367 | Debug | — |
| 52 | `handleDebugStep` | feature_handlers.cpp | 372 | Debug | — |
| 53 | `handleDebugContinue` | feature_handlers.cpp | 377 | Debug | — |
| 54 | `handleBreakpointAdd` | feature_handlers.cpp | 382 | Debug | — |
| 55 | `handleBreakpointList` | feature_handlers.cpp | 392 | Debug | — |
| 56 | `handleBreakpointRemove` | feature_handlers.cpp | 397 | Debug | — |
| 57 | `handleHotpatchApply` | feature_handlers.cpp | 406 | Hotpatch | `!hotpatch_apply` |
| 58 | `handleHotpatchCreate` | feature_handlers.cpp | 415 | Hotpatch | `!hotpatch_create` |
| 59 | `handleHotpatchStatus` | feature_handlers.cpp | 420 | Hotpatch | `!hotpatch_status` |
| 60 | `handleHotpatchMemory` | feature_handlers.cpp | 425 | Hotpatch | `!hotpatch_mem` |
| 61 | `handleHotpatchByte` | feature_handlers.cpp | 430 | Hotpatch | `!hotpatch_byte` |
| 62 | `handleHotpatchServer` | feature_handlers.cpp | 435 | Hotpatch | `!hotpatch_server` |
| 63 | `handleAIModeSet` | feature_handlers.cpp | 444 | AIMode | `!mode` |
| 64 | `handleAIEngineSelect` | feature_handlers.cpp | 453 | AIMode | `!engine` |
| 65 | `handleAIDeepThinking` | feature_handlers.cpp | 462 | AIMode | `!deep` |
| 66 | `handleAIDeepResearch` | feature_handlers.cpp | 467 | AIMode | `!research` |
| 67 | `handleAIMaxMode` | feature_handlers.cpp | 472 | AIMode | `!max` |
| 68 | `handleREDecisionTree` | feature_handlers.cpp | 481 | ReverseEng | `!decision_tree` |
| 69 | `handleRESSALift` | feature_handlers.cpp | 486 | ReverseEng | `!ssa_lift` |
| 70 | `handleREAutoPatch` | feature_handlers.cpp | 491 | ReverseEng | `!auto_patch` |
| 71 | `handleREDisassemble` | feature_handlers.cpp | 496 | ReverseEng | `!disasm` |
| 72 | `handleREDumpbin` | feature_handlers.cpp | 506 | ReverseEng | `!dumpbin` |
| 73 | `handleRECFGAnalysis` | feature_handlers.cpp | 511 | ReverseEng | `!cfg` |
| 74 | `handleVoiceInit` | feature_handlers.cpp | 520 | Voice | `!voice init` |
| 75 | `handleVoiceRecord` | feature_handlers.cpp | 525 | Voice | `!voice record` |
| 76 | `handleVoiceTranscribe` | feature_handlers.cpp | 530 | Voice | `!voice transcribe` |
| 77 | `handleVoiceSpeak` | feature_handlers.cpp | 535 | Voice | `!voice speak` |
| 78 | `handleVoiceDevices` | feature_handlers.cpp | 544 | Voice | `!voice devices` |
| 79 | `handleVoiceMode` | feature_handlers.cpp | 549 | Voice | `!voice mode` |
| 80 | `handleVoiceStatus` | feature_handlers.cpp | 554 | Voice | `!voice status` |
| 81 | `handleVoiceMetrics` | feature_handlers.cpp | 559 | Voice | `!voice metrics` |
| 82 | `handleSafety` | feature_handlers.cpp | 568 | Headless | — |
| 83 | `handleConfidence` | feature_handlers.cpp | 573 | Headless | — |
| 84 | `handleReplay` | feature_handlers.cpp | 578 | Headless | — |
| 85 | `handleGovernor` | feature_handlers.cpp | 583 | Headless | — |
| 86 | `handleMultiResponse` | feature_handlers.cpp | 588 | Headless | — |
| 87 | `handleHistory` | feature_handlers.cpp | 593 | Headless | — |
| 88 | `handleExplain` | feature_handlers.cpp | 598 | Headless | — |
| 89 | `handlePolicy` | feature_handlers.cpp | 603 | Headless | — |
| 90 | `handleTools` | feature_handlers.cpp | 608 | Headless | — |
| 91 | `handleServerStart` | feature_handlers.cpp | 617 | Server | `!server start` |
| 92 | `handleServerStop` | feature_handlers.cpp | 622 | Server | `!server stop` |
| 93 | `handleServerStatus` | feature_handlers.cpp | 627 | Server | `!server status` |
| 94 | `handleGitStatus` | feature_handlers.cpp | 636 | Git | `!git_status` |
| 95 | `handleGitCommit` | feature_handlers.cpp | 641 | Git | `!git_commit` |
| 96 | `handleGitPush` | feature_handlers.cpp | 651 | Git | `!git_push` |
| 97 | `handleGitPull` | feature_handlers.cpp | 656 | Git | `!git_pull` |
| 98 | `handleGitDiff` | feature_handlers.cpp | 661 | Git | `!git_diff` |
| 99 | `handleThemeSet` | feature_handlers.cpp | 670 | Theme | `!theme dark+` |
| 100 | `handleThemeList` | feature_handlers.cpp | 682 | Theme | `!theme_list` |
| 101 | `handleBackendList` | feature_handlers.cpp | 691 | Backend | — |
| 102 | `handleBackendSelect` | feature_handlers.cpp | 698 | Backend | — |
| 103 | `handleBackendStatus` | feature_handlers.cpp | 707 | Backend | — |
| 104 | `handleSwarmJoin` | feature_handlers.cpp | 716 | Swarm | `!swarm_join` |
| 105 | `handleSwarmStatus` | feature_handlers.cpp | 721 | Swarm | `!swarm_status` |
| 106 | `handleSwarmDistribute` | feature_handlers.cpp | 726 | Swarm | — |
| 107 | `handleSwarmRebalance` | feature_handlers.cpp | 731 | Swarm | — |
| 108 | `handleSwarmNodes` | feature_handlers.cpp | 736 | Swarm | `!swarm_nodes` |
| 109 | `handleSwarmLeave` | feature_handlers.cpp | 741 | Swarm | `!swarm_leave` |
| 110 | `handleSettingsOpen` | feature_handlers.cpp | 750 | Settings | `!settings` |
| 111 | `handleSettingsExport` | feature_handlers.cpp | 755 | Settings | `!settings_export` |
| 112 | `handleSettingsImport` | feature_handlers.cpp | 760 | Settings | `!settings_import` |
| 113 | `handleHelpAbout` | feature_handlers.cpp | 769 | Help | `!about` |
| 114 | `handleHelpDocs` | feature_handlers.cpp | 777 | Help | `!docs` |

**Note:** ~8 additional CLI-only handlers in feature_handlers.cpp (handleSearch L803, handleAnalyze L808, handleProfile L813, handleSubAgent L818, handleCOT L823, handleStatus L828, handleHelp L835, handleGenerateIDE L848) are also print-text stubs but have no direct subsystem target.

---

### TIER 2: delegateToGui() Stubs — 153 handlers across 2 files

These use `PostMessageA(hwnd, WM_COMMAND, cmdId, 0)` in GUI mode, or print `"[SSOT] X invoked via CLI"` in CLI mode.
**In CLI mode, these are effectively fake.** In GUI mode, they route to Win32IDE's WM_COMMAND handler but the underlying Win32IDE handler may itself be a stub.

#### ssot_handlers.cpp (138 handlers)

| # | Handler Function | Line | Category |
|---|-----------------|------|----------|
| 1 | `handleFileRecentClear` | 47 | File |
| 2 | `handleFileExit` | 51 | File |
| 3 | `handleEditSnippet` | 66 | Edit |
| 4 | `handleEditCopyFormat` | 70 | Edit |
| 5 | `handleEditPastePlain` | 74 | Edit |
| 6 | `handleEditClipboardHist` | 78 | Edit |
| 7 | `handleEditFindNext` | 82 | Edit |
| 8 | `handleEditFindPrev` | 86 | Edit |
| 9 | `handleViewMinimap` | 93 | View |
| 10 | `handleViewOutputTabs` | 94 | View |
| 11 | `handleViewModuleBrowser` | 95 | View |
| 12 | `handleViewThemeEditor` | 96 | View |
| 13 | `handleViewFloatingPanel` | 97 | View |
| 14 | `handleViewOutputPanel` | 98 | View |
| 15 | `handleViewStreamingLoader` | 99 | View |
| 16 | `handleViewVulkanRenderer` | 100 | View |
| 17 | `handleViewSidebar` | 101 | View |
| 18 | `handleViewTerminal` | 102 | View |
| 19 | `handleThemeLightPlus` | 112 | Theme |
| 20 | `handleThemeMonokai` | 113 | Theme |
| 21 | `handleThemeDracula` | 114 | Theme |
| 22 | `handleThemeNord` | 115 | Theme |
| 23 | `handleThemeSolDark` | 116 | Theme |
| 24 | `handleThemeSolLight` | 117 | Theme |
| 25 | `handleThemeCyberpunk` | 118 | Theme |
| 26 | `handleThemeGruvbox` | 119 | Theme |
| 27 | `handleThemeCatppuccin` | 120 | Theme |
| 28 | `handleThemeTokyo` | 121 | Theme |
| 29 | `handleThemeCrimson` | 122 | Theme |
| 30 | `handleThemeHighContrast` | 123 | Theme |
| 31 | `handleThemeOneDark` | 124 | Theme |
| 32 | `handleThemeSynthwave` | 125 | Theme |
| 33 | `handleThemeAbyss` | 126 | Theme |
| 34 | `handleTrans100` | 132 | Transparency |
| 35 | `handleTrans90` | 133 | Transparency |
| 36 | `handleTrans80` | 134 | Transparency |
| 37 | `handleTrans70` | 135 | Transparency |
| 38 | `handleTrans60` | 136 | Transparency |
| 39 | `handleTrans50` | 137 | Transparency |
| 40 | `handleTrans40` | 138 | Transparency |
| 41 | `handleTransCustom` | 139 | Transparency |
| 42 | `handleTransToggle` | 140 | Transparency |
| 43 | `handleHelpCmdRef` | 146 | Help |
| 44 | `handleHelpPsDocs` | 147 | Help |
| 45 | `handleHelpSearch` | 148 | Help |
| 46 | `handleTerminalSplitCode` | 154 | Terminal |
| 47 | `handleAutonomyStatus` | 160 | Autonomy |
| 48 | `handleAutonomyMemory` | 161 | Autonomy |
| 49 | `handleAINoRefusal` | 167 | AIMode |
| 50 | `handleAICtx4K` | 168 | AIContext |
| 51 | `handleAICtx32K` | 169 | AIContext |
| 52 | `handleAICtx64K` | 170 | AIContext |
| 53 | `handleAICtx128K` | 171 | AIContext |
| 54 | `handleAICtx256K` | 172 | AIContext |
| 55 | `handleAICtx512K` | 173 | AIContext |
| 56 | `handleAICtx1M` | 174 | AIContext |
| 57 | `handleRECompile` | 180 | ReverseEng |
| 58 | `handleRECompare` | 181 | ReverseEng |
| 59 | `handleREDetectVulns` | 182 | ReverseEng |
| 60 | `handleREExportIDA` | 183 | ReverseEng |
| 61 | `handleREExportGhidra` | 184 | ReverseEng |
| 62 | `handleREFunctions` | 185 | ReverseEng |
| 63 | `handleREDemangle` | 186 | ReverseEng |
| 64 | `handleRERecursiveDisasm` | 187 | ReverseEng |
| 65 | `handleRETypeRecovery` | 188 | ReverseEng |
| 66 | `handleREDataFlow` | 189 | ReverseEng |
| 67 | `handleRELicenseInfo` | 190 | ReverseEng |
| 68 | `handleREDecompilerView` | 191 | ReverseEng |
| 69 | `handleREDecompRename` | 192 | ReverseEng |
| 70 | `handleREDecompSync` | 193 | ReverseEng |
| 71 | `handleREDecompClose` | 194 | ReverseEng |
| 72 | `handleSwarmStartLeader` | 200 | Swarm |
| 73 | `handleSwarmStartWorker` | 201 | Swarm |
| 74 | `handleSwarmStartHybrid` | 202 | Swarm |
| 75 | `handleSwarmRemoveNode` | 203 | Swarm |
| 76 | `handleSwarmBlacklist` | 204 | Swarm |
| 77 | `handleSwarmBuildSources` | 205 | Swarm |
| 78 | `handleSwarmBuildCmake` | 206 | Swarm |
| 79 | `handleSwarmStartBuild` | 207 | Swarm |
| 80 | `handleSwarmCancelBuild` | 208 | Swarm |
| 81 | `handleSwarmCacheStatus` | 209 | Swarm |
| 82 | `handleSwarmCacheClear` | 210 | Swarm |
| 83 | `handleSwarmConfig` | 211 | Swarm |
| 84 | `handleSwarmDiscovery` | 212 | Swarm |
| 85 | `handleSwarmTaskGraph` | 213 | Swarm |
| 86 | `handleSwarmEvents` | 214 | Swarm |
| 87 | `handleSwarmStats` | 215 | Swarm |
| 88 | `handleSwarmResetStats` | 216 | Swarm |
| 89 | `handleSwarmWorkerStatus` | 217 | Swarm |
| 90 | `handleSwarmWorkerConnect` | 218 | Swarm |
| 91 | `handleSwarmWorkerDisconnect` | 219 | Swarm |
| 92 | `handleSwarmFitness` | 220 | Swarm |
| 93 | `handleHotpatchMemRevert` | 226 | Hotpatch |
| 94 | `handleHotpatchByteSearch` | 227 | Hotpatch |
| 95 | `handleHotpatchServerRemove` | 228 | Hotpatch |
| 96 | `handleHotpatchProxyBias` | 229 | Hotpatch |
| 97 | `handleHotpatchProxyRewrite` | 230 | Hotpatch |
| 98 | `handleHotpatchProxyTerminate` | 231 | Hotpatch |
| 99 | `handleHotpatchProxyValidate` | 232 | Hotpatch |
| 100 | `handleHotpatchPresetSave` | 233 | Hotpatch |
| 101 | `handleHotpatchPresetLoad` | 234 | Hotpatch |
| 102 | `handleHotpatchEventLog` | 235 | Hotpatch |
| 103 | `handleHotpatchResetStats` | 236 | Hotpatch |
| 104 | `handleHotpatchToggleAll` | 237 | Hotpatch |
| 105 | `handleHotpatchProxyStats` | 238 | Hotpatch |
| 106 | `handleMonacoToggle` | 244 | Monaco |
| 107 | `handleMonacoDevtools` | 245 | Monaco |
| 108 | `handleMonacoReload` | 246 | Monaco |
| 109 | `handleMonacoZoomIn` | 247 | Monaco |
| 110 | `handleMonacoZoomOut` | 248 | Monaco |
| 111 | `handleMonacoSyncTheme` | 249 | Monaco |
| 112 | `handleLspSrvStart` | 255 | LSPServer |
| 113 | `handleLspSrvStop` | 256 | LSPServer |
| 114 | `handleLspSrvStatus` | 257 | LSPServer |
| 115 | `handleLspSrvReindex` | 258 | LSPServer |
| 116 | `handleLspSrvStats` | 259 | LSPServer |
| 117 | `handleLspSrvPublishDiag` | 260 | LSPServer |
| 118 | `handleLspSrvConfig` | 261 | LSPServer |
| 119 | `handleLspSrvExportSymbols` | 262 | LSPServer |
| 120 | `handleLspSrvLaunchStdio` | 263 | LSPServer |
| 121 | `handleEditorRichEdit` | 269 | Editor |
| 122 | `handleEditorWebView2` | 270 | Editor |
| 123 | `handleEditorMonacoCore` | 271 | Editor |
| 124 | `handleEditorCycle` | 272 | Editor |
| 125 | `handleEditorStatus` | 273 | Editor |
| 126 | `handlePdbLoad` | 279 | PDB |
| 127 | `handlePdbFetch` | 280 | PDB |
| 128 | `handlePdbStatus` | 281 | PDB |
| 129 | `handlePdbCacheClear` | 282 | PDB |
| 130 | `handlePdbEnable` | 283 | PDB |
| 131 | `handlePdbResolve` | 284 | PDB |
| 132 | `handlePdbImports` | 285 | PDB |
| 133 | `handlePdbExports` | 286 | PDB |
| 134 | `handlePdbIatStatus` | 287 | PDB |
| 135 | `handleAuditDashboard`–`handleAuditQuickStats` | 293-299 | Audit (7) |
| 136 | `handleGauntletRun`, `handleGauntletExport` | 305-306 | Gauntlet (2) |
| 137 | `handleVoicePTT` | 312 | Voice |
| 138 | `handleVoiceJoinRoom` | 313 | Voice |
| 139 | `handleVoiceModeContinuous` | 314 | Voice |
| 140 | `handleVoiceModeDisabled` | 315 | Voice |
| 141 | `handleQwShortcutEditor`–`handleQwSloDashboard` | 321-332 | QW (12) |
| 142 | `handleTelemetryToggle`–`handleTelemetrySnapshot` | 338-343 | Telemetry (6) |
| 143 | `handleTier1SmoothScrollToggle`–`handleTier1UpdateDismiss` | 356-366 | Tier1 Cosmetic (15) |

#### ssot_handlers_ext.cpp (47 handlers)

| # | Handler Function | Line | Category |
|---|-----------------|------|----------|
| 1 | `handleFileAutoSave` | 45 | File |
| 2 | `handleFileCloseFolder` | 46 | File |
| 3 | `handleFileOpenFolder` | 47 | File |
| 4 | `handleFileNewWindow` | 48 | File |
| 5 | `handleFileCloseTab` | 49 | File |
| 6 | `handleEditMulticursorAdd` | 55 | Edit |
| 7 | `handleEditMulticursorRemove` | 56 | Edit |
| 8 | `handleEditGotoLine` | 57 | Edit |
| 9 | `handleViewToggleSidebar` | 63 | View |
| 10 | `handleViewToggleTerminal` | 64 | View |
| 11 | `handleViewToggleOutput` | 65 | View |
| 12 | `handleViewToggleFullscreen` | 66 | View |
| 13 | `handleViewZoomIn` | 67 | View |
| 14 | `handleViewZoomOut` | 68 | View |
| 15 | `handleViewZoomReset` | 69 | View |
| 16 | `handleAIInlineComplete` | 75 | AIMode |
| 17 | `handleAIChatMode` | 76 | AIMode |
| 18 | `handleAIExplainCode` | 77 | AIMode |
| 19 | `handleAIRefactor` | 78 | AIMode |
| 20 | `handleAIGenerateTests` | 79 | AIMode |
| 21 | `handleAIGenerateDocs` | 80 | AIMode |
| 22 | `handleAIFixErrors` | 81 | AIMode |
| 23 | `handleAIOptimizeCode` | 82 | AIMode |
| 24 | `handleAIModelSelect` | 83 | AIMode |
| 25 | `handleToolsCommandPalette` | 89 | Tools |
| 26 | `handleToolsSettings` | 90 | Tools |
| 27 | `handleToolsExtensions` | 91 | Tools |
| 28 | `handleToolsTerminal` | 92 | Tools |
| 29 | `handleToolsBuild` | 93 | Tools |
| 30 | `handleToolsDebug` | 94 | Tools |
| 31 | `handleDecompRenameVar` | 103 | Decompiler |
| 32 | `handleDecompGotoDef` | 104 | Decompiler |
| 33 | `handleDecompFindRefs` | 105 | Decompiler |
| 34 | `handleDecompCopyLine` | 106 | Decompiler |
| 35 | `handleDecompCopyAll` | 107 | Decompiler |
| 36 | `handleDecompGotoAddr` | 108 | Decompiler |
| 37 | `handleVscExtStatus`–`handleVscExtExportConfig` | 114-123 | VscExt (10) |
| 38 | `handleVoiceAutoToggle`–`handleVoiceAutoStop` | 129-135 | VoiceAuto (7) |

---

### TIER 3: Print-Text Stubs in missing_handler_stubs.cpp (21 handlers, end of file)

These are at the END of missing_handler_stubs.cpp, after the real implementations. They print tagged messages but don't call real subsystems.

| # | Handler Function | Line (approx) | Category |
|---|-----------------|---------------|----------|
| 1 | `handleUnrealInit` | ~2750 | GameEngine |
| 2 | `handleUnrealAttach` | ~2760 | GameEngine |
| 3 | `handleUnityInit` | ~2770 | GameEngine |
| 4 | `handleUnityAttach` | ~2780 | GameEngine |
| 5 | `handleRevengDisassemble` | ~2795 | ReverseEng |
| 6 | `handleRevengDecompile` | ~2805 | ReverseEng |
| 7 | `handleRevengFindVulnerabilities` | ~2815 | ReverseEng |
| 8 | `handleModelList` | ~2830 | Models |
| 9 | `handleModelLoad` | ~2840 | Models |
| 10 | `handleModelQuantize` | ~2852 | Models |
| 11 | `handleModelFinetune` | ~2858 | Models |
| 12 | `handleModelUnload` | ~2870 | Models |
| 13 | `handleDiskListDrives` | ~2885 | Disk |
| 14 | `handleDiskScanPartitions` | ~2890 | Disk |
| 15 | `handleGovernorStatus` | ~2900 | Governor |
| 16 | `handleGovernorSetPowerLevel` | ~2912 | Governor |
| 17 | `handleMarketplaceList` | ~2920 | Marketplace |
| 18 | `handleMarketplaceInstall` | ~2925 | Marketplace |
| 19 | `handleEmbeddingEncode` | ~2936 | Embedding |
| 20 | `handleVisionAnalyzeImage` | ~2944 | Vision |
| 21 | `handlePromptClassifyContext` | ~2952 | Prompt |

---

### SUMMARY COUNTS

| Tier | Description | Count | File(s) |
|------|------------|-------|---------|
| **Tier 1** | Pure `ctx.output()` print stubs | **~122** | feature_handlers.cpp |
| **Tier 2** | `delegateToGui()` stubs (fake in CLI mode) | **~153** | ssot_handlers.cpp, ssot_handlers_ext.cpp |
| **Tier 3** | Print stubs in missing_handler_stubs.cpp | **~21** | missing_handler_stubs.cpp (end section) |
| **TOTAL FAKE** | **All stubs needing real wiring** | **~296** | |

**Already-real implementations (missing_handler_stubs.cpp, first ~2700 lines): ~132 handlers**

---

## SECTION B: Real Subsystem API Functions Available

### 1. SubsystemRegistry (rawrxd_subsystem_api.hpp)

```cpp
class SubsystemRegistry {
    static SubsystemRegistry& instance();
    SubsystemResult invoke(SubsystemId id, const SubsystemParams& params);
    SubsystemResult invokeBySwitch(const char* switchName, const SubsystemParams& params);
    bool isAvailable(SubsystemId id) const;
    SubsystemStats getStats(SubsystemId id) const;
};
```

**23 SubsystemId modes:**
| ID | Name | Purpose |
|----|------|---------|
| 0 | Compile | C/C++ compilation |
| 1 | Encrypt | Data encryption |
| 2 | Inject | DLL/code injection |
| 3 | UACBypass | Privilege escalation |
| 4 | Persist | Persistence mechanisms |
| 5 | Sideload | DLL sideloading |
| 6 | AVScan | Antivirus scanning |
| 7 | Entropy | Entropy analysis |
| 8 | StubGen | Stub generation |
| 9 | Trace | Execution tracing |
| 10 | Agent | Agent execution |
| 11 | BBCov | Basic block coverage |
| 12 | CovFusion | Coverage fusion |
| 13 | DynTrace | Dynamic tracing |
| 14 | AgentTrace | Agent tracing |
| 15 | GapFuzz | Gap fuzzing |
| 16 | IntelPT | Intel PT tracing |
| 17 | DiffCov | Differential coverage |
| 18 | AnalyzerDistiller | Analysis distillation |
| 19 | StreamingOrchestrator | Streaming orchestration |
| 20 | VulkanKernel | Vulkan compute |
| 21 | DiskRecovery | Disk recovery |
| 22 | LSPDiagnostics | LSP diagnostics |

**Convenience macros:**
```cpp
RAWRXD_INVOKE(id, params)           // Returns SubsystemResult
RAWRXD_INVOKE_WITH(id, field, val)  // Sets one param field
```

### 2. SubsystemAgentBridge (subsystem_agent_bridge.hpp)

```cpp
class SubsystemAgentBridge {
    static SubsystemAgentBridge& instance();
    SubsystemResult executeAction(const char* actionName, const SubsystemParams& params,
                                   int maxRetries = 2);
    std::vector<Capability> enumerateCapabilities() const;
};
```

**22 capability entries** with `requiresArgs`, `requiresElevation`, `selfContained` flags.

### 3. AgentOllamaClient (used in missing_handler_stubs.cpp real handlers)

```cpp
class AgentOllamaClient {
    static AgentOllamaClient& instance();
    std::string ChatSync(const std::string& model, const std::string& prompt);
    bool isConnected() const;
    std::vector<std::string> listModels();
    bool switchBackend(const char* name);
    std::string getActiveBackend() const;
    bool testConnection(const char* endpoint);
    void routeToBackend(const char* prompt, const char* model);
};
```

### 4. NativeDebuggerEngine (used in missing_handler_stubs.cpp real handlers)

```cpp
class NativeDebuggerEngine {
    static NativeDebuggerEngine& instance();
    bool launch(const char* path, const char* args);
    bool attach(DWORD pid);
    bool detach();
    bool go();  // Continue
    bool stepOver();
    bool stepInto();
    bool stepOut();
    bool breakAll();
    bool kill();
    bool addBreakpoint(const char* location);
    bool removeBreakpoint(const char* location);
    bool enableBreakpoint(const char* location);
    void clearAllBreakpoints();
    std::vector<Breakpoint> listBreakpoints();
    std::vector<Register> getRegisters();
    std::vector<StackFrame> getCallStack();
    std::string readMemory(uintptr_t addr, size_t size);
    std::string disassembleAt(uintptr_t addr, int count);
    std::vector<Module> getModules();
    std::vector<Thread> getThreads();
    bool switchThread(DWORD tid);
    std::string evaluate(const char* expr);
    bool setRegister(const char* name, uint64_t value);
    std::string searchMemory(uintptr_t start, size_t len, const void* pattern, size_t patLen);
    bool isActive() const;
};
```

### 5. MultiResponseEngine (used in missing_handler_stubs.cpp real handlers)

```cpp
class MultiResponseEngine {
    static MultiResponseEngine& instance();
    void generate(const char* prompt, int maxResponses);
    void setMaxResponses(int n);
    int selectPreferred(int index);
    std::string compare();
    std::string getStats();
    std::vector<Template> getTemplates();
    void toggleTemplate(const char* name);
    std::string getPreferences();
    std::string getLatest();
    std::string getStatus();
    void clearHistory();
    void applyPreferred();
};
```

### 6. ReplayJournal (used in missing_handler_stubs.cpp real handlers)

```cpp
class ReplayJournal {
    static ReplayJournal& instance();
    std::string getStatus();
    std::string getLastEntry();
    void exportSession(const char* path);
    void checkpoint(const char* label);
    void record(const char* action, const char* data);
};
```

### 7. GovernorState (used in missing_handler_stubs.cpp real handlers)

```cpp
struct GovernorState {
    static GovernorState& instance();
    std::vector<Task> tasks;
    std::atomic<int> completed;
    void submit(Task t);
    void killAll();
    std::vector<Task> taskList();
};
```

### 8. Win32 APIs (used directly)

- `LoadLibraryA` / `FreeLibrary` — Plugin loading
- `GetProcAddress` — Plugin function resolution  
- `VirtualProtect` — Memory hotpatching
- `PostMessageA` — GUI command routing
- `CreateFileMapping` / `MapViewOfFile` — Byte-level hotpatching

---

## SECTION C: Handler → Subsystem API Mapping

This maps each category of fake handlers to the subsystem API that should implement the real logic.

| Handler Category | Fake Handler Count | Target Subsystem API | Notes |
|------------------|-------------------|---------------------|-------|
| **File Ops** (new/open/save/close) | 15 | Win32 `CreateFile`/`WriteFile`/editor buffer API | Basic file I/O, model loading needs GGUF parser |
| **Edit Ops** (undo/redo/cut/copy/paste) | 14 | Editor buffer / undo stack API | Need editor engine integration |
| **Agent** (loop/execute/memory) | 12 | `AgentOllamaClient::ChatSync()`, `SubsystemAgentBridge::executeAction()` | All agent ops → AgentOllamaClient |
| **Autonomy** | 6 | `SubsystemAgentBridge` + GovernorState for scheduling | Autonomous decision tree |
| **SubAgent** | 5 | `SubsystemAgentBridge::executeAction("Agent")` | Chain/swarm → multi-agent coordination |
| **Terminal** | 5 | Win32 `CreateProcess` / ConPTY API | Terminal management |
| **Debug** (legacy stubs) | 7 | `NativeDebuggerEngine` | Real impls already exist in missing_handler_stubs.cpp (28 handlers) — just need COMMAND_TABLE rewiring |
| **Hotpatch** (legacy stubs) | 6 | `model_memory_hotpatch.hpp`, `byte_level_hotpatcher.hpp`, `gguf_server_hotpatch.hpp` | Three-layer hotpatch APIs |
| **AI Mode/Context** | 12 | `AgentOllamaClient::switchBackend()`, context window config | Mode switching + context window resizing |
| **Reverse Eng** (all tiers) | 23 | `RAWRXD_INVOKE(Compile/Trace)`, PE parser, disassembler | ASM semantic handlers already real |
| **Voice** | 15 | Win32 `waveIn`/`waveOut`, SAPI, WebSocket for rooms | Audio I/O stack |
| **Headless Systems** | 9 | `GovernorState`, `ReplayJournal`, `MultiResponseEngine`, safety/confidence state | Real impls exist in missing_handler_stubs.cpp |
| **Server** | 3 | HTTP server (WinHTTP/WinSock), inference routing | `handleServerStart` → bind port + inference loop |
| **Git** | 5 | `CreateProcess` → `git.exe` invocation | Shell out to git CLI |
| **Themes** | 17 | `SetLayeredWindowAttributes`, color config registry | Theme engine API |
| **Transparency** | 9 | `SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)` | Direct Win32 |
| **Swarm** (legacy stubs) | 21 | Network socket layer, mDNS discovery, task distribution | Distributed compute |
| **Backend/Router** | ~3 legacy | `AgentOllamaClient` (real impls already exist for 32 handlers) | Legacy stubs can be removed |
| **LLM Router/Backend** (feature_handlers.cpp) | 3 | `AgentOllamaClient::routeToBackend()` | Policy-based routing |
| **Settings** | 3 | JSON config file read/write | `CreateFile` + JSON serializer |
| **Hotpatch extended** | 13 | `UnifiedHotpatchManager`, `proxy_hotpatcher.hpp` | Three-layer coordination |
| **Monaco** | 6 | WebView2 API / CEF integration | Browser control management |
| **LSP Server** | 9 | JSON-RPC over stdio/TCP, `RAWRXD_INVOKE(LSPDiagnostics)` | LSP protocol implementation |
| **Editor Engine** | 5 | RichEdit control / WebView2 / Monaco switching | Editor backend selection |
| **PDB** | 9 | DbgHelp API (`SymInitialize`, `SymLoadModule64`, etc.) | MS Debug Interface Access |
| **Audit** | 7 | Internal reflection: `g_commandRegistry[]` scan | Self-introspection |
| **Gauntlet** | 2 | Test harness integration | Automated test runner |
| **QW** | 12 | Config persistence, Win32 timer API | Quality/workflow infrastructure |
| **Telemetry** | 6 | Internal counters, JSON/CSV export | Telemetry aggregation |
| **Tier 1 Cosmetics** | 15 | Win32 window APIs, scroll, minimap buffer | UI polish features |
| **IDE Core File** | 5 | Win32 `CreateWindow`, folder picker dialogs | IDE windowing |
| **IDE Core Edit** | 3 | Scintilla/RichEdit multicursor API | Editor features |
| **IDE Core View** | 7 | `ShowWindow`, `SetWindowPos`, `MoveWindow` | Window management |
| **IDE AI Features** | 9 | `AgentOllamaClient::ChatSync()` | AI-powered code features |
| **IDE Tools** | 6 | Command palette, settings dialog, build system | IDE tooling |
| **Decompiler** | 6 | Decompiler engine API (Ghidra/IDA bridge) | Reverse engineering UI |
| **VSCode Ext API** | 10 | Plugin host, extension protocol | Extension system |
| **Voice Automation** | 7 | SAPI `ISpVoice`, rate/voice enumeration | TTS automation |
| **Game Engine** | 4 | `LoadLibraryA("UnrealEditor.dll")`, Unity C# bridge | Engine integration |
| **Model Management** | 5 | GGUF parser, quantization engine, LoRA fine-tuning | Model pipeline |
| **Disk Recovery** | 2 | `RAWRXD_INVOKE(DiskRecovery)`, `GetLogicalDrives()` | Low-level disk I/O |
| **Marketplace** | 2 | HTTP client for extension registry | Extension marketplace |
| **Embeddings** | 1 | `AgentOllamaClient` embeddings endpoint | Vector encoding |
| **Vision** | 1 | `AgentOllamaClient` vision model or VulkanKernel | Image analysis |
| **Prompt Engine** | 1 | Prompt classifier / `AgentOllamaClient` | Context classification |

---

## SECTION D: Exact Fake Implementation Code — First 20 Handlers

### 1. `handleFileNew` (feature_handlers.cpp:21)
```cpp
CommandResult handleFileNew(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] New file created\n");
    } else {
        ctx.output("New file buffer initialized.\n");
    }
    return CommandResult::ok("file.new");
}
```

### 2. `handleFileOpen` (feature_handlers.cpp:33)
```cpp
CommandResult handleFileOpen(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] Open file dialog...\n");
    } else {
        if (ctx.args && ctx.args[0]) {
            std::string msg = "Opening file: " + std::string(ctx.args) + "\n";
            ctx.output(msg.c_str());
        } else {
            ctx.output("Usage: !open <filename>\n");
        }
    }
    return CommandResult::ok("file.open");
}
```

### 3. `handleFileSave` (feature_handlers.cpp:47)
```cpp
CommandResult handleFileSave(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] File saved\n");
    } else {
        ctx.output("File saved.\n");
    }
    return CommandResult::ok("file.save");
}
```

### 4. `handleFileSaveAs` (feature_handlers.cpp:55)
```cpp
CommandResult handleFileSaveAs(const CommandContext& ctx) {
    if (ctx.isGui && ctx.idePtr) {
        ctx.output("[GUI] Save As dialog...\n");
    } else {
        if (ctx.args && ctx.args[0]) {
            std::string msg = "Saved as: " + std::string(ctx.args) + "\n";
            ctx.output(msg.c_str());
        } else {
            ctx.output("Usage: !save_as <filename>\n");
        }
    }
    return CommandResult::ok("file.saveAs");
}
```

### 5. `handleFileSaveAll` (feature_handlers.cpp:67)
```cpp
CommandResult handleFileSaveAll(const CommandContext& ctx) {
    ctx.output("All modified files saved.\n");
    return CommandResult::ok("file.saveAll");
}
```

### 6. `handleFileClose` (feature_handlers.cpp:72)
```cpp
CommandResult handleFileClose(const CommandContext& ctx) {
    ctx.output("File closed.\n");
    return CommandResult::ok("file.close");
}
```

### 7. `handleFileLoadModel` (feature_handlers.cpp:77)
```cpp
CommandResult handleFileLoadModel(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Loading GGUF model: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_load <path-to-gguf>\n");
    }
    return CommandResult::ok("file.loadModel");
}
```

### 8. `handleFileModelFromHF` (feature_handlers.cpp:86)
```cpp
CommandResult handleFileModelFromHF(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Downloading from HuggingFace: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_hf <repo/model>\n");
    }
    return CommandResult::ok("file.modelFromHF");
}
```

### 9. `handleFileModelFromOllama` (feature_handlers.cpp:95)
```cpp
CommandResult handleFileModelFromOllama(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Loading from Ollama: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_ollama <model-name>\n");
    }
    return CommandResult::ok("file.modelFromOllama");
}
```

### 10. `handleFileModelFromURL` (feature_handlers.cpp:104)
```cpp
CommandResult handleFileModelFromURL(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Downloading from URL: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !model_url <url>\n");
    }
    return CommandResult::ok("file.modelFromURL");
}
```

### 11. `handleFileUnifiedLoad` (feature_handlers.cpp:113)
```cpp
CommandResult handleFileUnifiedLoad(const CommandContext& ctx) {
    ctx.output("Unified model loader: auto-detecting source...\n");
    return CommandResult::ok("file.modelUnified");
}
```

### 12. `handleFileQuickLoad` (feature_handlers.cpp:118)
```cpp
CommandResult handleFileQuickLoad(const CommandContext& ctx) {
    ctx.output("Quick-loading from local cache...\n");
    return CommandResult::ok("file.quickLoad");
}
```

### 13. `handleFileRecentFiles` (feature_handlers.cpp:123)
```cpp
CommandResult handleFileRecentFiles(const CommandContext& ctx) {
    ctx.output("Recent files:\n");
    return CommandResult::ok("file.recentFiles");
}
```

### 14. `handleEditUndo` (feature_handlers.cpp:132)
```cpp
CommandResult handleEditUndo(const CommandContext& ctx) {
    ctx.output("Undo.\n");
    return CommandResult::ok("edit.undo");
}
```

### 15. `handleEditRedo` (feature_handlers.cpp:137)
```cpp
CommandResult handleEditRedo(const CommandContext& ctx) {
    ctx.output("Redo.\n");
    return CommandResult::ok("edit.redo");
}
```

### 16. `handleEditCut` (feature_handlers.cpp:142)
```cpp
CommandResult handleEditCut(const CommandContext& ctx) {
    ctx.output("Cut to clipboard.\n");
    return CommandResult::ok("edit.cut");
}
```

### 17. `handleEditCopy` (feature_handlers.cpp:147)
```cpp
CommandResult handleEditCopy(const CommandContext& ctx) {
    ctx.output("Copied to clipboard.\n");
    return CommandResult::ok("edit.copy");
}
```

### 18. `handleEditPaste` (feature_handlers.cpp:152)
```cpp
CommandResult handleEditPaste(const CommandContext& ctx) {
    ctx.output("Pasted from clipboard.\n");
    return CommandResult::ok("edit.paste");
}
```

### 19. `handleEditFind` (feature_handlers.cpp:157)
```cpp
CommandResult handleEditFind(const CommandContext& ctx) {
    if (ctx.args && ctx.args[0]) {
        std::string msg = "Searching for: " + std::string(ctx.args) + "\n";
        ctx.output(msg.c_str());
    } else {
        ctx.output("Usage: !find <text>\n");
    }
    return CommandResult::ok("edit.find");
}
```

### 20. `handleEditReplace` (feature_handlers.cpp:165)
```cpp
CommandResult handleEditReplace(const CommandContext& ctx) {
    ctx.output("Replace mode.\n");
    return CommandResult::ok("edit.replace");
}
```

---

## Key Observations

1. **ID Collisions:** The COMMAND_TABLE has duplicate IDs in several ranges at the bottom:
   - MODEL_LIST (9001) collides with HOTPATCH_STATUS (9001)
   - DISK_LIST_DRIVES (9200) collides with LSPSRV_START (9200)  
   - GOVERNOR_STATUS (9300) collides with EDITOR_RICHEDIT (9300)
   - MARKETPLACE_LIST (9400) collides with PDB_LOAD (9400)
   - EMBEDDING_ENCODE (9500) collides with AUDIT_DASHBOARD (9500)
   - VISION_ANALYZE (9600) collides with GAUNTLET_RUN (9600)
   - PROMPT_CLASSIFY (9700) collides with VOICE_RECORD (9700)
   
   **These collisions must be resolved before wiring — the bottom entries (Game Engine through Prompt Engine) need new unique IDs.**

2. **Duplicate handler functions:** Some functions exist in BOTH feature_handlers.cpp AND ssot_handlers.cpp (e.g., `handleThemeList`, `handleThemeSet`, `handleSwarmStatus`, `handleSwarmJoin`, `handleHotpatchStatus`, `handleHotpatchMemory`, etc.). The linker currently selects one — this needs cleanup (ODR violations).

3. **132 handlers already wired:** In missing_handler_stubs.cpp (first ~2700 lines), these categories have real subsystem implementations: LSP Client (13), ASM Semantic (12), Hybrid LSP-AI (12), Multi-Response (12), Governor (4), Safety (4), Replay (4), Confidence (2), Router Extended (21), Backend Extended (11), Debug Extended (28), Plugin System (9).

4. **Lowest-hanging fruit for wiring:** Agent handlers → `AgentOllamaClient`, Debug legacy stubs → `NativeDebuggerEngine` (already real in missing_handler_stubs.cpp), Hotpatch legacy stubs → `UnifiedHotpatchManager`, Disk Recovery → `RAWRXD_INVOKE(DiskRecovery)`.
