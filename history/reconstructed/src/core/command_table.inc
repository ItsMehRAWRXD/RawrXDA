// ============================================================================
// command_table.inc — SINGLE SOURCE OF TRUTH X-Macro Command Table
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// THIS IS THE CANONICAL COMMAND TABLE. ALL 324 commands defined once.
// Generated outputs:
//   - Win32 IDM_* enum values
//   - CLI alias dispatch
//   - GUI command routing
//   - Command palette population
//   - Help text auto-generation
//   - Compile-time coverage enforcement
//
// Format: X(IDM_VALUE, SYMBOL, canonicalName, cliAlias, exposure, category, handler, shortcut, description)
//
// Exposure: BOTH = GUI + CLI, GUI = GUI-only, CLI = CLI-only, INTERNAL = hidden
// Category: FILE, EDIT, VIEW, TERMINAL, AGENT, AUTONOMY, SUBAGENT, DEBUG,
//           HOTPATCH, REVENG, AIMODE, ROUTER, LSP, LSP_SERVER, BACKEND,
//           HYBRID, MULTI_RESP, GOV, SAFETY, REPLAY, CONFIDENCE, SWARM,
//           PLUGIN, ASM, MONACO, EDITOR_ENGINE, PDB, TELEMETRY, AUDIT,
//           GAUNTLET, VOICE, QW, VSCEXT, FLIGHT_REC, VOICE_AUTO, DISK_RECOVERY,
//           THEME, TRANSPARENCY, SERVER, SETTINGS, HELP, TOOLS, MANIFEST
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#ifndef COMMAND_TABLE
#error "Define COMMAND_TABLE(X) before including command_table.inc"
#endif

// ============================================================================
// FILE OPERATIONS (1001-1099)
// ============================================================================
COMMAND_TABLE(1001, FILE_NEW,           "file.new",             "!new",                 BOTH, FILE,           handleFileNew,              "Ctrl+N",       "Create a new empty file")
COMMAND_TABLE(1002, FILE_OPEN,          "file.open",            "!open",                BOTH, FILE,           handleFileOpen,             "Ctrl+O",       "Open an existing file with dialog")
COMMAND_TABLE(1003, FILE_SAVE,          "file.save",            "!save",                BOTH, FILE,           handleFileSave,             "Ctrl+S",       "Save current file to disk")
COMMAND_TABLE(1004, FILE_SAVEAS,        "file.saveAs",          "!save_as",             BOTH, FILE,           handleFileSaveAs,           "Ctrl+Shift+S", "Save current file with new name")
COMMAND_TABLE(1005, FILE_SAVEALL,       "file.saveAll",         "!save_all",            BOTH, FILE,           handleFileSaveAll,          "",             "Save all modified files")
COMMAND_TABLE(1006, FILE_CLOSE,         "file.close",           "!close",               BOTH, FILE,           handleFileClose,            "Ctrl+W",       "Close current file")
COMMAND_TABLE(1010, FILE_RECENT,        "file.recentFiles",     "!recent",              BOTH, FILE,           handleFileRecentFiles,      "",             "Open from recent files")
COMMAND_TABLE(1020, FILE_RECENT_CLEAR,  "file.recentClear",     "!recent_clear",        BOTH, FILE,           handleFileRecentClear,      "",             "Clear recent files list")
COMMAND_TABLE(1030, FILE_LOAD_MODEL,    "file.loadModel",       "!model_load",          BOTH, FILE,           handleFileLoadModel,        "",             "Load a GGUF model file")
COMMAND_TABLE(1031, FILE_MODEL_HF,      "file.modelFromHF",     "!model_hf",            BOTH, FILE,           handleFileModelFromHF,      "",             "Download model from HuggingFace")
COMMAND_TABLE(1032, FILE_MODEL_OLLAMA,  "file.modelFromOllama", "!model_ollama",        BOTH, FILE,           handleFileModelFromOllama,  "",             "Load from Ollama")
COMMAND_TABLE(1033, FILE_MODEL_URL,     "file.modelFromURL",    "!model_url",           BOTH, FILE,           handleFileModelFromURL,     "",             "Download from URL")
COMMAND_TABLE(1034, FILE_MODEL_UNIFIED, "file.modelUnified",    "!model_unified",       BOTH, FILE,           handleFileUnifiedLoad,      "",             "Universal loader")
COMMAND_TABLE(1035, FILE_QUICK_LOAD,    "file.quickLoad",       "!quick_load",          BOTH, FILE,           handleFileQuickLoad,        "",             "One-click local cache load")
COMMAND_TABLE(1099, FILE_EXIT,          "file.exit",            "!exit",                BOTH, FILE,           handleFileExit,             "Alt+F4",       "Exit application")

// ============================================================================
// EDITING (2001-2099)
// ============================================================================
COMMAND_TABLE(2001, EDIT_UNDO,          "edit.undo",            "!undo",                BOTH, EDIT,           handleEditUndo,             "Ctrl+Z",       "Undo last action")
COMMAND_TABLE(2002, EDIT_REDO,          "edit.redo",            "!redo",                BOTH, EDIT,           handleEditRedo,             "Ctrl+Y",       "Redo last undone action")
COMMAND_TABLE(2003, EDIT_CUT,           "edit.cut",             "!cut",                 BOTH, EDIT,           handleEditCut,              "Ctrl+X",       "Cut selection to clipboard")
COMMAND_TABLE(2004, EDIT_COPY,          "edit.copy",            "!copy",                BOTH, EDIT,           handleEditCopy,             "Ctrl+C",       "Copy selection to clipboard")
COMMAND_TABLE(2005, EDIT_PASTE,         "edit.paste",           "!paste",               BOTH, EDIT,           handleEditPaste,            "Ctrl+V",       "Paste from clipboard")
COMMAND_TABLE(2006, EDIT_SELECT_ALL,    "edit.selectAll",       "!select_all",          BOTH, EDIT,           handleEditSelectAll,        "Ctrl+A",       "Select entire buffer")
COMMAND_TABLE(2007, EDIT_FIND,          "edit.find",            "!find",                BOTH, EDIT,           handleEditFind,             "Ctrl+F",       "Search for text")
COMMAND_TABLE(2008, EDIT_REPLACE,       "edit.replace",         "!replace",             BOTH, EDIT,           handleEditReplace,          "Ctrl+H",       "Find and replace text")

// ============================================================================
// VIEW / THEMES (3100-3117)
// ============================================================================
COMMAND_TABLE(3100, THEME_BASE,             "theme.menu",           "!theme_menu",       GUI,  THEME,          handleThemeMenu,            "",             "Theme menu base")
COMMAND_TABLE(3101, THEME_DARK_PLUS,        "theme.darkPlus",       "!theme dark_plus",  BOTH, THEME,          handleThemeDarkPlus,        "",             "Dark+ theme")
COMMAND_TABLE(3102, THEME_LIGHT_PLUS,       "theme.lightPlus",      "!theme light_plus", BOTH, THEME,          handleThemeLightPlus,       "",             "Light+ theme")
COMMAND_TABLE(3103, THEME_MONOKAI,          "theme.monokai",        "!theme monokai",    BOTH, THEME,          handleThemeMonokai,         "",             "Monokai theme")
COMMAND_TABLE(3104, THEME_DRACULA,          "theme.dracula",        "!theme dracula",    BOTH, THEME,          handleThemeDracula,         "",             "Dracula theme")
COMMAND_TABLE(3105, THEME_NORD,             "theme.nord",           "!theme nord",       BOTH, THEME,          handleThemeNord,            "",             "Nord theme")
COMMAND_TABLE(3106, THEME_SOLARIZED_DARK,   "theme.solarizedDark",  "!theme sol_dark",   BOTH, THEME,          handleThemeSolarizedDark,   "",             "Solarized Dark theme")
COMMAND_TABLE(3107, THEME_SOLARIZED_LIGHT,  "theme.solarizedLight", "!theme sol_light",  BOTH, THEME,          handleThemeSolarizedLight,  "",             "Solarized Light theme")
COMMAND_TABLE(3108, THEME_CYBERPUNK_NEON,   "theme.cyberpunkNeon",  "!theme cyberpunk",  BOTH, THEME,          handleThemeCyberpunkNeon,   "",             "Cyberpunk Neon theme")
COMMAND_TABLE(3109, THEME_GRUVBOX_DARK,     "theme.gruvboxDark",    "!theme gruvbox",    BOTH, THEME,          handleThemeGruvboxDark,     "",             "Gruvbox Dark theme")
COMMAND_TABLE(3110, THEME_CATPPUCCIN_MOCHA, "theme.catppuccin",     "!theme catppuccin", BOTH, THEME,          handleThemeCatppuccinMocha, "",             "Catppuccin Mocha theme")
COMMAND_TABLE(3111, THEME_TOKYO_NIGHT,      "theme.tokyoNight",     "!theme tokyo",      BOTH, THEME,          handleThemeTokyoNight,      "",             "Tokyo Night theme")
COMMAND_TABLE(3112, THEME_RAWRXD_CRIMSON,   "theme.rawrxdCrimson",  "!theme crimson",    BOTH, THEME,          handleThemeRawrxdCrimson,   "",             "RawrXD Crimson theme")
COMMAND_TABLE(3113, THEME_HIGH_CONTRAST,    "theme.highContrast",   "!theme hc",         BOTH, THEME,          handleThemeHighContrast,    "",             "High Contrast theme")
COMMAND_TABLE(3114, THEME_ONE_DARK_PRO,     "theme.oneDarkPro",     "!theme onedark",    BOTH, THEME,          handleThemeOneDarkPro,      "",             "One Dark Pro theme")
COMMAND_TABLE(3115, THEME_SYNTHWAVE84,      "theme.synthwave84",    "!theme synthwave",  BOTH, THEME,          handleThemeSynthwave84,     "",             "Synthwave 84 theme")
COMMAND_TABLE(3116, THEME_ABYSS,            "theme.abyss",          "!theme abyss",      BOTH, THEME,          handleThemeAbyss,           "",             "Abyss theme")
COMMAND_TABLE(3117, THEME_END,              "theme.end",            "",                  INTERNAL, THEME,       handleThemeEnd,             "",             "Theme sentinel")

