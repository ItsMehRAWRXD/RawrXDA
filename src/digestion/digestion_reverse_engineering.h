#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <regex>
#include <atomic>
#include <mutex>
#include <map>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    std::string hash;
    int64_t lastModified;
    int lineCount = 0;
    bool hasStubs = false;
};

struct AgenticTask {
    std::string filePath;
    int lineNumber;
    std::string stubType;
    std::string contextBefore;
    std::string contextAfter;
    std::string fullContext;
    std::string suggestedFix;
    std::string confidence; // "high", "medium", "low"
    bool applied = false;
    int64_t timestamp;
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
    std::atomic<int64_t> elapsedMs{0};
    std::atomic<int64_t> bytesProcessed{0};
};

class DigestionReverseEngineeringSystem {
public:
    explicit DigestionReverseEngineeringSystem(void* parent = nullptr); // Added parent stub for compatibility
    ~DigestionReverseEngineeringSystem();

    void runFullDigestionPipeline(const std::string &rootDir, const DigestionConfig &config);
    void stop();
    bool isRunning() const;
    DigestionStats& stats(); // Return ref to modify/read
    json lastReport() const;
    
    // Callbacks
    std::function<void(int, int, int, int)> onProgress;
    std::function<void(const json&, int64_t)> onFinished;

private:
    void initializeLanguageProfiles();
    void scanDirectory(const std::string &rootDir);
    std::vector<AgenticTask> findStubs(const std::string &content, const LanguageProfile &lang, const FileDigest &file, int maxTasks);
    
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
    DigestionStats m_stats;
    json m_lastReportVec; // We'll store report as json
    
    std::vector<LanguageProfile> m_profiles;
    std::string m_rootDir;
    
    mutable std::mutex m_mutex;
};

struct DigestionStats {
    QAtomicInt totalFiles{0};
    QAtomicInt scannedFiles{0};
    QAtomicInt stubsFound{0};
    QAtomicInt extensionsApplied{0};
    QAtomicInt errors{0};
    QAtomicInt skippedLargeFiles{0};
    QAtomicInt cacheHits{0};
    int64_t elapsedMs = 0;
    int64_t bytesProcessed = 0;
};

class DigestionReverseEngineeringSystem  {public:
    explicit DigestionReverseEngineeringSystem();
    ~DigestionReverseEngineeringSystem();

    void runFullDigestionPipeline(const std::string &rootDir, const DigestionConfig &config = DigestionConfig());
    void stop();
    bool isRunning() const;
    DigestionStats stats() const;
    void* lastReport() const;
    void* generateIncrementalReport(const std::stringList &changedFiles);
    
    // Advanced: Pre-load hash cache for incremental mode
    void loadHashCache(const std::string &cacheFile);
    void saveHashCache(const std::string &cacheFile);

\npublic:\n    void pipelineStarted(const std::string &rootDir, int totalFiles);
    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int percent);
    void fileScanned(const std::string &path, const std::string &language, int stubsInFile);
    void agenticTaskDiscovered(const AgenticTask &task);
    void extensionApplied(const std::string &file, int line, const std::string &fixType);
    void extensionFailed(const std::string &file, int line, const std::string &reason);
    void errorOccurred(const std::string &file, const std::string &error);
    void chunkCompleted(int chunkIndex, int totalChunks);
    void pipelineFinished(const void* &report, int64_t elapsedMs);
    void backupCreated(const std::string &original, const std::string &backupPath);
    void rollbackAvailable(const std::string &backupId);

\npublic:\n    bool rollbackFile(const std::string &backupId);
    bool rollbackAll(const // DateTime &timestamp);
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
    QAtomicInt m_running{0};
    QAtomicInt m_stopRequested{0};
    DigestionStats m_stats;
    void* m_results;
    std::vector<LanguageProfile> m_profiles;
    std::string m_rootDir;
    void* m_lastReport;
    std::chrono::steady_clock::time_point m_timer;
    
    // Caches
    std::map<std::string, std::vector<uint8_t>> m_hashCache; // path -> hash
    std::map<std::string, std::string> m_backupRegistry; // backupId -> original path
    std::map<std::string, int64_t> m_backupTimes; // backupId -> timestamp
    std::map<std::string, LanguageProfile> m_profileCache;
    std::string m_backupDir;
    
    // Threading
    std::unique_ptr<std::threadPool> m_threadPool;
    
    // Git integration
    std::stringList getGitModifiedFiles(const std::string &rootDir);
    std::stringList getGitIgnoredPatterns(const std::string &rootDir);
};

