// ============================================================================
// instructions_provider.hpp — Production Instructions File Reader
// Phase 34: Unified Instructions Context Provider
//
// Reads tools.instructions.md (and other .instructions.md files) from
// configurable search paths. Provides the full text as context to all
// three surfaces: HTTP API, CLI Shell, Win32 GUI.
//
// Thread-safe singleton. Caches content. Supports hot-reload.
// NO exceptions. Returns PatchResult-style structured results.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <thread>
#include <functional>

// ============================================================================
// Result type (matches project convention)
// ============================================================================
struct InstructionsResult {
    bool success;
    const char* detail;
    int errorCode;

    static InstructionsResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static InstructionsResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// A single loaded instructions file
// ============================================================================
struct InstructionsFile {
    std::string filePath;       // Absolute path on disk
    std::string fileName;       // Just the filename
    std::string content;        // Full text content (all lines)
    uint64_t    sizeBytes;      // File size
    uint64_t    lineCount;      // Number of lines
    uint64_t    loadedAtEpochMs;// When it was loaded
    bool        valid;          // Whether the file was read successfully
};

// ============================================================================
// InstructionsProvider — Singleton
// ============================================================================
class InstructionsProvider {
public:
    static InstructionsProvider& instance() {
        static InstructionsProvider inst;
        return inst;
    }

    // ---- Search paths ----
    // Add a directory to search for *.instructions.md files
    void addSearchPath(const std::string& dirPath);

    // Set the default search paths (exe dir, user home, workspace)
    void setDefaultSearchPaths();

    // ---- Loading ----
    // Scan all search paths and load every *.instructions.md found
    InstructionsResult loadAll();

    // Load a specific file by absolute path
    InstructionsResult loadFile(const std::string& filePath);

    // Reload all (re-scan and re-read)
    InstructionsResult reload();

    // ---- Accessors ----
    // Get all loaded instruction files
    std::vector<InstructionsFile> getAll() const;

    // Get a specific file by name (e.g., "tools.instructions.md")
    InstructionsFile getByName(const std::string& fileName) const;

    // Get concatenated content of ALL instruction files
    std::string getAllContent() const;

    // Get content of a specific file by name
    std::string getContent(const std::string& fileName) const;

    // Get the primary instructions file (tools.instructions.md)
    std::string getPrimaryContent() const;

    // Get count of loaded files
    size_t getLoadedCount() const;

    // Get all search paths
    std::vector<std::string> getSearchPaths() const;

    // ---- JSON export (for HTTP API) ----
    std::string toJSON() const;
    std::string toJSONSummary() const;

    // ---- Markdown export (for CLI/GUI display) ----
    std::string toMarkdown() const;

    // ---- Priority Sort ----
    // Add a filename pattern that gets top priority in context buffer.
    // Files matching these patterns are placed first in getAllContent() output.
    // Default priorities: CORE_SAFETY > SECURITY > tools > everything else.
    void addPriorityPattern(const std::string& filenamePattern, int priority);

    // Clear all custom priority patterns (resets to defaults)
    void clearPriorityPatterns();

    // ---- Hot-Reload File Watcher ----
    // Start watching all search paths for *.instructions.md changes.
    // poll interval in milliseconds (default 500ms). On change, reloads.
    void startFileWatcher(uint32_t pollIntervalMs = 500);

    // Stop the file watcher thread
    void stopFileWatcher();

    // Check if watcher is running
    bool isWatcherRunning() const { return m_watcherRunning.load(); }

    // Register a callback for when files change (optional, for IDE integration)
    void setOnReloadCallback(std::function<void()> cb);

    // ---- Status ----
    bool isLoaded() const { return m_loaded.load(); }
    uint64_t getLastLoadTimeMs() const { return m_lastLoadTimeMs.load(); }

private:
    InstructionsProvider();

    // Internal file reading (no exceptions, returns result)
    InstructionsResult readFileInternal(const std::string& path, InstructionsFile& out);

    // Scan a directory for *.instructions.md files
    std::vector<std::string> scanDirectory(const std::string& dirPath) const;

    // Get executable directory
    std::string getExeDirectory() const;

    // Get user home directory
    std::string getUserHomeDirectory() const;

    // Sort m_files by priority (call after loading, under lock)
    void sortByPriority();

    // Get priority for a filename (lower number = higher priority)
    int getPriorityForFile(const std::string& fileName) const;

    // File watcher thread function
    void watcherThreadFunc(uint32_t pollIntervalMs);

    mutable std::mutex              m_mutex;
    std::vector<std::string>        m_searchPaths;
    std::vector<InstructionsFile>   m_files;
    std::atomic<bool>               m_loaded{false};
    std::atomic<uint64_t>           m_lastLoadTimeMs{0};

    // Priority patterns: pair<substring_pattern, priority_value>
    // Lower priority value = appears first in context buffer
    std::vector<std::pair<std::string, int>> m_priorityPatterns;

    // Hot-reload watcher
    std::thread                     m_watcherThread;
    std::atomic<bool>               m_watcherRunning{false};
    std::atomic<bool>               m_watcherStop{false};
    std::function<void()>           m_onReloadCallback;
    std::mutex                      m_callbackMutex;
};
