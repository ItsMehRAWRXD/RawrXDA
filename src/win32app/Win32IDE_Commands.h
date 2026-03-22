#pragma once

// =============================================================================
// Win32IDE_Commands.h — All Win32 menu / control / message IDs for Win32IDE
//
// ID allocation map (non-overlapping):
//   1001–1026   Control IDs (IDC_)
//   2030–2031   View: explorer, extensions
//   3060        View: collaboration
//   3100–3117   Theme commands
//   3200–3211   Transparency
//   4006–4010   Terminal (kill/split/clear)
//   4100–4120   Agent loop commands
//   4150–4162   Autonomy + pipeline
//   4200–4242   AI modes, engine 800B/Titan, context window, multi-agent
//   4243–4260   Omega Orchestrator (Phase Ω)
//   4261–4270   Agentic Planning Orchestrator (approval gates)
//   4271–4280   KnowledgeGraphCore (cross-session learning)
//   4281–4299   FailureIntelligence Orchestrator (autonomous recovery)
//   4300–4323   Reverse engineering / decompiler
//   5001–5004   AI mode toggle controls (IDC_)
//   5037–5081   Backend switcher, LLM router, LSP, UX extras
//   5200–5208   Plugin system
//   5300–5321   AI extensions, Monaco/thermal, agent test
//   7001–7057   Plan dialogs, Problems panel
//   9100–9349   Language mode, encoding selectors
//   9550–9554   Security scans
//   10400–10402 Build commands
// =============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// ---- Control IDs (editor, terminal, UI widgets) ----------------------------
#define IDC_EDITOR                  1001
#define IDC_TERMINAL                1002
#define IDC_COMMAND_INPUT           1003
#define IDC_STATUS_BAR              1004
#define IDC_OUTPUT_TABS             1005
#define IDC_MINIMAP                 1006
#define IDC_MODULE_BROWSER          1007
#define IDC_HELP_PANEL              1008
#define IDC_SNIPPET_LIST            1009
#define IDC_CLIPBOARD_HISTORY       1010
#define IDC_OUTPUT_TEXT             1011
#define IDC_OUTPUT_EDIT_GENERAL     1012
#define IDC_OUTPUT_EDIT_ERRORS      1013
#define IDC_OUTPUT_EDIT_DEBUG       1014
#define IDC_OUTPUT_EDIT_FIND        1015
#define IDC_SPLITTER                1016
#define IDC_SEVERITY_FILTER         1017
#define IDC_TITLE_TEXT              1018
#define IDC_BTN_MINIMIZE            1019
#define IDC_BTN_MAXIMIZE            1020
#define IDC_BTN_CLOSE               1021
#define IDC_BTN_GITHUB              1022
#define IDC_BTN_MICROSOFT           1023
#define IDC_BTN_SETTINGS            1024
#define IDC_FILE_EXPLORER           1025
#define IDC_FILE_TREE               1026

// ---- View commands ---------------------------------------------------------
#define IDM_VIEW_FILE_EXPLORER      2030
#define IDM_VIEW_EXTENSIONS         2031
#define IDM_VIEW_COLLABORATION      3060
#define IDM_VIEW_PROBLEMS           7056
#define IDM_VIEW_AGENT_PANEL        7057

// ---- Theme commands (3100–3117) --------------------------------------------
#define IDM_THEME_BASE              3100
#define IDM_THEME_DARK_PLUS         3101
#define IDM_THEME_LIGHT_PLUS        3102
#define IDM_THEME_MONOKAI           3103
#define IDM_THEME_DRACULA           3104
#define IDM_THEME_NORD              3105
#define IDM_THEME_SOLARIZED_DARK    3106
#define IDM_THEME_SOLARIZED_LIGHT   3107
#define IDM_THEME_CYBERPUNK_NEON    3108
#define IDM_THEME_GRUVBOX_DARK      3109
#define IDM_THEME_CATPPUCCIN_MOCHA  3110
#define IDM_THEME_TOKYO_NIGHT       3111
#define IDM_THEME_RAWRXD_CRIMSON    3112
#define IDM_THEME_HIGH_CONTRAST     3113
#define IDM_THEME_ONE_DARK_PRO      3114
#define IDM_THEME_SYNTHWAVE84       3115
#define IDM_THEME_ABYSS             3116
#define IDM_THEME_END               3117

