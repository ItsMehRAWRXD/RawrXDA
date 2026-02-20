// ============================================================================
// digestion_reverse_engineering.h — Digestion RE System (C++20, no Qt)
// ============================================================================
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <regex>
#include <cstdint>

struct LanguageProfile {
    std::string name;
    std::vector<std::string> extensions;
    std::vector<std::regex> stubPatterns;
    std::string singleLineComment;
    std::string multiLineCommentStart;
    std::string multiLineCommentEnd;
    bool supportsInlineAsm = false;
};

struct FileDigest {
    std::string path;
    std::string language;
    std::vector<uint8_t> hash;
    int64_t lastModified = 0;
    int lineCount = 0;
    bool hasStubs = false;
};

struct AgenticTask {
    std::string filePath;
    int lineNumber = 0;
    std::string stubType;
    std::string contextBefore;
    std::string contextAfter;
    std::string fullContext;
    std::string suggestedFix;
    std::string confidence; // "high", "medium", "low"
    bool applied = false;
    int64_t timestamp = 0;
    std::string backupId;
};

struct DigestionConfig {
    int maxFiles = 0;           // 0 = unlimited
    int chunkSize = 50;         // Files per parallel chunk
    int maxTasksPerFile = 0;    // 0 = unlimited stubs per file
    bool applyExtensions = false;
    bool createBackups = true;
    bool useGitMode = false;    // Only scan git-modified files
    bool incremental = true;    // Use hash cache
    int threadCount = 0;        // 0 = auto
    int maxFileSizeMB = 10;     // Skip files larger than this
    std::string backupDir = ".digest_backups";
    std::vector<std::string> excludePatterns; // Regex patterns to exclude
};

struct DigestionStats {
    std::atomic<int> totalFiles{0};
    std::atomic<int> scannedFiles{0};
    std::atomic<int> stubsFound{0};
    std::atomic<int> extensionsApplied{0};
    std::atomic<int> errors{0};
    std::atomic<int> skippedLargeFiles{0};
    std::atomic<int> cacheHits{0};
    int64_t elapsedMs = 0;
    int64_t bytesProcessed = 0;
};

// JSON report type — lightweight struct instead of void*/QJsonObject
struct DigestionReport {
    int totalFiles = 0;
    int scannedFiles = 0;
    int stubsFound = 0;
    int extensionsApplied = 0;
    int errors = 0;
    int64_t elapsedMs = 0;
    int64_t bytesProcessed = 0;
    std::vector<std::string> fileResults; // serialized JSON strings per file
};

class DigestionReverseEngineeringSystem {
public:
    explicit DigestionReverseEngineeringSystem();
    ~DigestionReverseEngineeringSystem();

    void runFullDigestionPipeline(const std::string &rootDir, const DigestionConfig &config = DigestionConfig());
    void stop();
    bool isRunning() const;
    const DigestionStats& stats() const;
    DigestionReport lastReport() const;
    DigestionReport generateIncrementalReport(const std::vector<std::string> &changedFiles);

    // Cache management
    void loadHashCache(const std::string &cacheFile);
    void saveHashCache(const std::string &cacheFile);

    // Callbacks — function pointers replacing Qt signals
    std::function<void(const std::string&, int)> onPipelineStarted;
    std::function<void(int, int, int, int)> onProgressUpdate;
    std::function<void(const std::string&, const std::string&, int)> onFileScanned;
    std::function<void(const AgenticTask&)> onAgenticTaskDiscovered;
    std::function<void(const std::string&, int, const std::string&)> onExtensionApplied;
    std::function<void(const std::string&, int, const std::string&)> onExtensionFailed;
    std::function<void(const std::string&, const std::string&)> onErrorOccurred;
    std::function<void(int, int)> onChunkCompleted;
    std::function<void(const DigestionReport&, int64_t)> onPipelineFinished;
    std::function<void(const std::string&, const std::string&)> onBackupCreated;
    std::function<void(const std::string&)> onRollbackAvailable;

    // Rollback
    bool rollbackFile(const std::string &backupId);
    bool rollbackAll(int64_t beforeTimestampMs);
    void clearCache();

private:
    void initializeLanguageProfiles();
    void scanDirectory(const std::string &rootDir);
    void processChunk(const std::vector<FileDigest> &files, int chunkId, const DigestionConfig &config);
    void scanSingleFile(const FileDigest &fileDigest, const DigestionConfig &config);
    std::vector<AgenticTask> findStubs(const std::string &content, const LanguageProfile &lang, const FileDigest &file, int maxTasks);
    bool applyAgenticFix(const std::string &filePath, const AgenticTask &task, const DigestionConfig &config);
    std::string generateIntelligentFix(const AgenticTask &task, const LanguageProfile &lang);
    std::vector<uint8_t> computeFileHash(const std::string &filePath);
    bool shouldProcessFile(const std::string &filePath, const DigestionConfig &config);
    void createBackup(const std::string &filePath, const std::string &backupId);
    void generateFinalReport();
    void updateProgress();

    // ASM-optimized internal
    std::vector<uint8_t> fastHash(const std::vector<uint8_t> &data);
    bool asmOptimizedScan(const std::vector<uint8_t> &data, const char *pattern);

    mutable std::mutex m_mutex;
    mutable std::mutex m_backupMutex;
    std::atomic<int> m_running{0};
    std::atomic<int> m_stopRequested{0};
    DigestionStats m_stats;
    std::vector<FileDigest> m_results;
    std::vector<LanguageProfile> m_profiles;
    std::string m_rootDir;
    DigestionReport m_lastReport;
    std::chrono::steady_clock::time_point m_timer;

    // Caches
    std::map<std::string, std::vector<uint8_t>> m_hashCache; // path -> hash
    std::map<std::string, std::string> m_backupRegistry; // backupId -> original path
    std::map<std::string, int64_t> m_backupTimes; // backupId -> timestamp
    std::map<std::string, LanguageProfile> m_profileCache;
    std::string m_backupDir;

    // Threading
    std::vector<std::thread> m_threadPool;

    // Git integration
    std::vector<std::string> getGitModifiedFiles(const std::string &rootDir);
    std::vector<std::string> getGitIgnoredPatterns(const std::string &rootDir);
};

