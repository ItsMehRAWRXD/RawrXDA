#include "incremental_indexer.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>

namespace fs = std::filesystem;

namespace RawrXD::Indexing {

// ============================================================================
// Hash calculation
// ============================================================================

std::string IncrementalRepositoryIndexer::hashFile(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";
        
        // Simple hash: XOR all bytes
        unsigned long hash = 0;
        unsigned char byte;
        while (file.read((char*)&byte, 1)) {
            hash = hash * 31 + byte;
        }
        
        return std::to_string(hash);
    } catch (...) {
        return "";
    }
}

int64_t IncrementalRepositoryIndexer::getFileModTime(const std::string& path) {
    try {
        auto lastWriteTime = fs::last_write_time(path);
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            lastWriteTime.time_since_epoch()).count();
    } catch (...) {
        return 0;
    }
}

bool IncrementalRepositoryIndexer::shouldIndexFile(
    const std::string& path, const std::string& repoRoot) {
    
    // Skip directories
    if (fs::is_directory(path)) return false;
    
    std::string rel = fs::relative(path, repoRoot).string();
    
    // Skip common non-indexable paths
    if (rel.find(".git\\") == 0 || rel.find(".git/") == 0) return false;
    if (rel.find("node_modules\\") == 0 || rel.find("node_modules/") == 0) return false;
    if (rel.find(".vscode\\") == 0 || rel.find(".vscode/") == 0) return false;
    if (rel.find("__pycache__\\") == 0 || rel.find("__pycache__/") == 0) return false;
    if (rel.find("build\\") == 0 || rel.find("build/") == 0) return false;
    if (rel.find("dist\\") == 0 || rel.find("dist/") == 0) return false;
    
    // Only index text files
    std::string ext = fs::path(path).extension().string();
    static const char* indexableExts[] = {
        ".cpp", ".h", ".hpp", ".c", ".cc", ".cxx", ".cs", ".java",
        ".py", ".js", ".ts", ".tsx", ".jsx", ".go", ".rs", ".rb",
        ".md", ".txt", ".json", ".xml", ".yaml", ".yml", ".toml",
        ".cmake", ".sh", ".bat", ".ps1", ".sql"
    };
    
    for (const auto* e : indexableExts) {
        if (ext == e) return true;
    }
    
    return false;
}

void IncrementalRepositoryIndexer::scanDirectory(
    const std::string& path, std::vector<FileChange>& outChanges) {
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (!shouldIndexFile(entry.path().string(), repoRoot)) {
                continue;
            }
            
            std::string filePath = entry.path().string();
            int64_t mtime = getFileModTime(filePath);
            std::string hash = hashFile(filePath);
            
            auto tsIt = fileTimestamps.find(filePath);
            auto hashIt = fileHashes.find(filePath);
            
            if (tsIt == fileTimestamps.end()) {
                // New file
                FileChange change;
                change.filePath = filePath;
                change.type = ChangeType::ADDED;
                change.detectedAtMs = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                outChanges.push_back(change);
                
                fileTimestamps[filePath] = mtime;
                fileHashes[filePath] = hash;
                
            } else if (tsIt->second != mtime || hashIt->second != hash) {
                // Modified file
                FileChange change;
                change.filePath = filePath;
                change.type = ChangeType::MODIFIED;
                change.detectedAtMs = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                outChanges.push_back(change);
                
                fileTimestamps[filePath] = mtime;
                fileHashes[filePath] = hash;
            }
        }
        
        // Check for deleted files
        std::vector<std::string> deletedFiles;
        for (const auto& [filePath, _] : fileTimestamps) {
            if (!fs::exists(filePath)) {
                FileChange change;
                change.filePath = filePath;
                change.type = ChangeType::DELETED;
                change.detectedAtMs = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                outChanges.push_back(change);
                deletedFiles.push_back(filePath);
            }
        }
        
        for (const auto& filePath : deletedFiles) {
            fileTimestamps.erase(filePath);
            fileHashes.erase(filePath);
        }
        
    } catch (...) {
        // Silent fail on filesystem errors
    }
}

void IncrementalRepositoryIndexer::watchThreadFunc() {
    while (!shouldStop) {
        // Sleep for debounce interval
        std::this_thread::sleep_for(std::chrono::milliseconds(debounceMs));
        
        // Detect changes
        auto changes = detectChanges();
        
        if (!changes.empty() && changeCallback) {
            changeCallback(changes);
        }
    }
}

// ============================================================================
// IncrementalRepositoryIndexer Implementation
// ============================================================================

IncrementalRepositoryIndexer& IncrementalRepositoryIndexer::instance() {
    static IncrementalRepositoryIndexer inst;
    return inst;
}

