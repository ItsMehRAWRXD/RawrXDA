// ============================================================================
// command_registry.hpp — Single Source of Truth (SSOT) Command Registry
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// This file IS the canonical command surface of RawrXD IDE.
// Every GUI menu item, every CLI command, every palette entry
// is generated from the COMMAND_TABLE X-macro below.
//
// To add a new command: Add ONE line to COMMAND_TABLE.
// Everything else (dispatch, CLI, help, telemetry) is automatic.
//
// To remove a command: Delete its line from COMMAND_TABLE.
//
// There is no second registry. There is no manual wiring.
// Drift is structurally impossible.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifndef RAWRXD_COMMAND_REGISTRY_HPP
#define RAWRXD_COMMAND_REGISTRY_HPP

#include <cstdint>
#include <cstring>
#include <string_view>

// ============================================================================
// COMMAND EXPOSURE — Where a command is accessible
// ============================================================================

enum class CmdExposure : uint8_t {
    GUI_ONLY  = 0x01,   // Win32 menu/toolbar only
    CLI_ONLY  = 0x02,   // Terminal/CLI only
    BOTH      = 0x03,   // GUI + CLI
    INTERNAL  = 0x04    // Internal dispatch only (not user-facing)
};

// ============================================================================
// COMMAND FLAGS — Behavioral metadata
// ============================================================================

enum CmdFlags : uint32_t {
    CMD_NONE            = 0x00,
    CMD_ASYNC           = 0x01,   // Runs off main thread (via governor)
    CMD_REQUIRES_FILE   = 0x02,   // Needs active file open
    CMD_REQUIRES_MODEL  = 0x04,   // Needs GGUF model loaded
    CMD_REQUIRES_CARET  = 0x08,   // Needs cursor position in editor
    CMD_REQUIRES_SELECT = 0x10,   // Needs text selection
    CMD_CONFIRM         = 0x20,   // Show confirmation dialog
    CMD_ASM_HOTPATH     = 0x40,   // Has x64 MASM fast-path
    CMD_REQUIRES_DEBUG  = 0x80,   // Needs active debug session
    CMD_REQUIRES_LSP    = 0x100,  // Needs LSP server running
    CMD_REQUIRES_NET    = 0x200,  // Needs network connectivity
};

// ============================================================================
// FULL DEFINITIONS for CommandContext & CommandResult
// (required by CmdDescriptor::handler function pointer and DispatchResult)
// ============================================================================
#include "shared_feature_dispatch.h"

// ============================================================================
// COMMAND TABLE — THE SINGLE SOURCE OF TRUTH
// ============================================================================
// Format: X(ID, SYMBOL, canonical_name, cli_alias, exposure, category, handler, flags)
//
// ID:         Win32 IDM_* value (0 = CLI-only, no GUI routing)
// SYMBOL:     C++ enum symbol (becomes CmdID::SYMBOL)
// canonical:  Dotted name for programmatic lookup ("file.new")
// cli_alias:  CLI command string ("!new")
// exposure:   GUI_ONLY / CLI_ONLY / BOTH
// category:   Category string for grouping
// handler:    Handler function name
// flags:      CmdFlags bitmask
// ============================================================================

