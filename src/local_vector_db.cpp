/**
 * @file local_vector_db.cpp
 * @brief LocalVectorDB implementation for Phase 4.2.1
 *
 * This implementation provides:
 * 1. HNSW index loading/initialization
 * 2. TinyBERT embedding generation
 * 3. Semantic search over code corpus
 * 4. Graceful fallback for testing without full HNSW
 */

#include "local_vector_db.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace RawrXD {

/**
 * @class LocalVectorDBImpl
 * Concrete implementation of LocalVectorDB
 *
 * Phase 4.2.1: Infrastructure for vector-based code search
 *
 * Current Status:
 * - Stub implementation ready for HNSW library integration
 * - Basic embedding skeleton (will use GGML infrastructure)
 * - Fallback mock search for development/testing
 */
class LocalVectorDBImpl : public LocalVectorDB {
public:
    LocalVectorDBImpl() : m_indexSize(0), m_isReady(false) {}

    /**
     * Initialize from index file or build from source
     */
    bool Initialize(std::string_view sourceDir,
                   std::string_view indexPath,
                   std::string_view embeddingModelPath) {
        m_sourceDir = sourceDir;
        m_indexPath = indexPath;
        m_embeddingModelPath = embeddingModelPath;

        // Phase 4.2.1: Check if pre-built index exists
        if (fs::exists(m_indexPath)) {
            std::cout << "[LocalVectorDB] Loading pre-built HNSW index from: " << m_indexPath << std::endl;
            if (LoadIndexFromDisk()) {
                m_isReady = true;
                std::cout << "[LocalVectorDB] Index loaded successfully. Size: " << m_indexSize << " blocks\n";
                return true;
            }
        }

        // Phase 4.2.1: Build index from source files on first run
        std::cout << "[LocalVectorDB] No pre-built index found. Building from source: " << m_sourceDir << std::endl;
        if (BuildIndexFromSource()) {
            m_isReady = true;
            std::cout << "[LocalVectorDB] Index built successfully. Size: " << m_indexSize << " blocks\n";
            
            // Save for next startup
            SaveIndexToDisk();
            return true;
        }

        std::cerr << "[LocalVectorDB] Failed to initialize vector database\n";
        return false;
    }

    /**
     * Embed a context window using TinyBERT
     *
     * Phase 4.2.1: Stub awaiting GGML TinyBERT integration
     * This will use ggml_graph_compute() with the TinyBERT model
     */
    std::vector<float> EmbedContext(std::string_view context) override {
        std::vector<float> embedding(768, 0.0f);  // 768-dim TinyBERT output

        // TODO Phase 4.2.1.1: Load TinyBERT model via GGML
        // TODO Phase 4.2.1.1: Tokenize context (BPE)
        // TODO Phase 4.2.1.1: Run inference: ggml_graph_compute()
        // TODO Phase 4.2.1.1: Extract final hidden state (768 dims)

        // For now: Generate mock embedding from context hash
        // This allows testing the search pipeline without TinyBERT
        _generateMockEmbedding(context, embedding);

        return embedding;
    }

    /**
     * Search HNSW index for similar code blocks (top-K variant)
     *
     * Phase 4.2.1.2: Mock brute-force search (placeholder for real HNSW)
     * Returns multiple results ranked by similarity instead of just best-1
     */
    std::vector<VectorSearchResult> SearchTopK(const std::vector<float>& queryEmbedding,
                                              int k, int ef) override {
        std::vector<VectorSearchResult> results;
        
        if (!m_isReady || m_codeBlocks.empty()) {
            return results;
        }

        // Calculate similarity scores for all blocks
        std::vector<std::pair<float, size_t>> similarities;
        for (size_t i = 0; i < m_codeBlocks.size(); i++) {
            float similarity = _cosineSimilarity(queryEmbedding, m_codeBlocks[i].embedding);
            similarities.push_back({similarity, i});
        }

        // Sort by descending similarity
        std::sort(similarities.begin(), similarities.end(),
                 [](const auto& a, const auto& b) { return a.first > b.first; });

        // Extract top-K results
        for (int i = 0; i < std::min(k, static_cast<int>(similarities.size())); i++) {
            if (similarities[i].first < 0.65f) {
                break;  // Stop if similarity falls below threshold
            }

            const auto& block = m_codeBlocks[similarities[i].second];
            VectorSearchResult result;
            result.found = true;
            result.snippet = block.text;
            result.source_file = block.sourceFile;
            result.line_number = block.lineNumber;
            result.similarity = similarities[i].first;
            result.relevance_score = similarities[i].first;

            results.push_back(result);
        }

        return results;
    }

