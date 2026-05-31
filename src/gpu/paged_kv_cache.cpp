// ============================================================================
// paged_kv_cache.cpp — vLLM-Style Paged KV Cache with Block Reuse
// ============================================================================
#include "paged_kv_cache.h"
#include <algorithm>
#include <cassert>
#include <numeric>

namespace rawrxd {

// ============================================================================
// Construction / destruction
// ============================================================================

PagedKVCacheManager::PagedKVCacheManager(const PagedKVConfig& cfg)
    : m_cfg(cfg)
{
    m_blocks.resize(cfg.numBlocks);
    m_freeList.resize(cfg.numBlocks);
    std::iota(m_freeList.begin(), m_freeList.end(), 0u);

    // KV storage layout per block:
    //   numLayers * 2 (K+V) * blockSizeTokens * numKVHeads * headDim
    m_blockStorageFloats = static_cast<size_t>(cfg.numLayers) * 2
                         * cfg.blockSizeTokens * cfg.numKVHeads * cfg.headDim;
    m_layerStrideFloats  = static_cast<size_t>(2) * cfg.blockSizeTokens
                         * cfg.numKVHeads * cfg.headDim;

    m_kvStorage.resize(static_cast<size_t>(cfg.numBlocks) * m_blockStorageFloats, 0.0f);
}

PagedKVCacheManager::~PagedKVCacheManager() = default;

// ============================================================================
// Block allocation primitives
// ============================================================================

uint32_t PagedKVCacheManager::allocBlock(uint64_t seqId)
{
    // Caller holds m_mu
    if (m_freeList.empty())
        return UINT32_MAX;

    uint32_t idx = m_freeList.back();
    m_freeList.pop_back();

    m_blocks[idx].refCount     = 1;
    m_blocks[idx].ownerSeqId   = seqId;
    m_blocks[idx].lastAccessTick = ++m_tick;

    // Zero the block's KV storage
    float* base = m_kvStorage.data() + static_cast<size_t>(idx) * m_blockStorageFloats;
    std::memset(base, 0, m_blockStorageFloats * sizeof(float));

    return idx;
}

void PagedKVCacheManager::releaseBlock(uint32_t idx)
{
    // Caller holds m_mu
    if (idx >= m_cfg.numBlocks)
        return;
    auto& blk = m_blocks[idx];
    if (blk.refCount == 0)
        return;
    --blk.refCount;
    if (blk.refCount == 0) {
        blk.ownerSeqId = 0;
        m_freeList.push_back(idx);
    }
}

// ============================================================================
// Prefix hash — FNV-1a over int32 tokens
// ============================================================================

uint64_t PagedKVCacheManager::hashTokens(const int32_t* tokens, size_t count)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(tokens);
    size_t bytes = count * sizeof(int32_t);
    for (size_t i = 0; i < bytes; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 0x100000001b3ULL;
    }
    return h;
}

// ============================================================================
// allocateSequence — fresh allocation (no prefix sharing)
// ============================================================================

SequenceKV* PagedKVCacheManager::allocateSequence(
    uint64_t seqId,
    const std::vector<int32_t>& promptTokens)
{
    std::lock_guard<std::mutex> lk(m_mu);

    uint32_t tokensNeeded = static_cast<uint32_t>(promptTokens.size());
    uint32_t blocksNeeded = (tokensNeeded + m_cfg.blockSizeTokens - 1) / m_cfg.blockSizeTokens;

    if (blocksNeeded > static_cast<uint32_t>(m_freeList.size()))
        return nullptr; // OOM

    SequenceKV seq;
    seq.seqId       = seqId;
    seq.tokenCount  = tokensNeeded;
    seq.prefixHash  = hashTokens(promptTokens.data(), promptTokens.size());
    seq.prefixBlocks = blocksNeeded;
    seq.blockTable.reserve(blocksNeeded);

    for (uint32_t i = 0; i < blocksNeeded; ++i) {
        uint32_t blk = allocBlock(seqId);
        if (blk == UINT32_MAX) {
            // Shouldn't happen — we checked above. Roll back.
            for (auto b : seq.blockTable)
                releaseBlock(b);
            return nullptr;
        }
        seq.blockTable.push_back(blk);
    }

    // Register in prefix index for future sharing
    m_prefixIndex[seq.prefixHash] = seqId;

    auto [it, ok] = m_sequences.emplace(seqId, std::move(seq));
    return ok ? &it->second : nullptr;
}

// ============================================================================
// allocateWithPrefixReuse — share KV blocks for matching prefix
// ============================================================================

