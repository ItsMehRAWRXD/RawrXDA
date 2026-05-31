// at_docs_indexer.h - `@Docs` documentation indexer (Cursor parity)
// Feature 5/15 (Cursor parity).
#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rawrxd::parity {

struct DocChunk {
    std::string doc_id;        // logical doc (e.g. "python/stdlib")
    std::string title;
    std::string path;          // absolute path to source file
    std::uint32_t start_line{0};
    std::uint32_t end_line{0};
    std::string text;          // clamped chunk body
};

struct DocSearchHit {
    const DocChunk* chunk{nullptr};
    double score{0.0};
};

class AtDocsIndexer {
public:
    // Index all .md/.mdx/.rst/.txt under `root` as doc_id logical bundle.
    std::size_t index_folder(const std::string& doc_id,
                             const std::filesystem::path& root,
                             std::size_t max_chunk_chars = 1200);

    // Bulk add raw chunks (e.g. from a remote fetcher).
    void add_chunk(DocChunk c);

    // BM25-lite ranked search scoped to a doc_id (or empty = all).
    std::vector<DocSearchHit> search(std::string_view query,
                                     std::string_view doc_id = "",
                                     std::size_t top_k = 5) const;

    std::size_t total_chunks() const;
    std::vector<std::string> list_docs() const;
    void clear();

private:
    static std::vector<std::string> tokenize(std::string_view s);

    mutable std::mutex mu_;
    std::vector<DocChunk> chunks_;
    // term → list of (chunk_index, tf)
    std::unordered_map<std::string, std::vector<std::pair<std::uint32_t, std::uint16_t>>> postings_;
    double avg_len_{0.0};
};

} // namespace rawrxd::parity
