#pragma once
/**
 * @file search_indexer.h
 * @brief Workspace-wide search indexing for symbols and text
 * Batch 3 - Item 33: Search indexing
 */

#include <string>
#include <vector>
#include <map>
#include <unordered_set>
#include <mutex>
#include <functional>

namespace RawrXD::Search {

struct Posting {
    std::string filePath;
    size_t lineNumber;
};

struct SearchResult {
    std::string filePath;
    size_t lineNumber;
    std::string lineContent;
    float relevanceScore;
};

class SearchIndexer {
public:
    SearchIndexer();
    ~SearchIndexer();

    // Indexing operations
    void indexFile(const std::string& path);
    void removeFile(const std::string& path);
    void updateFile(const std::string& path);
    void indexWorkspace(const std::string& rootPath, const std::vector<std::string>& extensions);

    // Search operations
    std::vector<SearchResult> search(const std::string& query, size_t maxResults = 100);
    std::vector<SearchResult> searchSymbols(const std::string& symbolName);
    std::vector<SearchResult> searchRegex(const std::string& pattern);

    // Status
    size_t getIndexedFileCount() const;
    size_t getTokenCount() const;
    void clear();

    // Incremental updates
    void pauseIndexing();
    void resumeIndexing();
    bool isIndexing() const;

private:
    mutable std::mutex m_mutex;
    std::map<std::string, std::vector<Posting>> m_index;
    std::map<std::string, std::unordered_set<std::string>> m_fileTokens;
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_indexing{false};

    std::string normalizeToken(const std::string& token);
    float calculateRelevance(const std::string& query, const Posting& posting);
};

// Global instance
SearchIndexer& getSearchIndexer();

} // namespace RawrXD::Search
