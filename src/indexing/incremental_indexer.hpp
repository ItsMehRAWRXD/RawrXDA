#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <thread>
#include <cstdint>
#include <functional>

/**
 * Incremental Repository Indexing
 * 
 * Monitors repository for file changes and performs targeted re-indexing.
 * - File watching (native Windows FileSystemWatcher)
 * - Changed file detection (modification time + content hash)
 * - Batch processing for bulk changes
 * - Embeddings cache update
 * 
 * NO EXTERNAL DEPENDENCIES except what's already linked
 */

namespace RawrXD::Indexing {

enum class ChangeType {
    ADDED = 0,
    MODIFIED = 1,
    DELETED = 2,
    RENAMED = 3,
};

struct FileChange {
    std::string filePath;
    ChangeType type;
    int64_t detectedAtMs;  // When change was detected
    std::string oldPath;   // For renames
    bool processed = false;
    std::string error;     // If processing failed
};

struct IndexUpdateSummary {
    int filesAdded = 0;
    int filesModified = 0;
    int filesDeleted = 0;
    int embeddingsUpdated = 0;
    int64_t elapsedMs = 0;
    std::vector<std::string> errors;
    bool success = true;
};

/**
 * Incremental indexer for repository
 */
class IncrementalRepositoryIndexer {
public:
    /**
     * Get singleton instance
     */
    static IncrementalRepositoryIndexer& instance();
    
    /**
     * Initialize indexer for a repository
     */
    void initialize(const std::string& repoRoot);
    
    /**
     * Start monitoring for changes
     * callback: Called when changes detected (can batch process)
     */
    void startMonitoring(
        std::function<void(const std::vector<FileChange>&)> callback = nullptr);
    
    /**
     * Stop monitoring
     */
    void stopMonitoring();
    
    /**
     * Manually detect changes (without watching)
     * Returns new and modified files since last scan
     */
    std::vector<FileChange> detectChanges();
    
    /**
     * Process a batch of file changes
     * Recalculates embeddings for changed files, updates index
     */
    IndexUpdateSummary processBatch(const std::vector<FileChange>& changes);
    
    /**
     * Get list of changed files since last indexing
     */
    std::vector<FileChange> getPendingChanges() const;
    
    /**
     * Force full re-index (fall back for watching failures)
     * More expensive but guaranteed
     */
    IndexUpdateSummary forceFullReindex();
    
    /**
     * Get statistics
     */
    struct Stats {
        int filesIndexed = 0;
        int filesDeleted = 0;
        int filesModified = 0;
        int64_t lastUpdateMs = 0;
        bool monitoringActive = false;
    };
    Stats getStats() const;
    
    /**
     * Set debounce interval (ms) for batch processing
     * Changes within this interval are batched together
     */
    void setDebounceMs(int ms) { debounceMs = ms; }
    
    /**
     * Clear all cached state
     */
    void reset();

private:
    IncrementalRepositoryIndexer() = default;
    
    std::string repoRoot;
    std::vector<FileChange> pendingChanges;
    std::map<std::string, int64_t> fileTimestamps;  // path -> mtime
    std::map<std::string, std::string> fileHashes;  // path -> content hash
    
    bool monitoring = false;
    int debounceMs = 500;
    
    std::thread watchThread;
    std::atomic<bool> shouldStop{false};
    
    std::function<void(const std::vector<FileChange>&)> changeCallback;
    
    // Helper: Calculate hash of file for change detection
    static std::string hashFile(const std::string& path);
    
    // Helper: Get modification time
    static int64_t getFileModTime(const std::string& path);
    
    // Helper: Check if file should be indexed (respect .gitignore)
    static bool shouldIndexFile(const std::string& path, const std::string& repoRoot);
    
    // Helper: Watch thread function
    void watchThreadFunc();
    
    // Helper: Scan directory for changes
    void scanDirectory(const std::string& path, std::vector<FileChange>& outChanges);
};

/**
 * File system watcher (Windows-specific stub)
 * Can be extended to use native Windows API
 */
class FileSystemWatcher {
public:
    FileSystemWatcher(const std::string& rootPath);
    ~FileSystemWatcher();
    
    void start();
    void stop();
    bool hasChanges() const;
    std::vector<FileChange> getChanges();

private:
    std::string rootPath;
    bool watching = false;
    // Can be extended with Windows FILE_NOTIFY_CHANGE API
};

} // namespace RawrXD::Indexing
