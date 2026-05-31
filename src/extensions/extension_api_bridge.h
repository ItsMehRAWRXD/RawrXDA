#pragma once

// Define Windows macros before including windows.h to avoid conflicts
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Include standard headers first
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <functional>
#include <future>
#include <memory>
#include <atomic>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>

// Include Windows headers last
#include <windows.h>

namespace RawrXD::Extensions {

// ============================================================================
// CORE TYPES
// ============================================================================

using ExtCommandCallback = void(*)(void* userData);
using ExtAsyncCommandCallback = std::function<void*(void* userData)>; // Returns result
using ExtEventCallback = void(*)(const char* eventType, const char* jsonPayload, void* userData);

enum class LogLevel : int32_t {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR_ = 3,  // ERROR is a Windows macro
    FATAL = 4
};

enum class StatusBarAlignment : int32_t {
    LEFT = 0,
    RIGHT = 1
};

enum class MessageSeverity : int32_t {
    INFORMATION = 0,
    WARNING = 1,
    ERROR_ = 2  // ERROR is a Windows macro
};

// ============================================================================
// URI (VS Code Compatible)
// ============================================================================

struct URI {
    std::string scheme;
    std::string authority;
    std::string path;
    std::string query;
    std::string fragment;
    
    static URI parse(const std::string& uri);
    static URI file(const std::string& path);
    std::string toString() const;
    std::string fsPath() const; // Normalized filesystem path
};

// ============================================================================
// FILE SYSTEM TYPES
// ============================================================================

struct FileStat {
    uint64_t size;
    uint64_t ctime; // creation time (epoch ms)
    uint64_t mtime; // modification time (epoch ms)
    bool isDirectory;
    bool isFile;
    bool isSymbolicLink;
};

struct FileChangeEvent {
    enum Type { CREATED, CHANGED, DELETED };
    Type type;
    URI uri;
};

using FileWatcherCallback = void(*)(const FileChangeEvent& event, void* userData);

// ============================================================================
// CONFIGURATION
// ============================================================================

class Configuration {
public:
    bool getBool(const std::string& key, bool defaultValue = false) const;
    int32_t getInt(const std::string& key, int32_t defaultValue = 0) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    std::vector<std::string> getArray(const std::string& key) const;
    
    void set(const std::string& key, bool value);
    void set(const std::string& key, int32_t value);
    void set(const std::string& key, double value);
    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, const std::vector<std::string>& value);
    
    bool has(const std::string& key) const;
    void remove(const std::string& key);
    
    // Event: key changed
    using ChangeCallback = void(*)(const std::string& key, const std::string& newValue, void* userData);
    uint64_t onDidChange(ChangeCallback cb, void* userData);
    void offDidChange(uint64_t handle);
    
private:
    friend class ExtensionAPIBridge;
    mutable std::shared_mutex m_mutex;
    std::map<std::string, std::string> m_data; // Stored as JSON strings
    std::map<uint64_t, std::pair<ChangeCallback, void*>> m_listeners;
    std::atomic<uint64_t> m_nextHandle{1};
    std::string m_section;
    
    void notifyChange(const std::string& key);
};

// ============================================================================
// OUTPUT CHANNEL (VS Code: vscode.OutputChannel)
// ============================================================================

class OutputChannel {
public:
    explicit OutputChannel(const std::string& name);
    
    void append(const std::string& value);
    void appendLine(const std::string& value);
    void clear();
    void show(bool preserveFocus = true);
    void hide();
    void dispose();
    
    const std::string& name() const { return m_name; }
    std::string getContents() const;
    
private:
    std::string m_name;
    mutable std::mutex m_mutex;
    std::vector<std::string> m_lines;
    std::atomic<bool> m_visible{false};
};

// ============================================================================
// STATUS BAR ITEM (VS Code: vscode.StatusBarItem)
// ============================================================================

class StatusBarItem {
public:
    StatusBarItem(const std::string& id, StatusBarAlignment alignment, int32_t priority);
    
