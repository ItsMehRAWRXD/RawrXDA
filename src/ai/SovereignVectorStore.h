// ============================================================================
// SovereignVectorStore.h — R15-Based Embedding Storage for RAG
// ============================================================================
// Flat float array in R15 heap. No SQL. No external vector DB.
// Cosine similarity via AVX2/AVX-512.
//
// Architecture:
//   [embedding_0 | embedding_1 | ... | embedding_N]  (dense float array)
//   metadata array (parallel, in R15)
//
// Pattern: Pre-allocated, LRU eviction on memory pressure.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace RawrXD {
namespace AI {

// ============================================================================
// Embedding chunk metadata
// ============================================================================
struct EmbeddingChunk {
    uint64_t    id = 0;              // Unique chunk ID
    uint64_t    fileId = 0;          // Source file identifier
    std::string name;                // Function/class name
    size_t      codeOffset = 0;      // Byte offset in source file
    size_t      codeLength = 0;      // Length of code snippet
    float*      embedding = nullptr; // Pointer into R15 float array
    uint32_t    dim = 0;             // Embedding dimension
    uint64_t    lastAccess = 0;      // For LRU eviction
    float       norm = 0.0f;         // Precomputed L2 norm
};

// ============================================================================
// Search result
// ============================================================================
struct SimilarityResult {
    const EmbeddingChunk* chunk = nullptr;
    float score = 0.0f;  // Cosine similarity [-1, 1]
};

// ============================================================================
// Sovereign Vector Store
// ============================================================================
class SovereignVectorStore {
public:
    explicit SovereignVectorStore(uint32_t embeddingDim = 384,
                                   size_t maxChunks = 100000,
                                   size_t r15ReserveBytes = 512 * 1024 * 1024);
    ~SovereignVectorStore();

    // Non-copyable
    SovereignVectorStore(const SovereignVectorStore&) = delete;
    SovereignVectorStore& operator=(const SovereignVectorStore&) = delete;

    // ------------------------------------------------------------------------
    // Indexing
    // ------------------------------------------------------------------------
    uint64_t AddChunk(const EmbeddingChunk& chunk, const float* embedding);
    bool RemoveChunk(uint64_t chunkId);
    void Clear();

    // ------------------------------------------------------------------------
    // Search
    // ------------------------------------------------------------------------
    std::vector<SimilarityResult> Search(
        const float* queryEmbedding,
        size_t topK = 10,
        float minScore = 0.7f
    );

    // Batch search (AVX-512 accelerated)
    std::vector<std::vector<SimilarityResult>> SearchBatch(
        const std::vector<const float*>& queryEmbeddings,
        size_t topK = 10
    );

    // ------------------------------------------------------------------------
    // Stats
    // ------------------------------------------------------------------------
    size_t GetChunkCount() const;
    size_t GetMemoryBytes() const;
    uint32_t GetEmbeddingDim() const { return embeddingDim_; }

    // ------------------------------------------------------------------------
    // Context assembly for LLM prompt
    // ------------------------------------------------------------------------
    std::string AssembleContext(
        const float* queryEmbedding,
        size_t maxChunks = 5,
        size_t maxTokens = 2048
    );

private:
    uint32_t embeddingDim_;
    size_t maxChunks_;
    size_t r15ReserveBytes_;

    // R15 memory block for embeddings
    float* embeddingStore_ = nullptr;
    size_t embeddingCapacity_ = 0;
    size_t embeddingCount_ = 0;

    // Metadata (heap — small relative to embeddings)
    std::unordered_map<uint64_t, EmbeddingChunk> chunks_;
    std::unordered_map<uint64_t, std::vector<uint64_t>> fileIndex_;
    uint64_t nextChunkId_ = 1;

    // Thread safety
    mutable std::mutex mutex_;
    mutable std::mutex searchMutex_; // Separate for concurrent searches

    // LRU
    uint64_t accessCounter_ = 0;

    // Internal
    bool GrowStore(size_t neededChunks);
    void EvictLRU(size_t neededSlots);
    float ComputeCosineSimilarity(const float* a, const float* b, float normB) const;
    float ComputeNorm(const float* vec) const;

    // AVX accelerated
    float ComputeCosineSimilarityAVX(const float* a, const float* b, float normB) const;
};

} // namespace AI
} // namespace RawrXD
