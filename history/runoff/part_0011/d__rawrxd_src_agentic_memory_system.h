#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>

// Define MemoryType enum
enum class MemoryType {
    Episode,
    Fact,
    Procedure,
    Concept,
    CodeSnippet,
    UserPreference,
    SystemConstraint
};

class AgenticMemorySystem {
public:
    struct MemoryEntry {
        std::string id;
        MemoryType type;
        std::string content;
        std::string metadata; // Changed from const void* to string for serialization safety
        std::chrono::system_clock::time_point timestamp;
        float relevanceScore;
        int accessCount;
        bool isPinned;
    };

    explicit AgenticMemorySystem();
    ~AgenticMemorySystem();

    // Storage
    std::string storeMemory(MemoryType type, const std::string& content, const std::string& metadata = "");
    void updateMemory(const std::string& memoryId, const std::string& content);
    void deleteMemory(const std::string& memoryId);
    void pinMemory(const std::string& memoryId, bool pinned);

    // Retrieval
    MemoryEntry* getMemory(const std::string& memoryId);
    std::vector<MemoryEntry*> getMemoriesByType(MemoryType type);
    std::vector<MemoryEntry*> getMemoriesByContentSearch(const std::string& query);
    
    // Management
    size_t getMemoryCount() const;
    void clearAll();

    // Stats
    size_t getTotalRetrieved() const { return m_totalRetrieved; }

private:
    std::string generateUUID();
    
    // Thread safety
    mutable std::mutex m_mutex;
    
    // Data storage
    std::map<std::string, std::unique_ptr<MemoryEntry>> m_memories;
    std::chrono::system_clock::time_point m_systemStartTime;
    
    // Metrics
    std::atomic<size_t> m_totalStored{0};
    std::atomic<size_t> m_totalRetrieved{0};
};