#define COMMAND_TABLE(X) \
    /* ═══════════════════ FILE OPERATIONS (1001-1099) ═══════════════════ */ \
    X(1001, FILE_NEW,              "file.new",              "!new",              BOTH, "File",        handleFileNew,             CMD_NONE) \
    X(1002, FILE_OPEN,             "file.open",             "!open",             BOTH, "File",        handleFileOpen,            CMD_NONE) \
    X(1003, FILE_SAVE,             "file.save",             "!save",             BOTH, "File",        handleFileSave,            CMD_REQUIRES_FILE) \
    X(1004, FILE_SAVE_AS,          "file.saveAs",           "!save_as",          BOTH, "File",        handleFileSaveAs,          CMD_REQUIRES_FILE) \
    X(1005, FILE_SAVE_ALL,         "file.saveAll",          "!save_all",         BOTH, "File",        handleFileSaveAll,         CMD_NONE) \
    X(1006, FILE_CLOSE,            "file.close",            "!close",            BOTH, "File",        handleFileClose,           CMD_REQUIRES_FILE) \
    X(1010, FILE_RECENT,           "file.recentFiles",      "!recent",           BOTH, "File",        handleFileRecentFiles,     CMD_NONE) \
    X(1020, FILE_RECENT_CLEAR,     "file.recentClear",      "!recent_clear",     BOTH, "File",        handleFileRecentClear,     CMD_NONE) \
    X(1030, FILE_LOAD_MODEL,       "file.loadModel",        "!model_load",       BOTH, "File",        handleFileLoadModel,       CMD_NONE) \
    X(1031, FILE_MODEL_HF,         "file.modelFromHF",      "!model_hf",         BOTH, "File",        handleFileModelFromHF,     CMD_REQUIRES_NET) \
    X(1032, FILE_MODEL_OLLAMA,     "file.modelFromOllama",  "!model_ollama",     BOTH, "File",        handleFileModelFromOllama, CMD_NONE) \
    X(1033, FILE_MODEL_URL,        "file.modelFromURL",     "!model_url",        BOTH, "File",        handleFileModelFromURL,    CMD_REQUIRES_NET) \
    X(1034, FILE_MODEL_UNIFIED,    "file.modelUnified",     "!model_unified",    BOTH, "File",        handleFileUnifiedLoad,     CMD_NONE) \
    X(1035, FILE_QUICK_LOAD,       "file.quickLoad",        "!quick_load",       BOTH, "File",        handleFileQuickLoad,       CMD_NONE) \
    X(1099, FILE_EXIT,             "file.exit",             "!exit",             BOTH, "File",        handleFileExit,            CMD_NONE) \
    \
    /* ═══════════════════ EDIT OPERATIONS (2001-2019) ═══════════════════ */ \
    X(2001, EDIT_UNDO,             "edit.undo",             "!undo",             BOTH, "Edit",        handleEditUndo,            CMD_REQUIRES_FILE) \
    X(2002, EDIT_REDO,             "edit.redo",             "!redo",             BOTH, "Edit",        handleEditRedo,            CMD_REQUIRES_FILE) \
    X(2003, EDIT_CUT,              "edit.cut",              "!cut",              BOTH, "Edit",        handleEditCut,             CMD_REQUIRES_SELECT) \
    X(2004, EDIT_COPY,             "edit.copy",             "!copy",             BOTH, "Edit",        handleEditCopy,            CMD_REQUIRES_SELECT) \
    X(2005, EDIT_PASTE,            "edit.paste",            "!paste",            BOTH, "Edit",        handleEditPaste,           CMD_REQUIRES_FILE) \
    X(2006, EDIT_SELECT_ALL,       "edit.selectAll",        "!select_all",       BOTH, "Edit",        handleEditSelectAll,       CMD_REQUIRES_FILE) \
    X(2012, EDIT_SNIPPET,          "edit.snippet",          "!snippet",          BOTH, "Edit",        handleEditSnippet,         CMD_REQUIRES_FILE) \
    X(2013, EDIT_COPY_FORMAT,      "edit.copyFormat",       "!copy_format",      BOTH, "Edit",        handleEditCopyFormat,      CMD_REQUIRES_SELECT) \
    X(2014, EDIT_PASTE_PLAIN,      "edit.pastePlain",       "!paste_plain",      BOTH, "Edit",        handleEditPastePlain,      CMD_REQUIRES_FILE) \
    X(2015, EDIT_CLIPBOARD_HIST,   "edit.clipboardHistory", "!clipboard",        BOTH, "Edit",        handleEditClipboardHist,   CMD_NONE) \
    X(2016, EDIT_FIND,             "edit.find",             "!find",             BOTH, "Edit",        handleEditFind,            CMD_NONE) \
    X(2017, EDIT_REPLACE,          "edit.replace",          "!replace",          BOTH, "Edit",        handleEditReplace,         CMD_NONE) \
    X(2018, EDIT_FIND_NEXT,        "edit.findNext",         "!find_next",        BOTH, "Edit",        handleEditFindNext,        CMD_NONE) \
    X(2019, EDIT_FIND_PREV,        "edit.findPrev",         "!find_prev",        BOTH, "Edit",        handleEditFindPrev,        CMD_NONE) \
    \
    /* ═══════════════════ VIEW OPERATIONS (2020-2029) ═══════════════════ */ \
    X(2020, VIEW_MINIMAP,          "view.minimap",          "!minimap",          BOTH, "View",        handleViewMinimap,         CMD_NONE) \
    X(2021, VIEW_OUTPUT_TABS,      "view.outputTabs",       "!output_tabs",      BOTH, "View",        handleViewOutputTabs,      CMD_NONE) \
    X(2022, VIEW_MODULE_BROWSER,   "view.moduleBrowser",    "!modules",          BOTH, "View",        handleViewModuleBrowser,   CMD_NONE) \
    X(2023, VIEW_THEME_EDITOR,     "view.themeEditor",      "!theme_editor",     BOTH, "View",        handleViewThemeEditor,     CMD_NONE) \
    X(2024, VIEW_FLOATING_PANEL,   "view.floatingPanel",    "!float_panel",      BOTH, "View",        handleViewFloatingPanel,   CMD_NONE) \
    X(2025, VIEW_OUTPUT_PANEL,     "view.outputPanel",      "!output",           BOTH, "View",        handleViewOutputPanel,     CMD_NONE) \
    X(2026, VIEW_STREAMING_LOADER, "view.streamingLoader",  "!streaming",        BOTH, "View",        handleViewStreamingLoader, CMD_NONE) \
    X(2027, VIEW_VULKAN_RENDERER,  "view.vulkanRenderer",   "!vulkan",           BOTH, "View",        handleViewVulkanRenderer,  CMD_NONE) \
    X(2028, VIEW_SIDEBAR,          "view.sidebar",          "!sidebar",          BOTH, "View",        handleViewSidebar,         CMD_NONE) \
    X(2029, VIEW_TERMINAL,         "view.terminal",         "!view_terminal",    BOTH, "View",        handleViewTerminal,        CMD_NONE) \
    \
    /* ═══════════════════ GIT (3020-3024) ═══════════════════ */ \
    X(3020, GIT_STATUS,            "git.status",            "!git_status",       BOTH, "Git",         handleGitStatus,           CMD_NONE) \
    X(3021, GIT_COMMIT,            "git.commit",            "!git_commit",       BOTH, "Git",         handleGitCommit,           CMD_NONE) \
    X(3022, GIT_PUSH,              "git.push",              "!git_push",         BOTH, "Git",         handleGitPush,             CMD_REQUIRES_NET) \
    X(3023, GIT_PULL,              "git.pull",              "!git_pull",         BOTH, "Git",         handleGitPull,             CMD_REQUIRES_NET) \
    X(3024, GIT_PANEL,             "git.diff",              "!git_diff",         BOTH, "Git",         handleGitDiff,             CMD_NONE) \
    \
    /* ═══════════════════ THEMES (3100-3117) ═══════════════════ */ \
    X(3100, THEME_BASE,            "theme.base",            "!theme_list",       BOTH, "Theme",       handleThemeList,            CMD_NONE) \
    X(3101, THEME_DARK_PLUS,       "theme.darkPlus",        "!theme dark+",      BOTH, "Theme",       handleThemeSet,             CMD_NONE) \
    X(3102, THEME_LIGHT_PLUS,      "theme.lightPlus",       "!theme light+",     BOTH, "Theme",       handleThemeLightPlus,      CMD_NONE) \
    X(3103, THEME_MONOKAI,         "theme.monokai",         "!theme monokai",    BOTH, "Theme",       handleThemeMonokai,        CMD_NONE) \
    X(3104, THEME_DRACULA,         "theme.dracula",         "!theme dracula",    BOTH, "Theme",       handleThemeDracula,        CMD_NONE) \
    X(3105, THEME_NORD,            "theme.nord",            "!theme nord",       BOTH, "Theme",       handleThemeNord,           CMD_NONE) \
    X(3106, THEME_SOL_DARK,        "theme.solarizedDark",   "!theme sol-dark",   BOTH, "Theme",       handleThemeSolDark,        CMD_NONE) \
    X(3107, THEME_SOL_LIGHT,       "theme.solarizedLight",  "!theme sol-light",  BOTH, "Theme",       handleThemeSolLight,       CMD_NONE) \
    X(3108, THEME_CYBERPUNK,       "theme.cyberpunk",       "!theme cyberpunk",  BOTH, "Theme",       handleThemeCyberpunk,      CMD_NONE) \
    X(3109, THEME_GRUVBOX,         "theme.gruvbox",         "!theme gruvbox",    BOTH, "Theme",       handleThemeGruvbox,        CMD_NONE) \
    X(3110, THEME_CATPPUCCIN,      "theme.catppuccin",      "!theme catppuccin", BOTH, "Theme",       handleThemeCatppuccin,     CMD_NONE) \
    X(3111, THEME_TOKYO,           "theme.tokyoNight",      "!theme tokyo",      BOTH, "Theme",       handleThemeTokyo,          CMD_NONE) \
    X(3112, THEME_CRIMSON,         "theme.rawrxdCrimson",   "!theme crimson",    BOTH, "Theme",       handleThemeCrimson,        CMD_NONE) \
    X(3113, THEME_HIGH_CONTRAST,   "theme.highContrast",    "!theme hc",         BOTH, "Theme",       handleThemeHighContrast,   CMD_NONE) \
    X(3114, THEME_ONE_DARK,        "theme.oneDarkPro",      "!theme onedark",    BOTH, "Theme",       handleThemeOneDark,        CMD_NONE) \
    X(3115, THEME_SYNTHWAVE,       "theme.synthwave84",     "!theme synthwave",  BOTH, "Theme",       handleThemeSynthwave,      CMD_NONE) \
    X(3116, THEME_ABYSS,           "theme.abyss",           "!theme abyss",      BOTH, "Theme",       handleThemeAbyss,          CMD_NONE) \
    \
    /* ═══════════════════ TRANSPARENCY (3200-3211) ═══════════════════ */ \
    X(3200, TRANS_100,             "view.transparency100",  "!opacity 100",      BOTH, "Transparency", handleTrans100,            CMD_NONE) \
    X(3201, TRANS_90,              "view.transparency90",   "!opacity 90",       BOTH, "Transparency", handleTrans90,             CMD_NONE) \
    X(3202, TRANS_80,              "view.transparency80",   "!opacity 80",       BOTH, "Transparency", handleTrans80,             CMD_NONE) \
    X(3203, TRANS_70,              "view.transparency70",   "!opacity 70",       BOTH, "Transparency", handleTrans70,             CMD_NONE) \
    X(3204, TRANS_60,              "view.transparency60",   "!opacity 60",       BOTH, "Transparency", handleTrans60,             CMD_NONE) \
    X(3205, TRANS_50,              "view.transparency50",   "!opacity 50",       BOTH, "Transparency", handleTrans50,             CMD_NONE) \
    X(3206, TRANS_40,              "view.transparency40",   "!opacity 40",       BOTH, "Transparency", handleTrans40,             CMD_NONE) \
    X(3210, TRANS_CUSTOM,          "view.transparencySet",  "!opacity set",      BOTH, "Transparency", handleTransCustom,         CMD_NONE) \
    X(3211, TRANS_TOGGLE,          "view.transparencyToggle","!opacity toggle",   BOTH, "Transparency", handleTransToggle,         CMD_NONE) \
    \
    /* ═══════════════════ HELP (legacy 4001-4004) ═══════════════════ */ \
    X(4001, HELP_ABOUT,            "help.about",            "!about",            BOTH, "Help",        handleHelpAbout,           CMD_NONE) \
    X(4002, HELP_CMDREF,           "help.cmdref",           "!cmdref",           BOTH, "Help",        handleHelpCmdRef,          CMD_NONE) \
    X(4003, HELP_PSDOCS,           "help.psdocs",           "!psdocs",           BOTH, "Help",        handleHelpPsDocs,          CMD_NONE) \
    X(4004, HELP_SEARCH,           "help.search",           "!help_search",      BOTH, "Help",        handleHelpSearch,          CMD_NONE) \
    \
    /* ═══════════════════ TERMINAL (4006-4010) ═══════════════════ */ \
    X(4006, TERMINAL_KILL,         "terminal.kill",         "!terminal_kill",    BOTH, "Terminal",    handleTerminalKill,        CMD_NONE) \
    X(4007, TERMINAL_SPLIT_H,      "terminal.splitH",       "!terminal_split",   BOTH, "Terminal",    handleTerminalSplitH,      CMD_NONE) \
    X(4008, TERMINAL_SPLIT_V,      "terminal.splitV",       "!terminal_split_v", BOTH, "Terminal",    handleTerminalSplitV,      CMD_NONE) \
    X(4009, TERMINAL_SPLIT_CODE,   "terminal.splitCode",    "!terminal_code",    BOTH, "Terminal",    handleTerminalSplitCode,   CMD_NONE) \
    X(4010, TERMINAL_LIST,         "terminal.list",         "!terminal_list",    BOTH, "Terminal",    handleTerminalList,        CMD_NONE) \
    \
    /* ═══════════════════ AGENT (4100-4121) ═══════════════════ */ \
    X(4100, AGENT_START_LOOP,      "agent.loop",            "!agent_loop",       BOTH, "Agent",       handleAgentLoop,           CMD_ASYNC) \
    X(4101, AGENT_EXECUTE_CMD,     "agent.execute",         "!agent_execute",    BOTH, "Agent",       handleAgentExecute,        CMD_ASYNC) \
    X(4102, AGENT_CONFIGURE,       "agent.configure",       "!agent_config",     BOTH, "Agent",       handleAgentConfigure,      CMD_NONE) \
    X(4103, AGENT_VIEW_TOOLS,      "agent.viewTools",       "!tools",            BOTH, "Agent",       handleAgentViewTools,      CMD_NONE) \
    X(4104, AGENT_VIEW_STATUS,     "agent.viewStatus",      "!agent_status",     BOTH, "Agent",       handleAgentViewStatus,     CMD_NONE) \
    X(4105, AGENT_STOP,            "agent.stop",            "!agent_stop",       BOTH, "Agent",       handleAgentStop,           CMD_NONE) \
    X(4106, AGENT_MEMORY,          "agent.memory",          "!agent_memory",     BOTH, "Agent",       handleAgentMemory,         CMD_NONE) \
    X(4107, AGENT_MEMORY_VIEW,     "agent.memoryView",      "!agent_memory_view",BOTH, "Agent",       handleAgentMemoryView,     CMD_NONE) \
    X(4108, AGENT_MEMORY_CLEAR,    "agent.memoryClear",     "!agent_memory_clear",BOTH,"Agent",       handleAgentMemoryClear,    CMD_CONFIRM) \
    X(4109, AGENT_MEMORY_EXPORT,   "agent.memoryExport",    "!agent_memory_export",BOTH,"Agent",      handleAgentMemoryExport,   CMD_NONE) \
    X(4110, SUBAGENT_CHAIN,        "subagent.chain",        "!chain",            BOTH, "SubAgent",    handleSubAgentChain,       CMD_ASYNC) \
    X(4111, SUBAGENT_SWARM,        "subagent.swarm",        "!swarm",            BOTH, "SubAgent",    handleSubAgentSwarm,       CMD_ASYNC) \
    X(4112, SUBAGENT_TODO_LIST,    "subagent.todoList",     "!todo",             BOTH, "SubAgent",    handleSubAgentTodoList,    CMD_NONE) \
    X(4113, SUBAGENT_TODO_CLEAR,   "subagent.todoClear",    "!todo_clear",       BOTH, "SubAgent",    handleSubAgentTodoClear,   CMD_CONFIRM) \
    X(4114, SUBAGENT_STATUS,       "subagent.status",       "!agents",           BOTH, "SubAgent",    handleSubAgentStatus,      CMD_NONE) \
    X(4120, AGENT_BOUNDED_LOOP,    "agent.boundedLoop",     "!agent_bounded",    BOTH, "Agent",       handleAgentBoundedLoop,    CMD_ASYNC) \
    \
    /* ═══════════════════ AUTONOMY (4150-4155) ═══════════════════ */ \
    X(4150, AUTONOMY_TOGGLE,       "autonomy.toggle",       "!autonomy_toggle",  BOTH, "Autonomy",   handleAutonomyToggle,      CMD_NONE) \
    X(4151, AUTONOMY_START,        "autonomy.start",        "!autonomy_start",   BOTH, "Autonomy",   handleAutonomyStart,       CMD_ASYNC) \
    X(4152, AUTONOMY_STOP,         "autonomy.stop",         "!autonomy_stop",    BOTH, "Autonomy",   handleAutonomyStop,        CMD_NONE) \
    X(4153, AUTONOMY_SET_GOAL,     "autonomy.goal",         "!autonomy_goal",    BOTH, "Autonomy",   handleAutonomyGoal,        CMD_NONE) \
    X(4154, AUTONOMY_STATUS,       "autonomy.status",       "!autonomy_status",  BOTH, "Autonomy",   handleAutonomyStatus,      CMD_NONE) \
    X(4155, AUTONOMY_MEMORY,       "autonomy.memory",       "!autonomy_memory",  BOTH, "Autonomy",   handleAutonomyMemory,      CMD_NONE) \
    \
    /* ═══════════════════ AI MODE (4200-4203) ═══════════════════ */ \
    X(4200, AI_MODE_MAX,           "ai.maxMode",            "!max",              BOTH, "AIMode",     handleAIMaxMode,           CMD_NONE) \
    X(4201, AI_MODE_DEEP_THINK,    "ai.deepThinking",       "!deep",             BOTH, "AIMode",     handleAIDeepThinking,      CMD_NONE) \
    X(4202, AI_MODE_DEEP_RESEARCH, "ai.deepResearch",       "!research",         BOTH, "AIMode",     handleAIDeepResearch,      CMD_NONE) \
    X(4203, AI_MODE_NO_REFUSAL,    "ai.noRefusal",          "!no_refusal",       BOTH, "AIMode",     handleAINoRefusal,         CMD_NONE) \
    \
    /* ═══════════════════ AI CONTEXT WINDOW (4210-4216) ═══════════════════ */ \
    X(4210, AI_CTX_4K,             "ai.context4k",          "!ctx 4k",           BOTH, "AIContext",  handleAICtx4K,             CMD_NONE) \
    X(4211, AI_CTX_32K,            "ai.context32k",         "!ctx 32k",          BOTH, "AIContext",  handleAICtx32K,            CMD_NONE) \
    X(4212, AI_CTX_64K,            "ai.context64k",         "!ctx 64k",          BOTH, "AIContext",  handleAICtx64K,            CMD_NONE) \
    X(4213, AI_CTX_128K,           "ai.context128k",        "!ctx 128k",         BOTH, "AIContext",  handleAICtx128K,           CMD_NONE) \
    X(4214, AI_CTX_256K,           "ai.context256k",        "!ctx 256k",         BOTH, "AIContext",  handleAICtx256K,           CMD_NONE) \
    X(4215, AI_CTX_512K,           "ai.context512k",        "!ctx 512k",         BOTH, "AIContext",  handleAICtx512K,           CMD_NONE) \
    X(4216, AI_CTX_1M,             "ai.context1m",          "!ctx 1m",           BOTH, "AIContext",  handleAICtx1M,             CMD_NONE) \
    \
    /* ═══════════════════ REVERSE ENGINEERING (4300-4319) ═══════════════════ */ \
    X(4300, REVENG_ANALYZE,        "re.analyze",            "!decision_tree",    BOTH, "ReverseEng", handleREDecisionTree,      CMD_REQUIRES_FILE) \
    X(4301, REVENG_DISASM,         "re.disassemble",        "!disasm",           BOTH, "ReverseEng", handleREDisassemble,       CMD_REQUIRES_FILE) \
    X(4302, REVENG_DUMPBIN,        "re.dumpbin",            "!dumpbin",          BOTH, "ReverseEng", handleREDumpbin,           CMD_REQUIRES_FILE) \
    X(4303, REVENG_COMPILE,        "re.compile",            "!re_compile",       BOTH, "ReverseEng", handleRECompile,           CMD_REQUIRES_FILE) \
    X(4304, REVENG_COMPARE,        "re.compare",            "!re_compare",       BOTH, "ReverseEng", handleRECompare,           CMD_REQUIRES_FILE) \
    X(4305, REVENG_DETECT_VULNS,   "re.detectVulns",        "!re_vulns",         BOTH, "ReverseEng", handleREDetectVulns,       CMD_REQUIRES_FILE | CMD_ASYNC) \
    X(4306, REVENG_EXPORT_IDA,     "re.exportIDA",          "!re_ida",           BOTH, "ReverseEng", handleREExportIDA,         CMD_REQUIRES_FILE) \
    X(4307, REVENG_EXPORT_GHIDRA,  "re.exportGhidra",       "!re_ghidra",        BOTH, "ReverseEng", handleREExportGhidra,      CMD_REQUIRES_FILE) \
    X(4308, REVENG_CFG,            "re.cfgAnalysis",        "!cfg",              BOTH, "ReverseEng", handleRECFGAnalysis,       CMD_REQUIRES_FILE) \
    X(4309, REVENG_FUNCTIONS,      "re.functions",          "!re_funcs",         BOTH, "ReverseEng", handleREFunctions,         CMD_REQUIRES_FILE) \
    X(4310, REVENG_DEMANGLE,       "re.demangle",           "!demangle",         BOTH, "ReverseEng", handleREDemangle,          CMD_REQUIRES_FILE) \
    X(4311, REVENG_SSA,            "re.ssaLift",            "!ssa_lift",         BOTH, "ReverseEng", handleRESSALift,           CMD_REQUIRES_FILE | CMD_ASM_HOTPATH) \
    X(4312, REVENG_RECURSIVE,      "re.recursiveDisasm",    "!re_recursive",     BOTH, "ReverseEng", handleRERecursiveDisasm,   CMD_REQUIRES_FILE | CMD_ASYNC) \
    X(4313, REVENG_TYPE_RECOVERY,  "re.typeRecovery",       "!re_types",         BOTH, "ReverseEng", handleRETypeRecovery,      CMD_REQUIRES_FILE | CMD_ASYNC) \
    X(4314, REVENG_DATA_FLOW,      "re.dataFlow",           "!re_dataflow",      BOTH, "ReverseEng", handleREDataFlow,          CMD_REQUIRES_FILE) \
    X(4315, REVENG_LICENSE_INFO,   "re.licenseInfo",        "!re_license",       BOTH, "ReverseEng", handleRELicenseInfo,       CMD_REQUIRES_FILE) \
    X(4316, REVENG_DECOMPILER,     "re.decompilerView",     "!decompile",        BOTH, "ReverseEng", handleREDecompilerView,    CMD_REQUIRES_FILE) \
    X(4317, REVENG_DECOMP_RENAME,  "re.decompRename",       "!decomp_rename",    BOTH, "ReverseEng", handleREDecompRename,      CMD_REQUIRES_FILE | CMD_REQUIRES_CARET) \
    X(4318, REVENG_DECOMP_SYNC,    "re.decompSync",         "!decomp_sync",      BOTH, "ReverseEng", handleREDecompSync,        CMD_REQUIRES_FILE) \
    X(4319, REVENG_DECOMP_CLOSE,   "re.decompClose",        "!decomp_close",     BOTH, "ReverseEng", handleREDecompClose,       CMD_NONE) \
    \
    /* ═══════════════════ BACKEND SWITCHER (5037-5047) ═══════════════════ */ \
    X(5037, BACKEND_LOCAL,         "backend.switchLocal",   "!backend local",    BOTH, "Backend",    handleBackendSwitchLocal,   CMD_NONE) \
    X(5038, BACKEND_OLLAMA,        "backend.switchOllama",  "!backend ollama",   BOTH, "Backend",    handleBackendSwitchOllama,  CMD_NONE) \
    X(5039, BACKEND_OPENAI,        "backend.switchOpenAI",  "!backend openai",   BOTH, "Backend",    handleBackendSwitchOpenAI,  CMD_REQUIRES_NET) \
    X(5040, BACKEND_CLAUDE,        "backend.switchClaude",  "!backend claude",   BOTH, "Backend",    handleBackendSwitchClaude,  CMD_REQUIRES_NET) \
    X(5041, BACKEND_GEMINI,        "backend.switchGemini",  "!backend gemini",   BOTH, "Backend",    handleBackendSwitchGemini,  CMD_REQUIRES_NET) \
    X(5042, BACKEND_STATUS,        "backend.status",        "!backend status",   BOTH, "Backend",    handleBackendShowStatus,    CMD_NONE) \
    X(5043, BACKEND_SWITCHER,      "backend.switcher",      "!backend list",     BOTH, "Backend",    handleBackendShowSwitcher,  CMD_NONE) \
    X(5044, BACKEND_CONFIGURE,     "backend.configure",     "!backend config",   BOTH, "Backend",    handleBackendConfigure,     CMD_NONE) \
    X(5045, BACKEND_HEALTH,        "backend.healthCheck",   "!backend health",   BOTH, "Backend",    handleBackendHealthCheck,   CMD_ASYNC) \
    X(5046, BACKEND_API_KEY,       "backend.setApiKey",     "!backend apikey",   BOTH, "Backend",    handleBackendSetApiKey,     CMD_NONE) \
    X(5047, BACKEND_SAVE_CFG,      "backend.saveConfigs",   "!backend save",     BOTH, "Backend",    handleBackendSaveConfigs,   CMD_NONE) \
    \
    /* ═══════════════════ ROUTER (5048-5081) ═══════════════════ */ \
    X(5048, ROUTER_ENABLE,         "router.enable",         "!router enable",    BOTH, "Router",     handleRouterEnable,         CMD_NONE) \
    X(5049, ROUTER_DISABLE,        "router.disable",        "!router disable",   BOTH, "Router",     handleRouterDisable,        CMD_NONE) \
    X(5050, ROUTER_STATUS,         "router.status",         "!router status",    BOTH, "Router",     handleRouterStatus,         CMD_NONE) \
    X(5051, ROUTER_DECISION,       "router.decision",       "!router decision",  BOTH, "Router",     handleRouterDecision,       CMD_NONE) \
    X(5052, ROUTER_SET_POLICY,     "router.setPolicy",      "!router policy",    BOTH, "Router",     handleRouterSetPolicy,      CMD_NONE) \
    X(5053, ROUTER_CAPABILITIES,   "router.capabilities",   "!router caps",      BOTH, "Router",     handleRouterCapabilities,   CMD_NONE) \
    X(5054, ROUTER_FALLBACKS,      "router.fallbacks",      "!router fallbacks", BOTH, "Router",     handleRouterFallbacks,      CMD_NONE) \
    X(5055, ROUTER_SAVE_CONFIG,    "router.saveConfig",     "!router save",      BOTH, "Router",     handleRouterSaveConfig,     CMD_NONE) \
    X(5056, ROUTER_ROUTE_PROMPT,   "router.routePrompt",    "!router route",     BOTH, "Router",     handleRouterRoutePrompt,    CMD_ASYNC) \
    X(5057, ROUTER_RESET_STATS,    "router.resetStats",     "!router reset",     BOTH, "Router",     handleRouterResetStats,     CMD_CONFIRM) \
    X(5071, ROUTER_WHY_BACKEND,    "router.whyBackend",     "!router why",       BOTH, "Router",     handleRouterWhyBackend,     CMD_NONE) \
    X(5072, ROUTER_PIN_TASK,       "router.pinTask",        "!router pin",       BOTH, "Router",     handleRouterPinTask,        CMD_NONE) \
    X(5073, ROUTER_UNPIN_TASK,     "router.unpinTask",      "!router unpin",     BOTH, "Router",     handleRouterUnpinTask,      CMD_NONE) \
    X(5074, ROUTER_SHOW_PINS,      "router.showPins",       "!router pins",      BOTH, "Router",     handleRouterShowPins,       CMD_NONE) \
    X(5075, ROUTER_HEATMAP,        "router.heatmap",        "!router heatmap",   BOTH, "Router",     handleRouterShowHeatmap,    CMD_NONE) \
    X(5076, ROUTER_ENSEMBLE_ON,    "router.ensembleEnable",  "!router ensemble",  BOTH, "Router",     handleRouterEnsembleEnable, CMD_NONE) \
    X(5077, ROUTER_ENSEMBLE_OFF,   "router.ensembleDisable", "!router noensemble",BOTH, "Router",     handleRouterEnsembleDisable,CMD_NONE) \
    X(5078, ROUTER_ENSEMBLE_STAT,  "router.ensembleStatus",  "!router ens_stat",  BOTH, "Router",     handleRouterEnsembleStatus, CMD_NONE) \
    X(5079, ROUTER_SIMULATE,       "router.simulate",        "!router simulate",  BOTH, "Router",     handleRouterSimulate,       CMD_ASYNC) \
    X(5080, ROUTER_SIMULATE_LAST,  "router.simulateLast",    "!router sim_last",  BOTH, "Router",     handleRouterSimulateLast,   CMD_NONE) \
    X(5081, ROUTER_COST_STATS,     "router.costStats",       "!router cost",      BOTH, "Router",     handleRouterShowCostStats,  CMD_NONE) \
    \
    /* ═══════════════════ LSP CLIENT (5058-5070) ═══════════════════ */ \
    X(5058, LSP_START_ALL,         "lsp.startAll",          "!lsp start",        BOTH, "LSP",        handleLspStartAll,          CMD_ASYNC) \
    X(5059, LSP_STOP_ALL,          "lsp.stopAll",           "!lsp stop",         BOTH, "LSP",        handleLspStopAll,           CMD_NONE) \
    X(5060, LSP_STATUS,            "lsp.status",            "!lsp status",       BOTH, "LSP",        handleLspStatus,            CMD_NONE) \
    X(5061, LSP_GOTO_DEF,          "lsp.gotoDef",           "!lsp goto",         BOTH, "LSP",        handleLspGotoDef,           CMD_REQUIRES_CARET) \
    X(5062, LSP_FIND_REFS,         "lsp.findRefs",          "!lsp refs",         BOTH, "LSP",        handleLspFindRefs,          CMD_REQUIRES_CARET) \
    X(5063, LSP_RENAME,            "lsp.rename",            "!lsp rename",       BOTH, "LSP",        handleLspRename,            CMD_REQUIRES_CARET) \
    X(5064, LSP_HOVER,             "lsp.hover",             "!lsp hover",        BOTH, "LSP",        handleLspHover,             CMD_REQUIRES_CARET) \
    X(5065, LSP_DIAGNOSTICS,       "lsp.diagnostics",       "!lsp diag",         BOTH, "LSP",        handleLspDiagnostics,       CMD_NONE) \
    X(5066, LSP_RESTART,           "lsp.restart",           "!lsp restart",      BOTH, "LSP",        handleLspRestart,           CMD_NONE) \
    X(5067, LSP_CLEAR_DIAG,        "lsp.clearDiag",         "!lsp clear",        BOTH, "LSP",        handleLspClearDiag,         CMD_NONE) \
    X(5068, LSP_SYMBOL_INFO,       "lsp.symbolInfo",        "!lsp symbol",       BOTH, "LSP",        handleLspSymbolInfo,        CMD_REQUIRES_CARET) \
    X(5069, LSP_CONFIGURE,         "lsp.configure",         "!lsp config",       BOTH, "LSP",        handleLspConfigure,         CMD_NONE) \
    X(5070, LSP_SAVE_CONFIG,       "lsp.saveConfig",        "!lsp save",         BOTH, "LSP",        handleLspSaveConfig,        CMD_NONE) \
    \
    /* ═══════════════════ ASM SEMANTIC (5082-5093) ═══════════════════ */ \
    X(5082, ASM_PARSE_SYMBOLS,     "asm.parseSymbols",      "!asm parse",        BOTH, "ASM",        handleAsmParse,             CMD_REQUIRES_FILE | CMD_ASM_HOTPATH) \
    X(5083, ASM_GOTO_LABEL,        "asm.gotoLabel",         "!asm goto",         BOTH, "ASM",        handleAsmGoto,              CMD_REQUIRES_FILE | CMD_REQUIRES_CARET) \
    X(5084, ASM_FIND_LABEL_REFS,   "asm.findLabelRefs",     "!asm refs",         BOTH, "ASM",        handleAsmFindRefs,          CMD_REQUIRES_FILE | CMD_REQUIRES_CARET) \
    X(5085, ASM_SYMBOL_TABLE,      "asm.symbolTable",       "!asm symbols",      BOTH, "ASM",        handleAsmSymbolTable,       CMD_REQUIRES_FILE) \
    X(5086, ASM_INSTRUCTION_INFO,  "asm.instructionInfo",   "!asm info",         BOTH, "ASM",        handleAsmInstructionInfo,   CMD_REQUIRES_FILE | CMD_REQUIRES_CARET) \
    X(5087, ASM_REGISTER_INFO,     "asm.registerInfo",      "!asm reg",          BOTH, "ASM",        handleAsmRegisterInfo,      CMD_REQUIRES_FILE | CMD_REQUIRES_CARET) \
    X(5088, ASM_ANALYZE_BLOCK,     "asm.analyzeBlock",      "!asm block",        BOTH, "ASM",        handleAsmAnalyzeBlock,      CMD_REQUIRES_FILE | CMD_REQUIRES_CARET) \
    X(5089, ASM_CALL_GRAPH,        "asm.callGraph",         "!asm callgraph",    BOTH, "ASM",        handleAsmCallGraph,         CMD_REQUIRES_FILE | CMD_ASYNC) \
    X(5090, ASM_DATA_FLOW,         "asm.dataFlow",          "!asm dataflow",     BOTH, "ASM",        handleAsmDataFlow,          CMD_REQUIRES_FILE | CMD_ASYNC) \
    X(5091, ASM_DETECT_CONVENTION, "asm.detectConvention",  "!asm convention",   BOTH, "ASM",        handleAsmDetectConvention,  CMD_REQUIRES_FILE) \
    X(5092, ASM_SECTIONS,          "asm.showSections",      "!asm sections",     BOTH, "ASM",        handleAsmSections,          CMD_REQUIRES_FILE) \
    X(5093, ASM_CLEAR_SYMBOLS,     "asm.clearSymbols",      "!asm clear",        BOTH, "ASM",        handleAsmClearSymbols,      CMD_CONFIRM) \
    \
    /* ═══════════════════ HYBRID LSP-AI (5094-5105) ═══════════════════ */ \
    X(5094, HYBRID_COMPLETE,       "hybrid.complete",       "!hybrid complete",  BOTH, "Hybrid",     handleHybridComplete,       CMD_REQUIRES_CARET | CMD_REQUIRES_LSP) \
    X(5095, HYBRID_DIAGNOSTICS,    "hybrid.diagnostics",    "!hybrid diag",      BOTH, "Hybrid",     handleHybridDiagnostics,    CMD_REQUIRES_LSP) \
    X(5096, HYBRID_SMART_RENAME,   "hybrid.smartRename",    "!hybrid rename",    BOTH, "Hybrid",     handleHybridSmartRename,    CMD_REQUIRES_CARET | CMD_REQUIRES_LSP) \
    X(5097, HYBRID_ANALYZE_FILE,   "hybrid.analyzeFile",    "!hybrid analyze",   BOTH, "Hybrid",     handleHybridAnalyzeFile,    CMD_REQUIRES_FILE | CMD_ASYNC) \
    X(5098, HYBRID_AUTO_PROFILE,   "hybrid.autoProfile",    "!hybrid profile",   BOTH, "Hybrid",     handleHybridAutoProfile,    CMD_REQUIRES_FILE | CMD_REQUIRES_DEBUG) \
    X(5099, HYBRID_STATUS,         "hybrid.status",         "!hybrid status",    BOTH, "Hybrid",     handleHybridStatus,         CMD_NONE) \
    X(5100, HYBRID_SYMBOL_USAGE,   "hybrid.symbolUsage",    "!hybrid usage",     BOTH, "Hybrid",     handleHybridSymbolUsage,    CMD_REQUIRES_CARET | CMD_REQUIRES_LSP) \
    X(5101, HYBRID_EXPLAIN_SYMBOL, "hybrid.explainSymbol",  "!hybrid explain",   BOTH, "Hybrid",     handleHybridExplainSymbol,  CMD_REQUIRES_CARET) \
    X(5102, HYBRID_ANNOTATE_DIAG,  "hybrid.annotateDiag",   "!hybrid annotate",  BOTH, "Hybrid",     handleHybridAnnotateDiag,   CMD_REQUIRES_LSP) \
    X(5103, HYBRID_STREAM_ANALYZE, "hybrid.streamAnalyze",  "!hybrid stream",    BOTH, "Hybrid",     handleHybridStreamAnalyze,  CMD_ASYNC) \
    X(5104, HYBRID_SEM_PREFETCH,   "hybrid.semanticPrefetch","!hybrid prefetch",  BOTH, "Hybrid",     handleHybridSemanticPrefetch,CMD_ASYNC) \
    X(5105, HYBRID_CORRECTION_LOOP,"hybrid.correctionLoop", "!hybrid correct",   BOTH, "Hybrid",     handleHybridCorrectionLoop, CMD_ASYNC) \
    \
    /* ═══════════════════ MULTI-RESPONSE (5106-5117) ═══════════════════ */ \
    X(5106, MULTI_GENERATE,        "multi.generate",        "!multi generate",   BOTH, "MultiResp",  handleMultiRespGenerate,    CMD_ASYNC) \
    X(5107, MULTI_SET_MAX,         "multi.setMax",          "!multi setmax",     BOTH, "MultiResp",  handleMultiRespSetMax,      CMD_NONE) \
    X(5108, MULTI_SELECT_PREF,     "multi.selectPreferred", "!multi select",     BOTH, "MultiResp",  handleMultiRespSelectPreferred, CMD_NONE) \
    X(5109, MULTI_COMPARE,         "multi.compare",         "!multi compare",    BOTH, "MultiResp",  handleMultiRespCompare,     CMD_NONE) \
    X(5110, MULTI_STATS,           "multi.stats",           "!multi stats",      BOTH, "MultiResp",  handleMultiRespShowStats,   CMD_NONE) \
    X(5111, MULTI_TEMPLATES,       "multi.templates",       "!multi templates",  BOTH, "MultiResp",  handleMultiRespShowTemplates,CMD_NONE) \
    X(5112, MULTI_TOGGLE_TMPL,     "multi.toggleTemplate",  "!multi toggle",     BOTH, "MultiResp",  handleMultiRespToggleTemplate,CMD_NONE) \
    X(5113, MULTI_PREFS,           "multi.prefs",           "!multi prefs",      BOTH, "MultiResp",  handleMultiRespShowPrefs,   CMD_NONE) \
    X(5114, MULTI_LATEST,          "multi.latest",          "!multi latest",     BOTH, "MultiResp",  handleMultiRespShowLatest,  CMD_NONE) \
    X(5115, MULTI_STATUS,          "multi.status",          "!multi status",     BOTH, "MultiResp",  handleMultiRespShowStatus,  CMD_NONE) \
    X(5116, MULTI_CLEAR_HIST,      "multi.clearHistory",    "!multi clear",      BOTH, "MultiResp",  handleMultiRespClearHistory,CMD_CONFIRM) \
    X(5117, MULTI_APPLY_PREF,      "multi.applyPreferred",  "!multi apply",      BOTH, "MultiResp",  handleMultiRespApplyPreferred,CMD_NONE) \
    \
    /* ═══════════════════ GOVERNOR (5118-5121) ═══════════════════ */ \
    X(5118, GOV_STATUS,            "gov.status",            "!gov status",       BOTH, "Governor",   handleGovStatus,            CMD_NONE) \
    X(5119, GOV_SUBMIT,            "gov.submit",            "!gov submit",       BOTH, "Governor",   handleGovSubmitCommand,     CMD_ASYNC) \
    X(5120, GOV_KILL_ALL,          "gov.killAll",           "!gov killall",      BOTH, "Governor",   handleGovKillAll,           CMD_CONFIRM) \
    X(5121, GOV_TASK_LIST,         "gov.taskList",          "!gov tasks",        BOTH, "Governor",   handleGovTaskList,          CMD_NONE) \
    \
    /* ═══════════════════ SAFETY (5122-5125) ═══════════════════ */ \
    X(5122, SAFETY_STATUS,         "safety.status",         "!safety status",    BOTH, "Safety",     handleSafetyStatus,         CMD_NONE) \
    X(5123, SAFETY_RESET_BUDGET,   "safety.resetBudget",    "!safety reset",     BOTH, "Safety",     handleSafetyResetBudget,    CMD_CONFIRM) \
    X(5124, SAFETY_ROLLBACK,       "safety.rollback",       "!safety rollback",  BOTH, "Safety",     handleSafetyRollbackLast,   CMD_CONFIRM) \
    X(5125, SAFETY_VIOLATIONS,     "safety.violations",     "!safety violations",BOTH, "Safety",     handleSafetyShowViolations, CMD_NONE) \
    \
    /* ═══════════════════ REPLAY (5126-5129) ═══════════════════ */ \
    X(5126, REPLAY_STATUS,         "replay.status",         "!replay status",    BOTH, "Replay",     handleReplayStatus,         CMD_NONE) \
    X(5127, REPLAY_SHOW_LAST,      "replay.showLast",       "!replay last",      BOTH, "Replay",     handleReplayShowLast,       CMD_NONE) \
    X(5128, REPLAY_EXPORT,         "replay.export",         "!replay export",    BOTH, "Replay",     handleReplayExportSession,  CMD_NONE) \
    X(5129, REPLAY_CHECKPOINT,     "replay.checkpoint",     "!replay checkpoint",BOTH, "Replay",     handleReplayCheckpoint,     CMD_NONE) \
    \
    /* ═══════════════════ CONFIDENCE (5130-5131) ═══════════════════ */ \
    X(5130, CONFIDENCE_STATUS,     "confidence.status",     "!confidence status",BOTH, "Confidence", handleConfidenceStatus,     CMD_NONE) \
    X(5131, CONFIDENCE_POLICY,     "confidence.setPolicy",  "!confidence policy",BOTH, "Confidence", handleConfidenceSetPolicy,  CMD_NONE) \
    \
    /* ═══════════════════ SWARM (5132-5156) ═══════════════════ */ \
    X(5132, SWARM_STATUS,          "swarm.status",          "!swarm_status",     BOTH, "Swarm",      handleSwarmStatus,          CMD_NONE) \
    X(5133, SWARM_LEADER,          "swarm.startLeader",     "!swarm leader",     BOTH, "Swarm",      handleSwarmStartLeader,     CMD_ASYNC) \
    X(5134, SWARM_WORKER,          "swarm.startWorker",     "!swarm worker",     BOTH, "Swarm",      handleSwarmStartWorker,     CMD_ASYNC) \
    X(5135, SWARM_HYBRID,          "swarm.startHybrid",     "!swarm hybrid",     BOTH, "Swarm",      handleSwarmStartHybrid,     CMD_ASYNC) \
    X(5136, SWARM_STOP,            "swarm.stop",            "!swarm_leave",      BOTH, "Swarm",      handleSwarmLeave,           CMD_NONE) \
    X(5137, SWARM_LIST_NODES,      "swarm.listNodes",       "!swarm_nodes",      BOTH, "Swarm",      handleSwarmNodes,           CMD_NONE) \
    X(5138, SWARM_ADD_NODE,        "swarm.addNode",         "!swarm_join",       BOTH, "Swarm",      handleSwarmJoin,            CMD_NONE) \
    X(5139, SWARM_REMOVE_NODE,     "swarm.removeNode",      "!swarm remove",     BOTH, "Swarm",      handleSwarmRemoveNode,      CMD_NONE) \
    X(5140, SWARM_BLACKLIST,       "swarm.blacklistNode",   "!swarm blacklist",  BOTH, "Swarm",      handleSwarmBlacklist,       CMD_CONFIRM) \
    X(5141, SWARM_BUILD_SOURCES,   "swarm.buildSources",    "!swarm build_src",  BOTH, "Swarm",      handleSwarmBuildSources,    CMD_ASYNC) \
    X(5142, SWARM_BUILD_CMAKE,     "swarm.buildCmake",      "!swarm cmake",      BOTH, "Swarm",      handleSwarmBuildCmake,      CMD_ASYNC) \
    X(5143, SWARM_START_BUILD,     "swarm.startBuild",      "!swarm build",      BOTH, "Swarm",      handleSwarmStartBuild,      CMD_ASYNC) \
    X(5144, SWARM_CANCEL_BUILD,    "swarm.cancelBuild",     "!swarm cancel",     BOTH, "Swarm",      handleSwarmCancelBuild,     CMD_NONE) \
    X(5145, SWARM_CACHE_STATUS,    "swarm.cacheStatus",     "!swarm cache",      BOTH, "Swarm",      handleSwarmCacheStatus,     CMD_NONE) \
    X(5146, SWARM_CACHE_CLEAR,     "swarm.cacheClear",      "!swarm cache_clear",BOTH, "Swarm",      handleSwarmCacheClear,      CMD_CONFIRM) \
    X(5147, SWARM_CONFIG,          "swarm.config",          "!swarm config",     BOTH, "Swarm",      handleSwarmConfig,          CMD_NONE) \
    X(5148, SWARM_DISCOVERY,       "swarm.discovery",       "!swarm discovery",  BOTH, "Swarm",      handleSwarmDiscovery,       CMD_NONE) \
    X(5149, SWARM_TASK_GRAPH,      "swarm.taskGraph",       "!swarm taskgraph",  BOTH, "Swarm",      handleSwarmTaskGraph,       CMD_NONE) \
    X(5150, SWARM_EVENTS,          "swarm.events",          "!swarm events",     BOTH, "Swarm",      handleSwarmEvents,          CMD_NONE) \
    X(5151, SWARM_STATS,           "swarm.stats",           "!swarm stats",      BOTH, "Swarm",      handleSwarmStats,           CMD_NONE) \
    X(5152, SWARM_RESET_STATS,     "swarm.resetStats",      "!swarm reset",      BOTH, "Swarm",      handleSwarmResetStats,      CMD_CONFIRM) \
    X(5153, SWARM_WORKER_STATUS,   "swarm.workerStatus",    "!swarm wstatus",    BOTH, "Swarm",      handleSwarmWorkerStatus,    CMD_NONE) \
    X(5154, SWARM_WORKER_CONNECT,  "swarm.workerConnect",   "!swarm wconnect",   BOTH, "Swarm",      handleSwarmWorkerConnect,   CMD_ASYNC) \
    X(5155, SWARM_WORKER_DISCONNECT,"swarm.workerDisconnect","!swarm wdisconnect",BOTH, "Swarm",      handleSwarmWorkerDisconnect,CMD_NONE) \
    X(5156, SWARM_FITNESS,         "swarm.fitnessTest",     "!swarm fitness",    BOTH, "Swarm",      handleSwarmFitness,         CMD_ASYNC) \
    \
    /* ═══════════════════ DEBUG/DBG (5157-5184) ═══════════════════ */ \
    X(5157, DBG_LAUNCH,            "dbg.launch",            "!debug_start",      BOTH, "Debug",      handleDbgLaunch,            CMD_NONE) \
    X(5158, DBG_ATTACH,            "dbg.attach",            "!dbg attach",       BOTH, "Debug",      handleDbgAttach,            CMD_NONE) \
    X(5159, DBG_DETACH,            "dbg.detach",            "!dbg detach",       BOTH, "Debug",      handleDbgDetach,            CMD_REQUIRES_DEBUG) \
    X(5160, DBG_GO,                "dbg.go",                "!debug_continue",   BOTH, "Debug",      handleDbgGo,                CMD_REQUIRES_DEBUG) \
    X(5161, DBG_STEP_OVER,         "dbg.stepOver",          "!debug_step",       BOTH, "Debug",      handleDbgStepOver,          CMD_REQUIRES_DEBUG) \
    X(5162, DBG_STEP_INTO,         "dbg.stepInto",          "!dbg step_into",    BOTH, "Debug",      handleDbgStepInto,          CMD_REQUIRES_DEBUG) \
    X(5163, DBG_STEP_OUT,          "dbg.stepOut",           "!dbg step_out",     BOTH, "Debug",      handleDbgStepOut,           CMD_REQUIRES_DEBUG) \
    X(5164, DBG_BREAK,             "dbg.break",             "!dbg break",        BOTH, "Debug",      handleDbgBreak,             CMD_REQUIRES_DEBUG) \
    X(5165, DBG_KILL,              "dbg.kill",              "!debug_stop",       BOTH, "Debug",      handleDbgKill,              CMD_REQUIRES_DEBUG) \
    X(5166, DBG_ADD_BP,            "dbg.addBp",             "!breakpoint_add",   BOTH, "Debug",      handleDbgAddBp,             CMD_NONE) \
    X(5167, DBG_REMOVE_BP,         "dbg.removeBp",          "!breakpoint_remove",BOTH, "Debug",      handleDbgRemoveBp,          CMD_NONE) \
    X(5168, DBG_ENABLE_BP,         "dbg.enableBp",          "!dbg enable_bp",    BOTH, "Debug",      handleDbgEnableBp,          CMD_NONE) \
    X(5169, DBG_CLEAR_BPS,         "dbg.clearBps",          "!dbg clear_bps",    BOTH, "Debug",      handleDbgClearBps,          CMD_CONFIRM) \
    X(5170, DBG_LIST_BPS,          "dbg.listBps",           "!breakpoint_list",  BOTH, "Debug",      handleDbgListBps,           CMD_NONE) \
    X(5171, DBG_ADD_WATCH,         "dbg.addWatch",          "!dbg watch",        BOTH, "Debug",      handleDbgAddWatch,          CMD_NONE) \
    X(5172, DBG_REMOVE_WATCH,      "dbg.removeWatch",       "!dbg unwatch",      BOTH, "Debug",      handleDbgRemoveWatch,       CMD_NONE) \
    X(5173, DBG_REGISTERS,         "dbg.registers",         "!dbg regs",         BOTH, "Debug",      handleDbgRegisters,         CMD_REQUIRES_DEBUG) \
    X(5174, DBG_STACK,             "dbg.stack",             "!dbg stack",        BOTH, "Debug",      handleDbgStack,             CMD_REQUIRES_DEBUG) \
    X(5175, DBG_MEMORY,            "dbg.memory",            "!dbg mem",          BOTH, "Debug",      handleDbgMemory,            CMD_REQUIRES_DEBUG) \
    X(5176, DBG_DISASM,            "dbg.disasm",            "!dbg disasm",       BOTH, "Debug",      handleDbgDisasm,            CMD_REQUIRES_DEBUG) \
    X(5177, DBG_MODULES,           "dbg.modules",           "!dbg modules",      BOTH, "Debug",      handleDbgModules,           CMD_REQUIRES_DEBUG) \
    X(5178, DBG_THREADS,           "dbg.threads",           "!dbg threads",      BOTH, "Debug",      handleDbgThreads,           CMD_REQUIRES_DEBUG) \
    X(5179, DBG_SWITCH_THREAD,     "dbg.switchThread",      "!dbg thread",       BOTH, "Debug",      handleDbgSwitchThread,      CMD_REQUIRES_DEBUG) \
    X(5180, DBG_EVALUATE,          "dbg.evaluate",          "!dbg eval",         BOTH, "Debug",      handleDbgEvaluate,          CMD_REQUIRES_DEBUG) \
    X(5181, DBG_SET_REGISTER,      "dbg.setRegister",       "!dbg setreg",       BOTH, "Debug",      handleDbgSetRegister,       CMD_REQUIRES_DEBUG) \
    X(5182, DBG_SEARCH_MEMORY,     "dbg.searchMemory",      "!dbg search",       BOTH, "Debug",      handleDbgSearchMemory,      CMD_REQUIRES_DEBUG | CMD_ASYNC) \
    X(5183, DBG_SYMBOL_PATH,       "dbg.symbolPath",        "!dbg sympath",      BOTH, "Debug",      handleDbgSymbolPath,        CMD_NONE) \
    X(5184, DBG_STATUS,            "dbg.status",            "!dbg status",       BOTH, "Debug",      handleDbgStatus,            CMD_NONE) \
    \
    /* ═══════════════════ PLUGIN (5200-5208) ═══════════════════ */ \
    X(5200, PLUGIN_PANEL,          "plugin.panel",          "!plugin panel",     BOTH, "Plugin",     handlePluginShowPanel,      CMD_NONE) \
    X(5201, PLUGIN_LOAD,           "plugin.load",           "!plugin load",      BOTH, "Plugin",     handlePluginLoad,           CMD_NONE) \
    X(5202, PLUGIN_UNLOAD,         "plugin.unload",         "!plugin unload",    BOTH, "Plugin",     handlePluginUnload,         CMD_NONE) \
    X(5203, PLUGIN_UNLOAD_ALL,     "plugin.unloadAll",      "!plugin unload_all",BOTH, "Plugin",     handlePluginUnloadAll,      CMD_CONFIRM) \
    X(5204, PLUGIN_REFRESH,        "plugin.refresh",        "!plugin refresh",   BOTH, "Plugin",     handlePluginRefresh,        CMD_NONE) \
    X(5205, PLUGIN_SCAN_DIR,       "plugin.scanDir",        "!plugin scan",      BOTH, "Plugin",     handlePluginScanDir,        CMD_NONE) \
    X(5206, PLUGIN_STATUS,         "plugin.status",         "!plugin status",    BOTH, "Plugin",     handlePluginShowStatus,     CMD_NONE) \
    X(5207, PLUGIN_HOTLOAD,        "plugin.toggleHotload",  "!plugin hotload",   BOTH, "Plugin",     handlePluginToggleHotload,  CMD_NONE) \
    X(5208, PLUGIN_CONFIGURE,      "plugin.configure",      "!plugin config",    BOTH, "Plugin",     handlePluginConfigure,      CMD_NONE) \
    \
    /* ═══════════════════ HOTPATCH (9001-9017) ═══════════════════ */ \
    X(9001, HOTPATCH_STATUS,       "hotpatch.status",       "!hotpatch_status",  BOTH, "Hotpatch",   handleHotpatchStatus,       CMD_NONE) \
    X(9002, HOTPATCH_MEM_APPLY,    "hotpatch.memApply",     "!hotpatch_mem",     BOTH, "Hotpatch",   handleHotpatchMemory,       CMD_ASM_HOTPATH) \
    X(9003, HOTPATCH_MEM_REVERT,   "hotpatch.memRevert",    "!hotpatch_revert",  BOTH, "Hotpatch",   handleHotpatchMemRevert,    CMD_ASM_HOTPATH) \
    X(9004, HOTPATCH_BYTE_APPLY,   "hotpatch.byteApply",    "!hotpatch_byte",    BOTH, "Hotpatch",   handleHotpatchByte,         CMD_ASM_HOTPATH) \
    X(9005, HOTPATCH_BYTE_SEARCH,  "hotpatch.byteSearch",   "!hotpatch_search",  BOTH, "Hotpatch",   handleHotpatchByteSearch,   CMD_REQUIRES_FILE | CMD_ASM_HOTPATH) \
    X(9006, HOTPATCH_SRV_ADD,      "hotpatch.serverAdd",    "!hotpatch_server",  BOTH, "Hotpatch",   handleHotpatchServer,       CMD_NONE) \
    X(9007, HOTPATCH_SRV_REMOVE,   "hotpatch.serverRemove", "!hotpatch_srv_rm",  BOTH, "Hotpatch",   handleHotpatchServerRemove, CMD_NONE) \
    X(9008, HOTPATCH_PROXY_BIAS,   "hotpatch.proxyBias",    "!hotpatch_bias",    BOTH, "Hotpatch",   handleHotpatchProxyBias,    CMD_NONE) \
    X(9009, HOTPATCH_PROXY_REWRITE,"hotpatch.proxyRewrite", "!hotpatch_rewrite", BOTH, "Hotpatch",   handleHotpatchProxyRewrite, CMD_NONE) \
    X(9010, HOTPATCH_PROXY_TERM,   "hotpatch.proxyTerminate","!hotpatch_term",   BOTH, "Hotpatch",   handleHotpatchProxyTerminate,CMD_NONE) \
    X(9011, HOTPATCH_PROXY_VALID,  "hotpatch.proxyValidate","!hotpatch_validate",BOTH, "Hotpatch",   handleHotpatchProxyValidate,CMD_NONE) \
    X(9012, HOTPATCH_PRESET_SAVE,  "hotpatch.presetSave",   "!hotpatch_save",    BOTH, "Hotpatch",   handleHotpatchPresetSave,   CMD_NONE) \
    X(9013, HOTPATCH_PRESET_LOAD,  "hotpatch.presetLoad",   "!hotpatch_load",    BOTH, "Hotpatch",   handleHotpatchPresetLoad,   CMD_NONE) \
    X(9014, HOTPATCH_EVENT_LOG,    "hotpatch.eventLog",     "!hotpatch_log",     BOTH, "Hotpatch",   handleHotpatchEventLog,     CMD_NONE) \
    X(9015, HOTPATCH_RESET_STATS,  "hotpatch.resetStats",   "!hotpatch_reset",   BOTH, "Hotpatch",   handleHotpatchResetStats,   CMD_CONFIRM) \
    X(9016, HOTPATCH_TOGGLE_ALL,   "hotpatch.toggleAll",    "!hotpatch_toggle",  BOTH, "Hotpatch",   handleHotpatchToggleAll,    CMD_NONE) \
    X(9017, HOTPATCH_PROXY_STATS,  "hotpatch.proxyStats",   "!hotpatch_pstats",  BOTH, "Hotpatch",   handleHotpatchProxyStats,   CMD_NONE) \
    \
    /* ═══════════════════ MONACO/VIEW (9100-9105) ═══════════════════ */ \
    X(9100, MONACO_TOGGLE,         "view.monacoToggle",     "!monaco toggle",    BOTH, "Monaco",     handleMonacoToggle,         CMD_NONE) \
    X(9101, MONACO_DEVTOOLS,       "view.monacoDevtools",   "!monaco devtools",  BOTH, "Monaco",     handleMonacoDevtools,       CMD_NONE) \
    X(9102, MONACO_RELOAD,         "view.monacoReload",     "!monaco reload",    BOTH, "Monaco",     handleMonacoReload,         CMD_NONE) \
    X(9103, MONACO_ZOOM_IN,        "view.monacoZoomIn",     "!monaco zoomin",    GUI_ONLY, "Monaco", handleMonacoZoomIn,         CMD_NONE) \
    X(9104, MONACO_ZOOM_OUT,       "view.monacoZoomOut",    "!monaco zoomout",   GUI_ONLY, "Monaco", handleMonacoZoomOut,        CMD_NONE) \
    X(9105, MONACO_SYNC_THEME,     "view.monacoSyncTheme",  "!monaco sync",      BOTH, "Monaco",     handleMonacoSyncTheme,      CMD_NONE) \
    \
    /* ═══════════════════ LSP SERVER (9200-9208) ═══════════════════ */ \
    X(9200, LSPSRV_START,          "lspServer.start",       "!lspsrv start",     BOTH, "LSPServer",  handleLspSrvStart,          CMD_ASYNC) \
    X(9201, LSPSRV_STOP,           "lspServer.stop",        "!lspsrv stop",      BOTH, "LSPServer",  handleLspSrvStop,           CMD_NONE) \
    X(9202, LSPSRV_STATUS,         "lspServer.status",      "!lspsrv status",    BOTH, "LSPServer",  handleLspSrvStatus,         CMD_NONE) \
    X(9203, LSPSRV_REINDEX,        "lspServer.reindex",     "!lspsrv reindex",   BOTH, "LSPServer",  handleLspSrvReindex,        CMD_ASYNC) \
    X(9204, LSPSRV_STATS,          "lspServer.stats",       "!lspsrv stats",     BOTH, "LSPServer",  handleLspSrvStats,          CMD_NONE) \
    X(9205, LSPSRV_PUBLISH_DIAG,   "lspServer.publishDiag", "!lspsrv diag",      BOTH, "LSPServer",  handleLspSrvPublishDiag,    CMD_NONE) \
    X(9206, LSPSRV_CONFIG,         "lspServer.config",      "!lspsrv config",    BOTH, "LSPServer",  handleLspSrvConfig,         CMD_NONE) \
    X(9207, LSPSRV_EXPORT_SYMS,    "lspServer.exportSyms",  "!lspsrv export",    BOTH, "LSPServer",  handleLspSrvExportSymbols,  CMD_NONE) \
    X(9208, LSPSRV_LAUNCH_STDIO,   "lspServer.launchStdio", "!lspsrv stdio",     BOTH, "LSPServer",  handleLspSrvLaunchStdio,    CMD_ASYNC) \
    \
    /* ═══════════════════ EDITOR ENGINE (9300-9304) ═══════════════════ */ \
    X(9300, EDITOR_RICHEDIT,       "editor.richedit",       "!editor richedit",  BOTH, "Editor",     handleEditorRichEdit,       CMD_NONE) \
    X(9301, EDITOR_WEBVIEW2,       "editor.webview2",       "!editor webview",   BOTH, "Editor",     handleEditorWebView2,       CMD_NONE) \
    X(9302, EDITOR_MONACOCORE,     "editor.monacocore",     "!editor monaco",    BOTH, "Editor",     handleEditorMonacoCore,     CMD_NONE) \
    X(9303, EDITOR_CYCLE,          "editor.cycle",          "!editor cycle",     BOTH, "Editor",     handleEditorCycle,          CMD_NONE) \
    X(9304, EDITOR_STATUS,         "editor.status",         "!editor status",    BOTH, "Editor",     handleEditorStatus,         CMD_NONE) \
    \
    /* ═══════════════════ PDB (9400-9412) ═══════════════════ */ \
    X(9400, PDB_LOAD,              "pdb.load",              "!pdb load",         BOTH, "PDB",        handlePdbLoad,              CMD_REQUIRES_FILE) \
    X(9401, PDB_FETCH,             "pdb.fetch",             "!pdb fetch",        BOTH, "PDB",        handlePdbFetch,             CMD_REQUIRES_NET | CMD_ASYNC) \
    X(9402, PDB_STATUS,            "pdb.status",            "!pdb status",       BOTH, "PDB",        handlePdbStatus,            CMD_NONE) \
    X(9403, PDB_CACHE_CLEAR,       "pdb.cacheClear",        "!pdb cache_clear",  BOTH, "PDB",        handlePdbCacheClear,        CMD_CONFIRM) \
    X(9404, PDB_ENABLE,            "pdb.enable",            "!pdb enable",       BOTH, "PDB",        handlePdbEnable,            CMD_NONE) \
    X(9405, PDB_RESOLVE,           "pdb.resolve",           "!pdb resolve",      BOTH, "PDB",        handlePdbResolve,           CMD_REQUIRES_FILE) \
    X(9410, PDB_IMPORTS,           "pdb.imports",           "!pdb imports",      BOTH, "PDB",        handlePdbImports,           CMD_REQUIRES_FILE) \
    X(9411, PDB_EXPORTS,           "pdb.exports",           "!pdb exports",      BOTH, "PDB",        handlePdbExports,           CMD_REQUIRES_FILE) \
    X(9412, PDB_IAT_STATUS,        "pdb.iatStatus",         "!pdb iat",          BOTH, "PDB",        handlePdbIatStatus,         CMD_NONE) \
    \
    /* ═══════════════════ AUDIT (9500-9506) ═══════════════════ */ \
    X(9500, AUDIT_DASHBOARD,       "audit.dashboard",       "!audit dashboard",  BOTH, "Audit",      handleAuditDashboard,       CMD_NONE) \
    X(9501, AUDIT_RUN_FULL,        "audit.runFull",         "!audit full",       BOTH, "Audit",      handleAuditRunFull,         CMD_ASYNC) \
    X(9502, AUDIT_DETECT_STUBS,    "audit.detectStubs",     "!audit stubs",      BOTH, "Audit",      handleAuditDetectStubs,     CMD_NONE) \
    X(9503, AUDIT_CHECK_MENUS,     "audit.checkMenus",      "!audit menus",      BOTH, "Audit",      handleAuditCheckMenus,      CMD_NONE) \
    X(9504, AUDIT_RUN_TESTS,       "audit.runTests",        "!audit tests",      BOTH, "Audit",      handleAuditRunTests,        CMD_ASYNC) \
    X(9505, AUDIT_EXPORT_REPORT,   "audit.exportReport",    "!audit export",     BOTH, "Audit",      handleAuditExportReport,    CMD_NONE) \
    X(9506, AUDIT_QUICK_STATS,     "audit.quickStats",      "!audit stats",      BOTH, "Audit",      handleAuditQuickStats,      CMD_NONE) \
    \
    /* ═══════════════════ GAUNTLET (9600-9601) ═══════════════════ */ \
    X(9600, GAUNTLET_RUN,          "gauntlet.run",          "!gauntlet run",     BOTH, "Gauntlet",   handleGauntletRun,          CMD_ASYNC) \
    X(9601, GAUNTLET_EXPORT,       "gauntlet.export",       "!gauntlet export",  BOTH, "Gauntlet",   handleGauntletExport,       CMD_NONE) \
    \
    /* ═══════════════════ VOICE (9700-9709) ═══════════════════ */ \
    X(9700, VOICE_RECORD,          "voice.record",          "!voice record",     BOTH, "Voice",      handleVoiceRecord,          CMD_NONE) \
    X(9701, VOICE_PTT,             "voice.ptt",             "!voice ptt",        BOTH, "Voice",      handleVoicePTT,             CMD_NONE) \
    X(9702, VOICE_SPEAK,           "voice.speak",           "!voice speak",      BOTH, "Voice",      handleVoiceSpeak,           CMD_NONE) \
    X(9703, VOICE_JOIN_ROOM,       "voice.joinRoom",        "!voice join",       BOTH, "Voice",      handleVoiceJoinRoom,        CMD_REQUIRES_NET) \
    X(9704, VOICE_DEVICES,         "voice.devices",         "!voice devices",    BOTH, "Voice",      handleVoiceDevices,         CMD_NONE) \
    X(9705, VOICE_METRICS,         "voice.metrics",         "!voice metrics",    BOTH, "Voice",      handleVoiceMetrics,         CMD_NONE) \
    X(9706, VOICE_TOGGLE_PANEL,    "voice.togglePanel",     "!voice status",     BOTH, "Voice",      handleVoiceStatus,          CMD_NONE) \
    X(9707, VOICE_MODE_PTT,        "voice.modePtt",         "!voice mode",       BOTH, "Voice",      handleVoiceMode,            CMD_NONE) \
    X(9708, VOICE_MODE_CONTINUOUS, "voice.modeContinuous",  "!voice continuous",  BOTH, "Voice",      handleVoiceModeContinuous,  CMD_NONE) \
    X(9709, VOICE_MODE_DISABLED,   "voice.modeDisabled",    "!voice off",        BOTH, "Voice",      handleVoiceModeDisabled,    CMD_NONE) \
    \
    /* ═══════════════════ QW (Quality/Workflow 9800-9830) ═══════════════════ */ \
    X(9800, QW_SHORTCUT_EDITOR,    "qw.shortcutEditor",     "!qw shortcuts",     BOTH, "QW",         handleQwShortcutEditor,     CMD_NONE) \
    X(9801, QW_SHORTCUT_RESET,     "qw.shortcutReset",      "!qw reset_keys",    BOTH, "QW",         handleQwShortcutReset,      CMD_CONFIRM) \
    X(9810, QW_BACKUP_CREATE,      "qw.backupCreate",       "!qw backup",        BOTH, "QW",         handleQwBackupCreate,       CMD_NONE) \
    X(9811, QW_BACKUP_RESTORE,     "qw.backupRestore",      "!qw restore",       BOTH, "QW",         handleQwBackupRestore,      CMD_CONFIRM) \
    X(9812, QW_BACKUP_AUTO,        "qw.backupAutoToggle",   "!qw autobackup",    BOTH, "QW",         handleQwBackupAutoToggle,   CMD_NONE) \
    X(9813, QW_BACKUP_LIST,        "qw.backupList",         "!qw backup_list",   BOTH, "QW",         handleQwBackupList,         CMD_NONE) \
    X(9814, QW_BACKUP_PRUNE,       "qw.backupPrune",        "!qw prune",         BOTH, "QW",         handleQwBackupPrune,        CMD_CONFIRM) \
    X(9820, QW_ALERT_MONITOR,      "qw.alertToggleMonitor", "!qw monitor",       BOTH, "QW",         handleQwAlertMonitor,       CMD_NONE) \
    X(9821, QW_ALERT_HISTORY,      "qw.alertShowHistory",   "!qw alerts",        BOTH, "QW",         handleQwAlertHistory,       CMD_NONE) \
    X(9822, QW_ALERT_DISMISS,      "qw.alertDismissAll",    "!qw dismiss",       BOTH, "QW",         handleQwAlertDismiss,       CMD_NONE) \
    X(9823, QW_ALERT_RESOURCE,     "qw.alertResourceStatus","!qw resources",     BOTH, "QW",         handleQwAlertResourceStatus,CMD_NONE) \
    X(9830, QW_SLO_DASHBOARD,      "qw.sloDashboard",       "!qw slo",           BOTH, "QW",         handleQwSloDashboard,       CMD_NONE) \
    \
    /* ═══════════════════ TELEMETRY (9900-9905) ═══════════════════ */ \
    X(9900, TELEMETRY_TOGGLE,      "telemetry.toggle",      "!telemetry toggle", BOTH, "Telemetry",  handleTelemetryToggle,      CMD_NONE) \
    X(9901, TELEMETRY_EXPORT_JSON, "telemetry.exportJson",  "!telemetry json",   BOTH, "Telemetry",  handleTelemetryExportJson,  CMD_NONE) \
    X(9902, TELEMETRY_EXPORT_CSV,  "telemetry.exportCsv",   "!telemetry csv",    BOTH, "Telemetry",  handleTelemetryExportCsv,   CMD_NONE) \
    X(9903, TELEMETRY_DASHBOARD,   "telemetry.dashboard",   "!telemetry show",   BOTH, "Telemetry",  handleTelemetryDashboard,   CMD_NONE) \
    X(9904, TELEMETRY_CLEAR,       "telemetry.clear",       "!telemetry clear",  BOTH, "Telemetry",  handleTelemetryClear,       CMD_CONFIRM) \
    X(9905, TELEMETRY_SNAPSHOT,    "telemetry.snapshot",     "!telemetry snap",   BOTH, "Telemetry",  handleTelemetrySnapshot,    CMD_NONE) \
    \
    /* ═══════════════════ LEGACY ALIASES (Win32IDE.cpp alternate IDs) ═══════════════════ */ \
    /* These are legacy IDM_* values from Win32IDE.cpp that differ from the canonical     */ \
    /* IDs above. They route to the same handlers so both old menus and new menus work.   */ \
    X(2007, EDIT_UNDO_LEGACY,      "edit.undo.legacy",      "!undo_alt",         GUI_ONLY, "Edit",        handleEditUndo,            CMD_REQUIRES_FILE) \
    X(2008, EDIT_REDO_LEGACY,      "edit.redo.legacy",      "!redo_alt",         GUI_ONLY, "Edit",        handleEditRedo,            CMD_REQUIRES_FILE) \
    X(2009, EDIT_CUT_LEGACY,       "edit.cut.legacy",       "!cut_alt",          GUI_ONLY, "Edit",        handleEditCut,             CMD_REQUIRES_SELECT) \
    X(2010, EDIT_COPY_LEGACY,      "edit.copy.legacy",      "!copy_alt",         GUI_ONLY, "Edit",        handleEditCopy,            CMD_REQUIRES_SELECT) \
    X(2011, EDIT_PASTE_LEGACY,     "edit.paste.legacy",     "!paste_alt",        GUI_ONLY, "Edit",        handleEditPaste,           CMD_REQUIRES_FILE) \
    X(3001, TERMINAL_PS_LEGACY,    "terminal.ps.legacy",    "!terminal_ps",      GUI_ONLY, "Terminal",    handleTerminalSplitH,      CMD_NONE) \
    X(3002, TERMINAL_CMD_LEGACY,   "terminal.cmd.legacy",   "!terminal_cmd",     GUI_ONLY, "Terminal",    handleTerminalSplitH,      CMD_NONE) \
    X(3003, TERMINAL_STOP_LEGACY,  "terminal.stop.legacy",  "!terminal_stop",    GUI_ONLY, "Terminal",    handleTerminalKill,        CMD_NONE) \
    X(3004, TERMINAL_SPLITH_LEGACY,"terminal.splitH.legacy","!terminal_sh",      GUI_ONLY, "Terminal",    handleTerminalSplitH,      CMD_NONE) \
    X(3005, TERMINAL_SPLITV_LEGACY,"terminal.splitV.legacy","!terminal_sv",      GUI_ONLY, "Terminal",    handleTerminalSplitV,      CMD_NONE) \
    X(3006, TERMINAL_CLEARALL_LEG, "terminal.clearAll.legacy","!terminal_clearall",GUI_ONLY,"Terminal",    handleTerminalKill,        CMD_NONE) \
    X(3010, TOOLS_PROFILE_START,   "tools.profileStart",    "!profile_start",    BOTH, "Tools",           handleProfile,             CMD_ASYNC) \
    X(3011, TOOLS_PROFILE_STOP,    "tools.profileStop",     "!profile_stop",     BOTH, "Tools",           handleProfile,             CMD_NONE) \
    X(3012, TOOLS_PROFILE_RESULTS, "tools.profileResults",  "!profile_results",  BOTH, "Tools",           handleProfile,             CMD_NONE) \
    X(3013, TOOLS_ANALYZE_SCRIPT,  "tools.analyzeScript",   "!analyze_script",   BOTH, "Tools",           handleAnalyze,             CMD_REQUIRES_FILE) \
    X(3050, MODULES_REFRESH,       "modules.refresh",       "!modules_refresh",  BOTH, "Modules",         handleViewModuleBrowser,   CMD_NONE) \
    X(3051, MODULES_IMPORT,        "modules.import",        "!modules_import",   BOTH, "Modules",         handleViewModuleBrowser,   CMD_NONE) \
    X(3052, MODULES_EXPORT,        "modules.export",        "!modules_export",   BOTH, "Modules",         handleViewModuleBrowser,   CMD_NONE) \
    \
    /* ═══════════════════ CLI-ONLY (ID=0, no GUI routing) ═══════════════════ */ \
    X(0,    CLI_SEARCH,            "cli.search",            "!search",           CLI_ONLY, "CLI",     handleSearch,               CMD_NONE) \
    X(0,    CLI_ANALYZE,           "cli.analyze",           "!analyze",          CLI_ONLY, "CLI",     handleAnalyze,              CMD_NONE) \
    X(0,    CLI_PROFILE,           "cli.profile",           "!profile",          CLI_ONLY, "CLI",     handleProfile,              CMD_ASYNC) \
    X(0,    CLI_SUBAGENT,          "cli.subagent",          "!subagent",         CLI_ONLY, "CLI",     handleSubAgent,             CMD_ASYNC) \
    X(0,    CLI_COT,               "cli.cot",               "!cot",              CLI_ONLY, "CLI",     handleCOT,                  CMD_NONE) \
    X(0,    CLI_STATUS,            "cli.status",            "!status",           CLI_ONLY, "CLI",     handleStatus,               CMD_NONE) \
    X(0,    CLI_HELP,              "cli.help",              "!help",             CLI_ONLY, "CLI",     handleHelp,                 CMD_NONE) \
    X(0,    CLI_GENERATE_IDE,      "cli.generateIDE",       "!generate_ide",     CLI_ONLY, "CLI",     handleGenerateIDE,          CMD_ASYNC) \
    X(0,    CLI_HOTPATCH_APPLY,    "cli.hotpatchApply",     "!hotpatch_apply",   CLI_ONLY, "CLI",     handleHotpatchApply,        CMD_ASM_HOTPATH) \
    X(0,    CLI_HOTPATCH_CREATE,   "cli.hotpatchCreate",    "!hotpatch_create",  CLI_ONLY, "CLI",     handleHotpatchCreate,       CMD_NONE) \
    X(0,    CLI_RE_AUTOPATCH,      "cli.reAutoPatch",       "!auto_patch",       CLI_ONLY, "CLI",     handleREAutoPatch,          CMD_ASM_HOTPATH) \
    X(0,    CLI_AI_MODE,           "cli.aiMode",            "!mode",             CLI_ONLY, "CLI",     handleAIModeSet,            CMD_NONE) \
    X(0,    CLI_AI_ENGINE,         "cli.aiEngine",          "!engine",           CLI_ONLY, "CLI",     handleAIEngineSelect,       CMD_NONE) \
    X(0,    CLI_AUTONOMY_RATE,     "cli.autonomyRate",      "!autonomy_rate",    CLI_ONLY, "CLI",     handleAutonomyRate,         CMD_NONE) \
    X(0,    CLI_AUTONOMY_RUN,      "cli.autonomyRun",       "!autonomy_run",     CLI_ONLY, "CLI",     handleAutonomyRun,          CMD_ASYNC) \
    X(0,    CLI_VOICE_INIT,        "cli.voiceInit",         "!voice init",       CLI_ONLY, "CLI",     handleVoiceInit,            CMD_NONE) \
    X(0,    CLI_VOICE_TRANSCRIBE,  "cli.voiceTranscribe",   "!voice transcribe", CLI_ONLY, "CLI",     handleVoiceTranscribe,      CMD_NONE) \
    X(0,    CLI_SERVER_START,      "cli.serverStart",       "!server start",     CLI_ONLY, "CLI",     handleServerStart,          CMD_ASYNC) \
    X(0,    CLI_SERVER_STOP,       "cli.serverStop",        "!server stop",      CLI_ONLY, "CLI",     handleServerStop,           CMD_NONE) \
    X(0,    CLI_SERVER_STATUS,     "cli.serverStatus",      "!server status",    CLI_ONLY, "CLI",     handleServerStatus,         CMD_NONE) \
    X(0,    CLI_SETTINGS_OPEN,     "cli.settingsOpen",      "!settings",         CLI_ONLY, "CLI",     handleSettingsOpen,         CMD_NONE) \
    X(0,    CLI_SETTINGS_EXPORT,   "cli.settingsExport",    "!settings_export",  CLI_ONLY, "CLI",     handleSettingsExport,       CMD_NONE) \
    X(0,    CLI_SETTINGS_IMPORT,   "cli.settingsImport",    "!settings_import",  CLI_ONLY, "CLI",     handleSettingsImport,       CMD_NONE) \
    X(0,    CLI_MANIFEST_JSON,     "cli.manifestJson",      "!manifest_json",    CLI_ONLY, "CLI",     handleManifestJSON,         CMD_NONE) \
    X(0,    CLI_MANIFEST_MD,       "cli.manifestMd",        "!manifest_md",      CLI_ONLY, "CLI",     handleManifestMarkdown,     CMD_NONE) \
    X(0,    CLI_SELF_TEST,         "cli.selfTest",          "!self_test",        CLI_ONLY, "CLI",     handleManifestSelfTest,     CMD_ASYNC) \
    /* ═══════════════════ END OF TABLE ═══════════════════ */