    std::vector<VectorSearchResult> Query(std::string_view context,
                                          int k,
                                          int ef,
                                          float minSimilarity) override {
        if (!m_isReady) {
            return {};
        }

        const auto embedding = EmbedContext(context);
        auto results = SearchTopK(embedding, k, ef);

        if (minSimilarity > 0.0f) {
            results.erase(std::remove_if(results.begin(), results.end(),
                                         [minSimilarity](const VectorSearchResult& r) {
                                             return r.similarity < minSimilarity;
                                         }),
                          results.end());
        }

        return results;
    }

    /**
     * Search HNSW index for similar code blocks (backwards compatible)
     *
     * Phase 4.2.1: Mock brute-force search (placeholder for real HNSW)
     * Returns best single result from top-K search
     */
    VectorSearchResult Search(const std::vector<float>& queryEmbedding,
                             int k, int ef) override {
        auto results = SearchTopK(queryEmbedding, k, ef);
        if (!results.empty()) {
            return results[0];  // Return best match
        }
        return VectorSearchResult{};
    }

    /**
     * Combined search pipeline: embed + search
     */
    VectorSearchResult SearchContext(std::string_view context) override {
        auto results = Query(context, 1, 40, 0.65f);
        if (!results.empty()) {
            return results.front();
        }
        return VectorSearchResult{};
    }

    /**
     * Get database statistics for monitoring
     */
    std::string GetStats() const override {
        std::ostringstream oss;
        oss << "LocalVectorDB {\n"
            << "  index_size: " << m_indexSize << " code blocks\n"
            << "  source_dir: " << m_sourceDir << "\n"
            << "  index_path: " << m_indexPath << "\n"
            << "  embedding_model: " << m_embeddingModelPath << "\n"
            << "  is_ready: " << (m_isReady ? "true" : "false") << "\n"
            << "  status: "
            << (m_isReady ? "Ready for semantic search" : "Not initialized")
            << "\n}";
        return oss.str();
    }

    bool IsReady() const override { return m_isReady; }

    uint32_t GetIndexSize() const override { return m_indexSize; }

private:
    std::string m_sourceDir;
    std::string m_indexPath;
    std::string m_embeddingModelPath;
    uint32_t m_indexSize;
    bool m_isReady;

    // Mock storage for Phase 4.2.1 testing (before real HNSW integration)
    struct CodeBlock {
        std::string text;
        std::string sourceFile;
        int lineNumber;
        std::vector<float> embedding;  // 768-dim
    };
    std::vector<CodeBlock> m_codeBlocks;

    /**
     * Load pre-built HNSW index from disk
     *
     * TODO Phase 4.2.1.2: Wire actual HNSW binary format
     */
    bool LoadIndexFromDisk() {
        try {
            std::ifstream file(m_indexPath, std::ios::binary);
            if (!file) {
                return false;
            }

            // TODO: Read HNSW binary index format
            // For now: Load mock index metadata
            uint32_t size;
            file.read(reinterpret_cast<char*>(&size), sizeof(size));
            m_indexSize = size;

            file.close();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[LocalVectorDB] Error loading index: " << e.what() << "\n";
            return false;
        }
    }

    /**
     * Build HNSW index from source code files
     *
     * Phase 4.2.1.0: Scan d:/rawrxd/src/ for .cpp/.h files
     * Split into code blocks (~256 tokens each)
     * Generate embeddings and build HNSW graph
     */
    bool BuildIndexFromSource() {
        try {
            if (!fs::exists(m_sourceDir)) {
                std::cerr << "[LocalVectorDB] Source directory not found: " << m_sourceDir << "\n";
                return false;
            }

            // Scan for source files
            int blockCount = 0;
            for (const auto& entry : fs::recursive_directory_iterator(m_sourceDir)) {
                if (!entry.is_regular_file()) continue;

                auto ext = entry.path().extension().string();
                if (ext != ".cpp" && ext != ".h" && ext != ".hpp") continue;

                // Read file and split into code blocks
                if (!_splitFileIntoCodeBlocks(entry.path())) {
                    std::cerr << "[LocalVectorDB] Warning: Failed to process " << entry.path() << "\n";
                    continue;
                }

                blockCount++;
                if (blockCount >= 100) break;  // Limit for Phase 4.2.1 testing
            }

            m_indexSize = static_cast<uint32_t>(m_codeBlocks.size());

            // TODO Phase 4.2.1.2: Build HNSW graph from embeddings

            std::cout << "[LocalVectorDB] Processed " << blockCount << " files, "
                     << m_indexSize << " code blocks\n";

            return m_indexSize > 0;
        } catch (const std::exception& e) {
            std::cerr << "[LocalVectorDB] Error building index: " << e.what() << "\n";
            return false;
        }
    }

