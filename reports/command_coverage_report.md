# RawrXD Command Registry Coverage Report

Generated: 2026-03-26 14:19:15

## Summary

| Metric | Count |
|---|---|
| Total IDM_* defines found | 727 |
| Already registered (manual) | 0 |
| Auto-registered (this run) | 710 |
| **Total after auto-reg** | **710** |
| Handler declarations found | 1154 |
| With existing handler | 710 |
| With stub handler | 0 |
| Coverage | 97.7% |

## Auto-Registered Commands

### AGENT (21 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| agent.start_loop | `!agent_start_loop` | IDM_AGENT_START_LOOP | 4100 | handleAgentStartLoop | REAL |
| agent.execute_cmd | `!agent_execute_cmd` | IDM_AGENT_EXECUTE_CMD | 4101 | handleAgentExecuteCmd | REAL |
| agent.configure_model | `!agent_configure_model` | IDM_AGENT_CONFIGURE_MODEL | 4102 | handleAgentConfigureModel | REAL |
| agent.view_tools | `!agent_view_tools` | IDM_AGENT_VIEW_TOOLS | 4103 | handleAgentViewTools | REAL |
| agent.view_status | `!agent_view_status` | IDM_AGENT_VIEW_STATUS | 4104 | handleAgentViewStatus | REAL |
| agent.stop | `!agent_stop` | IDM_AGENT_STOP | 4105 | handleAgentStop | REAL |
| agent.memory | `!agent_memory` | IDM_AGENT_MEMORY | 4106 | handleAgentMemory | REAL |
| agent.memory_view | `!agent_memory_view` | IDM_AGENT_MEMORY_VIEW | 4107 | handleAgentMemoryView | REAL |
| agent.memory_clear | `!agent_memory_clear` | IDM_AGENT_MEMORY_CLEAR | 4108 | handleAgentMemoryClear | REAL |
| agent.memory_export | `!agent_memory_export` | IDM_AGENT_MEMORY_EXPORT | 4109 | handleAgentMemoryExport | REAL |
| agent.bounded_loop | `!agent_bounded_loop` | IDM_AGENT_BOUNDED_LOOP | 4120 | handleAgentBoundedLoop | REAL |
| agent.autonomous_communicator | `!agent_autonomous_communicator` | IDM_AGENT_AUTONOMOUS_COMMUNICATOR | 4163 | handleAgentAutonomousCommunicator | REAL |
| agent.compact_conversation | `!agent_compact_conversation` | IDM_AGENT_COMPACT_CONVERSATION | 4165 | handleAgentCompactConversation | REAL |
| agent.optimize_tool_selection | `!agent_optimize_tool_selection` | IDM_AGENT_OPTIMIZE_TOOL_SELECTION | 4166 | handleAgentOptimizeToolSelection | REAL |
| agent.resolving_status | `!agent_resolving_status` | IDM_AGENT_RESOLVING_STATUS | 4167 | handleAgentResolvingStatus | REAL |
| agent.read_lines | `!agent_read_lines` | IDM_AGENT_READ_LINES | 4168 | handleAgentReadLines | REAL |
| agent.planning_exploration | `!agent_planning_exploration` | IDM_AGENT_PLANNING_EXPLORATION | 4169 | handleAgentPlanningExploration | REAL |
| agent.search_files | `!agent_search_files` | IDM_AGENT_SEARCH_FILES | 4170 | handleAgentSearchFiles | REAL |
| agent.evaluate_integration | `!agent_evaluate_integration` | IDM_AGENT_EVALUATE_INTEGRATION | 4171 | handleAgentEvaluateIntegration | REAL |
| agent.smoke_test | `!agent_smoke_test` | IDM_AGENT_SMOKE_TEST | 5320 | handleAgentSmokeTest | REAL |
| agent.set_cycle_agent_counter | `!agent_set_cycle_agent_counter` | IDM_AGENT_SET_CYCLE_AGENT_COUNTER | 5321 | handleAgentSetCycleAgentCounter | REAL |

### AI (33 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| ai.inline_complete | `!ai_inline_complete` | IDM_AI_INLINE_COMPLETE | 401 | handleAiInlineComplete | REAL |
| ai.chat_mode | `!ai_chat_mode` | IDM_AI_CHAT_MODE | 402 | handleAiChatMode | REAL |
| ai.explain_code | `!ai_explain_code` | IDM_AI_EXPLAIN_CODE | 403 | handleAiExplainCode | REAL |
| ai.refactor | `!ai_refactor` | IDM_AI_REFACTOR | 404 | handleAiRefactor | REAL |
| ai.generate_tests | `!ai_generate_tests` | IDM_AI_GENERATE_TESTS | 405 | handleAiGenerateTests | REAL |
| ai.generate_docs | `!ai_generate_docs` | IDM_AI_GENERATE_DOCS | 406 | handleAiGenerateDocs | REAL |
| ai.fix_errors | `!ai_fix_errors` | IDM_AI_FIX_ERRORS | 407 | handleAiFixErrors | REAL |
| ai.optimize_code | `!ai_optimize_code` | IDM_AI_OPTIMIZE_CODE | 408 | handleAiOptimizeCode | REAL |
| ai.model_select | `!ai_model_select` | IDM_AI_MODEL_SELECT | 409 | handleAiModelSelect | REAL |
| ai.mode_max | `!ai_mode_max` | IDM_AI_MODE_MAX | 4200 | handleAiModeMax | REAL |
| ai.mode_deep_think | `!ai_mode_deep_think` | IDM_AI_MODE_DEEP_THINK | 4201 | handleAiModeDeepThink | REAL |
| ai.mode_deep_research | `!ai_mode_deep_research` | IDM_AI_MODE_DEEP_RESEARCH | 4202 | handleAiModeDeepResearch | REAL |
| ai.mode_no_refusal | `!ai_mode_no_refusal` | IDM_AI_MODE_NO_REFUSAL | 4203 | handleAiModeNoRefusal | REAL |
| ai.mode_workflow_executor | `!ai_mode_workflow_executor` | IDM_AI_MODE_WORKFLOW_EXECUTOR | 4204 | handleAiModeWorkflowExecutor | REAL |
| ai.context_4k | `!ai_context_4k` | IDM_AI_CONTEXT_4K | 4210 | handleAiContext4k | REAL |
| ai.context_32k | `!ai_context_32k` | IDM_AI_CONTEXT_32K | 4211 | handleAiContext32k | REAL |
| ai.context_64k | `!ai_context_64k` | IDM_AI_CONTEXT_64K | 4212 | handleAiContext64k | REAL |
| ai.context_128k | `!ai_context_128k` | IDM_AI_CONTEXT_128K | 4213 | handleAiContext128k | REAL |
| ai.context_256k | `!ai_context_256k` | IDM_AI_CONTEXT_256K | 4214 | handleAiContext256k | REAL |
| ai.context_512k | `!ai_context_512k` | IDM_AI_CONTEXT_512K | 4215 | handleAiContext512k | REAL |
| ai.context_1m | `!ai_context_1m` | IDM_AI_CONTEXT_1M | 4216 | handleAiContext1m | REAL |
| ai.agent_cycles_set | `!ai_agent_cycles_set` | IDM_AI_AGENT_CYCLES_SET | 4217 | handleAiAgentCyclesSet | REAL |
| ai.agent_multi_enable | `!ai_agent_multi_enable` | IDM_AI_AGENT_MULTI_ENABLE | 4218 | handleAiAgentMultiEnable | REAL |
| ai.agent_multi_disable | `!ai_agent_multi_disable` | IDM_AI_AGENT_MULTI_DISABLE | 4219 | handleAiAgentMultiDisable | REAL |
| ai.agent_multi_status | `!ai_agent_multi_status` | IDM_AI_AGENT_MULTI_STATUS | 4240 | handleAiAgentMultiStatus | REAL |
| ai.titan_toggle | `!ai_titan_toggle` | IDM_AI_TITAN_TOGGLE | 4241 | handleAiTitanToggle | REAL |
| ai.800b_status | `!ai_800b_status` | IDM_AI_800B_STATUS | 4242 | handleAi800bStatus | REAL |
| ai.model_registry | `!ai_model_registry` | IDM_AI_MODEL_REGISTRY | 5300 | handleAiModelRegistry | REAL |
| ai.checkpoint_mgr | `!ai_checkpoint_mgr` | IDM_AI_CHECKPOINT_MGR | 5301 | handleAiCheckpointMgr | REAL |
| ai.interpret_panel | `!ai_interpret_panel` | IDM_AI_INTERPRET_PANEL | 5302 | handleAiInterpretPanel | REAL |
| ai.cicd_settings | `!ai_cicd_settings` | IDM_AI_CICD_SETTINGS | 5303 | handleAiCicdSettings | REAL |
| ai.multi_file_search | `!ai_multi_file_search` | IDM_AI_MULTI_FILE_SEARCH | 5304 | handleAiMultiFileSearch | REAL |
| ai.benchmark_menu | `!ai_benchmark_menu` | IDM_AI_BENCHMARK_MENU | 5305 | handleAiBenchmarkMenu | REAL |

### ASM (13 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| asm.parse_symbols | `!asm_parse_symbols` | IDM_ASM_PARSE_SYMBOLS | 5082 | handleAsmParseSymbols | REAL |
| asm.goto_label | `!asm_goto_label` | IDM_ASM_GOTO_LABEL | 5083 | handleAsmGotoLabel | REAL |
| asm.find_label_refs | `!asm_find_label_refs` | IDM_ASM_FIND_LABEL_REFS | 5084 | handleAsmFindLabelRefs | REAL |
| asm.show_symbol_table | `!asm_show_symbol_table` | IDM_ASM_SHOW_SYMBOL_TABLE | 5085 | handleAsmShowSymbolTable | REAL |
| asm.instruction_info | `!asm_instruction_info` | IDM_ASM_INSTRUCTION_INFO | 5086 | handleAsmInstructionInfo | REAL |
| asm.register_info | `!asm_register_info` | IDM_ASM_REGISTER_INFO | 5087 | handleAsmRegisterInfo | REAL |
| asm.analyze_block | `!asm_analyze_block` | IDM_ASM_ANALYZE_BLOCK | 5088 | handleAsmAnalyzeBlock | REAL |
| asm.show_call_graph | `!asm_show_call_graph` | IDM_ASM_SHOW_CALL_GRAPH | 5089 | handleAsmShowCallGraph | REAL |
| asm.show_data_flow | `!asm_show_data_flow` | IDM_ASM_SHOW_DATA_FLOW | 5090 | handleAsmShowDataFlow | REAL |
| asm.detect_convention | `!asm_detect_convention` | IDM_ASM_DETECT_CONVENTION | 5091 | handleAsmDetectConvention | REAL |
| asm.show_sections | `!asm_show_sections` | IDM_ASM_SHOW_SECTIONS | 5092 | handleAsmShowSections | REAL |
| asm.clear_symbols | `!asm_clear_symbols` | IDM_ASM_CLEAR_SYMBOLS | 5093 | handleAsmClearSymbols | REAL |
| asm.lint_current_file | `!asm_lint_current_file` | IDM_ASM_LINT_CURRENT_FILE | 5999 | handleAsmLintCurrentFile | REAL |

### AUDIT (7 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| audit.show_dashboard | `!audit_show_dashboard` | IDM_AUDIT_SHOW_DASHBOARD | 9500 | handleAuditShowDashboard | REAL |
| audit.run_full | `!audit_run_full` | IDM_AUDIT_RUN_FULL | 9501 | handleAuditRunFull | REAL |
| audit.detect_stubs | `!audit_detect_stubs` | IDM_AUDIT_DETECT_STUBS | 9502 | handleAuditDetectStubs | REAL |
| audit.check_menus | `!audit_check_menus` | IDM_AUDIT_CHECK_MENUS | 9503 | handleAuditCheckMenus | REAL |
| audit.run_tests | `!audit_run_tests` | IDM_AUDIT_RUN_TESTS | 9504 | handleAuditRunTests | REAL |
| audit.export_report | `!audit_export_report` | IDM_AUDIT_EXPORT_REPORT | 9505 | handleAuditExportReport | REAL |
| audit.quick_stats | `!audit_quick_stats` | IDM_AUDIT_QUICK_STATS | 9506 | handleAuditQuickStats | REAL |

### AUTONOMY (6 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| autonomy.toggle | `!autonomy_toggle` | IDM_AUTONOMY_TOGGLE | 4150 | handleAutonomyToggle | REAL |
| autonomy.start | `!autonomy_start` | IDM_AUTONOMY_START | 4151 | handleAutonomyStart | REAL |
| autonomy.stop | `!autonomy_stop` | IDM_AUTONOMY_STOP | 4152 | handleAutonomyStop | REAL |
| autonomy.set_goal | `!autonomy_set_goal` | IDM_AUTONOMY_SET_GOAL | 4153 | handleAutonomySetGoal | REAL |
| autonomy.status | `!autonomy_status` | IDM_AUTONOMY_STATUS | 4154 | handleAutonomyStatus | REAL |
| autonomy.memory | `!autonomy_memory` | IDM_AUTONOMY_MEMORY | 4155 | handleAutonomyMemory | REAL |

