// ============================================================================
// feature_registration.cpp — DEPRECATED: Manual Registration Replaced
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
//
// ┌──────────────────────────────────────────────────────────────────────────┐
// │ THIS FILE IS NOW DEPRECATED.                                            │
// │                                                                          │
// │ All feature registration is auto-generated from COMMAND_TABLE in         │
// │ command_registry.hpp by the AutoRegistrar in unified_command_dispatch.cpp.│
// │                                                                          │
// │ The 631 lines of manual reg() calls have been replaced by a single       │
// │ loop that reads g_commandRegistry[] and populates SharedFeatureRegistry.  │
// │                                                                          │
// │ To add a new command: Add ONE line to COMMAND_TABLE in                    │
// │ command_registry.hpp. Everything else is automatic.                       │
// │                                                                          │
// │ This file is kept as a compilation unit to avoid build breakage.          │
// │ It compiles to zero code.                                                │
// └──────────────────────────────────────────────────────────────────────────┘
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

// The auto-registration is performed by unified_command_dispatch.cpp
// via the static AutoRegistrar object. That code iterates
// g_commandRegistry[] and calls SharedFeatureRegistry::registerFeature()
// for every entry in COMMAND_TABLE.
//
// No manual registration calls needed here anymore.
// This file intentionally compiles to nothing.
#define IDM_FILE_CLOSE_R           1006
#define IDM_FILE_RECENT_R          1010
#define IDM_FILE_LOAD_MODEL_R      1030
#define IDM_FILE_MODEL_HF_R        1031
#define IDM_FILE_MODEL_OLLAMA_R    1032
#define IDM_FILE_MODEL_URL_R       1033
#define IDM_FILE_MODEL_UNIFIED_R   1034
#define IDM_FILE_QUICK_LOAD_R      1035

// ── Edit: 2000-2099 (from Win32IDE_Commands.cpp) ──
// NOTE: Win32IDE.cpp has Undo=2007, but Commands.cpp switch expects
// these values. The unified dispatch catches BOTH ranges.
#define IDM_EDIT_UNDO_R            2001
#define IDM_EDIT_REDO_R            2002
#define IDM_EDIT_CUT_R             2003
#define IDM_EDIT_COPY_R            2004
#define IDM_EDIT_PASTE_R           2005
#define IDM_EDIT_SELECT_ALL_R      2006
#define IDM_EDIT_FIND_R            2016  // from Win32IDE.cpp/Core.cpp
#define IDM_EDIT_REPLACE_R         2017  // from Win32IDE.cpp/Core.cpp

// ── Terminal: 4000-4099 (from Win32IDE.h) ──
#define IDM_TERMINAL_NEW_R         4001  // IDM_HELP_ABOUT collision! — see note
#define IDM_TERMINAL_KILL_R        4006
#define IDM_TERMINAL_SPLIT_H_R     4007
#define IDM_TERMINAL_SPLIT_V_R     4008
#define IDM_TERMINAL_LIST_R        4010

// ── Agent: 4100-4199 (from Win32IDE.h — canonical) ──
#define IDM_AGENT_START_LOOP_R     4100
#define IDM_AGENT_EXECUTE_CMD_R    4101
#define IDM_AGENT_CONFIGURE_R      4102
#define IDM_AGENT_VIEW_TOOLS_R     4103
#define IDM_AGENT_VIEW_STATUS_R    4104
#define IDM_AGENT_STOP_R           4105
#define IDM_AGENT_MEMORY_R         4106
#define IDM_AGENT_MEMORY_VIEW_R    4107
#define IDM_AGENT_MEMORY_CLEAR_R   4108
#define IDM_AGENT_MEMORY_EXPORT_R  4109
#define IDM_AGENT_BOUNDED_LOOP_R   4120
#define IDM_AGENT_GOAL_R           4121

// ── SubAgent: 4110-4119 (from Win32IDE.h) ──
#define IDM_SUBAGENT_CHAIN_R       4110
#define IDM_SUBAGENT_SWARM_R       4111
#define IDM_SUBAGENT_TODO_LIST_R   4112
#define IDM_SUBAGENT_TODO_CLEAR_R  4113
#define IDM_SUBAGENT_STATUS_R      4114

// ── Autonomy: 4150-4199 (from Win32IDE.h) ──
#define IDM_AUTONOMY_TOGGLE_R      4150
#define IDM_AUTONOMY_START_R       4151
#define IDM_AUTONOMY_STOP_R        4152
#define IDM_AUTONOMY_GOAL_R        4153
// NOTE: 4154=IDM_AUTONOMY_STATUS, 4155=IDM_AUTONOMY_MEMORY in Win32IDE.h
// We don't collide — autonomy.rate/run are CLI-only features
#define IDM_AUTONOMY_RATE_R        0     // CLI-only, no GUI command ID
#define IDM_AUTONOMY_RUN_R         0     // CLI-only, no GUI command ID

