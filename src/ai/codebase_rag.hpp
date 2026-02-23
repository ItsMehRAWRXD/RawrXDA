// codebase_rag.hpp — In-house RAG for codebase context (P0)
// TF-IDF style indexing; no external embedding model.
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace RawrXD {
namespace AI {

struct RAGChunk {
    std::string file;
    int         line = 0;
    std::string text;
    std::vector<float> embedding;  // TF-IDF vector
};

/** In-house codebase index: tokenize + TF-IDF, query by similarity. */
class CodebaseRAG {
public:
    CodebaseRAG() = default;

    /** Index a directory of source files. */
    void indexDirectory(const std::string& rootPath);

    /** Query: return top K chunks by cosine similarity. */
    std::vector<std::pair<float, RAGChunk>> query(const std::string& queryText, int topK = 10) const;

    /** Build context string for prompt (up to ~maxChars). */
    std::string getContextForPrompt(const std::string& queryText, size_t maxChars = 4000) const;

private:
    std::vector<RAGChunk> m_chunks;
    std::unordered_map<std::string, float> m_idf;
    size_t m_vocabSize = 8192;

    void indexFile(const std::string& path);
    std::vector<std::string> tokenize(const std::string& text) const;
    std::vector<float> embed(const std::string& text) const;
    void normalize(std::vector<float>& v) const;
    float cosine(const std::vector<float>& a, const std::vector<float>& b) const;
};

} // namespace AI
} // namespace RawrXD