### BACKEND (11 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| backend.switch_local | `!backend_switch_local` | IDM_BACKEND_SWITCH_LOCAL | 5037 | handleBackendSwitchLocal | REAL |
| backend.switch_ollama | `!backend_switch_ollama` | IDM_BACKEND_SWITCH_OLLAMA | 5038 | handleBackendSwitchOllama | REAL |
| backend.switch_openai | `!backend_switch_openai` | IDM_BACKEND_SWITCH_OPENAI | 5039 | handleBackendSwitchOpenai | REAL |
| backend.switch_claude | `!backend_switch_claude` | IDM_BACKEND_SWITCH_CLAUDE | 5040 | handleBackendSwitchClaude | REAL |
| backend.switch_gemini | `!backend_switch_gemini` | IDM_BACKEND_SWITCH_GEMINI | 5041 | handleBackendSwitchGemini | REAL |
| backend.show_status | `!backend_show_status` | IDM_BACKEND_SHOW_STATUS | 5042 | handleBackendShowStatus | REAL |
| backend.show_switcher | `!backend_show_switcher` | IDM_BACKEND_SHOW_SWITCHER | 5043 | handleBackendShowSwitcher | REAL |
| backend.configure | `!backend_configure` | IDM_BACKEND_CONFIGURE | 5044 | handleBackendConfigure | REAL |
| backend.health_check | `!backend_health_check` | IDM_BACKEND_HEALTH_CHECK | 5045 | handleBackendHealthCheck | REAL |
| backend.set_api_key | `!backend_set_api_key` | IDM_BACKEND_SET_API_KEY | 5046 | handleBackendSetApiKey | REAL |
| backend.save_configs | `!backend_save_configs` | IDM_BACKEND_SAVE_CONFIGS | 5047 | handleBackendSaveConfigs | REAL |

### BUILD (12 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| build.build | `!build_build` | IDM_BUILD_BUILD | 2201 | handleBuildBuild | REAL |
| build.run | `!build_run` | IDM_BUILD_RUN | 2203 | handleBuildRun | REAL |
| build.stop | `!build_stop` | IDM_BUILD_STOP | 2205 | handleBuildStop | REAL |
| build.project | `!build_project` | IDM_BUILD_PROJECT | 2801 | handleBuildProject | REAL |
| build.config_debug | `!build_config_debug` | IDM_BUILD_CONFIG_DEBUG | 2806 | handleBuildConfigDebug | REAL |
| build.config_release | `!build_config_release` | IDM_BUILD_CONFIG_RELEASE | 2807 | handleBuildConfigRelease | REAL |
| build.asm_current | `!build_asm_current` | IDM_BUILD_ASM_CURRENT | 2808 | handleBuildAsmCurrent | REAL |
| build.set_target | `!build_set_target` | IDM_BUILD_SET_TARGET | 2809 | handleBuildSetTarget | REAL |
| build.show_log | `!build_show_log` | IDM_BUILD_SHOW_LOG | 2810 | handleBuildShowLog | REAL |
| build.solution | `!build_solution` | IDM_BUILD_SOLUTION | 10400 | handleBuildSolution | REAL |
| build.clean | `!build_clean` | IDM_BUILD_CLEAN | 10401 | handleBuildClean | REAL |
| build.rebuild | `!build_rebuild` | IDM_BUILD_REBUILD | 10402 | handleBuildRebuild | REAL |

### CONFIDENCE (2 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| confidence.status | `!confidence_status` | IDM_CONFIDENCE_STATUS | 5130 | handleConfidenceStatus | REAL |
| confidence.set_policy | `!confidence_set_policy` | IDM_CONFIDENCE_SET_POLICY | 5131 | handleConfidenceSetPolicy | REAL |

### DBG (28 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| dbg.launch | `!dbg_launch` | IDM_DBG_LAUNCH | 5157 | handleDbgLaunch | REAL |
| dbg.attach | `!dbg_attach` | IDM_DBG_ATTACH | 5158 | handleDbgAttach | REAL |
| dbg.detach | `!dbg_detach` | IDM_DBG_DETACH | 5159 | handleDbgDetach | REAL |
| dbg.go | `!dbg_go` | IDM_DBG_GO | 5160 | handleDbgGo | REAL |
| dbg.step_over | `!dbg_step_over` | IDM_DBG_STEP_OVER | 5161 | handleDbgStepOver | REAL |
| dbg.step_into | `!dbg_step_into` | IDM_DBG_STEP_INTO | 5162 | handleDbgStepInto | REAL |
| dbg.step_out | `!dbg_step_out` | IDM_DBG_STEP_OUT | 5163 | handleDbgStepOut | REAL |
| dbg.break | `!dbg_break` | IDM_DBG_BREAK | 5164 | handleDbgBreak | REAL |
| dbg.kill | `!dbg_kill` | IDM_DBG_KILL | 5165 | handleDbgKill | REAL |
| dbg.add_bp | `!dbg_add_bp` | IDM_DBG_ADD_BP | 5166 | handleDbgAddBp | REAL |
| dbg.remove_bp | `!dbg_remove_bp` | IDM_DBG_REMOVE_BP | 5167 | handleDbgRemoveBp | REAL |
| dbg.enable_bp | `!dbg_enable_bp` | IDM_DBG_ENABLE_BP | 5168 | handleDbgEnableBp | REAL |
| dbg.clear_bps | `!dbg_clear_bps` | IDM_DBG_CLEAR_BPS | 5169 | handleDbgClearBps | REAL |
| dbg.list_bps | `!dbg_list_bps` | IDM_DBG_LIST_BPS | 5170 | handleDbgListBps | REAL |
| dbg.add_watch | `!dbg_add_watch` | IDM_DBG_ADD_WATCH | 5171 | handleDbgAddWatch | REAL |
| dbg.remove_watch | `!dbg_remove_watch` | IDM_DBG_REMOVE_WATCH | 5172 | handleDbgRemoveWatch | REAL |
| dbg.registers | `!dbg_registers` | IDM_DBG_REGISTERS | 5173 | handleDbgRegisters | REAL |
| dbg.stack | `!dbg_stack` | IDM_DBG_STACK | 5174 | handleDbgStack | REAL |
| dbg.memory | `!dbg_memory` | IDM_DBG_MEMORY | 5175 | handleDbgMemory | REAL |
| dbg.disasm | `!dbg_disasm` | IDM_DBG_DISASM | 5176 | handleDbgDisasm | REAL |
| dbg.modules | `!dbg_modules` | IDM_DBG_MODULES | 5177 | handleDbgModules | REAL |
| dbg.threads | `!dbg_threads` | IDM_DBG_THREADS | 5178 | handleDbgThreads | REAL |
| dbg.switch_thread | `!dbg_switch_thread` | IDM_DBG_SWITCH_THREAD | 5179 | handleDbgSwitchThread | REAL |
| dbg.evaluate | `!dbg_evaluate` | IDM_DBG_EVALUATE | 5180 | handleDbgEvaluate | REAL |
| dbg.set_register | `!dbg_set_register` | IDM_DBG_SET_REGISTER | 5181 | handleDbgSetRegister | REAL |
| dbg.search_memory | `!dbg_search_memory` | IDM_DBG_SEARCH_MEMORY | 5182 | handleDbgSearchMemory | REAL |
| dbg.symbol_path | `!dbg_symbol_path` | IDM_DBG_SYMBOL_PATH | 5183 | handleDbgSymbolPath | REAL |
| dbg.status | `!dbg_status` | IDM_DBG_STATUS | 5184 | handleDbgStatus | REAL |

### DEBUG (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| debug.start | `!debug_start` | IDM_DEBUG_START | 3001 | handleDebugStart | REAL |
| debug.stop | `!debug_stop` | IDM_DEBUG_STOP | 3002 | handleDebugStop | REAL |
| debug.test_ai | `!debug_test_ai` | IDM_DEBUG_TEST_AI | 5190 | handleDebugTestAi | REAL |

### DECOMP (6 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| decomp.rename_var | `!decomp_rename_var` | IDM_DECOMP_RENAME_VAR | 8001 | handleDecompRenameVar | REAL |
| decomp.goto_def | `!decomp_goto_def` | IDM_DECOMP_GOTO_DEF | 8002 | handleDecompGotoDef | REAL |
| decomp.find_refs | `!decomp_find_refs` | IDM_DECOMP_FIND_REFS | 8003 | handleDecompFindRefs | REAL |
| decomp.copy_line | `!decomp_copy_line` | IDM_DECOMP_COPY_LINE | 8004 | handleDecompCopyLine | REAL |
| decomp.copy_all | `!decomp_copy_all` | IDM_DECOMP_COPY_ALL | 8005 | handleDecompCopyAll | REAL |
| decomp.goto_addr | `!decomp_goto_addr` | IDM_DECOMP_GOTO_ADDR | 8006 | handleDecompGotoAddr | REAL |

### EDIT (20 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| edit.selectall | `!edit_selectall` | IDM_EDIT_SELECTALL | 208 | handleEditSelectall | REAL |
| edit.multicursor_add | `!edit_multicursor_add` | IDM_EDIT_MULTICURSOR_ADD | 209 | handleEditMulticursorAdd | REAL |
| edit.multicursor_remove | `!edit_multicursor_remove` | IDM_EDIT_MULTICURSOR_REMOVE | 210 | handleEditMulticursorRemove | REAL |
| edit.goto_line | `!edit_goto_line` | IDM_EDIT_GOTO_LINE | 211 | handleEditGotoLine | REAL |
| edit.toggle_comment | `!edit_toggle_comment` | IDM_EDIT_TOGGLE_COMMENT | 212 | handleEditToggleComment | REAL |
| edit.select_all | `!edit_select_all` | IDM_EDIT_SELECT_ALL | 2006 | handleEditSelectAll | REAL |
| edit.undo | `!edit_undo` | IDM_EDIT_UNDO | 2007 | handleEditUndo | REAL |
| edit.redo | `!edit_redo` | IDM_EDIT_REDO | 2008 | handleEditRedo | REAL |
| edit.cut | `!edit_cut` | IDM_EDIT_CUT | 2009 | handleEditCut | REAL |
| edit.copy | `!edit_copy` | IDM_EDIT_COPY | 2010 | handleEditCopy | REAL |
| edit.paste | `!edit_paste` | IDM_EDIT_PASTE | 2011 | handleEditPaste | REAL |
| edit.snippet | `!edit_snippet` | IDM_EDIT_SNIPPET | 2012 | handleEditSnippet | REAL |
| edit.copy_format | `!edit_copy_format` | IDM_EDIT_COPY_FORMAT | 2013 | handleEditCopyFormat | REAL |
| edit.paste_plain | `!edit_paste_plain` | IDM_EDIT_PASTE_PLAIN | 2014 | handleEditPastePlain | REAL |
| edit.clipboard_history | `!edit_clipboard_history` | IDM_EDIT_CLIPBOARD_HISTORY | 2015 | handleEditClipboardHistory | REAL |
| edit.find | `!edit_find` | IDM_EDIT_FIND | 2016 | handleEditFind | REAL |
| edit.replace | `!edit_replace` | IDM_EDIT_REPLACE | 2017 | handleEditReplace | REAL |
| edit.find_next | `!edit_find_next` | IDM_EDIT_FIND_NEXT | 2018 | handleEditFindNext | REAL |
| edit.find_prev | `!edit_find_prev` | IDM_EDIT_FIND_PREV | 2019 | handleEditFindPrev | REAL |
| edit.goto | `!edit_goto` | IDM_EDIT_GOTO | 2109 | handleEditGoto | REAL |

### EDITOR (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| editor.engine_richedit_cmd | `!editor_engine_richedit_cmd` | IDM_EDITOR_ENGINE_RICHEDIT_CMD | 9300 | handleEditorEngineRicheditCmd | REAL |
| editor.engine_webview2_cmd | `!editor_engine_webview2_cmd` | IDM_EDITOR_ENGINE_WEBVIEW2_CMD | 9301 | handleEditorEngineWebview2Cmd | REAL |
| editor.engine_monacocore_cmd | `!editor_engine_monacocore_cmd` | IDM_EDITOR_ENGINE_MONACOCORE_CMD | 9302 | handleEditorEngineMonacocoreCmd | REAL |
| editor.engine_cycle_cmd | `!editor_engine_cycle_cmd` | IDM_EDITOR_ENGINE_CYCLE_CMD | 9303 | handleEditorEngineCycleCmd | REAL |
| editor.engine_status_cmd | `!editor_engine_status_cmd` | IDM_EDITOR_ENGINE_STATUS_CMD | 9304 | handleEditorEngineStatusCmd | REAL |

### ENGINE (2 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| engine.unlock_800b | `!engine_unlock_800b` | IDM_ENGINE_UNLOCK_800B | 4220 | handleEngineUnlock800b | REAL |
| engine.load_800b | `!engine_load_800b` | IDM_ENGINE_LOAD_800B | 4221 | handleEngineLoad800b | REAL |