// ── AI Mode: 4200-4299 (from Win32IDE.h — canonical) ──
#define IDM_AI_MAX_MODE_R          4200  // IDM_AI_MODE_MAX
#define IDM_AI_DEEP_THINKING_R     4201  // IDM_AI_MODE_DEEP_THINK
#define IDM_AI_DEEP_RESEARCH_R     4202  // IDM_AI_MODE_DEEP_RESEARCH
#define IDM_AI_MODE_R              0     // No specific GUI ID for generic mode set
#define IDM_AI_ENGINE_R            0     // CLI-only

// ── Reverse Engineering: 4300-4399 (from Win32IDE.h) ──
#define IDM_RE_DECISION_TREE_R     4300  // IDM_REVENG_ANALYZE (closest)
#define IDM_RE_DISASM_R            4301  // IDM_REVENG_DISASM
#define IDM_RE_DUMPBIN_R           4302  // IDM_REVENG_DUMPBIN
#define IDM_RE_CFG_R               4308  // IDM_REVENG_CFG
#define IDM_RE_SSA_LIFT_R          4311  // IDM_REVENG_SSA
#define IDM_RE_AUTO_PATCH_R        0     // No GUI equivalent

// ── Debug: 5157-5184 (from Win32IDE.h — IDM_DBG_*) ──
#define IDM_DEBUG_START_R          5157  // IDM_DBG_LAUNCH
#define IDM_DEBUG_STOP_R           5165  // IDM_DBG_KILL
#define IDM_DEBUG_STEP_R           5161  // IDM_DBG_STEP_OVER
#define IDM_DEBUG_CONTINUE_R       5160  // IDM_DBG_GO
#define IDM_BREAKPOINT_ADD_R       5166  // IDM_DBG_ADD_BP
#define IDM_BREAKPOINT_LIST_R      5170  // IDM_DBG_LIST_BPS
#define IDM_BREAKPOINT_REMOVE_R    5167  // IDM_DBG_REMOVE_BP

// ── Hotpatch: 9001-9017 (from Win32IDE.h) ──
#define IDM_HOTPATCH_STATUS_R      9001  // IDM_HOTPATCH_SHOW_STATUS
#define IDM_HOTPATCH_MEMORY_R      9002  // IDM_HOTPATCH_MEMORY_APPLY
#define IDM_HOTPATCH_BYTE_R        9004  // IDM_HOTPATCH_BYTE_APPLY
#define IDM_HOTPATCH_SERVER_R      9006  // IDM_HOTPATCH_SERVER_ADD
#define IDM_HOTPATCH_APPLY_R       0     // CLI-only (no single GUI "apply" command)
#define IDM_HOTPATCH_CREATE_R      0     // CLI-only

// ── Voice: 9700-9799 (from Win32IDE.h — IDM_VOICE_*) ──
#define IDM_VOICE_RECORD_R         9700  // IDM_VOICE_RECORD
#define IDM_VOICE_SPEAK_R          9702  // IDM_VOICE_SPEAK
#define IDM_VOICE_DEVICES_R        9704  // IDM_VOICE_SHOW_DEVICES
#define IDM_VOICE_METRICS_R        9705  // IDM_VOICE_METRICS
#define IDM_VOICE_MODE_R           9707  // IDM_VOICE_MODE_PTT
#define IDM_VOICE_STATUS_R         9706  // IDM_VOICE_TOGGLE_PANEL
#define IDM_VOICE_INIT_R           0     // CLI-only
#define IDM_VOICE_TRANSCRIBE_R     0     // CLI-only

// ── Git: 3020-3024 (from Win32IDE.cpp) ──
// NOTE: routeCommand maps 3000-3999 → handleViewCommand, but
// handleGitCommand handles 8000-8999. Git IDs 3020-3024 go to
// handleViewCommand which delegates. Use the canonical 3020+ values.
#define IDM_GIT_STATUS_R           3020  // IDM_GIT_STATUS
#define IDM_GIT_COMMIT_R           3021  // IDM_GIT_COMMIT
#define IDM_GIT_PUSH_R             3022  // IDM_GIT_PUSH
#define IDM_GIT_PULL_R             3023  // IDM_GIT_PULL
#define IDM_GIT_DIFF_R             3024  // IDM_GIT_PANEL (closest)

// ── Themes: 3101-3116 (from Win32IDE.h — individual themes) ──
#define IDM_THEME_SET_R            3101  // IDM_THEME_DARK_PLUS (default)
#define IDM_THEME_LIST_R           3100  // IDM_THEME_BASE

// ── Backend/LLM Router: 5037-5081 (from Win32IDE.h — IDM_BACKEND_*) ──
#define IDM_BACKEND_LIST_R         5043  // IDM_BACKEND_SHOW_SWITCHER
#define IDM_BACKEND_SELECT_R       5037  // IDM_BACKEND_SWITCH_LOCAL
#define IDM_BACKEND_STATUS_R       5042  // IDM_BACKEND_SHOW_STATUS

