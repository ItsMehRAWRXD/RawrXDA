# RawrXD IDE — Comprehensive Capability Audit
## For AI Agent Training & Autonomous Operation

**Codebase Location**: `D:\rawrxd\src\`  
**Architecture**: C++20, Win32 Native (no Qt), MASM64 ASM kernels  
**Build**: CMake 3.20+, MSVC 2022 / Clang  
**Error Pattern**: `PatchResult` structured returns (no exceptions)  
**Threading**: `std::mutex` + `std::lock_guard` + `std::atomic` (no recursive locks)  
**JSON**: nlohmann/json throughout  
**Rendering**: Direct2D  
**HTTP**: WinHTTP streaming  
**Database**: SQLite3 (knowledge graph, digestion engine)  

---

## TABLE OF CONTENTS

1. [Command Surface (SSOT Registry)](#1-command-surface-ssot-registry)
2. [File Operations](#2-file-operations)
3. [Edit Operations](#3-edit-operations)
4. [View & UI](#4-view--ui)
5. [Terminal & Command Execution](#5-terminal--command-execution)
6. [Agent System](#6-agent-system)
7. [Agentic Subsystem (Tool-Calling Loop)](#7-agentic-subsystem-tool-calling-loop)
8. [Autonomy System](#8-autonomy-system)
9. [AI Modes & LLM Integration](#9-ai-modes--llm-integration)
10. [Hotpatching (3-Layer + Extensions)](#10-hotpatching-3-layer--extensions)
11. [LSP (Language Server Protocol)](#11-lsp-language-server-protocol)
12. [Debugging](#12-debugging)
13. [Reverse Engineering & Decompiler](#13-reverse-engineering--decompiler)
14. [Code Intelligence & Analysis](#14-code-intelligence--analysis)
15. [Refactoring](#15-refactoring)
16. [Git Operations](#16-git-operations)
17. [Model & Inference Engine](#17-model--inference-engine)
18. [Swarm / Distributed Inference](#18-swarm--distributed-inference)
19. [Voice & TTS](#19-voice--tts)
20. [Vision / Multi-Modal](#20-vision--multi-modal)
21. [Embeddings & Vector Search](#21-embeddings--vector-search)
22. [Knowledge Graph (Persistent Learning)](#22-knowledge-graph-persistent-learning)
23. [Background Daemon (Proactive Analysis)](#23-background-daemon-proactive-analysis)
24. [Plugin & Extension System](#24-plugin--extension-system)
25. [VS Code Extension API Compatibility](#25-vs-code-extension-api-compatibility)
26. [Safe Refactoring & Transactions](#26-safe-refactoring--transactions)
27. [Failure Detection & Self-Healing](#27-failure-detection--self-healing)
28. [Telemetry & Metrics](#28-telemetry--metrics)
29. [Quality, Backup & Governance](#29-quality-backup--governance)
30. [Agent Tool Schemas (OpenAI Function Calling)](#30-agent-tool-schemas-openai-function-calling)

---

## 1. COMMAND SURFACE (SSOT REGISTRY)

**File**: `src/core/command_registry.hpp` (772 lines)

The entire command surface is defined in a single `COMMAND_TABLE` X-macro. Every GUI menu item, CLI command, and palette entry is generated from this table. There is **no second registry**.

### Command Table Format
```
X(ID, SYMBOL, canonical_name, cli_alias, exposure, category, handler, flags)
```

### CmdExposure Enum
```cpp
enum class CmdExposure : uint8_t {
    GUI_ONLY  = 0x01,
    CLI_ONLY  = 0x02,
    BOTH      = 0x03,
    INTERNAL  = 0x04
};
```

### CmdFlags Enum
```cpp
enum CmdFlags : uint32_t {
    CMD_NONE            = 0x00,
    CMD_ASYNC           = 0x01,   // Runs off main thread
    CMD_REQUIRES_FILE   = 0x02,   // Needs active file
    CMD_REQUIRES_MODEL  = 0x04,   // Needs GGUF model
    CMD_REQUIRES_CARET  = 0x08,   // Needs cursor
    CMD_REQUIRES_SELECT = 0x10,   // Needs selection
    CMD_CONFIRM         = 0x20,   // Show confirmation
    CMD_ASM_HOTPATH     = 0x40,   // MASM fast-path
    CMD_REQUIRES_DEBUG  = 0x80,   // Needs debug session
    CMD_REQUIRES_LSP    = 0x100,  // Needs LSP server
    CMD_REQUIRES_NET    = 0x200,  // Needs network
};
```

### Dispatch System
**Files**: `src/core/unified_command_dispatch.hpp`, `src/core/shared_feature_dispatch.h`

```cpp
// Three dispatch paths — all read from g_commandRegistry[]
DispatchResult dispatchByGuiId(uint32_t commandId, CommandContext& ctx);
DispatchResult dispatchByCli(const char* cliInput, CommandContext& ctx);
DispatchResult dispatchByCanonical(const char* name, CommandContext& ctx);
```

**CommandContext** (passed to every handler):
```cpp
struct CommandContext {
    const char*   rawInput;       // Original user input
    const char*   args;           // Arguments after command
    void*         idePtr;         // Win32IDE* or nullptr
    void*         cliStatePtr;    // CLIState* or nullptr
    uint32_t      commandId;      // IDM_* for GUI
    bool          isGui;
    bool          isHeadless;
    void (*outputFn)(const char* text, void* userData);
    void* outputUserData;
    void output(const char* text) const;
    void outputLine(const std::string& text) const;
};
```

### Complete Command List by Category

#### File Operations (ID 105-1099)
| CLI Alias | Canonical Name | Handler | Flags |
|-----------|---------------|---------|-------|
| `!new` | `file.new` | handleFileNew | NONE |
| `!open` | `file.open` | handleFileOpen | NONE |
| `!save` | `file.save` | handleFileSave | REQUIRES_FILE |
| `!save_as` | `file.saveAs` | handleFileSaveAs | REQUIRES_FILE |
| `!save_all` | `file.saveAll` | handleFileSaveAll | NONE |
| `!close` | `file.close` | handleFileClose | REQUIRES_FILE |
| `!recent` | `file.recentFiles` | handleFileRecentFiles | NONE |
| `!recent_clear` | `file.recentClear` | handleFileRecentClear | NONE |
| `!model_load` | `file.loadModel` | handleFileLoadModel | NONE |
| `!model_hf` | `file.modelFromHF` | handleFileModelFromHF | REQUIRES_NET |
| `!model_ollama` | `file.modelFromOllama` | handleFileModelFromOllama | NONE |
| `!model_url` | `file.modelFromURL` | handleFileModelFromURL | REQUIRES_NET |
| `!model_unified` | `file.modelUnified` | handleFileUnifiedLoad | NONE |
| `!quick_load` | `file.quickLoad` | handleFileQuickLoad | NONE |
| `!exit` | `file.exit` | handleFileExit | NONE |
| `!autosave` | `file.autoSave` | handleFileAutoSave | NONE |
| `!close_folder` | `file.closeFolder` | handleFileCloseFolder | NONE |
| `!open_folder` | `file.openFolder` | handleFileOpenFolder | NONE |
| `!new_window` | `file.newWindow` | handleFileNewWindow | NONE |
| `!close_tab` | `file.closeTab` | handleFileCloseTab | REQUIRES_FILE |

#### Edit Operations (ID 2001-2019, 208-211)
| CLI Alias | Canonical Name | Handler | Flags |
|-----------|---------------|---------|-------|
| `!undo` | `edit.undo` | handleEditUndo | REQUIRES_FILE |
| `!redo` | `edit.redo` | handleEditRedo | REQUIRES_FILE |
| `!cut` | `edit.cut` | handleEditCut | REQUIRES_SELECT |
| `!copy` | `edit.copy` | handleEditCopy | REQUIRES_SELECT |
| `!paste` | `edit.paste` | handleEditPaste | REQUIRES_FILE |
| `!select_all` | `edit.selectAll` | handleEditSelectAll | REQUIRES_FILE |
| `!snippet` | `edit.snippet` | handleEditSnippet | REQUIRES_FILE |
| `!copy_format` | `edit.copyFormat` | handleEditCopyFormat | REQUIRES_SELECT |
| `!paste_plain` | `edit.pastePlain` | handleEditPastePlain | REQUIRES_FILE |
| `!clipboard` | `edit.clipboardHistory` | handleEditClipboardHist | NONE |
| `!find` | `edit.find` | handleEditFind | NONE |
| `!replace` | `edit.replace` | handleEditReplace | NONE |
| `!find_next` | `edit.findNext` | handleEditFindNext | NONE |
| `!find_prev` | `edit.findPrev` | handleEditFindPrev | NONE |
| `!multicursor_add` | `edit.multicursorAdd` | handleEditMulticursorAdd | REQUIRES_FILE+CARET |
| `!multicursor_rm` | `edit.multicursorRemove` | handleEditMulticursorRemove | REQUIRES_FILE |
| `!goto_line` | `edit.gotoLine` | handleEditGotoLine | REQUIRES_FILE |

#### View Operations (ID 301-307, 2020-2029)
| CLI Alias | Canonical Name | Handler |
|-----------|---------------|---------|
| `!minimap` | `view.minimap` | handleViewMinimap |
| `!output_tabs` | `view.outputTabs` | handleViewOutputTabs |
| `!modules` | `view.moduleBrowser` | handleViewModuleBrowser |
| `!theme_editor` | `view.themeEditor` | handleViewThemeEditor |
| `!float_panel` | `view.floatingPanel` | handleViewFloatingPanel |
| `!output` | `view.outputPanel` | handleViewOutputPanel |
| `!streaming` | `view.streamingLoader` | handleViewStreamingLoader |
| `!vulkan` | `view.vulkanRenderer` | handleViewVulkanRenderer |
| `!sidebar` | `view.sidebar` | handleViewSidebar |
| `!view_terminal` | `view.terminal` | handleViewTerminal |
| `!toggle_sidebar` | `view.toggleSidebar` | handleViewToggleSidebar |
| `!toggle_terminal` | `view.toggleTerminal` | handleViewToggleTerminal |
| `!toggle_output` | `view.toggleOutput` | handleViewToggleOutput |
| `!fullscreen` | `view.toggleFullscreen` | handleViewToggleFullscreen |
| `!zoom_in` | `view.zoomIn` | handleViewZoomIn |
| `!zoom_out` | `view.zoomOut` | handleViewZoomOut |
| `!zoom_reset` | `view.zoomReset` | handleViewZoomReset |

#### Themes (ID 3100-3116)
| CLI Alias | Theme |
|-----------|-------|
| `!theme dark+` | Dark+ |
| `!theme light+` | Light+ |
| `!theme monokai` | Monokai |
| `!theme dracula` | Dracula |
| `!theme nord` | Nord |
| `!theme sol-dark` | Solarized Dark |
| `!theme sol-light` | Solarized Light |
| `!theme cyberpunk` | Cyberpunk |
| `!theme gruvbox` | Gruvbox |
| `!theme catppuccin` | Catppuccin |
| `!theme tokyo` | Tokyo Night |
| `!theme crimson` | RawrXD Crimson |
| `!theme hc` | High Contrast |
| `!theme onedark` | One Dark Pro |
| `!theme synthwave` | Synthwave '84 |
| `!theme abyss` | Abyss |

#### Transparency (ID 3200-3211)
`!opacity 100` through `!opacity 50`, `!opacity set`, `!opacity toggle`

#### Git (ID 3020-3024)
| CLI Alias | Canonical Name | Handler |
|-----------|---------------|---------|
| `!git_status` | `git.status` | handleGitStatus |
| `!git_commit` | `git.commit` | handleGitCommit |
| `!git_push` | `git.push` | handleGitPush |
| `!git_pull` | `git.pull` | handleGitPull |
| `!git_diff` | `git.diff` | handleGitDiff |

#### Terminal (ID 4006-4010)
| CLI Alias | Canonical Name |
|-----------|---------------|
| `!terminal_kill` | `terminal.kill` |
| `!terminal_split` | `terminal.splitH` |
| `!terminal_split_v` | `terminal.splitV` |
| `!terminal_code` | `terminal.splitCode` |
| `!terminal_list` | `terminal.list` |

#### Agent (ID 4100-4121)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!agent_loop` | `agent.loop` | ASYNC |
| `!agent_execute` | `agent.execute` | ASYNC |
| `!agent_config` | `agent.configure` | NONE |
| `!tools` | `agent.viewTools` | NONE |
| `!agent_status` | `agent.viewStatus` | NONE |
| `!agent_stop` | `agent.stop` | NONE |
| `!agent_memory` | `agent.memory` | NONE |
| `!agent_memory_view` | `agent.memoryView` | NONE |
| `!agent_memory_clear` | `agent.memoryClear` | CONFIRM |
| `!agent_memory_export` | `agent.memoryExport` | NONE |
| `!chain` | `subagent.chain` | ASYNC |
| `!swarm` | `subagent.swarm` | ASYNC |
| `!todo` | `subagent.todoList` | NONE |
| `!todo_clear` | `subagent.todoClear` | CONFIRM |
| `!agents` | `subagent.status` | NONE |
| `!agent_bounded` | `agent.boundedLoop` | ASYNC |