// ---- Transparency commands (3200–3211) -------------------------------------
#define IDM_TRANSPARENCY_100        3200
#define IDM_TRANSPARENCY_90         3201
#define IDM_TRANSPARENCY_80         3202
#define IDM_TRANSPARENCY_70         3203
#define IDM_TRANSPARENCY_60         3204
#define IDM_TRANSPARENCY_50         3205
#define IDM_TRANSPARENCY_40         3206
#define IDM_TRANSPARENCY_CUSTOM     3210
#define IDM_TRANSPARENCY_TOGGLE     3211

// ---- Terminal commands (4006–4010) -----------------------------------------
#define IDM_TERMINAL_KILL           4006
#define IDM_TERMINAL_SPLIT_H        4007
#define IDM_TERMINAL_SPLIT_V        4008
#define IDM_TERMINAL_SPLIT_CODE     4009
#define IDM_TERMINAL_CLEAR          4010

// ---- Agent loop commands (4100–4120) ---------------------------------------
#define IDM_AGENT_START_LOOP        4100
#define IDM_AGENT_EXECUTE_CMD       4101
#define IDM_AGENT_CONFIGURE_MODEL   4102
#define IDM_AGENT_VIEW_TOOLS        4103
#define IDM_AGENT_VIEW_STATUS       4104
#define IDM_AGENT_STOP              4105
#define IDM_AGENT_MEMORY            4106
#define IDM_AGENT_MEMORY_VIEW       4107
#define IDM_AGENT_MEMORY_CLEAR      4108
#define IDM_AGENT_MEMORY_EXPORT     4109

// SubAgent / Chain / Swarm / Todo (4110–4119)
#define IDM_SUBAGENT_CHAIN          4110
#define IDM_SUBAGENT_SWARM          4111
#define IDM_SUBAGENT_TODO_LIST      4112
#define IDM_SUBAGENT_TODO_CLEAR     4113
#define IDM_SUBAGENT_STATUS         4114

#define IDM_AGENT_BOUNDED_LOOP      4120

// ---- Autonomy + pipeline (4150–4162) ---------------------------------------
#define IDM_AUTONOMY_TOGGLE         4150
#define IDM_AUTONOMY_START          4151
#define IDM_AUTONOMY_STOP           4152
#define IDM_AUTONOMY_SET_GOAL       4153
#define IDM_AUTONOMY_STATUS         4154
#define IDM_AUTONOMY_MEMORY         4155

#define IDM_PIPELINE_RUN            4160
#define IDM_PIPELINE_AUTONOMY_START 4161
#define IDM_PIPELINE_AUTONOMY_STOP  4162

// ---- AI mode commands (4200–4203) ------------------------------------------
#define IDM_AI_MODE_MAX             4200
#define IDM_AI_MODE_DEEP_THINK      4201
#define IDM_AI_MODE_DEEP_RESEARCH   4202
#define IDM_AI_MODE_NO_REFUSAL      4203

// ---- Context window / AI context IDs (4210–4219) ---------------------------
#define IDM_AI_CONTEXT_4K           4210
#define IDM_AI_CONTEXT_32K          4211
#define IDM_AI_CONTEXT_64K          4212
#define IDM_AI_CONTEXT_128K         4213
#define IDM_AI_CONTEXT_256K         4214
#define IDM_AI_CONTEXT_512K         4215
#define IDM_AI_CONTEXT_1M           4216
#define IDM_AI_AGENT_CYCLES_SET     4217
#define IDM_AI_AGENT_MULTI_ENABLE   4218
#define IDM_AI_AGENT_MULTI_DISABLE  4219

