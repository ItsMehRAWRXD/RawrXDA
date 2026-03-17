#pragma once

#include <QString>
#include <QByteArray>
#include <QJsonValue>
#include <memory>
#include <optional>

// Forward declare ggml structures
struct ggml_tensor;
struct ggml_context;

namespace mem {

/**
 * @class RepoMemory
 * @brief Manages GGUF side-car tensors for repository-level embeddings and summaries
 * 
 * Architecture:
 * - Stores 256-dim embeddings for every source file
 * - Maintains hierarchical summaries (chunk → file → folder)
 * - Provides semantic search via cosine similarity
 * - Side-car file: models/<model>.memory.gguf (~2% of model size)
 * 
 * Usage:
 * - Load with load(ggufPath) after model initialization
 * - retrieve(query) returns topK most relevant code chunks
 * - No explicit save required (read-only on startup)
 */
class RepoMemory {
public:
    /**
     * @struct CodeChunk
     * @brief Single code chunk with embedding and metadata
     */
    struct CodeChunk {
        QString filePath;
        QString content;
        int startLine = 0;
        int endLine = 0;
        double similarity = 0.0;  ///< Cosine similarity to query [0.0, 1.0]
    };

    RepoMemory();
    ~RepoMemory();

    /**
     * @brief Load repository memory from GGUF side-car
     * @param ggufPath Path to model.gguf (will look for model.memory.gguf)
     * @return True if side-car found and loaded
     */
    bool load(const QString& ggufPath);

    /**
     * @brief Check if memory is loaded and valid
     */
    bool isLoaded() const;

    /**
     * @brief Retrieve topK code chunks most relevant to query
     * @param query Natural language query or code snippet
     * @param topK Number of results to return (default 5)
     * @return Vector of CodeChunk ordered by similarity (descending)
     */
    std::vector<CodeChunk> retrieve(const QString& query, int topK = 5) const;

    /**
     * @brief Retrieve chunks from specific file
     * @param filePath Target file path
     * @param topK Number of results
     * @return Chunks from that file
     */
    std::vector<CodeChunk> retrieveFromFile(const QString& filePath, int topK = 10) const;

    /**
     * @brief Index a new source file (add to embeddings)
     * @param filePath File path
     * @param content File contents
     * @return True if successful
     * 
     * Note: Changes are in-memory only until saveIndex() called
     */
    bool indexFile(const QString& filePath, const QString& content);

    /**
     * @brief Save indexed changes back to side-car GGUF
     * @return True if successful
     */
    bool saveIndex();

    /**
     * @brief Get file summary (hierarchical)
     * @param filePath File to summarize
     * @param maxLen Maximum summary length
     * @return Summary string
     */
    QString getFileSummary(const QString& filePath, int maxLen = 500) const;

    /**
     * @brief Get folder-level summary
     * @param folderPath Folder to summarize
     * @param maxLen Maximum summary length
     * @return Summary string
     */
    QString getFolderSummary(const QString& folderPath, int maxLen = 1000) const;

    /**
     * @brief Rebuild entire index from workspace
     * @param workspacePath Root directory to index
     * @param filePattern Glob pattern (e.g., "*.cpp", "*.h")
     * @return Number of files indexed
     */
    int rebuildIndex(const QString& workspacePath, const QString& filePattern = "*.cpp;*.h");

    /**
     * @brief Clear all cached data (keep side-car file)
     */
    void clear();

    /**
     * @brief Get memory usage stats
     * @return Map of stat name → bytes
     */
    std::unordered_map<std::string, int64_t> getMemoryStats() const;

private:
    ggml_context* meta = nullptr;
    std::unordered_map<std::string, std::vector<float>> embeddings;  // filepath → 256-d vector
    std::unordered_map<std::string, QString> summaries;              // filepath → summary
    bool dirty = false;

    /**
     * @brief Generate 256-dim embedding for text
     */
    std::vector<float> embed(const QString& text) const;

    /**
     * @brief Compute cosine similarity between vectors
     */
    static double cosineSimilarity(const std::vector<float>& a,
                                   const std::vector<float>& b);

    /**
     * @brief Generate summary for code chunk
     */
    static QString generateSummary(const QString& content);

    /**
     * @brief Load tensors from side-car GGUF
     */
    bool loadFromSidecar(const QString& sidecarPath);

    /**
     * @brief Save tensors to side-car GGUF
     */
    bool saveToSidecar(const QString& sidecarPath);
};

} // namespace mem