### ENT (18 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| ent.multi_gpu_balance | `!ent_multi_gpu_balance` | IDM_ENT_MULTI_GPU_BALANCE | 3042 | handleEntMultiGpuBalance | REAL |
| ent.dynamic_batch | `!ent_dynamic_batch` | IDM_ENT_DYNAMIC_BATCH | 3043 | handleEntDynamicBatch | REAL |
| ent.api_key_mgmt | `!ent_api_key_mgmt` | IDM_ENT_API_KEY_MGMT | 3044 | handleEntApiKeyMgmt | REAL |
| ent.audit_logs | `!ent_audit_logs` | IDM_ENT_AUDIT_LOGS | 3045 | handleEntAuditLogs | REAL |
| ent.rawr_tuner | `!ent_rawr_tuner` | IDM_ENT_RAWR_TUNER | 3046 | handleEntRawrTuner | REAL |
| ent.dual_engine | `!ent_dual_engine` | IDM_ENT_DUAL_ENGINE | 3047 | handleEntDualEngine | REAL |
| ent.model_compare | `!ent_model_compare` | IDM_ENT_MODEL_COMPARE | 12330 | handleEntModelCompare | REAL |
| ent.batch_process | `!ent_batch_process` | IDM_ENT_BATCH_PROCESS | 12331 | handleEntBatchProcess | REAL |
| ent.custom_stop_seq | `!ent_custom_stop_seq` | IDM_ENT_CUSTOM_STOP_SEQ | 12332 | handleEntCustomStopSeq | REAL |
| ent.grammar_constraints | `!ent_grammar_constraints` | IDM_ENT_GRAMMAR_CONSTRAINTS | 12333 | handleEntGrammarConstraints | REAL |
| ent.lora_adapter | `!ent_lora_adapter` | IDM_ENT_LORA_ADAPTER | 12334 | handleEntLoraAdapter | REAL |
| ent.response_cache | `!ent_response_cache` | IDM_ENT_RESPONSE_CACHE | 12335 | handleEntResponseCache | REAL |
| ent.prompt_library | `!ent_prompt_library` | IDM_ENT_PROMPT_LIBRARY | 12336 | handleEntPromptLibrary | REAL |
| ent.session_export_import | `!ent_session_export_import` | IDM_ENT_SESSION_EXPORT_IMPORT | 12337 | handleEntSessionExportImport | REAL |
| ent.model_sharding | `!ent_model_sharding` | IDM_ENT_MODEL_SHARDING | 12338 | handleEntModelSharding | REAL |
| ent.tensor_parallel | `!ent_tensor_parallel` | IDM_ENT_TENSOR_PARALLEL | 12339 | handleEntTensorParallel | REAL |
| ent.pipeline_parallel | `!ent_pipeline_parallel` | IDM_ENT_PIPELINE_PARALLEL | 12340 | handleEntPipelineParallel | REAL |
| ent.custom_quant | `!ent_custom_quant` | IDM_ENT_CUSTOM_QUANT | 12341 | handleEntCustomQuant | REAL |

### EXT (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| ext.install | `!ext_install` | IDM_EXT_INSTALL | 11810 | handleExtInstall | REAL |
| ext.enable | `!ext_enable` | IDM_EXT_ENABLE | 11811 | handleExtEnable | REAL |
| ext.disable | `!ext_disable` | IDM_EXT_DISABLE | 11812 | handleExtDisable | REAL |
| ext.uninstall | `!ext_uninstall` | IDM_EXT_UNINSTALL | 11813 | handleExtUninstall | REAL |
| ext.reload | `!ext_reload` | IDM_EXT_RELOAD | 11814 | handleExtReload | REAL |

### FAILURE (15 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| failure.detect | `!failure_detect` | IDM_FAILURE_DETECT | 4281 | handleFailureDetect | REAL |
| failure.analyze | `!failure_analyze` | IDM_FAILURE_ANALYZE | 4282 | handleFailureAnalyze | REAL |
| failure.show_queue | `!failure_show_queue` | IDM_FAILURE_SHOW_QUEUE | 4283 | handleFailureShowQueue | REAL |
| failure.show_history | `!failure_show_history` | IDM_FAILURE_SHOW_HISTORY | 4284 | handleFailureShowHistory | REAL |
| failure.generate_recovery | `!failure_generate_recovery` | IDM_FAILURE_GENERATE_RECOVERY | 4285 | handleFailureGenerateRecovery | REAL |
| failure.execute_recovery | `!failure_execute_recovery` | IDM_FAILURE_EXECUTE_RECOVERY | 4286 | handleFailureExecuteRecovery | REAL |
| failure.autonomous_heal | `!failure_autonomous_heal` | IDM_FAILURE_AUTONOMOUS_HEAL | 4287 | handleFailureAutonomousHeal | REAL |
| failure.view_patterns | `!failure_view_patterns` | IDM_FAILURE_VIEW_PATTERNS | 4288 | handleFailureViewPatterns | REAL |
| failure.learn_pattern | `!failure_learn_pattern` | IDM_FAILURE_LEARN_PATTERN | 4289 | handleFailureLearnPattern | REAL |
| failure.stats | `!failure_stats` | IDM_FAILURE_STATS | 4290 | handleFailureStats | REAL |
| failure.set_policy | `!failure_set_policy` | IDM_FAILURE_SET_POLICY | 4291 | handleFailureSetPolicy | REAL |
| failure.show_health | `!failure_show_health` | IDM_FAILURE_SHOW_HEALTH | 4292 | handleFailureShowHealth | REAL |
| failure.export_analysis | `!failure_export_analysis` | IDM_FAILURE_EXPORT_ANALYSIS | 4293 | handleFailureExportAnalysis | REAL |
| failure.clear_history | `!failure_clear_history` | IDM_FAILURE_CLEAR_HISTORY | 4294 | handleFailureClearHistory | REAL |
| failure.diagnostics | `!failure_diagnostics` | IDM_FAILURE_DIAGNOSTICS | 4295 | handleFailureDiagnostics | REAL |

### FILE (19 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| file.autosave | `!file_autosave` | IDM_FILE_AUTOSAVE | 105 | handleFileAutosave | REAL |
| file.close_folder | `!file_close_folder` | IDM_FILE_CLOSE_FOLDER | 106 | handleFileCloseFolder | REAL |
| file.open_folder | `!file_open_folder` | IDM_FILE_OPEN_FOLDER | 108 | handleFileOpenFolder | REAL |
| file.new_window | `!file_new_window` | IDM_FILE_NEW_WINDOW | 109 | handleFileNewWindow | REAL |
| file.close_tab | `!file_close_tab` | IDM_FILE_CLOSE_TAB | 110 | handleFileCloseTab | REAL |
| file.saveall | `!file_saveall` | IDM_FILE_SAVEALL | 1005 | handleFileSaveall | REAL |
| file.close | `!file_close` | IDM_FILE_CLOSE | 1006 | handleFileClose | REAL |
| file.recent_clear | `!file_recent_clear` | IDM_FILE_RECENT_CLEAR | 1020 | handleFileRecentClear | REAL |
| file.load_model | `!file_load_model` | IDM_FILE_LOAD_MODEL | 1030 | handleFileLoadModel | REAL |
| file.model_from_hf | `!file_model_from_hf` | IDM_FILE_MODEL_FROM_HF | 1031 | handleFileModelFromHf | REAL |
| file.model_from_ollama | `!file_model_from_ollama` | IDM_FILE_MODEL_FROM_OLLAMA | 1032 | handleFileModelFromOllama | REAL |
| file.model_from_url | `!file_model_from_url` | IDM_FILE_MODEL_FROM_URL | 1033 | handleFileModelFromUrl | REAL |
| file.model_unified | `!file_model_unified` | IDM_FILE_MODEL_UNIFIED | 1034 | handleFileModelUnified | REAL |
| file.model_quick_load | `!file_model_quick_load` | IDM_FILE_MODEL_QUICK_LOAD | 1035 | handleFileModelQuickLoad | REAL |
| file.new | `!file_new` | IDM_FILE_NEW | 2001 | handleFileNew | REAL |
| file.open | `!file_open` | IDM_FILE_OPEN | 2002 | handleFileOpen | REAL |
| file.save | `!file_save` | IDM_FILE_SAVE | 2003 | handleFileSave | REAL |
| file.saveas | `!file_saveas` | IDM_FILE_SAVEAS | 2004 | handleFileSaveas | REAL |
| file.exit | `!file_exit` | IDM_FILE_EXIT | 2005 | handleFileExit | REAL |

### GAUNTLET (2 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| gauntlet.run | `!gauntlet_run` | IDM_GAUNTLET_RUN | 9600 | handleGauntletRun | REAL |
| gauntlet.export | `!gauntlet_export` | IDM_GAUNTLET_EXPORT | 9601 | handleGauntletExport | REAL |

### GIT (14 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| git.status | `!git_status` | IDM_GIT_STATUS | 3020 | handleGitStatus | REAL |
| git.commit | `!git_commit` | IDM_GIT_COMMIT | 3021 | handleGitCommit | REAL |
| git.push | `!git_push` | IDM_GIT_PUSH | 3022 | handleGitPush | REAL |
| git.pull | `!git_pull` | IDM_GIT_PULL | 3023 | handleGitPull | REAL |
| git.panel | `!git_panel` | IDM_GIT_PANEL | 3024 | handleGitPanel | REAL |
| git.stage_all | `!git_stage_all` | IDM_GIT_STAGE_ALL | 9850 | handleGitStageAll | REAL |
| git.unstage_all | `!git_unstage_all` | IDM_GIT_UNSTAGE_ALL | 9851 | handleGitUnstageAll | REAL |
| git.diff | `!git_diff` | IDM_GIT_DIFF | 9852 | handleGitDiff | REAL |
| git.branch_picker | `!git_branch_picker` | IDM_GIT_BRANCH_PICKER | 9853 | handleGitBranchPicker | REAL |
| git.stash | `!git_stash` | IDM_GIT_STASH | 9854 | handleGitStash | REAL |
| git.stash_pop | `!git_stash_pop` | IDM_GIT_STASH_POP | 9855 | handleGitStashPop | REAL |
| git.stage_file | `!git_stage_file` | IDM_GIT_STAGE_FILE | 9860 | handleGitStageFile | REAL |
| git.unstage_file | `!git_unstage_file` | IDM_GIT_UNSTAGE_FILE | 9861 | handleGitUnstageFile | REAL |
| git.log | `!git_log` | IDM_GIT_LOG | 9862 | handleGitLog | REAL |

### GOV (4 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| gov.status | `!gov_status` | IDM_GOV_STATUS | 5118 | handleGovStatus | REAL |
| gov.submit_command | `!gov_submit_command` | IDM_GOV_SUBMIT_COMMAND | 5119 | handleGovSubmitCommand | REAL |
| gov.kill_all | `!gov_kill_all` | IDM_GOV_KILL_ALL | 5120 | handleGovKillAll | REAL |
| gov.task_list | `!gov_task_list` | IDM_GOV_TASK_LIST | 5121 | handleGovTaskList | REAL |

### HELP (11 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| help.docs | `!help_docs` | IDM_HELP_DOCS | 601 | handleHelpDocs | REAL |
| help.shortcuts | `!help_shortcuts` | IDM_HELP_SHORTCUTS | 603 | handleHelpShortcuts | REAL |
| help.welcome | `!help_welcome` | IDM_HELP_WELCOME | 701 | handleHelpWelcome | REAL |
| help.release_notes | `!help_release_notes` | IDM_HELP_RELEASE_NOTES | 704 | handleHelpReleaseNotes | REAL |
| help.check_updates | `!help_check_updates` | IDM_HELP_CHECK_UPDATES | 705 | handleHelpCheckUpdates | REAL |
| help.report_issue | `!help_report_issue` | IDM_HELP_REPORT_ISSUE | 707 | handleHelpReportIssue | REAL |
| help.tips_tricks | `!help_tips_tricks` | IDM_HELP_TIPS_TRICKS | 708 | handleHelpTipsTricks | REAL |
| help.about | `!help_about` | IDM_HELP_ABOUT | 4001 | handleHelpAbout | REAL |
| help.cmdref | `!help_cmdref` | IDM_HELP_CMDREF | 4002 | handleHelpCmdref | REAL |
| help.psdocs | `!help_psdocs` | IDM_HELP_PSDOCS | 4003 | handleHelpPsdocs | REAL |
| help.search | `!help_search` | IDM_HELP_SEARCH | 4004 | handleHelpSearch | REAL |

### HOTPATCH (26 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| hotpatch.show_status | `!hotpatch_show_status` | IDM_HOTPATCH_SHOW_STATUS | 9001 | handleHotpatchShowStatus | REAL |
| hotpatch.memory_apply | `!hotpatch_memory_apply` | IDM_HOTPATCH_MEMORY_APPLY | 9002 | handleHotpatchMemoryApply | REAL |
| hotpatch.memory_revert | `!hotpatch_memory_revert` | IDM_HOTPATCH_MEMORY_REVERT | 9003 | handleHotpatchMemoryRevert | REAL |
| hotpatch.byte_apply | `!hotpatch_byte_apply` | IDM_HOTPATCH_BYTE_APPLY | 9004 | handleHotpatchByteApply | REAL |
| hotpatch.byte_search | `!hotpatch_byte_search` | IDM_HOTPATCH_BYTE_SEARCH | 9005 | handleHotpatchByteSearch | REAL |
| hotpatch.server_add | `!hotpatch_server_add` | IDM_HOTPATCH_SERVER_ADD | 9006 | handleHotpatchServerAdd | REAL |
| hotpatch.server_remove | `!hotpatch_server_remove` | IDM_HOTPATCH_SERVER_REMOVE | 9007 | handleHotpatchServerRemove | REAL |
| hotpatch.proxy_bias | `!hotpatch_proxy_bias` | IDM_HOTPATCH_PROXY_BIAS | 9008 | handleHotpatchProxyBias | REAL |
| hotpatch.proxy_rewrite | `!hotpatch_proxy_rewrite` | IDM_HOTPATCH_PROXY_REWRITE | 9009 | handleHotpatchProxyRewrite | REAL |
| hotpatch.proxy_terminate | `!hotpatch_proxy_terminate` | IDM_HOTPATCH_PROXY_TERMINATE | 9010 | handleHotpatchProxyTerminate | REAL |
| hotpatch.proxy_validate | `!hotpatch_proxy_validate` | IDM_HOTPATCH_PROXY_VALIDATE | 9011 | handleHotpatchProxyValidate | REAL |
| hotpatch.preset_save | `!hotpatch_preset_save` | IDM_HOTPATCH_PRESET_SAVE | 9012 | handleHotpatchPresetSave | REAL |
| hotpatch.preset_load | `!hotpatch_preset_load` | IDM_HOTPATCH_PRESET_LOAD | 9013 | handleHotpatchPresetLoad | REAL |
| hotpatch.show_event_log | `!hotpatch_show_event_log` | IDM_HOTPATCH_SHOW_EVENT_LOG | 9014 | handleHotpatchShowEventLog | REAL |
| hotpatch.reset_stats | `!hotpatch_reset_stats` | IDM_HOTPATCH_RESET_STATS | 9015 | handleHotpatchResetStats | REAL |
| hotpatch.toggle_all | `!hotpatch_toggle_all` | IDM_HOTPATCH_TOGGLE_ALL | 9016 | handleHotpatchToggleAll | REAL |
| hotpatch.show_proxy_stats | `!hotpatch_show_proxy_stats` | IDM_HOTPATCH_SHOW_PROXY_STATS | 9017 | handleHotpatchShowProxyStats | REAL |
| hotpatch.set_target_tps | `!hotpatch_set_target_tps` | IDM_HOTPATCH_SET_TARGET_TPS | 9018 | handleHotpatchSetTargetTps | REAL |
| hotpatch.compact_conversation | `!hotpatch_compact_conversation` | IDM_HOTPATCH_COMPACT_CONVERSATION | 9019 | handleHotpatchCompactConversation | REAL |
| hotpatch.optimize_tool_selection | `!hotpatch_optimize_tool_selection` | IDM_HOTPATCH_OPTIMIZE_TOOL_SELECTION | 9020 | handleHotpatchOptimizeToolSelection | REAL |
| hotpatch.resolving | `!hotpatch_resolving` | IDM_HOTPATCH_RESOLVING | 9021 | handleHotpatchResolving | REAL |
| hotpatch.read_lines | `!hotpatch_read_lines` | IDM_HOTPATCH_READ_LINES | 9022 | handleHotpatchReadLines | REAL |
| hotpatch.planning_exploration | `!hotpatch_planning_exploration` | IDM_HOTPATCH_PLANNING_EXPLORATION | 9023 | handleHotpatchPlanningExploration | REAL |
| hotpatch.search_files | `!hotpatch_search_files` | IDM_HOTPATCH_SEARCH_FILES | 9024 | handleHotpatchSearchFiles | REAL |
| hotpatch.evaluate_integration | `!hotpatch_evaluate_integration` | IDM_HOTPATCH_EVALUATE_INTEGRATION | 9025 | handleHotpatchEvaluateIntegration | REAL |
| hotpatch.restore_checkpoint | `!hotpatch_restore_checkpoint` | IDM_HOTPATCH_RESTORE_CHECKPOINT | 9026 | handleHotpatchRestoreCheckpoint | REAL |

