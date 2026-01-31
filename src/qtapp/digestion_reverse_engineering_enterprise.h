#pragma once
#include <functional>
#include <memory>


struct LanguageProfile {
    std::string name;
    std::stringList extensions;
    std::vector<std::regex> stubPatterns;
    std::string singleLineComment;
    std::string multiLineCommentStart;
    std::string multiLineCommentEnd;
    std::map<std::string, std::string> fixTemplates;
    int complexityWeight = 1;
};

struct CodeMetrics {
    int linesOfCode = 0;
    int commentLines = 0;
    int blankLines = 0;
    int cyclomaticComplexity = 0;
    int maxNestingDepth = 0;
    double maintainabilityIndex = 0.0;
};

struct AgenticTask {
    std::string filePath;
    int lineNumber;
    int column;
    std::string stubType;
    std::string severity; // "critical", "warning", "info"
    std::string contextBefore;
    std::string contextAfter;
    std::string fullFunctionContext;
    std::string suggestedFix;
    std::string originalCode;
    bool applied = false;
    int64_t processingTimeMs = 0;
    std::vector<std::string> dependencies;
};

struct DigestionStats {
    QAtomicInt totalFiles{0};
    QAtomicInt scannedFiles{0};
    QAtomicInt stubsFound{0};
    QAtomicInt extensionsApplied{0};
    QAtomicInt errors{0};
    QAtomicInt warnings{0};
    QAtomicInt criticals{0};
    std::map<std::string, int> stubsByLanguage;
    std::map<std::string, int> stubsByType;
    std::chrono::steady_clock::time_point timer;
    int64_t peakMemoryUsage = 0;
    int parallelWorkers = 0;
};

struct DigestionCheckpoint {
    std::string rootDir;
    int lastProcessedIndex;
    // DateTime timestamp;
    void* pendingResults;
    DigestionStats stats;
};

class DigestionReverseEngineeringSystem  {

public:
    explicit DigestionReverseEngineeringSystem();
    ~DigestionReverseEngineeringSystem();

    // Main pipeline
    void runFullDigestionPipeline(const std::string &rootDir, 
                                  int maxFiles = 0,
                                  int chunkSize = 50, 
                                  int maxTasksPerFile = 0,
                                  bool applyExtensions = true,
                                  const std::stringList &excludePatterns = std::stringList());
    
    // Checkpoint/resume for 1500+ file codebases
    void saveCheckpoint(const std::string &path);
    bool loadCheckpoint(const std::string &path);
    void resumeFromCheckpoint(const std::string &checkpointPath, bool applyExtensions = true);
    
    // Incremental scanning (git diff aware)
    void runIncrementalDigestion(const std::string &rootDir, 
                                 const std::stringList &modifiedFiles,
                                 bool applyExtensions = true);
    
    // Advanced analysis
    std::map<std::string, CodeMetrics> analyzeCodeMetrics(const std::stringList &files);
    std::vector<std::pair<std::string, std::string>> findDuplicateCode(int minLines = 5);
    std::map<std::string, std::vector<std::string>> buildDependencyGraph();
    
    // Control
    void stop();
    void pause();
    void resume();
    bool isRunning() const;
    bool isPaused() const;
    
    // Results
    DigestionStats stats() const;
    void* lastReport() const;
    std::vector<AgenticTask> pendingTasks() const;
    std::string generatePatchFile(const std::string &originalDir, const std::string &modifiedDir);
    
    // GUI Integration helpers
    void bindToProgressBar(void *bar);
    void bindToLogView(void *log);
    void bindToTreeView(QTreeWidget *tree);
\npublic:\n    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int percentComplete);
    void fileScanned(const std::string &path, int stubsInFile, const std::string &language);
    void agenticTaskGenerated(const AgenticTask &task);
    void extensionApplied(const std::string &file, int line, const std::string &fixType);
    void extensionFailed(const std::string &file, int line, const std::string &reason);
    void errorOccurred(const std::string &file, const std::string &error, bool critical);
    void warningIssued(const std::string &file, const std::string &warning);
    void pipelineFinished(const void* &report, int64_t elapsedMs);
    void chunkCompleted(int chunkIndex, int chunksTotal, int stubsInChunk);
    void metricsCalculated(const std::string &file, const CodeMetrics &metrics);
    void duplicateFound(const std::string &file1, const std::string &file2, int line1, int line2, int similarity);
    void runningChanged(bool running);
    void statsChanged(const DigestionStats &stats);
    void checkpointSaved(const std::string &path);
\nprivate:\n    void onFileProcessed(const std::string &path, const void* &result);
    void onTaskApplied(const std::string &file, int line, bool success);

private:
    void initializeLanguageProfiles();
    void initializeFixTemplates();
    std::stringList collectFiles(const std::string &rootDir, int maxFiles, const std::stringList &excludes);
    void processChunk(const std::stringList &files, int chunkId, int totalChunks, 
                      int maxTasksPerFile, bool applyExtensions);
    void scanFile(const std::string &filePath, int maxTasksPerFile, bool applyExtensions);
    std::vector<AgenticTask> findStubs(const std::string &content, const LanguageProfile &lang, 
                                  const std::string &filePath, int maxTasks);
    bool applyAgenticExtension(const std::string &filePath, const AgenticTask &task);
    std::string generateFix(const AgenticTask &task, const LanguageProfile &lang);
    std::string detectLanguage(const std::string &filePath);
    CodeMetrics calculateMetrics(const std::string &content, const LanguageProfile &lang);
    std::string extractFunctionContext(const std::string &content, int lineNumber);
    std::string generateUnifiedDiff(const std::string &original, const std::string &modified, 
                                const std::string &filename);
    void updateStats();
    void emitProgress();
    void generateReport();
    
    // Threading control
    mutable std::mutex m_mutex;
    mutable std::shared_mutex m_statsLock;
    QSemaphore m_pauseSemaphore{0};
    QAtomicInt m_running{0};
    QAtomicInt m_paused{0};
    QAtomicInt m_stopRequested{0};
    QAtomicInt m_currentFileIndex{0};
    
    // Data
    DigestionStats m_stats;
    void* m_results;
    std::vector<AgenticTask> m_pendingTasks;
    std::vector<LanguageProfile> m_profiles;
    std::string m_rootDir;
    void* m_lastReport;
    std::chrono::steady_clock::time_point m_timer;
    std::map<std::string, LanguageProfile> m_profileMap;
    
    // GUI bindings (weak refs)
    void *m_boundProgress = nullptr;
    void *m_boundLog = nullptr;
    QTreeWidget *m_boundTree = nullptr;
    
    // Checkpointing
    DigestionCheckpoint m_checkpoint;
    bool m_hasCheckpoint = false;
};

