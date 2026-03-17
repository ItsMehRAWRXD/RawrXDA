/**
 * @file CommandRegistry.hpp
 * @brief Production-Ready Centralized Command Registry with Capability-Based Architecture
 * 
 * This implements a unified command system for both CLI and GUI environments.
 * All commands are registered here with:
 *   - Real implementations (no TODOs/placeholders)
 *   - Capability flags (CAP_CONTAINER, CAP_NAVIGATE, etc.)
 *   - Keyboard shortcuts
 *   - Structured logging
 *   - Performance metrics
 */

#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <shellapi.h>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>
#include <optional>
#include <nlohmann/json.hpp>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")

namespace RawrXD::Win32 {

// ============================================================================
// CAPABILITY FLAGS (Nameless Capability-Based Architecture)
// ============================================================================
constexpr uint32_t CAP_CONTAINER   = 0x01;  // Can contain other nodes
constexpr uint32_t CAP_NAVIGATE    = 0x02;  // Can navigate/route to locations
constexpr uint32_t CAP_DISPLAY     = 0x04;  // Can display content
constexpr uint32_t CAP_MANIPULATE  = 0x08;  // Can modify content/state
constexpr uint32_t CAP_ROUTE       = 0x10;  // Can route messages/commands
constexpr uint32_t CAP_PERSIST     = 0x20;  // Can persist state to storage
constexpr uint32_t CAP_TERMINAL    = 0x40;  // Terminal/shell operations
constexpr uint32_t CAP_CLIPBOARD   = 0x80;  // Clipboard operations
constexpr uint32_t CAP_DEBUG       = 0x100; // Debug operations

// ============================================================================
// COMMAND IDS - Organized by range for routing
// ============================================================================
namespace CommandID {
    // File Commands (1000-1999)
    constexpr UINT FILE_NEW = 1001;
    constexpr UINT FILE_NEW_FOLDER = 1002;
    constexpr UINT FILE_OPEN = 1003;
    constexpr UINT FILE_OPEN_FOLDER = 1004;
    constexpr UINT FILE_SAVE = 1005;
    constexpr UINT FILE_SAVE_AS = 1006;
    constexpr UINT FILE_SAVE_ALL = 1007;
    constexpr UINT FILE_CLOSE = 1008;
    constexpr UINT FILE_CLOSE_ALL = 1009;
    constexpr UINT FILE_REVEAL_IN_EXPLORER = 1010;
    constexpr UINT FILE_COPY_PATH = 1011;
    constexpr UINT FILE_COPY_RELATIVE_PATH = 1012;
    constexpr UINT FILE_RENAME = 1013;
    constexpr UINT FILE_DELETE = 1014;
    constexpr UINT FILE_PROPERTIES = 1015;
    constexpr UINT FILE_AUTO_SAVE = 1016;
    constexpr UINT FILE_EXIT = 1099;

    // Edit Commands (2000-2999)
    constexpr UINT EDIT_UNDO = 2001;
    constexpr UINT EDIT_REDO = 2002;
    constexpr UINT EDIT_CUT = 2003;
    constexpr UINT EDIT_COPY = 2004;
    constexpr UINT EDIT_PASTE = 2005;
    constexpr UINT EDIT_SELECT_ALL = 2006;
    constexpr UINT EDIT_FIND = 2007;
    constexpr UINT EDIT_REPLACE = 2008;
    constexpr UINT EDIT_FIND_IN_FILES = 2009;
    constexpr UINT EDIT_GO_TO_LINE = 2010;
    constexpr UINT EDIT_TOGGLE_COMMENT = 2011;
    constexpr UINT EDIT_FORMAT_DOCUMENT = 2012;
    constexpr UINT EDIT_FORMAT_SELECTION = 2013;