// ============================================================================
// VIEW / GIT (3020-3024) — routed via handleViewCommand
// ============================================================================
COMMAND_TABLE(3020, GIT_STATUS,         "git.status",           "!git_status",          BOTH, GIT,            handleGitStatus,            "",             "Show repository status")
COMMAND_TABLE(3021, GIT_COMMIT,         "git.commit",           "!git_commit",          BOTH, GIT,            handleGitCommit,            "",             "Commit changes")
COMMAND_TABLE(3022, GIT_PUSH,           "git.push",             "!git_push",            BOTH, GIT,            handleGitPush,              "",             "Push to remote")
COMMAND_TABLE(3023, GIT_PULL,           "git.pull",             "!git_pull",            BOTH, GIT,            handleGitPull,              "",             "Pull from remote")
COMMAND_TABLE(3024, GIT_PANEL,          "git.diff",             "!git_diff",            BOTH, GIT,            handleGitDiff,              "",             "Show diff panel")

// ============================================================================
// VIEW / TRANSPARENCY (3200-3211)
// ============================================================================
COMMAND_TABLE(3200, TRANSPARENCY_100,       "view.transparency100", "!transparency 100", BOTH, TRANSPARENCY,   handleTransparency100,      "",             "Full opacity")
COMMAND_TABLE(3201, TRANSPARENCY_90,        "view.transparency90",  "!transparency 90",  BOTH, TRANSPARENCY,   handleTransparency90,       "",             "90% opacity")
COMMAND_TABLE(3202, TRANSPARENCY_80,        "view.transparency80",  "!transparency 80",  BOTH, TRANSPARENCY,   handleTransparency80,       "",             "80% opacity")
COMMAND_TABLE(3203, TRANSPARENCY_70,        "view.transparency70",  "!transparency 70",  BOTH, TRANSPARENCY,   handleTransparency70,       "",             "70% opacity")
COMMAND_TABLE(3204, TRANSPARENCY_60,        "view.transparency60",  "!transparency 60",  BOTH, TRANSPARENCY,   handleTransparency60,       "",             "60% opacity")
COMMAND_TABLE(3205, TRANSPARENCY_50,        "view.transparency50",  "!transparency 50",  BOTH, TRANSPARENCY,   handleTransparency50,       "",             "50% opacity")
COMMAND_TABLE(3206, TRANSPARENCY_40,        "view.transparency40",  "!transparency 40",  BOTH, TRANSPARENCY,   handleTransparency40,       "",             "40% opacity")
COMMAND_TABLE(3210, TRANSPARENCY_CUSTOM,    "view.transparencyCustom","!transparency custom",BOTH, TRANSPARENCY, handleTransparencyCustom,  "",             "Set custom transparency")
COMMAND_TABLE(3211, TRANSPARENCY_TOGGLE,    "view.transparencyToggle","!transparency toggle",BOTH, TRANSPARENCY, handleTransparencyToggle,  "",             "Toggle transparency")

// ============================================================================
// TERMINAL (4001-4009)
// ============================================================================
COMMAND_TABLE(4001, TERMINAL_NEW,       "terminal.new",         "!terminal_new",        BOTH, TERMINAL,       handleTerminalNew,          "Ctrl+`",       "Create new terminal pane")
COMMAND_TABLE(4006, TERMINAL_KILL,      "terminal.kill",        "!terminal_kill",       BOTH, TERMINAL,       handleTerminalKill,         "",             "Close terminal")
COMMAND_TABLE(4007, TERMINAL_SPLIT_H,   "terminal.splitH",      "!terminal_split",      BOTH, TERMINAL,       handleTerminalSplitH,       "",             "Split terminal horizontally")
COMMAND_TABLE(4008, TERMINAL_SPLIT_V,   "terminal.splitV",      "!terminal_split_v",    BOTH, TERMINAL,       handleTerminalSplitV,       "",             "Split terminal vertically")
COMMAND_TABLE(4009, TERMINAL_SPLIT_CODE,"terminal.splitCode",   "!terminal_split_code", BOTH, TERMINAL,       handleTerminalSplitCode,    "",             "Split with code panel")

// ============================================================================
// AGENT (4100-4120)
// ============================================================================
COMMAND_TABLE(4100, AGENT_START_LOOP,       "agent.loop",           "!agent_loop",          BOTH, AGENT,      handleAgentLoop,            "",             "Start agent loop")
COMMAND_TABLE(4101, AGENT_EXECUTE_CMD,      "agent.execute",        "!agent_execute",       BOTH, AGENT,      handleAgentExecute,         "",             "Execute agent instruction")
COMMAND_TABLE(4102, AGENT_CONFIGURE_MODEL,  "agent.configure",      "!agent_config",        BOTH, AGENT,      handleAgentConfigure,       "",             "Agent configuration")
COMMAND_TABLE(4103, AGENT_VIEW_TOOLS,       "agent.viewTools",      "!tools",               BOTH, AGENT,      handleAgentViewTools,       "",             "List available tools")
COMMAND_TABLE(4104, AGENT_VIEW_STATUS,      "agent.viewStatus",     "!agent_status",        BOTH, AGENT,      handleAgentViewStatus,      "",             "Show agent status")
COMMAND_TABLE(4105, AGENT_STOP,             "agent.stop",           "!agent_stop",          BOTH, AGENT,      handleAgentStop,            "",             "Stop current agent")
COMMAND_TABLE(4106, AGENT_MEMORY,           "agent.memory",         "!agent_memory",        BOTH, AGENT,      handleAgentMemory,          "",             "Agent memory system")
COMMAND_TABLE(4107, AGENT_MEMORY_VIEW,      "agent.memoryView",     "!agent_memory_view",   BOTH, AGENT,      handleAgentMemoryView,      "",             "Display memory contents")
COMMAND_TABLE(4108, AGENT_MEMORY_CLEAR,     "agent.memoryClear",    "!agent_memory_clear",  BOTH, AGENT,      handleAgentMemoryClear,     "",             "Reset agent memory")
COMMAND_TABLE(4109, AGENT_MEMORY_EXPORT,    "agent.memoryExport",   "!agent_memory_export", BOTH, AGENT,      handleAgentMemoryExport,    "",             "Save memory to file")
COMMAND_TABLE(4120, AGENT_BOUNDED_LOOP,     "agent.boundedLoop",    "!agent_bounded",       BOTH, AGENT,      handleAgentBoundedLoop,     "",             "Agent loop with max iters")

// ============================================================================
// SUBAGENT (4110-4114)
// ============================================================================
COMMAND_TABLE(4110, SUBAGENT_CHAIN,     "subagent.chain",       "!chain",               BOTH, SUBAGENT,       handleSubAgentChain,        "",             "Execute chain of sub-agents")
COMMAND_TABLE(4111, SUBAGENT_SWARM,     "subagent.swarm",       "!swarm",               BOTH, SUBAGENT,       handleSubAgentSwarm,        "",             "Launch sub-agent swarm")
COMMAND_TABLE(4112, SUBAGENT_TODO_LIST, "subagent.todoList",    "!todo",                BOTH, SUBAGENT,       handleSubAgentTodoList,     "",             "View todo items")
COMMAND_TABLE(4113, SUBAGENT_TODO_CLEAR,"subagent.todoClear",   "!todo_clear",          BOTH, SUBAGENT,       handleSubAgentTodoClear,    "",             "Clear all todo items")
COMMAND_TABLE(4114, SUBAGENT_STATUS,    "subagent.status",      "!agents",              BOTH, SUBAGENT,       handleSubAgentStatus,       "",             "Sub-agent system status")

// ============================================================================
// AUTONOMY (4150-4155)
// ============================================================================
COMMAND_TABLE(4150, AUTONOMY_TOGGLE,    "autonomy.toggle",      "!autonomy_toggle",     BOTH, AUTONOMY,       handleAutonomyToggle,       "",             "Toggle autonomy on/off")
COMMAND_TABLE(4151, AUTONOMY_START,     "autonomy.start",       "!autonomy_start",      BOTH, AUTONOMY,       handleAutonomyStart,        "",             "Activate autonomous loop")
COMMAND_TABLE(4152, AUTONOMY_STOP,      "autonomy.stop",        "!autonomy_stop",       BOTH, AUTONOMY,       handleAutonomyStop,         "",             "Deactivate autonomous loop")
COMMAND_TABLE(4153, AUTONOMY_SET_GOAL,  "autonomy.goal",        "!autonomy_goal",       BOTH, AUTONOMY,       handleAutonomyGoal,         "",             "Set autonomy target")
COMMAND_TABLE(4154, AUTONOMY_STATUS,    "autonomy.status",      "!autonomy_status",     BOTH, AUTONOMY,       handleAutonomyStatus,       "",             "Show autonomy status")
COMMAND_TABLE(4155, AUTONOMY_MEMORY,    "autonomy.memory",      "!autonomy_memory",     BOTH, AUTONOMY,       handleAutonomyMemory,       "",             "View autonomy memory")

// ============================================================================
// AI MODE (4200-4216)
// ============================================================================
COMMAND_TABLE(4200, AI_MODE_MAX,        "ai.maxMode",           "!max",                 BOTH, AIMODE,         handleAIMaxMode,            "",             "All systems max power")
COMMAND_TABLE(4201, AI_MODE_DEEP_THINK, "ai.deepThinking",      "!deep",                BOTH, AIMODE,         handleAIDeepThinking,       "",             "Enable deep thinking mode")
COMMAND_TABLE(4202, AI_MODE_DEEP_RESEARCH,"ai.deepResearch",    "!research",            BOTH, AIMODE,         handleAIDeepResearch,       "",             "Enable research mode")
COMMAND_TABLE(4203, AI_MODE_NO_REFUSAL, "ai.noRefusal",         "!no_refusal",          BOTH, AIMODE,         handleAINoRefusal,          "",             "Toggle no-refusal mode")
COMMAND_TABLE(4210, AI_CONTEXT_4K,      "ai.context4K",         "!ctx 4k",             BOTH, AIMODE,         handleAIContext4K,          "",             "Set 4K context window")
COMMAND_TABLE(4211, AI_CONTEXT_32K,     "ai.context32K",        "!ctx 32k",            BOTH, AIMODE,         handleAIContext32K,         "",             "Set 32K context window")
COMMAND_TABLE(4212, AI_CONTEXT_64K,     "ai.context64K",        "!ctx 64k",            BOTH, AIMODE,         handleAIContext64K,         "",             "Set 64K context window")
COMMAND_TABLE(4213, AI_CONTEXT_128K,    "ai.context128K",       "!ctx 128k",           BOTH, AIMODE,         handleAIContext128K,        "",             "Set 128K context window")
COMMAND_TABLE(4214, AI_CONTEXT_256K,    "ai.context256K",       "!ctx 256k",           BOTH, AIMODE,         handleAIContext256K,        "",             "Set 256K context window")
COMMAND_TABLE(4215, AI_CONTEXT_512K,    "ai.context512K",       "!ctx 512k",           BOTH, AIMODE,         handleAIContext512K,        "",             "Set 512K context window")
COMMAND_TABLE(4216, AI_CONTEXT_1M,      "ai.context1M",         "!ctx 1m",             BOTH, AIMODE,         handleAIContext1M,          "",             "Set 1M context window")

