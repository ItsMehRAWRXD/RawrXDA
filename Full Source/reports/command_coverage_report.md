# RawrXD Command Registry Coverage Report

Generated: 2026-02-11 07:10:24

## Summary

| Metric | Count |
|---|---|
| Total IDM_* defines found | 439 |
| Already registered (manual) | 0 |
| Auto-registered (this run) | 432 |
| **Total after auto-reg** | **432** |
| Handler declarations found | 258 |
| With existing handler | 146 |
| With stub handler | 286 |
| Coverage | 98.4% |

## Auto-Registered Commands

### AGENT (11 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| agent.start_loop | `!agent_start_loop` | IDM_AGENT_START_LOOP | 4100 | handleAgentStartLoop | STUB |
| agent.execute_cmd | `!agent_execute_cmd` | IDM_AGENT_EXECUTE_CMD | 4101 | handleAgentExecuteCmd | STUB |
| agent.configure_model | `!agent_configure_model` | IDM_AGENT_CONFIGURE_MODEL | 4102 | handleAgentConfigureModel | STUB |
| agent.view_tools | `!agent_view_tools` | IDM_AGENT_VIEW_TOOLS | 4103 | handleAgentViewTools | REAL |
| agent.view_status | `!agent_view_status` | IDM_AGENT_VIEW_STATUS | 4104 | handleAgentViewStatus | REAL |
| agent.stop | `!agent_stop` | IDM_AGENT_STOP | 4105 | handleAgentStop | REAL |
| agent.memory | `!agent_memory` | IDM_AGENT_MEMORY | 4106 | handleAgentMemory | REAL |
| agent.memory_view | `!agent_memory_view` | IDM_AGENT_MEMORY_VIEW | 4107 | handleAgentMemoryView | REAL |
| agent.memory_clear | `!agent_memory_clear` | IDM_AGENT_MEMORY_CLEAR | 4108 | handleAgentMemoryClear | REAL |
| agent.memory_export | `!agent_memory_export` | IDM_AGENT_MEMORY_EXPORT | 4109 | handleAgentMemoryExport | REAL |
| agent.bounded_loop | `!agent_bounded_loop` | IDM_AGENT_BOUNDED_LOOP | 4120 | handleAgentBoundedLoop | REAL |

### AI (20 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| ai.inline_complete | `!ai_inline_complete` | IDM_AI_INLINE_COMPLETE | 401 | handleAiInlineComplete | STUB |
| ai.chat_mode | `!ai_chat_mode` | IDM_AI_CHAT_MODE | 402 | handleAiChatMode | STUB |
| ai.explain_code | `!ai_explain_code` | IDM_AI_EXPLAIN_CODE | 403 | handleAiExplainCode | STUB |
| ai.refactor | `!ai_refactor` | IDM_AI_REFACTOR | 404 | handleAiRefactor | STUB |
| ai.generate_tests | `!ai_generate_tests` | IDM_AI_GENERATE_TESTS | 405 | handleAiGenerateTests | STUB |
| ai.generate_docs | `!ai_generate_docs` | IDM_AI_GENERATE_DOCS | 406 | handleAiGenerateDocs | STUB |
| ai.fix_errors | `!ai_fix_errors` | IDM_AI_FIX_ERRORS | 407 | handleAiFixErrors | STUB |
| ai.optimize_code | `!ai_optimize_code` | IDM_AI_OPTIMIZE_CODE | 408 | handleAiOptimizeCode | STUB |
| ai.model_select | `!ai_model_select` | IDM_AI_MODEL_SELECT | 409 | handleAiModelSelect | STUB |
| ai.mode_max | `!ai_mode_max` | IDM_AI_MODE_MAX | 4200 | handleAiModeMax | STUB |
| ai.mode_deep_think | `!ai_mode_deep_think` | IDM_AI_MODE_DEEP_THINK | 4201 | handleAiModeDeepThink | STUB |
| ai.mode_deep_research | `!ai_mode_deep_research` | IDM_AI_MODE_DEEP_RESEARCH | 4202 | handleAiModeDeepResearch | STUB |
| ai.mode_no_refusal | `!ai_mode_no_refusal` | IDM_AI_MODE_NO_REFUSAL | 4203 | handleAiModeNoRefusal | STUB |
| ai.context_4k | `!ai_context_4k` | IDM_AI_CONTEXT_4K | 4210 | handleAiContext4k | STUB |
| ai.context_32k | `!ai_context_32k` | IDM_AI_CONTEXT_32K | 4211 | handleAiContext32k | STUB |
| ai.context_64k | `!ai_context_64k` | IDM_AI_CONTEXT_64K | 4212 | handleAiContext64k | STUB |
| ai.context_128k | `!ai_context_128k` | IDM_AI_CONTEXT_128K | 4213 | handleAiContext128k | STUB |
| ai.context_256k | `!ai_context_256k` | IDM_AI_CONTEXT_256K | 4214 | handleAiContext256k | STUB |
| ai.context_512k | `!ai_context_512k` | IDM_AI_CONTEXT_512K | 4215 | handleAiContext512k | STUB |
| ai.context_1m | `!ai_context_1m` | IDM_AI_CONTEXT_1M | 4216 | handleAiContext1m | STUB |

### ASM (12 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| asm.parse_symbols | `!asm_parse_symbols` | IDM_ASM_PARSE_SYMBOLS | 5082 | handleAsmParseSymbols | STUB |
| asm.goto_label | `!asm_goto_label` | IDM_ASM_GOTO_LABEL | 5083 | handleAsmGotoLabel | STUB |
| asm.find_label_refs | `!asm_find_label_refs` | IDM_ASM_FIND_LABEL_REFS | 5084 | handleAsmFindLabelRefs | STUB |
| asm.show_symbol_table | `!asm_show_symbol_table` | IDM_ASM_SHOW_SYMBOL_TABLE | 5085 | handleAsmShowSymbolTable | STUB |
| asm.instruction_info | `!asm_instruction_info` | IDM_ASM_INSTRUCTION_INFO | 5086 | handleAsmInstructionInfo | REAL |
| asm.register_info | `!asm_register_info` | IDM_ASM_REGISTER_INFO | 5087 | handleAsmRegisterInfo | REAL |
| asm.analyze_block | `!asm_analyze_block` | IDM_ASM_ANALYZE_BLOCK | 5088 | handleAsmAnalyzeBlock | REAL |
| asm.show_call_graph | `!asm_show_call_graph` | IDM_ASM_SHOW_CALL_GRAPH | 5089 | handleAsmShowCallGraph | STUB |
| asm.show_data_flow | `!asm_show_data_flow` | IDM_ASM_SHOW_DATA_FLOW | 5090 | handleAsmShowDataFlow | STUB |
| asm.detect_convention | `!asm_detect_convention` | IDM_ASM_DETECT_CONVENTION | 5091 | handleAsmDetectConvention | REAL |
| asm.show_sections | `!asm_show_sections` | IDM_ASM_SHOW_SECTIONS | 5092 | handleAsmShowSections | STUB |
| asm.clear_symbols | `!asm_clear_symbols` | IDM_ASM_CLEAR_SYMBOLS | 5093 | handleAsmClearSymbols | REAL |

