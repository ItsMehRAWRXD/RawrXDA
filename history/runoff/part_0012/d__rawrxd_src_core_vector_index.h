// ============================================================================
// vector_index.h — Codebase Embeddings Index with HNSW
// ============================================================================
// Implements CodeChunk storage, HNSW approximate nearest neighbor search,
// chunking strategies, LRU cache, and incremental indexing.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <random>
#include <queue>

namespace RawrXD {
namespace Embeddings {

// ============================================================================
// Code Chunk — a segment of source code with its embedding
// ============================================================================

struct CodeChunk {
    std::filesystem::path file;
    uint32_t startLine;
    uint32_t endLine;
    std::vector<float> embedding;    // 384-dim (Phi-3) or 768-dim (gte-large)
    std::string content;             // Original text for re-ranking
    uint64_t lastModified;           // Epoch ms, for cache invalidation
    uint64_t chunkId;                // Unique ID for HNSW node

    enum class ChunkType : uint8_t {
        FUNCTION,       // Function-level chunk
        CLASS,          // Class-level chunk
        STRUCT,         // Struct-level chunk
        BLOCK,          // Generic block (if/while/etc)
        SLIDING_WINDOW, // Token-based sliding window
        FILE_SUMMARY    // AI-generated file summary
    };
    ChunkType type;
};

// ============================================================================
// Chunking Strategy
// ============================================================================

struct ChunkingConfig {
    uint32_t slidingWindowTokens = 512;
    uint32_t overlapTokens = 128;
    uint32_t maxChunkLines = 200;
    uint32_t minChunkLines = 5;
    bool enableClassLevel = true;
    bool enableFunctionLevel = true;
    bool enableSlidingWindow = true;
};

std::vector<CodeChunk> chunkFile(const std::filesystem::path& file,
                                  const std::string& content,
                                  const ChunkingConfig& config = {});

std::vector<CodeChunk> chunkFunction(const std::filesystem::path& file,
                                      const std::string& content,
                                      int startLine, int endLine);

// ============================================================================
// Index Result
// ============================================================================

struct IndexResult {
    bool success;
    const char* detail;
    int errorCode;

    static IndexResult ok(const char* msg = "OK") { return {true, msg, 0}; }
    static IndexResult error(const char* msg, int code = -1) { return {false, msg, code}; }
};

// ============================================================================
// Search Result — a nearest neighbor match
// ============================================================================

struct SearchResult {
    uint64_t chunkId;
    float distance;                  // L2 or cosine distance
    const CodeChunk* chunk;          // Pointer into index storage
};

// ============================================================================
// HNSW Index — Hierarchical Navigable Small World graph
// ============================================================================

class HNSWIndex {
public:
    // ---- Configuration ----
    struct Config {
        uint32_t M = 16;               // Max edges per node per layer
        uint32_t efConstruction = 200;  // Search width during build
        uint32_t efSearch = 64;         // Search width during query
        uint32_t dim = 384;             // Embedding dimension
        uint32_t maxElements = 100000;
        bool useCosineDistance = true;   // false = L2 distance
    };

    explicit HNSWIndex(const Config& config = {});
    ~HNSWIndex();

    // Non-copyable
    HNSWIndex(const HNSWIndex&) = delete;
    HNSWIndex& operator=(const HNSWIndex&) = delete;

    // ---- Insert ----
    IndexResult insert(uint64_t id, const float* embedding);
    IndexResult insertBatch(const std::vector<uint64_t>& ids,
                             const std::vector<const float*>& embeddings);

    // ---- Search ----
    std::vector<std::pair<uint64_t, float>> search(const float* query, uint32_t k) const;

    // ---- Delete ----
    IndexResult remove(uint64_t id);

    // ---- Persistence ----
    IndexResult saveToFile(const std::filesystem::path& path) const;
    IndexResult loadFromFile(const std::filesystem::path& path);

    // ---- Stats ----
    size_t size() const { return m_nodeCount.load(std::memory_order_relaxed); }
    uint32_t dimension() const { return m_config.dim; }
    size_t memoryUsageBytes() const;

private:
    // ---- HNSW internals ----
    struct Node {
        uint64_t id;
        std::vector<float> embedding;
        std::vector<std::vector<uint64_t>> neighbors;  // Per-level neighbor lists
        int maxLevel;
    };