// ---- Engine / 800B / Titan (4220–4239) — NOTE: owned by the Engine block ---
#define IDM_ENGINE_UNLOCK_800B      4220
#define IDM_ENGINE_LOAD_800B        4221
#define IDM_TITAN_TOGGLE            4230

// ---- AI multi-agent / Titan status (4240–4242) — SEPARATE from Engine IDs --
// FIXED: previously 4220–4222 which collided with IDM_ENGINE_UNLOCK_800B/LOAD.
#define IDM_AI_AGENT_MULTI_STATUS   4240
#define IDM_AI_TITAN_TOGGLE         4241
#define IDM_AI_800B_STATUS          4242

// ---- Omega Orchestrator — Phase Ω commands (4243–4260) ---------------------
// The Last Tool: autonomous software development pipeline
#define IDM_OMEGA_START             4243
#define IDM_OMEGA_STOP              4244
#define IDM_OMEGA_SUBMIT_TASK       4245
#define IDM_OMEGA_RUN_CYCLE         4246
#define IDM_OMEGA_SHOW_STATUS       4247
#define IDM_OMEGA_VIEW_PIPELINE     4248
#define IDM_OMEGA_SPAWN_AGENT       4249
#define IDM_OMEGA_SET_QUALITY_AUTO  4250
#define IDM_OMEGA_SET_QUALITY_BALANCE 4251
#define IDM_OMEGA_SET_QUALITY_MAX   4252
#define IDM_OMEGA_CANCEL_TASK       4253
#define IDM_OMEGA_WORLD_MODEL       4254
#define IDM_OMEGA_EXPORT_STATS      4255
#define IDM_OMEGA_DIAGNOSTICS       4256
#define IDM_OMEGA_VERSION           4257
#define IDM_OMEGA_HELP              4258
#define IDM_OMEGA_ADVANCED_SETTINGS 4259
#define IDM_OMEGA_SHELL_INTEGRATION 4260

// ---- Agentic Planning Orchestrator — Full Approval Gates (4261–4270) ------
// Multi-step planning with risk analysis, auto-approval policies, and human-in-the-loop
#define IDM_PLANNING_START          4261
#define IDM_PLANNING_SHOW_QUEUE     4262
#define IDM_PLANNING_APPROVE_STEP   4263
#define IDM_PLANNING_REJECT_STEP    4264
#define IDM_PLANNING_EXECUTE_STEP   4265
#define IDM_PLANNING_EXECUTE_ALL    4266
#define IDM_PLANNING_ROLLBACK       4267
#define IDM_PLANNING_SET_POLICY     4268
#define IDM_PLANNING_VIEW_STATUS    4269
#define IDM_PLANNING_DIAGNOSTICS    4270

// ---- KnowledgeGraphCore — Cross-session learning + decision archaeology (4271–4280)
// SQLite-backed WHY table, Bayesian preference learning, codebase archaeology
#define IDM_KNOWLEDGE_INIT          4271
#define IDM_KNOWLEDGE_RECORD        4272
#define IDM_KNOWLEDGE_SEARCH        4273
#define IDM_KNOWLEDGE_DECISIONS     4274
#define IDM_KNOWLEDGE_PREFERENCES   4275
#define IDM_KNOWLEDGE_ARCHAEOLOGY   4276
#define IDM_KNOWLEDGE_GRAPH         4277
#define IDM_KNOWLEDGE_EXPORT        4278
#define IDM_KNOWLEDGE_STATS         4279
#define IDM_KNOWLEDGE_FLUSH         4280