### AUDIT (7 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| audit.show_dashboard | `!audit_show_dashboard` | IDM_AUDIT_SHOW_DASHBOARD | 9500 | handleAuditShowDashboard | STUB |
| audit.run_full | `!audit_run_full` | IDM_AUDIT_RUN_FULL | 9501 | handleAuditRunFull | STUB |
| audit.detect_stubs | `!audit_detect_stubs` | IDM_AUDIT_DETECT_STUBS | 9502 | handleAuditDetectStubs | STUB |
| audit.check_menus | `!audit_check_menus` | IDM_AUDIT_CHECK_MENUS | 9503 | handleAuditCheckMenus | STUB |
| audit.run_tests | `!audit_run_tests` | IDM_AUDIT_RUN_TESTS | 9504 | handleAuditRunTests | STUB |
| audit.export_report | `!audit_export_report` | IDM_AUDIT_EXPORT_REPORT | 9505 | handleAuditExportReport | STUB |
| audit.quick_stats | `!audit_quick_stats` | IDM_AUDIT_QUICK_STATS | 9506 | handleAuditQuickStats | STUB |

### AUTONOMY (6 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| autonomy.toggle | `!autonomy_toggle` | IDM_AUTONOMY_TOGGLE | 4150 | handleAutonomyToggle | REAL |
| autonomy.start | `!autonomy_start` | IDM_AUTONOMY_START | 4151 | handleAutonomyStart | REAL |
| autonomy.stop | `!autonomy_stop` | IDM_AUTONOMY_STOP | 4152 | handleAutonomyStop | REAL |
| autonomy.set_goal | `!autonomy_set_goal` | IDM_AUTONOMY_SET_GOAL | 4153 | handleAutonomySetGoal | STUB |
| autonomy.status | `!autonomy_status` | IDM_AUTONOMY_STATUS | 4154 | handleAutonomyStatus | STUB |
| autonomy.memory | `!autonomy_memory` | IDM_AUTONOMY_MEMORY | 4155 | handleAutonomyMemory | STUB |

### BACKEND (11 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| backend.switch_local | `!backend_switch_local` | IDM_BACKEND_SWITCH_LOCAL | 5037 | handleBackendSwitchLocal | REAL |
| backend.switch_ollama | `!backend_switch_ollama` | IDM_BACKEND_SWITCH_OLLAMA | 5038 | handleBackendSwitchOllama | REAL |
| backend.switch_openai | `!backend_switch_openai` | IDM_BACKEND_SWITCH_OPENAI | 5039 | handleBackendSwitchOpenai | STUB |
| backend.switch_claude | `!backend_switch_claude` | IDM_BACKEND_SWITCH_CLAUDE | 5040 | handleBackendSwitchClaude | REAL |
| backend.switch_gemini | `!backend_switch_gemini` | IDM_BACKEND_SWITCH_GEMINI | 5041 | handleBackendSwitchGemini | REAL |
| backend.show_status | `!backend_show_status` | IDM_BACKEND_SHOW_STATUS | 5042 | handleBackendShowStatus | REAL |
| backend.show_switcher | `!backend_show_switcher` | IDM_BACKEND_SHOW_SWITCHER | 5043 | handleBackendShowSwitcher | REAL |
| backend.configure | `!backend_configure` | IDM_BACKEND_CONFIGURE | 5044 | handleBackendConfigure | REAL |
| backend.health_check | `!backend_health_check` | IDM_BACKEND_HEALTH_CHECK | 5045 | handleBackendHealthCheck | REAL |
| backend.set_api_key | `!backend_set_api_key` | IDM_BACKEND_SET_API_KEY | 5046 | handleBackendSetApiKey | REAL |
| backend.save_configs | `!backend_save_configs` | IDM_BACKEND_SAVE_CONFIGS | 5047 | handleBackendSaveConfigs | REAL |

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

### DECOMP (6 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| decomp.rename_var | `!decomp_rename_var` | IDM_DECOMP_RENAME_VAR | 8001 | handleDecompRenameVar | STUB |
| decomp.goto_def | `!decomp_goto_def` | IDM_DECOMP_GOTO_DEF | 8002 | handleDecompGotoDef | STUB |
| decomp.find_refs | `!decomp_find_refs` | IDM_DECOMP_FIND_REFS | 8003 | handleDecompFindRefs | STUB |
| decomp.copy_line | `!decomp_copy_line` | IDM_DECOMP_COPY_LINE | 8004 | handleDecompCopyLine | STUB |
| decomp.copy_all | `!decomp_copy_all` | IDM_DECOMP_COPY_ALL | 8005 | handleDecompCopyAll | STUB |
| decomp.goto_addr | `!decomp_goto_addr` | IDM_DECOMP_GOTO_ADDR | 8006 | handleDecompGotoAddr | STUB |

### EDIT (18 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| edit.selectall | `!edit_selectall` | IDM_EDIT_SELECTALL | 208 | handleEditSelectall | STUB |
| edit.multicursor_add | `!edit_multicursor_add` | IDM_EDIT_MULTICURSOR_ADD | 209 | handleEditMulticursorAdd | STUB |
| edit.multicursor_remove | `!edit_multicursor_remove` | IDM_EDIT_MULTICURSOR_REMOVE | 210 | handleEditMulticursorRemove | STUB |
| edit.goto_line | `!edit_goto_line` | IDM_EDIT_GOTO_LINE | 211 | handleEditGotoLine | STUB |
| edit.select_all | `!edit_select_all` | IDM_EDIT_SELECT_ALL | 2006 | handleEditSelectAll | REAL |
| edit.undo | `!edit_undo` | IDM_EDIT_UNDO | 2007 | handleEditUndo | REAL |
| edit.redo | `!edit_redo` | IDM_EDIT_REDO | 2008 | handleEditRedo | REAL |
| edit.cut | `!edit_cut` | IDM_EDIT_CUT | 2009 | handleEditCut | REAL |
| edit.copy | `!edit_copy` | IDM_EDIT_COPY | 2010 | handleEditCopy | REAL |
| edit.paste | `!edit_paste` | IDM_EDIT_PASTE | 2011 | handleEditPaste | REAL |
| edit.snippet | `!edit_snippet` | IDM_EDIT_SNIPPET | 2012 | handleEditSnippet | STUB |
| edit.copy_format | `!edit_copy_format` | IDM_EDIT_COPY_FORMAT | 2013 | handleEditCopyFormat | STUB |
| edit.paste_plain | `!edit_paste_plain` | IDM_EDIT_PASTE_PLAIN | 2014 | handleEditPastePlain | STUB |
| edit.clipboard_history | `!edit_clipboard_history` | IDM_EDIT_CLIPBOARD_HISTORY | 2015 | handleEditClipboardHistory | STUB |
| edit.find | `!edit_find` | IDM_EDIT_FIND | 2016 | handleEditFind | REAL |
| edit.replace | `!edit_replace` | IDM_EDIT_REPLACE | 2017 | handleEditReplace | REAL |
| edit.find_next | `!edit_find_next` | IDM_EDIT_FIND_NEXT | 2018 | handleEditFindNext | STUB |
| edit.find_prev | `!edit_find_prev` | IDM_EDIT_FIND_PREV | 2019 | handleEditFindPrev | STUB |

