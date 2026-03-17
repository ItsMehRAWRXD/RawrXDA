// ============================================================================
// feature_registration.cpp — Master Feature Registration Table
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Static-initializes all features into SharedFeatureRegistry.
// Both CLI and Win32 GUI see the same features after this runs.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "shared_feature_dispatch.h"
#include "feature_handlers.h"

// ============================================================================
// WIN32 COMMAND IDs — from Win32IDE.h (keep in sync)
// ============================================================================

// File range: 1000-1099
#define IDM_FILE_NEW          1001
#define IDM_FILE_OPEN         1002
#define IDM_FILE_SAVE         1003
#define IDM_FILE_SAVE_AS      1004
#define IDM_FILE_SAVE_ALL     1005
#define IDM_FILE_CLOSE        1006
#define IDM_FILE_RECENT       1010
#define IDM_FILE_LOAD_MODEL   1030
#define IDM_FILE_MODEL_HF     1031
#define IDM_FILE_MODEL_OLLAMA 1032
#define IDM_FILE_MODEL_URL    1033
#define IDM_FILE_MODEL_UNIFIED 1034
#define IDM_FILE_QUICK_LOAD   1035

// Edit range: 2000-2099
#define IDM_EDIT_UNDO         2001
#define IDM_EDIT_REDO         2002
#define IDM_EDIT_CUT          2003
#define IDM_EDIT_COPY         2004
#define IDM_EDIT_PASTE        2005
#define IDM_EDIT_FIND         2010
#define IDM_EDIT_REPLACE      2011
#define IDM_EDIT_SELECT_ALL   2012

// Agent range: 4100-4199
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

// SubAgent: 4110-4119
#define IDM_SUBAGENT_CHAIN_R       4110
#define IDM_SUBAGENT_SWARM_R       4111
#define IDM_SUBAGENT_TODO_LIST_R   4112
#define IDM_SUBAGENT_TODO_CLEAR_R  4113
#define IDM_SUBAGENT_STATUS_R      4114

// Autonomy: 4150-4199
#define IDM_AUTONOMY_TOGGLE_R      4150
#define IDM_AUTONOMY_START_R       4151
#define IDM_AUTONOMY_STOP_R        4152
#define IDM_AUTONOMY_GOAL_R        4153
#define IDM_AUTONOMY_RATE_R        4154
#define IDM_AUTONOMY_RUN_R         4155

// Terminal: 4000-4099
#define IDM_TERMINAL_NEW_R         4001
#define IDM_TERMINAL_SPLIT_H_R    4007
#define IDM_TERMINAL_SPLIT_V_R    4008
#define IDM_TERMINAL_KILL_R        4006
#define IDM_TERMINAL_LIST_R        4010

// Debug: 4400-4499
#define IDM_DEBUG_START_R          4400
#define IDM_DEBUG_STOP_R           4401
#define IDM_DEBUG_STEP_R           4402
#define IDM_DEBUG_CONTINUE_R       4403
#define IDM_BREAKPOINT_ADD_R       4410
#define IDM_BREAKPOINT_LIST_R      4411
#define IDM_BREAKPOINT_REMOVE_R    4412

// Hotpatch: 9000-9099
#define IDM_HOTPATCH_APPLY_R       9000
#define IDM_HOTPATCH_CREATE_R      9001
#define IDM_HOTPATCH_STATUS_R      9002
#define IDM_HOTPATCH_MEMORY_R      9003
#define IDM_HOTPATCH_BYTE_R        9004
#define IDM_HOTPATCH_SERVER_R      9005

// AI Mode: 4700-4799
#define IDM_AI_MODE_R              4700
#define IDM_AI_ENGINE_R            4701
#define IDM_AI_DEEP_THINKING_R     4702
#define IDM_AI_DEEP_RESEARCH_R     4703
#define IDM_AI_MAX_MODE_R          4704

// RE: 4600-4699
#define IDM_RE_DECISION_TREE_R     4600
#define IDM_RE_SSA_LIFT_R          4601
#define IDM_RE_AUTO_PATCH_R        4602
#define IDM_RE_DISASM_R            4603
#define IDM_RE_DUMPBIN_R           4604
#define IDM_RE_CFG_R               4605