SequenceKV* PagedKVCacheManager::allocateWithPrefixReuse(
    uint64_t seqId,
    const std::vector<int32_t>& promptTokens)
{
    std::lock_guard<std::mutex> lk(m_mu);

    uint64_t hash = hashTokens(promptTokens.data(), promptTokens.size());
    auto pit = m_prefixIndex.find(hash);

    if (pit == m_prefixIndex.end()) {
        ++m_prefixMisses;
        // Fall through to fresh allocation (unlock + re-lock avoided,
        // just inline the logic)
        uint32_t tokensNeeded = static_cast<uint32_t>(promptTokens.size());
        uint32_t blocksNeeded = (tokensNeeded + m_cfg.blockSizeTokens - 1)
                              / m_cfg.blockSizeTokens;
        if (blocksNeeded > static_cast<uint32_t>(m_freeList.size()))
            return nullptr;

        SequenceKV seq;
        seq.seqId        = seqId;
        seq.tokenCount   = tokensNeeded;
        seq.prefixHash   = hash;
        seq.prefixBlocks = blocksNeeded;
        seq.blockTable.reserve(blocksNeeded);

        for (uint32_t i = 0; i < blocksNeeded; ++i) {
            uint32_t blk = allocBlock(seqId);
            if (blk == UINT32_MAX) {
                for (auto b : seq.blockTable)
                    releaseBlock(b);
                return nullptr;
            }
            seq.blockTable.push_back(blk);
        }
        m_prefixIndex[hash] = seqId;
        auto [it, ok] = m_sequences.emplace(seqId, std::move(seq));
        return ok ? &it->second : nullptr;
    }

    // Found a matching prefix — share those blocks
    ++m_prefixHits;
    auto donorIt = m_sequences.find(pit->second);
    if (donorIt == m_sequences.end()) {
        // Stale entry — treat as miss
        m_prefixIndex.erase(pit);
        ++m_prefixMisses;
        // Recurse via fresh path (non-recursive in practice: just inline)
        uint32_t tokensNeeded = static_cast<uint32_t>(promptTokens.size());
        uint32_t blocksNeeded = (tokensNeeded + m_cfg.blockSizeTokens - 1)
                              / m_cfg.blockSizeTokens;
        if (blocksNeeded > static_cast<uint32_t>(m_freeList.size()))
            return nullptr;

        SequenceKV seq;
        seq.seqId        = seqId;
        seq.tokenCount   = tokensNeeded;
        seq.prefixHash   = hash;
        seq.prefixBlocks = blocksNeeded;
        seq.blockTable.reserve(blocksNeeded);

        for (uint32_t i = 0; i < blocksNeeded; ++i) {
            uint32_t blk = allocBlock(seqId);
            if (blk == UINT32_MAX) {
                for (auto b : seq.blockTable)
                    releaseBlock(b);
                return nullptr;
            }
            seq.blockTable.push_back(blk);
        }
        m_prefixIndex[hash] = seqId;
        auto [it, ok] = m_sequences.emplace(seqId, std::move(seq));
        return ok ? &it->second : nullptr;
    }

    const SequenceKV& donor = donorIt->second;

    SequenceKV seq;
    seq.seqId        = seqId;
    seq.tokenCount   = static_cast<uint32_t>(promptTokens.size());
    seq.prefixHash   = hash;
    seq.prefixBlocks = donor.prefixBlocks;
    seq.blockTable.reserve(donor.blockTable.size());

    // Share prefix blocks (increment refCount)
    for (uint32_t i = 0; i < donor.prefixBlocks && i < donor.blockTable.size(); ++i) {
        uint32_t blkIdx = donor.blockTable[i];
        m_blocks[blkIdx].refCount++;
        m_blocks[blkIdx].lastAccessTick = ++m_tick;
        seq.blockTable.push_back(blkIdx);
    }

    // Allocate any additional blocks beyond the shared prefix
    uint32_t totalBlocks = (seq.tokenCount + m_cfg.blockSizeTokens - 1)
                         / m_cfg.blockSizeTokens;
    for (uint32_t i = static_cast<uint32_t>(seq.blockTable.size()); i < totalBlocks; ++i) {
        uint32_t blk = allocBlock(seqId);
        if (blk == UINT32_MAX) {
            // Release what we allocated (but not shared blocks — just decref)
            for (uint32_t j = 0; j < seq.blockTable.size(); ++j) {
                if (j < donor.prefixBlocks)
                    m_blocks[seq.blockTable[j]].refCount--;
                else
                    releaseBlock(seq.blockTable[j]);
            }
            return nullptr;
        }
        seq.blockTable.push_back(blk);
    }

    auto [it, ok] = m_sequences.emplace(seqId, std::move(seq));
    return ok ? &it->second : nullptr;
}

// ============================================================================
// appendToken — extend a sequence by one token
// ============================================================================