### EDITOR (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| editor.engine_richedit_cmd | `!editor_engine_richedit_cmd` | IDM_EDITOR_ENGINE_RICHEDIT_CMD | 9300 | handleEditorEngineRicheditCmd | STUB |
| editor.engine_webview2_cmd | `!editor_engine_webview2_cmd` | IDM_EDITOR_ENGINE_WEBVIEW2_CMD | 9301 | handleEditorEngineWebview2Cmd | STUB |
| editor.engine_monacocore_cmd | `!editor_engine_monacocore_cmd` | IDM_EDITOR_ENGINE_MONACOCORE_CMD | 9302 | handleEditorEngineMonacocoreCmd | STUB |
| editor.engine_cycle_cmd | `!editor_engine_cycle_cmd` | IDM_EDITOR_ENGINE_CYCLE_CMD | 9303 | handleEditorEngineCycleCmd | STUB |
| editor.engine_status_cmd | `!editor_engine_status_cmd` | IDM_EDITOR_ENGINE_STATUS_CMD | 9304 | handleEditorEngineStatusCmd | STUB |

### FILE (19 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| file.autosave | `!file_autosave` | IDM_FILE_AUTOSAVE | 105 | handleFileAutosave | STUB |
| file.close_folder | `!file_close_folder` | IDM_FILE_CLOSE_FOLDER | 106 | handleFileCloseFolder | STUB |
| file.open_folder | `!file_open_folder` | IDM_FILE_OPEN_FOLDER | 108 | handleFileOpenFolder | STUB |
| file.new_window | `!file_new_window` | IDM_FILE_NEW_WINDOW | 109 | handleFileNewWindow | STUB |
| file.close_tab | `!file_close_tab` | IDM_FILE_CLOSE_TAB | 110 | handleFileCloseTab | STUB |
| file.saveall | `!file_saveall` | IDM_FILE_SAVEALL | 1005 | handleFileSaveall | STUB |
| file.close | `!file_close` | IDM_FILE_CLOSE | 1006 | handleFileClose | REAL |
| file.recent_clear | `!file_recent_clear` | IDM_FILE_RECENT_CLEAR | 1020 | handleFileRecentClear | STUB |
| file.load_model | `!file_load_model` | IDM_FILE_LOAD_MODEL | 1030 | handleFileLoadModel | REAL |
| file.model_from_hf | `!file_model_from_hf` | IDM_FILE_MODEL_FROM_HF | 1031 | handleFileModelFromHf | STUB |
| file.model_from_ollama | `!file_model_from_ollama` | IDM_FILE_MODEL_FROM_OLLAMA | 1032 | handleFileModelFromOllama | REAL |
| file.model_from_url | `!file_model_from_url` | IDM_FILE_MODEL_FROM_URL | 1033 | handleFileModelFromUrl | STUB |
| file.model_unified | `!file_model_unified` | IDM_FILE_MODEL_UNIFIED | 1034 | handleFileModelUnified | STUB |
| file.model_quick_load | `!file_model_quick_load` | IDM_FILE_MODEL_QUICK_LOAD | 1035 | handleFileModelQuickLoad | STUB |
| file.new | `!file_new` | IDM_FILE_NEW | 2001 | handleFileNew | REAL |
| file.open | `!file_open` | IDM_FILE_OPEN | 2002 | handleFileOpen | REAL |
| file.save | `!file_save` | IDM_FILE_SAVE | 2003 | handleFileSave | REAL |
| file.saveas | `!file_saveas` | IDM_FILE_SAVEAS | 2004 | handleFileSaveas | STUB |
| file.exit | `!file_exit` | IDM_FILE_EXIT | 2005 | handleFileExit | STUB |

### GAUNTLET (2 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| gauntlet.run | `!gauntlet_run` | IDM_GAUNTLET_RUN | 9600 | handleGauntletRun | STUB |
| gauntlet.export | `!gauntlet_export` | IDM_GAUNTLET_EXPORT | 9601 | handleGauntletExport | STUB |

### GIT (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| git.status | `!git_status` | IDM_GIT_STATUS | 3020 | handleGitStatus | REAL |
| git.commit | `!git_commit` | IDM_GIT_COMMIT | 3021 | handleGitCommit | REAL |
| git.push | `!git_push` | IDM_GIT_PUSH | 3022 | handleGitPush | REAL |
| git.pull | `!git_pull` | IDM_GIT_PULL | 3023 | handleGitPull | REAL |
| git.panel | `!git_panel` | IDM_GIT_PANEL | 3024 | handleGitPanel | STUB |

### GOV (4 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| gov.status | `!gov_status` | IDM_GOV_STATUS | 5118 | handleGovStatus | REAL |
| gov.submit_command | `!gov_submit_command` | IDM_GOV_SUBMIT_COMMAND | 5119 | handleGovSubmitCommand | REAL |
| gov.kill_all | `!gov_kill_all` | IDM_GOV_KILL_ALL | 5120 | handleGovKillAll | REAL |
| gov.task_list | `!gov_task_list` | IDM_GOV_TASK_LIST | 5121 | handleGovTaskList | REAL |

### HELP (6 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| help.docs | `!help_docs` | IDM_HELP_DOCS | 601 | handleHelpDocs | REAL |
| help.shortcuts | `!help_shortcuts` | IDM_HELP_SHORTCUTS | 603 | handleHelpShortcuts | REAL |
| help.about | `!help_about` | IDM_HELP_ABOUT | 4001 | handleHelpAbout | REAL |
| help.cmdref | `!help_cmdref` | IDM_HELP_CMDREF | 4002 | handleHelpCmdref | STUB |
| help.psdocs | `!help_psdocs` | IDM_HELP_PSDOCS | 4003 | handleHelpPsdocs | STUB |
| help.search | `!help_search` | IDM_HELP_SEARCH | 4004 | handleHelpSearch | STUB |