// Voice: 4A00-4A99
#define IDM_VOICE_INIT_R           0x4A00
#define IDM_VOICE_RECORD_R         0x4A01
#define IDM_VOICE_TRANSCRIBE_R     0x4A02
#define IDM_VOICE_SPEAK_R          0x4A03
#define IDM_VOICE_DEVICES_R        0x4A04
#define IDM_VOICE_MODE_R           0x4A05
#define IDM_VOICE_STATUS_R         0x4A06
#define IDM_VOICE_METRICS_R        0x4A07

// Server: 9100-9199
#define IDM_SERVER_START_R         9100
#define IDM_SERVER_STOP_R          9101
#define IDM_SERVER_STATUS_R        9102

// Git: 8000-8099
#define IDM_GIT_STATUS_R           8000
#define IDM_GIT_COMMIT_R           8001
#define IDM_GIT_PUSH_R             8002
#define IDM_GIT_PULL_R             8003
#define IDM_GIT_DIFF_R             8004

// Theme: 9500-9599
#define IDM_THEME_SET_R            9500
#define IDM_THEME_LIST_R           9501

// Backend/LLM Router: 4800-4899
#define IDM_BACKEND_LIST_R         4800
#define IDM_BACKEND_SELECT_R       4801
#define IDM_BACKEND_STATUS_R       4802

// Swarm: 4900-4999
#define IDM_SWARM_JOIN_R           4900
#define IDM_SWARM_STATUS_R         4901
#define IDM_SWARM_DISTRIBUTE_R     4902
#define IDM_SWARM_REBALANCE_R      4903
#define IDM_SWARM_NODES_R          4904
#define IDM_SWARM_LEAVE_R          4905

// Settings: 9400-9499
#define IDM_SETTINGS_OPEN_R        9400
#define IDM_SETTINGS_EXPORT_R      9401
#define IDM_SETTINGS_IMPORT_R      9402

