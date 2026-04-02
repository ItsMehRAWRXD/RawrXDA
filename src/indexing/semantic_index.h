// ============================================================================
// semantic_index.h — Vector-based Semantic Indexer Header
// Provides semantic search capabilities across codebase using embeddings
// ============================================================================

#pragma once

#include <string>
#include <vector>

namespace RawrXD {
namespace Indexing {

class SemanticIndex {
public:
    static SemanticIndex& instance();

    // Index a file's content
    void indexFile(const std::string& filePath, const std::string& content);

    // Search for semantically similar content
    std::vector<std::string> search(const std::string& query, size_t max_results = 10);

    // Clear the index
    void clear();

    // Get index size
    size_t size() const;

private:
    SemanticIndex();
    ~SemanticIndex();

    class Impl;
    std::unique_ptr<Impl> pimpl;
};

}} // namespace RawrXD::Indexing