### HYBRID (12 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| hybrid.complete | `!hybrid_complete` | IDM_HYBRID_COMPLETE | 5094 | handleHybridComplete | REAL |
| hybrid.diagnostics | `!hybrid_diagnostics` | IDM_HYBRID_DIAGNOSTICS | 5095 | handleHybridDiagnostics | REAL |
| hybrid.smart_rename | `!hybrid_smart_rename` | IDM_HYBRID_SMART_RENAME | 5096 | handleHybridSmartRename | REAL |
| hybrid.analyze_file | `!hybrid_analyze_file` | IDM_HYBRID_ANALYZE_FILE | 5097 | handleHybridAnalyzeFile | REAL |
| hybrid.auto_profile | `!hybrid_auto_profile` | IDM_HYBRID_AUTO_PROFILE | 5098 | handleHybridAutoProfile | REAL |
| hybrid.status | `!hybrid_status` | IDM_HYBRID_STATUS | 5099 | handleHybridStatus | REAL |
| hybrid.symbol_usage | `!hybrid_symbol_usage` | IDM_HYBRID_SYMBOL_USAGE | 5100 | handleHybridSymbolUsage | REAL |
| hybrid.explain_symbol | `!hybrid_explain_symbol` | IDM_HYBRID_EXPLAIN_SYMBOL | 5101 | handleHybridExplainSymbol | REAL |
| hybrid.annotate_diag | `!hybrid_annotate_diag` | IDM_HYBRID_ANNOTATE_DIAG | 5102 | handleHybridAnnotateDiag | REAL |
| hybrid.stream_analyze | `!hybrid_stream_analyze` | IDM_HYBRID_STREAM_ANALYZE | 5103 | handleHybridStreamAnalyze | REAL |
| hybrid.semantic_prefetch | `!hybrid_semantic_prefetch` | IDM_HYBRID_SEMANTIC_PREFETCH | 5104 | handleHybridSemanticPrefetch | REAL |
| hybrid.correction_loop | `!hybrid_correction_loop` | IDM_HYBRID_CORRECTION_LOOP | 5105 | handleHybridCorrectionLoop | REAL |

### IMPACT (11 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| impact.analyze_staged | `!impact_analyze_staged` | IDM_IMPACT_ANALYZE_STAGED | 4350 | handleImpactAnalyzeStaged | REAL |
| impact.analyze_unstaged | `!impact_analyze_unstaged` | IDM_IMPACT_ANALYZE_UNSTAGED | 4351 | handleImpactAnalyzeUnstaged | REAL |
| impact.analyze_file | `!impact_analyze_file` | IDM_IMPACT_ANALYZE_FILE | 4352 | handleImpactAnalyzeFile | REAL |
| impact.show_report | `!impact_show_report` | IDM_IMPACT_SHOW_REPORT | 4353 | handleImpactShowReport | REAL |
| impact.show_zones | `!impact_show_zones` | IDM_IMPACT_SHOW_ZONES | 4354 | handleImpactShowZones | REAL |
| impact.risk_score | `!impact_risk_score` | IDM_IMPACT_RISK_SCORE | 4355 | handleImpactRiskScore | REAL |
| impact.check_commit | `!impact_check_commit` | IDM_IMPACT_CHECK_COMMIT | 4356 | handleImpactCheckCommit | REAL |
| impact.set_config | `!impact_set_config` | IDM_IMPACT_SET_CONFIG | 4357 | handleImpactSetConfig | REAL |
| impact.history | `!impact_history` | IDM_IMPACT_HISTORY | 4358 | handleImpactHistory | REAL |
| impact.diagnostics | `!impact_diagnostics` | IDM_IMPACT_DIAGNOSTICS | 4359 | handleImpactDiagnostics | REAL |
| impact.export_json | `!impact_export_json` | IDM_IMPACT_EXPORT_JSON | 4360 | handleImpactExportJson | REAL |

### INFERENCE (1 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| inference.debugger_panel | `!inference_debugger_panel` | IDM_INFERENCE_DEBUGGER_PANEL | 5322 | handleInferenceDebuggerPanel | REAL |

### KNOWLEDGE (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| knowledge.init | `!knowledge_init` | IDM_KNOWLEDGE_INIT | 4271 | handleKnowledgeInit | REAL |
| knowledge.record | `!knowledge_record` | IDM_KNOWLEDGE_RECORD | 4272 | handleKnowledgeRecord | REAL |
| knowledge.search | `!knowledge_search` | IDM_KNOWLEDGE_SEARCH | 4273 | handleKnowledgeSearch | REAL |
| knowledge.decisions | `!knowledge_decisions` | IDM_KNOWLEDGE_DECISIONS | 4274 | handleKnowledgeDecisions | REAL |
| knowledge.preferences | `!knowledge_preferences` | IDM_KNOWLEDGE_PREFERENCES | 4275 | handleKnowledgePreferences | REAL |
| knowledge.archaeology | `!knowledge_archaeology` | IDM_KNOWLEDGE_ARCHAEOLOGY | 4276 | handleKnowledgeArchaeology | REAL |
| knowledge.graph | `!knowledge_graph` | IDM_KNOWLEDGE_GRAPH | 4277 | handleKnowledgeGraph | REAL |
| knowledge.export | `!knowledge_export` | IDM_KNOWLEDGE_EXPORT | 4278 | handleKnowledgeExport | REAL |
| knowledge.stats | `!knowledge_stats` | IDM_KNOWLEDGE_STATS | 4279 | handleKnowledgeStats | REAL |
| knowledge.flush | `!knowledge_flush` | IDM_KNOWLEDGE_FLUSH | 4280 | handleKnowledgeFlush | REAL |

### LSP (22 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| lsp.start_all | `!lsp_start_all` | IDM_LSP_START_ALL | 5058 | handleLspStartAll | REAL |
| lsp.stop_all | `!lsp_stop_all` | IDM_LSP_STOP_ALL | 5059 | handleLspStopAll | REAL |
| lsp.show_status | `!lsp_show_status` | IDM_LSP_SHOW_STATUS | 5060 | handleLspShowStatus | REAL |
| lsp.goto_definition | `!lsp_goto_definition` | IDM_LSP_GOTO_DEFINITION | 5061 | handleLspGotoDefinition | REAL |
| lsp.find_references | `!lsp_find_references` | IDM_LSP_FIND_REFERENCES | 5062 | handleLspFindReferences | REAL |
| lsp.rename_symbol | `!lsp_rename_symbol` | IDM_LSP_RENAME_SYMBOL | 5063 | handleLspRenameSymbol | REAL |
| lsp.hover_info | `!lsp_hover_info` | IDM_LSP_HOVER_INFO | 5064 | handleLspHoverInfo | REAL |
| lsp.show_diagnostics | `!lsp_show_diagnostics` | IDM_LSP_SHOW_DIAGNOSTICS | 5065 | handleLspShowDiagnostics | REAL |
| lsp.restart_server | `!lsp_restart_server` | IDM_LSP_RESTART_SERVER | 5066 | handleLspRestartServer | REAL |
| lsp.clear_diagnostics | `!lsp_clear_diagnostics` | IDM_LSP_CLEAR_DIAGNOSTICS | 5067 | handleLspClearDiagnostics | REAL |
| lsp.show_symbol_info | `!lsp_show_symbol_info` | IDM_LSP_SHOW_SYMBOL_INFO | 5068 | handleLspShowSymbolInfo | REAL |
| lsp.configure | `!lsp_configure` | IDM_LSP_CONFIGURE | 5069 | handleLspConfigure | REAL |
| lsp.save_config | `!lsp_save_config` | IDM_LSP_SAVE_CONFIG | 5070 | handleLspSaveConfig | REAL |
| lsp.server_start | `!lsp_server_start` | IDM_LSP_SERVER_START | 9200 | handleLspServerStart | REAL |
| lsp.server_stop | `!lsp_server_stop` | IDM_LSP_SERVER_STOP | 9201 | handleLspServerStop | REAL |
| lsp.server_status | `!lsp_server_status` | IDM_LSP_SERVER_STATUS | 9202 | handleLspServerStatus | REAL |
| lsp.server_reindex | `!lsp_server_reindex` | IDM_LSP_SERVER_REINDEX | 9203 | handleLspServerReindex | REAL |
| lsp.server_stats | `!lsp_server_stats` | IDM_LSP_SERVER_STATS | 9204 | handleLspServerStats | REAL |
| lsp.server_publish_diag | `!lsp_server_publish_diag` | IDM_LSP_SERVER_PUBLISH_DIAG | 9205 | handleLspServerPublishDiag | REAL |
| lsp.server_config | `!lsp_server_config` | IDM_LSP_SERVER_CONFIG | 9206 | handleLspServerConfig | REAL |
| lsp.server_export_symbols | `!lsp_server_export_symbols` | IDM_LSP_SERVER_EXPORT_SYMBOLS | 9207 | handleLspServerExportSymbols | REAL |
| lsp.server_launch_stdio | `!lsp_server_launch_stdio` | IDM_LSP_SERVER_LAUNCH_STDIO | 9208 | handleLspServerLaunchStdio | REAL |

### MODEL (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| model.load | `!model_load` | IDM_MODEL_LOAD | 801 | handleModelLoad | REAL |
| model.unload | `!model_unload` | IDM_MODEL_UNLOAD | 802 | handleModelUnload | REAL |
| model.info | `!model_info` | IDM_MODEL_INFO | 803 | handleModelInfo | REAL |
| model.reload | `!model_reload` | IDM_MODEL_RELOAD | 804 | handleModelReload | REAL |
| model.settings | `!model_settings` | IDM_MODEL_SETTINGS | 805 | handleModelSettings | REAL |

### MODULES (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| modules.refresh | `!modules_refresh` | IDM_MODULES_REFRESH | 3050 | handleModulesRefresh | REAL |
| modules.import | `!modules_import` | IDM_MODULES_IMPORT | 3051 | handleModulesImport | REAL |
| modules.export | `!modules_export` | IDM_MODULES_EXPORT | 3052 | handleModulesExport | REAL |

### MULTI (11 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| multi.resp_generate | `!multi_resp_generate` | IDM_MULTI_RESP_GENERATE | 5106 | handleMultiRespGenerate | REAL |
| multi.resp_select_preferred | `!multi_resp_select_preferred` | IDM_MULTI_RESP_SELECT_PREFERRED | 5108 | handleMultiRespSelectPreferred | REAL |
| multi.resp_compare | `!multi_resp_compare` | IDM_MULTI_RESP_COMPARE | 5109 | handleMultiRespCompare | REAL |
| multi.resp_show_stats | `!multi_resp_show_stats` | IDM_MULTI_RESP_SHOW_STATS | 5110 | handleMultiRespShowStats | REAL |
| multi.resp_show_templates | `!multi_resp_show_templates` | IDM_MULTI_RESP_SHOW_TEMPLATES | 5111 | handleMultiRespShowTemplates | REAL |
| multi.resp_toggle_template | `!multi_resp_toggle_template` | IDM_MULTI_RESP_TOGGLE_TEMPLATE | 5112 | handleMultiRespToggleTemplate | REAL |
| multi.resp_show_prefs | `!multi_resp_show_prefs` | IDM_MULTI_RESP_SHOW_PREFS | 5113 | handleMultiRespShowPrefs | REAL |
| multi.resp_show_latest | `!multi_resp_show_latest` | IDM_MULTI_RESP_SHOW_LATEST | 5114 | handleMultiRespShowLatest | REAL |
| multi.resp_show_status | `!multi_resp_show_status` | IDM_MULTI_RESP_SHOW_STATUS | 5115 | handleMultiRespShowStatus | REAL |
| multi.resp_clear_history | `!multi_resp_clear_history` | IDM_MULTI_RESP_CLEAR_HISTORY | 5116 | handleMultiRespClearHistory | REAL |
| multi.resp_apply_preferred | `!multi_resp_apply_preferred` | IDM_MULTI_RESP_APPLY_PREFERRED | 5117 | handleMultiRespApplyPreferred | REAL |