// ============================================================================
// REVERSE ENGINEERING (4300-4319)
// ============================================================================
COMMAND_TABLE(4300, REVENG_ANALYZE,         "re.analyze",           "!re_analyze",          BOTH, REVENG,     handleREDecisionTree,       "",             "Agentic decision tree analysis")
COMMAND_TABLE(4301, REVENG_DISASM,          "re.disasm",            "!disasm",              BOTH, REVENG,     handleREDisassemble,        "",             "Disassemble binary")
COMMAND_TABLE(4302, REVENG_DUMPBIN,         "re.dumpbin",           "!dumpbin",             BOTH, REVENG,     handleREDumpbin,            "",             "PE analysis")
COMMAND_TABLE(4303, REVENG_COMPILE,         "re.compile",           "!re_compile",          BOTH, REVENG,     handleRECompile,            "",             "Compile decompiled source")
COMMAND_TABLE(4304, REVENG_COMPARE,         "re.compare",           "!re_compare",          BOTH, REVENG,     handleRECompare,            "",             "Binary comparison")
COMMAND_TABLE(4305, REVENG_DETECT_VULNS,    "re.detectVulns",       "!re_vulns",            BOTH, REVENG,     handleREDetectVulns,        "",             "Vulnerability detection")
COMMAND_TABLE(4306, REVENG_EXPORT_IDA,      "re.exportIDA",         "!re_ida",              BOTH, REVENG,     handleREExportIDA,          "",             "Export to IDA format")
COMMAND_TABLE(4307, REVENG_EXPORT_GHIDRA,   "re.exportGhidra",      "!re_ghidra",           BOTH, REVENG,     handleREExportGhidra,       "",             "Export to Ghidra format")
COMMAND_TABLE(4308, REVENG_CFG,             "re.cfg",               "!cfg",                 BOTH, REVENG,     handleRECFGAnalysis,        "",             "Control flow graph")
COMMAND_TABLE(4309, REVENG_FUNCTIONS,       "re.functions",         "!re_functions",        BOTH, REVENG,     handleREFunctions,          "",             "List functions")
COMMAND_TABLE(4310, REVENG_DEMANGLE,        "re.demangle",          "!re_demangle",         BOTH, REVENG,     handleREDemangle,           "",             "Demangle symbols")
COMMAND_TABLE(4311, REVENG_SSA,             "re.ssa",               "!ssa_lift",            BOTH, REVENG,     handleRESSALift,            "",             "Static single assignment lift")
COMMAND_TABLE(4312, REVENG_RECURSIVE_DISASM,"re.recursiveDisasm",   "!re_recursive",        BOTH, REVENG,     handleRERecursiveDisasm,    "",             "Recursive disassembly")
COMMAND_TABLE(4313, REVENG_TYPE_RECOVERY,   "re.typeRecovery",      "!re_types",            BOTH, REVENG,     handleRETypeRecovery,       "",             "Type recovery")
COMMAND_TABLE(4314, REVENG_DATA_FLOW,       "re.dataFlow",          "!re_dataflow",         BOTH, REVENG,     handleREDataFlow,           "",             "Data flow analysis")
COMMAND_TABLE(4315, REVENG_LICENSE_INFO,    "re.licenseInfo",       "!re_license",          BOTH, REVENG,     handleRELicenseInfo,        "",             "License info")
COMMAND_TABLE(4316, REVENG_DECOMPILER_VIEW, "re.decompilerView",    "!re_decompile",        BOTH, REVENG,     handleREDecompilerView,     "",             "Decompiler view")
COMMAND_TABLE(4317, REVENG_DECOMP_RENAME,   "re.decompRename",      "!re_decomp_rename",    BOTH, REVENG,     handleREDecompRename,       "",             "Rename in decompiler")
COMMAND_TABLE(4318, REVENG_DECOMP_SYNC,     "re.decompSync",        "!re_decomp_sync",      BOTH, REVENG,     handleREDecompSync,         "",             "Sync decompiler view")
COMMAND_TABLE(4319, REVENG_DECOMP_CLOSE,    "re.decompClose",       "!re_decomp_close",     BOTH, REVENG,     handleREDecompClose,        "",             "Close decompiler")

// ============================================================================
// BACKEND SWITCHER (5037-5047)
// ============================================================================
COMMAND_TABLE(5037, BACKEND_SWITCH_LOCAL,   "backend.switchLocal",  "!backend local",       BOTH, BACKEND,    handleBackendSwitchLocal,   "",             "Switch to local backend")
COMMAND_TABLE(5038, BACKEND_SWITCH_OLLAMA,  "backend.switchOllama", "!backend ollama",      BOTH, BACKEND,    handleBackendSwitchOllama,  "",             "Switch to Ollama backend")
COMMAND_TABLE(5039, BACKEND_SWITCH_OPENAI,  "backend.switchOpenAI", "!backend openai",      BOTH, BACKEND,    handleBackendSwitchOpenAI,  "",             "Switch to OpenAI backend")
COMMAND_TABLE(5040, BACKEND_SWITCH_CLAUDE,  "backend.switchClaude", "!backend claude",      BOTH, BACKEND,    handleBackendSwitchClaude,  "",             "Switch to Claude backend")
COMMAND_TABLE(5041, BACKEND_SWITCH_GEMINI,  "backend.switchGemini", "!backend gemini",      BOTH, BACKEND,    handleBackendSwitchGemini,  "",             "Switch to Gemini backend")
COMMAND_TABLE(5042, BACKEND_SHOW_STATUS,    "backend.status",       "!backend status",      BOTH, BACKEND,    handleBackendShowStatus,    "",             "Current backend info")
COMMAND_TABLE(5043, BACKEND_SHOW_SWITCHER,  "backend.list",         "!backend list",        BOTH, BACKEND,    handleBackendShowSwitcher,  "",             "Show backend selector")
COMMAND_TABLE(5044, BACKEND_CONFIGURE,      "backend.configure",    "!backend config",      BOTH, BACKEND,    handleBackendConfigure,     "",             "Configure backend")
COMMAND_TABLE(5045, BACKEND_HEALTH_CHECK,   "backend.healthCheck",  "!backend health",      BOTH, BACKEND,    handleBackendHealthCheck,   "",             "Health check")
COMMAND_TABLE(5046, BACKEND_SET_API_KEY,    "backend.setApiKey",    "!backend apikey",      BOTH, BACKEND,    handleBackendSetApiKey,     "",             "Set API key")
COMMAND_TABLE(5047, BACKEND_SAVE_CONFIGS,   "backend.saveConfigs",  "!backend save",        BOTH, BACKEND,    handleBackendSaveConfigs,   "",             "Save backend configs")

// ============================================================================
// ROUTER (5048-5057, 5071-5081)
// ============================================================================
COMMAND_TABLE(5048, ROUTER_ENABLE,          "router.enable",        "!router enable",       BOTH, ROUTER,     handleRouterEnable,         "",             "Enable LLM router")
COMMAND_TABLE(5049, ROUTER_DISABLE,         "router.disable",       "!router disable",      BOTH, ROUTER,     handleRouterDisable,        "",             "Disable LLM router")
COMMAND_TABLE(5050, ROUTER_SHOW_STATUS,     "router.status",        "!router status",       BOTH, ROUTER,     handleRouterStatus,         "",             "Router status")
COMMAND_TABLE(5051, ROUTER_SHOW_DECISION,   "router.decision",      "!router decision",     BOTH, ROUTER,     handleRouterDecision,       "",             "Show routing decision")
COMMAND_TABLE(5052, ROUTER_SET_POLICY,      "router.setPolicy",     "!router policy",       BOTH, ROUTER,     handleRouterSetPolicy,      "",             "Set routing policy")
COMMAND_TABLE(5053, ROUTER_SHOW_CAPABILITIES,"router.capabilities", "!router caps",         BOTH, ROUTER,     handleRouterCapabilities,   "",             "Show capabilities")
COMMAND_TABLE(5054, ROUTER_SHOW_FALLBACKS,  "router.fallbacks",     "!router fallbacks",    BOTH, ROUTER,     handleRouterFallbacks,      "",             "Show fallback config")
COMMAND_TABLE(5055, ROUTER_SAVE_CONFIG,     "router.saveConfig",    "!router save",         BOTH, ROUTER,     handleRouterSaveConfig,     "",             "Save router config")
COMMAND_TABLE(5056, ROUTER_ROUTE_PROMPT,    "router.routePrompt",   "!router route",        BOTH, ROUTER,     handleRouterRoutePrompt,    "",             "Route a prompt")
COMMAND_TABLE(5057, ROUTER_RESET_STATS,     "router.resetStats",    "!router reset",        BOTH, ROUTER,     handleRouterResetStats,     "",             "Reset router stats")
COMMAND_TABLE(5071, ROUTER_WHY_BACKEND,     "router.whyBackend",    "!router why",          BOTH, ROUTER,     handleRouterWhyBackend,     "",             "Explain routing decision")
COMMAND_TABLE(5072, ROUTER_PIN_TASK,        "router.pinTask",       "!router pin",          BOTH, ROUTER,     handleRouterPinTask,        "",             "Pin task to backend")
COMMAND_TABLE(5073, ROUTER_UNPIN_TASK,      "router.unpinTask",     "!router unpin",        BOTH, ROUTER,     handleRouterUnpinTask,      "",             "Unpin task")
COMMAND_TABLE(5074, ROUTER_SHOW_PINS,       "router.showPins",      "!router pins",         BOTH, ROUTER,     handleRouterShowPins,       "",             "Show pinned tasks")
COMMAND_TABLE(5075, ROUTER_SHOW_HEATMAP,    "router.heatmap",       "!router heatmap",      BOTH, ROUTER,     handleRouterShowHeatmap,    "",             "Show router heatmap")
COMMAND_TABLE(5076, ROUTER_ENSEMBLE_ENABLE, "router.ensembleOn",    "!router ensemble on",  BOTH, ROUTER,     handleRouterEnsembleEnable, "",             "Enable ensemble routing")
COMMAND_TABLE(5077, ROUTER_ENSEMBLE_DISABLE,"router.ensembleOff",   "!router ensemble off", BOTH, ROUTER,     handleRouterEnsembleDisable,"",             "Disable ensemble routing")
COMMAND_TABLE(5078, ROUTER_ENSEMBLE_STATUS, "router.ensembleStat",  "!router ensemble stat",BOTH, ROUTER,     handleRouterEnsembleStatus, "",             "Ensemble status")
COMMAND_TABLE(5079, ROUTER_SIMULATE,        "router.simulate",      "!router simulate",     BOTH, ROUTER,     handleRouterSimulate,       "",             "Simulate routing")
COMMAND_TABLE(5080, ROUTER_SIMULATE_LAST,   "router.simulateLast",  "!router sim_last",     BOTH, ROUTER,     handleRouterSimulateLast,   "",             "Simulate last decision")
COMMAND_TABLE(5081, ROUTER_SHOW_COST_STATS, "router.costStats",     "!router cost",         BOTH, ROUTER,     handleRouterShowCostStats,  "",             "Show cost statistics")