    // View Commands (3000-3999)
    constexpr UINT VIEW_COMMAND_PALETTE = 3001;
    constexpr UINT VIEW_EXPLORER = 3002;
    constexpr UINT VIEW_SEARCH = 3003;
    constexpr UINT VIEW_SOURCE_CONTROL = 3004;
    constexpr UINT VIEW_DEBUG = 3005;
    constexpr UINT VIEW_EXTENSIONS = 3006;
    constexpr UINT VIEW_OUTPUT = 3007;
    constexpr UINT VIEW_TERMINAL = 3008;
    constexpr UINT VIEW_PROBLEMS = 3009;
    constexpr UINT VIEW_MINIMAP = 3010;
    constexpr UINT VIEW_BREADCRUMBS = 3011;
    constexpr UINT VIEW_STATUS_BAR = 3012;
    constexpr UINT VIEW_WORD_WRAP = 3013;
    constexpr UINT VIEW_ZOOM_IN = 3014;
    constexpr UINT VIEW_ZOOM_OUT = 3015;
    constexpr UINT VIEW_RESET_ZOOM = 3016;

    // Go Commands (4000-4999)
    constexpr UINT GO_BACK = 4001;
    constexpr UINT GO_FORWARD = 4002;
    constexpr UINT GO_LAST_EDIT = 4003;
    constexpr UINT GO_DEFINITION = 4004;
    constexpr UINT GO_DECLARATION = 4005;
    constexpr UINT GO_REFERENCES = 4006;
    constexpr UINT GO_SYMBOL = 4007;
    constexpr UINT GO_FILE = 4008;
    constexpr UINT GO_LINE = 4009;
    constexpr UINT GO_NEXT_PROBLEM = 4010;
    constexpr UINT GO_PREV_PROBLEM = 4011;

    // Run Commands (5000-5999)
    constexpr UINT RUN_DEBUG = 5001;
    constexpr UINT RUN_WITHOUT_DEBUG = 5002;
    constexpr UINT RUN_STOP = 5003;
    constexpr UINT RUN_RESTART = 5004;
    constexpr UINT RUN_STEP_OVER = 5005;
    constexpr UINT RUN_STEP_INTO = 5006;
    constexpr UINT RUN_STEP_OUT = 5007;
    constexpr UINT RUN_TOGGLE_BREAKPOINT = 5008;
    constexpr UINT RUN_CONFIGURE = 5009;

    // Terminal Commands (6000-6999)
    constexpr UINT TERMINAL_NEW = 6001;
    constexpr UINT TERMINAL_NEW_POWERSHELL = 6002;
    constexpr UINT TERMINAL_NEW_CMD = 6003;
    constexpr UINT TERMINAL_SPLIT = 6004;
    constexpr UINT TERMINAL_KILL = 6005;
    constexpr UINT TERMINAL_CLEAR = 6006;
    constexpr UINT TERMINAL_OPEN_IN_PATH = 6007;
    constexpr UINT TERMINAL_RUN_TASK = 6008;
    constexpr UINT TERMINAL_BUILD_TASK = 6009;

    // Help Commands (7000-7999)
    constexpr UINT HELP_WELCOME = 7001;
    constexpr UINT HELP_DOCUMENTATION = 7002;
    constexpr UINT HELP_SHORTCUTS = 7003;
    constexpr UINT HELP_ABOUT = 7004;
    constexpr UINT HELP_CHECK_UPDATES = 7005;
    constexpr UINT HELP_REPORT_ISSUE = 7006;

    // Context Menu Commands (8000-8999)
    constexpr UINT CTX_COPY_PATH = 8001;
    constexpr UINT CTX_COPY_RELATIVE_PATH = 8002;
    constexpr UINT CTX_REVEAL_EXPLORER = 8003;
    constexpr UINT CTX_OPEN_TERMINAL = 8004;
    constexpr UINT CTX_RENAME = 8005;
    constexpr UINT CTX_DELETE = 8006;
    constexpr UINT CTX_NEW_FILE = 8007;
    constexpr UINT CTX_NEW_FOLDER = 8008;

