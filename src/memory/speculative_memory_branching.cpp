#include "speculative_memory_branching.h"

namespace RawrXD::Memory {

uint64_t SpeculativeMemoryBranching::beginEpoch(const std::vector<uint64_t>& baseKvPages) {
    std::lock_guard<std::mutex> lock(m_mutex);

    const uint64_t id = m_nextEpochId++;
    EpochState epoch;
    epoch.id = id;
    epoch.basePages = baseKvPages;
    m_epochs.emplace(id, std::move(epoch));
    return id;
}

uint64_t SpeculativeMemoryBranching::forkBranch(uint64_t epochId, std::optional<uint64_t> parentBranchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_epochs.find(epochId);
    if (it == m_epochs.end() || it->second.committed) {
        return 0;
    }

    if (parentBranchId.has_value() && it->second.branches.find(*parentBranchId) == it->second.branches.end()) {
        return 0;
    }

    const uint64_t branchId = m_nextBranchId++;
    BranchState b;
    b.id = branchId;
    b.parent = parentBranchId;
    it->second.branches.emplace(branchId, std::move(b));
    return branchId;
}

void SpeculativeMemoryBranching::touchPage(uint64_t epochId, uint64_t branchId, uint64_t pageId, size_t pageBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto epochIt = m_epochs.find(epochId);
    if (epochIt == m_epochs.end() || epochIt->second.committed) {
        return;
    }

    auto branchIt = epochIt->second.branches.find(branchId);
    if (branchIt == epochIt->second.branches.end() || !branchIt->second.active) {
        return;
    }

    const auto [_, inserted] = branchIt->second.touchedPages.insert(pageId);
    if (inserted) {
        branchIt->second.touchedBytes += pageBytes;
    }
}

std::optional<SMBCommitResult> SpeculativeMemoryBranching::commitWinner(uint64_t epochId, uint64_t winnerBranchId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto epochIt = m_epochs.find(epochId);
    if (epochIt == m_epochs.end() || epochIt->second.committed) {
        return std::nullopt;
    }

    auto winnerIt = epochIt->second.branches.find(winnerBranchId);
    if (winnerIt == epochIt->second.branches.end() || !winnerIt->second.active) {
        return std::nullopt;
    }

    SMBCommitResult out;
    out.epochId = epochId;
    out.winnerBranchId = winnerBranchId;
    out.inheritedPages = epochIt->second.basePages;
    out.deltaPages.reserve(winnerIt->second.touchedPages.size());

    for (uint64_t page : winnerIt->second.touchedPages) {
        out.deltaPages.push_back(page);
    }

    size_t loserBytes = 0;
    for (auto& [id, branch] : epochIt->second.branches) {
        if (id != winnerBranchId && branch.active) {
            loserBytes += branch.touchedBytes;
            branch.active = false;
        }
    }
    out.estimatedBytesSaved = loserBytes;

    winnerIt->second.active = false;
    epochIt->second.committed = true;
    return out;
}

bool SpeculativeMemoryBranching::retireEpoch(uint64_t epochId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_epochs.erase(epochId) > 0;
}

} // namespace RawrXD::Memory