// ============================================================================
// LSP CLIENT (5058-5070)
// ============================================================================
COMMAND_TABLE(5058, LSP_START_ALL,          "lsp.startAll",         "!lsp start",           BOTH, LSP,        handleLspStartAll,          "",             "Start all LSP servers")
COMMAND_TABLE(5059, LSP_STOP_ALL,           "lsp.stopAll",          "!lsp stop",            BOTH, LSP,        handleLspStopAll,           "",             "Stop all LSP servers")
COMMAND_TABLE(5060, LSP_SHOW_STATUS,        "lsp.status",           "!lsp status",          BOTH, LSP,        handleLspStatus,            "",             "LSP server status")
COMMAND_TABLE(5061, LSP_GOTO_DEFINITION,    "lsp.gotoDef",          "!lsp goto",            BOTH, LSP,        handleLspGotoDef,           "F12",          "Go to definition")
COMMAND_TABLE(5062, LSP_FIND_REFERENCES,    "lsp.findRefs",         "!lsp refs",            BOTH, LSP,        handleLspFindRefs,          "Shift+F12",    "Find all references")
COMMAND_TABLE(5063, LSP_RENAME_SYMBOL,      "lsp.rename",           "!lsp rename",          BOTH, LSP,        handleLspRename,            "F2",           "Rename symbol")
COMMAND_TABLE(5064, LSP_HOVER_INFO,         "lsp.hover",            "!lsp hover",           BOTH, LSP,        handleLspHover,             "",             "Show hover info")
COMMAND_TABLE(5065, LSP_SHOW_DIAGNOSTICS,   "lsp.diagnostics",      "!lsp diag",            BOTH, LSP,        handleLspDiagnostics,       "Ctrl+Shift+M", "Show diagnostics")
COMMAND_TABLE(5066, LSP_RESTART_SERVER,     "lsp.restart",          "!lsp restart",         BOTH, LSP,        handleLspRestart,           "",             "Restart LSP server")
COMMAND_TABLE(5067, LSP_CLEAR_DIAGNOSTICS,  "lsp.clearDiag",        "!lsp clear",           BOTH, LSP,        handleLspClearDiag,         "",             "Clear diagnostics")
COMMAND_TABLE(5068, LSP_SHOW_SYMBOL_INFO,   "lsp.symbolInfo",       "!lsp symbol",          BOTH, LSP,        handleLspSymbolInfo,        "",             "Show symbol info")
COMMAND_TABLE(5069, LSP_CONFIGURE,          "lsp.configure",        "!lsp config",          BOTH, LSP,        handleLspConfigure,         "",             "Configure LSP")
COMMAND_TABLE(5070, LSP_SAVE_CONFIG,        "lsp.saveConfig",       "!lsp save",            BOTH, LSP,        handleLspSaveConfig,        "",             "Save LSP config")

// ============================================================================
// ASM SEMANTIC (5082-5093)
// ============================================================================
COMMAND_TABLE(5082, ASM_PARSE_SYMBOLS,      "asm.parseSymbols",     "!asm parse",           BOTH, ASM,        handleAsmParse,             "",             "Parse ASM symbols")
COMMAND_TABLE(5083, ASM_GOTO_LABEL,         "asm.gotoLabel",        "!asm goto",            BOTH, ASM,        handleAsmGoto,              "",             "Go to ASM label")
COMMAND_TABLE(5084, ASM_FIND_LABEL_REFS,    "asm.findLabelRefs",    "!asm refs",            BOTH, ASM,        handleAsmFindRefs,          "",             "Find label references")
COMMAND_TABLE(5085, ASM_SHOW_SYMBOL_TABLE,  "asm.symbolTable",      "!asm symbols",         BOTH, ASM,        handleAsmSymbolTable,       "",             "Show symbol table")
COMMAND_TABLE(5086, ASM_INSTRUCTION_INFO,   "asm.instructionInfo",  "!asm instruction",     BOTH, ASM,        handleAsmInstructionInfo,   "",             "Instruction info")
COMMAND_TABLE(5087, ASM_REGISTER_INFO,      "asm.registerInfo",     "!asm register",        BOTH, ASM,        handleAsmRegisterInfo,      "",             "Register info")
COMMAND_TABLE(5088, ASM_ANALYZE_BLOCK,      "asm.analyzeBlock",     "!asm block",           BOTH, ASM,        handleAsmAnalyzeBlock,      "",             "Analyze code block")
COMMAND_TABLE(5089, ASM_SHOW_CALL_GRAPH,    "asm.callGraph",        "!asm callgraph",       BOTH, ASM,        handleAsmCallGraph,         "",             "Show call graph")
COMMAND_TABLE(5090, ASM_SHOW_DATA_FLOW,     "asm.dataFlow",         "!asm dataflow",        BOTH, ASM,        handleAsmDataFlow,          "",             "Show data flow")
COMMAND_TABLE(5091, ASM_DETECT_CONVENTION,  "asm.detectConvention", "!asm convention",      BOTH, ASM,        handleAsmDetectConvention,  "",             "Detect calling convention")
COMMAND_TABLE(5092, ASM_SHOW_SECTIONS,      "asm.sections",         "!asm sections",        BOTH, ASM,        handleAsmSections,          "",             "Show PE sections")
COMMAND_TABLE(5093, ASM_CLEAR_SYMBOLS,      "asm.clearSymbols",     "!asm clear",           BOTH, ASM,        handleAsmClearSymbols,      "",             "Clear symbol cache")

// ============================================================================
// HYBRID LSP-AI (5094-5105)
// ============================================================================
COMMAND_TABLE(5094, HYBRID_COMPLETE,        "hybrid.complete",      "!hybrid complete",     BOTH, HYBRID,     handleHybridComplete,       "",             "AI-assisted completion")
COMMAND_TABLE(5095, HYBRID_DIAGNOSTICS,     "hybrid.diagnostics",   "!hybrid diag",         BOTH, HYBRID,     handleHybridDiagnostics,    "",             "Hybrid diagnostics")
COMMAND_TABLE(5096, HYBRID_SMART_RENAME,    "hybrid.smartRename",   "!hybrid rename",       BOTH, HYBRID,     handleHybridSmartRename,    "Alt+F2",       "AI-powered smart rename")
COMMAND_TABLE(5097, HYBRID_ANALYZE_FILE,    "hybrid.analyzeFile",   "!hybrid analyze",      BOTH, HYBRID,     handleHybridAnalyzeFile,    "",             "AI file analysis")
COMMAND_TABLE(5098, HYBRID_AUTO_PROFILE,    "hybrid.autoProfile",   "!hybrid profile",      BOTH, HYBRID,     handleHybridAutoProfile,    "",             "Auto-profile function")
COMMAND_TABLE(5099, HYBRID_STATUS,          "hybrid.status",        "!hybrid status",       BOTH, HYBRID,     handleHybridStatus,         "",             "Hybrid engine status")
COMMAND_TABLE(5100, HYBRID_SYMBOL_USAGE,    "hybrid.symbolUsage",   "!hybrid usage",        BOTH, HYBRID,     handleHybridSymbolUsage,    "",             "Symbol usage analysis")
COMMAND_TABLE(5101, HYBRID_EXPLAIN_SYMBOL,  "hybrid.explainSymbol", "!hybrid explain",      BOTH, HYBRID,     handleHybridExplainSymbol,  "Ctrl+Shift+E", "Explain symbol")
COMMAND_TABLE(5102, HYBRID_ANNOTATE_DIAG,   "hybrid.annotateDiag",  "!hybrid annotate",     BOTH, HYBRID,     handleHybridAnnotateDiag,   "",             "Annotate diagnostics")
COMMAND_TABLE(5103, HYBRID_STREAM_ANALYZE,  "hybrid.streamAnalyze", "!hybrid stream",       BOTH, HYBRID,     handleHybridStreamAnalyze,  "",             "Stream analysis")
COMMAND_TABLE(5104, HYBRID_SEMANTIC_PREFETCH,"hybrid.prefetch",     "!hybrid prefetch",     BOTH, HYBRID,     handleHybridSemanticPrefetch,"",            "Semantic prefetch")
COMMAND_TABLE(5105, HYBRID_CORRECTION_LOOP, "hybrid.correctionLoop","!hybrid correct",      BOTH, HYBRID,     handleHybridCorrectionLoop, "",             "Correction loop")

// ============================================================================
// MULTI-RESPONSE (5106-5117)
// ============================================================================
COMMAND_TABLE(5106, MULTI_RESP_GENERATE,        "multi.generate",       "!multi generate",      BOTH, MULTI_RESP, handleMultiRespGenerate,         "",     "Generate multiple responses")
COMMAND_TABLE(5107, MULTI_RESP_SET_MAX,         "multi.setMax",         "!multi setmax",        BOTH, MULTI_RESP, handleMultiRespSetMax,           "",     "Set max responses")
COMMAND_TABLE(5108, MULTI_RESP_SELECT_PREFERRED,"multi.selectPref",     "!multi select",        BOTH, MULTI_RESP, handleMultiRespSelectPreferred,  "",     "Select preferred response")
COMMAND_TABLE(5109, MULTI_RESP_COMPARE,         "multi.compare",        "!multi compare",       BOTH, MULTI_RESP, handleMultiRespCompare,          "",     "Compare responses")
COMMAND_TABLE(5110, MULTI_RESP_SHOW_STATS,      "multi.stats",          "!multi stats",         BOTH, MULTI_RESP, handleMultiRespShowStats,        "",     "Show multi-resp stats")
COMMAND_TABLE(5111, MULTI_RESP_SHOW_TEMPLATES,  "multi.templates",      "!multi templates",     BOTH, MULTI_RESP, handleMultiRespShowTemplates,    "",     "Show templates")
COMMAND_TABLE(5112, MULTI_RESP_TOGGLE_TEMPLATE, "multi.toggleTemplate", "!multi toggle",        BOTH, MULTI_RESP, handleMultiRespToggleTemplate,   "",     "Toggle template")
COMMAND_TABLE(5113, MULTI_RESP_SHOW_PREFS,      "multi.prefs",          "!multi prefs",         BOTH, MULTI_RESP, handleMultiRespShowPrefs,        "",     "Show preferences")
COMMAND_TABLE(5114, MULTI_RESP_SHOW_LATEST,     "multi.latest",         "!multi latest",        BOTH, MULTI_RESP, handleMultiRespShowLatest,       "",     "Show latest responses")
COMMAND_TABLE(5115, MULTI_RESP_SHOW_STATUS,     "multi.status",         "!multi status",        BOTH, MULTI_RESP, handleMultiRespShowStatus,       "",     "Multi-resp status")
COMMAND_TABLE(5116, MULTI_RESP_CLEAR_HISTORY,   "multi.clearHistory",   "!multi clear",         BOTH, MULTI_RESP, handleMultiRespClearHistory,     "",     "Clear response history")
COMMAND_TABLE(5117, MULTI_RESP_APPLY_PREFERRED, "multi.applyPref",      "!multi apply",         BOTH, MULTI_RESP, handleMultiRespApplyPreferred,   "",     "Apply preferred response")