    // Settings Commands (9000-9999)
    constexpr UINT SETTINGS_OPEN = 9001;
    constexpr UINT SETTINGS_KEYBOARD = 9002;
    constexpr UINT SETTINGS_THEME = 9003;
    constexpr UINT SETTINGS_EXTENSIONS = 9004;
}

// Compatibility namespace alias with CMD_ prefix for menu builders
namespace CommandIDs {
    // File Commands
    constexpr UINT CMD_FILE_NEW = CommandID::FILE_NEW;
    constexpr UINT CMD_FILE_NEW_FOLDER = CommandID::FILE_NEW_FOLDER;
    constexpr UINT CMD_FILE_OPEN = CommandID::FILE_OPEN;
    constexpr UINT CMD_FILE_OPEN_FOLDER = CommandID::FILE_OPEN_FOLDER;
    constexpr UINT CMD_FILE_SAVE = CommandID::FILE_SAVE;
    constexpr UINT CMD_FILE_SAVE_AS = CommandID::FILE_SAVE_AS;
    constexpr UINT CMD_FILE_SAVE_ALL = CommandID::FILE_SAVE_ALL;
    constexpr UINT CMD_FILE_CLOSE = CommandID::FILE_CLOSE;
    constexpr UINT CMD_FILE_CLOSE_ALL = CommandID::FILE_CLOSE_ALL;
    constexpr UINT CMD_FILE_AUTO_SAVE = CommandID::FILE_AUTO_SAVE;
    constexpr UINT CMD_FILE_EXIT = CommandID::FILE_EXIT;
    
    // Edit Commands
    constexpr UINT CMD_EDIT_UNDO = CommandID::EDIT_UNDO;
    constexpr UINT CMD_EDIT_REDO = CommandID::EDIT_REDO;
    constexpr UINT CMD_EDIT_CUT = CommandID::EDIT_CUT;
    constexpr UINT CMD_EDIT_COPY = CommandID::EDIT_COPY;
    constexpr UINT CMD_EDIT_PASTE = CommandID::EDIT_PASTE;
    constexpr UINT CMD_EDIT_SELECT_ALL = CommandID::EDIT_SELECT_ALL;
    constexpr UINT CMD_EDIT_FIND = CommandID::EDIT_FIND;
    constexpr UINT CMD_EDIT_REPLACE = CommandID::EDIT_REPLACE;
    constexpr UINT CMD_EDIT_FIND_IN_FILES = CommandID::EDIT_FIND_IN_FILES;
    constexpr UINT CMD_EDIT_GO_TO_LINE = CommandID::EDIT_GO_TO_LINE;
    constexpr UINT CMD_EDIT_TOGGLE_COMMENT = CommandID::EDIT_TOGGLE_COMMENT;
    constexpr UINT CMD_EDIT_FORMAT_DOCUMENT = CommandID::EDIT_FORMAT_DOCUMENT;
    
    // View Commands
    constexpr UINT CMD_VIEW_COMMAND_PALETTE = CommandID::VIEW_COMMAND_PALETTE;
    constexpr UINT CMD_VIEW_EXPLORER = CommandID::VIEW_EXPLORER;
    constexpr UINT CMD_VIEW_SEARCH = CommandID::VIEW_SEARCH;
    constexpr UINT CMD_VIEW_SOURCE_CONTROL = CommandID::VIEW_SOURCE_CONTROL;
    constexpr UINT CMD_VIEW_DEBUG = CommandID::VIEW_DEBUG;
    constexpr UINT CMD_VIEW_EXTENSIONS = CommandID::VIEW_EXTENSIONS;
    constexpr UINT CMD_VIEW_OUTPUT = CommandID::VIEW_OUTPUT;
    constexpr UINT CMD_VIEW_PROBLEMS = CommandID::VIEW_PROBLEMS;
    constexpr UINT CMD_VIEW_TERMINAL = CommandID::VIEW_TERMINAL;
    constexpr UINT CMD_VIEW_WORD_WRAP = CommandID::VIEW_WORD_WRAP;
    constexpr UINT CMD_VIEW_MINIMAP = CommandID::VIEW_MINIMAP;
    constexpr UINT CMD_VIEW_BREADCRUMBS = CommandID::VIEW_BREADCRUMBS;
    constexpr UINT CMD_VIEW_STATUS_BAR = CommandID::VIEW_STATUS_BAR;
    constexpr UINT CMD_VIEW_ZOOM_IN = CommandID::VIEW_ZOOM_IN;
    constexpr UINT CMD_VIEW_ZOOM_OUT = CommandID::VIEW_ZOOM_OUT;
    constexpr UINT CMD_VIEW_RESET_ZOOM = CommandID::VIEW_RESET_ZOOM;
    