    void setText(const std::string& text);
    void setTooltip(const std::string& tooltip);
    void setCommand(const std::string& commandId);
    void setColor(const std::string& color); // "#RRGGBB" or theme color ID
    void show();
    void hide();
    void dispose();
    
    std::string id() const { return m_id; }
    StatusBarAlignment alignment() const { return m_alignment; }
    int32_t priority() const { return m_priority; }
    
private:
    std::string m_id;
    StatusBarAlignment m_alignment;
    int32_t m_priority;
    std::string m_text;
    std::string m_tooltip;
    std::string m_commandId;
    std::string m_color;
    std::atomic<bool> m_visible{true};
    mutable std::mutex m_mutex;
};

// ============================================================================
// FILE SYSTEM WATCHER
// ============================================================================

class FileSystemWatcher {
public:
    FileSystemWatcher(const URI& uri, bool recursive, FileWatcherCallback cb, void* userData);
    ~FileSystemWatcher();
    
    void dispose();
    bool isActive() const { return m_active.load(); }
    
private:
    URI m_uri;
    bool m_recursive;
    FileWatcherCallback m_callback;
    void* m_userData;
    std::atomic<bool> m_active{true};
    HANDLE m_hDirectory;
    std::thread m_watcherThread;
    
    void watchLoop();
};

// ============================================================================
// PROGRESS NOTIFICATION (VS Code: vscode.Progress)
// ============================================================================

class ProgressReporter {
public:
    void report(int64_t increment, const std::string& message);
    void done();
    bool isCancelled() const;
    
private:
    friend class ExtensionAPIBridge;
    std::atomic<int64_t> m_current{0};
    std::atomic<int64_t> m_total{100};
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_done{false};
    std::string m_title;
    std::function<void()> m_onCancel;
};

// ============================================================================
// COMMAND REGISTRATION
// ============================================================================

struct CommandReg {
    std::string id;
    std::string label;
    std::string category;
    std::string icon; // Octicon codicon ID
    std::string keybinding; // e.g. "ctrl+shift+p"
    ExtCommandCallback callback;
    ExtAsyncCommandCallback asyncCallback;
    void* userData;
    bool isAsync = false;
};

// ============================================================================
// MAIN BRIDGE
// ============================================================================

class ExtensionAPIBridge {
public:
    static ExtensionAPIBridge& instance();
    
    // ------------------------------------------------------------------------
    // COMMANDS (VS Code: vscode.commands)
    // ------------------------------------------------------------------------
    int32_t registerCommand(const char* id, const char* label, ExtCommandCallback cb, void* userData);
    int32_t registerAsyncCommand(const char* id, const char* label, ExtAsyncCommandCallback cb, void* userData);
    void unregisterCommand(const char* id);
    void executeCommand(const char* id); // Sync
    void* executeCommandAsync(const char* id); // Async, returns result
    bool hasCommand(const char* id) const;
    std::vector<std::string> getCommandIds() const;
    
    // ------------------------------------------------------------------------
    // UI (VS Code: vscode.window)
    // ------------------------------------------------------------------------
    int32_t showMessageBox(const char* title, const char* message, uint32_t flags);
    void showStatusBarMessage(const char* message);
    StatusBarItem* createStatusBarItem(const char* id, StatusBarAlignment alignment, int32_t priority);
    void disposeStatusBarItem(const char* id);
    
    // Quick input dialogs
    bool showInputBox(const char* prompt, const char* placeholder, char* outBuffer, size_t outBufferSize);
    int32_t showQuickPick(const char* const* items, size_t itemCount, const char* placeholder);
    bool showOpenDialog(const char* title, const char* filters, char* outPath, size_t outPathSize);
    bool showSaveDialog(const char* title, const char* defaultName, char* outPath, size_t outPathSize);
    
    // Progress
    ProgressReporter* withProgress(const char* title, bool cancellable);
    
    // ------------------------------------------------------------------------
    // LOGGING (VS Code: vscode.OutputChannel)
    // ------------------------------------------------------------------------
    OutputChannel* createOutputChannel(const char* name);
    void disposeOutputChannel(const char* name);
    void logMessage(int32_t level, const char* message);
    void logDebug(const char* message);
    void logInfo(const char* message);
    void logWarn(const char* message);
    void logError(const char* message);
    
