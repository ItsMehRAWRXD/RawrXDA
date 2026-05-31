// ============================================================================
// kv_cache_ownership.cpp — KV Cache Ownership Tracking Implementation
// ============================================================================
// Enforces ownership constraints before mutating state:
//   - releaseBlock: Only sequence that owns/shares can release
//   - transferBlock: Requires sole ownership (refCount == 1)
//   - evictBlock: Blocked if pinned OR shared (refCount > 1)
// ============================================================================

#include "inference/kv_cache_ownership.h"
#include <algorithm>

namespace RawrXD::Inference {

// ============================================================================
// Constructor
// ============================================================================

KVCacheOwnershipTracker::KVCacheOwnershipTracker(uint32_t totalBlocks)
    : m_blocks(totalBlocks)
{
    for (auto& block : m_blocks) {
        block.reset();
    }
}

// ============================================================================
// Allocation
// ============================================================================

bool KVCacheOwnershipTracker::allocateBlock(BlockId id, SeqId seq) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];
    
    // Cannot allocate an already allocated block
    if (block.allocated) {
        return false;
    }

    block.allocated = true;
    block.owner = seq;
    block.pinned = false;
    block.sharers.clear();

    m_seqToBlocks[seq].insert(id);

    fireEvent(OwnershipEvent::Allocated, id, seq);
    return true;
}

std::vector<BlockId> KVCacheOwnershipTracker::allocateBlocks(uint32_t count, SeqId seq) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<BlockId> allocated;
    allocated.reserve(count);

    for (BlockId i = 0; i < m_blocks.size() && allocated.size() < count; ++i) {
        if (!m_blocks[i].allocated) {
            m_blocks[i].allocated = true;
            m_blocks[i].owner = seq;
            m_blocks[i].pinned = false;
            m_blocks[i].sharers.clear();
            m_seqToBlocks[seq].insert(i);
            allocated.push_back(i);
            fireEvent(OwnershipEvent::Allocated, i, seq);
        }
    }

    return allocated;
}

// ============================================================================
// Sharing
// ============================================================================

bool KVCacheOwnershipTracker::shareBlock(BlockId id, SeqId seq) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];
    
    // Cannot share an unallocated block
    if (!block.allocated) {
        return false;
    }

    // Cannot share with owner or existing sharers
    if (block.owner == seq || block.sharers.count(seq) > 0) {
        return false;
    }

    block.sharers.insert(seq);
    m_seqToBlocks[seq].insert(id);

    fireEvent(OwnershipEvent::Shared, id, seq);
    return true;
}

// ============================================================================
// Release (with ownership validation)
// ============================================================================

bool KVCacheOwnershipTracker::releaseBlock(BlockId id, SeqId seq) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];

    // 🔴 CRITICAL: Validate ownership
    // Only a sequence that actually owns/shares the block can release it
    if (!block.isOwnedBy(seq)) {
        return false;
    }

    // Remove from sequence mapping
    auto seqIt = m_seqToBlocks.find(seq);
    if (seqIt != m_seqToBlocks.end()) {
        seqIt->second.erase(id);
        if (seqIt->second.empty()) {
            m_seqToBlocks.erase(seqIt);
        }
    }

    // Update block ownership
    if (block.owner == seq) {
        // Owner is releasing
        if (block.sharers.empty()) {
            // No sharers - free the block
            freeBlock(id);
        } else {
            // Transfer ownership to first sharer
            auto newOwnerIt = block.sharers.begin();
            block.owner = *newOwnerIt;
            block.sharers.erase(newOwnerIt);
        }
    } else {
        // Sharer is releasing
        block.sharers.erase(seq);
    }

    fireEvent(OwnershipEvent::Released, id, seq);
    return true;
}

void KVCacheOwnershipTracker::releaseAllForSequence(SeqId seq) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto seqIt = m_seqToBlocks.find(seq);
    if (seqIt == m_seqToBlocks.end()) {
        return;
    }

    // Copy block IDs since we'll be modifying the set
    std::vector<BlockId> blocksToRelease(seqIt->second.begin(), seqIt->second.end());

    for (BlockId id : blocksToRelease) {
        auto& block = m_blocks[id];

        if (block.owner == seq) {
            if (block.sharers.empty()) {
                freeBlock(id);
            } else {
                auto newOwnerIt = block.sharers.begin();
                block.owner = *newOwnerIt;
                block.sharers.erase(newOwnerIt);
            }
        } else {
            block.sharers.erase(seq);
        }

        fireEvent(OwnershipEvent::Released, id, seq);
    }

    m_seqToBlocks.erase(seqIt);
}

// ============================================================================
// Transfer (sole ownership required)
// ============================================================================

bool KVCacheOwnershipTracker::transferBlock(BlockId id, SeqId from, SeqId to) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];

    // Must be allocated
    if (!block.allocated) {
        return false;
    }

    // 🔴 CRITICAL: Transfer requires sole ownership
    // Block must have exactly one owner (no sharers) - refCount == 1
    if (block.refCount() != 1) {
        return false;
    }

    // Must match the current owner
    if (block.owner != from) {
        return false;
    }

    // Update ownership
    m_seqToBlocks[from].erase(id);
    if (m_seqToBlocks[from].empty()) {
        m_seqToBlocks.erase(from);
    }

    block.owner = to;
    m_seqToBlocks[to].insert(id);

    fireEvent(OwnershipEvent::Transferred, id, to);
    return true;
}

