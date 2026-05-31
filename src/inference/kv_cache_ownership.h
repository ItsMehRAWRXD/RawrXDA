// ============================================================================
// kv_cache_ownership.h — KV Cache Ownership Tracking & Reference Counting
// ============================================================================
// Tracks which sequence owns which KV cache blocks. Enables:
// - Safe block reuse across sequences
// - Reference-counted prefix sharing
// - Ownership transfer for speculative decoding rollback
// - Memory pressure eviction with owner notification
//
// Thread-safe: all operations guarded by internal mutex.
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <functional>

namespace RawrXD::Inference {

// ---------------------------------------------------------------------------
// Ownership Record — per-block metadata
// ---------------------------------------------------------------------------
struct KVBlockOwnership {
    uint32_t    blockIdx;           // index into block pool
    uint64_t    ownerSeqId;         // sequence that allocated this block
    uint32_t    refCount;           // reference count for sharing
    uint32_t    generation;         // monotonic generation for LRU
    bool        isPinned;           // pinned blocks cannot be evicted
    bool        isDirty;            // modified since last sync
};

// ---------------------------------------------------------------------------
// Ownership Change Event
// ---------------------------------------------------------------------------
enum class OwnershipEvent {
    Allocated,      // New block allocated to sequence
    Shared,         // Block shared (refCount incremented)
    Released,       // Block released (refCount decremented)
    Transferred,    // Ownership transferred to new sequence
    Evicted,        // Block evicted under memory pressure
    Pinned,         // Block pinned
    Unpinned        // Block unpinned
};

using OwnershipCallback = std::function<void(OwnershipEvent event, uint32_t blockIdx, uint64_t seqId)>;

// ---------------------------------------------------------------------------
// KVCacheOwnershipTracker
// ---------------------------------------------------------------------------
class KVCacheOwnershipTracker {
public:
    explicit KVCacheOwnershipTracker(uint32_t totalBlocks);
    ~KVCacheOwnershipTracker();

    // --- Block lifecycle ---
    bool allocateBlock(uint32_t blockIdx, uint64_t seqId);
    bool shareBlock(uint32_t blockIdx, uint64_t newSeqId);
    bool releaseBlock(uint32_t blockIdx, uint64_t seqId);
    bool transferBlock(uint32_t blockIdx, uint64_t fromSeqId, uint64_t toSeqId);

    // --- Bulk operations ---
    std::vector<uint32_t> allocateBlocks(uint32_t count, uint64_t seqId);
    void releaseBlocks(const std::vector<uint32_t>& blocks, uint64_t seqId);
    void releaseAllForSequence(uint64_t seqId);
    void transferAllForSequence(uint64_t fromSeqId, uint64_t toSeqId);

    // --- Query ---
    bool isOwnedBy(uint32_t blockIdx, uint64_t seqId) const;
    bool isShared(uint32_t blockIdx) const;
    uint32_t getRefCount(uint32_t blockIdx) const;
    uint64_t getOwner(uint32_t blockIdx) const;
    std::vector<uint32_t> getBlocksForSequence(uint64_t seqId) const;
    std::vector<uint64_t> getSequencesForBlock(uint32_t blockIdx) const;

    // --- Pinning ---
    bool pinBlock(uint32_t blockIdx);
    bool unpinBlock(uint32_t blockIdx);
    bool isPinned(uint32_t blockIdx) const;
    std::vector<uint32_t> getPinnedBlocks() const;

    // --- Eviction support ---
    std::vector<uint32_t> getEvictableBlocks(uint32_t maxCount) const;
    bool evictBlock(uint32_t blockIdx);

    // --- Callbacks ---
    void setOwnershipCallback(OwnershipCallback cb);

    // --- Stats ---
    struct Stats {
        uint32_t totalBlocks;
        uint32_t allocatedBlocks;
        uint32_t sharedBlocks;
        uint32_t pinnedBlocks;
        uint32_t freeBlocks;
        uint64_t totalAllocations;
        uint64_t totalReleases;
        uint64_t totalShares;
        uint64_t totalTransfers;
        uint64_t totalEvictions;
    };
    Stats getStats() const;

private:
    mutable std::mutex m_mutex;
    uint32_t m_totalBlocks;
    std::vector<KVBlockOwnership> m_blocks;
    std::unordered_map<uint64_t, std::unordered_set<uint32_t>> m_seqToBlocks;
    std::unordered_map<uint32_t, std::unordered_set<uint64_t>> m_blockToSeqs;
    uint32_t m_generationCounter{0};
    OwnershipCallback m_callback;

    // Stats
    uint64_t m_totalAllocations{0};
    uint64_t m_totalReleases{0};
    uint64_t m_totalShares{0};
    uint64_t m_totalTransfers{0};
    uint64_t m_totalEvictions{0};

    void notify(OwnershipEvent event, uint32_t blockIdx, uint64_t seqId);
};

} // namespace RawrXD::Inference