#### Autonomy (ID 4150-4155)
| CLI Alias | Canonical Name |
|-----------|---------------|
| `!autonomy_toggle` | `autonomy.toggle` |
| `!autonomy_start` | `autonomy.start` |
| `!autonomy_stop` | `autonomy.stop` |
| `!autonomy_goal` | `autonomy.goal` |
| `!autonomy_status` | `autonomy.status` |
| `!autonomy_memory` | `autonomy.memory` |

#### AI Mode (ID 4200-4216)
| CLI Alias | Canonical Name |
|-----------|---------------|
| `!max` | `ai.maxMode` |
| `!deep` | `ai.deepThinking` |
| `!research` | `ai.deepResearch` |
| `!no_refusal` | `ai.noRefusal` |
| `!ctx 4k` | `ai.context4k` |
| `!ctx 32k` | `ai.context32k` |
| `!ctx 64k` | `ai.context64k` |
| `!ctx 128k` | `ai.context128k` |
| `!ctx 256k` | `ai.context256k` |
| `!ctx 512k` | `ai.context512k` |
| `!ctx 1m` | `ai.context1m` |

#### AI Features (ID 401-409)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!ai_complete` | `ai.inlineComplete` | REQUIRES_FILE+CARET |
| `!ai_chat` | `ai.chatMode` | NONE |
| `!ai_explain` | `ai.explainCode` | REQUIRES_SELECT |
| `!ai_refactor` | `ai.refactor` | REQUIRES_SELECT+ASYNC |
| `!ai_tests` | `ai.generateTests` | REQUIRES_FILE+ASYNC |
| `!ai_docs` | `ai.generateDocs` | REQUIRES_FILE+ASYNC |
| `!ai_fix` | `ai.fixErrors` | REQUIRES_FILE+ASYNC |
| `!ai_optimize` | `ai.optimizeCode` | REQUIRES_SELECT+ASYNC |
| `!ai_model` | `ai.modelSelect` | NONE |

#### Reverse Engineering (ID 4300-4319, 8001-8006, 8100-8102)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!decision_tree` | `re.analyze` | REQUIRES_FILE |
| `!disasm` | `re.disassemble` | REQUIRES_FILE |
| `!dumpbin` | `re.dumpbin` | REQUIRES_FILE |
| `!re_compile` | `re.compile` | REQUIRES_FILE |
| `!re_compare` | `re.compare` | REQUIRES_FILE |
| `!re_vulns` | `re.detectVulns` | REQUIRES_FILE+ASYNC |
| `!re_ida` | `re.exportIDA` | REQUIRES_FILE |
| `!re_ghidra` | `re.exportGhidra` | REQUIRES_FILE |
| `!cfg` | `re.cfgAnalysis` | REQUIRES_FILE |
| `!re_funcs` | `re.functions` | REQUIRES_FILE |
| `!demangle` | `re.demangle` | REQUIRES_FILE |
| `!ssa_lift` | `re.ssaLift` | REQUIRES_FILE+ASM |
| `!re_recursive` | `re.recursiveDisasm` | REQUIRES_FILE+ASYNC |
| `!re_types` | `re.typeRecovery` | REQUIRES_FILE+ASYNC |
| `!re_dataflow` | `re.dataFlow` | REQUIRES_FILE |
| `!re_license` | `re.licenseInfo` | REQUIRES_FILE |
| `!decompile` | `re.decompilerView` | REQUIRES_FILE |
| `!decomp_rename` | `re.decompRename` | REQUIRES_FILE+CARET |
| `!decomp_sync` | `re.decompSync` | REQUIRES_FILE |
| `!decomp_close` | `re.decompClose` | NONE |

#### Backend Switcher (ID 5037-5047)
| CLI Alias | Canonical Name |
|-----------|---------------|
| `!backend local` | `backend.switchLocal` |
| `!backend ollama` | `backend.switchOllama` |
| `!backend openai` | `backend.switchOpenAI` |
| `!backend claude` | `backend.switchClaude` |
| `!backend gemini` | `backend.switchGemini` |
| `!backend status` | `backend.status` |
| `!backend list` | `backend.switcher` |
| `!backend config` | `backend.configure` |
| `!backend health` | `backend.healthCheck` |
| `!backend apikey` | `backend.setApiKey` |
| `!backend save` | `backend.saveConfigs` |

#### Router (ID 5048-5081)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!router enable` | `router.enable` | NONE |
| `!router disable` | `router.disable` | NONE |
| `!router status` | `router.status` | NONE |
| `!router decision` | `router.decision` | NONE |
| `!router policy` | `router.setPolicy` | NONE |
| `!router caps` | `router.capabilities` | NONE |
| `!router fallbacks` | `router.fallbacks` | NONE |
| `!router save` | `router.saveConfig` | NONE |
| `!router route` | `router.routePrompt` | ASYNC |
| `!router reset` | `router.resetStats` | CONFIRM |
| `!router why` | `router.whyBackend` | NONE |
| `!router pin` | `router.pinTask` | NONE |
| `!router unpin` | `router.unpinTask` | NONE |
| `!router pins` | `router.showPins` | NONE |
| `!router heatmap` | `router.heatmap` | NONE |
| `!router ensemble` | `router.ensembleEnable` | NONE |
| `!router noensemble` | `router.ensembleDisable` | NONE |
| `!router ens_stat` | `router.ensembleStatus` | NONE |
| `!router simulate` | `router.simulate` | ASYNC |
| `!router sim_last` | `router.simulateLast` | NONE |
| `!router cost` | `router.costStats` | NONE |

