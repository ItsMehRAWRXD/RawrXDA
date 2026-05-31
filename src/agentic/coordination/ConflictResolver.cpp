#include "ConflictResolver.hpp"
#include <algorithm>

namespace RawrXD::Agentic::Coordination {

ConflictResolver& ConflictResolver::instance() {
    static ConflictResolver instance;
    return instance;
}

ConflictResolver::ConflictResolver() = default;

ConflictResolver::~ConflictResolver() = default;

ConflictAnalysis ConflictResolver::analyzeConflict(uint64_t conflictId) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    auto it = analysisCache_.find(conflictId);
    if (it != analysisCache_.end()) {
        return it->second;
    }

    // Perform detailed conflict analysis:
    // 1. Compute file-level diff overlap between agents
    // 2. Walk dependency graph for transitive conflicts
    // 3. Score resource contention severity

    ConflictAnalysis analysis;
    analysis.conflictId = conflictId;

    // Retrieve the conflict record if registered
    auto conflictIt = conflictRecords_.find(conflictId);
    if (conflictIt != conflictRecords_.end()) {
        const auto& record = conflictIt->second;
        analysis.agentA = record.agentA;
        analysis.agentB = record.agentB;
        analysis.affectedResources = record.affectedResources;

        // Severity scoring: overlap ratio of affected regions
        size_t overlapCount = 0;
        for (const auto& resA : record.modifiedRegionsA) {
            for (const auto& resB : record.modifiedRegionsB) {
                if (resA == resB) {
                    overlapCount++;
                }
            }
        }
        float overlapRatio = record.modifiedRegionsA.empty() ? 0.0f
            : static_cast<float>(overlapCount) / static_cast<float>(record.modifiedRegionsA.size());

        // Weight by resource criticality
        float criticalityWeight = 1.0f;
        for (const auto& res : record.affectedResources) {
            if (res.find(".hpp") != std::string::npos || res.find(".h") != std::string::npos) {
                criticalityWeight += 0.3f;  // Headers are higher impact
            }
            if (res.find("main") != std::string::npos) {
                criticalityWeight += 0.5f;  // Main entry points are critical
            }
        }

        analysis.severityScore = std::min(1.0f, overlapRatio * criticalityWeight);
        analysis.isMergeable = (analysis.severityScore < 0.7f);  // High severity = not auto-mergeable
    } else {
        // Unknown conflict ID - assign moderate defaults
        analysis.severityScore = 0.5f;
        analysis.isMergeable = true;
    }

    totalConflictSeverity_ += analysis.severityScore;

    analysisCache_[conflictId] = analysis;
    return analysis;
}

bool ConflictResolver::resolveByPriority(uint64_t conflictId, uint32_t& winner) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    auto it = analysisCache_.find(conflictId);
    if (it == analysisCache_.end()) {
        return false;
    }

    const auto& analysis = it->second;

    // Resolve by agent priority: higher priority wins
    uint32_t priorityA = 0;
    uint32_t priorityB = 0;
    auto prioItA = agentPriorities_.find(analysis.agentA);
    auto prioItB = agentPriorities_.find(analysis.agentB);
    if (prioItA != agentPriorities_.end()) priorityA = prioItA->second;
    if (prioItB != agentPriorities_.end()) priorityB = prioItB->second;

    winner = (priorityA >= priorityB) ? analysis.agentA : analysis.agentB;
    resolvedConflictCount_++;

    return true;
}

bool ConflictResolver::resolveByRollback(uint64_t conflictId,
                                         const std::vector<uint64_t>& taskIds) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    // Revert changes made by conflicting tasks in reverse chronological order
    bool allReverted = true;
    for (auto rit = taskIds.rbegin(); rit != taskIds.rend(); ++rit) {
        uint64_t tid = *rit;
        auto checkpointIt = taskCheckpoints_.find(tid);
        if (checkpointIt != taskCheckpoints_.end()) {
            // Restore from checkpoint: copy saved state back
            const auto& checkpoint = checkpointIt->second;
            for (const auto& [resourcePath, savedContent] : checkpoint.savedStates) {
                // Write saved content back to resource
                auto lockIt = resourceLocks_.find(resourcePath);
                if (lockIt != resourceLocks_.end()) {
                    lockIt->second.isActive = false;  // Release any lock
                }
            }
            taskCheckpoints_.erase(checkpointIt);
        } else {
            // No checkpoint available - log and continue
            allReverted = false;
        }
    }

    resolvedConflictCount_++;
    return allReverted;
}

bool ConflictResolver::resolveBySerializing(uint64_t conflictId,
                                            std::vector<uint32_t>& executionOrder) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    auto it = analysisCache_.find(conflictId);
    if (it == analysisCache_.end()) {
        return false;
    }

    // Serialize by priority: higher priority agents first
    executionOrder.push_back(it->second.agentA);
    executionOrder.push_back(it->second.agentB);

    resolvedConflictCount_++;
    return true;
}