// ── Swarm: 5132-5156 (from Win32IDE.h — IDM_SWARM_*) ──
#define IDM_SWARM_JOIN_R           5138  // IDM_SWARM_ADD_NODE
#define IDM_SWARM_STATUS_R         5132  // IDM_SWARM_STATUS
#define IDM_SWARM_DISTRIBUTE_R     0     // CLI-only
#define IDM_SWARM_REBALANCE_R      0     // CLI-only
#define IDM_SWARM_NODES_R          5137  // IDM_SWARM_LIST_NODES
#define IDM_SWARM_LEAVE_R          5136  // IDM_SWARM_STOP

// ── Server: uses dedicated IDs outside collision zones ──
// 9100-9105 collides with Monaco. Server gets CLI-only routing.
#define IDM_SERVER_START_R         0     // CLI-only (avoid Monaco collision)
#define IDM_SERVER_STOP_R          0     // CLI-only
#define IDM_SERVER_STATUS_R        0     // CLI-only

// ── Settings: uses dedicated IDs outside PDB collision ──
// 9400-9412 collides with PDB. Settings gets CLI-only routing.
#define IDM_SETTINGS_OPEN_R        0     // CLI-only (avoid PDB collision)
#define IDM_SETTINGS_EXPORT_R      0     // CLI-only
#define IDM_SETTINGS_IMPORT_R      0     // CLI-only

// ── Help: 4001-4004 (from Win32IDE.cpp) ──
// NOTE: 4001 collides with IDM_TERMINAL_NEW in the 4000-4099 range.
// Help commands route through handleHelpCommand via 7000+ in routeCommand.
// But menus use 4001-4004. Use 7000+ for non-colliding dispatch.
#define IDM_HELP_ABOUT_R           7001  // Dedicated non-colliding ID
#define IDM_HELP_DOCS_R            7002
#define IDM_HELP_SHORTCUTS_R       7003

// ============================================================================
// REGISTRATION HELPER
// ============================================================================

static void reg(const char* id, const char* name, const char* desc,
                FeatureGroup group, uint32_t cmdId, 
                const char* cliCmd, const char* shortcut,
                FeatureHandler handler, bool gui, bool cli, bool asmFast = false) {
    FeatureDescriptor fd{};
    fd.id = id;
    fd.name = name;
    fd.description = desc;
    fd.group = group;
    fd.commandId = cmdId;
    fd.cliCommand = cliCmd;
    fd.shortcut = shortcut;
    fd.handler = handler;
    fd.guiSupported = gui;
    fd.cliSupported = cli;
    fd.asmHotPath = asmFast;
    SharedFeatureRegistry::instance().registerFeature(fd);
}

// ============================================================================
// STATIC INITIALIZATION — Runs before main()
// ============================================================================

