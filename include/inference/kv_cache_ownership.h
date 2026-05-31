// ============================================================================
// kv_cache_ownership.h — KV Cache Ownership Tracker with Strict Ownership Model
// ============================================================================
// Enforces ownership constraints before mutating state
// Block-based KV cache with owner/sharer distinction
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace RawrXD::Inference {

// ============================================================================
// Types
// ============================================================================

using BlockId = uint32_t;
using SeqId = uint64_t;

enum class OwnershipEvent {
    Allocated,
    Shared,
    Released,
    Transferred,
    Evicted,
    Pinned,
    Unpinned
};

// ============================================================================
// Block Metadata with Owner vs Sharer Distinction
// ============================================================================

struct BlockMetadata {
    SeqId owner;                              // Single owner
    std::unordered_set<SeqId> sharers;        // Shared readers (0..N)
    bool pinned = false;
    bool allocated = false;

    // Computed refCount = 1 (owner) + sharers.size()
    size_t refCount() const {
        return allocated ? (1 + sharers.size()) : 0;
    }

    bool isOwnedBy(SeqId seq) const {
        return allocated && (owner == seq || sharers.count(seq) > 0);
    }

    bool isShared() const {
        return allocated && !sharers.empty();
    }

    void reset() {
        owner = 0;
        sharers.clear();
        pinned = false;
        allocated = false;
    }
};

// ============================================================================
// Statistics
// ============================================================================

struct OwnershipStats {
    uint32_t totalBlocks = 0;
    uint32_t allocatedBlocks = 0;
    uint32_t freeBlocks = 0;
    uint32_t pinnedBlocks = 0;
    uint32_t sharedBlocks = 0;
};

// ============================================================================
// KV Cache Ownership Tracker
// ============================================================================

class KVCacheOwnershipTracker {
public:
    explicit KVCacheOwnershipTracker(uint32_t totalBlocks);
    ~KVCacheOwnershipTracker() = default;

    // Delete copy/move to maintain consistency
    KVCacheOwnershipTracker(const KVCacheOwnershipTracker&) = delete;
    KVCacheOwnershipTracker& operator=(const KVCacheOwnershipTracker&) = delete;

    // -----------------------------------------------------------------------
    // Allocation
    // -----------------------------------------------------------------------
    bool allocateBlock(BlockId id, SeqId seq);
    std::vector<BlockId> allocateBlocks(uint32_t count, SeqId seq);

    // -----------------------------------------------------------------------
    // Sharing
    // -----------------------------------------------------------------------
    bool shareBlock(BlockId id, SeqId seq);

    // -----------------------------------------------------------------------
    // Release (with ownership validation)
    // -----------------------------------------------------------------------
    bool releaseBlock(BlockId id, SeqId seq);
    void releaseAllForSequence(SeqId seq);

    // -----------------------------------------------------------------------
    // Transfer (sole ownership required)
    // -----------------------------------------------------------------------
    bool transferBlock(BlockId id, SeqId from, SeqId to);
    void transferAllForSequence(SeqId from, SeqId to);

    // -----------------------------------------------------------------------
    // Eviction (blocked if pinned or shared)
    // -----------------------------------------------------------------------
    bool evictBlock(BlockId id);

    // -----------------------------------------------------------------------
    // Pinning
    // -----------------------------------------------------------------------
    bool pinBlock(BlockId id);
    bool unpinBlock(BlockId id);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------
    bool isOwnedBy(BlockId id, SeqId seq) const;
    bool isShared(BlockId id) const;
    bool isPinned(BlockId id) const;
    size_t getRefCount(BlockId id) const;
    SeqId getOwner(BlockId id) const;
    std::vector<BlockId> getBlocksForSequence(SeqId seq) const;
    std::vector<SeqId> getSequencesForBlock(BlockId id) const;

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    OwnershipStats getStats() const;

    // -----------------------------------------------------------------------
    // Callbacks
    // -----------------------------------------------------------------------
    using EventCallback = std::function<void(OwnershipEvent event, BlockId block, SeqId seq)>;
    void setCallback(EventCallback cb);
    void clearCallback();

private:
    mutable std::mutex m_mutex;
    std::vector<BlockMetadata> m_blocks;
    std::unordered_map<SeqId, std::unordered_set<BlockId>> m_seqToBlocks;
    EventCallback m_callback;

    void fireEvent(OwnershipEvent event, BlockId block, SeqId seq);
    bool isValidBlock(BlockId id) const;
    void freeBlock(BlockId id);
};

} // namespace RawrXD::Inference