void KVCacheOwnershipTracker::transferAllForSequence(SeqId from, SeqId to) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto fromIt = m_seqToBlocks.find(from);
    if (fromIt == m_seqToBlocks.end()) {
        return;
    }

    // Only transfer blocks where 'from' is the sole owner
    std::vector<BlockId> blocksToTransfer;
    for (BlockId id : fromIt->second) {
        auto& block = m_blocks[id];
        if (block.owner == from && block.sharers.empty()) {
            blocksToTransfer.push_back(id);
        }
    }

    for (BlockId id : blocksToTransfer) {
        auto& block = m_blocks[id];
        block.owner = to;
        m_seqToBlocks[to].insert(id);
        fireEvent(OwnershipEvent::Transferred, id, to);
    }

    // Update 'from' sequence mapping
    fromIt->second.erase(
        std::remove_if(fromIt->second.begin(), fromIt->second.end(),
            [&](BlockId id) { return m_blocks[id].owner != from; }),
        fromIt->second.end()
    );

    if (fromIt->second.empty()) {
        m_seqToBlocks.erase(fromIt);
    }
}

// ============================================================================
// Eviction (blocked if pinned OR shared)
// ============================================================================

bool KVCacheOwnershipTracker::evictBlock(BlockId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];

    // Cannot evict if not allocated
    if (!block.allocated) {
        return false;
    }

    // Cannot evict if pinned
    if (block.pinned) {
        return false;
    }

    // 🔴 CRITICAL: Cannot evict if shared (refCount > 1)
    // Eviction only allowed for single-owner blocks
    if (block.refCount() > 1) {
        return false;
    }

    // Get the owner for the event before freeing
    SeqId owner = block.owner;

    // Remove from sequence mapping
    auto seqIt = m_seqToBlocks.find(owner);
    if (seqIt != m_seqToBlocks.end()) {
        seqIt->second.erase(id);
        if (seqIt->second.empty()) {
            m_seqToBlocks.erase(seqIt);
        }
    }

    freeBlock(id);
    fireEvent(OwnershipEvent::Evicted, id, owner);
    return true;
}

// ============================================================================
// Pinning
// ============================================================================

bool KVCacheOwnershipTracker::pinBlock(BlockId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];
    if (!block.allocated) {
        return false;
    }

    block.pinned = true;
    fireEvent(OwnershipEvent::Pinned, id, block.owner);
    return true;
}

bool KVCacheOwnershipTracker::unpinBlock(BlockId id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    auto& block = m_blocks[id];
    if (!block.allocated || !block.pinned) {
        return false;
    }

    block.pinned = false;
    fireEvent(OwnershipEvent::Unpinned, id, block.owner);
    return true;
}

// ============================================================================
// Queries
// ============================================================================

bool KVCacheOwnershipTracker::isOwnedBy(BlockId id, SeqId seq) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    return m_blocks[id].isOwnedBy(seq);
}

bool KVCacheOwnershipTracker::isShared(BlockId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    return m_blocks[id].isShared();
}

bool KVCacheOwnershipTracker::isPinned(BlockId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return false;
    }

    return m_blocks[id].pinned;
}

size_t KVCacheOwnershipTracker::getRefCount(BlockId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id)) {
        return 0;
    }

    return m_blocks[id].refCount();
}

SeqId KVCacheOwnershipTracker::getOwner(BlockId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id) || !m_blocks[id].allocated) {
        return 0;
    }

    return m_blocks[id].owner;
}

std::vector<BlockId> KVCacheOwnershipTracker::getBlocksForSequence(SeqId seq) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_seqToBlocks.find(seq);
    if (it == m_seqToBlocks.end()) {
        return {};
    }

    return std::vector<BlockId>(it->second.begin(), it->second.end());
}

std::vector<SeqId> KVCacheOwnershipTracker::getSequencesForBlock(BlockId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidBlock(id) || !m_blocks[id].allocated) {
        return {};
    }

    const auto& block = m_blocks[id];
    std::vector<SeqId> seqs;
    seqs.reserve(1 + block.sharers.size());
    seqs.push_back(block.owner);
    seqs.insert(seqs.end(), block.sharers.begin(), block.sharers.end());
    return seqs;
}

// ============================================================================
// Statistics
// ============================================================================

OwnershipStats KVCacheOwnershipTracker::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    OwnershipStats stats;
    stats.totalBlocks = static_cast<uint32_t>(m_blocks.size());

    for (const auto& block : m_blocks) {
        if (block.allocated) {
            stats.allocatedBlocks++;
            if (block.pinned) {
                stats.pinnedBlocks++;
            }
            if (block.isShared()) {
                stats.sharedBlocks++;
            }
        }
    }

    stats.freeBlocks = stats.totalBlocks - stats.allocatedBlocks;
    return stats;
}

// ============================================================================
// Callbacks
// ============================================================================

void KVCacheOwnershipTracker::setCallback(EventCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = std::move(cb);
}

void KVCacheOwnershipTracker::clearCallback() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback = nullptr;
}

// ============================================================================
// Private Helpers
// ============================================================================

void KVCacheOwnershipTracker::fireEvent(OwnershipEvent event, BlockId block, SeqId seq) {
    if (m_callback) {
        m_callback(event, block, seq);
    }
}

bool KVCacheOwnershipTracker::isValidBlock(BlockId id) const {
    return id < m_blocks.size();
}

void KVCacheOwnershipTracker::freeBlock(BlockId id) {
    m_blocks[id].reset();
}

} // namespace RawrXD::Inference