### OMEGA (21 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| omega.start | `!omega_start` | IDM_OMEGA_START | 4243 | handleOmegaStart | REAL |
| omega.stop | `!omega_stop` | IDM_OMEGA_STOP | 4244 | handleOmegaStop | REAL |
| omega.submit_task | `!omega_submit_task` | IDM_OMEGA_SUBMIT_TASK | 4245 | handleOmegaSubmitTask | REAL |
| omega.run_cycle | `!omega_run_cycle` | IDM_OMEGA_RUN_CYCLE | 4246 | handleOmegaRunCycle | REAL |
| omega.show_status | `!omega_show_status` | IDM_OMEGA_SHOW_STATUS | 4247 | handleOmegaShowStatus | REAL |
| omega.view_pipeline | `!omega_view_pipeline` | IDM_OMEGA_VIEW_PIPELINE | 4248 | handleOmegaViewPipeline | REAL |
| omega.set_quality_auto | `!omega_set_quality_auto` | IDM_OMEGA_SET_QUALITY_AUTO | 4250 | handleOmegaSetQualityAuto | REAL |
| omega.set_quality_balance | `!omega_set_quality_balance` | IDM_OMEGA_SET_QUALITY_BALANCE | 4251 | handleOmegaSetQualityBalance | REAL |
| omega.world_model | `!omega_world_model` | IDM_OMEGA_WORLD_MODEL | 4254 | handleOmegaWorldModel | REAL |
| omega.export_stats | `!omega_export_stats` | IDM_OMEGA_EXPORT_STATS | 4255 | handleOmegaExportStats | REAL |
| omega.diagnostics | `!omega_diagnostics` | IDM_OMEGA_DIAGNOSTICS | 4256 | handleOmegaDiagnostics | REAL |
| omega.version | `!omega_version` | IDM_OMEGA_VERSION | 4257 | handleOmegaVersion | REAL |
| omega.help | `!omega_help` | IDM_OMEGA_HELP | 4258 | handleOmegaHelp | REAL |
| omega.advanced_settings | `!omega_advanced_settings` | IDM_OMEGA_ADVANCED_SETTINGS | 4259 | handleOmegaAdvancedSettings | REAL |
| omega.shell_integration | `!omega_shell_integration` | IDM_OMEGA_SHELL_INTEGRATION | 4260 | handleOmegaShellIntegration | REAL |
| omega.start_autonomous | `!omega_start_autonomous` | IDM_OMEGA_START_AUTONOMOUS | 12400 | handleOmegaStartAutonomous | REAL |
| omega.set_goal | `!omega_set_goal` | IDM_OMEGA_SET_GOAL | 12401 | handleOmegaSetGoal | REAL |
| omega.observe_pipeline | `!omega_observe_pipeline` | IDM_OMEGA_OBSERVE_PIPELINE | 12402 | handleOmegaObservePipeline | REAL |
| omega.cancel_task | `!omega_cancel_task` | IDM_OMEGA_CANCEL_TASK | 12403 | handleOmegaCancelTask | REAL |
| omega.spawn_agent | `!omega_spawn_agent` | IDM_OMEGA_SPAWN_AGENT | 12404 | handleOmegaSpawnAgent | REAL |
| omega.get_stats | `!omega_get_stats` | IDM_OMEGA_GET_STATS | 12405 | handleOmegaGetStats | REAL |

### PDB (9 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| pdb.load | `!pdb_load` | IDM_PDB_LOAD | 9400 | handlePdbLoad | REAL |
| pdb.fetch | `!pdb_fetch` | IDM_PDB_FETCH | 9401 | handlePdbFetch | REAL |
| pdb.status | `!pdb_status` | IDM_PDB_STATUS | 9402 | handlePdbStatus | REAL |
| pdb.cache_clear | `!pdb_cache_clear` | IDM_PDB_CACHE_CLEAR | 9403 | handlePdbCacheClear | REAL |
| pdb.enable | `!pdb_enable` | IDM_PDB_ENABLE | 9404 | handlePdbEnable | REAL |
| pdb.resolve | `!pdb_resolve` | IDM_PDB_RESOLVE | 9405 | handlePdbResolve | REAL |
| pdb.imports | `!pdb_imports` | IDM_PDB_IMPORTS | 9410 | handlePdbImports | REAL |
| pdb.exports | `!pdb_exports` | IDM_PDB_EXPORTS | 9411 | handlePdbExports | REAL |
| pdb.iat_status | `!pdb_iat_status` | IDM_PDB_IAT_STATUS | 9412 | handlePdbIatStatus | REAL |

### PEEK (4 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| peek.definition | `!peek_definition` | IDM_PEEK_DEFINITION | 5071 | handlePeekDefinition | REAL |
| peek.references | `!peek_references` | IDM_PEEK_REFERENCES | 5072 | handlePeekReferences | REAL |
| peek.close | `!peek_close` | IDM_PEEK_CLOSE | 5073 | handlePeekClose | REAL |
| peek.navigate | `!peek_navigate` | IDM_PEEK_NAVIGATE | 5074 | handlePeekNavigate | REAL |

### PIPELINE (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| pipeline.run | `!pipeline_run` | IDM_PIPELINE_RUN | 4160 | handlePipelineRun | REAL |
| pipeline.autonomy_start | `!pipeline_autonomy_start` | IDM_PIPELINE_AUTONOMY_START | 4161 | handlePipelineAutonomyStart | REAL |
| pipeline.autonomy_stop | `!pipeline_autonomy_stop` | IDM_PIPELINE_AUTONOMY_STOP | 4162 | handlePipelineAutonomyStop | REAL |

### PLANNING (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| planning.start | `!planning_start` | IDM_PLANNING_START | 4261 | handlePlanningStart | REAL |
| planning.show_queue | `!planning_show_queue` | IDM_PLANNING_SHOW_QUEUE | 4262 | handlePlanningShowQueue | REAL |
| planning.approve_step | `!planning_approve_step` | IDM_PLANNING_APPROVE_STEP | 4263 | handlePlanningApproveStep | REAL |
| planning.reject_step | `!planning_reject_step` | IDM_PLANNING_REJECT_STEP | 4264 | handlePlanningRejectStep | REAL |
| planning.execute_step | `!planning_execute_step` | IDM_PLANNING_EXECUTE_STEP | 4265 | handlePlanningExecuteStep | REAL |
| planning.execute_all | `!planning_execute_all` | IDM_PLANNING_EXECUTE_ALL | 4266 | handlePlanningExecuteAll | REAL |
| planning.rollback | `!planning_rollback` | IDM_PLANNING_ROLLBACK | 4267 | handlePlanningRollback | REAL |
| planning.set_policy | `!planning_set_policy` | IDM_PLANNING_SET_POLICY | 4268 | handlePlanningSetPolicy | REAL |
| planning.view_status | `!planning_view_status` | IDM_PLANNING_VIEW_STATUS | 4269 | handlePlanningViewStatus | REAL |
| planning.diagnostics | `!planning_diagnostics` | IDM_PLANNING_DIAGNOSTICS | 4270 | handlePlanningDiagnostics | REAL |

### PLUGIN (9 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| plugin.show_panel | `!plugin_show_panel` | IDM_PLUGIN_SHOW_PANEL | 5200 | handlePluginShowPanel | REAL |
| plugin.load | `!plugin_load` | IDM_PLUGIN_LOAD | 5201 | handlePluginLoad | REAL |
| plugin.unload | `!plugin_unload` | IDM_PLUGIN_UNLOAD | 5202 | handlePluginUnload | REAL |
| plugin.unload_all | `!plugin_unload_all` | IDM_PLUGIN_UNLOAD_ALL | 5203 | handlePluginUnloadAll | REAL |
| plugin.refresh | `!plugin_refresh` | IDM_PLUGIN_REFRESH | 5204 | handlePluginRefresh | REAL |
| plugin.scan_dir | `!plugin_scan_dir` | IDM_PLUGIN_SCAN_DIR | 5205 | handlePluginScanDir | REAL |
| plugin.show_status | `!plugin_show_status` | IDM_PLUGIN_SHOW_STATUS | 5206 | handlePluginShowStatus | REAL |
| plugin.toggle_hotload | `!plugin_toggle_hotload` | IDM_PLUGIN_TOGGLE_HOTLOAD | 5207 | handlePluginToggleHotload | REAL |
| plugin.configure | `!plugin_configure` | IDM_PLUGIN_CONFIGURE | 5208 | handlePluginConfigure | REAL |

### QW (12 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| qw.shortcut_editor | `!qw_shortcut_editor` | IDM_QW_SHORTCUT_EDITOR | 9800 | handleQwShortcutEditor | REAL |
| qw.shortcut_reset | `!qw_shortcut_reset` | IDM_QW_SHORTCUT_RESET | 9801 | handleQwShortcutReset | REAL |
| qw.backup_create | `!qw_backup_create` | IDM_QW_BACKUP_CREATE | 9810 | handleQwBackupCreate | REAL |
| qw.backup_restore | `!qw_backup_restore` | IDM_QW_BACKUP_RESTORE | 9811 | handleQwBackupRestore | REAL |
| qw.backup_auto_toggle | `!qw_backup_auto_toggle` | IDM_QW_BACKUP_AUTO_TOGGLE | 9812 | handleQwBackupAutoToggle | REAL |
| qw.backup_list | `!qw_backup_list` | IDM_QW_BACKUP_LIST | 9813 | handleQwBackupList | REAL |
| qw.backup_prune | `!qw_backup_prune` | IDM_QW_BACKUP_PRUNE | 9814 | handleQwBackupPrune | REAL |
| qw.alert_toggle_monitor | `!qw_alert_toggle_monitor` | IDM_QW_ALERT_TOGGLE_MONITOR | 9820 | handleQwAlertToggleMonitor | REAL |
| qw.alert_show_history | `!qw_alert_show_history` | IDM_QW_ALERT_SHOW_HISTORY | 9821 | handleQwAlertShowHistory | REAL |
| qw.alert_dismiss_all | `!qw_alert_dismiss_all` | IDM_QW_ALERT_DISMISS_ALL | 9822 | handleQwAlertDismissAll | REAL |
| qw.alert_resource_status | `!qw_alert_resource_status` | IDM_QW_ALERT_RESOURCE_STATUS | 9823 | handleQwAlertResourceStatus | REAL |
| qw.slo_dashboard | `!qw_slo_dashboard` | IDM_QW_SLO_DASHBOARD | 9830 | handleQwSloDashboard | REAL |

### REPLAY (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| replay.status | `!replay_status` | IDM_REPLAY_STATUS | 5126 | handleReplayStatus | REAL |
| replay.export_session | `!replay_export_session` | IDM_REPLAY_EXPORT_SESSION | 5128 | handleReplayExportSession | REAL |
| replay.checkpoint | `!replay_checkpoint` | IDM_REPLAY_CHECKPOINT | 5129 | handleReplayCheckpoint | REAL |

### REVENG (24 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| reveng.analyze | `!reveng_analyze` | IDM_REVENG_ANALYZE | 4300 | handleRevengAnalyze | REAL |
| reveng.disasm | `!reveng_disasm` | IDM_REVENG_DISASM | 4301 | handleRevengDisasm | REAL |
| reveng.dumpbin | `!reveng_dumpbin` | IDM_REVENG_DUMPBIN | 4302 | handleRevengDumpbin | REAL |
| reveng.compile | `!reveng_compile` | IDM_REVENG_COMPILE | 4303 | handleRevengCompile | REAL |
| reveng.compare | `!reveng_compare` | IDM_REVENG_COMPARE | 4304 | handleRevengCompare | REAL |
| reveng.detect_vulns | `!reveng_detect_vulns` | IDM_REVENG_DETECT_VULNS | 4305 | handleRevengDetectVulns | REAL |
| reveng.export_ida | `!reveng_export_ida` | IDM_REVENG_EXPORT_IDA | 4306 | handleRevengExportIda | REAL |
| reveng.export_ghidra | `!reveng_export_ghidra` | IDM_REVENG_EXPORT_GHIDRA | 4307 | handleRevengExportGhidra | REAL |
| reveng.cfg | `!reveng_cfg` | IDM_REVENG_CFG | 4308 | handleRevengCfg | REAL |
| reveng.functions | `!reveng_functions` | IDM_REVENG_FUNCTIONS | 4309 | handleRevengFunctions | REAL |
| reveng.demangle | `!reveng_demangle` | IDM_REVENG_DEMANGLE | 4310 | handleRevengDemangle | REAL |
| reveng.ssa | `!reveng_ssa` | IDM_REVENG_SSA | 4311 | handleRevengSsa | REAL |
| reveng.recursive_disasm | `!reveng_recursive_disasm` | IDM_REVENG_RECURSIVE_DISASM | 4312 | handleRevengRecursiveDisasm | REAL |
| reveng.type_recovery | `!reveng_type_recovery` | IDM_REVENG_TYPE_RECOVERY | 4313 | handleRevengTypeRecovery | REAL |
| reveng.data_flow | `!reveng_data_flow` | IDM_REVENG_DATA_FLOW | 4314 | handleRevengDataFlow | REAL |
| reveng.license_info | `!reveng_license_info` | IDM_REVENG_LICENSE_INFO | 4315 | handleRevengLicenseInfo | REAL |
| reveng.decompiler_view | `!reveng_decompiler_view` | IDM_REVENG_DECOMPILER_VIEW | 4316 | handleRevengDecompilerView | REAL |
| reveng.decomp_rename | `!reveng_decomp_rename` | IDM_REVENG_DECOMP_RENAME | 4317 | handleRevengDecompRename | REAL |
| reveng.decomp_sync | `!reveng_decomp_sync` | IDM_REVENG_DECOMP_SYNC | 4318 | handleRevengDecompSync | REAL |
| reveng.decomp_close | `!reveng_decomp_close` | IDM_REVENG_DECOMP_CLOSE | 4319 | handleRevengDecompClose | REAL |
| reveng.set_binary_from_active | `!reveng_set_binary_from_active` | IDM_REVENG_SET_BINARY_FROM_ACTIVE | 4320 | handleRevengSetBinaryFromActive | REAL |
| reveng.set_binary_from_debug_target | `!reveng_set_binary_from_debug_target` | IDM_REVENG_SET_BINARY_FROM_DEBUG_TARGET | 4321 | handleRevengSetBinaryFromDebugTarget | REAL |
| reveng.set_binary_from_build_output | `!reveng_set_binary_from_build_output` | IDM_REVENG_SET_BINARY_FROM_BUILD_OUTPUT | 4322 | handleRevengSetBinaryFromBuildOutput | REAL |
| reveng.disasm_at_rip | `!reveng_disasm_at_rip` | IDM_REVENG_DISASM_AT_RIP | 4323 | handleRevengDisasmAtRip | REAL |

