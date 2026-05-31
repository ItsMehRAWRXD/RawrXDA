// ============================================================================
// kv_cache_prefix_trie.cpp — KV Cache Prefix Trie Implementation
// ============================================================================
#include "kv_cache_prefix_trie.h"
#include "../config/IDEConfig.h"

#include <algorithm>
#include <chrono>

namespace rawrxd {

// ============================================================================
// Construction / Destruction
// ============================================================================

KVCachePrefixTrie::KVCachePrefixTrie(const PrefixTrieConfig& cfg)
    : m_cfg(cfg) {}

KVCachePrefixTrie::~KVCachePrefixTrie() = default;

// ============================================================================
// FNV-1a Hash
// ============================================================================

uint64_t KVCachePrefixTrie::fnv1a(const void* data, size_t len)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint64_t KVCachePrefixTrie::hashModelPath(const std::string& path)
{
    return fnv1a(path.data(), path.size());
}

// ============================================================================
// Model Root Management
// ============================================================================

TrieNode* KVCachePrefixTrie::getModelRoot(uint64_t modelHash)
{
    auto it = m_roots.find(modelHash);
    if (it != m_roots.end()) return it->second.get();
    auto node = std::make_unique<TrieNode>();
    node->depth = 0;
    auto* raw = node.get();
    m_roots[modelHash] = std::move(node);
    m_stats.nodeCount.fetch_add(1, std::memory_order_relaxed);
    return raw;
}

const TrieNode* KVCachePrefixTrie::getModelRoot(uint64_t modelHash) const
{
    auto it = m_roots.find(modelHash);
    return (it != m_roots.end()) ? it->second.get() : nullptr;
}

// ============================================================================
// Lookup — find longest matching prefix
// ============================================================================

PrefixMatch KVCachePrefixTrie::match(const int32_t* tokens, uint32_t numTokens,
                                      uint64_t modelHash) const
{
    auto t0 = std::chrono::steady_clock::now();
    PrefixMatch result;
    m_stats.lookups.fetch_add(1, std::memory_order_relaxed);

    std::shared_lock<std::shared_mutex> lock(m_rwLock);

    const TrieNode* root = getModelRoot(modelHash);
    if (!root || numTokens == 0) {
        m_stats.misses.fetch_add(1, std::memory_order_relaxed);
        METRICS.increment("runtime.kv.lookup_miss");
        METRICS.gauge("runtime.kv.hit_rate", m_stats.hitRate());
        return result;
    }

    const TrieNode* current = root;
    std::shared_ptr<KVSnapshot> bestSnapshot;
    uint32_t bestDepth = 0;

    // Walk the trie token by token
    uint32_t walkLen = std::min(numTokens, m_cfg.maxPrefixLen);
    for (uint32_t i = 0; i < walkLen; ++i) {
        auto it = current->children.find(tokens[i]);
        if (it == current->children.end())
            break;

        current = it->second.get();
        current->lastAccessEpoch.store(
            (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count(),
            std::memory_order_relaxed);
        current->accessCount.fetch_add(1, std::memory_order_relaxed);

        // Track deepest snapshot encountered
        if (current->snapshot && !current->snapshot->expired) {
            bestSnapshot = current->snapshot;
            bestDepth = i + 1;
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    result.lookupNs = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    METRICS.recordDuration("runtime.kv.lookup_ms", static_cast<double>(result.lookupNs) / 1000000.0);

    if (bestSnapshot) {
        result.found = true;
        result.matchedTokens = bestDepth;
        result.snapshot = std::move(bestSnapshot);
        result.snapshot->refCount.fetch_add(1, std::memory_order_relaxed);
        m_stats.hits.fetch_add(1, std::memory_order_relaxed);
        METRICS.increment("runtime.kv.lookup_hit");
        METRICS.gauge("runtime.kv.last_match_tokens", static_cast<double>(result.matchedTokens));
    } else {
        m_stats.misses.fetch_add(1, std::memory_order_relaxed);
        METRICS.increment("runtime.kv.lookup_miss");
    }

    METRICS.gauge("runtime.kv.hit_rate", m_stats.hitRate());
    METRICS.gauge("runtime.kv.snapshot_count",
                  static_cast<double>(m_stats.snapshotCount.load(std::memory_order_relaxed)));
    METRICS.gauge("runtime.kv.node_count",
                  static_cast<double>(m_stats.nodeCount.load(std::memory_order_relaxed)));

    return result;
}

// ============================================================================
// Insert — add a prefix + KV snapshot
// ============================================================================

bool KVCachePrefixTrie::insert(const int32_t* tokens, uint32_t numTokens,
                                uint64_t modelHash,
                                std::shared_ptr<KVSnapshot> snapshot)
{
    if (numTokens < m_cfg.minPrefixLen || numTokens > m_cfg.maxPrefixLen)
        return false;

    if (!snapshot) return false;

    // Check capacity
    if (m_stats.nodeCount.load(std::memory_order_relaxed) >= m_cfg.maxNodes) {
        if (m_cfg.enableLRUEvict) {
            std::unique_lock<std::shared_mutex> lock(m_rwLock);
            evictLRU(m_cfg.maxNodes / 2);
        } else {
            return false;
        }
    }

    if (m_stats.snapshotCount.load(std::memory_order_relaxed) >= m_cfg.maxSnapshots) {
        std::unique_lock<std::shared_mutex> lock(m_rwLock);
        evictExpired();
        if (m_stats.snapshotCount.load(std::memory_order_relaxed) >= m_cfg.maxSnapshots)
            return false;
    }

    std::unique_lock<std::shared_mutex> lock(m_rwLock);

    TrieNode* current = getModelRoot(modelHash);

    for (uint32_t i = 0; i < numTokens; ++i) {
        auto it = current->children.find(tokens[i]);
        if (it == current->children.end()) {
            auto node = std::make_unique<TrieNode>();
            node->depth = i + 1;
            auto* raw = node.get();
            current->children[tokens[i]] = std::move(node);
            m_stats.nodeCount.fetch_add(1, std::memory_order_relaxed);
            current = raw;
        } else {
            current = it->second.get();
        }
    }

    // Assign snapshot
    snapshot->snapshotId = m_nextSnapshotId.fetch_add(1, std::memory_order_relaxed);
    snapshot->createdAtEpoch = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();
    snapshot->numTokensCached = numTokens;
    snapshot->modelHash = modelHash;

    if (!current->snapshot)
        m_stats.snapshotCount.fetch_add(1, std::memory_order_relaxed);

    current->snapshot = std::move(snapshot);
    current->lastAccessEpoch.store(
        (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count(),
        std::memory_order_relaxed);

    m_stats.inserts.fetch_add(1, std::memory_order_relaxed);
    METRICS.increment("runtime.kv.inserts_total");
    METRICS.gauge("runtime.kv.snapshot_count",
                  static_cast<double>(m_stats.snapshotCount.load(std::memory_order_relaxed)));
    METRICS.gauge("runtime.kv.node_count",
                  static_cast<double>(m_stats.nodeCount.load(std::memory_order_relaxed)));

    if (current->depth > m_stats.maxDepth)
        m_stats.maxDepth = current->depth;
    METRICS.gauge("runtime.kv.max_depth", static_cast<double>(m_stats.maxDepth));

    return true;
}

// ============================================================================
// Eviction — TTL based
// ============================================================================

uint32_t KVCachePrefixTrie::evictExpired()
{
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    uint64_t ttlNs = m_cfg.snapshotTTLMs * 1000000ULL;
    uint32_t evicted = 0;

    // Walk all model roots
    for (auto& [mh, root] : m_roots) {
        // BFS/DFS to find expired snapshots
        std::vector<TrieNode*> stack;
        stack.push_back(root.get());
        while (!stack.empty()) {
            TrieNode* node = stack.back();
            stack.pop_back();

            if (node->snapshot) {
                uint64_t age = (uint64_t)now - node->snapshot->createdAtEpoch;
                if (age > ttlNs) {
                    node->snapshot->expired = true;
                    node->snapshot.reset();
                    m_stats.snapshotCount.fetch_sub(1, std::memory_order_relaxed);
                    m_stats.evictions.fetch_add(1, std::memory_order_relaxed);
                    ++evicted;
                }
            }

            for (auto& [tok, child] : node->children) {
                stack.push_back(child.get());
            }
        }
    }

    METRICS.increment("runtime.kv.evictions_total", evicted);
    METRICS.gauge("runtime.kv.snapshot_count",
                  static_cast<double>(m_stats.snapshotCount.load(std::memory_order_relaxed)));

    return evicted;
}

// ============================================================================
// Eviction — LRU based
// ============================================================================

uint32_t KVCachePrefixTrie::evictLRU(uint32_t targetNodes)
{
    // Collect all leaf nodes with their last access time
    std::vector<std::pair<uint64_t, TrieNode*>> leaves;
    for (auto& [mh, root] : m_roots) {
        collectLeaves(root.get(), leaves);
    }

    // Sort by last access (oldest first)
    std::sort(leaves.begin(), leaves.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    uint32_t evicted = 0;
    uint32_t currentNodes = m_stats.nodeCount.load(std::memory_order_relaxed);
    for (auto& [epoch, node] : leaves) {
        if (currentNodes <= targetNodes) break;

        if (node->snapshot) {
            node->snapshot->expired = true;
            node->snapshot.reset();
            m_stats.snapshotCount.fetch_sub(1, std::memory_order_relaxed);
        }

        // Can't delete the node itself (parent owns it), but mark it empty
        node->children.clear();
        m_stats.evictions.fetch_add(1, std::memory_order_relaxed);
        ++evicted;
        --currentNodes;
    }

    METRICS.increment("runtime.kv.evictions_total", evicted);
    METRICS.gauge("runtime.kv.snapshot_count",
                  static_cast<double>(m_stats.snapshotCount.load(std::memory_order_relaxed)));
    METRICS.gauge("runtime.kv.node_count",
                  static_cast<double>(m_stats.nodeCount.load(std::memory_order_relaxed)));

    return evicted;
}

void KVCachePrefixTrie::collectLeaves(
    const TrieNode* node,
    std::vector<std::pair<uint64_t, TrieNode*>>& leaves) const
{
    if (node->children.empty()) {
        leaves.push_back({
            node->lastAccessEpoch.load(std::memory_order_relaxed),
            const_cast<TrieNode*>(node)
        });
        return;
    }
    for (auto& [tok, child] : node->children) {
        collectLeaves(child.get(), leaves);
    }
}

// ============================================================================
// Bulk Operations
// ============================================================================

void KVCachePrefixTrie::clearModel(uint64_t modelHash)
{
    std::unique_lock<std::shared_mutex> lock(m_rwLock);
    auto it = m_roots.find(modelHash);
    if (it != m_roots.end()) {
        // Count nodes being removed
        std::vector<TrieNode*> stack;
        stack.push_back(it->second.get());
        uint32_t removed = 0;
        while (!stack.empty()) {
            auto* n = stack.back();
            stack.pop_back();
            ++removed;
            if (n->snapshot) {
                m_stats.snapshotCount.fetch_sub(1, std::memory_order_relaxed);
            }
            for (auto& [tok, child] : n->children) {
                stack.push_back(child.get());
            }
        }
        m_stats.nodeCount.fetch_sub(removed, std::memory_order_relaxed);
        m_roots.erase(it);
    }
}

void KVCachePrefixTrie::clear()
{
    std::unique_lock<std::shared_mutex> lock(m_rwLock);
    m_roots.clear();
    m_stats.nodeCount.store(0, std::memory_order_relaxed);
    m_stats.snapshotCount.store(0, std::memory_order_relaxed);
}

} // namespace rawrxd