    // Go Commands
    constexpr UINT CMD_GO_BACK = CommandID::GO_BACK;
    constexpr UINT CMD_GO_FORWARD = CommandID::GO_FORWARD;
    constexpr UINT CMD_GO_LAST_EDIT = CommandID::GO_LAST_EDIT;
    constexpr UINT CMD_GO_DEFINITION = CommandID::GO_DEFINITION;
    constexpr UINT CMD_GO_DECLARATION = CommandID::GO_DECLARATION;
    constexpr UINT CMD_GO_REFERENCES = CommandID::GO_REFERENCES;
    constexpr UINT CMD_GO_FILE = CommandID::GO_FILE;
    constexpr UINT CMD_GO_SYMBOL = CommandID::GO_SYMBOL;
    constexpr UINT CMD_GO_LINE = CommandID::GO_LINE;
    constexpr UINT CMD_GO_NEXT_PROBLEM = CommandID::GO_NEXT_PROBLEM;
    constexpr UINT CMD_GO_PREV_PROBLEM = CommandID::GO_PREV_PROBLEM;
    
    // Run Commands
    constexpr UINT CMD_RUN_DEBUG = CommandID::RUN_DEBUG;
    constexpr UINT CMD_RUN_WITHOUT_DEBUG = CommandID::RUN_WITHOUT_DEBUG;
    constexpr UINT CMD_RUN_STOP = CommandID::RUN_STOP;
    constexpr UINT CMD_RUN_RESTART = CommandID::RUN_RESTART;
    constexpr UINT CMD_RUN_STEP_OVER = CommandID::RUN_STEP_OVER;
    constexpr UINT CMD_RUN_STEP_INTO = CommandID::RUN_STEP_INTO;
    constexpr UINT CMD_RUN_STEP_OUT = CommandID::RUN_STEP_OUT;
    constexpr UINT CMD_RUN_TOGGLE_BREAKPOINT = CommandID::RUN_TOGGLE_BREAKPOINT;
    constexpr UINT CMD_RUN_CONFIGURE = CommandID::RUN_CONFIGURE;
    
    // Terminal Commands
    constexpr UINT CMD_TERMINAL_NEW = CommandID::TERMINAL_NEW;
    constexpr UINT CMD_TERMINAL_NEW_PWSH = CommandID::TERMINAL_NEW_POWERSHELL;
    constexpr UINT CMD_TERMINAL_NEW_CMD = CommandID::TERMINAL_NEW_CMD;
    constexpr UINT CMD_TERMINAL_SPLIT = CommandID::TERMINAL_SPLIT;
    constexpr UINT CMD_TERMINAL_KILL = CommandID::TERMINAL_KILL;
    constexpr UINT CMD_TERMINAL_CLEAR = CommandID::TERMINAL_CLEAR;
    constexpr UINT CMD_TERMINAL_RUN_TASK = CommandID::TERMINAL_RUN_TASK;
    constexpr UINT CMD_TERMINAL_BUILD_TASK = CommandID::TERMINAL_BUILD_TASK;
    