// ============================================================================
// GOVERNOR (5118-5121)
// ============================================================================
COMMAND_TABLE(5118, GOV_STATUS,             "gov.status",           "!gov status",          BOTH, GOV,        handleGovStatus,            "",             "Governor status")
COMMAND_TABLE(5119, GOV_SUBMIT_COMMAND,     "gov.submit",           "!gov submit",          BOTH, GOV,        handleGovSubmitCommand,     "",             "Submit task to governor")
COMMAND_TABLE(5120, GOV_KILL_ALL,           "gov.killAll",          "!gov killall",         BOTH, GOV,        handleGovKillAll,           "",             "Kill all governor tasks")
COMMAND_TABLE(5121, GOV_TASK_LIST,          "gov.taskList",         "!gov tasks",           BOTH, GOV,        handleGovTaskList,          "",             "List governor tasks")

// ============================================================================
// SAFETY (5122-5125)
// ============================================================================
COMMAND_TABLE(5122, SAFETY_STATUS,          "safety.status",        "!safety status",       BOTH, SAFETY,     handleSafetyStatus,         "",             "Safety system status")
COMMAND_TABLE(5123, SAFETY_RESET_BUDGET,    "safety.resetBudget",   "!safety reset",        BOTH, SAFETY,     handleSafetyResetBudget,    "",             "Reset safety budget")
COMMAND_TABLE(5124, SAFETY_ROLLBACK_LAST,   "safety.rollback",      "!safety rollback",     BOTH, SAFETY,     handleSafetyRollbackLast,   "",             "Rollback last action")
COMMAND_TABLE(5125, SAFETY_SHOW_VIOLATIONS, "safety.violations",    "!safety violations",   BOTH, SAFETY,     handleSafetyShowViolations,  "",            "Show safety violations")

// ============================================================================
// REPLAY (5126-5129)
// ============================================================================
COMMAND_TABLE(5126, REPLAY_STATUS,          "replay.status",        "!replay status",       BOTH, REPLAY,     handleReplayStatus,         "",             "Replay harness status")
COMMAND_TABLE(5127, REPLAY_SHOW_LAST,       "replay.showLast",      "!replay last",         BOTH, REPLAY,     handleReplayShowLast,       "",             "Show last replay")
COMMAND_TABLE(5128, REPLAY_EXPORT_SESSION,  "replay.export",        "!replay export",       BOTH, REPLAY,     handleReplayExportSession,  "",             "Export replay session")
COMMAND_TABLE(5129, REPLAY_CHECKPOINT,      "replay.checkpoint",    "!replay checkpoint",   BOTH, REPLAY,     handleReplayCheckpoint,     "",             "Create checkpoint")

// ============================================================================
// CONFIDENCE (5130-5131)
// ============================================================================
COMMAND_TABLE(5130, CONFIDENCE_STATUS,      "confidence.status",    "!confidence status",   BOTH, CONFIDENCE, handleConfidenceStatus,     "",             "Confidence gate status")
COMMAND_TABLE(5131, CONFIDENCE_SET_POLICY,  "confidence.setPolicy", "!confidence policy",   BOTH, CONFIDENCE, handleConfidenceSetPolicy,  "",             "Set confidence policy")

// ============================================================================
// SWARM (5132-5156)
// ============================================================================
COMMAND_TABLE(5132, SWARM_STATUS,           "swarm.status",         "!swarm_status",        BOTH, SWARM,      handleSwarmStatus,          "",             "Swarm cluster status")
COMMAND_TABLE(5133, SWARM_START_LEADER,     "swarm.startLeader",    "!swarm leader",        BOTH, SWARM,      handleSwarmStartLeader,     "",             "Start as leader node")
COMMAND_TABLE(5134, SWARM_START_WORKER,     "swarm.startWorker",    "!swarm worker",        BOTH, SWARM,      handleSwarmStartWorker,     "",             "Start as worker node")
COMMAND_TABLE(5135, SWARM_START_HYBRID,     "swarm.startHybrid",    "!swarm hybrid",        BOTH, SWARM,      handleSwarmStartHybrid,     "",             "Start as hybrid node")
COMMAND_TABLE(5136, SWARM_STOP,             "swarm.stop",           "!swarm_leave",         BOTH, SWARM,      handleSwarmLeave,           "",             "Stop swarm node")
COMMAND_TABLE(5137, SWARM_LIST_NODES,       "swarm.listNodes",      "!swarm_nodes",         BOTH, SWARM,      handleSwarmNodes,           "",             "List cluster nodes")
COMMAND_TABLE(5138, SWARM_ADD_NODE,         "swarm.addNode",        "!swarm_join",          BOTH, SWARM,      handleSwarmJoin,            "",             "Add node to cluster")
COMMAND_TABLE(5139, SWARM_REMOVE_NODE,      "swarm.removeNode",     "!swarm remove",        BOTH, SWARM,      handleSwarmRemoveNode,      "",             "Remove node")
COMMAND_TABLE(5140, SWARM_BLACKLIST_NODE,   "swarm.blacklistNode",  "!swarm blacklist",     BOTH, SWARM,      handleSwarmBlacklistNode,   "",             "Blacklist node")
COMMAND_TABLE(5141, SWARM_BUILD_SOURCES,    "swarm.buildSources",   "!swarm build_src",     BOTH, SWARM,      handleSwarmBuildSources,    "",             "Build sources")
COMMAND_TABLE(5142, SWARM_BUILD_CMAKE,      "swarm.buildCMake",     "!swarm build_cmake",   BOTH, SWARM,      handleSwarmBuildCMake,      "",             "CMake build")
COMMAND_TABLE(5143, SWARM_START_BUILD,      "swarm.startBuild",     "!swarm start_build",   BOTH, SWARM,      handleSwarmStartBuild,      "",             "Start distributed build")
COMMAND_TABLE(5144, SWARM_CANCEL_BUILD,     "swarm.cancelBuild",    "!swarm cancel_build",  BOTH, SWARM,      handleSwarmCancelBuild,     "",             "Cancel distributed build")
COMMAND_TABLE(5145, SWARM_CACHE_STATUS,     "swarm.cacheStatus",    "!swarm cache",         BOTH, SWARM,      handleSwarmCacheStatus,     "",             "Cache status")
COMMAND_TABLE(5146, SWARM_CACHE_CLEAR,      "swarm.cacheClear",     "!swarm cache_clear",   BOTH, SWARM,      handleSwarmCacheClear,      "",             "Clear cache")
COMMAND_TABLE(5147, SWARM_SHOW_CONFIG,      "swarm.showConfig",     "!swarm config",        BOTH, SWARM,      handleSwarmShowConfig,      "",             "Show config")
COMMAND_TABLE(5148, SWARM_TOGGLE_DISCOVERY, "swarm.discovery",      "!swarm discovery",     BOTH, SWARM,      handleSwarmToggleDiscovery, "",             "Toggle discovery")
COMMAND_TABLE(5149, SWARM_SHOW_TASK_GRAPH,  "swarm.taskGraph",      "!swarm taskgraph",     BOTH, SWARM,      handleSwarmShowTaskGraph,   "",             "Show task graph")
COMMAND_TABLE(5150, SWARM_SHOW_EVENTS,      "swarm.events",         "!swarm events",        BOTH, SWARM,      handleSwarmShowEvents,      "",             "Show events")
COMMAND_TABLE(5151, SWARM_SHOW_STATS,       "swarm.stats",          "!swarm stats",         BOTH, SWARM,      handleSwarmShowStats,       "",             "Show statistics")
COMMAND_TABLE(5152, SWARM_RESET_STATS,      "swarm.resetStats",     "!swarm reset",         BOTH, SWARM,      handleSwarmResetStats,      "",             "Reset statistics")
COMMAND_TABLE(5153, SWARM_WORKER_STATUS,    "swarm.workerStatus",   "!swarm wstatus",       BOTH, SWARM,      handleSwarmWorkerStatus,    "",             "Worker status")
COMMAND_TABLE(5154, SWARM_WORKER_CONNECT,   "swarm.workerConnect",  "!swarm wconnect",      BOTH, SWARM,      handleSwarmWorkerConnect,   "",             "Connect worker")
COMMAND_TABLE(5155, SWARM_WORKER_DISCONNECT,"swarm.workerDisconnect","!swarm wdisconnect",  BOTH, SWARM,      handleSwarmWorkerDisconnect,"",             "Disconnect worker")
COMMAND_TABLE(5156, SWARM_FITNESS_TEST,     "swarm.fitnessTest",    "!swarm fitness",       BOTH, SWARM,      handleSwarmFitnessTest,     "",             "Fitness test")