### ROUTER (20 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| router.enable | `!router_enable` | IDM_ROUTER_ENABLE | 5048 | handleRouterEnable | REAL |
| router.disable | `!router_disable` | IDM_ROUTER_DISABLE | 5049 | handleRouterDisable | REAL |
| router.show_status | `!router_show_status` | IDM_ROUTER_SHOW_STATUS | 5050 | handleRouterShowStatus | REAL |
| router.show_decision | `!router_show_decision` | IDM_ROUTER_SHOW_DECISION | 5051 | handleRouterShowDecision | REAL |
| router.set_policy | `!router_set_policy` | IDM_ROUTER_SET_POLICY | 5052 | handleRouterSetPolicy | REAL |
| router.show_capabilities | `!router_show_capabilities` | IDM_ROUTER_SHOW_CAPABILITIES | 5053 | handleRouterShowCapabilities | REAL |
| router.show_fallbacks | `!router_show_fallbacks` | IDM_ROUTER_SHOW_FALLBACKS | 5054 | handleRouterShowFallbacks | REAL |
| router.save_config | `!router_save_config` | IDM_ROUTER_SAVE_CONFIG | 5055 | handleRouterSaveConfig | REAL |
| router.route_prompt | `!router_route_prompt` | IDM_ROUTER_ROUTE_PROMPT | 5056 | handleRouterRoutePrompt | REAL |
| router.reset_stats | `!router_reset_stats` | IDM_ROUTER_RESET_STATS | 5057 | handleRouterResetStats | REAL |
| router.why_backend | `!router_why_backend` | IDM_ROUTER_WHY_BACKEND | 5071 | handleRouterWhyBackend | REAL |
| router.pin_task | `!router_pin_task` | IDM_ROUTER_PIN_TASK | 5072 | handleRouterPinTask | REAL |
| router.unpin_task | `!router_unpin_task` | IDM_ROUTER_UNPIN_TASK | 5073 | handleRouterUnpinTask | REAL |
| router.show_pins | `!router_show_pins` | IDM_ROUTER_SHOW_PINS | 5074 | handleRouterShowPins | REAL |
| router.show_heatmap | `!router_show_heatmap` | IDM_ROUTER_SHOW_HEATMAP | 5075 | handleRouterShowHeatmap | REAL |
| router.ensemble_enable | `!router_ensemble_enable` | IDM_ROUTER_ENSEMBLE_ENABLE | 5076 | handleRouterEnsembleEnable | REAL |
| router.ensemble_disable | `!router_ensemble_disable` | IDM_ROUTER_ENSEMBLE_DISABLE | 5077 | handleRouterEnsembleDisable | REAL |
| router.ensemble_status | `!router_ensemble_status` | IDM_ROUTER_ENSEMBLE_STATUS | 5078 | handleRouterEnsembleStatus | REAL |
| router.simulate | `!router_simulate` | IDM_ROUTER_SIMULATE | 5079 | handleRouterSimulate | REAL |
| router.show_cost_stats | `!router_show_cost_stats` | IDM_ROUTER_SHOW_COST_STATS | 5081 | handleRouterShowCostStats | REAL |

### RUN (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| run.start_debug | `!run_start_debug` | IDM_RUN_START_DEBUG | 501 | handleRunStartDebug | REAL |
| run.without_debug | `!run_without_debug` | IDM_RUN_WITHOUT_DEBUG | 502 | handleRunWithoutDebug | REAL |
| run.stop | `!run_stop` | IDM_RUN_STOP | 503 | handleRunStop | REAL |
| run.restart | `!run_restart` | IDM_RUN_RESTART | 504 | handleRunRestart | REAL |
| run.step_over | `!run_step_over` | IDM_RUN_STEP_OVER | 505 | handleRunStepOver | REAL |
| run.step_into | `!run_step_into` | IDM_RUN_STEP_INTO | 506 | handleRunStepInto | REAL |
| run.step_out | `!run_step_out` | IDM_RUN_STEP_OUT | 507 | handleRunStepOut | REAL |
| run.continue | `!run_continue` | IDM_RUN_CONTINUE | 508 | handleRunContinue | REAL |
| run.toggle_breakpoint | `!run_toggle_breakpoint` | IDM_RUN_TOGGLE_BREAKPOINT | 509 | handleRunToggleBreakpoint | REAL |
| run.clear_breakpoints | `!run_clear_breakpoints` | IDM_RUN_CLEAR_BREAKPOINTS | 510 | handleRunClearBreakpoints | REAL |

### SAFETY (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| safety.status | `!safety_status` | IDM_SAFETY_STATUS | 5122 | handleSafetyStatus | REAL |
| safety.reset_budget | `!safety_reset_budget` | IDM_SAFETY_RESET_BUDGET | 5123 | handleSafetyResetBudget | REAL |
| safety.show_violations | `!safety_show_violations` | IDM_SAFETY_SHOW_VIOLATIONS | 5125 | handleSafetyShowViolations | REAL |

### SECURITY (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| security.scan_secrets | `!security_scan_secrets` | IDM_SECURITY_SCAN_SECRETS | 9550 | handleSecurityScanSecrets | REAL |
| security.scan_sast | `!security_scan_sast` | IDM_SECURITY_SCAN_SAST | 9551 | handleSecurityScanSast | REAL |
| security.scan_dependencies | `!security_scan_dependencies` | IDM_SECURITY_SCAN_DEPENDENCIES | 9552 | handleSecurityScanDependencies | REAL |
| security.dashboard | `!security_dashboard` | IDM_SECURITY_DASHBOARD | 9553 | handleSecurityDashboard | REAL |
| security.export_sbom | `!security_export_sbom` | IDM_SECURITY_EXPORT_SBOM | 9554 | handleSecurityExportSbom | REAL |

### SEL (8 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| sel.all | `!sel_all` | IDM_SEL_ALL | 301 | handleSelAll | REAL |
| sel.expand | `!sel_expand` | IDM_SEL_EXPAND | 302 | handleSelExpand | REAL |
| sel.shrink | `!sel_shrink` | IDM_SEL_SHRINK | 303 | handleSelShrink | REAL |
| sel.column_mode | `!sel_column_mode` | IDM_SEL_COLUMN_MODE | 304 | handleSelColumnMode | REAL |
| sel.add_cursor_above | `!sel_add_cursor_above` | IDM_SEL_ADD_CURSOR_ABOVE | 305 | handleSelAddCursorAbove | REAL |
| sel.add_cursor_below | `!sel_add_cursor_below` | IDM_SEL_ADD_CURSOR_BELOW | 306 | handleSelAddCursorBelow | REAL |
| sel.add_next_occurrence | `!sel_add_next_occurrence` | IDM_SEL_ADD_NEXT_OCCURRENCE | 307 | handleSelAddNextOccurrence | REAL |
| sel.select_all_occurrences | `!sel_select_all_occurrences` | IDM_SEL_SELECT_ALL_OCCURRENCES | 308 | handleSelSelectAllOccurrences | REAL |

### SOURCEFILE (1 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| sourcefile.open_picker | `!sourcefile_open_picker` | IDM_SOURCEFILE_OPEN_PICKER | 1040 | handleSourcefileOpenPicker | REAL |

### SUBAGENT (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| subagent.chain | `!subagent_chain` | IDM_SUBAGENT_CHAIN | 4110 | handleSubagentChain | REAL |
| subagent.swarm | `!subagent_swarm` | IDM_SUBAGENT_SWARM | 4111 | handleSubagentSwarm | REAL |
| subagent.todo_list | `!subagent_todo_list` | IDM_SUBAGENT_TODO_LIST | 4112 | handleSubagentTodoList | REAL |
| subagent.todo_clear | `!subagent_todo_clear` | IDM_SUBAGENT_TODO_CLEAR | 4113 | handleSubagentTodoClear | REAL |
| subagent.status | `!subagent_status` | IDM_SUBAGENT_STATUS | 4114 | handleSubagentStatus | REAL |

### SWARM (25 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| swarm.status | `!swarm_status` | IDM_SWARM_STATUS | 5132 | handleSwarmStatus | REAL |
| swarm.start_leader | `!swarm_start_leader` | IDM_SWARM_START_LEADER | 5133 | handleSwarmStartLeader | REAL |
| swarm.start_worker | `!swarm_start_worker` | IDM_SWARM_START_WORKER | 5134 | handleSwarmStartWorker | REAL |
| swarm.start_hybrid | `!swarm_start_hybrid` | IDM_SWARM_START_HYBRID | 5135 | handleSwarmStartHybrid | REAL |
| swarm.stop | `!swarm_stop` | IDM_SWARM_STOP | 5136 | handleSwarmStop | REAL |
| swarm.list_nodes | `!swarm_list_nodes` | IDM_SWARM_LIST_NODES | 5137 | handleSwarmListNodes | REAL |
| swarm.add_node | `!swarm_add_node` | IDM_SWARM_ADD_NODE | 5138 | handleSwarmAddNode | REAL |
| swarm.remove_node | `!swarm_remove_node` | IDM_SWARM_REMOVE_NODE | 5139 | handleSwarmRemoveNode | REAL |
| swarm.blacklist_node | `!swarm_blacklist_node` | IDM_SWARM_BLACKLIST_NODE | 5140 | handleSwarmBlacklistNode | REAL |
| swarm.build_sources | `!swarm_build_sources` | IDM_SWARM_BUILD_SOURCES | 5141 | handleSwarmBuildSources | REAL |
| swarm.build_cmake | `!swarm_build_cmake` | IDM_SWARM_BUILD_CMAKE | 5142 | handleSwarmBuildCmake | REAL |
| swarm.start_build | `!swarm_start_build` | IDM_SWARM_START_BUILD | 5143 | handleSwarmStartBuild | REAL |
| swarm.cancel_build | `!swarm_cancel_build` | IDM_SWARM_CANCEL_BUILD | 5144 | handleSwarmCancelBuild | REAL |
| swarm.cache_status | `!swarm_cache_status` | IDM_SWARM_CACHE_STATUS | 5145 | handleSwarmCacheStatus | REAL |
| swarm.cache_clear | `!swarm_cache_clear` | IDM_SWARM_CACHE_CLEAR | 5146 | handleSwarmCacheClear | REAL |
| swarm.show_config | `!swarm_show_config` | IDM_SWARM_SHOW_CONFIG | 5147 | handleSwarmShowConfig | REAL |
| swarm.toggle_discovery | `!swarm_toggle_discovery` | IDM_SWARM_TOGGLE_DISCOVERY | 5148 | handleSwarmToggleDiscovery | REAL |
| swarm.show_task_graph | `!swarm_show_task_graph` | IDM_SWARM_SHOW_TASK_GRAPH | 5149 | handleSwarmShowTaskGraph | REAL |
| swarm.show_events | `!swarm_show_events` | IDM_SWARM_SHOW_EVENTS | 5150 | handleSwarmShowEvents | REAL |
| swarm.show_stats | `!swarm_show_stats` | IDM_SWARM_SHOW_STATS | 5151 | handleSwarmShowStats | REAL |
| swarm.reset_stats | `!swarm_reset_stats` | IDM_SWARM_RESET_STATS | 5152 | handleSwarmResetStats | REAL |
| swarm.worker_status | `!swarm_worker_status` | IDM_SWARM_WORKER_STATUS | 5153 | handleSwarmWorkerStatus | REAL |
| swarm.worker_connect | `!swarm_worker_connect` | IDM_SWARM_WORKER_CONNECT | 5154 | handleSwarmWorkerConnect | REAL |
| swarm.worker_disconnect | `!swarm_worker_disconnect` | IDM_SWARM_WORKER_DISCONNECT | 5155 | handleSwarmWorkerDisconnect | REAL |
| swarm.fitness_test | `!swarm_fitness_test` | IDM_SWARM_FITNESS_TEST | 5156 | handleSwarmFitnessTest | REAL |