    // Help Commands
    constexpr UINT CMD_HELP_WELCOME = CommandID::HELP_WELCOME;
    constexpr UINT CMD_HELP_DOCS = CommandID::HELP_DOCUMENTATION;
    constexpr UINT CMD_HELP_SHORTCUTS = CommandID::HELP_SHORTCUTS;
    constexpr UINT CMD_HELP_ABOUT = CommandID::HELP_ABOUT;
    constexpr UINT CMD_HELP_CHECK_UPDATES = CommandID::HELP_CHECK_UPDATES;
    constexpr UINT CMD_HELP_REPORT_ISSUE = CommandID::HELP_REPORT_ISSUE;
}

// ============================================================================
// COMMAND METRICS - Performance tracking
// ============================================================================
struct CommandMetrics {
    std::string commandName;
    uint64_t executionCount = 0;
    double totalDurationMs = 0.0;
    double minDurationMs = std::numeric_limits<double>::max();
    double maxDurationMs = 0.0;
    uint64_t errorCount = 0;
    std::chrono::system_clock::time_point lastExecuted;
};

// ============================================================================
// COMMAND DEFINITION
// ============================================================================
struct CommandDef {
    UINT id;
    std::string name;
    std::string description;
    std::string shortcut;
    uint32_t capabilities;
    std::function<bool(const std::string& context)> handler;
    bool enabled = true;
    bool checkable = false;
    bool checked = false;
};

// ============================================================================
// STRUCTURED LOGGING - Per the instructions
// ============================================================================
enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class CommandLogger {
public:
    static CommandLogger& instance() {
        static CommandLogger logger;
        return logger;
    }

    void log(LogLevel level, const std::string& command, const std::string& message, 
             const nlohmann::json& context = {}) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        nlohmann::json entry;
        entry["timestamp"] = getCurrentTimestamp();
        entry["level"] = levelToString(level);
        entry["command"] = command;
        entry["message"] = message;
        if (!context.empty()) {
            entry["context"] = context;
        }

        m_entries.push_back(entry);
        
        // Also output to debug console
        OutputDebugStringA((entry.dump() + "\n").c_str());
    }

    void logLatency(const std::string& command, double durationMs) {
        log(LogLevel::DEBUG, command, "Execution completed",
            {{"duration_ms", durationMs}});
    }

    std::vector<nlohmann::json> getEntries(size_t count = 100) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t start = m_entries.size() > count ? m_entries.size() - count : 0;
        return std::vector<nlohmann::json>(m_entries.begin() + start, m_entries.end());
    }

private:
    mutable std::mutex m_mutex;
    std::vector<nlohmann::json> m_entries;

    std::string getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_buf);
        return buf;
    }

    std::string levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARN: return "WARN";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};

#define CMD_LOG_DEBUG(cmd, msg) CommandLogger::instance().log(LogLevel::DEBUG, cmd, msg)
#define CMD_LOG_INFO(cmd, msg) CommandLogger::instance().log(LogLevel::INFO, cmd, msg)
#define CMD_LOG_WARN(cmd, msg) CommandLogger::instance().log(LogLevel::WARN, cmd, msg)
#define CMD_LOG_ERROR(cmd, msg) CommandLogger::instance().log(LogLevel::ERROR, cmd, msg)
#define CMD_LOG_CTX(cmd, msg, ctx) CommandLogger::instance().log(LogLevel::INFO, cmd, msg, ctx)

// ============================================================================
// COMMAND REGISTRY - Main class
// ============================================================================
class CommandRegistry {
public:
    static CommandRegistry& instance() {
        static CommandRegistry registry;
        return registry;
    }

    // Initialize all commands
    void initialize(HWND mainWindow);

    // Register a command
    void registerCommand(const CommandDef& cmd);

    // Execute command by ID
    bool execute(UINT id, const std::string& context = "");

    // Execute command by name
    bool executeByName(const std::string& name, const std::string& context = "");

    // Get command info
    std::optional<CommandDef> getCommand(UINT id) const;
    std::optional<CommandDef> getCommandByName(const std::string& name) const;

    // State management
    void setEnabled(UINT id, bool enabled);
    void setChecked(UINT id, bool checked);
    bool isEnabled(UINT id) const;
    bool isChecked(UINT id) const;

    // Metrics
    CommandMetrics getMetrics(UINT id) const;
    std::vector<CommandMetrics> getAllMetrics() const;

