// ============================================================================
// kv_cache_prefix_trie.h — Tree-Structured KV Cache Prefix Matching
// ============================================================================
// Builds a radix trie mapping (model, layer, token-sequence hash) → cached
// KV embeddings.  On new request, walks the trie to find the longest
// matching prefix, reuses pre-computed KV from that point.
//
// Key use-case: system prompts.  If 90% of requests share the same system
// prompt (512+ tokens), we skip those layers entirely on subsequent requests.
//
// Architecture:
//   TrieNode       — one node in the radix trie (token prefix → children)
//   KVSnapshot     — a frozen KV cache region (block table reference)
//   PrefixTrie     — the trie itself (insert, match, evict)
//   PrefixTrieStats — hit/miss/depth metrics
//
// Integration:
//   Called from RequestBatchScheduler before dispatching a batch.
//   Each slot's prompt tokens are matched against the trie.
//   If a prefix match is found, the slot's kvOffset is set to the
//   cached position and the batch skips those initial layers.
//
// Thread safety: reader-writer lock (shared for lookup, exclusive for insert).
// Hot-path: match() is O(prefix_length) with no allocation.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// KVSnapshot — a frozen reference to cached KV blocks
// ---------------------------------------------------------------------------
struct KVSnapshot {
    uint64_t snapshotId;
    uint64_t modelHash;         // FNV-1a of model path
    uint32_t numLayers;
    uint32_t numTokensCached;   // how many tokens this snapshot covers
    std::vector<uint32_t> blockTable;  // block indices in paged KV cache

    // Validity tracking
    uint64_t createdAtEpoch;
    std::atomic<uint32_t> refCount{0};
    bool     expired = false;
};

// ---------------------------------------------------------------------------
// TrieNode — radix trie node keyed by token ID sequences
// ---------------------------------------------------------------------------
struct TrieNode {
    // Children keyed by the NEXT token ID in the sequence
    std::unordered_map<int32_t, std::unique_ptr<TrieNode>> children;

    // If non-null, a KV snapshot is cached at this prefix depth
    std::shared_ptr<KVSnapshot> snapshot;

    // Depth in the trie (= number of tokens in prefix)
    uint32_t depth = 0;

    // Access tracking for LRU eviction
    mutable std::atomic<uint64_t> lastAccessEpoch{0};
    mutable std::atomic<uint32_t> accessCount{0};
};

// ---------------------------------------------------------------------------
// PrefixMatch — result of a trie lookup
// ---------------------------------------------------------------------------
struct PrefixMatch {
    bool     found = false;
    uint32_t matchedTokens = 0;      // how many prefix tokens matched
    std::shared_ptr<KVSnapshot> snapshot;  // the cached KV (may be null)
    uint64_t lookupNs = 0;           // timing
};

// ---------------------------------------------------------------------------
// PrefixTrieStats
// ---------------------------------------------------------------------------
struct PrefixTrieStats {
    std::atomic<uint64_t> lookups{0};
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> inserts{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint32_t> nodeCount{0};
    std::atomic<uint32_t> snapshotCount{0};
    uint32_t              maxDepth = 0;

    double hitRate() const {
        uint64_t total = lookups.load(std::memory_order_relaxed);
        if (total == 0) return 0.0;
        return (double)hits.load(std::memory_order_relaxed) / (double)total;
    }
};

// ---------------------------------------------------------------------------
// PrefixTrieConfig
// ---------------------------------------------------------------------------
struct PrefixTrieConfig {
    uint32_t maxNodes       = 16384;    // max trie nodes before eviction
    uint32_t maxSnapshots   = 256;      // max cached KV snapshots
    uint32_t minPrefixLen   = 16;       // don't cache prefixes shorter than this
    uint32_t maxPrefixLen   = 4096;     // don't cache prefixes longer than this
    uint64_t snapshotTTLMs  = 300000;   // 5 minute TTL for snapshots
    bool     enableLRUEvict = true;
};

// ---------------------------------------------------------------------------
// KVCachePrefixTrie — the main trie class
// ---------------------------------------------------------------------------
class KVCachePrefixTrie {
public:
    explicit KVCachePrefixTrie(const PrefixTrieConfig& cfg = {});
    ~KVCachePrefixTrie();

    // ── Lookup ──────────────────────────────────────────────────────────
    // Find the longest matching prefix in the trie.
    // tokens: the full prompt token sequence.
    // modelHash: FNV-1a of the model path (different models = different tries).
    // Returns PrefixMatch with the deepest snapshot found.
    PrefixMatch match(const int32_t* tokens, uint32_t numTokens,
                      uint64_t modelHash) const;

    // Convenience overload
    PrefixMatch match(const std::vector<int32_t>& tokens, uint64_t modelHash) const {
        return match(tokens.data(), (uint32_t)tokens.size(), modelHash);
    }

    // ── Insert ──────────────────────────────────────────────────────────
    // Insert a prefix + KV snapshot into the trie.
    // tokens: the token prefix that was computed.
    // snapshot: the frozen KV cache to associate.
    // Returns false if trie is full and eviction failed.
    bool insert(const int32_t* tokens, uint32_t numTokens,
                uint64_t modelHash,
                std::shared_ptr<KVSnapshot> snapshot);

    bool insert(const std::vector<int32_t>& tokens, uint64_t modelHash,
                std::shared_ptr<KVSnapshot> snapshot) {
        return insert(tokens.data(), (uint32_t)tokens.size(), modelHash,
                      std::move(snapshot));
    }

    // ── Eviction ────────────────────────────────────────────────────────
    // Evict expired snapshots (TTL-based)
    uint32_t evictExpired();

    // Evict least-recently-used nodes until under budget
    uint32_t evictLRU(uint32_t targetNodes);

    // Clear all entries for a specific model
    void clearModel(uint64_t modelHash);

    // Clear everything
    void clear();

    // ── Stats ───────────────────────────────────────────────────────────
    const PrefixTrieStats& stats() const { return m_stats; }

    // ── Hash Utility ────────────────────────────────────────────────────
    static uint64_t fnv1a(const void* data, size_t len);
    static uint64_t hashModelPath(const std::string& path);

private:
    // Get or create the model-specific root node
    TrieNode* getModelRoot(uint64_t modelHash);
    const TrieNode* getModelRoot(uint64_t modelHash) const;

    // Collect all leaf timestamps for LRU eviction
    void collectLeaves(const TrieNode* node, 
                       std::vector<std::pair<uint64_t, TrieNode*>>& leaves) const;

    PrefixTrieConfig                                    m_cfg;
    mutable PrefixTrieStats                             m_stats;
    mutable std::shared_mutex                           m_rwLock;
    std::unordered_map<uint64_t, std::unique_ptr<TrieNode>> m_roots; // per model
    std::atomic<uint64_t>                               m_nextSnapshotId{1};
};

} // namespace rawrxd