// ============================================================================
// DEBUG (5157-5184)
// ============================================================================
COMMAND_TABLE(5157, DBG_LAUNCH,             "dbg.launch",           "!debug_start",         BOTH, DEBUG,      handleDbgLaunch,            "F5",           "Launch debugger")
COMMAND_TABLE(5158, DBG_ATTACH,             "dbg.attach",           "!dbg attach",          BOTH, DEBUG,      handleDbgAttach,            "",             "Attach to process")
COMMAND_TABLE(5159, DBG_DETACH,             "dbg.detach",           "!dbg detach",          BOTH, DEBUG,      handleDbgDetach,            "",             "Detach from process")
COMMAND_TABLE(5160, DBG_GO,                 "dbg.go",               "!debug_continue",      BOTH, DEBUG,      handleDbgGo,                "F5",           "Resume execution")
COMMAND_TABLE(5161, DBG_STEP_OVER,          "dbg.stepOver",         "!debug_step",          BOTH, DEBUG,      handleDbgStepOver,          "F10",          "Step over")
COMMAND_TABLE(5162, DBG_STEP_INTO,          "dbg.stepInto",         "!dbg stepin",          BOTH, DEBUG,      handleDbgStepInto,          "F11",          "Step into")
COMMAND_TABLE(5163, DBG_STEP_OUT,           "dbg.stepOut",          "!dbg stepout",         BOTH, DEBUG,      handleDbgStepOut,           "Shift+F11",    "Step out")
COMMAND_TABLE(5164, DBG_BREAK,              "dbg.break",            "!dbg break",           BOTH, DEBUG,      handleDbgBreak,             "",             "Break execution")
COMMAND_TABLE(5165, DBG_KILL,               "dbg.kill",             "!debug_stop",          BOTH, DEBUG,      handleDbgKill,              "Shift+F5",     "Stop debugging")
COMMAND_TABLE(5166, DBG_ADD_BP,             "dbg.addBp",            "!breakpoint_add",      BOTH, DEBUG,      handleDbgAddBp,             "F9",           "Set breakpoint")
COMMAND_TABLE(5167, DBG_REMOVE_BP,          "dbg.removeBp",         "!breakpoint_remove",   BOTH, DEBUG,      handleDbgRemoveBp,          "",             "Remove breakpoint")
COMMAND_TABLE(5168, DBG_ENABLE_BP,          "dbg.enableBp",         "!dbg enable_bp",       BOTH, DEBUG,      handleDbgEnableBp,          "",             "Enable breakpoint")
COMMAND_TABLE(5169, DBG_CLEAR_BPS,          "dbg.clearBps",         "!dbg clear_bp",        BOTH, DEBUG,      handleDbgClearBps,          "",             "Clear all breakpoints")
COMMAND_TABLE(5170, DBG_LIST_BPS,           "dbg.listBps",          "!breakpoint_list",     BOTH, DEBUG,      handleDbgListBps,           "",             "List breakpoints")
COMMAND_TABLE(5171, DBG_ADD_WATCH,          "dbg.addWatch",         "!dbg watch",           BOTH, DEBUG,      handleDbgAddWatch,          "",             "Add watch expression")
COMMAND_TABLE(5172, DBG_REMOVE_WATCH,       "dbg.removeWatch",      "!dbg unwatch",         BOTH, DEBUG,      handleDbgRemoveWatch,       "",             "Remove watch")
COMMAND_TABLE(5173, DBG_REGISTERS,          "dbg.registers",        "!dbg registers",       BOTH, DEBUG,      handleDbgRegisters,         "",             "Show registers")
COMMAND_TABLE(5174, DBG_STACK,              "dbg.stack",            "!dbg stack",           BOTH, DEBUG,      handleDbgStack,             "",             "Show call stack")
COMMAND_TABLE(5175, DBG_MEMORY,             "dbg.memory",           "!dbg memory",          BOTH, DEBUG,      handleDbgMemory,            "",             "Memory view")
COMMAND_TABLE(5176, DBG_DISASM,             "dbg.disasm",           "!dbg disasm",          BOTH, DEBUG,      handleDbgDisasm,            "",             "Disassembly view")
COMMAND_TABLE(5177, DBG_MODULES,            "dbg.modules",          "!dbg modules",         BOTH, DEBUG,      handleDbgModules,           "",             "Loaded modules")
COMMAND_TABLE(5178, DBG_THREADS,            "dbg.threads",          "!dbg threads",         BOTH, DEBUG,      handleDbgThreads,           "",             "Thread list")
COMMAND_TABLE(5179, DBG_SWITCH_THREAD,      "dbg.switchThread",     "!dbg thread",          BOTH, DEBUG,      handleDbgSwitchThread,      "",             "Switch to thread")
COMMAND_TABLE(5180, DBG_EVALUATE,           "dbg.evaluate",         "!dbg eval",            BOTH, DEBUG,      handleDbgEvaluate,          "",             "Evaluate expression")
COMMAND_TABLE(5181, DBG_SET_REGISTER,       "dbg.setRegister",      "!dbg setreg",          BOTH, DEBUG,      handleDbgSetRegister,       "",             "Set register value")
COMMAND_TABLE(5182, DBG_SEARCH_MEMORY,      "dbg.searchMemory",     "!dbg search",          BOTH, DEBUG,      handleDbgSearchMemory,      "",             "Search memory")
COMMAND_TABLE(5183, DBG_SYMBOL_PATH,        "dbg.symbolPath",       "!dbg sympath",         BOTH, DEBUG,      handleDbgSymbolPath,        "",             "Set symbol path")
COMMAND_TABLE(5184, DBG_STATUS,             "dbg.status",           "!dbg status",          BOTH, DEBUG,      handleDbgStatus,            "",             "Debugger status")

// ============================================================================
// PLUGIN SYSTEM (5200-5208)
// ============================================================================
COMMAND_TABLE(5200, PLUGIN_SHOW_PANEL,      "plugin.panel",         "!plugin panel",        BOTH, PLUGIN,     handlePluginShowPanel,      "",             "Show plugin panel")
COMMAND_TABLE(5201, PLUGIN_LOAD,            "plugin.load",          "!plugin load",         BOTH, PLUGIN,     handlePluginLoad,           "",             "Load plugin")
COMMAND_TABLE(5202, PLUGIN_UNLOAD,          "plugin.unload",        "!plugin unload",       BOTH, PLUGIN,     handlePluginUnload,         "",             "Unload plugin")
COMMAND_TABLE(5203, PLUGIN_UNLOAD_ALL,      "plugin.unloadAll",     "!plugin unload_all",   BOTH, PLUGIN,     handlePluginUnloadAll,      "",             "Unload all plugins")
COMMAND_TABLE(5204, PLUGIN_REFRESH,         "plugin.refresh",       "!plugin refresh",      BOTH, PLUGIN,     handlePluginRefresh,        "",             "Refresh plugin list")
COMMAND_TABLE(5205, PLUGIN_SCAN_DIR,        "plugin.scanDir",       "!plugin scan",         BOTH, PLUGIN,     handlePluginScanDir,        "",             "Scan directory for plugins")
COMMAND_TABLE(5206, PLUGIN_SHOW_STATUS,     "plugin.status",        "!plugin status",       BOTH, PLUGIN,     handlePluginShowStatus,     "",             "Plugin system status")
COMMAND_TABLE(5207, PLUGIN_TOGGLE_HOTLOAD,  "plugin.hotload",       "!plugin hotload",      BOTH, PLUGIN,     handlePluginToggleHotload,  "",             "Toggle hot-loading")
COMMAND_TABLE(5208, PLUGIN_CONFIGURE,       "plugin.configure",     "!plugin config",       BOTH, PLUGIN,     handlePluginConfigure,      "",             "Configure plugin")

// ============================================================================
// HOTPATCH (9001-9017)
// ============================================================================
COMMAND_TABLE(9001, HOTPATCH_SHOW_STATUS,   "hotpatch.status",      "!hotpatch_status",     BOTH, HOTPATCH,   handleHotpatchStatus,       "",             "All 3 layers status")
COMMAND_TABLE(9002, HOTPATCH_MEMORY_APPLY,  "hotpatch.memoryApply", "!hotpatch_mem",        BOTH, HOTPATCH,   handleHotpatchMemory,       "",             "Apply memory hotpatch")
COMMAND_TABLE(9003, HOTPATCH_MEMORY_REVERT, "hotpatch.memoryRevert","!hotpatch_mem_revert", BOTH, HOTPATCH,   handleHotpatchMemoryRevert, "",             "Revert memory hotpatch")
COMMAND_TABLE(9004, HOTPATCH_BYTE_APPLY,    "hotpatch.byteApply",   "!hotpatch_byte",       BOTH, HOTPATCH,   handleHotpatchByte,         "",             "Apply byte-level patch")
COMMAND_TABLE(9005, HOTPATCH_BYTE_SEARCH,   "hotpatch.byteSearch",  "!hotpatch_search",     BOTH, HOTPATCH,   handleHotpatchByteSearch,   "",             "Search bytes in model")
COMMAND_TABLE(9006, HOTPATCH_SERVER_ADD,    "hotpatch.serverAdd",   "!hotpatch_server",     BOTH, HOTPATCH,   handleHotpatchServer,       "",             "Add server hotpatch")
COMMAND_TABLE(9007, HOTPATCH_SERVER_REMOVE, "hotpatch.serverRemove","!hotpatch_srv_remove", BOTH, HOTPATCH,   handleHotpatchServerRemove, "",             "Remove server hotpatch")
COMMAND_TABLE(9008, HOTPATCH_PROXY_BIAS,    "hotpatch.proxyBias",   "!hotpatch_bias",       BOTH, HOTPATCH,   handleHotpatchProxyBias,    "",             "Proxy token bias")
COMMAND_TABLE(9009, HOTPATCH_PROXY_REWRITE, "hotpatch.proxyRewrite","!hotpatch_rewrite",    BOTH, HOTPATCH,   handleHotpatchProxyRewrite, "",             "Proxy output rewrite")
COMMAND_TABLE(9010, HOTPATCH_PROXY_TERMINATE,"hotpatch.proxyTerminate","!hotpatch_terminate",BOTH,HOTPATCH,    handleHotpatchProxyTerminate,"",            "Proxy stream terminate")
COMMAND_TABLE(9011, HOTPATCH_PROXY_VALIDATE,"hotpatch.proxyValidate","!hotpatch_validate",  BOTH, HOTPATCH,   handleHotpatchProxyValidate,"",             "Proxy validation")
COMMAND_TABLE(9012, HOTPATCH_PRESET_SAVE,   "hotpatch.presetSave",  "!hotpatch_save",       BOTH, HOTPATCH,   handleHotpatchPresetSave,   "",             "Save hotpatch preset")
COMMAND_TABLE(9013, HOTPATCH_PRESET_LOAD,   "hotpatch.presetLoad",  "!hotpatch_load",       BOTH, HOTPATCH,   handleHotpatchPresetLoad,   "",             "Load hotpatch preset")
COMMAND_TABLE(9014, HOTPATCH_SHOW_EVENT_LOG,"hotpatch.eventLog",    "!hotpatch_log",        BOTH, HOTPATCH,   handleHotpatchShowEventLog, "",             "Show event log")
COMMAND_TABLE(9015, HOTPATCH_RESET_STATS,   "hotpatch.resetStats",  "!hotpatch_reset",      BOTH, HOTPATCH,   handleHotpatchResetStats,   "",             "Reset hotpatch stats")
COMMAND_TABLE(9016, HOTPATCH_TOGGLE_ALL,    "hotpatch.toggleAll",   "!hotpatch_toggle",     BOTH, HOTPATCH,   handleHotpatchToggleAll,    "",             "Toggle all hotpatches")
COMMAND_TABLE(9017, HOTPATCH_SHOW_PROXY_STATS,"hotpatch.proxyStats","!hotpatch_proxy",      BOTH, HOTPATCH,   handleHotpatchShowProxyStats,"",            "Proxy statistics")

