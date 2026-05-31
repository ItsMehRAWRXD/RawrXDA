// ============================================================================
// paged_kv_cache.h — vLLM-Style Paged KV Cache with Block Reuse
// ============================================================================
// Fixed-size blocks (default 16 tokens each).  Free-list allocator.
// Supports cross-request KV reuse for shared prompt prefixes.
//
// Block lifecycle:
//   Free → Allocated(seqId) → Referenced(shared prefix) → Free
//
// Prefix sharing:
//   Two requests with identical first-K tokens share KV blocks for those K
//   tokens via a radix-tree lookup keyed by FNV-1a prefix hash.
//
// Thread safety:
//   Coarse mutex for allocate/free.  Read paths (lookupPrefix) take shared
//   lock when the compiler's std::shared_mutex is available; otherwise
//   exclusive.
// ============================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
struct PagedKVConfig {
    uint32_t blockSizeTokens  = 16;       // tokens per block
    uint32_t numBlocks        = 4096;     // total blocks in the pool
    uint32_t kvDim            = 4096;     // KV head dimension (hidden_size)
    uint32_t numLayers        = 80;       // transformer layers
    uint32_t numKVHeads       = 8;        // GQA heads (may differ from Q heads)
    uint32_t headDim          = 128;      // per-head dimension
};

// ---------------------------------------------------------------------------
// BlockHandle — a lightweight reference to a KV block
// ---------------------------------------------------------------------------
struct BlockHandle {
    uint32_t blockIdx;     // index into the block pool
    uint32_t refCount;     // >1 when shared across sequences
};

// ---------------------------------------------------------------------------
// SequenceKV — per-sequence KV mapping (block table)
// ---------------------------------------------------------------------------
struct SequenceKV {
    uint64_t              seqId;
    std::vector<uint32_t> blockTable;   // ordered list of block indices
    uint32_t              tokenCount;   // total tokens stored
    uint64_t              prefixHash;   // FNV-1a of prompt tokens (for sharing)
    uint32_t              prefixBlocks; // # blocks that are shareable prefix
};

// ---------------------------------------------------------------------------
// CacheStats
// ---------------------------------------------------------------------------
struct PagedCacheStats {
    uint32_t totalBlocks;
    uint32_t freeBlocks;
    uint32_t sharedBlocks;       // blocks with refCount > 1
    uint32_t activeSequences;
    double   utilizationPercent;
    uint64_t prefixHits;         // successful cross-request reuse
    uint64_t prefixMisses;
};

// ---------------------------------------------------------------------------
// PagedKVCacheManager
// ---------------------------------------------------------------------------
class PagedKVCacheManager {
public:
    explicit PagedKVCacheManager(const PagedKVConfig& cfg = {});
    ~PagedKVCacheManager();

    // Allocate blocks for a new sequence.  Returns nullptr on OOM.
    SequenceKV* allocateSequence(uint64_t seqId,
                                 const std::vector<int32_t>& promptTokens);

    // Extend an existing sequence by one generated token.
    // Allocates a new block if the last block is full.
    bool appendToken(SequenceKV* seq, int32_t token);

    // Free all blocks owned by a sequence (decrements refCounts).
    void freeSequence(uint64_t seqId);

    // Prefix sharing: check if any existing sequence shares the same prompt
    // prefix.  If found, the returned SequenceKV will share those blocks
    // (refCount incremented) instead of allocating fresh ones.
    SequenceKV* allocateWithPrefixReuse(uint64_t seqId,
                                        const std::vector<int32_t>& promptTokens);

    // Get raw KV data pointer for a given (sequence, blockIdx, layer).
    // Returns nullptr on invalid arguments.
    float* getBlockKV(uint32_t blockIdx, uint32_t layer, bool isKey) const;

    // Stats snapshot
    PagedCacheStats getStats() const;

    // Evict least-recently-used sequences to free N blocks.
    uint32_t evictLRU(uint32_t blocksNeeded);

private:
    // Block pool
    struct Block {
        uint32_t refCount = 0;
        uint64_t ownerSeqId = 0;         // primary owner (0 = free)
        uint64_t lastAccessTick = 0;
    };

    uint32_t allocBlock(uint64_t seqId);
    void     releaseBlock(uint32_t idx);

    // Prefix hash (FNV-1a over int32 tokens)
    static uint64_t hashTokens(const int32_t* tokens, size_t count);

    PagedKVConfig                                  m_cfg;
    std::vector<Block>                             m_blocks;
    std::vector<uint32_t>                          m_freeList;
    std::unordered_map<uint64_t, SequenceKV>       m_sequences;  // seqId → KV
    std::unordered_map<uint64_t, uint64_t>         m_prefixIndex; // hash → seqId

    // KV storage: flat array [numBlocks * numLayers * 2 * blockSizeTokens * headDim * numKVHeads]
    // Accessed via getBlockKV().
    std::vector<float>                             m_kvStorage;
    size_t                                         m_blockStorageFloats = 0;
    size_t                                         m_layerStrideFloats  = 0;

    uint64_t                                       m_tick = 0;
    uint64_t                                       m_prefixHits = 0;
    uint64_t                                       m_prefixMisses = 0;

    mutable std::mutex                             m_mu;
};

} // namespace rawrxd
