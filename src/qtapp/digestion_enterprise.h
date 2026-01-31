// digestion_enterprise.h
// Production-hardened enterprise extensions for DigestionReverseEngineeringSystem
// Features: LLM integration, SQLite checkpointing, crash recovery, atomic writes, concurrency control
// Created: 2026-01-24

#pragma once
#include "digestion_reverse_engineering.h"
#include <functional>
#include <memory>

// Forward declare LLM client (connect to your existing LLMHttpClient)
class LLMHttpClient;

// -----------------------------------------------------------------------------
// Enterprise structs for production-grade digestion
// -----------------------------------------------------------------------------

struct LanguageProfile {
    std::string name;
    std::stringList extensions;
    std::vector<std::regex> stubPatterns;
    std::vector<std::regex> contextPatterns;  // For AST-level detection
    std::string singleLineComment;
    std::string multiLineCommentStart;
    std::string multiLineCommentEnd;
    bool requiresPreprocessing = false;
};

struct AgenticTask {
    std::string id;                 // UUID for tracking
    std::string filePath;
    int lineNumber = 0;
    int columnNumber = 0;
    std::string stubType;
    std::string severity;           // "critical", "warning", "info"
    std::string contextBefore;      // 10 lines before
    std::string stubLine;
    std::string contextAfter;       // 10 lines after
    std::string suggestedFix;
    std::string llmPrompt;          // The actual prompt sent to LLM
    std::string llmResponse;        // Raw LLM response
    bool applied = false;
    bool backedUp = false;
    std::string backupPath;
    // DateTime timestamp;
    int attemptCount = 0;
    std::string errorString;
};

struct DigestionCheckpoint {
    std::string sessionId;
    int totalFiles = 0;
    int processedFiles = 0;
    int currentChunk = 0;
    std::string lastFilePath;
    bool completed = false;
};

struct DigestionStats {
    QAtomicInt totalFiles{0};
    QAtomicInt scannedFiles{0};
    QAtomicInt stubsFound{0};
    QAtomicInt extensionsApplied{0};
    QAtomicInt extensionsFailed{0};
    QAtomicInt errors{0};
    QAtomicInt cacheHits{0};
    std::chrono::steady_clock::time_point timer;
    // DateTime startTime;
    std::string sessionId;
};

// -----------------------------------------------------------------------------
// DigestionEnterprise: production-hardened wrapper around the base system
// Adds: LLM integration, checkpointing, crash recovery, atomic writes, concurrency
// -----------------------------------------------------------------------------

class DigestionEnterprise  {

public:
    explicit DigestionEnterprise(LLMHttpClient *llmClient = nullptr, void* parent = nullptr);
    ~DigestionEnterprise();

    // ==================== Core Pipeline ====================

    // Initialize SQLite database for checkpointing and caching
    bool initializeDatabase(const std::string &dbPath = "digestion_cache.db");

    // Run full digestion pipeline with all enterprise features
    // - rootDir: project root
    // - maxFiles: 0 = unlimited
    // - chunkSize: files per batch (default 50)
    // - maxTasksPerFile: 0 = unlimited stubs per file
    // - applyExtensions: true = modify files, false = dry-run
    // - useLLM: true = query LLM for intelligent fixes
    // - maxConcurrency: parallel workers (tune to CPU cores)
    void runFullDigestionPipeline(const std::string &rootDir,
                                  int maxFiles = 0,
                                  int chunkSize = 50,
                                  int maxTasksPerFile = 0,
                                  bool applyExtensions = true,
                                  bool useLLM = true,
                                  int maxConcurrency = 8);

    // ==================== Checkpoint Control ====================

    bool resumeFromCheckpoint(const std::string &sessionId);
    void createCheckpoint();
    void clearCheckpoint(const std::string &sessionId = std::string());

    // ==================== Safety Controls ====================

    void emergencyStop();
    void rollbackFile(const std::string &filePath);
    void rollbackSession();
    bool isRunning() const;

    // ==================== Configuration ====================

    void setBackupEnabled(bool enabled) { m_backupEnabled = enabled; }
    void setLLMTimeout(int ms) { m_llmTimeout = ms; }
    void setDryRun(bool dry) { m_dryRun = dry; }