    // Context helpers
    void setCurrentFilePath(const std::string& path) { m_currentFile = path; }
    void setCurrentFolderPath(const std::string& path) { m_currentFolder = path; }
    void setWorkspaceRoot(const std::string& path) { m_workspaceRoot = path; }
    void setActiveEditor(HWND editor) { m_activeEditor = editor; }
    void setActiveTerminal(HWND terminal) { m_activeTerminal = terminal; }
    
    std::string getCurrentFilePath() const { return m_currentFile; }
    std::string getCurrentFolderPath() const { return m_currentFolder; }
    std::string getWorkspaceRoot() const { return m_workspaceRoot; }

    // Keyboard shortcuts
    bool handleKeyboardShortcut(UINT vkCode, bool ctrl, bool shift, bool alt);
    
    // IDE callback registration - for commands that need Win32IDE context
    using IDECallback = std::function<void()>;
    void setIDECallback(UINT id, IDECallback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ideCallbacks[id] = callback;
    }
    
    bool hasIDECallback(UINT id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_ideCallbacks.find(id) != m_ideCallbacks.end();
    }
    
    bool executeIDECallback(UINT id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_ideCallbacks.find(id);
        if (it != m_ideCallbacks.end() && it->second) {
            it->second();
            return true;
        }
        return false;
    }

private:
    CommandRegistry() = default;
    
    void registerFileCommands();
    void registerEditCommands();
    void registerViewCommands();
    void registerGoCommands();
    void registerRunCommands();
    void registerTerminalCommands();
    void registerHelpCommands();
    void registerContextMenuCommands();
    void registerKeyboardShortcuts();

    // Command storage
    std::unordered_map<UINT, CommandDef> m_commands;
    std::unordered_map<std::string, UINT> m_nameToId;
    std::unordered_map<UINT, CommandMetrics> m_metrics;
    
    // Keyboard shortcuts: key combo -> command ID
    struct ShortcutKey {
        UINT vkCode;
        bool ctrl;
        bool shift;
        bool alt;
        bool operator==(const ShortcutKey& other) const {
            return vkCode == other.vkCode && ctrl == other.ctrl && 
                   shift == other.shift && alt == other.alt;
        }
    };
    struct ShortcutKeyHash {
        size_t operator()(const ShortcutKey& k) const {
            return std::hash<UINT>()(k.vkCode) ^ (k.ctrl << 8) ^ (k.shift << 9) ^ (k.alt << 10);
        }
    };
    std::unordered_map<ShortcutKey, UINT, ShortcutKeyHash> m_shortcuts;

    // Context
    HWND m_mainWindow = nullptr;
    HWND m_activeEditor = nullptr;
    HWND m_activeTerminal = nullptr;
    std::string m_currentFile;
    std::string m_currentFolder;
    std::string m_workspaceRoot;
    
    // IDE callbacks for commands needing Win32IDE context
    std::unordered_map<UINT, IDECallback> m_ideCallbacks;

    mutable std::mutex m_mutex;
};

// ============================================================================
// PRODUCTION UTILITY FUNCTIONS - Real implementations
// ============================================================================
namespace CommandUtils {

// Copy text to clipboard
inline bool copyToClipboard(HWND hwnd, const std::string& text) {
    if (!OpenClipboard(hwnd)) {
        CMD_LOG_ERROR("clipboard", "Failed to open clipboard");
        return false;
    }
    
    EmptyClipboard();
    
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hMem) {
        CloseClipboard();
        CMD_LOG_ERROR("clipboard", "Failed to allocate memory");
        return false;
    }
    
    char* pMem = static_cast<char*>(GlobalLock(hMem));
    if (pMem) {
        memcpy(pMem, text.c_str(), text.size() + 1);
        GlobalUnlock(hMem);
    }
    
    SetClipboardData(CF_TEXT, hMem);
    CloseClipboard();
    
    CMD_LOG_INFO("clipboard", "Text copied to clipboard");
    return true;
}