#### LSP Client (ID 5058-5070)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!lsp start` | `lsp.startAll` | ASYNC |
| `!lsp stop` | `lsp.stopAll` | NONE |
| `!lsp status` | `lsp.status` | NONE |
| `!lsp goto` | `lsp.gotoDef` | REQUIRES_CARET |
| `!lsp refs` | `lsp.findRefs` | REQUIRES_CARET |
| `!lsp rename` | `lsp.rename` | REQUIRES_CARET |
| `!lsp hover` | `lsp.hover` | REQUIRES_CARET |
| `!lsp diag` | `lsp.diagnostics` | NONE |
| `!lsp restart` | `lsp.restart` | NONE |
| `!lsp clear` | `lsp.clearDiag` | NONE |
| `!lsp symbol` | `lsp.symbolInfo` | REQUIRES_CARET |
| `!lsp config` | `lsp.configure` | NONE |
| `!lsp save` | `lsp.saveConfig` | NONE |

#### LSP Server (ID 9200-9208)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!lspsrv start` | `lspServer.start` | ASYNC |
| `!lspsrv stop` | `lspServer.stop` | NONE |
| `!lspsrv status` | `lspServer.status` | NONE |
| `!lspsrv reindex` | `lspServer.reindex` | ASYNC |
| `!lspsrv stats` | `lspServer.stats` | NONE |
| `!lspsrv diag` | `lspServer.publishDiag` | NONE |
| `!lspsrv config` | `lspServer.config` | NONE |
| `!lspsrv export` | `lspServer.exportSyms` | NONE |
| `!lspsrv stdio` | `lspServer.launchStdio` | ASYNC |

#### ASM Semantic (ID 5082-5093)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!asm parse` | `asm.parseSymbols` | REQUIRES_FILE+ASM |
| `!asm goto` | `asm.gotoLabel` | REQUIRES_FILE+CARET |
| `!asm refs` | `asm.findLabelRefs` | REQUIRES_FILE+CARET |
| `!asm symbols` | `asm.symbolTable` | REQUIRES_FILE |
| `!asm info` | `asm.instructionInfo` | REQUIRES_FILE+CARET |
| `!asm reg` | `asm.registerInfo` | REQUIRES_FILE+CARET |
| `!asm block` | `asm.analyzeBlock` | REQUIRES_FILE+CARET |
| `!asm callgraph` | `asm.callGraph` | REQUIRES_FILE+ASYNC |
| `!asm dataflow` | `asm.dataFlow` | REQUIRES_FILE+ASYNC |
| `!asm convention` | `asm.detectConvention` | REQUIRES_FILE |
| `!asm sections` | `asm.showSections` | REQUIRES_FILE |
| `!asm clear` | `asm.clearSymbols` | CONFIRM |

#### Hybrid LSP-AI (ID 5094-5105)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!hybrid complete` | `hybrid.complete` | REQUIRES_CARET+LSP |
| `!hybrid diag` | `hybrid.diagnostics` | REQUIRES_LSP |
| `!hybrid rename` | `hybrid.smartRename` | REQUIRES_CARET+LSP |
| `!hybrid analyze` | `hybrid.analyzeFile` | REQUIRES_FILE+ASYNC |
| `!hybrid profile` | `hybrid.autoProfile` | REQUIRES_FILE+DEBUG |
| `!hybrid status` | `hybrid.status` | NONE |
| `!hybrid usage` | `hybrid.symbolUsage` | REQUIRES_CARET+LSP |
| `!hybrid explain` | `hybrid.explainSymbol` | REQUIRES_CARET |
| `!hybrid annotate` | `hybrid.annotateDiag` | REQUIRES_LSP |
| `!hybrid stream` | `hybrid.streamAnalyze` | ASYNC |
| `!hybrid prefetch` | `hybrid.semanticPrefetch` | ASYNC |
| `!hybrid correct` | `hybrid.correctionLoop` | ASYNC |

#### Multi-Response (ID 5106-5117)
`!multi generate`, `!multi setmax`, `!multi select`, `!multi compare`, `!multi stats`, `!multi templates`, `!multi toggle`, `!multi prefs`, `!multi latest`, `!multi status`, `!multi clear`, `!multi apply`

#### Governor (ID 5118-5121)
`!gov status`, `!gov submit`, `!gov killall`, `!gov tasks`

#### Safety (ID 5122-5125)
`!safety status`, `!safety reset`, `!safety rollback`, `!safety violations`

#### Replay (ID 5126-5129)
`!replay status`, `!replay last`, `!replay export`, `!replay checkpoint`

#### Debug/DBG (ID 5157-5184)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!debug_start` | `dbg.launch` | NONE |
| `!dbg attach` | `dbg.attach` | NONE |
| `!dbg detach` | `dbg.detach` | REQUIRES_DEBUG |
| `!debug_continue` | `dbg.go` | REQUIRES_DEBUG |
| `!debug_step` | `dbg.stepOver` | REQUIRES_DEBUG |
| `!dbg step_into` | `dbg.stepInto` | REQUIRES_DEBUG |
| `!dbg step_out` | `dbg.stepOut` | REQUIRES_DEBUG |
| `!dbg break` | `dbg.break` | REQUIRES_DEBUG |
| `!debug_stop` | `dbg.kill` | REQUIRES_DEBUG |
| `!breakpoint_add` | `dbg.addBp` | NONE |
| `!breakpoint_remove` | `dbg.removeBp` | NONE |
| `!dbg enable_bp` | `dbg.enableBp` | NONE |
| `!dbg clear_bps` | `dbg.clearBps` | CONFIRM |
| `!breakpoint_list` | `dbg.listBps` | NONE |
| `!dbg watch` | `dbg.addWatch` | NONE |
| `!dbg unwatch` | `dbg.removeWatch` | NONE |
| `!dbg regs` | `dbg.registers` | REQUIRES_DEBUG |
| `!dbg stack` | `dbg.stack` | REQUIRES_DEBUG |
| `!dbg mem` | `dbg.memory` | REQUIRES_DEBUG |
| `!dbg disasm` | `dbg.disasm` | REQUIRES_DEBUG |
| `!dbg modules` | `dbg.modules` | REQUIRES_DEBUG |
| `!dbg threads` | `dbg.threads` | REQUIRES_DEBUG |
| `!dbg thread` | `dbg.switchThread` | REQUIRES_DEBUG |
| `!dbg eval` | `dbg.evaluate` | REQUIRES_DEBUG |
| `!dbg setreg` | `dbg.setRegister` | REQUIRES_DEBUG |
| `!dbg search` | `dbg.searchMemory` | REQUIRES_DEBUG+ASYNC |
| `!dbg sympath` | `dbg.symbolPath` | NONE |
| `!dbg status` | `dbg.status` | NONE |

#### Hotpatch (ID 9001-9017)
| CLI Alias | Canonical Name | Flags |
|-----------|---------------|-------|
| `!hotpatch_status` | `hotpatch.status` | NONE |
| `!hotpatch_mem` | `hotpatch.memApply` | ASM_HOTPATH |
| `!hotpatch_revert` | `hotpatch.memRevert` | ASM_HOTPATH |
| `!hotpatch_byte` | `hotpatch.byteApply` | ASM_HOTPATH |
| `!hotpatch_search` | `hotpatch.byteSearch` | REQUIRES_FILE+ASM |
| `!hotpatch_server` | `hotpatch.serverAdd` | NONE |
| `!hotpatch_srv_rm` | `hotpatch.serverRemove` | NONE |
| `!hotpatch_bias` | `hotpatch.proxyBias` | NONE |
| `!hotpatch_rewrite` | `hotpatch.proxyRewrite` | NONE |
| `!hotpatch_term` | `hotpatch.proxyTerminate` | NONE |
| `!hotpatch_validate` | `hotpatch.proxyValidate` | NONE |
| `!hotpatch_save` | `hotpatch.presetSave` | NONE |
| `!hotpatch_load` | `hotpatch.presetLoad` | NONE |
| `!hotpatch_log` | `hotpatch.eventLog` | NONE |
| `!hotpatch_reset` | `hotpatch.resetStats` | CONFIRM |
| `!hotpatch_toggle` | `hotpatch.toggleAll` | NONE |
| `!hotpatch_pstats` | `hotpatch.proxyStats` | NONE |

#### Plugin (ID 5200-5208)
`!plugin panel`, `!plugin load`, `!plugin unload`, `!plugin unload_all`, `!plugin refresh`, `!plugin scan`, `!plugin status`, `!plugin hotload`, `!plugin config`

#### Swarm (ID 5132-5156)
25 commands total covering: status, start leader/worker/hybrid, stop, nodes, join, remove, blacklist, build operations (cmake/sources/start/cancel), cache, config, discovery, task graph, events, stats, worker management, fitness test.

#### Voice (ID 9700-9709, 10200-10206)
`!voice record`, `!voice ptt`, `!voice speak`, `!voice join`, `!voice devices`, `!voice metrics`, `!voice status`, `!voice mode`, `!voice continuous`, `!voice off`
Plus automation: `!voice auto`, `!voice settings`, `!voice next`, `!voice prev`, `!voice rate_up`, `!voice rate_down`, `!voice auto_stop`