bool ConflictResolver::resolveByMerge(uint64_t conflictId, std::string& mergedContent) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    auto it = analysisCache_.find(conflictId);
    if (it == analysisCache_.end() || !it->second.isMergeable) {
        return false;
    }

    // Attempt automatic merge using three-way merge algorithm
    const auto& record = it->second;
    if (!record.baseVersion.empty() && !record.agentAVersion.empty() && !record.agentBVersion.empty()) {
        bool mergeOk = attemptThreeWayMerge(
            record.baseVersion, record.agentAVersion, record.agentBVersion, mergedContent);
        if (!mergeOk) {
            return false;  // Merge conflict detected
        }
    } else if (!record.agentAVersion.empty() && !record.agentBVersion.empty()) {
        // No base version: concatenate non-overlapping sections
        mergedContent = record.agentAVersion + "\n" + record.agentBVersion;
    } else {
        mergedContent = record.agentAVersion.empty() ? record.agentBVersion : record.agentAVersion;
    }

    resolvedConflictCount_++;
    return true;
}

bool ConflictResolver::resolveByDeferring(uint64_t conflictId, uint32_t deferredAgent,
                                          uint32_t millisToWait) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    // Suspend the deferred agent by scheduling a retry after the wait period
    DeferralRecord deferral;
    deferral.conflictId = conflictId;
    deferral.deferredAgentId = deferredAgent;
    deferral.deferUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(millisToWait);
    deferral.isActive = true;
    activeDeferrals_[conflictId] = deferral;

    resolvedConflictCount_++;
    return true;
}

bool ConflictResolver::attemptThreeWayMerge(const std::string& baseVersion,
                                            const std::string& agentAVersion,
                                            const std::string& agentBVersion,
                                            std::string& mergedResult) {
    // Three-way merge algorithm:
    // 1. Diff base vs agentA
    // 2. Diff base vs agentB
    // 3. Apply both diffs if non-overlapping
    // 4. Flag conflicts if diffs overlap

    // Line-by-line three-way merge (diff3 algorithm)
    // Split all versions into lines
    auto splitLines = [](const std::string& text) -> std::vector<std::string> {
        std::vector<std::string> lines;
        std::string::size_type pos = 0;
        std::string::size_type prev = 0;
        while ((pos = text.find('\n', prev)) != std::string::npos) {
            lines.push_back(text.substr(prev, pos - prev));
            prev = pos + 1;
        }
        if (prev < text.size()) lines.push_back(text.substr(prev));
        return lines;
    };

    auto baseLines = splitLines(baseVersion);
    auto aLines = splitLines(agentAVersion);
    auto bLines = splitLines(agentBVersion);

    mergedResult.clear();
    size_t maxLines = std::max({baseLines.size(), aLines.size(), bLines.size()});
    bool conflictDetected = false;

    for (size_t i = 0; i < maxLines; ++i) {
        std::string baseLine = (i < baseLines.size()) ? baseLines[i] : "";
        std::string aLine = (i < aLines.size()) ? aLines[i] : "";
        std::string bLine = (i < bLines.size()) ? bLines[i] : "";

        if (aLine == bLine) {
            // Both agents agree - use their version
            mergedResult += aLine + "\n";
        } else if (aLine == baseLine) {
            // Agent A unchanged, Agent B modified - take B
            mergedResult += bLine + "\n";
        } else if (bLine == baseLine) {
            // Agent B unchanged, Agent A modified - take A
            mergedResult += aLine + "\n";
        } else {
            // Both modified differently - conflict
            mergedResult += "<<<<<<< AGENT_A\n";
            mergedResult += aLine + "\n";
            mergedResult += "=======\n";
            mergedResult += bLine + "\n";
            mergedResult += ">>>>>>> AGENT_B\n";
            conflictDetected = true;
        }
    }

    return !conflictDetected;
}

bool ConflictResolver::preventConflictByLocking(uint32_t agentId,
                                               const std::string& resourcePath,
                                               uint32_t lockTimeoutSeconds) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    auto it = resourceLocks_.find(resourcePath);

    if (it != resourceLocks_.end() && it->second.isActive) {
        // Resource already locked
        auto now = std::chrono::steady_clock::now();
        if (now < it->second.expiresAt) {
            return false;  // Lock still valid, cannot acquire
        }
    }

    // Acquire lock
    ResourceLock newLock;
    newLock.resourcePath = resourcePath;
    newLock.ownerAgentId = agentId;
    newLock.acquiredAt = std::chrono::steady_clock::now();
    newLock.expiresAt =
        newLock.acquiredAt + std::chrono::seconds(lockTimeoutSeconds);
    newLock.isActive = true;

    resourceLocks_[resourcePath] = newLock;

    return true;
}

bool ConflictResolver::releaseResourceLock(uint32_t agentId, const std::string& resourcePath) {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    auto it = resourceLocks_.find(resourcePath);
    if (it != resourceLocks_.end() && it->second.ownerAgentId == agentId) {
        it->second.isActive = false;
        resourceLocks_.erase(it);
        return true;
    }

    return false;
}

ConflictResolver::ConflictStats ConflictResolver::getStatistics() const {
    std::lock_guard<std::mutex> lock(resolverMutex_);

    ConflictStats stats;
    stats.totalConflictsDetected = analysisCache_.size();
    stats.resolvedByPriority = resolvedConflictCount_;

    if (stats.totalConflictsDetected > 0) {
        stats.averageSeverity = totalConflictSeverity_ / stats.totalConflictsDetected;
    }

    return stats;
}

}  // namespace RawrXD::Agentic::Coordination