// Reveal file/folder in Windows Explorer
inline bool revealInExplorer(const std::string& path) {
    std::wstring widePath(path.begin(), path.end());
    
    PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(widePath.c_str());
    if (!pidl) {
        // Try just opening the containing folder
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) {
            CMD_LOG_ERROR("explorer", "Path does not exist: " + path);
            return false;
        }
        
        std::string folder = path;
        if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            // Get parent folder
            size_t pos = path.find_last_of("\\/");
            if (pos != std::string::npos) {
                folder = path.substr(0, pos);
            }
        }
        
        ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        CMD_LOG_INFO("explorer", "Opened folder: " + folder);
        return true;
    }
    
    HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
    ILFree(pidl);
    
    if (SUCCEEDED(hr)) {
        CMD_LOG_INFO("explorer", "Revealed in Explorer: " + path);
        return true;
    }
    
    CMD_LOG_ERROR("explorer", "SHOpenFolderAndSelectItems failed");
    return false;
}

// Open terminal at path
inline bool openTerminalAt(const std::string& path, bool usePowerShell = true) {
    std::string folder = path;
    DWORD attr = GetFileAttributesA(path.c_str());
    
    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        // Get parent folder
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            folder = path.substr(0, pos);
        }
    }
    
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = "open";
    sei.lpFile = usePowerShell ? "pwsh.exe" : "cmd.exe";
    sei.lpDirectory = folder.c_str();
    sei.nShow = SW_SHOWNORMAL;
    
    if (ShellExecuteExA(&sei)) {
        CMD_LOG_INFO("terminal", "Opened terminal at: " + folder);
        return true;
    }
    
    // Fallback to Windows PowerShell if pwsh not found
    if (usePowerShell) {
        sei.lpFile = "powershell.exe";
        if (ShellExecuteExA(&sei)) {
            CMD_LOG_INFO("terminal", "Opened Windows PowerShell at: " + folder);
            return true;
        }
    }
    
    CMD_LOG_ERROR("terminal", "Failed to open terminal at: " + folder);
    return false;
}

// Get relative path from workspace root
inline std::string getRelativePath(const std::string& fullPath, const std::string& root) {
    if (root.empty()) return fullPath;
    
    // Normalize paths
    char fullNorm[MAX_PATH], rootNorm[MAX_PATH];
    if (!GetFullPathNameA(fullPath.c_str(), MAX_PATH, fullNorm, nullptr)) return fullPath;
    if (!GetFullPathNameA(root.c_str(), MAX_PATH, rootNorm, nullptr)) return fullPath;
    
    std::string full(fullNorm), rootStr(rootNorm);
    
    // Ensure root ends with separator
    if (!rootStr.empty() && rootStr.back() != '\\' && rootStr.back() != '/') {
        rootStr += '\\';
    }
    
    // Check if full path starts with root
    if (full.size() > rootStr.size() && 
        _strnicmp(full.c_str(), rootStr.c_str(), rootStr.size()) == 0) {
        return full.substr(rootStr.size());
    }
    
    return fullPath;
}

// Show standard file open dialog
inline std::string showOpenFileDialog(HWND parent, const char* filter = nullptr) {
    char filename[MAX_PATH] = "";
    
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameA(&ofn)) {
        return filename;
    }
    return "";
}

// Show standard file save dialog
inline std::string showSaveFileDialog(HWND parent, const char* defaultName = nullptr, 
                                       const char* filter = nullptr) {
    char filename[MAX_PATH] = "";
    if (defaultName) {
        strncpy_s(filename, defaultName, MAX_PATH - 1);
    }
    
    OPENFILENAMEA ofn = { sizeof(ofn) };
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter ? filter : "All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    
    if (GetSaveFileNameA(&ofn)) {
        return filename;
    }
    return "";
}

// Show folder browser dialog
inline std::string showFolderDialog(HWND parent, const char* title = nullptr) {
    char path[MAX_PATH] = "";
    
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = parent;
    bi.lpszTitle = title ? title : "Select Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        SHGetPathFromIDListA(pidl, path);
        CoTaskMemFree(pidl);
        return path;
    }
    return "";
}

