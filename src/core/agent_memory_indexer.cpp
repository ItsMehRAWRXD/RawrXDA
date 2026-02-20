// ============================================================================
// agent_memory_indexer.cpp — Agent Memory Indexer (Semantic Search Index)
// ============================================================================
// Provides an in-memory vector index of agent conversation history,
// tool call results, correction events, and failure patterns.
// Used by AutoRepairOrchestrator and AgenticPuppeteer to recall
// previously successful corrections and avoid repeated failures.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>

namespace RawrXD {

// ============================================================================
// Memory Entry — a single indexed memory item
// ============================================================================
struct MemoryEntry {
    uint64_t    id          = 0;
    uint64_t    timestamp   = 0;      // Unix epoch ms
    const char* category    = "";     // "correction", "failure", "tool_result", "conversation"
    std::string content;              // Raw text content
    std::string metadata;             // JSON metadata blob
    std::vector<float> embedding;     // Semantic embedding vector (if computed)
    float       relevanceScore = 0.0f;
    bool        pinned      = false;  // Pinned entries are never evicted
};

// ============================================================================
// Agent Memory Indexer
// ============================================================================
class AgentMemoryIndexer {
public:
    AgentMemoryIndexer() = default;
    ~AgentMemoryIndexer() = default;

    // No copy (contains mutex)
    AgentMemoryIndexer(const AgentMemoryIndexer&) = delete;
    AgentMemoryIndexer& operator=(const AgentMemoryIndexer&) = delete;

    // ===== Core Operations =====

    // Add a memory entry to the index
    uint64_t addEntry(const char* category, const std::string& content,
                      const std::string& metadata = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        MemoryEntry entry;
        entry.id        = m_nextId++;
        entry.timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        entry.category  = category;
        entry.content   = content;
        entry.metadata  = metadata;
        m_entries.push_back(std::move(entry));
        m_totalAdded.fetch_add(1, std::memory_order_relaxed);
        return entry.id;
    }

    // Search for entries by keyword (simple substring match)
    std::vector<const MemoryEntry*> search(const std::string& query,
                                            uint32_t maxResults = 10) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<const MemoryEntry*> results;
        for (const auto& entry : m_entries) {
            if (entry.content.find(query) != std::string::npos) {
                results.push_back(&entry);
                if (results.size() >= maxResults) break;
            }
        }
        return results;
    }

    // Search by category
    std::vector<const MemoryEntry*> searchByCategory(const char* category,
                                                      uint32_t maxResults = 10) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<const MemoryEntry*> results;
        for (const auto& entry : m_entries) {
            if (std::strcmp(entry.category, category) == 0) {
                results.push_back(&entry);
                if (results.size() >= maxResults) break;
            }
        }
        return results;
    }

    // Get entry by ID
    const MemoryEntry* getEntry(uint64_t id) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& entry : m_entries) {
            if (entry.id == id) return &entry;
        }
        return nullptr;
    }

    // Remove entry by ID
    bool removeEntry(uint64_t id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
            if (it->id == id) {
                m_entries.erase(it);
                return true;
            }
        }
        return false;
    }

    // Clear all non-pinned entries
    void clearUnpinned() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                           [](const MemoryEntry& e) { return !e.pinned; }),
            m_entries.end()
        );
    }

    // Clear all entries
    void clearAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
    }

    // Pin/unpin an entry (pinned entries survive eviction)
    bool pinEntry(uint64_t id, bool pin) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& entry : m_entries) {
            if (entry.id == id) {
                entry.pinned = pin;
                return true;
            }
        }
        return false;
    }

    // ===== Stats =====
    uint64_t getEntryCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_entries.size();
    }

    uint64_t getTotalAdded() const {
        return m_totalAdded.load(std::memory_order_relaxed);
    }

    // ===== Eviction (LRU by timestamp) =====
    void evictOldest(uint32_t count) {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Sort by timestamp ascending, remove the oldest non-pinned entries
        std::sort(m_entries.begin(), m_entries.end(),
                  [](const MemoryEntry& a, const MemoryEntry& b) {
                      return a.timestamp < b.timestamp;
                  });
        uint32_t removed = 0;
        auto it = m_entries.begin();
        while (it != m_entries.end() && removed < count) {
            if (!it->pinned) {
                it = m_entries.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
    }

private:
    std::vector<MemoryEntry>    m_entries;
    uint64_t                    m_nextId = 1;
    std::atomic<uint64_t>       m_totalAdded{0};
    mutable std::mutex          m_mutex;
};

// ============================================================================
// Global accessor (lazy-initialized singleton)
// ============================================================================
AgentMemoryIndexer& getAgentMemoryIndexer() {
    static AgentMemoryIndexer instance;
    return instance;
}

} // namespace RawrXD
