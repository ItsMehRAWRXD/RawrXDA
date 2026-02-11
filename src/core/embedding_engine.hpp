// ============================================================================
// embedding_engine.hpp — Local Embedding Model Bridge
// ============================================================================
// Connects the HNSW vector index to actual embedding generation.
// Supports:
//   - GGUF-format embedding models (Phi-3-mini, gte-large, etc.)
//   - MASM-accelerated tokenization (when available)
//   - SIMD-optimized distance computation (SSE4.2 / AVX2)
//   - Batched embedding for throughput
//   - Codebase-aware chunking with language-specific parsers
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "vector_index.h"  // HNSWIndex, CodeChunk, IncrementalIndexer
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <unordered_map>
#include <thread>
#include <queue>
#include <condition_variable>

namespace RawrXD {
namespace Embeddings {

// ============================================================================
// Result type — PatchResult-compatible
// ============================================================================
struct EmbedResult {
    bool success;
    const char* detail;
    int errorCode;

    static EmbedResult ok(const char* msg = "OK") {
        EmbedResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }

    static EmbedResult error(const char* msg, int code = -1) {
        EmbedResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Embedding Model Configuration
// ============================================================================
struct EmbeddingModelConfig {
    std::string modelPath;       // Path to GGUF embedding model
    uint32_t dimensions;         // Output dimensions (384, 768, 1024)
    uint32_t maxTokens;          // Max input tokens per chunk
    uint32_t batchSize;          // Batch size for throughput
    bool useGPU;                 // Use Vulkan compute if available
    bool useMASMTokenizer;       // Use MASM SIMD tokenizer
    bool normalizeOutput;        // L2-normalize embeddings
    float quantizationScale;     // For quantized models
    uint32_t numThreads;         // Inference threads

    EmbeddingModelConfig()
        : dimensions(384), maxTokens(512), batchSize(32),
          useGPU(false), useMASMTokenizer(true), normalizeOutput(true),
          quantizationScale(1.0f), numThreads(4) {}
};

// ============================================================================
// Distance metrics (SIMD-optimized where available)
// ============================================================================
enum class DistanceMetric : uint8_t {
    COSINE     = 0,  // Default for code embeddings
    L2         = 1,  // Euclidean distance
    DOT        = 2,  // Dot product (for normalized vectors)
    MANHATTAN  = 3   // L1 distance
};

// ============================================================================
// SIMD-Accelerated Distance Functions
// ============================================================================

// Forward declarations for SIMD implementations
float distance_cosine_scalar(const float* a, const float* b, uint32_t dim);
float distance_l2_scalar(const float* a, const float* b, uint32_t dim);
float distance_dot_scalar(const float* a, const float* b, uint32_t dim);

#ifdef __SSE4_2__
float distance_cosine_sse42(const float* a, const float* b, uint32_t dim);
float distance_l2_sse42(const float* a, const float* b, uint32_t dim);
#endif

#ifdef __AVX2__
float distance_cosine_avx2(const float* a, const float* b, uint32_t dim);
float distance_l2_avx2(const float* a, const float* b, uint32_t dim);
#endif

// Auto-dispatch — picks best available at runtime
using DistanceFn = float(*)(const float* a, const float* b, uint32_t dim);
DistanceFn getOptimalDistanceFn(DistanceMetric metric);

// ============================================================================
// Chunking Strategy for code files
// ============================================================================
struct ChunkingConfig {
    uint32_t maxChunkTokens;      // Max tokens per chunk
    uint32_t overlapTokens;       // Overlap between sliding windows
    bool splitByFunction;         // Use language parser to split by function
    bool splitByClass;            // Split by class boundary
    bool generateFileSummary;     // Generate a summary chunk per file
    bool includeImports;          // Include import/include section
    uint32_t minChunkLines;       // Skip chunks smaller than this

    ChunkingConfig()
        : maxChunkTokens(256), overlapTokens(32),
          splitByFunction(true), splitByClass(true),
          generateFileSummary(true), includeImports(true),
          minChunkLines(3) {}
};

// ============================================================================
// Language-Aware Chunker
// ============================================================================
struct LanguageChunker {
    // Function boundary patterns per language
    struct FunctionPattern {
        const char* language;
        const char* startPattern;   // Regex-like start
        const char* endPattern;     // Regex-like end
    };

    static std::vector<CodeChunk> chunkFile(
        const std::string& filepath,
        const std::string& content,
        const ChunkingConfig& config);

    static std::vector<CodeChunk> chunkBySlidingWindow(
        const std::string& filepath,
        const std::string& content,
        uint32_t windowSize,
        uint32_t overlap);

    static std::vector<CodeChunk> chunkByFunctions(
        const std::string& filepath,
        const std::string& content,
        const std::string& language);

    static std::string detectLanguage(const std::string& filepath);

    // Built-in patterns for C/C++/Python/JavaScript/Rust/Go
    static const FunctionPattern PATTERNS[];
    static const size_t PATTERN_COUNT;
};

// ============================================================================
// Embedding Engine — The main bridge
// ============================================================================
class EmbeddingEngine {
public:
    static EmbeddingEngine& instance();

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    // Load an embedding model (GGUF format)
    EmbedResult loadModel(const EmbeddingModelConfig& config);

    // Unload model, free resources
    EmbedResult unloadModel();

    // Check if model is loaded and ready
    bool isReady() const;

    // Get model info
    const EmbeddingModelConfig& getConfig() const;

    // -----------------------------------------------------------------------
    // Single Embedding
    // -----------------------------------------------------------------------

    // Embed a single text string → float vector
    EmbedResult embed(const std::string& text,
                      std::vector<float>& outEmbedding);

    // Embed a code chunk (uses language-aware preprocessing)
    EmbedResult embedChunk(const CodeChunk& chunk,
                           std::vector<float>& outEmbedding);

    // -----------------------------------------------------------------------
    // Batch Embedding
    // -----------------------------------------------------------------------

    // Embed multiple texts in a batch (higher throughput)
    EmbedResult embedBatch(const std::vector<std::string>& texts,
                           std::vector<std::vector<float>>& outEmbeddings);

    // Embed all chunks from a file
    EmbedResult embedFile(const std::string& filepath,
                          const ChunkingConfig& chunkConfig,
                          std::vector<CodeChunk>& outChunks);

    // -----------------------------------------------------------------------
    // Codebase Indexing (integrates with HNSWIndex)
    // -----------------------------------------------------------------------

    // Index an entire directory recursively
    EmbedResult indexDirectory(const std::string& dirPath,
                               const ChunkingConfig& chunkConfig);

    // Update index for a single changed file
    EmbedResult updateFile(const std::string& filepath);

    // Remove a file from the index
    EmbedResult removeFile(const std::string& filepath);

    // -----------------------------------------------------------------------
    // Semantic Search
    // -----------------------------------------------------------------------

    struct SearchResult {
        CodeChunk chunk;
        float score;           // Similarity score (0.0 - 1.0)
        float distance;        // Raw distance value
    };

    // Search for similar code by natural language query
    EmbedResult search(const std::string& query,
                       uint32_t topK,
                       std::vector<SearchResult>& results);

    // Search for similar code by code snippet
    EmbedResult searchByCode(const std::string& codeSnippet,
                             uint32_t topK,
                             std::vector<SearchResult>& results);

    // HyDE search: generate hypothetical document, then search
    EmbedResult searchHyDE(const std::string& query,
                           uint32_t topK,
                           std::vector<SearchResult>& results);

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    struct EngineStats {
        uint64_t totalEmbeddings;
        uint64_t totalSearches;
        uint64_t indexedFiles;
        uint64_t indexedChunks;
        uint64_t cacheHits;
        uint64_t cacheMisses;
        double avgEmbedTimeMs;
        double avgSearchTimeMs;
        uint64_t modelMemoryBytes;
    };

    EngineStats getStats() const;

    // -----------------------------------------------------------------------
    // Persistence
    // -----------------------------------------------------------------------

    // Save the entire index to disk
    EmbedResult saveIndex(const std::string& filepath);

    // Load index from disk
    EmbedResult loadIndex(const std::string& filepath);

    // Shutdown and cleanup
    void shutdown();

private:
    EmbeddingEngine();
    ~EmbeddingEngine();
    EmbeddingEngine(const EmbeddingEngine&) = delete;
    EmbeddingEngine& operator=(const EmbeddingEngine&) = delete;

    // Model inference — calls either GGUF engine or ONNX runtime
    EmbedResult inferEmbedding(const std::string& text,
                                std::vector<float>& out);

    // Preprocessing: add language prefix, truncate, normalize
    std::string preprocess(const std::string& text,
                           const std::string& language = "");

    // L2 normalization in-place
    void normalizeL2(std::vector<float>& vec);

    // Background indexing thread
    void indexWorker();

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex engineMutex_;
    EmbeddingModelConfig config_;
    bool modelLoaded_;

    // HNSW index (from vector_index.h)
    std::unique_ptr<HNSWIndex> hnswIndex_;

    // Embedding cache (LRU)
    std::unique_ptr<EmbeddingCache> cache_;

    // Incremental indexer
    std::unique_ptr<IncrementalIndexer> indexer_;

    // File tracking for change detection
    struct FileRecord {
        std::string filepath;
        uint64_t lastModified;
        uint64_t fileSize;
        std::vector<uint64_t> chunkIds;
    };
    std::unordered_map<std::string, FileRecord> fileIndex_;

    // Background indexing
    std::thread indexThread_;
    std::queue<std::string> indexQueue_;
    std::mutex indexQueueMutex_;
    std::condition_variable indexQueueCV_;
    std::atomic<bool> indexRunning_;

    // Statistics
    std::atomic<uint64_t> totalEmbeddings_;
    std::atomic<uint64_t> totalSearches_;
    std::atomic<uint64_t> cacheHits_;
    std::atomic<uint64_t> cacheMisses_;
    double embedTimeAccumMs_;
    double searchTimeAccumMs_;

    // Model handle (opaque — implementation depends on backend)
    void* modelHandle_;

    // Distance function (auto-selected based on CPU features)
    DistanceFn distanceFn_;
};

} // namespace Embeddings
} // namespace RawrXD
