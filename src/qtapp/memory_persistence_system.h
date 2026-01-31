#pragma once
#include <memory>

/**
 * @class MemoryPersistenceSystem
 * @brief Provides context persistence and intelligent memory management
 * 
 * Features:
 * - Automatic context snapshots and restoration
 * - Session persistence across IDE restarts
 * - Intelligent memory optimization
 * - Knowledge graph of code relationships
 * - Context-aware suggestions based on history
 * - Memory leak detection and cleanup
 */
class MemoryPersistenceSystem  {

public:
    explicit MemoryPersistenceSystem( = nullptr);
    virtual ~MemoryPersistenceSystem();

    // Memory statistics structure
    struct MemoryStats {
        int64_t totalSize;
        int activeSnapshots;
        int totalSessions;
        double compressionRatio;
    };

    // Context management
    void saveContextSnapshot(const std::string& sessionId, const void*& context);
    void* loadContextSnapshot(const std::string& sessionId);
    void* listSnapshots();
    void deleteSnapshot(const std::string& sessionId);
    
    // Session persistence
    void saveSessionState(const std::string& sessionName, const void*& state);
    void* loadSessionState(const std::string& sessionName);
    void saveCurrentSession();
    void restoreLastSession();
    
    // Knowledge graph
    void addCodeRelationship(const std::string& filePath, const std::string& symbol, const void*& metadata);
    void* findRelatedCode(const std::string& symbol);
    void* buildKnowledgeGraph(const std::string& projectPath);
    
    // Memory optimization
    void optimizeMemoryUsage();
    void* getMemoryUsageStats();
    void cleanupExpiredData();
    
    // Context suggestions
    void* suggestContextBasedOnHistory(const std::string& currentContext);
    void* suggestRelevantFiles(const std::string& currentFile);
    std::string suggestNextAction(const void*& currentState);
\npublic:\n    void onSessionStarted(const std::string& sessionId);
    void onSessionEnded(const std::string& sessionId);
    void onCodeFileOpened(const std::string& filePath);
    void onCodeFileModified(const std::string& filePath);
    void onBuildCompleted(const std::string& buildId, bool success);
    void enableAutoSnapshot(bool enable);
    void setSnapshotInterval(int minutes);
\npublic:\n    void snapshotSaved(const std::string& sessionId);
    void sessionRestored(const std::string& sessionName);
    void memoryOptimized(const void*& stats);
    void suggestionGenerated(const void*& suggestions);
    void memoryAlert(const std::string& level, const std::string& message);

private:
    // Internal helpers
    std::string generateSnapshotId();
    std::string getStoragePath();
    void* serializeContext(const void*& context);
    void* deserializeContext(const void*& data);
    void* createCurrentContext();
    void applyContext(const void*& context);
    void saveKnowledgeGraph();
    std::string findFileForSymbol(const std::string& symbol);
    MemoryStats calculateMemoryStats();
    void* serializeMemoryStats(const MemoryStats& stats);
    
    // Data structures
    struct SessionData {
        std::string sessionId;
        std::string timestamp;
        void* context;
        std::string projectPath;
        std::stringList openFiles;
        void* buildHistory;
        void* settings;
    };
    
    std::map<std::string, std::shared_ptr<SessionData>> m_activeSessions;
    std::map<std::string, std::shared_ptr<SessionData>> m_persistentSessions;
    void* m_knowledgeGraph;
    // Timer m_optimizationTimer;
    // Timer m_snapshotTimer;
    
    // Configuration
    bool m_autoSnapshot = true;
    int m_snapshotIntervalMinutes = 30;
    int m_maxSnapshots = 50;
    int64_t m_maxStorageMB = 1024; // 1GB limit
    
    // Storage paths
    std::string m_baseStoragePath;
    std::string m_snapshotsPath;
    std::string m_sessionsPath;
    std::string m_knowledgePath;
};

