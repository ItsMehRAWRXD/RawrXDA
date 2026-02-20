#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>

namespace RawrXD::Agentic::Coordination {

// Types of conflicts that can occur
enum class ConflictType : uint8_t {
    FILE_WRITE_CONFLICT = 0,        // Two agents trying to write same file
    RESOURCE_CONTENTION = 1,        // Competing for GPU/memory/disk
    DEPENDENCY_CONFLICT = 2,        // Circular or broken dependencies
    STATE_CONFLICT = 3,             // Incompatible state transitions
    EXECUTION_ORDER = 4,            // Race condition in execution
    MODEL_CONTENTION = 5            // Multiple agents using same model context
};

// Conflict resolution outcome
enum class ResolutionOutcome : uint8_t {
    WINNER_PROCEEDS = 0,
    BOTH_ROLLBACK = 1,
    SERIALIZATION = 2,
    AUTOMATIC_MERGE = 3,
    HUMAN_REVIEW = 4,
    CANCEL_LOWER_PRIORITY = 5
};

// File diff for automatic merge attempt
struct FileDiff {
    std::string filepath;
    std::vector<std::string> conflictingLines;
    uint32_t lineStart = 0;
    uint32_t lineEnd = 0;
    uint32_t agentAVersion = 0;
    uint32_t agentBVersion = 0;
};

// Detailed conflict analysis
struct ConflictAnalysis {
    uint64_t conflictId = 0;
    ConflictType conflictType;
    uint32_t agentA = 0;
    uint32_t agentB = 0;
    std::string resourceInConflict;
    std::vector<FileDiff> fileDiffs;
    float severityScore = 0.0f;  // 0.0 (minor) to 1.0 (critical)
    bool isMergeable = false;    // Can automatic merge succeed?
    std::string recommendedResolution;
};

class ConflictResolver {
public:
    static ConflictResolver& instance();

    // Conflict analysis
    ConflictAnalysis analyzeConflict(uint64_t conflictId);

    // Resolution strategies
    bool resolveByPriority(uint64_t conflictId, uint32_t& winner);
    bool resolveByRollback(uint64_t conflictId, const std::vector<uint64_t>& taskIds);
    bool resolveBySerializing(uint64_t conflictId, std::vector<uint32_t>& executionOrder);
    bool resolveByMerge(uint64_t conflictId, std::string& mergedContent);
    bool resolveByDeferring(uint64_t conflictId, uint32_t deferredAgent, uint32_t millisToWait);

    // Merge-related helpers
    bool attemptThreeWayMerge(const std::string& baseVersion, const std::string& agentAVersion,
                             const std::string& agentBVersion, std::string& mergedResult);

    // Conflict prevention (proactive)
    bool preventConflictByLocking(uint32_t agentId, const std::string& resourcePath,
                                 uint32_t lockTimeoutSeconds);
    bool releaseResourceLock(uint32_t agentId, const std::string& resourcePath);

    // Statistics
    struct ConflictStats {
        uint64_t totalConflictsDetected = 0;
        uint64_t resolvedByPriority = 0;
        uint64_t resolvedByMerge = 0;
        uint64_t resolvedBySerializing = 0;
        uint64_t resolvedByRollback = 0;
        uint64_t escalatedToHuman = 0;
        float averageSeverity = 0.0f;
        float mergeSuccessRate = 0.0f;
    };
    ConflictStats getStatistics() const;

private:
    ConflictResolver();
    ~ConflictResolver();
    ConflictResolver(const ConflictResolver&) = delete;
    ConflictResolver& operator=(const ConflictResolver&) = delete;

    mutable std::mutex resolverMutex_;

    // Resource locks to prevent future conflicts
    struct ResourceLock {
        std::string resourcePath;
        uint32_t ownerAgentId = 0;
        std::chrono::steady_clock::time_point acquiredAt;
        std::chrono::steady_clock::time_point expiresAt;
        bool isActive = false;
    };

    std::map<std::string, ResourceLock> resourceLocks_;
    std::map<uint64_t, ConflictAnalysis> analysisCache_;

    uint64_t resolvedConflictCount_ = 0;
    float totalConflictSeverity_ = 0.0f;
};

}  // namespace RawrXD::Agentic::Coordination