    /**
     * Save HNSW index to disk for future startup
     *
     * Phase 4.2.1.2: Wire actual HNSW binary format
     */
    void SaveIndexToDisk() {
        try {
            fs::create_directories(fs::path(m_indexPath).parent_path());

            std::ofstream file(m_indexPath, std::ios::binary);
            if (!file) {
                std::cerr << "[LocalVectorDB] Warning: Could not save index to " << m_indexPath << "\n";
                return;
            }

            // Write index metadata
            uint32_t size = m_indexSize;
            file.write(reinterpret_cast<const char*>(&size), sizeof(size));

            // TODO: Write HNSW binary index format

            file.close();
            std::cout << "[LocalVectorDB] Index saved to: " << m_indexPath << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[LocalVectorDB] Error saving index: " << e.what() << "\n";
        }
    }

    /**
     * Split source file into code blocks (~256 tokens)
     */
    bool _splitFileIntoCodeBlocks(const fs::path& filePath) {
        try {
            std::ifstream file(filePath);
            if (!file) return false;

            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();

            // Split into ~256-token blocks (estimate: 4 chars per token)
            const size_t blockSize = 256 * 4;  // ~1024 bytes per block
            size_t lineNumber = 1;

            for (size_t i = 0; i < content.size(); i += blockSize) {
                std::string blockText = content.substr(i, blockSize);

                CodeBlock block;
                block.text = blockText;
                block.sourceFile = filePath.filename().string();
                block.lineNumber = lineNumber;
                block.embedding = std::vector<float>(768, 0.0f);  // Placeholder

                m_codeBlocks.push_back(block);

                // Count lines for next block
                lineNumber += std::count(blockText.begin(), blockText.end(), '\n');
            }

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    /**
     * Generate mock embedding from text hash
     * Allows testing search pipeline without TinyBERT
     */
    void _generateMockEmbedding(std::string_view text, std::vector<float>& embedding) {
        // Deterministic mock: use text hash to seed predictable embeddings
        uint32_t hash = 5381;
        for (char c : text) {
            hash = ((hash << 5) + hash) + static_cast<uint8_t>(c);
        }

        // Fill embedding with hash-based pseudo-random values
        for (size_t i = 0; i < embedding.size(); i++) {
            hash = hash * 1103515245 + 12345;
            embedding[i] = (hash % 1000) / 1000.0f;  // [0, 1)
        }
    }

    /**
     * Mock HNSW search using in-memory code blocks
     * Calculates cosine similarity to all blocks
     */
    VectorSearchResult _searchMockIndex(const std::vector<float>& queryEmbedding) {
        if (m_codeBlocks.empty()) {
            return VectorSearchResult{};
        }

        float bestSimilarity = -1.0f;
        size_t bestIdx = 0;

        // Find block with highest cosine similarity
        for (size_t i = 0; i < m_codeBlocks.size(); i++) {
            float similarity = _cosineSimilarity(queryEmbedding, m_codeBlocks[i].embedding);
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                bestIdx = i;
            }
        }

        VectorSearchResult result;
        result.found = bestSimilarity > 0.65f;
        if (result.found) {
            result.snippet = m_codeBlocks[bestIdx].text;
            result.source_file = m_codeBlocks[bestIdx].sourceFile;
            result.line_number = m_codeBlocks[bestIdx].lineNumber;
            result.similarity = bestSimilarity;
            result.relevance_score = bestSimilarity;
        }

        return result;
    }

    /**
     * Calculate cosine similarity between two 768-dim vectors
     */
    static float _cosineSimilarity(const std::vector<float>& a,
                                  const std::vector<float>& b) {
        if (a.size() != b.size() || a.empty()) return 0.0f;

        float dotProduct = 0.0f;
        float normA = 0.0f;
        float normB = 0.0f;

        for (size_t i = 0; i < a.size(); i++) {
            dotProduct += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }

        float denom = std::sqrt(normA * normB);
        return denom > 1e-6f ? dotProduct / denom : 0.0f;
    }
};

/**
 * Factory method to initialize LocalVectorDB
 */
std::unique_ptr<LocalVectorDB> LocalVectorDB::Initialize(
    std::string_view sourceDir,
    std::string_view indexPath,
    std::string_view embeddingModelPath) {

    auto db = std::make_unique<LocalVectorDBImpl>();
    if (db->Initialize(sourceDir, indexPath, embeddingModelPath)) {
        return db;
    }
    return nullptr;
}

}  // namespace RawrXD