### T1 (32 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| t1.smooth_scroll_toggle | `!t1_smooth_scroll_toggle` | IDM_T1_SMOOTH_SCROLL_TOGGLE | 12000 | handleT1SmoothScrollToggle | REAL |
| t1.smooth_scroll_speed | `!t1_smooth_scroll_speed` | IDM_T1_SMOOTH_SCROLL_SPEED | 12001 | handleT1SmoothScrollSpeed | REAL |
| t1.minimap_toggle | `!t1_minimap_toggle` | IDM_T1_MINIMAP_TOGGLE | 12010 | handleT1MinimapToggle | REAL |
| t1.minimap_highlight | `!t1_minimap_highlight` | IDM_T1_MINIMAP_HIGHLIGHT | 12011 | handleT1MinimapHighlight | REAL |
| t1.minimap_slider | `!t1_minimap_slider` | IDM_T1_MINIMAP_SLIDER | 12012 | handleT1MinimapSlider | REAL |
| t1.breadcrumbs_toggle | `!t1_breadcrumbs_toggle` | IDM_T1_BREADCRUMBS_TOGGLE | 12020 | handleT1BreadcrumbsToggle | REAL |
| t1.breadcrumbs_click | `!t1_breadcrumbs_click` | IDM_T1_BREADCRUMBS_CLICK | 12021 | handleT1BreadcrumbsClick | REAL |
| t1.fuzzy_palette | `!t1_fuzzy_palette` | IDM_T1_FUZZY_PALETTE | 12030 | handleT1FuzzyPalette | REAL |
| t1.fuzzy_files | `!t1_fuzzy_files` | IDM_T1_FUZZY_FILES | 12031 | handleT1FuzzyFiles | REAL |
| t1.fuzzy_symbols | `!t1_fuzzy_symbols` | IDM_T1_FUZZY_SYMBOLS | 12032 | handleT1FuzzySymbols | REAL |
| t1.settings_gui | `!t1_settings_gui` | IDM_T1_SETTINGS_GUI | 12040 | handleT1SettingsGui | REAL |
| t1.settings_search | `!t1_settings_search` | IDM_T1_SETTINGS_SEARCH | 12041 | handleT1SettingsSearch | REAL |
| t1.settings_reset | `!t1_settings_reset` | IDM_T1_SETTINGS_RESET | 12042 | handleT1SettingsReset | REAL |
| t1.welcome_show | `!t1_welcome_show` | IDM_T1_WELCOME_SHOW | 12050 | handleT1WelcomeShow | REAL |
| t1.welcome_clone | `!t1_welcome_clone` | IDM_T1_WELCOME_CLONE | 12051 | handleT1WelcomeClone | REAL |
| t1.welcome_open_folder | `!t1_welcome_open_folder` | IDM_T1_WELCOME_OPEN_FOLDER | 12052 | handleT1WelcomeOpenFolder | REAL |
| t1.welcome_new_file | `!t1_welcome_new_file` | IDM_T1_WELCOME_NEW_FILE | 12053 | handleT1WelcomeNewFile | REAL |
| t1.icon_theme_set | `!t1_icon_theme_set` | IDM_T1_ICON_THEME_SET | 12060 | handleT1IconThemeSet | REAL |
| t1.icon_theme_seti | `!t1_icon_theme_seti` | IDM_T1_ICON_THEME_SETI | 12061 | handleT1IconThemeSeti | REAL |
| t1.icon_theme_material | `!t1_icon_theme_material` | IDM_T1_ICON_THEME_MATERIAL | 12062 | handleT1IconThemeMaterial | REAL |
| t1.tab_drag_enable | `!t1_tab_drag_enable` | IDM_T1_TAB_DRAG_ENABLE | 12070 | handleT1TabDragEnable | REAL |
| t1.tab_tearoff | `!t1_tab_tearoff` | IDM_T1_TAB_TEAROFF | 12071 | handleT1TabTearoff | REAL |
| t1.tab_merge | `!t1_tab_merge` | IDM_T1_TAB_MERGE | 12072 | handleT1TabMerge | REAL |
| t1.split_vertical | `!t1_split_vertical` | IDM_T1_SPLIT_VERTICAL | 12080 | handleT1SplitVertical | REAL |
| t1.split_horizontal | `!t1_split_horizontal` | IDM_T1_SPLIT_HORIZONTAL | 12081 | handleT1SplitHorizontal | REAL |
| t1.split_grid_2x2 | `!t1_split_grid_2x2` | IDM_T1_SPLIT_GRID_2X2 | 12082 | handleT1SplitGrid2x2 | REAL |
| t1.split_close | `!t1_split_close` | IDM_T1_SPLIT_CLOSE | 12083 | handleT1SplitClose | REAL |
| t1.split_focus_next | `!t1_split_focus_next` | IDM_T1_SPLIT_FOCUS_NEXT | 12084 | handleT1SplitFocusNext | REAL |
| t1.update_check | `!t1_update_check` | IDM_T1_UPDATE_CHECK | 12090 | handleT1UpdateCheck | REAL |
| t1.update_install | `!t1_update_install` | IDM_T1_UPDATE_INSTALL | 12091 | handleT1UpdateInstall | REAL |
| t1.update_dismiss | `!t1_update_dismiss` | IDM_T1_UPDATE_DISMISS | 12092 | handleT1UpdateDismiss | REAL |
| t1.update_release_notes | `!t1_update_release_notes` | IDM_T1_UPDATE_RELEASE_NOTES | 12093 | handleT1UpdateReleaseNotes | REAL |

### TELEMETRY (7 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| telemetry.unified_core | `!telemetry_unified_core` | IDM_TELEMETRY_UNIFIED_CORE | 4164 | handleTelemetryUnifiedCore | REAL |
| telemetry.toggle | `!telemetry_toggle` | IDM_TELEMETRY_TOGGLE | 9900 | handleTelemetryToggle | REAL |
| telemetry.export_json | `!telemetry_export_json` | IDM_TELEMETRY_EXPORT_JSON | 9901 | handleTelemetryExportJson | REAL |
| telemetry.export_csv | `!telemetry_export_csv` | IDM_TELEMETRY_EXPORT_CSV | 9902 | handleTelemetryExportCsv | REAL |
| telemetry.show_dashboard | `!telemetry_show_dashboard` | IDM_TELEMETRY_SHOW_DASHBOARD | 9903 | handleTelemetryShowDashboard | REAL |
| telemetry.clear | `!telemetry_clear` | IDM_TELEMETRY_CLEAR | 9904 | handleTelemetryClear | REAL |
| telemetry.snapshot | `!telemetry_snapshot` | IDM_TELEMETRY_SNAPSHOT | 9905 | handleTelemetrySnapshot | REAL |

### TERM (9 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| term.new | `!term_new` | IDM_TERM_NEW | 601 | handleTermNew | REAL |
| term.split | `!term_split` | IDM_TERM_SPLIT | 602 | handleTermSplit | REAL |
| term.run_task | `!term_run_task` | IDM_TERM_RUN_TASK | 603 | handleTermRunTask | REAL |
| term.clear | `!term_clear` | IDM_TERM_CLEAR | 604 | handleTermClear | REAL |
| term.kill | `!term_kill` | IDM_TERM_KILL | 605 | handleTermKill | REAL |
| term.pwsh | `!term_pwsh` | IDM_TERM_PWSH | 606 | handleTermPwsh | REAL |
| term.cmd | `!term_cmd` | IDM_TERM_CMD | 607 | handleTermCmd | REAL |
| term.gitbash | `!term_gitbash` | IDM_TERM_GITBASH | 608 | handleTermGitbash | REAL |
| term.run_file | `!term_run_file` | IDM_TERM_RUN_FILE | 609 | handleTermRunFile | REAL |

### TERMINAL (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| terminal.toggle | `!terminal_toggle` | IDM_TERMINAL_TOGGLE | 4000 | handleTerminalToggle | REAL |
| terminal.powershell | `!terminal_powershell` | IDM_TERMINAL_POWERSHELL | 4001 | handleTerminalPowershell | REAL |
| terminal.cmd | `!terminal_cmd` | IDM_TERMINAL_CMD | 4002 | handleTerminalCmd | REAL |
| terminal.stop | `!terminal_stop` | IDM_TERMINAL_STOP | 4003 | handleTerminalStop | REAL |
| terminal.kill | `!terminal_kill` | IDM_TERMINAL_KILL | 4006 | handleTerminalKill | REAL |
| terminal.split_h | `!terminal_split_h` | IDM_TERMINAL_SPLIT_H | 4007 | handleTerminalSplitH | REAL |
| terminal.split_v | `!terminal_split_v` | IDM_TERMINAL_SPLIT_V | 4008 | handleTerminalSplitV | REAL |
| terminal.split_code | `!terminal_split_code` | IDM_TERMINAL_SPLIT_CODE | 4009 | handleTerminalSplitCode | REAL |
| terminal.clear_all | `!terminal_clear_all` | IDM_TERMINAL_CLEAR_ALL | 4010 | handleTerminalClearAll | REAL |
| terminal.clear | `!terminal_clear` | IDM_TERMINAL_CLEAR | 4010 | handleTerminalClear | REAL |

### THEME (16 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| theme.dark_plus | `!theme_dark_plus` | IDM_THEME_DARK_PLUS | 3101 | handleThemeDarkPlus | REAL |
| theme.light_plus | `!theme_light_plus` | IDM_THEME_LIGHT_PLUS | 3102 | handleThemeLightPlus | REAL |
| theme.monokai | `!theme_monokai` | IDM_THEME_MONOKAI | 3103 | handleThemeMonokai | REAL |
| theme.dracula | `!theme_dracula` | IDM_THEME_DRACULA | 3104 | handleThemeDracula | REAL |
| theme.nord | `!theme_nord` | IDM_THEME_NORD | 3105 | handleThemeNord | REAL |
| theme.solarized_dark | `!theme_solarized_dark` | IDM_THEME_SOLARIZED_DARK | 3106 | handleThemeSolarizedDark | REAL |
| theme.solarized_light | `!theme_solarized_light` | IDM_THEME_SOLARIZED_LIGHT | 3107 | handleThemeSolarizedLight | REAL |
| theme.cyberpunk_neon | `!theme_cyberpunk_neon` | IDM_THEME_CYBERPUNK_NEON | 3108 | handleThemeCyberpunkNeon | REAL |
| theme.gruvbox_dark | `!theme_gruvbox_dark` | IDM_THEME_GRUVBOX_DARK | 3109 | handleThemeGruvboxDark | REAL |
| theme.catppuccin_mocha | `!theme_catppuccin_mocha` | IDM_THEME_CATPPUCCIN_MOCHA | 3110 | handleThemeCatppuccinMocha | REAL |
| theme.tokyo_night | `!theme_tokyo_night` | IDM_THEME_TOKYO_NIGHT | 3111 | handleThemeTokyoNight | REAL |
| theme.rawrxd_crimson | `!theme_rawrxd_crimson` | IDM_THEME_RAWRXD_CRIMSON | 3112 | handleThemeRawrxdCrimson | REAL |
| theme.high_contrast | `!theme_high_contrast` | IDM_THEME_HIGH_CONTRAST | 3113 | handleThemeHighContrast | REAL |
| theme.one_dark_pro | `!theme_one_dark_pro` | IDM_THEME_ONE_DARK_PRO | 3114 | handleThemeOneDarkPro | REAL |
| theme.synthwave84 | `!theme_synthwave84` | IDM_THEME_SYNTHWAVE84 | 3115 | handleThemeSynthwave84 | REAL |
| theme.abyss | `!theme_abyss` | IDM_THEME_ABYSS | 3116 | handleThemeAbyss | REAL |

### TITAN (1 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| titan.toggle | `!titan_toggle` | IDM_TITAN_TOGGLE | 4230 | handleTitanToggle | REAL |

### TOOLS (14 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| tools.command_palette | `!tools_command_palette` | IDM_TOOLS_COMMAND_PALETTE | 501 | handleToolsCommandPalette | REAL |
| tools.settings | `!tools_settings` | IDM_TOOLS_SETTINGS | 502 | handleToolsSettings | REAL |
| tools.extensions | `!tools_extensions` | IDM_TOOLS_EXTENSIONS | 503 | handleToolsExtensions | REAL |
| tools.terminal | `!tools_terminal` | IDM_TOOLS_TERMINAL | 504 | handleToolsTerminal | REAL |
| tools.build | `!tools_build` | IDM_TOOLS_BUILD | 505 | handleToolsBuild | REAL |
| tools.debug | `!tools_debug` | IDM_TOOLS_DEBUG | 506 | handleToolsDebug | REAL |
| tools.pe_inspector | `!tools_pe_inspector` | IDM_TOOLS_PE_INSPECTOR | 2301 | handleToolsPeInspector | REAL |
| tools.instr_encoder | `!tools_instr_encoder` | IDM_TOOLS_INSTR_ENCODER | 2302 | handleToolsInstrEncoder | REAL |
| tools.ext_manager | `!tools_ext_manager` | IDM_TOOLS_EXT_MANAGER | 2303 | handleToolsExtManager | REAL |
| tools.options | `!tools_options` | IDM_TOOLS_OPTIONS | 2304 | handleToolsOptions | REAL |
| tools.profile_start | `!tools_profile_start` | IDM_TOOLS_PROFILE_START | 3010 | handleToolsProfileStart | REAL |
| tools.profile_stop | `!tools_profile_stop` | IDM_TOOLS_PROFILE_STOP | 3011 | handleToolsProfileStop | REAL |
| tools.profile_results | `!tools_profile_results` | IDM_TOOLS_PROFILE_RESULTS | 3012 | handleToolsProfileResults | REAL |
| tools.analyze_script | `!tools_analyze_script` | IDM_TOOLS_ANALYZE_SCRIPT | 3013 | handleToolsAnalyzeScript | REAL |

