// ============================================================================
// command_registry.cpp — RawrXD Unified Command Registry Implementation
// ============================================================================
#include "command_registry.h"
#include "../ide_constants.h"
#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// ---------------------------------------------------------------------------
CommandRegistry& CommandRegistry::instance() {
    static CommandRegistry s_instance;
    return s_instance;
}

bool CommandRegistry::registerCommand(CommandDescriptor desc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_commands.count(desc.id)) return false;
    m_commands[desc.id] = std::move(desc);
    return true;
}

bool CommandRegistry::overrideCommand(const std::string& id,
    std::function<CommandResult(const CommandArgs&)> handler,
    const std::string& ownerExtId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_commands.count(id)) return false;
    m_overrideStack[id].push_back({ ownerExtId, handler });
    return true;
}

void CommandRegistry::releaseOverride(const std::string& id, const std::string& ownerExtId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_overrideStack.find(id);
    if (it == m_overrideStack.end()) return;
    auto& stack = it->second;
    for (auto sit = stack.rbegin(); sit != stack.rend(); ++sit) {
        if (sit->first == ownerExtId) {
            stack.erase(std::next(sit).base());
            return;
        }
    }
}

CommandResult CommandRegistry::execute(const std::string& id, const CommandArgs& args) {
    std::function<CommandResult(const CommandArgs&)> handler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_commands.find(id);
        if (it == m_commands.end())
            return { false, "", "Unknown command: " + id };
        if (!it->second.enabled)
            return { false, "", "Command disabled: " + id };

        // Check override stack first
        auto oit = m_overrideStack.find(id);
        if (oit != m_overrideStack.end() && !oit->second.empty()) {
            handler = oit->second.back().second;
        } else {
            handler = it->second.handler;
        }
    }
    if (!handler)
        return { false, "", "No handler for command: " + id };
    try {
        return handler(args);
    } catch (const std::exception& e) {
        return { false, "", std::string("Command threw: ") + e.what() };
    }
}

CommandResult CommandRegistry::executeByMenuId(int menuId, const CommandArgs& args) {
    std::string cmdId;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_menuMap.find(menuId);
        if (it == m_menuMap.end())
            return { false, "", "No command mapped to menu ID " + std::to_string(menuId) };
        cmdId = it->second;
    }
    return execute(cmdId, args);
}

bool CommandRegistry::hasCommand(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_commands.count(id) > 0;
}

std::optional<CommandDescriptor> CommandRegistry::getDescriptor(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_commands.find(id);
    if (it == m_commands.end()) return std::nullopt;
    return it->second;
}

std::vector<CommandDescriptor> CommandRegistry::enumerate(unsigned int accessFilter) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CommandDescriptor> out;
    out.reserve(m_commands.size());
    for (const auto& [k, v] : m_commands) {
        if ((v.accessModes & accessFilter) && v.enabled)
            out.push_back(v);
    }
    std::sort(out.begin(), out.end(), [](const CommandDescriptor& a, const CommandDescriptor& b) {
        if (a.category != b.category) return a.category < b.category;
        return a.displayName < b.displayName;
    });
    return out;
}

void CommandRegistry::setEnabled(const std::string& id, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_commands.find(id);
    if (it != m_commands.end()) it->second.enabled = enabled;
}

void CommandRegistry::registerMenuMapping(int menuId, const std::string& commandId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_menuMap[menuId] = commandId;
}

std::string CommandRegistry::commandIdForMenuId(int menuId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_menuMap.find(menuId);
    return it != m_menuMap.end() ? it->second : "";
}

// ============================================================================
// Built-in command registrations
// ============================================================================
// Forward declarations for handlers (defined in respective cpp files)
static CommandResult stub_noop(const CommandArgs&) { return { true }; }

