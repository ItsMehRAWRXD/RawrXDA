/**
 * @file command_registry.h
 * @brief Production-ready command registry for RawrXD IDE
 * 
 * This system provides:
 * - Unified command registration for CLI and GUI
 * - Keybinding management with accelerator table generation
 * - Context-aware command availability
 * - Command execution with undo/redo support
 * 
 * NO PLACEHOLDERS - All commands have real implementations or proper stubs
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>

namespace RawrXD::Commands {

// ============================================================================
// COMMAND IDENTIFIERS
// ============================================================================

// File commands (1000-1099)
constexpr UINT CMD_FILE_NEW           = 1001;
constexpr UINT CMD_FILE_NEW_FOLDER    = 1002;
constexpr UINT CMD_FILE_OPEN          = 1003;
constexpr UINT CMD_FILE_OPEN_FOLDER   = 1004;
constexpr UINT CMD_FILE_OPEN_WORKSPACE = 1005;
constexpr UINT CMD_FILE_SAVE          = 1006;
constexpr UINT CMD_FILE_SAVE_AS       = 1007;
constexpr UINT CMD_FILE_SAVE_ALL      = 1008;
constexpr UINT CMD_FILE_AUTO_SAVE     = 1009;
constexpr UINT CMD_FILE_CLOSE         = 1010;
constexpr UINT CMD_FILE_CLOSE_FOLDER  = 1011;
constexpr UINT CMD_FILE_PREFERENCES   = 1012;
constexpr UINT CMD_FILE_EXIT          = 1099;

// Edit commands (1100-1199)
constexpr UINT CMD_EDIT_UNDO          = 1101;
constexpr UINT CMD_EDIT_REDO          = 1102;
constexpr UINT CMD_EDIT_CUT           = 1103;
constexpr UINT CMD_EDIT_COPY          = 1104;
constexpr UINT CMD_EDIT_PASTE         = 1105;
constexpr UINT CMD_EDIT_SELECT_ALL    = 1106;
constexpr UINT CMD_EDIT_SELECT_LINE   = 1107;
constexpr UINT CMD_EDIT_FIND          = 1108;
constexpr UINT CMD_EDIT_REPLACE       = 1109;
constexpr UINT CMD_EDIT_FIND_IN_FILES = 1110;
constexpr UINT CMD_EDIT_FORMAT_DOC    = 1111;
constexpr UINT CMD_EDIT_FORMAT_SEL    = 1112;
constexpr UINT CMD_EDIT_TOGGLE_COMMENT = 1113;
constexpr UINT CMD_EDIT_TOGGLE_BLOCK_COMMENT = 1114;

// Selection commands (1200-1299)
constexpr UINT CMD_SEL_EXPAND         = 1201;
constexpr UINT CMD_SEL_SHRINK         = 1202;
constexpr UINT CMD_SEL_COPY_LINE_UP   = 1203;
constexpr UINT CMD_SEL_COPY_LINE_DOWN = 1204;
constexpr UINT CMD_SEL_MOVE_LINE_UP   = 1205;
constexpr UINT CMD_SEL_MOVE_LINE_DOWN = 1206;
constexpr UINT CMD_SEL_ADD_CURSOR_UP  = 1207;
constexpr UINT CMD_SEL_ADD_CURSOR_DOWN = 1208;

// View commands (1300-1399)
constexpr UINT CMD_VIEW_COMMAND_PALETTE = 1301;
constexpr UINT CMD_VIEW_OPEN_VIEW     = 1302;
constexpr UINT CMD_VIEW_EXPLORER      = 1303;
constexpr UINT CMD_VIEW_SEARCH        = 1304;
constexpr UINT CMD_VIEW_SOURCE_CONTROL = 1305;
constexpr UINT CMD_VIEW_RUN_DEBUG     = 1306;
constexpr UINT CMD_VIEW_EXTENSIONS    = 1307;
constexpr UINT CMD_VIEW_OUTPUT        = 1308;
constexpr UINT CMD_VIEW_DEBUG_CONSOLE = 1309;
constexpr UINT CMD_VIEW_TERMINAL      = 1310;
constexpr UINT CMD_VIEW_WORD_WRAP     = 1311;
constexpr UINT CMD_VIEW_MINIMAP       = 1312;
constexpr UINT CMD_VIEW_BREADCRUMBS   = 1313;
constexpr UINT CMD_VIEW_STATUS_BAR    = 1314;
constexpr UINT CMD_VIEW_ZOOM_IN       = 1315;
constexpr UINT CMD_VIEW_ZOOM_OUT      = 1316;
constexpr UINT CMD_VIEW_RESET_ZOOM    = 1317;
constexpr UINT CMD_VIEW_FULLSCREEN    = 1318;

// Go commands (1400-1499)
constexpr UINT CMD_GO_BACK            = 1401;
constexpr UINT CMD_GO_FORWARD         = 1402;
constexpr UINT CMD_GO_LAST_EDIT       = 1403;
constexpr UINT CMD_GO_NEXT_PROBLEM    = 1404;
constexpr UINT CMD_GO_PREV_PROBLEM    = 1405;
constexpr UINT CMD_GO_LINE            = 1406;
constexpr UINT CMD_GO_FILE            = 1407;
constexpr UINT CMD_GO_SYMBOL_FILE     = 1408;
constexpr UINT CMD_GO_SYMBOL_WORKSPACE = 1409;
constexpr UINT CMD_GO_DEFINITION      = 1410;
constexpr UINT CMD_GO_REFERENCES      = 1411;
constexpr UINT CMD_GO_TYPE_DEFINITION = 1412;
constexpr UINT CMD_GO_IMPLEMENTATION  = 1413;

// Run/Debug commands (1500-1599)
constexpr UINT CMD_RUN_START_DEBUG    = 1501;
constexpr UINT CMD_RUN_NO_DEBUG       = 1502;
constexpr UINT CMD_RUN_STOP           = 1503;
constexpr UINT CMD_RUN_RESTART        = 1504;
constexpr UINT CMD_RUN_STEP_OVER      = 1505;
constexpr UINT CMD_RUN_STEP_INTO      = 1506;
constexpr UINT CMD_RUN_STEP_OUT       = 1507;
constexpr UINT CMD_RUN_CONTINUE       = 1508;
constexpr UINT CMD_RUN_TOGGLE_BREAKPOINT = 1509;
constexpr UINT CMD_RUN_OPEN_CONFIGS   = 1510;
constexpr UINT CMD_RUN_ADD_CONFIG     = 1511;

// Terminal commands (1600-1699)
constexpr UINT CMD_TERMINAL_NEW       = 1601;
constexpr UINT CMD_TERMINAL_SPLIT     = 1602;
constexpr UINT CMD_TERMINAL_KILL      = 1603;
constexpr UINT CMD_TERMINAL_CLEAR     = 1604;
constexpr UINT CMD_TERMINAL_RUN_TASK  = 1605;
constexpr UINT CMD_TERMINAL_BUILD_TASK = 1606;
constexpr UINT CMD_TERMINAL_CONFIG_TASKS = 1607;
constexpr UINT CMD_TERMINAL_POWERSHELL = 1608;
constexpr UINT CMD_TERMINAL_CMD       = 1609;
constexpr UINT CMD_TERMINAL_GIT_BASH  = 1610;

// Help commands (1700-1799)
constexpr UINT CMD_HELP_WELCOME       = 1701;
constexpr UINT CMD_HELP_DOCS          = 1702;
constexpr UINT CMD_HELP_SHORTCUTS     = 1703;
constexpr UINT CMD_HELP_TIPS          = 1704;
constexpr UINT CMD_HELP_PLAYGROUND    = 1705;
constexpr UINT CMD_HELP_GITHUB        = 1706;
constexpr UINT CMD_HELP_FEATURE_REQ   = 1707;
constexpr UINT CMD_HELP_REPORT_ISSUE  = 1708;
constexpr UINT CMD_HELP_LICENSE       = 1709;
constexpr UINT CMD_HELP_PRIVACY       = 1710;
constexpr UINT CMD_HELP_CHECK_UPDATES = 1711;
constexpr UINT CMD_HELP_ABOUT         = 1799;

// Context menu commands (2000-2099)
constexpr UINT CMD_CTX_OPEN           = 2001;
constexpr UINT CMD_CTX_OPEN_WITH      = 2002;
constexpr UINT CMD_CTX_CUT            = 2003;
constexpr UINT CMD_CTX_COPY           = 2004;
constexpr UINT CMD_CTX_COPY_PATH      = 2005;
constexpr UINT CMD_CTX_COPY_REL_PATH  = 2006;
constexpr UINT CMD_CTX_PASTE          = 2007;
constexpr UINT CMD_CTX_RENAME         = 2008;
constexpr UINT CMD_CTX_DELETE         = 2009;
constexpr UINT CMD_CTX_REVEAL_EXPLORER = 2010;
constexpr UINT CMD_CTX_OPEN_TERMINAL  = 2011;
constexpr UINT CMD_CTX_NEW_FILE       = 2012;
constexpr UINT CMD_CTX_NEW_FOLDER     = 2013;
constexpr UINT CMD_CTX_FIND_IN_FOLDER = 2014;
constexpr UINT CMD_CTX_PROPERTIES     = 2015;

// AI/Copilot commands (3000-3099)
constexpr UINT CMD_AI_OPEN_CHAT       = 3001;
constexpr UINT CMD_AI_QUICK_CHAT      = 3002;
constexpr UINT CMD_AI_NEW_CHAT_EDITOR = 3003;
constexpr UINT CMD_AI_NEW_CHAT_WINDOW = 3004;
constexpr UINT CMD_AI_CONFIGURE_INLINE = 3005;
constexpr UINT CMD_AI_MANAGE_CHAT     = 3006;
constexpr UINT CMD_AI_EXPLAIN_CODE    = 3007;
constexpr UINT CMD_AI_FIX_CODE        = 3008;
constexpr UINT CMD_AI_GENERATE_TESTS  = 3009;
constexpr UINT CMD_AI_GENERATE_DOCS   = 3010;
constexpr UINT CMD_AI_AGENT_START     = 3011;
constexpr UINT CMD_AI_AGENT_STOP      = 3012;
constexpr UINT CMD_AI_AGENT_STATUS    = 3013;

// Settings commands (4000-4099)
constexpr UINT CMD_SETTINGS_OPEN      = 4001;
constexpr UINT CMD_SETTINGS_JSON      = 4002;
constexpr UINT CMD_SETTINGS_KEYBOARD  = 4003;
constexpr UINT CMD_SETTINGS_THEMES    = 4004;
constexpr UINT CMD_SETTINGS_SNIPPETS  = 4005;
constexpr UINT CMD_SETTINGS_SYNC      = 4006;

// ============================================================================
// COMMAND CONTEXT - When commands are available
// ============================================================================

enum class CommandContext : uint32_t {
    Global          = 0x0001,  // Always available
    EditorFocus     = 0x0002,  // Editor has focus
    TerminalFocus   = 0x0004,  // Terminal has focus
    ExplorerFocus   = 0x0008,  // File explorer has focus
    FileSelected    = 0x0010,  // File is selected in explorer
    FolderSelected  = 0x0020,  // Folder is selected
    TextSelected    = 0x0040,  // Text is selected in editor
    FileOpen        = 0x0080,  // A file is open
    FileDirty       = 0x0100,  // Current file has unsaved changes
    GitRepo         = 0x0200,  // Working in a git repository
    ModelLoaded     = 0x0400,  // AI model is loaded
    Debugging       = 0x0800,  // Debug session active
    CanUndo         = 0x1000,  // Undo is possible
    CanRedo         = 0x2000,  // Redo is possible
    
    // Common combinations
    EditorWithFile  = EditorFocus | FileOpen,
    EditorTextSel   = EditorFocus | TextSelected,
};

inline CommandContext operator|(CommandContext a, CommandContext b) {
    return static_cast<CommandContext>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline CommandContext operator&(CommandContext a, CommandContext b) {
    return static_cast<CommandContext>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline bool hasContext(CommandContext current, CommandContext required) {
    return (static_cast<uint32_t>(current) & static_cast<uint32_t>(required)) == static_cast<uint32_t>(required);
}

// ============================================================================
// KEYBINDING STRUCTURE
// ============================================================================

struct KeyBinding {
    BYTE modifiers = 0;  // FCONTROL, FALT, FSHIFT
    WORD key = 0;        // Virtual key code
    std::string chord;   // For two-step keybindings like "Ctrl+K Ctrl+O"
    
    bool isChord() const { return !chord.empty(); }
    
    std::string toString() const;
    static KeyBinding fromString(const std::string& str);
    
    bool matches(BYTE mods, WORD vk) const {
        return modifiers == mods && key == vk;
    }
    
    // Create accelerator entry
    ACCEL toAccel(UINT cmdId) const {
        ACCEL accel;
        accel.fVirt = modifiers | FVIRTKEY;
        accel.key = key;
        accel.cmd = static_cast<WORD>(cmdId);
        return accel;
    }
};

// ============================================================================
// COMMAND STRUCTURE
// ============================================================================

using CommandHandler = std::function<void()>;
using CommandEnabledChecker = std::function<bool()>;
using CommandArgHandler = std::function<void(const nlohmann::json&)>;

struct Command {
    UINT id = 0;
    std::string internalId;      // e.g., "workbench.action.files.save"
    std::string label;           // Display name
    std::string description;     // Tooltip/help text
    std::string category;        // Menu category
    
    KeyBinding keybinding;
    CommandContext context = CommandContext::Global;
    
    CommandHandler handler;
    CommandEnabledChecker enabledChecker;
    CommandArgHandler argHandler;  // For commands that take arguments
    
    bool isCheckable = false;
    bool isChecked = false;
    bool isVisible = true;
    bool isEnabled = true;
    
    std::string iconPath;  // Optional icon
};

// ============================================================================
// COMMAND REGISTRY - Singleton
// ============================================================================

class CommandRegistry {
public:
    static CommandRegistry& instance();
    
    // Registration
    void registerCommand(const Command& cmd);
    void registerCommand(UINT id, const std::string& label, const std::string& shortcut, 
                        CommandHandler handler, CommandContext ctx = CommandContext::Global);
    void unregisterCommand(UINT id);
    
    // Keybinding management
    void setKeybinding(UINT cmdId, const KeyBinding& binding);
    void setKeybinding(UINT cmdId, const std::string& bindingStr);
    std::optional<KeyBinding> getKeybinding(UINT cmdId) const;
    
    // Command lookup
    Command* getCommand(UINT id);
    Command* getCommandByInternalId(const std::string& internalId);
    std::vector<Command*> getCommandsByCategory(const std::string& category);
    std::vector<Command*> getAllCommands();
    
    // Execution
    bool executeCommand(UINT id);
    bool executeCommand(const std::string& internalId);
    bool executeCommandWithArgs(UINT id, const nlohmann::json& args);
    
    // Context management
    void setCurrentContext(CommandContext ctx);
    void addContext(CommandContext ctx);
    void removeContext(CommandContext ctx);
    CommandContext getCurrentContext() const;
    bool isCommandEnabled(UINT id) const;
    
    // State update
    void updateCommandState(UINT id, bool enabled);
    void updateCommandState(UINT id, bool enabled, bool checked);
    
    // Accelerator table generation for Win32
    HACCEL createAcceleratorTable();
    void destroyAcceleratorTable(HACCEL hAccel);
    
    // Serialization
    nlohmann::json exportKeybindings() const;
    void importKeybindings(const nlohmann::json& bindings);
    void loadKeybindingsFromFile(const std::string& path);
    void saveKeybindingsToFile(const std::string& path) const;
    
    // Search (for command palette)
    std::vector<Command*> searchCommands(const std::string& query);
    
private:
    CommandRegistry() = default;
    ~CommandRegistry() = default;
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
    
    std::unordered_map<UINT, Command> m_commands;
    std::unordered_map<std::string, UINT> m_internalIdMap;
    CommandContext m_currentContext = CommandContext::Global;
    mutable CRITICAL_SECTION m_cs;
    bool m_initialized = false;
    
    void ensureInitialized();
};

// ============================================================================
// HELPER MACROS FOR REGISTRATION
// ============================================================================

#define REGISTER_COMMAND(id, label, shortcut, handler) \
    RawrXD::Commands::CommandRegistry::instance().registerCommand( \
        id, label, shortcut, handler, RawrXD::Commands::CommandContext::Global)

#define REGISTER_COMMAND_CTX(id, label, shortcut, handler, ctx) \
    RawrXD::Commands::CommandRegistry::instance().registerCommand( \
        id, label, shortcut, handler, ctx)

} // namespace RawrXD::Commands
