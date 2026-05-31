#pragma once
/**
 * IncrementalPersistence - Enhancement #2: Delta State Persistence
 * 
 * Tracks state changes and only persists modified fields.
 * Dramatically reduces I/O for large workflows with small changes.
 * 
 * Symbols: IP_DIRTY_LOOP, IP_DIRTY_MEMORY, IP_DIRTY_CONTEXT
 */

#include <string>
#include <unordered_set>
#include <mutex>
#include <chrono>
#include <nlohmann/json.hpp>

// Dirty flags for incremental persistence
#define IP_DIRTY_NONE       0x0000
#define IP_DIRTY_LOOP       0x0001  // AgenticLoopState changed
#define IP_DIRTY_MEMORY     0x0002  // AgenticMemorySystem changed
#define IP_DIRTY_CONTEXT    0x0004  // Global context changed
#define IP_DIRTY_METADATA   0x0008  // Metadata changed
#define IP_DIRTY_CHECKPOINT 0x0010  // New checkpoint added
#define IP_DIRTY_ALL        0xFFFF  // Full persistence required

namespace IncrementalPersistence {

    /**
     * Tracks which parts of state have changed
     */
    class ChangeTracker {
    public:
        ChangeTracker();
        ~ChangeTracker();

        // Mark sections as dirty
        void markDirty(uint16_t flags);
        void markClean(uint16_t flags);
        bool isDirty(uint16_t flags) const;
        uint16_t getDirtyFlags() const;

        // Track specific field changes
        void markFieldDirty(const std::string& fieldPath);
        bool isFieldDirty(const std::string& fieldPath) const;
        void clearFieldDirty(const std::string& fieldPath);
        std::vector<std::string> getDirtyFields() const;

        // Compute delta between old and new state
        nlohmann::json computeDelta(
            const nlohmann::json& oldState,
            const nlohmann::json& newState);

        // Apply delta to base state
        bool applyDelta(nlohmann::json& baseState, const nlohmann::json& delta);

        // Reset all dirty flags
        void reset();

    private:
        uint16_t m_dirtyFlags = IP_DIRTY_NONE;
        std::unordered_set<std::string> m_dirtyFields;
        mutable std::mutex m_mutex;
    };

    /**
     * Incremental persistence session
     */
    class IncrementalSession {
    public:
        explicit IncrementalSession(const std::string& executionId);
        ~IncrementalSession();

        // Begin incremental update
        void beginUpdate();

        // Stage changes for specific components
        void stageLoopState(const nlohmann::json& loopState);
        void stageMemorySystem(const nlohmann::json& memorySystem);
        void stageContext(const nlohmann::json& context);

        // Commit changes - returns delta JSON
        nlohmann::json commit();

        // Get accumulated delta since last full persistence
        nlohmann::json getAccumulatedDelta() const;

        // Force full persistence
        void forceFullPersistence();

        // Get change tracker
        ChangeTracker& getTracker() { return m_tracker; }

    private:
        std::string m_executionId;
        ChangeTracker m_tracker;
        nlohmann::json m_stagedChanges;
        nlohmann::json m_accumulatedDelta;
        int m_deltaCount = 0;
        static constexpr int MAX_DELTA_BEFORE_FULL = 10;
    };

    /**
     * Statistics for incremental persistence
     */
    struct Stats {
        size_t fullPersistences = 0;
        size_t incrementalUpdates = 0;
        size_t bytesSaved = 0;
        float avgDeltaSize = 0.0f;
    };
    Stats getGlobalStats();
    void resetGlobalStats();

} // namespace IncrementalPersistence