    // ==================== Results ====================

    DigestionStats stats() const;
    void* lastReport() const;
    std::vector<AgenticTask> pendingTasks() const;
    std::stringList modifiedFiles() const;
\npublic:\n    void pipelineStarted(std::string sessionId, int totalFiles);
    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int percent);
    void fileScanned(const std::string &path, int stubsInFile, std::string language);
    void chunkCompleted(int chunkIndex, int chunksTotal, int filesInChunk);
    void agenticTaskDiscovered(const AgenticTask &task);
    void llmQueryStarted(const std::string &taskId);
    void llmQueryCompleted(const std::string &taskId, bool success, int tokensUsed);
    void extensionApplied(const std::string &file, int line, std::string changeType);
    void extensionFailed(const std::string &file, int line, std::string error);
    void backupCreated(const std::string &original, std::string backup);
    void checkpointSaved(std::string sessionId, int progress);
    void pipelineFinished(const void* &report, bool success);
    void errorOccurred(const std::string &file, const std::string &error, bool fatal);
    void statisticsUpdate(const DigestionStats &stats);
\nprivate:\n    void onLLMResponse(const std::string &taskId, const std::string &response, bool success);

private:
    // ==================== Initialization ====================

    void initializeLanguageProfiles();
    bool loadCheckpoint(const std::string &sessionId);
    void saveCheckpoint();

    // ==================== File Processing ====================

    void scanFile(const std::string &filePath, int maxTasksPerFile, bool applyExtensions, bool useLLM);
    void processChunk(const std::stringList &files, int chunkId, int totalChunks,
                      int maxTasksPerFile, bool applyExtensions, bool useLLM);
    std::string detectLanguage(const std::string &filePath);

    // ==================== Advanced Detection ====================

    std::vector<AgenticTask> findStubsAdvanced(const std::string &content,
                                         const LanguageProfile &lang,
                                         const std::string &filePath,
                                         int maxTasks);
    std::vector<AgenticTask> findStubsAST(const std::string &content,
                                    const LanguageProfile &lang,
                                    const std::string &filePath);

    // ==================== LLM Integration ====================

    void queryLLMForFix(AgenticTask &task, const LanguageProfile &lang);
    std::string constructLLMPrompt(const AgenticTask &task, const LanguageProfile &lang);
    std::string parseLLMResponse(const std::string &response, const LanguageProfile &lang);

    // ==================== Application with Safety ====================

    bool applyAgenticExtension(AgenticTask &task, const LanguageProfile &lang);
    bool createBackup(const std::string &filePath);
    bool restoreFromBackup(const std::string &filePath);
    std::string generateDiff(const std::string &original, const std::string &modified);

    // ==================== Database Operations ====================

    void cacheTask(const AgenticTask &task);
    std::vector<AgenticTask> loadCachedTasks(const std::string &filePath);
    void markTaskCompleted(const std::string &taskId);

    // ==================== Reporting ====================

    void generateReport(bool success);
    void streamReportChunk(const void* &chunk);

    // ==================== Member Variables ====================

    mutable std::mutex m_mutex;
    mutable std::mutex m_dbMutex;
    QAtomicInt m_running{0};
    QAtomicInt m_stopRequested{0};
    QSemaphore *m_concurrencySemaphore = nullptr;

    DigestionStats m_stats;
    QSqlDatabase m_db;
    std::string m_rootDir;
    std::string m_sessionId;
    void* m_results;
    void* m_lastReport;
    QTemporaryDir m_backupDir;
    std::stringList m_modifiedFiles;
    std::vector<AgenticTask> m_pendingTasks;
    LLMHttpClient *m_llmClient = nullptr;
    std::map<std::string, AgenticTask*> m_activeLLMQueries;

    // Configuration
    bool m_backupEnabled = true;
    bool m_dryRun = false;
    int m_llmTimeout = 30000;
    int m_chunkSize = 50;
    std::vector<LanguageProfile> m_profiles;
    std::chrono::steady_clock::time_point m_timer;

    // Reference to base system for pattern access
    std::unique_ptr<DigestionReverseEngineeringSystem> m_baseSystem;
};