void CommandRegistry::registerBuiltins() {
    auto& reg = CommandRegistry::instance();

    // ---- FILE ---------------------------------------------------------------
    reg.registerMenuMapping(IDM_FILE_NEW,       "file.newFile");
    reg.registerMenuMapping(IDM_FILE_OPEN,      "file.openFile");
    reg.registerMenuMapping(IDM_FILE_OPEN_FOLDER, "file.openFolder");
    reg.registerMenuMapping(IDM_FILE_SAVE,      "file.save");
    reg.registerMenuMapping(IDM_FILE_SAVEAS,    "file.saveAs");
    reg.registerMenuMapping(IDM_FILE_CLOSE_TAB, "file.closeTab");
    reg.registerMenuMapping(IDM_FILE_CLOSE_FOLDER, "file.closeFolder");
    reg.registerMenuMapping(IDM_FILE_AUTOSAVE,  "file.toggleAutoSave");
    reg.registerMenuMapping(IDM_FILE_NEW_WINDOW,"file.newWindow");
    reg.registerMenuMapping(IDM_FILE_EXIT,      "app.quit");

    RAWRXD_REGISTER_CMD("file.newFile",       "New File",            "File",       "Ctrl+N",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.openFile",      "Open File...",        "File",       "Ctrl+O",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.openFolder",    "Open Folder...",      "File",       "Ctrl+K Ctrl+O",CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.save",          "Save",                "File",       "Ctrl+S",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.saveAs",        "Save As...",          "File",       "Ctrl+Shift+S", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.saveAll",       "Save All",            "File",       "Ctrl+K S",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.closeTab",      "Close Tab",           "File",       "Ctrl+W",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.closeAll",      "Close All Tabs",      "File",       "Ctrl+K W",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.closeFolder",   "Close Folder",        "File",       "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.toggleAutoSave","Toggle Auto Save",    "File",       "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.newWindow",     "New Window",          "File",       "Ctrl+Shift+N", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("file.revertFile",    "Revert File",         "File",       "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("app.quit",           "Quit",                "Application","Alt+F4",       CMD_ACCESS_ALL, stub_noop);

    // ---- EDIT ---------------------------------------------------------------
    reg.registerMenuMapping(IDM_EDIT_UNDO,       "editor.undo");
    reg.registerMenuMapping(IDM_EDIT_REDO,       "editor.redo");
    reg.registerMenuMapping(IDM_EDIT_CUT,        "editor.cut");
    reg.registerMenuMapping(IDM_EDIT_COPY,       "editor.copy");
    reg.registerMenuMapping(IDM_EDIT_PASTE,      "editor.paste");
    reg.registerMenuMapping(IDM_EDIT_FIND,       "editor.find");
    reg.registerMenuMapping(IDM_EDIT_REPLACE,    "editor.replace");
    reg.registerMenuMapping(IDM_EDIT_SELECTALL,  "editor.selectAll");
    reg.registerMenuMapping(IDM_EDIT_GOTO_LINE,  "editor.gotoLine");
    reg.registerMenuMapping(IDM_EDIT_MULTICURSOR_ADD, "editor.addCursor");

    RAWRXD_REGISTER_CMD("editor.undo",           "Undo",                  "Edit",  "Ctrl+Z",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.redo",           "Redo",                  "Edit",  "Ctrl+Y",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.cut",            "Cut",                   "Edit",  "Ctrl+X",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.copy",           "Copy",                  "Edit",  "Ctrl+C",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.paste",          "Paste",                 "Edit",  "Ctrl+V",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.find",           "Find",                  "Edit",  "Ctrl+F",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.replace",        "Replace",               "Edit",  "Ctrl+H",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.findInFiles",    "Find in Files",         "Edit",  "Ctrl+Shift+F", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.replaceInFiles", "Replace in Files",      "Edit",  "Ctrl+Shift+H", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.selectAll",      "Select All",            "Edit",  "Ctrl+A",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.gotoLine",       "Go to Line...",         "Edit",  "Ctrl+G",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.gotoFile",       "Go to File...",         "Edit",  "Ctrl+P",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.gotoDefinition", "Go to Definition",      "Edit",  "F12",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.gotoDeclaration","Go to Declaration",     "Edit",  "Ctrl+F12",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.gotoSymbol",     "Go to Symbol in File",  "Edit",  "Ctrl+Shift+O", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.addCursor",      "Add Cursor",            "Edit",  "Ctrl+Alt+Down",CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.formatDocument", "Format Document",       "Edit",  "Shift+Alt+F",  CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.toggleComment",  "Toggle Line Comment",   "Edit",  "Ctrl+/",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.indentLines",    "Indent Lines",          "Edit",  "Ctrl+]",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.outdentLines",   "Outdent Lines",         "Edit",  "Ctrl+[",       CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.renameSymbol",   "Rename Symbol",         "Edit",  "F2",           CMD_ACCESS_ALL, stub_noop);

    // ---- VIEW ---------------------------------------------------------------
    reg.registerMenuMapping(IDM_VIEW_TOGGLE_SIDEBAR,    "view.toggleSidebar");
    reg.registerMenuMapping(IDM_VIEW_TOGGLE_TERMINAL,   "view.toggleTerminal");
    reg.registerMenuMapping(IDM_VIEW_TOGGLE_OUTPUT,     "view.toggleOutput");
    reg.registerMenuMapping(IDM_VIEW_TOGGLE_FULLSCREEN, "view.toggleFullscreen");
    reg.registerMenuMapping(IDM_VIEW_ZOOM_IN,           "view.zoomIn");
    reg.registerMenuMapping(IDM_VIEW_ZOOM_OUT,          "view.zoomOut");
    reg.registerMenuMapping(IDM_VIEW_ZOOM_RESET,        "view.zoomReset");

    RAWRXD_REGISTER_CMD("view.toggleSidebar",      "Toggle Sidebar",         "View", "Ctrl+B",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.toggleTerminal",     "Toggle Terminal",         "View", "Ctrl+`",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.toggleOutput",       "Toggle Output Panel",     "View", "Ctrl+Shift+U",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.togglePanel",        "Toggle Panel",            "View", "Ctrl+J",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.toggleFullscreen",   "Toggle Full Screen",      "View", "F11",              CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.zoomIn",             "Zoom In",                 "View", "Ctrl+=",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.zoomOut",            "Zoom Out",                "View", "Ctrl+-",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.zoomReset",          "Reset Zoom",              "View", "Ctrl+0",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.splitEditorRight",   "Split Editor Right",      "View", "Ctrl+\\",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.splitEditorDown",    "Split Editor Down",       "View", "Ctrl+K Ctrl+\\",   CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.focusNextTab",       "Focus Next Tab",          "View", "Ctrl+Tab",         CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.focusPrevTab",       "Focus Previous Tab",      "View", "Ctrl+Shift+Tab",   CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.openProblems",       "Open Problems Panel",     "View", "Ctrl+Shift+M",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.activityBarExplorer","Show Explorer",           "View", "Ctrl+Shift+E",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.activityBarSearch",  "Show Search",             "View", "Ctrl+Shift+F",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.activityBarGit",     "Show Source Control",     "View", "Ctrl+Shift+G",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.activityBarDebug",   "Show Run & Debug",        "View", "Ctrl+Shift+D",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("view.activityBarExtensions","Show Extensions",       "View", "Ctrl+Shift+X",     CMD_ACCESS_ALL, stub_noop);

    // ---- AI / COPILOT -------------------------------------------------------
    reg.registerMenuMapping(IDM_AI_INLINE_COMPLETE,  "ai.inlineComplete");
    reg.registerMenuMapping(IDM_AI_CHAT_MODE,        "ai.openChat");
    reg.registerMenuMapping(IDM_AI_EXPLAIN_CODE,     "ai.explainCode");
    reg.registerMenuMapping(IDM_AI_REFACTOR,         "ai.refactorCode");
    reg.registerMenuMapping(IDM_AI_GENERATE_TESTS,   "ai.generateTests");
    reg.registerMenuMapping(IDM_AI_GENERATE_DOCS,    "ai.generateDocs");
    reg.registerMenuMapping(IDM_AI_FIX_ERRORS,       "ai.fixErrors");
    reg.registerMenuMapping(IDM_AI_OPTIMIZE_CODE,    "ai.optimizeCode");
    reg.registerMenuMapping(IDM_AI_MODEL_SELECT,     "ai.selectModel");

    RAWRXD_REGISTER_CMD("ai.inlineComplete",     "Trigger Inline Completion", "AI", "Alt+\\",         CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.openChat",           "Open AI Chat",              "AI", "Ctrl+Shift+I",   CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.explainCode",        "Explain Code",              "AI", "Ctrl+Shift+E",   CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.refactorCode",       "Refactor Code",             "AI", "",               CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.generateTests",      "Generate Tests",            "AI", "",               CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.generateDocs",       "Generate Documentation",    "AI", "",               CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.fixErrors",          "Fix Errors",                "AI", "",               CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.optimizeCode",       "Optimize Code",             "AI", "",               CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.selectModel",        "Select AI Model...",        "AI", "",               CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.acceptSuggestion",   "Accept AI Suggestion",      "AI", "Tab",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.dismissSuggestion",  "Dismiss AI Suggestion",     "AI", "Escape",         CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.nextSuggestion",     "Next AI Suggestion",        "AI", "Alt+]",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("ai.prevSuggestion",     "Previous AI Suggestion",    "AI", "Alt+[",          CMD_ACCESS_ALL, stub_noop);

    // GitHub Copilot compatible command IDs
    RAWRXD_REGISTER_CMD("github.copilot.generate",      "Copilot: Generate",          "Copilot", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("github.copilot.chat.open",     "Copilot: Open Chat",         "Copilot", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("github.copilot.explain",       "Copilot: Explain Code",      "Copilot", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("github.copilot.fix",           "Copilot: Fix This",          "Copilot", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("github.copilot.tests",         "Copilot: Generate Tests",    "Copilot", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("github.copilot.review",        "Copilot: Review Code",       "Copilot", "", CMD_ACCESS_ALL, stub_noop);

    // Amazon Q compatible command IDs
    RAWRXD_REGISTER_CMD("aws.amazonq.chat.open",        "Amazon Q: Open Chat",        "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("aws.amazonq.explainCode",      "Amazon Q: Explain Code",     "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("aws.amazonq.refactorCode",     "Amazon Q: Refactor Code",    "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("aws.amazonq.generateTests",    "Amazon Q: Generate Tests",   "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("aws.amazonq.fixCode",          "Amazon Q: Fix Code",         "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("aws.amazonq.scanCode",         "Amazon Q: Security Scan",    "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("aws.amazonq.sendToPrompt",     "Amazon Q: Send to Prompt",   "Amazon Q", "", CMD_ACCESS_ALL, stub_noop);

    // ---- AGENTIC ------------------------------------------------------------
    RAWRXD_REGISTER_CMD("agent.run",             "Run Agent",                 "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.stop",            "Stop Agent",                "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.history",         "Agent History",             "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.settings",        "Agent Settings",            "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.approve",         "Approve Agent Action",      "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.reject",          "Reject Agent Action",       "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.openPlan",        "View Agent Plan",           "Agent", "",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("agent.swarmStart",      "Start Agent Swarm",         "Agent", "",            CMD_ACCESS_ALL & ~CMD_ACCESS_MENU, stub_noop);
    RAWRXD_REGISTER_CMD("agent.swarmStop",       "Stop Agent Swarm",          "Agent", "",            CMD_ACCESS_ALL & ~CMD_ACCESS_MENU, stub_noop);

    // ---- GIT / SOURCE CONTROL -----------------------------------------------
    RAWRXD_REGISTER_CMD("git.commit",            "Git: Commit",               "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.commitAll",         "Git: Commit All",           "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.push",              "Git: Push",                 "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.pull",              "Git: Pull",                 "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.fetch",             "Git: Fetch",                "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.status",            "Git: Status",               "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.checkout",          "Git: Checkout Branch...",   "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.createBranch",      "Git: Create Branch...",     "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.stash",             "Git: Stash",                "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.stashPop",          "Git: Pop Stash",            "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.showLog",           "Git: Log",                  "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.openChanges",       "Git: Open Changes",         "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.stageAll",          "Git: Stage All Changes",    "Git",  "",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("git.unstageAll",        "Git: Unstage All Changes",  "Git",  "",             CMD_ACCESS_ALL, stub_noop);

    // ---- DEBUG -------------------------------------------------------------
    reg.registerMenuMapping(IDM_TOOLS_DEBUG, "debug.start");

    RAWRXD_REGISTER_CMD("debug.start",           "Start Debugging",           "Debug","F5",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.stop",            "Stop Debugging",            "Debug","Shift+F5",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.restart",         "Restart Debugging",         "Debug","Ctrl+Shift+F5",CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.stepOver",        "Step Over",                 "Debug","F10",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.stepInto",        "Step Into",                 "Debug","F11",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.stepOut",         "Step Out",                  "Debug","Shift+F11",    CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.continue",        "Continue",                  "Debug","F5",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.toggleBreakpoint","Toggle Breakpoint",         "Debug","F9",           CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.addWatch",        "Add to Watch",              "Debug","",             CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("debug.openConsole",     "Open Debug Console",        "Debug","Ctrl+Shift+Y", CMD_ACCESS_ALL, stub_noop);

    // ---- EXTENSIONS --------------------------------------------------------
    reg.registerMenuMapping(IDM_TOOLS_EXTENSIONS, "extensions.marketplace");

    RAWRXD_REGISTER_CMD("extensions.marketplace",      "Open Extensions Marketplace","Extensions","Ctrl+Shift+X", CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.install",          "Install Extension...",        "Extensions","",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.installFromVSIX",  "Install from VSIX...",        "Extensions","",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.manage",           "Manage Extensions",           "Extensions","",            CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.enableAll",        "Enable All Extensions",       "Extensions","",            CMD_ACCESS_ALL & ~CMD_ACCESS_MENU, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.disableAll",       "Disable All Extensions",      "Extensions","",            CMD_ACCESS_ALL & ~CMD_ACCESS_MENU, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.listInstalled",    "List Installed Extensions",   "Extensions","",            CMD_ACCESS_API|CMD_ACCESS_AGENTIC|CMD_ACCESS_EXTENSION, stub_noop);
    RAWRXD_REGISTER_CMD("extensions.reload",           "Reload Extension Host",       "Extensions","",            CMD_ACCESS_ALL, stub_noop);

    // ---- TOOLS / SETTINGS --------------------------------------------------
    reg.registerMenuMapping(IDM_TOOLS_COMMAND_PALETTE, "workbench.action.showCommands");
    reg.registerMenuMapping(IDM_TOOLS_SETTINGS,        "workbench.action.openSettings");
    reg.registerMenuMapping(IDM_TOOLS_TERMINAL,        "workbench.action.terminal.new");
    reg.registerMenuMapping(IDM_TOOLS_BUILD,           "workbench.action.tasks.build");
    reg.registerMenuMapping(IDM_HELP_SHORTCUTS,        "workbench.action.openGlobalKeybindings");
    reg.registerMenuMapping(IDM_HELP_DOCS,             "workbench.action.openDocumentation");
    reg.registerMenuMapping(IDM_HELP_ABOUT,            "workbench.action.showAboutDialog");

    RAWRXD_REGISTER_CMD("workbench.action.showCommands",       "Show All Commands",              "Workbench","Ctrl+Shift+P",CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.openSettings",       "Open Settings",                  "Workbench","Ctrl+,",      CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.openSettingsJson",   "Open Settings (JSON)",           "Workbench","",            CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.openGlobalKeybindings","Open Keyboard Shortcuts",      "Workbench","Ctrl+K Ctrl+S",CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.selectTheme",        "Color Theme...",                 "Workbench","Ctrl+K Ctrl+T",CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.terminal.new",       "New Terminal",                   "Terminal", "Ctrl+`",      CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.terminal.kill",      "Kill Terminal",                  "Terminal", "",            CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.terminal.clear",     "Clear Terminal",                 "Terminal", "",            CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.tasks.build",        "Run Build Task",                 "Tasks",    "Ctrl+Shift+B",CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.tasks.runTask",      "Run Task...",                    "Tasks",    "",            CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.openDocumentation",  "Open Documentation",             "Help",     "",            CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.showAboutDialog",    "About RawrXD",                   "Help",     "",            CMD_ACCESS_ALL,  stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.reloadWindow",       "Reload Window",                  "Developer","Ctrl+Shift+P",CMD_ACCESS_ALL & ~CMD_ACCESS_MENU, stub_noop);
    RAWRXD_REGISTER_CMD("workbench.action.toggleDevTools",     "Toggle Developer Tools",         "Developer","Ctrl+Shift+I",CMD_ACCESS_ALL & ~CMD_ACCESS_MENU, stub_noop);

    // ---- LANGUAGE / LSP ----------------------------------------------------
    RAWRXD_REGISTER_CMD("editor.action.triggerSuggest",     "Trigger Suggest",           "Language","Ctrl+Space",  CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.action.triggerParameterHints","Parameter Hints",         "Language","Ctrl+Shift+Space",CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.action.peekDefinition",     "Peek Definition",           "Language","Alt+F12",     CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.action.findReferences",     "Find All References",       "Language","Shift+F12",   CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.action.quickFix",           "Quick Fix...",              "Language","Ctrl+.",      CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("editor.action.codeAction",         "Code Action...",            "Language","",            CMD_ACCESS_ALL, stub_noop);

    // ---- REVERSE ENGINEERING (RawrXD-specific) -----------------------------
    RAWRXD_REGISTER_CMD("rawrxd.reverseEngineer",    "Reverse Engineer File...",  "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.openDumpbin",        "Open Dumpbin Analysis",     "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.openBenchmark",      "Open Benchmark Panel",      "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.modelBrowser",       "Open Model Browser",        "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.loadModel",          "Load GGUF Model...",        "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.unloadModel",        "Unload Current Model",      "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.openMetrics",        "Open Metrics Dashboard",    "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
    RAWRXD_REGISTER_CMD("rawrxd.openTelemetry",      "Open Telemetry View",       "RawrXD","",          CMD_ACCESS_ALL, stub_noop);
}