// Help: 7000-7099
#define IDM_HELP_ABOUT_R           7000
#define IDM_HELP_DOCS_R            7001
#define IDM_HELP_SHORTCUTS_R       7002

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
            FeatureGroup::FileOps, IDM_FILE_NEW, "!new", "Ctrl+N",
            handleFileNew, true, true);
        reg("file.open", "Open File", "Open an existing file with dialog",
            FeatureGroup::FileOps, IDM_FILE_OPEN, "!open", "Ctrl+O",
            handleFileOpen, true, true);
        reg("file.save", "Save File", "Save current file to disk",
            FeatureGroup::FileOps, IDM_FILE_SAVE, "!save", "Ctrl+S",
            handleFileSave, true, true);
        reg("file.saveAs", "Save As", "Save current file with new name",
            FeatureGroup::FileOps, IDM_FILE_SAVE_AS, "!save_as", "Ctrl+Shift+S",
            handleFileSaveAs, true, true);
        reg("file.saveAll", "Save All", "Save all modified files",
            FeatureGroup::FileOps, IDM_FILE_SAVE_ALL, "!save_all", "",
            handleFileSaveAll, true, true);
        reg("file.close", "Close File", "Close current file",
            FeatureGroup::FileOps, IDM_FILE_CLOSE, "!close", "Ctrl+W",
            handleFileClose, true, true);
        reg("file.recentFiles", "Recent Files", "Open from recent files",
            FeatureGroup::FileOps, IDM_FILE_RECENT, "!recent", "",
            handleFileRecentFiles, true, true);
        reg("file.loadModel", "Load GGUF Model", "Load a GGUF model file",
            FeatureGroup::FileOps, IDM_FILE_LOAD_MODEL, "!model_load", "",
            handleFileLoadModel, true, true);
        reg("file.modelFromHF", "Model from HuggingFace", "Download model from HF",
            FeatureGroup::FileOps, IDM_FILE_MODEL_HF, "!model_hf", "",
            handleFileModelFromHF, true, true);
        reg("file.modelFromOllama", "Model from Ollama", "Load from Ollama",
            FeatureGroup::FileOps, IDM_FILE_MODEL_OLLAMA, "!model_ollama", "",
            handleFileModelFromOllama, true, true);
        reg("file.modelFromURL", "Model from URL", "Download from URL",
            FeatureGroup::FileOps, IDM_FILE_MODEL_URL, "!model_url", "",
            handleFileModelFromURL, true, true);
        reg("file.modelUnified", "Unified Model Load", "Universal loader",
            FeatureGroup::FileOps, IDM_FILE_MODEL_UNIFIED, "!model_unified", "",
            handleFileUnifiedLoad, true, true);
        reg("file.quickLoad", "Quick Load Model", "One-click local cache load",
            FeatureGroup::FileOps, IDM_FILE_QUICK_LOAD, "!quick_load", "",
            handleFileQuickLoad, true, true);

        // ══════════════ EDITING ══════════════
        reg("edit.undo", "Undo", "Undo last action",
            FeatureGroup::Editing, IDM_EDIT_UNDO, "!undo", "Ctrl+Z",
            handleEditUndo, true, true);
        reg("edit.redo", "Redo", "Redo last undone action",
            FeatureGroup::Editing, IDM_EDIT_REDO, "!redo", "Ctrl+Y",
            handleEditRedo, true, true);
        reg("edit.cut", "Cut", "Cut selection to clipboard",
            FeatureGroup::Editing, IDM_EDIT_CUT, "!cut", "Ctrl+X",
            handleEditCut, true, true);
        reg("edit.copy", "Copy", "Copy selection to clipboard",
            FeatureGroup::Editing, IDM_EDIT_COPY, "!copy", "Ctrl+C",
            handleEditCopy, true, true);
        reg("edit.paste", "Paste", "Paste from clipboard",
            FeatureGroup::Editing, IDM_EDIT_PASTE, "!paste", "Ctrl+V",
            handleEditPaste, true, true);
        reg("edit.find", "Find", "Search for text",
            FeatureGroup::Editing, IDM_EDIT_FIND, "!find", "Ctrl+F",
            handleEditFind, true, true);
        reg("edit.replace", "Replace", "Find and replace text",
            FeatureGroup::Editing, IDM_EDIT_REPLACE, "!replace", "Ctrl+H",
            handleEditReplace, true, true);
        reg("edit.selectAll", "Select All", "Select entire buffer",
            FeatureGroup::Editing, IDM_EDIT_SELECT_ALL, "!select_all", "Ctrl+A",
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
            handleAutonomyRate, true, true);
        reg("autonomy.run", "Autonomy Run", "Execute decision tree",
            FeatureGroup::Autonomy, IDM_AUTONOMY_RUN_R, "!autonomy_run", "",
            handleAutonomyRun, true, true);
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
            handleHotpatchApply, true, true, true); // MASM hot-path
        reg("hotpatch.create", "Create Hotpatch", "Create new hotpatch",
            FeatureGroup::Hotpatch, IDM_HOTPATCH_CREATE_R, "!hotpatch_create", "",
            handleHotpatchCreate, true, true, true);
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
            handleAIModeSet, true, true);
        reg("ai.engine", "Select Engine", "Choose inference engine",
            FeatureGroup::AIMode, IDM_AI_ENGINE_R, "!engine", "",
            handleAIEngineSelect, true, true);
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
            handleREAutoPatch, true, true, true);
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
            handleServerStart, true, true);
        reg("server.stop", "Stop Server", "Stop inference server",
            FeatureGroup::Server, IDM_SERVER_STOP_R, "!server stop", "",
            handleServerStop, true, true);
        reg("server.status", "Server Status", "Server health check",
            FeatureGroup::Server, IDM_SERVER_STATUS_R, "!server status", "",
            handleServerStatus, true, true);

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
            handleSettingsOpen, true, true);
        reg("settings.export", "Export Settings", "Export to file",
            FeatureGroup::Settings, IDM_SETTINGS_EXPORT_R, "!settings_export", "",
            handleSettingsExport, true, true);
        reg("settings.import", "Import Settings", "Import from file",
            FeatureGroup::Settings, IDM_SETTINGS_IMPORT_R, "!settings_import", "",
            handleSettingsImport, true, true);

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
    }
} g_featureRegistrar;