### HOTPATCH (17 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| hotpatch.show_status | `!hotpatch_show_status` | IDM_HOTPATCH_SHOW_STATUS | 9001 | handleHotpatchShowStatus | STUB |
| hotpatch.memory_apply | `!hotpatch_memory_apply` | IDM_HOTPATCH_MEMORY_APPLY | 9002 | handleHotpatchMemoryApply | STUB |
| hotpatch.memory_revert | `!hotpatch_memory_revert` | IDM_HOTPATCH_MEMORY_REVERT | 9003 | handleHotpatchMemoryRevert | STUB |
| hotpatch.byte_apply | `!hotpatch_byte_apply` | IDM_HOTPATCH_BYTE_APPLY | 9004 | handleHotpatchByteApply | STUB |
| hotpatch.byte_search | `!hotpatch_byte_search` | IDM_HOTPATCH_BYTE_SEARCH | 9005 | handleHotpatchByteSearch | STUB |
| hotpatch.server_add | `!hotpatch_server_add` | IDM_HOTPATCH_SERVER_ADD | 9006 | handleHotpatchServerAdd | STUB |
| hotpatch.server_remove | `!hotpatch_server_remove` | IDM_HOTPATCH_SERVER_REMOVE | 9007 | handleHotpatchServerRemove | STUB |
| hotpatch.proxy_bias | `!hotpatch_proxy_bias` | IDM_HOTPATCH_PROXY_BIAS | 9008 | handleHotpatchProxyBias | STUB |
| hotpatch.proxy_rewrite | `!hotpatch_proxy_rewrite` | IDM_HOTPATCH_PROXY_REWRITE | 9009 | handleHotpatchProxyRewrite | STUB |
| hotpatch.proxy_terminate | `!hotpatch_proxy_terminate` | IDM_HOTPATCH_PROXY_TERMINATE | 9010 | handleHotpatchProxyTerminate | STUB |
| hotpatch.proxy_validate | `!hotpatch_proxy_validate` | IDM_HOTPATCH_PROXY_VALIDATE | 9011 | handleHotpatchProxyValidate | STUB |
| hotpatch.preset_save | `!hotpatch_preset_save` | IDM_HOTPATCH_PRESET_SAVE | 9012 | handleHotpatchPresetSave | STUB |
| hotpatch.preset_load | `!hotpatch_preset_load` | IDM_HOTPATCH_PRESET_LOAD | 9013 | handleHotpatchPresetLoad | STUB |
| hotpatch.show_event_log | `!hotpatch_show_event_log` | IDM_HOTPATCH_SHOW_EVENT_LOG | 9014 | handleHotpatchShowEventLog | STUB |
| hotpatch.reset_stats | `!hotpatch_reset_stats` | IDM_HOTPATCH_RESET_STATS | 9015 | handleHotpatchResetStats | STUB |
| hotpatch.toggle_all | `!hotpatch_toggle_all` | IDM_HOTPATCH_TOGGLE_ALL | 9016 | handleHotpatchToggleAll | STUB |
| hotpatch.show_proxy_stats | `!hotpatch_show_proxy_stats` | IDM_HOTPATCH_SHOW_PROXY_STATS | 9017 | handleHotpatchShowProxyStats | STUB |

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

### LSP (22 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| lsp.start_all | `!lsp_start_all` | IDM_LSP_START_ALL | 5058 | handleLspStartAll | REAL |
| lsp.stop_all | `!lsp_stop_all` | IDM_LSP_STOP_ALL | 5059 | handleLspStopAll | REAL |
| lsp.show_status | `!lsp_show_status` | IDM_LSP_SHOW_STATUS | 5060 | handleLspShowStatus | STUB |
| lsp.goto_definition | `!lsp_goto_definition` | IDM_LSP_GOTO_DEFINITION | 5061 | handleLspGotoDefinition | STUB |
| lsp.find_references | `!lsp_find_references` | IDM_LSP_FIND_REFERENCES | 5062 | handleLspFindReferences | STUB |
| lsp.rename_symbol | `!lsp_rename_symbol` | IDM_LSP_RENAME_SYMBOL | 5063 | handleLspRenameSymbol | STUB |
| lsp.hover_info | `!lsp_hover_info` | IDM_LSP_HOVER_INFO | 5064 | handleLspHoverInfo | STUB |
| lsp.show_diagnostics | `!lsp_show_diagnostics` | IDM_LSP_SHOW_DIAGNOSTICS | 5065 | handleLspShowDiagnostics | STUB |
| lsp.restart_server | `!lsp_restart_server` | IDM_LSP_RESTART_SERVER | 5066 | handleLspRestartServer | STUB |
| lsp.clear_diagnostics | `!lsp_clear_diagnostics` | IDM_LSP_CLEAR_DIAGNOSTICS | 5067 | handleLspClearDiagnostics | STUB |
| lsp.show_symbol_info | `!lsp_show_symbol_info` | IDM_LSP_SHOW_SYMBOL_INFO | 5068 | handleLspShowSymbolInfo | STUB |
| lsp.configure | `!lsp_configure` | IDM_LSP_CONFIGURE | 5069 | handleLspConfigure | REAL |
| lsp.save_config | `!lsp_save_config` | IDM_LSP_SAVE_CONFIG | 5070 | handleLspSaveConfig | REAL |
| lsp.server_start | `!lsp_server_start` | IDM_LSP_SERVER_START | 9200 | handleLspServerStart | STUB |
| lsp.server_stop | `!lsp_server_stop` | IDM_LSP_SERVER_STOP | 9201 | handleLspServerStop | STUB |
| lsp.server_status | `!lsp_server_status` | IDM_LSP_SERVER_STATUS | 9202 | handleLspServerStatus | STUB |
| lsp.server_reindex | `!lsp_server_reindex` | IDM_LSP_SERVER_REINDEX | 9203 | handleLspServerReindex | STUB |
| lsp.server_stats | `!lsp_server_stats` | IDM_LSP_SERVER_STATS | 9204 | handleLspServerStats | STUB |
| lsp.server_publish_diag | `!lsp_server_publish_diag` | IDM_LSP_SERVER_PUBLISH_DIAG | 9205 | handleLspServerPublishDiag | STUB |
| lsp.server_config | `!lsp_server_config` | IDM_LSP_SERVER_CONFIG | 9206 | handleLspServerConfig | STUB |
| lsp.server_export_symbols | `!lsp_server_export_symbols` | IDM_LSP_SERVER_EXPORT_SYMBOLS | 9207 | handleLspServerExportSymbols | STUB |
| lsp.server_launch_stdio | `!lsp_server_launch_stdio` | IDM_LSP_SERVER_LAUNCH_STDIO | 9208 | handleLspServerLaunchStdio | STUB |

### MODULES (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| modules.refresh | `!modules_refresh` | IDM_MODULES_REFRESH | 3050 | handleModulesRefresh | STUB |
| modules.import | `!modules_import` | IDM_MODULES_IMPORT | 3051 | handleModulesImport | STUB |
| modules.export | `!modules_export` | IDM_MODULES_EXPORT | 3052 | handleModulesExport | STUB |

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