#### Monaco (ID 9100-9105)
`!monaco toggle`, `!monaco devtools`, `!monaco reload`, `!monaco zoomin`, `!monaco zoomout`, `!monaco sync`

#### Editor Engine (ID 9300-9304)
`!editor richedit`, `!editor webview`, `!editor monaco`, `!editor cycle`, `!editor status`

#### PDB (ID 9400-9412)
`!pdb load`, `!pdb fetch`, `!pdb status`, `!pdb cache_clear`, `!pdb enable`, `!pdb resolve`, `!pdb imports`, `!pdb exports`, `!pdb iat`

#### Audit (ID 9500-9506)
`!audit dashboard`, `!audit full`, `!audit stubs`, `!audit menus`, `!audit tests`, `!audit export`, `!audit stats`

#### Gauntlet (ID 9600-9601)
`!gauntlet run`, `!gauntlet export`

#### Telemetry (ID 9900-9905)
`!telemetry toggle`, `!telemetry json`, `!telemetry csv`, `!telemetry show`, `!telemetry clear`, `!telemetry snap`

#### Quality/Workflow (ID 9800-9830)
`!qw shortcuts`, `!qw reset_keys`, `!qw backup`, `!qw restore`, `!qw autobackup`, `!qw backup_list`, `!qw prune`, `!qw monitor`, `!qw alerts`, `!qw dismiss`, `!qw resources`, `!qw slo`

#### Cosmetic/Tier 1 (ID 12000-12091)
`!smooth_scroll`, `!minimap_enhanced`, `!breadcrumbs`, `!fuzzy`, `!settings_gui`, `!welcome`, `!file_icons`, `!tab_drag`, `!split_v`, `!split_h`, `!split_grid`, `!split_close`, `!split_next`, `!auto_update`, `!update_dismiss`

#### Game Engine (ID 7001-7004)
`!unreal_init`, `!unreal_attach`, `!unity_init`, `!unity_attach`

#### CLI-Only Commands (ID=0)
`!search`, `!analyze`, `!profile`, `!subagent`, `!cot`, `!status`, `!help`, `!generate_ide`, `!hotpatch_apply`, `!hotpatch_create`, `!auto_patch`, `!mode`, `!engine`, `!autonomy_rate`, `!autonomy_run`, `!voice init`, `!voice transcribe`, `!server start`, `!server stop`, `!server status`, `!settings`, `!settings_export`, `!settings_import`, `!manifest_json`, `!manifest_md`, `!self_test`

---

## 2. FILE OPERATIONS

**File**: `src/tools/file_ops.h`

```cpp
class FileOps {
    static std::string readText(const std::string& path);
    static bool writeText(const std::string& path, const std::string& content);
    static bool appendText(const std::string& path, const std::string& text);
    static bool remove(const std::string& path);
    static bool rename(const std::string& src, const std::string& dst);
    static bool copy(const std::string& src, const std::string& dst);
    static bool move(const std::string& src, const std::string& dst);
    static bool ensureDir(const std::string& path);
    static std::vector<std::string> list(const std::string& dir, bool recursive);
    static bool exists(const std::string& path);
};
```

---

## 3. EDIT OPERATIONS

**File**: `src/ide/RawrXD_Editor.h`

```cpp
class Editor : public Window {
    void setText(const std::wstring& text);
    void insert(wchar_t ch);
    void backspace();
    void cut();
    void copy();
    void paste();
    void selectAll();
    bool loadFile(const std::wstring& path);
    bool saveFile(const std::wstring& path);
};
```

**TextBuffer (Rope)**: `src/ide/RawrXD_TextBuffer.h`
- O(log n) insert/remove at any position
- UTF-16 surrogate pair handling
- Line-based access

**UndoStack**: `src/ide/RawrXD_UndoStack.h`
- Command-pattern undo/redo
- Clean state tracking
- Signals: `canUndoChanged`, `canRedoChanged`, `cleanChanged`

---

## 4. VIEW & UI

**File**: `src/ide/ide_window.h` (IDEWindow class)

Key Win32 UI creation methods:
- `CreateMainWindow`, `CreateMenuBar`, `CreateToolBar`, `CreateStatusBar`
- `CreateEditorControl`, `CreateFileExplorer`
- `CreateTerminalPanel`, `CreateOutputPanel`
- `CreateTabControl`, `CreateWebBrowser`
- Tab management, Marketplace (SearchMarketplace, InstallExtension)
- Autocomplete, syntax highlighting, command palette
- PowerShell execution, ReactIDEGenerator integration

---

## 5. TERMINAL & COMMAND EXECUTION

**File**: `src/terminal/sandboxed_terminal.hpp`

```cpp
class SandboxedTerminal {
    // Command whitelist/blacklist security
    // Timeout enforcement per command
    // Output sanitization (credential scrubbing)
    // Audit logging (all commands logged)
    // Resource limits (max memory, CPU time)
    // Win32 CreateProcess backend
};
```

---

## 6. AGENT SYSTEM

### 6.1 Planner
**File**: `src/agent/planner.hpp`
```cpp
class Planner {
    nlohmann::json plan(const std::string& humanWish);
    nlohmann::json planQuantKernel(const std::string& wish);
    nlohmann::json planRelease(const std::string& wish);
    nlohmann::json planWebProject(const std::string& wish);
    nlohmann::json planSelfReplication(const std::string& wish);
    nlohmann::json planBulkFix(const std::string& wish);
    nlohmann::json planGeneric(const std::string& wish);
};
```

### 6.2 Meta Planner
**File**: `src/agent/meta_planner.hpp`
```cpp
class MetaPlanner {
    nlohmann::json plan(const std::string& wish);
    nlohmann::json decomposeGoal(const std::string& goal);
    // Specializations:
    nlohmann::json quantPlan(const std::string& w);
    nlohmann::json kernelPlan(const std::string& w);
    nlohmann::json releasePlan(const std::string& w);
    nlohmann::json fixPlan(const std::string& w);
    nlohmann::json perfPlan(const std::string& w);
    nlohmann::json testPlan(const std::string& w);
    nlohmann::json genericPlan(const std::string& w);
};
```

### 6.3 Action Executor
**File**: `src/agent/action_executor.hpp`

```cpp
enum class ActionType {
    FileEdit, SearchFiles, RunBuild, ExecuteTests,
    CommitGit, InvokeCommand, QueryUser, RecursiveAgent
};

class ActionExecutor {
    ActionResult executeAction(const Action& action);
    bool executePlan(const nlohmann::json& plan);  // async
    void cancelExecution();
    // Callbacks: onProgress, onComplete, onFailure
};
```

### 6.4 Model Invoker
**File**: `src/agent/model_invoker.hpp`
```cpp
class ModelInvoker {
    void setLLMBackend(const std::string& backend); // ollama/claude/openai
    InvocationResult invoke(const InvocationParams& params);
    void invokeAsync(const InvocationParams& params, InvocationCallback cb);
    // Internal: sendOllamaRequest, sendClaudeRequest, sendOpenAIRequest
    // Response caching
};
```

### 6.5 IDE Agent Bridge
**File**: `src/agent/ide_agent_bridge.hpp`
```cpp
class IDEAgentBridge {
    AgentResult executeWish(const std::string& wish);
    nlohmann::json planWish(const std::string& wish);
    void approvePlan(const std::string& planId);
    void rejectPlan(const std::string& planId);
    void cancelExecution();
    bool dryRunMode;
    std::vector<ExecutionRecord> executionHistory;
};
```

### 6.6 Agentic Copilot Bridge
**File**: `src/agent/agentic_copilot_bridge.hpp`
```cpp
class AgenticCopilotBridge {
    std::string generateCodeCompletion(const std::string& context);
    std::string analyzeActiveFile(const std::string& filepath);
    std::string suggestRefactoring(const std::string& code);
    std::string generateTestsForCode(const std::string& code);
    std::string askAgent(const std::string& question);
    std::string executeWithFailureRecovery(const std::string& prompt);
    std::string hotpatchResponse(const std::string& response);
    std::string transformCode(const std::string& code, const std::string& instruction);
    std::string explainCode(const std::string& code);
    std::string findBugs(const std::string& code);
    void trainModel(const std::string& data);
};
```

### 6.7 Deep Thinking Engine
**File**: `src/agent/agentic_deep_thinking_engine.hpp`

```cpp
enum class ThinkingStep : uint8_t {
    Initialization, ProblemAnalysis, ContextGathering,
    HypothesisGeneration, ExperimentationRun, ResultEvaluation,
    SelfCorrection, FinalSynthesis, Complete
};

class AgenticDeepThinkingEngine {
    // Chain-of-thought reasoning
    // Streamable output
    // Self-correcting
    // Caching/memory
};
```

### 6.8 Autonomous SubAgent
**File**: `src/agent/autonomous_subagent.hpp`
```cpp
class AutonomousSubAgent {
    // Bulk repetitive operations
    // BulkFixStrategy: promptTemplate, verification, self-healing
    // BulkFixTarget: status tracking
    // Parallel dispatch
};
```

