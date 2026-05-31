#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD::Memory {

struct SMBCommitResult {
    uint64_t epochId = 0;
    uint64_t winnerBranchId = 0;
    std::vector<uint64_t> inheritedPages;
    std::vector<uint64_t> deltaPages;
    size_t estimatedBytesSaved = 0;
};

class SpeculativeMemoryBranching {
public:
    uint64_t beginEpoch(const std::vector<uint64_t>& baseKvPages);
    uint64_t forkBranch(uint64_t epochId, std::optional<uint64_t> parentBranchId = std::nullopt);

    // Mark a page write for this branch (copy-on-write semantics).
    void touchPage(uint64_t epochId, uint64_t branchId, uint64_t pageId, size_t pageBytes);

    // Winner branch commits; loser branches are retired logically in O(1).
    std::optional<SMBCommitResult> commitWinner(uint64_t epochId, uint64_t winnerBranchId);

    bool retireEpoch(uint64_t epochId);

private:
    struct BranchState {
        uint64_t id = 0;
        std::optional<uint64_t> parent;
        std::unordered_set<uint64_t> touchedPages;
        size_t touchedBytes = 0;
        bool active = true;
    };

    struct EpochState {
        uint64_t id = 0;
        std::vector<uint64_t> basePages;
        std::unordered_map<uint64_t, BranchState> branches;
        bool committed = false;
    };

    std::mutex m_mutex;
    uint64_t m_nextEpochId = 1;
    uint64_t m_nextBranchId = 1;
    std::unordered_map<uint64_t, EpochState> m_epochs;
};

} // namespace RawrXD::Memory