### PDB (9 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| pdb.load | `!pdb_load` | IDM_PDB_LOAD | 9400 | handlePdbLoad | STUB |
| pdb.fetch | `!pdb_fetch` | IDM_PDB_FETCH | 9401 | handlePdbFetch | STUB |
| pdb.status | `!pdb_status` | IDM_PDB_STATUS | 9402 | handlePdbStatus | STUB |
| pdb.cache_clear | `!pdb_cache_clear` | IDM_PDB_CACHE_CLEAR | 9403 | handlePdbCacheClear | STUB |
| pdb.enable | `!pdb_enable` | IDM_PDB_ENABLE | 9404 | handlePdbEnable | STUB |
| pdb.resolve | `!pdb_resolve` | IDM_PDB_RESOLVE | 9405 | handlePdbResolve | STUB |
| pdb.imports | `!pdb_imports` | IDM_PDB_IMPORTS | 9410 | handlePdbImports | STUB |
| pdb.exports | `!pdb_exports` | IDM_PDB_EXPORTS | 9411 | handlePdbExports | STUB |
| pdb.iat_status | `!pdb_iat_status` | IDM_PDB_IAT_STATUS | 9412 | handlePdbIatStatus | STUB |

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
| qw.shortcut_editor | `!qw_shortcut_editor` | IDM_QW_SHORTCUT_EDITOR | 9800 | handleQwShortcutEditor | STUB |
| qw.shortcut_reset | `!qw_shortcut_reset` | IDM_QW_SHORTCUT_RESET | 9801 | handleQwShortcutReset | STUB |
| qw.backup_create | `!qw_backup_create` | IDM_QW_BACKUP_CREATE | 9810 | handleQwBackupCreate | STUB |
| qw.backup_restore | `!qw_backup_restore` | IDM_QW_BACKUP_RESTORE | 9811 | handleQwBackupRestore | STUB |
| qw.backup_auto_toggle | `!qw_backup_auto_toggle` | IDM_QW_BACKUP_AUTO_TOGGLE | 9812 | handleQwBackupAutoToggle | STUB |
| qw.backup_list | `!qw_backup_list` | IDM_QW_BACKUP_LIST | 9813 | handleQwBackupList | STUB |
| qw.backup_prune | `!qw_backup_prune` | IDM_QW_BACKUP_PRUNE | 9814 | handleQwBackupPrune | STUB |
| qw.alert_toggle_monitor | `!qw_alert_toggle_monitor` | IDM_QW_ALERT_TOGGLE_MONITOR | 9820 | handleQwAlertToggleMonitor | STUB |
| qw.alert_show_history | `!qw_alert_show_history` | IDM_QW_ALERT_SHOW_HISTORY | 9821 | handleQwAlertShowHistory | STUB |
| qw.alert_dismiss_all | `!qw_alert_dismiss_all` | IDM_QW_ALERT_DISMISS_ALL | 9822 | handleQwAlertDismissAll | STUB |
| qw.alert_resource_status | `!qw_alert_resource_status` | IDM_QW_ALERT_RESOURCE_STATUS | 9823 | handleQwAlertResourceStatus | STUB |
| qw.slo_dashboard | `!qw_slo_dashboard` | IDM_QW_SLO_DASHBOARD | 9830 | handleQwSloDashboard | STUB |

### REPLAY (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| replay.status | `!replay_status` | IDM_REPLAY_STATUS | 5126 | handleReplayStatus | REAL |
| replay.export_session | `!replay_export_session` | IDM_REPLAY_EXPORT_SESSION | 5128 | handleReplayExportSession | REAL |
| replay.checkpoint | `!replay_checkpoint` | IDM_REPLAY_CHECKPOINT | 5129 | handleReplayCheckpoint | REAL |

### REVENG (20 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| reveng.analyze | `!reveng_analyze` | IDM_REVENG_ANALYZE | 4300 | handleRevengAnalyze | STUB |
| reveng.disasm | `!reveng_disasm` | IDM_REVENG_DISASM | 4301 | handleRevengDisasm | STUB |
| reveng.dumpbin | `!reveng_dumpbin` | IDM_REVENG_DUMPBIN | 4302 | handleRevengDumpbin | STUB |
| reveng.compile | `!reveng_compile` | IDM_REVENG_COMPILE | 4303 | handleRevengCompile | STUB |
| reveng.compare | `!reveng_compare` | IDM_REVENG_COMPARE | 4304 | handleRevengCompare | STUB |
| reveng.detect_vulns | `!reveng_detect_vulns` | IDM_REVENG_DETECT_VULNS | 4305 | handleRevengDetectVulns | STUB |
| reveng.export_ida | `!reveng_export_ida` | IDM_REVENG_EXPORT_IDA | 4306 | handleRevengExportIda | STUB |
| reveng.export_ghidra | `!reveng_export_ghidra` | IDM_REVENG_EXPORT_GHIDRA | 4307 | handleRevengExportGhidra | STUB |
| reveng.cfg | `!reveng_cfg` | IDM_REVENG_CFG | 4308 | handleRevengCfg | STUB |
| reveng.functions | `!reveng_functions` | IDM_REVENG_FUNCTIONS | 4309 | handleRevengFunctions | STUB |
| reveng.demangle | `!reveng_demangle` | IDM_REVENG_DEMANGLE | 4310 | handleRevengDemangle | STUB |
| reveng.ssa | `!reveng_ssa` | IDM_REVENG_SSA | 4311 | handleRevengSsa | STUB |
| reveng.recursive_disasm | `!reveng_recursive_disasm` | IDM_REVENG_RECURSIVE_DISASM | 4312 | handleRevengRecursiveDisasm | STUB |
| reveng.type_recovery | `!reveng_type_recovery` | IDM_REVENG_TYPE_RECOVERY | 4313 | handleRevengTypeRecovery | STUB |
| reveng.data_flow | `!reveng_data_flow` | IDM_REVENG_DATA_FLOW | 4314 | handleRevengDataFlow | STUB |
| reveng.license_info | `!reveng_license_info` | IDM_REVENG_LICENSE_INFO | 4315 | handleRevengLicenseInfo | STUB |
| reveng.decompiler_view | `!reveng_decompiler_view` | IDM_REVENG_DECOMPILER_VIEW | 4316 | handleRevengDecompilerView | STUB |
| reveng.decomp_rename | `!reveng_decomp_rename` | IDM_REVENG_DECOMP_RENAME | 4317 | handleRevengDecompRename | STUB |
| reveng.decomp_sync | `!reveng_decomp_sync` | IDM_REVENG_DECOMP_SYNC | 4318 | handleRevengDecompSync | STUB |
| reveng.decomp_close | `!reveng_decomp_close` | IDM_REVENG_DECOMP_CLOSE | 4319 | handleRevengDecompClose | STUB |

### ROUTER (20 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| router.enable | `!router_enable` | IDM_ROUTER_ENABLE | 5048 | handleRouterEnable | REAL |
| router.disable | `!router_disable` | IDM_ROUTER_DISABLE | 5049 | handleRouterDisable | REAL |
| router.show_status | `!router_show_status` | IDM_ROUTER_SHOW_STATUS | 5050 | handleRouterShowStatus | STUB |
| router.show_decision | `!router_show_decision` | IDM_ROUTER_SHOW_DECISION | 5051 | handleRouterShowDecision | STUB |
| router.set_policy | `!router_set_policy` | IDM_ROUTER_SET_POLICY | 5052 | handleRouterSetPolicy | REAL |
| router.show_capabilities | `!router_show_capabilities` | IDM_ROUTER_SHOW_CAPABILITIES | 5053 | handleRouterShowCapabilities | STUB |
| router.show_fallbacks | `!router_show_fallbacks` | IDM_ROUTER_SHOW_FALLBACKS | 5054 | handleRouterShowFallbacks | STUB |
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

