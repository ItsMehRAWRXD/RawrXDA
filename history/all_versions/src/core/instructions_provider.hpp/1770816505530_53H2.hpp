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

    mutable std::mutex              m_mutex;
    std::vector<std::string>        m_searchPaths;
    std::vector<InstructionsFile>   m_files;
    std::atomic<bool>               m_loaded{false};
    std::atomic<uint64_t>           m_lastLoadTimeMs{0};
};