// ---- FailureIntelligence Orchestrator — Autonomous recovery & root cause analysis (4281–4299)
// Real-time failure detection, pattern recognition, recovery strategy selection, autonomous healing
#define IDM_FAILURE_DETECT          4281
#define IDM_FAILURE_ANALYZE         4282
#define IDM_FAILURE_SHOW_QUEUE      4283
#define IDM_FAILURE_SHOW_HISTORY    4284
#define IDM_FAILURE_GENERATE_RECOVERY 4285
#define IDM_FAILURE_EXECUTE_RECOVERY 4286
#define IDM_FAILURE_AUTONOMOUS_HEAL 4287
#define IDM_FAILURE_VIEW_PATTERNS   4288
#define IDM_FAILURE_LEARN_PATTERN   4289
#define IDM_FAILURE_STATS           4290
#define IDM_FAILURE_SET_POLICY      4291
#define IDM_FAILURE_SHOW_HEALTH     4292
#define IDM_FAILURE_EXPORT_ANALYSIS 4293
#define IDM_FAILURE_CLEAR_HISTORY   4294
#define IDM_FAILURE_DIAGNOSTICS     4295

// ---- Change Impact Analyzer — Pre-commit ripple effect prediction (4350–4370)
// Dependency graph traversal, risk scoring, commit blocking, approval gate integration
#define IDM_IMPACT_ANALYZE_STAGED       4350
#define IDM_IMPACT_ANALYZE_UNSTAGED     4351
#define IDM_IMPACT_ANALYZE_FILE         4352
#define IDM_IMPACT_SHOW_REPORT          4353
#define IDM_IMPACT_SHOW_ZONES           4354
#define IDM_IMPACT_RISK_SCORE           4355
#define IDM_IMPACT_CHECK_COMMIT         4356
#define IDM_IMPACT_SET_CONFIG           4357
#define IDM_IMPACT_HISTORY              4358
#define IDM_IMPACT_DIAGNOSTICS          4359
#define IDM_IMPACT_EXPORT_JSON          4360

// ---- Reverse engineering commands (4300–4323) ------------------------------
#define IDM_REVENG_ANALYZE              4300
#define IDM_REVENG_DISASM               4301
#define IDM_REVENG_DUMPBIN              4302
#define IDM_REVENG_COMPILE              4303
#define IDM_REVENG_COMPARE              4304
#define IDM_REVENG_DETECT_VULNS         4305
#define IDM_REVENG_EXPORT_IDA           4306
#define IDM_REVENG_EXPORT_GHIDRA        4307
#define IDM_REVENG_CFG                  4308
#define IDM_REVENG_FUNCTIONS            4309
#define IDM_REVENG_DEMANGLE             4310
#define IDM_REVENG_SSA                  4311
#define IDM_REVENG_RECURSIVE_DISASM     4312
#define IDM_REVENG_TYPE_RECOVERY        4313
#define IDM_REVENG_DATA_FLOW            4314
#define IDM_REVENG_LICENSE_INFO         4315
#define IDM_REVENG_DECOMPILER_VIEW      4316
#define IDM_REVENG_DECOMP_RENAME        4317
#define IDM_REVENG_DECOMP_SYNC          4318
#define IDM_REVENG_DECOMP_CLOSE         4319
#define IDM_REVENG_SET_BINARY_FROM_ACTIVE           4320
#define IDM_REVENG_SET_BINARY_FROM_DEBUG_TARGET     4321
#define IDM_REVENG_SET_BINARY_FROM_BUILD_OUTPUT     4322
#define IDM_REVENG_DISASM_AT_RIP                    4323

// ---- AI mode toggle controls (IDC_, 5001–5004) -----------------------------
#define IDC_AI_MAX_MODE             5001
#define IDC_AI_DEEP_THINK           5002
#define IDC_AI_DEEP_RESEARCH        5003
#define IDC_AI_NO_REFUSAL           5004

