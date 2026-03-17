#pragma once

/**
 * AgenticMemorySystem — C++20, no Qt. Persistent memory for agents with
 * learning, recall, and pattern recognition.
 *
 * Memory types: Episodic, Semantic, Procedural, Working.
 * Features: retention/forgetting, pattern recognition, similarity retrieval,
 * temporal decay, experience scoring, strategy effectiveness.
 */

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <deque>
#include <chrono>
#include <functional>

class AgenticMemorySystem
{
public:
    enum class MemoryType {
        Episodic,
        Semantic,
        Procedural,
        Working
    };

    struct MemoryEntry {
        std::string id;
        MemoryType type = MemoryType::Episodic;
        std::string content;
        std::string metadata;   // JSON
        std::chrono::system_clock::time_point timestamp{};
        float relevanceScore = 0.f;
        int accessCount = 0;
        std::string tags;
        bool isPinned = false;
    };

    struct Experience {
        std::string experienceId;
        std::string taskDescription;
        std::string goalState;    // JSON
        std::string resultState;  // JSON
        bool successful = false;
        float successRate = 0.f;
        std::vector<std::string> strategiesUsed;
        int iterationsNeeded = 0;
        float similarityToCurrentTask = 0.f;
        int usageCount = 0;
        std::chrono::system_clock::time_point timestamp{};
    };

    struct Pattern {
        std::string patternId;
        std::string description;
        std::string examples;     // JSON array
        float confidence = 0.f;
        std::string category;
        int occurrences = 0;
        std::chrono::system_clock::time_point firstObserved{};
        std::chrono::system_clock::time_point lastObserved{};
    };

    AgenticMemorySystem();
    ~AgenticMemorySystem();

    // Callbacks (replace Qt signals)
    using MemoryStoredFn = std::function<void(const std::string& memoryId)>;
    using MemoryRetrievedFn = std::function<void(const std::string& memoryId)>;
    using ExperienceRecordedFn = std::function<void(const std::string& experienceId)>;
    using PatternDetectedFn = std::function<void(const std::string& patternId)>;
    using MemoriesConsolidatedFn = std::function<void()>;
    using MemoryPrunedFn = std::function<void(int prunedCount)>;

    void setOnMemoryStored(MemoryStoredFn f) { m_onMemoryStored = std::move(f); }
    void setOnMemoryRetrieved(MemoryRetrievedFn f) { m_onMemoryRetrieved = std::move(f); }
    void setOnExperienceRecorded(ExperienceRecordedFn f) { m_onExperienceRecorded = std::move(f); }
    void setOnPatternDetected(PatternDetectedFn f) { m_onPatternDetected = std::move(f); }
    void setOnMemoriesConsolidated(MemoriesConsolidatedFn f) { m_onMemoriesConsolidated = std::move(f); }
    void setOnMemoryPruned(MemoryPrunedFn f) { m_onMemoryPruned = std::move(f); }

    // ===== MEMORY STORAGE =====
    std::string storeMemory(
        MemoryType type,
        const std::string& content,
        const std::string& metadataJson = "{}"
    );
    void updateMemory(const std::string& memoryId, const std::string& content);
    void deleteMemory(const std::string& memoryId);
    void pinMemory(const std::string& memoryId, bool pinned);

    // ===== MEMORY RETRIEVAL =====
    MemoryEntry* getMemory(const std::string& memoryId);
    std::vector<MemoryEntry*> getMemoriesByType(MemoryType type);
    std::vector<MemoryEntry*> getMemoriesByTag(const std::string& tag);
    std::vector<MemoryEntry*> searchMemories(const std::string& query, int limit = 10);
    std::vector<MemoryEntry*> getRelevantMemories(const std::string& context, int limit = 5);

    // ===== EXPERIENCE MANAGEMENT =====
    std::string recordExperience(
        const std::string& taskDescription,
        const std::string& goalStateJson,
        const std::string& resultStateJson,
        bool successful,
        const std::vector<std::string>& strategiesUsed
    );
    std::vector<Experience> findSimilarExperiences(
        const std::string& currentTask,
        float minSimilarity = 0.6f
    );
    Experience* getExperience(const std::string& experienceId);
    void recordExperienceUsage(const std::string& experienceId);
    std::vector<Experience> getMostSuccessfulExperiences(int limit = 10);

    // ===== PATTERN RECOGNITION =====
    std::string recordPattern(
        const std::string& description,
        const std::string& category,
        const std::string& examplesJson
    );
    std::vector<Pattern> detectPatterns(const std::string& context);
    std::vector<Pattern> getPatternsForCategory(const std::string& category);
    Pattern* getPattern(const std::string& patternId);
    float getPatternConfidence(const std::string& patternId);

    // ===== LEARNING AND ADAPTATION =====
    void recordSuccess(
        const std::string& taskDescription,
        const std::string& strategy,
        float effectiveness
    );
    void recordFailure(
        const std::string& taskDescription,
        const std::string& strategy,
        const std::string& failureReason
    );

    float getStrategyEffectiveness(const std::string& strategy) const;
    std::vector<std::string> getRankedStrategies(const std::string& taskType);

    // ===== MEMORY CONSOLIDATION =====
    void consolidateMemories();
    void pruneOldMemories(int ageInDays = 30);
    void forgetMemory(const std::string& memoryId);
    float getMemoryDecayFactor(const MemoryEntry& entry) const;

    // ===== STATISTICAL ANALYSIS =====
    std::string getMemoryStatistics() const;  // JSON
    int getTotalMemories() const { return static_cast<int>(m_memories.size()); }
    int getExperienceCount() const { return static_cast<int>(m_experiences.size()); }
    int getPatternCount() const { return static_cast<int>(m_patterns.size()); }
    float getAverageSuccessRate() const;
    std::string getMostCommonSuccessStrategy() const;

    // ===== EXPORT/IMPORT =====
    std::string exportMemories() const;
    bool importMemories(const std::string& jsonData);
    std::string exportExperiences() const;
    bool importExperiences(const std::string& jsonData);

    // ===== CONFIGURATION =====
    void setRetentionPolicy(int maxMemories);
    void setDecayRate(float rate);
    void setAccessWeightFactor(float factor);

private:
    float calculateSimilarity(const std::string& str1, const std::string& str2);
    void updateMemoryDecay();
    float calculateRelevanceScore(const MemoryEntry& entry, const std::string& context);
    void extractPatterns();
    std::string hashContent(const std::string& content);

    std::unordered_map<std::string, std::unique_ptr<MemoryEntry>> m_memories;
    std::unordered_map<std::string, std::unique_ptr<Experience>> m_experiences;
    std::unordered_map<std::string, std::unique_ptr<Pattern>> m_patterns;
    std::deque<std::string> m_accessHistory;

    int m_maxMemories = 1000;
    float m_decayRate = 0.99f;
    float m_accessWeightFactor = 1.5f;
    int m_contextWindowSize = 10;

    std::chrono::system_clock::time_point m_systemStartTime;
    int m_totalStored = 0;
    int m_totalRetrieved = 0;

    MemoryStoredFn m_onMemoryStored;
    MemoryRetrievedFn m_onMemoryRetrieved;
    ExperienceRecordedFn m_onExperienceRecorded;
    PatternDetectedFn m_onPatternDetected;
    MemoriesConsolidatedFn m_onMemoriesConsolidated;
    MemoryPrunedFn m_onMemoryPruned;
};