### SAFETY (3 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| safety.status | `!safety_status` | IDM_SAFETY_STATUS | 5122 | handleSafetyStatus | REAL |
| safety.reset_budget | `!safety_reset_budget` | IDM_SAFETY_RESET_BUDGET | 5123 | handleSafetyResetBudget | REAL |
| safety.show_violations | `!safety_show_violations` | IDM_SAFETY_SHOW_VIOLATIONS | 5125 | handleSafetyShowViolations | REAL |

### SUBAGENT (5 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| subagent.chain | `!subagent_chain` | IDM_SUBAGENT_CHAIN | 4110 | handleSubagentChain | STUB |
| subagent.swarm | `!subagent_swarm` | IDM_SUBAGENT_SWARM | 4111 | handleSubagentSwarm | STUB |
| subagent.todo_list | `!subagent_todo_list` | IDM_SUBAGENT_TODO_LIST | 4112 | handleSubagentTodoList | STUB |
| subagent.todo_clear | `!subagent_todo_clear` | IDM_SUBAGENT_TODO_CLEAR | 4113 | handleSubagentTodoClear | STUB |
| subagent.status | `!subagent_status` | IDM_SUBAGENT_STATUS | 4114 | handleSubagentStatus | STUB |

### SWARM (25 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| swarm.status | `!swarm_status` | IDM_SWARM_STATUS | 5132 | handleSwarmStatus | REAL |
| swarm.start_leader | `!swarm_start_leader` | IDM_SWARM_START_LEADER | 5133 | handleSwarmStartLeader | STUB |
| swarm.start_worker | `!swarm_start_worker` | IDM_SWARM_START_WORKER | 5134 | handleSwarmStartWorker | STUB |
| swarm.start_hybrid | `!swarm_start_hybrid` | IDM_SWARM_START_HYBRID | 5135 | handleSwarmStartHybrid | STUB |
| swarm.stop | `!swarm_stop` | IDM_SWARM_STOP | 5136 | handleSwarmStop | STUB |
| swarm.list_nodes | `!swarm_list_nodes` | IDM_SWARM_LIST_NODES | 5137 | handleSwarmListNodes | STUB |
| swarm.add_node | `!swarm_add_node` | IDM_SWARM_ADD_NODE | 5138 | handleSwarmAddNode | STUB |
| swarm.remove_node | `!swarm_remove_node` | IDM_SWARM_REMOVE_NODE | 5139 | handleSwarmRemoveNode | STUB |
| swarm.blacklist_node | `!swarm_blacklist_node` | IDM_SWARM_BLACKLIST_NODE | 5140 | handleSwarmBlacklistNode | STUB |
| swarm.build_sources | `!swarm_build_sources` | IDM_SWARM_BUILD_SOURCES | 5141 | handleSwarmBuildSources | STUB |
| swarm.build_cmake | `!swarm_build_cmake` | IDM_SWARM_BUILD_CMAKE | 5142 | handleSwarmBuildCmake | STUB |
| swarm.start_build | `!swarm_start_build` | IDM_SWARM_START_BUILD | 5143 | handleSwarmStartBuild | STUB |
| swarm.cancel_build | `!swarm_cancel_build` | IDM_SWARM_CANCEL_BUILD | 5144 | handleSwarmCancelBuild | STUB |
| swarm.cache_status | `!swarm_cache_status` | IDM_SWARM_CACHE_STATUS | 5145 | handleSwarmCacheStatus | STUB |
| swarm.cache_clear | `!swarm_cache_clear` | IDM_SWARM_CACHE_CLEAR | 5146 | handleSwarmCacheClear | STUB |
| swarm.show_config | `!swarm_show_config` | IDM_SWARM_SHOW_CONFIG | 5147 | handleSwarmShowConfig | STUB |
| swarm.toggle_discovery | `!swarm_toggle_discovery` | IDM_SWARM_TOGGLE_DISCOVERY | 5148 | handleSwarmToggleDiscovery | STUB |
| swarm.show_task_graph | `!swarm_show_task_graph` | IDM_SWARM_SHOW_TASK_GRAPH | 5149 | handleSwarmShowTaskGraph | STUB |
| swarm.show_events | `!swarm_show_events` | IDM_SWARM_SHOW_EVENTS | 5150 | handleSwarmShowEvents | STUB |
| swarm.show_stats | `!swarm_show_stats` | IDM_SWARM_SHOW_STATS | 5151 | handleSwarmShowStats | STUB |
| swarm.reset_stats | `!swarm_reset_stats` | IDM_SWARM_RESET_STATS | 5152 | handleSwarmResetStats | STUB |
| swarm.worker_status | `!swarm_worker_status` | IDM_SWARM_WORKER_STATUS | 5153 | handleSwarmWorkerStatus | STUB |
| swarm.worker_connect | `!swarm_worker_connect` | IDM_SWARM_WORKER_CONNECT | 5154 | handleSwarmWorkerConnect | STUB |
| swarm.worker_disconnect | `!swarm_worker_disconnect` | IDM_SWARM_WORKER_DISCONNECT | 5155 | handleSwarmWorkerDisconnect | STUB |
| swarm.fitness_test | `!swarm_fitness_test` | IDM_SWARM_FITNESS_TEST | 5156 | handleSwarmFitnessTest | STUB |

### TELEMETRY (6 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| telemetry.toggle | `!telemetry_toggle` | IDM_TELEMETRY_TOGGLE | 9900 | handleTelemetryToggle | STUB |
| telemetry.export_json | `!telemetry_export_json` | IDM_TELEMETRY_EXPORT_JSON | 9901 | handleTelemetryExportJson | STUB |
| telemetry.export_csv | `!telemetry_export_csv` | IDM_TELEMETRY_EXPORT_CSV | 9902 | handleTelemetryExportCsv | STUB |
| telemetry.show_dashboard | `!telemetry_show_dashboard` | IDM_TELEMETRY_SHOW_DASHBOARD | 9903 | handleTelemetryShowDashboard | STUB |
| telemetry.clear | `!telemetry_clear` | IDM_TELEMETRY_CLEAR | 9904 | handleTelemetryClear | STUB |
| telemetry.snapshot | `!telemetry_snapshot` | IDM_TELEMETRY_SNAPSHOT | 9905 | handleTelemetrySnapshot | STUB |

### TERMINAL (8 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| terminal.powershell | `!terminal_powershell` | IDM_TERMINAL_POWERSHELL | 3001 | handleTerminalPowershell | STUB |
| terminal.cmd | `!terminal_cmd` | IDM_TERMINAL_CMD | 3002 | handleTerminalCmd | STUB |
| terminal.stop | `!terminal_stop` | IDM_TERMINAL_STOP | 3003 | handleTerminalStop | STUB |
| terminal.clear_all | `!terminal_clear_all` | IDM_TERMINAL_CLEAR_ALL | 3006 | handleTerminalClearAll | STUB |
| terminal.kill | `!terminal_kill` | IDM_TERMINAL_KILL | 4006 | handleTerminalKill | REAL |
| terminal.split_h | `!terminal_split_h` | IDM_TERMINAL_SPLIT_H | 4007 | handleTerminalSplitH | REAL |
| terminal.split_v | `!terminal_split_v` | IDM_TERMINAL_SPLIT_V | 4008 | handleTerminalSplitV | REAL |
| terminal.split_code | `!terminal_split_code` | IDM_TERMINAL_SPLIT_CODE | 4009 | handleTerminalSplitCode | STUB |