void IncrementalRepositoryIndexer::initialize(const std::string& root) {
    repoRoot = root;
    reset();
}

void IncrementalRepositoryIndexer::startMonitoring(
    std::function<void(const std::vector<FileChange>&)> callback) {
    
    if (monitoring) return;
    
    monitoring = true;
    changeCallback = callback;
    shouldStop = false;
    
    // Start watch thread
    watchThread = std::thread(&IncrementalRepositoryIndexer::watchThreadFunc, this);
}

void IncrementalRepositoryIndexer::stopMonitoring() {
    if (!monitoring) return;
    
    monitoring = false;
    shouldStop = true;
    
    if (watchThread.joinable()) {
        watchThread.join();
    }
}

std::vector<FileChange> IncrementalRepositoryIndexer::detectChanges() {
    std::vector<FileChange> changes;
    scanDirectory(repoRoot, changes);
    pendingChanges.insert(pendingChanges.end(), changes.begin(), changes.end());
    return changes;
}

IndexUpdateSummary IncrementalRepositoryIndexer::processBatch(
    const std::vector<FileChange>& changes) {
    
    IndexUpdateSummary summary;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (const auto& change : changes) {
        try {
            if (change.type == ChangeType::ADDED ||
                change.type == ChangeType::MODIFIED) {
                // Would call vector_index to recalculate embeddings
                summary.embeddingsUpdated++;
                
                if (change.type == ChangeType::ADDED) {
                    summary.filesAdded++;
                } else {
                    summary.filesModified++;
                }
            } else if (change.type == ChangeType::DELETED) {
                // Would call vector_index to remove embeddings
                summary.filesDeleted++;
            }
        } catch (...) {
            summary.errors.push_back("Failed to process: " + change.filePath);
            summary.success = false;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    summary.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    // Mark as processed
    for (auto& change : pendingChanges) {
        for (const auto& processedChange : changes) {
            if (change.filePath == processedChange.filePath) {
                change.processed = true;
            }
        }
    }
    
    return summary;
}

std::vector<FileChange> IncrementalRepositoryIndexer::getPendingChanges() const {
    std::vector<FileChange> unprocessed;
    for (const auto& change : pendingChanges) {
        if (!change.processed) {
            unprocessed.push_back(change);
        }
    }
    return unprocessed;
}

IndexUpdateSummary IncrementalRepositoryIndexer::forceFullReindex() {
    pendingChanges.clear();
    fileTimestamps.clear();
    fileHashes.clear();
    
    auto changes = detectChanges();
    return processBatch(changes);
}

IncrementalRepositoryIndexer::Stats IncrementalRepositoryIndexer::getStats() const {
    Stats s;
    s.filesIndexed = fileTimestamps.size();
    s.monitoringActive = monitoring;
    
    for (const auto& change : pendingChanges) {
        if (change.type == ChangeType::ADDED) s.filesIndexed++;
        else if (change.type == ChangeType::DELETED) s.filesDeleted++;
        else if (change.type == ChangeType::MODIFIED) s.filesModified++;
    }
    
    return s;
}

void IncrementalRepositoryIndexer::reset() {
    pendingChanges.clear();
    fileTimestamps.clear();
    fileHashes.clear();
}

// ============================================================================
// FileSystemWatcher Implementation
// ============================================================================

FileSystemWatcher::FileSystemWatcher(const std::string& root) 
    : rootPath(root) {
}

FileSystemWatcher::~FileSystemWatcher() {
    stop();
}

void FileSystemWatcher::start() {
    watching = true;
    // Can be extended with Windows FileSystemWatcher API
}

void FileSystemWatcher::stop() {
    watching = false;
}

bool FileSystemWatcher::hasChanges() const {
    return watching;
}

std::vector<FileChange> FileSystemWatcher::getChanges() {
    // Production implementation: use FindFirstChangeNotification for Windows
    std::vector<FileChange> changes;
    if (!watching || rootPath.empty()) {
        return changes;
    }

    // Poll directory modification time as lightweight change detection
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    std::wstring wpath(rootPath.begin(), rootPath.end());
    if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &attrs)) {
        static ULONGLONG lastWriteTime = 0;
        ULONGLONG currentWriteTime = (static_cast<ULONGLONG>(attrs.ftLastWriteTime.dwHighDateTime) << 32) |
                                      attrs.ftLastWriteTime.dwLowDateTime;
        if (currentWriteTime != lastWriteTime) {
            lastWriteTime = currentWriteTime;
            FileChange change;
            change.filePath = rootPath;
            change.type = ChangeType::MODIFIED;
            changes.push_back(change);
        }
    }
    return changes;
}

} // namespace RawrXD::Indexing