// ---- AI Backend Switcher (5037–5047) ----------------------------------------
#define IDM_BACKEND_SWITCH_LOCAL    5037
#define IDM_BACKEND_SWITCH_OLLAMA   5038
#define IDM_BACKEND_SWITCH_OPENAI   5039
#define IDM_BACKEND_SWITCH_CLAUDE   5040
#define IDM_BACKEND_SWITCH_GEMINI   5041
#define IDM_BACKEND_SHOW_STATUS     5042
#define IDM_BACKEND_SHOW_SWITCHER   5043
#define IDM_BACKEND_CONFIGURE       5044
#define IDM_BACKEND_HEALTH_CHECK    5045
#define IDM_BACKEND_SET_API_KEY     5046
#define IDM_BACKEND_SAVE_CONFIGS    5047

// ---- LLM Router (5048–5057) ------------------------------------------------
#define IDM_ROUTER_ENABLE           5048
#define IDM_ROUTER_DISABLE          5049
#define IDM_ROUTER_SHOW_STATUS      5050
#define IDM_ROUTER_SHOW_DECISION    5051
#define IDM_ROUTER_SET_POLICY       5052
#define IDM_ROUTER_SHOW_CAPABILITIES 5053
#define IDM_ROUTER_SHOW_FALLBACKS   5054
#define IDM_ROUTER_SAVE_CONFIG      5055
#define IDM_ROUTER_ROUTE_PROMPT     5056
#define IDM_ROUTER_RESET_STATS      5057

// ---- LSP Client commands (5058–5070) ----------------------------------------
#define IDM_LSP_START_ALL           5058
#define IDM_LSP_STOP_ALL            5059
#define IDM_LSP_SHOW_STATUS         5060
#define IDM_LSP_GOTO_DEFINITION     5061
#define IDM_LSP_FIND_REFERENCES     5062
#define IDM_LSP_RENAME_SYMBOL       5063
#define IDM_LSP_HOVER_INFO          5064
#define IDM_LSP_SHOW_DIAGNOSTICS    5065
#define IDM_LSP_RESTART_SERVER      5066
#define IDM_LSP_CLEAR_DIAGNOSTICS   5067
#define IDM_LSP_SHOW_SYMBOL_INFO    5068
#define IDM_LSP_CONFIGURE           5069
#define IDM_LSP_SAVE_CONFIG         5070

// ---- Peek Overlay commands (5071–5074) -------------------------------------
#define IDM_PEEK_DEFINITION         5071
#define IDM_PEEK_REFERENCES         5072
#define IDM_PEEK_CLOSE              5073
#define IDM_PEEK_NAVIGATE           5074

// ---- UX / Research Track extras (5075–5085) --------------------------------
#define IDM_ROUTER_WHY_BACKEND      5071
#define IDM_ROUTER_PIN_TASK         5072
#define IDM_ROUTER_UNPIN_TASK       5073
#define IDM_ROUTER_SHOW_PINS        5074
#define IDM_ROUTER_SHOW_HEATMAP     5075
#define IDM_ROUTER_ENSEMBLE_ENABLE  5076
#define IDM_ROUTER_ENSEMBLE_DISABLE 5077
#define IDM_ROUTER_ENSEMBLE_STATUS  5078
#define IDM_ROUTER_SIMULATE         5079
#define IDM_ROUTER_SIMULATE_LAST    5080
#define IDM_ROUTER_SHOW_COST_STATS  5081

// ---- Plugin system (5200–5208) ---------------------------------------------
#define IDM_PLUGIN_SHOW_PANEL       5200
#define IDM_PLUGIN_LOAD             5201
#define IDM_PLUGIN_UNLOAD           5202
#define IDM_PLUGIN_UNLOAD_ALL       5203
#define IDM_PLUGIN_REFRESH          5204
#define IDM_PLUGIN_SCAN_DIR         5205
#define IDM_PLUGIN_SHOW_STATUS      5206
#define IDM_PLUGIN_TOGGLE_HOTLOAD   5207
#define IDM_PLUGIN_CONFIGURE        5208