bool PagedKVCacheManager::appendToken(SequenceKV* seq, int32_t /*token*/)
{
    if (!seq)
        return false;

    std::lock_guard<std::mutex> lk(m_mu);

    uint32_t posInBlock = seq->tokenCount % m_cfg.blockSizeTokens;

    // If current block is full, allocate a new one
    if (posInBlock == 0 && seq->tokenCount > 0) {
        uint32_t blk = allocBlock(seq->seqId);
        if (blk == UINT32_MAX)
            return false; // OOM
        seq->blockTable.push_back(blk);
    }

    seq->tokenCount++;
    return true;
}

// ============================================================================
// freeSequence
// ============================================================================

void PagedKVCacheManager::freeSequence(uint64_t seqId)
{
    std::lock_guard<std::mutex> lk(m_mu);

    auto it = m_sequences.find(seqId);
    if (it == m_sequences.end())
        return;

    // Remove from prefix index if this was the registered donor
    auto pit = m_prefixIndex.find(it->second.prefixHash);
    if (pit != m_prefixIndex.end() && pit->second == seqId)
        m_prefixIndex.erase(pit);

    // Release all blocks (shared blocks just get refCount--)
    for (auto blkIdx : it->second.blockTable)
        releaseBlock(blkIdx);

    m_sequences.erase(it);
}

// ============================================================================
// getBlockKV — raw pointer into KV storage
// ============================================================================

float* PagedKVCacheManager::getBlockKV(uint32_t blockIdx, uint32_t layer, bool isKey) const
{
    if (blockIdx >= m_cfg.numBlocks || layer >= m_cfg.numLayers)
        return nullptr;

    size_t offset = static_cast<size_t>(blockIdx) * m_blockStorageFloats
                  + static_cast<size_t>(layer) * m_layerStrideFloats
                  + (isKey ? 0u : static_cast<size_t>(m_cfg.blockSizeTokens)
                                * m_cfg.numKVHeads * m_cfg.headDim);

    return const_cast<float*>(m_kvStorage.data() + offset);
}

// ============================================================================
// getStats
// ============================================================================

PagedCacheStats PagedKVCacheManager::getStats() const
{
    std::lock_guard<std::mutex> lk(m_mu);

    PagedCacheStats s;
    s.totalBlocks      = m_cfg.numBlocks;
    s.freeBlocks       = static_cast<uint32_t>(m_freeList.size());
    s.activeSequences  = static_cast<uint32_t>(m_sequences.size());
    s.prefixHits       = m_prefixHits;
    s.prefixMisses     = m_prefixMisses;

    s.sharedBlocks = 0;
    for (auto& b : m_blocks) {
        if (b.refCount > 1)
            ++s.sharedBlocks;
    }

    s.utilizationPercent = (s.totalBlocks > 0)
        ? 100.0 * static_cast<double>(s.totalBlocks - s.freeBlocks)
                / static_cast<double>(s.totalBlocks)
        : 0.0;
    return s;
}

// ============================================================================
// evictLRU — free least-recently-used sequences to reclaim blocks
// ============================================================================

uint32_t PagedKVCacheManager::evictLRU(uint32_t blocksNeeded)
{
    std::lock_guard<std::mutex> lk(m_mu);

    if (static_cast<uint32_t>(m_freeList.size()) >= blocksNeeded)
        return 0; // already have enough

    // Build a sorted list of sequences by oldest access
    struct SeqAge {
        uint64_t seqId;
        uint64_t oldestTick;
        uint32_t blockCount;
    };
    std::vector<SeqAge> ages;
    ages.reserve(m_sequences.size());
    for (auto& [sid, seq] : m_sequences) {
        uint64_t oldest = UINT64_MAX;
        for (auto bIdx : seq.blockTable) {
            if (m_blocks[bIdx].lastAccessTick < oldest)
                oldest = m_blocks[bIdx].lastAccessTick;
        }
        ages.push_back({sid, oldest, static_cast<uint32_t>(seq.blockTable.size())});
    }
    std::sort(ages.begin(), ages.end(), [](const SeqAge& a, const SeqAge& b) {
        return a.oldestTick < b.oldestTick;
    });

    uint32_t freed = 0;
    for (auto& age : ages) {
        if (static_cast<uint32_t>(m_freeList.size()) >= blocksNeeded)
            break;

        // Remove from prefix index
        auto sit = m_sequences.find(age.seqId);
        if (sit == m_sequences.end())
            continue;

        auto pit = m_prefixIndex.find(sit->second.prefixHash);
        if (pit != m_prefixIndex.end() && pit->second == age.seqId)
            m_prefixIndex.erase(pit);

        for (auto blkIdx : sit->second.blockTable)
            releaseBlock(blkIdx);
        freed += age.blockCount;
        m_sequences.erase(sit);
    }
    return freed;
}

} // namespace rawrxd
