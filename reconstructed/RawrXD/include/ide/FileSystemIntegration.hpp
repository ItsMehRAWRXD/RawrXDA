// ═══════════════════════════════════════════════════════════════════════════════
// FileSystemIntegration.hpp
// Live File Watcher + Text Encoding Detection + Dirty Buffer Tracking
// Pure Win32 API, C++20, Zero Dependencies
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <unordered_map>

namespace RawrXD::FileSystem {

// ─────────────────────────────────────────────────────────────────────────────
// Text Encoding
// ─────────────────────────────────────────────────────────────────────────────

enum class TextEncoding : uint32_t {
    UTF8        = 0,    // UTF-8 (with or without BOM)
    UTF8_BOM    = 1,    // UTF-8 with BOM (EF BB BF)
    UTF16_LE    = 2,    // UTF-16 Little Endian (FF FE)
    UTF16_BE    = 3,    // UTF-16 Big Endian (FE FF)
    ASCII       = 4,    // Pure 7-bit ASCII
    LATIN1      = 5,    // ISO 8859-1 / Windows-1252
    UNKNOWN     = 0xFF
};

enum class LineEnding : uint32_t {
    CRLF = 0,   // Windows (\r\n)
    LF   = 1,   // Unix (\n)
    CR   = 2,   // Old Mac (\r)
    MIXED = 3,  // File has mixed endings
    UNKNOWN = 0xFF
};

struct EncodingInfo {
    TextEncoding encoding   = TextEncoding::UTF8;
    LineEnding   lineEnding = LineEnding::CRLF;
    uint32_t     bomSize    = 0;        // BOM byte count (0, 2, or 3)
    bool         hasBOM     = false;
    bool         hasNulls   = false;    // Binary file indicator
};

/// Detect encoding and line ending from raw file bytes
EncodingInfo DetectEncoding(const uint8_t* data, size_t size);

/// Convert file bytes to UTF-8 std::string based on detected encoding
std::string DecodeToUTF8(const uint8_t* data, size_t size, const EncodingInfo& info);

/// Encode UTF-8 string back to original encoding for saving
std::vector<uint8_t> EncodeFromUTF8(const std::string& utf8, const EncodingInfo& info);

/// Convert line endings in a string
std::string NormalizeLineEndings(const std::string& text, LineEnding target);

/// Get human-readable encoding name (for status bar)
const char* EncodingName(TextEncoding enc);
const char* LineEndingName(LineEnding le);


// ─────────────────────────────────────────────────────────────────────────────
// Dirty Buffer Tracking
// ─────────────────────────────────────────────────────────────────────────────

struct BufferState {
    std::string   filePath;             // Absolute path on disk
    std::string   content;              // Current buffer content (UTF-8)
    std::string   diskContent;          // Last known content on disk (UTF-8)
    EncodingInfo  encoding;             // Detected file encoding
    uint64_t      diskModTimeEpochMs;   // Last known disk modification time
    uint64_t      lastEditEpochMs;      // Last in-memory edit time
    uint32_t      editSequence;         // Monotonic edit counter
    uint32_t      savedAtSequence;      // Edit sequence at last save
    bool          isNewFile;            // Never saved to disk
    bool          externallyModified;   // Disk changed under us

    bool IsDirty() const { return editSequence != savedAtSequence; }
};

class BufferManager {
public:
    BufferManager() = default;
    ~BufferManager() = default;

    /// Open a file: reads from disk, detects encoding, creates buffer
    bool OpenFile(const std::wstring& path, BufferState& out_state);

    /// Save buffer back to disk using original encoding
    bool SaveFile(BufferState& state);

    /// Save buffer to a new path (Save As)
    bool SaveFileAs(BufferState& state, const std::wstring& newPath);

    /// Mark buffer as edited (bumps edit sequence)
    void MarkEdited(BufferState& state);

    /// Check if disk file has changed since last load/save
    bool CheckExternalModification(BufferState& state);

    /// Reload from disk (after external modification)
    bool ReloadFromDisk(BufferState& state);

    /// Get disk modification time for a file
    static uint64_t GetFileModTime(const std::wstring& path);

private:
    /// Read raw file bytes using Win32 API (no CRT)
    static bool ReadFileRaw(const std::wstring& path, std::vector<uint8_t>& out_data);

    /// Write raw bytes using Win32 API
    static bool WriteFileRaw(const std::wstring& path, const uint8_t* data, size_t size);
};


// ─────────────────────────────────────────────────────────────────────────────
// Live File Watcher (ReadDirectoryChangesW)
// ─────────────────────────────────────────────────────────────────────────────

enum class FileChangeType : uint32_t {
    Created   = FILE_ACTION_ADDED,          // 1
    Deleted   = FILE_ACTION_REMOVED,        // 2
    Modified  = FILE_ACTION_MODIFIED,       // 3
    RenamedFrom = FILE_ACTION_RENAMED_OLD_NAME, // 4
    RenamedTo   = FILE_ACTION_RENAMED_NEW_NAME  // 5
};

struct FileChangeEvent {
    FileChangeType type;
    std::wstring   filePath;    // Relative path from watched directory
    std::wstring   watchedDir;  // The root directory being watched
};

/// Callback signature: void(const FileChangeEvent& event)
using FileChangeCallback = std::function<void(const FileChangeEvent&)>;

class DirectoryWatcher {
public:
    DirectoryWatcher();
    ~DirectoryWatcher();

    /// Start watching a directory (recursive).
    /// Returns false if directory doesn't exist or watch fails.
    bool Watch(const std::wstring& directoryPath, FileChangeCallback callback);

    /// Stop watching a specific directory.
    void Unwatch(const std::wstring& directoryPath);

    /// Stop all watches and shutdown watcher thread.
    void ShutdownAll();

    /// Check if currently watching any directory.
    bool IsWatching() const;

    /// Get list of currently watched directories.
    std::vector<std::wstring> GetWatchedPaths() const;

private:
    struct WatchEntry {
        std::wstring   dirPath;
        HANDLE         dirHandle  = INVALID_HANDLE_VALUE;
        OVERLAPPED     overlapped = {};
        uint8_t        buffer[32768];  // FILE_NOTIFY_INFORMATION buffer
        FileChangeCallback callback;
        bool           active = false;
    };

    void WatcherThread();
    bool IssueRead(WatchEntry& entry);
    void ProcessNotifications(WatchEntry& entry, DWORD bytesTransferred);

    std::vector<WatchEntry>  m_watches;
    std::mutex               m_mutex;
    std::thread              m_thread;
    std::atomic<bool>        m_shutdown{false};
    HANDLE                   m_wakeEvent = nullptr;
};

} // namespace RawrXD::FileSystem
