#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <queue>
#include <chrono>
#include <regex>
#include <thread>

// Forward declarations — SQLite3 C API (no Qt)
struct sqlite3;
struct sqlite3_stmt;

// Forward declarations for AI system integration
class CompletionEngine;
class CodebaseContextAnalyzer;
class SmartRewriteEngine;
class MultiModalModelRouter;
class AdvancedCodingAgent;

struct DigestionTask {
    int id;
    std::string filePath;
    std::string language;
    int priority;
    // DateTime created;
    // DateTime started;
    // DateTime completed;
    int stubsFound;
    int stubsFixed;
    std::string status; // pending|running|completed|error
    std::string errorMsg;
};

struct StubInstance {
    int id;
    int taskId;
    std::string filePath;
    int lineNumber;
    std::string stubType;
    std::string originalCode;
    std::string context; // Added for context-aware fixes
    std::string suggestedFix;
    std::string appliedFix;
    bool applied;
    float confidenceScore;
    // DateTime detected;
    // DateTime fixed;
};

class RawrXDDigestionEngine  {public:
    explicit RawrXDDigestionEngine();
    ~RawrXDDigestionEngine();
    
    // Core pipeline
    void initializeDatabase(const std::string &dbPath = "digestion.db");
    void runFullDigestionPipeline(const std::string &rootDir, 
                                  int maxFiles = 0,
                                  int chunkSize = 50,
                                  int maxTasksPerFile = 0,
                                  bool applyExtensions = true,
                                  bool useAVX512 = true);
    
    // AI Integration hooks
    void setCompletionEngine(CompletionEngine *engine);
    void setContextAnalyzer(CodebaseContextAnalyzer *analyzer);
    void setRewriteEngine(SmartRewriteEngine *engine);
    void setModelRouter(MultiModalModelRouter *router);
    void setCodingAgent(AdvancedCodingAgent *agent);
    
    // Control
    void pause();
    void resume();
    void stop();
    bool isRunning() const;
    bool isPaused() const;
    
    // Queries
    std::vector<DigestionTask> getTaskHistory(int limit = 100);
    std::vector<StubInstance> getPendingStubs();
    void* generateReport(const // DateTime &from = // DateTime(), 
                               const // DateTime &to = // DateTime());
    bool exportToJson(const std::string &path);
    bool importFromJson(const std::string &path);
    
    // Statistics
    struct Stats {
        int totalFilesScanned;
        int totalStubsFound;
        int totalStubsFixed;
        int totalErrors;
        int64_t totalTimeMs;
        float avgTimePerFileMs;
        std::map<std::string, int> stubsByLanguage;
    };
    Stats getStatistics() const;

\npublic:\n    void pipelineStarted(int totalFiles);
    void fileScanStarted(const std::string &path);
    void fileScanCompleted(const std::string &path, int stubsFound, int stubsFixed);
    void stubDetected(const StubInstance &stub);
    void stubFixed(const StubInstance &stub);
    void progressUpdate(int filesProcessed, int totalFiles, int stubsFound, int stubsFixed);
    void errorOccurred(const std::string &file, const std::string &error);
    void pipelineCompleted(const Stats &stats);
    void checkpointSaved(int filesProcessed);
    void aiFixRequested(const std::string &context, std::string *outFix);
    void aiFixGenerated(const std::string &fix, float confidence);

\nprivate:\n    void processNextChunk();
    void onFileScanned(const std::string &path, const std::vector<StubInstance> &stubs);
    void onStubFixApplied(int stubId, bool success);

private:
    void setupTables();
    void scanFileInternal(const std::string &filePath, int maxTasksPerFile, bool applyExtensions, bool useAVX512);
    std::vector<StubInstance> detectStubsAVX512(const std::string &content, const std::string &language, 
                                          const std::string &filePath);
    std::vector<StubInstance> detectStubsFallback(const std::string &content, const std::string &language, 
                                            const std::string &filePath);
    std::string generateAIFix(const StubInstance &stub);
    bool applyFix(const StubInstance &stub, const std::string &fix);
    void saveCheckpoint();
    void loadCheckpoint();
    void updateTaskStatus(int taskId, const std::string &status);
    
    // Database — SQLite3 C API (no Qt)
    sqlite3* m_db = nullptr;
    mutable std::mutex m_dbMutex;
    
    // State
    struct SystemState {
        std::atomic<int> running{0};
        std::atomic<int> paused{0};
        std::atomic<int> stopRequested{0};
        std::atomic<int> filesProcessed{0};
        std::atomic<int> totalFiles{0};
        std::atomic<int> stubsFound{0};
        std::atomic<int> stubsFixed{0};
        std::string currentRootDir;
        std::chrono::steady_clock::time_point timer;
    } m_state;
    
    // AI System pointers
    CompletionEngine *m_completionEngine = nullptr;
    CodebaseContextAnalyzer *m_contextAnalyzer = nullptr;
    SmartRewriteEngine *m_rewriteEngine = nullptr;
    MultiModalModelRouter *m_modelRouter = nullptr;
    AdvancedCodingAgent *m_codingAgent = nullptr;
    
    // Threading
    unsigned int m_threadPoolSize = 0;  // std::thread::hardware_concurrency()
    std::queue<std::string> m_pendingFiles;
    std::mutex m_queueMutex;
    int m_chunkSize = 50;
    int m_checkpointInterval = 100; // Save every 100 files
    
    // Language profiles
    struct LangProfile {
        std::string name;
        std::vector<std::string> extensions;
        std::vector<std::vector<uint8_t>> stubSignatures; // For AVX-512
        std::regex stubRegex;
    };
    std::vector<LangProfile> m_profiles;
    void initProfiles();
};