### 6.9 Self-Modification
| File | Class | Purpose |
|------|-------|---------|
| `self_patch.hpp` | SelfPatch | addKernel, addCpp, hotReload, patchFile |
| `self_code.hpp` | SelfCode | editSource, addInclude, regenerateMOC, rebuildTarget |
| `hot_reload.hpp` | HotReload | reloadQuant, reloadModule |
| `rollback.hpp` | Rollback | detectRegression, revertLastCommit, openIssue |
| `auto_bootstrap.hpp` | AutoBootstrap | installZeroTouch, startWithWish |
| `zero_touch.hpp` | ZeroTouch | installFileWatcher, installGitHook, installVoiceTrigger |

### 6.10 Lifecycle
| File | Class | Purpose |
|------|-------|---------|
| `self_test.hpp` | SelfTest | runAll, runUnitTests, runIntegrationTests, runLint, runBenchmarkBaseline |
| `release_agent.hpp` | ReleaseAgent | bumpVersion, tagAndUpload, createGitHubRelease, uploadToCDN, tweet, signBinary |
| `code_signer.hpp` | CodeSigner | signWindowsExecutable, signMacOSBundle, verifySignature, notarizeMacOSApp |
| `sentry_integration.hpp` | SentryIntegration | captureException, captureMessage, addBreadcrumb, startTransaction |
| `telemetry_collector.hpp` | TelemetryCollector | trackFeatureUsage, trackCrash, trackPerformance |
| `meta_learn.hpp` | MetaLearn | performance recording/suggestion for quant+kernel auto-tuning |

---

## 7. AGENTIC SUBSYSTEM (TOOL-CALLING LOOP)

### 7.1 Bounded Agent Loop
**File**: `src/agentic/BoundedAgentLoop.h`

Core agent loop: maxSteps (default 8), `while(step < MAX) { LLM → tool_call → execute → result → LLM }`

```cpp
enum class AgentLoopState { Idle, Running, WaitingForTool, Complete, Error, MaxStepsReached };

class BoundedAgentLoop {
    void run(const std::string& userGoal);
    AgentLoopState getState() const;
    // Full transcript recording
    // Ollama /api/chat backend
};
```

### 7.2 Tool Registry (X-Macro)
**File**: `src/agentic/ToolRegistry.h`

```cpp
#define AGENT_TOOLS_X(X) \
    X("read_file",        "Read a file's content", ...) \
    X("write_file",       "Write content to a file", ...) \
    X("replace_in_file",  "Replace text in a file", ...) \
    X("execute_command",  "Execute a shell command", ...) \
    X("search_code",      "Search for patterns in code", ...) \
    X("get_diagnostics",  "Get compiler diagnostics", ...) \
    X("list_directory",   "List directory contents", ...) \
    X("get_coverage",     "Get test coverage data", ...) \
    X("run_build",        "Build the project", ...) \
    X("apply_hotpatch",   "Apply a hotpatch", ...) \
    X("disk_recovery",    "Disk recovery operations", ...)
```

Auto-generates:
- OpenAI function-calling schemas (JSON)
- System prompt listing all tools
- Tool dispatch table

### 7.3 Tool Handlers
**File**: `src/agentic/AgentToolHandlers.h`

```cpp
// Concrete implementations
class ToolReadFile { ... };
class WriteFile { ... };
class ReplaceInFile { ... };
class ListDir { ... };
class ExecuteCommand { ... };
class SearchCode { ... };
class GetDiagnostics { ... };

// Security guardrails
struct ToolGuardrails {
    std::vector<std::string> allowedRoots;
    std::vector<std::string> denyPatterns;
    size_t maxFileSize;
    int timeoutMs;
    bool requireBackup;
};
```

### 7.4 Tool Call Result
**File**: `src/agentic/ToolCallResult.h`

```cpp
enum class ToolOutcome : uint8_t {
    Success, PartialSuccess, ValidationFailed,
    SandboxBlocked, ExecutionError, Timeout,
    Cancelled, NotFound, RateLimited
};
```

### 7.5 Agent Orchestrator
**File**: `src/agentic/AgentOrchestrator.h`

Wires ToolRegistry + AgentOllamaClient + FIMPromptBuilder:
- `RunAgentLoop()` — multi-turn tool-calling
- `RequestCompletion()` — FIM ghost text
- Session management
- Auto-build after edits
- Coverage-aware

### 7.6 Ollama Client
**File**: `src/agentic/AgentOllamaClient.h`

WinHTTP streaming:
- `ChatSync` / `ChatStream` — agentic with tool calling
- `FIMSync` / `FIMStream` — ghost text (Fill-In-Middle)
- Supports Qwen 2.5 Coder models
- OllamaConfig: host, port, models, timeout

### 7.7 FIM Prompt Builder
**File**: `src/agentic/FIMPromptBuilder.h`

- Supports Qwen / DeepSeek / StarCoder / CodeLlama formats
- Context windowing
- Prefix/suffix splitting

### 7.8 Context Assembler
**File**: `src/agentic/context_assembler.h`

Priority hierarchy:
1. currentFunction
2. imports
3. recentEdits (circular buffer)
4. siblingFunctions
5. fileSummary

Max 8192 token context. GitDiffParser integration.

### 7.9 Diff Engine
**File**: `src/agentic/DiffEngine.h`

Myers O(ND) diff algorithm:
- DiffOp: Equal, Insert, Delete
- DiffHunk with accept/reject
- Unified diff generation
- `ApplyHunk()`, `ApplyAcceptedHunks()`

### 7.10 Agent Transcript
**File**: `src/agentic/AgentTranscript.h`

Append-only execution log: stepNumber, timestamps, model response, tool calls, results, latency, token counts, JSON serialization.

### 7.11 Deterministic Replay Engine
**File**: `src/agentic/DeterministicReplayEngine.h`

3 modes: Verify, Simulate, Audit

- Workspace snapshots (file hashes)
- Divergence detection: ToolOutputMismatch, FileStateMismatch, etc.
- FailureClass triaging: OOM, Timeout, LogicDrift, DiskPressure, GpuStall, HotpatchCascade
- Telemetry counter snapshots

### 7.12 Model Cascade
**File**: `src/agentic/model_cascade.h`

Multi-armed bandit model selection:
- Thompson sampling
- CircuitBreaker (disable after consecutive failures)
- LatencyHistogram (P50/P95/P99)
- TokenEstimator
- ModelCapability with task affinities

```cpp
enum class TaskType : uint8_t {
    CODE_COMPLETION, CHAT, REFACTOR, EXPLAIN,
    TEST_GEN, DOC_GEN, BUG_FIX, CODE_REVIEW, AGENT_TOOL_CALL
};
```

### 7.13 Multi-File Transaction
**File**: `src/agentic/multi_file_transaction.h`