// ============================================================================
// MONACO (9100-9105)
// ============================================================================
COMMAND_TABLE(9100, VIEW_TOGGLE_MONACO,     "monaco.toggle",        "!monaco toggle",       BOTH, MONACO,     handleMonacoToggle,         "",             "Toggle Monaco editor")
COMMAND_TABLE(9101, VIEW_MONACO_DEVTOOLS,   "monaco.devtools",      "!monaco devtools",     BOTH, MONACO,     handleMonacoDevtools,       "",             "Monaco DevTools")
COMMAND_TABLE(9102, VIEW_MONACO_RELOAD,     "monaco.reload",        "!monaco reload",       BOTH, MONACO,     handleMonacoReload,         "",             "Reload Monaco")
COMMAND_TABLE(9103, VIEW_MONACO_ZOOM_IN,    "monaco.zoomIn",        "!monaco zoomin",       BOTH, MONACO,     handleMonacoZoomIn,         "Ctrl+=",       "Zoom in")
COMMAND_TABLE(9104, VIEW_MONACO_ZOOM_OUT,   "monaco.zoomOut",       "!monaco zoomout",      BOTH, MONACO,     handleMonacoZoomOut,        "Ctrl+-",       "Zoom out")
COMMAND_TABLE(9105, VIEW_MONACO_SYNC_THEME, "monaco.syncTheme",     "!monaco theme",        BOTH, MONACO,     handleMonacoSyncTheme,      "",             "Sync theme with Monaco")

// ============================================================================
// LSP SERVER (9200-9208)
// ============================================================================
COMMAND_TABLE(9200, LSP_SERVER_START,       "lspServer.start",      "!lsps start",          BOTH, LSP_SERVER, handleLspServerStart,       "",             "Start LSP server")
COMMAND_TABLE(9201, LSP_SERVER_STOP,        "lspServer.stop",       "!lsps stop",           BOTH, LSP_SERVER, handleLspServerStop,        "",             "Stop LSP server")
COMMAND_TABLE(9202, LSP_SERVER_STATUS,      "lspServer.status",     "!lsps status",         BOTH, LSP_SERVER, handleLspServerStatus,      "",             "LSP server status")
COMMAND_TABLE(9203, LSP_SERVER_REINDEX,     "lspServer.reindex",    "!lsps reindex",        BOTH, LSP_SERVER, handleLspServerReindex,     "",             "Reindex workspace")
COMMAND_TABLE(9204, LSP_SERVER_STATS,       "lspServer.stats",      "!lsps stats",          BOTH, LSP_SERVER, handleLspServerStats,       "",             "LSP server statistics")
COMMAND_TABLE(9205, LSP_SERVER_PUBLISH_DIAG,"lspServer.publishDiag","!lsps publish",        BOTH, LSP_SERVER, handleLspServerPublishDiag, "",             "Publish diagnostics")
COMMAND_TABLE(9206, LSP_SERVER_CONFIG,      "lspServer.config",     "!lsps config",         BOTH, LSP_SERVER, handleLspServerConfig,      "",             "LSP server config")
COMMAND_TABLE(9207, LSP_SERVER_EXPORT_SYMBOLS,"lspServer.export",   "!lsps export",         BOTH, LSP_SERVER, handleLspServerExportSymbols,"",            "Export symbols")
COMMAND_TABLE(9208, LSP_SERVER_LAUNCH_STDIO,"lspServer.stdio",      "!lsps stdio",          BOTH, LSP_SERVER, handleLspServerLaunchStdio, "",             "Launch via stdio")

// ============================================================================
// EDITOR ENGINE (9300-9304)
// ============================================================================
COMMAND_TABLE(9300, EDITOR_ENGINE_RICHEDIT, "editor.richedit",      "!editor richedit",     BOTH, EDITOR_ENGINE, handleEditorRichEdit,    "",             "Switch to RichEdit")
COMMAND_TABLE(9301, EDITOR_ENGINE_WEBVIEW2, "editor.webview2",      "!editor webview2",     BOTH, EDITOR_ENGINE, handleEditorWebView2,    "",             "Switch to WebView2")
COMMAND_TABLE(9302, EDITOR_ENGINE_MONACOCORE,"editor.monacocore",   "!editor monaco",       BOTH, EDITOR_ENGINE, handleEditorMonacoCore,  "",             "Switch to Monaco Core")
COMMAND_TABLE(9303, EDITOR_ENGINE_CYCLE,    "editor.cycle",         "!editor cycle",        BOTH, EDITOR_ENGINE, handleEditorCycle,       "",             "Cycle editor engine")
COMMAND_TABLE(9304, EDITOR_ENGINE_STATUS,   "editor.status",        "!editor status",       BOTH, EDITOR_ENGINE, handleEditorStatus,      "",             "Editor engine status")

// ============================================================================
// PDB (9400-9412)
// ============================================================================
COMMAND_TABLE(9400, PDB_LOAD,               "pdb.load",             "!pdb load",            BOTH, PDB,        handlePdbLoad,              "",             "Load PDB file")
COMMAND_TABLE(9401, PDB_FETCH,              "pdb.fetch",            "!pdb fetch",           BOTH, PDB,        handlePdbFetch,             "",             "Fetch PDB from server")
COMMAND_TABLE(9402, PDB_STATUS,             "pdb.status",           "!pdb status",          BOTH, PDB,        handlePdbStatus,            "",             "PDB status")
COMMAND_TABLE(9403, PDB_CACHE_CLEAR,        "pdb.cacheClear",       "!pdb clear",           BOTH, PDB,        handlePdbCacheClear,        "",             "Clear PDB cache")
COMMAND_TABLE(9404, PDB_ENABLE,             "pdb.enable",           "!pdb enable",          BOTH, PDB,        handlePdbEnable,            "",             "Enable PDB symbols")
COMMAND_TABLE(9405, PDB_RESOLVE,            "pdb.resolve",          "!pdb resolve",         BOTH, PDB,        handlePdbResolve,           "",             "Resolve symbol")
COMMAND_TABLE(9410, PDB_IMPORTS,            "pdb.imports",          "!pdb imports",         BOTH, PDB,        handlePdbImports,           "",             "Show imports")
COMMAND_TABLE(9411, PDB_EXPORTS,            "pdb.exports",          "!pdb exports",         BOTH, PDB,        handlePdbExports,           "",             "Show exports")
COMMAND_TABLE(9412, PDB_IAT_STATUS,         "pdb.iatStatus",        "!pdb iat",             BOTH, PDB,        handlePdbIatStatus,         "",             "IAT status")

// ============================================================================
// AUDIT (9500-9506)
// ============================================================================
COMMAND_TABLE(9500, AUDIT_SHOW_DASHBOARD,   "audit.dashboard",      "!audit dashboard",     BOTH, AUDIT,      handleAuditShowDashboard,   "Ctrl+Shift+A", "Show audit dashboard")
COMMAND_TABLE(9501, AUDIT_RUN_FULL,         "audit.runFull",        "!audit run",           BOTH, AUDIT,      handleAuditRunFull,         "",             "Run full audit")
COMMAND_TABLE(9502, AUDIT_DETECT_STUBS,     "audit.detectStubs",    "!audit stubs",         BOTH, AUDIT,      handleAuditDetectStubs,     "",             "Detect stub functions")
COMMAND_TABLE(9503, AUDIT_CHECK_MENUS,      "audit.checkMenus",     "!audit menus",         BOTH, AUDIT,      handleAuditCheckMenus,      "",             "Check menu wiring")
COMMAND_TABLE(9504, AUDIT_RUN_TESTS,        "audit.runTests",       "!audit tests",         BOTH, AUDIT,      handleAuditRunTests,        "",             "Run test suite")
COMMAND_TABLE(9505, AUDIT_EXPORT_REPORT,    "audit.exportReport",   "!audit export",        BOTH, AUDIT,      handleAuditExportReport,    "",             "Export audit report")
COMMAND_TABLE(9506, AUDIT_QUICK_STATS,      "audit.quickStats",     "!audit stats",         BOTH, AUDIT,      handleAuditQuickStats,      "",             "Quick statistics")

// ============================================================================
// GAUNTLET (9600-9601)
// ============================================================================
COMMAND_TABLE(9600, GAUNTLET_RUN,           "gauntlet.run",         "!gauntlet run",        BOTH, GAUNTLET,   handleGauntletRun,          "",             "Run gauntlet test suite")
COMMAND_TABLE(9601, GAUNTLET_EXPORT,        "gauntlet.export",      "!gauntlet export",     BOTH, GAUNTLET,   handleGauntletExport,       "",             "Export gauntlet results")

// ============================================================================
// VOICE (9700-9709)
// ============================================================================
COMMAND_TABLE(9700, VOICE_RECORD,           "voice.record",         "!voice record",        BOTH, VOICE,      handleVoiceRecord,          "",             "Start recording")
COMMAND_TABLE(9701, VOICE_PTT,              "voice.ptt",            "!voice ptt",           BOTH, VOICE,      handleVoicePTT,             "",             "Push to talk")
COMMAND_TABLE(9702, VOICE_SPEAK,            "voice.speak",          "!voice speak",         BOTH, VOICE,      handleVoiceSpeak,           "",             "Text-to-speech")
COMMAND_TABLE(9703, VOICE_JOIN_ROOM,        "voice.joinRoom",       "!voice room",          BOTH, VOICE,      handleVoiceJoinRoom,        "",             "Join voice room")
COMMAND_TABLE(9704, VOICE_SHOW_DEVICES,     "voice.devices",        "!voice devices",       BOTH, VOICE,      handleVoiceDevices,         "",             "List audio devices")
COMMAND_TABLE(9705, VOICE_METRICS,          "voice.metrics",        "!voice metrics",       BOTH, VOICE,      handleVoiceMetrics,         "",             "Voice performance metrics")
COMMAND_TABLE(9706, VOICE_TOGGLE_PANEL,     "voice.togglePanel",    "!voice status",        BOTH, VOICE,      handleVoiceStatus,          "",             "Toggle voice panel")
COMMAND_TABLE(9707, VOICE_MODE_PTT,         "voice.modePtt",        "!voice mode ptt",      BOTH, VOICE,      handleVoiceMode,            "",             "Push-to-talk mode")
COMMAND_TABLE(9708, VOICE_MODE_CONTINUOUS,  "voice.modeContinuous", "!voice mode cont",     BOTH, VOICE,      handleVoiceModeContinuous,  "",             "Continuous mode")
COMMAND_TABLE(9709, VOICE_MODE_DISABLED,    "voice.modeDisabled",   "!voice mode off",      BOTH, VOICE,      handleVoiceModeDisabled,    "",             "Disable voice")