    // ------------------------------------------------------------------------
    // FILE SYSTEM (VS Code: vscode.workspace)
    // ------------------------------------------------------------------------
    bool readFile(const char* path, char** outData, size_t* outLen);
    bool writeFile(const char* path, const char* data, size_t len);
    bool readFileURI(const URI& uri, std::vector<uint8_t>& outData);
    bool writeFileURI(const URI& uri, const std::vector<uint8_t>& data);
    bool stat(const char* path, FileStat* outStat);
    bool statURI(const URI& uri, FileStat* outStat);
    bool readDirectory(const char* path, std::vector<std::string>* outEntries);
    bool createDirectory(const char* path);
    bool deleteFile(const char* path, bool recursive);
    bool rename(const char* src, const char* dst);
    bool exists(const char* path);
    
    FileSystemWatcher* watchFile(const char* path, FileWatcherCallback cb, void* userData);
    FileSystemWatcher* watchDirectory(const char* path, bool recursive, FileWatcherCallback cb, void* userData);
    
    // ------------------------------------------------------------------------
    // CONFIGURATION (VS Code: vscode.workspace.getConfiguration)
    // ------------------------------------------------------------------------
    Configuration* getConfiguration(const char* section);
    void reloadConfiguration(); // Force reload from disk
    
    // ------------------------------------------------------------------------
    // EVENTS (VS Code: vscode.EventEmitter / vscode.Disposable)
    // ------------------------------------------------------------------------
    uint64_t subscribeToEvent(const char* eventType, ExtEventCallback callback, void* userData);
    void unsubscribeFromEvent(uint64_t handle);
    void emitEvent(const char* eventType, const char* jsonPayload);
    void publishEvent(const char* eventType, const char* jsonPayload) { emitEvent(eventType, jsonPayload); } // Alias for compatibility
    
    // ------------------------------------------------------------------------
    // EXTENSION LIFECYCLE
    // ------------------------------------------------------------------------
    void activateExtension(const char* extensionId);
    void deactivateExtension(const char* extensionId);
    bool isExtensionActive(const char* extensionId) const;
    
    // ------------------------------------------------------------------------
    // VS CODE COMPAT SHIM
    // ------------------------------------------------------------------------
    // These map VS Code extension API calls to RawrXD native APIs
    void* vscode_compat_shim(); // Returns a struct of function pointers matching vscode API
    
private:
    ExtensionAPIBridge();
    ~ExtensionAPIBridge();
    
    mutable std::shared_mutex m_cmdMutex;
    std::unordered_map<std::string, CommandReg> m_commands;
    
    mutable std::shared_mutex m_statusMutex;
    std::unordered_map<std::string, std::unique_ptr<StatusBarItem>> m_statusItems;
    
    mutable std::shared_mutex m_channelMutex;
    std::unordered_map<std::string, std::unique_ptr<OutputChannel>> m_channels;
    std::unique_ptr<OutputChannel> m_defaultLogChannel;
    
    mutable std::shared_mutex m_configMutex;
    std::unordered_map<std::string, std::unique_ptr<Configuration>> m_configs;
    std::string m_configPath;
    
    mutable std::shared_mutex m_eventMutex;
    std::unordered_map<std::string, std::vector<std::pair<uint64_t, std::pair<ExtEventCallback, void*>>>> m_eventListeners;
    std::atomic<uint64_t> m_nextEventHandle{1};
    
    mutable std::shared_mutex m_watcherMutex;
    std::vector<std::unique_ptr<FileSystemWatcher>> m_watchers;
    
    mutable std::shared_mutex m_progressMutex;
    std::vector<std::unique_ptr<ProgressReporter>> m_progressReporters;
    
    mutable std::shared_mutex m_extMutex;
    std::unordered_map<std::string, bool> m_extensionActive;
    
    // Internal helpers
    void persistConfig(const std::string& section);
    void loadConfig();
    std::string getConfigFilePath() const;
};

} // namespace RawrXD::Extensions