### THEME (16 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| theme.dark_plus | `!theme_dark_plus` | IDM_THEME_DARK_PLUS | 3101 | handleThemeDarkPlus | STUB |
| theme.light_plus | `!theme_light_plus` | IDM_THEME_LIGHT_PLUS | 3102 | handleThemeLightPlus | STUB |
| theme.monokai | `!theme_monokai` | IDM_THEME_MONOKAI | 3103 | handleThemeMonokai | STUB |
| theme.dracula | `!theme_dracula` | IDM_THEME_DRACULA | 3104 | handleThemeDracula | STUB |
| theme.nord | `!theme_nord` | IDM_THEME_NORD | 3105 | handleThemeNord | STUB |
| theme.solarized_dark | `!theme_solarized_dark` | IDM_THEME_SOLARIZED_DARK | 3106 | handleThemeSolarizedDark | STUB |
| theme.solarized_light | `!theme_solarized_light` | IDM_THEME_SOLARIZED_LIGHT | 3107 | handleThemeSolarizedLight | STUB |
| theme.cyberpunk_neon | `!theme_cyberpunk_neon` | IDM_THEME_CYBERPUNK_NEON | 3108 | handleThemeCyberpunkNeon | STUB |
| theme.gruvbox_dark | `!theme_gruvbox_dark` | IDM_THEME_GRUVBOX_DARK | 3109 | handleThemeGruvboxDark | STUB |
| theme.catppuccin_mocha | `!theme_catppuccin_mocha` | IDM_THEME_CATPPUCCIN_MOCHA | 3110 | handleThemeCatppuccinMocha | STUB |
| theme.tokyo_night | `!theme_tokyo_night` | IDM_THEME_TOKYO_NIGHT | 3111 | handleThemeTokyoNight | STUB |
| theme.rawrxd_crimson | `!theme_rawrxd_crimson` | IDM_THEME_RAWRXD_CRIMSON | 3112 | handleThemeRawrxdCrimson | STUB |
| theme.high_contrast | `!theme_high_contrast` | IDM_THEME_HIGH_CONTRAST | 3113 | handleThemeHighContrast | STUB |
| theme.one_dark_pro | `!theme_one_dark_pro` | IDM_THEME_ONE_DARK_PRO | 3114 | handleThemeOneDarkPro | STUB |
| theme.synthwave84 | `!theme_synthwave84` | IDM_THEME_SYNTHWAVE84 | 3115 | handleThemeSynthwave84 | STUB |
| theme.abyss | `!theme_abyss` | IDM_THEME_ABYSS | 3116 | handleThemeAbyss | STUB |

### TOOLS (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| tools.command_palette | `!tools_command_palette` | IDM_TOOLS_COMMAND_PALETTE | 501 | handleToolsCommandPalette | STUB |
| tools.settings | `!tools_settings` | IDM_TOOLS_SETTINGS | 502 | handleToolsSettings | STUB |
| tools.extensions | `!tools_extensions` | IDM_TOOLS_EXTENSIONS | 503 | handleToolsExtensions | STUB |
| tools.terminal | `!tools_terminal` | IDM_TOOLS_TERMINAL | 504 | handleToolsTerminal | STUB |
| tools.build | `!tools_build` | IDM_TOOLS_BUILD | 505 | handleToolsBuild | STUB |
| tools.debug | `!tools_debug` | IDM_TOOLS_DEBUG | 506 | handleToolsDebug | STUB |
| tools.profile_start | `!tools_profile_start` | IDM_TOOLS_PROFILE_START | 3010 | handleToolsProfileStart | STUB |
| tools.profile_stop | `!tools_profile_stop` | IDM_TOOLS_PROFILE_STOP | 3011 | handleToolsProfileStop | STUB |
| tools.profile_results | `!tools_profile_results` | IDM_TOOLS_PROFILE_RESULTS | 3012 | handleToolsProfileResults | STUB |
| tools.analyze_script | `!tools_analyze_script` | IDM_TOOLS_ANALYZE_SCRIPT | 3013 | handleToolsAnalyzeScript | STUB |

### TRANSPARENCY (9 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| transparency.100 | `!transparency_100` | IDM_TRANSPARENCY_100 | 3200 | handleTransparency100 | STUB |
| transparency.90 | `!transparency_90` | IDM_TRANSPARENCY_90 | 3201 | handleTransparency90 | STUB |
| transparency.80 | `!transparency_80` | IDM_TRANSPARENCY_80 | 3202 | handleTransparency80 | STUB |
| transparency.70 | `!transparency_70` | IDM_TRANSPARENCY_70 | 3203 | handleTransparency70 | STUB |
| transparency.60 | `!transparency_60` | IDM_TRANSPARENCY_60 | 3204 | handleTransparency60 | STUB |
| transparency.50 | `!transparency_50` | IDM_TRANSPARENCY_50 | 3205 | handleTransparency50 | STUB |
| transparency.40 | `!transparency_40` | IDM_TRANSPARENCY_40 | 3206 | handleTransparency40 | STUB |
| transparency.custom | `!transparency_custom` | IDM_TRANSPARENCY_CUSTOM | 3210 | handleTransparencyCustom | STUB |
| transparency.toggle | `!transparency_toggle` | IDM_TRANSPARENCY_TOGGLE | 3211 | handleTransparencyToggle | STUB |