// ============================================================================
// QUICK WINS (9800-9830)
// ============================================================================
COMMAND_TABLE(9800, QW_SHORTCUT_EDITOR,     "qw.shortcutEditor",   "!qw shortcuts",        BOTH, QW,         handleQwShortcutEditor,     "",             "Shortcut editor")
COMMAND_TABLE(9801, QW_SHORTCUT_RESET,      "qw.shortcutReset",    "!qw shortcuts_reset",  BOTH, QW,         handleQwShortcutReset,      "",             "Reset shortcuts to default")
COMMAND_TABLE(9810, QW_BACKUP_CREATE,       "qw.backupCreate",     "!qw backup",           BOTH, QW,         handleQwBackupCreate,       "",             "Create backup")
COMMAND_TABLE(9811, QW_BACKUP_RESTORE,      "qw.backupRestore",    "!qw restore",          BOTH, QW,         handleQwBackupRestore,      "",             "Restore backup")
COMMAND_TABLE(9812, QW_BACKUP_AUTO_TOGGLE,  "qw.backupAutoToggle", "!qw auto_backup",      BOTH, QW,         handleQwBackupAutoToggle,   "",             "Toggle auto-backup")
COMMAND_TABLE(9813, QW_BACKUP_LIST,         "qw.backupList",       "!qw backup_list",      BOTH, QW,         handleQwBackupList,         "",             "List backups")
COMMAND_TABLE(9814, QW_BACKUP_PRUNE,        "qw.backupPrune",      "!qw backup_prune",     BOTH, QW,         handleQwBackupPrune,        "",             "Prune old backups")
COMMAND_TABLE(9820, QW_ALERT_TOGGLE_MONITOR,"qw.alertToggle",      "!qw alert_toggle",     BOTH, QW,         handleQwAlertToggleMonitor, "",             "Toggle resource monitor")
COMMAND_TABLE(9821, QW_ALERT_SHOW_HISTORY,  "qw.alertHistory",     "!qw alert_history",    BOTH, QW,         handleQwAlertShowHistory,   "",             "Alert history")
COMMAND_TABLE(9822, QW_ALERT_DISMISS_ALL,   "qw.alertDismissAll",  "!qw alert_dismiss",    BOTH, QW,         handleQwAlertDismissAll,    "",             "Dismiss all alerts")
COMMAND_TABLE(9823, QW_ALERT_RESOURCE_STATUS,"qw.resourceStatus",  "!qw resource_status",  BOTH, QW,         handleQwAlertResourceStatus,"",             "Resource status")
COMMAND_TABLE(9830, QW_SLO_DASHBOARD,       "qw.sloDashboard",     "!qw slo",              BOTH, QW,         handleQwSloDashboard,       "",             "SLO dashboard")

// ============================================================================
// TELEMETRY (9900-9905)
// ============================================================================
COMMAND_TABLE(9900, TELEMETRY_TOGGLE,       "telemetry.toggle",     "!telemetry toggle",    BOTH, TELEMETRY,  handleTelemetryToggle,      "",             "Toggle telemetry")
COMMAND_TABLE(9901, TELEMETRY_EXPORT_JSON,  "telemetry.exportJson", "!telemetry json",      BOTH, TELEMETRY,  handleTelemetryExportJson,  "",             "Export telemetry as JSON")
COMMAND_TABLE(9902, TELEMETRY_EXPORT_CSV,   "telemetry.exportCsv",  "!telemetry csv",       BOTH, TELEMETRY,  handleTelemetryExportCsv,   "",             "Export telemetry as CSV")
COMMAND_TABLE(9903, TELEMETRY_SHOW_DASHBOARD,"telemetry.dashboard", "!telemetry dashboard", BOTH, TELEMETRY,  handleTelemetryShowDashboard,"",            "Telemetry dashboard")
COMMAND_TABLE(9904, TELEMETRY_CLEAR,        "telemetry.clear",      "!telemetry clear",     BOTH, TELEMETRY,  handleTelemetryClear,       "",             "Clear telemetry data")
COMMAND_TABLE(9905, TELEMETRY_SNAPSHOT,     "telemetry.snapshot",   "!telemetry snapshot",  BOTH, TELEMETRY,  handleTelemetrySnapshot,    "",             "Snapshot telemetry")

// ============================================================================
// CLI-ONLY VIRTUAL COMMANDS (ID=0 — no GUI dispatch, CLI-only)
// ============================================================================
COMMAND_TABLE(0, CLI_SEARCH,                "cli.search",           "!search",              CLI,  TOOLS,      handleSearch,               "",             "Search for text across files")
COMMAND_TABLE(0, CLI_ANALYZE,               "cli.analyze",          "!analyze",             CLI,  TOOLS,      handleAnalyze,              "",             "Analyze current context")
COMMAND_TABLE(0, CLI_PROFILE,               "cli.profile",          "!profile",             CLI,  TOOLS,      handleProfile,              "",             "Performance profiling")
COMMAND_TABLE(0, CLI_SUBAGENT,              "cli.subagent",         "!subagent",            CLI,  TOOLS,      handleSubAgent,             "",             "Launch sub-agent")
COMMAND_TABLE(0, CLI_COT,                   "cli.cot",              "!cot",                 CLI,  TOOLS,      handleCOT,                  "",             "Toggle chain-of-thought")
COMMAND_TABLE(0, CLI_STATUS,                "cli.status",           "!status",              CLI,  TOOLS,      handleStatus,               "",             "Show system status")
COMMAND_TABLE(0, CLI_HELP,                  "cli.help",             "!help",                CLI,  TOOLS,      handleHelp,                 "",             "Show help text")
COMMAND_TABLE(0, CLI_GENERATE_IDE,          "cli.generateIDE",      "!generate_ide",        CLI,  TOOLS,      handleGenerateIDE,          "",             "Generate React IDE server")
COMMAND_TABLE(0, CLI_MODE,                  "cli.mode",             "!mode",                CLI,  AIMODE,     handleAIModeSet,            "",             "Set AI mode")
COMMAND_TABLE(0, CLI_ENGINE,                "cli.engine",           "!engine",              CLI,  AIMODE,     handleAIEngineSelect,       "",             "Select inference engine")
COMMAND_TABLE(0, CLI_AUTONOMY_RATE,         "cli.autonomyRate",     "!autonomy_rate",       CLI,  AUTONOMY,   handleAutonomyRate,         "",             "Configure polling rate")
COMMAND_TABLE(0, CLI_AUTONOMY_RUN,          "cli.autonomyRun",      "!autonomy_run",        CLI,  AUTONOMY,   handleAutonomyRun,          "",             "Execute decision tree")
COMMAND_TABLE(0, CLI_HOTPATCH_APPLY,        "cli.hotpatchApply",    "!hotpatch_apply",      CLI,  HOTPATCH,   handleHotpatchApply,        "",             "Apply a hotpatch preset")
COMMAND_TABLE(0, CLI_HOTPATCH_CREATE,       "cli.hotpatchCreate",   "!hotpatch_create",     CLI,  HOTPATCH,   handleHotpatchCreate,       "",             "Create new hotpatch")
COMMAND_TABLE(0, CLI_RE_AUTO_PATCH,         "cli.autoPatch",        "!auto_patch",          CLI,  REVENG,     handleREAutoPatch,          "",             "Automated binary patching")
COMMAND_TABLE(0, CLI_VOICE_INIT,            "cli.voiceInit",        "!voice init",          CLI,  VOICE,      handleVoiceInit,            "",             "Initialize voice engine")
COMMAND_TABLE(0, CLI_VOICE_TRANSCRIBE,      "cli.voiceTranscribe",  "!voice transcribe",    CLI,  VOICE,      handleVoiceTranscribe,      "",             "Transcribe recording")
COMMAND_TABLE(0, CLI_SERVER_START,          "cli.serverStart",      "!server start",        CLI,  SERVER,     handleServerStart,          "",             "Start inference server")
COMMAND_TABLE(0, CLI_SERVER_STOP,           "cli.serverStop",       "!server stop",         CLI,  SERVER,     handleServerStop,           "",             "Stop inference server")
COMMAND_TABLE(0, CLI_SERVER_STATUS,         "cli.serverStatus",     "!server status",       CLI,  SERVER,     handleServerStatus,         "",             "Server health check")
COMMAND_TABLE(0, CLI_SETTINGS_OPEN,         "cli.settingsOpen",     "!settings",            CLI,  SETTINGS,   handleSettingsOpen,         "",             "Settings panel")
COMMAND_TABLE(0, CLI_SETTINGS_EXPORT,       "cli.settingsExport",   "!settings_export",     CLI,  SETTINGS,   handleSettingsExport,       "",             "Export settings")
COMMAND_TABLE(0, CLI_SETTINGS_IMPORT,       "cli.settingsImport",   "!settings_import",     CLI,  SETTINGS,   handleSettingsImport,       "",             "Import settings")
COMMAND_TABLE(0, CLI_MANIFEST_JSON,         "cli.manifestJson",     "!manifest_json",       CLI,  MANIFEST,   handleManifestJSON,         "",             "Generate manifest as JSON")
COMMAND_TABLE(0, CLI_MANIFEST_MD,           "cli.manifestMd",       "!manifest_md",         CLI,  MANIFEST,   handleManifestMarkdown,     "",             "Generate manifest as MD")
COMMAND_TABLE(0, CLI_SELF_TEST,             "cli.selfTest",         "!self_test",           CLI,  MANIFEST,   handleManifestSelfTest,     "",             "Run feature self-tests")
COMMAND_TABLE(0, CLI_HELP_ABOUT,            "cli.about",            "!about",               CLI,  HELP,       handleHelpAbout,            "F1",           "About RawrXD IDE")
COMMAND_TABLE(0, CLI_HELP_DOCS,             "cli.docs",             "!docs",                CLI,  HELP,       handleHelpDocs,             "",             "Open documentation")
COMMAND_TABLE(0, CLI_HELP_SHORTCUTS,        "cli.shortcuts",        "!shortcuts",           CLI,  HELP,       handleHelpShortcuts,        "",             "List all shortcuts")

// ============================================================================
// END OF COMMAND TABLE
// ============================================================================

#undef COMMAND_TABLE
