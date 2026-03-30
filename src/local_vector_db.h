/**
 * @file local_vector_db.h
 * @brief LocalVectorDB: Sovereign vector database for semantic code search
 *
 * Phase 4.2.1 RAG Infrastructure
 * ==============================
 * Manages HNSW (Hierarchical Navigable Small World) index over RawrXD source code.
 * Uses TinyBERT 768-dim embeddings for semantic search across codebase.
 *
 * Architecture:
 * - Index: HNSW graph built from source file code blocks
 * - Embeddings: TinyBERT-768 (cached on load for O(1) lookup)
 * - Search: k-NN search (k=5) with ef=40 parameter for quality/speed tradeoff
 * - Context window: 256-512 tokens from Primary lane pulse
 * - Latency: 5-15ms per search (2-8ms embedding + 3-10ms HNSW search)
 *
 * Thread Safety:
 * - Index loading: One-time initialization in orchestrator constructor
 * - Search: Lock-free reads from Librarian thread
 * - No mutations during inference (static index)
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdint>
#include <map>

namespace RawrXD {

/**
 * @struct VectorSearchResult
 * Result from HNSW vector search
 */
struct VectorSearchResult {
    bool found = false;
    std::string snippet;           // Code snippet text
    float similarity = 0.0f;       // Cosine similarity [0, 1]
    std::string source_file;       // Filename where snippet came from
    int line_number = 0;           // Starting line in source file
    float relevance_score = 0.0f;  // Combined relevance metric
};

/**
 * @class LocalVectorDB
 * Manages HNSW vector index for code semantic search
 *
 * Phase 4.2.1 Implementation:
 * - Loads pre-built HNSW index from disk or builds on first run
 * - Embeds query context windows using TinyBERT
 * - Searches HNSW with configurable k and ef parameters
 * - Returns top-K snippets ranked by similarity
 */
class LocalVectorDB {
public:
    /**
     * @brief Initialize vector database from index file or build from source
     * @param sourceDir Directory containing .cpp/.h files (e.g., d:/rawrxd/src/)
     * @param indexPath Path to save/load HNSW index (e.g., d:/rawrxd/src/hnsw_index.bin)
     * @param embeddingModelPath Path to TinyBERT model weights (e.g., tinybert-768.gguf)
     * @return true if initialization successful
     */
    static std::unique_ptr<LocalVectorDB> Initialize(
        std::string_view sourceDir,
        std::string_view indexPath,
        std::string_view embeddingModelPath);

    virtual ~LocalVectorDB() = default;

    /**
     * @brief Embed a context window (query) using TinyBERT
     * 
     * Phase 4.2.1.1 (TinyBERT Integration):
     * - Loads tinybert-768.gguf model via GGML
     * - Tokenizes input using WordPiece tokenizer
     * - Runs inference: [CLS] + tokens + [SEP]
     * - Extracts final hidden state (768-dim)
     * 
     * Current Stage (Phase 4.2.1):
     * - Uses mock deterministic embedding from text hash
     * - Allows testing full search pipeline without TinyBERT
     * - Latency: ~100µs per embedding (mock)
     * - Future: 2-8ms per embedding (TinyBERT)
     * 
     * @param context Raw text from Primary lane pulse
     * @return 768-dimensional embedding vector
     */
    virtual std::vector<float> EmbedContext(std::string_view context) = 0;

    /**
     * @brief Search for top-K similar code blocks
     * 
     * Phase 4.2.1.2 (HNSW Integration):
     * - Uses real HNSW graph traversal (when available)
     * - Returns k best matches with similarity scores
     * 
     * Current Stage (Phase 4.2.1):
     * - Uses mock brute-force cosine similarity search
     * - Computes similarity to all indexed blocks
     * - Returns top-K ranked by relevance
     * - Latency: ~500µs for 1481 blocks (mock)
     * - Future: 3-10ms for same corpus (HNSW)
     * 
     * @param queryEmbedding 768-dim query vector
     * @param k Number of results to return
     * @param ef Heuristic for HNSW search quality (ef >= k)
     * @return Top-K results ranked by similarity
     */
    virtual std::vector<VectorSearchResult> SearchTopK(
        const std::vector<float>& queryEmbedding,
        int k = 5, int ef = 40) = 0;

    /**
     * @brief Full query pipeline for Librarian lane: embed + top-K + threshold filter
     *
     * This is the primary API for orchestration code. It performs embedding
     * and returns ranked results that meet the similarity threshold.
     *
     * @param context Raw context window from pulse payload
     * @param k Number of results to return
     * @param ef Search parameter for quality/speed tradeoff
     * @param minSimilarity Similarity floor for returned results
     * @return Ranked results that satisfy minSimilarity
     */
    virtual std::vector<VectorSearchResult> Query(
        std::string_view context,
        int k = 5,
        int ef = 40,
        float minSimilarity = 0.65f) = 0;

    /**
     * @brief Search HNSW index with embedded query
     * @param queryEmbedding 768-dim vector from EmbedContext()
     * @param k Number of results to return (default: 5)
     * @param ef Search parameter for HNSW (default: 40)
     * @return Top match by similarity, or empty result if no matches found
     */
    virtual VectorSearchResult Search(
        const std::vector<float>& queryEmbedding,
        int k = 5,
        int ef = 40) = 0;

    /**
     * @brief Combined search: embed + search
     * @param context Raw context window from pulse
     * @return Best match from index
     */
    virtual VectorSearchResult SearchContext(std::string_view context) = 0;

    /**
     * @brief Get index statistics for monitoring/diagnostics
     */
    virtual std::string GetStats() const = 0;

    /**
     * @brief Check if vector database is ready for queries
     */
    virtual bool IsReady() const = 0;

    /**
     * @brief Get number of code blocks in index
     */
    virtual uint32_t GetIndexSize() const = 0;
};

}  // namespace RawrXD