### VIEW (23 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| view.toggle_sidebar | `!view_toggle_sidebar` | IDM_VIEW_TOGGLE_SIDEBAR | 301 | handleViewToggleSidebar | STUB |
| view.toggle_terminal | `!view_toggle_terminal` | IDM_VIEW_TOGGLE_TERMINAL | 302 | handleViewToggleTerminal | STUB |
| view.toggle_output | `!view_toggle_output` | IDM_VIEW_TOGGLE_OUTPUT | 303 | handleViewToggleOutput | STUB |
| view.toggle_fullscreen | `!view_toggle_fullscreen` | IDM_VIEW_TOGGLE_FULLSCREEN | 304 | handleViewToggleFullscreen | STUB |
| view.zoom_in | `!view_zoom_in` | IDM_VIEW_ZOOM_IN | 305 | handleViewZoomIn | STUB |
| view.zoom_out | `!view_zoom_out` | IDM_VIEW_ZOOM_OUT | 306 | handleViewZoomOut | STUB |
| view.zoom_reset | `!view_zoom_reset` | IDM_VIEW_ZOOM_RESET | 307 | handleViewZoomReset | STUB |
| view.minimap | `!view_minimap` | IDM_VIEW_MINIMAP | 2020 | handleViewMinimap | STUB |
| view.output_tabs | `!view_output_tabs` | IDM_VIEW_OUTPUT_TABS | 2021 | handleViewOutputTabs | STUB |
| view.module_browser | `!view_module_browser` | IDM_VIEW_MODULE_BROWSER | 2022 | handleViewModuleBrowser | STUB |
| view.theme_editor | `!view_theme_editor` | IDM_VIEW_THEME_EDITOR | 2023 | handleViewThemeEditor | STUB |
| view.floating_panel | `!view_floating_panel` | IDM_VIEW_FLOATING_PANEL | 2024 | handleViewFloatingPanel | STUB |
| view.output_panel | `!view_output_panel` | IDM_VIEW_OUTPUT_PANEL | 2025 | handleViewOutputPanel | STUB |
| view.use_streaming_loader | `!view_use_streaming_loader` | IDM_VIEW_USE_STREAMING_LOADER | 2026 | handleViewUseStreamingLoader | STUB |
| view.use_vulkan_renderer | `!view_use_vulkan_renderer` | IDM_VIEW_USE_VULKAN_RENDERER | 2027 | handleViewUseVulkanRenderer | STUB |
| view.sidebar | `!view_sidebar` | IDM_VIEW_SIDEBAR | 2028 | handleViewSidebar | STUB |
| view.terminal | `!view_terminal` | IDM_VIEW_TERMINAL | 2029 | handleViewTerminal | STUB |
| view.toggle_monaco | `!view_toggle_monaco` | IDM_VIEW_TOGGLE_MONACO | 9100 | handleViewToggleMonaco | STUB |
| view.monaco_devtools | `!view_monaco_devtools` | IDM_VIEW_MONACO_DEVTOOLS | 9101 | handleViewMonacoDevtools | STUB |
| view.monaco_reload | `!view_monaco_reload` | IDM_VIEW_MONACO_RELOAD | 9102 | handleViewMonacoReload | STUB |
| view.monaco_zoom_in | `!view_monaco_zoom_in` | IDM_VIEW_MONACO_ZOOM_IN | 9103 | handleViewMonacoZoomIn | STUB |
| view.monaco_zoom_out | `!view_monaco_zoom_out` | IDM_VIEW_MONACO_ZOOM_OUT | 9104 | handleViewMonacoZoomOut | STUB |
| view.monaco_sync_theme | `!view_monaco_sync_theme` | IDM_VIEW_MONACO_SYNC_THEME | 9105 | handleViewMonacoSyncTheme | STUB |

### VOICE (17 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| voice.record | `!voice_record` | IDM_VOICE_RECORD | 9700 | handleVoiceRecord | REAL |
| voice.ptt | `!voice_ptt` | IDM_VOICE_PTT | 9701 | handleVoicePtt | STUB |
| voice.speak | `!voice_speak` | IDM_VOICE_SPEAK | 9702 | handleVoiceSpeak | REAL |
| voice.join_room | `!voice_join_room` | IDM_VOICE_JOIN_ROOM | 9703 | handleVoiceJoinRoom | STUB |
| voice.show_devices | `!voice_show_devices` | IDM_VOICE_SHOW_DEVICES | 9704 | handleVoiceShowDevices | STUB |
| voice.metrics | `!voice_metrics` | IDM_VOICE_METRICS | 9705 | handleVoiceMetrics | REAL |
| voice.toggle_panel | `!voice_toggle_panel` | IDM_VOICE_TOGGLE_PANEL | 9706 | handleVoiceTogglePanel | STUB |
| voice.mode_ptt | `!voice_mode_ptt` | IDM_VOICE_MODE_PTT | 9707 | handleVoiceModePtt | STUB |
| voice.mode_continuous | `!voice_mode_continuous` | IDM_VOICE_MODE_CONTINUOUS | 9708 | handleVoiceModeContinuous | STUB |
| voice.mode_disabled | `!voice_mode_disabled` | IDM_VOICE_MODE_DISABLED | 9709 | handleVoiceModeDisabled | STUB |
| voice.auto_toggle | `!voice_auto_toggle` | IDM_VOICE_AUTO_TOGGLE | 10200 | handleVoiceAutoToggle | STUB |
| voice.auto_settings | `!voice_auto_settings` | IDM_VOICE_AUTO_SETTINGS | 10201 | handleVoiceAutoSettings | STUB |
| voice.auto_next_voice | `!voice_auto_next_voice` | IDM_VOICE_AUTO_NEXT_VOICE | 10202 | handleVoiceAutoNextVoice | STUB |
| voice.auto_prev_voice | `!voice_auto_prev_voice` | IDM_VOICE_AUTO_PREV_VOICE | 10203 | handleVoiceAutoPrevVoice | STUB |
| voice.auto_rate_up | `!voice_auto_rate_up` | IDM_VOICE_AUTO_RATE_UP | 10204 | handleVoiceAutoRateUp | STUB |
| voice.auto_rate_down | `!voice_auto_rate_down` | IDM_VOICE_AUTO_RATE_DOWN | 10205 | handleVoiceAutoRateDown | STUB |
| voice.auto_stop | `!voice_auto_stop` | IDM_VOICE_AUTO_STOP | 10206 | handleVoiceAutoStop | STUB |

### VSCEXT (10 commands)

| Feature ID | CLI Command | IDM_* | Value | Handler | Status |
|---|---|---|---|---|---|
| vscext.api_status | `!vscext_api_status` | IDM_VSCEXT_API_STATUS | 10000 | handleVscextApiStatus | STUB |
| vscext.api_reload | `!vscext_api_reload` | IDM_VSCEXT_API_RELOAD | 10001 | handleVscextApiReload | STUB |
| vscext.api_list_commands | `!vscext_api_list_commands` | IDM_VSCEXT_API_LIST_COMMANDS | 10002 | handleVscextApiListCommands | STUB |
| vscext.api_list_providers | `!vscext_api_list_providers` | IDM_VSCEXT_API_LIST_PROVIDERS | 10003 | handleVscextApiListProviders | STUB |
| vscext.api_diagnostics | `!vscext_api_diagnostics` | IDM_VSCEXT_API_DIAGNOSTICS | 10004 | handleVscextApiDiagnostics | STUB |
| vscext.api_extensions | `!vscext_api_extensions` | IDM_VSCEXT_API_EXTENSIONS | 10005 | handleVscextApiExtensions | STUB |
| vscext.api_stats | `!vscext_api_stats` | IDM_VSCEXT_API_STATS | 10006 | handleVscextApiStats | STUB |
| vscext.api_load_native | `!vscext_api_load_native` | IDM_VSCEXT_API_LOAD_NATIVE | 10007 | handleVscextApiLoadNative | STUB |
| vscext.api_deactivate_all | `!vscext_api_deactivate_all` | IDM_VSCEXT_API_DEACTIVATE_ALL | 10008 | handleVscextApiDeactivateAll | STUB |
| vscext.api_export_config | `!vscext_api_export_config` | IDM_VSCEXT_API_EXPORT_CONFIG | 10009 | handleVscextApiExportConfig | STUB |

## How to Update

1. Add new `IDM_*` defines in `Win32IDE.h` (or any scanned header)
2. Optionally add handler declarations in `feature_handlers.h`
3. Run: `python scripts/auto_register_commands.py`
4. The system automatically generates registrations + stubs
5. Replace stubs with real handlers as you implement features
6. CMake pre-build step does this automatically on every build