static struct FeatureRegistrar {
    FeatureRegistrar() {
        auto& R = SharedFeatureRegistry::instance();
        
        // ══════════════ FILE OPERATIONS ══════════════
        reg("file.new", "New File", "Create a new empty file",
            FeatureGroup::FileOps, IDM_FILE_NEW_R, "!new", "Ctrl+N",
            handleFileNew, true, true);
        reg("file.open", "Open File", "Open an existing file with dialog",
            FeatureGroup::FileOps, IDM_FILE_OPEN_R, "!open", "Ctrl+O",
            handleFileOpen, true, true);
        reg("file.save", "Save File", "Save current file to disk",
            FeatureGroup::FileOps, IDM_FILE_SAVE_R, "!save", "Ctrl+S",
            handleFileSave, true, true);
        reg("file.saveAs", "Save As", "Save current file with new name",
            FeatureGroup::FileOps, IDM_FILE_SAVE_AS_R, "!save_as", "Ctrl+Shift+S",
            handleFileSaveAs, true, true);
        reg("file.saveAll", "Save All", "Save all modified files",
            FeatureGroup::FileOps, IDM_FILE_SAVE_ALL_R, "!save_all", "",
            handleFileSaveAll, true, true);
        reg("file.close", "Close File", "Close current file",
            FeatureGroup::FileOps, IDM_FILE_CLOSE_R, "!close", "Ctrl+W",
            handleFileClose, true, true);
        reg("file.recentFiles", "Recent Files", "Open from recent files",
            FeatureGroup::FileOps, IDM_FILE_RECENT_R, "!recent", "",
            handleFileRecentFiles, true, true);
        reg("file.loadModel", "Load GGUF Model", "Load a GGUF model file",
            FeatureGroup::FileOps, IDM_FILE_LOAD_MODEL_R, "!model_load", "",
            handleFileLoadModel, true, true);
        reg("file.modelFromHF", "Model from HuggingFace", "Download model from HF",
            FeatureGroup::FileOps, IDM_FILE_MODEL_HF_R, "!model_hf", "",
            handleFileModelFromHF, true, true);
        reg("file.modelFromOllama", "Model from Ollama", "Load from Ollama",
            FeatureGroup::FileOps, IDM_FILE_MODEL_OLLAMA_R, "!model_ollama", "",
            handleFileModelFromOllama, true, true);
        reg("file.modelFromURL", "Model from URL", "Download from URL",
            FeatureGroup::FileOps, IDM_FILE_MODEL_URL_R, "!model_url", "",
            handleFileModelFromURL, true, true);
        reg("file.modelUnified", "Unified Model Load", "Universal loader",
            FeatureGroup::FileOps, IDM_FILE_MODEL_UNIFIED_R, "!model_unified", "",
            handleFileUnifiedLoad, true, true);
        reg("file.quickLoad", "Quick Load Model", "One-click local cache load",
            FeatureGroup::FileOps, IDM_FILE_QUICK_LOAD_R, "!quick_load", "",
            handleFileQuickLoad, true, true);

        // ══════════════ EDITING ══════════════
        reg("edit.undo", "Undo", "Undo last action",
            FeatureGroup::Editing, IDM_EDIT_UNDO_R, "!undo", "Ctrl+Z",
            handleEditUndo, true, true);
        reg("edit.redo", "Redo", "Redo last undone action",
            FeatureGroup::Editing, IDM_EDIT_REDO_R, "!redo", "Ctrl+Y",
            handleEditRedo, true, true);
        reg("edit.cut", "Cut", "Cut selection to clipboard",
            FeatureGroup::Editing, IDM_EDIT_CUT_R, "!cut", "Ctrl+X",
            handleEditCut, true, true);
        reg("edit.copy", "Copy", "Copy selection to clipboard",
            FeatureGroup::Editing, IDM_EDIT_COPY_R, "!copy", "Ctrl+C",
            handleEditCopy, true, true);
        reg("edit.paste", "Paste", "Paste from clipboard",
            FeatureGroup::Editing, IDM_EDIT_PASTE_R, "!paste", "Ctrl+V",
            handleEditPaste, true, true);
        reg("edit.find", "Find", "Search for text",
            FeatureGroup::Editing, IDM_EDIT_FIND_R, "!find", "Ctrl+F",
            handleEditFind, true, true);
        reg("edit.replace", "Replace", "Find and replace text",
            FeatureGroup::Editing, IDM_EDIT_REPLACE_R, "!replace", "Ctrl+H",
            handleEditReplace, true, true);
        reg("edit.selectAll", "Select All", "Select entire buffer",
            FeatureGroup::Editing, IDM_EDIT_SELECT_ALL_R, "!select_all", "Ctrl+A",
            handleEditSelectAll, true, true);

        // ══════════════ AGENT ══════════════
        reg("agent.execute", "Agent Execute", "Execute agent instruction",
            FeatureGroup::Agent, IDM_AGENT_EXECUTE_CMD_R, "!agent_execute", "",
            handleAgentExecute, true, true);
        reg("agent.loop", "Agent Loop", "Start agent loop",
            FeatureGroup::Agent, IDM_AGENT_START_LOOP_R, "!agent_loop", "",
            handleAgentLoop, true, true);
        reg("agent.boundedLoop", "Bounded Agent Loop", "Agent loop with max iters",
            FeatureGroup::Agent, IDM_AGENT_BOUNDED_LOOP_R, "!agent_bounded", "",
            handleAgentBoundedLoop, true, true);
        reg("agent.stop", "Agent Stop", "Stop current agent",
            FeatureGroup::Agent, IDM_AGENT_STOP_R, "!agent_stop", "",
            handleAgentStop, true, true);
        reg("agent.goal", "Agent Goal", "Set agent goal",
            FeatureGroup::Agent, IDM_AGENT_GOAL_R, "!agent_goal", "",
            handleAgentGoal, true, true);
        reg("agent.memory", "Agent Memory", "Agent memory system",
            FeatureGroup::Agent, IDM_AGENT_MEMORY_R, "!agent_memory", "",
            handleAgentMemory, true, true);
        reg("agent.memoryView", "View Agent Memory", "Display memory contents",
            FeatureGroup::Agent, IDM_AGENT_MEMORY_VIEW_R, "!agent_memory_view", "",
            handleAgentMemoryView, true, true);
        reg("agent.memoryClear", "Clear Agent Memory", "Reset agent memory",
            FeatureGroup::Agent, IDM_AGENT_MEMORY_CLEAR_R, "!agent_memory_clear", "",
            handleAgentMemoryClear, true, true);
        reg("agent.memoryExport", "Export Agent Memory", "Save memory to file",
            FeatureGroup::Agent, IDM_AGENT_MEMORY_EXPORT_R, "!agent_memory_export", "",
            handleAgentMemoryExport, true, true);
        reg("agent.configure", "Configure Agent", "Agent configuration",
            FeatureGroup::Agent, IDM_AGENT_CONFIGURE_R, "!agent_config", "",
            handleAgentConfigure, true, true);
        reg("agent.viewTools", "View Agent Tools", "List available tools",
            FeatureGroup::Agent, IDM_AGENT_VIEW_TOOLS_R, "!tools", "",
            handleAgentViewTools, true, true);
        reg("agent.viewStatus", "Agent Status", "Show agent status",
            FeatureGroup::Agent, IDM_AGENT_VIEW_STATUS_R, "!agent_status", "",
            handleAgentViewStatus, true, true);

        // ══════════════ AUTONOMY ══════════════
        reg("autonomy.start", "Start Autonomy", "Activate autonomous loop",
            FeatureGroup::Autonomy, IDM_AUTONOMY_START_R, "!autonomy_start", "",
            handleAutonomyStart, true, true);
        reg("autonomy.stop", "Stop Autonomy", "Deactivate autonomous loop",
            FeatureGroup::Autonomy, IDM_AUTONOMY_STOP_R, "!autonomy_stop", "",
            handleAutonomyStop, true, true);
        reg("autonomy.goal", "Autonomy Goal", "Set autonomy target",
            FeatureGroup::Autonomy, IDM_AUTONOMY_GOAL_R, "!autonomy_goal", "",
            handleAutonomyGoal, true, true);
        reg("autonomy.rate", "Autonomy Rate", "Configure polling rate",
            FeatureGroup::Autonomy, IDM_AUTONOMY_RATE_R, "!autonomy_rate", "",
            handleAutonomyRate, false, true);  // CLI-only
        reg("autonomy.run", "Autonomy Run", "Execute decision tree",
            FeatureGroup::Autonomy, IDM_AUTONOMY_RUN_R, "!autonomy_run", "",
            handleAutonomyRun, false, true);  // CLI-only
        reg("autonomy.toggle", "Toggle Autonomy", "Toggle on/off",
            FeatureGroup::Autonomy, IDM_AUTONOMY_TOGGLE_R, "!autonomy_toggle", "",
            handleAutonomyToggle, true, true);

        // ══════════════ SUB-AGENT ══════════════
        reg("subagent.chain", "Chain SubAgents", "Execute chain of sub-agents",
            FeatureGroup::SubAgent, IDM_SUBAGENT_CHAIN_R, "!chain", "",
            handleSubAgentChain, true, true);
        reg("subagent.swarm", "Swarm SubAgents", "Launch sub-agent swarm",
            FeatureGroup::SubAgent, IDM_SUBAGENT_SWARM_R, "!swarm", "",
            handleSubAgentSwarm, true, true);
        reg("subagent.todoList", "Todo List", "View todo items",
            FeatureGroup::SubAgent, IDM_SUBAGENT_TODO_LIST_R, "!todo", "",
            handleSubAgentTodoList, true, true);
        reg("subagent.todoClear", "Clear Todos", "Clear all todo items",
            FeatureGroup::SubAgent, IDM_SUBAGENT_TODO_CLEAR_R, "!todo_clear", "",
            handleSubAgentTodoClear, true, true);
        reg("subagent.status", "SubAgent Status", "Sub-agent system status",
            FeatureGroup::SubAgent, IDM_SUBAGENT_STATUS_R, "!agents", "",
            handleSubAgentStatus, true, true);

        // ══════════════ TERMINAL ══════════════
        reg("terminal.new", "New Terminal", "Create new terminal pane",
            FeatureGroup::Terminal, IDM_TERMINAL_NEW_R, "!terminal_new", "Ctrl+`",
            handleTerminalNew, true, true);
        reg("terminal.splitH", "Split Terminal H", "Split terminal horizontally",
            FeatureGroup::Terminal, IDM_TERMINAL_SPLIT_H_R, "!terminal_split", "",
            handleTerminalSplitH, true, true);
        reg("terminal.splitV", "Split Terminal V", "Split terminal vertically",
            FeatureGroup::Terminal, IDM_TERMINAL_SPLIT_V_R, "!terminal_split_v", "",
            handleTerminalSplitV, true, true);
        reg("terminal.kill", "Kill Terminal", "Close terminal",
            FeatureGroup::Terminal, IDM_TERMINAL_KILL_R, "!terminal_kill", "",
            handleTerminalKill, true, true);
        reg("terminal.list", "List Terminals", "Show active terminals",
            FeatureGroup::Terminal, IDM_TERMINAL_LIST_R, "!terminal_list", "",
            handleTerminalList, true, true);

        // ══════════════ DEBUG ══════════════
        reg("debug.start", "Start Debugger", "Launch debugger",
            FeatureGroup::Debug, IDM_DEBUG_START_R, "!debug_start", "F5",
            handleDebugStart, true, true);
        reg("debug.stop", "Stop Debugger", "Stop debugging",
            FeatureGroup::Debug, IDM_DEBUG_STOP_R, "!debug_stop", "Shift+F5",
            handleDebugStop, true, true);
        reg("debug.step", "Step", "Step over",
            FeatureGroup::Debug, IDM_DEBUG_STEP_R, "!debug_step", "F10",
            handleDebugStep, true, true);
        reg("debug.continue", "Continue", "Resume execution",
            FeatureGroup::Debug, IDM_DEBUG_CONTINUE_R, "!debug_continue", "F5",
            handleDebugContinue, true, true);
        reg("debug.breakpointAdd", "Add Breakpoint", "Set breakpoint",
            FeatureGroup::Debug, IDM_BREAKPOINT_ADD_R, "!breakpoint_add", "F9",
            handleBreakpointAdd, true, true);
        reg("debug.breakpointList", "List Breakpoints", "Show all breakpoints",
            FeatureGroup::Debug, IDM_BREAKPOINT_LIST_R, "!breakpoint_list", "",
            handleBreakpointList, true, true);
        reg("debug.breakpointRemove", "Remove Breakpoint", "Delete breakpoint",
            FeatureGroup::Debug, IDM_BREAKPOINT_REMOVE_R, "!breakpoint_remove", "",
            handleBreakpointRemove, true, true);

        // ══════════════ HOTPATCH (3-Layer) ══════════════
        reg("hotpatch.apply", "Apply Hotpatch", "Apply a hotpatch preset",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_APPLY_R, "!hotpatch_apply", "",
            handleHotpatchApply, false, true, true); // CLI-only
        reg("hotpatch.create", "Create Hotpatch", "Create new hotpatch",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_CREATE_R, "!hotpatch_create", "",
            handleHotpatchCreate, false, true, true); // CLI-only
        reg("hotpatch.status", "Hotpatch Status", "All 3 layers status",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_STATUS_R, "!hotpatch_status", "",
            handleHotpatchStatus, true, true);
        reg("hotpatch.memory", "Memory Hotpatch", "Memory layer panel",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_MEMORY_R, "!hotpatch_mem", "",
            handleHotpatchMemory, true, true, true);
        reg("hotpatch.byte", "Byte Hotpatch", "Byte-level layer panel",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_BYTE_R, "!hotpatch_byte", "",
            handleHotpatchByte, true, true, true);
        reg("hotpatch.server", "Server Hotpatch", "Server layer panel",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_SERVER_R, "!hotpatch_server", "",
            handleHotpatchServer, true, true, true);

        // ══════════════ AI MODE ══════════════
        reg("ai.mode", "Set AI Mode", "Switch AI operating mode",
            FeatureGroup::AIMode, IDM_AI_MODE_R, "!mode", "",
            handleAIModeSet, false, true);  // CLI-only (no specific GUI command)
        reg("ai.engine", "Select Engine", "Choose inference engine",
            FeatureGroup::AIMode, IDM_AI_ENGINE_R, "!engine", "",
            handleAIEngineSelect, false, true);  // CLI-only
        reg("ai.deepThinking", "Deep Thinking", "Enable deep thinking mode",
            FeatureGroup::AIMode, IDM_AI_DEEP_THINKING_R, "!deep", "",
            handleAIDeepThinking, true, true);
        reg("ai.deepResearch", "Deep Research", "Enable research mode",
            FeatureGroup::AIMode, IDM_AI_DEEP_RESEARCH_R, "!research", "",
            handleAIDeepResearch, true, true);
        reg("ai.maxMode", "Max Mode", "All systems max power",
            FeatureGroup::AIMode, IDM_AI_MAX_MODE_R, "!max", "",
            handleAIMaxMode, true, true);

        // ══════════════ REVERSE ENGINEERING ══════════════
        reg("re.decisionTree", "Decision Tree", "Agentic decision tree",
            FeatureGroup::ReverseEng, IDM_RE_DECISION_TREE_R, "!decision_tree", "",
            handleREDecisionTree, true, true);
        reg("re.ssaLift", "SSA Lift", "Static single assignment lift",
            FeatureGroup::ReverseEng, IDM_RE_SSA_LIFT_R, "!ssa_lift", "",
            handleRESSALift, true, true, true); // MASM hot-path
        reg("re.autoPatch", "Auto Patch", "Automated binary patching",
            FeatureGroup::ReverseEng, IDM_RE_AUTO_PATCH_R, "!auto_patch", "",
            handleREAutoPatch, false, true, true);  // CLI-only
        reg("re.disassemble", "Disassemble", "Disassemble binary",
            FeatureGroup::ReverseEng, IDM_RE_DISASM_R, "!disasm", "",
            handleREDisassemble, true, true);
        reg("re.dumpbin", "Dumpbin", "PE analysis",
            FeatureGroup::ReverseEng, IDM_RE_DUMPBIN_R, "!dumpbin", "",
            handleREDumpbin, true, true);
        reg("re.cfgAnalysis", "CFG Analysis", "Control flow graph",
            FeatureGroup::ReverseEng, IDM_RE_CFG_R, "!cfg", "",
            handleRECFGAnalysis, true, true);

        // ══════════════ VOICE ══════════════
        reg("voice.init", "Voice Init", "Initialize voice engine",
            FeatureGroup::Voice, IDM_VOICE_INIT_R, "!voice init", "",
            handleVoiceInit, true, true);
        reg("voice.record", "Voice Record", "Start recording",
            FeatureGroup::Voice, IDM_VOICE_RECORD_R, "!voice record", "",
            handleVoiceRecord, true, true);
        reg("voice.transcribe", "Voice Transcribe", "Transcribe recording",
            FeatureGroup::Voice, IDM_VOICE_TRANSCRIBE_R, "!voice transcribe", "",
            handleVoiceTranscribe, true, true);
        reg("voice.speak", "Voice Speak", "Text-to-speech",
            FeatureGroup::Voice, IDM_VOICE_SPEAK_R, "!voice speak", "",
            handleVoiceSpeak, true, true);
        reg("voice.devices", "Voice Devices", "List audio devices",
            FeatureGroup::Voice, IDM_VOICE_DEVICES_R, "!voice devices", "",
            handleVoiceDevices, true, true);
        reg("voice.mode", "Voice Mode", "Configure voice mode",
            FeatureGroup::Voice, IDM_VOICE_MODE_R, "!voice mode", "",
            handleVoiceMode, true, true);
        reg("voice.status", "Voice Status", "Voice engine status",
            FeatureGroup::Voice, IDM_VOICE_STATUS_R, "!voice status", "",
            handleVoiceStatus, true, true);
        reg("voice.metrics", "Voice Metrics", "Voice performance metrics",
            FeatureGroup::Voice, IDM_VOICE_METRICS_R, "!voice metrics", "",
            handleVoiceMetrics, true, true);

        // ══════════════ HEADLESS SYSTEMS ══════════════
        reg("headless.safety", "Safety Check", "Run safety validation",
            FeatureGroup::Tools, 0, "!safety", "",
            handleSafety, true, true);
        reg("headless.confidence", "Confidence Score", "Confidence scoring",
            FeatureGroup::Tools, 0, "!confidence", "",
            handleConfidence, true, true);
        reg("headless.replay", "Replay Journal", "View replay journal",
            FeatureGroup::Tools, 0, "!replay", "",
            handleReplay, true, true);
        reg("headless.governor", "Governor", "Execution governor status",
            FeatureGroup::Tools, 0, "!governor", "",
            handleGovernor, true, true);
        reg("headless.multiResponse", "Multi Response", "Multi-response engine",
            FeatureGroup::Tools, 0, "!multi", "",
            handleMultiResponse, true, true);
        reg("headless.history", "History", "Agent history",
            FeatureGroup::Tools, 0, "!history", "",
            handleHistory, true, true);
        reg("headless.explain", "Explain", "Explainability panel",
            FeatureGroup::Tools, 0, "!explain", "",
            handleExplain, true, true);
        reg("headless.policy", "Policy", "Policy engine",
            FeatureGroup::Tools, 0, "!policy", "",
            handlePolicy, true, true);
        reg("headless.tools", "Tools List", "Available tools listing",
            FeatureGroup::Tools, 0, "!tools", "",
            handleTools, true, true);

        // ══════════════ SERVER ══════════════
        reg("server.start", "Start Server", "Start inference server",
            FeatureGroup::Server, IDM_SERVER_START_R, "!server start", "",
            handleServerStart, false, true);  // CLI-only (9100 collides with Monaco)
        reg("server.stop", "Stop Server", "Stop inference server",
            FeatureGroup::Server, IDM_SERVER_STOP_R, "!server stop", "",
            handleServerStop, false, true);
        reg("server.status", "Server Status", "Server health check",
            FeatureGroup::Server, IDM_SERVER_STATUS_R, "!server status", "",
            handleServerStatus, false, true);

        // ══════════════ GIT ══════════════
        reg("git.status", "Git Status", "Show repository status",
            FeatureGroup::Git, IDM_GIT_STATUS_R, "!git_status", "",
            handleGitStatus, true, true);
        reg("git.commit", "Git Commit", "Commit changes",
            FeatureGroup::Git, IDM_GIT_COMMIT_R, "!git_commit", "",
            handleGitCommit, true, true);
        reg("git.push", "Git Push", "Push to remote",
            FeatureGroup::Git, IDM_GIT_PUSH_R, "!git_push", "",
            handleGitPush, true, true);
        reg("git.pull", "Git Pull", "Pull from remote",
            FeatureGroup::Git, IDM_GIT_PULL_R, "!git_pull", "",
            handleGitPull, true, true);
        reg("git.diff", "Git Diff", "Show diff",
            FeatureGroup::Git, IDM_GIT_DIFF_R, "!git_diff", "",
            handleGitDiff, true, true);

        // ══════════════ THEMES ══════════════
        reg("theme.set", "Set Theme", "Change IDE theme",
            FeatureGroup::Themes, IDM_THEME_SET_R, "!theme", "",
            handleThemeSet, true, true);
        reg("theme.list", "List Themes", "Show available themes",
            FeatureGroup::Themes, IDM_THEME_LIST_R, "!theme_list", "",
            handleThemeList, true, true);

        // ══════════════ BACKEND / LLM ROUTER ══════════════
        reg("backend.list", "List Backends", "Show available AI backends",
            FeatureGroup::LLMRouter, IDM_BACKEND_LIST_R, "!backend list", "",
            handleBackendList, true, true);
        reg("backend.select", "Select Backend", "Choose AI backend",
            FeatureGroup::LLMRouter, IDM_BACKEND_SELECT_R, "!backend use", "",
            handleBackendSelect, true, true);
        reg("backend.status", "Backend Status", "Current backend info",
            FeatureGroup::LLMRouter, IDM_BACKEND_STATUS_R, "!backend status", "",
            handleBackendStatus, true, true);

        // ══════════════ SWARM ══════════════
        reg("swarm.join", "Join Swarm", "Join distributed cluster",
            FeatureGroup::Swarm, IDM_SWARM_JOIN_R, "!swarm_join", "",
            handleSwarmJoin, true, true);
        reg("swarm.status", "Swarm Status", "Cluster status",
            FeatureGroup::Swarm, IDM_SWARM_STATUS_R, "!swarm_status", "",
            handleSwarmStatus, true, true);
        reg("swarm.distribute", "Distribute Work", "Distribute across nodes",
            FeatureGroup::Swarm, IDM_SWARM_DISTRIBUTE_R, "!swarm_distribute", "",
            handleSwarmDistribute, true, true);
        reg("swarm.rebalance", "Rebalance Swarm", "Rebalance shard distribution",
            FeatureGroup::Swarm, IDM_SWARM_REBALANCE_R, "!swarm_rebalance", "",
            handleSwarmRebalance, true, true);
        reg("swarm.nodes", "List Swarm Nodes", "Show cluster nodes",
            FeatureGroup::Swarm, IDM_SWARM_NODES_R, "!swarm_nodes", "",
            handleSwarmNodes, true, true);
        reg("swarm.leave", "Leave Swarm", "Disconnect from cluster",
            FeatureGroup::Swarm, IDM_SWARM_LEAVE_R, "!swarm_leave", "",
            handleSwarmLeave, true, true);

        // ══════════════ SETTINGS ══════════════
        reg("settings.open", "Open Settings", "Settings panel",
            FeatureGroup::Settings, IDM_SETTINGS_OPEN_R, "!settings", "",
            handleSettingsOpen, false, true);  // CLI-only (9400 collides with PDB)
        reg("settings.export", "Export Settings", "Export to file",
            FeatureGroup::Settings, IDM_SETTINGS_EXPORT_R, "!settings_export", "",
            handleSettingsExport, false, true);
        reg("settings.import", "Import Settings", "Import from file",
            FeatureGroup::Settings, IDM_SETTINGS_IMPORT_R, "!settings_import", "",
            handleSettingsImport, false, true);

        // ══════════════ HELP ══════════════
        reg("help.about", "About", "About RawrXD IDE",
            FeatureGroup::Help, IDM_HELP_ABOUT_R, "!about", "F1",
            handleHelpAbout, true, true);
        reg("help.docs", "Documentation", "Open documentation",
            FeatureGroup::Help, IDM_HELP_DOCS_R, "!docs", "",
            handleHelpDocs, true, true);
        reg("help.shortcuts", "Keyboard Shortcuts", "List all shortcuts",
            FeatureGroup::Help, IDM_HELP_SHORTCUTS_R, "!shortcuts", "",
            handleHelpShortcuts, true, true);

        // ══════════════ MANIFEST ══════════════
        reg("manifest.json", "Manifest JSON", "Generate feature manifest as JSON",
            FeatureGroup::Help, 0, "!manifest_json", "",
            handleManifestJSON, true, true);
        reg("manifest.markdown", "Manifest Markdown", "Generate feature manifest as MD",
            FeatureGroup::Help, 0, "!manifest_md", "",
            handleManifestMarkdown, true, true);
        reg("manifest.selfTest", "Self-Test", "Run feature self-tests",
            FeatureGroup::Help, 0, "!self_test", "",
            handleManifestSelfTest, true, true);

        // ══════════════ CLI-ONLY (previously in legacy chain only) ══════════════
        reg("cli.search", "Search Files", "Search for text across files",
            FeatureGroup::Tools, 0, "!search", "",
            handleSearch, false, true);
        reg("cli.analyze", "Analyze", "Analyze current context",
            FeatureGroup::Tools, 0, "!analyze", "",
            handleAnalyze, false, true);
        reg("cli.profile", "Profile", "Performance profiling",
            FeatureGroup::Performance, 0, "!profile", "",
            handleProfile, false, true);
        reg("cli.subagent", "SubAgent", "Launch sub-agent",
            FeatureGroup::SubAgent, 0, "!subagent", "",
            handleSubAgent, false, true);
        reg("cli.cot", "Chain of Thought", "Toggle chain-of-thought",
            FeatureGroup::AIMode, 0, "!cot", "",
            handleCOT, false, true);
        reg("cli.status", "Status", "Show system status",
            FeatureGroup::Tools, 0, "!status", "",
            handleStatus, false, true);
        reg("cli.help", "Help", "Show help text",
            FeatureGroup::Help, 0, "!help", "",
            handleHelp, false, true);
        reg("cli.generateIDE", "Generate IDE", "Generate React IDE server",
            FeatureGroup::Tools, 0, "!generate_ide", "",
            handleGenerateIDE, false, true);
    }
} g_featureRegistrar;