// ---- AI Extensions (5300–5321) ---------------------------------------------
#define IDM_AI_MODEL_REGISTRY       5300
#define IDM_AI_CHECKPOINT_MGR       5301
#define IDM_AI_INTERPRET_PANEL      5302
#define IDM_AI_CICD_SETTINGS        5303
#define IDM_AI_MULTI_FILE_SEARCH    5304
#define IDM_AI_BENCHMARK_MENU       5305
#define IDM_VIEW_MONACO_SETTINGS    5310
#define IDM_VIEW_THERMAL_DASHBOARD  5311
#define IDM_AGENT_SMOKE_TEST        5320
#define IDM_AGENT_SET_CYCLE_AGENT_COUNTER 5321

// ---- Plan Approval Dialog controls (7001–7032) -----------------------------
#define IDC_PLAN_LIST               7001
#define IDC_PLAN_DETAIL             7002
#define IDC_PLAN_GOAL_LABEL         7003
#define IDC_PLAN_SUMMARY_LABEL      7004
#define IDC_PLAN_BTN_APPROVE        7010
#define IDC_PLAN_BTN_APPROVE_SAFE   7015
#define IDC_PLAN_BTN_EDIT           7011
#define IDC_PLAN_BTN_REJECT         7012
#define IDC_PLAN_BTN_PAUSE          7013
#define IDC_PLAN_BTN_CANCEL         7014
#define IDC_PLAN_PROGRESS           7020
#define IDC_PLAN_PROGRESS_LABEL     7021
#define IDC_PLAN_BTN_RETRY_YES      7030
#define IDC_PLAN_BTN_RETRY_NO       7031
#define IDC_PLAN_RETRY_LABEL        7032

// ---- Unified Problems Panel (7050–7056) ------------------------------------
#define IDC_PROBLEMS_PANEL          7050
#define IDC_PROBLEMS_LISTVIEW       7051
#define IDC_PROBLEMS_FILTER         7052
#define IDC_PROBLEMS_CLEAR          7053
#define IDC_PROBLEMS_SHOW_ERRORS    7054
#define IDC_PROBLEMS_SHOW_WARNINGS  7055

// ---- Language mode quick switch (9100–9199) --------------------------------
#define IDM_LANGMODE_FIRST          9100
#define IDM_LANGMODE_LAST           9199

// ---- Encoding selectors (9200–9349) ----------------------------------------
#define IDM_ENCODING_REOPEN_FIRST   9200
#define IDM_ENCODING_REOPEN_LAST    9249
#define IDM_ENCODING_SAVE_FIRST     9300
#define IDM_ENCODING_SAVE_LAST      9349

// ---- Security scan commands (9550–9554) ------------------------------------
#define IDM_SECURITY_SCAN_SECRETS       9550
#define IDM_SECURITY_SCAN_SAST          9551
#define IDM_SECURITY_SCAN_DEPENDENCIES  9552
#define IDM_SECURITY_DASHBOARD          9553
#define IDM_SECURITY_EXPORT_SBOM        9554

// ---- Build commands (10400–10402) ------------------------------------------
// routed via handleBuildCommand() for the 10400–10499 range
#define IDM_BUILD_SOLUTION          10400
#define IDM_BUILD_CLEAN             10401
#define IDM_BUILD_REBUILD           10402

// ---- Custom WM_APP window messages -----------------------------------------
#define WM_FILE_CHANGED_EXTERNAL        (WM_APP + 200)
#define WM_GHOST_TEXT_READY             (WM_APP + 400)
#define WM_PLAN_READY                   (WM_APP + 500)
#define WM_PLAN_STEP_DONE               (WM_APP + 501)
#define WM_PLAN_COMPLETE                (WM_APP + 502)
#define WM_AGENT_HISTORY_REPLAY_DONE    (WM_APP + 600)

// ---- Utility macro ---------------------------------------------------------
#ifndef LOG_FUNCTION
#define LOG_FUNCTION() LOG_DEBUG(std::string("ENTER ") + __FUNCTION__)
#endif
