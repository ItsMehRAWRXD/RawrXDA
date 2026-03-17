#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>

/**
 * @class AgenticMemorySystem
 * @brief Persistent memory for agents with learning, recall, and pattern recognition
 * 
 * Memory types:
 * 1. EPISODIC: Memories of specific interactions and outcomes
 * 2. SEMANTIC: General knowledge and patterns learned
 * 3. PROCEDURAL: Remembered processes and strategies that worked
 * 4. WORKING: Current task context and short-term goals
 * 
 * Features:
 * - Automatic retention and forgetting policies
 * - Pattern recognition and generalization
 * - Similarity-based retrieval
 * - Temporal decay of memories
 * - Experience scoring
 * - Strategy effectiveness tracking
 */
class AgenticMemorySystem : public QObject
{
    Q_OBJECT

public:
    enum class MemoryType {
        Episodic,   // Specific events and interactions
        Semantic,   // General knowledge and patterns
        Procedural, // Processes and strategies
        Working     // Current context
    };

    struct MemoryEntry {
        QString id;
        MemoryType type;
        QString content;
        QJsonObject metadata;
        QDateTime timestamp;
        float relevanceScore;
        int accessCount;
        QString tags;
        bool isPinned;
    };

    struct Experience {
        QString experienceId;
        QString taskDescription;
        QJsonObject goalState;
        QJsonObject resultState;
        bool successful;
        float successRate;
        QStringList strategiesUsed;
        int iterationsNeeded;
        float similarityToCurrentTask;
        int usageCount;
        QDateTime timestamp;
    };

    struct Pattern {
        QString patternId;
        QString description;
        QJsonArray examples;
        float confidence;
        QString category;
        int occurrences;
        QDateTime firstObserved;
        QDateTime lastObserved;
    };

public:
    explicit AgenticMemorySystem(QObject* parent = nullptr);
    ~AgenticMemorySystem();

    // ===== MEMORY STORAGE =====
    QString storeMemory(
        MemoryType type,
        const QString& content,
        const QJsonObject& metadata = QJsonObject()
    );
    void updateMemory(const QString& memoryId, const QString& content);
    void deleteMemory(const QString& memoryId);
    void pinMemory(const QString& memoryId, bool pinned);
    
    // ===== MEMORY RETRIEVAL =====
    MemoryEntry* getMemory(const QString& memoryId);
    std::vector<MemoryEntry*> getMemoriesByType(MemoryType type);
    std::vector<MemoryEntry*> getMemoriesByTag(const QString& tag);
    std::vector<MemoryEntry*> searchMemories(const QString& query, int limit = 10);
    std::vector<MemoryEntry*> getRelevantMemories(const QString& context, int limit = 5);
    
    // ===== EXPERIENCE MANAGEMENT =====
    QString recordExperience(
        const QString& taskDescription,
        const QJsonObject& goalState,
        const QJsonObject& resultState,
        bool successful,
        const QStringList& strategiesUsed
    );
    std::vector<Experience> findSimilarExperiences(
        const QString& currentTask,
        float minSimilarity = 0.6f
    );
    Experience* getExperience(const QString& experienceId);
    void recordExperienceUsage(const QString& experienceId);
    std::vector<Experience> getMostSuccessfulExperiences(int limit = 10);

    // ===== PATTERN RECOGNITION =====
    QString recordPattern(
        const QString& description,
        const QString& category,
        const QJsonArray& examples
    );
    std::vector<Pattern> detectPatterns(const QString& context);
    std::vector<Pattern> getPatternsForCategory(const QString& category);
    Pattern* getPattern(const QString& patternId);
    float getPatternConfidence(const QString& patternId);
    
    // ===== LEARNING AND ADAPTATION =====
    void recordSuccess(
        const QString& taskDescription,
        const QString& strategy,
        float effectiveness
    );
    void recordFailure(
        const QString& taskDescription,
        const QString& strategy,
        const QString& failureReason
    );
    
    // Get strategy effectiveness
    float getStrategyEffectiveness(const QString& strategy) const;
    QStringList getRankedStrategies(const QString& taskType);
    
    // ===== MEMORY CONSOLIDATION =====
    void consolidateMemories();
    void pruneOldMemories(int ageInDays = 30);
    void forgetMemory(const QString& memoryId);
    float getMemoryDecayFactor(const MemoryEntry& entry) const;
    
    // ===== STATISTICAL ANALYSIS =====
    QJsonObject getMemoryStatistics() const;
    int getTotalMemories() const { return m_memories.size(); }
    int getExperienceCount() const { return m_experiences.size(); }
    int getPatternCount() const { return m_patterns.size(); }
    float getAverageSuccessRate() const;
    QString getMostCommonSuccessStrategy() const;
    
    // ===== EXPORT/IMPORT =====
    QString exportMemories() const;
    bool importMemories(const QString& jsonData);
    QString exportExperiences() const;
    bool importExperiences(const QString& jsonData);
    
    // ===== CONFIGURATION =====
    void setRetentionPolicy(int maxMemories);
    void setDecayRate(float rate);
    void setAccessWeightFactor(float factor);

signals:
    void memoryStored(const QString& memoryId);
    void memoryRetrieved(const QString& memoryId);
    void experienceRecorded(const QString& experienceId);
    void patternDetected(const QString& patternId);
    void memoriesConsolidated();
    void memoryPruned(int prunedCount);

private:
    // Calculate similarity between strings
    float calculateSimilarity(const QString& str1, const QString& str2);
    
    // Decay calculation
    void updateMemoryDecay();
    float calculateRelevanceScore(
        const MemoryEntry& entry,
        const QString& context
    );
    
    // Pattern extraction
    void extractPatterns();
    QString hashContent(const QString& content);

    // Memory stores
    std::unordered_map<std::string, std::unique_ptr<MemoryEntry>> m_memories;
    std::unordered_map<std::string, std::unique_ptr<Experience>> m_experiences;
    std::unordered_map<std::string, std::unique_ptr<Pattern>> m_patterns;
    std::deque<QString> m_accessHistory;
    
    // Configuration
    int m_maxMemories = 1000;
    float m_decayRate = 0.99f;
    float m_accessWeightFactor = 1.5f;
    int m_contextWindowSize = 10;
    
    // Metrics
    QDateTime m_systemStartTime;
    int m_totalStored = 0;
    int m_totalRetrieved = 0;
};
