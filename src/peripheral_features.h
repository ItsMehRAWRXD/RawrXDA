// ============================================================================
// peripheral_features.h — Production-Ready Peripheral Features for RawrXD IDE
// ============================================================================
// Header for various peripheral IDE features
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <nlohmann/json.hpp>

namespace RawrXD {

// ============================================================================
// File System Watcher
// ============================================================================

enum class FileChangeType {
    CREATED,
    MODIFIED,
    DELETED
};

class FileSystemWatcher {
public:
    FileSystemWatcher();
    ~FileSystemWatcher();
    
    bool start(const std::string& path);
    void stop();
    
    std::function<void(const std::string&, FileChangeType)> onFileChanged;
    
private:
    void watchLoop();
    
    std::string watchPath_;
    std::atomic<bool> running_;
    std::thread watchThread_;
};

// ============================================================================
// Recent Files Manager
// ============================================================================

struct RecentFile {
    std::string path;
    std::chrono::system_clock::time_point timestamp;
};

class RecentFilesManager {
public:
    RecentFilesManager();
    
    void addRecentFile(const std::string& filePath);
    std::vector<RecentFile> getRecentFiles(int maxCount = 0) const;
    void clearRecentFiles();
    
private:
    void loadRecentFiles();
    void saveRecentFiles();
    std::string getConfigPath() const;
    
    std::vector<RecentFile> recentFiles_;
};

// ============================================================================
// Clipboard Manager
// ============================================================================

class ClipboardManager {
public:
    ClipboardManager();
    
    bool copyToClipboard(const std::string& text);
    std::string pasteFromClipboard();
    
    std::vector<std::string> getClipboardHistory(int maxItems = 0) const;
    void clearClipboardHistory();
    
private:
    void addToHistory(const std::string& text);
    
    std::vector<std::string> clipboardHistory_;
    size_t maxHistorySize_;
};

// ============================================================================
// Session Manager
// ============================================================================

struct Session {
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    std::vector<std::string> filePaths;
};

class SessionManager {
public:
    SessionManager();
    
    bool saveSession(const std::string& sessionName, 
                    const std::vector<std::string>& filePaths);
    std::vector<std::string> loadSession(const std::string& sessionName);
    std::vector<Session> getSessions() const;
    bool deleteSession(const std::string& sessionName);
    
private:
    void loadSessions();
    void saveSessions();
    std::string getConfigPath() const;
    
    std::vector<Session> sessions_;
};

// ============================================================================
// Theme Manager
// ============================================================================

struct Theme {
    std::string name;
    bool isDark;
    std::unordered_map<std::string, std::string> colors;
};

class ThemeManager {
public:
    ThemeManager();
    
    bool loadTheme(const std::string& themeName);
    bool saveTheme(const Theme& theme);
    std::vector<Theme> getThemes() const;
    
    Theme getCurrentTheme() const { return currentTheme_; }
    
private:
    void loadThemes();
    void saveThemes();
    void loadDefaultThemes();
    std::string getConfigPath() const;
    
    std::vector<Theme> themes_;
    Theme currentTheme_;
};

} // namespace RawrXD