### TRANSPARENCY (9 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| transparency.100 | `!transparency_100` | IDM_TRANSPARENCY_100 | 3200 | handleTransparency100 | REAL |
| transparency.90 | `!transparency_90` | IDM_TRANSPARENCY_90 | 3201 | handleTransparency90 | REAL |
| transparency.80 | `!transparency_80` | IDM_TRANSPARENCY_80 | 3202 | handleTransparency80 | REAL |
| transparency.70 | `!transparency_70` | IDM_TRANSPARENCY_70 | 3203 | handleTransparency70 | REAL |
| transparency.60 | `!transparency_60` | IDM_TRANSPARENCY_60 | 3204 | handleTransparency60 | REAL |
| transparency.50 | `!transparency_50` | IDM_TRANSPARENCY_50 | 3205 | handleTransparency50 | REAL |
| transparency.40 | `!transparency_40` | IDM_TRANSPARENCY_40 | 3206 | handleTransparency40 | REAL |
| transparency.custom | `!transparency_custom` | IDM_TRANSPARENCY_CUSTOM | 3210 | handleTransparencyCustom | REAL |
| transparency.toggle | `!transparency_toggle` | IDM_TRANSPARENCY_TOGGLE | 3211 | handleTransparencyToggle | REAL |

### VIEW (49 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| view.toggle_sidebar | `!view_toggle_sidebar` | IDM_VIEW_TOGGLE_SIDEBAR | 301 | handleViewToggleSidebar | REAL |
| view.toggle_terminal | `!view_toggle_terminal` | IDM_VIEW_TOGGLE_TERMINAL | 302 | handleViewToggleTerminal | REAL |
| view.toggle_output | `!view_toggle_output` | IDM_VIEW_TOGGLE_OUTPUT | 303 | handleViewToggleOutput | REAL |
| view.toggle_fullscreen | `!view_toggle_fullscreen` | IDM_VIEW_TOGGLE_FULLSCREEN | 304 | handleViewToggleFullscreen | REAL |
| view.zoom_in | `!view_zoom_in` | IDM_VIEW_ZOOM_IN | 305 | handleViewZoomIn | REAL |
| view.zoom_out | `!view_zoom_out` | IDM_VIEW_ZOOM_OUT | 306 | handleViewZoomOut | REAL |
| view.zoom_reset | `!view_zoom_reset` | IDM_VIEW_ZOOM_RESET | 307 | handleViewZoomReset | REAL |
| view.activity_bar | `!view_activity_bar` | IDM_VIEW_ACTIVITY_BAR | 401 | handleViewActivityBar | REAL |
| view.primary_sidebar | `!view_primary_sidebar` | IDM_VIEW_PRIMARY_SIDEBAR | 402 | handleViewPrimarySidebar | REAL |
| view.secondary_sidebar | `!view_secondary_sidebar` | IDM_VIEW_SECONDARY_SIDEBAR | 403 | handleViewSecondarySidebar | REAL |
| view.panel | `!view_panel` | IDM_VIEW_PANEL | 404 | handleViewPanel | REAL |
| view.status_bar | `!view_status_bar` | IDM_VIEW_STATUS_BAR | 405 | handleViewStatusBar | REAL |
| view.zen_mode | `!view_zen_mode` | IDM_VIEW_ZEN_MODE | 406 | handleViewZenMode | REAL |
| view.command_palette | `!view_command_palette` | IDM_VIEW_COMMAND_PALETTE | 407 | handleViewCommandPalette | REAL |
| view.explorer | `!view_explorer` | IDM_VIEW_EXPLORER | 408 | handleViewExplorer | REAL |
| view.search | `!view_search` | IDM_VIEW_SEARCH | 409 | handleViewSearch | REAL |
| view.source_control | `!view_source_control` | IDM_VIEW_SOURCE_CONTROL | 410 | handleViewSourceControl | REAL |
| view.extensions | `!view_extensions` | IDM_VIEW_EXTENSIONS | 411 | handleViewExtensions | REAL |
| view.problems | `!view_problems` | IDM_VIEW_PROBLEMS | 412 | handleViewProblems | REAL |
| view.word_wrap | `!view_word_wrap` | IDM_VIEW_WORD_WRAP | 416 | handleViewWordWrap | REAL |
| view.line_numbers | `!view_line_numbers` | IDM_VIEW_LINE_NUMBERS | 417 | handleViewLineNumbers | REAL |
| view.minimap | `!view_minimap` | IDM_VIEW_MINIMAP | 2020 | handleViewMinimap | REAL |
| view.output_tabs | `!view_output_tabs` | IDM_VIEW_OUTPUT_TABS | 2021 | handleViewOutputTabs | REAL |
| view.module_browser | `!view_module_browser` | IDM_VIEW_MODULE_BROWSER | 2022 | handleViewModuleBrowser | REAL |
| view.theme_editor | `!view_theme_editor` | IDM_VIEW_THEME_EDITOR | 2023 | handleViewThemeEditor | REAL |
| view.floating_panel | `!view_floating_panel` | IDM_VIEW_FLOATING_PANEL | 2024 | handleViewFloatingPanel | REAL |
| view.output_panel | `!view_output_panel` | IDM_VIEW_OUTPUT_PANEL | 2025 | handleViewOutputPanel | REAL |
| view.use_streaming_loader | `!view_use_streaming_loader` | IDM_VIEW_USE_STREAMING_LOADER | 2026 | handleViewUseStreamingLoader | REAL |
| view.use_vulkan_renderer | `!view_use_vulkan_renderer` | IDM_VIEW_USE_VULKAN_RENDERER | 2027 | handleViewUseVulkanRenderer | REAL |
| view.sidebar | `!view_sidebar` | IDM_VIEW_SIDEBAR | 2028 | handleViewSidebar | REAL |
| view.terminal | `!view_terminal` | IDM_VIEW_TERMINAL | 2029 | handleViewTerminal | REAL |
| view.file_explorer | `!view_file_explorer` | IDM_VIEW_FILE_EXPLORER | 2030 | handleViewFileExplorer | REAL |
| view.ollama_service | `!view_ollama_service` | IDM_VIEW_OLLAMA_SERVICE | 2032 | handleViewOllamaService | REAL |
| view.filebrowser | `!view_filebrowser` | IDM_VIEW_FILEBROWSER | 2401 | handleViewFilebrowser | REAL |
| view.output | `!view_output` | IDM_VIEW_OUTPUT | 2402 | handleViewOutput | REAL |
| view.widget | `!view_widget` | IDM_VIEW_WIDGET | 2403 | handleViewWidget | REAL |
| view.fullscreen | `!view_fullscreen` | IDM_VIEW_FULLSCREEN | 2404 | handleViewFullscreen | REAL |
| view.dark_theme | `!view_dark_theme` | IDM_VIEW_DARK_THEME | 2405 | handleViewDarkTheme | REAL |
| view.light_theme | `!view_light_theme` | IDM_VIEW_LIGHT_THEME | 2406 | handleViewLightTheme | REAL |
| view.collaboration | `!view_collaboration` | IDM_VIEW_COLLABORATION | 3060 | handleViewCollaboration | REAL |
| view.monaco_settings | `!view_monaco_settings` | IDM_VIEW_MONACO_SETTINGS | 5310 | handleViewMonacoSettings | REAL |
| view.thermal_dashboard | `!view_thermal_dashboard` | IDM_VIEW_THERMAL_DASHBOARD | 5311 | handleViewThermalDashboard | REAL |
| view.agent_panel | `!view_agent_panel` | IDM_VIEW_AGENT_PANEL | 7057 | handleViewAgentPanel | REAL |
| view.toggle_monaco | `!view_toggle_monaco` | IDM_VIEW_TOGGLE_MONACO | 9100 | handleViewToggleMonaco | REAL |
| view.monaco_devtools | `!view_monaco_devtools` | IDM_VIEW_MONACO_DEVTOOLS | 9101 | handleViewMonacoDevtools | REAL |
| view.monaco_reload | `!view_monaco_reload` | IDM_VIEW_MONACO_RELOAD | 9102 | handleViewMonacoReload | REAL |
| view.monaco_zoom_in | `!view_monaco_zoom_in` | IDM_VIEW_MONACO_ZOOM_IN | 9103 | handleViewMonacoZoomIn | REAL |
| view.monaco_zoom_out | `!view_monaco_zoom_out` | IDM_VIEW_MONACO_ZOOM_OUT | 9104 | handleViewMonacoZoomOut | REAL |
| view.monaco_sync_theme | `!view_monaco_sync_theme` | IDM_VIEW_MONACO_SYNC_THEME | 9105 | handleViewMonacoSyncTheme | REAL |

### VOICE (23 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| voice.record | `!voice_record` | IDM_VOICE_RECORD | 9700 | handleVoiceRecord | REAL |
| voice.ptt | `!voice_ptt` | IDM_VOICE_PTT | 9701 | handleVoicePtt | REAL |
| voice.speak | `!voice_speak` | IDM_VOICE_SPEAK | 9702 | handleVoiceSpeak | REAL |
| voice.join_room | `!voice_join_room` | IDM_VOICE_JOIN_ROOM | 9703 | handleVoiceJoinRoom | REAL |
| voice.show_devices | `!voice_show_devices` | IDM_VOICE_SHOW_DEVICES | 9704 | handleVoiceShowDevices | REAL |
| voice.metrics | `!voice_metrics` | IDM_VOICE_METRICS | 9705 | handleVoiceMetrics | REAL |
| voice.toggle_panel | `!voice_toggle_panel` | IDM_VOICE_TOGGLE_PANEL | 9706 | handleVoiceTogglePanel | REAL |
| voice.mode_ptt | `!voice_mode_ptt` | IDM_VOICE_MODE_PTT | 9707 | handleVoiceModePtt | REAL |
| voice.mode_continuous | `!voice_mode_continuous` | IDM_VOICE_MODE_CONTINUOUS | 9708 | handleVoiceModeContinuous | REAL |
| voice.mode_disabled | `!voice_mode_disabled` | IDM_VOICE_MODE_DISABLED | 9709 | handleVoiceModeDisabled | REAL |
| voice.auto_toggle | `!voice_auto_toggle` | IDM_VOICE_AUTO_TOGGLE | 10200 | handleVoiceAutoToggle | REAL |
| voice.auto_settings | `!voice_auto_settings` | IDM_VOICE_AUTO_SETTINGS | 10201 | handleVoiceAutoSettings | REAL |
| voice.auto_next | `!voice_auto_next` | IDM_VOICE_AUTO_NEXT | 10202 | handleVoiceAutoNext | REAL |
| voice.auto_next_voice | `!voice_auto_next_voice` | IDM_VOICE_AUTO_NEXT_VOICE | 10202 | handleVoiceAutoNextVoice | REAL |
| voice.auto_prev | `!voice_auto_prev` | IDM_VOICE_AUTO_PREV | 10203 | handleVoiceAutoPrev | REAL |
| voice.auto_prev_voice | `!voice_auto_prev_voice` | IDM_VOICE_AUTO_PREV_VOICE | 10203 | handleVoiceAutoPrevVoice | REAL |
| voice.auto_rate_up | `!voice_auto_rate_up` | IDM_VOICE_AUTO_RATE_UP | 10204 | handleVoiceAutoRateUp | REAL |
| voice.auto_rate_down | `!voice_auto_rate_down` | IDM_VOICE_AUTO_RATE_DOWN | 10205 | handleVoiceAutoRateDown | REAL |
| voice.auto_stop | `!voice_auto_stop` | IDM_VOICE_AUTO_STOP | 10206 | handleVoiceAutoStop | REAL |
| voice.toggle_listening | `!voice_toggle_listening` | IDM_VOICE_TOGGLE_LISTENING | 10300 | handleVoiceToggleListening | REAL |
| voice.toggle_tts | `!voice_toggle_tts` | IDM_VOICE_TOGGLE_TTS | 10301 | handleVoiceToggleTts | REAL |
| voice.settings | `!voice_settings` | IDM_VOICE_SETTINGS | 10302 | handleVoiceSettings | REAL |
| voice.tts_test | `!voice_tts_test` | IDM_VOICE_TTS_TEST | 10303 | handleVoiceTtsTest | REAL |

### VSCEXT (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| vscext.api_status | `!vscext_api_status` | IDM_VSCEXT_API_STATUS | 10000 | handleVscextApiStatus | REAL |
| vscext.api_reload | `!vscext_api_reload` | IDM_VSCEXT_API_RELOAD | 10001 | handleVscextApiReload | REAL |
| vscext.api_list_commands | `!vscext_api_list_commands` | IDM_VSCEXT_API_LIST_COMMANDS | 10002 | handleVscextApiListCommands | REAL |
| vscext.api_list_providers | `!vscext_api_list_providers` | IDM_VSCEXT_API_LIST_PROVIDERS | 10003 | handleVscextApiListProviders | REAL |
| vscext.api_diagnostics | `!vscext_api_diagnostics` | IDM_VSCEXT_API_DIAGNOSTICS | 10004 | handleVscextApiDiagnostics | REAL |
| vscext.api_extensions | `!vscext_api_extensions` | IDM_VSCEXT_API_EXTENSIONS | 10005 | handleVscextApiExtensions | REAL |
| vscext.api_stats | `!vscext_api_stats` | IDM_VSCEXT_API_STATS | 10006 | handleVscextApiStats | REAL |
| vscext.api_load_native | `!vscext_api_load_native` | IDM_VSCEXT_API_LOAD_NATIVE | 10007 | handleVscextApiLoadNative | REAL |
| vscext.api_deactivate_all | `!vscext_api_deactivate_all` | IDM_VSCEXT_API_DEACTIVATE_ALL | 10008 | handleVscextApiDeactivateAll | REAL |
| vscext.api_export_config | `!vscext_api_export_config` | IDM_VSCEXT_API_EXPORT_CONFIG | 10009 | handleVscextApiExportConfig | REAL |

## How to Update

1. Add new `IDM_*` defines in `Win32IDE.h` (or any scanned header)
2. Optionally add handler declarations in `feature_handlers.h`
3. Run: `python scripts/auto_register_commands.py`
4. The system automatically generates registrations + stubs
5. Replace stubs with real handlers as you implement features
6. CMake pre-build step does this automatically on every build