// ============================================================================
// GENERATED: Command ID Enum
// ============================================================================

#define EXPAND_CMD_ENUM(id, sym, name, cli, exp, cat, handler, flags) \
    CMD_##sym = id,

namespace RawrXD::Commands {

// Note: CLI-only commands all have ID=0, so they share that value.
// Use canonical name for CLI-only dispatch, not ID.

} // namespace RawrXD::Commands

// ============================================================================
// GENERATED: Command Descriptor Structure
// ============================================================================

struct CmdDescriptor {
    uint32_t        id;
    const char*     symbol;
    const char*     canonicalName;
    const char*     cliAlias;
    CmdExposure     exposure;
    const char*     category;
    CommandResult   (*handler)(const CommandContext&);
    uint32_t        flags;
};

// ============================================================================
// GENERATED: Command Registry Array
// ============================================================================

#define EXPAND_CMD_DESC(id, sym, name, cli, exp, cat, handler, flags) \
    { id, #sym, name, cli, CmdExposure::exp, cat, handler, flags },

// Forward-declare all handlers (they must exist or link fails)
#define EXPAND_CMD_DECL(id, sym, name, cli, exp, cat, handler, flags) \
    CommandResult handler(const CommandContext& ctx);

COMMAND_TABLE(EXPAND_CMD_DECL)
#undef EXPAND_CMD_DECL

inline const CmdDescriptor g_commandRegistry[] = {
    COMMAND_TABLE(EXPAND_CMD_DESC)
};
#undef EXPAND_CMD_DESC

inline constexpr size_t g_commandRegistrySize = sizeof(g_commandRegistry) / sizeof(g_commandRegistry[0]);

// ============================================================================
// COMPILE-TIME VALIDATION
// ============================================================================

// Count all non-zero IDs (GUI-routable commands)
#define COUNT_GUI_CMD(id, sym, name, cli, exp, cat, handler, flags) + (id != 0 ? 1 : 0)
inline constexpr size_t g_guiCommandCount = 0 COMMAND_TABLE(COUNT_GUI_CMD);
#undef COUNT_GUI_CMD

// Count CLI-accessible commands (BOTH + CLI_ONLY)
#define COUNT_CLI_CMD(id, sym, name, cli, exp, cat, handler, flags) \
    + (CmdExposure::exp == CmdExposure::CLI_ONLY || CmdExposure::exp == CmdExposure::BOTH ? 1 : 0)
inline constexpr size_t g_cliCommandCount = 0 COMMAND_TABLE(COUNT_CLI_CMD);
#undef COUNT_CLI_CMD

#endif // RAWRXD_COMMAND_REGISTRY_HPP