    float distance(const float* a, const float* b) const;
    int randomLevel() const;

    using MinHeap = std::priority_queue<std::pair<float, uint64_t>,
                                         std::vector<std::pair<float, uint64_t>>,
                                         std::greater<>>;

    MinHeap searchLayer(const float* query, uint64_t entryPoint,
                        uint32_t ef, int layer) const;

    void selectNeighbors(const float* query,
                         std::vector<std::pair<float, uint64_t>>& candidates,
                         uint32_t M) const;

    // ---- Storage ----
    Config m_config;
    std::unordered_map<uint64_t, std::unique_ptr<Node>> m_nodes;
    uint64_t m_entryPoint = 0;
    int m_maxLevel = 0;
    std::atomic<size_t> m_nodeCount{0};
    mutable std::mutex m_mutex;
    mutable std::mt19937 m_rng;
    float m_levelMultiplier;
};

// ============================================================================
// LRU Embedding Cache — bounded cache with eviction
// ============================================================================

class EmbeddingCache {
public:
    explicit EmbeddingCache(size_t maxEntries = 10000);

    // Store an embedding (evicts LRU if full)
    void put(const std::string& key, const std::vector<float>& embedding);

    // Retrieve an embedding (returns nullptr if not cached)
    const std::vector<float>* get(const std::string& key);

    // Invalidate
    void evict(const std::string& key);
    void clear();

    // Stats
    size_t size() const;
    size_t maxSize() const { return m_maxEntries; }
    size_t hitCount() const { return m_hits; }
    size_t missCount() const { return m_misses; }
    float hitRate() const;

private:
    struct CacheEntry {
        std::string key;
        std::vector<float> embedding;
        uint64_t lastAccess;
    };

    size_t m_maxEntries;
    std::unordered_map<std::string, size_t> m_keyToIndex;
    std::vector<CacheEntry> m_entries;
    mutable std::mutex m_mutex;
    uint64_t m_accessCounter = 0;
    size_t m_hits = 0;
    size_t m_misses = 0;
};

// ============================================================================
// Incremental Indexer — diff-based index updates
// ============================================================================

class IncrementalIndexer {
public:
    IncrementalIndexer(HNSWIndex& index, EmbeddingCache& cache);

    // Embedding function type (provided externally — ONNX, etc.)
    using EmbedFunction = std::function<std::vector<float>(const std::string& text)>;

    void setEmbedFunction(EmbedFunction fn) { m_embedFn = std::move(fn); }
    void setChunkingConfig(const ChunkingConfig& config) { m_chunkConfig = config; }

    // ---- Indexing ----
    IndexResult indexFile(const std::filesystem::path& file);
    IndexResult indexDirectory(const std::filesystem::path& dir,
                                const std::vector<std::string>& extensions = {});
    IndexResult updateFile(const std::filesystem::path& file);  // Diff-based update
    IndexResult removeFile(const std::filesystem::path& file);

    // ---- Query ----
    std::vector<SearchResult> search(const std::string& query, uint32_t topK = 10);

    // ---- Query Preprocessing ----
    // Keyword extraction via TF-IDF
    std::vector<std::string> extractKeywords(const std::string& query, int maxKeywords = 5);

    // HyDE: generate hypothetical document from query
    std::string hydeExpand(const std::string& query);

    // ---- Stats ----
    size_t indexedFileCount() const;
    size_t totalChunks() const;

private:
    HNSWIndex& m_index;
    EmbeddingCache& m_cache;
    EmbedFunction m_embedFn;
    ChunkingConfig m_chunkConfig;

    // File → chunk IDs mapping for incremental updates
    std::unordered_map<std::string, std::vector<uint64_t>> m_fileChunks;
    // Chunk ID → CodeChunk for search results
    std::unordered_map<uint64_t, CodeChunk> m_chunkStore;
    // File → last modified time
    std::unordered_map<std::string, uint64_t> m_fileModTimes;

    uint64_t m_nextChunkId = 1;
    mutable std::mutex m_mutex;
};

} // namespace Embeddings
} // namespace RawrXD
