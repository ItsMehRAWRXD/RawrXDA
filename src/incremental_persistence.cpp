/**
 * IncrementalPersistence Implementation
 * Enhancement #2: Delta State Persistence
 */

#include "incremental_persistence.h"
#include <algorithm>
#include <set>
#include <mutex>

namespace IncrementalPersistence {

    // ===== ChangeTracker Implementation =====

    ChangeTracker::ChangeTracker() = default;
    ChangeTracker::~ChangeTracker() = default;

    void ChangeTracker::markDirty(uint16_t flags) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirtyFlags |= flags;
    }

    void ChangeTracker::markClean(uint16_t flags) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirtyFlags &= ~flags;
    }

    bool ChangeTracker::isDirty(uint16_t flags) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return (m_dirtyFlags & flags) != 0;
    }

    uint16_t ChangeTracker::getDirtyFlags() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dirtyFlags;
    }

    void ChangeTracker::markFieldDirty(const std::string& fieldPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirtyFields.insert(fieldPath);
    }

    bool ChangeTracker::isFieldDirty(const std::string& fieldPath) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_dirtyFields.count(fieldPath) > 0;
    }

    void ChangeTracker::clearFieldDirty(const std::string& fieldPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirtyFields.erase(fieldPath);
    }

    std::vector<std::string> ChangeTracker::getDirtyFields() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::vector<std::string>(m_dirtyFields.begin(), m_dirtyFields.end());
    }

    nlohmann::json ChangeTracker::computeDelta(
        const nlohmann::json& oldState,
        const nlohmann::json& newState) {
        
        nlohmann::json delta = nlohmann::json::object();
        
        // Find added/modified fields
        for (auto it = newState.begin(); it != newState.end(); ++it) {
            const std::string& key = it.key();
            
            if (!oldState.contains(key)) {
                // Added
                delta["+"][key] = it.value();
            } else if (oldState[key] != it.value()) {
                // Modified
                delta["~"][key] = it.value();
            }
        }
        
        // Find removed fields
        for (auto it = oldState.begin(); it != oldState.end(); ++it) {
            const std::string& key = it.key();
            if (!newState.contains(key)) {
                delta["-"].push_back(key);
            }
        }
        
        return delta;
    }

    bool ChangeTracker::applyDelta(nlohmann::json& baseState, const nlohmann::json& delta) {
        try {
            // Apply additions
            if (delta.contains("+")) {
                for (auto it = delta["+"].begin(); it != delta["+"].end(); ++it) {
                    baseState[it.key()] = it.value();
                }
            }
            
            // Apply modifications
            if (delta.contains("~")) {
                for (auto it = delta["~"].begin(); it != delta["~"].end(); ++it) {
                    baseState[it.key()] = it.value();
                }
            }
            
            // Apply deletions
            if (delta.contains("-")) {
                for (const auto& key : delta["-"]) {
                    baseState.erase(key.get<std::string>());
                }
            }
            
            return true;
        } catch (...) {
            return false;
        }
    }

    void ChangeTracker::reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dirtyFlags = IP_DIRTY_NONE;
        m_dirtyFields.clear();
    }

    // ===== IncrementalSession Implementation =====

    IncrementalSession::IncrementalSession(const std::string& executionId)
        : m_executionId(executionId) {
    }

    IncrementalSession::~IncrementalSession() = default;

    void IncrementalSession::beginUpdate() {
        m_stagedChanges = nlohmann::json::object();
    }

    void IncrementalSession::stageLoopState(const nlohmann::json& loopState) {
        m_stagedChanges["loopState"] = loopState;
        m_tracker.markDirty(IP_DIRTY_LOOP);
    }

    void IncrementalSession::stageMemorySystem(const nlohmann::json& memorySystem) {
        m_stagedChanges["memorySystem"] = memorySystem;
        m_tracker.markDirty(IP_DIRTY_MEMORY);
    }

    void IncrementalSession::stageContext(const nlohmann::json& context) {
        m_stagedChanges["context"] = context;
        m_tracker.markDirty(IP_DIRTY_CONTEXT);
    }

    nlohmann::json IncrementalSession::commit() {
        nlohmann::json delta = nlohmann::json::object();
        delta["executionId"] = m_executionId;
        delta["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        delta["changes"] = m_stagedChanges;
        delta["dirtyFlags"] = m_tracker.getDirtyFlags();
        
        // Accumulate
        if (m_accumulatedDelta.empty()) {
            m_accumulatedDelta = delta;
        } else {
            // Merge changes
            for (auto it = m_stagedChanges.begin(); it != m_stagedChanges.end(); ++it) {
                m_accumulatedDelta["changes"][it.key()] = it.value();
            }
        }
        
        m_deltaCount++;
        
        // Check if we need full persistence
        if (m_deltaCount >= MAX_DELTA_BEFORE_FULL) {
            delta["forceFull"] = true;
            m_deltaCount = 0;
            m_accumulatedDelta = nlohmann::json::object();
        }
        
        m_tracker.reset();
        m_stagedChanges = nlohmann::json::object();
        
        return delta;
    }

    nlohmann::json IncrementalSession::getAccumulatedDelta() const {
        return m_accumulatedDelta;
    }

    void IncrementalSession::forceFullPersistence() {
        m_deltaCount = MAX_DELTA_BEFORE_FULL;
    }

    // ===== Global Stats =====

    namespace {
        Stats g_stats;
        std::mutex g_statsMutex;
    }

    Stats getGlobalStats() {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        return g_stats;
    }

    void resetGlobalStats() {
        std::lock_guard<std::mutex> lock(g_statsMutex);
        g_stats = Stats{};
    }

} // namespace IncrementalPersistence