Atomic multi-file edits:
- FileEditNode with dependency tracking
- IncludeGraph (#include relationships)
- DependencyGraph (topological sort, cycle detection)
- Three-way merge conflict detection
- TransactionState machine

---

## 8. AUTONOMY SYSTEM

### 8.1 Autonomous Workflow Engine
**File**: `src/core/autonomous_workflow_engine.hpp`

End-to-end pipeline: **Scan → BulkFix → Verify → Build → Test → SummarizeDiff**

```cpp
enum class WorkflowStage : uint8_t {
    Idle, Scan, BulkFix, Verify, Build, Test, SummarizeDiff,
    Complete, RolledBack, Aborted
};

struct WorkflowPolicy {
    bool rollbackOnBuildFailure;     // default: true
    bool rollbackOnTestFailure;      // default: true
    bool requireDiffApproval;        // default: false
    int  maxBuildRetries;            // default: 2
    int  maxTotalTimeMs;             // default: 600000 (10 min)
    std::string buildTarget;         // default: "RawrXD-Shell"
};

class AutonomousWorkflowEngine {
    WorkflowResult executeWorkflow(const WorkflowPolicy& policy);
    void cancelWorkflow();
    // Callbacks: onStageStarted, onStageCompleted, onDiffApproval, onCompleted
};
```

### 8.2 Background Daemon
**File**: `src/agentic/autonomous_background_daemon.hpp`

Continuous proactive analysis (24/7 low-priority threads):

```cpp
enum class DaemonTaskType : uint8_t {
    DeadCodeScan, DocumentationSync, HotPathOptimize,
    SecurityAudit, DependencyCheck, CodeQualityLint,
    TestCoverageGap, MemoryLeakScan, PerformanceRegression,
    RefactorSuggestion
};
```

Scan results: `DeadCodeEntry`, `DocSyncEntry`, `SecurityFinding`, `OptimizationHint`

Configurable intervals (default: dead code 5min, doc sync 1min, security 10min, deps 1hr).

---

## 9. AI MODES & LLM INTEGRATION

### 9.1 AI Assistant Engine
**File**: `src/ai/ai_assistant_engine.h`

```cpp
enum class AssistanceMode : uint8_t {
    InlineComplete, ChatMode, CommandMode, EditMode, AgentMode
};

enum class ModelProvider : uint8_t {
    Local_GGUF, Ollama, OpenAI_Compatible, Anthropic, Custom
};

class AIAssistantEngine {
    void RequestInlineCompletion(const CompletionContext& ctx);
    void SendChatMessage(const std::string& message);
    void RequestEdit(const EditRequest& req);   // Cursor-style
    void CreateAgentTask(const AgentTaskRequest& req);
    std::string ExplainCode(const std::string& code);
    std::vector<Suggestion> SuggestRefactorings(const std::string& code);
    std::string GenerateTests(const std::string& code);
    std::string GenerateDocumentation(const std::string& code);
    std::vector<Bug> FindBugs(const std::string& code);
    std::string OptimizeCode(const std::string& code);
};
```

### 9.2 AI IDE Integration
**File**: `src/ai/ai_ide_integration.h`

Win32 UI integration: inline completion overlay, chat panel, edit prompt dialog, agent panel, model selector, quick actions (explain/refactor/test/docs/bugs/optimize).

### 9.3 Digestion Engine
**File**: `src/ai/digestion_engine.h`

Codebase-wide stub scanner/fixer:
- AVX-512 accelerated detection
- AI-powered fix generation
- SQLite persistence
- Checkpoint/resume

---

## 10. HOTPATCHING (3-LAYER + EXTENSIONS)

### Layer 1: Memory
**File**: `src/core/model_memory_hotpatch.hpp`
```cpp
PatchResult apply_memory_patch(void* addr, size_t size, const void* data);
PatchResult revert_memory_patch(const std::string& patchId);
// VirtualProtect-based, backup/rollback per entry
```

### Layer 2: Byte-Level
**File**: `src/core/byte_level_hotpatcher.hpp`
```cpp
PatchResult patch_bytes(const char* filename, const BytePatch& patch);
// SIMD pattern search (Boyer-Moore / SIMD scan)
// mmap I/O (CreateFileMapping)
// ByteMutation: XOR, Rotate, Swap, Reverse
```

### Layer 3: Server Proxy
**File**: `src/core/proxy_hotpatcher.hpp`
```cpp
// TokenBias injection
// StreamTerminationRule
// OutputRewriteRule
// ProxyValidator (function pointer, not std::function)
```

### Unified Manager
**File**: `src/core/unified_hotpatch_manager.hpp`
```cpp
class UnifiedHotpatchManager {
    // Coordinates 5 layers: PT Driver L0, Memory L1, Byte L2, Server L3, Live Binary L5
    UnifiedResult apply_memory_patch(...);
    UnifiedResult apply_byte_patch(...);
    UnifiedResult add_server_patch(ServerHotpatch*);
    // Preset save/load (JSON)
    // Event ring buffer
};
```

### Server Hotpatch
**File**: `src/server/gguf_server_hotpatch.hpp`
```cpp
// Injection Points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk
struct ServerHotpatch {
    const char* name;
    bool (*transform)(Request*, Response*);
    uint64_t hit_count;
};
```

### GGUF Proxy Server
**File**: `src/agent/gguf_proxy_server.hpp`
- TCP proxy between IDE-agent and GGUF model server
- Hot-patching interception

---

## 11. LSP (LANGUAGE SERVER PROTOCOL)

### Custom Methods
**File**: `src/lsp/lsp_bridge_protocol.hpp`
```
rawrxd/hotpatch/list
rawrxd/hotpatch/apply
rawrxd/hotpatch/revert
rawrxd/hotpatch/diagnostics
rawrxd/gguf/modelInfo
rawrxd/gguf/tensorList
rawrxd/gguf/validate
rawrxd/workspace/symbols
rawrxd/workspace/stats
```

### Hotpatch Symbol Provider
**File**: `src/lsp/hotpatch_symbol_provider.hpp`
- Exports hotpatch symbols as navigable LSP symbols
- ASM-accelerated hash lookup
- Supports documentSymbol, workspaceSymbol, definition, hover, references

### GGUF Diagnostic Provider
**File**: `src/lsp/gguf_diagnostic_provider.hpp`
- 5 diagnostic sources: rawrxd-gguf, rawrxd-memory, rawrxd-byte, rawrxd-server, rawrxd-cross

---

## 12. DEBUGGING

**File**: `src/core/native_debugger_engine.h`

Full DbgEng COM-based native debugger:

```cpp
class NativeDebuggerEngine {
    // Process control
    DebugResult launchProcess(const LaunchConfig& config);
    DebugResult attachToProcess(DWORD pid);
    DebugResult detach();
    DebugResult go();
    DebugResult stepOver();
    DebugResult stepInto();
    DebugResult stepOut();
    DebugResult breakExecution();
    DebugResult terminateProcess();
    
    // Breakpoints
    DebugResult addSoftwareBreakpoint(uint64_t address);
    DebugResult addSourceBreakpoint(const char* file, int line);
    DebugResult addHardwareBreakpoint(uint64_t address, HwBpType type, uint32_t size);
    DebugResult addConditionalBreakpoint(uint64_t address, const char* condition);
    DebugResult addDataBreakpoint(uint64_t address, uint32_t size, DataBpAccess access);
    DebugResult removeBreakpoint(uint32_t bpId);
    DebugResult removeAllBreakpoints();
    
    // Registers
    DebugResult getRegisters(RegisterSnapshot& out);
    DebugResult setRegister(const char* name, uint64_t value);
    
    // Stack
    DebugResult getCallStack(std::vector<StackFrame>& out, uint32_t maxFrames);
    
    // Memory
    DebugResult readMemory(uint64_t addr, void* buf, size_t size, size_t* bytesRead);
    DebugResult writeMemory(uint64_t addr, const void* buf, size_t size);
    DebugResult searchMemory(uint64_t start, uint64_t size, const void* pattern, size_t patternSize, std::vector<uint64_t>& results);
    
    // Disassembly
    DebugResult disassemble(uint64_t addr, uint32_t count, std::vector<DisassembledInstruction>& out);
    DebugResult setDisassemblyFlavor(DisasmFlavor flavor);  // Intel/ATT/MASM
    
    // Expression evaluation
    DebugResult evaluate(const char* expression, uint64_t& result, std::string& formatted);
    
    // Watch
    DebugResult addWatch(const char* expression);
    DebugResult removeWatch(uint32_t watchId);
    DebugResult getWatchValues(std::vector<WatchValue>& out);
    
    // Modules & Threads
    DebugResult getModules(std::vector<ModuleInfo>& out);
    DebugResult getThreads(std::vector<ThreadInfo>& out);
    DebugResult switchThread(DWORD threadId);
    
    // Symbol path
    DebugResult setSymbolPath(const char* path);
    
    // JSON serialization for all types
};
```

---

## 13. REVERSE ENGINEERING & DECOMPILER

**File**: `src/reverse_engineering/RawrReverseEngine.hpp` (1011 lines)

```cpp
class RawrReverseEngine {
    bool LoadBinary(const std::string& path);
    
    // Call Graph
    CallGraph BuildCallGraph();
    std::string PrintCallGraph(const CallGraph& graph);
    
    // Data Flow
    std::vector<DataFlowInfo> AnalyzeDataFlow(uint64_t address);
    
    // Signature Matching
    std::vector<SignatureMatch> MatchSignatures();
    
    // Decompilation
    DecompilationResult Decompile(uint64_t address);
    
    // Binary Diff
    BinaryDiff CompareBinaries(const std::string& path1, const std::string& path2);
};
```

Supporting classes:
- `RawrCodex.hpp` — Binary loader + symbol table + disassembler
- `RawrDumpBin.hpp` — PE header analysis (like dumpbin)
- `RawrCompiler.hpp` — Compilation from the IDE

---

## 14. CODE INTELLIGENCE & ANALYSIS

### Static Analysis Engine
**File**: `src/core/static_analysis_engine.hpp`

CFG/SSA construction:
```cpp
enum class IROpcode : uint16_t {
    // 40+ opcodes including:
    NOP, ASSIGN, ADD, SUB, MUL, DIV, MOD, NEG,
    AND, OR, XOR, NOT, SHL, SHR,
    LOAD, STORE, ALLOCA, GEP,
    BR, BR_COND, SWITCH, RET,
    CALL, INVOKE, PHI, SELECT,
    // ... comparisons, conversions, vector ops
};

struct BasicBlock {
    // Predecessor/successor lists
    // Dominator tree
    // Liveness analysis
};

class ControlFlowGraph {
    // Dead code elimination
    // Constant propagation
};
```

### Semantic Code Intelligence
**File**: `src/core/semantic_code_intelligence.hpp`

Cross-reference database:
```cpp
enum class SymbolKind : uint8_t {
    Function, Method, Constructor, Destructor,
    Variable, Parameter, Field, Property,
    Class, Struct, Enum, EnumMember,
    Interface, Namespace, Module, TypeAlias,
    Macro, Template, Concept, Label
};

enum class ReferenceKind : uint8_t {
    Read, Write, Call, TypeRef, Inherit,
    Override, Implement, Import, Instantiate, AddressOf
};
```

---

## 15. REFACTORING

**File**: `src/ide/refactoring_plugin.h`

```cpp
enum class RefactoringCategory : uint8_t {
    Extract, Inline, Rename, Convert,
    ModernCpp, Organize, Safety, General, Custom
};

class RefactoringEngine {
    static RefactoringEngine& instance();
    
    void registerRefactoring(const RefactoringDescriptor& desc);
    std::vector<const RefactoringDescriptor*> getAvailable(const RefactoringContext& ctx);
    RefactoringResult executeRefactoring(const std::string& id, const RefactoringContext& ctx);
    
    // Plugin DLL support (C-ABI)
    bool loadPlugin(const std::string& path);
    void unloadPlugin(const std::string& name);
};

// C-ABI plugin interface:
// CRefactoringDescriptor, CRefactoringResult, CPluginInfo
// Plugin function pointer typedefs for LoadedPlugin DLL
```

### Safe Refactor Engine
**File**: `src/core/safe_refactor_engine.hpp`

Pipeline: **Snapshot → DiffPreview → VerificationGate → ApprovalGate → Commit/Rollback**

```cpp
struct VerificationGateConfig {
    bool checkSyntax;
    bool checkSymbols;
    bool checkCRC;
    bool checkCollateralDamage;
};
```

---

## 16. GIT OPERATIONS

**File**: `src/tools/git_client.h`

```cpp
class GitClient {
    std::string run(const std::vector<std::string>& args);
    std::string version();
    std::string status();
    bool add(const std::string& path);
    bool commit(const std::string& message);
    bool checkout(const std::string& branchOrRef);
    bool createBranch(const std::string& name);
    std::string currentBranch();
    std::string diff();
    bool stashSave(const std::string& message);
    bool stashPop();
    bool fetch();
    bool pull();
    bool push();
};
```

---

## 17. MODEL & INFERENCE ENGINE

### RawrEngine (Core)
**File**: `src/engine/rawr_engine.h`
```cpp
class RawrEngine {
    bool load(const char* model_path, const char* tok_path, const char* merge_path);
    std::string generate(const std::string& prompt, int max_tokens,
                         std::function<void(const char*)> callback);
};
```

### Ultra-Fast Inference
**File**: `src/inference/ultra_fast_inference.h`
- TensorPruningScorer (magnitude/activation/gradient)
- StreamingTensorReducer (3.3x reduction, SVD, VQ, mixed precision)
- ModelHotpatcher (tier-based: 70B→21B→6B→2B, <100ms swap)

### Polymorphic Loader
**File**: `src/inference/polymorphic_loader.h`
- IFormatAdapter: GGUF, Sharded, MixedTier
- Universal TensorDesc
- SlotLattice fixed-memory pool
- π-partitioned ActiveWindowBudget (2.5GB total)

### KV Cache Optimizer
**File**: `src/gpu/kv_cache_optimizer.h`
- Dynamic sliding-window cache
- Eviction when context > 32k
- GPU acceleration support

### Speculative Decoder
**File**: `src/gpu/speculative_decoder.h`
- Draft model (TinyLlama) + Target model (Phi-3)
- 1.8x tokens/sec boost
- Acceptance rate tracking

### Engine Manager
**File**: `src/modules/engine_manager.h`
```cpp
class EngineManager {
    bool LoadEngine(const std::string& engine_path, const std::string& engine_id);
    bool UnloadEngine(const std::string& engine_id);
    bool SwitchEngine(const std::string& engine_id);
    bool Load800BModel(const std::string& model_name);
    bool Setup5DriveLayout(const std::string& base_dir);
};
```

### Memory-Mapped File
**File**: `src/ai/memory_mapped_file.h`
- Windows memory mapping for large GGUF models (3-70GB)
- Zero-copy access

---

## 18. SWARM / DISTRIBUTED INFERENCE

**File**: `src/cli/swarm_orchestrator.h`

Distributed transformer inference across LAN:

```cpp
// Protocol: UDP Discovery (7946), TCP Data (7947)
// 32-byte header: [magic:4][type:2][size:2][payload:8][quant:4][crc:4][res:8]

enum class NodeRole : uint8_t { Coordinator, Worker, Hybrid };
enum class NodeState : uint8_t { Offline, Joining, Active, Overloaded, Failed };

class SwarmOrchestrator {
    SwarmResult startAsCoordinator();
    SwarmResult startAsWorker(const std::string& coordinatorHost);
    SwarmResult startAsHybrid();
    SwarmResult distributeModel(const std::string& modelPath);
    SwarmResult submitInference(const SwarmInferenceRequest& req);
    // Automatic rebalancing on VRAM pressure or node failure
    // Heartbeat thread (15s timeout)
    // MASM ASM: swarm_stream_layer, swarm_compute_layer_crc32
};
```

---

## 19. VOICE & TTS

### Voice Chat Engine
**File**: `src/core/voice_chat.hpp`
```cpp
class VoiceChat {
    VoiceChatResult startRecording();
    VoiceChatResult stopRecording();
    VoiceChatResult playAudio(const int16_t* samples, size_t count);
    VADState getVADState();
    VoiceChatResult setMode(VoiceChatMode mode);  // PushToTalk / Continuous
    VoiceChatResult pttBegin() / pttEnd();
    VoiceChatResult transcribe(const std::vector<int16_t>& audio, std::string& outText);
    VoiceChatResult speak(const std::string& text);
    VoiceChatResult joinRoom(const std::string& roomName);
    // Device enumeration, WebSocket relay
};
```

### Voice Automation (TTS)
**File**: `src/core/voice_automation.hpp`

```cpp
class VoiceAutomation {
    // Toggle voice on/off
    // Pluggable voice providers via DLL (C-ABI)
    // Built-in Windows SAPI TTS
    // Voice selection, rate/volume/pitch
    // Async speech queue with interrupt/cancel
    // Streaming TTS (chunks as they arrive)
};

// Plugin C-ABI:
VoiceProvider_GetInfoFn, VoiceProvider_InitFn, VoiceProvider_ShutdownFn,
VoiceProvider_EnumVoicesFn, VoiceProvider_SpeakFn, VoiceProvider_CancelFn,
VoiceProvider_IsSpeakingFn, VoiceProvider_SetCallbackFn
```

---

## 20. VISION / MULTI-MODAL

**File**: `src/core/vision_encoder.hpp`

```cpp
enum class ImageFormat : uint8_t {
    RGB8, RGBA8, GRAY8, RGB_F32, BGR8, PNG, JPEG, BMP, WEBP
};

struct VisionModelConfig {
    enum class Architecture : uint8_t {
        CLIP_VIT_L14, CLIP_VIT_B32, SIGLIP_SO400M,
        LLAVA_NEXT, PHI3_VISION, CUSTOM
    };
};

class VisionEncoder {
    VisionResult loadModel(const VisionModelConfig& config);
    VisionResult encodeImage(const ImageBuffer& image, VisionEmbedding& out);
    VisionResult screenshotToContext(VisionTextPair& out);
    VisionResult diagramToStructured(const ImageBuffer& diagram, std::string& json);
    // Support: resize, normalize, patch extraction, clipboard paste
};
```

---

## 21. EMBEDDINGS & VECTOR SEARCH

### Vector Index (HNSW)
**File**: `src/core/vector_index.h`

```cpp
class HNSWIndex {
    IndexResult insert(uint64_t id, const float* embedding);
    std::vector<std::pair<uint64_t, float>> search(const float* query, uint32_t k);
    IndexResult remove(uint64_t id);
    IndexResult saveToFile(const std::filesystem::path& path);
    IndexResult loadFromFile(const std::filesystem::path& path);
    // Config: M=16, efConstruction=200, efSearch=64, dim=384, maxElements=100000
};

class EmbeddingCache {
    void put(const std::string& key, const std::vector<float>& embedding);
    const std::vector<float>* get(const std::string& key);
    // LRU eviction, hit rate tracking
};
```

### Embedding Engine
**File**: `src/core/embedding_engine.hpp`

```cpp
class EmbeddingEngine {
    EmbedResult loadModel(const EmbeddingModelConfig& config);
    EmbedResult embed(const std::string& text, std::vector<float>& out);
    EmbedResult embedBatch(const std::vector<std::string>& texts, std::vector<std::vector<float>>& out);
    EmbedResult embedFile(const std::string& filepath, const ChunkingConfig& config, std::vector<CodeChunk>& out);
    EmbedResult indexDirectory(const std::string& rootPath);
    // SIMD distance: Cosine, L2, Dot, Manhattan
    // Language-aware chunking: C/C++/Python/JavaScript/Rust/Go
};
```

---

## 22. KNOWLEDGE GRAPH (PERSISTENT LEARNING)

**File**: `src/core/knowledge_graph_core.hpp`

Cross-session persistent learning with SQLite backend:

```cpp
enum class DecisionType : uint8_t {
    ArchitecturalChoice, RefactorReason, BugFixRationale,
    PerformanceOptimization, SecurityDecision, UserPreference,
    ToolChoice, DependencyChoice, ApiDesign, TestStrategy
};

enum class PreferenceCategory : uint8_t {
    LoopStyle, NamingConvention, ErrorHandling, MemoryManagement,
    CodeOrganization, TestingStyle, CommentStyle, IndentationStyle,
    BraceStyle, TemplateUsage
};

struct DecisionRecord {
    DecisionType type;
    char summary[256], rationale[1024], alternatives[512];
    float embedding[384];  // For semantic search
    float confidence;
};

struct UserPreference {
    PreferenceCategory category;
    float bayesianScore;  // Bayesian confidence
    int observationCount;
};
```

---

## 23. BACKGROUND DAEMON (PROACTIVE ANALYSIS)

**File**: `src/agentic/autonomous_background_daemon.hpp`

Runs 24/7 on low-priority threads without human intervention:

| Task Type | Default Interval | Description |
|-----------|-----------------|-------------|
| DeadCodeScan | 5 min | Find unreachable/uncalled functions |
| DocumentationSync | 1 min | Detect signature changes, update docs |
| HotPathOptimize | 15 min | Profile-guided optimization hints |
| SecurityAudit | 10 min | CVE scanning, unsafe patterns |
| DependencyCheck | 1 hr | Unused deps, version vulnerabilities |
| CodeQualityLint | 5 min | Style, complexity, duplication |
| TestCoverageGap | — | Find untested code paths |
| MemoryLeakScan | — | Static analysis for leaks |
| PerformanceRegression | — | Compare against baseline |
| RefactorSuggestion | — | Suggest improvements |

---

## 24. PLUGIN & EXTENSION SYSTEM

### Refactoring Plugin (C-ABI DLL)
**File**: `src/ide/refactoring_plugin.h`
- `CPluginInfo`, `CRefactoringDescriptor`, `CRefactoringResult`
- DLL load/unload with function pointer dispatch

### Voice Provider Plugin (C-ABI DLL)
**File**: `src/core/voice_automation.hpp`
- `VoiceProvider_*` function pointer typedefs
- LoadedVoiceProvider with HMODULE + function table

---

## 25. VS CODE EXTENSION API COMPATIBILITY

**File**: `src/modules/vscode_extension_api.h` (1910 lines!)

Full VS Code API surface reimplemented in C++:

```
vscode.commands        → CommandRegistry
vscode.window          → WindowAPI (Win32 HWND)
vscode.workspace       → WorkspaceAPI (file watchers, config)
vscode.languages       → LanguagesAPI (diagnostics, completions, hovers)
vscode.env             → EnvironmentAPI (shell, clipboard, URI)
vscode.extensions      → ExtensionsRegistry
vscode.debug           → DebugAPI
vscode.tasks           → TaskAPI
vscode.scm             → SCMProvider
Disposable             → RAII
EventEmitter<T>        → Function pointer ring buffer
TreeDataProvider<T>    → Native TreeView bridge
StatusBarItem          → Win32 status bar
OutputChannel          → IDE output panel
TextDocument           → Buffer model
TextEditor             → Active editor
Uri                    → file:// and custom schemes
Range / Position       → Line/column geometry
Diagnostic             → LSP-compatible diagnostics
CompletionItem         → Autocomplete entries
CancellationToken      → Cooperative cancellation
```

---

## 26. SAFE REFACTORING & TRANSACTIONS

### Safe Refactor Engine
**File**: `src/core/safe_refactor_engine.hpp`

Pipeline: Snapshot → DiffPreview → VerificationGate → ApprovalGate → Commit/Rollback

### Multi-File Transaction
**File**: `src/agentic/multi_file_transaction.h`

- Atomic multi-file edits with rollback
- #include dependency graph
- Topological ordering
- Three-way merge conflict detection

---

## 27. FAILURE DETECTION & SELF-HEALING

### Failure Detector
**File**: `src/agent/agentic_failure_detector.hpp`
```cpp
enum class AgentFailureType : uint8_t {
    Refusal, Hallucination, FormatViolation, InfiniteLoop,
    TokenLimitExceeded, ResourceExhausted, Timeout, SafetyViolation
};
```

### Puppeteer (Auto-Correction)
**File**: `src/agent/agentic_puppeteer.hpp`
```cpp
class AgenticPuppeteer { ... };
class RefusalBypassPuppeteer : public AgenticPuppeteer { ... };
class HallucinationCorrectorPuppeteer : public AgenticPuppeteer { ... };
class FormatEnforcerPuppeteer : public AgenticPuppeteer { ... };
```

### Hotpatch Orchestrator
**File**: `src/agent/agentic_hotpatch_orchestrator.hpp`
```cpp
enum class CorrectionAction : uint8_t {
    RetryWithBias, RewriteOutput, TerminateStream,
    PatchMemory, PatchBytes, InjectServerPatch,
    EscalateToUser, SwitchModel
};
```

### Self-Healing Orchestrator
**File**: `src/agent/agent_self_healing_orchestrator.hpp`
```cpp
enum class SelfHealAction {
    PatternFix, TrampolineRedirect, CASPointerFix,
    NopSledCleanup, OutputRewrite, BiasInjection, FullRollback
};
```

### Self-Repair (Enterprise)
**File**: `src/agent/agent_self_repair.hpp`

- RepairableFunction registry
- BugSignature scanning (read-only MASM64)
- LiveBinaryPatcher function-level redirections
- CRC-guarded patches

---

## 28. TELEMETRY & METRICS

**File**: `src/agent/telemetry_collector.hpp`
```cpp
class TelemetryCollector {
    // Privacy-respecting opt-in
    void trackFeatureUsage(const std::string& feature);
    void trackCrash(const std::string& stackTrace);
    void trackPerformance(const std::string& metric, double value);
};
```

**File**: `src/ide/ide_diagnostic_system.h`
```cpp
enum class DiagnosticEvent {
    CompileWarning, RuntimeError, MemoryLeak,
    SecurityIssue, PerformanceDegradation
};

class IDEDiagnosticSystem {
    // Monitoring, health score, performance profiling
};
```

---

## 29. QUALITY, BACKUP & GOVERNANCE

### Zero Retention Manager
**File**: `src/terminal/zero_retention_manager.hpp`
- GDPR/privacy data retention
- Automatic deletion policies
- Session cleanup, secure wipe
- Data anonymization, audit logging

### Quality/Workflow Commands
- Shortcut editor & reset
- Backup create/restore/auto/list/prune
- Alert monitoring & history
- SLO dashboard
- Resource status monitoring

---

## 30. AGENT TOOL SCHEMAS (OPENAI FUNCTION CALLING)

From `src/agentic/ToolRegistry.h`, auto-generated schemas:

```json
{
  "tools": [
    {
      "type": "function",
      "function": {
        "name": "read_file",
        "description": "Read a file's content",
        "parameters": { "type": "object", "properties": { "path": {"type":"string"} }, "required": ["path"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "write_file",
        "description": "Write content to a file",
        "parameters": { "type": "object", "properties": { "path": {"type":"string"}, "content": {"type":"string"} }, "required": ["path","content"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "replace_in_file",
        "description": "Replace text in a file",
        "parameters": { "type": "object", "properties": { "path": {"type":"string"}, "old_text": {"type":"string"}, "new_text": {"type":"string"} }, "required": ["path","old_text","new_text"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "execute_command",
        "description": "Execute a shell command",
        "parameters": { "type": "object", "properties": { "command": {"type":"string"}, "cwd": {"type":"string"} }, "required": ["command"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "search_code",
        "description": "Search for patterns in code",
        "parameters": { "type": "object", "properties": { "pattern": {"type":"string"}, "path": {"type":"string"} }, "required": ["pattern"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "get_diagnostics",
        "description": "Get compiler diagnostics",
        "parameters": { "type": "object", "properties": { "path": {"type":"string"} } }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "list_directory",
        "description": "List directory contents",
        "parameters": { "type": "object", "properties": { "path": {"type":"string"}, "recursive": {"type":"boolean"} }, "required": ["path"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "get_coverage",
        "description": "Get test coverage data",
        "parameters": { "type": "object", "properties": {} }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "run_build",
        "description": "Build the project",
        "parameters": { "type": "object", "properties": { "target": {"type":"string"}, "config": {"type":"string"} } }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "apply_hotpatch",
        "description": "Apply a hotpatch",
        "parameters": { "type": "object", "properties": { "layer": {"type":"string"}, "patch": {"type":"string"} }, "required": ["layer","patch"] }
      }
    },
    {
      "type": "function",
      "function": {
        "name": "disk_recovery",
        "description": "Disk recovery operations",
        "parameters": { "type": "object", "properties": { "operation": {"type":"string"} }, "required": ["operation"] }
      }
    }
  ]
}
```

---

## SUMMARY STATISTICS

| Category | Count |
|----------|-------|
| Total SSOT Commands | ~250+ (in COMMAND_TABLE) |
| GUI-Routable Commands | ~200+ |
| CLI Commands | ~230+ |
| Header Files Read | ~100 |
| Major Subsystems | 30 |
| Agent Tools (Function Calling) | 11 |
| Hotpatch Layers | 5 (L0-L5) |
| AI Backends Supported | 5 (Local GGUF, Ollama, OpenAI, Claude, Gemini) |
| Themes | 16 |
| Debug Operations | 28 |
| Reverse Engineering Operations | 26 |
| Voice Commands | 17 |
| Swarm Commands | 25 |
| LSP Custom Methods | 9 |

---

## KEY ARCHITECTURAL PATTERNS FOR AGENT TRAINING

1. **All commands go through `COMMAND_TABLE` X-macro** — no manual wiring
2. **PatchResult/CommandResult structured returns** — never raw booleans
3. **Factory static methods** — `::ok()` and `::error()` constructors
4. **Function pointers for callbacks** — not `std::function` in hot paths
5. **Mutex with `lock_guard` only** — no manual unlock, no recursive locks
6. **CLI prefix `!`** — all CLI commands start with `!`
7. **`uintptr_t` for pointer math** — never raw casts
8. **Agent loop bounded at 8 steps** — configurable via `BoundedAgentLoop`
9. **All async ops use `CMD_ASYNC` flag** — routed through governor
10. **No exceptions anywhere** — structured error returns throughout