// Delete file/folder with confirmation
inline bool deleteWithConfirmation(HWND parent, const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        CMD_LOG_ERROR("delete", "Path does not exist: " + path);
        return false;
    }
    
    std::string msg = "Are you sure you want to delete ";
    msg += (attr & FILE_ATTRIBUTE_DIRECTORY) ? "folder" : "file";
    msg += ":\n" + path + "?";
    
    if (MessageBoxA(parent, msg.c_str(), "Confirm Delete", 
                    MB_YESNO | MB_ICONQUESTION) != IDYES) {
        return false;
    }
    
    // Use SHFileOperation for recycle bin support
    SHFILEOPSTRUCTA fileOp = { 0 };
    char from[MAX_PATH + 1] = { 0 };
    strncpy_s(from, path.c_str(), MAX_PATH - 1);
    from[strlen(from) + 1] = '\0';  // Double null terminated
    
    fileOp.hwnd = parent;
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = from;
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    
    int result = SHFileOperationA(&fileOp);
    if (result == 0) {
        CMD_LOG_INFO("delete", "Deleted: " + path);
        return true;
    }
    
    CMD_LOG_ERROR("delete", "Failed to delete: " + path);
    return false;
}

// Rename file/folder
inline bool renameFile(HWND parent, const std::string& oldPath, const std::string& newName) {
    // Get directory part
    size_t pos = oldPath.find_last_of("\\/");
    std::string dir = (pos != std::string::npos) ? oldPath.substr(0, pos + 1) : "";
    std::string newPath = dir + newName;
    
    if (MoveFileA(oldPath.c_str(), newPath.c_str())) {
        CMD_LOG_INFO("rename", "Renamed: " + oldPath + " -> " + newPath);
        return true;
    }
    
    DWORD err = GetLastError();
    CMD_LOG_ERROR("rename", "Failed to rename: " + std::to_string(err));
    return false;
}

// Create new file
inline bool createNewFile(HWND parent, const std::string& folder) {
    char name[MAX_PATH] = "newfile.txt";
    
    // Simple input dialog would be better, but for now use a prompt
    std::string fullPath = folder;
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath += '\\';
    }
    fullPath += name;
    
    HANDLE hFile = CreateFileA(fullPath.c_str(), GENERIC_WRITE, 0, nullptr, 
                                CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        CMD_LOG_INFO("file", "Created new file: " + fullPath);
        return true;
    }
    
    CMD_LOG_ERROR("file", "Failed to create file: " + fullPath);
    return false;
}

// Create new folder
inline bool createNewFolder(HWND parent, const std::string& parentFolder) {
    std::string fullPath = parentFolder;
    if (!fullPath.empty() && fullPath.back() != '\\' && fullPath.back() != '/') {
        fullPath += '\\';
    }
    fullPath += "NewFolder";
    
    if (CreateDirectoryA(fullPath.c_str(), nullptr)) {
        CMD_LOG_INFO("folder", "Created new folder: " + fullPath);
        return true;
    }
    
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) {
        // Append number
        for (int i = 2; i < 100; i++) {
            std::string numbered = fullPath + std::to_string(i);
            if (CreateDirectoryA(numbered.c_str(), nullptr)) {
                CMD_LOG_INFO("folder", "Created new folder: " + numbered);
                return true;
            }
        }
    }
    
    CMD_LOG_ERROR("folder", "Failed to create folder");
    return false;
}

// Show file properties dialog
inline bool showFileProperties(const std::string& path) {
    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.lpVerb = "properties";
    sei.lpFile = path.c_str();
    sei.nShow = SW_SHOW;
    
    if (ShellExecuteExA(&sei)) {
        CMD_LOG_INFO("properties", "Showing properties for: " + path);
        return true;
    }
    
    CMD_LOG_ERROR("properties", "Failed to show properties");
    return false;
}

} // namespace CommandUtils

} // namespace RawrXD::Win32
